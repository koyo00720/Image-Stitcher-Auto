#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "application_settings_dialog.h"
#include "file_input_wiz.h"
#include "image_utils.h"
#include "cornerdirectionselector.h"
#include "detail_dialog.h"
#include "trws.h"
#include "opti_settings.h"
#include "least_squares_settings.h"
#include "merge_settings.h"
#include "canvas_history_graph_widget.h"
#include "metal_ssim.h"
#include "unsupported_files_dialog.h"
#include "explorer_context_menu.h"
#include "platform_setup.h"

#include <QBuffer>
#include <QCloseEvent>
#include <QDataStream>
#include <QFile>
#include <QFileDialog>
#include <QSaveFile>
#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QAction>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEvent>
#include <QEventLoop>
#include <QFileInfo>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QMenu>
#include <QPen>
#include <QSignalBlocker>
#include <QSet>
#include <QSplitter>
#include <QtConcurrent/QtConcurrent>
#include <QIntValidator>
#include <QImageReader>
#include <QTimer>
#include <QFuture>
#include <QDebug>
#include <QStandardItemModel>
#include <QGraphicsSceneMouseEvent>
#include <QMetaObject>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QProgressBar>
#include <QVariantList>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <array>
#include <cstdint>
#include <exception>
#include <limits>
#include <mutex>
#include <utility>
//#include <numeric>

namespace
{
constexpr quint32 kProjectMagic = 0x49534155U; // "ISAU"
constexpr quint16 kProjectSchemaVersion = 1;

QVariantList pointsToVariant(const QVector<QPoint>& points)
{
    QVariantList result;
    result.reserve(points.size());
    for (const QPoint& point : points) {
        QVariantMap entry;
        entry.insert(QStringLiteral("x"), point.x());
        entry.insert(QStringLiteral("y"), point.y());
        result.push_back(entry);
    }
    return result;
}

QVector<QPoint> pointsFromVariant(const QVariant& value)
{
    const QVariantList list = value.toList();
    QVector<QPoint> result;
    result.reserve(list.size());
    for (const QVariant& item : list) {
        const QVariantMap entry = item.toMap();
        result.push_back(QPoint(entry.value(QStringLiteral("x")).toInt(),
                                entry.value(QStringLiteral("y")).toInt()));
    }
    return result;
}

QVariantList intsToVariant(const QVector<int>& values)
{
    QVariantList result;
    result.reserve(values.size());
    for (int value : values) result.push_back(value);
    return result;
}

QVector<int> intsFromVariant(const QVariant& value)
{
    const QVariantList list = value.toList();
    QVector<int> result;
    result.reserve(list.size());
    for (const QVariant& item : list) result.push_back(item.toInt());
    return result;
}

QVariantList boolsToVariant(const QVector<bool>& values)
{
    QVariantList result;
    result.reserve(values.size());
    for (bool value : values) result.push_back(value);
    return result;
}

QVector<bool> boolsFromVariant(const QVariant& value)
{
    const QVariantList list = value.toList();
    QVector<bool> result;
    result.reserve(list.size());
    for (const QVariant& item : list) result.push_back(item.toBool());
    return result;
}

QVariantList doublesToVariant(const std::vector<double>& values)
{
    QVariantList result;
    result.reserve(static_cast<int>(values.size()));
    for (double value : values) result.push_back(value);
    return result;
}

std::vector<double> doublesFromVariant(const QVariant& value)
{
    const QVariantList list = value.toList();
    std::vector<double> result;
    result.reserve(static_cast<size_t>(list.size()));
    for (const QVariant& item : list) result.push_back(item.toDouble());
    return result;
}

QVariantMap ifftResultToVariant(const ifft_thread_output& result)
{
    QVariantMap value;
    value.insert(QStringLiteral("img1"), result.img1_id);
    value.insert(QStringLiteral("img2"), result.img2_id);
    value.insert(QStringLiteral("x"), result.vecX);
    value.insert(QStringLiteral("y"), result.vecY);
    value.insert(QStringLiteral("score"), result.score);
    value.insert(QStringLiteral("stability"), result.stability);
    value.insert(QStringLiteral("loop"), result.loop_num);
    value.insert(QStringLiteral("ssim"), result.ssim);
    value.insert(QStringLiteral("error"), result.calc_error);
    value.insert(QStringLiteral("cancelled"), result.cancelled);
    return value;
}

ifft_thread_output ifftResultFromVariant(const QVariant& value)
{
    const QVariantMap map = value.toMap();
    ifft_thread_output result{};
    result.img1_id = map.value(QStringLiteral("img1")).toInt();
    result.img2_id = map.value(QStringLiteral("img2")).toInt();
    result.vecX = map.value(QStringLiteral("x")).toDouble();
    result.vecY = map.value(QStringLiteral("y")).toDouble();
    result.score = map.value(QStringLiteral("score")).toDouble();
    result.stability = map.value(QStringLiteral("stability")).toBool();
    result.loop_num = map.value(QStringLiteral("loop")).toInt();
    result.ssim = map.value(QStringLiteral("ssim")).toDouble();
    result.calc_error = map.value(QStringLiteral("error")).toBool();
    result.cancelled = map.value(QStringLiteral("cancelled")).toBool();
    return result;
}

QVariantList ifftResultsToVariant(const QList<ifft_thread_output>& results)
{
    QVariantList list;
    list.reserve(results.size());
    for (const ifft_thread_output& result : results) {
        list.push_back(ifftResultToVariant(result));
    }
    return list;
}

QList<ifft_thread_output> ifftResultsFromVariant(const QVariant& value)
{
    QList<ifft_thread_output> results;
    const QVariantList list = value.toList();
    results.reserve(list.size());
    for (const QVariant& item : list) {
        results.push_back(ifftResultFromVariant(item));
    }
    return results;
}

QVariantMap leastDetailToVariant(const LeastSquaresPairDetail& detail)
{
    QVariantMap value;
    value.insert(QStringLiteral("img1"), detail.img1);
    value.insert(QStringLiteral("img2"), detail.img2);
    value.insert(QStringLiteral("status"), detail.status);
    value.insert(QStringLiteral("loop"), detail.loop_num);
    value.insert(QStringLiteral("stability"), detail.stability);
    value.insert(QStringLiteral("score"), detail.score);
    value.insert(QStringLiteral("measuredSsim"), detail.measuredSsim);
    value.insert(QStringLiteral("optimizedSsim"), detail.optimizedSsim);
    value.insert(QStringLiteral("measuredDx"), detail.measuredDx);
    value.insert(QStringLiteral("measuredDy"), detail.measuredDy);
    value.insert(QStringLiteral("optimizedDx"), detail.optimizedDx);
    value.insert(QStringLiteral("optimizedDy"), detail.optimizedDy);
    value.insert(QStringLiteral("residual"), detail.residual);
    return value;
}

LeastSquaresPairDetail leastDetailFromVariant(const QVariant& value)
{
    const QVariantMap map = value.toMap();
    LeastSquaresPairDetail detail;
    detail.img1 = map.value(QStringLiteral("img1"), -1).toInt();
    detail.img2 = map.value(QStringLiteral("img2"), -1).toInt();
    detail.status = map.value(QStringLiteral("status")).toString();
    detail.loop_num = map.value(QStringLiteral("loop")).toInt();
    detail.stability = map.value(QStringLiteral("stability")).toBool();
    detail.score = map.value(QStringLiteral("score")).toDouble();
    detail.measuredSsim = map.value(QStringLiteral("measuredSsim")).toDouble();
    detail.optimizedSsim = map.value(QStringLiteral("optimizedSsim")).toDouble();
    detail.measuredDx = map.value(QStringLiteral("measuredDx")).toDouble();
    detail.measuredDy = map.value(QStringLiteral("measuredDy")).toDouble();
    detail.optimizedDx = map.value(QStringLiteral("optimizedDx")).toDouble();
    detail.optimizedDy = map.value(QStringLiteral("optimizedDy")).toDouble();
    detail.residual = map.value(QStringLiteral("residual")).toDouble();
    return detail;
}

QByteArray encodeMatAsPng(const cv::Mat& image)
{
    if (image.empty()) return {};
    std::vector<uchar> encoded;
    try {
        if (!cv::imencode(".png", image, encoded)) return {};
    } catch (const cv::Exception&) {
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(encoded.data()),
                      static_cast<int>(encoded.size()));
}

cv::Mat decodeEmbeddedImage(const QByteArray& data)
{
    if (data.isEmpty()) return {};
    const cv::Mat bytes(1, data.size(), CV_8UC1,
                        const_cast<char*>(data.constData()));
    try {
        return cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    } catch (const cv::Exception&) {
        return {};
    }
}

QByteArray pixmapAsPng(const QPixmap& pixmap)
{
    QByteArray data;
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::WriteOnly) || !pixmap.save(&buffer, "PNG")) {
        return {};
    }
    return data;
}
}

static bool workerCancellationRequested(
    const std::shared_ptr<std::atomic_bool>& cancellation)
{
    return cancellation && cancellation->load(std::memory_order_relaxed);
}

// iFFT用関数
static cv::Mat1f clahe_then_grad(const cv::Mat& im_bgr)
{
    CV_Assert(!im_bgr.empty());
    CV_Assert(im_bgr.channels() == 3);

    cv::Mat g8;
    cv::cvtColor(im_bgr, g8, cv::COLOR_BGR2GRAY);

    // CLAHE
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    clahe->apply(g8, g8);

    cv::Mat1f g;
    g8.convertTo(g, CV_32F);

    // Gaussian blur (sigma=1.0, ksize=(0,0) means auto)
    cv::GaussianBlur(g, g, cv::Size(0, 0), 1.0);

    // Sobel gradients
    cv::Mat1f gx, gy;
    cv::Sobel(g, gx, CV_32F, 1, 0, 3);
    cv::Sobel(g, gy, CV_32F, 0, 1, 3);

    // magnitude
    cv::Mat1f mag;
    cv::magnitude(gx, gy, mag);

    // mag -= mean; mag /= std
    cv::Scalar mean, stddev;
    cv::meanStdDev(mag, mean, stddev);
    mag -= (float)mean[0];
    float s = (float)stddev[0];
    if (s > 1e-6f) mag /= s;

    // Hanning window (reduce edge/DC effects)
    cv::Mat1f win;
    cv::createHanningWindow(win, mag.size(), CV_32F);
    mag = mag.mul(win);

    return mag;
}

static void paste_over(cv::Mat& dst, const cv::Mat& src, int x, int y)
{
    CV_Assert(dst.channels() == src.channels());
    CV_Assert(dst.depth() == src.depth());
    CV_Assert(x >= 0 && y >= 0);
    CV_Assert(x + src.cols <= dst.cols);
    CV_Assert(y + src.rows <= dst.rows);

    src.copyTo(dst(cv::Rect(x, y, src.cols, src.rows)));
}


// BGRA画像２枚を合成（距離変換フェザー）
// cam1, cam2: CV_8UC4 (BGRA)
// shift: phaseCorrelate(a,b) の戻り値を想定（あなたの符号規約に合わせて x2=-shift.x）
// featherRadius: フェザー幅（ピクセル）。0以下なら無制限（画像内側ほど重くなる）
cv::Mat make_canvas_bgra_feather_dt(
    const cv::Mat& cam1,
    const cv::Mat& cam2,
    const cv::Point2d& shift_from_phaseCorrelate,
    float featherRadius = 80.0f,
    const std::shared_ptr<std::atomic_bool>& cancellation = {})
{
    CV_Assert(!cam1.empty() && !cam2.empty());
    CV_Assert(cam1.type() == CV_8UC4 && cam2.type() == CV_8UC4);

    // 貼り付けオフセット（あなたのコードと同じ）
    const int x1 = 0, y1 = 0;
    const int x2 = (int)std::lround(-shift_from_phaseCorrelate.x);
    const int y2 = (int)std::lround(-shift_from_phaseCorrelate.y);

    const int h1 = cam1.rows, w1 = cam1.cols;
    const int h2 = cam2.rows, w2 = cam2.cols;

    // キャンバスサイズ
    const int min_x = std::min(x1, x2);
    const int min_y = std::min(y1, y2);
    const int max_x = std::max(x1 + w1, x2 + w2);
    const int max_y = std::max(y1 + h1, y2 + h2);

    const int out_w = max_x - min_x;
    const int out_h = max_y - min_y;

    const int sx = -min_x;
    const int sy = -min_y;

    // 各画像をキャンバス座標に配置（未合成で保持）
    cv::Mat img1(out_h, out_w, CV_8UC4, cv::Scalar(0,0,0,0));
    cv::Mat img2(out_h, out_w, CV_8UC4, cv::Scalar(0,0,0,0));

    {
        cv::Rect roi1(x1 + sx, y1 + sy, w1, h1);
        CV_Assert(0 <= roi1.x && 0 <= roi1.y && roi1.x + roi1.width <= out_w && roi1.y + roi1.height <= out_h);
        cam1.copyTo(img1(roi1));
    }
    {
        cv::Rect roi2(x2 + sx, y2 + sy, w2, h2);
        CV_Assert(0 <= roi2.x && 0 <= roi2.y && roi2.x + roi2.width <= out_w && roi2.y + roi2.height <= out_h);
        cam2.copyTo(img2(roi2));
    }

    // 有効領域マスク（alpha > 0）
    cv::Mat1b m1(out_h, out_w, uchar(0));
    cv::Mat1b m2(out_h, out_w, uchar(0));

    for (int r = 0; r < out_h; ++r) {
        if (workerCancellationRequested(cancellation)) return {};
        const cv::Vec4b* p1 = img1.ptr<cv::Vec4b>(r);
        const cv::Vec4b* p2 = img2.ptr<cv::Vec4b>(r);
        uchar* q1 = m1.ptr<uchar>(r);
        uchar* q2 = m2.ptr<uchar>(r);
        for (int c = 0; c < out_w; ++c) {
            q1[c] = (p1[c][3] > 0) ? 255 : 0;
            q2[c] = (p2[c][3] > 0) ? 255 : 0;
        }
    }

    // 距離変換（非ゼロ画素について、最も近いゼロ画素までの距離）
    // → 有効領域内部ほど距離が大きく、境界で0に近い
    cv::Mat1f d1, d2;
    cv::distanceTransform(m1, d1, cv::DIST_L2, 3);
    cv::distanceTransform(m2, d2, cv::DIST_L2, 3);

    // フェザー幅制御（任意）
    if (featherRadius > 0.0f) {
        cv::min(d1, featherRadius, d1);
        cv::min(d2, featherRadius, d2);
    }

    // 合成（フェザー）
    cv::Mat canvas(out_h, out_w, CV_8UC4, cv::Scalar(0,0,0,0));
    constexpr float eps = 1e-6f;

    for (int r = 0; r < out_h; ++r) {
        if (workerCancellationRequested(cancellation)) return {};
        const cv::Vec4b* p1 = img1.ptr<cv::Vec4b>(r);
        const cv::Vec4b* p2 = img2.ptr<cv::Vec4b>(r);
        const float* dd1 = d1.ptr<float>(r);
        const float* dd2 = d2.ptr<float>(r);
        cv::Vec4b* out = canvas.ptr<cv::Vec4b>(r);

        for (int c = 0; c < out_w; ++c) {
            const cv::Vec4b a = p1[c];
            const cv::Vec4b b = p2[c];

            const float a1 = a[3] / 255.0f;
            const float a2 = b[3] / 255.0f;

            const bool v1 = a1 > 0.0f;
            const bool v2 = a2 > 0.0f;

            if (!v1 && !v2) {
                out[c] = cv::Vec4b(0,0,0,0);
                continue;
            }
            if (v1 && !v2) {
                out[c] = a;
                continue;
            }
            if (!v1 && v2) {
                out[c] = b;
                continue;
            }

            // 両方有効：距離から重み
            float ww1 = dd1[c];
            float ww2 = dd2[c];
            float wws = ww1 + ww2;

            // あり得る：境界ピッタリで両方ほぼ0 → その場合は等分
            if (wws < eps) { ww1 = 0.5f; ww2 = 0.5f;}
            else { ww1 /= wws; ww2 /= wws; }

            // αも含めて「事前乗算」で混ぜる（境界が破綻しにくい）
            // premult = rgb * alpha
            const float p1b = (a[0]/255.0f) * a1;
            const float p1g = (a[1]/255.0f) * a1;
            const float p1r = (a[2]/255.0f) * a1;

            const float p2b = (b[0]/255.0f) * a2;
            const float p2g = (b[1]/255.0f) * a2;
            const float p2r = (b[2]/255.0f) * a2;

            // フェザー重みで混合
            const float ao = std::clamp(a1*ww1 + a2*ww2, 0.0f, 1.0f); // 出力alpha
            float ob = 0.0f, og = 0.0f, or_ = 0.0f;

            if (ao > eps) {
                ob = (p1b*ww1 + p2b*ww2) / ao;
                og = (p1g*ww1 + p2g*ww2) / ao;
                or_ = (p1r*ww1 + p2r*ww2) / ao;
            }

            out[c][0] = (uchar)std::lround(std::clamp(ob, 0.0f, 1.0f) * 255.0f);
            out[c][1] = (uchar)std::lround(std::clamp(og, 0.0f, 1.0f) * 255.0f);
            out[c][2] = (uchar)std::lround(std::clamp(or_, 0.0f, 1.0f) * 255.0f);
            out[c][3] = (uchar)std::lround(ao * 255.0f);
        }
    }

    return canvas;
}

namespace
{
struct MergePlacedImage
{
    cv::Mat bgra;
    cv::Mat1b mask;
    cv::Mat1f focus;
    QPoint topLeft;
};

cv::Mat mergeEnsureBgra(const cv::Mat& input)
{
    if (input.empty()) {
        return {};
    }
    if (input.type() == CV_8UC4) {
        return input.clone();
    }

    cv::Mat source8;
    if (input.depth() == CV_8U) {
        source8 = input;
    } else {
        input.convertTo(source8, CV_8U);
    }

    cv::Mat bgra;
    if (source8.channels() == 4) {
        bgra = source8.clone();
    } else if (source8.channels() == 3) {
        cv::cvtColor(source8, bgra, cv::COLOR_BGR2BGRA);
    } else if (source8.channels() == 1) {
        cv::cvtColor(source8, bgra, cv::COLOR_GRAY2BGRA);
    }
    return bgra;
}

cv::Mat1b mergeAlphaMask255FromBgra(const cv::Mat& bgra)
{
    std::vector<cv::Mat> channels;
    cv::split(bgra, channels);

    cv::Mat1b mask;
    cv::compare(channels[3], 0, mask, cv::CMP_GT);
    return mask;
}

cv::Mat1f mergeTenengradFocusMap(const cv::Mat& bgra)
{
    cv::Mat gray8;
    cv::cvtColor(bgra, gray8, cv::COLOR_BGRA2GRAY);

    cv::Mat1f gray;
    gray8.convertTo(gray, CV_32F);

    cv::Mat1f gx;
    cv::Mat1f gy;
    cv::Sobel(gray, gx, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gy, CV_32F, 0, 1, 3);

    cv::Mat1f focus;
    cv::add(gx.mul(gx), gy.mul(gy), focus);
    return focus;
}

bool mergeCalculateBounds(const std::vector<cv::Mat>& imgs,
                          const QVector<QPoint>& poss,
                          int& minX,
                          int& minY,
                          int& maxX,
                          int& maxY)
{
    if (imgs.empty() || poss.size() != static_cast<int>(imgs.size())) {
        return false;
    }

    bool initialized = false;
    for (int i = 0; i < static_cast<int>(imgs.size()); ++i) {
        if (imgs[i].empty()) {
            continue;
        }

        const int x0 = poss[i].x();
        const int y0 = poss[i].y();
        const int x1 = x0 + imgs[i].cols;
        const int y1 = y0 + imgs[i].rows;

        if (!initialized) {
            minX = x0;
            minY = y0;
            maxX = x1;
            maxY = y1;
            initialized = true;
        } else {
            minX = std::min(minX, x0);
            minY = std::min(minY, y0);
            maxX = std::max(maxX, x1);
            maxY = std::max(maxY, y1);
        }
    }

    return initialized && maxX > minX && maxY > minY;
}

std::vector<MergePlacedImage> mergePreparePlacedImages(const std::vector<cv::Mat>& imgs,
                                                       const QVector<QPoint>& poss,
                                                       int minX,
                                                       int minY,
                                                       const std::shared_ptr<std::atomic_bool>& cancellation)
{
    std::vector<MergePlacedImage> placed;
    placed.reserve(imgs.size());

    for (int i = 0; i < static_cast<int>(imgs.size()); ++i) {
        if (workerCancellationRequested(cancellation)) return {};
        cv::Mat bgra = mergeEnsureBgra(imgs[i]);
        if (bgra.empty()) {
            continue;
        }

        MergePlacedImage item;
        item.bgra = std::move(bgra);
        item.mask = mergeAlphaMask255FromBgra(item.bgra);
        item.focus = mergeTenengradFocusMap(item.bgra);
        item.topLeft = QPoint(poss[i].x() - minX, poss[i].y() - minY);
        placed.push_back(std::move(item));
    }

    return placed;
}

cv::Mat mergeByTenengradPixel(const std::vector<MergePlacedImage>& placed,
                              const cv::Size& canvasSize,
                              const std::shared_ptr<std::atomic_bool>& cancellation)
{
    cv::Mat canvas(canvasSize.height, canvasSize.width, CV_8UC4,
                   cv::Scalar(0, 0, 0, 0));
    cv::Mat1f bestFocus(canvasSize.height, canvasSize.width,
                        -std::numeric_limits<float>::max());

    for (const MergePlacedImage& item : placed) {
        for (int y = 0; y < item.bgra.rows; ++y) {
            if (workerCancellationRequested(cancellation)) return {};
            const int cy = item.topLeft.y() + y;
            if (cy < 0 || cy >= canvasSize.height) {
                continue;
            }

            const uchar* maskRow = item.mask.ptr<uchar>(y);
            const float* focusRow = item.focus.ptr<float>(y);
            const cv::Vec4b* srcRow = item.bgra.ptr<cv::Vec4b>(y);
            float* bestRow = bestFocus.ptr<float>(cy);
            cv::Vec4b* outRow = canvas.ptr<cv::Vec4b>(cy);

            for (int x = 0; x < item.bgra.cols; ++x) {
                if (!maskRow[x]) {
                    continue;
                }

                const int cx = item.topLeft.x() + x;
                if (cx < 0 || cx >= canvasSize.width) {
                    continue;
                }

                if (focusRow[x] > bestRow[cx]) {
                    bestRow[cx] = focusRow[x];
                    outRow[cx] = srcRow[x];
                }
            }
        }
    }

    return canvas;
}

std::uint64_t mergeSourceKeyForImage(int image)
{
    std::uint64_t z = static_cast<std::uint64_t>(image) + 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

cv::Mat mergeBySimpleOverlayAndBuildSourceMap(const std::vector<MergePlacedImage>& placed,
                                               const cv::Size& canvasSize,
                                               std::vector<std::uint64_t>& sourceKeys,
                                               std::vector<unsigned short>& sourceCounts,
                                               const std::shared_ptr<std::atomic_bool>& cancellation)
{
    cv::Mat canvas(canvasSize.height, canvasSize.width, CV_8UC4,
                   cv::Scalar(0, 0, 0, 0));
    const size_t pixelCount = static_cast<size_t>(canvasSize.width) * canvasSize.height;
    sourceKeys.assign(pixelCount, 0ULL);
    sourceCounts.assign(pixelCount, 0);

    for (int image = 0; image < static_cast<int>(placed.size()); ++image) {
        const MergePlacedImage& item = placed[image];
        const std::uint64_t imageKey = mergeSourceKeyForImage(image);
        for (int y = 0; y < item.bgra.rows; ++y) {
            if (workerCancellationRequested(cancellation)) return {};
            const int cy = item.topLeft.y() + y;
            if (cy < 0 || cy >= canvasSize.height) {
                continue;
            }

            const uchar* maskRow = item.mask.ptr<uchar>(y);
            const cv::Vec4b* srcRow = item.bgra.ptr<cv::Vec4b>(y);
            cv::Vec4b* outRow = canvas.ptr<cv::Vec4b>(cy);
            const size_t rowOffset = static_cast<size_t>(cy) * canvasSize.width;

            for (int x = 0; x < item.bgra.cols; ++x) {
                if (!maskRow[x]) {
                    continue;
                }

                const int cx = item.topLeft.x() + x;
                if (cx < 0 || cx >= canvasSize.width) {
                    continue;
                }

                const size_t pixelIndex = rowOffset + static_cast<size_t>(cx);
                sourceKeys[pixelIndex] ^= imageKey;
                if (sourceCounts[pixelIndex] < std::numeric_limits<unsigned short>::max()) {
                    ++sourceCounts[pixelIndex];
                }
                outRow[cx] = srcRow[x];
            }
        }
    }

    return canvas;
}

cv::Mat mergeByImageNumberOverwrite(
    const std::vector<MergePlacedImage>& placed,
    const cv::Size& canvasSize,
    const std::shared_ptr<std::atomic_bool>& cancellation)
{
    cv::Mat canvas(canvasSize.height, canvasSize.width, CV_8UC4,
                   cv::Scalar(0, 0, 0, 0));
    // placed is in image-number order. Each later opaque pixel replaces the
    // previous one, so the largest image number wins in every overlap.
    for (const MergePlacedImage& item : placed) {
        for (int y = 0; y < item.bgra.rows; ++y) {
            if (workerCancellationRequested(cancellation)) return {};
            const int canvasY = item.topLeft.y() + y;
            if (canvasY < 0 || canvasY >= canvasSize.height) continue;
            const uchar* maskRow = item.mask.ptr<uchar>(y);
            const cv::Vec4b* sourceRow = item.bgra.ptr<cv::Vec4b>(y);
            cv::Vec4b* canvasRow = canvas.ptr<cv::Vec4b>(canvasY);
            for (int x = 0; x < item.bgra.cols; ++x) {
                if (!maskRow[x]) continue;
                const int canvasX = item.topLeft.x() + x;
                if (canvasX >= 0 && canvasX < canvasSize.width) {
                    canvasRow[canvasX] = sourceRow[x];
                }
            }
        }
    }
    return canvas;
}

std::vector<int> mergeImagesCoveringCanvasPixel(const std::vector<MergePlacedImage>& placed,
                                                int cx,
                                                int cy)
{
    std::vector<int> images;
    images.reserve(4);
    for (int image = 0; image < static_cast<int>(placed.size()); ++image) {
        const MergePlacedImage& item = placed[image];
        const int lx = cx - item.topLeft.x();
        const int ly = cy - item.topLeft.y();
        if (lx < 0 || ly < 0 || lx >= item.bgra.cols || ly >= item.bgra.rows) {
            continue;
        }
        if (!item.mask.at<uchar>(ly, lx)) {
            continue;
        }
        images.push_back(image);
    }
    return images;
}

cv::Mat mergeByFocusRegion(const std::vector<MergePlacedImage>& placed,
                           const cv::Size& canvasSize,
                           const std::shared_ptr<std::atomic_bool>& cancellation)
{
    std::vector<std::uint64_t> sourceKeys;
    std::vector<unsigned short> sourceCounts;
    cv::Mat canvas = mergeBySimpleOverlayAndBuildSourceMap(
        placed, canvasSize, sourceKeys, sourceCounts, cancellation);
    if (sourceKeys.empty()) {
        return canvas;
    }

    std::vector<uchar> visited(sourceKeys.size(), 0);
    std::vector<int> stack;
    std::vector<int> regionPixels;
    stack.reserve(4096);
    regionPixels.reserve(4096);

    const int width = canvasSize.width;
    const int height = canvasSize.height;
    const int neighborDx[8] = {-1, 0, 1, -1, 1, -1, 0, 1};
    const int neighborDy[8] = {-1, -1, -1, 0, 0, 1, 1, 1};

    for (int startIndex = 0; startIndex < static_cast<int>(sourceKeys.size()); ++startIndex) {
        if (workerCancellationRequested(cancellation)) return {};
        if (visited[startIndex] || sourceCounts[static_cast<size_t>(startIndex)] < 2) {
            continue;
        }

        const std::uint64_t regionSourceKey = sourceKeys[static_cast<size_t>(startIndex)];
        const unsigned short regionSourceCount = sourceCounts[static_cast<size_t>(startIndex)];
        stack.clear();
        regionPixels.clear();
        stack.push_back(startIndex);
        visited[startIndex] = 1;

        while (!stack.empty()) {
            if (workerCancellationRequested(cancellation)) return {};
            const int index = stack.back();
            stack.pop_back();
            regionPixels.push_back(index);

            const int x = index % width;
            const int y = index / width;
            for (int k = 0; k < 8; ++k) {
                const int nx = x + neighborDx[k];
                const int ny = y + neighborDy[k];
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    continue;
                }

                const int neighborIndex = ny * width + nx;
                if (visited[neighborIndex] ||
                    sourceCounts[static_cast<size_t>(neighborIndex)] != regionSourceCount ||
                    sourceKeys[static_cast<size_t>(neighborIndex)] != regionSourceKey) {
                    continue;
                }

                visited[neighborIndex] = 1;
                stack.push_back(neighborIndex);
            }
        }

        int winner = -1;
        double bestMeanFocus = -1.0;
        const int sampleX = startIndex % width;
        const int sampleY = startIndex / width;
        const std::vector<int> candidateImages =
            mergeImagesCoveringCanvasPixel(placed, sampleX, sampleY);
        for (int image : candidateImages) {
            const MergePlacedImage& item = placed[image];
            double focusSum = 0.0;
            int focusCount = 0;
            for (int pixelIndex : regionPixels) {
                const int cx = pixelIndex % width;
                const int cy = pixelIndex / width;
                const int lx = cx - item.topLeft.x();
                const int ly = cy - item.topLeft.y();
                if (lx < 0 || ly < 0 || lx >= item.bgra.cols || ly >= item.bgra.rows) {
                    continue;
                }
                if (!item.mask.at<uchar>(ly, lx)) {
                    continue;
                }

                focusSum += item.focus.at<float>(ly, lx);
                ++focusCount;
            }

            if (focusCount <= 0) {
                continue;
            }

            const double meanFocus = focusSum / focusCount;
            if (meanFocus > bestMeanFocus) {
                bestMeanFocus = meanFocus;
                winner = image;
            }
        }

        if (winner >= 0) {
            const MergePlacedImage& item = placed[winner];
            for (int pixelIndex : regionPixels) {
                const int cx = pixelIndex % width;
                const int cy = pixelIndex / width;
                const int lx = cx - item.topLeft.x();
                const int ly = cy - item.topLeft.y();
                if (lx < 0 || ly < 0 || lx >= item.bgra.cols || ly >= item.bgra.rows) {
                    continue;
                }
                if (!item.mask.at<uchar>(ly, lx)) {
                    continue;
                }

                canvas.at<cv::Vec4b>(cy, cx) = item.bgra.at<cv::Vec4b>(ly, lx);
            }
        }
    }

    return canvas;
}

cv::Mat mergeByLegacyDistanceL2(const std::vector<cv::Mat>& imgs,
                                const QVector<QPoint>& poss,
                                const std::shared_ptr<std::atomic_bool>& cancellation)
{
    if (imgs.empty() || poss.size() != static_cast<int>(imgs.size())) {
        return {};
    }

    cv::Mat out = mergeEnsureBgra(imgs[0]);
    if (out.empty()) {
        return {};
    }

    // out is cropped after each merge, so retain the scene coordinate of its pixel (0, 0).
    QPoint outOrigin = poss[0];

    for (int i = 1; i < static_cast<int>(imgs.size()); ++i) {
        if (workerCancellationRequested(cancellation)) return {};
        cv::Mat next = mergeEnsureBgra(imgs[i]);
        if (next.empty()) {
            return {};
        }

        const cv::Point2d shiftV(poss[i].x() - outOrigin.x(),
                                 poss[i].y() - outOrigin.y());
        out = make_canvas_bgra_feather_dt(next, out, shiftV, 80.0f, cancellation);
        outOrigin.setX(std::min(outOrigin.x(), poss[i].x()));
        outOrigin.setY(std::min(outOrigin.y(), poss[i].y()));
    }

    return out;
}

cv::Mat mergeImagesForExport(const std::vector<cv::Mat>& imgs,
                             const QVector<QPoint>& poss,
                             ImageMergeMode mode,
                             const std::shared_ptr<std::atomic_bool>& cancellation)
{
    if (imgs.empty() || workerCancellationRequested(cancellation)) {
        return {};
    }
    if (imgs.size() == 1) {
        return mergeEnsureBgra(imgs[0]);
    }
    if (mode == ImageMergeMode::DistanceL2) {
        return mergeByLegacyDistanceL2(imgs, poss, cancellation);
    }

    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    if (!mergeCalculateBounds(imgs, poss, minX, minY, maxX, maxY)) {
        return {};
    }

    const cv::Size canvasSize(maxX - minX, maxY - minY);
    const auto placed = mergePreparePlacedImages(imgs, poss, minX, minY, cancellation);
    if (placed.empty()) {
        return {};
    }

    switch (mode) {
    case ImageMergeMode::FocusRegion:
        return mergeByFocusRegion(placed, canvasSize, cancellation);
    case ImageMergeMode::FocusStackTenengrad:
        return mergeByTenengradPixel(placed, canvasSize, cancellation);
    case ImageMergeMode::ImageNumberOverwrite:
        return mergeByImageNumberOverwrite(placed, canvasSize, cancellation);
    case ImageMergeMode::DistanceL2:
    default:
        return mergeByLegacyDistanceL2(imgs, poss, cancellation);
    }
}
}

static bool calculateOverlapCropRois(const cv::Mat& input1,
                                     const cv::Mat& input2,
                                     const QPoint& pos1,
                                     const QPoint& pos2,
                                     const cv::Mat1b* fullMask1,
                                     const cv::Mat1b* fullMask2,
                                     cv::Rect& cropRoi1,
                                     cv::Rect& cropRoi2)
{
    if (input1.empty() || input2.empty()) {
        return false;
    }

    const int left = std::max(pos1.x(), pos2.x());
    const int top = std::max(pos1.y(), pos2.y());
    const int right = std::min(pos1.x() + input1.cols, pos2.x() + input2.cols);
    const int bottom = std::min(pos1.y() + input1.rows, pos2.y() + input2.rows);
    if (right <= left || bottom <= top) {
        return false;
    }

    const cv::Rect roi1(left - pos1.x(), top - pos1.y(), right - left, bottom - top);
    const cv::Rect roi2(left - pos2.x(), top - pos2.y(), right - left, bottom - top);

    cv::Mat1b logicalMask1;
    cv::Mat1b logicalMask2;
    if (fullMask1 && fullMask2) {
        logicalMask1 = (*fullMask1)(roi1);
        logicalMask2 = (*fullMask2)(roi2);
    } else {
        logicalMask1 = ImageUtils::alphaMaskFromBGRA(input1(roi1), 0.5); // 0/1
        logicalMask2 = ImageUtils::alphaMaskFromBGRA(input2(roi2), 0.5); // 0/1
    }

    // 重なり領域を得る
    cv::Mat1b andMask;
    cv::bitwise_and(logicalMask1, logicalMask2, andMask);

    // and領域を矩形化する
    cv::Rect rect = ImageUtils::maxRectOnesFromLogical(andMask);
    if (rect.empty()) {
        return false;
    }

    cropRoi1 = cv::Rect(roi1.x + rect.x, roi1.y + rect.y, rect.width, rect.height);
    cropRoi2 = cv::Rect(roi2.x + rect.x, roi2.y + rect.y, rect.width, rect.height);
    return true;
}

// 2つの画像から重なり領域をクロップして取り出す
return_struct2 Crop_2ImageTo2Image(const cv::Mat& input1, const cv::Mat& input2,
                                   QSize px1, QPoint pos1, QSize px2, QPoint pos2)
{
    Q_UNUSED(px1);
    Q_UNUSED(px2);

    return_struct2 r;
    cv::Rect cropRoi1;
    cv::Rect cropRoi2;
    if (!calculateOverlapCropRois(input1, input2, pos1, pos2,
                                  nullptr, nullptr, cropRoi1, cropRoi2)) {
        return r;
    }

    // 重なり領域をcropして取り出す
    cv::cvtColor(input1(cropRoi1), r.img1, cv::COLOR_BGRA2BGR);
    cv::cvtColor(input2(cropRoi2), r.img2, cv::COLOR_BGRA2BGR);
    return r;
};

// SSIM計算関数
static double ssim_single_channel(const cv::Mat& i1u8, const cv::Mat& i2u8)
{
    cv::Mat I1, I2;
    i1u8.convertTo(I1, CV_32F);
    i2u8.convertTo(I2, CV_32F);

    const double C1 = (0.01 * 255) * (0.01 * 255);
    const double C2 = (0.03 * 255) * (0.03 * 255);

    cv::Mat mu1, mu2;
    cv::GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
    cv::GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

    cv::Mat mu1_2 = mu1.mul(mu1);
    cv::Mat mu2_2 = mu2.mul(mu2);
    cv::Mat mu1_mu2 = mu1.mul(mu2);

    cv::Mat sigma1_2, sigma2_2, sigma12;
    cv::GaussianBlur(I1.mul(I1), sigma1_2, cv::Size(11, 11), 1.5);
    sigma1_2 -= mu1_2;

    cv::GaussianBlur(I2.mul(I2), sigma2_2, cv::Size(11, 11), 1.5);
    sigma2_2 -= mu2_2;

    cv::GaussianBlur(I1.mul(I2), sigma12, cv::Size(11, 11), 1.5);
    sigma12 -= mu1_mu2;

    cv::Mat t1 = 2 * mu1_mu2 + C1;
    cv::Mat t2 = 2 * sigma12 + C2;
    cv::Mat t3 = mu1_2 + mu2_2 + C1;
    cv::Mat t4 = sigma1_2 + sigma2_2 + C2;

    cv::Mat ssim_map = (t1.mul(t2)) / (t3.mul(t4));
    return cv::mean(ssim_map)[0];
}

