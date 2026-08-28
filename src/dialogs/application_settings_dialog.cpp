#include "application_settings_dialog.h"

#include "app_settings.h"
#include "metal_ssim.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QFrame>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

#include <opencv2/core/version.hpp>

#include <algorithm>
#include <utility>

namespace
{
class SettingsTabDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(std::max(40, option.fontMetrics.height() + 20));
        return size;
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem itemOption(option);
        initStyleOption(&itemOption, index);

        const bool selected = itemOption.state & QStyle::State_Selected;
        const bool hovered = itemOption.state & QStyle::State_MouseOver;
        const bool enabled = itemOption.state & QStyle::State_Enabled;
        const QPalette::ColorGroup colorGroup = !enabled
                                                     ? QPalette::Disabled
                                                     : (itemOption.state & QStyle::State_Active
                                                            ? QPalette::Active
                                                            : QPalette::Inactive);
        const QPalette currentPalette = QApplication::palette();
        const QRect itemRect = itemOption.rect.adjusted(3, 1, -3, -1);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        if (hovered) {
            painter->setBrush(currentPalette.brush(colorGroup, QPalette::AlternateBase));
            painter->drawRoundedRect(itemRect, 6, 6);
        }
        if (selected) {
            const QRectF indicatorRect(itemRect.left() + 1,
                                       itemRect.top() + 8,
                                       3,
                                       std::max(8, itemRect.height() - 16));
            painter->setBrush(currentPalette.brush(colorGroup, QPalette::Highlight));
            painter->drawRoundedRect(indicatorRect, 1.5, 1.5);
        }

