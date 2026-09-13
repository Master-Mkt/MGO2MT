#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace mgo2win::clan {
struct Image {
    static constexpr unsigned width=64,height=64;
    // Top-down, premultiplied BGRA for AC_SRC_ALPHA, not raw RGBA wire bytes.
    std::array<uint32_t,width*height> bgra{};
    bool operator==(const Image&)const=default;
};
Image image_from_rgba(std::span<const uint8_t>);
// Incremental published EM64 download: one 4B4A request / 4B4B reply per chunk.
// Framing/opcode dispatch belongs to the existing authenticated connection.
class Download {
    uint32_t clan_=0;
    bool done_=false;
    std::array<uint8_t,32> digest_{};
    std::vector<uint8_t> bytes_;
    std::optional<Image> image_;
public:
    void begin(uint32_t clan);
    void cancel();
    uint32_t clan()const{return clan_;}
    std::vector<uint8_t> requestPayload()const;
    bool accept(std::span<const uint8_t> payload);
    bool done()const{return done_;}
    std::optional<Image> image()const{return image_;}
};
struct State {uint32_t clan=0;uint64_t serial=0;std::optional<Image> image;};
class Cache {
    mutable std::mutex mutex_;
    State state_;
public:
    void want(uint32_t clan);
    uint32_t wanted()const;
    void put(uint32_t clan,std::optional<Image>);
    State state()const;
};
class Bitmap {
    HBITMAP bitmap_=nullptr;
public:
    explicit Bitmap(const Image&);
    ~Bitmap();
    Bitmap(const Bitmap&)=delete;
    Bitmap& operator=(const Bitmap&)=delete;
    HBITMAP get()const{return bitmap_;}
};
}
