# Third-party sources and notices / 出典・第三者ライセンス

These notices describe MGO2MT's software dependencies and implementation references. They do not grant rights in original game content or certify that all source-origin questions have been resolved. The current public packages contain software only; older statements permitting converted game resources, video, fonts or `network.gnk` in public archives are superseded. See [LICENSE_STATUS.md](LICENSE_STATUS.md) and [PUBLICATION.md](PUBLICATION.md).

本書はMGO2MTのソフトウェア依存物と実装時の参照先を示します。原作素材の権利を許諾せず、出典・権利の確認完了を保証しません。現在の公開物はソフトウェアのみであり、変換資産、動画、フォント、`network.gnk`の公開同梱を認める旧記載は撤回しています。[LICENSE_STATUS.md](LICENSE_STATUS.md)と[PUBLICATION.md](PUBLICATION.md)を併せて確認してください。

The project's own unified license remains unselected. This does not replace or cancel licenses applying to third-party components. Retain the actual copyright, license and notice files accompanying each package. A tool's license does not automatically apply to the game files it processes.

独自コードの統一ライセンスは未選定ですが、第三者コンポーネントの条件を変更・無効化するものではありません。各パッケージに付随する著作権・許諾文・noticeを保持してください。変換ツールのライセンスが処理対象のゲーム内容に自動適用されることはありません。

## Software dependencies / ソフトウェア依存物

CLIENT and HOST use native C++ and Windows graphics, audio, networking and security interfaces. EXCV is a packaged Python program and includes an interpreter and collected Python dependencies. Statements in older documents that Python and conversion programs were never bundled are not applicable to EXCV.

CLIENT／HOSTはC++とWindowsの描画・音声・通信・保護機能を使用します。EXCVはPython実行環境と収集された依存物を含むプログラムです。過去の「Pythonや変換器を同梱しない」という説明をEXCVへ適用しません。

The versions below identify the inspected local EXCV build environment as of 2026-09-21, not a guarantee that every listed build package is embedded in every executable. Final package inventories and their license folders determine the delivered versions. Build-only notices may also be retained. The inspection is not a complete audit of every transitive dependency.

次の版は2026-09-21時点で確認したローカルEXCVビルド環境のものです。すべてが各実行ファイルへ内蔵されるという意味ではなく、配布版の構成表と許諾文を優先します。ビルド専用依存物のnoticeも保持する場合があります。間接依存を含めた全面的な監査完了は主張しません。

