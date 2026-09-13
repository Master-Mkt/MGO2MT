#pragma once
#include <filesystem>
#ifndef _WINDEF_
struct HWND__;using HWND=HWND__*;
#endif
namespace mgo2win::items {
// True only after a successful atomic Save; cancel/error leaves disk untouched.
bool edit_item_settings(HWND owner,const std::filesystem::path& settingsPath,const std::filesystem::path& catalogPath={});
}
