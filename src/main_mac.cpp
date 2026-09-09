#include "DistortionPlusModel.h"

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using circuitpedal::DistortionPlusModel;

namespace {

void checkStatus(OSStatus status, const char* what)
{
    if (status == noErr)
        return;

    char code[5] = {};
    const UInt32 raw = CFSwapInt32HostToBig(static_cast<UInt32>(status));
    std::memcpy(code, &raw, 4);
    std::cerr << what << " failed (OSStatus " << status;
    if (std::isprint(static_cast<unsigned char>(code[0]))
        && std::isprint(static_cast<unsigned char>(code[1]))
        && std::isprint(static_cast<unsigned char>(code[2]))
        && std::isprint(static_cast<unsigned char>(code[3])))
    {
        std::cerr << ", '" << code << "'";
    }
    std::cerr << ")\n";
    std::exit(1);
}

template <typename T>
bool getDeviceProperty(AudioDeviceID device,
                       AudioObjectPropertySelector selector,
                       AudioObjectPropertyScope scope,
                       T& value)
{
    UInt32 size = sizeof(value);
    AudioObjectPropertyAddress address { selector, scope, kAudioObjectPropertyElementMain };
    return AudioObjectGetPropertyData(device,
                                      &address,
                                      0,
                                      nullptr,
                                      &size,
                                      &value) == noErr;
}

AudioDeviceID defaultOutputDevice()
{
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 size = sizeof(device);
    AudioObjectPropertyAddress address {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    checkStatus(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                           &address,
                                           0,
                                           nullptr,
                                           &size,
                                           &device),
                "Get default output device");
    return device;
}

double deviceSampleRate(AudioDeviceID device)
{
    Float64 rate = 0.0;
    if (!getDeviceProperty(device,
                           kAudioDevicePropertyNominalSampleRate,
                           kAudioObjectPropertyScopeGlobal,
                           rate)
        || !std::isfinite(rate)
        || rate < 8000.0)
    {
        std::cerr << "Could not obtain a valid sample rate for device " << device << ".\n";
        std::exit(1);
    }
    return rate;
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
    if (AudioObjectGetPropertyData(device,
                                   &address,
                                   0,
                                   nullptr,
                                   &size,
                                   &name) != noErr
        || name == nullptr)
    {
        return "Unknown audio device";
    }

    char buffer[256] = {};
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
    if (AudioObjectGetPropertyDataSize(device,
                                       &address,
                                       0,
                                       nullptr,
                                       &size) != noErr
        || size < sizeof(AudioBufferList))
    {
        return 0;
    }

    const std::size_t wordCount =
        (static_cast<std::size_t>(size) + sizeof(std::max_align_t) - 1)
        / sizeof(std::max_align_t);
    std::vector<std::max_align_t> storage(wordCount);
    auto* list = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(device,
                                   &address,
                                   0,
                                   nullptr,
                                   &size,
                                   list) != noErr)
    {
        return 0;
    }

    UInt32 channels = 0;
    for (UInt32 i = 0; i < list->mNumberBuffers; ++i)
        channels += list->mBuffers[i].mNumberChannels;
    return channels;
}

std::vector<AudioDeviceID> audioDevices()
{
    AudioObjectPropertyAddress address {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    UInt32 size = 0;
    checkStatus(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                               &address,
                                               0,
                                               nullptr,
                                               &size),
                "Get audio device list size");
    std::vector<AudioDeviceID> devices(size / sizeof(AudioDeviceID));
    checkStatus(AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                           &address,
                                           0,
                                           nullptr,
                                           &size,
                                           devices.data()),
                "Get audio device list");
    return devices;
}

void listAudioDevices()
{
    std::cout << "Available Core Audio devices:\n";
    for (const AudioDeviceID device : audioDevices())
    {
        std::cout << "  ID " << device
                  << "  inputs=" << deviceChannelCount(device, kAudioObjectPropertyScopeInput)
                  << "  outputs=" << deviceChannelCount(device, kAudioObjectPropertyScopeOutput)
                  << "  " << deviceName(device) << '\n';
    }
}

AudioStreamBasicDescription floatFormat(double sampleRate, UInt32 channels)
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

struct Options {
    bool listDevices = false;
    bool hasDevice = false;
    AudioDeviceID device = kAudioObjectUnknown;
    UInt32 inputChannel = 0;
    UInt32 requestedBufferFrames = 64;
};

