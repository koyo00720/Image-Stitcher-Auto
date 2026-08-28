#include "app_settings.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QPalette>
#include <QSettings>
#include <QSaveFile>
#include <QTranslator>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QStyleHints>
#endif

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <utility>

namespace
{
constexpr auto kThemeKey = "general/theme";
constexpr auto kLanguageKey = "general/language";
constexpr auto kVulkanEnabledKey = "vulkan/enabled";
constexpr auto kVulkanIgnoreLimitKey = "vulkan/ignoreVramLimit";
constexpr auto kVulkanDeviceKey = "vulkan/deviceKey";
constexpr auto kCanvasBackgroundKey = "canvas/background";
constexpr auto kDefaultsFileName = "Image_Stitcher_Auto.conf";
constexpr auto kEnglishTranslationFileName = "Image_Stitcher_Auto_en.qm";

constexpr auto kStateApplicationSection = "stateApplication";
constexpr auto kStateVulkanSection = "stateVulkan";
constexpr auto kStateCanvasSection = "stateCanvas";
constexpr auto kStateAlignmentSection = "stateAlignment";
constexpr auto kStateArrangementSection = "stateArrangement";
constexpr auto kStateTrwsPamiSection = "stateTrwsPami";
constexpr auto kStateLeastSquaresSection = "stateLeastSquares";
constexpr auto kStateImageMergeSection = "stateImageMerge";
constexpr auto kStateWindowsSection = "stateWindows";

QTranslator& applicationTranslator()
{
    static QTranslator translator;
    return translator;
}

bool& applicationTranslatorInstalled()
{
    static bool installed = false;
    return installed;
}

QString normalizedToken(QString value)
{
    value = value.trimmed().toLower();
    value.remove('-');
    value.remove('_');
    value.remove(' ');
    return value;
}

bool isIniSectionHeader(const QString& line)
{
    const QString trimmed = line.trimmed();
    return trimmed.startsWith(QLatin1Char('['))
           && trimmed.endsWith(QLatin1Char(']'));
}

QString iniSectionName(const QString& line)
{
    const QString trimmed = line.trimmed();
    return isIniSectionHeader(trimmed)
               ? trimmed.mid(1, trimmed.size() - 2).trimmed()
               : QString();
}

void persistIniValue(const QString& filePath,
                     const QString& section,
                     const QString& key,
                     const std::optional<QString>& value)
{
    QFile input(filePath);
    QString text;
    if (input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        text = QString::fromUtf8(input.readAll());
        input.close();
    }

    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    if (lines.size() == 1 && lines.first().isEmpty()) {
        lines.clear();
    }

    int sectionLine = -1;
    int sectionEnd = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        if (!isIniSectionHeader(lines[i])) {
            continue;
        }
        if (sectionLine >= 0) {
            sectionEnd = i;
            break;
        }
        if (iniSectionName(lines[i]).compare(section, Qt::CaseInsensitive) == 0) {
            sectionLine = i;
        }
    }

    int keyLine = -1;
    if (sectionLine >= 0) {
        for (int i = sectionLine + 1; i < sectionEnd; ++i) {
            const QString trimmed = lines[i].trimmed();
            if (trimmed.startsWith(QLatin1Char(';'))
                || trimmed.startsWith(QLatin1Char('#'))) {
                continue;
            }
            const int equals = trimmed.indexOf(QLatin1Char('='));
            if (equals >= 0
                && trimmed.left(equals).trimmed().compare(
                       key, Qt::CaseInsensitive) == 0) {
                keyLine = i;
                break;
            }
        }
    }

