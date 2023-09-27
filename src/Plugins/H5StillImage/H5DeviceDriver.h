#ifndef H5_DEVICE_DRIVER_H
#define H5_DEVICE_DRIVER_H

#include <qiodevice.h>
#include "H5StillImageConfig.h"


H5_STILL_IMAGE_EXPORT qint64 H5OpenQIODevice(QIODevice * device);

#endif