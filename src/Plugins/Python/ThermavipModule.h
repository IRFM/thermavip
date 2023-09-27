#pragma once
#undef slots
#undef _DEBUG
extern "C" {

#include "Python.h"
#include "bytesobject.h"
#include "unicodeobject.h"
#include <frameobject.h>
#include <traceback.h>
}

PyObject* PyInit_thermavip();