    if (value.has_value()) {
        const QString settingLine = key + QLatin1Char('=') + value.value();
        if (keyLine >= 0) {
            lines[keyLine] = settingLine;
        } else if (sectionLine >= 0) {
            lines.insert(sectionEnd, settingLine);
        } else {
            if (!lines.isEmpty() && !lines.last().isEmpty()) {
                lines.push_back(QString());
            }
            lines.push_back(QLatin1Char('[') + section + QLatin1Char(']'));
            lines.push_back(settingLine);
        }
    } else if (keyLine >= 0) {
        lines.removeAt(keyLine);
    } else {
        return;
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    const QByteArray encoded = lines.join(QLatin1Char('\n')).toUtf8();
    if (output.write(encoded) != encoded.size()) {
        output.cancelWriting();
        return;
    }
    output.commit();
}

void removeIniSection(const QString& filePath, const QString& section)
{
    QFile input(filePath);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QString text = QString::fromUtf8(input.readAll());
    input.close();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    int sectionLine = -1;
    int sectionEnd = lines.size();
    for (int i = 0; i < lines.size(); ++i) {
        if (!isIniSectionHeader(lines[i])) {
            continue;
        }
        if (sectionLine >= 0) {
            sectionEnd = i;
            break;
        }
        if (iniSectionName(lines[i]).compare(section, Qt::CaseInsensitive) == 0) {
            sectionLine = i;
        }
    }
    if (sectionLine < 0) {
        return;
    }

    lines.erase(lines.begin() + sectionLine, lines.begin() + sectionEnd);
    while (sectionLine > 0 && sectionLine <= lines.size()
           && lines[sectionLine - 1].isEmpty()
           && (sectionLine == lines.size() || lines[sectionLine].isEmpty())) {
        lines.removeAt(sectionLine - 1);
        --sectionLine;
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    const QByteArray encoded = lines.join(QLatin1Char('\n')).toUtf8();
    if (output.write(encoded) != encoded.size()) {
        output.cancelWriting();
        return;
    }
    output.commit();
}

QVariant persistedValue(const char* userKey,
                        const char* stateSection,
                        const char* stateKey,
                        const QVariant& fallback)
{
    QSettings userSettings;
    if (userSettings.contains(userKey)) {
        return userSettings.value(userKey);
    }

    QSettings fileSettings(AppSettings::defaultsFilePath(), QSettings::IniFormat);
    const QString fileKey = QString::fromLatin1(stateSection)
                            + QLatin1Char('/')
                            + QString::fromLatin1(stateKey);
    return fileSettings.contains(fileKey) ? fileSettings.value(fileKey) : fallback;
}

void setPersistedValue(const char* userKey,
                       const char* stateSection,
                       const char* stateKey,
                       const QVariant& value,
                       const QString& fileValue)
{
    QSettings userSettings;
    userSettings.setValue(userKey, value);
    userSettings.sync();
    persistIniValue(AppSettings::defaultsFilePath(),
                    QString::fromLatin1(stateSection),
                    QString::fromLatin1(stateKey),
                    fileValue);
}

void resetPersistedValue(const char* userKey,
                         const char* stateSection,
                         const char* stateKey)
{
    QSettings userSettings;
    userSettings.remove(userKey);
    userSettings.sync();
    persistIniValue(AppSettings::defaultsFilePath(),
                    QString::fromLatin1(stateSection),
                    QString::fromLatin1(stateKey),
                    std::nullopt);
}

int boundedPersistedInt(const char* userKey,
                        const char* stateSection,
                        const char* stateKey,
                        int fallback,
                        int minimum,
                        int maximum)
{
    bool ok = false;
    const int value = persistedValue(userKey, stateSection, stateKey, fallback)
                          .toInt(&ok);
    return ok ? std::clamp(value, minimum, maximum) : fallback;
}

double boundedPersistedDouble(const char* userKey,
                              const char* stateSection,
                              const char* stateKey,
                              double fallback,
                              double minimum,
                              double maximum)
{
    bool ok = false;
    const double value = persistedValue(userKey, stateSection, stateKey, fallback)
                             .toDouble(&ok);
    return ok && std::isfinite(value)
               ? std::clamp(value, minimum, maximum)
               : fallback;
}

bool readBool(QSettings& settings, const char* key, bool fallback)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    const QString value = normalizedToken(settings.value(key).toString());
    if (value == "true" || value == "yes" || value == "on" || value == "1") {
        return true;
    }
    if (value == "false" || value == "no" || value == "off" || value == "0") {
        return false;
    }
    return fallback;
}

int readBoundedInt(QSettings& settings,
                   const char* key,
                   int fallback,
                   int minimum,
                   int maximum)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const int value = settings.value(key).toInt(&ok);
    return ok ? std::clamp(value, minimum, maximum) : fallback;
}

double readBoundedDouble(QSettings& settings,
                         const char* key,
                         double fallback,
                         double minimum,
                         double maximum)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    bool ok = false;
    const double value = settings.value(key).toDouble(&ok);
    return ok && std::isfinite(value)
               ? std::clamp(value, minimum, maximum)
               : fallback;
}

int readChoice(QSettings& settings,
               const char* key,
               int fallback,
               std::initializer_list<std::pair<const char*, int>> choices)
{
    if (!settings.contains(key)) {
        return fallback;
    }

    const QVariant rawValue = settings.value(key);
    bool numericOk = false;
    const int numericValue = rawValue.toInt(&numericOk);
    if (numericOk) {
        for (const auto& choice : choices) {
            if (numericValue == choice.second) {
                return numericValue;
            }
        }
    }

    const QString value = normalizedToken(rawValue.toString());
    for (const auto& choice : choices) {
        if (value == normalizedToken(QString::fromLatin1(choice.first))) {
            return choice.second;
        }
    }
    return fallback;
}

