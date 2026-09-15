#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mgo2win::motion_blend {
// Native transition progress per second, NOT a per-frame blend weight.
// This user setting belongs in motion_blend.cfg, outside the asset manifest.
struct Settings {
 static constexpr unsigned minimum=10,maximum=2000,defaultPercentPerSecond=500;
 unsigned percentPerSecond=defaultPercentPerSecond;
 bool valid()const noexcept;
 bool set(unsigned value)noexcept; // Invalid input preserves the current value.
 double rate_per_second()const; // 500%/s -> normalized progress 5.0/s.
 double completion_seconds()const; // Time for progress 0 -> 1; default 0.2s.
 bool operator==(const Settings&)const=default;
};
std::string encode(const Settings&);
std::optional<Settings> decode(std::string_view)noexcept;
// Missing/invalid/unreadable files return the enabled default; never rewrite them.
Settings load(const std::filesystem::path&,bool* usedFallback=nullptr)noexcept;
// Exclusive temporary file + flush + atomic replacement. Failure preserves old data.
bool save(const std::filesystem::path&,const Settings&)noexcept;
}
