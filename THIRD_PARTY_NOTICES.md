# Trial release scope update / お試し版の同梱範囲更新

This trial includes converted runtime resources (GWA, GWC, M2AN, M2PV, DDS, GNK and compiled GWP). Historical local-only/exclusion statements below describe earlier research packages or source-only exports, not this Release ZIP. Original archives and original script/model/audio files, reference tools and their binaries are not bundled. Source attributions below are retained; this update does not establish or grant redistribution rights.

今回のRelease ZIPには変換済み実行資産を含みます。以下の過去の「ローカル専用」「公開除外」は旧工程やソース書出しの説明です。原本・参考ツール本体は同梱しません。出典は保持し、権利関係の確認済み宣言や新たな許諾を与えるものではありません。

# Third-party sources and notices / 出典・第三者ライセンス

Icon conversion uses Pillow 12.3.0 offline; its installed LICENSE identifies MIT-CMU and includes additional component notices. DAR reading reuses the local Solideye-informed reader, whose upstream version/terms remain unverified. QAR/TXN/DLD reuse the existing Noesis-informed readers described below. These tools and references are not bundled. / アイコン変換ではPillow 12.3.0をオフライン利用しました。インストール済みLICENSEでMIT-CMUと追加依存のnoticeを確認。DARは既存Solideye構造参照reader（元版・条件の確認は未完）、QAR/TXN/DLDは下記Noesis構造参照readerを再利用しています。ツール・参照元の実行物は同梱しません。

2026-09-13 local weapon-card update:38 original weapon images are recovered through the reviewed ELF ID→LA2 mapping and local TXN/DCI texture references. The PNGs retain their game-derived rights; this project grants no new rights over them. Their index and individual digests are recorded in the local GWP/package. Windows WIC reads them at runtime; no third-party image-decoder DLL or extraction tool is bundled. / ローカル版では原ELFの武器ID→LA2とTXN/DCIの参照を照合し、通常武器38種類の元画像をPNG化しました。権利は原ゲーム由来のままで、本プロジェクトから新しい許諾を与えません。indexと各画像のhashをGWP・出力manifestへ記録。実行時はWindows標準WICを使用し、第三者decoder DLLや抽出ツールは同梱しません。

Reviewed / 確認日: 2026-09-08. Attribution is not a substitute for permission. A build tool's license does not automatically license generated game content. / 出典記載は配布許可の代わりではありません。変換ツールのライセンスはゲーム内容に自動適用されません。

