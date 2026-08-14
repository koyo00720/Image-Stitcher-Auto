#include "platform_setup.h"

#include <QByteArray>
#include <QtGlobal>

namespace image_stitcher::platform {

void configureEnvironment()
{
    // Large stitched images can exceed Qt's default image allocation limit.
    qputenv("QT_IMAGEIO_MAXALLOC", QByteArray("0"));
}

} // namespace image_stitcher::platform
