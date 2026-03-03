#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "file_input_wiz.h"
#include "image_utils.h"
#include "cornerdirectionselector.h"
#include "detail_dialog.h"

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

#include <QPointer>
#include <QDebug>
#include <QStandardItemModel>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <array>

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

    // PNG exportボタン
    connect(ui->pushButton_4, &QPushButton::clicked, this, &MainWindow::png_export);

    // 結合品質の詳細
    connect(ui->pushButton, &QPushButton::clicked, this, &MainWindow::show_detail);

    // レイアウトロックのチェックボックス
    connect(ui->checkBox, &QCheckBox::toggled,this, &MainWindow::posi_lock);

    // 未実装部分をenableしない
    ui->radioButton_4->setEnabled(false);
    ui->radioButton_6->setEnabled(false);
    ui->pushButton_3->setEnabled(false);

    // 選択中の画像番号を表示
    ui->label_9->setText("なし");
    ui->label_8->setEnabled(false);
    ui->label_9->setEnabled(false);

    // 画像を作成 finish通知
    connect(&image_make_Watcher, &QFutureWatcher<cv::Mat>::finished, this, [this]() {
        output_img = image_make_Watcher.result();
        ui->label_6->setText("完了");
        ui->pushButton_2->setEnabled(true);
    });
}

MainWindow::~MainWindow()
{
    if (watcher.isRunning()) { watcher.cancel(); watcher.waitForFinished(); }
    delete ui;
}


static int viewZoomPercent(const QGraphicsView *view)
{
    const double s = view->transform().m11();   // x方向スケール（等方ならこれでOK）
    return int(std::round(s * 100.0));
}

// ファイル入力ウィザードでok押した時に実行
void MainWindow::File_input(const QStringList& paths_old, const QStringList& paths_new)
{
    const int n_new = paths_new.size();
    const int n_old = paths_old.size();
    //qDebug() << "new size " << n_new;
    //qDebug() << "old size " << n_old;
    const int n_max = std::max(n_new, n_old);

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
            QPixmap pix(paths_new[i]);
            if (pix.isNull()) {
                QMessageBox::warning(
                    this,
                    tr("エラー"),
                    tr("%1枚目の画像の読み込みに失敗しました。\nファイル入力ウィザードから削除してください。").arg(i + 1)
                    );
                break;
            }

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
        QPixmap pix(paths_new[i]);
        if (pix.isNull()) {
            QMessageBox::warning(
                this,
                tr("エラー"),
                tr("%1枚目の画像の読み込みに失敗しました。\nファイル入力ウィザードから削除してください。").arg(i + 1)
                );
            break;
        }

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
    ui->cornerSelector->setMax(input_files.size());
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
    ui->cornerSelector->setMax(input_files.size());
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
        toumeido[idx] = percent;
    }
}

