# MGO2WIN — experimental preview

[日本語](README.ja.md) · [Trial downloads](https://github.com/Master-Mkt/MGO2WIN/releases)

A native Windows client experiment for OpenMGO2, using Direct3D 11 and converted local game resources. This is an early preview of the title, account, character, lobby and room screens; playable matches are not implemented.

## Source update — 2026-09-11

The source now includes lobby entry, host admission, participant updates and stage metadata. The downloadable `v0.1.0-preview.1` executable predates these changes; this source update does not replace that Release ZIP.

## Try it

Download the Windows x64 ZIP from Releases, extract the entire folder, and run `MGO2WIN.exe`. Keep `data` beside the executable. Windows 10/11 x64 with D3D11 support is required. Python, IDA, Noesis and an emulator are not required at runtime.

Enter starts the title; choose YES on the downloaded OpenMGO2 policy, then enter your own game account. The connection target is OpenMGO2. This preview has a ten-minute session limit; reopen the executable to start another session. The executable is unsigned. Logs and optional settings are stored under `%LOCALAPPDATA%/MGO2WIN`.

## Included

- Original background animation, title/lobby music, START and menu sounds in converted formats.
- HTTPS policy text, login, optional ID storage or current-user DPAPI-protected automatic login.
- Port settings/STUN diagnostics, 256–2048 kbps setting, keyboard/XInput bindings and graphics controls.
- Account character list, original model/texture assembly and a looping lobby motion; 228 model entries and 1,314 textures.
- Character creation form, Unicode name input, sex, equipment, eight voices per sex and pitch −7 to +7. Explicit YES submits a real registration request.
- Restored clothing textures and mode2 accessory RGB, including goggles. Names currently require at least four Unicode code points and at most 16 UTF-8 bytes.

- Six lobby categories, room browsing/details, password entry and host admission with cancellation, timeout and rejection handling.
- Up to 24 replicated participant slots, join/leave updates and host/self labels.
- Stage information showing the host-selected map/rule IDs and rotation position, with round/map transition tracking.
- Cut-corner translucent orange menu highlights and shared headings inspired by the original screens.

## Current limits

Real-account PC selection, Free Battle lobby entry and joining/leaving an existing user-hosted room have been verified in earlier local builds. The latest participant-update and stage-information changes pass offline UI tests and original-packet replay, but have not yet been verified in a live room. Stage metadata does not mean a playable map has loaded: world asset loading, loading-complete notifications, player movement, peer mesh and host migration remain unimplemented.

PC deletion transport, slot purchases and payment remain unimplemented; deletion confirmation is UI only. Four free slots are the client specification, with matching server enforcement deployed. Character creation was tested with simulated responses; successful creation with the distributed executable has not been confirmed on a real account. Do not retry an uncertain registration response without checking the character list. Japanese names of up to 16 characters remain a future coordinated client/server change; the current 16 UTF-8-byte limit is unchanged.

Seventeen model entries, mode3 material parameters, original reflection/transparency, accessory motion and exact PS3 visual parity remain incomplete. Shadow-buffer and bandwidth selections are stored settings pending gameplay integration. Video playback is deferred.

## Source and resources

Git contains the reviewed native source and notices. The trial Release ZIP additionally contains converted runtime resources; raw game archives, ELF/SELF, original GCX/MDN/MTAR/TXN, IDA databases, SDK files, credentials and personal settings are excluded. The title route is compiled offline into `data/title.gwp`; the trial does not read or include original GCX. This is a bounded normal-title route with deferred effects, not a complete GCX implementation.

Game-derived resources retain their original rights status. Conversion does not assign them a new license. No project-wide open-source license is selected; see [license status](LICENSE_STATUS.md) and [third-party notices](THIRD_PARTY_NOTICES.md). This is an independent experiment, not an official Konami product.

## Build source

Install Visual Studio C++ Build Tools with a Windows SDK and CMake 3.24 or newer. Configure with `cmake -S . -B build -A x64 -DMGO2WIN_STATIC_RUNTIME=ON`, build with `cmake --build build --config Release`, and run `ctest --test-dir build -C Release --output-on-failure`.

Tests requiring original local research inputs are enabled only when those inputs exist. The source checkout does not contain those inputs or the historical asset converters. `Build-Title.cmd` requires a prepared local scene and resources; it is not a fresh-dump conversion workflow. Download the trial package to test the prepared build.
