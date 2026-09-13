#pragma once
#include <string_view>

namespace mgo2win::menu_audio {
// Current MGO2 ELF 1a55a41e...: AA7DB4 input -> 989D0C/988404
// cancel(-1)/confirm(1)/changed cursor -> 482B0. Full evidence in
// notes/MENU_AUDIO_MUSIC_20260913.md. Windows UI action mapping is native.
inline constexpr unsigned Cancel = 92;
inline constexpr unsigned Confirm = 93;
inline constexpr unsigned Cursor = 94;
constexpr std::wstring_view asset(unsigned cue) {
    switch (cue) {
    case Cancel: return L"92.gwa";
    case Confirm: return L"93.gwa";
    case Cursor: return L"94.gwa";
    default: return {}; // Unknown events must not become a cursor sound.
    }
}
}
