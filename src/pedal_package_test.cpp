#include "PedalPackage.h"

#include <filesystem>
#include <iostream>
#include <string>

#ifndef CIRCUITPEDAL_SOURCE_DIR
#define CIRCUITPEDAL_SOURCE_DIR "."
#endif

namespace {

bool expect(bool condition, const std::string& message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

} // namespace

int main()
{
    const std::filesystem::path sourceRoot(CIRCUITPEDAL_SOURCE_DIR);
    const std::filesystem::path manifestPath =
        sourceRoot / "pedals" / "woolly_mammoth" / "pedal.json";

    circuitpedal::PedalPackageManifest manifest;
    std::string error;
    if (!circuitpedal::loadPedalPackageManifest(manifestPath.string(), manifest, error))
    {
        std::cerr << "FAIL: could not load Woolly package: " << error << '\n';
        return 1;
    }

    bool ok = true;
    ok &= expect(manifest.id == "woolly_mammoth", "package id");
    ok &= expect(manifest.displayName == "Woolly Mammoth", "display name");
    ok &= expect(manifest.category == "fuzz", "category");
    ok &= expect(manifest.controls.size() == 4U, "four overlay controls");
    ok &= expect(manifest.controls[0].id == "OUTPUT", "OUTPUT is first control");
    ok &= expect(manifest.controls[3].id == "WOOL", "WOOL is fourth control");
    ok &= expect(manifest.controls[0].x > 0.0 && manifest.controls[0].x < 1.0,
                 "normalized x coordinate");
    ok &= expect(manifest.controls[0].y > 0.0 && manifest.controls[0].y < 1.0,
                 "normalized y coordinate");
    ok &= expect(manifest.presets.size() == 1U,
                 "default preset declared");

    const std::filesystem::path circuitPath(
        circuitpedal::resolvePedalPackagePath(manifest, manifest.circuitFile));
    ok &= expect(circuitPath.filename() == "woolly_mammoth_reference_draft.cpedal",
                 "circuit path resolves to existing model name");
    ok &= expect(std::filesystem::exists(circuitPath),
                 "package resolves to the authoritative Woolly circuit file");

    const std::filesystem::path presetPath(
        circuitpedal::resolvePedalPackagePath(manifest, manifest.presets.front()));
    ok &= expect(std::filesystem::exists(presetPath),
                 "default preset path resolves");

    circuitpedal::PedalPackageManifest invalid = manifest;
    invalid.controls[1].x = 1.5;
    error.clear();
    ok &= expect(!circuitpedal::validatePedalPackageManifest(invalid, error),
                 "out-of-range control coordinates are rejected");

    invalid = manifest;
    invalid.controls[1].id = invalid.controls[0].id;
    error.clear();
    ok &= expect(!circuitpedal::validatePedalPackageManifest(invalid, error),
                 "duplicate control ids are rejected");

    if (!ok)
        return 1;

    std::cout << "Pedal package validation passed for "
              << manifest.displayName << '\n';
    return 0;
}