bool parseUnsigned(const char* text, UInt32& value)
{
    if (text == nullptr || *text == '\0' || *text == '-')
        return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (*end != '\0' || parsed > std::numeric_limits<UInt32>::max())
        return false;
    value = static_cast<UInt32>(parsed);
    return true;
}

void printUsage(const char* executable)
{
    std::cout << "Usage: " << executable << " [options]\n"
              << "  --list-devices          list available device IDs and channels\n"
              << "  --device-id ID          use one duplex device instead of default output\n"
              << "  --input-channel N       one-based interface input channel (default 1)\n"
              << "  --buffer-size FRAMES    request hardware buffer size (default 64)\n";
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--list-devices")
        {
            options.listDevices = true;
        }
        else if (argument == "--help" || argument == "-h")
        {
            printUsage(argv[0]);
            std::exit(0);
        }
        else if ((argument == "--device-id"
                  || argument == "--input-channel"
                  || argument == "--buffer-size")
                 && i + 1 < argc)
        {
            UInt32 parsed = 0;
            if (!parseUnsigned(argv[++i], parsed))
            {
                printUsage(argv[0]);
                std::exit(2);
            }
            if (argument == "--device-id")
            {
                options.device = static_cast<AudioDeviceID>(parsed);
                options.hasDevice = true;
            }
            else if (argument == "--input-channel")
            {
                if (parsed == 0)
                {
                    std::cerr << "Input channels are numbered from 1.\n";
                    std::exit(2);
                }
                options.inputChannel = parsed - 1;
            }
            else
            {
                if (parsed == 0)
                {
                    std::cerr << "Buffer size must be greater than zero.\n";
                    std::exit(2);
                }
                options.requestedBufferFrames = parsed;
            }
        }
        else
        {
            printUsage(argv[0]);
            std::exit(2);
        }
    }
    return options;
}

UInt32 configureBufferSize(AudioDeviceID device, UInt32 requested)
{
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
    if (AudioObjectGetPropertyData(device,
                                   &rangeAddress,
                                   0,
                                   nullptr,
                                   &rangeSize,
                                   &range) == noErr)
    {
        requested = std::clamp(requested,
                               static_cast<UInt32>(std::ceil(range.mMinimum)),
                               static_cast<UInt32>(std::floor(range.mMaximum)));
    }

    Boolean settable = false;
    if (AudioObjectIsPropertySettable(device, &address, &settable) == noErr && settable)
    {
        const OSStatus status = AudioObjectSetPropertyData(device,
                                                           &address,
                                                           0,
                                                           nullptr,
                                                           sizeof(requested),
                                                           &requested);
        if (status != noErr)
            std::cerr << "Warning: device rejected requested buffer size; using current value.\n";
    }

    UInt32 actual = 0;
    if (!getDeviceProperty(device,
                           kAudioDevicePropertyBufferFrameSize,
                           kAudioObjectPropertyScopeGlobal,
                           actual)
        || actual == 0)
    {
        std::cerr << "Could not obtain the active device buffer size.\n";
        std::exit(1);
    }
    return actual;
}

UInt32 latencyProperty(AudioDeviceID device,
                       AudioObjectPropertySelector selector,
                       AudioObjectPropertyScope scope)
{
    UInt32 value = 0;
    getDeviceProperty(device, selector, scope, value);
    return value;
}

struct AppState {
    AudioUnit unit = nullptr;
    DistortionPlusModel pedal;
    std::vector<Float32> inputScratch;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

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
        const std::uint64_t requiredBytes =
            static_cast<std::uint64_t>(frameCount)
            * current.mNumberChannels
            * sizeof(Float32);
        if (current.mData == nullptr
            || current.mNumberChannels == 0
            || requiredBytes > current.mDataByteSize)
        {
            return false;
        }
    }
    return true;
}

