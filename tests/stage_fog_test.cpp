#include "stage_fog.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace mgo2win::stage;
void require(bool ok) { if (!ok) throw std::runtime_error("stage fog test"); }
void close(float a,float b) { require(std::abs(a-b) <= 0.000001f * std::max(1.f,std::abs(b))); }
template<class F> void rejects(F f) {
    bool threw=false; try { f(); } catch(const std::invalid_argument&) { threw=true; }
    require(threw);
}
int main() {
    const FogScriptValues normal{0,400000,{745,756,509},{0,371}};
    const auto n=decode_fog_values(normal);
    require(n.rgbBytes==std::array<std::uint8_t,3>{189,192,129});
    close(n.constant21[0],0.0000025f);close(n.constant21[1],0);close(n.constant21[3],.371f);
    const auto sand=decode_fog_values({-25000,75000,{411,400,188},{0,709}});
    require(sand.rgbBytes==std::array<std::uint8_t,3>{104,102,47});
    close(sand.constant21[0],.00001f);close(sand.constant21[1],.25f);close(sand.constant21[3],.709f);
    const auto half=decode_fog_values({-125000,162500,{411,400,188},{0,709}});
    close(half.constant21[1],125000.f/287500.f);
    const auto hh=decode_fog_values({10000,90000,{109,125,137},{0,875}});
    require(hh.rgbBytes==std::array<std::uint8_t,3>{27,31,34});
    close(hh.constant21[0],1.f/80000.f);close(hh.constant21[1],-.125f);close(hh.constant21[3],.875f);
    // Original integer quantization boundary, including full authored scale.
    require(decode_fog_values({0,1,{0,3,1000},{0,1000}}).rgbBytes==std::array<std::uint8_t,3>{0,0,255});
    require(decode_fog_values({0,1,{4,500,999},{0,1000}}).rgbBytes==std::array<std::uint8_t,3>{1,127,254});
    close(fog_transition_seconds(600),10);close(fog_transition_seconds(1200),20);close(fog_transition_seconds(0),0);
    for (auto bad : {FogScriptValues{0,0,{0,0,0},{0,1}}, {1,0,{0,0,0},{0,1}},
        {-10000001,1,{0,0,0},{0,1}}, {0,10000001,{0,0,0},{0,1}},
        {0,1,{-1,0,0},{0,1}}, {0,1,{1001,0,0},{0,1}},
        {0,1,{0,0,0},{-1,1}}, {0,1,{0,0,0},{0,1001}}, {0,1,{0,0,0},{2,1}}})
        rejects([&]{decode_fog_values(bad);});
    rejects([]{fog_transition_seconds(-1);});rejects([]{fog_transition_seconds(360001);});
    require(decode_fog_values(normal).rgbBytes==n.rgbBytes); // no state retained by rejection
    std::cout << "PASS CC normal/sand/half, HH, original quantization, constants, transition and invalid boundaries\n";
}
