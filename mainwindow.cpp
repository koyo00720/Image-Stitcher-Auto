#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "file_input_wiz.h"
#include "image_utils.h"
#include "cornerdirectionselector.h"
#include "detail_dialog.h"
#include "trws.h"
#include "opti_settings.h"

#include <QFileDialog>
#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QAction>
#include <QGraphicsPixmapItem>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QtConcurrent/QtConcurrent>
#include <QIntValidator>
#include <QImageReader>
#include <QTimer>
#include <QFuture>
#include <QPointer>
#include <QDebug>
#include <QStandardItemModel>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <array>
//#include <numeric>

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
    float featherRadius = 80.0f)
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

// 2つの画像から重なり領域をクロップして取り出す
return_struct2 Crop_2ImageTo2Image(cv::Mat input1, cv::Mat input2, QSize px1, QPoint pos1, QSize px2, QPoint pos2)
{
    // 座標移動ベクトルを計算
    int dx = std::min(pos1.x(), pos2.x());
    int dy = std::min(pos1.y(), pos2.y());

    // キャンパスサイズを計算
    int camX = std::max(pos1.x() + px1.width(), pos2.x() + px2.width()) - dx;
    int camY = std::max(pos1.y() + px1.height(), pos2.y() + px2.height()) - dy;

    // キャンパスを作成し、各画像を割り当て
    cv::Mat cam1(camY, camX, CV_8UC4, cv::Scalar(0,0,0,0));
    cv::Rect roi(pos1.x() - dx, pos1.y() - dy, input1.cols, input1.rows); // 貼り付け先座標
    input1.copyTo(cam1(roi));

    cv::Mat cam2(camY, camX, CV_8UC4, cv::Scalar(0,0,0,0));
    roi = cv::Rect(pos2.x() - dx, pos2.y() - dy, input2.cols, input2.rows); // 貼り付け先座標
    input2.copyTo(cam2(roi));

    // Alphaをlogical配列へ変換
    cv::Mat1b logicalMask1 = ImageUtils::alphaMaskFromBGRA(cam1, 0.5); // 0/1
    cv::Mat1b logicalMask2 = ImageUtils::alphaMaskFromBGRA(cam2, 0.5); // 0/1

    // 重なり領域を得る
    cv::Mat1b andMask;
    cv::bitwise_and(logicalMask1, logicalMask2, andMask);

    // and領域を矩形化する
    cv::Rect rect = ImageUtils::maxRectOnesFromLogical(andMask);

    // 画像を3ch化
    cv::Mat cam1_3ch, cam2_3ch;
    cv::cvtColor(cam1, cam1_3ch, cv::COLOR_BGRA2BGR);
    cv::cvtColor(cam2, cam2_3ch, cv::COLOR_BGRA2BGR);

    // 重なり領域をcropして取り出す
    cv::Mat crop1 = cam1_3ch(rect).clone();
    cv::Mat crop2 = cam2_3ch(rect).clone();

    // 返り値を設定
    return_struct2 r;
    r.img1 = crop1;
    r.img2 = crop2;
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

    // 入力画像設定ボタン
    connect(ui->pushButton_1, &QPushButton::clicked, this, [this]() {
        FileInputDialog dlg(this->input_files, this);

        if (dlg.exec() == QDialog::Accepted) {
            QStringList input_files_new = dlg.selectedFiles();

            if (input_files_new != input_files)
            {
                // ファイルを読み込む
                File_input(input_files, input_files_new);
            }
        }
    });

    // 自動配列 or 手動配列 の選択
    ui->radioButton_5->setChecked(true);

    connect(ui->radioButton_5, &QRadioButton::toggled, this, [this](bool checked){
        if (checked) {
            ui->groupBox_2->setEnabled(true);
            ui->groupBox_3->setEnabled(true);
        }
    });

    connect(ui->radioButton_6, &QRadioButton::toggled, this, [this](bool checked){
        if (checked) {
            ui->groupBox_2->setEnabled(false);
            ui->groupBox_3->setEnabled(false);
        }
    });

    // 画像同士の重なり割合の設定
    connect(ui->horizontalSlider, &QSlider::valueChanged,
            ui->spinBox_2, &QSpinBox::setValue);
    connect(ui->spinBox_2, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider, &QSlider::setValue);
    connect(ui->spinBox_2, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::photo_Arrange);

    connect(ui->horizontalSlider_2, &QSlider::valueChanged,
            ui->spinBox_3, &QSpinBox::setValue);
    connect(ui->spinBox_3, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_2, &QSlider::setValue);
    connect(ui->spinBox_3, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::photo_Arrange);

    connect(ui->horizontalSlider_3, &QSlider::valueChanged,
            ui->spinBox, &QSpinBox::setValue);
    connect(ui->spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            ui->horizontalSlider_3, &QSlider::setValue);

    ui->spinBox->setValue(15);
    ui->horizontalSlider_3->setValue(15);
    //ui->spinBox_2->setValue(25);
    //ui->spinBox_3->setValue(25);

    // 配列指定エリアの設定
    connect(ui->cornerSelector, &CornerDirectionSelector::stateChanged,
            this, [this](int state) {
                photo_Arrange();
            });

    connect(ui->cornerSelector, &CornerDirectionSelector::r_Changed,
            this, [this](int rows){
                photo_Arrange();
            });

    connect(ui->cornerSelector, &CornerDirectionSelector::c_Changed,
            this, [this](int cols){
                photo_Arrange();
            });

    // 折り返し方法の選択
    ui->radioButton_3->setChecked(true);

    connect(ui->radioButton_3, &QRadioButton::toggled, this, [this](bool checked){
        if (checked) {
            //qDebug() << "折り返し = ジグザグ";
            orikaeshi = true;
            photo_Arrange();
        }
    });

    connect(ui->radioButton_4, &QRadioButton::toggled, this, [this](bool checked){
        if (checked) {
            //qDebug() << "折り返し = 一方向";
            orikaeshi = false;
            photo_Arrange();
        }
    });

    connect(ui->graphicsView, &maincampus::zoomChanged,
            this, [this](int pct){ zoomLabel->setText(QString("%1%").arg(pct)); });

    scene = new QGraphicsScene(this);

    ui->graphicsView->setScene(scene);

    // imageの削除
    auto *actDelete = new QAction(this);
    actDelete->setShortcut(QKeySequence::Delete);
    actDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut); // MainWindow配下で有効
    addAction(actDelete);
    connect(actDelete, &QAction::triggered, this, &MainWindow::deleteSelectedItems);

    // 拡大率表示
    zoomLabel = new QLabel(this);
    zoomLabel->setText("100%");
    statusBar()->addPermanentWidget(zoomLabel);

    // 透明度制御
    ui->sliderOpacity1->setRange(0, 100);
    ui->spinOpacity1->setRange(0, 100);
    ui->sliderOpacity1->setValue(0);
    ui->spinOpacity1->setValue(0);

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

    // 背景色を設定
    ui->comboBox->clear();
    ui->comboBox->addItems({"黒", "白"});

    auto applyBg = [this](int index) {
        ui->graphicsView->setBackgroundBrush(index == 0 ? Qt::black : Qt::white);
    };

    connect(scene, &QGraphicsScene::selectionChanged,
            this, &MainWindow::onSceneSelectionChanged);

    connect(ui->comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, applyBg);

    applyBg(ui->comboBox->currentIndex());

    // 計算開始ボタン
    connect(ui->pushButton_Calc1, &QPushButton::clicked, this, &MainWindow::calc_iFFT);

    // 計算完了通知を受け取る
    connect(&watcher, &QFutureWatcher<ifft_thread_output>::finished, this, &MainWindow::calc_finish_1);
    connect(&watcher_re, &QFutureWatcher<ifft_thread_output>::finished, this, &MainWindow::calc_finish_2);

    // 画像作成ボタン
    connect(ui->pushButton_2, &QPushButton::clicked, this, &MainWindow::make_image);
    ui->pushButton_2->setEnabled(false);

    // PNG exportボタン
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::png_export);

    // 結合品質の詳細
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::show_detail);

    // レイアウトロックのチェックボックス
    connect(ui->checkBox, &QCheckBox::toggled,this, &MainWindow::posi_lock);

    // 全体最適化ボタン
    connect(ui->pushButton_3, &QPushButton::clicked, this, &MainWindow::calc_TRWS);

    // 未実装部分をenableしない
    ui->radioButton_4->setEnabled(false);
    ui->radioButton_6->setEnabled(false);
    ui->pushButton_3->setEnabled(false);

    // 選択中の画像番号を表示
    ui->label_9->setText("なし");
    ui->label_8->setEnabled(false);
    ui->label_9->setEnabled(false);
    ui->pushButton->setEnabled(false);
    ui->pushButton_5->setEnabled(false);

    // 画像を作成 finish通知
    connect(&image_make_Watcher, &QFutureWatcher<cv::Mat>::finished, this, [this]() {
        output_img = image_make_Watcher.result();
        ui->label_6->setText("完了");
        ui->pushButton_2->setEnabled(true);
        if (calc_finish_sig) {
            emit makeimageFinished();
        }
    });
    //ui->spinBox_4->setValue(2);

    // 最適化ボタン
    connect(ui->pushButton_6, &QPushButton::clicked, this, &MainWindow::show_opti_settings);
    trwsWatcher = new QFutureWatcher<CalcTRWSoutput>(this);
    connect(trwsWatcher, &QFutureWatcher<CalcTRWSoutput>::finished, this, [this]() {calc_TRWS_finish();});

    // 最適化結果の詳細
    connect(ui->pushButton_5, &QPushButton::clicked, this, &MainWindow::show_detail_opti);

    m_detailDialog = new detail_opti_dialog(this);
}