OSStatus renderCallback(void* refCon,
                        AudioUnitRenderActionFlags* flags,
                        const AudioTimeStamp* timeStamp,
                        UInt32,
                        UInt32 frameCount,
                        AudioBufferList* ioData)
{
    auto* state = static_cast<AppState*>(refCon);
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

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    for (UInt32 frame = 0; frame < frameCount; ++frame)
    {
        const float input = state->inputScratch[frame];
        const float output = state->pedal.processSample(input);
        if (std::isfinite(input))
            inputPeak = std::max(inputPeak, std::abs(input));
        outputPeak = std::max(outputPeak, std::abs(output));

        for (UInt32 buffer = 0; buffer < ioData->mNumberBuffers; ++buffer)
        {
            AudioBuffer& current = ioData->mBuffers[buffer];
            auto* samples = static_cast<Float32*>(current.mData);
            for (UInt32 channel = 0; channel < current.mNumberChannels; ++channel)
                samples[frame * current.mNumberChannels + channel] = output;
        }
    }

    state->inputPeak.store(inputPeak, std::memory_order_relaxed);
    state->outputPeak.store(outputPeak, std::memory_order_relaxed);
    return noErr;
}

void printControls(const AppState& state)
{
    std::cout
        << "\nCircuitPedal V0.2 — circuit-driven Distortion+ prototype\n"
        << "Distortion: " << static_cast<int>(state.pedal.getDistortion() * 100.0f) << "%\n"
        << "Output:     " << static_cast<int>(state.pedal.getOutput() * 100.0f) << "%\n"
        << "Bypass:     " << (state.pedal.getBypass() ? "ON" : "OFF") << "\n\n"
        << "Type a command, then press Enter:\n"
        << "  g / G  distortion down / up\n"
        << "  o / O  output down / up\n"
        << "  b      bypass\n"
        << "  m      show signal peaks\n"
        << "  q      quit\n\n";
}

