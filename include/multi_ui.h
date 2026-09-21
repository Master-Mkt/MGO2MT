#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace mgo2mt::multi_ui {
class AudioBackend;
struct ActionEvent {std::string kind,element,name,value,message;};
struct Image {uint32_t width=0,height=0;std::vector<uint8_t> rgba;};
Image decode_image(const std::filesystem::path&);
Image decode_dds(std::span<const uint8_t>);
std::vector<uint8_t> encode_dds(const Image&);
void export_dds(const std::filesystem::path& source,const std::filesystem::path& destination);
struct Context {std::string screen="title",state="default";std::set<std::string> flags;std::map<std::string,std::string> bindings;};
struct Viewport {float x=0,y=0,scale=1,width=1280,height=720;};
Viewport viewport(unsigned width,unsigned height);
// The caller owns its default UI pixels. A not-ready runtime leaves them intact.
// Failed load clears readiness; partially parsed layouts are never painted.
class Runtime {
 struct Impl;std::unique_ptr<Impl> impl_;std::unique_ptr<AudioBackend> audio_;mutable std::string error_;
 std::vector<ActionEvent> events_;uint64_t revision_=0,nextToken_=1;
 void advance_actions();void notice(ActionEvent);
 bool begin_sequence(size_t element,bool change,std::string value={});
 bool activate(float x,float y,const Context&);
public:
 Runtime();~Runtime();Runtime(Runtime&&)noexcept;Runtime&operator=(Runtime&&)noexcept;
 explicit Runtime(std::unique_ptr<AudioBackend>);
 bool load(const std::filesystem::path& jsonPath,const std::filesystem::path& assetRoot={});
 bool load_json(std::string_view json,const std::filesystem::path& assetRoot);
 bool paint(std::span<uint8_t> rgba,unsigned width,unsigned height,const Context&,size_t stride=0)const;
 bool ready()const noexcept;const std::string& error()const noexcept{return error_;}
 // Design-space pointer coordinates (1280x720). Only visible elements with
 // Controls and visible onClick elements consume input. One sequence at a
 // time; repeated activation is ignored. Pointer actions fire on release over
 // the same control. click() remains a complete logical click for old callers.
 bool pointer(float x,float y,bool down,const Context&);
 // Virtual-key navigation: Tab, arrows, Enter/Space, Escape. Call activation
 // keys on their initial keydown; arrow repeat may be forwarded.
 bool key(unsigned virtualKey,const Context&);
 bool click(float x,float y,const Context&);
 bool trigger(std::string_view element,const Context&);
 void update(const Context&,bool focused=true,bool soundEnabled=true,uint64_t now=UINT64_MAX);
 void cancel_actions(std::string reason="cancelled");
 std::vector<ActionEvent> take_events();
 uint64_t revision()const{return revision_;}
 bool busy()const;
};
Context parse_context(std::string_view screen,std::string_view state,std::string_view flagsJson,std::string_view bindingsJson);
}
