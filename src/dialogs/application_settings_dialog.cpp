#include "application_settings_dialog.h"

#include "app_settings.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <opencv2/core/version.hpp>

#include <algorithm>

namespace
{
QString formatBytes(quint64 bytes)
{
    constexpr double gib = 1024.0 * 1024.0 * 1024.0;
    constexpr double mib = 1024.0 * 1024.0;
    if (bytes >= static_cast<quint64>(gib)) {
        return QString::number(static_cast<double>(bytes) / gib, 'f', 1) + " GiB";
    }
    return QString::number(static_cast<double>(bytes) / mib, 'f', 0) + " MiB";
}
}

ApplicationSettingsDialog::ApplicationSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("設定");
    setModal(false);
    resize(680, 430);

    auto* rootLayout = new QVBoxLayout(this);
    auto* bodyLayout = new QHBoxLayout;
    rootLayout->addLayout(bodyLayout, 1);

    tabList = new QListWidget(this);
    tabList->setObjectName("settingsTabList");
    tabList->setFixedWidth(120);
    tabList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);
    tabList->setStyleSheet(
        "QListWidget::item { padding: 12px 16px; }"
        "QListWidget::item:selected { font-weight: 600; }");
    tabList->addItem("一般");
    tabList->addItem("情報");
    bodyLayout->addWidget(tabList);

    pageStack = new QStackedWidget(this);
    bodyLayout->addWidget(pageStack, 1);

    auto* generalPage = new QWidget(pageStack);
    auto* generalLayout = new QVBoxLayout(generalPage);

    auto* themeGroup = new QGroupBox("テーマ", generalPage);
    auto* themeLayout = new QFormLayout(themeGroup);
    themeCombo = new QComboBox(themeGroup);
    themeCombo->addItem("システム", static_cast<int>(ApplicationTheme::System));
    themeCombo->addItem("ライト", static_cast<int>(ApplicationTheme::Light));
    themeCombo->addItem("ダーク", static_cast<int>(ApplicationTheme::Dark));
    themeLayout->addRow("テーマ:", themeCombo);
    generalLayout->addWidget(themeGroup);

    auto* vulkanGroup = new QGroupBox("Vulkan", generalPage);
    auto* vulkanLayout = new QVBoxLayout(vulkanGroup);
    useVulkanCheck = new QCheckBox("利用可能ならVulkan GPU計算を使用する", vulkanGroup);
    ignoreVramLimitCheck = new QCheckBox("VRAM limitを無視する", vulkanGroup);
    vulkanLayout->addWidget(useVulkanCheck);
    vulkanLayout->addWidget(ignoreVramLimitCheck);

    auto* gpuRow = new QHBoxLayout;
    gpuRow->addWidget(new QLabel("GPU:", vulkanGroup));
    gpuCombo = new QComboBox(vulkanGroup);
    gpuRow->addWidget(gpuCombo, 1);
    vulkanLayout->addLayout(gpuRow);

    vulkanStatusLabel = new QLabel(vulkanGroup);
    vulkanStatusLabel->setWordWrap(true);
    vulkanLayout->addWidget(vulkanStatusLabel);
    generalLayout->addWidget(vulkanGroup);
    generalLayout->addStretch(1);
    pageStack->addWidget(generalPage);

    auto* informationPage = new QWidget(pageStack);
    auto* informationLayout = new QFormLayout(informationPage);
    informationLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    informationLayout->addRow("アプリケーション:",
                              new QLabel(QCoreApplication::applicationName(), informationPage));
    informationLayout->addRow("バージョン:",
                              new QLabel(QCoreApplication::applicationVersion(), informationPage));
    informationLayout->addRow("Qt:", new QLabel(QString::fromLatin1(qVersion()), informationPage));
    informationLayout->addRow("OpenCV:", new QLabel(QString::fromLatin1(CV_VERSION), informationPage));
    informationVulkanLabel = new QLabel(informationPage);
    informationVulkanLabel->setWordWrap(true);
    informationLayout->addRow("Vulkan:", informationVulkanLabel);
    pageStack->addWidget(informationPage);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);
    rootLayout->addWidget(buttonBox);

    connect(tabList, &QListWidget::currentRowChanged,
            pageStack, &QStackedWidget::setCurrentIndex);
    tabList->setCurrentRow(0);

    const ApplicationTheme currentTheme = AppSettings::theme();
    const int themeIndex = themeCombo->findData(static_cast<int>(currentTheme));
    themeCombo->setCurrentIndex(std::max(0, themeIndex));

    const VulkanExecutionOptions options = AppSettings::vulkanOptions();
    useVulkanCheck->setChecked(options.enabled);
    ignoreVramLimitCheck->setChecked(options.ignoreVramLimit);

    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        const auto theme = static_cast<ApplicationTheme>(themeCombo->itemData(index).toInt());
        AppSettings::setTheme(theme);
        applyApplicationTheme(theme);
        emit themeChanged();
    });
    connect(useVulkanCheck, &QCheckBox::toggled, this, [](bool enabled) {
        AppSettings::setVulkanEnabled(enabled);
    });
    connect(ignoreVramLimitCheck, &QCheckBox::toggled, this, [](bool ignore) {
        AppSettings::setIgnoreVramLimit(ignore);
    });
    connect(gpuCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            AppSettings::setVulkanDeviceKey(gpuCombo->itemData(index).toString());
        }
    });

    updateVulkanControls();
    updateVulkanStatusText();
}

