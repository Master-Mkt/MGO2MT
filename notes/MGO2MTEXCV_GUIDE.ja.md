# MGO2MT EXCV の使い方

`MGO2MTEXCV.exe` は、利用者の手元にある原データを MGO2MultiPlatform 向けに変換するソフトです。Python の別途インストールは不要です。ゲーム素材・原フォント・動画・通信鍵を同梱せず、ネットから取得しません。

## 変換

1. 「原データの場所」に、MGO2 の `stage` がある `o` フォルダーを指定します。
2. 別の空フォルダーを保存先に選びます。原データは上書きしません。
3. 暗号化原データ・音声・一部ステージ用に原ゲームの ELF を指定します。BGM 用の vgmstream は利用者が別途用意した実行ファイルを指定します。この配布には vgmstream 本体や DLL は含みません。
4. 原日本語フォントが別にある場合は、そのフォルダーを指定します。OS にはインストールしません。
5. 「変換を開始」を押します。原 MGS4 の `stage02.dat` がある場合は月光欄に指定します。未指定でも、成功した他の生成物は `runtime/data` にまとまります。ただし月光を含めた起動準備の完了とは表示しません。

対象は MGO2 の対戦用 21 ステージです。全ての変換が、原作の動的スクリプト・材質・DSP の完全再現を意味するものではありません。未対応・不足は `conversion-run.json` と各 `manifest.json` に記録します。

「再開」は原データと出力を照合します。原データや変換器のレシピが変わった場合は、別の保存先で変換してください。後から完了したモジュールを組み立てる場合、以前の `runtime` は `runtime.previous-*` に残ります。

## クライアント／HOSTへ適用

1. ソフトのみのクライアント／HOST ZIP を展開します。配布に含まれる `data` の設定は残してください。
2. EXCV の「MGO2MT.exe のフォルダー」「MGO2MTHOST.exe のフォルダー」に、展開したフォルダーを指定します。片方だけでも構いません。
3. 必要なら「利用者の既存data」に、自分で用意した `network.gnk`、任意の `movie_01.mp4`、任意の `character/hit_geometry.gwhit` があるフォルダーを指定します。通信情報を自動生成・配布・取得する機能はありません。鍵の中身は画面や記録へ出力しません。
4. ゲームを終了して「ゲームへ適用」を押します。原データや EXE は変更しません。`gameplay.json`、`mounted_weapons.json`、`weapon_effects.json` など、アプリに既にある設定は保持します。
5. 不足一覧を確認してください。適用完了と必要素材が全部揃うことは別です。月光の原データ、原 ELF、通信設定などが不足していれば、必要なファイル名を表示します。動画は任意です。

全コピーと検証を終えてから `data` を切り替えます。以前の `data` は同じアプリフォルダーの `data.before-excv-*` に保存します。失敗時に現行 `data` を中途半端な状態にしません。途中フォルダー `data.excv-stage-*` には失敗記録が残ります。`assets.sha256` は適用後の全ファイルで作り直します。

HOST の `resource-requirements.json` がある場合、そこに示された必要なファイルだけを適用します。適用結果は `data/excv-install.json`。これはローカルの資産準備結果で、接続やゲームの起動成功を試験した記録ではありません。

## コマンド操作

```text
MGO2MTEXCV.exe --source "原データのo" --output "変換先" --with-ui --with-runtime --elf "原ゲーム.ELF" --decoder "vgmstream-cli.exe" --mgs-source "stage02.dat" --report "変換結果.json"
MGO2MTEXCV.exe --verify "変換先\runtime" --report "照合結果.json"
MGO2MTEXCV.exe --install "変換先" --client "クライアントフォルダー" --host "HOSTフォルダー" --local-data "利用者の既存data" --report "適用結果.json"
```

`--mgs-source`、`--host`、`--local-data` は必要な場合だけ指定します。stage02.dat の元 DAT からの再抽出は現在の手元ファイルで未検証であり、以前抽出した原ファイルとの照合で検証しています。

水の足音・通知音など少数の音は、このプロジェクトの決定的な合成処理で生成します。原作音声の代替と明記したもので、原音声を EXE に埋め込んだものではありません。

判定形状 `character/hit_geometry.gwhit` は、利用者が保存したファイルを検証して取り込めます。原モーションからこの形式を新規生成する処理は未実装です。なくても、ゲームが用意する簡易的な独自形状で動作します。

任意の追加資産は `character/hit_geometry.gwhit`、`motion/evade_travel.gwet`、`weapon/connection_points.gwcp`、`special/gekko_jump.gwjc` の4つです。選択した既存 `data` からのみ読み、長さ・バージョン・件数・値の範囲を検証します。この EXCV では原モーション／CNP から4形式を新たに生成する処理は未実装です。欠損時は、ゲーム側が判定形状／移動の独自近似を使用し、原 CNP 接続・月光ジャンプ補正を省略します。不正ファイルのコピーは拒否します。

BGM 用デコーダーなど任意入力が不足していても、ファイル一覧と SHA-256 を照合できる生成済みの効果音などは `runtime` に組み立てます。不足名は残り、原作の資産が全部揃ったとは表示しません。
