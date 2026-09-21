#pragma once
#include <windows.h>
#include <filesystem>
#include "host_room.h"
namespace mgo2mt {
// Changes only the draft on explicit acceptance. No network operations.
bool edit_host_options(HWND owner,host::Settings& draft);
// Isolated hidden-window validation and captures; never opens a game room.
void inspect_host_options(const std::filesystem::path& output);
}