ApplicationDefaultSettings loadDefaultSettings(const QString& filePath)
{
    ApplicationDefaultSettings defaults;
    QSettings settings(filePath, QSettings::IniFormat);

    defaults.theme = static_cast<ApplicationTheme>(
        readChoice(settings,
                   "application/theme",
                   static_cast<int>(defaults.theme),
                   {{"system", 0}, {"light", 1}, {"dark", 2}}));

    defaults.language = static_cast<ApplicationLanguage>(
        readChoice(settings,
                   "application/language",
                   static_cast<int>(defaults.language),
                   {{"system", 0}, {"japanese", 1}, {"ja", 1}, {"english", 2}, {"en", 2}}));

    defaults.vulkan.enabled =
        readBool(settings, "vulkan/enabled", defaults.vulkan.enabled);
    defaults.vulkan.ignoreVramLimit =
        readBool(settings, "vulkan/ignoreVramLimit", defaults.vulkan.ignoreVramLimit);
    defaults.vulkan.deviceKey =
        settings.value("vulkan/deviceKey", defaults.vulkan.deviceKey).toString().trimmed();

    defaults.fileInput.sortMode =
        readChoice(settings,
                   "fileInput/sortMode",
                   defaults.fileInput.sortMode,
                   {{"fileNameAscending", 0},
                    {"fileNameDescending", 1},
                    {"dateTimeAscending", 2},
                    {"dateTimeDescending", 3}});

    defaults.alignment.horizontalOverlapPercent =
        readBoundedInt(settings,
                       "alignment/horizontalOverlapPercent",
                       defaults.alignment.horizontalOverlapPercent,
                       1,
                       100);
    defaults.alignment.verticalOverlapPercent =
        readBoundedInt(settings,
                       "alignment/verticalOverlapPercent",
                       defaults.alignment.verticalOverlapPercent,
                       1,
                       100);
    defaults.alignment.searchRangePercent =
        readBoundedInt(settings,
                       "alignment/searchRangePercent",
                       defaults.alignment.searchRangePercent,
                       0,
                       100);

    defaults.arrangement.direction =
        readBoundedInt(settings,
                       "arrangement/direction",
                       defaults.arrangement.direction,
                       1,
                       8);
    defaults.arrangement.horizontalImageCount =
        readBoundedInt(settings,
                       "arrangement/horizontalImageCount",
                       defaults.arrangement.horizontalImageCount,
                       0,
                       100000);
    defaults.arrangement.verticalImageCount =
        readBoundedInt(settings,
                       "arrangement/verticalImageCount",
                       defaults.arrangement.verticalImageCount,
                       0,
                       100000);
    defaults.arrangement.zigzag =
        readBool(settings, "arrangement/zigzag", defaults.arrangement.zigzag);

    const QString backgroundColor =
        settings.value("canvas/background", defaults.canvas.backgroundColor).toString().trimmed();
    const QString normalizedBackground = normalizedToken(backgroundColor);
    if (normalizedBackground == "none" || normalizedBackground == "transparent"
        || normalizedBackground == "nocolor") {
        defaults.canvas.backgroundColor = "none";
    } else if (QColor(backgroundColor).isValid()) {
        defaults.canvas.backgroundColor = backgroundColor;
    }
    defaults.canvas.selectedImageOpacityPercent =
        readBoundedInt(settings,
                       "canvas/selectedImageOpacityPercent",
                       defaults.canvas.selectedImageOpacityPercent,
                       0,
                       100);
    defaults.canvas.layoutLocked =
        readBool(settings, "canvas/layoutLocked", defaults.canvas.layoutLocked);
    defaults.canvas.useCanvasAsSource =
        readBool(settings, "canvas/useCanvasAsSource", defaults.canvas.useCanvasAsSource);
    const QString highlightColor =
        settings.value("canvas/highlightColor", defaults.canvas.highlightColor).toString().trimmed();
    if (QColor(highlightColor).isValid()) {
        defaults.canvas.highlightColor = highlightColor;
    }

    defaults.trwsPami.localEnabled =
        readBool(settings, "trwsPami/localEnabled", defaults.trwsPami.localEnabled);
    defaults.trwsPami.localImageCount =
        readBoundedInt(settings,
                       "trwsPami/localImageCount",
                       defaults.trwsPami.localImageCount,
                       4,
                       100);
    defaults.trwsPami.localAutoIncrement =
        readBool(settings,
                 "trwsPami/localAutoIncrement",
                 defaults.trwsPami.localAutoIncrement);
    defaults.trwsPami.localImageCountIncrement =
        readBoundedInt(settings,
                       "trwsPami/localImageCountIncrement",
                       defaults.trwsPami.localImageCountIncrement,
                       0,
                       100);
    defaults.trwsPami.localIncrementCount =
        readBoundedInt(settings,
                       "trwsPami/localIncrementCount",
                       defaults.trwsPami.localIncrementCount,
                       1,
                       100);
    defaults.trwsPami.localSearchRadius =
        readBoundedInt(settings,
                       "trwsPami/localSearchRadius",
                       defaults.trwsPami.localSearchRadius,
                       1,
                       10);
    defaults.trwsPami.localMaxIterations =
        readBoundedInt(settings,
                       "trwsPami/localMaxIterations",
                       defaults.trwsPami.localMaxIterations,
                       20,
                       100000);
    defaults.trwsPami.localMaxLoops =
        readBoundedInt(settings,
                       "trwsPami/localMaxLoops",
                       defaults.trwsPami.localMaxLoops,
                       1,
                       20);
    defaults.trwsPami.globalEnabled =
        readBool(settings, "trwsPami/globalEnabled", defaults.trwsPami.globalEnabled);
    defaults.trwsPami.globalSearchRadius =
        readBoundedInt(settings,
                       "trwsPami/globalSearchRadius",
                       defaults.trwsPami.globalSearchRadius,
                       1,
                       10);
    defaults.trwsPami.globalMaxIterations =
        readBoundedInt(settings,
                       "trwsPami/globalMaxIterations",
                       defaults.trwsPami.globalMaxIterations,
                       20,
                       100000);
    defaults.trwsPami.globalMaxLoops =
        readBoundedInt(settings,
                       "trwsPami/globalMaxLoops",
                       defaults.trwsPami.globalMaxLoops,
                       1,
                       50);

    defaults.leastSquares.regressionThreshold =
        readBoundedDouble(settings,
                          "leastSquares/regressionThreshold",
                          defaults.leastSquares.regressionThreshold,
                          0.0,
                          1.0);
    defaults.leastSquares.relativeThreshold =
        readBoundedDouble(settings,
                          "leastSquares/relativeThreshold",
                          defaults.leastSquares.relativeThreshold,
                          0.0,
                          100.0);
    defaults.leastSquares.absoluteThreshold =
        readBoundedDouble(settings,
                          "leastSquares/absoluteThreshold",
                          defaults.leastSquares.absoluteThreshold,
                          0.0,
                          1000.0);
    defaults.leastSquares.maxPairErrorForRelative =
        readBoundedDouble(settings,
                          "leastSquares/maxPairErrorForRelative",
                          defaults.leastSquares.maxPairErrorForRelative,
                          0.0,
                          1000.0);

    defaults.imageMerge.mode =
        readChoice(settings,
                   "imageMerge/mode",
                   defaults.imageMerge.mode,
                   {{"distanceL2", 0},
                    {"focusRegion", 1},
                    {"focusStackTenengrad", 2}});

    return defaults;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 8, 0)
