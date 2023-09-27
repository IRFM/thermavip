#ifndef PY_PLUGIN_CONFIG_H
#define PY_PLUGIN_CONFIG_H

#include <QtGlobal>

#ifdef BUILD_PYTHON_LIB
	#define PYTHON_EXPORT Q_DECL_EXPORT
#else
	#define PYTHON_EXPORT Q_DECL_IMPORT
#endif

#endif
