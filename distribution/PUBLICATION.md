# Publication policy / 公開方針

Repository / 公開先: [Master-Mkt/MGO2MT](https://github.com/Master-Mkt/MGO2MT)

The public product name is **MGO2MT (MGO2MultiPlatform)**. The implementation and executable releases currently target Windows x64 only. This policy supersedes previous MGO2WIN trial instructions permitting converted game resources in public Release archives.

公開名は**MGO2MT（MGO2MultiPlatform）**です。現実装・実行物はWindows x64専用です。この方針は、過去のMGO2WINお試し版で変換済みゲーム資産のRelease同梱を認めていた説明に優先します。

## What may be published / 公開対象

- Reviewed project implementation source, build/packaging code, synthetic test fixtures and documentation, subject to the source-origin limitations and third-party terms in the accompanying notices.
- Software-only CLIENT (`MGO2MT.exe`), HOST (`MGO2MTHOST.exe`) and EXCV (`MGO2MTEXCV.exe`) packages; required software dependencies; minimal native settings and non-game placeholder material where explicitly inventoried.
- Applicable software license texts and file inventories/checksums. A checksum establishes file identity, not a license or safety/legal certification.

- 出典と第三者条件の制約を確認した実装ソース、ビルド・出力処理、独自の試験データ、説明書。
- ソフトのみのCLIENT（`MGO2MT.exe`）、HOST（`MGO2MTHOST.exe`）、EXCV（`MGO2MTEXCV.exe`）、必要なソフトウェア依存物、最小限の独自設定、明示的に一覧化した原作由来でない仮素材。
- 適用されるソフトウェアの許諾文、ファイル一覧・照合値。照合値はファイルの同一性を確認するもので、利用許諾や安全性・適法性の認証ではありません。

The independent code's unified license remains unselected. This publication scope does not create one. See [LICENSE_STATUS.md](LICENSE_STATUS.md).

独自コードの統一ライセンスは未選定であり、この公開範囲の指定によって新たなライセンスを付与しません。[LICENSE_STATUS.md](LICENSE_STATUS.md)を参照してください。

## What must stay out / 公開除外

The exclusion applies to Git history additions, release attachments, embedded executable payloads, archives, screenshots containing asset payloads submitted as resource substitutes, and user contributions:

- Original game archives/executables and extracted or converted models, textures, motions, UI images, audio, scripts, stage/collision/lighting resources and resource bundles.
- Video, including `movie_01.mp4`, and PS3 font files.
- Communication-constant assets and `network.gnk`; private keys, credentials, tokens, saved login data and personal configuration.
- IDA databases, full disassembly/decompilation exports, proprietary SDK material, private packet captures and analysis/log dumps; copied reference code or tools without the required publication permission.

除外は、Gitへの追加、Release添付、実行ファイルへの埋め込み、アーカイブ、素材の代替として渡す画像、利用者の投稿にも適用します。

- 原ゲームのアーカイブ・実行ファイル、抽出・変換済みのモデル、テクスチャ、モーション、UI画像、音声、スクリプト、地形・当たり判定・ライト資産、資産バンドル。
- `movie_01.mp4`を含む動画、PS3のフォントファイル。
- 通信定数資産と`network.gnk`、秘密鍵、認証情報、トークン、保存済みログイン情報、個人設定。
- IDAデータベース、逆アセンブル／逆コンパイルの全文、非公開SDK資料、私的な通信記録・解析・ログ、公開に必要な許諾のない参考コードやツールのコピー。

Renaming, compression, compiling a resource into an executable, conversion to a native format, or crediting its author does not remove an item from this exclusion. Do not create a public mirror of the private complete development folders or converter output.

名前変更、圧縮、実行物への埋め込み、独自形式への変換、出典記載によって除外対象から外れることはありません。非公開の完全開発フォルダーや変換出力を公開用に複製しないでください。

## Local conversion and release replacement / ローカル変換と旧公開物の更新

EXCV reads authorized user-supplied local inputs and writes local results. No asset-download service or redistribution right is supplied. Input/output manifests and missing-input reports distinguish conversion from a complete runnable installation. Current conversion coverage and prerequisites are described in the [README](README.md).

EXCVは利用権限のある手元の入力からローカル出力を作成します。素材のダウンロードサービスや再配布権は提供しません。入力・出力のmanifestと不足レポートを用い、変換処理と起動に必要な一式の完成を区別します。現時点の対応範囲・不足は[日本語README](README.ja.md)を参照してください。

The release replacement procedure removes legacy compiled Release attachments before uploading the new software-only CLIENT/HOST/EXCV packages. Retaining historical tag/source records must not be described as retaining or re-authorizing an asset-inclusive binary download. Verify the actual remote attachment inventory after replacement. A policy statement alone is not evidence that an upload or removal succeeded, and remote removal cannot retract copies already obtained by other people.

Release更新では、旧compiled添付物を削除してから、新しいソフトのみのCLIENT／HOST／EXCVを公開します。履歴としてタグやソース記録を残す場合も、資産同梱バイナリの維持・再許諾とは扱いません。更新後に実際の添付物一覧を照合します。方針の記載だけでアップロードや削除の成功を主張せず、公開先からの削除によって第三者が取得済みのコピーまで回収できるとは扱いません。

## Release verification and contributions / 公開確認と投稿

Prepare public software packages separately from local full test folders. Inspect the actual archive inventory, embedded converter inputs and dependency notices; record executable versions, sizes and SHA-256 values; run software checks and appropriate synthetic tests. Report missing local inputs honestly. Do not perform live account actions, deploy a service, alter production data or reactivate the original game patch feature as a side effect of publication. Any updater remains a separate project with its own activation approval.

公開用ソフトフォルダーとローカル完全検証フォルダーを分離し、実アーカイブの内容・変換器への埋め込み・依存物の許諾文を確認します。実行物の版・サイズ・SHA-256と、ソフトの確認・適切な独自試験の結果を記録し、ローカル入力不足を隠しません。公開作業に付随して実アカウントの操作、サービスの配備、本番データの変更、原ゲームのパッチ機能の再有効化を行いません。更新ソフトは別の工程・有効化承認として扱います。

For issues or proposed changes, provide a minimal reproduction and redacted diagnostics. Do not submit excluded assets or private records. Include the provenance and applicable terms of third-party code; submitting a contribution does not automatically settle its ownership or license.

Issueや変更提案には最小限の再現手順と秘匿情報を除いた診断結果を使用し、除外対象を投稿しないでください。第三者コードは出典と適用条件を示し、投稿されたことだけで権利・ライセンス確認が済んだとは扱いません。
