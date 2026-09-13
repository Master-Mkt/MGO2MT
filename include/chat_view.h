#pragma once
#include "chat_session.h"
#include <windows.h>
namespace mgo2win::chat {
std::wstring wide(std::string_view);
std::string utf8(std::wstring_view);
// Bounded two-line rows, newest at bottom; maxAge=0 shows the saved room history.
void draw_history(HDC,HFONT,const State&,RECT,uint64_t now,uint64_t maxAge=0);
}
