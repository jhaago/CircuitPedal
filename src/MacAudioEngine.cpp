#include "MacAudioEngine.h"

#include "DistortionPlusModel.h"
#include "GenericCircuit.h"
#include "GenericCircuitProcessor.h"

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <sstream>
#include <utility>
#include <vector>

namespace circuitpedal {
namespace {

static_assert(std::atomic<float>::is_always_lock_free,
              "Real-time meter atomics must be lock-free on this target");
static_assert(std::atomic<bool>::is_always_lock_free,
              "Real-time state atomics must be lock-free on this target");

std::string statusError(const char* operation, OSStatus status)
{
    char code[5] = {};
    const UInt32 raw = CFSwapInt32HostToBig(static_cast<UInt32>(status));
    std::memcpy(code, &raw, 4);

    std::ostringstream message;
    message << operation << " failed (OSStatus " << status;
    if (std::isprint(static_cast<unsigned char>(code[0]))
        && std::isprint(static_cast<unsigned char>(code[1]))
        && std::isprint(static_cast<unsigned char>(code[2]))
        && std::isprint(static_cast<unsigned char>(code[3])))
    {
        message << ", '" << code << "'";
    }
    message << ").";
    return message.str();
}

template <typename T>
bool getDeviceProperty(AudioDeviceID device,
                       AudioObjectPropertySelector selector,
                       AudioObjectPropertyScope scope,
                       T& value) noexcept
{
    UInt32 size = sizeof(value);
    AudioObjectPropertyAddress address { selector, scope, kAudioObjectPropertyElementMain };
    return AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &value) == noErr;
}

std::string deviceName(AudioDeviceID device)
{
    CFStringRef name = nullptr;
    UInt32 size = sizeof(name);
    AudioObjectPropertyAddress address {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &name) != noErr
        || name == nullptr)
    {
        return "Unknown audio device";
    }

    char buffer[512] = {};
    const bool converted = CFStringGetCString(name,
                                               buffer,
                                               sizeof(buffer),
                                               kCFStringEncodingUTF8);
    CFRelease(name);
    return converted ? buffer : "Unknown audio device";
}

UInt32 deviceChannelCount(AudioDeviceID device, AudioObjectPropertyScope scope)
{
    AudioObjectPropertyAddress address {
        kAudioDevicePropertyStreamConfiguration,
        scope,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(device, &address, 0, nullptr, &size) != noErr
        || size < sizeof(AudioBufferList))
    {
        return 0;
    }

    const std::size_t wordCount =
        (static_cast<std::size_t>(size) + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t);
    std::vector<std::max_align_t> storage(wordCount);
    auto* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, list) != noErr)
        return 0;

    UInt32 channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i)
        channels += list->mBuffers[i].mNumberChannels;
    return channels;
}

std::vector<AudioDeviceID> audioDeviceIds(std::string& error)
{
    AudioObjectPropertyAddress address {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                     &address,
                                                     0,
                                                     nullptr,
                                                     &size);
    if (status != noErr)
    {
        error = statusError("Get audio device list size", status);
        return {};
    }

    std::vector<AudioDeviceID> devices(size / sizeof(AudioDeviceID));
    status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                        &address,
                                        0,
                                        nullptr,
                                        &size,
                                        devices.data());
    if (status != noErr)
    {
        error = statusError("Get audio device list", status);
        return {};
    }
    return devices;
}

double deviceSampleRate(AudioDeviceID device, std::string& error)
{
    Float64 rate = 0.0;
    if (!getDeviceProperty(device,
                           kAudioDevicePropertyNominalSampleRate,
                           kAudioObjectPropertyScopeGlobal,
                           rate)
        || !std::isfinite(rate)
        || rate < 8000.0)
    {
        error = "Could not obtain a valid sample rate from the selected device.";
        return 0.0;
    }
    return rate;
}

