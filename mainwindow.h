#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QLabel>
#include <QFutureWatcher>
#include <QHash>

#include <opencv2/core.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class CornerDirectionSelector;

struct return_struct1 {
    double score = 0.0;
    int x = 0;
    int y = 0;
};

struct return_struct2 {
    cv::Mat img1;
    cv::Mat img2;
};

// SSIM 1スレッドの入力
struct SSIM_TaskInput {
    cv::Mat input1;
    cv::Mat input2;
    QSize px1;
    QPoint pos1;
    QSize px2;
    QPoint pos2;
    int dx;
    int dy;
};

// iFFT 1スレッドの入力
struct ifft_thread_input {
    int img1_id;
    int img2_id;
    cv::Mat input1;
    cv::Mat input2;
    QSize px1;
    QPoint pos1;
    QSize px2;
    QPoint pos2;
    int calc_loop_num;
};
// iFFT 1スレッドの出力
struct ifft_thread_output {
    int img1_id;
    int img2_id;
    double vecX;
    double vecY;
    double score;
    bool stability; // tureは安定、falseは不安定
    int loop_num; // ループ計算回数
    double ssim;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //ファイル名の入出力
    //void set_inputF(QStringList);
    //QStringList get_inputF();

private slots:
    void onOpacity1Changed(int percent);
    void calc_iFFT(); // ボタンを押した時に実行
    void make_image(); // 画像を作成ボタン
    void png_export(); // exportボタンを押した時に実行
    void calc_finish_1(); // iFFT 並列計算終了後計算
    void calc_finish_2(); // iFFT 並列計算終了後計算
    void show_detail(); // 詳細の表示
    void onSceneSelectionChanged(); // キャンパス内の画像を選択
    void posi_lock(bool); // 画像レイアウトをロック

private:
    Ui::MainWindow *ui;
    QGraphicsScene *scene;

    // inputファイル名
    QStringList input_files;

    // ファイル入力 QStringListをQGraphicsPixmapItemへ変換
    void File_input(const QStringList&, const QStringList&);

    // 画像データ
    QVector<QGraphicsPixmapItem*> items;
    QHash<int, QGraphicsPixmapItem*> itemById; // idとitemのリンク
    std::vector<cv::Mat> imgs; // 入力データ
    cv::Mat output_img; // 出力データ

    // 全画像の解像度
    QVector<QSize> res_all;

    // 全画像の位置
    QVector<QPoint> pos_all;

    // 計算後の全画像の位置
    QVector<QPoint> poss;

    // 計算結果を保持
    QList<ifft_thread_output> calc_results;
    QList<ifft_thread_output> calc_results_re;

    // 移動方向のデータを保持
    QVector<int> idou_dir;

    // 各画像の透明度を保持
    QVector<int> toumeido;

    // 画像配列関数
    void photo_Arrange();

    // 位置合わせ再計算関数
    void calc_iFFT_rerun();

    int calc_loop_num = 5; // 最大5回ループ計算する

    // キャンパスデータを保持
    QGraphicsRectItem *itemC = nullptr;

    // キャンパスサイズを画像プラス〇%のサイズにする
    int plus_per_camp = 100;

    // カメラフィットを画像プラス〇%のサイズにする
    int plus_per_camera = 5;

    // 折り返しの選択
    bool orikaeshi = true; // ジグザグ

    // iFFT watcher
    QFutureWatcher<ifft_thread_output> watcher;
    QFutureWatcher<ifft_thread_output> watcher_re;

    // 画像作成 watcher
    QFutureWatcher<cv::Mat> image_make_Watcher;

    // iFFTの集約後の良好・不良 (表示用)
    QVector<bool> checkTF;

    // iFFTの集約後の良好・不良 (計算用)
    QVector<bool> checkTF_calc;

    // 位置合わせ用ssim不良の閾値
    double ssim_th_calc = 0.4;

    // ssim不良の閾値
    double ssim_th = 0.2;

    // 画像データの削除
    void deleteSelectedItems();

    // 拡大率表示
    QLabel *zoomLabel = nullptr;
    void updateZoomLabel();

    // SSIM入力値を保存
    QVector<SSIM_TaskInput> ssim_inputs_save;

    // rerun時の対応関係
    std::vector<int> i2id;

    // rerunの探索範囲のステップ（1以上）
    int re_step = 10;

    // rerunの探索範囲数
    int ovc;
};

class MyGraphicsView : public QGraphicsView
{
protected:
    void wheelEvent(QWheelEvent *event) override;
};


#endif // MAINWINDOW_H
