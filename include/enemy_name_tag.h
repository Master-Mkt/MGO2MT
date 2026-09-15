#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <optional>

namespace mgo2win::hud {
struct EnemyVitals {std::optional<uint8_t> level;uint32_t hp=0,maxHp=0;};
// Presentation only. The caller selects an eligible enemy and supplies its
// projected head position. Paint after menu color-key/alpha finalization.
class EnemyNameTagRenderer {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    EnemyNameTagRenderer();
    ~EnemyNameTagRenderer();
    EnemyNameTagRenderer(const EnemyNameTagRenderer&)=delete;
    EnemyNameTagRenderer& operator=(const EnemyNameTagRenderer&)=delete;
    // Anchor is the panel's bottom center. Optional clan is a decoded top-down
    // premultiplied BGRA 32x32 or 64x64 DIB; its on-screen size is32x32.
    // Invalid/missing clan images are omitted. Invalid/empty UTF8 names or a
    // fully clipped panel produce no pixels and return false.
    bool paint(std::span<uint32_t> finalStraightBgra,int width,int height,
               int anchorX,int anchorY,std::string_view utf8Name,HBITMAP clan=nullptr,
               std::optional<EnemyVitals> vitals=std::nullopt);
};
}
