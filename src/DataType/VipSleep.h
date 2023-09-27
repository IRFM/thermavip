#ifndef VIP_SLEEP_H
#define VIP_SLEEP_H

#include "VipConfig.h"

/// @brief Sleep for given amount of milliseconds
/// 
/// vipSleep will use OS features to provide the most possibly precise sleep duration.
/// 
/// @param ms sleep duration in milliseconds
VIP_DATA_TYPE_EXPORT void vipSleep(double ms);


#endif
