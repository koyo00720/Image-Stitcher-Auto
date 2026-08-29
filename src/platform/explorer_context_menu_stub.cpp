#include "explorer_context_menu.h"

#include <QCoreApplication>

namespace image_stitcher::platform {

bool explorerContextMenuSupported()
{
    return false;
}

ExplorerContextMenuResult setExplorerContextMenuEnabled(bool enabled)
{
    if (!enabled) {
        return {true, {}};
    }
    return {
        false,
        QCoreApplication::translate(
            "ExplorerContextMenu",
            "この機能はWindows 11以降でのみ利用できます。")
    };
}

} // namespace image_stitcher::platform
