#ifndef FFMPEG_CONFIG_H
#define FFMPEG_CONFIG_H

#include <QtGlobal>

#ifdef BUILD_FFMPEG_LIB
#define FFMPEG_EXPORT Q_DECL_EXPORT
#else
#define FFMPEG_EXPORT Q_DECL_IMPORT
#endif

#endif
