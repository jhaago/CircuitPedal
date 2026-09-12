#pragma once

#include "CircuitFile.h"
#include "DistortionPlusModel.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace circuitpedal {

struct AudioDeviceInfo {
    std::uint32_t id = 0;
    std::string name;
    std::uint32_t inputChannels = 0;
    std::uint32_t outputChannels = 0;

    bool isDuplex() const noexcept { return inputChannels > 0 && outputChannels > 0; }
};

struct AudioStartConfiguration {
    std::uint32_t deviceId = 0;
    std::uint32_t inputChannel = 0; // Zero-based.
    std::uint32_t requestedBufferFrames = 64;
};

struct AudioRuntimeInfo {
    double sampleRate = 0.0;
    std::uint32_t requestedBufferFrames = 0;
    std::uint32_t actualBufferFrames = 0;
    std::uint32_t inputLatencyFrames = 0;
    std::uint32_t inputSafetyOffsetFrames = 0;
    std::uint32_t outputLatencyFrames = 0;
    std::uint32_t outputSafetyOffsetFrames = 0;
    std::uint32_t dspDelayFrames = 0;

    double bufferDurationMilliseconds() const noexcept;
    double reportedLatencyMilliseconds() const noexcept;
};

// Device discovery and lifecycle methods belong on a non-real-time thread.
// Parameter setters and meter getters remain lock-free while audio is running.
class MacAudioEngine {
public:
    MacAudioEngine();
    ~MacAudioEngine();

    MacAudioEngine(const MacAudioEngine&) = delete;
    MacAudioEngine& operator=(const MacAudioEngine&) = delete;

    static std::vector<AudioDeviceInfo> enumerateDevices(std::string& error);
    static std::uint32_t defaultOutputDeviceId(std::string& error);

    bool start(const AudioStartConfiguration& configuration, std::string& error);
    void stop() noexcept;
    bool isRunning() const noexcept;

    // Model selection/loading belongs on the non-real-time thread and is only
    // allowed while audio is stopped.
    bool loadCircuitFile(const std::string& path, std::string& error);
    bool useBuiltInDistortionPlus() noexcept;
    bool usingCircuitFile() const noexcept;
    std::string activeModelName() const;
    std::vector<CircuitFileControl> circuitControls() const;
    bool setCircuitControl(std::size_t index, float normalized) noexcept;
    float circuitControl(std::size_t index) const noexcept;

    void setDistortion(float normalized) noexcept;
    void setOutput(float normalized) noexcept;
    void setInputTrimDb(float decibels) noexcept;
    void setMasterOutputDb(float decibels) noexcept;
    void setBypass(bool bypassed) noexcept;
    void setClippingDiodePreset(ClippingDiodePreset preset) noexcept;
    bool setCircuitParameters(const DistortionPlusCircuitParameters& parameters) noexcept;

    float distortion() const noexcept;
    float output() const noexcept;
    float inputTrimDb() const noexcept;
    float masterOutputDb() const noexcept;
    bool bypassed() const noexcept;
    ClippingDiodePreset clippingDiodePreset() const noexcept;
    DistortionPlusCircuitParameters circuitParameters() const noexcept;
    float inputPeak() const noexcept;
    float outputPeak() const noexcept;
    AudioRuntimeInfo runtimeInfo() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace circuitpedal
