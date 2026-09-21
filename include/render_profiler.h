#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace mgo2mt::render_profiler {
inline constexpr size_t HistoryCapacity=240, QueryRingCapacity=8;
inline constexpr double Missing=std::numeric_limits<double>::quiet_NaN();
enum class Stage : uint8_t {Shadow,Opaque,AmbientReflection,TransparentFx,Post,Ui,Total,Count};
inline constexpr size_t StageCount=size_t(Stage::Count);
enum class SampleState : uint8_t {Pending,Valid,Disjoint,Dropped,Unavailable,Invalid};
struct CpuTimes {double frameMs=Missing,submitMs=Missing,presentMs=Missing;};
struct Sample {
 uint64_t frame=0;CpuTimes cpu{};SampleState state=SampleState::Pending;
 std::array<double,StageCount> gpuMs{Missing,Missing,Missing,Missing,Missing,Missing,Missing};
 uint32_t invalidStages=0;
};
struct Status {
 bool supported=false,software=false;std::string adapter,reason;
 size_t pending=0;uint64_t submitted=0,resolved=0,disjoint=0,dropped=0,invalid=0;
};
struct Summary {size_t count=0;double latest=Missing,average=Missing,p95=Missing,maximum=Missing;};
enum class Series : uint8_t {CpuFrame,CpuSubmit,Present,GpuTotal,Shadow,Opaque,AmbientReflection,TransparentFx,Post,Ui,Count};
Summary summarize(std::span<const Sample>,Series);
std::string_view series_name(Series);
std::string_view state_name(SampleState);
// Pure validation/conversion is separately testable. NaN denotes unavailable;
// zero only denotes an actually measured zero-length interval.
struct TimestampData {
 uint64_t frequency=0;bool disjoint=false;
 std::array<std::array<uint64_t,2>,StageCount> ticks{};
 uint32_t issued=0,invalid=0;
};
Sample evaluate_timestamps(const TimestampData&);

// Single immediate-context/render-thread owner. No Flush, Map, blocking query
// loops, or cross-thread context access. Create queries once; call begin_frame
// only while F12 diagnostics are active. poll can retire older results when off.
class Profiler {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 Profiler();~Profiler();Profiler(Profiler&&)noexcept;Profiler&operator=(Profiler&&)noexcept;
 bool initialize(ID3D11Device*);
 bool begin_frame(ID3D11DeviceContext*,uint64_t frameId);
 void begin_stage(ID3D11DeviceContext*,Stage);
 void end_stage(ID3D11DeviceContext*,Stage);
 void end_frame(ID3D11DeviceContext*);
 // Exceptional exit: closes a begun disjoint query, invalidates that sample.
 void abandon_frame(ID3D11DeviceContext*);
 void poll(ID3D11DeviceContext*);
 void record_cpu(uint64_t frameId,CpuTimes);
 std::span<const Sample> history()const;
 Status status()const;
};
struct GraphOptions {int left=16,top=360,width=1248,height=288;};
struct GraphResult {bool painted=false;double axisMaximumMs=0;size_t lineSegments=0,spikes=0;};
// ARGB32 straight alpha, same surface convention as physics_debug::paint.
// Drawing only reads history. Missing/pending/disjoint values break the line.
GraphResult paint(std::span<uint32_t> argb,unsigned width,unsigned height,
                  std::span<const Sample>,const Status&,GraphOptions={});
GraphResult paint(std::span<uint32_t> argb,unsigned width,unsigned height,
                  const Profiler&,GraphOptions={});
}
