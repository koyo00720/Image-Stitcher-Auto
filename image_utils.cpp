#include "image_utils.h"


namespace ImageUtils {

cv::Mat qimage_to_mat_bgra(const QImage& img)
{
    QImage converted = img.convertToFormat(QImage::Format_ARGB32); // 32-bit BGRA相当
    cv::Mat mat(converted.height(), converted.width(), CV_8UC4,
                (void*)converted.bits(), converted.bytesPerLine());
    return mat.clone(); // QImageの寿命から独立させる
};


cv::Mat1b alphaMaskFromBGRA(const cv::Mat& bgra, double alphaThreshold)
{
    CV_Assert(!bgra.empty());
    CV_Assert(bgra.type() == CV_8UC4); // BGRA 8bit

    // alpha >= 0.5 → alpha >= 128
    const int thr = (int)std::lround(alphaThreshold * 255.0);

    std::vector<cv::Mat> ch;
    cv::split(bgra, ch);         // ch[3] が alpha
    cv::Mat1b mask;
    cv::compare(ch[3], thr, mask, cv::CMP_GE); // 0 or 255 のマスク

    // 「logical配列」(0/1)にしたいなら 0/255 を 0/1 に落とす
    mask /= 255;

    return mask; // CV_8U, 値は 0 or 1
}

void largestRectHistogram(
    const std::vector<int>& h,
    int& bestArea, int& bestL, int& bestR, int& bestH)
{
    const int W = (int)h.size();
    bestArea = 0; bestL = 0; bestR = -1; bestH = 0;

    std::vector<int> st;
    st.reserve(W + 1);

    auto hh = [&](int i)->int { return (i == W) ? 0 : h[i]; };

    for (int i = 0; i <= W; ++i) {
        int cur = hh(i);
        while (!st.empty() && hh(st.back()) > cur) {
            int height = hh(st.back());
            st.pop_back();

            int left = st.empty() ? 0 : st.back() + 1;
            int right = i - 1;
            int area = height * (right - left + 1);

            if (area > bestArea) {
                bestArea = area;
                bestL = left;
                bestR = right;
                bestH = height;
            }
        }
        st.push_back(i);
    }
}


cv::Rect maxRectOnesFromLogical(const cv::Mat1b& mask)
{
    CV_Assert(!mask.empty());
    CV_Assert(mask.channels() == 1);
    CV_Assert(mask.depth() == CV_8U);

    const int H = mask.rows;
    const int W = mask.cols;

    std::vector<int> heights(W, 0);

    int bestAreaAll = 0;
    int bestTop = 0, bestLeft = 0, bestBottom = -1, bestRight = -1;

    for (int r = 0; r < H; ++r) {
        const uchar* row = mask.ptr<uchar>(r);

        for (int c = 0; c < W; ++c) {
            if (row[c]) heights[c] += 1;
            else heights[c] = 0;
        }

        int area, l, rr, h;
        largestRectHistogram(heights, area, l, rr, h);

        if (area > bestAreaAll) {
            bestAreaAll = area;
            bestBottom = r;
            bestLeft = l;
            bestRight = rr;
            bestTop = r - h + 1;
        }
    }

    if (bestAreaAll <= 0) return cv::Rect(0, 0, 0, 0);

    return cv::Rect(
        bestLeft,
        bestTop,
        bestRight - bestLeft + 1,
        bestBottom - bestTop + 1
        );
}

}
