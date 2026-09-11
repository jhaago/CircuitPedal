#include "PedalPackage.h"

#include <filesystem>
#include <fstream>
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

bool writeText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    output << text;
    return static_cast<bool>(output);
}

bool validatePackageLibrary(const std::filesystem::path& pedalsRoot)
{
    bool ok = true;
    std::size_t packageCount = 0U;

    if (!std::filesystem::exists(pedalsRoot))
        return expect(false, "pedals directory exists");

    for (const auto& entry : std::filesystem::directory_iterator(pedalsRoot))
    {
        if (!entry.is_directory())
            continue;

        const std::filesystem::path manifestPath = entry.path() / "pedal.json";
        if (!std::filesystem::exists(manifestPath))
            continue;

        ++packageCount;
        circuitpedal::PedalPackageManifest manifest;
        std::string error;
        ok &= expect(circuitpedal::loadPedalPackageManifest(
                         manifestPath.string(), manifest, error),
                     "package manifest loads: " + manifestPath.string() +
                         (error.empty() ? std::string() : " (" + error + ")"));
        if (!error.empty())
            continue;

        const std::filesystem::path circuitPath(
            circuitpedal::resolvePedalPackagePath(manifest, manifest.circuitFile));
        ok &= expect(std::filesystem::exists(circuitPath),
                     "package circuit exists: " + manifest.id);

        for (const auto& preset : manifest.presets)
        {
            const std::filesystem::path presetPath(
                circuitpedal::resolvePedalPackagePath(manifest, preset));
            ok &= expect(std::filesystem::exists(presetPath),
                         "package preset exists: " + manifest.id + " -> " + preset);
        }
    }

    ok &= expect(packageCount > 0U, "at least one pedal package is discovered");
    return ok;
}

} // namespace

int main()
{
    const std::filesystem::path sourceRoot(CIRCUITPEDAL_SOURCE_DIR);
    const std::filesystem::path pedalsRoot = sourceRoot / "pedals";
    const std::filesystem::path manifestPath =
        pedalsRoot / "woolly_mammoth" / "pedal.json";

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

    ok &= validatePackageLibrary(pedalsRoot);

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

    invalid = manifest;
    invalid.id = "Woolly Mammoth";
    error.clear();
    ok &= expect(!circuitpedal::validatePedalPackageManifest(invalid, error),
                 "non-snake-case package ids are rejected");

    invalid = manifest;
    invalid.controls[0].type = "fader";
    error.clear();
    ok &= expect(!circuitpedal::validatePedalPackageManifest(invalid, error),
                 "unsupported control types are rejected");

    invalid = manifest;
    invalid.controls[0].size = 0.0;
    error.clear();
    ok &= expect(!circuitpedal::validatePedalPackageManifest(invalid, error),
                 "non-positive control sizes are rejected");

    invalid = manifest;
    invalid.faceplateAspectRatio = 20.0;
    error.clear();
    ok &= expect(!circuitpedal::validatePedalPackageManifest(invalid, error),
                 "unreasonable faceplate aspect ratios are rejected");

    const std::filesystem::path tempRoot =
        std::filesystem::temp_directory_path() / "circuitpedal-package-test";
    std::error_code ec;
    std::filesystem::remove_all(tempRoot, ec);
    std::filesystem::create_directories(tempRoot, ec);
    ok &= expect(!ec, "temporary package test directory can be created");

    const std::filesystem::path malformed = tempRoot / "malformed.json";
    ok &= expect(writeText(malformed, "{not valid json"),
                 "malformed test manifest can be written");
    circuitpedal::PedalPackageManifest scratch;
    error.clear();
    ok &= expect(!circuitpedal::loadPedalPackageManifest(
                     malformed.string(), scratch, error),
                 "malformed JSON is rejected");

    const std::filesystem::path missingCircuit = tempRoot / "missing-circuit.json";
    ok &= expect(writeText(missingCircuit,
                           "{\"id\":\"test_pedal\",\"displayName\":\"Test\"}"),
                 "missing-circuit test manifest can be written");
    error.clear();
    ok &= expect(!circuitpedal::loadPedalPackageManifest(
                     missingCircuit.string(), scratch, error),
                 "manifest without circuit.file is rejected");

    error.clear();
    ok &= expect(!circuitpedal::loadPedalPackageManifest(
                     (tempRoot / "does-not-exist.json").string(), scratch, error),
                 "missing manifest file is rejected");

    std::filesystem::remove_all(tempRoot, ec);

    if (!ok)
        return 1;

    std::cout << "Pedal package validation passed for "
              << manifest.displayName << " and all discovered packages\n";
    return 0;
}
