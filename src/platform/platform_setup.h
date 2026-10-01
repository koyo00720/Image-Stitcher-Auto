#pragma once

namespace image_stitcher::platform {

// Applies process-wide settings that must be configured before QApplication.
void configureEnvironment();

// Applies platform-specific constraints while preserving the UI-defined value.
int controlPanelMinimumWidth(int uiMinimumWidth);

} // namespace image_stitcher::platform
