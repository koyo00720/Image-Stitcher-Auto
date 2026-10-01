#include "platform_setup.h"

#include <algorithm>

namespace {
constexpr int kLinuxControlPanelMinimumWidth = 360;
}

namespace image_stitcher::platform {

void configureEnvironment()
{
    // No Linux-specific process configuration is currently required.
}

int controlPanelMinimumWidth(int uiMinimumWidth)
{
    return std::max(uiMinimumWidth, kLinuxControlPanelMinimumWidth);
}

} // namespace image_stitcher::platform
