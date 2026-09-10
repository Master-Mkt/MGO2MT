#include <windows.h>
#include <d3d11.h>
#include <xaudio2.h>
#include <bit>
#include <iostream>
// Keep imports in the probe to check the installed Windows SDK link libraries.
decltype(&D3D11CreateDevice) volatile graphics_api = &D3D11CreateDevice;
decltype(&XAudio2Create) volatile audio_api = &XAudio2Create;
int main() {
    static_assert(sizeof(void*) == 8);
    static_assert(std::endian::native == std::endian::little);
    if (!graphics_api || !audio_api) return 1;
    std::cout << "Windows x64 C++20 / Direct3D 11 / XAudio2 headers and imports OK; devices not opened\n";
}
