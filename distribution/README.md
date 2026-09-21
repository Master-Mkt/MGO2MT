# MGO2MT — MGO2MultiPlatform

[日本語](README.ja.md) · [Repository](https://github.com/Master-Mkt/MGO2MT) · [Releases](https://github.com/Master-Mkt/MGO2MT/releases)

MGO2MT is an independent, experimental client, dedicated host and local resource converter for MGO2-related interoperability work. **The current implementation supports Windows x64 only.** MultiPlatform describes the project's direction; Linux, macOS, consoles and mobile platforms are not supported releases.

This project was previously called MGO2WIN. Older internal identifiers and historical reports may retain that name. It is not an official Konami product and is not endorsed by the original rights holders or by the operators of an external service.

## Downloads and required local data

Public packages contain **software only**: executables, necessary software dependencies, minimal native configuration, documentation and applicable notices. They do not contain a playable copy of the original game.

| Package | Executable | Purpose |
| --- | --- | --- |
| CLIENT | `MGO2MT.exe` | Windows client, rendering, input and supported network/gameplay adapters |
| HOST | `MGO2MTHOST.exe` | Dedicated host application for the supported native room and combat implementation |
| EXCV | `MGO2MTEXCV.exe` | Offline conversion of user-supplied local data |

Use matching CLIENT and HOST versions and matching gameplay settings. Extract each complete software folder; retain its notices and any dependencies. Windows 10/11 x64 and the Windows graphics/audio components are required; the client uses Direct3D 11. The packaged converter includes its Python runtime, so a separate Python installation is not needed to run it. IDA, Noesis and an emulator are not client/HOST runtime requirements.

**Excluded from all public packages:** original or converted game models, textures, animation, UI images, audio, stage/collision data and scripts; videos including `movie_01.mp4`; PS3 font files; communication-constant assets including `network.gnk`; original executables/archives; SDK material; private analysis databases, packet captures, credentials, personal settings and logs. A converted format does not change this policy. Privately prepared complete development folders are not public release packages.

## Prepare resources with EXCV

Use only locally held data that you own or otherwise lawfully possess **and have the necessary rights to process for the intended purpose**. Possessing a disc, converting a file, or keeping an output local does not by itself establish that every use is lawful. See [license and rights status](LICENSE_STATUS.md).

1. Run `MGO2MTEXCV.exe`. Choose your original data's `o` folder and a separate output folder. The converter does not download game assets or modify the originals.
2. Select the stage/UI/runtime jobs you need and supply the compatible original MGO2 ELF. Encrypted packages require that ELF: EXCV checks it and reads the necessary tables into memory; those tables are not embedded in the program. Already decrypted inputs and native generated material can be processed without it, but encrypted modules report a missing-input error. Other inputs include an optional local font directory, your separately obtained `vgmstream-cli.exe` and its dependencies for BGM, and MGS4 `stage02.dat` for Gekko resources. These inputs and the external decoder are not supplied.
3. Read `conversion-run.json` and each module's `manifest.json`. When assembly succeeds, generated runtime files are collected under `runtime/data`. A successful conversion or matching hash checks integrity, not completeness, permission or original-game parity.
4. In the GUI, choose the CLIENT and/or HOST software folders and select **Apply to game** (`ゲームへ適用`). If needed, also select your authorized existing local `data` folder for `network.gnk` and an optional movie. EXCV checks the conversion inventory, preserves existing application settings and keeps the previous `data` as a backup. Read `data/excv-install.json` for missing resources; a completed copy is not a successful application check.
5. Run `MGO2MT.exe --check` and `MGO2MTHOST.exe --check` to find missing or inconsistent local inputs before attempting a connection. A software-only download is expected to report missing game resources. Application checks and live gameplay are separate verification steps.

Current limitations matter: the conversion inventory covers 21 multiplayer stages, while the native game integration has a smaller five-stage selection with differing coverage. Not all original GCX branches, placements, materials or behaviors are reproduced. Gekko requires the separate original data; regeneration directly from the original `stage02.dat` was not verified in the latest local converter work. The converter does not produce `movie_01.mp4` or `network.gnk`. The movie is optional: when it is absent, the idle START screen remains visible. Network data must already exist in an authorized local source; EXCV can import it, but neither the constants nor a ready-made file is offered here. The native water-step sound is synthesized by the converter; the optional full round-BGM library is not regenerated. Missing-input reports must not be bypassed or treated as a ready-to-play result.

Original pose, motion and connection-point data are separate optional local resources, not embedded or publicly supplied:

- `character/hit_geometry.gwhit`: locally prepared six-pose hit geometry. Without it, an authored simple human proxy is used.
- `motion/evade_travel.gwet`: rolling travel samples. Without it, an authored mathematical travel curve is used.
- `special/gekko_jump.gwjc`: original Gekko root-motion compensation. Without it, that compensation is omitted; the native physical jump remains available.
- `weapon/connection_points.gwcp`: model-matched CNP axes and first-person sight lines. Without it, precise CNP sight/ejection axes are unavailable; existing JSON/GWI weapon attachment, muzzle and magazine settings remain usable. The legacy two-model adapter uses generic authored points.

EXCV can import these files from authorized existing `--local-data`; regeneration of these four new formats from original files is not currently implemented. Invalid files are reported and rejected. CLIENT and HOST must match the presence and content of the hit-geometry and movement resources; CNP data is a client rendering input.

CLI examples (PowerShell; supply your own authorized paths):

```powershell
.\MGO2MTEXCV.exe --source "D:\LocalData\o" --output "D:\MGO2MT-Converted" --elf "D:\LocalData\MGO2.ELF" --with-ui --report "D:\conversion-result.json"
.\MGO2MTEXCV.exe --source "D:\LocalData\o" --output "D:\MGO2MT-Runtime" --with-ui --with-runtime --elf "D:\LocalData\MGO2.ELF" --decoder "D:\Tools\vgmstream-cli.exe" --mgs-source "D:\LocalData\stage02.dat"
.\MGO2MTEXCV.exe --verify "D:\MGO2MT-Converted\stages\n022a" --report "D:\verification.json"
.\MGO2MTEXCV.exe --install "D:\MGO2MT-Runtime" --client "D:\MGO2MT-CLIENT" --host "D:\MGO2MT-HOST" --local-data "D:\LocalData\data" --report "D:\application-result.json"
```

`--resume` verifies existing inputs and results before reusing them. If inputs, recipes or selected modules have changed, or verification fails, select a new output folder. Retain the failed report instead of treating partial files as valid output.

## Implemented scope

With appropriately prepared local resources, the current Windows code includes:

- Title, account/character, lobby, room and briefing screens; keyboard/XInput controls and graphics settings.
- Native DM/TDM round preparation, weapon selection, HOST-owned firing and reloads, damage, death/ragdoll presentation, respawn and kill events.
- Multiple firearm, throwable and support-weapon adapters; mounted weapons, mortar/catapult adapters and an experimental Gekko implementation. Coverage and original motion/attachment accuracy vary by weapon.
- JSON weapon parameters and effect settings, body-hit blood, blast knockback, projectile and particle effects. The separate WPN Effect Editor and Multi UI Designer are development components; this CLIENT/HOST/EXCV release list does not imply that editor binaries are included.
- Stage collision attributes, foot IK, lighting/weather controls, hemisphere and directional lighting, cascaded directional shadows, optional HDR/FXAA/AO/Bloom/SSR, static stage LOD and F12 diagnostics including GPU timing where supported.

Some parameters, effects and motion adapters are explicit native approximations. This is not a complete recreation of the original game. Full Forward+ rendering, all original rules/stages/actors and exact PS3 material, animation and audio parity are not established. Passing automated or offline render tests does not establish full live multiplayer compatibility. See each release's actual validation record; do not apply a historical test result to a different executable.

## Connections and hosting

The client has OpenMGO2-oriented HTTPS authentication, lobby/room access and UDP/STUN diagnostics. The HOST application has its own login/character selection, room creation/closure, rule and restriction controls, and native combat/environment synchronization. It is not a replacement for the external account/lobby service. Matching native client/HOST protocols are required; compatibility with unmodified PS3 clients or arbitrary server versions is not promised.

Login and live room actions are initiated by the user with their own account and service authorization. Entering a character-creation confirmation or creating/closing a room may affect the live service; offline checks do not perform those actions. Required local network data, service availability, firewall/NAT configuration and server support are separate prerequisites. Credential saving, where selected, uses Windows current-user protection. CLIENT and HOST use separate new settings folders under `%LOCALAPPDATA%\MGO2MT` and `%LOCALAPPDATA%\MGO2MTHOST`; old-name account settings are not automatically read or migrated. Do not attach credentials, saved login files or private traffic captures to public issues.

## Build and report problems

The C++ implementation uses Visual Studio C++ Build Tools, a Windows SDK and CMake 3.24 or newer:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The published source does not include private research inputs or the original resources required by some tests. Available tests depend on the source export and local prerequisites. Building software does not generate all required game data. Building EXCV also requires its Python packaging dependencies; the packaged EXCV is intended for users who do not need to build it.

For a report, include the software version, Windows/GPU information, concise reproduction steps and a redacted error. Do not upload game files, converted assets or restricted analysis material.

## Rights and publication policy

No project-wide license has been selected for the project's own code. Public visibility is not an open-source license or a blanket permission to modify or redistribute it. Third-party components retain their own terms. The project grants no rights in original game content, trademarks or external services, and makes no completed clean-room or non-infringement claim.

Read [LICENSE_STATUS.md](LICENSE_STATUS.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [PUBLICATION.md](PUBLICATION.md). The current software-only policy supersedes older instructions that allowed converted assets in public release archives. Do not upload or redistribute excluded material without the necessary rights.
