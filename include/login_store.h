#pragma once
#include <filesystem>
#include "login_form.h"
namespace mgo2mt {
// Current Windows user DPAPI protection. No portable plaintext credentials.
bool load_login(const std::filesystem::path&,LoginForm&);
void save_login(const std::filesystem::path&,const LoginForm&);
}
