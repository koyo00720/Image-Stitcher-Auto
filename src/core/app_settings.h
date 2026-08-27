#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QString>

enum class ApplicationTheme {
    System = 0,
    Light = 1,
    Dark = 2
};

struct VulkanExecutionOptions {
    bool enabled = true;
    bool ignoreVramLimit = false;
    QString deviceKey;
};

class AppSettings
{
public:
    static ApplicationTheme theme();
    static void setTheme(ApplicationTheme theme);

    static VulkanExecutionOptions vulkanOptions();
    static void setVulkanEnabled(bool enabled);
    static void setIgnoreVramLimit(bool ignore);
    static void setVulkanDeviceKey(const QString& key);
};

void applyApplicationTheme(ApplicationTheme theme);

#endif // APP_SETTINGS_H
