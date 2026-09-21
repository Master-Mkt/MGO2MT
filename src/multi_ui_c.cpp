#define MGO2_MULTI_UI_EXPORTS
#include "multi_ui_c.h"
#include "multi_ui.h"
#include "multi_ui_json.h"
#include <windows.h>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <stdexcept>
namespace {
using namespace mgo2mt::multi_ui;
struct Handle {Runtime runtime;std::mutex mutex;std::string error,eventJson;Context context;};
thread_local std::string globalError;
std::string_view text(const char*p,size_t maximum=1024*1024){if(!p)return {};auto n=strnlen_s(p,maximum+1);if(n>maximum)throw std::runtime_error("UI API string extent");return {p,n};}
std::filesystem::path path(const char*p){auto s=text(p,32767);json::utf8(s);std::wstring w(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0),L'\0');if(!w.empty())MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),w.data(),int(w.size()));return std::filesystem::path(w);}
size_t copy_error(const std::string&s,char*p,size_t capacity){if(p&&capacity){auto n=std::min(s.size(),capacity-1);std::memcpy(p,s.data(),n);p[n]=0;}return s.size()+1;}
std::string json_quote(std::string_view s){std::string out="\"";constexpr char digits[]="0123456789abcdef";for(unsigned char c:s){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=digits[c>>4];out+=digits[c&15];}else out+=char(c);}return out+'"';}
}
extern "C" {
void* mui_create(){try{return new Handle;}catch(const std::exception&e){globalError=e.what();return nullptr;}}
void mui_destroy(void*h){delete static_cast<Handle*>(h);}
int mui_load(void*h,const char*json,size_t length,const char*root){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);v.eventJson.clear();v.context=Context{};try{if(!json||length>1024*1024)throw std::runtime_error("UI API JSON extent");bool ok=v.runtime.load_json({json,length},path(root));v.error=v.runtime.error();return ok;}catch(const std::exception&e){v.runtime=Runtime{};v.error=e.what();return 0;}}
int mui_load_file(void*h,const char*file,const char*root){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);v.eventJson.clear();v.context=Context{};try{bool ok=v.runtime.load(path(file),path(root));v.error=v.runtime.error();return ok;}catch(const std::exception&e){v.runtime=Runtime{};v.error=e.what();return 0;}}
int mui_render(void*h,unsigned w,unsigned height,const char*screen,const char*state,const char*flags,const char*bindings,uint32_t bg,uint8_t*rgba,size_t stride){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{viewport(w,height);if(!rgba||stride<size_t(w)*4||stride>8192*4)throw std::runtime_error("UI API output extent");for(unsigned y=0;y<height;++y)for(unsigned x=0;x<w;++x)for(unsigned k=0;k<4;++k)rgba[size_t(y)*stride+x*4+k]=uint8_t(bg>>(8*k));auto context=parse_context(text(screen,128),text(state,128),text(flags),text(bindings));bool ok=v.runtime.paint({rgba,stride*height},w,height,context,stride);v.error=ok?"":v.runtime.error();return ok;}catch(const std::exception&e){v.error=e.what();return 0;}}
size_t mui_error(void*h,char*p,size_t capacity){if(!h)return copy_error(globalError,p,capacity);auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);return copy_error(v.error,p,capacity);}
int mui_validate(const char*json,size_t n,const char*root,char*error,size_t capacity){try{if(!json||n>1024*1024)throw std::runtime_error("UI validation extent");Runtime v;bool ok=v.load_json({json,n},path(root));copy_error(v.error(),error,capacity);return ok;}catch(const std::exception&e){copy_error(e.what(),error,capacity);return 0;}}
int mui_image_rgba(const char*file,uint8_t*rgba,unsigned*w,unsigned*h,size_t capacity){try{if(!w||!h)throw std::runtime_error("UI image dimensions pointers required");auto image=decode_image(path(file));*w=image.width;*h=image.height;if(!rgba)return 2;if(capacity<image.rgba.size())throw std::runtime_error("UI image output too small");std::memcpy(rgba,image.rgba.data(),image.rgba.size());globalError.clear();return 1;}catch(const std::exception&e){globalError=e.what();return 0;}}
int mui_export_dds(const char*input,const char*output){try{export_dds(path(input),path(output));globalError.clear();return 1;}catch(const std::exception&e){globalError=e.what();return 0;}}
int mui_tick(void*h,const char*screen,const char*state,const char*flags,const char*bindings,int focused,int sound){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{auto next=parse_context(text(screen,128),text(state,128),text(flags),text(bindings));if(!focused||next.screen!=v.context.screen||next.state!=v.context.state)v.eventJson.clear();v.context=std::move(next);v.runtime.update(v.context,focused!=0,sound!=0);return v.runtime.ready();}catch(const std::exception&e){v.eventJson.clear();v.runtime.cancel_actions("invalid context");v.error=e.what();return 0;}}
int mui_click(void*h,float x,float y){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{return v.runtime.click(x,y,v.context);}catch(const std::exception&e){v.runtime.cancel_actions("click failed");v.error=e.what();return 0;}}
int mui_pointer(void*h,float x,float y,int down){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{return v.runtime.pointer(x,y,down!=0,v.context);}catch(const std::exception&e){v.runtime.cancel_actions("pointer failed");v.error=e.what();return 0;}}
int mui_key(void*h,unsigned key){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{return v.runtime.key(key,v.context);}catch(const std::exception&e){v.runtime.cancel_actions("keyboard failed");v.error=e.what();return 0;}}
int mui_trigger(void*h,const char*id){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{return v.runtime.trigger(text(id,128),v.context);}catch(const std::exception&e){v.runtime.cancel_actions("trigger failed");v.error=e.what();return 0;}}
void mui_cancel_actions(void*h){if(!h)return;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);v.eventJson.clear();v.runtime.cancel_actions("stopped");}
int mui_busy(void*h){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);return v.runtime.busy();}
size_t mui_action_events(void*h,char*p,size_t capacity){if(!h)return 0;auto&v=*static_cast<Handle*>(h);std::lock_guard lock(v.mutex);try{if(v.eventJson.empty()){auto events=v.runtime.take_events();v.eventJson="[";for(const auto&e:events){if(v.eventJson.size()>1)v.eventJson+=',';v.eventJson+="{\"kind\":"+json_quote(e.kind)+",\"element\":"+json_quote(e.element)+",\"name\":"+json_quote(e.name)+",\"value\":"+json_quote(e.value)+",\"message\":"+json_quote(e.message)+"}";}v.eventJson+=']';}auto n=v.eventJson.size()+1;if(p&&capacity>=n){copy_error(v.eventJson,p,capacity);v.eventJson.clear();}return n;}catch(const std::exception&e){v.error=e.what();return 0;}}
}

