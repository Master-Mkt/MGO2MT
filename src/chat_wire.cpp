#include "chat_wire.h"
#include <algorithm>
#include <stdexcept>

namespace mgo2win::chat {
namespace {
bool next(std::string_view s, size_t& i, uint32_t& cp) {
    const auto byte = [&](size_t at) { return static_cast<uint8_t>(s[at]); };
    const auto first = byte(i++);
    if (first < 0x80) { cp = first; return true; }
    unsigned extra = 0; uint32_t minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { extra = 1; cp = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { extra = 2; cp = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { extra = 3; cp = first & 7; minimum = 0x10000; }
    else return false;
    if (s.size() - i < extra) return false;
    while (extra--) {
        const auto b = byte(i++);
        if ((b & 0xc0) != 0x80) return false;
        cp = (cp << 6) | (b & 63);
    }
    return cp >= minimum && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}
bool utf8(std::string_view s) {
    size_t i = 0; uint32_t cp = 0;
    while (i < s.size()) if (!next(s, i, cp)) return false;
    return true;
}
void encoding_valid(Encoding encoding) {
    if (encoding != Encoding::utf8 && encoding != Encoding::latin1)
        throw std::invalid_argument("unknown chat encoding");
}
}

std::vector<uint8_t> room_payload(std::string_view text, Encoding encoding) {
    encoding_valid(encoding);
    if (text.empty()) throw std::invalid_argument("empty chat text");
    std::string wire;
    size_t i = 0; uint32_t cp = 0;
    while (i < text.size()) {
        const auto start = i;
        if (!next(text, i, cp)) throw std::invalid_argument("malformed chat UTF-8");
        if (cp < 0x20 || cp == 0x7f) throw std::invalid_argument("chat control character");
        if (encoding == Encoding::latin1) {
            if (cp > 0xff) throw std::invalid_argument("chat text is not Latin-1");
            wire.push_back(static_cast<char>(cp));
        } else wire.append(text.substr(start, i - start));
        if (wire.size() > max_text_bytes) throw std::invalid_argument("chat text exceeds wire limit");
    }
    std::vector<uint8_t> result(request_bytes, 0);
    result[1] = '0';
    std::copy(wire.begin(), wire.end(), result.begin() + 2);
    return result;
}

Message receive_payload(std::span<const uint8_t> payload, Encoding encoding) {
    encoding_valid(encoding);
    // The server's normal/system replies use variable zero padding. The outer
    // packet limit is 0x3ff, not the request's fixed 129-byte length.
    if (payload.size() < 6 || payload.size() > 0x3ff)
        throw std::runtime_error("invalid chat reply length");
    if (payload[4] < '0' || payload[4] > '4')
        throw std::runtime_error("invalid chat reply mode");
    auto body = payload.subspan(5);
    const auto nul = std::find(body.begin(), body.end(), uint8_t{0});
    if (nul == body.end() || std::any_of(nul, body.end(), [](uint8_t b) { return b != 0; }))
        throw std::runtime_error("invalid chat reply terminator or padding");
    Message result;
    result.character = (uint32_t(payload[0]) << 24) | (uint32_t(payload[1]) << 16)
                     | (uint32_t(payload[2]) << 8) | uint32_t(payload[3]);
    result.mode = payload[4] - '0';
    if (encoding == Encoding::utf8) {
        result.text.assign(body.begin(), nul);
        if (!utf8(result.text)) throw std::runtime_error("malformed chat reply UTF-8");
    } else {
        for (auto p = body.begin(); p != nul; ++p) {
            if (*p < 0x80) result.text.push_back(static_cast<char>(*p));
            else {
                result.text.push_back(static_cast<char>(0xc0 | (*p >> 6)));
                result.text.push_back(static_cast<char>(0x80 | (*p & 63)));
            }
        }
    }
    return result;
}

std::vector<uint8_t> capability_payload(uint64_t nonce, uint32_t room, uint32_t character) {
    if (!nonce || !room || room > 0x7fffffff || !character || character > 0x7fffffff)
        throw std::invalid_argument("invalid chat capability identity");
    std::vector<uint8_t> out(24, 0);
    out[0] = 'G'; out[1] = 'W'; out[2] = 'C'; out[3] = 'H'; out[4] = 1; out[5] = 1;
    for (unsigned i = 0; i < 8; ++i) out[8 + i] = uint8_t(nonce >> ((7 - i) * 8));
    for (unsigned i = 0; i < 4; ++i) {
        out[16 + i] = uint8_t(room >> ((3 - i) * 8));
        out[20 + i] = uint8_t(character >> ((3 - i) * 8));
    }
    return out;
}

Capability capability_reply(std::span<const uint8_t> bytes) {
    if (bytes.size() != 28 || bytes[0] != 'G' || bytes[1] != 'W' || bytes[2] != 'C'
        || bytes[3] != 'H' || bytes[4] != 1 || bytes[5] > 3
        || bytes[24] || bytes[25] || bytes[26] || bytes[27])
        throw std::runtime_error("invalid chat capability reply");
    Capability result;
    result.status = bytes[5]; result.flags = bytes[7];
    for (unsigned i = 8; i != 16; ++i) result.nonce = (result.nonce << 8) | bytes[i];
    for (unsigned i = 16; i != 20; ++i) result.room = (result.room << 8) | bytes[i];
    for (unsigned i = 20; i != 24; ++i) result.character = (result.character << 8) | bytes[i];
    if (!result.status) {
        if ((bytes[6] != 1 && bytes[6] != 2) || !(result.flags & 1) || (result.flags & ~3)
            || !result.nonce || !result.room || result.room > 0x7fffffff
            || !result.character || result.character > 0x7fffffff)
            throw std::runtime_error("invalid chat capability success");
        result.encoding = bytes[6] == 1 ? Encoding::utf8 : Encoding::latin1;
    } else if (bytes[6] || result.flags
               || (result.status == 1 && (result.nonce || result.room || result.character))
               || (result.status != 1 && (!result.nonce || !result.room || result.room > 0x7fffffff
                   || !result.character || result.character > 0x7fffffff)))
        throw std::runtime_error("invalid chat capability failure");
    return result;
}
}