        QFont textFont = itemOption.font;
        if (selected) {
            textFont.setWeight(QFont::DemiBold);
        }
        painter->setFont(textFont);
        painter->setPen(currentPalette.color(colorGroup, QPalette::Text));
        painter->drawText(itemRect.adjusted(12, 0, -8, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          itemOption.text);
        painter->restore();
    }
};

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
    setModal(false);
    setMinimumWidth(640);
    resize(700, 440);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 16, 16, 12);
    rootLayout->setSpacing(12);
    auto* bodyLayout = new QHBoxLayout;
    bodyLayout->setSpacing(12);
    rootLayout->addLayout(bodyLayout, 1);

    tabList = new QListWidget(this);
    tabList->setObjectName("settingsTabList");
    tabList->setFixedWidth(132);
    tabList->setFrameShape(QFrame::NoFrame);
    tabList->setAutoFillBackground(false);
    tabList->viewport()->setAutoFillBackground(false);
    tabList->setAttribute(Qt::WA_TranslucentBackground);
    tabList->viewport()->setAttribute(Qt::WA_TranslucentBackground);
    tabList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tabList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tabList->setSelectionMode(QAbstractItemView::SingleSelection);
    tabList->setSpacing(2);
    tabList->setMouseTracking(true);
    tabList->setItemDelegate(new SettingsTabDelegate(tabList));
    tabList->setStyleSheet(
        "QListWidget#settingsTabList {"
        "  background: transparent; border: none; outline: none; padding: 3px;"
        "}");
    tabList->addItem(QString());
    tabList->addItem(QString());
    tabList->addItem(QString());
    tabList->addItem(QString());
    bodyLayout->addWidget(tabList);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    bodyLayout->addWidget(separator);

    pageStack = new QStackedWidget(this);
    bodyLayout->addWidget(pageStack, 1);

    auto addScrollablePage = [this](QWidget* page) {
        auto* scrollArea = new QScrollArea(pageStack);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setAutoFillBackground(false);
        scrollArea->viewport()->setAutoFillBackground(false);
        page->setAutoFillBackground(false);
        if (page->layout()) {
            page->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
        }
        scrollArea->setWidget(page);
        pageStack->addWidget(scrollArea);
    };

    auto* generalPage = new QWidget;
    auto* generalLayout = new QVBoxLayout(generalPage);
    generalLayout->setContentsMargins(8, 2, 4, 2);
    generalLayout->setSpacing(14);

    themeGroup = new QGroupBox(generalPage);
    auto* themeLayout = new QHBoxLayout(themeGroup);
    themeLayout->setContentsMargins(14, 12, 14, 12);
    themeLayout->setSpacing(20);
    themeButtonGroup = new QButtonGroup(this);
    auto addThemeButton = [this, themeLayout](QRadioButton*& button,
                                              ApplicationTheme theme) {
        button = new QRadioButton(themeGroup);
        themeButtonGroup->addButton(button, static_cast<int>(theme));
        themeLayout->addWidget(button);
        connect(button, &QRadioButton::toggled, this, [this, theme](bool checked) {
            if (!checked) {
                return;
            }
            AppSettings::setTheme(theme);
            applyApplicationTheme(theme);
            emit themeChanged();
        });
    };
    addThemeButton(systemThemeButton, ApplicationTheme::System);
    addThemeButton(lightThemeButton, ApplicationTheme::Light);
    addThemeButton(darkThemeButton, ApplicationTheme::Dark);
    themeLayout->addStretch(1);
    generalLayout->addWidget(themeGroup);

    languageGroup = new QGroupBox(generalPage);
    auto* languageLayout = new QHBoxLayout(languageGroup);
    languageLayout->setContentsMargins(14, 12, 14, 12);
    languageLayout->setSpacing(20);
    languageButtonGroup = new QButtonGroup(this);
    auto addLanguageButton = [this, languageLayout](QRadioButton*& button,
                                                    ApplicationLanguage language) {
        button = new QRadioButton(languageGroup);
        languageButtonGroup->addButton(button, static_cast<int>(language));
        languageLayout->addWidget(button);
        connect(button, &QRadioButton::toggled, this, [this, language](bool checked) {
            if (!checked) {
                return;
            }
            AppSettings::setLanguage(language);
            applyApplicationLanguage(language);
            emit languageChanged();
        });
    };
    addLanguageButton(systemLanguageButton, ApplicationLanguage::System);
    addLanguageButton(japaneseLanguageButton, ApplicationLanguage::Japanese);
    addLanguageButton(englishLanguageButton, ApplicationLanguage::English);
    languageLayout->addStretch(1);
    generalLayout->addWidget(languageGroup);

    vulkanGroup = new QGroupBox(generalPage);
    auto* vulkanLayout = new QVBoxLayout(vulkanGroup);
    vulkanLayout->setContentsMargins(14, 12, 14, 12);
    vulkanLayout->setSpacing(9);
    useVulkanCheck = new QCheckBox(vulkanGroup);
    ignoreVramLimitCheck = new QCheckBox(vulkanGroup);
    vulkanLayout->addWidget(useVulkanCheck);
    vulkanLayout->addWidget(ignoreVramLimitCheck);

    auto* gpuRow = new QFormLayout;
    gpuRow->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    gpuCombo = new QComboBox(vulkanGroup);
    gpuCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    gpuCombo->setMinimumContentsLength(28);
    gpuCombo->setMaxVisibleItems(12);
    gpuCaptionLabel = new QLabel(vulkanGroup);
    gpuRow->addRow(gpuCaptionLabel, gpuCombo);
    vulkanLayout->addLayout(gpuRow);

    vulkanStatusLabel = new QLabel(vulkanGroup);
    vulkanStatusLabel->setWordWrap(true);
    vulkanStatusLabel->setForegroundRole(QPalette::PlaceholderText);
    vulkanLayout->addWidget(vulkanStatusLabel);
    generalLayout->addWidget(vulkanGroup);

    projectFileGroup = new QGroupBox(generalPage);
    auto* projectFileLayout = new QVBoxLayout(projectFileGroup);
    projectFileLayout->setContentsMargins(14, 12, 14, 12);
    confirmProjectSaveCheck = new QCheckBox(projectFileGroup);
    projectFileLayout->addWidget(confirmProjectSaveCheck);
    generalLayout->addWidget(projectFileGroup);
    generalLayout->addStretch(1);
    addScrollablePage(generalPage);

    auto* resetPage = new QWidget;
    auto* resetPageLayout = new QVBoxLayout(resetPage);
    resetPageLayout->setContentsMargins(8, 2, 4, 2);
    resetGroup = new QGroupBox(resetPage);
    auto* resetLayout = new QVBoxLayout(resetGroup);
    resetLayout->setContentsMargins(14, 12, 14, 12);
    resetLayout->setSpacing(8);

    auto addResetButton = [this, resetLayout](QPushButton*& button,
                                               SettingsResetCategory category) {
        button = new QPushButton(resetGroup);
        button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        resetLayout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, category]() {
            emit resetRequested(category);
        });
    };
    addResetButton(resetAllButton, SettingsResetCategory::All);
    addResetButton(resetApplicationButton,
                   SettingsResetCategory::ApplicationDialog);
    addResetButton(resetAlignmentButton, SettingsResetCategory::Alignment);
    addResetButton(resetLayoutButton, SettingsResetCategory::Layout);
    addResetButton(resetLeastSquaresButton,
                   SettingsResetCategory::LeastSquares);
    addResetButton(resetTrwsPamiButton, SettingsResetCategory::TrwsPami);
    addResetButton(resetImageMergeButton, SettingsResetCategory::ImageMerge);
    resetPageLayout->addWidget(resetGroup);
    resetPageLayout->addStretch(1);
    addScrollablePage(resetPage);

    auto* shortcutPage = new QWidget;
    auto* shortcutLayout = new QVBoxLayout(shortcutPage);
    shortcutLayout->setContentsMargins(8, 2, 4, 2);
    shortcutLayout->setSpacing(14);

    shortcutFileGroup = new QGroupBox(shortcutPage);
    auto* shortcutFileLayout = new QFormLayout(shortcutFileGroup);
    shortcutFileLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    shortcutOpenProjectLabel = new QLabel(shortcutFileGroup);
    shortcutSaveProjectLabel = new QLabel(shortcutFileGroup);
    shortcutFileLayout->addRow(new QLabel(QStringLiteral("Ctrl+O"), shortcutFileGroup),
                               shortcutOpenProjectLabel);
    shortcutFileLayout->addRow(new QLabel(QStringLiteral("Ctrl+S"), shortcutFileGroup),
                               shortcutSaveProjectLabel);
    shortcutLayout->addWidget(shortcutFileGroup);

    shortcutCanvasGroup = new QGroupBox(shortcutPage);
    auto* shortcutCanvasLayout = new QFormLayout(shortcutCanvasGroup);
    shortcutCanvasLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    shortcutUndoLabel = new QLabel(shortcutCanvasGroup);
    shortcutRedoLabel = new QLabel(shortcutCanvasGroup);
    shortcutDeleteLabel = new QLabel(shortcutCanvasGroup);
    shortcutFindLabel = new QLabel(shortcutCanvasGroup);
    shortcutCanvasLayout->addRow(new QLabel(QStringLiteral("Ctrl+Z"), shortcutCanvasGroup),
                                 shortcutUndoLabel);
    shortcutCanvasLayout->addRow(new QLabel(QStringLiteral("Ctrl+Y"), shortcutCanvasGroup),
                                 shortcutRedoLabel);
    shortcutCanvasLayout->addRow(new QLabel(QStringLiteral("Delete"), shortcutCanvasGroup),
                                 shortcutDeleteLabel);
    shortcutCanvasLayout->addRow(new QLabel(QStringLiteral("Ctrl+F"), shortcutCanvasGroup),
                                 shortcutFindLabel);
    shortcutLayout->addWidget(shortcutCanvasGroup);
    shortcutLayout->addStretch(1);
    addScrollablePage(shortcutPage);

    auto* informationPage = new QWidget;
    auto* informationLayout = new QFormLayout(informationPage);
    informationLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    informationApplicationCaption = new QLabel(informationPage);
    informationVersionCaption = new QLabel(informationPage);
    informationQtCaption = new QLabel(informationPage);
    informationOpenCvCaption = new QLabel(informationPage);
    informationVulkanCaption = new QLabel(informationPage);
    informationLayout->addRow(informationApplicationCaption,
                              new QLabel(QCoreApplication::applicationName(), informationPage));
    informationLayout->addRow(informationVersionCaption,
                              new QLabel(QCoreApplication::applicationVersion(), informationPage));
    informationLayout->addRow(informationQtCaption,
                              new QLabel(QString::fromLatin1(qVersion()), informationPage));
    informationLayout->addRow(informationOpenCvCaption,
                              new QLabel(QString::fromLatin1(CV_VERSION), informationPage));
    informationVulkanLabel = new QLabel(informationPage);
    informationVulkanLabel->setWordWrap(true);
    informationVulkanLabel->setForegroundRole(QPalette::PlaceholderText);
    informationLayout->addRow(informationVulkanCaption, informationVulkanLabel);
    addScrollablePage(informationPage);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    closeButton = buttonBox->button(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::close);
    rootLayout->addWidget(buttonBox);

    connect(tabList, &QListWidget::currentRowChanged,
            pageStack, &QStackedWidget::setCurrentIndex);
    tabList->setCurrentRow(0);

    connect(useVulkanCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        AppSettings::setVulkanEnabled(enabled);
        updateVulkanControls();
    });
    connect(ignoreVramLimitCheck, &QCheckBox::toggled, this, [](bool ignore) {
        AppSettings::setIgnoreVramLimit(ignore);
    });
    connect(confirmProjectSaveCheck, &QCheckBox::toggled, this, [](bool enabled) {
        AppSettings::setConfirmProjectSaveOnClose(enabled);
    });
    connect(gpuCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            AppSettings::setVulkanDeviceKey(gpuCombo->itemData(index).toString());
        }
    });

    reloadFromSettings();
    retranslateUi();
    updateMinimumHeightForTabs();
    updateVulkanControls();
    updateVulkanStatusText();
}

