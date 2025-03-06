/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VIP_ENVIRONMENT_H
#define VIP_ENVIRONMENT_H

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
