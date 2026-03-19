#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QLabel>
#include <QFutureWatcher>
#include <QHash>

#include <opencv2/core.hpp>
#include "detail_opti_dialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class CornerDirectionSelector;
class detail_opti_dialog;

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
    bool calc_error = false; // OpenCV計算例外
};

struct CalcTRWSinput
{
    std::vector<cv::Mat> imgs;
    QVector<QSize> res_all;
    QVector<QPoint> poss;
    bool pa_TF;
    int pa_num;
    int pa_radi;
    int pa_opti;
    int pa_itr;
    bool all_TF;
    int all_radi;
    int all_opti;
    int all_itr;
};

/*
struct out_detail {
    bool PaAll;
    int start;
    int end;
    int itr;
    int loop;
    bool shuusoku;
    int lowSSIM_num;
    double minSSIM;
    double energy;
};

struct out_log {
    std::vector<double> ssim;
    std::vector<int> edge1;
    std::vector<int> edge2;
};
*/

struct CalcTRWSoutput {
    std::vector<out_detail> detail;
    std::vector<out_log> log;
    QVector<QPoint> poss;
    std::string err;
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

    // CLI由来のファイルパスをチェックする
    void File_input_check(QStringList);
    void File_input_UI();
    void File_input_dummy();
    void set_manu_auto(int);
    void set_over_value(int, int, int);
    //int get_inputF_num() {return input_files.size();};
    void set_array_value(int, int, int);
    void set_zigzag_value(int);
    void set_opti_value(int,int,int,int,int,int,int,int,int);
    void run_manual(int);
    void cli_make_image() {make_image();}
    void set_output_path(QString);
    void cli_exp_image();
    void set_close_value(bool closeTF) {closeTFm = closeTF;}

signals:
    void fileInputFinished();   // 完了通知
    void calcFinished();
    void makeimageFinished();
    void exportFinished();

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
    void calc_TRWS();
    void show_opti_settings();
    void calc_TRWS_finish();
    void show_detail_opti();

private:
    Ui::MainWindow *ui;
    QGraphicsScene *scene;

    // inputファイル名
    QStringList input_files;

    // outputファイル名
    QString output_file;

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

    // ファイル名ソート
    QStringList onSortUpFname(QStringList);

    // 計算finishでシグナル発出するか
    bool calc_finish_sig = false;

    // 良好状態
    bool ryoukou = false;

    // 計算後にアプリを閉じる
    bool closeTFm = false;

    // 全体最適化計算時、エッジを張る重なりピクセル数の閾値
    //int edge_th = 100;

    // 部分最適化するかどうか
    bool pa_TF = true;

    // 部分最適化計算の画像枚数
    int pa_num = 6;

    // 部分最適化計算の最大移動距離
    int pa_radi = 2; // 半径3pixを探索する。

    // 部分最適化計算の最大計算回数
    int pa_opti = 5000;

    // 部分最適化計算の最大反復回数
    int pa_itr = 4;

    // 部分最適化するかどうか
    bool all_TF = false;

    // 全体最適化計算の最大移動距離
    int all_radi = 2; // 半径3pixを探索する。

    // 全体最適化計算の最大計算回数
    int all_opti = 10000; // 5000回

    // 全体最適化計算の最大反復回数
    int all_itr = 10; // 5回

    // 最適化を別スレッド実行
    QFutureWatcher<CalcTRWSoutput>* trwsWatcher = nullptr;
    bool trwsRunning = false;
    CalcTRWSoutput calc_TRWS_core(CalcTRWSinput);

    // 計算状態制御用
    bool calc1_finished_state = false;
    bool calc2_finished_state = false;

    detail_opti_dialog* m_detailDialog = nullptr;

    bool cal_opti = false;
};

class MyGraphicsView : public QGraphicsView
{
protected:
    void wheelEvent(QWheelEvent *event) override;
};


#endif // MAINWINDOW_H