double ssim(const cv::Mat& a, const cv::Mat& b)
{
    CV_Assert(!a.empty() && !b.empty());
    CV_Assert(a.size() == b.size());

    cv::Mat A = a, B = b;

    // SSIMは基本「同じチャンネル数」で。迷ったらグレースケールに落とすのが簡単
    if (A.channels() == 3) cv::cvtColor(A, A, cv::COLOR_BGR2GRAY);
    if (B.channels() == 3) cv::cvtColor(B, B, cv::COLOR_BGR2GRAY);
    if (A.channels() == 4) cv::cvtColor(A, A, cv::COLOR_BGRA2GRAY);
    if (B.channels() == 4) cv::cvtColor(B, B, cv::COLOR_BGRA2GRAY);

    CV_Assert(A.type() == CV_8U && B.type() == CV_8U);
    return ssim_single_channel(A, B);
}

// SSIM 各スレッドの計算処理
static double SSIM_calc_oneshot(const SSIM_TaskInput& in)
{
    // 2枚目画像を(dx,dy)移動
    QPoint in_pos2(in.pos2.x() + in.dx, in.pos2.y() + in.dy);

    // 重なり領域をcropして取り出す。
    return_struct2 r_st = Crop_2ImageTo2Image(in.input1, in.input2, in.px1, in.pos1, in.px2, in_pos2);
    cv::Mat crop1 = r_st.img1;
    cv::Mat crop2 = r_st.img2;

    /*
    cv::imshow("crop1",crop1);
    cv::imshow("crop2",crop2);
    cv::waitKey(0);
    */

    if (crop1.rows == 0) {
        return 0.0;
    }

    double v = ssim(crop1, crop2);
    return v;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->leftScrollArea->setMinimumWidth(
        image_stitcher::platform::controlPanelMinimumWidth(
            ui->leftScrollArea->minimumWidth()));

    settingsPersistenceTimer = new QTimer(this);
    settingsPersistenceTimer->setSingleShot(true);
    settingsPersistenceTimer->setInterval(180);
    connect(settingsPersistenceTimer, &QTimer::timeout, this, [this]() {
        persistAlignmentSettings();
        persistArrangementSettings();
        persistControlPanelWidth();
    });

    ui->mainSplitter->setChildrenCollapsible(false);
    ui->mainSplitter->setStretchFactor(0, 0);
    ui->mainSplitter->setStretchFactor(1, 1);
    const int minimumControlPanelWidth = ui->leftScrollArea->minimumWidth();
    const int savedControlPanelWidth =
        AppSettings::controlPanelWidth(minimumControlPanelWidth);
    QTimer::singleShot(0, this, [this, savedControlPanelWidth]() {
        const int availableWidth = std::max(1, ui->mainSplitter->width());
        ui->mainSplitter->setSizes(
            {savedControlPanelWidth,
             std::max(1, availableWidth - savedControlPanelWidth)});
        connect(ui->mainSplitter, &QSplitter::splitterMoved,
                this, [this](int, int) { scheduleSettingsPersistence(); });
    });

    const ApplicationDefaultSettings& defaults = AppSettings::defaults();
    const AlignmentDefaultSettings alignment = AppSettings::alignmentOptions();
    const ArrangementDefaultSettings arrangement = AppSettings::arrangementOptions();
    const TrwsPamiDefaultSettings trwsPami = AppSettings::trwsPamiOptions();
    const LeastSquaresDefaultSettings leastSquares = AppSettings::leastSquaresOptions();
    const ImageMergeDefaultSettings imageMerge = AppSettings::imageMergeOptions();

    pa_TF = trwsPami.localEnabled;
    pa_num = trwsPami.localImageCount;
    pa_auto_increment_TF = trwsPami.localAutoIncrement;
    pa_increment = trwsPami.localImageCountIncrement;
    pa_increment_count = trwsPami.localIncrementCount;
    pa_radi = trwsPami.localSearchRadius;
    pa_opti = trwsPami.localMaxIterations;
    pa_itr = trwsPami.localMaxLoops;
    all_TF = trwsPami.globalEnabled;
    all_radi = trwsPami.globalSearchRadius;
    all_opti = trwsPami.globalMaxIterations;
    all_itr = trwsPami.globalMaxLoops;
    leastSquaresSettings.regressionThreshold = leastSquares.regressionThreshold;
    leastSquaresSettings.relativeThreshold = leastSquares.relativeThreshold;
    leastSquaresSettings.absoluteThreshold = leastSquares.absoluteThreshold;
    leastSquaresSettings.maxPairErrorForRelative =
        leastSquares.maxPairErrorForRelative;
    imageMergeSettings.mode = static_cast<ImageMergeMode>(imageMerge.mode);
    defaultImageOpacity = defaults.canvas.selectedImageOpacityPercent;
    imageHighlightColor = QColor(defaults.canvas.highlightColor);
    configuredHorizontalImageCount = arrangement.horizontalImageCount;
    configuredVerticalImageCount = arrangement.verticalImageCount;
    orikaeshi = arrangement.zigzag;

    ui->horizontalSlider->setValue(alignment.horizontalOverlapPercent);
    ui->spinBox_2->setValue(alignment.horizontalOverlapPercent);
    ui->horizontalSlider_2->setValue(alignment.verticalOverlapPercent);
    ui->spinBox_3->setValue(alignment.verticalOverlapPercent);
    ui->horizontalSlider_3->setValue(alignment.searchRangePercent);
    ui->spinBox->setValue(alignment.searchRangePercent);
    ui->cornerSelector->setUI(arrangement.direction);
    ui->cornerSelector->setZigzagChecked(arrangement.zigzag);
    orikaeshi = ui->cornerSelector->zigzagChecked();
    ui->sliderOpacity1->setValue(defaultImageOpacity);
    ui->spinOpacity1->setValue(defaultImageOpacity);
    ui->checkBox->setChecked(defaults.canvas.layoutLocked);
    ui->checkBox_2->setChecked(defaults.canvas.useCanvasAsSource);

    // メニューバー左端のファイルメニュー、その右にキャンパスと設定。
    fileMenu = ui->menubar->addMenu(tr("ファイル"));
    projectOpenAction = fileMenu->addAction(tr("プロジェクトを開く"));
    projectOpenAction->setShortcut(QKeySequence::Open);
    projectSaveAction = fileMenu->addAction(tr("プロジェクト保存"));
    projectSaveAction->setShortcut(QKeySequence::Save);
    projectSaveAsAction = fileMenu->addAction(tr("名前を付けてプロジェクト保存"));
    fileMenu->addSeparator();
    projectPngExportAction = fileMenu->addAction(tr("PNGエクスポート"));
    connect(projectOpenAction, &QAction::triggered,
            this, &MainWindow::showProjectOpenDialog);
    connect(projectSaveAction, &QAction::triggered,
            this, &MainWindow::saveProject);
    connect(projectSaveAsAction, &QAction::triggered,
            this, [this]() { saveProjectAs(); });
    connect(projectPngExportAction, &QAction::triggered,
            this, &MainWindow::png_export);

    canvasMenu = ui->menubar->addMenu(tr("キャンパス"));
    canvasBackgroundAction = canvasMenu->addAction(tr("背景色"));
    connect(canvasBackgroundAction, &QAction::triggered,
            this, &MainWindow::showCanvasBackgroundDialog);
    applicationSettingsAction = ui->menubar->addAction(tr("設定"));
    connect(applicationSettingsAction, &QAction::triggered,
            this, &MainWindow::show_application_settings);

    // 入力画像設定ボタン
    connect(ui->pushButton_1, &QPushButton::clicked, this, [this]() {
        if (!fileInputDialog) {
            QHash<QString, QByteArray> embeddedImages;
            for (int i = 0; i < input_files.size()
                            && i < projectImageData.size(); ++i) {
                if (!projectImageData[i].isEmpty()) {
                    embeddedImages.insert(input_files[i], projectImageData[i]);
                }
            }
            fileInputDialog = new FileInputDialog(
                input_files, embeddedImages, this);
            fileInputDialog->setAttribute(Qt::WA_DeleteOnClose);
            fileInputDialog->setModal(false);
            fileInputDialog->setWindowModality(Qt::NonModal);
            trackDialogSize(fileInputDialog, QStringLiteral("fileInput"),
                            QSize(800, 600));
            connect(fileInputDialog, &QDialog::accepted, this, [this]() {
                if (!fileInputDialog) {
                    return;
                }
                const QStringList inputFilesNew = fileInputDialog->selectedFiles();
                if (inputFilesNew != input_files) {
                    File_input(input_files, inputFilesNew);
                }
            });
            connect(fileInputDialog, &QObject::destroyed, this, [this]() {
                fileInputDialog = nullptr;
            });
        }
        fileInputDialog->show();
        fileInputDialog->raise();
        fileInputDialog->activateWindow();
    });

    // 画像同士の重なり割合の設定
    connect(ui->horizontalSlider, &QSlider::valueChanged,
            ui->spinBox_2, &QSpinBox::setValue);
    connect(ui->spinBox_2, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider, &QSlider::setValue);
    connect(ui->spinBox_2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int){
        arrangeSettingsChanged();
        scheduleSettingsPersistence();
    });

    connect(ui->horizontalSlider_2, &QSlider::valueChanged,
            ui->spinBox_3, &QSpinBox::setValue);
    connect(ui->spinBox_3, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_2, &QSlider::setValue);
    connect(ui->spinBox_3, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int){
        arrangeSettingsChanged();
        scheduleSettingsPersistence();
    });

    connect(ui->horizontalSlider_3, &QSlider::valueChanged,
            ui->spinBox, &QSpinBox::setValue);
    connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_3, &QSlider::setValue);
    connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int){
        ui->checkBox_2->setChecked(false);
        scheduleSettingsPersistence();
    });

    // 配列指定エリアの設定
    connect(ui->cornerSelector, &CornerDirectionSelector::stateChanged,
            this, [this](int state) {
                Q_UNUSED(state);
                arrangeSettingsChanged();
                scheduleSettingsPersistence();
            });

    connect(ui->cornerSelector, &CornerDirectionSelector::r_Changed,
            this, [this](int rows){
                configuredVerticalImageCount = rows;
                arrangeSettingsChanged();
                scheduleSettingsPersistence();
            });

    connect(ui->cornerSelector, &CornerDirectionSelector::c_Changed,
            this, [this](int cols){
                configuredHorizontalImageCount = cols;
                arrangeSettingsChanged();
                scheduleSettingsPersistence();
            });

    connect(ui->cornerSelector, &CornerDirectionSelector::zigzagChanged,
            this, [this](bool zigzag) {
        orikaeshi = zigzag;
        arrangeSettingsChanged();
        scheduleSettingsPersistence();
    });

    connect(ui->graphicsView, &maincampus::zoomChanged,
            this, [this](int pct){ zoomLabel->setText(QString("%1%").arg(pct)); });
    connect(ui->graphicsView, &maincampus::filesDropped,
            this, &MainWindow::handleCanvasFilesDropped);

    scene = new QGraphicsScene(this);
    scene->installEventFilter(this);

    ui->graphicsView->setScene(scene);

    // imageの削除
    auto *actDelete = new QAction(this);
    actDelete->setShortcut(QKeySequence::Delete);
    actDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut); // MainWindow配下で有効
    addAction(actDelete);
    connect(actDelete, &QAction::triggered, this, &MainWindow::deleteSelectedItems);

    auto* actUndo = new QAction(this);
    actUndo->setShortcut(QKeySequence::Undo);
    actUndo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(actUndo);
    connect(actUndo, &QAction::triggered, this, &MainWindow::undoCanvasHistory);

    auto* actRedo = new QAction(this);
    actRedo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    actRedo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(actRedo);
    connect(actRedo, &QAction::triggered, this, &MainWindow::redoCanvasHistory);

    auto* actFind = new QAction(this);
    actFind->setShortcut(QKeySequence::Find);
    actFind->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(actFind);
    connect(actFind, &QAction::triggered, this, &MainWindow::showImageHighlightDialog);

    connect(ui->pushButton_7, &QPushButton::clicked,
            this, &MainWindow::showCanvasHistoryDialog);

    // 拡大率表示
    statusMessageLabel = new QLabel(this);
    statusMessageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    statusBar()->setStyleSheet(QStringLiteral("QStatusBar::item { border: none; }"));
    statusBar()->addWidget(statusMessageLabel, 1);
    zoomLabel = new QLabel(this);
    zoomLabel->setText("100%");
    statusBar()->addPermanentWidget(zoomLabel);
    updateStatusBarTheme();

    // 透明度制御
    ui->sliderOpacity1->setRange(0, 100);
    ui->spinOpacity1->setRange(0, 100);
    ui->sliderOpacity1->setValue(defaultImageOpacity);
    ui->spinOpacity1->setValue(defaultImageOpacity);

    // --- 同期：slider <-> spin（画像1）---
    connect(ui->sliderOpacity1, &QSlider::valueChanged,
            ui->spinOpacity1, &QSpinBox::setValue);
    connect(ui->spinOpacity1, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->sliderOpacity1, &QSlider::setValue);

    // 透明度反映（画像1）
    connect(ui->sliderOpacity1, &QSlider::valueChanged,
            this, &MainWindow::onOpacity1Changed);

    ui->label_2->setEnabled(false);
    ui->sliderOpacity1->setEnabled(false);
    ui->spinOpacity1->setEnabled(false);

    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::onSceneSelectionChanged);
    applyCanvasBackgroundSetting(AppSettings::canvasBackground());

    // 計算開始ボタン
    connect(ui->pushButton_Calc1, &QPushButton::clicked, this, &MainWindow::calc_iFFT);

    // 計算完了通知を受け取る
    connect(&watcher, &QFutureWatcher<ifft_thread_output>::finished, this, &MainWindow::calc_finish_1);
    connect(&watcher_re, &QFutureWatcher<ifft_thread_output>::finished, this, &MainWindow::calc_finish_2);
    connect(&watcher, &QFutureWatcher<ifft_thread_output>::progressValueChanged,
            this, [this](int value) {
        updateCalculationProgress(value, watcher.progressMaximum(),
                                  tr("位置合わせを計算中"));
    });
    connect(&watcher_re, &QFutureWatcher<ifft_thread_output>::progressValueChanged,
            this, [this](int value) {
        updateCalculationProgress(value, watcher_re.progressMaximum(),
                                  tr("不良箇所を再計算中"));
    });

    // 画像作成ボタン
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::make_image);
    ui->pushButton_2->setEnabled(false);
    connect(ui->pushButton_11, &QPushButton::clicked, this, &MainWindow::show_merge_settings);

    // PNG exportボタン
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::png_export);
    ui->pushButton_4->setEnabled(false);

    // 結合品質の詳細
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::show_detail);
    connect(ui->pushButton_9, &QPushButton::clicked, this, &MainWindow::show_detail_least_squares);

    // レイアウトロックのチェックボックス
    connect(ui->checkBox, &QCheckBox::toggled,this, &MainWindow::posi_lock);

    // 全体最適化ボタン
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::calc_TRWS);
    connect(ui->pushButton_8, &QPushButton::clicked, this, &MainWindow::calc_least_squares);

    // 未実装の一方向はCornerDirectionSelector内で常に無効化する。
    ui->pushButton_3->setEnabled(false);
    ui->pushButton_8->setEnabled(false);

    // 選択中の画像番号を表示
    ui->label_9->setText(tr("なし"));
    ui->label_8->setEnabled(false);
    ui->label_9->setEnabled(false);
    ui->pushButton->setEnabled(false);
    ui->pushButton_5->setEnabled(false);
    ui->label_4->setAlignment(Qt::AlignCenter);
    ui->label_4->setText("");
    ui->pushButton_9->setEnabled(false);
    ui->checkBox_2->setEnabled(true);
    ui->checkBox_2->setChecked(defaults.canvas.useCanvasAsSource);

    // 画像を作成 finish通知
    connect(&image_make_Watcher, &QFutureWatcher<cv::Mat>::finished, this, [this]() {
        if (calculationCancellationRequested() || image_make_Watcher.future().isCanceled()) {
            imageMakeSourcePositions.clear();
            ui->label_6->setText(tr("キャンセル"));
            ui->pushButton_2->setEnabled(true);
            ui->pushButton_11->setEnabled(true);
            ui->pushButton_4->setEnabled(!output_img.empty());
            finishCancelledCalculation(tr("画像作成をキャンセルしました。"));
            return;
        }
        cv::Mat result = image_make_Watcher.result();
        const bool sourceChanged =
            !samePositions(imageMakeSourcePositions, readScenePositions());
        if (sourceChanged) {
            result.release();
        }
        output_img = result;
        imageMakeSourcePositions.clear();
        ui->label_6->setText(sourceChanged
                                 ? tr("キャンパス更新・再作成が必要")
                                 : (output_img.empty() ? tr("不良") : tr("完了")));
        ui->pushButton_2->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        ui->pushButton_4->setEnabled(!output_img.empty());
        hideCalculationProgressDialog();
        if (calc_finish_sig) {
            emit makeimageFinished();
        }
    });
    //ui->spinBox_4->setValue(2);

    // 最適化ボタン
    connect(ui->pushButton_6, &QPushButton::clicked, this, &MainWindow::show_opti_settings);
    connect(ui->pushButton_10, &QPushButton::clicked, this, &MainWindow::show_least_squares_settings);
    trwsWatcher = new QFutureWatcher<CalcTRWSoutput>(this);
    connect(trwsWatcher, &QFutureWatcher<CalcTRWSoutput>::finished, this, [this]() {calc_TRWS_finish();});
    leastSquaresWatcher = new QFutureWatcher<CalcLeastSquaresOutput>(this);
    connect(leastSquaresWatcher, &QFutureWatcher<CalcLeastSquaresOutput>::finished,
            this, [this]() { calc_least_squares_finish(); });

    // 最適化結果の詳細
    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::show_detail_opti);

    m_detailDialog = new detail_opti_dialog(this);
    trackDialogSize(m_detailDialog, QStringLiteral("optimizationDetails"),
                    QSize(1000, 560));

    vulkanScanWatcher = new QFutureWatcher<VulkanDeviceScanResult>(this);
    // タイマー発火前に設定画面を開いた場合も「未検出」ではなく待機中と表示する。
    vulkanDetectionInProgress = VulkanSsimEngine::isBuilt();
    connect(vulkanScanWatcher, &QFutureWatcher<VulkanDeviceScanResult>::finished,
            this, [this]() {
        vulkanDetectionInProgress = false;
        vulkanScanResult = vulkanScanWatcher->result();
        if (applicationSettingsDialog) {
            applicationSettingsDialog->setVulkanScanResult(vulkanScanResult);
        }
    });

    // イベントループ開始後にバックグラウンド検出し、起動時間へ影響させない。
    QTimer::singleShot(750, this, &MainWindow::startDelayedVulkanDetection);
    posi_lock(ui->checkBox->isChecked());
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    persistAlignmentSettings();
    persistArrangementSettings();
    persistControlPanelWidth();
    AppSettings::setWindowSize(QStringLiteral("mainWindow"), size());
    saveTrackedWindowSizes();

    // 終了時の queued signal が破棄中オブジェクトへ届くのを防ぐ
    disconnect(&watcher, nullptr, this, nullptr);
    disconnect(&watcher_re, nullptr, this, nullptr);
    disconnect(&image_make_Watcher, nullptr, this, nullptr);
    disconnect(trwsWatcher, nullptr, this, nullptr);
    disconnect(leastSquaresWatcher, nullptr, this, nullptr);
    disconnect(vulkanScanWatcher, nullptr, this, nullptr);
    if (scene) {
        disconnect(scene, nullptr, this, nullptr);
    }

    clearImageHighlights();
    if (watcher.isRunning()) { watcher.cancel(); watcher.waitForFinished(); }
    if (watcher_re.isRunning()) { watcher_re.cancel(); watcher_re.waitForFinished(); }
    if (image_make_Watcher.isRunning()) { image_make_Watcher.cancel(); image_make_Watcher.waitForFinished(); }
    if (trwsWatcher->isRunning()) { trwsWatcher->cancel(); trwsWatcher->waitForFinished(); }
    if (leastSquaresWatcher->isRunning()) { leastSquaresWatcher->cancel(); leastSquaresWatcher->waitForFinished(); }
    if (vulkanScanWatcher->isRunning()) { vulkanScanWatcher->cancel(); vulkanScanWatcher->waitForFinished(); }
    delete ui;
}

void MainWindow::show_application_settings()
{
    if (!applicationSettingsDialog) {
        applicationSettingsDialog = new ApplicationSettingsDialog(this);
        applicationSettingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);
        trackDialogSize(applicationSettingsDialog,
                        QStringLiteral("applicationSettings"),
                        QSize(700, 440));
        connect(applicationSettingsDialog,
                &ApplicationSettingsDialog::resetRequested,
                this, &MainWindow::handleSettingsReset);
    }

    if (vulkanDetectionInProgress) {
        applicationSettingsDialog->setVulkanDetectionInProgress();
    } else {
        applicationSettingsDialog->setVulkanScanResult(vulkanScanResult);
    }
    applicationSettingsDialog->show();
    applicationSettingsDialog->raise();
    applicationSettingsDialog->activateWindow();
}

void MainWindow::showCanvasBackgroundDialog()
{
    if (!canvasBackgroundDialog) {
        canvasBackgroundDialog = new QDialog(this);
        canvasBackgroundDialog->setWindowTitle(tr("背景色"));
        canvasBackgroundDialog->setModal(false);
        canvasBackgroundDialog->setWindowModality(Qt::NonModal);
        trackDialogSize(canvasBackgroundDialog,
                        QStringLiteral("canvasBackground"),
                        QSize(580, 470));

        auto* layout = new QVBoxLayout(canvasBackgroundDialog);
        canvasBackgroundColorPicker =
            new QColorDialog(canvasBackgroundColor, canvasBackgroundDialog);
        canvasBackgroundColorPicker->setWindowFlags(Qt::Widget);
        canvasBackgroundColorPicker->setOption(QColorDialog::NoButtons, true);
        canvasNoColorCheck = new QCheckBox(
            tr("無色（背景を塗りつぶさない）"), canvasBackgroundDialog);
        connect(canvasNoColorCheck, &QCheckBox::toggled,
                canvasBackgroundColorPicker, &QWidget::setDisabled);

        layout->addWidget(canvasBackgroundColorPicker, 1);
        layout->addWidget(canvasNoColorCheck);

        auto* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            canvasBackgroundDialog);
        connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
            if (canvasNoColorCheck->isChecked()) {
                applyCanvasBackgroundSetting(QStringLiteral("none"));
                AppSettings::setCanvasBackground(QStringLiteral("none"));
            } else {
                const QColor selectedColor = canvasBackgroundColorPicker->currentColor();
                if (selectedColor.isValid()) {
                    const QString setting = selectedColor.name(QColor::HexArgb);
                    applyCanvasBackgroundSetting(setting);
                    AppSettings::setCanvasBackground(setting);
                }
            }
            canvasBackgroundDialog->accept();
        });
        connect(buttonBox, &QDialogButtonBox::rejected,
                canvasBackgroundDialog, &QDialog::reject);
        layout->addWidget(buttonBox);
    }

    canvasBackgroundDialog->setWindowTitle(tr("背景色"));
    canvasNoColorCheck->setText(tr("無色（背景を塗りつぶさない）"));
    canvasNoColorCheck->setChecked(canvasBackgroundIsTransparent);
    canvasBackgroundColorPicker->setCurrentColor(canvasBackgroundColor);
    canvasBackgroundColorPicker->setEnabled(!canvasBackgroundIsTransparent);
    canvasBackgroundDialog->show();
    canvasBackgroundDialog->raise();
    canvasBackgroundDialog->activateWindow();
}

void MainWindow::applyCanvasBackgroundSetting(const QString& setting)
{
    const QString normalized = setting.trimmed().toLower();
    if (normalized == QStringLiteral("none")
        || normalized == QStringLiteral("transparent")
        || normalized == QStringLiteral("no-color")) {
        canvasBackgroundIsTransparent = true;
        ui->graphicsView->setBackgroundBrush(Qt::NoBrush);
        return;
    }

    const QColor color(setting);
    canvasBackgroundColor = color.isValid() ? color : QColor(Qt::black);
    canvasBackgroundIsTransparent = false;
    ui->graphicsView->setBackgroundBrush(canvasBackgroundColor);
}

void MainWindow::retranslateDynamicUi()
{
    updateWindowTitle();
    if (fileMenu) {
        fileMenu->setTitle(tr("ファイル"));
    }
    if (projectOpenAction) {
        projectOpenAction->setText(tr("プロジェクトを開く"));
    }
    if (projectSaveAction) {
        projectSaveAction->setText(tr("プロジェクト保存"));
    }
    if (projectSaveAsAction) {
        projectSaveAsAction->setText(tr("名前を付けてプロジェクト保存"));
    }
    if (projectPngExportAction) {
        projectPngExportAction->setText(tr("PNGエクスポート"));
    }
    if (applicationSettingsAction) {
        applicationSettingsAction->setText(tr("設定"));
    }
    if (canvasMenu) {
        canvasMenu->setTitle(tr("キャンパス"));
    }
    if (canvasBackgroundAction) {
        canvasBackgroundAction->setText(tr("背景色"));
    }
    if (canvasBackgroundDialog) {
        canvasBackgroundDialog->setWindowTitle(tr("背景色"));
    }
    if (canvasNoColorCheck) {
        canvasNoColorCheck->setText(tr("無色（背景を塗りつぶさない）"));
    }
    if (canvasHistoryDialog) {
        canvasHistoryDialog->setWindowTitle(tr("変更履歴"));
    }
    if (canvasHistoryLegendLabels.size() == 3) {
        canvasHistoryLegendLabels[0]->setText(tr("現在地"));
        canvasHistoryLegendLabels[1]->setText(tr("位相相関結果"));
        canvasHistoryLegendLabels[2]->setText(tr("最適化結果"));
    }
    if (canvasHistoryGraphWidget) {
        canvasHistoryGraphWidget->update();
    }
    if (imageHighlightDialog) {
        imageHighlightDialog->setWindowTitle(tr("画像ハイライト"));
    }
    if (imageHighlightIdLabel) {
        imageHighlightIdLabel->setText(tr("画像ID"));
    }
    if (imageHighlightEdit) {
        imageHighlightEdit->setPlaceholderText(tr("例: 2-6,8-10"));
    }
    if (imageHighlightColorButton) {
        imageHighlightColorButton->setText(tr("色"));
    }
    if (imageHighlightApplyButton) {
        imageHighlightApplyButton->setText(tr("ハイライト"));
    }
    if (imageHighlightClearButton) {
        imageHighlightClearButton->setText(tr("クリア"));
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        retranslateDynamicUi();
    } else if (event->type() == QEvent::ApplicationPaletteChange
               || event->type() == QEvent::PaletteChange) {
        updateStatusBarTheme();
    }
}

