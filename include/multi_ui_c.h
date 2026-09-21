#pragma once
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(MGO2_MULTI_UI_EXPORTS)
#define MUI_API __declspec(dllexport)
#elif defined(_WIN32)
#define MUI_API __declspec(dllimport)
#else
#define MUI_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
MUI_API void* mui_create(void);
MUI_API void mui_destroy(void* handle);
MUI_API int mui_load(void* handle,const char* json_utf8,size_t json_length,const char* asset_root_utf8);
MUI_API int mui_load_file(void* handle,const char* path_utf8,const char* asset_root_utf8);
// RGBA byte order; background_rgba stores R in bits0..7, A in bits24..31.
// Render initializes the requested background even when no valid layout is loaded.
MUI_API int mui_render(void* handle,unsigned width,unsigned height,const char* screen_utf8,const char* state_utf8,const char* flags_json_utf8,const char* bindings_json_utf8,uint32_t background_rgba,uint8_t* rgba,size_t stride);
MUI_API size_t mui_error(void* handle,char* output,size_t capacity);
MUI_API int mui_validate(const char* json_utf8,size_t json_length,const char* asset_root_utf8,char* error,size_t error_capacity);
// Returns2 for size query (rgba=NULL),1 for decoded pixels,0 for failure.
MUI_API int mui_image_rgba(const char* path_utf8,uint8_t* rgba,unsigned* width,unsigned* height,size_t capacity);
MUI_API int mui_export_dds(const char* input_utf8,const char* output_utf8);
MUI_API int mui_tick(void* handle,const char* screen_utf8,const char* state_utf8,const char* flags_json_utf8,const char* bindings_json_utf8,int focused,int sound_enabled);
MUI_API int mui_click(void* handle,float design_x,float design_y);
MUI_API int mui_pointer(void* handle,float design_x,float design_y,int down);
MUI_API int mui_key(void* handle,unsigned virtual_key);
MUI_API int mui_trigger(void* handle,const char* element_utf8);
MUI_API void mui_cancel_actions(void* handle);
MUI_API int mui_busy(void* handle);
// UTF-8 JSON array, includes terminating zero in the required size. A size
// query does not discard events; a sufficiently large copy consumes them.
MUI_API size_t mui_action_events(void* handle,char* output,size_t capacity);
#ifdef __cplusplus
}
#endif