| Component / 依存物 | Use and terms / 用途・条件 |
| --- | --- |
| [Python](https://docs.python.org/3/license.html), Tcl/Tk | EXCV interpreter, standard library and GUI. Preserve Python's LICENSE and Tcl/Tk's `license.terms`, including incorporated components' terms. CLIENT/HOST do not require this interpreter. / EXCVの実行環境・標準ライブラリ・GUI。PythonとTcl/Tkの許諾文・付随条件を保持。CLIENT／HOSTの依存ではありません。 |
| Pillow 12.3.0 | Local image conversion. The inspected LICENSE identifies MIT-CMU and additional component notices. Preserve the complete included LICENSE. / 画像変換。確認済みLICENSEはMIT-CMUと追加コンポーネント表記を含み、全文を保持します。 |
| cryptography 50.0.1 | Python conversion helpers. The inspected LICENSE permits selection of Apache-2.0 or BSD terms; retain LICENSE, LICENSE.APACHE and LICENSE.BSD and applicable dependency notices. / Python側処理。確認した許諾はApache-2.0またはBSDを選択する方式で、関係する全文・依存物表記を保持します。 |
| cffi 2.1.1, pycparser 3.0 | Collected Python/build dependencies. Their own installed LICENSE files apply. / Python・ビルド依存。各インストール物のLICENSEを適用します。 |
| [PyInstaller](https://pyinstaller.org/en/stable/license.html) 6.22.3 | Packages EXCV. GPL terms include a bootloader exception; certain files have separate terms, including Apache-2.0. The exception does not remove the licenses of packaged dependencies. Preserve COPYING and relevant notices. / EXCVのパッケージング。bootloader例外付きGPLと一部ファイルの別条件があり、内蔵依存物のライセンスは別途適用されます。 |
| pyinstaller-hooks-contrib 2026.7, altgraph 0.17.5, pefile 2024.8.26, pywin32-ctypes 0.2.3, packaging 26.3, setuptools 84.0.0 | Packaging/build environment. Retain their license texts where collected; inclusion of a notice alone does not mean the whole tool is shipped. / パッケージング・ビルド環境。収集した許諾文を保持し、noticeの存在だけでツール全体の同梱を意味しません。 |
| [Microsoft Visual Studio and Windows SDK](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170) | Compiler/runtime and D3D11, D3DCompiler, XAudio2, WinHTTP, Winsock, BCrypt, Crypt32, GDI, WIC and Media Foundation interfaces. Microsoft terms and the actual build's redistribution conditions apply. Windows system DLLs and SDK installers are not supplied as game resources. / コンパイラー・実行環境・Windows API。使用版の条件に従い、OSのDLLやSDKインストーラーをゲーム素材として配布しません。 |
| [DirectXMath](https://github.com/microsoft/DirectXMath/blob/main/LICENSE) | Rendering math headers. The upstream project identifies an MIT license, copyright Microsoft Corporation. Preserve the notice applicable to the actual SDK/header version used; the upstream link alone is not a substitute. / 描画計算ヘッダー。公開元はMicrosoft CorporationのMIT表記。使用した版の許諾文を保持します。 |
| CMake | Build generation and test tooling; BSD 3-Clause. Not a client/HOST runtime dependency. / ビルド生成・試験用、BSD 3-Clause。CLIENT／HOSTの実行依存ではありません。 |

## External tools and historical preparation / 外部ツール・過去の変換工程

| Source / 出典 | Use and status / 用途・確認状況 |
| --- | --- |
| [vgmstream r2117](https://github.com/vgmstream/vgmstream/releases/tag/r2117) | Historical/local MTA2 and background-audio decoding; EXCV can use a decoder supplied separately by the user. `licenses/vgmstream-r2117-COPYING.txt` retains the inspected ISC-style permission and copyright notices. The decoder executable and its DLLs are not bundled. / 過去・ローカルの音声変換。EXCVでは利用者が別途指定。確認したCOPYINGを保持し、デコーダー本体・DLLは非同梱です。 |
| FFmpeg and other dependencies of an external decoder | Terms depend on the actual decoder build. vgmstream's COPYING does not license every file in a downloaded decoder archive. See [FFmpeg legal information](https://ffmpeg.org/legal.html). These decoder dependencies are not supplied by this project. / 外部デコーダーの実構成ごとに条件が異なります。vgmstreamの許諾をアーカイブ全体へ広げず、これらの依存物は提供しません。 |
| [Solid4 / local Drebin checkout](https://github.com/GHzGangster/Drebin) | Historical MTA2 comparison. The local README declares MIT, copyright 2015 GHzGangster and SaveMGO, and credits daemon1, tbg and OrangeC for AudioTool work. Its exact notice is retained in `licenses/Solid4-local-README.txt`. The current native client/HOST and SSW2 converter do not invoke or link the Drebin decoder. / MTA2の比較資料。MIT表記、著作権者およびAudioTool貢献者を保持。現CLIENT／HOST・SSW2変換ではDrebinデコーダーを呼び出し・リンクしません。 |
| NumPy | Used in earlier local BGM downmix preparation; BSD 3-Clause. This historical use is not a claim that NumPy is a current client/HOST dependency. Any converter package that includes it must retain its applicable notices. / 過去のローカルBGMミックスで使用、BSD 3-Clause。現CLIENT／HOSTの依存という意味ではなく、変換器へ含める場合は該当許諾文を保持します。 |
| Hex-Rays IDA 9.1 / IDALib and processor modules | Proprietary local static-analysis tools. Not redistributed and not required to run CLIENT, HOST or packaged EXCV. / 商用のローカル静的解析ツール。再配布せず、配布ソフトの実行に必要ありません。 |

## Format and interoperability references / 形式・互換処理の参照先

These references informed parts of the current implementation. Excluding an upstream binary or writing a separate parser is not, by itself, a completed permission or source-origin review. The reference files and private analysis records are not distributed. EXCV does contain this project's conversion implementation, so the former blanket statement that all reference-informed converters are excluded is no longer accurate.

次の資料は実装の一部で参照しました。参考元のバイナリを除外したことや別の読み込み処理を記述したことだけで、許諾・出典確認の完了とは扱いません。参考資料のコピーと非公開の解析記録は配布しません。一方、EXCVには本プロジェクトの変換実装を含むため、過去の「参照に基づく変換器もすべて除外」という説明は現状に合いません。

| Reference / 参照先 | Implementation context and unresolved scope / 用途・未確認範囲 |
| --- | --- |
| [Jayveer/MGS-MDN-Noesis](https://github.com/Jayveer/MGS-MDN-Noesis) | MDN meshes/bones/materials, TXN/DLZ/DLD and MTCM/MTAR format references for local readers and converters. The reference project's README also credits JinMar. No explicit license was identified in the previously inspected root/README and five reference files; this is limited evidence, not a statement that every upstream file is unlicensed. Exact local revisions and permission for reference-derived portions require further review. No Noesis or upstream plugin binary is bundled. / モデル・画像・モーション形式の参考。READMEのJinMarへの謝辞も記録。確認済みの限定範囲では明示許諾を見つけておらず、全ファイル無許諾と断定しません。利用版と参照に基づく部分の条件は引き続き確認が必要です。 |
| Solideye | Local SLOT/config/DAR and related archive-structure references. The exact local version, provenance and applicable terms are not fully verified. Upstream tools/reference copies are not bundled; their influence on current converters is recorded rather than treated as cleared. / SLOT・設定・DARなどの構造参照。ローカル版・出典・条件は確認未完。元ツールや参照コピーは同梱せず、変換器への参照関係を明記します。 |
| HavenPX / HavenStudio StageEditor | User-provided secondary GCX, GEOM, MDN, TXN and LT3 structure references. The reference application/source is not included. Redistribution permission and the applicable terms of the inspected local reference have not been established. / 利用者提供の地形・スクリプト・モデル・照明形式の補助資料。元アプリ・ソースは非同梱で、確認した資料の条件・再配布許諾は未確定です。 |
| OpenMGO2 / NomadPX local candidate sources | Authentication, transport, room rules, character/lobby and host-setting contracts were checked against user-provided PHP/Java sources and original behavior where established. The native adapter is source-informed; no completed clean-room claim is made. Reference sources/classes, Java/Netty runtimes, oracle binaries and private packet fixtures are not included. Permission questions for the inspected references remain distinct from runtime interoperability. / PHP／Java候補を認証・通信・部屋・HOST設定の照合に使用。参照に基づく実装であり、クリーンルーム完了とは扱いません。参照コード・クラス・実行環境・比較用バイナリ・私的通信試料は非同梱です。 |
| [RFC 8489](https://www.rfc-editor.org/rfc/rfc8489.html) | STUN message-format reference for the native Windows adapter. No RFC code snippets or coturn executable/source are bundled. An external STUN/service endpoint remains independently operated. / Windows側STUN処理の形式資料。RFCのコード断片やcoturn本体・ソースは同梱せず、外部サービスの運営権限を与えません。 |
| Earlier local crypto helper and legacy gcx.exe | Historical preparation/secondary references. Their origin and license review is incomplete. The legacy tools, private keys and unrestricted analysis exports are not supplied. This does not declare every current conversion routine legally cleared. / 過去の準備・補助参照。出典・条件は未確認部分があり、旧ツール・秘密鍵・解析全文は非公開です。現変換処理すべての法的確認済み宣言ではありません。 |

`network.gnk` contains reference-derived communication constants for local interoperability. This project does not claim that those exact tables were extracted from the original ELF or that redistribution rights are established. Neither the file, the tables nor private reference/oracle material belongs in public source, executable payloads or release archives. Any local import is the user's separately authorized preparation, not a license granted by MGO2MT.

`network.gnk`はローカル互換処理向けの参照由来の通信定数を含みます。同一の表を原ELFから抽出できたとも、再配布権を確認済みとも扱いません。GNK・定数表・私的な参照／比較資料は公開ソース、実行物への埋め込み、Releaseへ含めません。ローカルへの取込みにも別途利用権限が必要です。

## Game content, behavior and external services / 原作素材・動作・外部サービス

Original MGO2/MGS4 models, textures, animation, icons, UI layouts, scripts, stage/collision/lighting data, fonts, video, sound and music remain subject to their respective rights, including after conversion to GWA/GWC/GWM/GWMOT/M2AN/M2PV/DDS/PNG or another format. They are not public package contents. Windows-installed fonts used for native text are distinct from PS3 font inputs; no PS3 font is supplied.

原MGO2／MGS4のモデル、テクスチャ、モーション、アイコン、画面配置、スクリプト、地形・当たり判定・照明、フォント、動画、音声、音楽は、GWA／GWC／GWM／GWMOT／M2AN／M2PV／DDS／PNGなどへ変換しても各権利の対象です。公開物には含めません。独自文字描画で利用するWindowsのインストール済みフォントと、PS3のフォント入力は別であり、PS3フォントを提供しません。

Native GCX/LA2 adapters, rendering/animation behavior and the SSWF/SSW2 audio converter were informed by disassembly and local experiments. Original coefficient/table provenance and reference-derived behavior must not be represented as a finished legal or clean-room review. Original DSP, material, motion and gameplay parity remain incomplete; attribution is not proof of equivalence or permission.

GCX／LA2動作、描画・モーション処理、SSWF／SSW2音声変換には逆アセンブルとローカル実験を根拠とする部分があります。元の係数・表や参照由来の動作について、法的・クリーンルーム確認の完了を主張しません。原DSP・材質・動作・ゲーム処理の完全一致も未達で、出典記載だけで一致や許諾を証明しません。

OpenMGO2 policy text and service responses retain their own rights and conditions. The project uses Windows interfaces to communicate with user-selected endpoints; it does not license those services, their policy text or accounts. Live account/room actions are performed by the user. Independent service operation, names and references do not imply endorsement or a trademark license.

OpenMGO2の規約本文やサービス応答にも各自の権利・条件があります。Windowsの機能で利用者指定先へ接続する実装であり、外部サービス・本文・アカウントの権利を許諾しません。実アカウント・部屋の操作は利用者が行い、サービス名の記載や参照は公認・商標許諾を意味しません。
