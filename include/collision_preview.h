#pragma once
#include <filesystem>

namespace mgo2mt {
// Offline verification with the original QQ model, collision, lighting and
// motion resources. Movement is produced by Navigation and admitted by HOST.
int collision_test_capture(const std::filesystem::path& data,
                           const std::filesystem::path& output);
}