const QPalette& originalSystemPalette()
{
    // 最初のテーマ適用前にQtが取得したシステムパレットを保持する。
    static const QPalette palette = qApp ? qApp->palette() : QPalette();
    return palette;
}

QPalette fallbackLightPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(243, 243, 243));
    palette.setColor(QPalette::WindowText, Qt::black);
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor(247, 247, 247));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, Qt::black);
    palette.setColor(QPalette::Text, Qt::black);
    palette.setColor(QPalette::Button, Qt::white);
    palette.setColor(QPalette::ButtonText, Qt::black);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(109, 109, 109));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(109, 109, 109));
    return palette;
}

QPalette fallbackDarkPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 30));
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Base, QColor(45, 45, 45));
    palette.setColor(QPalette::AlternateBase, QColor(60, 60, 60));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 45));
    palette.setColor(QPalette::ToolTipText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, QColor(60, 60, 60));
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::BrightText, QColor(255, 96, 96));
    palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    return palette;
}
#endif
}

const ApplicationDefaultSettings& AppSettings::defaults()
{
    static const ApplicationDefaultSettings values = loadDefaultSettings(defaultsFilePath());
    return values;
}

QString AppSettings::defaultsFilePath()
{
    const QString applicationDirectory = QCoreApplication::instance()
                                             ? QCoreApplication::applicationDirPath()
                                             : QDir::currentPath();
    const QString applicationPath =
        QDir(applicationDirectory).filePath(QString::fromLatin1(kDefaultsFileName));
    if (QFileInfo::exists(applicationPath)) {
        return applicationPath;
    }

    // ソースツリーから直接実行する開発用途も許容する。
    const QString workingDirectoryPath =
        QDir::current().filePath(QString::fromLatin1(kDefaultsFileName));
    return QFileInfo::exists(workingDirectoryPath) ? workingDirectoryPath : applicationPath;
}

ApplicationTheme AppSettings::theme()
{
    const ApplicationTheme defaultTheme = defaults().theme;
    const int value = persistedValue(
                          kThemeKey, kStateApplicationSection, "theme",
                          static_cast<int>(defaultTheme)).toInt();
    switch (value) {
    case static_cast<int>(ApplicationTheme::System):
        return ApplicationTheme::System;
    case static_cast<int>(ApplicationTheme::Light):
        return ApplicationTheme::Light;
    case static_cast<int>(ApplicationTheme::Dark):
        return ApplicationTheme::Dark;
    default:
        return defaultTheme;
    }
}

void AppSettings::setTheme(ApplicationTheme theme)
{
    setPersistedValue(kThemeKey, kStateApplicationSection, "theme",
                      static_cast<int>(theme),
                      QString::number(static_cast<int>(theme)));
}

ApplicationLanguage AppSettings::language()
{
    const ApplicationLanguage defaultLanguage = defaults().language;
    const int value = persistedValue(
                          kLanguageKey, kStateApplicationSection, "language",
                          static_cast<int>(defaultLanguage)).toInt();
    switch (value) {
    case static_cast<int>(ApplicationLanguage::System):
        return ApplicationLanguage::System;
    case static_cast<int>(ApplicationLanguage::Japanese):
        return ApplicationLanguage::Japanese;
    case static_cast<int>(ApplicationLanguage::English):
        return ApplicationLanguage::English;
    default:
        return defaultLanguage;
    }
}

void AppSettings::setLanguage(ApplicationLanguage language)
{
    setPersistedValue(kLanguageKey, kStateApplicationSection, "language",
                      static_cast<int>(language),
                      QString::number(static_cast<int>(language)));
}

VulkanExecutionOptions AppSettings::vulkanOptions()
{
    VulkanExecutionOptions options;
    const VulkanExecutionOptions& defaultOptions = defaults().vulkan;
    options.enabled = persistedValue(
                          kVulkanEnabledKey, kStateVulkanSection, "enabled",
                          defaultOptions.enabled).toBool();
    options.ignoreVramLimit = persistedValue(
                                  kVulkanIgnoreLimitKey, kStateVulkanSection,
                                  "ignoreVramLimit",
                                  defaultOptions.ignoreVramLimit).toBool();
    options.deviceKey = persistedValue(
                            kVulkanDeviceKey, kStateVulkanSection, "deviceKey",
                            defaultOptions.deviceKey).toString();
    return options;
}

void AppSettings::setVulkanEnabled(bool enabled)
{
    setPersistedValue(kVulkanEnabledKey, kStateVulkanSection, "enabled",
                      enabled, enabled ? QStringLiteral("true")
                                       : QStringLiteral("false"));
}

void AppSettings::setIgnoreVramLimit(bool ignore)
{
    setPersistedValue(kVulkanIgnoreLimitKey, kStateVulkanSection,
                      "ignoreVramLimit", ignore,
                      ignore ? QStringLiteral("true")
                             : QStringLiteral("false"));
}

