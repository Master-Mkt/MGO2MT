#include "preset_radio_wire.h"
#include <stdexcept>
namespace mgo2::radio::wire {
bool reviewed_id(uint8_t id) noexcept { return id <= 7 || (id >= 9 && id <= 16); }
namespace {
bool valid(const Record& r) {
    return (r.kind == Kind::client_request || r.kind == Kind::host_notification)
        && r.slot < 24 && reviewed_id(r.presetId) && r.third == 2;
}
}
std::array<uint8_t,4> encode(const Record& r) {
    if (!valid(r)) throw std::invalid_argument("unsupported preset radio body");
    return {static_cast<uint8_t>(r.kind), r.slot, r.presetId, r.third};
}
Record decode(std::span<const uint8_t> bytes) {
    if (bytes.size() != 4) throw std::runtime_error("invalid preset radio body extent");
    Record r{static_cast<Kind>(bytes[0]), bytes[1], bytes[2], bytes[3]};
    if (!valid(r)) throw std::runtime_error("unsupported preset radio body fields");
    return r;
}
}
