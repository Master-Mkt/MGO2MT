#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
namespace mgo2win::notifications {
enum class Kind:uint8_t {mail,tournament,survival};
struct Scope {uint64_t connection=0,generation=0;uint32_t character=0;bool operator==(const Scope&)const=default;};
// id is a stable arrival/message identifier within this scope, not a poll ID.
// active=false includes read/answered/cancelled records. Zero expiry means none.
struct Notice {Kind kind=Kind::mail;uint64_t id=0,expiresUnixMs=0;bool active=true;};
struct Alert {
 uint64_t id=0,version=0,publishedUnixMs=0,expiresUnixMs=0;std::string text;
 bool operator==(const Alert&)const=default;
};
struct News {uint64_t id=0,version=0,beganMs=0;std::wstring text;};
struct View {Scope scope;std::array<bool,3> badges{};bool bright=false;std::optional<News> news;};
// Strict UTF-8 with native single-line CR/LF/TAB flattening. No HTML, markup,
// bidi formatting or control interpretation. Up to4096 UTF-8 bytes.
std::optional<std::wstring> news_text(std::string_view);
class Presentation {
 using Key=std::pair<Kind,uint64_t>;
 std::optional<Scope> scope_;std::set<Key> seen_,acknowledged_,pingPending_;std::map<Key,uint64_t> active_;
 View view_;std::optional<Alert> alert_;uint64_t now_=0;
 std::array<bool,3> initializedKinds_{};uint64_t mailHighwater_=0;
 bool soundSaturated_=false;
public:
 static constexpr size_t maximumNotices=256,maximumSeen=256;
 // notices and alert are the latest complete authoritative view. A failed or
 // incomplete network poll must not replace it with an empty successful view.
 // Unix time is only for server publication/expiry; monotonic time animates UI.
 View update(Scope,std::span<const Notice>,std::optional<Alert>,uint64_t unixNowMs,uint64_t monotonicNowMs,std::array<bool,3> ready={true,true,true});
 bool take_ping(){const bool result=!pingPending_.empty();pingPending_.clear();return result;}
 // Local badge acknowledgement only; never marks a server mail read or answers
 // an invitation. Caller still owns the existing explicit response workflow.
 bool acknowledge(Scope,Kind,uint64_t id);
 void reset();
 const View& view()const{return view_;}
};
struct Rect {int left=0,top=0,right=0,bottom=0;};
struct PaintLayout {
 int iconRight=1256,iconTop=76,newsLeft=24,newsRight=1256,newsTop=112;
 // Caller may exclude the actual movie rectangle on non-gameplay screens.
 std::optional<Rect> excluded;
};
inline constexpr uint32_t orange=0xffffa52b,dimOrange=0xff8a5917;
class Renderer {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 Renderer();~Renderer();Renderer(const Renderer&)=delete;Renderer&operator=(const Renderer&)=delete;
 // Native envelope/cup/group glyphs, not recovered original artwork. Composes
 // into a fresh straight-alpha1280x720 HUD after theme finalization.
 bool paint(std::span<uint32_t>,int width,int height,const View&,uint64_t monotonicNowMs,PaintLayout={});
};
}
