#include "app_settings.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyle>

namespace
{
constexpr auto kThemeKey = "general/theme";
constexpr auto kVulkanEnabledKey = "vulkan/enabled";
constexpr auto kVulkanIgnoreLimitKey = "vulkan/ignoreVramLimit";
constexpr auto kVulkanDeviceKey = "vulkan/deviceKey";
}

ApplicationTheme AppSettings::theme()
{
    QSettings settings;
    const int value = settings.value(kThemeKey, static_cast<int>(ApplicationTheme::System)).toInt();
    switch (value) {
    case static_cast<int>(ApplicationTheme::Light):
        return ApplicationTheme::Light;
    case static_cast<int>(ApplicationTheme::Dark):
        return ApplicationTheme::Dark;
    default:
        return ApplicationTheme::System;
    }
}

void AppSettings::setTheme(ApplicationTheme theme)
{
    QSettings settings;
    settings.setValue(kThemeKey, static_cast<int>(theme));
}

VulkanExecutionOptions AppSettings::vulkanOptions()
{
    QSettings settings;
    VulkanExecutionOptions options;
    options.enabled = settings.value(kVulkanEnabledKey, true).toBool();
    options.ignoreVramLimit = settings.value(kVulkanIgnoreLimitKey, false).toBool();
    options.deviceKey = settings.value(kVulkanDeviceKey).toString();
    return options;
}

void AppSettings::setVulkanEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kVulkanEnabledKey, enabled);
}

void AppSettings::setIgnoreVramLimit(bool ignore)
{
    QSettings settings;
    settings.setValue(kVulkanIgnoreLimitKey, ignore);
}

void AppSettings::setVulkanDeviceKey(const QString& key)
{
    QSettings settings;
    settings.setValue(kVulkanDeviceKey, key);
}

void applyApplicationTheme(ApplicationTheme theme)
{
    if (!qApp) {
        return;
    }

    qApp->setStyleSheet({});
    if (theme == ApplicationTheme::System) {
        qApp->setPalette(qApp->style()->standardPalette());
        return;
    }

    QPalette palette;
    if (theme == ApplicationTheme::Light) {
        palette.setColor(QPalette::Window, QColor(245, 245, 245));
        palette.setColor(QPalette::WindowText, QColor(24, 24, 24));
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::AlternateBase, QColor(238, 238, 238));
        palette.setColor(QPalette::ToolTipBase, Qt::white);
        palette.setColor(QPalette::ToolTipText, QColor(24, 24, 24));
        palette.setColor(QPalette::Text, QColor(24, 24, 24));
        palette.setColor(QPalette::Button, QColor(245, 245, 245));
        palette.setColor(QPalette::ButtonText, QColor(24, 24, 24));
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(112, 112, 112));
    } else {
        palette.setColor(QPalette::Window, QColor(45, 45, 48));
        palette.setColor(QPalette::WindowText, QColor(232, 232, 232));
        palette.setColor(QPalette::Base, QColor(30, 30, 30));
        palette.setColor(QPalette::AlternateBase, QColor(53, 53, 56));
        palette.setColor(QPalette::ToolTipBase, QColor(35, 35, 38));
        palette.setColor(QPalette::ToolTipText, QColor(232, 232, 232));
        palette.setColor(QPalette::Text, QColor(232, 232, 232));
        palette.setColor(QPalette::Button, QColor(53, 53, 56));
        palette.setColor(QPalette::ButtonText, QColor(232, 232, 232));
        palette.setColor(QPalette::BrightText, QColor(255, 96, 96));
        palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(160, 160, 160));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(128, 128, 128));
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(128, 128, 128));
    }
    qApp->setPalette(palette);
}