void stopAudio(AppState& state)
{
    if (state.unit == nullptr)
        return;
    AudioOutputUnitStop(state.unit);
    AudioUnitUninitialize(state.unit);
    AudioComponentInstanceDispose(state.unit);
    state.unit = nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);
    if (options.listDevices)
    {
        listAudioDevices();
        return 0;
    }

    AppState state;
    AudioComponentDescription description {};
    description.componentType = kAudioUnitType_Output;
    description.componentSubType = kAudioUnitSubType_HALOutput;
    description.componentManufacturer = kAudioUnitManufacturer_Apple;

    const AudioComponent component = AudioComponentFindNext(nullptr, &description);
    if (component == nullptr)
    {
        std::cerr << "Could not find Apple's HAL Output Audio Unit.\n";
        return 1;
    }
    checkStatus(AudioComponentInstanceNew(component, &state.unit), "Create HAL Audio Unit");

    UInt32 enable = 1;
    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioOutputUnitProperty_EnableIO,
                                     kAudioUnitScope_Input,
                                     1,
                                     &enable,
                                     sizeof(enable)),
                "Enable audio input");

    const AudioDeviceID device = options.hasDevice ? options.device : defaultOutputDevice();
    const UInt32 inputChannels = deviceChannelCount(device, kAudioObjectPropertyScopeInput);
    const UInt32 deviceOutputChannels = deviceChannelCount(device, kAudioObjectPropertyScopeOutput);
    if (inputChannels == 0 || deviceOutputChannels == 0)
    {
        std::cerr << "Selected device must provide both input and output.\n"
                  << "Run with --list-devices and choose a duplex interface using --device-id.\n";
        stopAudio(state);
        return 1;
    }
    if (options.inputChannel >= inputChannels)
    {
        std::cerr << "Input channel " << (options.inputChannel + 1)
                  << " does not exist on the selected device.\n";
        stopAudio(state);
        return 1;
    }

    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioOutputUnitProperty_CurrentDevice,
                                     kAudioUnitScope_Global,
                                     0,
                                     &device,
                                     sizeof(device)),
                "Select audio device");

    const UInt32 bufferFrames = configureBufferSize(device, options.requestedBufferFrames);
    const double sampleRate = deviceSampleRate(device);
    const UInt32 clientOutputChannels = std::min<UInt32>(2, deviceOutputChannels);
    const auto inputFormat = floatFormat(sampleRate, 1);
    const auto outputFormat = floatFormat(sampleRate, clientOutputChannels);

    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioUnitProperty_StreamFormat,
                                     kAudioUnitScope_Output,
                                     1,
                                     &inputFormat,
                                     sizeof(inputFormat)),
                "Set client input format");
    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioUnitProperty_StreamFormat,
                                     kAudioUnitScope_Input,
                                     0,
                                     &outputFormat,
                                     sizeof(outputFormat)),
                "Set client output format");

    const SInt32 inputChannelMap = static_cast<SInt32>(options.inputChannel);
    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioOutputUnitProperty_ChannelMap,
                                     kAudioUnitScope_Output,
                                     1,
                                     &inputChannelMap,
                                     sizeof(inputChannelMap)),
                "Select interface input channel");

    UInt32 requestedMaximumFrames = std::max<UInt32>(4096, bufferFrames);
    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioUnitProperty_MaximumFramesPerSlice,
                                     kAudioUnitScope_Global,
                                     0,
                                     &requestedMaximumFrames,
                                     sizeof(requestedMaximumFrames)),
                "Set maximum frames per slice");
    UInt32 confirmedMaximumFrames = 0;
    UInt32 maximumFramesSize = sizeof(confirmedMaximumFrames);
    checkStatus(AudioUnitGetProperty(state.unit,
                                     kAudioUnitProperty_MaximumFramesPerSlice,
                                     kAudioUnitScope_Global,
                                     0,
                                     &confirmedMaximumFrames,
                                     &maximumFramesSize),
                "Get maximum frames per slice");
    if (confirmedMaximumFrames == 0 || confirmedMaximumFrames > 65536)
    {
        std::cerr << "Audio Unit returned an unreasonable maximum frame count.\n";
        stopAudio(state);
        return 1;
    }
    state.inputScratch.resize(confirmedMaximumFrames);

    AURenderCallbackStruct callback {};
    callback.inputProc = renderCallback;
    callback.inputProcRefCon = &state;
    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioUnitProperty_SetRenderCallback,
                                     kAudioUnitScope_Input,
                                     0,
                                     &callback,
                                     sizeof(callback)),
                "Install render callback");

    state.pedal.prepare(sampleRate);
    checkStatus(AudioUnitInitialize(state.unit), "Initialize audio unit");
    checkStatus(AudioOutputUnitStart(state.unit), "Start audio");

    const UInt32 inputLatency = latencyProperty(device,
                                                kAudioDevicePropertyLatency,
                                                kAudioObjectPropertyScopeInput);
    const UInt32 inputSafety = latencyProperty(device,
                                               kAudioDevicePropertySafetyOffset,
                                               kAudioObjectPropertyScopeInput);
    const UInt32 outputLatency = latencyProperty(device,
                                                 kAudioDevicePropertyLatency,
                                                 kAudioObjectPropertyScopeOutput);
    const UInt32 outputSafety = latencyProperty(device,
                                                kAudioDevicePropertySafetyOffset,
                                                kAudioObjectPropertyScopeOutput);

    std::cout << "Audio device:      " << deviceName(device) << " (ID " << device << ")\n"
              << "Input channel:     " << (options.inputChannel + 1) << '\n'
              << "Sample rate:       " << sampleRate << " Hz\n"
              << "Hardware buffer:   " << bufferFrames << " frames ("
              << (1000.0 * bufferFrames / sampleRate) << " ms per period)\n"
              << "Input latency:     " << inputLatency << " + " << inputSafety
              << " safety frames\n"
              << "Output latency:    " << outputLatency << " + " << outputSafety
              << " safety frames\n"
              << "DSP FIR delay:     " << circuitpedal::Oversampler4x::wetDelayHostSamples
              << " frames\n"
              << "Measured physical loopback is still required for true round-trip latency.\n"
              << "\nIMPORTANT: start with your interface/headphone/amp volume LOW.\n";
    printControls(state);

    std::string command;
    while (std::getline(std::cin, command))
    {
        if (command.empty())
            continue;

        switch (command[0])
        {
            case 'q':
                stopAudio(state);
                std::cout << "CircuitPedal stopped.\n";
                return 0;
            case 'g': state.pedal.setDistortion(state.pedal.getDistortion() - 0.05f); break;
            case 'G': state.pedal.setDistortion(state.pedal.getDistortion() + 0.05f); break;
            case 'o': state.pedal.setOutput(state.pedal.getOutput() - 0.05f); break;
            case 'O': state.pedal.setOutput(state.pedal.getOutput() + 0.05f); break;
            case 'b': state.pedal.setBypass(!state.pedal.getBypass()); break;
            case 'm':
                std::cout << "Input peak: " << state.inputPeak.load(std::memory_order_relaxed)
                          << "   Output peak: " << state.outputPeak.load(std::memory_order_relaxed)
                          << '\n';
                break;
            default: break;
        }
        printControls(state);
    }

    stopAudio(state);
    return 0;
}
