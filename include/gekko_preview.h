#pragma once
#include <filesystem>
namespace mgo2mt {
// Explicit offline special-PC inspection; never opens authentication or sockets.
// data is the distribution's data root. Capture drives this same runtime with
// deterministic local input and writes its D3D backbuffer before returning.
int run_gekko_preview(const std::filesystem::path& data,bool capture,const std::filesystem::path& output);
}
