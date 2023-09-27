#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "VipConfig.h"
#include <QString>

/// \addtogroup Core
/// @{

/// \brief Returns the data directory path.
/// The data directory is used to store persistent informations per user.
VIP_CORE_EXPORT QString vipGetDataDirectory(const QString& suffix = "thermavip");

/// \brief Returns the temporary directory path.
/// The temporary directory is used to store data that should not remains when exiting from the executable.
VIP_CORE_EXPORT QString vipGetTempDirectory(const QString& suffix = "thermavip");

/// \brief Returns the log directory.
/// It is located inside the data directory and stores all log files for the SDK and the plugins.
VIP_CORE_EXPORT QString vipGetLogDirectory(const QString& suffix = "thermavip");

/// \brief Returns the plugin log directory.
/// It is located inside the data directory and stores all log files for the plugins.
VIP_CORE_EXPORT QString vipGetLogPluginsDirectory(const QString& suffix = "thermavip");

/// \brief Returns the global perspective directory.
/// It is located next to the executable and stores all global perspectives.
VIP_CORE_EXPORT QString vipGetPerspectiveDirectory(const QString& suffix = "thermavip");

/// \brief Returns the user perspective log directory.
/// It is located inside the data directory and stores all user defined perspectives.
VIP_CORE_EXPORT QString vipGetUserPerspectiveDirectory(const QString& suffix = "thermavip");

/// \brief Returns the global device directory.
/// It is located next to the executable and stores all global devices.
/// In this case, a device is a video file format designed with the PINUP tool
/// for wrapping new file format.
VIP_CORE_EXPORT QString vipGetRawDeviceDirectory(const QString& suffix = "thermavip");

/// \brief Returns the user device directory.
/// It is located inside the data directory and stores all user defined devices.
/// In this case, a device is a video file format designed with the PINUP tool
/// for wrapping new file format.
VIP_CORE_EXPORT QString vipGetUserRawDeviceDirectory(const QString& suffix = "thermavip");

/// \brief Returns the plugins directory.
/// It is located next to the executable and stores the plugins (dynamic libraries and possible configuration files).
VIP_CORE_EXPORT QString vipGetPluginsDirectory();

/// @}
// end Core

#endif
