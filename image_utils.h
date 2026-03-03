// 画像処理関係の関数を定義
#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H

#include <QImage>
#include <opencv2/core.hpp>
#include <vector>

namespace ImageUtils {
    // QImageをOpenCV形式へ変換する。
    cv::Mat qimage_to_mat_bgra(const QImage& img);

    // BGRAからAを取り出し、logical配列にする
    cv::Mat1b alphaMaskFromBGRA(const cv::Mat& bgra, double alphaThreshold = 0.5);

    // 最大矩形を探索する。
    void largestRectHistogram(
        const std::vector<int>& h,
        int& bestArea, int& bestL, int& bestR, int& bestH);

    // 入力: logical配列 mask（CV_8U, 値0/1推奨。非0をtrue扱いでもOK）
    // 出力: 最大面積矩形の (x,y,w,h)。無ければ (0,0,0,0)
    cv::Rect maxRectOnesFromLogical(const cv::Mat1b& mask);
}

#endif // IMAGE_UTILS_H
