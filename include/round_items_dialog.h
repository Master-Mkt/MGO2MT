#pragma once
#include <filesystem>
#ifndef _WINDEF_
struct HWND__;using HWND=HWND__*;
#endif
namespace mgo2mt::items {
bool edit_round_items(HWND,const std::filesystem::path&,const std::filesystem::path&,const std::filesystem::path& capture={});
}
