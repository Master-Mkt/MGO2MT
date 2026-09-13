#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <mutex>

namespace mgo2win::clan {
struct Image {
 static constexpr unsigned width=32,height=32;
 // Native top-down BGRA. Original index zero is transparent; others are opaque.
 std::array<uint32_t,width*height> bgra{};
 bool operator==(const Image&)const=default;
};
// Nomad's original EMBD payload: 565 bytes. The all-zero absent-emblem payload
// is distinct from unsupported or malformed data, which raises runtime_error.
std::optional<Image> decode_blob(std::span<const uint8_t>);
// Current GameLobby 4B48 -> 4B49: BE32 result + the 565-byte saved emblem.
// Responses do not echo clan ID: the connection owner must correlate one request.
std::optional<Image> decode_reply(std::span<const uint8_t>);
struct State {uint32_t clan=0;uint64_t serial=0;std::optional<Image> image;};
class Cache {
 mutable std::mutex mutex_;State state_;
public:
 void want(uint32_t clan);
 uint32_t wanted()const;
 void put(uint32_t clan,std::optional<Image> image);
 State state()const;
};
class Bitmap {
 HBITMAP value_=nullptr;
public:
 explicit Bitmap(const Image&);
 ~Bitmap();Bitmap(const Bitmap&)=delete;Bitmap&operator=(const Bitmap&)=delete;
 HBITMAP get()const{return value_;}
};
}