void AppSettings::setVulkanDeviceKey(const QString& key)
{
    setPersistedValue(kVulkanDeviceKey, kStateVulkanSection, "deviceKey",
                      key, key);
}

QString AppSettings::canvasBackground()
{
    const QString fallback = defaults().canvas.backgroundColor;
    const QString value = persistedValue(
                              kCanvasBackgroundKey, kStateCanvasSection,
                              "background", fallback).toString().trimmed();
    const QString normalized = normalizedToken(value);
    if (normalized == "none" || normalized == "transparent"
        || normalized == "nocolor") {
        return QStringLiteral("none");
    }
    return QColor(value).isValid() ? value : fallback;
}

void AppSettings::setCanvasBackground(const QString& setting)
{
    const QString normalized = normalizedToken(setting);
    QString value;
    if (normalized == "none" || normalized == "transparent"
        || normalized == "nocolor") {
        value = QStringLiteral("none");
    } else {
        const QColor color(setting);
        if (!color.isValid()) {
            return;
        }
        value = color.name(QColor::HexArgb);
    }

    setPersistedValue(kCanvasBackgroundKey, kStateCanvasSection, "background",
                      value, value);
}

AlignmentDefaultSettings AppSettings::alignmentOptions()
{
    AlignmentDefaultSettings options = defaults().alignment;
    options.horizontalOverlapPercent = boundedPersistedInt(
        "alignment/horizontalOverlapPercent", kStateAlignmentSection,
        "horizontalOverlapPercent", options.horizontalOverlapPercent, 1, 100);
    options.verticalOverlapPercent = boundedPersistedInt(
        "alignment/verticalOverlapPercent", kStateAlignmentSection,
        "verticalOverlapPercent", options.verticalOverlapPercent, 1, 100);
    options.searchRangePercent = boundedPersistedInt(
        "alignment/searchRangePercent", kStateAlignmentSection,
        "searchRangePercent", options.searchRangePercent, 0, 100);
    return options;
}

void AppSettings::setAlignmentOptions(const AlignmentDefaultSettings& options)
{
    const int horizontal = std::clamp(options.horizontalOverlapPercent, 1, 100);
    const int vertical = std::clamp(options.verticalOverlapPercent, 1, 100);
    const int search = std::clamp(options.searchRangePercent, 0, 100);
    setPersistedValue("alignment/horizontalOverlapPercent",
                      kStateAlignmentSection, "horizontalOverlapPercent",
                      horizontal, QString::number(horizontal));
    setPersistedValue("alignment/verticalOverlapPercent",
                      kStateAlignmentSection, "verticalOverlapPercent",
                      vertical, QString::number(vertical));
    setPersistedValue("alignment/searchRangePercent",
                      kStateAlignmentSection, "searchRangePercent",
                      search, QString::number(search));
}

void AppSettings::resetAlignmentOptions()
{
    resetPersistedValue("alignment/horizontalOverlapPercent",
                        kStateAlignmentSection, "horizontalOverlapPercent");
    resetPersistedValue("alignment/verticalOverlapPercent",
                        kStateAlignmentSection, "verticalOverlapPercent");
    resetPersistedValue("alignment/searchRangePercent",
                        kStateAlignmentSection, "searchRangePercent");
}

ArrangementDefaultSettings AppSettings::arrangementOptions()
{
    ArrangementDefaultSettings options = defaults().arrangement;
    options.direction = boundedPersistedInt(
        "arrangement/direction", kStateArrangementSection, "direction",
        options.direction, 1, 8);
    options.horizontalImageCount = boundedPersistedInt(
        "arrangement/horizontalImageCount", kStateArrangementSection,
        "horizontalImageCount", options.horizontalImageCount, 0, 100000);
    options.verticalImageCount = boundedPersistedInt(
        "arrangement/verticalImageCount", kStateArrangementSection,
        "verticalImageCount", options.verticalImageCount, 0, 100000);
    options.zigzag = persistedValue(
                          "arrangement/zigzag", kStateArrangementSection,
                          "zigzag", options.zigzag).toBool();
    return options;
}

void AppSettings::setArrangementOptions(const ArrangementDefaultSettings& options)
{
    const int direction = std::clamp(options.direction, 1, 8);
    const int horizontal = std::clamp(options.horizontalImageCount, 0, 100000);
    const int vertical = std::clamp(options.verticalImageCount, 0, 100000);
    setPersistedValue("arrangement/direction", kStateArrangementSection,
                      "direction", direction, QString::number(direction));
    setPersistedValue("arrangement/horizontalImageCount",
                      kStateArrangementSection, "horizontalImageCount",
                      horizontal, QString::number(horizontal));
    setPersistedValue("arrangement/verticalImageCount",
                      kStateArrangementSection, "verticalImageCount",
                      vertical, QString::number(vertical));
    setPersistedValue("arrangement/zigzag", kStateArrangementSection,
                      "zigzag", options.zigzag,
                      options.zigzag ? QStringLiteral("true")
                                     : QStringLiteral("false"));
}

void AppSettings::resetArrangementOptions()
{
    resetPersistedValue("arrangement/direction", kStateArrangementSection,
                        "direction");
    resetPersistedValue("arrangement/horizontalImageCount",
                        kStateArrangementSection, "horizontalImageCount");
    resetPersistedValue("arrangement/verticalImageCount",
                        kStateArrangementSection, "verticalImageCount");
    resetPersistedValue("arrangement/zigzag", kStateArrangementSection,
                        "zigzag");
}

