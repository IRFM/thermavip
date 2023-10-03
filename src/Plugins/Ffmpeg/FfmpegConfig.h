#ifndef FFMPEG_CONFIG_H
#define FFMPEG_CONFIG_H

#include <QtGlobal>

// FFMPEG plugin can also be used as a library to link with, therefore most classes are exported

#ifdef BUILD_FFMPEG_LIB
#define FFMPEG_EXPORT Q_DECL_EXPORT
#else
#define FFMPEG_EXPORT Q_DECL_IMPORT
#endif

#endif
