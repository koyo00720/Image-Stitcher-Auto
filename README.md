## 概要 
複数の画像を結合する。顕微鏡画像などを想定。

## 仕様上の性能限界
上下または左右方向に画像が移動することを想定。
斜め方向に画像が移動する場合、適切に結合できない可能性が高い。

## ターミナル経由の使い方
- 自動化最小構成  
Image_Stitcher_Auto.exe --input "C:\〇〇" --calc 2 --make_image --output "C:\〇〇.png" --export --close

- 設定のみ  
Image_Stitcher_Auto.exe --manual --over_x 25 --over_y 25 --over_r 15 --array 8 --y_num 0 --zig 1 --part 1 --part_num 6 --part_pix 2 --part_itr 5000 --part_loop 4 --all 0 --all_pix 2 --all_itr 10000 --all_loop 10 --output "C:\〇〇.png" --close

- 全てのオプション  
Image_Stitcher_Auto.exe --input "C:\〇〇" --manual --over_x 25 --over_y 25 --over_r 15 --array 8 --y_num 0 --zig 1 --part 1 --part_num 6 --part_pix 2 --part_itr 5000 --part_loop 4 --all 1 --all_pix 2 --all_itr 10000 --all_loop 10 --calc 2 --make_image --output "C:\〇〇.png" --export --close

- ヘルプ  
Image_Stitcher_Auto.exe -h

## Build
- Qt 6.10.2 (MinGW 64-bit)
- OpenCV 4.12.0
- CMake + Ninja (optional)

## Third-party libraries
This project uses:
- Qt — LGPL v3
- OpenCV — Apache License 2.0  
Each library is distributed under its own license.

## Contributing
This repository is not accepting pull requests.
Please do not submit contributions.
