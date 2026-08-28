## 概要 
複数の画像を結合する。顕微鏡画像などを想定。

## 仕様上の性能限界
上下または左右方向に画像が移動することを想定。
斜め方向に画像が移動する場合、適切に結合できない可能性が高い。

## GUIのデフォルト設定

`Image_Stitcher_Auto.conf` に、GUIで変更できる設定項目の起動時デフォルトを
INI形式で定義します。CMake構成時にビルド先へコピーされ、インストール時は
実行ファイルと同じディレクトリへ配置されます。

対象は、テーマ、言語、Vulkan、入力ファイルの並べ替え、重なりと配列、キャンパス、
TRW-S-PAMI、最小二乗法、画像結合です。値の範囲と選択肢は設定ファイル内の
コメントを参照してください。不正な値はGUIの範囲内へ補正され、解釈できない値や
ファイルが存在しない場合は、プログラム内の従来値へフォールバックします。

設定の優先順位は次のとおりです。

1. 明示的なコマンドラインオプション
2. GUIから保存済みのユーザー設定（テーマ、言語、GPU計算、キャンパス背景色、
   重なり、レイアウト、各計算設定、メインウインドウとダイアログのサイズ、
   左側制御パネルの幅）
3. `Image_Stitcher_Auto.conf`
4. プログラム内のフォールバック値

GUIから変更した値は同ファイルの `state...` セクションにも保存されます。設定画面の
「リセット」タブでは、全項目またはカテゴリ単位で起動時デフォルトへ戻せます。
キャンパスへ画像ファイルまたはフォルダをドラッグ＆ドロップすると、ファイル入力
ウィザードと同様に再帰展開・ファイル名順の並べ替え・画像形式の検証を行って追加します。

## ターミナル経由の使い方
- 自動化最小構成  
Image_Stitcher_Auto.exe --input "C:\〇〇" --calc 2 --make_image --output "C:\〇〇.png" --export --close

- 設定のみ  
Image_Stitcher_Auto.exe --manual --over_x 25 --over_y 25 --over_r 15 --array 8 --y_num 0 --zig 1 --part 1 --part_num 6 --part_pix 2 --part_itr 5000 --part_loop 4 --all 0 --all_pix 2 --all_itr 10000 --all_loop 10 --output "C:\〇〇.png" --close

- 全てのオプション  
Image_Stitcher_Auto.exe --input "C:\〇〇" --manual --over_x 25 --over_y 25 --over_r 15 --array 8 --y_num 0 --zig 1 --part 1 --part_num 6 --part_pix 2 --part_itr 5000 --part_loop 4 --all 0 --all_pix 2 --all_itr 10000 --all_loop 10 --calc 2 --make_image --output "C:\〇〇.png" --export --close

- ヘルプ  
Image_Stitcher_Auto.exe -h

Linuxでは実行ファイル名を `Image_Stitcher_Auto`、パスをLinux形式に読み替えてください。

## Linux向け
- インストール (.deb)  
sudo apt install ./dist/image-stitcher-auto_*.deb
- インストール後に実行  
ターミナルにて Image_Stitcher_Auto または /usr/bin/Image_Stitcher_Auto
- アンインストール (.deb)  
sudo apt remove image-stitcher-auto
- インストールせずにそのまま実行 (.AppImage)  
chmod +x Image_Stitcher_Auto*.AppImage  
./Image_Stitcher_Auto*.AppImage

## ソース構成

```text
src/
├── app/                 # エントリーポイント、メインウィンドウ
├── core/                # Qtの画面構成に依存しない主要ロジック
├── dialogs/             # ダイアログと対応する.uiファイル
├── widgets/             # 再利用するカスタムウィジェット
└── platform/
    ├── platform_setup.h # 共通インターフェース
    ├── windows/         # Windows実装、アイコン、リソース
    ├── macos/           # macOS実装
    └── linux/           # Linux実装
resources/               # OS共通の画像とQtリソース
```

CMake構成時に対象OSを判定し、`src/platform/windows`、`src/platform/macos`、または
`src/platform/linux` の実装だけをビルドします。OS固有処理を追加する場合は、
共通インターフェースを `src/platform` に置き、各OSの同名実装へ分けてください。

## ビルド

必要なもの:

- C++17対応コンパイラ
- Qt 5またはQt 6（Widgets、Concurrent、LinguistTools）
- OpenCV（core、imgcodecs、imgproc、highgui）
- CMake 3.16以降
- Vulkan SDK、またはVulkanのヘッダーとローダー（任意。未検出時はCPU計算のみ）

QtとOpenCVを標準外の場所へインストールした場合は、各パッケージの場所を指定します。

```sh
cmake -S . -B build/release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt \
  -DOpenCV_DIR=/path/to/opencv/lib/cmake/opencv4
cmake --build build/release
```

英語翻訳は `translations/Image_Stitcher_Auto_en.ts` からビルド時に `.qm` へ変換され、
Qt 6では実行ファイルにも組み込まれます。実行ファイル横の `translations`
ディレクトリにも配置されるため、Qt 5ビルドでも利用できます。

Vulkan計算を明示的に無効化する場合は、CMake構成時に
`-DIMAGE_STITCHER_ENABLE_VULKAN=OFF` を追加してください。Vulkan対応ビルドでは、
設定画面から使用GPUを選択できます。既定では必要VRAMが選択GPUのVRAMの70%を
超える計算をCPUへフォールバックします。

macOSでは、Vulkan対応のSSIMバッチ計算に対してMetal計算経路もビルドされます。
Metalが利用可能な場合はMetalを優先し、必要メモリがGPUの推奨ワーキングセットの
70%を超える場合（設定で上限を無視していない場合）はCPUへフォールバックします。

Windowsの既存開発環境では、`C:/OpenCV/opencv_install_qtmingw/x64/mingw/lib`
が存在する場合に限り、OpenCVの検索候補として自動的に使用します。

## Third-party libraries
This project uses:
- Qt — LGPL v3
- OpenCV — Apache License 2.0  
Each library is distributed under its own license.

## Contributing
This repository is not accepting pull requests.
Please do not submit contributions.
