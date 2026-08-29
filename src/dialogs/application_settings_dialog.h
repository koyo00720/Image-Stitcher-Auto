#ifndef APPLICATION_SETTINGS_DIALOG_H
#define APPLICATION_SETTINGS_DIALOG_H

#include "vulkan_ssim.h"

#include <QDialog>

class QCheckBox;
class QButtonGroup;
class QComboBox;
class QEvent;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QRadioButton;
class QStackedWidget;

enum class SettingsResetCategory {
    All,
    ApplicationDialog,
    Alignment,
    Layout,
    LeastSquares,
    TrwsPami,
    ImageMerge
};

class ApplicationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApplicationSettingsDialog(QWidget* parent = nullptr);

    void setVulkanDetectionInProgress();
    void setVulkanScanResult(const VulkanDeviceScanResult& result);
    void reloadFromSettings();

signals:
    void themeChanged();
    void languageChanged();
    void resetRequested(SettingsResetCategory category);

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void updateMinimumHeightForTabs();
    void updateExplorerControls();
    void updateVulkanControls();
    void updateVulkanStatusText();

    QListWidget* tabList = nullptr;
    QStackedWidget* pageStack = nullptr;
    QGroupBox* themeGroup = nullptr;
    QButtonGroup* themeButtonGroup = nullptr;
    QRadioButton* systemThemeButton = nullptr;
    QRadioButton* lightThemeButton = nullptr;
    QRadioButton* darkThemeButton = nullptr;
    QGroupBox* languageGroup = nullptr;
    QButtonGroup* languageButtonGroup = nullptr;
    QRadioButton* systemLanguageButton = nullptr;
    QRadioButton* japaneseLanguageButton = nullptr;
    QRadioButton* englishLanguageButton = nullptr;
    QGroupBox* vulkanGroup = nullptr;
    QCheckBox* useVulkanCheck = nullptr;
    QCheckBox* ignoreVramLimitCheck = nullptr;
    QComboBox* gpuCombo = nullptr;
    QLabel* gpuCaptionLabel = nullptr;
    QLabel* vulkanStatusLabel = nullptr;
    QGroupBox* projectFileGroup = nullptr;
    QCheckBox* confirmProjectSaveCheck = nullptr;
    QGroupBox* explorerGroup = nullptr;
    QCheckBox* explorerContextMenuCheck = nullptr;
    QGroupBox* resetGroup = nullptr;
    QPushButton* resetAllButton = nullptr;
    QPushButton* resetApplicationButton = nullptr;
    QPushButton* resetAlignmentButton = nullptr;
    QPushButton* resetLayoutButton = nullptr;
    QPushButton* resetLeastSquaresButton = nullptr;
    QPushButton* resetTrwsPamiButton = nullptr;
    QPushButton* resetImageMergeButton = nullptr;
    QGroupBox* shortcutFileGroup = nullptr;
    QGroupBox* shortcutCanvasGroup = nullptr;
    QLabel* shortcutOpenProjectLabel = nullptr;
    QLabel* shortcutSaveProjectLabel = nullptr;
    QLabel* shortcutUndoLabel = nullptr;
    QLabel* shortcutRedoLabel = nullptr;
    QLabel* shortcutDeleteLabel = nullptr;
    QLabel* shortcutFindLabel = nullptr;
    QLabel* informationApplicationCaption = nullptr;
    QLabel* informationVersionCaption = nullptr;
    QLabel* informationQtCaption = nullptr;
    QLabel* informationOpenCvCaption = nullptr;
    QLabel* informationVulkanCaption = nullptr;
    QLabel* informationVulkanLabel = nullptr;
    QPushButton* closeButton = nullptr;

    VulkanDeviceScanResult scanResult;
    bool detectionInProgress = false;
};

#endif // APPLICATION_SETTINGS_DIALOG_H