| Source / 出典 | Use / 用途 | License and packaging / ライセンス・同梱 |
|---|---|---|
| [vgmstream r2117](https://github.com/vgmstream/vgmstream/releases/tag/r2117) | Historical title BGM → WAV decoding. / タイトルBGMのWAV化に使用。 | Upstream [COPYING](https://github.com/vgmstream/vgmstream/blob/r2117/COPYING) contains an ISC-style permission notice and multiple copyright notices. Exact text retained in `licenses/vgmstream-r2117-COPYING.txt`. Decoder EXE/DLLs are not bundled. / 許諾文原文を保存。変換exe/DLLは同梱しません。 |
| FFmpeg and other libraries in the vgmstream Windows download | Transitive tools in that downloaded archive; not runtime dependencies of MGO2WIN. / vgmstream取得物の依存ライブラリ。MGO2WINの実行依存ではありません。 | Their licenses are separate; vgmstream COPYING alone is insufficient to redistribute the whole archive. No redistribution of those DLLs in these outputs. See [FFmpeg legal information](https://ffmpeg.org/legal.html). / アーカイブ全体を単一ライセンスとは扱わず、DLLを再配布しません。 |
| [Jayveer/MGS-MDN-Noesis](https://github.com/Jayveer/MGS-MDN-Noesis) | Format reference for DLZ/DLD/TXN; bounded Python readers were written after consulting these structures. / DLZ/DLD/TXN構造の参考。参照後に境界検査付きreaderを作成。 | No explicit license identified in the inspected root/README and five reference files. This is unresolved, not a claim that every file is unlicensed. The five copied references and dependent conversion scripts are omitted from the public review export. / 確認範囲で明示ライセンス未発見。引用元コピーと関連変換スクリプトは公開候補から除外。 |
| [GHzGangster/SaveMGO Solid4 (local Drebin checkout)](https://github.com/GHzGangster/Drebin) | Historical MTA2 investigation; this title build does not invoke or link Drebin. / 過去のMTA2調査。今回のビルドでは呼出・リンクなし。 | Local README declares MIT, copyright 2015 GHzGangster, SaveMGO, with daemon1/tbg/OrangeC credits. Exact local README retained as a notice. Origin URL checked against the local Git remote. No Drebin code/binary is bundled. / ローカルREADMEでMITを確認、原文を保存。URLはローカルGit remoteと照合。コード・実行物は非同梱。 |
| [Hex-Rays IDA 9.1 / IDALib](https://docs.hex-rays.com/user-guide/idalib) | Static analysis of a private working database. / 作業DBの静的解析。 | Proprietary licensed tooling; not redistributed and not needed to build/run a prepared title package. / 商用ツール。再配布せず、準備済みタイトルのビルド・実行には不要。 |
| [Microsoft Visual Studio / Windows SDK](https://learn.microsoft.com/en-us/cpp/windows/deployment-in-visual-cpp) | C++ compiler, D3D11, D3DCompiler, XAudio2, CNG/BCrypt and OS interfaces. / C++ビルド・Windows描画・音声・hash API。 | Microsoft terms apply. Release package uses static MSVC runtime; system DLLs and SDK installers are not copied. Verify the build tool license and applicable redistribution rights before a public binary release. / Release版はMSVCランタイムを静的リンク。システムDLL・SDKはコピーせず、公開バイナリ配布時に適用条件を確認。 |
| [CMake](https://cmake.org/licensing/) | Build generation/tests. / ビルド・テスト。 | BSD 3-Clause; tool not bundled. / BSD 3-Clause、ツール非同梱。 |
| [Python](https://docs.python.org/3/license.html) | GWP validation, packaging, analysis. / GWP検証・出力・解析。 | PSF license and included components' terms. Only standard library modules are needed by the new packager. Interpreter not bundled. / PSF等の条件。今回の出力ツールは標準ライブラリのみ、Pythonは非同梱。 |
| Prior local stage-crypto helper and legacy gcx.exe | Historical preparation of local data/secondary text references. Neither is called by the new packager. / 過去のローカル資産準備・補助参照。新出力処理では不使用。 | Origin/license review incomplete; no helper, key material or legacy decompiler is exported. / 出典・条件の確認は未完了。helper、鍵情報、旧逆コンパイラーは書き出しません。 |
| MGO2 executable, data, logos, textures, audio and scripts | User-provided research inputs and local generated assets. / ユーザー提供の研究入力とローカル生成資産。 | Not licensed by this project. Not included in a public source export. / 本プロジェクトから利用許諾せず、公開用ソース出力に含めません。 |

The generated title EXE links the project's C++ modules and Microsoft libraries. It does not link vgmstream, FFmpeg, Noesis, Drebin or IDA. This statement concerns this build graph, not all past research tools. / タイトルexeのリンク対象は本プロジェクトのC++実装とMicrosoftライブラリです。過去の研究ツールすべてが不要・不使用だったという意味ではありません。

The native GCX/LA2 behavior implementation was informed by disassembly and experiments. Public release needs a source-origin review; do not describe the current work as a completed clean-room implementation. / GCX/LA2動作の実装は逆アセンブルと実験を根拠としています。公開前の出典レビューが必要であり、完了したクリーンルーム実装とは称しません。

Host networking / ホスト通信: `host_protocol.cpp` and `host_session.cpp` are native implementations informed by the local retail PPC transport routines (264C78/2666C8, EFD840, 270E00, 281AF8, 27E8B0) and the reviewed OpenMGO2 server. Windows BCrypt supplies MD5 and random bytes; the bounded 512-byte-window decompressor is implemented locally, with no additional third-party codec dependency. Private legacy packet fixtures used for comparison are excluded from source exports and packages. / 元PPCとサーバー実装を照合して作成したネイティブ通信処理です。比較用の私的パケットやIDAデータベースは配布しません。実ホストでの互換性とゲームプレイ全体の再現を保証するものではありません。

## START sound and loading screen / START音とロード画面

The START converter uses reviewed PPU/SPU instructions and coefficient tables from the local game binary. It decodes SSWF PCM16BE and SSW2 float predictors, preserves 23 note events, and produces a native stereo mix. It does not invoke or incorporate the Drebin decoder. Drebin AudioTool/MTA2 were consulted for comparison; the actual SSW2 SPU coefficients, sample ordering and compressed silence differ. IDA's installed SPU processor module was used for local instruction decoding and is not redistributed.

START音はローカルの元ゲームのPPU/SPU命令と係数表を根拠に変換します。Drebinは比較参照のみで、デコーダーの呼出・同梱はありません。ロード画面は元のloading_MGOレイアウト・画像を変換します。これらのゲーム由来データは公開用ソースに含めません。変換した音声・画面の原機との完全一致は未確認です。

## OpenMGO2 policy and interface audio / 同意画面と操作音

The policy is fetched at runtime from the user-designated OpenMGO2 HTTPS endpoint using Microsoft WinHTTP. GDI renders plain UTF-8 text with an installed Windows font; no font file, HTML engine or external networking library is bundled. The original l_free_2_bg frame and six lobby texture atlases are converted to native M2PV/DDS with a static pose and approximate special blending; native text is drawn over it. No claim of original LA2 visual parity is made.

同意画面の元動作はPPU AB30AC / AB24A0 / AB3208とPPC命令を参照。操作音93/94は元init_n/mgs4int_nt.sspを既存ローカルhelperで復号し、前工程の独自SSW2デコーダーを再利用してGWAへ変換しました。CC121の既定状態とCC91のPS3残響は完全再現していません。変換器の出典レビューは引き続き必要です。Windowsフォント・IDA・旧helper・元音声バンクは再配布しません。取得したOpenMGO2本文の権利を本プロジェクトのコードライセンスで許諾するものではありません。

Original lobby music / 元ロビーBGM: `bgm_mgo_lobby01` is extracted from the local `bgm_n.dat`, decoded by the same vgmstream r2117 build listed above, then channels 0–3 and original loop frames are stored in GWA. The conversion uses NumPy (BSD 3-Clause); NumPy and the decoder are not bundled in the runtime. Channel/layer routing is a native implementation choice with PS3 parity pending. 元BGMの変換にも上記vgmstreamを使用し、ループ情報を保持します。元音楽・背景画像はローカル専用です。

Animated background / 動く背景: the original `bg_anime.la2` is converted with its node hierarchy and timed events. PPU A2DE84 starts rear/front instances and thirteen loop events; 24D078 supplies wait and loop-clock-reset semantics. Images include direct TXN BC1 payloads and DLD atlases. A native approximation handles the special shader on node 88. Original scene-change triggers and full RSX composition remain pending. 背景の元画像・時間指定を流用し、実行時に位置と色を更新します。


Login frame / ログイン画面: PPU AA5B00 selects layout 0x4D179B and setup/entry events 0x6A367F / 0xC874A6. The matching local `l_shib_y03_01_bg_login.la2` and six original texture atlases are converted using the existing LA2/TXN/DLD readers. The entry endpoint pose and special-shader approximations are reused. The native form uses Windows GDI and installed MS Gothic; no new font or third-party library is bundled. ID/password input rules and screen routing are native host choices, not a claim of original server compatibility. 元データと変換資産はローカル専用です。入力処理・同意YESからの直接遷移は今回のWindows実装で、元の全ログイン手順を再現したという意味ではありません。

Credential persistence / 入力保存: Windows Crypt32 DPAPI protects only the current user’s selected credentials. The Windows system library is linked; no encryption library is redistributed. パスワードはIDのみ保存では除外し、保存しない設定の適用で保存データを削除します。

Authentication adapter / 認証アダプター: Newly written Windows C++ uses Microsoft WinHTTP, BCrypt (legacy MD5 compatibility) and current-user DPAPI. The request/reply contract was checked against the user-authorized local OpenMGO2 `web/ps3-root/index.php`, `mgo2/ps3-login.php`, and `register.php` sources. PHP source is neither copied into the executable nor bundled. This records interoperability evidence, not a license grant for the reference checkout. 元の認証プロトコル全体の一致は未確認で、OpenMGO2向けに新規実装した接続処理です。

Port settings / ポート設定: Newly written Windows controls/configuration and Microsoft Winsock2 exclusive UDP bind. No new third-party library or original UPnP implementation is bundled. Default5730 and the lower-bound guard are evidenced by original PPU AA19E0–AA19F0; native automatic fallback is a host choice. The existing l_free_2_bg frame, bg_anime motion, lobby BGM and cue93/94 are reused. 外部到達性や原機のNAT判定との一致は未確認です。

STUN adapter / STUN接続: Newly written C++ using Windows Winsock2 and BCrypt. Wire-format reference: [RFC8489](https://www.rfc-editor.org/rfc/rfc8489.html), especially sections5,6 and14. No RFC source-code snippets or coturn source/binary are bundled; the existing OpenMGO2-hosted coturn is a remote dependency. This implements unauthenticated IPv4 Binding diagnostics, not TURN allocation, peer connectivity, original PS3 NAT classification or UPnP.


## Native character-list adapter / キャラクター一覧

Stage restoration references / ステージ復旧の参照: the user-provided local HavenPX/HavenStudio StageEditor supplied secondary GCX, GEOM, MDN, TXN and LT3 structure references. The bounded native readers are independently written; selected lighting/audio behavior was checked against the user's original PPC binary. No HavenPX code or executable is bundled, and redistribution permission for that local reference has not been established. / ローカルStageEditorの構造を参照し、独立した読み込み処理を実装。元PPCで確認した範囲と推定を調査記録で区別しています。参照ツール自体は同梱せず、その再配布許諾を取得したとは扱いません。

The five n022a environmental tracks are decoded using the vgmstream r2117 build credited above, retaining all four channels and original loop points in GWA. GCX cue bindings are preserved; the current scene preview exposes manual audition, while VLM/SDS-driven automatic region switching remains pending. / 環境音5種は上記vgmstreamで変換し、4チャンネルと元ループを保持。現在は試聴操作までで、領域による自動切替は未実装です。元音源、テクスチャ、ステージ形状、派生した照明・当たり判定データはローカル専用で、公開ソースには含めません。

Player-slot controls and the three-second Backspace confirmation are newly written from the user's stated UI requirements. They are not claimed as an IDA-verified recreation. The slot controls introduce no additional third-party library. / スロット操作・3秒長押し確認はユーザー指定の仕様を元に新規実装し、元バイナリとの動作一致は主張しません。

Interoperability reference: the user-authorized local NomadPX/OpenMGO2 candidate sources `packet/Packet.java`, `PacketDecoder.java`, `PacketEncoder.java`, `helper/Hub.java`, `helper/Users.java`, `util/Packets.java`, `util/Util.java`, `crypto/Crypto.java` and `crypto/Constants.java`, plus the PS3-compatible PHP login handler. This is a source-informed native adapter, not a claimed original-ELF decompilation or completed clean-room implementation. A newly written C++ expanded-table block transform is checked against unchanged Java reference classes using synthetic vectors. BCrypt supplies HMAC-MD5 and Winsock2 supplies TCP; no Java, Netty or reference class is bundled in the EXE.

`network.gnk` stores reference-derived packet/authentication round tables and legacy wire constants for local interoperability. Exact table byte sequences were not found in the original ELF; no original-binary extraction claim is made. Reference redistribution rights are not established here. GNK, the local conversion helper, oracle binaries and reference source copies are excluded from the source-only review export; public release remains subject to origin/license review.

OpenMGO2/NomadPXのローカル候補ソースを参照した新規Windowsアダプターです。定数表はGNKへ変換し、出典ファイルと変換物のハッシュをローカルで記録します。原ELFから同じ定数表を抽出できたとは扱いません。元Javaクラス・Netty・JavaランタイムはEXEに含みません。GNKの公開許諾は未確認で、公開用ソース書き出しから除外します。キャラクター一覧の配置は新規のWindows UIで、元キャラクター画面の完全再現ではありません。

## Native model preview / 3D表示

The local Jayveer MGS-MDN-Noesis MDN, mesh, face, bone, material and TXN readers informed a newly written offline converter. Source paths and SHA-256 are recorded in the local conversion report. Existing local stage decryption and DLZ/TXN helpers prepare inputs; they are not called by the packager or executable. Original male chest, trousers, hands, face and boots are assembled by checking common bind-bone translations against mgo_base_bounding. GWM1 embeds geometry and seven original diffuse textures; normals are rebuilt. The new D3D11 renderer uses Microsoft DirectXMath and Windows graphics libraries, with no Noesis binary or shader code linked. Original skinning, material parity and account-specific appearance are pending.

ローカルのNoesis用MDN/TXN資料を形式参照として利用し、独自変換器を新規実装しています。元モデル・テクスチャ・GWM・復号helper・変換器は公開用ソース書き出しに含めません。参照コードの版と再配布条件は確認未完了で、公開許諾を主張しません。骨の位置を照合した固定外見であり、元機のアニメーションや材質の再現とは扱いません。

## Character appearance and motion update / 外見・モーション更新

The local Solideye SLOT/config readers informed the bounded SLOT section extractor. The local MGS-MDN-Noesis MTCM/MTAR readers informed motion decoding. Both are offline format references; no binaries or original code are linked into the executable. Exact local files/hashes, original ELF table evidence (8D29F0, 8D2BC0, 8CED08), SLOT/DLD/TXN/DCI inputs and derived outputs are recorded in the local phase19 provenance. Their local versions and redistribution terms remain unverified; their source/binaries and the private converters are excluded from the publication review export. GWC1 v2/v3 contains game-derived meshes, textures, appearance mappings and sampled animation and must stay out of the source-only export. The new renderer uses Microsoft DirectXMath, D3D11 and a native approximate material shader. The executable does not read MDN, MTAR, SLOT, DLD or TXN.

ローカルSolideyeをSLOT区画の形式参照、Noesis用MTCM/MTAR資料をモーションの形式参照として利用しました。参照ファイルのhashとELFの外見対応表、元資産・変換出力はphase19のローカル記録で追跡します。両ツールの版・再配布条件は未確認で、バイナリや元コードはEXEにリンクせず、変換器・GWC・元資産も公開用書き出しから除外します。材質は独自の近似処理で、元機との完全一致は主張しません。

Character voice audition / キャラクター音声試聴: 16 short samples are converted locally from original on_slot_som/sof banks to the existing GWA1 format with the previously reviewed original SSW2 decoder. Original game audio is local-only and excluded from source exports. Native semitone playback is an implementation choice; no PS3 DSP or creation-screen phrase parity is claimed. / 元音声16件はローカル専用で公開対象外。新規形式・外部codecは追加していません。

## Stage BGM conversion and lighting follow-up / ステージBGMと照明の追加対応

vgmstream r2117 (COPYING retained above) decodes the local Konami MTA2 BGM once during private preparation. NumPy performs an independently chosen stereo downmix of each four-channel bank. The executable links only the Windows XAudio2 playback path; no decoder, NumPy or Python runtime is bundled. Standard PCM16 WAV and RIFF smpl metadata are used instead of a private music codec. Original music remains game-derived content; these source notices do not grant rights to it.

元BGMはローカル準備時にvgmstream r2117で復号し、4chバンク単位の独自ステレオミックスをNumPyで作成しています。実行時はWindowsのXAudio2でPCM WAVを再生し、変換ツールは同梱しません。元のBGMセット番号とバンクの対応やPS3ミキサーの完全再現は未確認です。

LT3 point-light activation/range checks were cross-checked against retail PPC at 0x125E90–0x125FAC. n022a's 31 point records have 0x200, whereas this original dynamic evaluation requires 0x100 and rejects 0x8000; the implementation preserves that distinction. Native Lambert diffuse and temporary-light lifetime are implementation choices, not claims of full original lighting parity. HavenStudio remains a local format reference, not a bundled runtime.

Scene replication update / 配置共有更新 (2026-09-12): Item-box models and their common diffuse images are converted from the user's local game files. The stage n022a MDNs refer to texture keys recovered from n012a's ibox_item_small.txn/DLD. No reference-tool binaries or original archives are included. Retail PPC was used to verify channel 592, coordinate quantization, the separate destructible-object snapshot protocol, and light key/disable flags. Native scene composition remains partial; this local package is not a public release. / 原ゲーム由来の派生素材を含むローカル試験版です。破壊actor登録順・照明更新の未接続部分があり、全同期の完成版ではありません。


## Dedicated hosting and restriction controls / 専用ホストと武器制限 (2026-09-12)

The dedicated host, briefing tracker, common-settings serialization and restriction editor are newly written C++ components. Interoperability field positions, bit masks and category groupings were checked against the user-authorized local NomadPX/OpenMGO2 candidate, especially `HostGameEnvFactory.java`, `Hosts.java` and `HostGameEnvWire.java`, and original PPC where established. The reference sources and their compiled classes are not bundled. Existing licensing uncertainty for those local references remains unchanged; no clean-room or new license grant is claimed. Ordinary weapon eligibility/DP annotations are distinct from host restriction bits; recovered runtime tables and original weapon images are not part of this source update.

専用ホスト、待機管理、共通設定の送信、武器制限画面は新規C++実装です。参照したローカルNomadPX/OpenMGO2候補と、確認できた範囲の元PPCで配置・ビット・分類を照合しました。元のJavaソースやクラスは同梱せず、参照物への新しい許諾やクリーンルーム実装を主張しません。装備資格・DP表示とホスト制限は別の情報として扱い、実行用の復旧武器表や元画像はこのソース更新に含めません。

Folder-shaped tabs, alternating row shades, orange focus guides and the two-column lobby layout are native GDI implementations informed by user-provided screenshots. The screenshots themselves are not included. Source-only UI tests use independently specified synthetic values. Dedicated hosting adds no third-party runtime beyond the Windows libraries already listed.

フォルダー形タブ・行の濃淡・オレンジのクロス・左右2列のロビーは、利用者の参考画像を元にGDIで描く独自実装です。参考画像自体は含めません。公開ソースの画面検証には独自の模擬値を使い、既出のWindowsライブラリ以外の実行環境は追加していません。

## Player input and sampled motion (2026-09-12) / 入力と原モーション

Input, posture state, menu routing and shared-depth rendering are newly written native C++. GWMOT contains locally sampled original MTAR clips; this game-derived data has no new license grant. The offline decoder uses the Jayveer MGS-MDN-Noesis format reference already attributed above; no reference binary or Python runtime is bundled. Bone poses and source root travel support the native action choices, but original MTSQ/C++ dispatch and weapon layers remain unverified.

入力・姿勢管理・画面切替・深度共有は新規C++実装です。GWMOTは原MTAR由来で、素材への新たな許諾を主張しません。既述のJayveer形式資料を参照したローカル変換で、参照ツール本体は同梱せず、原版の動作選択条件の完全復旧も主張しません。


## Native round preparation and original spawn data / 出撃準備と原配置 (2026-09-13)

GWCBv2 readiness, the host DP ledger, transactional loadout/spawn coordination and their UI are newly written C++. The local n022a.tdm-spawns.cfg contains 128 original GCX/GEOM-derived records verified against the user's local MGO2 ELF through IDA 9.1; no new rights in those records or the original assets are claimed. The original 64-bit selector was checked against PPC. Conservative vertical capsule landing and Windows host RNG seeding are native implementation choices, not a claim of original physics or seed parity. No IDA database, original ELF, credentials or reference tool is bundled with this addition.

出撃準備・DP管理・武器承認・配置確定と画面は新規C++実装です。配置ファイルの128行は利用者のローカル原GCX／GEOMと現MGO2 ELFをIDA9.1で照合したもので、原素材への新たな権利を主張しません。原64bit選択処理と、Windows側の乱数初期化・垂直着地の設計を区別しています。IDAデータベース、元ELF、認証情報、参照ツールを同梱していません。

Menu sound update / メニュー効果音更新（2026-09-13）: Cancel92, Confirm93 and Cursor94 use the original init_n/mgs4int_nt.ssp interface bank. IDA9.1 read-only current MGO2 analysis AA7DB4 → 988404/989D0C → 482B0 and PPC bytes establish the three call paths. Only92 was newly converted with the existing independently written SSW2 decoder;93/94 bytes are unchanged. The Music selector and Windows action policy are newly written. Original PS3 reset defaults/reverb and exact DSP parity remain incomplete; no original executable, SDK, IDA or decryption helper is included. 元のメニュー音を再利用し、今回は取消92のみ変換。原音の権利・元ツールのライセンスを本プロジェクトが新たに許諾するものではありません。