bool configureBufferSize(AudioDeviceID device,
                         UInt32 requested,
                         UInt32& actual,
                         std::string& error)
{
    if (requested == 0)
    {
        error = "Buffer size must be greater than zero.";
        return false;
    }

    AudioObjectPropertyAddress address {
        kAudioDevicePropertyBufferFrameSize,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    AudioValueRange range {};
    UInt32 rangeSize = sizeof(range);
    AudioObjectPropertyAddress rangeAddress {
        kAudioDevicePropertyBufferFrameSizeRange,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    if (AudioObjectGetPropertyData(device, &rangeAddress, 0, nullptr, &rangeSize, &range) == noErr
        && (static_cast<double>(requested) < range.mMinimum
            || static_cast<double>(requested) > range.mMaximum))
    {
        std::ostringstream message;
        message << "The selected device does not support a " << requested
                << "-frame buffer. Its reported range is "
                << static_cast<UInt32>(std::ceil(range.mMinimum)) << "-"
                << static_cast<UInt32>(std::floor(range.mMaximum)) << " frames.";
        error = message.str();
        return false;
    }

    Boolean settable = false;
    const OSStatus settableStatus = AudioObjectIsPropertySettable(device, &address, &settable);
    if (settableStatus != noErr)
    {
        error = statusError("Check device buffer-size control", settableStatus);
        return false;
    }
    if (settable)
    {
        const OSStatus status = AudioObjectSetPropertyData(device,
                                                           &address,
                                                           0,
                                                           nullptr,
                                                           sizeof(requested),
                                                           &requested);
        if (status != noErr)
        {
            error = statusError("Request device buffer size", status);
            return false;
        }
    }

    actual = 0;
    if (!getDeviceProperty(device,
                           kAudioDevicePropertyBufferFrameSize,
                           kAudioObjectPropertyScopeGlobal,
                           actual)
        || actual == 0)
    {
        error = "Could not obtain the active device buffer size.";
        return false;
    }
    return true;
}

UInt32 latencyProperty(AudioDeviceID device,
                       AudioObjectPropertySelector selector,
                       AudioObjectPropertyScope scope) noexcept
{
    UInt32 value = 0;
    getDeviceProperty(device, selector, scope, value);
    return value;
}

AudioStreamBasicDescription floatFormat(double sampleRate, UInt32 channels) noexcept
{
    AudioStreamBasicDescription format {};
    format.mSampleRate = sampleRate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat
                        | kAudioFormatFlagIsPacked
                        | kAudioFormatFlagsNativeEndian;
    format.mBytesPerPacket = static_cast<UInt32>(sizeof(Float32)) * channels;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = static_cast<UInt32>(sizeof(Float32)) * channels;
    format.mChannelsPerFrame = channels;
    format.mBitsPerChannel = 8U * static_cast<UInt32>(sizeof(Float32));
    return format;
}

void clearOutput(AudioBufferList* data) noexcept
{
    if (data == nullptr)
        return;
    for (UInt32 buffer = 0; buffer < data->mNumberBuffers; ++buffer)
    {
        if (data->mBuffers[buffer].mData != nullptr)
            std::memset(data->mBuffers[buffer].mData, 0, data->mBuffers[buffer].mDataByteSize);
    }
}

bool outputBuffersAreValid(const AudioBufferList* data, UInt32 frameCount) noexcept
{
    if (data == nullptr || data->mNumberBuffers == 0)
        return false;
    for (UInt32 buffer = 0; buffer < data->mNumberBuffers; ++buffer)
    {
        const AudioBuffer& current = data->mBuffers[buffer];
        const std::uint64_t requiredBytes = static_cast<std::uint64_t>(frameCount)
            * current.mNumberChannels * sizeof(Float32);
        if (current.mData == nullptr
            || current.mNumberChannels == 0
            || requiredBytes > current.mDataByteSize)
        {
            return false;
        }
    }
    return true;
}

} // namespace

struct MacAudioEngine::Impl {
    AudioUnit unit = nullptr;
    DistortionPlusModel pedal;
    OversampledGenericCircuit genericCircuit;
    CircuitFileDocument circuitDocument;
    bool circuitFileSelected = false;
    std::array<double, 64> genericDryDelay {};
    std::size_t genericDryDelayWriteIndex = 0;
    std::array<float, GenericCircuit::maximumLivePotentiometers> circuitControlTargets {};
    std::size_t circuitControlCount = 0;
    std::atomic<bool> genericBypass { false };
    double genericWetMix = 1.0;
    double genericBypassCoefficient = 1.0;

    std::vector<Float32> inputScratch;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<bool> running { false };
    AudioRuntimeInfo info;

    static OSStatus renderCallback(void* refCon,
                                   AudioUnitRenderActionFlags* flags,
                                   const AudioTimeStamp* timeStamp,
                                   UInt32,
                                   UInt32 frameCount,
                                   AudioBufferList* ioData) noexcept
    {
        auto* state = static_cast<Impl*>(refCon);
        if (state == nullptr
            || frameCount > state->inputScratch.size()
            || !outputBuffersAreValid(ioData, frameCount))
        {
            clearOutput(ioData);
            return kAudio_ParamError;
        }

        AudioBufferList inputData {};
        inputData.mNumberBuffers = 1;
        inputData.mBuffers[0].mNumberChannels = 1;
        inputData.mBuffers[0].mDataByteSize = frameCount * static_cast<UInt32>(sizeof(Float32));
        inputData.mBuffers[0].mData = state->inputScratch.data();

        const OSStatus status = AudioUnitRender(state->unit,
                                                flags,
                                                timeStamp,
                                                1,
                                                frameCount,
                                                &inputData);
        if (status != noErr)
        {
            clearOutput(ioData);
            return status;
        }

        float blockInputPeak = 0.0f;
        float blockOutputPeak = 0.0f;
        for (UInt32 frame = 0; frame < frameCount; ++frame)
        {
            const float input = state->inputScratch[frame];
            float output = 0.0f;
            if (state->circuitFileSelected)
            {
                const float wet = state->genericCircuit.processSample(input);

                const std::size_t dryReadIndex =
                    (state->genericDryDelayWriteIndex
                     + state->genericDryDelay.size()
                     - OversampledGenericCircuit::delayHostSamples)
                    % state->genericDryDelay.size();
                const double delayedDry =
                    state->genericDryDelay[dryReadIndex];
                state->genericDryDelay[state->genericDryDelayWriteIndex] = input;
                state->genericDryDelayWriteIndex =
                    (state->genericDryDelayWriteIndex + 1)
                    % state->genericDryDelay.size();

                const double wetTarget =
                    state->genericBypass.load(std::memory_order_relaxed) ? 0.0 : 1.0;
                state->genericWetMix += state->genericBypassCoefficient
                    * (wetTarget - state->genericWetMix);
                const double mixed =
                    delayedDry * (1.0 - state->genericWetMix)
                    + static_cast<double>(wet) * state->genericWetMix;
                output = static_cast<float>(std::clamp(mixed, -1.0, 1.0));
            }
            else
            {
                output = state->pedal.processSample(input);
            }
            if (std::isfinite(input))
                blockInputPeak = std::max(blockInputPeak, std::abs(input));
            blockOutputPeak = std::max(blockOutputPeak, std::abs(output));

            for (UInt32 buffer = 0; buffer < ioData->mNumberBuffers; ++buffer)
            {
                AudioBuffer& current = ioData->mBuffers[buffer];
                auto* samples = static_cast<Float32*>(current.mData);
                for (UInt32 channel = 0; channel < current.mNumberChannels; ++channel)
                    samples[frame * current.mNumberChannels + channel] = output;
            }
        }

        state->inputPeak.store(blockInputPeak, std::memory_order_relaxed);
        state->outputPeak.store(blockOutputPeak, std::memory_order_relaxed);
        return noErr;
    }
};

double AudioRuntimeInfo::bufferDurationMilliseconds() const noexcept
{
    return sampleRate > 0.0
        ? 1000.0 * static_cast<double>(actualBufferFrames) / sampleRate
        : 0.0;
}

double AudioRuntimeInfo::reportedLatencyMilliseconds() const noexcept
{
    if (sampleRate <= 0.0)
        return 0.0;
    const std::uint64_t frames = static_cast<std::uint64_t>(inputLatencyFrames)
        + inputSafetyOffsetFrames + outputLatencyFrames + outputSafetyOffsetFrames
        + dspDelayFrames;
    return 1000.0 * static_cast<double>(frames) / sampleRate;
}

MacAudioEngine::MacAudioEngine() : impl_(std::make_unique<Impl>()) {}

MacAudioEngine::~MacAudioEngine() { stop(); }

std::vector<AudioDeviceInfo> MacAudioEngine::enumerateDevices(std::string& error)
{
    error.clear();
    try
    {
        const auto deviceIds = audioDeviceIds(error);
        if (!error.empty())
            return {};

        std::vector<AudioDeviceInfo> devices;
        devices.reserve(deviceIds.size());
        for (const AudioDeviceID device : deviceIds)
        {
            AudioDeviceInfo info;
            info.id = static_cast<std::uint32_t>(device);
            info.name = deviceName(device);
            info.inputChannels = deviceChannelCount(device, kAudioObjectPropertyScopeInput);
            info.outputChannels = deviceChannelCount(device, kAudioObjectPropertyScopeOutput);
            devices.push_back(std::move(info));
        }
        return devices;
    }
    catch (const std::exception& exception)
    {
        error = std::string("Could not enumerate audio devices: ") + exception.what();
        return {};
    }
}

std::uint32_t MacAudioEngine::defaultOutputDeviceId(std::string& error)
{
    error.clear();
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress address {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    const OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                       &address,
                                                       0,
                                                       nullptr,
                                                       &size,
                                                       &device);
    if (status != noErr)
    {
        error = statusError("Get default output device", status);
        return 0;
    }
    return static_cast<std::uint32_t>(device);
}

bool MacAudioEngine::start(const AudioStartConfiguration& configuration, std::string& error)
{
    stop();
    error.clear();

    try
    {
        std::string enumerationError;
        const auto devices = enumerateDevices(enumerationError);
        if (!enumerationError.empty())
        {
            error = enumerationError;
            return false;
        }
        const auto selected = std::find_if(devices.begin(), devices.end(),
            [&configuration](const AudioDeviceInfo& device) {
                return device.id == configuration.deviceId;
            });
        if (selected == devices.end())
        {
            error = "The selected audio device is no longer available. Choose another device and retry.";
            return false;
        }
        if (!selected->isDuplex())
        {
            error = "The selected device is not duplex; it must provide both input and output channels.";
            return false;
        }
        if (configuration.inputChannel >= selected->inputChannels)
        {
            std::ostringstream message;
            message << "Input channel " << (configuration.inputChannel + 1)
                    << " does not exist on the selected device.";
            error = message.str();
            return false;
        }

        AudioComponentDescription description {};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_HALOutput;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        const AudioComponent component = AudioComponentFindNext(nullptr, &description);
        if (component == nullptr)
        {
            error = "Could not find Apple's HAL Output Audio Unit.";
            return false;
        }

        auto failStatus = [this, &error](const char* operation, OSStatus status,
                                         bool microphoneHint = false) {
            error = statusError(operation, status);
            if (microphoneHint)
            {
                error += " Microphone permission may be unavailable; check System Settings > "
                         "Privacy & Security > Microphone.";
            }
            stop();
            return false;
        };

        OSStatus status = AudioComponentInstanceNew(component, &impl_->unit);
        if (status != noErr)
            return failStatus("Create HAL Audio Unit", status);

        UInt32 enable = 1;
        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Input,
                                      1,
                                      &enable,
                                      sizeof(enable));
        if (status != noErr)
            return failStatus("Enable audio input", status, true);

        const AudioDeviceID device = static_cast<AudioDeviceID>(configuration.deviceId);
        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioOutputUnitProperty_CurrentDevice,
                                      kAudioUnitScope_Global,
                                      0,
                                      &device,
                                      sizeof(device));
        if (status != noErr)
            return failStatus("Select audio device", status);

        UInt32 actualBufferFrames = 0;
        if (!configureBufferSize(device,
                                 static_cast<UInt32>(configuration.requestedBufferFrames),
                                 actualBufferFrames,
                                 error))
        {
            stop();
            return false;
        }

        const double sampleRate = deviceSampleRate(device, error);
        if (sampleRate <= 0.0)
        {
            stop();
            return false;
        }
        const UInt32 clientOutputChannels =
            std::min<UInt32>(2, static_cast<UInt32>(selected->outputChannels));
        const auto inputFormat = floatFormat(sampleRate, 1);
        const auto outputFormat = floatFormat(sampleRate, clientOutputChannels);

        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioUnitProperty_StreamFormat,
                                      kAudioUnitScope_Output,
                                      1,
                                      &inputFormat,
                                      sizeof(inputFormat));
        if (status != noErr)
            return failStatus("Set client input format", status);

        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioUnitProperty_StreamFormat,
                                      kAudioUnitScope_Input,
                                      0,
                                      &outputFormat,
                                      sizeof(outputFormat));
        if (status != noErr)
            return failStatus("Set client output format", status);

        const SInt32 inputChannelMap = static_cast<SInt32>(configuration.inputChannel);
        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioOutputUnitProperty_ChannelMap,
                                      kAudioUnitScope_Output,
                                      1,
                                      &inputChannelMap,
                                      sizeof(inputChannelMap));
        if (status != noErr)
            return failStatus("Select interface input channel", status);

        UInt32 requestedMaximumFrames = std::max<UInt32>(4096, actualBufferFrames);
        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioUnitProperty_MaximumFramesPerSlice,
                                      kAudioUnitScope_Global,
                                      0,
                                      &requestedMaximumFrames,
                                      sizeof(requestedMaximumFrames));
        if (status != noErr)
            return failStatus("Set maximum frames per slice", status);

        UInt32 maximumFrames = 0;
        UInt32 maximumFramesSize = sizeof(maximumFrames);
        status = AudioUnitGetProperty(impl_->unit,
                                      kAudioUnitProperty_MaximumFramesPerSlice,
                                      kAudioUnitScope_Global,
                                      0,
                                      &maximumFrames,
                                      &maximumFramesSize);
        if (status != noErr)
            return failStatus("Get maximum frames per slice", status);
        if (maximumFrames == 0 || maximumFrames > 65536)
        {
            error = "Audio Unit returned an unsafe maximum frame count.";
            stop();
            return false;
        }
        impl_->inputScratch.resize(maximumFrames);

        AURenderCallbackStruct callback {};
        callback.inputProc = Impl::renderCallback;
        callback.inputProcRefCon = impl_.get();
        status = AudioUnitSetProperty(impl_->unit,
                                      kAudioUnitProperty_SetRenderCallback,
                                      kAudioUnitScope_Input,
                                      0,
                                      &callback,
                                      sizeof(callback));
        if (status != noErr)
            return failStatus("Install render callback", status);

        if (impl_->circuitFileSelected)
        {
            std::string circuitError;
            if (!impl_->genericCircuit.compile(
                    impl_->circuitDocument.definition,
                    sampleRate,
                    circuitError))
            {
                error = "Could not compile loaded circuit '" + impl_->circuitDocument.name
                    + "': " + circuitError;
                stop();
                return false;
            }
            for (std::size_t control = 0; control < impl_->circuitControlCount; ++control)
            {
                (void)impl_->genericCircuit.setPotentiometerPosition(
                    control,
                    static_cast<double>(impl_->circuitControlTargets[control]));
            }
            impl_->genericWetMix =
                impl_->genericBypass.load(std::memory_order_relaxed) ? 0.0 : 1.0;
            impl_->genericDryDelay.fill(0.0);
            impl_->genericDryDelayWriteIndex = 0;
            constexpr double bypassSmoothingSeconds = 0.005;
            impl_->genericBypassCoefficient =
                1.0 - std::exp(-1.0 / (bypassSmoothingSeconds * sampleRate));
        }
        else
        {
            impl_->pedal.prepare(sampleRate);
        }
        impl_->inputPeak.store(0.0f, std::memory_order_relaxed);
        impl_->outputPeak.store(0.0f, std::memory_order_relaxed);

        status = AudioUnitInitialize(impl_->unit);
        if (status != noErr)
            return failStatus("Initialise Audio Unit", status, true);

        AudioRuntimeInfo runtime;
        runtime.sampleRate = sampleRate;
        runtime.requestedBufferFrames = configuration.requestedBufferFrames;
        runtime.actualBufferFrames = actualBufferFrames;
        runtime.inputLatencyFrames = latencyProperty(device,
                                                      kAudioDevicePropertyLatency,
                                                      kAudioObjectPropertyScopeInput);
        runtime.inputSafetyOffsetFrames = latencyProperty(device,
                                                           kAudioDevicePropertySafetyOffset,
                                                           kAudioObjectPropertyScopeInput);
        runtime.outputLatencyFrames = latencyProperty(device,
                                                       kAudioDevicePropertyLatency,
                                                       kAudioObjectPropertyScopeOutput);
        runtime.outputSafetyOffsetFrames = latencyProperty(device,
                                                            kAudioDevicePropertySafetyOffset,
                                                            kAudioObjectPropertyScopeOutput);
        runtime.dspDelayFrames = impl_->circuitFileSelected
            ? static_cast<std::uint32_t>(
                  OversampledGenericCircuit::delayHostSamples)
            : static_cast<std::uint32_t>(
                  Oversampler4x::wetDelayHostSamples);
        impl_->info = runtime;

        status = AudioOutputUnitStart(impl_->unit);
        if (status != noErr)
            return failStatus("Start audio", status, true);

        impl_->running.store(true, std::memory_order_release);
        return true;
    }
    catch (const std::exception& exception)
    {
        stop();
        error = std::string("Could not start audio: ") + exception.what();
        return false;
    }
}

