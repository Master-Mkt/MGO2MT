#pragma once
#include <stdint.h>
#ifdef MGO2WPN_EXPORTS
#define MW_API __declspec(dllexport)
#else
#define MW_API __declspec(dllimport)
#endif
#ifdef __cplusplus
extern "C" {
#endif
// Paths are UTF-16. Error messages are UTF-8. 1=success, 0=invalid.
MW_API int mw_validate(const wchar_t* gameplayPath,const wchar_t* effectsPath,char* error,int capacity);
// Straight RGBA. key=0 loads a standard image; key>0 selects a GWFX image.
// Query with null pixels: 2=size available; 1=copied; 0=invalid.
MW_API int mw_image(const wchar_t* file,uint32_t key,uint8_t* rgba,uint32_t* width,uint32_t* height,int capacity);
// Returns count; null keys queries count. -1=invalid/insufficient capacity.
MW_API int mw_texture_keys(const wchar_t* bundle,uint32_t* keys,int capacity);
MW_API int mw_wave(const wchar_t* file,char* error,int capacity);
MW_API int mw_last_error(char* error,int capacity);
typedef struct MWParticle {
 float position[3],radius,rotation,stretch[2],rgba[4],uv[4];
 uint32_t texture,additive;
} MWParticle;
// Same deterministic evaluator as game particles; origin=(0,0,0), +Z emission.
// Weapon 0 samples defaults only; no weapon with ID0 can be stored in JSON.
// null output queries count; otherwise capacity is the number of MWParticle.
MW_API int mw_sample(const char* json,int length,uint16_t weapon,const char* channel,uint64_t ageMs,uint64_t seed,MWParticle* particles,int capacity);
// UTF-8 data-relative imported image path from last successful mw_sample.
MW_API int mw_texture_path(uint32_t key,char* utf8,int capacity);
#ifdef __cplusplus
}
#endif