TrwsPamiDefaultSettings AppSettings::trwsPamiOptions()
{
    TrwsPamiDefaultSettings options = defaults().trwsPami;
    options.localEnabled = persistedValue(
                               "trwsPami/localEnabled", kStateTrwsPamiSection,
                               "localEnabled", options.localEnabled).toBool();
    options.localImageCount = boundedPersistedInt(
        "trwsPami/localImageCount", kStateTrwsPamiSection, "localImageCount",
        options.localImageCount, 4, 100);
    options.localAutoIncrement = persistedValue(
                                     "trwsPami/localAutoIncrement",
                                     kStateTrwsPamiSection,
                                     "localAutoIncrement",
                                     options.localAutoIncrement).toBool();
    options.localImageCountIncrement = boundedPersistedInt(
        "trwsPami/localImageCountIncrement", kStateTrwsPamiSection,
        "localImageCountIncrement", options.localImageCountIncrement, 0, 100);
    options.localIncrementCount = boundedPersistedInt(
        "trwsPami/localIncrementCount", kStateTrwsPamiSection,
        "localIncrementCount", options.localIncrementCount, 1, 100);
    options.localSearchRadius = boundedPersistedInt(
        "trwsPami/localSearchRadius", kStateTrwsPamiSection,
        "localSearchRadius", options.localSearchRadius, 1, 10);
    options.localMaxIterations = boundedPersistedInt(
        "trwsPami/localMaxIterations", kStateTrwsPamiSection,
        "localMaxIterations", options.localMaxIterations, 20, 100000);
    options.localMaxLoops = boundedPersistedInt(
        "trwsPami/localMaxLoops", kStateTrwsPamiSection, "localMaxLoops",
        options.localMaxLoops, 1, 20);
    options.globalEnabled = persistedValue(
                                "trwsPami/globalEnabled", kStateTrwsPamiSection,
                                "globalEnabled", options.globalEnabled).toBool();
    options.globalSearchRadius = boundedPersistedInt(
        "trwsPami/globalSearchRadius", kStateTrwsPamiSection,
        "globalSearchRadius", options.globalSearchRadius, 1, 10);
    options.globalMaxIterations = boundedPersistedInt(
        "trwsPami/globalMaxIterations", kStateTrwsPamiSection,
        "globalMaxIterations", options.globalMaxIterations, 20, 100000);
    options.globalMaxLoops = boundedPersistedInt(
        "trwsPami/globalMaxLoops", kStateTrwsPamiSection, "globalMaxLoops",
        options.globalMaxLoops, 1, 50);
    return options;
}

void AppSettings::setTrwsPamiOptions(const TrwsPamiDefaultSettings& input)
{
    TrwsPamiDefaultSettings options = input;
    options.localImageCount = std::clamp(options.localImageCount, 4, 100);
    options.localImageCountIncrement =
        std::clamp(options.localImageCountIncrement, 0, 100);
    options.localIncrementCount = std::clamp(options.localIncrementCount, 1, 100);
    options.localSearchRadius = std::clamp(options.localSearchRadius, 1, 10);
    options.localMaxIterations = std::clamp(options.localMaxIterations, 20, 100000);
    options.localMaxLoops = std::clamp(options.localMaxLoops, 1, 20);
    options.globalSearchRadius = std::clamp(options.globalSearchRadius, 1, 10);
    options.globalMaxIterations = std::clamp(options.globalMaxIterations, 20, 100000);
    options.globalMaxLoops = std::clamp(options.globalMaxLoops, 1, 50);

    auto setBool = [](const char* userKey, const char* stateKey, bool value) {
        setPersistedValue(userKey, kStateTrwsPamiSection, stateKey, value,
                          value ? QStringLiteral("true")
                                : QStringLiteral("false"));
    };
    auto setInt = [](const char* userKey, const char* stateKey, int value) {
        setPersistedValue(userKey, kStateTrwsPamiSection, stateKey, value,
                          QString::number(value));
    };
    setBool("trwsPami/localEnabled", "localEnabled", options.localEnabled);
    setInt("trwsPami/localImageCount", "localImageCount", options.localImageCount);
    setBool("trwsPami/localAutoIncrement", "localAutoIncrement",
            options.localAutoIncrement);
    setInt("trwsPami/localImageCountIncrement", "localImageCountIncrement",
           options.localImageCountIncrement);
    setInt("trwsPami/localIncrementCount", "localIncrementCount",
           options.localIncrementCount);
    setInt("trwsPami/localSearchRadius", "localSearchRadius",
           options.localSearchRadius);
    setInt("trwsPami/localMaxIterations", "localMaxIterations",
           options.localMaxIterations);
    setInt("trwsPami/localMaxLoops", "localMaxLoops", options.localMaxLoops);
    setBool("trwsPami/globalEnabled", "globalEnabled", options.globalEnabled);
    setInt("trwsPami/globalSearchRadius", "globalSearchRadius",
           options.globalSearchRadius);
    setInt("trwsPami/globalMaxIterations", "globalMaxIterations",
           options.globalMaxIterations);
    setInt("trwsPami/globalMaxLoops", "globalMaxLoops", options.globalMaxLoops);
}

