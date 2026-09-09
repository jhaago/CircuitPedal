#include "MacAudioEngine.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

struct Options {
    bool listDevices = false;
    bool hasDevice = false;
    std::uint32_t deviceId = 0;
    std::uint32_t inputChannel = 0;
    std::uint32_t requestedBufferFrames = 64;
};

bool parseUnsigned(const char* text, std::uint32_t& value)
{
    if (text == nullptr || *text == '\0' || *text == '-')
        return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (*end != '\0' || parsed > std::numeric_limits<std::uint32_t>::max())
        return false;
    value = static_cast<std::uint32_t>(parsed);
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
            std::uint32_t parsed = 0;
            if (!parseUnsigned(argv[++i], parsed))
            {
                printUsage(argv[0]);
                std::exit(2);
            }
            if (argument == "--device-id")
            {
                options.deviceId = parsed;
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

void printDevices(const std::vector<circuitpedal::AudioDeviceInfo>& devices)
{
    std::cout << "Available Core Audio devices:\n";
    for (const auto& device : devices)
    {
        std::cout << "  ID " << device.id
                  << "  inputs=" << device.inputChannels
                  << "  outputs=" << device.outputChannels
                  << "  " << device.name;
        if (!device.isDuplex())
            std::cout << " (not duplex)";
        std::cout << '\n';
    }
}

void printControls(const circuitpedal::MacAudioEngine& engine)
{
    std::cout
        << "\nCircuitPedal V0.3 — circuit-driven Distortion+ prototype\n"
        << "Distortion: " << static_cast<int>(engine.distortion() * 100.0f) << "%\n"
        << "Output:     " << static_cast<int>(engine.output() * 100.0f) << "%\n"
        << "Bypass:     " << (engine.bypassed() ? "ON" : "OFF") << "\n\n"
        << "Type a command, then press Enter:\n"
        << "  g / G  distortion down / up\n"
        << "  o / O  output down / up\n"
        << "  b      bypass\n"
        << "  m      show signal peaks\n"
        << "  q      quit\n\n";
}

} // namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);
    std::string error;
    const auto devices = circuitpedal::MacAudioEngine::enumerateDevices(error);
    if (!error.empty())
    {
        std::cerr << error << '\n';
        return 1;
    }
    if (options.listDevices)
    {
        printDevices(devices);
        return 0;
    }

    std::uint32_t deviceId = options.deviceId;
    if (!options.hasDevice)
    {
        deviceId = circuitpedal::MacAudioEngine::defaultOutputDeviceId(error);
        if (!error.empty())
        {
            std::cerr << error << '\n';
            return 1;
        }
    }

    circuitpedal::MacAudioEngine engine;
    circuitpedal::AudioStartConfiguration configuration;
    configuration.deviceId = deviceId;
    configuration.inputChannel = options.inputChannel;
    configuration.requestedBufferFrames = options.requestedBufferFrames;
    if (!engine.start(configuration, error))
    {
        std::cerr << error << '\n'
                  << "Run with --list-devices and choose a duplex interface using --device-id.\n";
        return 1;
    }

    const auto selected = std::find_if(devices.begin(), devices.end(),
        [deviceId](const circuitpedal::AudioDeviceInfo& device) {
            return device.id == deviceId;
        });
    const auto info = engine.runtimeInfo();
    std::cout << "Audio device:      "
              << (selected != devices.end() ? selected->name : "Unknown audio device")
              << " (ID " << deviceId << ")\n"
              << "Input channel:     " << (options.inputChannel + 1) << '\n'
              << "Sample rate:       " << info.sampleRate << " Hz\n"
              << "Requested buffer:  " << info.requestedBufferFrames << " frames\n"
              << "Actual buffer:     " << info.actualBufferFrames << " frames ("
              << info.bufferDurationMilliseconds() << " ms per period)\n"
              << "Input latency:     " << info.inputLatencyFrames << " + "
              << info.inputSafetyOffsetFrames << " safety frames\n"
              << "Output latency:    " << info.outputLatencyFrames << " + "
              << info.outputSafetyOffsetFrames << " safety frames\n"
              << "DSP FIR delay:     " << info.dspDelayFrames << " frames\n"
              << "Reported component sum: " << info.reportedLatencyMilliseconds() << " ms\n"
              << "Physical loopback is still required for true round-trip latency.\n"
              << "\nIMPORTANT: start with your interface/headphone/amp volume LOW.\n";
    printControls(engine);

    std::string command;
    while (std::getline(std::cin, command))
    {
        if (command.empty())
            continue;
        switch (command[0])
        {
            case 'q':
                engine.stop();
                std::cout << "CircuitPedal stopped.\n";
                return 0;
            case 'g': engine.setDistortion(engine.distortion() - 0.05f); break;
            case 'G': engine.setDistortion(engine.distortion() + 0.05f); break;
            case 'o': engine.setOutput(engine.output() - 0.05f); break;
            case 'O': engine.setOutput(engine.output() + 0.05f); break;
            case 'b': engine.setBypass(!engine.bypassed()); break;
            case 'm':
                std::cout << "Input peak: " << engine.inputPeak()
                          << "   Output peak: " << engine.outputPeak() << '\n';
                break;
            default: break;
        }
        printControls(engine);
    }

    engine.stop();
    return 0;
}
