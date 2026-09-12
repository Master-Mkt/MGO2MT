# MGO2WIN — experimental preview

[日本語](README.ja.md) · [Trial downloads](https://github.com/Master-Mkt/MGO2WIN/releases)

A native Windows client experiment for OpenMGO2, using Direct3D 11 and converted local game resources. This is an early preview of the title, account, character, lobby and room screens; playable matches are not implemented.

## Source update — 2026-09-12

Lobby selection now uses game types on the left and lobby names/player counts on the right. Use Left/Right or Tab to change columns, Up/Down to select, and Enter or the row’s right-hand arrow to enter. Settings use folder-shaped tabs, alternating row shades and orange focus guides extending across the screen. Existing original background animation and music are retained. This update publishes source and documentation only. The downloadable `v0.1.0-preview.1` executable predates these changes; its Release ZIP is unchanged.

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

Real-account PC selection, Free Battle lobby entry and joining/leaving an existing user-hosted room have been verified in earlier local builds. The latest participant-update and stage-information changes pass offline UI tests and original-packet replay, but have not yet been verified in a live room. The current source can preload an n022a stage preview with textures, approximate hemisphere lighting, collision queries and reference placements. This is not a playable map: scene/object completion, full actor activation, player movement, peer mesh and host migration remain incomplete.

PC deletion transport, slot purchases and payment remain unimplemented; deletion confirmation is UI only. Four free slots are the client specification, with matching server enforcement deployed. Character creation was tested with simulated responses; successful creation with the distributed executable has not been confirmed on a real account. Do not retry an uncertain registration response without checking the character list. Japanese names of up to 16 characters remain a future coordinated client/server change; the current 16 UTF-8-byte limit is unchanged.

Seventeen model entries, mode3 material parameters, original reflection/transparency, accessory motion and exact PS3 visual parity remain incomplete. Shadow-buffer and bandwidth selections are stored settings pending gameplay integration. Video playback is deferred.

## Source and resources

Git contains the reviewed native source and notices. The trial Release ZIP additionally contains converted runtime resources; raw game archives, ELF/SELF, original GCX/MDN/MTAR/TXN, IDA databases, SDK files, credentials and personal settings are excluded. The title route is compiled offline into `data/title.gwp`; the trial does not read or include original GCX. This is a bounded normal-title route with deferred effects, not a complete GCX implementation.

Game-derived resources retain their original rights status. Conversion does not assign them a new license. No project-wide open-source license is selected; see [license status](LICENSE_STATUS.md) and [third-party notices](THIRD_PARTY_NOTICES.md). This is an independent experiment, not an official Konami product.

## Build source

Install Visual Studio C++ Build Tools with a Windows SDK and CMake 3.24 or newer. Configure with `cmake -S . -B build -A x64 -DMGO2WIN_STATIC_RUNTIME=ON`, build with `cmake --build build --config Release`, and run `ctest --test-dir build -C Release --output-on-failure`.

Tests requiring original local research inputs are enabled only when those inputs exist. The source checkout does not contain those inputs, the recovered runtime weapon catalog, or the historical asset converters. Synthetic catalog and UI fixtures keep source-only tests runnable. The two DPAPI tests require access to the current Windows user’s cryptographic profile. `Build-Title.cmd` requires a prepared local scene and resources; it is not a fresh-dump conversion workflow. Download the trial package to test the prepared build.

## Stage debug and PCM music (2026-09-12)

F12 toggles debug. In room stage information: F5 resets local stage preparation, F7/F8 select music, F9 plays/stops it, and F6 auditions ambience. This does not reset the server round. The n022a diagnostic view composes 18 reference instances and shuffles car variants among original car anchors; original activation, RNG and destruction behavior remain unverified.

`data/bgm` contains standard PCM16 WAV played directly with Windows XAudio2. The private build prepares 79 WAVs from 52 original files (about 1.44 GB). Source identifiers are retained; extra four-channel banks use `_layer1` etc. Each bank is downmixed to stereo with an explicit native policy, not a verified original mixer or state-selector mapping. Standard `smpl` chunks retain original loops.

Place up to 32 WAV files in `data/bgm/additional`, then restart: PCM16, 8–192 kHz, 1–8 channels, at most 256 MiB each. Filenames are default display titles, editable through playlist1.txt / playlist2.txt; duplicate content is excluded. Additional IDs use whole-file SHA-256, never a playlist row. Missing forced music falls back to local selection, an available original, then silence. Respawn-queued selection and forced-track resolution are unit-tested but not wired to live respawn or Snake BGM network events yet.

## Editable BGM titles and reset confirmation (2026-09-12)

Re-selecting the same resolved song at respawn preserves the existing audio voice and playback position. A display-title edit cannot restart it. Live respawn event binding is still pending.

Edit UTF-8 `data/bgm/playlist1.txt` for originals and `data/bgm/additional/playlist2.txt` for additional music. Each line is `filename.wav=Display title` (a tab separator also works). Japanese, Korean and other valid Unicode titles are supported, up to 128 characters. Blank lines and # comments are ignored; missing/invalid entries fall back to filename stems. Close and reopen F12 debug to reload titles without restarting music. Adding/removing audio files still requires an application restart. These editable text files are outside the mandatory startup hash checks.

F5 now opens a YES/NO reset confirmation, defaulting to NO. Use arrows/Tab and Enter, or click a button. NO, Escape, closing debug, loss of focus or a changed stage request cancels without resetting.

## Briefing and weapon draft (2026-09-12)

After room admission, F4 opens the weapon screen. Left/right or Tab changes category; up/down and Enter select; F10 retains the draft. Esc returns to the roster without discarding a draft in the same round. Stage assets preload while viewing the roster or weapons and survive panel changes.

The reviewed profile is ordinary-character n022a/TDM. Prices and room restrictions come from the original data. DP-enabled rooms display prices but cannot confirm a draft until a live balance is received; the original minimum initial 1,000 DP is never substituted for an authoritative balance. Special-only weapons, attachments, loadout transmission and spawning remain pending.

MGO2HOST counts the configured briefing duration from the first player admission and enters preparation only after reliable room-metadata receipt. This is not proof of scene/object completion or combat start. Original START/cancel messages, all-START early transition and actual round start remain pending.

## Dedicated host and common settings (2026-09-12)

`MGO2HOST.exe` uses the native account/lobby transport to publish dedicated-only TDM / n022a rooms. It defaults to UDP 5732 and supports up to 16 players plus a separate host identity. Enter credentials in the application; optional storage uses the current Windows user’s DPAPI profile. No account is compiled into the source. Runtime requires locally prepared `data/network.gnk` and `data/lobbies.cfg` beside the executable.

Before creating a room, Common Settings / Weapon Restrictions opens folder-shaped tabs. Capacity, briefing, statistics, friendly fire, auto aim, nametags, team policies, voice-chat policy and kick settings populate room creation and environment messages. Apply retains the draft for the current application session; Cancel/Close leaves the previous settings intact. Invalid input stays on the relevant field.

Weapon restrictions expose five categories and 52 controls, with independent master enable, individual and category-wide allow/forbid. Known masks are changed without clearing unrelated or unknown bits. Base LEVEL remains fixed at 22 and Quick Join is unavailable because the inspected server does not preserve those settings as required. The native combat/voice/moderation effects, original weapon icons and all-player special-character selection remain pending. Dedicated room publication on UDP 5732 was reported working by the project owner; this latest settings UI has offline validation only.

Run `MGO2HOST.exe --smoke-options --report <output-directory>` for a hidden settings validation using synthetic values; it does not load saved credentials or networking resources, authenticate, or create a room. It checks Apply, Cancel, Close, invalid values and exact serialization. Hidden Windows control captures may omit standard input text and are not a substitute for visible-window testing.