void AppSettings::resetTrwsPamiOptions()
{
    const std::pair<const char*, const char*> keys[] = {
        {"trwsPami/localEnabled", "localEnabled"},
        {"trwsPami/localImageCount", "localImageCount"},
        {"trwsPami/localAutoIncrement", "localAutoIncrement"},
        {"trwsPami/localImageCountIncrement", "localImageCountIncrement"},
        {"trwsPami/localIncrementCount", "localIncrementCount"},
        {"trwsPami/localSearchRadius", "localSearchRadius"},
        {"trwsPami/localMaxIterations", "localMaxIterations"},
        {"trwsPami/localMaxLoops", "localMaxLoops"},
        {"trwsPami/globalEnabled", "globalEnabled"},
        {"trwsPami/globalSearchRadius", "globalSearchRadius"},
        {"trwsPami/globalMaxIterations", "globalMaxIterations"},
        {"trwsPami/globalMaxLoops", "globalMaxLoops"}
    };
    for (const auto& key : keys) {
        resetPersistedValue(key.first, kStateTrwsPamiSection, key.second);
    }
}

LeastSquaresDefaultSettings AppSettings::leastSquaresOptions()
{
    LeastSquaresDefaultSettings options = defaults().leastSquares;
    options.regressionThreshold = boundedPersistedDouble(
        "leastSquares/regressionThreshold", kStateLeastSquaresSection,
        "regressionThreshold", options.regressionThreshold, 0.0, 1.0);
    options.relativeThreshold = boundedPersistedDouble(
        "leastSquares/relativeThreshold", kStateLeastSquaresSection,
        "relativeThreshold", options.relativeThreshold, 0.0, 100.0);
    options.absoluteThreshold = boundedPersistedDouble(
        "leastSquares/absoluteThreshold", kStateLeastSquaresSection,
        "absoluteThreshold", options.absoluteThreshold, 0.0, 1000.0);
    options.maxPairErrorForRelative = boundedPersistedDouble(
        "leastSquares/maxPairErrorForRelative", kStateLeastSquaresSection,
        "maxPairErrorForRelative", options.maxPairErrorForRelative, 0.0, 1000.0);
    return options;
}

void AppSettings::setLeastSquaresOptions(const LeastSquaresDefaultSettings& input)
{
    LeastSquaresDefaultSettings options = input;
    options.regressionThreshold = std::clamp(options.regressionThreshold, 0.0, 1.0);
    options.relativeThreshold = std::clamp(options.relativeThreshold, 0.0, 100.0);
    options.absoluteThreshold = std::clamp(options.absoluteThreshold, 0.0, 1000.0);
    options.maxPairErrorForRelative =
        std::clamp(options.maxPairErrorForRelative, 0.0, 1000.0);
    auto setDouble = [](const char* userKey, const char* stateKey, double value) {
        setPersistedValue(userKey, kStateLeastSquaresSection, stateKey, value,
                          QString::number(value, 'g', 16));
    };
    setDouble("leastSquares/regressionThreshold", "regressionThreshold",
              options.regressionThreshold);
    setDouble("leastSquares/relativeThreshold", "relativeThreshold",
              options.relativeThreshold);
    setDouble("leastSquares/absoluteThreshold", "absoluteThreshold",
              options.absoluteThreshold);
    setDouble("leastSquares/maxPairErrorForRelative", "maxPairErrorForRelative",
              options.maxPairErrorForRelative);
}

void AppSettings::resetLeastSquaresOptions()
{
    resetPersistedValue("leastSquares/regressionThreshold",
                        kStateLeastSquaresSection, "regressionThreshold");
    resetPersistedValue("leastSquares/relativeThreshold",
                        kStateLeastSquaresSection, "relativeThreshold");
    resetPersistedValue("leastSquares/absoluteThreshold",
                        kStateLeastSquaresSection, "absoluteThreshold");
    resetPersistedValue("leastSquares/maxPairErrorForRelative",
                        kStateLeastSquaresSection, "maxPairErrorForRelative");
}

ImageMergeDefaultSettings AppSettings::imageMergeOptions()
{
    ImageMergeDefaultSettings options = defaults().imageMerge;
    options.mode = boundedPersistedInt(
        "imageMerge/mode", kStateImageMergeSection, "mode",
        options.mode, 0, 2);
    return options;
}

void AppSettings::setImageMergeOptions(const ImageMergeDefaultSettings& options)
{
    const int mode = std::clamp(options.mode, 0, 2);
    setPersistedValue("imageMerge/mode", kStateImageMergeSection, "mode",
                      mode, QString::number(mode));
}

void AppSettings::resetImageMergeOptions()
{
    resetPersistedValue("imageMerge/mode", kStateImageMergeSection, "mode");
}

QSize AppSettings::windowSize(const QString& windowKey, const QSize& fallback)
{
    if (windowKey.trimmed().isEmpty()) {
        return fallback;
    }
    const QString userKey = QStringLiteral("windows/") + windowKey;
    QSettings userSettings;
    QVariant raw;
    if (userSettings.contains(userKey)) {
        raw = userSettings.value(userKey);
    } else {
        QSettings fileSettings(defaultsFilePath(), QSettings::IniFormat);
        raw = fileSettings.value(QString::fromLatin1(kStateWindowsSection)
                                 + QLatin1Char('/') + windowKey);
    }

    QSize size = raw.toSize();
    if (!size.isValid()) {
        const QStringList parts = raw.toString().split(QLatin1Char('x'));
        if (parts.size() == 2) {
            bool widthOk = false;
            bool heightOk = false;
            const int width = parts[0].toInt(&widthOk);
            const int height = parts[1].toInt(&heightOk);
            if (widthOk && heightOk) {
                size = QSize(width, height);
            }
        }
    }
    return size.width() >= 200 && size.height() >= 100 ? size : fallback;
}

