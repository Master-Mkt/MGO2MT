# MGO2WIN / MGO2HOST

Windows client and dedicated HOST experiment, **v0.01-20260915142232**.

[Complete client and HOST downloads](https://github.com/Master-Mkt/MGO2WIN/releases/tag/v0.01-20260915142232-full) · [Release notes](docs/RELEASE_NOTES.md) · [導入方法](docs/FULL_README.ja.txt)

3Dの左右反転と5ステージの法線を修正。半球ライト、AKの手接続/リロード/発射、死亡/再出撃、原UI/日本語フォントを含みます。原作の材質・照明の完全一致は未完です。

The source tree excludes runtime resources. Use the complete ZIPs to run the program, with the same build on every client and HOST.

Build on Windows with Visual Studio C++ Build Tools, the Windows SDK and CMake:

```powershell
cmake -S . -B build -A x64 -DMGO2WIN_STATIC_RUNTIME=ON -DMGO2WIN_BUILD_TIMESTAMP=20260915142232
cmake --build build --config Release --target MGO2WIN MGO2HOST
```

The local runtime build passed 284 Release tests. Tests that use original fixtures require separately prepared local resources. Hardware GPU performance, physical gamepads and live LAN play were not validated in this delivery.

See [license status](LICENSE_STATUS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