void MainWindow::onSceneSelectionChanged()
{
    const auto selected = scene->selectedItems();
    const int n = input_files.size();
    if (selected.isEmpty()) {
        for (int i = 0; i < n; ++i) {
            items[i]->setZValue(i + 1);
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
    ui->sliderOpacity1->setValue(toumeido[idx]);

    // 選択された画像を前面へ出す
    QVector<int> idx_list;
    for (QGraphicsItem* item : selected) {
        int idx = items.indexOf(static_cast<QGraphicsPixmapItem*>(item));
        idx_list.push_back(idx);
    }

    // 選択番号を表示
    QVector<int> idx_list_show = idx_list;
    std::sort(idx_list_show.begin(), idx_list_show.end());
    QStringList parts;
    parts.reserve(idx_list_show.size());
    for (int x : idx_list_show) parts << QString::number(x+1);
    ui->label_9->setText(parts.join(", "));

    for (int i = 0; i < n; ++i) {
        if (idx_list.contains(i)) {
            items[i]->setZValue(i + n + 1);
        } else {
            items[i]->setZValue(i + 1);
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

    int x_r, y_r;
    double response = 0.0;
    bool stab_now = false;
    int count = 0;
    double x_d, y_d;

    ifft_thread_output r;

    // 最大4回計算して安定するか確認する
    for (int l = 0; l < in.calc_loop_num; ++l) {

    // 重なり領域をcropして取り出す。
        return_struct2 r_st = Crop_2ImageTo2Image(in.input1, in.input2, in.px1, c_pos1, in.px2, c_posVs[l]);
        cv::Mat crop1 = r_st.img1;
        cv::Mat crop2 = r_st.img2;

        if (crop1.rows == 0 || crop1.cols == 0) {
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

    r.img1_id = in.img1_id;
    r.img2_id = in.img2_id;
    r.vecX = x_d;
    r.vecY = y_d;
    r.score = response;
    r.stability = stab_now;
    r.loop_num = count;
    r.ssim = SSIM_calc_oneshot(SSIM_TaskInput{in.input1,in.input2,in.px1,QPoint(0,0),in.px2,QPoint(x_r,y_r),0,0});
    return r;
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
    ui->groupBox_5->setEnabled(false);
    ui->pushButton_Calc1->setEnabled(false);
    ui->pushButton->setEnabled(false);
    ui->label_4->setEnabled(false);
    ui->pushButton_4->setEnabled(false);
    ui->pushButton_2->setEnabled(false);
    ui->label_6->setText("");

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
                } else if (idou_dir[i-1] == 1 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (idou_dir[i-1] == 1 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 0 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 3;
                } else if (idou_dir[i-1] == 0 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 1 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (idou_dir[i-1] == 1 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 2 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else if (idou_dir[i-1] == 2 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 3 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (idou_dir[i-1] == 3 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 0 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else if (idou_dir[i-1] == 0 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 3 && idou_dir[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 2;
                } else if (idou_dir[i-1] == 3 && idou_dir[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                } else if (idou_dir[i-1] == 2 && idou_dir[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                    idx = 1;
                } else if (idou_dir[i-1] == 2 && idou_dir[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
    } else {
        ui->label_5->setText(QString::number(countF) + " ヶ所不良");
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

    ui->groupBox_5->setEnabled(true);
    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->label_4->setEnabled(true);
    ui->pushButton_4->setEnabled(true);
    ui->pushButton_2->setEnabled(true);

}

void MainWindow::calc_finish_2()
{
    calc_results_re = watcher_re.future().results();

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
                int ssim_max_j0N;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+0].ssim;
                    if (now > ssim_max0) {
                        ssim_max_j0N = j;
                        ssim_max0 = now;
                    }
                }

                double ssim_max1 = 0.0;
                int ssim_max_j1N;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+1].ssim;
                    if (now > ssim_max1) {
                        ssim_max_j1N = j;
                        ssim_max1 = now;
                    }
                }

                double ssim_max2 = 0.0;
                int ssim_max_j2N;
                for (int j = 0; j < ovc; ++j) {
                    double now = calc_results_re[ind*ovc*4+j*4+2].ssim;
                    if (now > ssim_max2) {
                        ssim_max_j2N = j;
                        ssim_max2 = now;
                    }
                }
                double ssim_max3 = 0.0;
                int ssim_max_j3N;
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
                    } else if (idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 3;
                    } else if (idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 1 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 1;
                    } else if (idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 3;
                    } else if (idou_dir_new[i-1] == 0 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 0 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 2;
                    } else if (idou_dir_new[i-1] == 3 && idou_dir_new[i-2] == 2 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                    } else if (idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 3 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
                        idx = 1;
                    } else if (idou_dir_new[i-1] == 2 && idou_dir_new[i-2] == 1 && calc_results_new[i-1].ssim >= ssim_th_calc && calc_results_new[i-2].ssim >= ssim_th_calc) {
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
                int ssim_max_j;
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
    } else {
        ui->label_5->setText(QString::number(countF) + " ヶ所不良");
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

    ui->groupBox_5->setEnabled(true);
    ui->pushButton_Calc1->setEnabled(true);
    ui->pushButton->setEnabled(true);
    ui->label_4->setEnabled(true);
    ui->pushButton_4->setEnabled(true);
    ui->pushButton_2->setEnabled(true);
}

void MainWindow::png_export() {

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

    ui->label_6->setText("作成中");
    ui->pushButton_2->setEnabled(false);

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
