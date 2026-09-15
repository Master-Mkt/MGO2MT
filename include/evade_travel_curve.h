#pragma once
#include <array>
#include <cmath>
#include <cstddef>
namespace mgo2win::combat::evade_runtime {
// Exact root-Z float samples from reviewed evade.gwmot, source56/57.
// Source archive and bank are unchanged. The native physical curve below caps
// each frame at 6000 units/s, discards reverse jitter and forces recover35..45
// stationary. Capped displacement is never carried into a later stopped frame.
inline constexpr std::array<float,41> roll_source_z{-6.f,-6.f,20.328125f,101.875f,242.5f,446.f,686.5f,931.f,1172.f,1404.f,1621.f,1827.f,2022.f,2204.f,2376.f,2538.f,2686.f,2824.f,2952.f,3068.f,3172.f,3268.f,3350.f,3422.f,3484.f,3534.f,3576.f,3614.f,3648.f,3678.f,3706.f,3736.f,3764.f,3792.f,3820.f,3850.f,3884.f,3920.f,3956.f,3994.f,4032.f};
inline constexpr std::array<float,46> recover_source_z{4070.f,4070.f,4108.f,4148.f,4188.f,4224.f,4264.f,4304.f,4348.f,4388.f,4432.f,4472.f,4508.f,4548.f,4584.f,4620.f,4652.f,4684.f,4712.f,4736.f,4756.f,4766.f,4776.f,4816.f,4832.f,4844.f,4860.f,4868.f,4876.f,4884.f,4892.f,4900.f,4908.f,4912.f,4920.f,4924.f,4924.f,4928.f,4928.f,4928.f,4924.f,4924.f,4924.f,4924.f,4924.f,4924.f};
inline constexpr float travel_speed_cap=6000.f;
inline constexpr double travel_fps=60.;
inline constexpr size_t recover_stop_frame=35;
inline constexpr auto travel_distances=[] {
 std::array<float,86> out{};size_t k=1;
 auto append=[&](float delta){delta=delta<0?0:delta>100?100:delta;out[k]=out[k-1]+delta;++k;};
 for(size_t i=1;i<roll_source_z.size();++i)append(roll_source_z[i]-roll_source_z[i-1]);
 for(size_t i=1;i<recover_source_z.size();++i)append(i>recover_stop_frame?0:recover_source_z[i]-recover_source_z[i-1]);
 return out;
}();
// Pure cumulative distance: call differences at action ages, not an integrated
// per-tick speed. Finite huge times clamp; negative/NaN/infinite input returns0.
// Exact source phase duration85/60s; caller's1417ms expiry adds no extra travel.
inline float distance_seconds(double seconds){
 if(!std::isfinite(seconds)||seconds<0)return 0;
 if(seconds>=85./60)return travel_distances.back();
 const double f=seconds*travel_fps;const auto i=static_cast<size_t>(f);
 return float(double(travel_distances[i])+(double(travel_distances[i+1])-travel_distances[i])*(f-double(i)));
}
}
