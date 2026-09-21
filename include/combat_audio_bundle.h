#pragma once
#include "combat_audio.h"
#include <algorithm>
#include <optional>
#include <vector>

namespace mgo2mt::combat {
// Input is the sfx directory. Returned paths are relative to the containing data root.
// Effects performs the same bounded PCM, manifest, duplicate and safe-path validation
// used by runtime playback. Nothing from an unvalidated manifest is returned.
inline std::optional<std::vector<std::filesystem::path>> audio_bundle_files(
    const std::filesystem::path& sfxDirectory) {
    try {
        Effects checked;
        if (!checked.load(sfxDirectory)) return std::nullopt;
        const auto& files = checked.files();
        if (std::filesystem::exists(sfxDirectory / "ak102_10002_v0.wav") && !files.contains(10002)) return std::nullopt;
        const auto bound = [&](uint32_t cue, const char* name) {
            const auto it = files.find(cue);
            return it != files.end() && it->second.filename() == name;
        };
        if (!bound(1369, "body_impact_1369_v0.wav") || !bound(8168, "body_impact_8168_v0.wav")
            || (files.contains(10002) && !bound(10002, "ak102_10002_v0.wav")))
            return std::nullopt;
        std::vector<std::filesystem::path> result{std::filesystem::path("sfx") / "combat.txt"};
        for (const auto& [cue, file] : files) {
            (void)cue;
            const auto relative = std::filesystem::path("sfx") / file.filename();
            // Distinct cue bindings may share a validated file; inventory each file once.
            if (std::find(result.begin(), result.end(), relative) == result.end()) result.push_back(relative);
        }
        return result;
    } catch (...) { return std::nullopt; }
}
}
