#pragma once
#include <windows.h>
#include "environment_settings.h"
namespace mgo2mt {
bool edit_host_environment(HWND,environment::Config&);
void inspect_host_environment(const std::filesystem::path&);
}
