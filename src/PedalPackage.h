#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace circuitpedal {

struct PedalPackageAssets {
    std::string faceplate;
    std::string thumbnail;
    std::string icon;
};

struct PedalPackageControl {
    std::string id;
    std::string label;
    std::string type;
    double x = 0.5;
    double y = 0.5;
    double size = 1.0;
};

struct PedalPackageManifest {
    std::string manifestPath;
    std::string packageDirectory;

    std::string id;
    std::string displayName;
    std::string modelStatus;
    std::string category;
    std::string inspiredBy;
    std::string description;

    PedalPackageAssets assets;
    std::string circuitFile;
    double faceplateAspectRatio = 1.6;
    std::string controlStyle = "knob_overlay";

    std::vector<PedalPackageControl> controls;
    std::vector<std::string> presets;
};

// Loads and validates a CircuitPedal package manifest. The parser intentionally
// implements JSON locally so the core package format remains usable without
// platform frameworks or an online dependency.
bool loadPedalPackageManifest(const std::string& manifestPath,
                              PedalPackageManifest& manifest,
                              std::string& error);

// Resolves a package-relative path against the directory containing pedal.json.
// Absolute paths are returned unchanged.
std::string resolvePedalPackagePath(const PedalPackageManifest& manifest,
                                    const std::string& relativePath);

// Performs schema-level validation without requiring referenced files to exist.
bool validatePedalPackageManifest(const PedalPackageManifest& manifest,
                                  std::string& error);

} // namespace circuitpedal
