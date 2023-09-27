#pragma once

#include <qregion.h>
#include "VipConfig.h"

class QPainterPath;

/// Extract the pixels covered by given path.
/// This function uses the equivalent to QRegion(const QBitmap &), but working directly on a QImage instead.
/// This avoid a potential costly conversion to QImage (that uses QRegion anyway) and allow to call this function in a multithreaded
/// context (QBitmap inherits QPixmap, which does not support drawing in a non GUI thread as opposed to QImage).
VIP_DATA_TYPE_EXPORT QRegion vipExtractRegion(const QPainterPath & p);
