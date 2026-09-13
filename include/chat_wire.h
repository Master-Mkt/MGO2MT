#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mgo2win::chat {
inline constexpr uint16_t send_opcode = 0x4400;
inline constexpr uint16_t receive_opcode = 0x4401;
inline constexpr uint16_t capability_request_opcode = 0x44e0;
inline constexpr uint16_t capability_reply_opcode = 0x44e1;
inline constexpr size_t request_bytes = 129;
inline constexpr size_t max_text_bytes = 126;
// Nomad JP uses UTF-8; ENG uses ISO-8859-1. No wire negotiation exists.
enum class Encoding { utf8, latin1 };
struct Capability {
    uint8_t status = 0; // 0 OK, 1 malformed, 2 session, 3 membership.
    Encoding encoding = Encoding::utf8; // Valid only when status == 0.
    uint8_t flags = 0; // bit0 room, bit1 team; no other bits in v1.
    uint64_t nonce = 0;
    uint32_t room = 0, character = 0;
};
std::vector<uint8_t> capability_payload(uint64_t nonce, uint32_t room, uint32_t character);
Capability capability_reply(std::span<const uint8_t> payload);
struct Message {
    uint32_t character = 0;
    uint8_t mode = 0; // Numeric value 0..4 (wire is ASCII '0'..'4').
    std::string text; // Always UTF-8, including when the wire uses Latin-1.
};
// Room/all only: route 0, mode '0', 127-byte NUL-padded text field.
// Throws invalid_argument on empty/control/malformed/unrepresentable/long text.
// Authorization, room lifetime and slash-command policy belong to the caller.
std::vector<uint8_t> room_payload(std::string_view text, Encoding encoding);
// Throws runtime_error on malformed payload. Never infers sender authentication,
// channel delivery, ACK or a roster name from text/mode alone.
Message receive_payload(std::span<const uint8_t> payload, Encoding encoding);
}
