#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QSize>
#include <QString>

enum class ApplicationTheme {
    System = 0,
    Light = 1,
    Dark = 2
};

enum class ApplicationLanguage {
    System = 0,
    Japanese = 1,
    English = 2
};

struct VulkanExecutionOptions {
    bool enabled = true;
    bool ignoreVramLimit = false;
    QString deviceKey;
};

struct FileInputDefaultSettings {
    int sortMode = 0;
};

struct AlignmentDefaultSettings {
    int horizontalOverlapPercent = 25;
    int verticalOverlapPercent = 25;
    int searchRangePercent = 15;
};

struct ArrangementDefaultSettings {
    int direction = 8;
    int horizontalImageCount = 0;
    int verticalImageCount = 0;
    bool zigzag = true;
};

struct CanvasDefaultSettings {
    // QColor-compatible value, or "none" for an unpainted background.
    QString backgroundColor = "black";
    int selectedImageOpacityPercent = 0;
    bool layoutLocked = false;
    bool useCanvasAsSource = false;
    QString highlightColor = "#ff4040";
};

struct TrwsPamiDefaultSettings {
    bool localEnabled = false;
    int localImageCount = 6;
    bool localAutoIncrement = false;
    int localImageCountIncrement = 0;
    int localIncrementCount = 1;
    int localSearchRadius = 2;
    int localMaxIterations = 5000;
    int localMaxLoops = 4;

    bool globalEnabled = true;
    int globalSearchRadius = 2;
    int globalMaxIterations = 10000;
    int globalMaxLoops = 10;
};

struct LeastSquaresDefaultSettings {
    double regressionThreshold = 0.3;
    double relativeThreshold = 2.5;
    double absoluteThreshold = 3.5;
    double maxPairErrorForRelative = 0.95;
};

struct ImageMergeDefaultSettings {
    int mode = 1;
};

struct ProjectFileDefaultSettings {
    bool confirmSaveOnClose = true;
};

struct ExplorerDefaultSettings {
    bool contextMenuEnabled = false;
};

struct ApplicationDefaultSettings {
    ApplicationTheme theme = ApplicationTheme::System;
    ApplicationLanguage language = ApplicationLanguage::System;
    VulkanExecutionOptions vulkan;
    FileInputDefaultSettings fileInput;
    AlignmentDefaultSettings alignment;
    ArrangementDefaultSettings arrangement;
    CanvasDefaultSettings canvas;
    TrwsPamiDefaultSettings trwsPami;
    LeastSquaresDefaultSettings leastSquares;
    ImageMergeDefaultSettings imageMerge;
    ProjectFileDefaultSettings projectFile;
    ExplorerDefaultSettings explorer;
};

class AppSettings
{
public:
    // 実行ファイルと同じ場所にあるImage_Stitcher_Auto.confを読み込む。
    // ファイルや値が不正な場合は、各構造体の従来値へフォールバックする。
    static const ApplicationDefaultSettings& defaults();
    static QString defaultsFilePath();

    static ApplicationTheme theme();
    static void setTheme(ApplicationTheme theme);

    static ApplicationLanguage language();
    static void setLanguage(ApplicationLanguage language);

    static VulkanExecutionOptions vulkanOptions();
    static void setVulkanEnabled(bool enabled);
    static void setIgnoreVramLimit(bool ignore);
    static void setVulkanDeviceKey(const QString& key);

    static bool confirmProjectSaveOnClose();
    static void setConfirmProjectSaveOnClose(bool enabled);

    static bool explorerContextMenuEnabled();
    static void setExplorerContextMenuEnabled(bool enabled);

    static QString canvasBackground();
    static void setCanvasBackground(const QString& setting);

    static AlignmentDefaultSettings alignmentOptions();
    static void setAlignmentOptions(const AlignmentDefaultSettings& options);
    static void resetAlignmentOptions();

    static ArrangementDefaultSettings arrangementOptions();
    static void setArrangementOptions(const ArrangementDefaultSettings& options);
    static void resetArrangementOptions();

    static TrwsPamiDefaultSettings trwsPamiOptions();
    static void setTrwsPamiOptions(const TrwsPamiDefaultSettings& options);
    static void resetTrwsPamiOptions();

    static LeastSquaresDefaultSettings leastSquaresOptions();
    static void setLeastSquaresOptions(const LeastSquaresDefaultSettings& options);
    static void resetLeastSquaresOptions();

    static ImageMergeDefaultSettings imageMergeOptions();
    static void setImageMergeOptions(const ImageMergeDefaultSettings& options);
    static void resetImageMergeOptions();

    static QSize windowSize(const QString& windowKey, const QSize& fallback);
    static void setWindowSize(const QString& windowKey, const QSize& size);
    static int controlPanelWidth(int minimumWidth);
    static void setControlPanelWidth(int width);
    static void resetWindowSizes();

    static void resetApplicationDialogSettings();
    static void resetCanvasSettings();
    static void resetAllUserSettings();
};

void applyApplicationTheme(ApplicationTheme theme);
bool applyApplicationLanguage(ApplicationLanguage language);

#endif // APP_SETTINGS_H
