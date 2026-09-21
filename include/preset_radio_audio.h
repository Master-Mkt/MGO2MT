#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
namespace mgo2mt::radio_audio {
struct CuePair { uint32_t self{}, remote{}; friend bool operator==(const CuePair&,const CuePair&)=default; };
struct AppearanceVoice { unsigned type{}, pitchByte{}; };
// Original B1A348 ordinary actor branch; only the 16 native default presets.
// The type23 special 7FCCF8 branch has different cues and is deliberately absent.
std::optional<CuePair> resolve(unsigned voiceType,unsigned presetId,bool specialType23=false) noexcept;
// Input is original wire appearance (27 or 28 bytes), not creation draft indices.
std::optional<AppearanceVoice> appearance_voice(std::span<const uint8_t>) noexcept;
// Original 7F74D0 domain: types0..25, pitchByte0..30. Unsupported values fail.
// Native std::pow approximates the original finite-positive power routine.
std::optional<float> pitch_ratio(unsigned voiceType,unsigned pitchByte) noexcept;
// root is the data/radio directory. Only supported ordinary preset cues accepted.
std::filesystem::path asset_path(const std::filesystem::path& root,uint32_t cue);
}
