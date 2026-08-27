#ifndef APPLICATION_SETTINGS_DIALOG_H
#define APPLICATION_SETTINGS_DIALOG_H

#include "vulkan_ssim.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QStackedWidget;

class ApplicationSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ApplicationSettingsDialog(QWidget* parent = nullptr);

    void setVulkanDetectionInProgress();
    void setVulkanScanResult(const VulkanDeviceScanResult& result);

signals:
    void themeChanged();

private:
    void updateVulkanControls();
    void updateVulkanStatusText();

    QListWidget* tabList = nullptr;
    QStackedWidget* pageStack = nullptr;
    QComboBox* themeCombo = nullptr;
    QCheckBox* useVulkanCheck = nullptr;
    QCheckBox* ignoreVramLimitCheck = nullptr;
    QComboBox* gpuCombo = nullptr;
    QLabel* vulkanStatusLabel = nullptr;
    QLabel* informationVulkanLabel = nullptr;

    VulkanDeviceScanResult scanResult;
    bool detectionInProgress = false;
};

#endif // APPLICATION_SETTINGS_DIALOG_H