void ApplicationSettingsDialog::changeEvent(QEvent* event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
        updateMinimumHeightForTabs();
        updateVulkanStatusText();
    }
}

void ApplicationSettingsDialog::updateMinimumHeightForTabs()
{
    if (!tabList || !layout()) {
        return;
    }

    int tabHeight = 2 * tabList->frameWidth();
    for (int row = 0; row < tabList->count(); ++row) {
        tabHeight += tabList->sizeHintForRow(row);
    }
    if (tabList->count() > 1) {
        tabHeight += (tabList->count() - 1) * tabList->spacing();
    }
    // Include the list's stylesheet padding without imposing extra blank rows.
    tabHeight += 8;
    tabList->setMinimumHeight(tabHeight);

    layout()->activate();
    setMinimumHeight(layout()->minimumSize().height());
}

void ApplicationSettingsDialog::retranslateUi()
{
    setWindowTitle(tr("設定"));
    tabList->item(0)->setText(tr("一般"));
    tabList->item(1)->setText(tr("リセット"));
    tabList->item(2)->setText(tr("ショートカット"));
    tabList->item(3)->setText(tr("情報"));

    themeGroup->setTitle(tr("テーマ"));
    systemThemeButton->setText(tr("システム"));
    lightThemeButton->setText(tr("ライト"));
    darkThemeButton->setText(tr("ダーク"));

    languageGroup->setTitle(tr("言語"));
    systemLanguageButton->setText(tr("システム"));
    japaneseLanguageButton->setText(tr("日本語"));
    englishLanguageButton->setText(tr("英語"));

    const bool metalBuild = MetalSsimEngine::isBuilt();
    vulkanGroup->setTitle(metalBuild ? tr("GPU計算") : tr("Vulkan"));
    useVulkanCheck->setText(
        metalBuild ? tr("利用可能ならMetal GPU計算を使用する")
                   : tr("利用可能ならVulkan GPU計算を使用する"));
    ignoreVramLimitCheck->setText(tr("VRAM limitを無視する"));
    gpuCaptionLabel->setText(tr("使用するGPU:"));

    projectFileGroup->setTitle(tr("プロジェクトファイル"));
    confirmProjectSaveCheck->setText(
        tr("ウインドウを閉じる時にプロジェクトファイル保存を確認する"));

    resetGroup->setTitle(tr("デフォルト設定にリセット"));
    resetAllButton->setText(tr("全て"));
    resetApplicationButton->setText(tr("設定ダイアログ"));
    resetAlignmentButton->setText(tr("画像の重なりの目安"));
    resetLayoutButton->setText(tr("レイアウト"));
    resetLeastSquaresButton->setText(
        tr("位置合わせ最適化（最小二乗法）"));
    resetTrwsPamiButton->setText(
        tr("位置合わせ最適化（TRW-S-PAMI）"));
    resetImageMergeButton->setText(tr("画像を作成"));

    shortcutFileGroup->setTitle(tr("ファイル"));
    shortcutCanvasGroup->setTitle(tr("キャンパス"));
    shortcutOpenProjectLabel->setText(tr("プロジェクトを開く"));
    shortcutSaveProjectLabel->setText(tr("プロジェクト保存"));
    shortcutUndoLabel->setText(tr("変更履歴を戻す"));
    shortcutRedoLabel->setText(tr("変更履歴を進める"));
    shortcutDeleteLabel->setText(tr("選択中の画像を削除"));
    shortcutFindLabel->setText(tr("画像をハイライト"));

    informationApplicationCaption->setText(tr("アプリケーション:"));
    informationVersionCaption->setText(tr("バージョン:"));
    informationQtCaption->setText(tr("Qt:"));
    informationOpenCvCaption->setText(tr("OpenCV:"));
    informationVulkanCaption->setText(metalBuild ? tr("GPU計算:") : tr("Vulkan:"));
    closeButton->setText(tr("閉じる"));
}