void MacAudioEngine::stop() noexcept
{
    impl_->running.store(false, std::memory_order_release);
    if (impl_->unit != nullptr)
    {
        AudioOutputUnitStop(impl_->unit);
        AudioUnitUninitialize(impl_->unit);
        AudioComponentInstanceDispose(impl_->unit);
        impl_->unit = nullptr;
    }
    impl_->inputScratch.clear();
    impl_->inputPeak.store(0.0f, std::memory_order_relaxed);
    impl_->outputPeak.store(0.0f, std::memory_order_relaxed);
    impl_->info = {};
}

bool MacAudioEngine::isRunning() const noexcept
{
    return impl_->running.load(std::memory_order_acquire);
}

bool MacAudioEngine::loadCircuitFile(const std::string& path, std::string& error)
{
    if (isRunning())
    {
        error = "Stop audio before loading a circuit file.";
        return false;
    }

    CircuitFileDocument document;
    if (!circuitpedal::loadCircuitFile(path, document, error))
        return false;
    if (document.controls.size() > GenericCircuit::maximumLivePotentiometers)
    {
        error = "Circuit file exposes too many live controls.";
        return false;
    }

    impl_->circuitDocument = std::move(document);
    impl_->circuitControlCount = impl_->circuitDocument.controls.size();
    for (std::size_t i = 0; i < impl_->circuitControlCount; ++i)
    {
        impl_->circuitControlTargets[i] = static_cast<float>(
            impl_->circuitDocument.controls[i].initialPosition);
    }
    impl_->circuitFileSelected = true;
    impl_->genericBypass.store(false, std::memory_order_relaxed);
    return true;
}

