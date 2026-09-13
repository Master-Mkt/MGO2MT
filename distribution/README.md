# MGO2WIN / MGO2HOST

Native Windows client and dedicated HOST experiment for OpenMGO2. This source snapshot accompanies **v0.01-20260914031351**, dated 2026-09-14. Use the matching client and HOST from the [complete Release archives](https://github.com/Master-Mkt/MGO2WIN/releases); source alone does not include the runtime resources.

[日本語](README.ja.md) · [Changes and limitations](docs/RELEASE_NOTES.md) · [Installation and controls](docs/FULL_README.ja.txt)

The exact published archive sizes, hashes and executable identities are recorded in [ARTIFACTS.json](release/2026-09-14/ARTIFACTS.json) and [SHA256SUMS.txt](release/2026-09-14/SHA256SUMS.txt). A locally rebuilt executable is not implicitly the same binary.

The current limited implementation includes four stage profiles, ordinary DM/TDM, authoritative deployment/respawn, AK102 combat and placement/recovery, material penetration/audio, native effects, chat/radio, camera settings, SOP and invitation UI. Server-dependent extensions require a compatible server. This is an experimental implementation; the release notes explicitly distinguish recovered behavior, native approximations and unfinished work. Real LAN endurance and live invitation operation remain unverified.

## Build the native programs

Use Windows x64, CMake 3.24 or newer, a recent Visual Studio C++ toolchain with C++20 support, and the Windows SDK (D3D11, DirectXMath, GDI, WinHTTP, BCrypt, XAudio2 and XInput headers/libraries). Open an x64 Native Tools Command Prompt with Ninja on PATH:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DMGO2WIN_STATIC_RUNTIME=ON -DMGO2WIN_BUILD_TIMESTAMP=20260914031351
cmake --build build --parallel 4 --target MGO2WIN MGO2HOST
```

The timestamp identifies the build and is shared by both programs. It does not reproduce compiler/linker byte-for-byte identity by itself. No game assets, IDA installation or extraction tools are needed to compile these executables. To run them, supply the complete matching Release data rather than mixing versions or copying an EXE into an older data set.

## Source-only checks

```bat
cmake --build build --parallel 4 --target host_hit_geometry_test combat_hit_regions_test tournament_invitation_test chat_session_test native_radio_test world_inventory_test world_inventory_wire_test player_control_test item_settings_test
ctest --test-dir build --output-on-failure -R "^(host_hit_geometry_original_pose|host_hit_regions_and_penetration|original_tournament_invitation_protocol|room_chat_lifecycle|native_radio_protocol|native_world_inventory|native_world_inventory_wire|player_actions_and_posture|native_item_settings)$"
```

`ctest -N` lists all configured tests. Some original-asset/render tests refer to local `work/` or `outputs/` fixtures which are deliberately absent from Git. An unfiltered CTest run is not a source-only test suite. The local release's broader verification is described separately from the source snapshot checks.

`Build-Title.cmd` and `tools/package_title.py` are the retained local title-package workflow and require prepared GWP/resource inputs. They do not reconstruct the complete September 14 client/HOST archives from raw game files. The repository does not include original game archives, network key resources, extraction/decryption tools, IDA databases, private captures, credentials, personal settings or updater test keys. The original MGO2 patch feature is not used.

The source exporter in `tools/export_public_review.py` includes current native headers, C++ sources, generated `.inc` constants, CMake and standalone C++ tests. It writes a separate source directory and does not publish or access a remote.

## Notices

See [LICENSE_STATUS.md](LICENSE_STATUS.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [PUBLICATION.md](PUBLICATION.md). No new project-wide license or redistribution-rights guarantee is implied by public visibility. Game-derived runtime data remains separate from the source snapshot.