void ApplicationSettingsDialog::reloadFromSettings()
{
    const QSignalBlocker systemThemeBlocker(systemThemeButton);
    const QSignalBlocker lightThemeBlocker(lightThemeButton);
    const QSignalBlocker darkThemeBlocker(darkThemeButton);
    const QSignalBlocker systemLanguageBlocker(systemLanguageButton);
    const QSignalBlocker japaneseLanguageBlocker(japaneseLanguageButton);
    const QSignalBlocker englishLanguageBlocker(englishLanguageButton);
    const QSignalBlocker vulkanBlocker(useVulkanCheck);
    const QSignalBlocker limitBlocker(ignoreVramLimitCheck);
    const QSignalBlocker gpuBlocker(gpuCombo);
    const QSignalBlocker confirmProjectBlocker(confirmProjectSaveCheck);

    const ApplicationTheme currentTheme = AppSettings::theme();
    if (QAbstractButton* currentThemeButton =
            themeButtonGroup->button(static_cast<int>(currentTheme))) {
        currentThemeButton->setChecked(true);
    }

    const ApplicationLanguage currentLanguage = AppSettings::language();
    if (QAbstractButton* currentLanguageButton =
            languageButtonGroup->button(static_cast<int>(currentLanguage))) {
        currentLanguageButton->setChecked(true);
    }

    const VulkanExecutionOptions options = AppSettings::vulkanOptions();
    useVulkanCheck->setChecked(options.enabled);
    ignoreVramLimitCheck->setChecked(options.ignoreVramLimit);
    confirmProjectSaveCheck->setChecked(AppSettings::confirmProjectSaveOnClose());
    int gpuIndex = gpuCombo->findData(options.deviceKey);
    if (gpuIndex < 0 && gpuCombo->count() > 0) {
        gpuIndex = 0;
    }
    if (gpuIndex >= 0) {
        gpuCombo->setCurrentIndex(gpuIndex);
    }

    applyApplicationTheme(currentTheme);
    applyApplicationLanguage(currentLanguage);
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

    updateVulkanControls();
    updateVulkanStatusText();
}