void AppSettings::setWindowSize(const QString& windowKey, const QSize& size)
{
    if (windowKey.trimmed().isEmpty()
        || size.width() < 200 || size.height() < 100) {
        return;
    }
    const QString userKey = QStringLiteral("windows/") + windowKey;
    QSettings userSettings;
    userSettings.setValue(userKey, size);
    userSettings.sync();
    persistIniValue(defaultsFilePath(), QString::fromLatin1(kStateWindowsSection),
                    windowKey,
                    QStringLiteral("%1x%2").arg(size.width()).arg(size.height()));
}

int AppSettings::controlPanelWidth(int minimumWidth)
{
    minimumWidth = std::clamp(minimumWidth, 1, 10000);
    return boundedPersistedInt(
        "windows/controlPanelWidth", kStateWindowsSection,
        "controlPanelWidth", minimumWidth, minimumWidth, 10000);
}

void AppSettings::setControlPanelWidth(int width)
{
    width = std::clamp(width, 1, 10000);
    setPersistedValue("windows/controlPanelWidth", kStateWindowsSection,
                      "controlPanelWidth", width, QString::number(width));
}

void AppSettings::resetWindowSizes()
{
    QSettings userSettings;
    userSettings.beginGroup(QStringLiteral("windows"));
    userSettings.remove(QString());
    userSettings.endGroup();
    userSettings.sync();
    removeIniSection(defaultsFilePath(), QString::fromLatin1(kStateWindowsSection));
}

void AppSettings::resetApplicationDialogSettings()
{
    resetPersistedValue(kThemeKey, kStateApplicationSection, "theme");
    resetPersistedValue(kLanguageKey, kStateApplicationSection, "language");
    resetPersistedValue(kVulkanEnabledKey, kStateVulkanSection, "enabled");
    resetPersistedValue(kVulkanIgnoreLimitKey, kStateVulkanSection,
                        "ignoreVramLimit");
    resetPersistedValue(kVulkanDeviceKey, kStateVulkanSection, "deviceKey");
}

void AppSettings::resetCanvasSettings()
{
    resetPersistedValue(kCanvasBackgroundKey, kStateCanvasSection, "background");
}

void AppSettings::resetAllUserSettings()
{
    resetApplicationDialogSettings();
    resetCanvasSettings();
    resetAlignmentOptions();
    resetArrangementOptions();
    resetTrwsPamiOptions();
    resetLeastSquaresOptions();
    resetImageMergeOptions();
    resetWindowSizes();
}

void applyApplicationTheme(ApplicationTheme theme)
{
    if (!qApp) {
        return;
    }

    qApp->setStyleSheet({});
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // 以前の手動パレットを解除してから、QtにOSネイティブの配色を要求する。
    // Systemではオーバーライドを外すため、OS側の変更にも追従する。
    qApp->setPalette(QPalette());
    Qt::ColorScheme scheme = Qt::ColorScheme::Unknown;
    if (theme == ApplicationTheme::Light) {
        scheme = Qt::ColorScheme::Light;
    } else if (theme == ApplicationTheme::Dark) {
        scheme = Qt::ColorScheme::Dark;
    }
    QGuiApplication::styleHints()->setColorScheme(scheme);
    qApp->setPalette(QPalette());
#else
    // Qt 6.7以前では起動時のシステム配色を保持し、明示指定だけを補完する。
    switch (theme) {
    case ApplicationTheme::Light:
        qApp->setPalette(fallbackLightPalette());
        break;
    case ApplicationTheme::Dark:
        qApp->setPalette(fallbackDarkPalette());
        break;
    default:
        qApp->setPalette(originalSystemPalette());
        break;
    }
#endif
}

bool applyApplicationLanguage(ApplicationLanguage language)
{
    if (!qApp) {
        return false;
    }

    QTranslator& translator = applicationTranslator();
    bool& translatorInstalled = applicationTranslatorInstalled();
    if (translatorInstalled) {
        qApp->removeTranslator(&translator);
        translatorInstalled = false;
    }

    ApplicationLanguage resolvedLanguage = language;
    if (resolvedLanguage == ApplicationLanguage::System) {
        resolvedLanguage = QLocale::system().language() == QLocale::Japanese
                               ? ApplicationLanguage::Japanese
                               : ApplicationLanguage::English;
    }

    // Japanese is the source language, so no translator is needed.
    if (resolvedLanguage == ApplicationLanguage::Japanese) {
        return true;
    }

    const QString fileName = QString::fromLatin1(kEnglishTranslationFileName);
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QStringLiteral(":/translations/") + fileName,
        QDir(applicationDirectory).filePath(QStringLiteral("translations/") + fileName),
        QDir(applicationDirectory).filePath(fileName),
        QDir::current().filePath(QStringLiteral("translations/") + fileName),
        QDir::current().filePath(fileName)
    };
    bool loaded = false;
    for (const QString& candidate : candidates) {
        if (translator.load(candidate)) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        return false;
    }

    translatorInstalled = qApp->installTranslator(&translator);
    return translatorInstalled;
}