bool MacAudioEngine::useBuiltInDistortionPlus() noexcept
{
    if (isRunning())
        return false;
    impl_->circuitFileSelected = false;
    return true;
}

bool MacAudioEngine::usingCircuitFile() const noexcept
{
    return impl_->circuitFileSelected;
}

std::string MacAudioEngine::activeModelName() const
{
    return impl_->circuitFileSelected
        ? impl_->circuitDocument.name
        : "Built-in Distortion+";
}

std::vector<CircuitFileControl> MacAudioEngine::circuitControls() const
{
    return impl_->circuitFileSelected
        ? impl_->circuitDocument.controls
        : std::vector<CircuitFileControl> {};
}

bool MacAudioEngine::setCircuitControl(std::size_t index, float normalized) noexcept
{
    if (!impl_->circuitFileSelected
        || index >= impl_->circuitControlCount
        || !std::isfinite(normalized))
    {
        return false;
    }

    const float bounded = std::clamp(normalized, 0.0f, 1.0f);
    impl_->circuitControlTargets[index] = bounded;
    if (isRunning())
    {
        return impl_->genericCircuit.setPotentiometerPosition(
            index,
            static_cast<double>(bounded));
    }
    return true;
}

float MacAudioEngine::circuitControl(std::size_t index) const noexcept
{
    if (!impl_->circuitFileSelected || index >= impl_->circuitControlCount)
        return 0.0f;
    if (isRunning())
    {
        return static_cast<float>(
            impl_->genericCircuit.potentiometerPosition(index));
    }
    return impl_->circuitControlTargets[index];
}