void ApplicationSettingsDialog::updateVulkanControls()
{
    const bool metalAvailable = MetalSsimEngine::isBuilt()
                                && MetalSsimEngine::isAvailable();
    const bool vulkanAvailable = !detectionInProgress
                                 && !scanResult.devices.isEmpty();
    const bool available = metalAvailable || vulkanAvailable;
    useVulkanCheck->setEnabled(available);
    const bool detailsEnabled = available && useVulkanCheck->isChecked();
    ignoreVramLimitCheck->setEnabled(detailsEnabled);
    gpuCaptionLabel->setVisible(!metalAvailable);
    gpuCombo->setVisible(!metalAvailable);
    gpuCombo->setEnabled(detailsEnabled && !metalAvailable);
}

void ApplicationSettingsDialog::updateVulkanStatusText()
{
    QString status;
    if (MetalSsimEngine::isBuilt() && MetalSsimEngine::isAvailable()) {
        status = tr("Metal GPU「%1」を検出しました。メモリ上限は推奨ワーキングセットの70%です。")
                     .arg(MetalSsimEngine::deviceName());
    } else if (!VulkanSsimEngine::isBuilt()) {
        status = tr("このビルドではVulkan計算が無効です。CPU計算を使用します。");
    } else if (detectionInProgress) {
        status = tr("GPUを検出中…");
    } else if (!scanResult.error.isEmpty()) {
        status = scanResult.error + tr(" CPU計算を使用します。");
    } else if (scanResult.devices.isEmpty()) {
        status = tr("計算に利用できるVulkan GPUが見つかりません。CPU計算を使用します。");
    } else {
        status = tr("%1台のVulkan GPUを検出しました。VRAM上限は選択GPUの70%です。")
                     .arg(scanResult.devices.size());
    }
    vulkanStatusLabel->setText(status);
    informationVulkanLabel->setText(status);
}
