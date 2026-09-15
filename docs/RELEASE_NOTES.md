# MGO2WIN + HOST v0.01-20260915142232

3D描画の左右反転を修正しました。壁の文字と武器の手持ち姿勢が正しい向きになり、左右操作・照準・敵名表示・リーンも同じ座標系に揃いました。

床が黒くなる原因だった原法線の読み違いを修正。5ステージ、1,219,616頂点の原RSX CMP法線を補助リソースから読み込み、LT3照明へ渡します。既存のモデル位置・ボーン・UV・テクスチャは変更していません。

前回公開版以降の原UI/日本語フォント/装備アイコン、START動画、メニュー入力ウェイト、AK手接続・リロード・発射音/効果、死亡・再出撃、敵LV/HP表示、半球ライトと方向光の影設定も含む完全版です。

クライアントとHOSTは今回の同じ版を使用してください。両ZIPは前の版なしで導入できます。通信形式はGWCB19/GWAV2です。

Windows Releaseの全284テスト、両EXEのバージョン/資産検査、全配布ファイルのSHA-256を検証。動作画像は最終EXEのオフラインD3D11 WARP描画によるものです。

原作の材質・照明の完全一致は未完で、床の一部に材質境界が残っています。実LAN、物理ゲームパッド、実GPU性能は今回未検証です。原本のMDN/GEOM/IDAデータや個人の認証情報は含みません。

Source and matching full program packages are published together. Use the `win64-full.zip` downloads to run the program; the source archive does not include runtime game resources.