void MacAudioEngine::setDistortion(float normalized) noexcept
{
    impl_->pedal.setDistortion(normalized);
}

void MacAudioEngine::setOutput(float normalized) noexcept
{
    impl_->pedal.setOutput(normalized);
}

void MacAudioEngine::setBypass(bool bypassed) noexcept
{
    impl_->pedal.setBypass(bypassed);
    impl_->genericBypass.store(bypassed, std::memory_order_relaxed);
}

void MacAudioEngine::setClippingDiodePreset(ClippingDiodePreset preset) noexcept
{
    impl_->pedal.setClippingDiodePreset(preset);
}

bool MacAudioEngine::setCircuitParameters(
    const DistortionPlusCircuitParameters& parameters) noexcept
{
    if (isRunning())
        return false;
    return impl_->pedal.setCircuitParameters(parameters);
}

float MacAudioEngine::distortion() const noexcept { return impl_->pedal.getDistortion(); }
float MacAudioEngine::output() const noexcept { return impl_->pedal.getOutput(); }
bool MacAudioEngine::bypassed() const noexcept
{
    return impl_->circuitFileSelected
        ? impl_->genericBypass.load(std::memory_order_relaxed)
        : impl_->pedal.getBypass();
}
ClippingDiodePreset MacAudioEngine::clippingDiodePreset() const noexcept
{
    return impl_->pedal.getClippingDiodePreset();
}

DistortionPlusCircuitParameters MacAudioEngine::circuitParameters() const noexcept
{
    return impl_->pedal.getCircuitParameters();
}

float MacAudioEngine::inputPeak() const noexcept
{
    return impl_->inputPeak.load(std::memory_order_relaxed);
}

float MacAudioEngine::outputPeak() const noexcept
{
    return impl_->outputPeak.load(std::memory_order_relaxed);
}

AudioRuntimeInfo MacAudioEngine::runtimeInfo() const noexcept { return impl_->info; }

} // namespace circuitpedal