void ApplicationSettingsDialog::setVulkanDetectionInProgress()
{
    detectionInProgress = true;
    scanResult = {};
    updateVulkanControls();
    updateVulkanStatusText();
}

void ApplicationSettingsDialog::setVulkanScanResult(const VulkanDeviceScanResult& result)
{
    detectionInProgress = false;
    scanResult = result;

    const QSignalBlocker blocker(gpuCombo);
    gpuCombo->clear();
    for (const VulkanGpuInfo& device : scanResult.devices) {
        QString label = device.name;
        if (!device.deviceType.isEmpty()) {
            label += QString(" (%1)").arg(device.deviceType);
        }
        if (device.localMemoryBytes > 0) {
            label += QString(" — %1").arg(formatBytes(device.localMemoryBytes));
        }
        gpuCombo->addItem(label, device.key);
    }

    const QString savedKey = AppSettings::vulkanOptions().deviceKey;
    int selectedIndex = gpuCombo->findData(savedKey);
    if (selectedIndex < 0 && gpuCombo->count() > 0) {
        selectedIndex = 0;
    }
    gpuCombo->setCurrentIndex(selectedIndex);
    if (selectedIndex >= 0) {
        AppSettings::setVulkanDeviceKey(gpuCombo->itemData(selectedIndex).toString());
    }

    updateVulkanControls();
    updateVulkanStatusText();
}

void ApplicationSettingsDialog::updateVulkanControls()
{
    const bool available = !detectionInProgress && !scanResult.devices.isEmpty();
    useVulkanCheck->setEnabled(available);
    ignoreVramLimitCheck->setEnabled(available);
    gpuCombo->setEnabled(available);
}

void ApplicationSettingsDialog::updateVulkanStatusText()
{
    QString status;
    if (!VulkanSsimEngine::isBuilt()) {
        status = "このビルドではVulkan計算が無効です。CPU計算を使用します。";
    } else if (detectionInProgress) {
        status = "GPUを検出中…";
    } else if (!scanResult.error.isEmpty()) {
        status = scanResult.error + " CPU計算を使用します。";
    } else if (scanResult.devices.isEmpty()) {
        status = "計算に利用できるVulkan GPUが見つかりません。CPU計算を使用します。";
    } else {
        status = QString("%1台のVulkan GPUを検出しました。VRAM上限は選択GPUの70%です。")
                     .arg(scanResult.devices.size());
    }
    vulkanStatusLabel->setText(status);
    informationVulkanLabel->setText(status);
}