void MainWindow::updateStatusBarTheme()
{
    if (!statusMessageLabel || !zoomLabel) {
        return;
    }

    const QPalette sourcePalette = palette();
    const auto updateLabelPalette = [&sourcePalette](QLabel* label) {
        QPalette labelPalette = label->palette();
        for (const QPalette::ColorGroup group : {
                 QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
            labelPalette.setBrush(
                group, QPalette::WindowText,
                sourcePalette.brush(group, QPalette::WindowText));
        }
        label->setForegroundRole(QPalette::WindowText);
        label->setPalette(labelPalette);
    };
    updateLabelPalette(statusMessageLabel);
    updateLabelPalette(zoomLabel);
}

int MainWindow::maximumCanvasHistorySequence() const
{
    int maximum = 0;
    for (const CanvasHistoryNode& node : canvasHistoryTree) {
        maximum = std::max(maximum, node.sequence);
    }
    return maximum;
}

bool MainWindow::hasUnsavedProjectHistory() const
{
    if (canvasHistoryTree.isEmpty()) {
        return false;
    }
    for (const CanvasHistoryNode& node : canvasHistoryTree) {
        if (node.sequence > savedProjectHistorySequence) {
            return true;
        }
    }
    return false;
}

bool MainWindow::hasExistingProjectData() const
{
    return !input_files.isEmpty() || !canvasHistoryTree.isEmpty()
           || !output_img.empty();
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("%1 v%2")
                        .arg(QCoreApplication::applicationName(),
                             QCoreApplication::applicationVersion());
    if (!linkedProjectFilePath.isEmpty()) {
        title += QStringLiteral(" (%1)").arg(
            QDir::toNativeSeparators(
                QFileInfo(linkedProjectFilePath).absoluteFilePath()));
    }
    if (hasUnsavedProjectHistory()) {
        title += QLatin1Char('*');
    }
    setWindowTitle(title);
}

void MainWindow::showProjectOpenDialog()
{
    if (isAlignmentOrOptimizationRunning()) {
        showNonModalMessage(QMessageBox::Information, tr("プロジェクトを開く"),
                            tr("計算中はプロジェクトを開けません。"));
        return;
    }

    auto* dialog = new QFileDialog(
        this, tr("プロジェクトを開く"), QString(),
        tr("Image Stitcher Auto プロジェクト (*.isauto)"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setDefaultSuffix(QStringLiteral("isauto"));
    dialog->setOption(
        QFileDialog::DontUseNativeDialog,
        !image_stitcher::platform::useNativeFileDialogs());
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->resize(AppSettings::windowSize(QStringLiteral("projectOpen"),
                                           QSize(900, 600)));
    connect(dialog, &QFileDialog::fileSelected,
            this, &MainWindow::requestProjectOpen);
    connect(dialog, &QDialog::finished, dialog, [dialog]() {
        AppSettings::setWindowSize(QStringLiteral("projectOpen"),
                                   dialog->size());
    });
    dialog->show();
}

void MainWindow::saveProject()
{
    if (linkedProjectFilePath.isEmpty()) {
        saveProjectAs();
        return;
    }
    saveProjectToPath(linkedProjectFilePath);
}

void MainWindow::saveProjectAs(std::function<void(bool)> completion)
{
    auto* dialog = new QFileDialog(
        this, tr("名前を付けてプロジェクト保存"), QString(),
        tr("Image Stitcher Auto プロジェクト (*.isauto)"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setDefaultSuffix(QStringLiteral("isauto"));
    dialog->setOption(
        QFileDialog::DontUseNativeDialog,
        !image_stitcher::platform::useNativeFileDialogs());
    dialog->setOption(QFileDialog::DontConfirmOverwrite, true);
    dialog->setModal(false);
    dialog->setWindowModality(Qt::NonModal);
    dialog->resize(AppSettings::windowSize(QStringLiteral("projectSaveAs"),
                                           QSize(900, 600)));
    if (!linkedProjectFilePath.isEmpty()) {
        dialog->selectFile(linkedProjectFilePath);
    }

    auto completed = std::make_shared<bool>(false);
    auto finish = [completion = std::move(completion), completed](bool result) {
        if (*completed) return;
        *completed = true;
        if (completion) completion(result);
    };
    connect(dialog, &QFileDialog::fileSelected, this,
            [this, finish](QString path) {
        if (QFileInfo(path).suffix().compare(QStringLiteral("isauto"),
                                             Qt::CaseInsensitive) != 0) {
            path += QStringLiteral(".isauto");
        }
        if (!QFileInfo::exists(path)) {
            finish(saveProjectToPath(path));
            return;
        }

        auto* confirmation = new QMessageBox(
            QMessageBox::Warning, tr("上書き確認"),
            tr("ファイルは既に存在します。上書きしますか？\n%1")
                .arg(QDir::toNativeSeparators(path)),
            QMessageBox::Yes | QMessageBox::No, this);
        confirmation->setAttribute(Qt::WA_DeleteOnClose);
        confirmation->setDefaultButton(QMessageBox::No);
        confirmation->setModal(false);
        confirmation->setWindowModality(Qt::NonModal);
        connect(confirmation, &QDialog::finished, this,
                [this, path, finish](int result) {
            finish(result == QMessageBox::Yes
                       ? saveProjectToPath(path) : false);
        });
        confirmation->show();
    });
    connect(dialog, &QDialog::rejected, this, [finish]() { finish(false); });
    connect(dialog, &QDialog::finished, dialog, [dialog]() {
        AppSettings::setWindowSize(QStringLiteral("projectSaveAs"),
                                   dialog->size());
    });
    dialog->show();
}

bool MainWindow::saveProjectToPath(const QString& requestedPath)
{
    if (requestedPath.trimmed().isEmpty()) return false;

    if (optimizationSettingsDialog && optimizationSettingsDialog->isVisible()) {
        readOptimizationSettingsDialog();
    }
    if (leastSquaresSettingsDialog && leastSquaresSettingsDialog->isVisible()) {
        readLeastSquaresSettingsDialog();
    }
    if (mergeSettingsDialog && mergeSettingsDialog->isVisible()) {
        readMergeSettingsDialog();
    }

    QString path = QFileInfo(requestedPath).absoluteFilePath();
    if (QFileInfo(path).suffix().compare(QStringLiteral("isauto"),
                                         Qt::CaseInsensitive) != 0) {
        path += QStringLiteral(".isauto");
    }

    syncPossFromScene();

    QVariantMap project;
    project.insert(QStringLiteral("format"), QStringLiteral("Image Stitcher Auto"));
    project.insert(QStringLiteral("applicationVersion"),
                   QCoreApplication::applicationVersion());

    QVariantList images;
    QVector<QByteArray> savedImageData;
    images.reserve(input_files.size());
    savedImageData.reserve(input_files.size());
    for (int i = 0; i < input_files.size(); ++i) {
        QByteArray data = i < projectImageData.size()
                              ? projectImageData[i]
                              : QByteArray();
        if (data.isEmpty() && i < items.size() && items[i]) {
            data = pixmapAsPng(items[i]->pixmap());
        }
        if (data.isEmpty()) {
            QFile source(input_files[i]);
            if (source.open(QIODevice::ReadOnly)) {
                data = source.readAll();
            }
        }
        if (data.isEmpty()) {
            showNonModalMessage(
                QMessageBox::Warning, tr("プロジェクト保存"),
                tr("画像「%1」をプロジェクトへ保存できませんでした。")
                    .arg(QFileInfo(input_files[i]).fileName()));
            return false;
        }

        QVariantMap image;
        image.insert(QStringLiteral("path"), input_files[i]);
        image.insert(QStringLiteral("data"), data);
        const QPoint position = i < items.size() && items[i]
                                    ? items[i]->pos().toPoint()
                                    : QPoint();
        image.insert(QStringLiteral("position"), pointsToVariant({position}).first());
        image.insert(QStringLiteral("opacity"),
                     i < toumeido.size() ? toumeido[i] : defaultImageOpacity);
        image.insert(QStringLiteral("selected"),
                     i < items.size() && items[i] && items[i]->isSelected());
        images.push_back(image);
        savedImageData.push_back(data);
    }
    project.insert(QStringLiteral("images"), images);

    QVariantMap settings;
    const VulkanExecutionOptions vulkanSettings = AppSettings::vulkanOptions();
    settings.insert(QStringLiteral("applicationTheme"),
                    static_cast<int>(AppSettings::theme()));
    settings.insert(QStringLiteral("applicationLanguage"),
                    static_cast<int>(AppSettings::language()));
    settings.insert(QStringLiteral("vulkanEnabled"), vulkanSettings.enabled);
    settings.insert(QStringLiteral("ignoreVramLimit"),
                    vulkanSettings.ignoreVramLimit);
    settings.insert(QStringLiteral("vulkanDeviceKey"), vulkanSettings.deviceKey);
    settings.insert(QStringLiteral("confirmProjectSaveOnClose"),
                    AppSettings::confirmProjectSaveOnClose());
    settings.insert(QStringLiteral("horizontalOverlap"), ui->spinBox_2->value());
    settings.insert(QStringLiteral("verticalOverlap"), ui->spinBox_3->value());
    settings.insert(QStringLiteral("searchRange"), ui->spinBox->value());
    settings.insert(QStringLiteral("direction"), ui->cornerSelector->getStatus());
    settings.insert(QStringLiteral("horizontalCount"), configuredHorizontalImageCount);
    settings.insert(QStringLiteral("verticalCount"), configuredVerticalImageCount);
    settings.insert(QStringLiteral("zigzag"), ui->cornerSelector->zigzagChecked());
    settings.insert(QStringLiteral("layoutLocked"), ui->checkBox->isChecked());
    settings.insert(QStringLiteral("useCanvasSource"), ui->checkBox_2->isChecked());
    settings.insert(QStringLiteral("defaultOpacity"), defaultImageOpacity);
    settings.insert(QStringLiteral("background"),
                    canvasBackgroundIsTransparent
                        ? QStringLiteral("none")
                        : canvasBackgroundColor.name(QColor::HexArgb));
    settings.insert(QStringLiteral("highlightColor"),
                    imageHighlightColor.name(QColor::HexArgb));
    settings.insert(QStringLiteral("highlightIndices"),
                    intsToVariant(imageHighlightIndices));
    settings.insert(QStringLiteral("localEnabled"), pa_TF);
    settings.insert(QStringLiteral("localImageCount"), pa_num);
    settings.insert(QStringLiteral("localAutoIncrement"), pa_auto_increment_TF);
    settings.insert(QStringLiteral("localIncrement"), pa_increment);
    settings.insert(QStringLiteral("localIncrementCount"), pa_increment_count);
    settings.insert(QStringLiteral("localRadius"), pa_radi);
    settings.insert(QStringLiteral("localIterations"), pa_opti);
    settings.insert(QStringLiteral("localLoops"), pa_itr);
    settings.insert(QStringLiteral("globalEnabled"), all_TF);
    settings.insert(QStringLiteral("globalRadius"), all_radi);
    settings.insert(QStringLiteral("globalIterations"), all_opti);
    settings.insert(QStringLiteral("globalLoops"), all_itr);
    settings.insert(QStringLiteral("leastRegression"),
                    leastSquaresSettings.regressionThreshold);
    settings.insert(QStringLiteral("leastRelative"),
                    leastSquaresSettings.relativeThreshold);
    settings.insert(QStringLiteral("leastAbsolute"),
                    leastSquaresSettings.absoluteThreshold);
    settings.insert(QStringLiteral("leastPairError"),
                    leastSquaresSettings.maxPairErrorForRelative);
    settings.insert(QStringLiteral("mergeMode"),
                    static_cast<int>(imageMergeSettings.mode));
    project.insert(QStringLiteral("settings"), settings);

    QVariantList history;
    history.reserve(canvasHistoryTree.size());
    for (const CanvasHistoryNode& node : canvasHistoryTree) {
        QVariantMap entry;
        entry.insert(QStringLiteral("positions"), pointsToVariant(node.positions));
        entry.insert(QStringLiteral("parent"), node.parent);
        entry.insert(QStringLiteral("children"), intsToVariant(node.children));
        entry.insert(QStringLiteral("activeChild"), node.activeChild);
        entry.insert(QStringLiteral("sequence"), node.sequence);
        entry.insert(QStringLiteral("markers"), node.markers);
        history.push_back(entry);
    }
    project.insert(QStringLiteral("history"), history);
    project.insert(QStringLiteral("currentHistoryNode"), currentCanvasHistoryNode);
    project.insert(QStringLiteral("nextHistorySequence"), nextCanvasHistorySequence);

    QVariantMap calculation;
    calculation.insert(QStringLiteral("poss"), pointsToVariant(poss));
    calculation.insert(QStringLiteral("posAll"), pointsToVariant(pos_all));
    calculation.insert(QStringLiteral("idouDir"), intsToVariant(idou_dir));
    calculation.insert(QStringLiteral("checkTF"), boolsToVariant(checkTF));
    calculation.insert(QStringLiteral("checkTFCalc"), boolsToVariant(checkTF_calc));
    QVector<int> i2idVector;
    i2idVector.reserve(static_cast<int>(i2id.size()));
    for (int value : i2id) i2idVector.push_back(value);
    calculation.insert(QStringLiteral("i2id"), intsToVariant(i2idVector));
    calculation.insert(QStringLiteral("results"), ifftResultsToVariant(calc_results));
    calculation.insert(QStringLiteral("rerunResults"),
                       ifftResultsToVariant(calc_results_re));
    QVariantList leastDetails;
    leastDetails.reserve(static_cast<int>(leastSquaresDetails.size()));
    for (const LeastSquaresPairDetail& detail : leastSquaresDetails) {
        leastDetails.push_back(leastDetailToVariant(detail));
    }
    calculation.insert(QStringLiteral("leastDetails"), leastDetails);
    QVariantList trwsDetails;
    if (m_detailDialog) {
        const std::vector<one_line> detailLines = m_detailDialog->data();
        trwsDetails.reserve(static_cast<int>(detailLines.size()));
        for (const one_line& line : detailLines) {
            QVariantMap entry;
            entry.insert(QStringLiteral("partial"), line.abst.PaAll);
            entry.insert(QStringLiteral("start"), line.abst.start);
            entry.insert(QStringLiteral("end"), line.abst.end);
            entry.insert(QStringLiteral("iterations"), line.abst.itr);
            entry.insert(QStringLiteral("loops"), line.abst.loop);
            entry.insert(QStringLiteral("converged"), line.abst.shuusoku);
            entry.insert(QStringLiteral("lowSsimCount"), line.abst.lowSSIM_num);
            entry.insert(QStringLiteral("minSsim"), line.abst.minSSIM);
            entry.insert(QStringLiteral("energy"), line.abst.energy);
            entry.insert(QStringLiteral("ssim"), doublesToVariant(line.detail.ssim));
            QVector<int> edge1;
            QVector<int> edge2;
            edge1.reserve(static_cast<int>(line.detail.edge1.size()));
            edge2.reserve(static_cast<int>(line.detail.edge2.size()));
            for (int value : line.detail.edge1) edge1.push_back(value);
            for (int value : line.detail.edge2) edge2.push_back(value);
            entry.insert(QStringLiteral("edge1"), intsToVariant(edge1));
            entry.insert(QStringLiteral("edge2"), intsToVariant(edge2));
            trwsDetails.push_back(entry);
        }
    }
    calculation.insert(QStringLiteral("trwsDetails"), trwsDetails);
    calculation.insert(QStringLiteral("calc1Finished"), calc1_finished_state);
    calculation.insert(QStringLiteral("calc2Finished"), calc2_finished_state);
    calculation.insert(QStringLiteral("good"), ryoukou);
    calculation.insert(QStringLiteral("label4"), ui->label_4->text());
    calculation.insert(QStringLiteral("label5"), ui->label_5->text());
    calculation.insert(QStringLiteral("label6"), ui->label_6->text());
    calculation.insert(QStringLiteral("label12"), ui->label_12->text());
    project.insert(QStringLiteral("calculation"), calculation);
    project.insert(QStringLiteral("createdImage"), encodeMatAsPng(output_img));
    project.insert(QStringLiteral("outputFile"), output_file);
    const QPointF viewCenter = ui->graphicsView->mapToScene(
        ui->graphicsView->viewport()->rect().center());
    project.insert(QStringLiteral("viewScale"),
                   ui->graphicsView->transform().m11());
    project.insert(QStringLiteral("viewCenterX"), viewCenter.x());
    project.insert(QStringLiteral("viewCenterY"), viewCenter.y());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        showNonModalMessage(QMessageBox::Warning, tr("プロジェクト保存"),
                            tr("プロジェクトファイルを開けませんでした。\n%1")
                                .arg(file.errorString()));
        return false;
    }
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_12);
    stream << kProjectMagic << kProjectSchemaVersion << project;
    if (stream.status() != QDataStream::Ok || !file.commit()) {
        showNonModalMessage(QMessageBox::Warning, tr("プロジェクト保存"),
                            tr("プロジェクトファイルの保存に失敗しました。"));
        return false;
    }

    linkedProjectFilePath = path;
    projectImageData = savedImageData;
    savedProjectHistorySequence = maximumCanvasHistorySequence();
    updateWindowTitle();
    if (statusMessageLabel) {
        statusMessageLabel->setText(
            tr("プロジェクトを保存しました: %1")
                .arg(QDir::toNativeSeparators(path)));
    }
    return true;
}

void MainWindow::requestProjectOpen(const QString& path)
{
    if (path.trimmed().isEmpty()) return;
    if (isAlignmentOrOptimizationRunning()) {
        showNonModalMessage(QMessageBox::Information, tr("プロジェクトを開く"),
                            tr("計算中はプロジェクトを開けません。"));
        return;
    }
    if (hasExistingProjectData()) {
        showProjectReplacementPrompt(path);
    } else {
        loadProjectFromPath(path);
    }
}

void MainWindow::showProjectReplacementPrompt(const QString& path)
{
    auto* box = new QMessageBox(QMessageBox::Question,
                                tr("プロジェクトを開く"),
                                tr("既存のデータを上書きしますか？"),
                                QMessageBox::NoButton, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setModal(false);
    box->setWindowModality(Qt::NonModal);
    QPushButton* saveButton = box->addButton(
        tr("既存のデータを保存"), QMessageBox::AcceptRole);
    QPushButton* discardButton = box->addButton(
        tr("既存のデータを破棄"), QMessageBox::DestructiveRole);
    QPushButton* cancelButton = box->addButton(
        tr("キャンセル"), QMessageBox::RejectRole);
    connect(box, &QMessageBox::buttonClicked, this,
            [this, box, path, saveButton, discardButton, cancelButton](
                QAbstractButton* clicked) {
        if (clicked == saveButton) {
            saveProjectAs([this, path](bool saved) {
                if (saved) loadProjectFromPath(path);
            });
        } else if (clicked == discardButton) {
            loadProjectFromPath(path);
        } else if (clicked == cancelButton) {
            // Explicitly keep the current project.
        }
        box->close();
    });
    box->show();
}

bool MainWindow::loadProjectFromPath(const QString& requestedPath)
{
    const QString path = QFileInfo(requestedPath).absoluteFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showNonModalMessage(QMessageBox::Warning, tr("プロジェクトを開く"),
                            tr("プロジェクトファイルを開けませんでした。\n%1")
                                .arg(file.errorString()));
        return false;
    }

    quint32 magic = 0;
    quint16 schemaVersion = 0;
    QVariantMap project;
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_12);
    stream >> magic >> schemaVersion >> project;
    if (stream.status() != QDataStream::Ok || magic != kProjectMagic
        || schemaVersion == 0 || schemaVersion > kProjectSchemaVersion) {
        showNonModalMessage(QMessageBox::Warning, tr("プロジェクトを開く"),
                            tr("対応していない、または破損したプロジェクトファイルです。"));
        return false;
    }

    const QVariantList imageEntries = project.value(QStringLiteral("images")).toList();
    QStringList paths;
    QVector<QByteArray> imageData;
    QVector<QPoint> positions;
    QVector<int> opacities;
    QVector<bool> selections;
    paths.reserve(imageEntries.size());
    imageData.reserve(imageEntries.size());
    positions.reserve(imageEntries.size());
    opacities.reserve(imageEntries.size());
    selections.reserve(imageEntries.size());
    for (int i = 0; i < imageEntries.size(); ++i) {
        const QVariantMap entry = imageEntries[i].toMap();
        const QByteArray data = entry.value(QStringLiteral("data")).toByteArray();
        QPixmap validation;
        validation.loadFromData(data);
        if (data.isEmpty() || validation.isNull()) {
            showNonModalMessage(
                QMessageBox::Warning, tr("プロジェクトを開く"),
                tr("プロジェクト内の%1枚目の画像を復元できません。")
                    .arg(i + 1));
            return false;
        }
        QString originalPath = entry.value(QStringLiteral("path")).toString();
        if (originalPath.isEmpty()) {
            originalPath = tr("プロジェクト画像_%1.png").arg(i + 1);
        }
        paths.push_back(originalPath);
        imageData.push_back(data);
        positions.push_back(pointsFromVariant(
            QVariantList{entry.value(QStringLiteral("position"))}).value(0));
        opacities.push_back(std::clamp(
            entry.value(QStringLiteral("opacity"), 0).toInt(), 0, 100));
        selections.push_back(entry.value(QStringLiteral("selected")).toBool());
    }

    const QVariantMap settings = project.value(QStringLiteral("settings")).toMap();
    if (fileInputDialog) {
        fileInputDialog->close();
    }
    const bool previousSuppression = suppressSettingsPersistence;
    suppressSettingsPersistence = true;
    const ApplicationTheme projectTheme = static_cast<ApplicationTheme>(std::clamp(
        settings.value(QStringLiteral("applicationTheme"),
                       static_cast<int>(AppSettings::theme())).toInt(), 0, 2));
    const ApplicationLanguage projectLanguage = static_cast<ApplicationLanguage>(std::clamp(
        settings.value(QStringLiteral("applicationLanguage"),
                       static_cast<int>(AppSettings::language())).toInt(), 0, 2));
    AppSettings::setTheme(projectTheme);
    AppSettings::setLanguage(projectLanguage);
    AppSettings::setVulkanEnabled(
        settings.value(QStringLiteral("vulkanEnabled"),
                       AppSettings::vulkanOptions().enabled).toBool());
    AppSettings::setIgnoreVramLimit(
        settings.value(QStringLiteral("ignoreVramLimit"),
                       AppSettings::vulkanOptions().ignoreVramLimit).toBool());
    AppSettings::setVulkanDeviceKey(
        settings.value(QStringLiteral("vulkanDeviceKey"),
                       AppSettings::vulkanOptions().deviceKey).toString());
    AppSettings::setConfirmProjectSaveOnClose(
        settings.value(QStringLiteral("confirmProjectSaveOnClose"),
                       AppSettings::confirmProjectSaveOnClose()).toBool());
    defaultImageOpacity = std::clamp(
        settings.value(QStringLiteral("defaultOpacity"), defaultImageOpacity).toInt(),
        0, 100);
    File_input(input_files, paths, imageData);

    set_over_value(
        std::clamp(settings.value(QStringLiteral("horizontalOverlap"), 25).toInt(), 1, 100),
        std::clamp(settings.value(QStringLiteral("verticalOverlap"), 25).toInt(), 1, 100),
        std::clamp(settings.value(QStringLiteral("searchRange"), 15).toInt(), 0, 100));
    set_array_value(
        std::clamp(settings.value(QStringLiteral("direction"), 8).toInt(), 1, 8),
        std::max(0, settings.value(QStringLiteral("horizontalCount"), 0).toInt()),
        std::max(0, settings.value(QStringLiteral("verticalCount"), 0).toInt()));
    set_zigzag_value(settings.value(QStringLiteral("zigzag"), true).toBool() ? 1 : 0);

    pa_TF = settings.value(QStringLiteral("localEnabled"), false).toBool();
    pa_num = std::clamp(settings.value(QStringLiteral("localImageCount"), 6).toInt(), 4, 100);
    pa_auto_increment_TF =
        settings.value(QStringLiteral("localAutoIncrement"), false).toBool();
    pa_increment = std::clamp(settings.value(QStringLiteral("localIncrement"), 0).toInt(), 0, 100);
    pa_increment_count =
        std::clamp(settings.value(QStringLiteral("localIncrementCount"), 1).toInt(), 1, 100);
    pa_radi = std::clamp(settings.value(QStringLiteral("localRadius"), 2).toInt(), 1, 10);
    pa_opti = std::clamp(settings.value(QStringLiteral("localIterations"), 5000).toInt(), 20, 100000);
    pa_itr = std::clamp(settings.value(QStringLiteral("localLoops"), 4).toInt(), 1, 20);
    all_TF = settings.value(QStringLiteral("globalEnabled"), true).toBool();
    all_radi = std::clamp(settings.value(QStringLiteral("globalRadius"), 2).toInt(), 1, 10);
    all_opti = std::clamp(settings.value(QStringLiteral("globalIterations"), 10000).toInt(), 20, 100000);
    all_itr = std::clamp(settings.value(QStringLiteral("globalLoops"), 10).toInt(), 1, 50);
    leastSquaresSettings.regressionThreshold =
        std::clamp(settings.value(QStringLiteral("leastRegression"), 0.3).toDouble(), 0.0, 1.0);
    leastSquaresSettings.relativeThreshold =
        std::clamp(settings.value(QStringLiteral("leastRelative"), 2.5).toDouble(), 0.0, 100.0);
    leastSquaresSettings.absoluteThreshold =
        std::clamp(settings.value(QStringLiteral("leastAbsolute"), 3.5).toDouble(), 0.0, 1000.0);
    leastSquaresSettings.maxPairErrorForRelative =
        std::clamp(settings.value(QStringLiteral("leastPairError"), 0.95).toDouble(), 0.0, 1000.0);
    imageMergeSettings.mode = static_cast<ImageMergeMode>(std::clamp(
        settings.value(QStringLiteral("mergeMode"), 1).toInt(), 0, 3));
    applyCanvasBackgroundSetting(
        settings.value(QStringLiteral("background"), QStringLiteral("black")).toString());
    imageHighlightColor = QColor(
        settings.value(QStringLiteral("highlightColor"),
                       QStringLiteral("#ff4040")).toString());
    if (!imageHighlightColor.isValid()) imageHighlightColor = QColor(255, 64, 64);
    imageHighlightIndices =
        intsFromVariant(settings.value(QStringLiteral("highlightIndices")));

    if (positions.size() == items.size()) {
        applyCanvasPositions(positions);
    }
    toumeido = opacities;
    {
        const QSignalBlocker opacitySliderBlocker(ui->sliderOpacity1);
        const QSignalBlocker opacitySpinBlocker(ui->spinOpacity1);
        ui->sliderOpacity1->setValue(defaultImageOpacity);
        ui->spinOpacity1->setValue(defaultImageOpacity);
        for (int i = 0; i < items.size(); ++i) {
            if (!items[i]) continue;
            const int opacity = i < toumeido.size()
                                    ? toumeido[i] : defaultImageOpacity;
            items[i]->setOpacity((100 - opacity) / 100.0);
            items[i]->setSelected(i < selections.size() && selections[i]);
        }
    }
    ui->checkBox_2->setChecked(
        settings.value(QStringLiteral("useCanvasSource"), false).toBool());
    ui->checkBox->setChecked(
        settings.value(QStringLiteral("layoutLocked"), false).toBool());
    posi_lock(ui->checkBox->isChecked());

    canvasHistoryTree.clear();
    const QVariantList history = project.value(QStringLiteral("history")).toList();
    canvasHistoryTree.reserve(history.size());
    bool validHistory = true;
    for (const QVariant& item : history) {
        const QVariantMap entry = item.toMap();
        CanvasHistoryNode node;
        node.positions = pointsFromVariant(entry.value(QStringLiteral("positions")));
        node.parent = entry.value(QStringLiteral("parent"), -1).toInt();
        node.children = intsFromVariant(entry.value(QStringLiteral("children")));
        node.activeChild = entry.value(QStringLiteral("activeChild"), -1).toInt();
        node.sequence = entry.value(QStringLiteral("sequence"), 0).toInt();
        node.markers = entry.value(QStringLiteral("markers"), 0).toInt();
        if (node.positions.size() != items.size()) validHistory = false;
        canvasHistoryTree.push_back(node);
    }
    for (int i = 0; i < canvasHistoryTree.size() && validHistory; ++i) {
        const CanvasHistoryNode& node = canvasHistoryTree[i];
        if (node.parent < -1 || node.parent >= canvasHistoryTree.size()
            || node.activeChild < -1
            || node.activeChild >= canvasHistoryTree.size()
            || node.sequence <= 0) {
            validHistory = false;
            break;
        }
        for (int child : node.children) {
            if (child < 0 || child >= canvasHistoryTree.size()) {
                validHistory = false;
                break;
            }
        }
    }
    if (!validHistory) canvasHistoryTree.clear();
    currentCanvasHistoryNode = project.value(
        QStringLiteral("currentHistoryNode"), -1).toInt();
    if (currentCanvasHistoryNode < 0
        || currentCanvasHistoryNode >= canvasHistoryTree.size()) {
        currentCanvasHistoryNode = canvasHistoryTree.isEmpty()
                                       ? -1 : canvasHistoryTree.size() - 1;
    }
    nextCanvasHistorySequence = std::max(
        maximumCanvasHistorySequence() + 1,
        project.value(QStringLiteral("nextHistorySequence"), 1).toInt());
    if (canvasHistoryTree.isEmpty() && !items.isEmpty()) {
        CanvasHistoryNode node;
        node.positions = readScenePositions();
        node.sequence = 1;
        canvasHistoryTree.push_back(node);
        currentCanvasHistoryNode = 0;
        nextCanvasHistorySequence = 2;
    }

    const QVariantMap calculation =
        project.value(QStringLiteral("calculation")).toMap();
    poss = pointsFromVariant(calculation.value(QStringLiteral("poss")));
    pos_all = pointsFromVariant(calculation.value(QStringLiteral("posAll")));
    if (poss.size() != items.size()) poss = readScenePositions();
    if (pos_all.size() != items.size()) pos_all = poss;
    idou_dir = intsFromVariant(calculation.value(QStringLiteral("idouDir")));
    checkTF = boolsFromVariant(calculation.value(QStringLiteral("checkTF")));
    checkTF_calc = boolsFromVariant(
        calculation.value(QStringLiteral("checkTFCalc")));
    const QVector<int> restoredI2id =
        intsFromVariant(calculation.value(QStringLiteral("i2id")));
    i2id.assign(restoredI2id.begin(), restoredI2id.end());
    calc_results = ifftResultsFromVariant(
        calculation.value(QStringLiteral("results")));
    calc_results_re = ifftResultsFromVariant(
        calculation.value(QStringLiteral("rerunResults")));
    leastSquaresDetails.clear();
    for (const QVariant& detail :
         calculation.value(QStringLiteral("leastDetails")).toList()) {
        leastSquaresDetails.push_back(leastDetailFromVariant(detail));
    }
    std::vector<one_line> restoredTrwsDetails;
    const QVariantList trwsDetails =
        calculation.value(QStringLiteral("trwsDetails")).toList();
    restoredTrwsDetails.reserve(static_cast<size_t>(trwsDetails.size()));
    for (const QVariant& detailValue : trwsDetails) {
        const QVariantMap entry = detailValue.toMap();
        one_line line{};
        line.abst.PaAll = entry.value(QStringLiteral("partial")).toBool();
        line.abst.start = entry.value(QStringLiteral("start")).toInt();
        line.abst.end = entry.value(QStringLiteral("end")).toInt();
        line.abst.itr = entry.value(QStringLiteral("iterations")).toInt();
        line.abst.loop = entry.value(QStringLiteral("loops")).toInt();
        line.abst.shuusoku = entry.value(QStringLiteral("converged")).toBool();
        line.abst.lowSSIM_num =
            entry.value(QStringLiteral("lowSsimCount")).toInt();
        line.abst.minSSIM = entry.value(QStringLiteral("minSsim")).toDouble();
        line.abst.energy = entry.value(QStringLiteral("energy")).toDouble();
        line.detail.ssim = doublesFromVariant(entry.value(QStringLiteral("ssim")));
        const QVector<int> edge1 =
            intsFromVariant(entry.value(QStringLiteral("edge1")));
        const QVector<int> edge2 =
            intsFromVariant(entry.value(QStringLiteral("edge2")));
        line.detail.edge1.assign(edge1.begin(), edge1.end());
        line.detail.edge2.assign(edge2.begin(), edge2.end());
        if (line.detail.ssim.size() == line.detail.edge1.size()
            && line.detail.ssim.size() == line.detail.edge2.size()) {
            restoredTrwsDetails.push_back(std::move(line));
        }
    }
    if (m_detailDialog) {
        m_detailDialog->setAllData(restoredTrwsDetails);
    }
    calc1_finished_state =
        calculation.value(QStringLiteral("calc1Finished"), false).toBool();
    calc2_finished_state =
        calculation.value(QStringLiteral("calc2Finished"), false).toBool();
    ryoukou = calculation.value(QStringLiteral("good"), false).toBool();
    ui->label_4->setText(calculation.value(QStringLiteral("label4")).toString());
    ui->label_5->setText(calculation.value(QStringLiteral("label5")).toString());
    ui->label_6->setText(calculation.value(QStringLiteral("label6")).toString());
    ui->label_12->setText(calculation.value(QStringLiteral("label12")).toString());

    output_img = decodeEmbeddedImage(
        project.value(QStringLiteral("createdImage")).toByteArray());
    output_file = project.value(QStringLiteral("outputFile")).toString();
    const double viewScale = std::clamp(
        project.value(QStringLiteral("viewScale"), 1.0).toDouble(),
        0.01, 100.0);
    ui->graphicsView->setTransform(QTransform::fromScale(viewScale, viewScale));
    ui->graphicsView->centerOn(
        project.value(QStringLiteral("viewCenterX"), 0.0).toDouble(),
        project.value(QStringLiteral("viewCenterY"), 0.0).toDouble());
    updateZoomLabel();
    res_all.resize(items.size());
    imgs.resize(static_cast<size_t>(items.size()));
    for (int i = 0; i < items.size(); ++i) {
        res_all[i] = items[i] ? items[i]->pixmap().size() : QSize();
        if (items[i]) {
            imgs[static_cast<size_t>(i)] = ImageUtils::qimage_to_mat_bgra(
                items[i]->pixmap().toImage());
        }
    }

    ui->pushButton_Calc1->setEnabled(!items.isEmpty());
    ui->pushButton_2->setEnabled(calc1_finished_state);
    ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
    ui->pushButton_8->setEnabled(calc1_finished_state && items.size() > 1);
    ui->pushButton_4->setEnabled(!output_img.empty());
    ui->pushButton->setEnabled(!calc_results.isEmpty());
    ui->pushButton_5->setEnabled(calc2_finished_state
                                 && !restoredTrwsDetails.empty());
    ui->pushButton_9->setEnabled(!leastSquaresDetails.empty());
    if (optimizationSettingsDialog) {
        optimizationSettingsDialog->setValues(
            pa_num, pa_radi, pa_opti, pa_itr,
            all_radi, all_opti, all_itr,
            pa_TF, all_TF, pa_auto_increment_TF,
            pa_increment, pa_increment_count);
    }
    if (leastSquaresSettingsDialog) {
        leastSquaresSettingsDialog->setValues(
            leastSquaresSettings.regressionThreshold,
            leastSquaresSettings.relativeThreshold,
            leastSquaresSettings.absoluteThreshold,
            leastSquaresSettings.maxPairErrorForRelative);
    }
    if (mergeSettingsDialog) {
        mergeSettingsDialog->setMode(static_cast<int>(imageMergeSettings.mode));
    }
    if (canvasBackgroundDialog && canvasNoColorCheck
        && canvasBackgroundColorPicker) {
        canvasNoColorCheck->setChecked(canvasBackgroundIsTransparent);
        canvasBackgroundColorPicker->setCurrentColor(canvasBackgroundColor);
        canvasBackgroundColorPicker->setEnabled(!canvasBackgroundIsTransparent);
    }

    suppressSettingsPersistence = previousSuppression;
    AlignmentDefaultSettings restoredAlignment;
    restoredAlignment.horizontalOverlapPercent = ui->spinBox_2->value();
    restoredAlignment.verticalOverlapPercent = ui->spinBox_3->value();
    restoredAlignment.searchRangePercent = ui->spinBox->value();
    AppSettings::setAlignmentOptions(restoredAlignment);
    ArrangementDefaultSettings restoredArrangement;
    restoredArrangement.direction = ui->cornerSelector->getStatus();
    restoredArrangement.horizontalImageCount = configuredHorizontalImageCount;
    restoredArrangement.verticalImageCount = configuredVerticalImageCount;
    restoredArrangement.zigzag = ui->cornerSelector->zigzagChecked();
    AppSettings::setArrangementOptions(restoredArrangement);
    TrwsPamiDefaultSettings restoredTrws;
    restoredTrws.localEnabled = pa_TF;
    restoredTrws.localImageCount = pa_num;
    restoredTrws.localAutoIncrement = pa_auto_increment_TF;
    restoredTrws.localImageCountIncrement = pa_increment;
    restoredTrws.localIncrementCount = pa_increment_count;
    restoredTrws.localSearchRadius = pa_radi;
    restoredTrws.localMaxIterations = pa_opti;
    restoredTrws.localMaxLoops = pa_itr;
    restoredTrws.globalEnabled = all_TF;
    restoredTrws.globalSearchRadius = all_radi;
    restoredTrws.globalMaxIterations = all_opti;
    restoredTrws.globalMaxLoops = all_itr;
    AppSettings::setTrwsPamiOptions(restoredTrws);
    LeastSquaresDefaultSettings restoredLeastSquares;
    restoredLeastSquares.regressionThreshold =
        leastSquaresSettings.regressionThreshold;
    restoredLeastSquares.relativeThreshold = leastSquaresSettings.relativeThreshold;
    restoredLeastSquares.absoluteThreshold = leastSquaresSettings.absoluteThreshold;
    restoredLeastSquares.maxPairErrorForRelative =
        leastSquaresSettings.maxPairErrorForRelative;
    AppSettings::setLeastSquaresOptions(restoredLeastSquares);
    ImageMergeDefaultSettings restoredMerge;
    restoredMerge.mode = static_cast<int>(imageMergeSettings.mode);
    AppSettings::setImageMergeOptions(restoredMerge);
    AppSettings::setCanvasBackground(
        canvasBackgroundIsTransparent
            ? QStringLiteral("none")
            : canvasBackgroundColor.name(QColor::HexArgb));
    applyApplicationTheme(projectTheme);
    applyApplicationLanguage(projectLanguage);
    if (applicationSettingsDialog) {
        applicationSettingsDialog->reloadFromSettings();
    }
    rebuildImageHighlights();
    refreshCanvasHistoryDialog();
    linkedProjectFilePath = path;
    savedProjectHistorySequence = maximumCanvasHistorySequence();
    updateWindowTitle();
    if (statusMessageLabel) {
        statusMessageLabel->setText(
            tr("プロジェクトを開きました: %1")
                .arg(QDir::toNativeSeparators(path)));
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (allowCloseWithoutProjectPrompt
        || !AppSettings::confirmProjectSaveOnClose()
        || !hasUnsavedProjectHistory()) {
        event->accept();
        return;
    }

    event->ignore();
    if (projectClosePromptOpen) return;
    projectClosePromptOpen = true;

    auto* box = new QMessageBox(QMessageBox::Question,
                                tr("プロジェクト保存"),
                                tr("保存されていない変更履歴があります。プロジェクトを保存しますか？"),
                                QMessageBox::NoButton, this);
    box->setAttribute(Qt::WA_DeleteOnClose);
    box->setModal(false);
    box->setWindowModality(Qt::NonModal);
    QPushButton* saveButton = box->addButton(tr("プロジェクト保存"),
                                              QMessageBox::AcceptRole);
    QPushButton* discardButton = box->addButton(tr("保存せず終了"),
                                                 QMessageBox::DestructiveRole);
    QPushButton* cancelButton = box->addButton(tr("キャンセル"),
                                                QMessageBox::RejectRole);
    connect(box, &QDialog::finished, this, [this]() {
        projectClosePromptOpen = false;
    });
    connect(box, &QMessageBox::buttonClicked, this,
            [this, box, saveButton, discardButton, cancelButton](
                QAbstractButton* clicked) {
        projectClosePromptOpen = false;
        if (clicked == saveButton) {
            if (!linkedProjectFilePath.isEmpty()) {
                if (saveProjectToPath(linkedProjectFilePath)) {
                    allowCloseWithoutProjectPrompt = true;
                    close();
                }
            } else {
                saveProjectAs([this](bool saved) {
                    if (saved) {
                        allowCloseWithoutProjectPrompt = true;
                        close();
                    }
                });
            }
        } else if (clicked == discardButton) {
            allowCloseWithoutProjectPrompt = true;
            close();
        } else if (clicked == cancelButton) {
            // Keep the window open.
        }
        box->close();
    });
    box->show();
}

void MainWindow::startDelayedVulkanDetection()
{
    if (vulkanScanWatcher->isRunning()) {
        return;
    }
    vulkanDetectionInProgress = true;
    if (applicationSettingsDialog) {
        applicationSettingsDialog->setVulkanDetectionInProgress();
    }
    vulkanScanWatcher->setFuture(QtConcurrent::run([]() {
        return VulkanSsimEngine::detectDevices();
    }));
}

static int viewZoomPercent(const QGraphicsView *view)
{
    const double s = view->transform().m11();   // x方向スケール（等方ならこれでOK）
    return int(std::round(s * 100.0));
}

// CLI由来のファイルパスをチェックする
void MainWindow::File_input_UI()
{
    ui->pushButton_1->setEnabled(false);
    ui->pushButton_1->setText(tr("ファイル読み込み中..."));
    ui->pushButton_1->repaint();
    QApplication::processEvents();
}

// CLI由来のファイルパスをチェックする
void MainWindow::File_input_check(QStringList cli_paths)
{
    // 再帰的展開
    QStringList result;
    QStringList unreadableFiles;
    QSet<QString> seen;

    for (const QString& path : cli_paths) {
        QFileInfo info(path);

        if (!info.exists()) {
            unreadableFiles.push_back(path);
            continue;
        }

        if (info.isFile()) {
            QString filePath = info.absoluteFilePath();
            if (!seen.contains(filePath)) {
                result << filePath;
                seen.insert(filePath);
            }
        }
        else if (info.isDir()) {
            QDirIterator it(
                info.absoluteFilePath(),
                QDir::Files,                       // ファイルのみ取得
                QDirIterator::Subdirectories       // 再帰
                );

            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo fInfo(filePath);
                QString absPath = fInfo.absoluteFilePath();

                if (!seen.contains(absPath)) {
                    result << absPath;
                    seen.insert(absPath);
                }
            }
        }
    }
    result = onSortUpFname(result); // 順番を整理

    // 画像としてファイルを読み込めるか確認
    QStringList validFiles;
    validFiles.reserve(result.size());

    for (const QString &path : std::as_const(result)) {
        // 先に画像読み込み
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();

        // 読み取り不可ならスキップ（fiw_filesにも入れない）
        if (img.isNull()) {
            unreadableFiles.push_back(path);
            continue;
        }

        // 読めたファイルだけ保持
        validFiles.append(path);
    }

    QStringList file_null;
    File_input(file_null,validFiles);

    showUnsupportedFilesDialog(unreadableFiles, this);

    ui->pushButton_1->setEnabled(true);
    ui->pushButton_1->setText(tr("入力画像"));

    emit fileInputFinished(); // 完了通知
}

void MainWindow::File_input_dummy()
{
    emit fileInputFinished(); // 完了通知
};

QStringList MainWindow::onSortUpFname(QStringList input_files)
{
    QCollator collator;
    collator.setNumericMode(true);          // "2" < "10" を自然に
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    std::sort(input_files.begin(), input_files.end(),
              [&collator](const QString &a, const QString &b) {
                  const QFileInfo ia(a);
                  const QFileInfo ib(b);

                  // 第1キー: 親パス
                  const QString dirA = ia.path();       // 例: C:/xxx/yyy
                  const QString dirB = ib.path();
                  int cmp = collator.compare(dirA, dirB);
                  if (cmp != 0) return cmp < 0;

                  // 第2キー: ファイル名（拡張子なし）
                  // completeBaseName() は "a.tar.gz" -> "a.tar"
                  const QString baseA = ia.completeBaseName();
                  const QString baseB = ib.completeBaseName();
                  cmp = collator.compare(baseA, baseB);
                  if (cmp != 0) return cmp < 0;

                  // 第3キー: 拡張子
                  const QString extA = ia.suffix();
                  const QString extB = ib.suffix();
                  cmp = collator.compare(extA, extB);
                  if (cmp != 0) return cmp < 0;

                  // 同値時の最終タイブレーク（安定化のためフルパス）
                  return collator.compare(a, b) < 0;
              });
    return input_files;
};

void MainWindow::handleCanvasFilesDropped(const QStringList& paths)
{
    if (isAlignmentOrOptimizationRunning()) {
        if (statusMessageLabel) {
            statusMessageLabel->setText(
                tr("計算中は画像ファイルを追加できません。"));
        }
        return;
    }

    QStringList droppedProjectFiles;
    for (const QString& path : paths) {
        const QFileInfo info(path);
        if (info.isFile()
            && info.suffix().compare(QStringLiteral("isauto"),
                                     Qt::CaseInsensitive) == 0) {
            droppedProjectFiles.push_back(info.absoluteFilePath());
        }
    }
    if (!droppedProjectFiles.isEmpty()) {
        if (paths.size() != 1 || droppedProjectFiles.size() != 1) {
            showNonModalMessage(
                QMessageBox::Warning, tr("プロジェクトを開く"),
                tr("プロジェクトを開くには、1つの.isautoファイルだけをドロップしてください。"));
            return;
        }
        requestProjectOpen(droppedProjectFiles.first());
        return;
    }

    QStringList expandedFiles;
    QStringList unreadableFiles;
    QSet<QString> seen;
    for (const QString& path : paths) {
        const QFileInfo info(path);
        if (!info.exists()) {
            unreadableFiles.push_back(path);
            continue;
        }
        if (info.isFile()) {
            const QString absolutePath = info.absoluteFilePath();
            if (!seen.contains(absolutePath)) {
                seen.insert(absolutePath);
                expandedFiles.push_back(absolutePath);
            }
            continue;
        }
        if (!info.isDir()) {
            continue;
        }

        QDirIterator iterator(info.absoluteFilePath(), QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString absolutePath =
                QFileInfo(iterator.next()).absoluteFilePath();
            if (!seen.contains(absolutePath)) {
                seen.insert(absolutePath);
                expandedFiles.push_back(absolutePath);
            }
        }
    }

    expandedFiles = onSortUpFname(expandedFiles);
    QStringList readableFiles;
    readableFiles.reserve(expandedFiles.size());
    for (const QString& path : std::as_const(expandedFiles)) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        if (!reader.read().isNull()) {
            readableFiles.push_back(path);
        } else {
            unreadableFiles.push_back(path);
        }
    }
    if (readableFiles.isEmpty()) {
        showUnsupportedFilesDialog(unreadableFiles, this);
        if (statusMessageLabel) {
            statusMessageLabel->setText(tr("読み込める画像ファイルがありません。"));
        }
        return;
    }

    QStringList updatedFiles = input_files;
    updatedFiles.append(readableFiles);
    File_input(input_files, updatedFiles);
    showUnsupportedFilesDialog(unreadableFiles, this);
    if (statusMessageLabel) {
        statusMessageLabel->setText(
            tr("%1 個の画像ファイルを追加しました。").arg(readableFiles.size()));
    }
}

// 画像同士の重なりの目安を設定
void MainWindow::set_over_value(int o_x, int o_y, int o_r)
{
    const bool previousSuppression = suppressSettingsPersistence;
    suppressSettingsPersistence = true;
    ui->horizontalSlider->setValue(o_x);
    ui->horizontalSlider_2->setValue(o_y);
    ui->horizontalSlider_3->setValue(o_r);
    suppressSettingsPersistence = previousSuppression;
}

// 画像の配列を設定
void MainWindow::set_array_value(int ar, int arh, int arv)
{
    const bool previousSuppression = suppressSettingsPersistence;
    suppressSettingsPersistence = true;
    configuredHorizontalImageCount = std::max(0, arh);
    configuredVerticalImageCount = std::max(0, arv);
    ui->cornerSelector->setUI(ar);

    const int n = input_files.size();
    if (ar == 1 || ar == 3 || ar == 5 || ar == 7) {
        if (arh > n) {
            arh = n;
        } else if (arh < 0) {
            arh = 0;
        }
        ui->cornerSelector->setCols(arh);
    } else {
        if (arv > n) {
            arv = n;
        } else if (arv < 0) {
            arv = 0;
        }
        ui->cornerSelector->setRows(arv);
    }
    suppressSettingsPersistence = previousSuppression;
}

// 折り返しを指定
void MainWindow::set_zigzag_value(int g)
{
    Q_UNUSED(g);
    // 一方向は未実装のため、現時点では常にジグザグを使用する。
    const bool previousSuppression = suppressSettingsPersistence;
    suppressSettingsPersistence = true;
    orikaeshi = true;
    ui->cornerSelector->setZigzagChecked(true);
    suppressSettingsPersistence = previousSuppression;
}

void MainWindow::set_canvas_state(bool layoutLocked, bool useCanvasAsSource)
{
    ui->checkBox_2->setChecked(useCanvasAsSource);
    ui->checkBox->setChecked(layoutLocked);
    posi_lock(layoutLocked);
}

// 最適化設定値
void MainWindow::set_opti_value(int i1,int i2,int i3,int i4,int i5,int i6,int i7,int i8,int i9)
{
    if (i1 == 1) {
        pa_TF = true;
    } else {
        pa_TF = false;
    }
    pa_num = i2;
    pa_radi = i3;
    pa_opti = i4;
    pa_itr = i5;
    if (i6 == 1) {
        all_TF = true;
    } else {
        all_TF = false;
    }
    all_radi = i7;
    all_opti = i8;
    all_itr = i9;
}

// 計算の実行
void MainWindow::run_manual(int cal_st) {
    if (cal_st == 2) {
        cal_opti = true;
    } else {
        cal_opti = false;
    }
    if (cal_st == 1 || cal_st == 2) {
        calc_finish_sig = true;
        calc_iFFT();
    }
}


// ファイル入力ウィザードでok押した時に実行
void MainWindow::File_input(const QStringList& paths_old,
                            const QStringList& paths_new,
                            const QVector<QByteArray>& embeddedImageData)
{
    clearImageHighlights();

    const int n_new = paths_new.size();
    const int n_old = paths_old.size();
    //qDebug() << "new size " << n_new;
    //qDebug() << "old size " << n_old;
    const int n_max = std::max(n_new, n_old);

    // 適用前に全画像を検証して、途中失敗による状態不整合を避ける
    const bool suppliedEmbeddedData = embeddedImageData.size() == n_new;
    QVector<QByteArray> imageDataNew(n_new);
    QVector<QPixmap> pix_new(n_new);
    QStringList unreadableFiles;
    for (int i = 0; i < n_new; ++i) {
        if (suppliedEmbeddedData) {
            imageDataNew[i] = embeddedImageData[i];
        } else {
            int oldImageIndex = -1;
            if (i < n_old && paths_old.value(i) == paths_new[i]) {
                oldImageIndex = i;
            } else {
                oldImageIndex = paths_old.indexOf(paths_new[i]);
            }
            if (oldImageIndex >= 0
                && oldImageIndex < projectImageData.size()) {
                imageDataNew[i] = projectImageData[oldImageIndex];
            }
        }

        QPixmap pix;
        if (!imageDataNew[i].isEmpty()) {
            pix.loadFromData(imageDataNew[i]);
        } else {
            pix.load(paths_new[i]);
        }
        if (pix.isNull()) {
            unreadableFiles.push_back(paths_new[i]);
        } else {
            pix_new[i] = pix;
        }
    }
    if (!unreadableFiles.isEmpty()) {
        showUnsupportedFilesDialog(unreadableFiles, this);
        return;
    }

    // items は「new の個数」に合わせる
    items.resize(n_new);

    for (int i = 0; i < n_max; ++i)
    {
        // (A) 新しい方が短い → 古いを削除
        if (i >= n_new) {
            auto it_old = itemById.find(i);
            if (it_old != itemById.end()) {
                QGraphicsPixmapItem* item_old = it_old.value();
                scene->removeItem(item_old);
                delete item_old;
                itemById.erase(it_old);
            }
            // items は n_new までしかないので触らない
            continue;
        }

        // (B) 古い方が短い → 新規追加
        if (i >= n_old) {
            const QPixmap& pix = pix_new[i];

            auto* it = scene->addPixmap(pix);
            items[i] = it;
            itemById.insert(i, it);
            /*
            it->setFlags(QGraphicsItem::ItemIsMovable |
                         QGraphicsItem::ItemIsSelectable |
                         QGraphicsItem::ItemIsFocusable);
            */
            it->setFlags(QGraphicsItem::ItemIsSelectable |
                         QGraphicsItem::ItemIsFocusable);
            it->setZValue(i + 1);

            continue;
        }

        // (C) 両方に存在 → 同じなら何もしない
        const QByteArray oldData = i < projectImageData.size()
                                       ? projectImageData[i]
                                       : QByteArray();
        if (paths_old[i] == paths_new[i] && oldData == imageDataNew[i]) {
            continue;
        }

        // (D) 両方に存在 → パスが違うので置換
        const QPixmap& pix = pix_new[i];

        // 古い item を取得（無い可能性もあるのでチェック）
        auto it_old = itemById.find(i);
        if (it_old != itemById.end()) {
            QGraphicsPixmapItem* item_old = it_old.value();
            scene->removeItem(item_old);
            delete item_old;
            itemById.erase(it_old);
        }

        // 新しい item を追加
        auto* it = scene->addPixmap(pix);
        items[i] = it;
        itemById.insert(i, it);
        /*
        it->setFlags(QGraphicsItem::ItemIsMovable |
                     QGraphicsItem::ItemIsSelectable |
                     QGraphicsItem::ItemIsFocusable);
        */
        it->setFlags(QGraphicsItem::ItemIsSelectable |
                     QGraphicsItem::ItemIsFocusable);


        it->setZValue(i + 1);
    }
    input_files = paths_new;
    projectImageData = imageDataNew;
    imgs.clear();
    const int requestedHorizontalImageCount = configuredHorizontalImageCount;
    const int requestedVerticalImageCount = configuredVerticalImageCount;
    const int requestedDirection = ui->cornerSelector->getStatus();
    ui->cornerSelector->setMax(input_files.size(),true);
    set_array_value(requestedDirection,
                    requestedHorizontalImageCount,
                    requestedVerticalImageCount);
    ui->checkBox_2->setChecked(AppSettings::defaults().canvas.useCanvasAsSource);
    toumeido.fill(defaultImageOpacity, input_files.size());
    const qreal defaultOpacity = (100 - defaultImageOpacity) / 100.0;
    for (QGraphicsPixmapItem* item : std::as_const(items)) {
        if (item) {
            item->setOpacity(defaultOpacity);
        }
    }
    if (input_files.isEmpty()) {
        ui->checkBox_2->setChecked(false);
    }
    ui->checkBox_2->setEnabled(true);
    calc1FinishedBeforeCalculation = calc1_finished_state;
    calc1_finished_state = false;
    ui->pushButton_8->setEnabled(false);
    ui->label_4->setText("");
    ui->pushButton_9->setEnabled(false);
    leastSquaresDetails.clear();
    if (m_detailDialog) m_detailDialog->clearData();
    calc_results.clear();
    calc_results_re.clear();
    checkTF.clear();
    checkTF_calc.clear();
    idou_dir.clear();
    i2id.clear();
    calc2_finished_state = false;
    ryoukou = false;
    ui->pushButton->setEnabled(false);
    ui->pushButton_5->setEnabled(false);
    applySceneMoveMode(!ui->checkBox->isChecked());
    invalidateCreatedImage();
    recordCanvasHistory();
};


// deleteで削除した場合
void MainWindow::deleteSelectedItems()
{
    if (ui->checkBox->isChecked()) return;
    const auto selected = scene->selectedItems();
    if (selected.isEmpty()) return;

    clearImageHighlights();

    for (QGraphicsItem *it : selected) {
        int idx = items.indexOf(static_cast<QGraphicsPixmapItem*>(it)); // itemsの型に合わせる
        if (idx >= 0) {
            input_files.removeAt(idx);
            if (idx < projectImageData.size()) {
                projectImageData.removeAt(idx);
            }
            items.removeAt(idx);
            itemById.remove(idx);
        }
        scene->removeItem(it);
        delete it;
    }
    itemById.clear();
    for (int i = 0; i < items.size(); ++i) {
        if (items[i]) itemById.insert(i, items[i]);
    }
    ui->cornerSelector->setMax(input_files.size(),true);
    photo_Arrange();
    if (input_files.isEmpty()) {
        ui->checkBox_2->setChecked(false);
    }
    ui->checkBox_2->setEnabled(true);
    calc1_finished_state = false;
    ui->pushButton_8->setEnabled(false);
    ui->label_4->setText("");
    ui->pushButton_9->setEnabled(false);
    leastSquaresDetails.clear();
    if (m_detailDialog) m_detailDialog->clearData();
    calc_results.clear();
    calc_results_re.clear();
    checkTF.clear();
    checkTF_calc.clear();
    idou_dir.clear();
    i2id.clear();
    calc2_finished_state = false;
    ryoukou = false;
    ui->pushButton->setEnabled(false);
    ui->pushButton_5->setEnabled(false);
    applySceneMoveMode(!ui->checkBox->isChecked());
    recordCanvasHistory();
}


void MainWindow::updateZoomLabel()
{
    const int pct = viewZoomPercent(ui->graphicsView);
    zoomLabel->setText(QString("%1%").arg(pct));
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == scene && event->type() == QEvent::GraphicsSceneMousePress) {
        auto* mouseEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            sceneMousePressPositions = readScenePositions();
            const int clickedIndex = imageIndexAtScenePos(mouseEvent->scenePos());
            if (clickedIndex >= 0) {
                if (mouseEvent->modifiers() & Qt::ShiftModifier) {
                    if (sceneSelectionAnchor < 0 || sceneSelectionAnchor >= items.size()) {
                        sceneSelectionAnchor = clickedIndex;
                    }
                    selectImageRange(sceneSelectionAnchor, clickedIndex);
                    mouseEvent->accept();
                    return true;
                }
                sceneSelectionAnchor = clickedIndex;
            }
        }
    } else if (watched == scene && event->type() == QEvent::GraphicsSceneMouseRelease) {
        const QVector<QPoint> releasedPositions = readScenePositions();
        if (!sceneMousePressPositions.isEmpty() &&
            !samePositions(sceneMousePressPositions, releasedPositions)) {
            ui->checkBox_2->setChecked(true);
            recordCanvasHistory();
        }
        sceneMousePressPositions.clear();
    } else if (watched == scene && event->type() == QEvent::GraphicsSceneMouseMove) {
        if (!imageHighlightIndices.isEmpty()) {
            QTimer::singleShot(0, this, [this]() {
                if (!imageHighlightIndices.isEmpty()) {
                    rebuildImageHighlights();
                }
            });
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::applySceneMoveMode(bool enabled)
{
    for (QGraphicsPixmapItem* item : std::as_const(items)) {
        if (!item) continue;
        item->setFlag(QGraphicsItem::ItemIsSelectable, true);
        item->setFlag(QGraphicsItem::ItemIsFocusable, true);
        item->setFlag(QGraphicsItem::ItemIsMovable, enabled);
    }

    if (!enabled) {
        sceneSelectionAnchor = -1;
    }
}

QVector<QPoint> MainWindow::readScenePositions() const
{
    QVector<QPoint> positions;
    positions.reserve(items.size());
    for (QGraphicsPixmapItem* item : items) {
        if (!item) {
            positions.push_back(QPoint(0, 0));
            continue;
        }
        const QPointF p = item->pos();
        positions.push_back(QPoint(static_cast<int>(std::round(p.x())),
                                   static_cast<int>(std::round(p.y()))));
    }
    return positions;
}

void MainWindow::syncPossFromScene()
{
    poss = readScenePositions();
    pos_all = poss;
}

void MainWindow::invalidateCreatedImage()
{
    output_img.release();
    ui->label_6->clear();
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_4->setText(tr("PNG エクスポート"));
}

void MainWindow::arrangeSettingsChanged()
{
    ui->checkBox_2->setChecked(false);
    calc1_finished_state = false;
    ui->pushButton_8->setEnabled(false);
    ui->label_4->setText("");
    ui->pushButton_9->setEnabled(false);
    leastSquaresDetails.clear();
    photo_Arrange();
}

bool MainWindow::samePositions(const QVector<QPoint>& a, const QVector<QPoint>& b) const
{
    if (a.size() != b.size()) return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

void MainWindow::applyCanvasPositions(const QVector<QPoint>& positions)
{
    if (positions.size() != items.size()) return;
    invalidateCreatedImage();
    restoringCanvasHistory = true;
    for (int i = 0; i < items.size(); ++i) {
        if (items[i]) {
            items[i]->setPos(positions[i]);
        }
    }
    poss = positions;
    pos_all = positions;
    restoringCanvasHistory = false;
    rebuildImageHighlights();
}

void MainWindow::recordCanvasHistory(int markers)
{
    if (restoringCanvasHistory) return;

    if (items.isEmpty()) {
        invalidateCreatedImage();
        canvasHistoryTree.clear();
        currentCanvasHistoryNode = -1;
        nextCanvasHistorySequence = std::max(
            nextCanvasHistorySequence, savedProjectHistorySequence + 1);
        poss.clear();
        pos_all.clear();
        clearImageHighlights();
        refreshCanvasHistoryDialog();
        updateWindowTitle();
        return;
    }

    const QVector<QPoint> positions = readScenePositions();
    if (positions.isEmpty()) return;

    const bool validCurrent = currentCanvasHistoryNode >= 0 &&
                              currentCanvasHistoryNode < canvasHistoryTree.size();
    if (validCurrent &&
        canvasHistoryTree[currentCanvasHistoryNode].positions.size() != positions.size()) {
        canvasHistoryTree.clear();
        currentCanvasHistoryNode = -1;
        nextCanvasHistorySequence = std::max(
            nextCanvasHistorySequence, savedProjectHistorySequence + 1);
    } else if (validCurrent &&
               samePositions(canvasHistoryTree[currentCanvasHistoryNode].positions, positions)) {
        if (markers == CanvasHistoryMarkerNone) {
            poss = positions;
            pos_all = positions;
            rebuildImageHighlights();
            return;
        }
    }

    invalidateCreatedImage();

    CanvasHistoryNode node;
    node.positions = positions;
    node.parent = (currentCanvasHistoryNode >= 0 &&
                   currentCanvasHistoryNode < canvasHistoryTree.size())
                      ? currentCanvasHistoryNode
                      : -1;
    node.sequence = nextCanvasHistorySequence++;
    node.markers = markers;

    const int newNodeIndex = canvasHistoryTree.size();
    canvasHistoryTree.push_back(node);

    if (node.parent >= 0 && node.parent < canvasHistoryTree.size()) {
        canvasHistoryTree[node.parent].children.push_back(newNodeIndex);
        canvasHistoryTree[node.parent].activeChild = newNodeIndex;
    }

    currentCanvasHistoryNode = newNodeIndex;
    poss = positions;
    pos_all = positions;
    rebuildImageHighlights();
    refreshCanvasHistoryDialog();
    updateWindowTitle();
}

void MainWindow::undoCanvasHistory()
{
    if (isAlignmentOrOptimizationRunning()) return;
    if (currentCanvasHistoryNode < 0 ||
        currentCanvasHistoryNode >= canvasHistoryTree.size()) {
        return;
    }

    const int parent = canvasHistoryTree[currentCanvasHistoryNode].parent;
    if (parent < 0 || parent >= canvasHistoryTree.size()) return;

    canvasHistoryTree[parent].activeChild = currentCanvasHistoryNode;
    restoreCanvasHistoryNode(parent);
}

void MainWindow::redoCanvasHistory()
{
    if (isAlignmentOrOptimizationRunning()) return;
    if (currentCanvasHistoryNode < 0 ||
        currentCanvasHistoryNode >= canvasHistoryTree.size()) {
        return;
    }

    const CanvasHistoryNode& current = canvasHistoryTree[currentCanvasHistoryNode];
    int child = current.activeChild;
    if (child < 0 || child >= canvasHistoryTree.size() ||
        canvasHistoryTree[child].parent != currentCanvasHistoryNode) {
        child = current.children.isEmpty() ? -1 : current.children.last();
    }
    if (child < 0 || child >= canvasHistoryTree.size()) return;

    restoreCanvasHistoryNode(child);
}

int MainWindow::latestCanvasHistoryNode() const
{
    int latestNode = -1;
    int latestSequence = -1;
    for (int i = 0; i < canvasHistoryTree.size(); ++i) {
        if (canvasHistoryTree[i].sequence > latestSequence) {
            latestSequence = canvasHistoryTree[i].sequence;
            latestNode = i;
        }
    }
    return latestNode;
}

void MainWindow::restoreCanvasHistoryNode(int nodeIndex)
{
    if (isAlignmentOrOptimizationRunning()) return;
    if (nodeIndex < 0 || nodeIndex >= canvasHistoryTree.size()) return;
    if (canvasHistoryTree[nodeIndex].positions.size() != items.size()) {
        showNonModalMessage(
            QMessageBox::Warning, tr("変更履歴"),
            tr("現在の画像枚数と異なる履歴のため、キャンパスへ反映できません。"));
        return;
    }

    int child = nodeIndex;
    int parent = canvasHistoryTree[child].parent;
    while (parent >= 0 && parent < canvasHistoryTree.size()) {
        canvasHistoryTree[parent].activeChild = child;
        child = parent;
        parent = canvasHistoryTree[child].parent;
    }

    currentCanvasHistoryNode = nodeIndex;
    applyCanvasPositions(canvasHistoryTree[nodeIndex].positions);
    if (nodeIndex != latestCanvasHistoryNode()) {
        ui->checkBox_2->setChecked(true);
    }
    refreshCanvasHistoryDialog();
}

void MainWindow::showCanvasHistoryDialog()
{
    if (canvasHistoryDialog) {
        refreshCanvasHistoryDialog();
        canvasHistoryDialog->show();
        canvasHistoryDialog->raise();
        canvasHistoryDialog->activateWindow();
        return;
    }

    canvasHistoryDialog = new QDialog(this);
    canvasHistoryDialog->setWindowTitle(tr("変更履歴"));
    canvasHistoryDialog->setModal(false);
    canvasHistoryDialog->setWindowModality(Qt::NonModal);
    trackDialogSize(canvasHistoryDialog, QStringLiteral("canvasHistory"),
                    QSize(720, 250));

    auto* rootLayout = new QVBoxLayout(canvasHistoryDialog);
    auto* graph = new CanvasHistoryGraphWidget(canvasHistoryDialog);
    graph->setProviders(
        [this]() { return canvasHistoryTree; },
        [this]() { return currentCanvasHistoryNode; },
        [this](int nodeIndex) { restoreCanvasHistoryNode(nodeIndex); });
    canvasHistoryGraphWidget = graph;

    auto* scrollArea = new QScrollArea(canvasHistoryDialog);
    scrollArea->setWidget(graph);
    scrollArea->setWidgetResizable(true);
    canvasHistoryScrollArea = scrollArea;
    rootLayout->addWidget(scrollArea, 1);

    auto* legendLayout = new QHBoxLayout();
    legendLayout->setContentsMargins(0, 0, 0, 0);
    legendLayout->setSpacing(14);

    auto addLegendItem = [this, legendLayout](const QString& text,
                                              const QColor& color,
                                              bool outlineOnly = false) {
        auto* itemLayout = new QHBoxLayout();
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(5);

        auto* swatch = new QFrame(canvasHistoryDialog);
        swatch->setFixedSize(16, 16);
        const QString bg = outlineOnly
                               ? QStringLiteral("transparent")
                               : QString("rgba(%1, %2, %3, %4)")
                                     .arg(color.red())
                                     .arg(color.green())
                                     .arg(color.blue())
                                     .arg(color.alpha());
        swatch->setStyleSheet(
            QString("background-color: %1; border: 2px solid rgb(%2, %3, %4); border-radius: 8px;")
                .arg(bg)
                .arg(color.red())
                .arg(color.green())
                .arg(color.blue()));

        auto* label = new QLabel(text, canvasHistoryDialog);
        canvasHistoryLegendLabels.push_back(label);
        itemLayout->addWidget(swatch);
        itemLayout->addWidget(label);
        legendLayout->addLayout(itemLayout);
    };

    addLegendItem(tr("現在地"), CanvasHistoryGraphWidget::currentMarkerColor(), true);
    addLegendItem(tr("位相相関結果"), CanvasHistoryGraphWidget::phaseMarkerColor());
    addLegendItem(tr("最適化結果"), CanvasHistoryGraphWidget::optimizationMarkerColor());
    legendLayout->addStretch();
    rootLayout->addLayout(legendLayout);

    connect(canvasHistoryDialog, &QObject::destroyed, this, [this]() {
        canvasHistoryDialog = nullptr;
        canvasHistoryGraphWidget = nullptr;
        canvasHistoryScrollArea = nullptr;
        canvasHistoryLegendLabels.clear();
    });

    canvasHistoryDialog->show();
    refreshCanvasHistoryDialog();
}

void MainWindow::refreshCanvasHistoryDialog()
{
    if (!canvasHistoryGraphWidget) return;
    canvasHistoryGraphWidget->setEnabled(!isAlignmentOrOptimizationRunning());
    canvasHistoryGraphWidget->refresh();
    QTimer::singleShot(0, this, &MainWindow::scrollCanvasHistoryToCurrent);
}

void MainWindow::scrollCanvasHistoryToCurrent()
{
    if (!canvasHistoryScrollArea || !canvasHistoryGraphWidget) return;

    const QRect currentRect = canvasHistoryGraphWidget->currentNodeRect();
    if (currentRect.isNull()) return;

    const QSize viewportSize = canvasHistoryScrollArea->viewport()->size();
    const int xMargin = std::min(180, std::max(50, viewportSize.width() / 4));
    const int yMargin = std::min(100, std::max(30, viewportSize.height() / 4));
    const QPoint center = currentRect.center();
    canvasHistoryScrollArea->ensureVisible(center.x(), center.y(), xMargin, yMargin);
}

bool MainWindow::isAlignmentOrOptimizationRunning() const
{
    return watcher.isRunning() || watcher_re.isRunning() ||
           image_make_Watcher.isRunning() ||
           trwsRunning || leastSquaresRunning ||
           (leastSquaresWatcher && leastSquaresWatcher->isRunning());
}

QString MainWindow::formatSelectedImageIndices(QVector<int> indices) const
{
    if (indices.isEmpty()) return QString();

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

    QStringList ranges;
    int start = indices[0];
    int prev = indices[0];
    for (int i = 1; i < indices.size(); ++i) {
        if (indices[i] == prev + 1) {
            prev = indices[i];
            continue;
        }

        ranges << (start == prev
                       ? QString::number(start + 1)
                       : QString("%1-%2").arg(start + 1).arg(prev + 1));
        start = prev = indices[i];
    }
    ranges << (start == prev
                   ? QString::number(start + 1)
                   : QString("%1-%2").arg(start + 1).arg(prev + 1));
    return ranges.join(",");
}

QVector<int> MainWindow::parseImageIndexSpec(const QString& text, QString* errorMessage) const
{
    QVector<int> indices;
    const QString trimmed = text.trimmed();
    if (items.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("ハイライトできる画像がありません。");
        }
        return indices;
    }
    if (trimmed.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("画像番号を入力してください。");
        }
        return indices;
    }

    const int maxIndex = items.size();
    const QStringList tokens = trimmed.split(',', Qt::SkipEmptyParts);
    for (const QString& rawToken : tokens) {
        const QString token = rawToken.trimmed();
        if (token.isEmpty()) {
            continue;
        }

        int first = 0;
        int last = 0;
        if (token.count(QChar('-')) == 0) {
            bool ok = false;
            first = token.toInt(&ok);
            last = first;
            if (!ok) {
                if (errorMessage) {
                    *errorMessage = tr("画像番号の指定が不正です: %1").arg(token);
                }
                return {};
            }
        } else if (token.count(QChar('-')) == 1) {
            const QStringList parts = token.split('-', Qt::KeepEmptyParts);
            if (parts.size() != 2) {
                if (errorMessage) {
                    *errorMessage = tr("範囲指定が不正です: %1").arg(token);
                }
                return {};
            }

            bool okFirst = false;
            bool okLast = false;
            first = parts[0].trimmed().toInt(&okFirst);
            last = parts[1].trimmed().toInt(&okLast);
            if (!okFirst || !okLast) {
                if (errorMessage) {
                    *errorMessage = tr("範囲指定が不正です: %1").arg(token);
                }
                return {};
            }
            if (first > last) {
                std::swap(first, last);
            }
        } else {
            if (errorMessage) {
                *errorMessage = tr("範囲指定が不正です: %1").arg(token);
            }
            return {};
        }

        if (first < 1 || last < 1 || first > maxIndex || last > maxIndex) {
            if (errorMessage) {
                *errorMessage = tr("画像番号は 1-%1 の範囲で指定してください。").arg(maxIndex);
            }
            return {};
        }

        for (int imageId = first; imageId <= last; ++imageId) {
            indices.push_back(imageId - 1);
        }
    }

    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    if (indices.isEmpty() && errorMessage) {
        *errorMessage = tr("ハイライト対象の画像がありません。");
    }
    return indices;
}

void MainWindow::showImageHighlightDialog()
{
    if (!imageHighlightDialog) {
        imageHighlightDialog = new QDialog(this);
        imageHighlightDialog->setWindowTitle(tr("画像ハイライト"));
        imageHighlightDialog->setModal(false);
        imageHighlightDialog->setWindowModality(Qt::NonModal);
        trackDialogSize(imageHighlightDialog,
                        QStringLiteral("imageHighlight"), QSize(360, 110));

        auto* rootLayout = new QVBoxLayout(imageHighlightDialog);
        auto* inputLayout = new QHBoxLayout();
        imageHighlightIdLabel = new QLabel(tr("画像ID"), imageHighlightDialog);
        imageHighlightEdit = new QLineEdit(imageHighlightDialog);
        imageHighlightEdit->setPlaceholderText(tr("例: 2-6,8-10"));
        inputLayout->addWidget(imageHighlightIdLabel);
        inputLayout->addWidget(imageHighlightEdit, 1);
        rootLayout->addLayout(inputLayout);

        auto* buttonLayout = new QHBoxLayout();
        imageHighlightColorButton = new QPushButton(tr("色"), imageHighlightDialog);
        imageHighlightApplyButton = new QPushButton(tr("ハイライト"), imageHighlightDialog);
        imageHighlightClearButton = new QPushButton(tr("クリア"), imageHighlightDialog);
        buttonLayout->addWidget(imageHighlightColorButton);
        buttonLayout->addStretch();
        buttonLayout->addWidget(imageHighlightApplyButton);
        buttonLayout->addWidget(imageHighlightClearButton);
        rootLayout->addLayout(buttonLayout);

        connect(imageHighlightColorButton, &QPushButton::clicked, this, [this]() {
            auto* colorDialog = new QColorDialog(imageHighlightColor,
                                                 imageHighlightDialog);
            colorDialog->setWindowTitle(tr("ハイライト色"));
            colorDialog->setAttribute(Qt::WA_DeleteOnClose);
            colorDialog->setModal(false);
            colorDialog->setWindowModality(Qt::NonModal);
            trackDialogSize(colorDialog,
                            QStringLiteral("imageHighlightColor"),
                            QSize(600, 450));
            imageHighlightColorButton->setEnabled(false);
            connect(colorDialog, &QColorDialog::colorSelected, this,
                    [this](const QColor& color) {
                if (color.isValid()) {
                    imageHighlightColor = color;
                    updateImageHighlightColorButton();
                }
            });
            connect(colorDialog, &QDialog::finished, this, [this]() {
                if (imageHighlightColorButton) imageHighlightColorButton->setEnabled(true);
            });
            colorDialog->show();
        });
        connect(imageHighlightApplyButton, &QPushButton::clicked,
                this, &MainWindow::applyImageHighlightsFromDialog);
        connect(imageHighlightClearButton, &QPushButton::clicked, this, [this]() {
            if (imageHighlightEdit) {
                imageHighlightEdit->clear();
            }
            clearImageHighlights();
        });
        connect(imageHighlightEdit, &QLineEdit::returnPressed,
                this, &MainWindow::applyImageHighlightsFromDialog);
        connect(imageHighlightDialog, &QDialog::finished, this, [this](int) {
            if (imageHighlightEdit) {
                imageHighlightEdit->clear();
            }
            clearImageHighlights();
        });
    }

    updateImageHighlightColorButton();
    imageHighlightDialog->show();
    imageHighlightDialog->raise();
    imageHighlightDialog->activateWindow();
    if (imageHighlightEdit) {
        imageHighlightEdit->setFocus();
    }
}

void MainWindow::updateImageHighlightColorButton()
{
    if (!imageHighlightColorButton) {
        return;
    }

    const int luminance = static_cast<int>(
        0.299 * imageHighlightColor.red() +
        0.587 * imageHighlightColor.green() +
        0.114 * imageHighlightColor.blue());
    const QString textColor = luminance > 140 ? "black" : "white";
    imageHighlightColorButton->setStyleSheet(
        QString("QPushButton { background-color: rgb(%1, %2, %3); color: %4; }")
            .arg(imageHighlightColor.red())
            .arg(imageHighlightColor.green())
            .arg(imageHighlightColor.blue())
            .arg(textColor));
}

void MainWindow::applyImageHighlightsFromDialog()
{
    if (!imageHighlightEdit) {
        return;
    }

    QString errorMessage;
    const QVector<int> indices = parseImageIndexSpec(imageHighlightEdit->text(), &errorMessage);
    if (!errorMessage.isEmpty()) {
        QWidget* parentWidget = imageHighlightDialog
                                    ? static_cast<QWidget*>(imageHighlightDialog)
                                    : static_cast<QWidget*>(this);
        showNonModalMessage(QMessageBox::Warning, tr("画像ハイライト"),
                            errorMessage, parentWidget);
        return;
    }

    imageHighlightIndices = indices;
    rebuildImageHighlights();
}

void MainWindow::clearImageHighlightRects()
{
    for (QGraphicsRectItem* rect : std::as_const(imageHighlightRects)) {
        delete rect;
    }
    imageHighlightRects.clear();
}

void MainWindow::rebuildImageHighlights()
{
    clearImageHighlightRects();
    if (!scene) {
        return;
    }

    QPen pen(imageHighlightColor, 4.0);
    pen.setCosmetic(true);

    for (int index : std::as_const(imageHighlightIndices)) {
        if (index < 0 || index >= items.size() || !items[index]) {
            continue;
        }

        auto* rect = scene->addRect(items[index]->sceneBoundingRect(), pen, Qt::NoBrush);
        rect->setPen(pen);
        rect->setBrush(Qt::NoBrush);
        rect->setZValue(1000000.0);
        rect->setAcceptedMouseButtons(Qt::NoButton);
        rect->setFlag(QGraphicsItem::ItemIsSelectable, false);
        imageHighlightRects.push_back(rect);
    }
}

void MainWindow::clearImageHighlights()
{
    imageHighlightIndices.clear();
    clearImageHighlightRects();
}

void MainWindow::showCalculationProgressDialog(const QString& title,
                                               const QString& computePath,
                                               int maximum)
{
    if (!calculationProgressDialog) {
        calculationProgressDialog = new QDialog(this);
        calculationProgressDialog->setModal(false);
        calculationProgressDialog->setWindowModality(Qt::NonModal);
        calculationProgressDialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
        calculationProgressDialog->setMinimumWidth(360);
        trackDialogSize(calculationProgressDialog,
                        QStringLiteral("calculationProgress"),
                        QSize(400, 170));

        auto* layout = new QVBoxLayout(calculationProgressDialog);
        calculationComputePathLabel = new QLabel(calculationProgressDialog);
        calculationComputePathLabel->setAlignment(Qt::AlignCenter);
        calculationProgressLabel = new QLabel(calculationProgressDialog);
        calculationProgressLabel->setAlignment(Qt::AlignCenter);
        calculationProgressBar = new QProgressBar(calculationProgressDialog);
        calculationCancelButton = new QPushButton(calculationProgressDialog);
        layout->addWidget(calculationComputePathLabel);
        layout->addWidget(calculationProgressLabel);
        layout->addWidget(calculationProgressBar);
        layout->addWidget(calculationCancelButton, 0, Qt::AlignRight);

        connect(calculationCancelButton, &QPushButton::clicked,
                this, &MainWindow::cancelCurrentCalculation);
        connect(calculationProgressDialog, &QDialog::rejected,
                this, &MainWindow::cancelCurrentCalculation);
    }

    calculationCancelRequested = std::make_shared<std::atomic_bool>(false);
    setCalculationUiLocked(true);
    calculationProgressDialog->setWindowTitle(title);
    calculationComputePathLabel->setText(tr("計算パス: %1").arg(computePath));
    calculationCancelButton->setText(tr("キャンセル"));
    calculationCancelButton->setEnabled(true);
    calculationProgressTimer.restart();
    updateCalculationProgress(0, maximum, tr("準備中"));
    calculationProgressDialog->show();
    calculationProgressDialog->raise();
    calculationProgressDialog->activateWindow();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void MainWindow::updateCalculationProgress(int value, int maximum, const QString& text)
{
    if (!calculationProgressDialog || !calculationProgressLabel
        || !calculationProgressBar) {
        return;
    }

    if (maximum <= 0) {
        calculationProgressBar->setRange(0, 0);
        calculationProgressLabel->setText(text);
        return;
    }

    maximum = std::max(1, maximum);
    const int displayValue = std::clamp(value, 0, maximum);
    calculationProgressBar->setRange(0, maximum);
    calculationProgressBar->setValue(displayValue);

    const int percent = static_cast<int>(std::round((100.0 * displayValue) / maximum));
    QString remaining = tr("計算中");
    if (displayValue > 0 && displayValue < maximum && calculationProgressTimer.isValid()) {
        const double elapsed = static_cast<double>(calculationProgressTimer.elapsed());
        const qint64 remainingMs = static_cast<qint64>(
            elapsed * (maximum - displayValue) / displayValue);
        remaining = tr("残り %1").arg(formatDuration(remainingMs));
    } else if (displayValue >= maximum) {
        remaining = tr("完了処理中");
    }

    calculationProgressLabel->setText(
        QString("%1\n%2%  %3").arg(text).arg(percent).arg(remaining));
}

void MainWindow::hideCalculationProgressDialog()
{
    if (calculationProgressDialog) {
        AppSettings::setWindowSize(QStringLiteral("calculationProgress"),
                                   calculationProgressDialog->size());
        calculationProgressDialog->hide();
    }
    setCalculationUiLocked(false);
    calculationCancelRequested.reset();
}

bool MainWindow::calculationCancellationRequested() const
{
    return calculationCancelRequested
           && calculationCancelRequested->load(std::memory_order_relaxed);
}

void MainWindow::cancelCurrentCalculation()
{
    if (!calculationCancelRequested) {
        return;
    }
    calculationCancelRequested->store(true, std::memory_order_relaxed);
    if (calculationCancelButton) {
        calculationCancelButton->setEnabled(false);
    }
    if (calculationProgressLabel) {
        calculationProgressLabel->setText(tr("キャンセル中…"));
    }

    if (watcher.isRunning()) watcher.cancel();
    if (watcher_re.isRunning()) watcher_re.cancel();
    if (image_make_Watcher.isRunning()) image_make_Watcher.cancel();
    if (trwsWatcher && trwsWatcher->isRunning()) trwsWatcher->cancel();
    if (leastSquaresWatcher && leastSquaresWatcher->isRunning()) {
        leastSquaresWatcher->cancel();
    }
}

void MainWindow::setCalculationUiLocked(bool locked)
{
    if (calculationUiLocked == locked) {
        return;
    }

    const QList<QWidget*> controls = {
        ui->pushButton_1,
        ui->checkBox,
        ui->pushButton_7,
        ui->groupBox_4,
        ui->groupBox_3,
        ui->groupBox_6
    };

    if (locked) {
        calculationControlStates.clear();
        for (QWidget* control : controls) {
            calculationControlStates.insert(control, control->isEnabled());
        }
        layoutLockedBeforeCalculation = ui->checkBox->isChecked();
        ui->checkBox->setChecked(true);
        for (QWidget* control : controls) {
            control->setEnabled(false);
        }
        if (applicationSettingsAction) {
            applicationSettingsActionEnabledBeforeCalculation =
                applicationSettingsAction->isEnabled();
            applicationSettingsAction->setEnabled(false);
        }
        if (fileMenu) fileMenu->setEnabled(false);
        if (projectOpenAction) projectOpenAction->setEnabled(false);
        if (projectSaveAction) projectSaveAction->setEnabled(false);
        if (projectSaveAsAction) projectSaveAsAction->setEnabled(false);
        if (projectPngExportAction) projectPngExportAction->setEnabled(false);
        if (optimizationSettingsDialog) optimizationSettingsDialog->setEnabled(false);
        if (leastSquaresSettingsDialog) leastSquaresSettingsDialog->setEnabled(false);
        if (mergeSettingsDialog) mergeSettingsDialog->setEnabled(false);
        if (applicationSettingsDialog) applicationSettingsDialog->setEnabled(false);
        if (fileInputDialog) fileInputDialog->setEnabled(false);
    } else {
        ui->checkBox->setChecked(layoutLockedBeforeCalculation);
        for (auto it = calculationControlStates.cbegin();
             it != calculationControlStates.cend(); ++it) {
            if (it.key()) {
                it.key()->setEnabled(it.value());
            }
        }
        calculationControlStates.clear();
        if (applicationSettingsAction) {
            applicationSettingsAction->setEnabled(
                applicationSettingsActionEnabledBeforeCalculation);
        }
        if (fileMenu) fileMenu->setEnabled(true);
        if (projectOpenAction) projectOpenAction->setEnabled(true);
        if (projectSaveAction) projectSaveAction->setEnabled(true);
        if (projectSaveAsAction) projectSaveAsAction->setEnabled(true);
        if (projectPngExportAction) projectPngExportAction->setEnabled(true);
        if (optimizationSettingsDialog) optimizationSettingsDialog->setEnabled(true);
        if (leastSquaresSettingsDialog) leastSquaresSettingsDialog->setEnabled(true);
        if (mergeSettingsDialog) mergeSettingsDialog->setEnabled(true);
        if (applicationSettingsDialog) applicationSettingsDialog->setEnabled(true);
        if (fileInputDialog) fileInputDialog->setEnabled(true);
    }
    calculationUiLocked = locked;
}

void MainWindow::finishCancelledCalculation(const QString& resultText)
{
    if (statusMessageLabel) {
        statusMessageLabel->setText(resultText);
    }
    hideCalculationProgressDialog();
    refreshCanvasHistoryDialog();
}

void MainWindow::showNonModalMessage(QMessageBox::Icon icon,
                                     const QString& title,
                                     const QString& text,
                                     QWidget* parent)
{
    auto* message = new QMessageBox(icon, title, text, QMessageBox::Ok,
                                    parent ? parent : this);
    message->setAttribute(Qt::WA_DeleteOnClose);
    message->setModal(false);
    message->setWindowModality(Qt::NonModal);
    message->show();
}

QString MainWindow::selectedOptimizationComputePath() const
{
    const VulkanExecutionOptions options = AppSettings::vulkanOptions();
    if (!options.enabled) {
        return QStringLiteral("CPU");
    }
    if (MetalSsimEngine::isBuilt() && MetalSsimEngine::isAvailable()) {
        return QStringLiteral("Metal");
    }
    if (VulkanSsimEngine::isBuilt()
        && (vulkanDetectionInProgress || !vulkanScanResult.devices.isEmpty())) {
        return QStringLiteral("Vulkan");
    }
    return QStringLiteral("CPU");
}

QString MainWindow::formatDuration(qint64 milliseconds) const
{
    if (milliseconds < 0) milliseconds = 0;
    const qint64 totalSeconds = (milliseconds + 999) / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    if (minutes > 0) {
        return tr("%1分%2秒").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
    }
    return tr("%1秒").arg(seconds);
}

int MainWindow::imageIndexAtScenePos(const QPointF& scenePos) const
{
    if (!scene) return -1;
    const QList<QGraphicsItem*> hitItems = scene->items(scenePos);
    for (QGraphicsItem* hit : hitItems) {
        auto* pixmapItem = qgraphicsitem_cast<QGraphicsPixmapItem*>(hit);
        if (!pixmapItem) continue;
        const int idx = items.indexOf(pixmapItem);
        if (idx >= 0) return idx;
    }
    return -1;
}

void MainWindow::selectImageRange(int first, int last)
{
    if (!scene || items.isEmpty()) return;

    const int maxIndex = static_cast<int>(items.size()) - 1;
    first = std::clamp(first, 0, maxIndex);
    last = std::clamp(last, 0, maxIndex);
    if (first > last) {
        std::swap(first, last);
    }

    QSignalBlocker blocker(scene);
    scene->clearSelection();
    for (int i = first; i <= last; ++i) {
        if (items[i]) {
            items[i]->setSelected(true);
        }
    }
    onSceneSelectionChanged();
}

void MainWindow::onOpacity1Changed(int percent)
{
    const auto selected = scene->selectedItems();
    if (selected.isEmpty()) return;
    qreal alpha = (100 - percent) / 100.0;
    for (QGraphicsItem* item : selected) {
        item->setOpacity(alpha);
        int idx = items.indexOf(static_cast<QGraphicsPixmapItem*>(item));
        if (idx >= 0 && idx < toumeido.size()) {
            toumeido[idx] = percent;
        }
    }
}

void MainWindow::onSceneSelectionChanged()
{
    const auto selected = scene->selectedItems();
    const int n = input_files.size();
    if (selected.isEmpty()) {
        for (int i = 0; i < n; ++i) {
            if (i < items.size() && items[i]) {
                items[i]->setZValue(i + 1);
            }
        }
        ui->sliderOpacity1->setValue(0);
        ui->label_2->setEnabled(false);
        ui->sliderOpacity1->setEnabled(false);
        ui->spinOpacity1->setEnabled(false);
        ui->label_9->setText(tr("なし"));
        ui->label_8->setEnabled(false);
        ui->label_9->setEnabled(false);
        return;
    }
    ui->label_2->setEnabled(true);
    ui->sliderOpacity1->setEnabled(true);
    ui->spinOpacity1->setEnabled(true);
    ui->label_8->setEnabled(true);
    ui->label_9->setEnabled(true);

    QGraphicsItem* firstItem = selected.first();
    int idx = items.indexOf(static_cast<QGraphicsPixmapItem*>(firstItem));
    if (idx < 0 || idx >= toumeido.size()) {
        return;
    }
    ui->sliderOpacity1->setValue(toumeido[idx]);

    // 選択された画像を前面へ出す
    QVector<int> idx_list;
    for (QGraphicsItem* item : selected) {
        int idx = items.indexOf(static_cast<QGraphicsPixmapItem*>(item));
        if (idx >= 0) {
            idx_list.push_back(idx);
        }
    }

    // 選択番号を表示
    ui->label_9->setText(formatSelectedImageIndices(idx_list));

    for (int i = 0; i < n; ++i) {
        if (i < items.size() && items[i]) {
            if (idx_list.contains(i)) {
                items[i]->setZValue(i + n + 1);
            } else {
                items[i]->setZValue(i + 1);
            }
        }
    }
}


// 各スレッドで並列計算
static ifft_thread_output calc_oneshot(const ifft_thread_input& in)
{
    QPoint c_pos1 = QPoint(0,0);
    QPoint c_pos2 = QPoint(in.pos2.x() - in.pos1.x(), in.pos2.y() - in.pos1.y());
    QVector<QPoint> c_posVs;
    c_posVs.reserve(in.calc_loop_num);
    c_posVs.push_back(c_pos2);

    int x_r = c_pos2.x();
    int y_r = c_pos2.y();
    double response = 0.0;
    bool stab_now = false;
    int count = 0;
    double x_d = static_cast<double>(c_pos2.x());
    double y_d = static_cast<double>(c_pos2.y());

    ifft_thread_output r;
    r.img1_id = in.img1_id;
    r.img2_id = in.img2_id;
    if (workerCancellationRequested(in.cancelRequested)) {
        r.cancelled = true;
        return r;
    }

    try {
        // 最大4回計算して安定するか確認する
        for (int l = 0; l < in.calc_loop_num; ++l) {
            if (workerCancellationRequested(in.cancelRequested)) {
                r.cancelled = true;
                return r;
            }

        // 重なり領域をcropして取り出す。
            return_struct2 r_st = Crop_2ImageTo2Image(in.input1, in.input2, in.px1, c_pos1, in.px2, c_posVs[l]);
            cv::Mat crop1 = r_st.img1;
            cv::Mat crop2 = r_st.img2;

            if (crop1.rows <= 1 || crop1.cols <= 1 || crop2.rows <= 1 || crop2.cols <= 1) {
                // 元のvecを元に計算
                r.score = -1;
                r.vecX = c_pos2.x();
                r.vecY = c_pos2.y();
                r.stability = false;
                r.loop_num = count;
                r.ssim = 0.0;
                return r;
            }

            // 色変換
            cv::Mat1f a = clahe_then_grad(crop1);
            cv::Mat1f b = clahe_then_grad(crop2);

            // 位相相関法による位置合わせ
            cv::Point2d shift = cv::phaseCorrelate(a, b, cv::noArray(), &response);

            // 四捨五入
            cv::Point2d shift_r(std::round(shift.x), std::round(shift.y));

            // 1枚目→2枚目画像の新しい移動ベクトル (int)
            x_r = c_posVs[l].x() - c_pos1.x() - shift_r.x;
            y_r = c_posVs[l].y() - c_pos1.y() - shift_r.y;

            // 1枚目→2枚目画像の新しい移動ベクトル (double)
            x_d = static_cast<double>(c_posVs[l].x() - c_pos1.x()) - shift.x;
            y_d = static_cast<double>(c_posVs[l].y() - c_pos1.y()) - shift.y;

            count++;

            if (c_posVs[l].x() == x_r && c_posVs[l].y() == y_r) {
                stab_now = true;
                break;
            }
            c_posVs.push_back(QPoint(x_r,y_r));
        }

        r.vecX = x_d;
        r.vecY = y_d;
        r.score = response;
        r.stability = stab_now;
        r.loop_num = count;
        if (workerCancellationRequested(in.cancelRequested)) {
            r.cancelled = true;
            return r;
        }
        r.ssim = SSIM_calc_oneshot(SSIM_TaskInput{in.input1,in.input2,in.px1,QPoint(0,0),in.px2,QPoint(x_r,y_r),0,0});
        return r;
    } catch (const cv::Exception&) {
        r.calc_error = true;
        r.score = -1;
        r.vecX = c_pos2.x();
        r.vecY = c_pos2.y();
        r.stability = false;
        r.loop_num = count;
        r.ssim = 0.0;
        return r;
    } catch (...) {
        r.calc_error = true;
        r.score = -1;
        r.vecX = c_pos2.x();
        r.vecY = c_pos2.y();
        r.stability = false;
        r.loop_num = count;
        r.ssim = 0.0;
        return r;
    }
}

// iFFTを別スレッドで開始
void MainWindow::calc_iFFT()
{
    if (isAlignmentOrOptimizationRunning()) return;

    // 画像があるか判定
    const int n = input_files.size();
    if (n <= 1) {
        showNonModalMessage(QMessageBox::Warning, tr("計算"),
                            tr("2枚以上の画像を入力してください。"));
        return;
    }

    showCalculationProgressDialog(tr("位置合わせ（位相相関法）"),
                                  QStringLiteral("CPU"));
    ui->label_5->clear();
    calc1FinishedBeforeCalculation = calc1_finished_state;
    calc1_finished_state = false;
    leastSquaresDetails.clear();
    ui->checkBox->setChecked(true);
    ui->checkBox->setEnabled(false);
    //ui->groupBox_5->setEnabled(false);
    ui->pushButton_Calc1->setEnabled(false);
    //ui->pushButton->setEnabled(false);
    //ui->label_4->setEnabled(false);
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    ui->label_6->setText("");
    ui->label_12->setText("");
    ui->pushButton_3->setEnabled(false);
    ui->pushButton_8->setEnabled(false);
    //ui->pushButton_5->setEnabled(false);
    ui->pushButton_6->setEnabled(false);
    ui->pushButton_10->setEnabled(false);
    ui->pushButton_11->setEnabled(false);
    ui->checkBox_2->setEnabled(false);
    refreshCanvasHistoryDialog();

    // 画像データをOpenCV向けに変換
    imgs.resize(n);
    for (int i = 0; i < n; ++i) {
        QImage img_QI = items[i]->pixmap().toImage();
        imgs[i] = ImageUtils::qimage_to_mat_bgra(img_QI);
    }
    res_all.resize(n);
    for (int i = 0; i < n; ++i) {
        res_all[i] = items[i]->pixmap().size();
    }

    // 並列処理向けの入力値を作成する
    QVector<ifft_thread_input> inputs;
    const bool sceneSource = ui->checkBox_2->isChecked();
    if (sceneSource) {
        syncPossFromScene();
        inputs.resize(n - 1);
        for (int i = 0; i < n - 1; ++i) {
            inputs[i].img1_id = i;
            inputs[i].img2_id = i + 1;
            inputs[i].input1 = imgs[i];
            inputs[i].input2 = imgs[i + 1];
            inputs[i].px1 = res_all[i];
            inputs[i].px2 = res_all[i + 1];
            inputs[i].pos1 = poss[i];
            inputs[i].pos2 = poss[i + 1];
            inputs[i].calc_loop_num = calc_loop_num;
            inputs[i].cancelRequested = calculationCancelRequested;
        }

        updateCalculationProgress(0, inputs.size(), tr("位置合わせを計算中"));
        QFuture<ifft_thread_output> future = QtConcurrent::mapped(inputs, calc_oneshot);
        watcher.setFuture(future);
        refreshCanvasHistoryDialog();
        return;
    }

    int m_rows = ui->cornerSelector->getRows();
    int m_cols = ui->cornerSelector->getCols();

    if (m_rows == 0 && m_cols == 0) {
        inputs.resize((n-1)*4);
        // inputsへ格納していく。移動方向は4方向全て
        int overX = ui->horizontalSlider->value(); // x方向重なり率
        int overY = ui->horizontalSlider_2->value(); // y方向重なり率
        int x,y;
        for (int i = 0; i < n-1; ++i) {
            QSize res1 = res_all[i];
            QSize res2 = res_all[i+1];
            for (int j = 0; j < 4; ++j) {
                inputs[i*4+j].input1 = imgs[i];
                inputs[i*4+j].input2 = imgs[i+1];
                inputs[i*4+j].px1 = res_all[i];
                inputs[i*4+j].px2 = res_all[i+1];
                if (j == 0) { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                } else if (j == 1) { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                } else if (j == 2) { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                } else if (j == 3) { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                }
                QPoint c_pos1 = QPoint(0,0);
                inputs[i*4+j].pos1 = QPoint(0,0);
                inputs[i*4+j].pos2 = QPoint(x,y);
                inputs[i*4+j].img1_id = i;
                inputs[i*4+j].img2_id = i+1;
                inputs[i*4+j].calc_loop_num = calc_loop_num;
            }
        }
    } else {
        inputs.resize(n-1);
        // inputsへ格納していく
        for (int i = 0; i < n-1; ++i) {
            inputs[i].img1_id = i;
            inputs[i].img2_id = i+1;
            inputs[i].input1 = imgs[i];
            inputs[i].input2 = imgs[i+1];
            inputs[i].px1 = res_all[i];
            inputs[i].px2 = res_all[i+1];
            inputs[i].pos1 = pos_all[i];
            inputs[i].pos2 = pos_all[i+1];
            inputs[i].calc_loop_num = calc_loop_num;
        }
    }
    for (ifft_thread_input& input : inputs) {
        input.cancelRequested = calculationCancelRequested;
    }
    // 並列計算開始
    updateCalculationProgress(0, inputs.size(), tr("位置合わせを計算中"));
    QFuture<ifft_thread_output> future = QtConcurrent::mapped(inputs, calc_oneshot);
    // 監視開始
    watcher.setFuture(future);
    refreshCanvasHistoryDialog();
}


// iFFTを別スレッドで開始
void MainWindow::calc_iFFT_rerun()
{
    if (watcher.isRunning()) { watcher.cancel(); watcher.waitForFinished();}

    // 再計算する画像の枚数を数える
    int nF = std::count(checkTF_calc.begin(), checkTF_calc.end(), false);

    // 計算範囲を取得
    int overX = ui->horizontalSlider->value(); // x方向重なり率
    int overY = ui->horizontalSlider_2->value(); // y方向重なり率
    int overR = ui->horizontalSlider_3->value(); // 探索範囲

    // 探索範囲を計算(10%ずつずらす)
    int tempN = (overR + re_step - 1) / re_step;
    QVector<QPoint> ov_list;
    ovc = 0;
    ov_list.reserve(tempN * tempN * 3);
    for (int i = -tempN; i <= tempN; ++i) {
        for (int j = -tempN; j <= tempN; ++j) {
            if (std::hypot(i, j) <= (static_cast<double>(overR) / re_step)) {
                int xo = overX+(i*re_step);
                int yo = overY+(j*re_step);
                if (xo > 0 && xo < 100 && yo > 0 && yo < 100) {
                    ov_list.push_back(QPoint(xo,yo));
                    ovc++;
                }
            }
        }
    }

    // 探索候補が0件だと後段で未初期化参照が起きるため、最低1件を保証
    if (ov_list.isEmpty()) {
        const int xo = std::clamp(overX, 1, 99);
        const int yo = std::clamp(overY, 1, 99);
        ov_list.push_back(QPoint(xo, yo));
        ovc = 1;
    }

    // 並列処理向けの入力値を作成する
    QVector<ifft_thread_input> inputs;
    int m_layoutState = ui->cornerSelector->getStatus();
    int m_rows = ui->cornerSelector->getRows();
    int m_cols = ui->cornerSelector->getCols();

    i2id.clear();
    i2id.reserve(nF);
    for (int i = 0; i < input_files.size()-1; ++i) {
        if (!checkTF_calc[i]) {
            i2id.push_back(i);
        }
    }

    if (m_rows == 0 && m_cols == 0) {
        inputs.resize(nF*ovc*4);
        // inputsへ格納していく。移動方向は4方向全て
        int x,y;
        for (int i = 0; i < nF; ++i) {
            QSize res1 = res_all[i2id[i]];
            QSize res2 = res_all[i2id[i]+1];
            for (int j = 0; j < ovc; ++j) {
                for (int k = 0; k < 4; ++k) {
                    inputs[i*ovc*4+j*4+k].input1 = imgs[i2id[i]];
                    inputs[i*ovc*4+j*4+k].input2 = imgs[i2id[i]+1];
                    inputs[i*ovc*4+j*4+k].px1 = res1;
                    inputs[i*ovc*4+j*4+k].px2 = res2;
                    if (k == 0) { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    } else if (k == 1) { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    } else if (k == 2) { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    } else if (k == 3) { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    }
                    QPoint c_pos1 = QPoint(0,0);
                    inputs[i*ovc*4+j*4+k].pos1 = QPoint(0,0);
                    inputs[i*ovc*4+j*4+k].pos2 = QPoint(x,y);
                    inputs[i*ovc*4+j*4+k].img1_id = i2id[i];
                    inputs[i*ovc*4+j*4+k].img2_id = i2id[i]+1;
                    inputs[i*ovc*4+j*4+k].calc_loop_num = calc_loop_num;
                }
            }
        }
    } else {
        inputs.resize(nF*ovc);
        int x,y;
        for (int i = 0; i < nF; ++i) {
            QSize res1 = res_all[i2id[i]];
            QSize res2 = res_all[i2id[i]+1];
            for (int j = 0; j < ovc; ++j) {
                inputs[i*ovc+j].input1 = imgs[i2id[i]];
                inputs[i*ovc+j].input2 = imgs[i2id[i]+1];
                inputs[i*ovc+j].px1 = res1;
                inputs[i*ovc+j].px2 = res2;
                int x, y;
                if (m_layoutState == 1) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    } else if (q % 2 == 0) { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    } else { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    }
                } else if (m_layoutState == 2) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    } else if (q % 2 == 0) { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    } else { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    }
                } else if (m_layoutState == 3) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    } else if (q % 2 == 0) { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    } else { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    }
                } else if (m_layoutState == 4) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    } else if (q % 2 == 0) { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    } else { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    }
                } else if (m_layoutState == 5) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    } else if (q % 2 == 0) { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    } else { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    }
                } else if (m_layoutState == 6) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    } else if (q % 2 == 0) { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    } else { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    }
                } else if (m_layoutState == 7) {
                    int q = i2id[i] / m_cols;
                    int r = i2id[i] % m_cols;
                    if (r == 0) { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    } else if (q % 2 == 0) { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    } else { // →
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = res1.width() - overlap;
                        y = (res1.height() - res2.height()) / 2;
                    }
                } else if (m_layoutState == 8) {
                    int q = i2id[i] / m_rows;
                    int r = i2id[i] % m_rows;
                    if (r == 0) { // ←
                        int overlap = std::min(res1.width(), res2.width()) * ov_list[j].x() / 100;
                        x = overlap - res2.width();
                        y = (res1.height() - res2.height()) / 2;
                    } else if (q % 2 == 0) { // ↑
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = overlap - res2.height();
                        x = (res1.width() - res2.width()) / 2;
                    } else { // ↓
                        int overlap = std::min(res1.height(), res2.height()) * ov_list[j].y() / 100;
                        y = res1.height() - overlap;
                        x = (res1.width() - res2.width()) / 2;
                    }
                }
                QPoint c_pos1 = QPoint(0,0);
                inputs[i*ovc+j].pos1 = QPoint(0,0);
                inputs[i*ovc+j].pos2 = QPoint(x,y);
                inputs[i*ovc+j].img1_id = i2id[i];
                inputs[i*ovc+j].img2_id = i2id[i]+1;
                inputs[i*ovc+j].calc_loop_num = calc_loop_num;
            }
        }
    }
    for (ifft_thread_input& input : inputs) {
        input.cancelRequested = calculationCancelRequested;
    }
    // 並列計算開始
    updateCalculationProgress(0, inputs.size(), tr("不良箇所を再計算中"));
    QFuture<ifft_thread_output> future = QtConcurrent::mapped(inputs, calc_oneshot);
    // 監視開始
    watcher_re.setFuture(future);
    refreshCanvasHistoryDialog();
}

