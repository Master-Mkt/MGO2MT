#include "host_briefing.h"
#include <iostream>
#include <stdexcept>

using namespace mgo2mt::host;
namespace {
void require(bool pass) { if (!pass) throw std::runtime_error("briefing phase check failed"); }
template<class F> void reject(F fn) {
    bool rejected = false;
    try { fn(); } catch (const Invalid&) { rejected = true; }
    require(rejected);
}
}
int main() {
    // Fixed original layout: descriptor 0 is mask bit 0. Seven mask bytes.
    const std::vector<uint8_t> active = {0x0b,0,1,0,0,0,0,0,0,4};
    require(phase_update(4) == active);
    require(phase_value(active) == 4);
    for (unsigned phase = 0; phase != 8; ++phase)
        require(phase_value(phase_update(uint8_t(phase))) == phase);
    reject([] { phase_update(8); });
    reject([] { phase_update(255); });
    // Generation-only updates and explicit zero phase are different facts.
    require(!phase_value(std::vector<uint8_t>{11,0,0,0,0,0,0,0,8,9}));
    require(phase_value(phase_update(0)).has_value());
    // An unknown phase received from the host is observable, never normalized.
    auto unknown = active; unknown.back() = 255;
    require(phase_value(unknown) == 255);
    // Reject the whole update even when an earlier phase field was complete.
    for (size_t n = 0; n < active.size(); ++n)
        reject([&] { phase_value(std::span(active).first(n)); });
    auto extra = active; extra.push_back(0);
    reject([&] { phase_value(extra); });
    auto invalid_mask = active; invalid_mask[8] |= 128;
    reject([&] { phase_value(invalid_mask); });
    auto incomplete_later_field = active; incomplete_later_field[2] |= 2;
    reject([&] { phase_value(incomplete_later_field); });
    auto player = active; player[1] = 1;
    reject([&] { phase_value(player); });
    auto opcode = active; opcode[0] = 6;
    reject([&] { phase_value(opcode); });
    std::cout << "host briefing phase tests passed\n";
}
