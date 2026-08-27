#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QLabel>
#include <QFutureWatcher>
#include <QHash>
#include <QElapsedTimer>
#include <QString>
#include <QColor>

#include <opencv2/core.hpp>
#include "app_settings.h"
#include "canvas_history_graph_widget.h"
#include "detail_opti_dialog.h"
#include "vulkan_ssim.h"
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QLabel;
class CornerDirectionSelector;
class detail_opti_dialog;
class QDialog;
class QProgressBar;
class QScrollArea;
class QLineEdit;
class QPushButton;
class QGraphicsRectItem;
class ApplicationSettingsDialog;

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
    bool pa_auto_increment_TF;
    int pa_increment;
    int pa_increment_count;
    int pa_radi;
    int pa_opti;
    int pa_itr;
    bool all_TF;
    int all_radi;
    int all_opti;
    int all_itr;
    VulkanExecutionOptions vulkan;
    std::function<void(int, int, const QString&)> progressCallback;
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

struct LeastSquaresStitchSettings {
    double regressionThreshold = 0.3;
    double relativeThreshold = 2.5;
    double absoluteThreshold = 3.5;
    double maxPairErrorForRelative = 0.95;
};

enum class ImageMergeMode {
    DistanceL2 = 0,
    FocusRegion = 1,
    FocusStackTenengrad = 2
};

struct ImageMergeSettings {
    ImageMergeMode mode = ImageMergeMode::FocusRegion;
};

struct CalcLeastSquaresInput {
    std::vector<cv::Mat> imgs;
    QVector<QSize> res_all;
    QVector<QPoint> poss;
    int calc_loop_num = 5;
    LeastSquaresStitchSettings settings;
    std::function<void(int, int, const QString&)> progressCallback;
};

struct LeastSquaresPairDetail {
    int img1 = -1;
    int img2 = -1;
    QString status;
    int loop_num = 0;
    bool stability = false;
    double score = 0.0;
    double measuredSsim = 0.0;
    double optimizedSsim = 0.0;
    double measuredDx = 0.0;
    double measuredDy = 0.0;
    double optimizedDx = 0.0;
    double optimizedDy = 0.0;
    double residual = 0.0;
};

struct CalcLeastSquaresOutput {
    QVector<QPoint> poss;
    std::vector<LeastSquaresPairDetail> details;
    int acceptedPairs = 0;
    int removedPairs = 0;
    double avgError = 0.0;
    double maxError = 0.0;
    double minSsim = 0.0;
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
    void set_over_value(int, int, int);
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
    void calc_least_squares();
    void show_opti_settings();
    void show_least_squares_settings();
    void show_merge_settings();
    void calc_TRWS_finish();
    void calc_least_squares_finish();
    void show_detail_opti();
    void show_detail_least_squares();
    void show_application_settings();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

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

    void applySceneMoveMode(bool enabled);
    QVector<QPoint> readScenePositions() const;
    void syncPossFromScene();
    void invalidateCreatedImage();
    void arrangeSettingsChanged();
    int imageIndexAtScenePos(const QPointF& scenePos) const;
    void selectImageRange(int first, int last);
    void applyCanvasPositions(const QVector<QPoint>& positions);
    void recordCanvasHistory(int markers = CanvasHistoryMarkerNone);
    void undoCanvasHistory();
    void redoCanvasHistory();
    void showCanvasHistoryDialog();
    void refreshCanvasHistoryDialog();
    void scrollCanvasHistoryToCurrent();
    void restoreCanvasHistoryNode(int nodeIndex);
    int latestCanvasHistoryNode() const;
    bool samePositions(const QVector<QPoint>& a, const QVector<QPoint>& b) const;
    bool isAlignmentOrOptimizationRunning() const;
    QString formatSelectedImageIndices(QVector<int> indices) const;
    QVector<int> parseImageIndexSpec(const QString& text, QString* errorMessage) const;
    void showImageHighlightDialog();
    void updateImageHighlightColorButton();
    void applyImageHighlightsFromDialog();
    void clearImageHighlights();
    void clearImageHighlightRects();
    void rebuildImageHighlights();
    void startDelayedVulkanDetection();
    void showOptimizationProgressDialog();
    void updateOptimizationProgress(int value, int maximum, const QString& text);
    void hideOptimizationProgressDialog();
    QString formatDuration(qint64 milliseconds) const;
    int sceneSelectionAnchor = -1;
    QVector<QPoint> sceneMousePressPositions;
    QVector<CanvasHistoryNode> canvasHistoryTree;
    int currentCanvasHistoryNode = -1;
    int nextCanvasHistorySequence = 1;
    bool restoringCanvasHistory = false;
    QDialog* canvasHistoryDialog = nullptr;
    CanvasHistoryGraphWidget* canvasHistoryGraphWidget = nullptr;
    QScrollArea* canvasHistoryScrollArea = nullptr;
    QDialog* imageHighlightDialog = nullptr;
    QLineEdit* imageHighlightEdit = nullptr;
    QPushButton* imageHighlightColorButton = nullptr;
    QColor imageHighlightColor = QColor(255, 64, 64);
    QVector<int> imageHighlightIndices;
    QVector<QGraphicsRectItem*> imageHighlightRects;

    ApplicationSettingsDialog* applicationSettingsDialog = nullptr;
    QFutureWatcher<VulkanDeviceScanResult>* vulkanScanWatcher = nullptr;
    VulkanDeviceScanResult vulkanScanResult;
    bool vulkanDetectionInProgress = false;

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
    QVector<QPoint> imageMakeSourcePositions;

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
    bool pa_TF = false;

    // 部分最適化計算の画像枚数
    int pa_num = 6;

    // 部分最適化計算で収束時に画像枚数を増やすかどうか
    bool pa_auto_increment_TF = false;

    // 部分最適化計算の画像枚数増分
    int pa_increment = 0;

    // 部分最適化計算の画像枚数増分の実行回数
    int pa_increment_count = 1;

    // 部分最適化計算の最大移動距離
    int pa_radi = 2; // 半径3pixを探索する。

    // 部分最適化計算の最大計算回数
    int pa_opti = 5000;

    // 部分最適化計算の最大反復回数
    int pa_itr = 4;

    // 全体最適化するかどうか
    bool all_TF = true;

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

    QFutureWatcher<CalcLeastSquaresOutput>* leastSquaresWatcher = nullptr;
    bool leastSquaresRunning = false;
    LeastSquaresStitchSettings leastSquaresSettings;
    CalcLeastSquaresOutput calc_least_squares_core(CalcLeastSquaresInput);
    std::vector<LeastSquaresPairDetail> leastSquaresDetails;

    ImageMergeSettings imageMergeSettings;

    // 計算状態制御用
    bool calc1_finished_state = false;
    bool calc2_finished_state = false;

    detail_opti_dialog* m_detailDialog = nullptr;

    QDialog* optimizationProgressDialog = nullptr;
    QLabel* optimizationProgressLabel = nullptr;
    QProgressBar* optimizationProgressBar = nullptr;
    QElapsedTimer optimizationProgressTimer;

    bool cal_opti = false;
};


#endif // MAINWINDOW_H