void MainWindow::calc_finish_1()
{
    calc_results = watcher.future().results();
    const bool cancelled = calculationCancellationRequested()
                           || watcher.future().isCanceled()
                           || std::any_of(calc_results.cbegin(), calc_results.cend(),
                                          [](const ifft_thread_output& r) {
        return r.cancelled;
    });
    if (cancelled) {
        calc_results.clear();
        calc1_finished_state = calc1FinishedBeforeCalculation;
        ui->label_5->setText(tr("キャンセル"));
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton_4->setEnabled(!output_img.empty());
        ui->pushButton_2->setEnabled(calc1_finished_state);
        ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
        ui->pushButton_8->setEnabled(calc1_finished_state && input_files.size() > 1);
        ui->pushButton_6->setEnabled(true);
        ui->pushButton_10->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        finishCancelledCalculation(tr("位置合わせ計算をキャンセルしました。"));
        return;
    }
    const bool has_calc_error = std::any_of(calc_results.cbegin(), calc_results.cend(),
                                            [](const ifft_thread_output& r){ return r.calc_error; });
    if (has_calc_error) {
        ui->label_5->setText(tr("計算失敗"));
        output_img.release();
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton->setEnabled(false);
        ui->checkBox->setEnabled(false);
        //ui->label_4->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->checkBox_2->setEnabled(true);
        calc1_finished_state = false;
        ui->pushButton_8->setEnabled(false);
        ui->pushButton_6->setEnabled(true);
        ui->pushButton_10->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        refreshCanvasHistoryDialog();
        ui->label_6->setText("");
        hideCalculationProgressDialog();
        return;
    }
    int n = input_files.size();
    const bool sceneSource = ui->checkBox_2->isChecked();
    int m_layoutState = ui->cornerSelector->getStatus();
    int m_rows = ui->cornerSelector->getRows();
    int m_cols = ui->cornerSelector->getCols();
    idou_dir.resize(n-1);
/*
    for (int p = 0; p < calc_results.size(); ++p) {
        qDebug() << p << calc_results[p].vecXi << calc_results[p].vecX;
    }
*/

    if (!sceneSource && m_rows == 0 && m_cols == 0) {
        QList<ifft_thread_output> calc_results_new;
        calc_results_new.resize(n-1);
        int idx;
        for(int i = 0; i < n-1; ++i) {
            if (m_layoutState == 1) {
                if (i == 0) {
                    idx = 0;
                } else if (idou_dir[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, 0.0, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 1 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (i >= 2 && idou_dir[i-1] == 1 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 0;
                } else {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 2) {
                if (i == 0) {
                    idx = 1;
                } else if (idou_dir[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, 0.0, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, 0.0, 0.0, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 0 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 3;
                } else if (i >= 2 && idou_dir[i-1] == 0 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, 0.0, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 3) {
                if (i == 0) {
                    idx = 2;
                } else if (idou_dir[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, 0.0, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 1 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (i >= 2 && idou_dir[i-1] == 1 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 0;
                } else {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 4) {
                if (i == 0) {
                    idx = 1;
                } else if (idou_dir[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, 0.0, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 2 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else if (i >= 2 && idou_dir[i-1] == 2 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 3;
                } else {
                    std::array<double, 4> v = {0.0, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 5) {
                if (i == 0) {
                    idx = 0;
                } else if (idou_dir[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, 0.0, 0.0, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, 0.0, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 3 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (i >= 2 && idou_dir[i-1] == 3 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 0;
                } else {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, 0.0, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 6) {
                if (i == 0) {
                    idx = 3;
                } else if (idou_dir[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, 0.0, 0.0, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, 0.0, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 0 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else if (i >= 2 && idou_dir[i-1] == 0 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 3;
                } else {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, 0.0, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 7) {
                if (i == 0) {
                    idx = 2;
                } else if (idou_dir[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, 0.0, 0.0, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, 0.0, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 3 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (i >= 2 && idou_dir[i-1] == 3 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 0;
                } else {
                    std::array<double, 4> v = {calc_results[i*4+0].ssim, 0.0, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else if (m_layoutState == 8) {
                if (i == 0) {
                    idx = 3;
                } else if (idou_dir[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, 0.0, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (idou_dir[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                    std::array<double, 4> v = {0.0, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, 0.0};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                } else if (i >= 2 && idou_dir[i-1] == 2 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else if (i >= 2 && idou_dir[i-1] == 2 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 3;
                } else {
                    std::array<double, 4> v = {0.0, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                }
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            } else {
                std::array<double, 4> v = {calc_results[i*4+0].ssim, calc_results[i*4+1].ssim, calc_results[i*4+2].ssim, calc_results[i*4+3].ssim};
                int idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                calc_results_new[i] = calc_results[i*4+idx];
                idou_dir[i] = idx;
            }
        }
        calc_results = calc_results_new;
    }

    int countF = 0;
    int countF_calc = 0;
    checkTF.resize(n-1);
    checkTF_calc.resize(n-1);

    QVector<QPointF> possF;
    possF.reserve(n);
    if (sceneSource && poss.size() == n) {
        possF.push_back(QPointF(poss[0]));
    } else {
        possF.push_back(QPointF(0,0));
    }
    for(int i = 0; i < n-1; ++i) {
        possF.push_back(QPointF(possF[i].x() + calc_results[i].vecX, possF[i].y() + calc_results[i].vecY));
        if (!calc_results[i].stability || calc_results[i].ssim < ssim_th) {
            countF++;
            checkTF[i] = false;
        } else {
            checkTF[i] = true;
        }
        if (!calc_results[i].stability || calc_results[i].ssim < ssim_th_calc) {
            countF_calc++;
            checkTF_calc[i] = false;
        } else {
            checkTF_calc[i] = true;
        }
    }

    // 四捨五入
    poss.clear();
    poss.reserve(n);
    for (const auto& p : std::as_const(possF)) {
        poss.append(QPoint(static_cast<int>(std::round(p.x())),static_cast<int>(std::round(p.y()))));
    }

    int overR = ui->horizontalSlider_3->value(); // 探索範囲
    if (!sceneSource && countF_calc >= 1 && overR >= re_step) { // 再計算の実行
        ui->label_5->clear();
        updateCalculationProgress(0, 0, tr("不良箇所を再計算中"));
        calc_iFFT_rerun();
        return;
    }

    if (countF == 0) {
        ui->label_5->setText(tr("良好"));
        ryoukou = true;
    } else {
        ui->label_5->setText(tr("%1 ヶ所不良").arg(countF));
        ryoukou = false;
    }

    // キャンパスを計算する
    QVector<int> xs(n);
    std::transform(poss.cbegin(), poss.cend(), xs.begin(), [](const QPoint& p){ return p.x(); });
    QVector<int> ys(n);
    std::transform(poss.cbegin(), poss.cend(), ys.begin(), [](const QPoint& p){ return p.y(); });
    QVector<int> ws(n);
    std::transform(res_all.cbegin(), res_all.cend(), ws.begin(), [](const QSize& p){ return p.width(); });
    QVector<int> hs(n);
    std::transform(res_all.cbegin(), res_all.cend(), hs.begin(), [](const QSize& p){ return p.height(); });

    int x_min = *std::min_element(xs.cbegin(), xs.cend());
    int y_min = *std::min_element(ys.cbegin(), ys.cend());
    QVector<int> xws(n);
    for (int i = 0; i < n; ++i) xws[i] = xs[i] + ws[i];
    int x_max = *std::max_element(xws.cbegin(), xws.cend());
    QVector<int> yhs(n);
    for (int i = 0; i < n; ++i) yhs[i] = ys[i] + hs[i];
    int y_max = *std::max_element(yhs.cbegin(), yhs.cend());

    int camp_w = x_max - x_min;
    int camp_h = y_max - y_min;
    int border_W = camp_w * plus_per_camp / 100;
    int border_H = camp_h * plus_per_camp / 100;
    int border_Wc = camp_w * plus_per_camera / 100;
    int border_Hc = camp_h * plus_per_camera / 100;

    QRectF camp(x_min - border_W, y_min - border_H, camp_w + (border_W * 2), camp_h + (border_H * 2));
    QRectF cameraC(x_min - border_Wc, y_min - border_Hc, camp_w + (border_Wc * 2), camp_h + (border_Hc * 2));

    // 古いキャンパスを削除
    if (itemC) {
        delete itemC;
        itemC = nullptr;
    }

    // キャンパス描画
    itemC = scene->addRect(camp, Qt::NoPen, Qt::NoBrush);
    itemC->setZValue(0);

    // カメラ制御
    ui->graphicsView->set_Camera(cameraC);
    ui->graphicsView->fitInView(cameraC, Qt::KeepAspectRatio);


    for(int i = 0; i < n; ++i) {
        items[i]->setPos(poss[i]);
    }
    syncPossFromScene();
    recordCanvasHistory(CanvasHistoryMarkerPhase);

    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->checkBox->setEnabled(true);
    //ui->label_4->setEnabled(true);
    ui->pushButton_4->setEnabled(!output_img.empty());
    ui->pushButton_2->setEnabled(true);
    ui->checkBox_2->setEnabled(true);
    refreshCanvasHistoryDialog();

    calc1_finished_state = true;
    ui->pushButton_3->setEnabled(true);
    ui->pushButton_8->setEnabled(n > 1);
    //ui->pushButton_5->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
    ui->pushButton_10->setEnabled(true);
    ui->pushButton_11->setEnabled(true);

    hideCalculationProgressDialog();
    if (calc_finish_sig && ryoukou) {
        if (cal_opti) {
            calc_TRWS();
        } else {
            emit calcFinished();
        }
    }
}

void MainWindow::calc_finish_2()
{
    calc_results_re = watcher_re.future().results();
    const bool cancelled = calculationCancellationRequested()
                           || watcher_re.future().isCanceled()
                           || std::any_of(calc_results_re.cbegin(), calc_results_re.cend(),
                                          [](const ifft_thread_output& r) {
        return r.cancelled;
    });
    if (cancelled) {
        calc_results_re.clear();
        calc1_finished_state = calc1FinishedBeforeCalculation;
        ui->label_5->setText(tr("キャンセル"));
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton_4->setEnabled(!output_img.empty());
        ui->pushButton_2->setEnabled(calc1_finished_state);
        ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
        ui->pushButton_8->setEnabled(calc1_finished_state && input_files.size() > 1);
        ui->pushButton_6->setEnabled(true);
        ui->pushButton_10->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        finishCancelledCalculation(tr("位置合わせ計算をキャンセルしました。"));
        return;
    }
    const bool has_calc_error = std::any_of(calc_results_re.cbegin(), calc_results_re.cend(),
                                            [](const ifft_thread_output& r){ return r.calc_error; });
    if (has_calc_error) {
        ui->label_5->setText(tr("計算失敗"));
        output_img.release();
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton->setEnabled(false);
        ui->checkBox->setEnabled(true);
        //ui->label_4->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->label_6->setText("");
        ui->checkBox_2->setEnabled(true);
        calc1_finished_state = false;
        ui->pushButton_8->setEnabled(false);
        ui->pushButton_6->setEnabled(true);
        ui->pushButton_10->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        refreshCanvasHistoryDialog();
        hideCalculationProgressDialog();
        return;
    }

    int n = input_files.size();
    //int c = calc_results_re.size();

    /*
    for (int i = 0; i < c; ++i) {
        qDebug() <<
            "image" <<
            calc_results_re[i].img1_id <<
            "image" <<
            calc_results_re[i].img2_id <<
            "stab" <<
            calc_results_re[i].stability <<
            "ssim" <<
            calc_results_re[i].ssim;
    }
    */

    int m_layoutState = ui->cornerSelector->getStatus();
    int m_rows = ui->cornerSelector->getRows();
    int m_cols = ui->cornerSelector->getCols();

    QVector<int> idou_dir_new;
    idou_dir_new.resize(n-1);

    if (m_rows == 0 && m_cols == 0) {
        QList<ifft_thread_output> calc_results_new;
        calc_results_new.resize(n-1);
        int idx;
        for(int i = 0; i < n-1; ++i) {
            if (checkTF_calc[i]) {
                calc_results_new[i] = calc_results[i];
                idou_dir_new[i] = idou_dir[i];
            } else {
                std::size_t ind = std::find(i2id.begin(), i2id.end(), i) - i2id.begin();
                //qDebug() << "i ind" << i << ind;

                double ssim_max0 = 0.0;
                int ssim_max_j0N = 0;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+0].ssim;
                    if (now > ssim_max0) {
                        ssim_max_j0N = j;
                        ssim_max0 = now;
                    }
                }

                double ssim_max1 = 0.0;
                int ssim_max_j1N = 0;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+1].ssim;
                    if (now > ssim_max1) {
                        ssim_max_j1N = j;
                        ssim_max1 = now;
                    }
                }

                double ssim_max2 = 0.0;
                int ssim_max_j2N = 0;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+2].ssim;
                    if (now > ssim_max2) {
                        ssim_max_j2N = j;
                        ssim_max2 = now;
                    }
                }
                double ssim_max3 = 0.0;
                int ssim_max_j3N = 0;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+3].ssim;
                    if (now > ssim_max3) {
                        ssim_max_j3N = j;
                        ssim_max3 = now;
                    }
                }
                std::vector<int> ssim_max_j;
                ssim_max_j.resize(4);
                ssim_max_j[0] = ssim_max_j0N;
                ssim_max_j[1] = ssim_max_j1N;
                ssim_max_j[2] = ssim_max_j2N;
                ssim_max_j[3] = ssim_max_j3N;

                //qDebug() << "ssim_max" << ssim_max0 << ssim_max1 << ssim_max2 << ssim_max3;
                //qDebug() << "ssim_maxJ" << ssim_max_j[0] << ssim_max_j[1] << ssim_max_j[2] << ssim_max_j[3];

                if (m_layoutState == 1) {
                    if (i == 0) {
                        idx = 0;
                    } else if (idou_dir_new[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, 0.0, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, ssim_max1, ssim_max2, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (i >= 2 && idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 0;
                    } else {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, ssim_max2, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 2) {
                    if (i == 0) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, 0.0, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, 0.0, 0.0, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 3;
                    } else if (i >= 2 && idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 1;
                    } else {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, 0.0, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 3) {
                    if (i == 0) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, 0.0, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, ssim_max1, ssim_max2, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (i >= 2 && idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 0;
                    } else {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, ssim_max2, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 4) {
                    if (i == 0) {
                        idx = 1;
                    } else if (idou_dir_new[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, 0.0, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, ssim_max1, ssim_max2, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 1;
                    } else if (i >= 2 && idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 3;
                    } else {
                        std::array<double, 4> v = {0.0, ssim_max1, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 5) {
                    if (i == 0) {
                        idx = 0;
                    } else if (idou_dir_new[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, 0.0, 0.0, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, 0.0, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (i >= 2 && idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 0;
                    } else {
                        std::array<double, 4> v = {ssim_max0, 0.0, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 6) {
                    if (i == 0) {
                        idx = 3;
                    } else if (idou_dir_new[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, 0.0, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, 0.0, 0.0, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 3;
                    } else if (i >= 2 && idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 1;
                    } else {
                        std::array<double, 4> v = {ssim_max0, ssim_max1, 0.0, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 7) {
                    if (i == 0) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {ssim_max0, 0.0, 0.0, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, 0.0, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (i >= 2 && idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 0;
                    } else {
                        std::array<double, 4> v = {ssim_max0, 0.0, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else if (m_layoutState == 8) {
                    if (i == 0) {
                        idx = 3;
                    } else if (idou_dir_new[i-1] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, 0.0, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (idou_dir_new[i-1] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc) {
                        std::array<double, 4> v = {0.0, ssim_max1, ssim_max2, 0.0};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    } else if (i >= 2 && idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 1;
                    } else if (i >= 2 && idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 3;
                    } else {
                        std::array<double, 4> v = {0.0, ssim_max1, ssim_max2, ssim_max3};
                        idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    }
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                } else {
                    std::array<double, 4> v = {ssim_max0, ssim_max1, ssim_max2, ssim_max3};
                    idx = int(std::distance(v.begin(), std::max_element(v.begin(), v.end())));
                    calc_results_new[i] = calc_results_re[ind*ovc*4+ssim_max_j[idx]*4+idx];
                    idou_dir_new[i] = idx;
                }
            }
        }
        calc_results = calc_results_new;

        /*
        for (int i = 0; i < n-1; ++i) {
            if (calc_results[i].ssim < 0.4) {
                qDebug() <<
                    "image" <<
                    calc_results_new[i].img1_id <<
                    "image" <<
                    calc_results_new[i].img2_id <<
                    "stab" <<
                    calc_results_new[i].stability <<
                    "ssim" <<
                    calc_results_new[i].ssim;
            }
        }
        */
    } else {
        QList<ifft_thread_output> calc_results_new;
        calc_results_new.resize(n-1);
        int idx;
        for(int i = 0; i < n-1; ++i) {
            if (checkTF_calc[i]) {
                calc_results_new[i] = calc_results[i];
            } else {
                std::size_t ind = std::find(i2id.begin(), i2id.end(), i) - i2id.begin();

                double ssim_max = 0.0;
                int ssim_max_j = 0;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc+j].ssim;
                    if (now > ssim_max) {
                        ssim_max_j = j;
                        ssim_max = now;
                    }
                }

                calc_results_new[i] = calc_results_re[ind*ovc+ssim_max_j];
            }
            /*
            qDebug() << "i:" << i <<
                "old_ssim:" << calc_results[i].ssim <<
                "new_ssim" << calc_results_new[i].ssim;
            */
        }
        calc_results = calc_results_new;
    }

    int countF = 0;
    int countF_calc = 0;
    checkTF.resize(n-1);
    checkTF_calc.resize(n-1);

    QVector<QPointF> possF;
    possF.reserve(n);
    possF.push_back(QPointF(0,0));

    for(int i = 0; i < n-1; ++i) {
        possF.push_back(QPointF(possF[i].x() + calc_results[i].vecX, possF[i].y() + calc_results[i].vecY));
        if (!calc_results[i].stability || calc_results[i].ssim < ssim_th) {
            countF++;
            checkTF[i] = false;
        } else {
            checkTF[i] = true;
        }
        if (!calc_results[i].stability || calc_results[i].ssim < ssim_th_calc) {
            countF_calc++;
            checkTF_calc[i] = false;
        } else {
            checkTF_calc[i] = true;
        }
    }

    // 四捨五入
    poss.clear();
    poss.reserve(n);
    for (const auto& p : std::as_const(possF)) {
        poss.append(QPoint(static_cast<int>(std::round(p.x())),static_cast<int>(std::round(p.y()))));
    }

    if (countF == 0) {
        ui->label_5->setText(tr("良好"));
        ryoukou = true;
    } else {
        ui->label_5->setText(tr("%1 ヶ所不良").arg(countF));
        ryoukou = false;
    }

    // キャンパスを計算する
    QVector<int> xs(n);
    std::transform(poss.cbegin(), poss.cend(), xs.begin(), [](const QPoint& p){ return p.x(); });
    QVector<int> ys(n);
    std::transform(poss.cbegin(), poss.cend(), ys.begin(), [](const QPoint& p){ return p.y(); });
    QVector<int> ws(n);
    std::transform(res_all.cbegin(), res_all.cend(), ws.begin(), [](const QSize& p){ return p.width(); });
    QVector<int> hs(n);
    std::transform(res_all.cbegin(), res_all.cend(), hs.begin(), [](const QSize& p){ return p.height(); });

    int x_min = *std::min_element(xs.cbegin(), xs.cend());
    int y_min = *std::min_element(ys.cbegin(), ys.cend());
    QVector<int> xws(n);
    for (int i = 0; i < n; ++i) xws[i] = xs[i] + ws[i];
    int x_max = *std::max_element(xws.cbegin(), xws.cend());
    QVector<int> yhs(n);
    for (int i = 0; i < n; ++i) yhs[i] = ys[i] + hs[i];
    int y_max = *std::max_element(yhs.cbegin(), yhs.cend());

    int camp_w = x_max - x_min;
    int camp_h = y_max - y_min;
    int border_W = camp_w * plus_per_camp / 100;
    int border_H = camp_h * plus_per_camp / 100;
    int border_Wc = camp_w * plus_per_camera / 100;
    int border_Hc = camp_h * plus_per_camera / 100;

    QRectF camp(x_min - border_W, y_min - border_H, camp_w + (border_W * 2), camp_h + (border_H * 2));
    QRectF cameraC(x_min - border_Wc, y_min - border_Hc, camp_w + (border_Wc * 2), camp_h + (border_Hc * 2));

    // 古いキャンパスを削除
    if (itemC) {
        delete itemC;
        itemC = nullptr;
    }

    // キャンパス描画
    itemC = scene->addRect(camp, Qt::NoPen, Qt::NoBrush);
    itemC->setZValue(0);

    // カメラ制御
    ui->graphicsView->set_Camera(cameraC);
    ui->graphicsView->fitInView(cameraC, Qt::KeepAspectRatio);

    for(int i = 0; i < n; ++i) {
        items[i]->setPos(poss[i]);
    }
    syncPossFromScene();
    recordCanvasHistory(CanvasHistoryMarkerPhase);

    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->checkBox->setEnabled(true);
    //ui->label_4->setEnabled(true);
    ui->pushButton_4->setEnabled(!output_img.empty());
    ui->pushButton_2->setEnabled(true);
    calc1_finished_state = true;
    ui->pushButton_3->setEnabled(true);
    ui->pushButton_8->setEnabled(n > 1);
    //ui->pushButton_5->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
    ui->pushButton_10->setEnabled(true);
    ui->pushButton_11->setEnabled(true);
    ui->checkBox_2->setEnabled(true);
    refreshCanvasHistoryDialog();

    hideCalculationProgressDialog();
    if (calc_finish_sig && ryoukou) {
        if (cal_opti) {
            calc_TRWS();
        } else {
            emit calcFinished();
        }
    }
}

void MainWindow::png_export() {

    if (!output_file.isEmpty()) {
        cli_exp_image();
    } else {
        if (input_files.size() == 0 || output_img.empty()) {
            showNonModalMessage(QMessageBox::Warning, tr("PNG export"),
                                tr("出力できる画像がありません。"));
            return;
        }

        QFileInfo fi(input_files[0]);
        QDir dir = fi.dir();

        QString newName = "stitched_" + fi.completeBaseName() + ".png";
        QString initialPath = dir.filePath(newName);

        auto* dialog = new QFileDialog(this, tr("保存"), initialPath,
                                       tr("PNG画像 (*.png);;すべてのファイル (*)"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setDefaultSuffix(QStringLiteral("png"));
        dialog->setOption(
            QFileDialog::DontUseNativeDialog,
            !image_stitcher::platform::useNativeFileDialogs());
        dialog->setOption(QFileDialog::DontConfirmOverwrite, true);
        dialog->setModal(false);
        dialog->setWindowModality(Qt::NonModal);
        trackDialogSize(dialog, QStringLiteral("pngExport"), QSize(900, 600));
        connect(dialog, &QFileDialog::fileSelected,
                this, [this](const QString& path) {
            if (!QFileInfo::exists(path)) {
                savePngToPath(path);
                return;
            }

            auto* confirmation = new QMessageBox(
                QMessageBox::Warning, tr("上書き確認"),
                tr("ファイルは既に存在します。上書きしますか？\n%1")
                    .arg(QDir::toNativeSeparators(path)),
                QMessageBox::Yes | QMessageBox::No, this);
            confirmation->setAttribute(Qt::WA_DeleteOnClose);
            confirmation->setDefaultButton(QMessageBox::No);
            confirmation->setModal(false);
            confirmation->setWindowModality(Qt::NonModal);
            connect(confirmation, &QDialog::finished, this,
                    [this, path](int result) {
                if (result == QMessageBox::Yes) {
                    savePngToPath(path);
                }
            });
            confirmation->show();
        });
        dialog->show();
    }
}

void MainWindow::set_output_path(QString out_p)
{
    output_file = out_p;
};

void MainWindow::cli_exp_image() {
    if (input_files.size() == 0 || output_img.empty()) {
        showNonModalMessage(QMessageBox::Warning, tr("PNG export"),
                            tr("出力できる画像がありません。"));
        return;
    }
    if (savePngToPath(output_file) && closeTFm) {
        QCoreApplication::quit();
    }
}

bool MainWindow::savePngToPath(const QString& path)
{
    if (path.isEmpty() || output_img.empty()) {
        return false;
    }

    const QImage image(output_img.data, output_img.cols, output_img.rows,
                       output_img.step, QImage::Format_ARGB32);
    if (!image.save(path, "PNG")) {
        showNonModalMessage(
            QMessageBox::Critical, tr("保存エラー"),
            tr("画像を保存できませんでした。\n"
               "出力先パスを確認してください。\n\n"
               "Path: %1").arg(path));
        return false;
    }

    if (statusMessageLabel) {
        statusMessageLabel->setText(
            tr("PNGの保存が完了しました: %1")
                .arg(QDir::toNativeSeparators(path)));
    }
    return true;
}


// SSIM 各スレッドのデータ構造化
static return_struct1 SSIM_calc_oneshot_struct(const SSIM_TaskInput& in)
{
    return_struct1 r;
    r.x = in.pos2.x() - in.pos1.x() + in.dx;
    r.y = in.pos2.y() - in.pos1.y() + in.dy;
    r.score = SSIM_calc_oneshot(in); // double を返す純計算
    return r;
}

// スコア最大だけ保持
static void SSIM_calc_reduceMax(return_struct1& acc, const return_struct1& v)
{
    if (v.score > acc.score) acc = v;
}

// 全画像をGUI上で配列させる
void MainWindow::photo_Arrange()
{
    int m_layoutState = ui->cornerSelector->getStatus();
    int m_rows = ui->cornerSelector->getRows();
    int m_cols = ui->cornerSelector->getCols();

    if (m_layoutState == -1) return;
    if (m_rows == -1 || m_cols == -1) return;

    // x方向重なり率
    int overX = ui->horizontalSlider->value();
    // y方向重なり率
    int overY = ui->horizontalSlider_2->value();

    // 画像の枚数
    const int n = input_files.size();

    if (n == 0) return;
    if (n == 1) {
        items[0]->setPos(0, 0);
        recordCanvasHistory();
        return;
    }

    // Autoの場合
    if (m_rows == 0 || m_cols == 0) {
        if (m_layoutState == 1 || m_layoutState == 3 || m_layoutState == 5 || m_layoutState == 7) {
            int r = (int)std::sqrt(n);
            int q = (n + r - 1) / r;
            m_cols = q;
            m_rows = (n + m_cols - 1) / m_cols;
        } else if (m_layoutState == 2 || m_layoutState == 4 || m_layoutState == 6 || m_layoutState == 8) {
            int r = (int)std::sqrt(n);
            int q = (n + r - 1) / r;
            m_rows = q;
            m_cols = (n + m_rows - 1) / m_rows;
        }
    }

    // 全画像の解像度を取得する
    res_all.resize(n);
    for (int i = 0; i < n; ++i) {
        res_all[i] = items[i]->pixmap().size(); // 画像の解像度を取得
    }

    // 全画像の位置を計算する
    pos_all.resize(n);
    pos_all[0] = QPoint(0,0);

    if (orikaeshi) {
        if (m_layoutState == 1) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_cols;
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                } else if (q % 2 == 0) { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                } else { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 2) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_rows;
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                } else if (q % 2 == 0) { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                } else { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 3) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_cols;
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                } else if (q % 2 == 0) { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                } else { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 4) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_rows;
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                } else if (q % 2 == 0) { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                } else { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 5) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_cols;
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                } else if (q % 2 == 0) { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                } else { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 6) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_rows;
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                } else if (q % 2 == 0) { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                } else { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 7) {
            for (int i = 1; i < n; ++i) {
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                int q = i / m_cols;
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                } else if (q % 2 == 0) { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                } else { // →
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        } else if (m_layoutState == 8) {
            for (int i = 1; i < n; ++i) {
                //qDebug() << "photo " << i;
                QSize res1 = res_all[i-1];
                QSize res2 = res_all[i];
                //qDebug() << "res1_W " << res1.width() << " res1_H " << res1.height();
                //qDebug() << "res2_W " << res2.width() << " res2_H " << res2.height();
                int q = i / m_rows;
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // ←
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                } else if (q % 2 == 0) { // ↑
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                } else { // ↓
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                }
                pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
            }
        }
    } else {
        if (m_layoutState == 1) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↓
                    QSize res1 = res_all[i-m_cols];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_cols].x() + x, pos_all[i-m_cols].y() + y);
                } else { // →
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 2) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // →
                    QSize res1 = res_all[i-m_rows];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_rows].x() + x, pos_all[i-m_rows].y() + y);
                } else { // ↓
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 3) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↓
                    QSize res1 = res_all[i-m_cols];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_cols].x() + x, pos_all[i-m_cols].y() + y);
                } else { // ←
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 4) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // ←
                    QSize res1 = res_all[i-m_rows];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_rows].x() + x, pos_all[i-m_rows].y() + y);
                } else { // ↓
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = res1.height() - overlap;
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 5) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↑
                    QSize res1 = res_all[i-m_cols];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_cols].x() + x, pos_all[i-m_cols].y() + y);
                } else { // →
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 6) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // →
                    QSize res1 = res_all[i-m_rows];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = res1.width() - overlap;
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_rows].x() + x, pos_all[i-m_rows].y() + y);
                } else { // ↑
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 7) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_cols;
                int x, y;
                if (r == 0) { // ↑
                    QSize res1 = res_all[i-m_cols];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_cols].x() + x, pos_all[i-m_cols].y() + y);
                } else { // ←
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        } else if (m_layoutState == 8) {
            for (int i = 1; i < n; ++i) {
                int r = i % m_rows;
                int x, y;
                if (r == 0) { // ←
                    QSize res1 = res_all[i-m_rows];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.width(), res2.width()) * overX / 100;
                    x = overlap - res2.width();
                    y = (res1.height() - res2.height()) / 2;
                    pos_all[i] = QPoint(pos_all[i-m_rows].x() + x, pos_all[i-m_rows].y() + y);
                } else { // ↑
                    QSize res1 = res_all[i-1];
                    QSize res2 = res_all[i];
                    int overlap = std::min(res1.height(), res2.height()) * overY / 100;
                    y = overlap - res2.height();
                    x = (res1.width() - res2.width()) / 2;
                    pos_all[i] = QPoint(pos_all[i-1].x() + x, pos_all[i-1].y() + y);
                }
            }
        }
    }

    // キャンパスを計算する
    QVector<int> xs(n);
    std::transform(pos_all.cbegin(), pos_all.cend(), xs.begin(), [](const QPoint& p){ return p.x(); });
    QVector<int> ys(n);
    std::transform(pos_all.cbegin(), pos_all.cend(), ys.begin(), [](const QPoint& p){ return p.y(); });
    QVector<int> ws(n);
    std::transform(res_all.cbegin(), res_all.cend(), ws.begin(), [](const QSize& p){ return p.width(); });
    QVector<int> hs(n);
    std::transform(res_all.cbegin(), res_all.cend(), hs.begin(), [](const QSize& p){ return p.height(); });

    int x_min = *std::min_element(xs.cbegin(), xs.cend());
    int y_min = *std::min_element(ys.cbegin(), ys.cend());
    QVector<int> xws(n);
    for (int i = 0; i < n; ++i) xws[i] = xs[i] + ws[i];
    int x_max = *std::max_element(xws.cbegin(), xws.cend());
    QVector<int> yhs(n);
    for (int i = 0; i < n; ++i) yhs[i] = ys[i] + hs[i];
    int y_max = *std::max_element(yhs.cbegin(), yhs.cend());

    int camp_w = x_max - x_min;
    int camp_h = y_max - y_min;
    int border_W = camp_w * plus_per_camp / 100;
    int border_H = camp_h * plus_per_camp / 100;
    int border_Wc = camp_w * plus_per_camera / 100;
    int border_Hc = camp_h * plus_per_camera / 100;

    QRectF camp(x_min - border_W, y_min - border_H, camp_w + (border_W * 2), camp_h + (border_H * 2));
    QRectF cameraC(x_min - border_Wc, y_min - border_Hc, camp_w + (border_Wc * 2), camp_h + (border_Hc * 2));

    // 古いキャンパスを削除
    if (itemC) {
        delete itemC;
        itemC = nullptr;
    }

    // キャンパス描画
    itemC = scene->addRect(camp, Qt::NoPen, Qt::NoBrush);
    itemC->setZValue(0);

    // カメラ制御
    ui->graphicsView->set_Camera(cameraC);
    ui->graphicsView->fitInView(cameraC, Qt::KeepAspectRatio);

    // 全画像を配置する
    for (int i = 0; i < n; ++i) {
        items[i]->setPos(pos_all[i]);
    }
    recordCanvasHistory();
}

void MainWindow::show_detail()
{
    const int n = input_files.size();
    if (n == 0) {
        showNonModalMessage(QMessageBox::Warning, tr("詳細"),
                            tr("表示できるデータがありません。"));
        return;
    }
    if (n < 2 || calc_results.size() < (n - 1) || checkTF.size() < (n - 1)) {
        showNonModalMessage(
            QMessageBox::Warning, tr("詳細"),
            tr("表示するデータがありません。先に位置合わせ計算を実行してください。"));
        return;
    }

    auto* model = new QStandardItemModel(this);
    model->setColumnCount(7);
    model->setHorizontalHeaderLabels({tr("画像1"), tr("画像2"), tr("計算回数"),
                                      tr("安定性"), tr("位相相関スコア"),
                                      tr("SSIM"), tr("品質")});
    model->setRowCount(n-1);

    for (int i = 0; i < n-1; ++i) {
        // 画像1（表示は文字列、ソート用は int）
        {
            auto* it = new QStandardItem(QString::number(i+1));
            it->setData(i+1, Qt::UserRole);
            model->setItem(i, 0, it);
        }
        // 画像2
        {
            auto* it = new QStandardItem(QString::number(i+2));
            it->setData(i+2, Qt::UserRole);
            model->setItem(i, 1, it);
        }
        // 計算回数（int）
        {
            int v = calc_results[i].loop_num;
            auto* it = new QStandardItem(QString::number(v));
            it->setData(v, Qt::UserRole);
            model->setItem(i, 2, it);
        }
        // 安定性（表示は文字列、ソート用は 0/1）
        {
            bool st = calc_results[i].stability;
            auto* it = new QStandardItem(st ? tr("安定") : tr("不安定"));
            it->setData(st ? 1 : 0, Qt::UserRole);
            it->setData(st ? QStringLiteral("stable")
                           : QStringLiteral("unstable"),
                        Qt::UserRole + 1);
            model->setItem(i, 3, it);
        }
        // 位相相関スコア（double）
        {
            double v = calc_results[i].score;
            auto* it = new QStandardItem(QString::number(v, 'f', 4));
            it->setData(v, Qt::UserRole);
            model->setItem(i, 4, it);
        }
        // SSIM（double）
        {
            double v = calc_results[i].ssim;
            auto* it = new QStandardItem(QString::number(v, 'f', 4));
            it->setData(v, Qt::UserRole);
            model->setItem(i, 5, it);
        }
        // 品質（表示は文字列、ソート用は 0/1）
        {
            bool ok = checkTF[i];
            auto* it = new QStandardItem(ok ? tr("良") : tr("不良"));
            it->setData(ok ? 1 : 0, Qt::UserRole);
            it->setData(ok ? QStringLiteral("good")
                           : QStringLiteral("bad"),
                        Qt::UserRole + 1);
            model->setItem(i, 6, it);
        }
    }
    auto* dlg = new Detail_Dialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(model);
    dlg->setModal(false);
    trackDialogSize(dlg, QStringLiteral("alignmentDetails"),
                    QSize(620, 600));
    dlg->show();
}

void MainWindow::show_detail_least_squares()
{
    if (leastSquaresDetails.empty()) {
        showNonModalMessage(
            QMessageBox::Warning, tr("最小二乗法の詳細"),
            tr("表示するデータがありません。先に位置合わせ最適化（最小二乗法）を実行してください。"));
        return;
    }

    auto* model = new QStandardItemModel(this);
    model->setColumnCount(13);
    model->setHorizontalHeaderLabels({
        tr("画像1"), tr("画像2"), tr("状態"), tr("計算回数"), tr("安定性"),
        tr("位相相関スコア"), tr("測定SSIM"), tr("計算後SSIM"),
        tr("測定dx"), tr("測定dy"), tr("最適dx"), tr("最適dy"), tr("残差")
    });
    model->setRowCount(static_cast<int>(leastSquaresDetails.size()));

    auto setInt = [model](int row, int col, int value) {
        auto* item = new QStandardItem(QString::number(value));
        item->setData(value, Qt::UserRole);
        model->setItem(row, col, item);
    };
    auto setDouble = [model](int row, int col, double value, int digits = 4) {
        auto* item = new QStandardItem(QString::number(value, 'f', digits));
        item->setData(value, Qt::UserRole);
        model->setItem(row, col, item);
    };
    auto setText = [model](int row, int col, const QString& text,
                           const QString& translationKey = QString()) {
        auto* item = new QStandardItem(text);
        item->setData(text, Qt::UserRole);
        if (!translationKey.isEmpty()) {
            item->setData(translationKey, Qt::UserRole + 1);
        }
        model->setItem(row, col, item);
    };

    for (int row = 0; row < static_cast<int>(leastSquaresDetails.size()); ++row) {
        const LeastSquaresPairDetail& detail = leastSquaresDetails[row];
        setInt(row, 0, detail.img1 + 1);
        setInt(row, 1, detail.img2 + 1);
        QString statusKey;
        if (detail.status == tr("計算失敗")) {
            statusKey = QStringLiteral("calculation_failed");
        } else if (detail.status == tr("しきい値未満")) {
            statusKey = QStringLiteral("below_threshold");
        } else if (detail.status == tr("採用")) {
            statusKey = QStringLiteral("accepted");
        } else if (detail.status == tr("除外")) {
            statusKey = QStringLiteral("excluded");
        }
        setText(row, 2, detail.status, statusKey);
        setInt(row, 3, detail.loop_num);
        setText(row, 4, detail.stability ? tr("安定") : tr("不安定"),
                detail.stability ? QStringLiteral("stable")
                                 : QStringLiteral("unstable"));
        setDouble(row, 5, detail.score);
        setDouble(row, 6, detail.measuredSsim);
        setDouble(row, 7, detail.optimizedSsim);
        setDouble(row, 8, detail.measuredDx, 2);
        setDouble(row, 9, detail.measuredDy, 2);
        setDouble(row, 10, detail.optimizedDx, 2);
        setDouble(row, 11, detail.optimizedDy, 2);
        setDouble(row, 12, detail.residual, 2);
    }

    auto* dlg = new Detail_Dialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(model);
    dlg->setModal(false);
    trackDialogSize(dlg, QStringLiteral("leastSquaresDetails"),
                    QSize(620, 600));
    dlg->show();
}


void MainWindow::posi_lock(bool checked) {
    applySceneMoveMode(!checked);

    if (checked) {
        ui->pushButton_1->setEnabled(false);
        ui->groupBox_3->setEnabled(false);
        ui->groupBox_4->setEnabled(false);
    } else {
        ui->pushButton_1->setEnabled(true);
        ui->groupBox_3->setEnabled(true);
        ui->groupBox_4->setEnabled(true);
    }
}

void MainWindow::make_image()
{
    if (isAlignmentOrOptimizationRunning()) {
        return;
    }

    const int n = input_files.size();
    if (n == 0) {
        showNonModalMessage(QMessageBox::Warning, tr("画像作成"),
                            tr("出力できる画像がありません。"));
        return;
    }

    const QVector<QPoint> canvasPositions = readScenePositions();

    if (imgs.size() != n || canvasPositions.size() != n) {
        showNonModalMessage(QMessageBox::Warning, tr("画像作成"),
                            tr("先に位置合わせ計算を実行してください。"));
        return;
    }

    readMergeSettingsDialog();
    showCalculationProgressDialog(tr("画像作成"), QStringLiteral("CPU"));

    poss = canvasPositions;
    pos_all = canvasPositions;
    imageMakeSourcePositions = canvasPositions;
    invalidateCreatedImage();

    ui->label_6->clear();
    ui->pushButton_2->setEnabled(false);
    ui->pushButton_11->setEnabled(false);

    // 別スレッドに渡すために必要データをコピー
    auto imgs_copy = imgs;
    const auto poss_copy = canvasPositions;
    const ImageMergeMode mergeMode = imageMergeSettings.mode;
    const auto cancellation = calculationCancelRequested;

    // ワーカースレッドで実行
    image_make_Watcher.setFuture(QtConcurrent::run([imgs_copy, poss_copy, mergeMode, cancellation]() -> cv::Mat {
        return mergeImagesForExport(imgs_copy, poss_copy, mergeMode, cancellation);
    }));
}

// 全体最適化ボタン_old
/*
void MainWindow::calc_TRWS()
{
    // 画像の数
    const int N = input_files.size();
    // 探索範囲を設定
    const int radi = 2; // 半径1pixを探索する。
    const int K = ((radi * 2) + 1) * ((radi * 2) + 1); // 状態数

    // 各状態のx,yのシフト量を計算
    QVector<QPoint> shifts;
    shifts.reserve(K);
    for (int y = -radi; y <= radi; ++y) { // 行優先
        for (int x = -radi; x <= radi; ++x) {
            shifts.push_back(QPoint(x,y));
        }
    }

    // エッジを構築
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<double>> pairCosts;
    qDebug() << "edge lists (i,j)";
    for (int i = 0; i < N; ++i) { // 1枚目画像を選択
        cv::Mat img1 = imgs[i];
        QSize res1 = res_all[i];
        QPoint pos1 = poss[i];
        cv::Mat1b mask1 = ImageUtils::alphaMaskFromBGRA(img1, 0.5);

        for (int j = 0; j < N; ++j) { // 2枚目画像を選択
            // 1枚目と2枚目が同じ場合
            if (j <= i) continue; // 次のjへ
            cv::Mat img2 = imgs[j];
            QSize res2 = res_all[j];
            QPoint pos2 = poss[j];
            cv::Mat1b mask2 = ImageUtils::alphaMaskFromBGRA(img2, 0.5);

            // デフォルト位置で重複が存在するか確認する
            const int x_t1 = std::max(pos1.x()+res1.width(),pos2.x()+res2.width())
                       - std::min(pos1.x(),pos2.x());
            const int x_t2 = res1.width() + res2.width();
            const int x_t3 = x_t2 - x_t1 - radi;
            if (x_t3 <= 0) continue; // 次のjへ
            const int y_t1 = std::max(pos1.y()+res1.height(),pos2.y()+res2.height())
                             - std::min(pos1.y(),pos2.y());
            const int y_t2 = res1.height() + res2.height();
            const int y_t3 = y_t2 - y_t1 - radi;
            if (y_t3 <= 0) continue; // 次のjへ

            std::vector<bool> TF_temp(K * K, false); // エッジを張れるかどうか
            std::vector<double> nowCost(K * K, 0.0); // そのエッジにおけるコスト
            for (int k1 = 0; k1 < K; ++k1) { // 1枚目の状態選択
                const int x1 = pos1.x() + shifts[k1].x();
                const int y1 = pos1.y() + shifts[k1].y();
                for (int k2 = 0; k2 < K; ++k2) { // 2枚目の状態選択
                    const int x2 = pos2.x() + shifts[k2].x();
                    const int y2 = pos2.y() + shifts[k2].y();
                    // キャンパス上の座標を計算する
                    const int x_c = std::min(x1,x2);
                    const int x1c = x1 - x_c;
                    const int x2c = x2 - x_c;
                    const int y_c = std::min(y1,y2);
                    const int y1c = y1 - y_c;
                    const int y2c = y2 - y_c;
                    // キャンパス作成（全てfalseで初期化）
                    const int cam_hei = std::max(y1c+res1.height(),y2c+res2.height());
                    const int cam_wid = std::max(x1c+res1.width(),x2c+res2.width());
                    cv::Mat1b camp1(cam_hei, cam_wid, uchar(0));
                    mask1.copyTo(camp1(cv::Rect(x1c, y1c, mask1.cols, mask1.rows)));
                    cv::Mat1b camp2(cam_hei, cam_wid, uchar(0));
                    mask2.copyTo(camp2(cv::Rect(x2c, y2c, mask2.cols, mask2.rows)));
                    // 重なり領域検出
                    cv::Mat1b andImg;
                    cv::bitwise_and(camp1, camp2, andImg);
                    // 重なりピクセル数をカウント
                    int trueCount = cv::countNonZero(andImg);
                    if (trueCount >= edge_th) {
                        TF_temp[k1 * K + k2] = true;
                        // 画像を3ch化
                        cv::Mat img1_3ch, img2_3ch;
                        cv::cvtColor(img1, img1_3ch, cv::COLOR_BGRA2BGR);
                        cv::cvtColor(img2, img2_3ch, cv::COLOR_BGRA2BGR);
                        // キャンパスに貼り付け
                        cv::Mat3b camp1_3ch = cv::Mat3b::zeros(cam_hei, cam_wid);
                        img1_3ch.copyTo(camp1_3ch(cv::Rect(x1c, y1c, img1_3ch.cols, img1_3ch.rows)));
                        cv::Mat3b camp2_3ch = cv::Mat3b::zeros(cam_hei, cam_wid);
                        img2_3ch.copyTo(camp2_3ch(cv::Rect(x2c, y2c, img2_3ch.cols, img2_3ch.rows)));
                        // 重なりピクセルのみ抽出
                        std::vector<double> im1_ch0, im1_ch1, im1_ch2;
                        ImageUtils::extractMaskedChannels(andImg,camp1_3ch,im1_ch0,im1_ch1,im1_ch2);
                        std::vector<double> im2_ch0, im2_ch1, im2_ch2;
                        ImageUtils::extractMaskedChannels(andImg,camp2_3ch,im2_ch0,im2_ch1,im2_ch2);
                        // コストを計算
                        const double mse0 = ImageUtils::calcMSE(im1_ch0, im2_ch0);
                        const double mse1 = ImageUtils::calcMSE(im1_ch1, im2_ch1);
                        const double mse2 = ImageUtils::calcMSE(im1_ch2, im2_ch2);
                        const double mse_mean = (mse0 + mse1 + mse2) / 3;
                        nowCost[k1 * K + k2] = mse_mean;
                    }
                }
            }
            int falseCount = std::count(TF_temp.begin(), TF_temp.end(), false);
            if (falseCount > 0) continue; // 次のjへ
            // 変数格納
            edges.push_back({i,j});
            qDebug() << i << j;
            pairCosts.push_back(nowCost);
        }
    }
    // order は row-major そのまま
    std::vector<int> order(N);
    for (int i = 0; i < N; ++i) order[i] = i;

    TRWS::Options opts;
    opts.maxIter = 200;
    opts.tol = 1e-8;
    opts.stallIters = 10;

    TRWS solver(N, K, edges, pairCosts, order, opts);
    TRWSResult result = solver.run();

    qDebug() << "iterations = " << result.iterations;
    qDebug() << "energy     = " << result.energy;
    qDebug() << "reward     = " << result.reward;

    qDebug() << "labels:";
    for (int l : result.labels) {
        qDebug() << "x =" << shifts[l].x() << "  y =" << shifts[l].y();
    }

    qDebug() << "lower bound history:";
    for (double v : result.lowerBoundHistory) {
        qDebug() << v;
    }
}
*/
/*
namespace
{
    struct PrecomputedImage
    {
        cv::Mat4b bgra;   // 元画像
        cv::Mat3b bgr;    // 前処理済み
        cv::Mat1b mask;   // 前処理済み
        QPoint pos;
        QSize  res;
    };

    struct PairTask
    {
        int i;
        int j;
    };

    struct PairResult
    {
        bool valid = false;
        int i = -1;
        int j = -1;
        std::vector<double> costs;
    };

    inline QRect shiftedRect(const QPoint& pos, const QPoint& shift, const QSize& sz)
    {
        return QRect(pos.x() + shift.x(),
                     pos.y() + shift.y(),
                     sz.width(),
                     sz.height());
    }

    inline bool defaultMayOverlap(const PrecomputedImage& a,
                                  const PrecomputedImage& b,
                                  int radi)
    {
        const int x_t1 = std::max(a.pos.x() + a.res.width(),  b.pos.x() + b.res.width())
        - std::min(a.pos.x(), b.pos.x());
        const int x_t2 = a.res.width() + b.res.width();
        const int x_t3 = x_t2 - x_t1 - radi;
        if (x_t3 <= 0) return false;

        const int y_t1 = std::max(a.pos.y() + a.res.height(), b.pos.y() + b.res.height())
                         - std::min(a.pos.y(), b.pos.y());
        const int y_t2 = a.res.height() + b.res.height();
        const int y_t3 = y_t2 - y_t1 - radi;
        if (y_t3 <= 0) return false;

        return true;
    }


    // 重なり領域の MSE を直接計算
    // 戻り値:
    //   false = edge_th 未満
    //   true  = 有効で mseOut に平均MSEを返す
    bool computePairCostDirect(const PrecomputedImage& A,
                               const PrecomputedImage& B,
                               const QPoint& shiftA,
                               const QPoint& shiftB,
                               int edge_th,
                               double& mseOut)
    {
        const QRect rectA = shiftedRect(A.pos, shiftA, A.res);
        const QRect rectB = shiftedRect(B.pos, shiftB, B.res);
        const QRect inter = rectA.intersected(rectB);

        if (inter.isEmpty()) {
            return false;
        }

        const int ax0 = inter.x() - rectA.x();
        const int ay0 = inter.y() - rectA.y();
        const int bx0 = inter.x() - rectB.x();
        const int by0 = inter.y() - rectB.y();

        const int w = inter.width();
        const int h = inter.height();

        // ROI取得
        const cv::Rect roiA(ax0, ay0, w, h);
        const cv::Rect roiB(bx0, by0, w, h);

        const cv::Mat1b maskAroi = A.mask(roiA);
        const cv::Mat1b maskBroi = B.mask(roiB);

        // overlap mask
        cv::Mat1b overlapMask;
        cv::bitwise_and(maskAroi, maskBroi, overlapMask);

        const int overlapCount = cv::countNonZero(overlapMask);
        if (overlapCount < edge_th) {
            return false;
        }

        const cv::Mat3b imgAroi = A.bgr(roiA);
        const cv::Mat3b imgBroi = B.bgr(roiB);

        double sum0 = 0.0, sum1 = 0.0, sum2 = 0.0;

        for (int y = 0; y < h; ++y) {
            const uchar* m = overlapMask.ptr<uchar>(y);
            const cv::Vec3b* pA = imgAroi.ptr<cv::Vec3b>(y);
            const cv::Vec3b* pB = imgBroi.ptr<cv::Vec3b>(y);

            for (int x = 0; x < w; ++x) {
                if (!m[x]) continue;

                const double d0 = double(pA[x][0]) - double(pB[x][0]);
                const double d1 = double(pA[x][1]) - double(pB[x][1]);
                const double d2 = double(pA[x][2]) - double(pB[x][2]);

                sum0 += d0 * d0;
                sum1 += d1 * d1;
                sum2 += d2 * d2;
            }
        }

        const double mse0 = sum0 / overlapCount;
        const double mse1 = sum1 / overlapCount;
        const double mse2 = sum2 / overlapCount;
        mseOut = (mse0 + mse1 + mse2) / 3.0;
        return true;
    }

    PairResult processOnePair(const PairTask& task,
                              const std::vector<PrecomputedImage>& pre,
                              const QVector<QPoint>& shifts,
                              int K,
                              int radi,
                              int edge_th)
    {
        const int i = task.i;
        const int j = task.j;

        const auto& A = pre[i];
        const auto& B = pre[j];

        PairResult result;
        result.i = i;
        result.j = j;

        if (!defaultMayOverlap(A, B, radi)) {
            return result;
        }

        result.costs.resize(K * K);

        for (int k1 = 0; k1 < K; ++k1) {
            for (int k2 = 0; k2 < K; ++k2) {
                double mse = 0.0;
                const bool ok = computePairCostDirect(A, B,
                                                      shifts[k1], shifts[k2],
                                                      edge_th, mse);
                if (!ok) {
                    result.valid = false;
                    result.costs.clear();
                    return result; // 1つでもダメならこのpairは不採用
                }
                result.costs[k1 * K + k2] = mse;
            }
        }

        result.valid = true;
        return result;
    }
}


// 全体最適化ボタン_PSNR
void MainWindow::calc_TRWS()
{
    // 画像の数
    const int N = input_files.size();
    // 探索範囲を設定
    const int K = ((radi * 2) + 1) * ((radi * 2) + 1); // 状態数

    // 各状態のx,yのシフト量を計算
    QVector<QPoint> shifts;
    shifts.reserve(K);
    for (int y = -radi; y <= radi; ++y) { // 行優先
        for (int x = -radi; x <= radi; ++x) {
            shifts.push_back(QPoint(x,y));
        }
    }



    // エッジを構築
    // 前処理
    std::vector<PrecomputedImage> pre;
    pre.reserve(N);

    for (int i = 0; i < N; ++i) {
        PrecomputedImage p;
        p.bgra = imgs[i];
        p.pos  = poss[i];
        p.res  = res_all[i];

        // ここは1回だけ
        cv::cvtColor(p.bgra, p.bgr, cv::COLOR_BGRA2BGR);
        p.mask = ImageUtils::alphaMaskFromBGRA(p.bgra, 0.5);

        pre.push_back(std::move(p));
    }

    // ペア一覧を先に作る
    std::vector<PairTask> tasks;
    tasks.reserve(N * (N - 1) / 2);

    for (int i = 0; i < N; ++i) {
        for (int j = i + 1; j < N; ++j) {
            tasks.push_back({i, j});
        }
    }

    //qDebug() << "edge lists (i,j)";

    // QtConcurrentでペア単位並列
    auto mapFunc = [&](const PairTask& t) -> PairResult {
        return processOnePair(t, pre, shifts, K, radi, edge_th);
    };

    auto reduceFunc = [&](QVector<PairResult>& out, const PairResult& r) {
        if (r.valid) {
            out.push_back(r);
        }
    };

    // ordered にすると元タスク順で reduce される
    QVector<PairResult> results = QtConcurrent::blockingMappedReduced<QVector<PairResult>>(
        tasks, mapFunc, reduceFunc, QtConcurrent::OrderedReduce);

    // 結果を格納
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<double>> pairCosts;

    edges.reserve(results.size());
    pairCosts.reserve(results.size());

    for (const auto& r : std::as_const(results)) {
        edges.push_back({r.i, r.j});
        pairCosts.push_back(r.costs);
        //qDebug() << r.i << r.j;
    }


    // order は row-major そのまま
    std::vector<int> order(N);
    for (int i = 0; i < N; ++i) order[i] = i;

    TRWS::Options opts;
    opts.maxIter = 1000;
    opts.tol = 1e-5;
    opts.stallIters = 10;

    TRWS solver(N, K, edges, pairCosts, order, opts);
    TRWSResult result = solver.run();

    qDebug() << "iterations = " << result.iterations;
    qDebug() << "energy     = " << result.energy;
    qDebug() << "reward     = " << result.reward;

    qDebug() << "labels:";
    for (int l : result.labels) {
        qDebug() << "x =" << shifts[l].x() << "  y =" << shifts[l].y();
    }

    // 変更を適用
    for(int i = 0; i < N; ++i) {
        poss[i] = QPoint(poss[i].x()+shifts[i].x(), poss[i].y()+shifts[i].y());
    }

    for(int i = 0; i < N; ++i) {
        items[i]->setPos(poss[i]);
    }

    qDebug() << "lower bound history:";
    for (double v : result.lowerBoundHistory) {
        qDebug() << v;
    }
}
*/

// 全体最適化ボタン_SSIM
/*
void MainWindow::calc_TRWS()
{
    // 画像の数
    const int N = input_files.size();
    // 探索範囲を設定
    const int radi = 2; // 半径1pixを探索する。
    const int K = ((radi * 2) + 1) * ((radi * 2) + 1); // 状態数

    // 各状態のx,yのシフト量を計算
    QVector<QPoint> shifts;
    shifts.reserve(K);
    for (int y = -radi; y <= radi; ++y) { // 行優先
        for (int x = -radi; x <= radi; ++x) {
            shifts.push_back(QPoint(x,y));
        }
    }

    // エッジを構築
    std::vector<std::pair<int,int>> edges;
    std::vector<std::vector<double>> pairCosts;
    qDebug() << "edge lists (i,j)";
    for (int i = 0; i < N; ++i) { // 1枚目画像を選択
        cv::Mat img1 = imgs[i];
        QSize res1 = res_all[i];
        QPoint pos1 = poss[i];
        cv::Mat1b mask1 = ImageUtils::alphaMaskFromBGRA(img1, 0.5);

        for (int j = 0; j < N; ++j) { // 2枚目画像を選択
            // 1枚目と2枚目が同じ場合
            if (j <= i) continue; // 次のjへ
            cv::Mat img2 = imgs[j];
            QSize res2 = res_all[j];
            QPoint pos2 = poss[j];
            cv::Mat1b mask2 = ImageUtils::alphaMaskFromBGRA(img2, 0.5);

            // デフォルト位置で重複が存在するか確認する
            const int x_t1 = std::max(pos1.x()+res1.width(),pos2.x()+res2.width())
                             - std::min(pos1.x(),pos2.x());
            const int x_t2 = res1.width() + res2.width();
            const int x_t3 = x_t2 - x_t1 - radi;
            if (x_t3 <= 7) continue; // 次のjへ
            const int y_t1 = std::max(pos1.y()+res1.height(),pos2.y()+res2.height())
                             - std::min(pos1.y(),pos2.y());
            const int y_t2 = res1.height() + res2.height();
            const int y_t3 = y_t2 - y_t1 - radi;
            if (y_t3 <= 7) continue; // 次のjへ

            std::vector<bool> TF_temp(K * K, false); // エッジを張れるかどうか
            std::vector<double> nowCost(K * K, 0.0); // そのエッジにおけるコスト
            for (int k1 = 0; k1 < K; ++k1) { // 1枚目の状態選択
                for (int k2 = 0; k2 < K; ++k2) { // 2枚目の状態選択
                    TF_temp[k1 * K + k2] = true;
                    nowCost[k1 * K + k2] = -SSIM_calc_oneshot(SSIM_TaskInput{img1,img2,res1,pos1,res2,pos2,shifts[k2].x()-shifts[k1].x(),shifts[k2].y()-shifts[k1].y()});
                }
            }
            int falseCount = std::count(TF_temp.begin(), TF_temp.end(), false);
            if (falseCount > 0) continue; // 次のjへ

            // 変数格納
            edges.push_back({i,j});
            qDebug() << i << j;
            pairCosts.push_back(nowCost);
        }
    }
    // order は row-major そのまま
    std::vector<int> order(N);
    for (int i = 0; i < N; ++i) order[i] = i;

    TRWS::Options opts;
    opts.maxIter = 200;
    opts.tol = 1e-8;
    opts.stallIters = 10;

    TRWS solver(N, K, edges, pairCosts, order, opts);
    TRWSResult result = solver.run();

    qDebug() << "iterations = " << result.iterations;
    qDebug() << "energy     = " << result.energy;
    qDebug() << "reward     = " << result.reward;

    qDebug() << "labels:";
    for (int l : result.labels) {
        qDebug() << "x =" << shifts[l].x() << "  y =" << shifts[l].y();
    }

    qDebug() << "lower bound history:";
    for (double v : result.lowerBoundHistory) {
        qDebug() << v;

    }

    // 変更を適用
    for(int i = 0; i < N; ++i) {
        poss[i] = QPoint(poss[i].x()+shifts[i].x(), poss[i].y()+shifts[i].y());
    }

    for(int i = 0; i < N; ++i) {
        items[i]->setPos(poss[i]);
    }

}
*/


namespace
{
struct PairTask
{
    int i;
    int j;
};

struct ShiftDelta
{
    int dx;
    int dy;
};

struct PairResult
{
    bool valid = false;
    int i = -1;
    int j = -1;
    std::vector<double> costs;
};

std::mutex ssimGpuDispatchMutex;

inline bool defaultMayOverlap(const QSize& res1, const QPoint& pos1,
                              const QSize& res2, const QPoint& pos2,
                              int radi)
{
    const int x_t1 = std::max(pos1.x() + res1.width(),  pos2.x() + res2.width())
    - std::min(pos1.x(), pos2.x());
    const int x_t2 = res1.width() + res2.width();
    const int x_t3 = x_t2 - x_t1 - radi;
    if (x_t3 <= 7) return false;

    const int y_t1 = std::max(pos1.y() + res1.height(), pos2.y() + res2.height())
                     - std::min(pos1.y(), pos2.y());
    const int y_t2 = res1.height() + res2.height();
    const int y_t3 = y_t2 - y_t1 - radi;
    if (y_t3 <= 7) return false;

    return true;
}

PairResult processOnePair(
    const PairTask& task,
    const std::vector<cv::Mat>& imgs,
    const QVector<QSize>& res_all,
    const QVector<QPoint>& poss,
    const std::vector<ShiftDelta>& deltas,
    int K,
    int radi,
    const VulkanExecutionOptions& vulkanOptions,
    const std::shared_ptr<std::atomic_bool>& cancellation)
{
    PairResult result;
    result.i = task.i;
    result.j = task.j;
    if (workerCancellationRequested(cancellation)) {
        return result;
    }

    const cv::Mat& img1 = imgs[task.i];
    const cv::Mat& img2 = imgs[task.j];
    const QSize& res1 = res_all[task.i];
    const QSize& res2 = res_all[task.j];
    const QPoint& pos1 = poss[task.i];
    const QPoint& pos2 = poss[task.j];

    result.costs.resize(K * K);

    // dx,dy は [-2*radi, 2*radi]
    const int diffSpan = 4 * radi + 1;

    // キャッシュ
    std::vector<double> cachedCosts(diffSpan * diffSpan, 0.0);
    std::vector<unsigned char> computed(diffSpan * diffSpan, 0);

    auto diffIndex = [diffSpan, radi](int dx, int dy) -> int {
        return (dy + 2 * radi) * diffSpan + (dx + 2 * radi);
    };

    // GPUでは元画像2枚を一度だけ転送し、全相対変位のROIを一括処理する。
    if (vulkanOptions.enabled
        && (MetalSsimEngine::isAvailable() || VulkanSsimEngine::isBuilt())) {
        std::vector<ShiftDelta> uniqueDeltas;
        std::vector<int> uniqueCostIndices;
        std::vector<int> taskIndexForCost(diffSpan * diffSpan, -1);
        uniqueDeltas.reserve(diffSpan * diffSpan);
        uniqueCostIndices.reserve(diffSpan * diffSpan);

        for (int idx = 0; idx < K * K; ++idx) {
            const ShiftDelta& delta = deltas[idx];
            const int costIndex = diffIndex(delta.dx, delta.dy);
            if (taskIndexForCost[costIndex] >= 0) {
                continue;
            }
            taskIndexForCost[costIndex] = static_cast<int>(uniqueDeltas.size());
            uniqueDeltas.push_back(delta);
            uniqueCostIndices.push_back(costIndex);
        }

        try {
            if (workerCancellationRequested(cancellation)) {
                return result;
            }
            const cv::Mat1b mask1 = ImageUtils::alphaMaskFromBGRA(img1, 0.5);
            const cv::Mat1b mask2 = ImageUtils::alphaMaskFromBGRA(img2, 0.5);
            std::vector<VulkanSsimRoiPair> rois(uniqueDeltas.size());
            for (size_t i = 0; i < uniqueDeltas.size(); ++i) {
                const QPoint shiftedPos2(pos2.x() + uniqueDeltas[i].dx,
                                         pos2.y() + uniqueDeltas[i].dy);
                calculateOverlapCropRois(img1, img2, pos1, shiftedPos2,
                                         &mask1, &mask2,
                                         rois[i].first, rois[i].second);
            }

            std::unique_lock<std::mutex> gpuDispatchLock(ssimGpuDispatchMutex);
            if (workerCancellationRequested(cancellation)) {
                return result;
            }

            bool gpuCompleted = false;
            if (MetalSsimEngine::isAvailable()) {
                const MetalSsimBatchResult metalResult = MetalSsimEngine::computeBatch(
                    img1, img2, rois, vulkanOptions.ignoreVramLimit);
                if (metalResult.succeeded()
                    && metalResult.scores.size() == uniqueDeltas.size()) {
                    static std::atomic_bool metalSuccessLogged{false};
                    if (!metalSuccessLogged.exchange(true)) {
                        qInfo().noquote()
                            << QString("Metal SSIM enabled (memory estimate: %1 MiB, limit: %2 MiB)")
                                   .arg(metalResult.requiredVramBytes / (1024 * 1024))
                                   .arg(metalResult.vramLimitBytes / (1024 * 1024));
                    }
                    for (size_t i = 0; i < uniqueDeltas.size(); ++i) {
                        const double s_v = metalResult.scores[i];
                        const double ratio = (1.0 - s_v) / s_v;
                        const int costIndex = uniqueCostIndices[i];
                        cachedCosts[costIndex] = ratio * ratio / 81.0;
                        computed[costIndex] = 1;
                    }
                    gpuCompleted = true;
                } else if (metalResult.status == MetalSsimStatus::VramLimitExceeded) {
                    static std::atomic_bool metalVramFallbackLogged{false};
                    if (!metalVramFallbackLogged.exchange(true)) {
                        qInfo().noquote()
                            << QString("Metal SSIM CPU fallback: memory estimate %1 MiB exceeds limit %2 MiB")
                                   .arg(metalResult.requiredVramBytes / (1024 * 1024))
                                   .arg(metalResult.vramLimitBytes / (1024 * 1024));
                    }
                }
            }

            if (!gpuCompleted && VulkanSsimEngine::isBuilt()) {
                const VulkanSsimBatchResult gpuResult = VulkanSsimEngine::computeBatch(
                    img1, img2, rois, vulkanOptions.deviceKey,
                    vulkanOptions.ignoreVramLimit);
                if (gpuResult.succeeded()
                    && gpuResult.scores.size() == uniqueDeltas.size()) {
                    static std::atomic_bool vulkanSuccessLogged{false};
                    if (!vulkanSuccessLogged.exchange(true)) {
                        qInfo().noquote()
                            << QString("Vulkan SSIM enabled (VRAM estimate: %1 MiB, limit: %2 MiB)")
                                   .arg(gpuResult.requiredVramBytes / (1024 * 1024))
                                   .arg(gpuResult.vramLimitBytes / (1024 * 1024));
                    }
                    for (size_t i = 0; i < uniqueDeltas.size(); ++i) {
                        const double s_v = gpuResult.scores[i];
                        const double ratio = (1.0 - s_v) / s_v;
                        const int costIndex = uniqueCostIndices[i];
                        cachedCosts[costIndex] = ratio * ratio / 81.0;
                        computed[costIndex] = 1;
                    }
                } else if (gpuResult.status == VulkanSsimStatus::VramLimitExceeded) {
                    static std::atomic_bool vramFallbackLogged{false};
                    if (!vramFallbackLogged.exchange(true)) {
                        qInfo().noquote()
                            << QString("Vulkan SSIM CPU fallback: VRAM estimate %1 MiB exceeds limit %2 MiB")
                                   .arg(gpuResult.requiredVramBytes / (1024 * 1024))
                                   .arg(gpuResult.vramLimitBytes / (1024 * 1024));
                    }
                }
            }
        } catch (const cv::Exception&) {
            // Vulkan前処理が失敗した場合も、未計算分を下のCPU経路で処理する。
        } catch (const std::exception&) {
            // メモリ確保失敗等でもCPUフォールバックを維持する。
        }
    }

    for (int idx = 0; idx < K * K; ++idx) {
        if (workerCancellationRequested(cancellation)) {
            return result;
        }
        const auto& d = deltas[idx];
        const int ci = diffIndex(d.dx, d.dy);

        if (!computed[ci]) {
            double s_v = SSIM_calc_oneshot(
                SSIM_TaskInput{
                    img1, img2,
                    res1, pos1,
                    res2, pos2,
                    d.dx, d.dy
                }
                );
            const double ratio = (1.0 - s_v) / s_v;
            cachedCosts[ci] = ratio * ratio / 81.0;
            computed[ci] = 1;
        }

        result.costs[idx] = cachedCosts[ci];
    }

    result.valid = true;
    return result;
}

struct LeastSquaresEdge
{
    int i = -1;
    int j = -1;
    double dx = 0.0;
    double dy = 0.0;
    double weight = 1.0;
    bool active = true;
    double residual = 0.0;
};

bool solveDenseLinearSystem(std::vector<std::vector<double>> A,
                            std::vector<double> b,
                            std::vector<double>& x,
                            const std::shared_ptr<std::atomic_bool>& cancellation)
{
    const int n = static_cast<int>(b.size());
    x.assign(n, 0.0);
    if (n == 0) return true;

    for (int col = 0; col < n; ++col) {
        if (workerCancellationRequested(cancellation)) return false;
        int pivot = col;
        double pivotAbs = std::abs(A[col][col]);
        for (int row = col + 1; row < n; ++row) {
            const double v = std::abs(A[row][col]);
            if (v > pivotAbs) {
                pivotAbs = v;
                pivot = row;
            }
        }

        if (pivotAbs < 1e-10) {
            return false;
        }

        if (pivot != col) {
            std::swap(A[pivot], A[col]);
            std::swap(b[pivot], b[col]);
        }

        const double diag = A[col][col];
        for (int c = col; c < n; ++c) {
            A[col][c] /= diag;
        }
        b[col] /= diag;

        for (int row = 0; row < n; ++row) {
            if (row == col) continue;
            const double factor = A[row][col];
            if (std::abs(factor) < 1e-14) continue;
            for (int c = col; c < n; ++c) {
                A[row][c] -= factor * A[col][c];
            }
            b[row] -= factor * b[col];
        }
    }

    x = std::move(b);
    return true;
}

bool solveLeastSquaresPositions(const QVector<QPoint>& initial,
                                const std::vector<LeastSquaresEdge>& edges,
                                QVector<QPointF>& optimized,
                                QString& error,
                                const std::shared_ptr<std::atomic_bool>& cancellation)
{
    const int N = initial.size();
    optimized.resize(N);
    for (int i = 0; i < N; ++i) {
        optimized[i] = QPointF(initial[i]);
    }

    QVector<QVector<int>> adjacency(N);
    for (int e = 0; e < static_cast<int>(edges.size()); ++e) {
        if (!edges[e].active) continue;
        adjacency[edges[e].i].push_back(e);
        adjacency[edges[e].j].push_back(e);
    }

    QVector<bool> visited(N, false);
    for (int start = 0; start < N; ++start) {
        if (workerCancellationRequested(cancellation)) return false;
        if (visited[start] || adjacency[start].isEmpty()) {
            continue;
        }

        QVector<int> component;
        QVector<int> stack;
        stack.push_back(start);
        visited[start] = true;
        while (!stack.isEmpty()) {
            if (workerCancellationRequested(cancellation)) return false;
            const int node = stack.back();
            stack.pop_back();
            component.push_back(node);

            for (int edgeIndex : std::as_const(adjacency[node])) {
                const LeastSquaresEdge& e = edges[edgeIndex];
                const int other = (e.i == node) ? e.j : e.i;
                if (!visited[other]) {
                    visited[other] = true;
                    stack.push_back(other);
                }
            }
        }

        int fixedNode = component[0];
        if (component.contains(0)) {
            fixedNode = 0;
        }

        QVector<int> variableIndex(N, -1);
        int variableCount = 0;
        for (int node : std::as_const(component)) {
            if (node == fixedNode) continue;
            variableIndex[node] = variableCount++;
        }
        if (variableCount == 0) {
            continue;
        }

        std::vector<std::vector<double>> A(
            variableCount, std::vector<double>(variableCount, 0.0));
        std::vector<double> bx(variableCount, 0.0);
        std::vector<double> by(variableCount, 0.0);

        for (const LeastSquaresEdge& e : edges) {
            if (!e.active) continue;
            if (variableIndex[e.i] < 0 && e.i != fixedNode &&
                variableIndex[e.j] < 0 && e.j != fixedNode) {
                continue;
            }

            struct Term { int node; double coeff; };
            const Term terms[2] = {{e.i, -1.0}, {e.j, 1.0}};
            double targetX = e.dx;
            double targetY = e.dy;
            for (const Term& term : terms) {
                if (term.node == fixedNode) {
                    targetX -= term.coeff * initial[fixedNode].x();
                    targetY -= term.coeff * initial[fixedNode].y();
                }
            }

            for (const Term& rowTerm : terms) {
                const int row = variableIndex[rowTerm.node];
                if (row < 0) continue;

                bx[row] += e.weight * rowTerm.coeff * targetX;
                by[row] += e.weight * rowTerm.coeff * targetY;

                for (const Term& colTerm : terms) {
                    const int col = variableIndex[colTerm.node];
                    if (col < 0) continue;
                    A[row][col] += e.weight * rowTerm.coeff * colTerm.coeff;
                }
            }
        }

        std::vector<double> x;
        std::vector<double> y;
        if (!solveDenseLinearSystem(A, bx, x, cancellation) ||
            !solveDenseLinearSystem(A, by, y, cancellation)) {
            error = QCoreApplication::translate(
                "MainWindow", "最小二乗法の線形方程式を解けませんでした。");
            return false;
        }

        optimized[fixedNode] = QPointF(initial[fixedNode]);
        for (int node : std::as_const(component)) {
            const int vi = variableIndex[node];
            if (vi < 0) continue;
            optimized[node] = QPointF(x[vi], y[vi]);
        }
    }

    return true;
}

void updateLeastSquaresResiduals(const QVector<QPointF>& positions,
                                 std::vector<LeastSquaresEdge>& edges,
                                 double& avgError,
                                 double& maxError,
                                 int& worstEdge)
{
    avgError = 0.0;
    maxError = 0.0;
    worstEdge = -1;

    int count = 0;
    for (int e = 0; e < static_cast<int>(edges.size()); ++e) {
        LeastSquaresEdge& edge = edges[e];
        if (!edge.active) continue;

        const double rx = (positions[edge.j].x() - positions[edge.i].x()) - edge.dx;
        const double ry = (positions[edge.j].y() - positions[edge.i].y()) - edge.dy;
        edge.residual = std::sqrt(rx * rx + ry * ry);
        avgError += edge.residual;
        if (edge.residual > maxError) {
            maxError = edge.residual;
            worstEdge = e;
        }
        ++count;
    }

    if (count > 0) {
        avgError /= count;
    }
}
}

void MainWindow::calc_least_squares()
{
    if (isAlignmentOrOptimizationRunning()) {
        return;
    }

    const int n = input_files.size();
    if (n <= 1) {
        showNonModalMessage(QMessageBox::Warning, tr("最小二乗法"),
                            tr("2枚以上の画像を入力してください。"));
        return;
    }

    readLeastSquaresSettingsDialog();
    leastSquaresRunning = true;
    ui->label_4->clear();
    ui->pushButton_9->setEnabled(false);
    leastSquaresDetails.clear();

    ui->pushButton_Calc1->setEnabled(false);
    ui->pushButton_3->setEnabled(false);
    ui->pushButton_8->setEnabled(false);
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    ui->pushButton_6->setEnabled(false);
    ui->pushButton_10->setEnabled(false);
    ui->pushButton_11->setEnabled(false);
    ui->checkBox_2->setEnabled(false);
    refreshCanvasHistoryDialog();
    showCalculationProgressDialog(tr("位置合わせ最適化（最小二乗法）"),
                                  QStringLiteral("CPU"));

    ui->checkBox->setChecked(true);
    ui->checkBox->setEnabled(false);

    imgs.resize(n);
    for (int i = 0; i < n; ++i) {
        QImage img_QI = items[i]->pixmap().toImage();
        imgs[i] = ImageUtils::qimage_to_mat_bgra(img_QI);
    }
    res_all.resize(n);
    for (int i = 0; i < n; ++i) {
        res_all[i] = items[i]->pixmap().size();
    }
    syncPossFromScene();

    CalcLeastSquaresInput input;
    input.imgs = imgs;
    input.res_all = res_all;
    input.poss = poss;
    input.calc_loop_num = calc_loop_num;
    input.settings = leastSquaresSettings;
    input.cancelRequested = calculationCancelRequested;
    const auto leastSquaresCalculation = input.cancelRequested;
    input.progressCallback = [this, leastSquaresCalculation](
                                 int value, int maximum, const QString& text) {
        QMetaObject::invokeMethod(this, [this, leastSquaresCalculation,
                                         value, maximum, text]() {
            if (calculationCancelRequested != leastSquaresCalculation) {
                return;
            }
            updateCalculationProgress(value, maximum, text);
        }, Qt::QueuedConnection);
    };

    auto future = QtConcurrent::run([=]() {
        return calc_least_squares_core(input);
    });

    leastSquaresWatcher->setFuture(future);
    refreshCanvasHistoryDialog();
}

void MainWindow::calc_least_squares_finish()
{
    const CalcLeastSquaresOutput out = leastSquaresWatcher->result();
    if (out.cancelled || calculationCancellationRequested()
        || leastSquaresWatcher->future().isCanceled()) {
        ui->label_4->setText(tr("キャンセル"));
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton_4->setEnabled(!output_img.empty());
        ui->pushButton_2->setEnabled(calc1_finished_state);
        ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
        ui->pushButton_8->setEnabled(calc1_finished_state && input_files.size() > 1);
        ui->pushButton_6->setEnabled(true);
        ui->pushButton_10->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        leastSquaresRunning = false;
        finishCancelledCalculation(tr("最小二乗最適化をキャンセルしました。"));
        return;
    }
    leastSquaresDetails = out.details;
    ui->pushButton_9->setEnabled(!leastSquaresDetails.empty());
    if (out.err.empty()) {
        poss = out.poss;
        pos_all = poss;
        for (int i = 0; i < poss.size() && i < items.size(); ++i) {
            items[i]->setPos(poss[i]);
        }
        syncPossFromScene();
        recordCanvasHistory(CanvasHistoryMarkerOptimization);
        ui->label_4->setText(tr("最小SSIM:%1")
                                 .arg(out.minSsim, 0, 'f', 3));
        ui->pushButton_2->setEnabled(true);
    } else {
        ui->label_4->setText(tr("不良"));
        showNonModalMessage(QMessageBox::Warning, tr("最小二乗法"),
                            QString::fromStdString(out.err));
        ui->pushButton_2->setEnabled(poss.size() == items.size() && !poss.isEmpty());
    }

    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton_4->setEnabled(!output_img.empty());
    ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
    ui->pushButton_8->setEnabled(calc1_finished_state && input_files.size() > 1);
    ui->pushButton_6->setEnabled(true);
    ui->pushButton_10->setEnabled(true);
    ui->pushButton_11->setEnabled(true);
    ui->checkBox->setEnabled(true);
    ui->checkBox_2->setEnabled(true);
    applySceneMoveMode(!ui->checkBox->isChecked());

    leastSquaresRunning = false;
    hideCalculationProgressDialog();
    refreshCanvasHistoryDialog();
}

CalcLeastSquaresOutput MainWindow::calc_least_squares_core(CalcLeastSquaresInput in)
{
    CalcLeastSquaresOutput ret;
    ret.poss = in.poss;
    const int N = in.poss.size();
    auto cancelled = [&]() {
        return workerCancellationRequested(in.cancelRequested);
    };
    if (cancelled()) {
        ret.cancelled = true;
        return ret;
    }
    if (N <= 1) {
        ret.err = tr("2枚以上の画像が必要です。").toStdString();
        return ret;
    }

    auto reportProgress = [&](int value, int maximum, const QString& text) {
        if (in.progressCallback) {
            in.progressCallback(value, maximum, text);
        }
    };

    std::vector<PairTask> pairTasks;
    pairTasks.reserve(N * (N - 1) / 2);
    for (int i = 0; i < N; ++i) {
        if (cancelled()) {
            ret.cancelled = true;
            return ret;
        }
        for (int j = i + 1; j < N; ++j) {
            if (defaultMayOverlap(in.res_all[i], in.poss[i],
                                  in.res_all[j], in.poss[j], 0)) {
                pairTasks.push_back({i, j});
            }
        }
    }

    if (pairTasks.empty()) {
        ret.err = tr("キャンパス座標上で重なる画像ペアがありません。").toStdString();
        return ret;
    }

    const int progressMaximum = static_cast<int>(pairTasks.size()) + 1;
    reportProgress(0, progressMaximum, tr("重なりペアを計算中"));

    QVector<PairTask> taskVec = QVector<PairTask>(pairTasks.begin(), pairTasks.end());
    QVector<ifft_thread_output> pairResults = QtConcurrent::blockingMapped<QVector<ifft_thread_output>>(
        taskVec,
        [&](const PairTask& task) -> ifft_thread_output {
            ifft_thread_input input;
            input.img1_id = task.i;
            input.img2_id = task.j;
            input.input1 = in.imgs[task.i];
            input.input2 = in.imgs[task.j];
            input.px1 = in.res_all[task.i];
            input.px2 = in.res_all[task.j];
            input.pos1 = in.poss[task.i];
            input.pos2 = in.poss[task.j];
            input.calc_loop_num = in.calc_loop_num;
            input.cancelRequested = in.cancelRequested;
            return calc_oneshot(input);
        });

    if (cancelled()) {
        ret.cancelled = true;
        return ret;
    }

    std::vector<LeastSquaresEdge> edges;
    edges.reserve(pairResults.size());
    for (const ifft_thread_output& result : std::as_const(pairResults)) {
        if (cancelled()) {
            ret.cancelled = true;
            return ret;
        }
        if (result.calc_error) {
            continue;
        }
        if (result.score < in.settings.regressionThreshold) {
            continue;
        }

        LeastSquaresEdge edge;
        edge.i = result.img1_id;
        edge.j = result.img2_id;
        edge.dx = result.vecX;
        edge.dy = result.vecY;
        edge.weight = std::max(1e-6, result.score);
        edges.push_back(edge);
    }

    if (edges.empty()) {
        ret.err = tr("しきい値を満たす相関ペアがありません。").toStdString();
        return ret;
    }

    QVector<QPointF> optimized;
    int removedPairs = 0;
    double avgError = 0.0;
    double maxError = 0.0;
    bool redo = false;
    do {
        redo = false;
        QString solveError;
        if (cancelled()) {
            ret.cancelled = true;
            return ret;
        }
        if (!solveLeastSquaresPositions(in.poss, edges, optimized, solveError,
                                        in.cancelRequested)) {
            if (cancelled()) {
                ret.cancelled = true;
                return ret;
            }
            ret.err = solveError.toStdString();
            return ret;
        }

        int worstEdge = -1;
        updateLeastSquaresResiduals(optimized, edges, avgError, maxError, worstEdge);

        const bool hasRelativeOutlier =
            avgError * in.settings.relativeThreshold < maxError &&
            maxError > in.settings.maxPairErrorForRelative;
        const bool hasAbsoluteOutlier = avgError > in.settings.absoluteThreshold;

        if ((hasRelativeOutlier || hasAbsoluteOutlier) && worstEdge >= 0) {
            int activeCount = 0;
            for (const LeastSquaresEdge& edge : edges) {
                if (edge.active) {
                    ++activeCount;
                }
            }
            if (activeCount <= 1) {
                break;
            }

            edges[worstEdge].active = false;
            ++removedPairs;
            redo = true;
            reportProgress(std::min(removedPairs, static_cast<int>(pairTasks.size())),
                           progressMaximum,
                           tr("外れリンクを除去中 (%1)").arg(removedPairs));
        }
    } while (redo);

    ret.poss.clear();
    ret.poss.reserve(N);
    for (const QPointF& p : std::as_const(optimized)) {
        ret.poss.push_back(QPoint(static_cast<int>(std::round(p.x())),
                                  static_cast<int>(std::round(p.y()))));
    }
    ret.acceptedPairs = 0;
    for (const LeastSquaresEdge& edge : edges) {
        if (edge.active) {
            ++ret.acceptedPairs;
        }
    }
    ret.removedPairs = removedPairs;
    ret.avgError = avgError;
    ret.maxError = maxError;
    ret.minSsim = 1.0;
    ret.details.clear();
    ret.details.reserve(pairResults.size());
    for (const ifft_thread_output& result : std::as_const(pairResults)) {
        if (cancelled()) {
            ret.cancelled = true;
            return ret;
        }
        LeastSquaresPairDetail detail;
        detail.img1 = result.img1_id;
        detail.img2 = result.img2_id;
        detail.loop_num = result.loop_num;
        detail.stability = result.stability;
        detail.score = result.score;
        detail.measuredSsim = result.ssim;
        detail.measuredDx = result.vecX;
        detail.measuredDy = result.vecY;

        const LeastSquaresEdge* matchedEdge = nullptr;
        for (const LeastSquaresEdge& edge : edges) {
            if (edge.i == result.img1_id && edge.j == result.img2_id) {
                matchedEdge = &edge;
                break;
            }
        }

        if (result.calc_error) {
            detail.status = tr("計算失敗");
        } else if (!matchedEdge) {
            detail.status = tr("しきい値未満");
        } else if (matchedEdge->active) {
            detail.status = tr("採用");
        } else {
            detail.status = tr("除外");
        }

        if (matchedEdge) {
            detail.residual = matchedEdge->residual;
        }

        detail.optimizedDx = optimized[result.img2_id].x() - optimized[result.img1_id].x();
        detail.optimizedDy = optimized[result.img2_id].y() - optimized[result.img1_id].y();

        try {
            detail.optimizedSsim = SSIM_calc_oneshot(
                SSIM_TaskInput{
                    in.imgs[result.img1_id],
                    in.imgs[result.img2_id],
                    in.res_all[result.img1_id],
                    ret.poss[result.img1_id],
                    in.res_all[result.img2_id],
                    ret.poss[result.img2_id],
                    0,
                    0
                });
        } catch (...) {
            detail.optimizedSsim = 0.0;
        }

        ret.minSsim = std::min(ret.minSsim, detail.optimizedSsim);
        ret.details.push_back(detail);
    }
    if (ret.details.empty()) {
        ret.minSsim = 0.0;
    }
    ret.err = "";

    reportProgress(progressMaximum, progressMaximum,
                   tr("最小二乗最適化完了  採用:%1  除外:%2")
                       .arg(ret.acceptedPairs)
                       .arg(ret.removedPairs));
    return ret;
}

void MainWindow::calc_TRWS()
{
    if (isAlignmentOrOptimizationRunning()) {
        return; // 二重起動防止
    }
    readOptimizationSettingsDialog();
    if (!pa_TF && !all_TF) {
        ui->pushButton_3->setEnabled(false);
        showNonModalMessage(
            QMessageBox::Warning, tr("最適化計算"),
            tr("部分最適化または全体最適化を有効にしてください。"));
        return;
    }
    trwsRunning = true;
    ui->label_12->clear();

    ui->pushButton_Calc1->setEnabled(false);
    ui->pushButton_3->setEnabled(false);
    ui->pushButton_8->setEnabled(false);
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    //ui->pushButton_5->setEnabled(false);
    ui->pushButton_6->setEnabled(false);
    ui->pushButton_10->setEnabled(false);
    ui->pushButton_11->setEnabled(false);
    ui->checkBox_2->setEnabled(false);
    refreshCanvasHistoryDialog();
    showCalculationProgressDialog(tr("位置合わせ最適化（TRW-S-PAMI）"),
                                  selectedOptimizationComputePath());

    ui->checkBox->setChecked(true);
    ui->checkBox->setEnabled(false);

    //ui->groupBox_5->setEnabled(false);
    syncPossFromScene();

    CalcTRWSinput input;
    input.imgs = imgs;
    input.res_all = res_all;
    input.poss = poss;
    input.pa_TF = pa_TF;
    input.pa_num = pa_num;
    input.pa_auto_increment_TF = pa_auto_increment_TF;
    input.pa_increment = pa_increment;
    input.pa_increment_count = pa_increment_count;
    input.pa_radi = pa_radi;
    input.pa_opti = pa_opti;
    input.pa_itr = pa_itr;
    input.all_TF = all_TF;
    input.all_radi = all_radi;
    input.all_opti = all_opti;
    input.all_itr = all_itr;
    input.vulkan = AppSettings::vulkanOptions();
    input.cancelRequested = calculationCancelRequested;
    const auto trwsCalculation = input.cancelRequested;
    input.progressCallback = [this, trwsCalculation](
                                 int value, int maximum, const QString& text) {
        QMetaObject::invokeMethod(this, [this, trwsCalculation,
                                         value, maximum, text]() {
            if (calculationCancelRequested != trwsCalculation) {
                return;
            }
            updateCalculationProgress(value, maximum, text);
        }, Qt::QueuedConnection);
    };

    auto future = QtConcurrent::run([=]() {
        return calc_TRWS_core(input);
    });

    trwsWatcher->setFuture(future);

}

void MainWindow::calc_TRWS_finish()
{

    const CalcTRWSoutput out = trwsWatcher->result();
    if (out.cancelled || calculationCancellationRequested()
        || trwsWatcher->future().isCanceled()) {
        ui->label_12->setText(tr("キャンセル"));
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton_4->setEnabled(!output_img.empty());
        ui->pushButton_2->setEnabled(true);
        ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
        ui->pushButton_8->setEnabled(calc1_finished_state && input_files.size() > 1);
        ui->pushButton_5->setEnabled(calc2_finished_state);
        ui->pushButton_6->setEnabled(true);
        ui->pushButton_10->setEnabled(true);
        ui->pushButton_11->setEnabled(true);
        trwsRunning = false;
        finishCancelledCalculation(tr("TRW-S-PAMI最適化をキャンセルしました。"));
        return;
    }
    if (out.err.empty() && !out.detail.empty()) {
        ui->label_12->setText(tr("完了"));
        poss = out.poss;
        const int N = poss.size();

        // detail dialogへデータを投げる
        const int odn = out.detail.size();
        for (int od = 0; od < odn; ++od) {
            m_detailDialog->setData(out.detail[od],out.log[od]);
        }

        bool ryoukou2 = false;
        if (out.detail[odn-1].minSSIM > 0.1) {
            //ui->label_12->setText("良好");
            ryoukou2 = true;
        }// else {
        //    ui->label_12->setText("一部不良");
        //}
        ui->label_12->setText(tr("最小SSIM:%1")
                                 .arg(out.detail[odn-1].minSSIM, 0, 'f', 3));

        // UI更新
        for (int i = 0; i < N; ++i) {
            items[i]->setPos(poss[i]);
        }
        syncPossFromScene();
        recordCanvasHistory(CanvasHistoryMarkerOptimization);

        if (calc_finish_sig && ryoukou2) {
            emit calcFinished();
        }
    } else {
        ui->label_12->setText(tr("不良"));
        showNonModalMessage(QMessageBox::Warning, tr("最適化計算"),
                            out.err.empty()
                                ? tr("最適化結果を取得できませんでした。")
                                : QString::fromStdString(out.err));

    }
    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton_4->setEnabled(!output_img.empty());
    ui->pushButton_2->setEnabled(true);
    ui->pushButton_3->setEnabled(true);
    ui->pushButton_8->setEnabled(calc1_finished_state && input_files.size() > 1);
    ui->pushButton_5->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
    ui->pushButton_10->setEnabled(true);
    ui->pushButton_11->setEnabled(true);
    ui->checkBox->setEnabled(true);
    ui->checkBox_2->setEnabled(true);
    applySceneMoveMode(!ui->checkBox->isChecked());

    calc2_finished_state = true;
    /*
    std::vector<out_detail> det = out.detail;
    for (int d = 0; d < det.size(); ++d) {
        qDebug() << det[d].PaAll << det[d].start << det[d].end << det[d].itr << det[d].loop << det[d].shuusoku << det[d].lowSSIM_num << det[d].minSSIM << det[d].energy;
    }
    */
    trwsRunning = false;
    hideCalculationProgressDialog();
    refreshCanvasHistoryDialog();


}

CalcTRWSoutput MainWindow::calc_TRWS_core(CalcTRWSinput in)
{
    CalcTRWSoutput ret;
    auto cancelled = [&]() {
        return workerCancellationRequested(in.cancelRequested);
    };
    if (cancelled()) {
        ret.cancelled = true;
        return ret;
    }
    const int N = in.poss.size();
    QVector<QPoint> in_poss = in.poss;
    int progressValue = 0;
    int progressMaximum = 1;
    auto reportProgress = [&](int value, int maximum, const QString& text) {
        if (in.progressCallback) {
            in.progressCallback(value, maximum, text);
        }
    };
    auto advanceProgress = [&](const QString& text) {
        progressValue = std::min(progressMaximum, progressValue + 1);
        reportProgress(progressValue, progressMaximum, text);
    };
    reportProgress(0, 1, tr("準備中"));

    // 部分最適化
    if (in.pa_TF) {
        const int localPassCount = in.pa_auto_increment_TF ? in.pa_increment_count + 1 : 1;
        for (int localPass = 0; localPass < localPassCount; ++localPass) {
        if (cancelled()) {
            ret.cancelled = true;
            return ret;
        }
        const int targetImageCount = in.pa_num + localPass * in.pa_increment;

        // edgeを計算
        std::vector<PairTask> tasks;
        tasks.reserve(N * (N - 1) / 2);

        //qDebug() << "edge lists (i,j)";
        for (int i = 0; i < N; ++i) {
            const QSize& res1 = in.res_all[i];
            const QPoint& pos1 = in_poss[i];

            for (int j = i + 1; j < N; ++j) {
                const QSize& res2 = in.res_all[j];
                const QPoint& pos2 = in_poss[j];

                if (!defaultMayOverlap(res1, pos1, res2, pos2, in.pa_radi * in.pa_itr)) {
                    continue;
                }
                tasks.push_back({i, j});
            }
        }

        std::vector<PairTask> imgId_list;
        std::vector<int> matchedI;
        const int loopSpan = targetImageCount - 1;
        for (const auto& t : tasks) {
            if (t.j - t.i == loopSpan) {
                matchedI.push_back(t.i);
            }
        }
        if (matchedI.empty()) {
            ret.err = "No loop structure was found.";
            return ret;
        }

        std::sort(matchedI.begin(), matchedI.end());
        matchedI.erase(std::unique(matchedI.begin(), matchedI.end()), matchedI.end());

        std::vector<int> filtered;
        filtered.reserve(matchedI.size());
        for (size_t k = 0; k < matchedI.size(); ++k) {
            const int x = matchedI[k];
            bool hasPrev = (k > 0 && matchedI[k - 1] == x - 1);
            bool hasNext = (k + 1 < matchedI.size() && matchedI[k + 1] == x + 1);
            if ((hasPrev || hasNext) && (x % 2 != 0)) {
                continue; // 連続ペアに含まれる奇数は捨てる
            }
            filtered.push_back(x);
        }
        matchedI = std::move(filtered);
        for (int mi : matchedI) {
            imgId_list.push_back({mi, mi + loopSpan});
        }
        progressMaximum = std::max(
            progressMaximum,
            progressValue + static_cast<int>(imgId_list.size()) * (in.pa_itr + 1) +
                (in.all_TF ? in.all_itr + 1 : 0));
        reportProgress(progressValue, progressMaximum,
                       tr("局所最適化を準備中 (%1枚)").arg(targetImageCount));

        const int Kp = ((in.pa_radi * 2) + 1) * ((in.pa_radi * 2) + 1);

        // 状態シフト
        QVector<QPoint> shiftsp;
        shiftsp.reserve(Kp);
        for (int y = -in.pa_radi; y <= in.pa_radi; ++y) {
            for (int x = -in.pa_radi; x <= in.pa_radi; ++x) {
                shiftsp.push_back(QPoint(x, y));
            }
        }

        // (k1, k2) -> (dx, dy) を前計算
        std::vector<ShiftDelta> deltasp;
        deltasp.resize(Kp * Kp);
        for (int k1 = 0; k1 < Kp; ++k1) {
            for (int k2 = 0; k2 < Kp; ++k2) {
                deltasp[k1 * Kp + k2] = {
                    shiftsp[k2].x() - shiftsp[k1].x(),
                    shiftsp[k2].y() - shiftsp[k1].y()
                };
            }
        }

        //qDebug() << "calc lists";
        for (PairTask p : imgId_list) {
            if (cancelled()) {
                ret.cancelled = true;
                return ret;
            }
            //qDebug() << p.i << p.j;
            out_detail det;
            // edgeを抽出
            std::vector<PairTask> task_now;
            for (const auto& t : tasks) {
                if (t.i >= p.i && t.i <= p.j && t.j >= p.i && t.j <= p.j) {
                    task_now.push_back(t);
                }
            }
            det.PaAll = true;
            det.start = p.i + 1;
            det.end = p.j + 1;
            det.shuusoku = false;
            if (task_now.size() > 0) {
                int whi = 0;
                while (whi < in.pa_itr) {
                    if (cancelled()) {
                        ret.cancelled = true;
                        return ret;
                    }
                    whi++;
                    det.loop = whi;
                    // コスト計算用関数を作成
                    //QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());

                    auto mapFunc = [&](const PairTask& t) -> PairResult {
                        return processOnePair(t, in.imgs, in.res_all, in_poss,
                                              deltasp, Kp, in.pa_radi, in.vulkan,
                                              in.cancelRequested);
                    };
                    auto reduceFunc = [&](QVector<PairResult>& out, const PairResult& r) {
                        if (r.valid) {
                            out.push_back(r);
                        }
                    };
                    QVector<PairResult> results = QtConcurrent::blockingMappedReduced<QVector<PairResult>>(
                            task_now, mapFunc, reduceFunc, QtConcurrent::OrderedReduce);
                    if (cancelled()) {
                        ret.cancelled = true;
                        return ret;
                    }

                    std::vector<std::pair<int,int>> edges;
                    std::vector<std::vector<double>> pairCosts;
                    edges.reserve(results.size());
                    pairCosts.reserve(results.size());

                    for (const auto& r : std::as_const(results)) {
                        edges.push_back({r.i - p.i, r.j - p.i});
                        pairCosts.push_back(r.costs);
                    }


                    const int Np = p.j - p.i + 1;
                    std::vector<int> order(Np);
                    for (int i = 0; i < Np; ++i) {
                        order[i] = i;
                    }

                    TRWS::Options opts;
                    opts.maxIter = in.pa_opti;
                    opts.tol = 1e-8;
                    opts.stallIters = 10;
                    opts.shouldCancel = cancelled;
                    std::vector<std::vector<double>> unaryCosts(Np, std::vector<double>(Kp, 0.0));
                    int fixedLabel = -1;
                    for (int l = 0; l < Kp; ++l) {
                        if (shiftsp[l].x() == 0 && shiftsp[l].y() == 0) {
                            fixedLabel = l;
                            break;
                        }
                    }
                    if (fixedLabel < 0) {
                        ret.err = "fixed label not found";
                        return ret;
                    }
                    const double INF = 1e100;
                    for (int l = 0; l < Kp; ++l) {
                        unaryCosts[0][l] = (l == fixedLabel) ? 0.0 : INF;
                    }

                    TRWS solver(Np, Kp, unaryCosts, edges, pairCosts, order, opts);
                    TRWSResult result = solver.run();
                    if (result.cancelled || cancelled()) {
                        ret.cancelled = true;
                        return ret;
                    }

                    // 結果を表示
                    det.itr = result.iterations;
                    det.energy = result.energy;

                    //qDebug() << "labels:";
                    std::vector<int> xs_result, ys_result;
                    xs_result.reserve(Np);
                    ys_result.reserve(Np);
                    for (int l : result.labels) {
                        //qDebug() << "l =" << l << " x =" << shiftsp[l].x() << "  y =" << shiftsp[l].y();
                        xs_result.push_back(shiftsp[l].x());
                        ys_result.push_back(shiftsp[l].y());
                    }

                    int xsum = std::count_if(xs_result.begin(), xs_result.end(),[](int x) { return x != 0; });
                    int ysum = std::count_if(ys_result.begin(), ys_result.end(),[](int x) { return x != 0; });
                    advanceProgress(tr("局所最適化 %1-%2")
                                        .arg(p.i + 1)
                                        .arg(p.j + 1));
                    if (xsum + ysum == 0) {
                        det.shuusoku = true;
                        break;
                    }

                    // poss更新
                    for (int i = p.i + 1; i <= p.j; ++i) {
                        in_poss[i] = QPoint(in_poss[i].x() + xs_result[i-p.i], in_poss[i].y() + ys_result[i-p.i]);
                    }
                    for (int i = p.j + 1; i < N; ++i) {
                        in_poss[i] = QPoint(in_poss[i].x() + xs_result[Np-1], in_poss[i].y() + ys_result[Np-1]);
                    }
                }

            }
            out_log lo;
            // ssim計算
            // 対象ペアを先に列挙
            std::vector<PairTask> taskss;
            const int expectN = N * (N - 1) / 2;
            taskss.reserve(expectN);
            lo.edge1.reserve(expectN);
            lo.edge2.reserve(expectN);
            lo.ssim.reserve(expectN);
            //qDebug() << "edge lists (i,j)";
            for (int i = 0; i < N; ++i) {
                const QSize& res1 = in.res_all[i];
                const QPoint& pos1 = in_poss[i];

                for (int j = i + 1; j < N; ++j) {
                    const QSize& res2 = in.res_all[j];
                    const QPoint& pos2 = in_poss[j];

                    if (!defaultMayOverlap(res1, pos1, res2, pos2, 0)) {
                        continue;
                    }
                    taskss.push_back({i, j});
                    lo.edge1.push_back(i);
                    lo.edge2.push_back(j);
                }
            }
            /*
            int ts_count = 0;
            double ssim_min = 1.0;
            for (PairTask ts : taskss) {
                double s_v = SSIM_calc_oneshot(
                    SSIM_TaskInput{
                        in.imgs[ts.i], in.imgs[ts.j],
                        in.res_all[ts.i], in_poss[ts.i],
                        in.res_all[ts.j], in_poss[ts.j],
                        0, 0
                    }
                    );
                qDebug() << ts.i << ts.j << " :" << s_v;
                lo.ssim.push_back(s_v);
                if (s_v < ssim_min) {
                    ssim_min = s_v;
                }
                if (s_v < 0.1) {
                    ts_count++;
                }
            }
            */

            struct SsimEvalResult {
                int i;
                int j;
                double ssim;
            };

            QVector<PairTask> taskVec = QVector<PairTask>(taskss.begin(), taskss.end());

            QVector<SsimEvalResult> results = QtConcurrent::blockingMapped<QVector<SsimEvalResult>>(
                taskVec,
                [&](const PairTask& ts) -> SsimEvalResult {
                    if (cancelled()) return SsimEvalResult{ts.i, ts.j, 0.0};
                    double s_v = SSIM_calc_oneshot(
                        SSIM_TaskInput{
                            in.imgs[ts.i], in.imgs[ts.j],
                            in.res_all[ts.i], in_poss[ts.i],
                            in.res_all[ts.j], in_poss[ts.j],
                            0, 0
                        }
                        );
                    return SsimEvalResult{ts.i, ts.j, s_v};
                }
                );
            if (cancelled()) {
                ret.cancelled = true;
                return ret;
            }

            int ts_count = 0;
            double ssim_min = 1.0;
            lo.ssim.clear();
            lo.ssim.reserve(results.size());

            for (const auto& r : std::as_const(results)) {
                //qDebug() << r.i << r.j << " :" << r.ssim;
                lo.ssim.push_back(r.ssim);

                if (r.ssim < ssim_min) {
                    ssim_min = r.ssim;
                }
                if (r.ssim < 0.1) {
                    ++ts_count;
                }
            }
            //qDebug() << "low ssim :" << ts_count;
            det.lowSSIM_num = ts_count;
            det.minSSIM = ssim_min;
            // 変数格納
            ret.detail.push_back(det);
            ret.log.push_back(lo);
            ret.poss = in_poss;
            advanceProgress(tr("局所最適化 SSIM %1-%2")
                                .arg(p.i + 1)
                                .arg(p.j + 1));
        }
        }
    }

    // 全体最適化
    if (in.all_TF) {
        if (!in.pa_TF) {
            progressMaximum = std::max(1, in.all_itr + 1);
            reportProgress(progressValue, progressMaximum, tr("全体最適化を準備中"));
        }

        const int K = ((in.all_radi * 2) + 1) * ((in.all_radi * 2) + 1);

        // 状態シフト
        QVector<QPoint> shifts;
        shifts.reserve(K);
        for (int y = -in.all_radi; y <= in.all_radi; ++y) {
            for (int x = -in.all_radi; x <= in.all_radi; ++x) {
                shifts.push_back(QPoint(x, y));
            }
        }

        // (k1, k2) -> (dx, dy) を前計算
        std::vector<ShiftDelta> deltas;
        deltas.resize(K * K);
        for (int k1 = 0; k1 < K; ++k1) {
            for (int k2 = 0; k2 < K; ++k2) {
                deltas[k1 * K + k2] = {
                    shifts[k2].x() - shifts[k1].x(),
                    shifts[k2].y() - shifts[k1].y()
                };
            }
        }

        out_detail det;
        det.PaAll = false;
        det.start = 1;
        det.end = N;
        det.shuusoku = false;

        int witr = 0;
        while (witr < in.all_itr) {
            if (cancelled()) {
                ret.cancelled = true;
                return ret;
            }
            witr++;
            det.loop = witr;
            // 対象ペアを先に列挙
            std::vector<PairTask> tasks;
            tasks.reserve(N * (N - 1) / 2);

            //qDebug() << "edge lists (i,j)";
            for (int i = 0; i < N; ++i) {
                const QSize& res1 = in.res_all[i];
                const QPoint& pos1 = in_poss[i];

                for (int j = i + 1; j < N; ++j) {
                    const QSize& res2 = in.res_all[j];
                    const QPoint& pos2 = in_poss[j];

                    if (!defaultMayOverlap(res1, pos1, res2, pos2, in.all_radi)) {
                        continue;
                    }
                    tasks.push_back({i, j});
                }
            }

            int maxJ = -1;
            for (const auto& t : tasks) {
                if (t.i == 0 && t.j > maxJ) {
                    maxJ = t.j;
                }
            }

            if (maxJ == -1) {
                ret.err = "No loop structure was found.";
                return ret;
            }
            int kaisu = maxJ / (in.pa_num * 2) + 1;
            if (kaisu == 0) {
                ret.err = "Something seems wrong.";
                return ret;
            }

            // 必要ならスレッド数調整
            //QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());

            auto mapFunc = [&](const PairTask& t) -> PairResult {
                return processOnePair(t, in.imgs, in.res_all, in_poss,
                                      deltas, K, in.all_radi, in.vulkan,
                                      in.cancelRequested);
            };
            auto reduceFunc = [&](QVector<PairResult>& out, const PairResult& r) {
                if (r.valid) {
                    out.push_back(r);
                }
            };

            QVector<PairResult> results =
                QtConcurrent::blockingMappedReduced<QVector<PairResult>>(
                    tasks, mapFunc, reduceFunc, QtConcurrent::OrderedReduce);
            if (cancelled()) {
                ret.cancelled = true;
                return ret;
            }

            std::vector<std::pair<int,int>> edges;
            std::vector<std::vector<double>> pairCosts;
            edges.reserve(results.size());
            pairCosts.reserve(results.size());

            for (const auto& r : std::as_const(results)) {
                edges.push_back({r.i, r.j});
                pairCosts.push_back(r.costs);
                //qDebug() << r.i << r.j;
            }

            std::vector<int> order(N);
            for (int i = 0; i < N; ++i) {
                order[i] = i;
            }

            TRWS::Options opts;
            opts.maxIter = in.all_opti;
            opts.tol = 1e-8;
            opts.stallIters = 10;
            opts.shouldCancel = cancelled;
            std::vector<std::vector<double>> unaryCosts(N, std::vector<double>(K, 0.0));
            int fixedLabel = -1;
            for (int l = 0; l < K; ++l) {
                if (shifts[l].x() == 0 && shifts[l].y() == 0) {
                    fixedLabel = l;
                    break;
                }
            }
            if (fixedLabel < 0) {
                ret.err = "fixed label not found";
                return ret;
            }
            const double INF = 1e100;
            for (int l = 0; l < K; ++l) {
                unaryCosts[maxJ][l] = (l == fixedLabel) ? 0.0 : INF;
            }

            TRWS solver(N, K, unaryCosts, edges, pairCosts, order, opts);
            TRWSResult result = solver.run();
            if (result.cancelled || cancelled()) {
                ret.cancelled = true;
                return ret;
            }

            det.itr = result.iterations;
            det.energy = result.energy;
            /*
            qDebug() << "iterations = " << result.iterations;
            qDebug() << "energy     = " << result.energy;
            qDebug() << "reward     = " << result.reward;

            qDebug() << "labels:";
            for (int l : result.labels) {
                qDebug() << "x =" << shifts[l].x() << "  y =" << shifts[l].y();
            }
            */

            /*
            qDebug() << "lower bound history:";
            for (double v : result.lowerBoundHistory) {
                qDebug() << v;
            }
            */

            // 最適ラベルを反映
            for (int i = 0; i < N; ++i) {
                const int label = result.labels[i];
                in_poss[i] = QPoint(in_poss[i].x() + shifts[label].x(),
                                 in_poss[i].y() + shifts[label].y());
            }


            // 収束判定
            std::vector<int> xs_result, ys_result;
            xs_result.reserve(N);
            ys_result.reserve(N);
            for (int l : result.labels) {
                xs_result.push_back(shifts[l].x());
                ys_result.push_back(shifts[l].y());
            }

            int xsum = std::count_if(xs_result.begin(), xs_result.end(),[](int x) { return x != 0; });
            int ysum = std::count_if(ys_result.begin(), ys_result.end(),[](int x) { return x != 0; });
            advanceProgress(tr("全体最適化 %1/%2").arg(witr).arg(in.all_itr));
            if (xsum + ysum == 0) {
                det.shuusoku = true;
                break;
            }
        }

        out_log lo;
        // ssim計算
        // 対象ペアを先に列挙
        std::vector<PairTask> taskss;
        const int expectN = N * (N - 1) / 2;
        taskss.reserve(expectN);
        lo.edge1.reserve(expectN);
        lo.edge2.reserve(expectN);
        lo.ssim.reserve(expectN);
        //qDebug() << "edge lists (i,j)";
        for (int i = 0; i < N; ++i) {
            const QSize& res1 = in.res_all[i];
            const QPoint& pos1 = in_poss[i];

            for (int j = i + 1; j < N; ++j) {
                const QSize& res2 = in.res_all[j];
                const QPoint& pos2 = in_poss[j];

                if (!defaultMayOverlap(res1, pos1, res2, pos2, 0)) {
                    continue;
                }
                taskss.push_back({i, j});
                lo.edge1.push_back(i);
                lo.edge2.push_back(j);
            }
        }
        /*
        int ts_count = 0;
        double ssim_min = 1.0;
        for (PairTask ts : taskss) {
            double s_v = SSIM_calc_oneshot(
                SSIM_TaskInput{
                    in.imgs[ts.i], in.imgs[ts.j],
                    in.res_all[ts.i], in_poss[ts.i],
                    in.res_all[ts.j], in_poss[ts.j],
                    0, 0
                }
                );
            qDebug() << ts.i << ts.j << " :" << s_v;
            lo.ssim.push_back(s_v);
            if (s_v < ssim_min) {
                ssim_min = s_v;
            }
            if (s_v < 0.1) {
                ts_count++;
            }
        }
        */
        struct SsimEvalResult {
            int i;
            int j;
            double ssim;
        };

        QVector<PairTask> taskVec = QVector<PairTask>(taskss.begin(), taskss.end());

        QVector<SsimEvalResult> results = QtConcurrent::blockingMapped<QVector<SsimEvalResult>>(
            taskVec,
            [&](const PairTask& ts) -> SsimEvalResult {
                if (cancelled()) return SsimEvalResult{ts.i, ts.j, 0.0};
                double s_v = SSIM_calc_oneshot(
                    SSIM_TaskInput{
                        in.imgs[ts.i], in.imgs[ts.j],
                        in.res_all[ts.i], in_poss[ts.i],
                        in.res_all[ts.j], in_poss[ts.j],
                        0, 0
                    }
                    );
                return SsimEvalResult{ts.i, ts.j, s_v};
            }
            );
        if (cancelled()) {
            ret.cancelled = true;
            return ret;
        }

        int ts_count = 0;
        double ssim_min = 1.0;
        lo.ssim.clear();
        lo.ssim.reserve(results.size());

        for (const auto& r : std::as_const(results)) {
            //qDebug() << r.i << r.j << " :" << r.ssim;
            lo.ssim.push_back(r.ssim);

            if (r.ssim < ssim_min) {
                ssim_min = r.ssim;
            }
            if (r.ssim < 0.1) {
                ++ts_count;
            }
        }

        //qDebug() << "low ssim :" << ts_count;
        det.lowSSIM_num = ts_count;
        det.minSSIM = ssim_min;

        ret.detail.push_back(det);
        ret.log.push_back(lo);
        ret.poss = in_poss;
        advanceProgress(tr("全体最適化 SSIM"));
    }
    ret.err = "";
    if (cancelled()) {
        ret.cancelled = true;
        return ret;
    }
    reportProgress(progressMaximum, progressMaximum, tr("最適化完了"));
    return ret;
}

void MainWindow::show_opti_settings()
{
    if (!optimizationSettingsDialog) {
        optimizationSettingsDialog = new opti_settings(this);
        optimizationSettingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);
        optimizationSettingsDialog->setModal(false);
        optimizationSettingsDialog->setWindowModality(Qt::NonModal);
        trackDialogSize(optimizationSettingsDialog,
                        QStringLiteral("trwsPamiSettings"),
                        QSize(550, 398));
        optimizationSettingsDialog->setValues(
            pa_num, pa_radi, pa_opti, pa_itr,
            all_radi, all_opti, all_itr,
            pa_TF, all_TF, pa_auto_increment_TF,
            pa_increment, pa_increment_count);
        connect(optimizationSettingsDialog, &QDialog::accepted,
                this, &MainWindow::readOptimizationSettingsDialog);
    }
    optimizationSettingsDialog->show();
    optimizationSettingsDialog->raise();
    optimizationSettingsDialog->activateWindow();
}

void MainWindow::show_least_squares_settings()
{
    if (!leastSquaresSettingsDialog) {
        leastSquaresSettingsDialog = new least_squares_settings(this);
        leastSquaresSettingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);
        leastSquaresSettingsDialog->setModal(false);
        leastSquaresSettingsDialog->setWindowModality(Qt::NonModal);
        trackDialogSize(leastSquaresSettingsDialog,
                        QStringLiteral("leastSquaresSettings"),
                        QSize(430, 190));
        leastSquaresSettingsDialog->setValues(
            leastSquaresSettings.regressionThreshold,
            leastSquaresSettings.relativeThreshold,
            leastSquaresSettings.absoluteThreshold,
            leastSquaresSettings.maxPairErrorForRelative);
        connect(leastSquaresSettingsDialog, &QDialog::accepted,
                this, &MainWindow::readLeastSquaresSettingsDialog);
    }
    leastSquaresSettingsDialog->show();
    leastSquaresSettingsDialog->raise();
    leastSquaresSettingsDialog->activateWindow();
}

void MainWindow::show_merge_settings()
{
    if (isAlignmentOrOptimizationRunning()) {
        return;
    }

    if (!mergeSettingsDialog) {
        mergeSettingsDialog = new merge_settings(this);
        mergeSettingsDialog->setAttribute(Qt::WA_DeleteOnClose, false);
        mergeSettingsDialog->setModal(false);
        mergeSettingsDialog->setWindowModality(Qt::NonModal);
        trackDialogSize(mergeSettingsDialog,
                        QStringLiteral("imageMergeSettings"),
                        QSize(430, 190));
        mergeSettingsDialog->setMode(static_cast<int>(imageMergeSettings.mode));
        connect(mergeSettingsDialog, &QDialog::accepted,
                this, &MainWindow::readMergeSettingsDialog);
    }
    mergeSettingsDialog->show();
    mergeSettingsDialog->raise();
    mergeSettingsDialog->activateWindow();
}

void MainWindow::readOptimizationSettingsDialog()
{
    if (!optimizationSettingsDialog) return;
    const std::vector<int> values = optimizationSettingsDialog->getValues();
    const std::vector<bool> flags = optimizationSettingsDialog->getTFs();
    if (values.size() < 9 || flags.size() < 3) return;

    pa_num = values[0];
    pa_radi = values[1];
    pa_opti = values[2];
    pa_itr = values[3];
    all_radi = values[4];
    all_opti = values[5];
    all_itr = values[6];
    pa_increment = values[7];
    pa_increment_count = values[8];
    pa_TF = flags[0];
    all_TF = flags[1];
    pa_auto_increment_TF = flags[2];
    ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));

    TrwsPamiDefaultSettings settings;
    settings.localEnabled = pa_TF;
    settings.localImageCount = pa_num;
    settings.localAutoIncrement = pa_auto_increment_TF;
    settings.localImageCountIncrement = pa_increment;
    settings.localIncrementCount = pa_increment_count;
    settings.localSearchRadius = pa_radi;
    settings.localMaxIterations = pa_opti;
    settings.localMaxLoops = pa_itr;
    settings.globalEnabled = all_TF;
    settings.globalSearchRadius = all_radi;
    settings.globalMaxIterations = all_opti;
    settings.globalMaxLoops = all_itr;
    AppSettings::setTrwsPamiOptions(settings);
}

void MainWindow::readLeastSquaresSettingsDialog()
{
    if (!leastSquaresSettingsDialog) return;
    const auto values = leastSquaresSettingsDialog->getValues();
    leastSquaresSettings.regressionThreshold = values[0];
    leastSquaresSettings.relativeThreshold = values[1];
    leastSquaresSettings.absoluteThreshold = values[2];
    leastSquaresSettings.maxPairErrorForRelative = values[3];
    LeastSquaresDefaultSettings settings;
    settings.regressionThreshold = leastSquaresSettings.regressionThreshold;
    settings.relativeThreshold = leastSquaresSettings.relativeThreshold;
    settings.absoluteThreshold = leastSquaresSettings.absoluteThreshold;
    settings.maxPairErrorForRelative =
        leastSquaresSettings.maxPairErrorForRelative;
    AppSettings::setLeastSquaresOptions(settings);
}

void MainWindow::readMergeSettingsDialog()
{
    if (!mergeSettingsDialog) return;
    imageMergeSettings.mode = static_cast<ImageMergeMode>(mergeSettingsDialog->getMode());
    ImageMergeDefaultSettings settings;
    settings.mode = static_cast<int>(imageMergeSettings.mode);
    AppSettings::setImageMergeOptions(settings);
}

void MainWindow::scheduleSettingsPersistence()
{
    if (!suppressSettingsPersistence && settingsPersistenceTimer) {
        settingsPersistenceTimer->start();
    }
}

void MainWindow::persistAlignmentSettings()
{
    if (suppressSettingsPersistence || !ui) {
        return;
    }
    AlignmentDefaultSettings settings;
    settings.horizontalOverlapPercent = ui->spinBox_2->value();
    settings.verticalOverlapPercent = ui->spinBox_3->value();
    settings.searchRangePercent = ui->spinBox->value();
    AppSettings::setAlignmentOptions(settings);
}

void MainWindow::persistArrangementSettings()
{
    if (suppressSettingsPersistence || !ui) {
        return;
    }
    ArrangementDefaultSettings settings;
    settings.direction = ui->cornerSelector->getStatus();
    settings.horizontalImageCount = std::max(0, configuredHorizontalImageCount);
    settings.verticalImageCount = std::max(0, configuredVerticalImageCount);
    settings.zigzag = ui->cornerSelector->zigzagChecked();
    AppSettings::setArrangementOptions(settings);
}

void MainWindow::persistControlPanelWidth()
{
    if (suppressSettingsPersistence || !ui || !ui->mainSplitter) {
        return;
    }
    const QList<int> sizes = ui->mainSplitter->sizes();
    if (!sizes.isEmpty()) {
        AppSettings::setControlPanelWidth(
            std::max(ui->leftScrollArea->minimumWidth(), sizes.first()));
    }
}

void MainWindow::trackDialogSize(QDialog* dialog,
                                 const QString& key,
                                 const QSize& defaultSize)
{
    if (!dialog || trackedDialogSizeKeys.contains(dialog)) {
        return;
    }
    trackedDialogSizeKeys.insert(dialog, key);
    trackedDialogDefaultSizes.insert(dialog, defaultSize);
    dialog->resize(AppSettings::windowSize(key, defaultSize));
    connect(dialog, &QDialog::finished, this, [dialog, key]() {
        AppSettings::setWindowSize(key, dialog->size());
    });
    connect(dialog, &QObject::destroyed, this, [this, dialog]() {
        trackedDialogSizeKeys.remove(dialog);
        trackedDialogDefaultSizes.remove(dialog);
    });
}

void MainWindow::saveTrackedWindowSizes()
{
    for (auto it = trackedDialogSizeKeys.constBegin();
         it != trackedDialogSizeKeys.constEnd(); ++it) {
        if (it.key()) {
            AppSettings::setWindowSize(it.value(), it.key()->size());
        }
    }
}

void MainWindow::resetOpenWindowSizes()
{
    resize(QSize(1200, 750));
    const int minimumControlPanelWidth = ui->leftScrollArea->minimumWidth();
    const int availableWidth = std::max(1, ui->mainSplitter->width());
    ui->mainSplitter->setSizes(
        {minimumControlPanelWidth,
         std::max(1, availableWidth - minimumControlPanelWidth)});
    for (auto it = trackedDialogDefaultSizes.constBegin();
         it != trackedDialogDefaultSizes.constEnd(); ++it) {
        if (it.key()) {
            it.key()->resize(it.value());
        }
    }
}

void MainWindow::handleSettingsReset(SettingsResetCategory category)
{
    if (settingsPersistenceTimer) {
        settingsPersistenceTimer->stop();
    }

    const bool resetAll = category == SettingsResetCategory::All;
    if (resetAll) {
        AppSettings::resetAllUserSettings();
    } else {
        switch (category) {
        case SettingsResetCategory::ApplicationDialog:
            AppSettings::resetApplicationDialogSettings();
            break;
        case SettingsResetCategory::Alignment:
            AppSettings::resetAlignmentOptions();
            break;
        case SettingsResetCategory::Layout:
            AppSettings::resetArrangementOptions();
            break;
        case SettingsResetCategory::LeastSquares:
            AppSettings::resetLeastSquaresOptions();
            break;
        case SettingsResetCategory::TrwsPami:
            AppSettings::resetTrwsPamiOptions();
            break;
        case SettingsResetCategory::ImageMerge:
            AppSettings::resetImageMergeOptions();
            break;
        case SettingsResetCategory::All:
            break;
        }
    }

    suppressSettingsPersistence = true;

    if (resetAll || category == SettingsResetCategory::ApplicationDialog) {
        const image_stitcher::platform::ExplorerContextMenuResult explorerResult =
            image_stitcher::platform::setExplorerContextMenuEnabled(
                AppSettings::explorerContextMenuEnabled());
        if (!explorerResult.success) {
            showNonModalMessage(
                QMessageBox::Warning,
                tr("エクスプローラー"),
                tr("コンテキストメニューの設定を変更できませんでした。\n%1")
                    .arg(explorerResult.errorMessage));
        }
        if (applicationSettingsDialog) {
            applicationSettingsDialog->reloadFromSettings();
        } else {
            applyApplicationTheme(AppSettings::theme());
            applyApplicationLanguage(AppSettings::language());
        }
    }

    if (resetAll || category == SettingsResetCategory::Alignment) {
        const AlignmentDefaultSettings settings = AppSettings::alignmentOptions();
        const QSignalBlocker horizontalSliderBlocker(ui->horizontalSlider);
        const QSignalBlocker horizontalSpinBlocker(ui->spinBox_2);
        const QSignalBlocker verticalSliderBlocker(ui->horizontalSlider_2);
        const QSignalBlocker verticalSpinBlocker(ui->spinBox_3);
        const QSignalBlocker rangeSliderBlocker(ui->horizontalSlider_3);
        const QSignalBlocker rangeSpinBlocker(ui->spinBox);
        ui->horizontalSlider->setValue(settings.horizontalOverlapPercent);
        ui->spinBox_2->setValue(settings.horizontalOverlapPercent);
        ui->horizontalSlider_2->setValue(settings.verticalOverlapPercent);
        ui->spinBox_3->setValue(settings.verticalOverlapPercent);
        ui->horizontalSlider_3->setValue(settings.searchRangePercent);
        ui->spinBox->setValue(settings.searchRangePercent);
        arrangeSettingsChanged();
    }

    if (resetAll || category == SettingsResetCategory::Layout) {
        const ArrangementDefaultSettings settings = AppSettings::arrangementOptions();
        configuredHorizontalImageCount = settings.horizontalImageCount;
        configuredVerticalImageCount = settings.verticalImageCount;
        if (input_files.isEmpty()) {
            ui->cornerSelector->setUI(settings.direction);
        } else {
            set_array_value(settings.direction,
                            settings.horizontalImageCount,
                            settings.verticalImageCount);
        }
        ui->cornerSelector->setZigzagChecked(settings.zigzag);
        orikaeshi = ui->cornerSelector->zigzagChecked();
        arrangeSettingsChanged();
    }

    if (resetAll || category == SettingsResetCategory::LeastSquares) {
        const LeastSquaresDefaultSettings settings =
            AppSettings::leastSquaresOptions();
        leastSquaresSettings.regressionThreshold = settings.regressionThreshold;
        leastSquaresSettings.relativeThreshold = settings.relativeThreshold;
        leastSquaresSettings.absoluteThreshold = settings.absoluteThreshold;
        leastSquaresSettings.maxPairErrorForRelative =
            settings.maxPairErrorForRelative;
        if (leastSquaresSettingsDialog) {
            leastSquaresSettingsDialog->setValues(
                settings.regressionThreshold,
                settings.relativeThreshold,
                settings.absoluteThreshold,
                settings.maxPairErrorForRelative);
        }
    }

    if (resetAll || category == SettingsResetCategory::TrwsPami) {
        const TrwsPamiDefaultSettings settings = AppSettings::trwsPamiOptions();
        pa_TF = settings.localEnabled;
        pa_num = settings.localImageCount;
        pa_auto_increment_TF = settings.localAutoIncrement;
        pa_increment = settings.localImageCountIncrement;
        pa_increment_count = settings.localIncrementCount;
        pa_radi = settings.localSearchRadius;
        pa_opti = settings.localMaxIterations;
        pa_itr = settings.localMaxLoops;
        all_TF = settings.globalEnabled;
        all_radi = settings.globalSearchRadius;
        all_opti = settings.globalMaxIterations;
        all_itr = settings.globalMaxLoops;
        if (optimizationSettingsDialog) {
            optimizationSettingsDialog->setValues(
                pa_num, pa_radi, pa_opti, pa_itr,
                all_radi, all_opti, all_itr,
                pa_TF, all_TF, pa_auto_increment_TF,
                pa_increment, pa_increment_count);
        }
        ui->pushButton_3->setEnabled(calc1_finished_state && (pa_TF || all_TF));
    }

    if (resetAll || category == SettingsResetCategory::ImageMerge) {
        const ImageMergeDefaultSettings settings = AppSettings::imageMergeOptions();
        imageMergeSettings.mode = static_cast<ImageMergeMode>(settings.mode);
        if (mergeSettingsDialog) {
            mergeSettingsDialog->setMode(settings.mode);
        }
    }

    if (resetAll) {
        applyCanvasBackgroundSetting(AppSettings::canvasBackground());
        resetOpenWindowSizes();
    }

    suppressSettingsPersistence = false;
    if (statusMessageLabel) {
        statusMessageLabel->setText(tr("設定をデフォルト値に戻しました。"));
    }
}


void MainWindow::show_detail_opti()
{
    const int n = input_files.size();
    if (n == 0) {
        showNonModalMessage(QMessageBox::Warning, tr("最適化結果の詳細"),
                            tr("表示できるデータがありません。"));
        return;
    }
    if (!calc2_finished_state) {
        showNonModalMessage(
            QMessageBox::Warning, tr("最適化結果の詳細"),
            tr("表示するデータがありません。先に位置合わせ最適化計算を実行してください。"));
        return;
    }

    m_detailDialog->show();
}
