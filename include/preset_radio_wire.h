#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace mgo2::radio::wire {
// B31D60/B32098..B320B4 registers Game+104 with object channel 600,
// topology 0, flags 3 (reliable), selector 255. This is NOT channel 1.
inline constexpr uint16_t object_channel = 600;
enum class Kind : uint8_t { client_request = 1, host_notification = 2 };
struct Record {
    Kind kind = Kind::client_request;
    uint8_t slot = 0;
    uint8_t presetId = 0;
    uint8_t third = 2; // Observed menu argument; full semantics unresolved.
    bool operator==(const Record&) const = default;
};
// Only the 16 presets confirmed in the current default selection menu.
// Other original presets may exist; this is an evidence-bounded allowlist.
bool reviewed_id(uint8_t id) noexcept;
// Checked four-byte application body only (B55A48/B558D0). Neither function
// creates the original 12-byte stream/transport header, queues a packet,
// relays a message, resolves text/audio, or grants sender authority.
// In particular, a decoded request's slot MUST be checked against a separate
// admitted peer's full identity. Receiving kind2 alone does not prove HOST.
// Never send this bare body on channel 1: 1/2 mean leave/profile there.
std::array<uint8_t,4> encode(const Record&); // invalid_argument on unsupported input
Record decode(std::span<const uint8_t>); // runtime_error on unsupported bytes
}
