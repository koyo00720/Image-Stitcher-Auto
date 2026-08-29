#pragma once

#include <QString>

namespace image_stitcher::platform {

struct ExplorerContextMenuResult
{
    bool success = false;
    QString errorMessage;
};

// The Windows 11 modern context menu requires a packaged IExplorerCommand.
bool explorerContextMenuSupported();
ExplorerContextMenuResult setExplorerContextMenuEnabled(bool enabled);

} // namespace image_stitcher::platform