MainWindow::~MainWindow()
{
    // 終了時の queued signal が破棄中オブジェクトへ届くのを防ぐ
    disconnect(&watcher, nullptr, this, nullptr);
    disconnect(&watcher_re, nullptr, this, nullptr);
    disconnect(&image_make_Watcher, nullptr, this, nullptr);
    disconnect(trwsWatcher, nullptr, this, nullptr);
    if (scene) {
        disconnect(scene, nullptr, this, nullptr);
    }

    if (watcher.isRunning()) { watcher.cancel(); watcher.waitForFinished(); }
    if (watcher_re.isRunning()) { watcher_re.cancel(); watcher_re.waitForFinished(); }
    if (image_make_Watcher.isRunning()) { image_make_Watcher.cancel(); image_make_Watcher.waitForFinished(); }
    if (trwsWatcher->isRunning()) { trwsWatcher->cancel(); trwsWatcher->waitForFinished(); }
    delete ui;
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
    ui->pushButton_1->setText("ファイル読み込み中...");
    ui->pushButton_1->repaint();
    QApplication::processEvents();
}

// CLI由来のファイルパスをチェックする
void MainWindow::File_input_check(QStringList cli_paths)
{
    // 再帰的展開
    QStringList result;
    QSet<QString> seen;

    for (const QString& path : cli_paths) {
        QFileInfo info(path);

        if (!info.exists()) {
            // 存在しないパスは無視
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
            continue;
        }

        // 読めたファイルだけ保持
        validFiles.append(path);
    }

    QStringList file_null;
    File_input(file_null,validFiles);

    ui->pushButton_1->setEnabled(true);
    ui->pushButton_1->setText("入力画像");

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

// manualかautoの設定
void MainWindow::set_manu_auto(int manu) {
    if (manu == 0) {
        ui->radioButton_6->setChecked(true);
    } else {
        ui->radioButton_5->setChecked(true);
    }
}

// 画像同士の重なりの目安を設定
void MainWindow::set_over_value(int o_x, int o_y, int o_r)
{
    ui->horizontalSlider->setValue(o_x);
    ui->horizontalSlider_2->setValue(o_y);
    ui->horizontalSlider_3->setValue(o_r);
}

// 画像の配列を設定
void MainWindow::set_array_value(int ar, int arh, int arv)
{
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
}

// 折り返しを指定
void MainWindow::set_zigzag_value(int g)
{
    if (g == 0) {
        ui->radioButton_4->setChecked(true);
    } else {
        ui->radioButton_3->setChecked(true);
    }
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
void MainWindow::File_input(const QStringList& paths_old, const QStringList& paths_new)
{
    const int n_new = paths_new.size();
    const int n_old = paths_old.size();
    //qDebug() << "new size " << n_new;
    //qDebug() << "old size " << n_old;
    const int n_max = std::max(n_new, n_old);

    // 適用前に全画像を検証して、途中失敗による状態不整合を避ける
    QVector<QPixmap> pix_new(n_new);
    for (int i = 0; i < n_new; ++i) {
        QPixmap pix(paths_new[i]);
        if (pix.isNull()) {
            QMessageBox::warning(
                this,
                tr("エラー"),
                tr("%1枚目の画像の読み込みに失敗しました。\nファイル入力ウィザードから削除してください。").arg(i + 1)
                );
            return;
        }
        pix_new[i] = pix;
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
        if (paths_old[i] == paths_new[i]) {
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
    ui->cornerSelector->setMax(input_files.size(),true);
    toumeido.fill(0, input_files.size());
};


// deleteで削除した場合
void MainWindow::deleteSelectedItems()
{
    if (ui->checkBox->isChecked()) return;
    const auto selected = scene->selectedItems();
    if (selected.isEmpty()) return;

    for (QGraphicsItem *it : selected) {
        int idx = items.indexOf(static_cast<QGraphicsPixmapItem*>(it)); // itemsの型に合わせる
        if (idx >= 0) {
            input_files.removeAt(idx);
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
}


void MainWindow::updateZoomLabel()
{
    const int pct = viewZoomPercent(ui->graphicsView);
    zoomLabel->setText(QString("%1%").arg(pct));
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
        ui->label_9->setText("なし");
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
    QVector<int> idx_list_show = idx_list;
    std::sort(idx_list_show.begin(), idx_list_show.end());
    QStringList parts;
    parts.reserve(idx_list_show.size());
    for (int x : idx_list_show) parts << QString::number(x+1);
    ui->label_9->setText(parts.join(", "));

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

    try {
        // 最大4回計算して安定するか確認する
        for (int l = 0; l < in.calc_loop_num; ++l) {

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
    if (watcher.isRunning()) return;

    // 画像があるか判定
    const int n = input_files.size();
    if (n <= 1) {
        QMessageBox::warning(this, "計算", "2枚以上の画像を入力してください。");
        return;
    }

    ui->label_5->setText("計算中");
    ui->checkBox->setChecked(true);
    ui->checkBox->setEnabled(false);
    //ui->groupBox_5->setEnabled(false);
    ui->pushButton_Calc1->setEnabled(false);
    //ui->pushButton->setEnabled(false);
    ui->label_4->setEnabled(false);
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    ui->label_6->setText("");
    ui->label_12->setText("");
    ui->pushButton_3->setEnabled(false);
    //ui->pushButton_5->setEnabled(false);
    ui->pushButton_6->setEnabled(false);

    // 画像データをOpenCV向けに変換
    imgs.resize(n);
    for (int i = 0; i < n; ++i) {
        QImage img_QI = items[i]->pixmap().toImage();
        imgs[i] = ImageUtils::qimage_to_mat_bgra(img_QI);
    }

    // 並列処理向けの入力値を作成する
    QVector<ifft_thread_input> inputs;
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
                inputs[i*4+j].input1 = imgs[i].clone();
                inputs[i*4+j].input2 = imgs[i+1].clone();
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
            inputs[i].input1 = imgs[i].clone();
            inputs[i].input2 = imgs[i+1].clone();
            inputs[i].px1 = res_all[i];
            inputs[i].px2 = res_all[i+1];
            inputs[i].pos1 = pos_all[i];
            inputs[i].pos2 = pos_all[i+1];
            inputs[i].calc_loop_num = calc_loop_num;
        }
    }
    // 並列計算開始
    QFuture<ifft_thread_output> future = QtConcurrent::mapped(inputs, calc_oneshot);
    // 監視開始
    watcher.setFuture(future);
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
                    inputs[i*ovc*4+j*4+k].input1 = imgs[i2id[i]].clone();
                    inputs[i*ovc*4+j*4+k].input2 = imgs[i2id[i]+1].clone();
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
                inputs[i*ovc+j].input1 = imgs[i2id[i]].clone();
                inputs[i*ovc+j].input2 = imgs[i2id[i]+1].clone();
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
    // 並列計算開始
    QFuture<ifft_thread_output> future = QtConcurrent::mapped(inputs, calc_oneshot);
    // 監視開始
    watcher_re.setFuture(future);
}

void MainWindow::calc_finish_1()
{
    calc_results = watcher.future().results();
    const bool has_calc_error = std::any_of(calc_results.cbegin(), calc_results.cend(),
                                            [](const ifft_thread_output& r){ return r.calc_error; });
    if (has_calc_error) {
        ui->label_5->setText("計算失敗");
        output_img.release();
        //ui->groupBox_5->setEnabled(true);
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton->setEnabled(false);
        ui->checkBox->setEnabled(false);
        ui->label_4->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->label_6->setText("");
        return;
    }
    int n = input_files.size();
    int m_layoutState = ui->cornerSelector->getStatus();
    int m_rows = ui->cornerSelector->getRows();
    int m_cols = ui->cornerSelector->getCols();
    idou_dir.resize(n-1);
/*
    for (int p = 0; p < calc_results.size(); ++p) {
        qDebug() << p << calc_results[p].vecXi << calc_results[p].vecX;
    }
*/

    if (m_rows == 0 && m_cols == 0) {
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

    int overR = ui->horizontalSlider_3->value(); // 探索範囲
    if (countF_calc >= 1 && overR >= re_step) { // 再計算の実行
        ui->label_5->setText("不良箇所を再計算中");
        calc_iFFT_rerun();
        return;
    }

    if (countF == 0) {
        ui->label_5->setText("良好");
        ryoukou = true;
    } else {
        ui->label_5->setText(QString::number(countF) + " ヶ所不良");
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

    //ui->groupBox_5->setEnabled(true);
    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->checkBox->setEnabled(true);
    ui->label_4->setEnabled(true);
    ui->pushButton_4->setEnabled(true);
    ui->pushButton_2->setEnabled(true);

    ui->pushButton_3->setEnabled(true);
    //ui->pushButton_5->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
    calc1_finished_state = true;

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
    const bool has_calc_error = std::any_of(calc_results_re.cbegin(), calc_results_re.cend(),
                                            [](const ifft_thread_output& r){ return r.calc_error; });
    if (has_calc_error) {
        ui->label_5->setText("計算失敗");
        output_img.release();
        //ui->groupBox_5->setEnabled(true);
        ui->pushButton_Calc1->setEnabled(true);
        ui->pushButton->setEnabled(false);
        ui->checkBox->setEnabled(true);
        ui->label_4->setEnabled(false);
        ui->pushButton_4->setEnabled(false);
        ui->pushButton_2->setEnabled(false);
        ui->label_6->setText("");
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
        ui->label_5->setText("良好");
        ryoukou = true;
    } else {
        ui->label_5->setText(QString::number(countF) + " ヶ所不良");
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

    //ui->groupBox_5->setEnabled(true);
    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->checkBox->setEnabled(true);
    ui->label_4->setEnabled(true);
    ui->pushButton_4->setEnabled(true);
    ui->pushButton_2->setEnabled(true);
    ui->pushButton_3->setEnabled(true);
    //ui->pushButton_5->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
    calc1_finished_state = true;

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
            QMessageBox::warning(this, "PNG export", "出力できる画像がありません。");
            return;
        }

        QFileInfo fi(input_files[0]);
        QDir dir = fi.dir();

        QString newName = "stitched_" + fi.completeBaseName() + ".png";
        QString initialPath = dir.filePath(newName);

        QString newpath = QFileDialog::getSaveFileName(
            this,
            "Save File",
            initialPath,
            "PNG Image (*.png);;All Files (*.*)"
            );

        if (newpath.isEmpty()) // キャンセルが押された場合
        {
            return;
        }

        QImage qimg(output_img.data, output_img.cols, output_img.rows, output_img.step, QImage::Format_ARGB32);
        qimg.save(newpath, "PNG");
    }
}

void MainWindow::set_output_path(QString out_p)
{
    output_file = out_p;
};

void MainWindow::cli_exp_image() {
    if (input_files.size() == 0 || output_img.empty()) {
        QMessageBox::warning(this, "PNG export", "出力できる画像がありません。");
        return;
    }
    QImage qimg(output_img.data, output_img.cols, output_img.rows, output_img.step, QImage::Format_ARGB32);

    bool ok = qimg.save(output_file, "PNG");
    if (!ok) {
        QMessageBox::critical(this, "保存エラー",
                              "画像を保存できませんでした。\n"
                              "出力先パスを確認してください。\n\n"
                              "Path: " + output_file);
        return;
    }

    ui->pushButton_4->setText("PNG エクスポート: 完了");

    if (closeTFm) {
        QCoreApplication::quit();
    }

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
}

void MainWindow::show_detail()
{
    const int n = input_files.size();
    if (n == 0) {
        QMessageBox::warning(this, "詳細", "表示できるデータがありません。");
        return;
    }
    if (n < 2 || calc_results.size() < (n - 1) || checkTF.size() < (n - 1)) {
        QMessageBox::warning(this, "詳細", "表示するデータがありません。先に位置合わせ計算を実行してください。");
        return;
    }

    auto* model = new QStandardItemModel(this);
    model->setColumnCount(7);
    model->setHorizontalHeaderLabels({"画像1","画像2","計算回数","安定性","位相相関スコア","SSIM", "品質"});
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
            auto* it = new QStandardItem(st ? QStringLiteral("安定") : QStringLiteral("不安定"));
            it->setData(st ? 1 : 0, Qt::UserRole);
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
            auto* it = new QStandardItem(ok ? QStringLiteral("良") : QStringLiteral("不良"));
            it->setData(ok ? 1 : 0, Qt::UserRole);
            model->setItem(i, 6, it);
        }
    }
    auto* dlg = new Detail_Dialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(model);
    dlg->setModal(false);
    dlg->show();
}


void MainWindow::posi_lock(bool checked) {
    if (checked) {
        ui->pushButton_1->setEnabled(false);
        ui->groupBox->setEnabled(false);
        ui->groupBox_2->setEnabled(false);
        ui->groupBox_3->setEnabled(false);
        ui->groupBox_4->setEnabled(false);
    } else {
        ui->pushButton_1->setEnabled(true);
        ui->groupBox->setEnabled(true);
        ui->groupBox_2->setEnabled(true);
        ui->groupBox_3->setEnabled(true);
        ui->groupBox_4->setEnabled(true);
    }
}

void MainWindow::make_image()
{
    const int n = input_files.size();
    if (n == 0) {
        QMessageBox::warning(this, "画像作成", "出力できる画像がありません。");
        return;
    }
    if (imgs.size() != n || poss.size() != n) {
        QMessageBox::warning(this, "画像作成", "先に位置合わせ計算を実行してください。");
        return;
    }

    ui->label_6->setText("作成中");
    ui->pushButton_2->setEnabled(false);
    ui->pushButton_4->setText("PNG エクスポート");

    // 別スレッドに渡すために必要データをコピー
    auto imgs_copy = imgs;
    auto poss_copy = poss;

    // ワーカースレッドで実行
    image_make_Watcher.setFuture(QtConcurrent::run([imgs_copy, poss_copy, n]() -> cv::Mat {
        cv::Mat out = imgs_copy[0].clone();
        int minX = 0, minY = 0;

        for (int i = 0; i < n - 1; ++i) {
            minX = std::min(poss_copy[i].x(), minX);
            minY = std::min(poss_copy[i].y(), minY);
            cv::Point2d shiftV(poss_copy[i+1].x() - minX, poss_copy[i+1].y() - minY);

            out = make_canvas_bgra_feather_dt(imgs_copy[i+1], out, shiftV, 80.0f);
        }
        return out;
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
    int radi)
{
    PairResult result;
    result.i = task.i;
    result.j = task.j;

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

    for (int idx = 0; idx < K * K; ++idx) {
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
            cachedCosts[ci] = ((1 - s_v) / s_v) * ((1 - s_v) / s_v) / 81;
            computed[ci] = 1;
        }

        result.costs[idx] = cachedCosts[ci];
    }

    result.valid = true;
    return result;
}
}

void MainWindow::calc_TRWS()
{
    if (trwsRunning) {
        return; // 二重起動防止
    }
    trwsRunning = true;
    ui->label_12->setText("計算中");

    ui->pushButton_Calc1->setEnabled(false);
    ui->pushButton_3->setEnabled(false);
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    //ui->pushButton_5->setEnabled(false);
    ui->pushButton_6->setEnabled(false);

    ui->checkBox->setChecked(true);
    ui->checkBox->setEnabled(false);

    //ui->groupBox_5->setEnabled(false);

    CalcTRWSinput input;
    input.imgs = imgs;
    input.res_all = res_all;
    input.poss = poss;
    input.pa_TF = pa_TF;
    input.pa_num = pa_num;
    input.pa_radi = pa_radi;
    input.pa_opti = pa_opti;
    input.pa_itr = pa_itr;
    input.all_TF = all_TF;
    input.all_radi = all_radi;
    input.all_opti = all_opti;
    input.all_itr = all_itr;

    auto future = QtConcurrent::run([=]() {
        return calc_TRWS_core(input);
    });

    trwsWatcher->setFuture(future);

}

void MainWindow::calc_TRWS_finish()
{

    const CalcTRWSoutput out = trwsWatcher->result();
    if (out.err.empty()) {
        ui->label_12->setText("完了");
        poss = out.poss;
        const int N = poss.size();

        // detail dialogへデータを投げる
        const int odn = out.detail.size();
        for (int od = 0; od < odn; ++od) {
            m_detailDialog->setData(out.detail[od],out.log[od]);
        }

        bool ryoukou2 = false;
        if (out.detail[odn-1].minSSIM > 0.1) {
            ui->label_12->setText("良好");
            ryoukou2 = true;
        } else {
            ui->label_12->setText("一部不良");
        }

        // UI更新
        for (int i = 0; i < N; ++i) {
            items[i]->setPos(poss[i]);
        }

        if (calc_finish_sig && ryoukou2) {
            emit calcFinished();
        }
    } else {
        ui->label_12->setText("不良");
        QMessageBox::warning(this, "最適化計算", QString::fromStdString(out.err));

    }
    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton_4->setEnabled(true);
    ui->pushButton_2->setEnabled(true);
    ui->pushButton_3->setEnabled(true);
    ui->pushButton_5->setEnabled(true);
    ui->pushButton_6->setEnabled(true);
    ui->checkBox->setEnabled(true);

    calc2_finished_state = true;
    /*
    std::vector<out_detail> det = out.detail;
    for (int d = 0; d < det.size(); ++d) {
        qDebug() << det[d].PaAll << det[d].start << det[d].end << det[d].itr << det[d].loop << det[d].shuusoku << det[d].lowSSIM_num << det[d].minSSIM << det[d].energy;
    }
    */
    trwsRunning = false;


}

CalcTRWSoutput MainWindow::calc_TRWS_core(CalcTRWSinput in)
{
    CalcTRWSoutput ret;
    const int N = in.poss.size();
    QVector<QPoint> in_poss = in.poss;
    // 部分最適化
    if (in.pa_TF) {
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

        std::vector<PairTask> imgId_list;
        for (int ka = 1; ka <= kaisu; ++ka) {
            std::vector<int> matchedI;
            for (const auto& t : tasks) {
                if (t.j - t.i == in.pa_num * ka) {
                    matchedI.push_back(t.i);
                }
            }
            if (matchedI.size() != 0) {
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
                    imgId_list.push_back({mi, mi + (in.pa_num * ka)});
                }
            } else {
                ret.err = "No large loop structure was found.";
                return ret;
            }
        }

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
                    whi++;
                    det.loop = whi;
                    // コスト計算用関数を作成
                    //QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount());

                    auto mapFunc = [&](const PairTask& t) -> PairResult {
                        return processOnePair(t, in.imgs, in.res_all, in_poss, deltasp, Kp, in.pa_radi);
                    };
                    auto reduceFunc = [&](QVector<PairResult>& out, const PairResult& r) {
                        if (r.valid) {
                            out.push_back(r);
                        }
                    };
                    QVector<PairResult> results = QtConcurrent::blockingMappedReduced<QVector<PairResult>>(
                            task_now, mapFunc, reduceFunc, QtConcurrent::OrderedReduce);

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
        }
    }

    // 全体最適化
    if (in.all_TF) {
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
                return processOnePair(t, in.imgs, in.res_all, in_poss, deltas, K, in.all_radi);
            };
            auto reduceFunc = [&](QVector<PairResult>& out, const PairResult& r) {
                if (r.valid) {
                    out.push_back(r);
                }
            };

            QVector<PairResult> results =
                QtConcurrent::blockingMappedReduced<QVector<PairResult>>(
                    tasks, mapFunc, reduceFunc, QtConcurrent::OrderedReduce);

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
    }
    ret.err = "";
    return ret;
}

void MainWindow::show_opti_settings()
{
    opti_settings dialog(this);
    dialog.setValues(pa_num,pa_radi,pa_opti,pa_itr,all_radi,all_opti,all_itr,pa_TF,all_TF);

    if (dialog.exec() == QDialog::Accepted) {
        // OKが押されたとき
        std::vector<int> retV = dialog.getValues();
        pa_num = retV[0];
        pa_radi = retV[1];
        pa_opti = retV[2];
        pa_itr = retV[3];
        all_radi = retV[4];
        all_opti = retV[5];
        all_itr = retV[6];
        std::vector<bool> retTF = dialog.getTFs();
        pa_TF = retTF[0];
        all_TF = retTF[1];
        if ((!pa_TF) && (!all_TF)) {
            ui->pushButton_3->setEnabled(false);
        } else if (!calc1_finished_state) {
            ui->pushButton_3->setEnabled(false);
        } else {
            ui->pushButton_3->setEnabled(true);
        }
    }
}


void MainWindow::show_detail_opti()
{
    const int n = input_files.size();
    if (n == 0) {
        QMessageBox::warning(this, "最適化結果の詳細", "表示できるデータがありません。");
        return;
    }
    if (!calc2_finished_state) {
        QMessageBox::warning(this, "最適化結果の詳細", "表示するデータがありません。先に位置合わせ最適化計算を実行してください。");
        return;
    }

    m_detailDialog->show();
}