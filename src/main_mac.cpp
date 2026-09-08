#include "DistortionPlusModel.h"

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using circuitpedal::DistortionPlusModel;

namespace {

void checkStatus(OSStatus status, const char* what)
{
    if (status == noErr)
        return;

    char code[5] = {};
    UInt32 raw = CFSwapInt32HostToBig(static_cast<UInt32>(status));
    std::memcpy(code, &raw, 4);
    std::cerr << what << " failed (OSStatus " << status;
    if (std::isprint(code[0]) && std::isprint(code[1]) && std::isprint(code[2]) && std::isprint(code[3]))
        std::cerr << ", '" << code << "'";
    std::cerr << ")\n";
    std::exit(1);
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
    checkStatus(AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, &device),
                "Get default output device");
    return device;
}

double deviceSampleRate(AudioDeviceID device)
{
    Float64 rate = 48000.0;
    UInt32 size = sizeof(rate);
    AudioObjectPropertyAddress address {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    checkStatus(AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &rate),
                "Get device sample rate");
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
    if (AudioObjectGetPropertyData(device, &address, 0, nullptr, &size, &name) != noErr || name == nullptr)
        return "Unknown audio device";

    char buffer[256] = {};
    CFStringGetCString(name, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    CFRelease(name);
    return buffer;
}

AudioStreamBasicDescription floatFormat(double sampleRate, UInt32 channels)
{
    AudioStreamBasicDescription f {};
    f.mSampleRate = sampleRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
    f.mBytesPerPacket = sizeof(Float32) * channels;
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = sizeof(Float32) * channels;
    f.mChannelsPerFrame = channels;
    f.mBitsPerChannel = 8 * sizeof(Float32);
    return f;
}

struct AppState {
    AudioUnit unit = nullptr;
    DistortionPlusModel pedal;
    std::vector<Float32> inputScratch;
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
};

OSStatus renderCallback(void* refCon,
                        AudioUnitRenderActionFlags* flags,
                        const AudioTimeStamp* timeStamp,
                        UInt32,
                        UInt32 frameCount,
                        AudioBufferList* ioData)
{
    auto* state = static_cast<AppState*>(refCon);

    if (frameCount > state->inputScratch.size())
    {
        // Never allocate on the real-time thread. Fail silent for this block.
        for (UInt32 b = 0; b < ioData->mNumberBuffers; ++b)
            std::memset(ioData->mBuffers[b].mData, 0, ioData->mBuffers[b].mDataByteSize);
        return noErr;
    }

    AudioBufferList inData {};
    inData.mNumberBuffers = 1;
    inData.mBuffers[0].mNumberChannels = 1;
    inData.mBuffers[0].mDataByteSize = frameCount * sizeof(Float32);
    inData.mBuffers[0].mData = state->inputScratch.data();

    OSStatus status = AudioUnitRender(state->unit, flags, timeStamp, 1, frameCount, &inData);
    if (status != noErr)
    {
        for (UInt32 b = 0; b < ioData->mNumberBuffers; ++b)
            std::memset(ioData->mBuffers[b].mData, 0, ioData->mBuffers[b].mDataByteSize);
        return status;
    }

    float inPeak = 0.0f;
    float outPeak = 0.0f;

    // We request interleaved stereo output. HAL normally supplies one buffer.
    if (ioData->mNumberBuffers == 1 && ioData->mBuffers[0].mNumberChannels >= 2)
    {
        auto* out = static_cast<Float32*>(ioData->mBuffers[0].mData);
        for (UInt32 i = 0; i < frameCount; ++i)
        {
            const float x = state->inputScratch[i];
            const float y = state->pedal.processSample(x);
            inPeak = std::max(inPeak, std::abs(x));
            outPeak = std::max(outPeak, std::abs(y));
            out[i * 2] = y;
            out[i * 2 + 1] = y;
        }
    }
    else
    {
        // Defensive path for a non-interleaved layout.
        for (UInt32 i = 0; i < frameCount; ++i)
        {
            const float x = state->inputScratch[i];
            const float y = state->pedal.processSample(x);
            inPeak = std::max(inPeak, std::abs(x));
            outPeak = std::max(outPeak, std::abs(y));
            for (UInt32 b = 0; b < ioData->mNumberBuffers; ++b)
            {
                auto* out = static_cast<Float32*>(ioData->mBuffers[b].mData);
                out[i] = y;
            }
        }
    }

    state->inputPeak.store(inPeak, std::memory_order_relaxed);
    state->outputPeak.store(outPeak, std::memory_order_relaxed);
    return noErr;
}

void printControls(const AppState& state)
{
    std::cout
        << "\nCircuitPedal V0.1 — component-driven Distortion+ prototype\n"
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

} // namespace

int main()
{
    AppState state;
    state.inputScratch.resize(8192);

    AudioComponentDescription desc {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
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

    AudioDeviceID device = defaultOutputDevice();
    checkStatus(AudioUnitSetProperty(state.unit,
                                     kAudioOutputUnitProperty_CurrentDevice,
                                     kAudioUnitScope_Global,
                                     0,
                                     &device,
                                     sizeof(device)),
                "Select default audio device");

    const double rate = deviceSampleRate(device);
    const auto inputFormat = floatFormat(rate, 1);
    const auto outputFormat = floatFormat(rate, 2);

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

    UInt32 maxFrames = static_cast<UInt32>(state.inputScratch.size());
    AudioUnitSetProperty(state.unit,
                         kAudioUnitProperty_MaximumFramesPerSlice,
                         kAudioUnitScope_Global,
                         0,
                         &maxFrames,
                         sizeof(maxFrames));

    state.pedal.prepare(rate);

    checkStatus(AudioUnitInitialize(state.unit), "Initialize audio unit");
    checkStatus(AudioOutputUnitStart(state.unit), "Start audio");

    std::cout << "Audio device: " << deviceName(device) << "\n";
    std::cout << "Sample rate:  " << rate << " Hz\n";
    std::cout << "\nIMPORTANT: start with your interface/headphone/amp volume LOW.\n";
    std::cout << "V0.1 expects one interface to be used for both guitar input and output.\n";
    printControls(state);

    std::string command;
    while (std::getline(std::cin, command))
    {
        if (command.empty())
            continue;

        switch (command[0])
        {
            case 'q':
                AudioOutputUnitStop(state.unit);
                AudioUnitUninitialize(state.unit);
                AudioComponentInstanceDispose(state.unit);
                std::cout << "CircuitPedal stopped.\n";
                return 0;
            case 'g': state.pedal.setDistortion(state.pedal.getDistortion() - 0.05f); break;
            case 'G': state.pedal.setDistortion(state.pedal.getDistortion() + 0.05f); break;
            case 'o': state.pedal.setOutput(state.pedal.getOutput() - 0.05f); break;
            case 'O': state.pedal.setOutput(state.pedal.getOutput() + 0.05f); break;
            case 'b': state.pedal.setBypass(!state.pedal.getBypass()); break;
            case 'm':
                std::cout << "Input peak: " << state.inputPeak.load()
                          << "   Output peak: " << state.outputPeak.load() << "\n";
                break;
            default: break;
        }
        printControls(state);
    }

    AudioOutputUnitStop(state.unit);
    AudioUnitUninitialize(state.unit);
    AudioComponentInstanceDispose(state.unit);
    return 0;
}
