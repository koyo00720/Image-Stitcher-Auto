## 概要 
複数の画像を結合する。顕微鏡画像などを想定。

## 仕様上の性能限界
上下または左右方向に画像が移動することを想定。
斜め方向に画像が移動する場合、適切に結合できない可能性が高い。

## GUIのデフォルト設定

`Image_Stitcher_Auto.conf` に、GUIで変更できる設定項目の起動時デフォルトを
INI形式で定義します。CMake構成時にビルド先へコピーされ、インストール時は
実行ファイルと同じディレクトリへ配置されます。

対象は、テーマ、言語、Vulkan、プロジェクトファイル、Windows Explorer、入力
ファイルの並べ替え、重なりと配列、キャンパス、TRW-S-PAMI、最小二乗法、画像結合
です。値の範囲と選択肢は設定ファイル内のコメントを参照してください。不正な値は
GUIの範囲内へ補正され、解釈できない値やファイルが存在しない場合は、プログラム内の
従来値へフォールバックします。

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

## Linux向けパッケージ

以下はUbuntu 24.04系のx86_64環境でQt 6を使用する例です。`.deb`はビルド環境の
共有ライブラリを依存関係として記録し、AppImageはQtとOpenCVの実行時ライブラリを
同梱します。互換性を広げる場合は、対応対象の中で最も古いLinux環境上でビルドして
ください。

### ビルド環境

必要なパッケージをインストールします。

```sh
sudo apt update
sudo apt install \
  build-essential cmake ninja-build curl dpkg-dev \
  qt6-base-dev qt6-gtk-platformtheme qt6-tools-dev qt6-l10n-tools \
  libopencv-dev libvulkan-dev
```

`qt6-gtk-platformtheme`は、Linuxのファイル選択でデスクトップ環境のネイティブ
ダイアログを使用するために必要です。AppImageにはビルドスクリプトが
対応するQtプラットフォームテーマを同梱します。

Vulkan対応が不要な場合は`libvulkan-dev`を省略でき、後述のビルドコマンドへ
`-DIMAGE_STITCHER_ENABLE_VULKAN=OFF`を追加します。

AppImage作成用の`linuxdeploy`とQtプラグインを`tools`へ配置します。

```sh
mkdir -p tools
curl --fail --location \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
  --output tools/linuxdeploy-x86_64.AppImage
curl --fail --location \
  https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
  --output tools/linuxdeploy-plugin-qt-x86_64.AppImage
```

### `.deb`とAppImageの作成

リポジトリのルートで次を実行します。

```sh
bash packaging/linux/build-packages.sh
```

スクリプトはRelease構成をビルドし、Linuxネイティブな一時ディレクトリで`.deb`を
パッケージングした後、AppDirへQt/OpenCVを収集してAppImageを作成します。成果物は
次の場所へ出力されます。

```text
dist/image-stitcher-auto_<version>_amd64.deb
dist/Image_Stitcher_Auto-<version>-x86_64.AppImage
```

CMakeオプションはスクリプトの引数として渡せます。例えばVulkan対応を無効化する
場合は次のようにします。

```sh
bash packaging/linux/build-packages.sh \
  -DIMAGE_STITCHER_ENABLE_VULKAN=OFF
```

ビルド先・出力先・`linuxdeploy`の場所を変更する場合は、それぞれ`BUILD_DIR`、
`DIST_DIR`、`LINUXDEPLOY`、`LINUXDEPLOY_PLUGIN_QT`環境変数で指定できます。Qtを
標準外の場所へ入れている場合は、使用する`qmake`を`QMAKE`で指定できます。

### インストールと実行

`.deb`をインストールする場合:

```sh
sudo apt install ./dist/image-stitcher-auto_*.deb
Image_Stitcher_Auto
```

アンインストールする場合:

```sh
sudo apt remove image-stitcher-auto
```

AppImageをインストールせずに実行する場合:

```sh
chmod +x dist/Image_Stitcher_Auto-*.AppImage
./dist/Image_Stitcher_Auto-*.AppImage
```

FUSEを利用できないコンテナなどでは、展開実行モードを使用できます。

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./dist/Image_Stitcher_Auto-*.AppImage
```

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
- Windows SDK（Windows 11の新しいExplorerコンテキストメニューをビルドする場合）

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

Windows SDKの`makeappx.exe`が見つかる場合は、本体と同時に
`ImageStitcherExplorerCommand.dll`と`ImageStitcherAuto.ContextMenu.msix`を生成します。
設定画面でExplorer連携を初めて有効にすると、sparse MSIXを現在の実行フォルダへ関連
付けます。開発ビルドのMSIXはローカルテスト用の未署名形式です。配布時は
`IMAGE_STITCHER_CONTEXT_MENU_PUBLISHER`を署名証明書のSubjectへ変更し、生成された
MSIXへ信頼済み証明書で署名してください。

## Third-party libraries
This project uses:
- Qt — LGPL v3
- OpenCV — Apache License 2.0  
Each library is distributed under its own license.

## Contributing
This repository is not accepting pull requests.
Please do not submit contributions.
