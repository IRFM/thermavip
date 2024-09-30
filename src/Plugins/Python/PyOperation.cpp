#include <iostream>
#include <cmath>
#include <set>
#include <deque>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

// undef _DEBUG or pyconfig.h will automatically try to link to python34_d.lib
#undef _DEBUG
extern "C" {

#include "Python.h"
#include "frameobject.h"
#include "numpy/arrayobject.h"
#include <frameobject.h>
#include <traceback.h>

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#include <dlfcn.h>
#endif
}

#include <iostream>

#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>
#include <QThread>
#include <QProcess>
#include <QReadWriteLock>
#include <QLibrary>

#include "VipDataType.h"
#include "VipLogging.h"
#include "PyOperation.h"
#include "VipEnvironment.h"
#include "VipProcessingObject.h"
#include "VipCore.h"
#include "VipSleep.h"
#include "ThermavipModule.h"

#define PY_BUILD_VERSION(major, minor, micro) ((major << 24) | (minor << 16) | (micro << 8) | (0 << 4) | (0 << 0))

quint64 vipPyThreadId()
{
	return (quint64)QThread::currentThreadId();
}

#if PY_MINOR_VERSION < 4
/// @brief Reinplementation of PyGILState_Check for older Python version
static inline int PyGILState_Check(void)
{
	PyThreadState* tstate = (PyThreadState*)_PyThreadState_Current._value;
	return tstate && (tstate == PyGILState_GetThisThreadState());
}
#endif

#include "PyProcessing.h"

QString vipGetPythonDirectory(const QString& suffix)
{
	QString path = vipGetDataDirectory(suffix) + "Python/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetPythonScriptsDirectory(const QString& suffix)
{
	QString path = vipGetPythonDirectory(suffix) + "Scripts/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetPythonScriptsPlayerDirectory(const QString& suffix)
{
	QString path = vipGetPythonScriptsDirectory(suffix) + "Player/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

static QStringList listPyDir(const QString& dir)
{
	QDir d(dir);
	QFileInfoList lst = d.entryInfoList(QStringList(), QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs, QDir::Name);
	QStringList res;
	for (int i = 0; i < lst.size(); ++i) {
		if (lst[i].isDir())
			res += listPyDir(lst[i].canonicalFilePath());
		else if (lst[i].suffix() == "py")
			res += lst[i].canonicalFilePath();
	}
	return res;
}

QStringList vipGetPythonPlayerScripts(const QString& suffix)
{
	QString root = vipGetPythonScriptsPlayerDirectory(suffix);
	root = root.mid(0, root.size() - 1);
	QStringList r = listPyDir(root);
	for (int i = 0; i < r.size(); ++i) {
		r[i].replace("\\", "/");
		r[i].remove(root + "/");
	}
	return r;
}

// tells if PyFinalize() has been called
static bool __python_closed = false;

// Numpy to Qt metatype id
int vipNumpyToQt(int type)
{
	switch (type) {
		case NPY_BOOL:
			return QMetaType::Bool;
		case NPY_BYTE:
			return QMetaType::Char;
		case NPY_UBYTE:
			return QMetaType::UChar;
		case NPY_SHORT:
			return QMetaType::Short;
		case NPY_USHORT:
			return QMetaType::UShort;
		case NPY_INT:
			return QMetaType::Int;
		case NPY_UINT:
			return QMetaType::UInt;
		case NPY_LONG:
			return QMetaType::Long;
		case NPY_ULONG:
			return QMetaType::ULong;
		case NPY_LONGLONG:
			return QMetaType::LongLong;
		case NPY_ULONGLONG:
			return QMetaType::ULongLong;
		case NPY_FLOAT:
			return QMetaType::Float;
		case NPY_DOUBLE:
			return QMetaType::Double;
		case NPY_CFLOAT:
			return qMetaTypeId<complex_f>();
		case NPY_CDOUBLE:
			return qMetaTypeId<complex_d>();
		case NPY_LONGDOUBLE:
			return qMetaTypeId<vip_long_double>();
	}
	return 0;
}

// Qt to numpy metatype id
int vipQtToNumpy(int type)
{
	switch (type) {
		case QMetaType::Bool:
			return NPY_BOOL;
		case QMetaType::Char:
			return NPY_BYTE;
		case QMetaType::UChar:
			return NPY_UBYTE;
		case QMetaType::Short:
			return NPY_SHORT;
		case QMetaType::UShort:
			return NPY_USHORT;
		case QMetaType::Int:
			return NPY_INT;
		case QMetaType::UInt:
			return NPY_UINT;
		case QMetaType::Long:
			return NPY_LONG;
		case QMetaType::ULong:
			return NPY_ULONG;
		case QMetaType::LongLong:
			return NPY_LONGLONG;
		case QMetaType::ULongLong:
			return NPY_ULONGLONG;
		case QMetaType::Float:
			return NPY_FLOAT;
		case QMetaType::Double:
			return NPY_DOUBLE;
	}

	if (type == qMetaTypeId<complex_f>())
		return NPY_CFLOAT;
	else if (type == qMetaTypeId<complex_d>())
		return NPY_CDOUBLE;
	else if (type == qMetaTypeId<vip_long_double>())
		return NPY_LONGDOUBLE;

	return -1;
}

// Vector of Python to QVariant converters
static QVector<python_to_variant>& getToVariant()
{
	static QVector<python_to_variant> instance;
	return instance;
}

// Vector of QVariant to Python converters
static QVector<variant_to_python>& getToPython()
{
	static QVector<variant_to_python> instance;
	return instance;
}

void vipRegisterToVariantConverter(python_to_variant fun)
{
	getToVariant().push_back(fun);
}

void vipRegisterToPythonConverter(variant_to_python fun)
{
	getToPython().push_back(fun);
}

QVariant vipPythonToVariant(void* pyobject)
{
	if (!pyobject)
		return QVariant();
	else if (pyobject == Py_None)
		return QVariant();

	for (int i = 0; i < getToVariant().size(); ++i) {
		QVariant tmp = getToVariant()[i](pyobject);
		if (!tmp.isNull())
			return tmp;
	}
	return QVariant();
}

void* vipVariantToPython(const QVariant& variant)
{
	if (variant.userType() == 0) {
		Py_RETURN_NONE;
	}

	for (int i = 0; i < getToPython().size(); ++i) {
		void* tmp = getToPython()[i](variant);
		if (tmp)
			return tmp;
	}

	Py_RETURN_NONE;
}

static QVariant stdToVariant(void* pyobject)
{
	QVariant res;
	PyObject* res_object = (PyObject*)pyobject;

	if (PyByteArray_Check(res_object)) {
		int size = PyByteArray_Size(res_object);
		char* data = PyByteArray_AsString(res_object);
		res = QVariant(QByteArray(data, size));
	}
	else if (PyBytes_Check(res_object)) {
		int size = PyBytes_Size(res_object);
		char* data = PyBytes_AsString(res_object);
		res = QVariant(QByteArray(data, size));
	}
	else if (PyUnicode_Check(res_object)) {
		Py_ssize_t size = 0;
		wchar_t* data = PyUnicode_AsWideCharString(res_object, &size);
		res = QVariant(QString::fromWCharArray(data, size));
		PyMem_Free(data);
	}
	else if (PyLong_Check(res_object)) {
		res = QVariant(PyLong_AsLongLong(res_object));
	}
	else if (PyBool_Check(res_object)) {
		res = QVariant(res_object == Py_True);
	}
	else if (PyFloat_Check(res_object)) {
		res = QVariant(PyFloat_AsDouble(res_object));
	}
	else if (PyArray_Check(res_object)) {
		// Py_INCREF(res_object);
		// VipNDArray ar = PyNDArray(res_object).copy();
		VipNDArray ar = vipFromNumpyArray(res_object);
		if (ar.shapeCount() == 3 && ar.shape(2) == 3) {
			// RGB image
			QImage img(ar.shape(1), ar.shape(0), QImage::Format_ARGB32);
			uint* bits = (uint*)img.bits();

			if (ar.dataType() != QMetaType::UChar) {
				VipNDArrayType<double> ard = ar.toDouble();
				vipEval(ard, vipClamp(ard, 0., 255.));
				ar = vipCast<uchar>(ard);
			}

			const uchar* pix = (const uchar*)ar.constData();
			int size = img.width() * img.height();
			for (int i = 0; i < size; ++i, pix += 3) {
				bits[i] = qRgb(pix[0], pix[1], pix[2]);
			}
			ar = vipToArray(img);
		}
		res = QVariant::fromValue(ar);
	}
	else if (PyComplex_Check(res_object)) {
		Py_complex comp = PyComplex_AsCComplex(res_object);
		res = QVariant::fromValue(complex_d(comp.real, comp.imag));
	}
	else if (PyList_Check(res_object)) {
		int len = PyList_GET_SIZE(res_object);
		QVariantList tmp;
		for (int i = 0; i < len; ++i) {
			PyObject* obj = PyList_GetItem(res_object, i);
			tmp.append(vipPythonToVariant(obj));
		}
		res = QVariant::fromValue(tmp);
	}
	else if (PyTuple_Check(res_object)) {
		int len = PyTuple_GET_SIZE(res_object);
		QVariantList tmp;
		for (int i = 0; i < len; ++i) {
			PyObject* obj = PyTuple_GetItem(res_object, i);
			tmp.append(vipPythonToVariant(obj));
		}
		res = QVariant::fromValue(tmp);
	}
	else if (PyDict_Check(res_object)) {
		PyObject *key, *value;
		Py_ssize_t pos = 0;
		QVariantMap tmp;

		while (PyDict_Next(res_object, &pos, &key, &value)) {
			/* do something interesting with the values... */
			if (key && value) {
				QString k = vipPythonToVariant(key).toString();
				QVariant v = vipPythonToVariant(value);
				tmp[k] = v;
			}
		}
		res = QVariant::fromValue(tmp);
	}
	else if (PyArray_IsIntegerScalar(res_object)) // strcmp(res_object->ob_type->tp_name,"numpy.int64")==0)
	{
		long long value = PyLong_AsLongLong(res_object);
		res = QVariant(value);
	}
	else if (PyArray_IsAnyScalar(res_object)) // strcmp(res_object->ob_type->tp_name,"numpy.int64")==0)
	{
		vip_double value = PyFloat_AsDouble(res_object);
		res = QVariant::fromValue(value);
	}
	else {
		PyObject* new_object = PyArray_FromAny(res_object, nullptr, 0, 0, NPY_ARRAY_ENSUREARRAY | NPY_ARRAY_C_CONTIGUOUS, nullptr);
		if (new_object) {
			// res = QVariant::fromValue(PyNDArray(new_object).copy());
			res = QVariant::fromValue(vipFromNumpyArray(new_object));
			Py_DECREF(new_object);
		}
	}

	return res;
}

static void* stdToPython(const QVariant& obj)
{
	PyObject* obj_object = nullptr;
	switch (obj.userType()) {
		case QMetaType::Bool:
			obj_object = PyBool_FromLong(obj.toBool());
			break;
		case QMetaType::Char:
		case QMetaType::SChar:
		case QMetaType::UChar:
		case QMetaType::Short:
		case QMetaType::UShort:
		case QMetaType::Int:
		case QMetaType::UInt:
		case QMetaType::Long:
		case QMetaType::ULong:
		case QMetaType::LongLong:
		case QMetaType::ULongLong:
			obj_object = PyLong_FromLongLong(obj.toLongLong());
			break;
		case QMetaType::Float:
		case QMetaType::Double:
			obj_object = PyFloat_FromDouble(obj.toDouble());
			break;
		case QMetaType::QString: {
			const QString in = obj.toString();
			std::vector<wchar_t> str((size_t)in.size(), 0);
			in.toWCharArray(str.data());
			obj_object = PyUnicode_FromWideChar(str.data(), str.size());
			break;
		}
		case QMetaType::QByteArray: {
			QByteArray ar = obj.toByteArray();
			obj_object = PyByteArray_FromStringAndSize(ar.data(), ar.size());
		}
	}

	// other types (complex and array)
	if (!obj_object) {
		if (obj.userType() == qMetaTypeId<vip_long_double>()) {
			obj_object = PyFloat_FromDouble(obj.value<vip_long_double>());
		}
		else if (obj.userType() == qMetaTypeId<complex_d>()) {
			complex_d comp = obj.value<complex_d>();
			obj_object = PyComplex_FromDoubles(comp.real(), comp.imag());
		}
		else if (obj.userType() == qMetaTypeId<complex_f>()) {
			complex_f comp = obj.value<complex_f>();
			obj_object = PyComplex_FromDoubles(comp.real(), comp.imag());
		}
		else if (obj.userType() == qMetaTypeId<QVariantList>()) {
			QVariantList lst = obj.value<QVariantList>();
			obj_object = PyList_New(lst.size());
			for (int i = 0; i < lst.size(); ++i) {
				PyObject* item = (PyObject*)vipVariantToPython(lst[i]);
				if (item)
					PyList_SetItem(obj_object, i, item);
			}
		}
		else if (obj.userType() == qMetaTypeId<QStringList>()) {
			QStringList lst = obj.value<QStringList>();
			obj_object = PyList_New(lst.size());
			for (int i = 0; i < lst.size(); ++i) {
				PyObject* item = (PyObject*)vipVariantToPython(lst[i]);
				if (item)
					PyList_SetItem(obj_object, i, item);
			}
		}
		else if (obj.userType() == qMetaTypeId<QVariantMap>()) {
			QVariantMap map = obj.value<QVariantMap>();
			obj_object = PyDict_New();
			for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it) {
				PyObject* item = (PyObject*)vipVariantToPython(it.value());
				if (item) {
					std::vector<wchar_t> str((size_t)it.key().size(), 0);
					it.key().toWCharArray(str.data());
					PyDict_SetItem(obj_object, PyUnicode_FromWideChar(str.data(), str.size()), item);
				}
			}
		}
		else if (obj.userType() == qMetaTypeId<VipPointVector>()) {
			VipPointVector vec = obj.value<VipPointVector>();
			// array of 2 rows (x and y)
			VipNDArrayType<vip_double> ar(vipVector(2, vec.size()));
			for (int i = 0; i < vec.size(); ++i) {
				ar(vipVector(0, i)) = vec[i].x();
				ar(vipVector(1, i)) = vec[i].y();
			}
			obj_object = (PyObject*)vipVariantToPython(QVariant::fromValue(VipNDArray(ar)));
		}
		else if (obj.userType() == qMetaTypeId<VipComplexPointVector>()) {
			VipComplexPointVector vec = obj.value<VipComplexPointVector>();
			// array of 2 rows (x and y)
			VipNDArrayType<complex_d> ar(vipVector(2, vec.size()));
			for (int i = 0; i < vec.size(); ++i) {
				ar(vipVector(0, i)) = vec[i].x();
				ar(vipVector(1, i)) = vec[i].y();
			}
			obj_object = (PyObject*)vipVariantToPython(QVariant::fromValue(VipNDArray(ar)));
		}
		else if (obj.userType() == qMetaTypeId<VipIntervalSampleVector>()) {
			VipIntervalSampleVector vec = obj.value<VipIntervalSampleVector>();
			// list of 2 arrays: values and intervals
			VipNDArrayType<double> values(vipVector(vec.size()));
			VipNDArrayType<double> intervals(vipVector(vec.size() * 2));
			for (int i = 0; i < vec.size(); ++i) {
				values(vipVector(i)) = vec[i].value;
				intervals(vipVector(i * 2)) = vec[i].interval.minValue();
				intervals(vipVector(i * 2 + 1)) = vec[i].interval.maxValue();
			}
			QVariantList tmp;
			tmp.append(QVariant::fromValue(VipNDArray(values)));
			tmp.append(QVariant::fromValue(VipNDArray(intervals)));
			obj_object = (PyObject*)vipVariantToPython(tmp);
		}
		else if (obj.userType() == qMetaTypeId<VipNDArray>()) {
			const VipNDArray info = obj.value<VipNDArray>();

			if (vipIsImageArray(info)) {
				// convert QImage to 3 dims array
				const QImage img = vipToImage(info);
				npy_intp shape[20] = { img.height(), img.width(), 3 };
				std::vector<uchar> image(img.height() * img.width() * 3);
				int size = img.height() * img.width();
				const uint* pixels = (const uint*)img.bits();
				for (int i = 0; i < size; ++i) {
					image[i * 3] = qRed(pixels[i]);
					image[i * 3 + 1] = qGreen(pixels[i]);
					image[i * 3 + 2] = qBlue(pixels[i]);
				}
				PyObject* ref = PyArray_SimpleNewFromData(3, shape, NPY_UBYTE, (void*)image.data());
				obj_object = PyArray_NewCopy((PyArrayObject*)ref, NPY_CORDER);
				Py_DECREF(ref);
			}
			else {
				int numpy_type = vipQtToNumpy(info.dataType());
				if (numpy_type >= 0 && info.data()) {
					npy_intp shape[20];
					std::copy(info.shape().begin(), info.shape().end(), shape);
					int nd = info.shape().size();
					PyObject* ref = PyArray_SimpleNewFromData(nd, shape, numpy_type, (void*)info.constData());
					obj_object = PyArray_NewCopy((PyArrayObject*)ref, NPY_CORDER);
					Py_DECREF(ref);
				}
			}
		}
	}

	return obj_object;
}

// Python traceback module
static PyObject* __traceback = nullptr;

QString vipFromPyString(PyObject* obj)
{
	if (PyObject* s = PyObject_Str(obj)) {
		Py_ssize_t size;
		wchar_t* fname = PyUnicode_AsWideCharString(s, &size);
		QString res = QString::fromWCharArray(fname, size);
		Py_DECREF(s);
		return res;
	}
	return QString();
}

VipPyError::VipPyError(compute_error_t)
{
	if (PyErr_Occurred()) {
		PyObject *ptype, *pvalue, *ptraceback;
		PyErr_Fetch(&ptype, &pvalue, &ptraceback);
		if (!pvalue)
			return;

		if (PyTuple_Check(pvalue) && ptype == PyExc_SyntaxError) {
			// if the error is a SyntaxError, use a different method to retrieve the error info

			char* msg;
			char* file;
			int offset;
			char* text;
			int res = PyArg_ParseTuple(pvalue, "s(siis)", &msg, &file, &line, &offset, &text);
			if (res) {
				// build the filename and traceback

				filename = QString(file);
				traceback = "\tFile '" + filename + ", line " + QString::number(line) + "\n";
				traceback += "\t\t" + QString(text);
				traceback += "\t\t" + QString(offset - 1, QChar(' ')) + "^\n";
				traceback += "SyntaxError: " + QString(msg);
			}
		}
		/**/
		else {
			// build traceback from pvalue
			Py_ssize_t s;
			wchar_t* data = PyUnicode_AsWideCharString(pvalue, &s);
			if (data)
				traceback += QString::fromWCharArray(data, s);

			PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

			// try to build the traceback from "traceback.format_exception" function

			bool failed = true;
			if (/*PyObject * trace = PyImport_ImportModule("traceback")*/ __traceback) {
				if (PyObject* format_exc = PyObject_GetAttrString(__traceback, "format_exception")) {
					if (PyObject* lst = PyObject_CallFunctionObjArgs(format_exc, ptype, pvalue, ptraceback, nullptr)) {
						traceback.clear();
						Py_ssize_t size = PyList_Size(lst);
						for (int i = 0; i < size; ++i) {
							PyObject* str = PyList_GetItem(lst, i);
							Py_ssize_t s;
							wchar_t* data = PyUnicode_AsWideCharString(str, &s);
							if (data)
								traceback += QString::fromWCharArray(data, s);
						}
						failed = size <= 0;
						Py_DECREF(lst);
					}

					Py_DECREF(format_exc);
				}
			}

			/*if (failed) {
			int r = PyRun_SimpleString("import traceback;traceback.print_exc();tmp_error = traceback.format_exc()");
			if (r == 0) {
			PyObject *m = PyImport_AddModule("__main__");
			PyObject *v = PyObject_GetAttrString(m, "tmp_error");
			if (v) {
			traceback = vipPythonToVariant(v).toString();
			Py_DECREF(v);
			failed = false;
			}
			}
			}*/
			if (failed && strcmp(pvalue->ob_type->tp_name, "SystemError") == 0) {
				if (PyObject* o = PyObject_Str(pvalue)) {
					traceback = "SystemError: " + vipPythonToVariant(o).toString();
					Py_DECREF(o);
				}
			}
		}

		// retrieve the other info based on the traceback

		PyTracebackObject* tstate = (PyTracebackObject*)ptraceback;
		if (nullptr != tstate && nullptr != tstate->tb_frame) {
			auto* frame = tstate->tb_frame;
#if PY_VERSION_HEX < PY_BUILD_VERSION(3, 11, 0)
			line = frame->f_lineno;
			Py_ssize_t size;
			wchar_t* fname = PyUnicode_AsWideCharString(frame->f_code->co_filename, &size);
			filename = QString::fromWCharArray(fname, size);
			wchar_t* funcname = PyUnicode_AsWideCharString(frame->f_code->co_name, &size);
			functionName = QString::fromWCharArray(funcname, size);
#else
			line = PyFrame_GetLineNumber(frame);
			PyCodeObject* code = PyFrame_GetCode(frame);
			Py_ssize_t size;
			wchar_t* fname = PyUnicode_AsWideCharString(code->co_filename, &size);
			filename = QString::fromWCharArray(fname, size);
			wchar_t* funcname = PyUnicode_AsWideCharString(code->co_name, &size);
			functionName = QString::fromWCharArray(funcname, size);
			Py_DECREF(code);
#endif
		}

		if (traceback.isEmpty()) {
			traceback = vipFromPyString(pvalue);
			/*QByteArray v1 = pyToString(ptype).toLatin1();
			QByteArray v2 = pyToString(pvalue).toLatin1();
			QByteArray v3 = pyToString(ptraceback).toLatin1();
			vip_debug("'%s' '%s' '%s'\n", v1.data(), v2.data(), v3.data());
			if (PyObject* s = PyObject_Str(ptraceback)) {
				Py_ssize_t size;
				wchar_t* fname = PyUnicode_AsWideCharString(s, &size);
				traceback = QString::fromWCharArray(fname, size);
				Py_DECREF(s);
			}
			else
				traceback = "Unknown error";*/
		}

		PyErr_Restore(ptype, pvalue, ptraceback);
	}
}

/// @briefCreate the error object from a Python tuple.
/// The tuple must contain 5 elements: 'Error', traceback, filename, function name, line number.
/// The GIL must be held.
/* VipPyError::VipPyError(void* tuple)
  : line(0)
{
	// create the error object from a tuple.
	// The tuple must contain 5 elements: 'Error', traceback, filename, function name, line number.
	PyObject* t = (PyObject*)(tuple);

	if (PyTuple_Check(t)) {
		int size = PyTuple_GET_SIZE(t);
		if (size != 5)
			return;

		PyObject* error = PyTuple_GET_ITEM(t, 0);
		if (!PyUnicode_Check(error))
			return;

		Py_ssize_t s;
		wchar_t* data = PyUnicode_AsWideCharString(error, &s);
		if (QString::fromWCharArray(data, s) != "Error")
			return;

		// retrieve all information
		PyObject* _trace = PyTuple_GET_ITEM(t, 1);
		PyObject* _filename = PyTuple_GET_ITEM(t, 2);
		PyObject* _funname = PyTuple_GET_ITEM(t, 3);
		PyObject* _line = PyTuple_GET_ITEM(t, 4);

		data = PyUnicode_AsWideCharString(_trace, &s);
		traceback = QString::fromWCharArray(data, s);

		data = PyUnicode_AsWideCharString(_filename, &s);
		filename = QString::fromWCharArray(data, s);

		data = PyUnicode_AsWideCharString(_funname, &s);
		functionName = QString::fromWCharArray(data, s);

		line = PyLong_AsLong(_line);
	}
}*/

bool VipPyError::isNull() const noexcept
{
	return traceback.isEmpty() && line == 0;
}

void VipPyError::printDebug(std::ostream& oss)
{
	oss << "filename: " << filename.toLatin1().data() << std::endl;
	oss << "functionName: " << functionName.toLatin1().data() << std::endl;
	oss << "line: " << line << std::endl;
	oss << "traceback: \n" << traceback.toLatin1().data() << std::endl;
}

// Associate each VipPyLocal instance to a QThread.
// Used to select the right VipPyLocal instance for I/O operations based on sys.stdin, sys.stderr and sys.stdout
static QMap<quint64, VipPyIOOperation*> PyLocalThreads;
static QMutex PyLocalThreadsMutex;

static void registerPyIOOperation(VipPyIOOperation* local)
{
	QMutexLocker locker(&PyLocalThreadsMutex);
	PyLocalThreads[vipPyThreadId()] = local;
}

static void unregisterPyIOOperation(VipPyIOOperation*)
{
	QMutexLocker locker(&PyLocalThreadsMutex);
	PyLocalThreads.remove(vipPyThreadId());
}

static VipPyIOOperation* getCurrentPyIOOperation()
{
	QMutexLocker locker(&PyLocalThreadsMutex);
	auto it = PyLocalThreads.find(vipPyThreadId());
	if (it != PyLocalThreads.end())
		return it.value();
	else
		return nullptr;
}

struct PythonRedirect
{

	static PyObject* Stdout_write(PyObject* self, PyObject* args)
	{
		Q_UNUSED(self)
		char* data;
		if (!PyArg_ParseTuple(args, "s", &data))
			return 0;

		VipPyIOOperation* loc = getCurrentPyIOOperation();
		int size = 0;
		Py_BEGIN_ALLOW_THREADS size = (int)strlen(data);
		if (loc)
			loc->__addStandardOutput(QByteArray(data, size));
		else {
			if (VipPyInterpreter::instance()->pyIOOperation())
				VipPyInterpreter::instance()->pyIOOperation()->__addStandardOutput(QByteArray(data, size));
		}
		Py_END_ALLOW_THREADS

		  if (loc) loc->__stopCodeIfNeeded();
		return PyLong_FromSize_t(size);
	}

	static PyObject* Stderr_write(PyObject* self, PyObject* args)
	{
		Q_UNUSED(self)
		char* data;
		if (!PyArg_ParseTuple(args, "s", &data))
			return 0;

		VipPyIOOperation* loc = getCurrentPyIOOperation();
		int size = 0;
		Py_BEGIN_ALLOW_THREADS size = (int)strlen(data);
		if (loc)
			loc->__addStandardError(QByteArray(data, size));
		else {
			if (VipPyInterpreter::instance()->pyIOOperation())
				VipPyInterpreter::instance()->pyIOOperation()->__addStandardError(QByteArray(data, size));
		}
		Py_END_ALLOW_THREADS

		  if (loc) loc->__stopCodeIfNeeded();
		return PyLong_FromSize_t(size);
	}

	static PyObject* Stdin_readline(PyObject* self, PyObject* args)
	{
		Q_UNUSED(self)
		Q_UNUSED(args)
		QByteArray data;
		VipPyIOOperation* loc = getCurrentPyIOOperation();
		Py_BEGIN_ALLOW_THREADS if (loc) data = loc->__readinput();
		else
		{
			if (VipPyInterpreter::instance()->pyIOOperation())
				data = VipPyInterpreter::instance()->pyIOOperation()->__readinput();
		}
		Py_END_ALLOW_THREADS

		  if (loc) loc->__stopCodeIfNeeded();
		PyErr_Clear();
		return PyUnicode_FromString(data.data());
	}
};

static PyMethodDef Redirect_methods[] = {
	{ "out_write", PythonRedirect::Stdout_write, METH_VARARGS, "sys.stdout.write" },
	{ "err_write", PythonRedirect::Stderr_write, METH_VARARGS, "sys.stderr.write" },
	{ "in_readline", PythonRedirect::Stdin_readline, METH_VARARGS, "sys.stdin.readline" },
	{ 0, 0, 0, 0 } // sentinel
};

static struct PyModuleDef Redirect_module = { PyModuleDef_HEAD_INIT,
					      "redirect", /* name of module */
					      nullptr,	  /* module documentation, may be nullptr */
					      -1,	  /* size of per-interpreter state of the module,
								  or -1 if the module keeps state in global variables. */
					      Redirect_methods };

// Export the "redirect" module that defines the redirection functions for standard output, error and input
static PyObject* PyInit_redirect()
{
	static QBasicAtomicInt reg = Q_BASIC_ATOMIC_INITIALIZER(0);
	static PyObject* redirect = nullptr;
	if (reg.loadAcquire())
		return redirect;
	else {
		reg.storeRelease(1);
		redirect = PyModule_Create(&Redirect_module);
		return redirect;
	}
}

static bool import_numpy_internal()
{
#ifdef _MSC_VER
	// on Windows, use the standard approach that works most of the time
	if (_import_array() < 0) {
		PyErr_Print();
		return false;
	}
	return true;
#else
	// on Linux, PyCapsule_CheckExact failed for no valid reason.
	// we do the same as _import_array() but without PyCapsule_CheckExact.
	vip_debug("Initialize numpy...\n");
	int numpy = PyRun_SimpleString("import numpy");
	if (numpy == 0)
		vip_debug("numpy module imported\n") else
		{
			vip_debug("Error while importing numpy");
			return false;
		}
	//
	PyObject* _numpy = PyImport_ImportModule("numpy.core.multiarray");
	if (!_numpy) {
		vip_debug("error, nullptr module numpy.core.multiarray\n");
		return false;
	}
	PyObject* c_api = PyObject_GetAttrString(_numpy, "_ARRAY_API");
	Py_DECREF(_numpy);
	if (c_api == nullptr) {
		vip_debug("_ARRAY_API not found\n");
		return false;
	}
	// PyObject* name= PyObject_Bytes(Py_TYPE(c_api));
	// vip_debug("_ARRAY_API type: %s\n",Py_TYPE(c_api)->tp_name);
	/*if (!PyCapsule_CheckExact(c_api)) {
	vip_debug("_ARRAY_API is not PyCapsule object");
	PyErr_SetString(PyExc_RuntimeError, "_ARRAY_API is not PyCapsule object");
	Py_DECREF(c_api);
	return ;
	}*/
	PyArray_API = (void**)PyCapsule_GetPointer(c_api, nullptr);
	vip_debug("numpy properly imported\n");
	return true;
#endif
}

// Handle Python initialization and finalization
class PythonInit
{
public:
	QString local_pip;
	bool pythonInit;
	PythonInit()
	  : pythonInit(false)
	{
		QString python;
		QByteArray python_path;


#ifdef VIP_PYTHON_SHARED_LIBS
		python_path = VIP_PYTHON_STDLIB;
		vip_debug("stdlib: %s\n", python_path.data());

		//Load python libs
		QList<QByteArray> libs = QByteArray(VIP_PYTHON_SHARED_LIBS).split(" ");
		for ( QByteArray& lib : libs) {
			if (lib.isEmpty())
				continue;
			lib.replace("\\", "/");
			QLibrary l(lib);
			if (l.load()) {
				vip_debug("loaded %s\n", lib.data());
			}
		}

#endif

		// QStringList additional_lib_paths;
#if 0 && !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
		/* UNIX-style OS. ------------------------------------------- */
		// try to find a Python3 installation and use it
		{
#ifdef VIP_PYTHONHOME
			// use Python installation defined at compilation
			python_path = VIP_PYTHONHOME;
			vip_debug("Use Python installation at %s\n", python_path.data());
			/*void * tmp =*/dlopen(VIP_PYTHONDYNLIB, RTLD_GLOBAL | RTLD_NOW);
#else
			vip_debug("Try to find Python installation...\n");
			QProcess p;
			p.start("python3 -c \"import os;print(os.path.dirname(os.__file__))\"");
			bool started = p.waitForStarted();
			if (!started) {
				p.start("./python3 -c \"import os;print(os.path.dirname(os.__file__))\"");
				started = p.waitForStarted();
				if (!started) {
					vip_debug("Error with command 'python3', try with 'python'...");
					p.start("python -c 'import os;print(os.path.dirname(os.__file__))'");
					started = p.waitForStarted();
					if (started)
						python = "python";
				}
				else
					python = "./python3";
			}
			else
				python = "python3";

			bool finished = started ? p.waitForFinished() : false;

			// the expected python version
			QString py_version = QString::number(PY_MAJOR_VERSION) + "." + QString::number(PY_MINOR_VERSION);
			QString found_version; // the found one
			if (started && finished) {
				QByteArray path = p.readAllStandardOutput();
				path.replace("'", "");
				path.replace("\n", "");
				vip_debug("Python command output: '%s'\n", path.data());

				if (QDir(path).exists()) {
					python_path = path;
					vip_debug("Found Python in '%s'\n", path.data());
				}

				// retrieve lib paths
				// p.start(python + " -c \"import sys;print('\\n'.join(sys.path))\"");
				// p.waitForStarted();
				// p.waitForFinished();
				// QString out = p.readAllStandardOutput() + p.readAllStandardError();;
				// additional_lib_paths = out.split("\n",QString::SkipEmptyParts);

				// get the version and load the library
				if (path.size() > 3) {
#ifdef VIP_PYTHON_SHARED_LIB
					vip_debug("Load library %s...\n", VIP_PYTHON_SHARED_LIB);
					void* tmp = dlopen(VIP_PYTHON_SHARED_LIB, RTLD_GLOBAL | RTLD_NOW);
#endif
					found_version = path;
					int start = found_version.lastIndexOf("/");
					if (start >= 0)
						found_version = found_version.mid(start + 1);
					found_version.replace("python", "");
					/* found_version = path.mid(path.size() - 3);
					if (found_version == py_version)
					{
						QString libPython = "/usr/local/lib/libpython" + found_version + "m.so.1.0";
						vip_debug("Try %s ...\n", libPython.toLatin1().data());
						void * tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
						if (!tmp)
						{
							libPython = "/usr/local/lib/libpython" + found_version + "m.so";
							vip_debug("Try %s ...\n", libPython.toLatin1().data());
							tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
							if (!tmp)
							{
								libPython = path + "/../../bin/libpython" + found_version + "m.so.1.0";
								vip_debug("Try %s ...\n", libPython.toLatin1().data());
								tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
								if (!tmp)
								{
									libPython = path + "/../../bin/libpython" + found_version + "m.so";
									vip_debug("Try %s ...\n", libPython.toLatin1().data());
									tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
								}
							}

						}
						if (tmp)
							vip_debug("load '%s'\n", libPython.toLatin1().data());
						if (tmp == nullptr) {
							vip_debug("Cannot find python shared library'\n");
						}
					}*/
				}
			}
			if (py_version != found_version) {
				vip_debug("No valid Python installation found (got %s, expected %s), use the local one\n", found_version.toLatin1().data(), py_version.toLatin1().data());
				// no valid python 3 installation found: use the local one
				QFileInfo info(qApp->arguments()[0]);
				QString dirname = info.canonicalPath();
				if (dirname.endsWith("/"))
					dirname = dirname.mid(0, dirname.size() - 1);
				QString libPython = dirname + "/libpython" + py_version + "m.so";
				vip_debug("load '%s'\n", libPython.toLatin1().data());
				void* tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
				if (tmp == nullptr) {
					fprintf(stderr, "%s\n", dlerror());
				}
				QString ver = py_version;
				ver.remove(".");
				python_path = (dirname + "/python" + ver).toLatin1();
			}
#endif
		}

#endif
		vip_debug("python path: %s\n", python_path.data());
		fflush(stdout);
		// init Python
		if (python_path.size()) {
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
			setenv("PYTHONPATH", python_path.data(), 1);
			setenv("PYTHONHOME", python_path.data(), 1);

			/*size_t size = python_path.size();
			wchar_t* home = Py_DecodeLocale(python_path.data(), &size);
			Py_SetPythonHome(home);
			if(home)
			PyMem_RawFree(home);*/
			vip_debug("Set Python path to %s\n", python_path.data());
#endif
		}
		else {
			QByteArray env = getenv("PYTHONHOME");
#ifdef VIP_PYTHONHOME
			if (env.isEmpty()) {
				// VIP_PYTHONHOME specifies an absolute path.
				// We need to translate it (if possible) to a local path from thermavip folder
				QByteArray thermavip_folder = QFileInfo(qApp->arguments()[0]).canonicalPath().toLatin1();
				QByteArray python_folder = thermavip_folder + "/" + QFileInfo(VIP_PYTHONHOME).fileName().toLatin1();
				if (QFileInfo(python_folder).exists())
					env = python_folder;
				else
					env = VIP_PYTHONHOME;
			}
#endif
			vip_debug("python env: %s\n", env.data());
			fflush(stdout);
			if (!env.isEmpty()) {
				python_path = env;
				std::string str = env;
				std::wstring ws(str.begin(), str.end());
				Py_SetPythonHome((wchar_t*)ws.c_str());
			}
			else {
				QString miniconda = QFileInfo(qApp->arguments()[0]).canonicalPath() + "/miniconda";
				if (QFileInfo(miniconda).exists()) {
					python_path = miniconda.toLatin1();
					python_path.replace("\\", "/");
					vip_debug("found miniconda at %s\n", python_path.data());
					local_pip = miniconda + "/Scripts/pip";

					wchar_t home[256];
					memset(home, 0, sizeof(home));
					miniconda.toWCharArray(home);
					Py_SetPythonHome(home);
				}
				else {
					python_path = "./";
					wchar_t home[256] = L"./";
					Py_SetPythonHome(home);
				}
			}
		}

		Py_Initialize();

		// init threading and acquire lock
		PyEval_InitThreads();
		threadState = PyThreadState_Get();
		interpreterState = threadState->interp;
		// init path
		const wchar_t* argv[2] = { L"", L"./" };
		PySys_SetArgv(2, const_cast<wchar_t**>(argv));
		// import numpy
		PyRun_SimpleString("import sys");
		if (python_path.size()) {
			if (python_path == "./") {
				// check for miniconda
				QString miniconda = QFileInfo(qApp->arguments()[0]).canonicalPath() + "/miniconda";
				miniconda.replace("\\", "/");
				if (QFileInfo(miniconda).exists()) {
					local_pip = miniconda + "/Scripts/pip";

					python_path = QFileInfo(qApp->arguments()[0]).canonicalPath().toLatin1() + "/miniconda/Lib";

					// for miniconda, add path to Library/bin to PATH
					QByteArray PATH = qgetenv("PATH");
					PATH += ";" + miniconda + "/Library/bin";
					qputenv("PATH", PATH);
				}
				else
					python_path = QFileInfo(qApp->arguments()[0]).canonicalPath().toLatin1() + "/Lib";
			}
			else {
				// check for miniconda
				if (python_path.endsWith("miniconda")) {

					QString miniconda = python_path;

					// for miniconda, add path to Library/bin to PATH
					QByteArray PATH = qgetenv("PATH");
					PATH += ";" + python_path + "/Library/bin";
					qputenv("PATH", PATH);
				}
			}
			// vip_debug("%s\n", python_path.data());
			// for linux only, we need to add manually the site-packages and lib-dynload folders
			PyRun_SimpleString(("sys.path.append('" + python_path + "/site-packages')").data());
			PyRun_SimpleString(("sys.path.append('" + python_path + "/lib-dynload')").data());
			PyRun_SimpleString(("sys.path.append('" + python_path + "')").data());
			// configure matplot lib thermavip backend
			PyRun_SimpleString(("sys.path.append('" + python_path + "/site-packages/matplotlib/backends')").data());
			// add the Python folder in Thermavip directory
			QByteArray pypath = (QFileInfo(qApp->arguments()[0]).canonicalPath() + "/Python").toLatin1();
			PyRun_SimpleString(("sys.path.append('" + pypath + "')").data());
		}
		else {
			if (QFileInfo(QFileInfo(qApp->arguments()[0]).canonicalPath() + "/miniconda").exists()) {
				local_pip = QFileInfo(qApp->arguments()[0]).canonicalPath() + "/miniconda/Scripts/pip";
				PyRun_SimpleString("sys.path.append('./miniconda/Lib/site-packages/matplotlib/backends')");
				PyRun_SimpleString("sys.path.append('./miniconda/Lib/site-packages')");
				PyRun_SimpleString("sys.path.append('./miniconda/Python')");
			}
			else {
				PyRun_SimpleString("sys.path.append('./Lib/site-packages/matplotlib/backends')");
				PyRun_SimpleString("sys.path.append('./Lib/site-packages')");
				PyRun_SimpleString("sys.path.append('./Python')");
			}
		}

		vip_debug("Initialize numpy with python %s...\n", python_path.data());
		vip_debug("env PATH: %s\n", qgetenv("PATH").data());
		// import and configure numpy
		import_numpy_internal();
		vip_debug("Done\n");

		// import and configure matplotlib
		/*PyRun_SimpleString(
			"import matplotlib; matplotlib.use('module://backend_thermavipagg')"
		);*/

		// register standard from/to python converters
		vipRegisterToPythonConverter(stdToPython);
		vipRegisterToVariantConverter(stdToVariant);

		PyObject* __main__ = PyImport_ImportModule("__main__");
		PyObject* globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);

		PyObject* redirect = PyInit_redirect();
		PyDict_SetItemString(globals, "redirect", redirect);
		// TODO
		PyObject* thermavip = PyInit_thermavip();
		PyDict_SetItemString(globals, "internal", thermavip);
		int i = PyRun_SimpleString("import builtins;builtins.internal = internal");
		i = PyRun_SimpleString(("import sys; sys.path.append('" + QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python')").toLatin1().data());
		i = PyRun_SimpleString("class _Redirect:\n"
				       "    def fileno(self): return 0\n"
				       "    def clear(self): pass\n"
				       "    def flush(self): pass\n"
				       "    def flush(self): return 0\n");

		i = PyRun_SimpleString("import sys");
		i = PyRun_SimpleString("sys.stdout = _Redirect()");
		i = PyRun_SimpleString("sys.stderr = _Redirect()");
		i = PyRun_SimpleString("sys.stdin = _Redirect();sys.stdin.encoding='cp1252';sys.stdin.errors='strict';");

		i = PyRun_SimpleString("sys.stdout.write = redirect.out_write;"
				       "sys.stderr.write = redirect.err_write;"
				       "sys.stdin.readline = redirect.in_readline;");
		i = PyRun_SimpleString("globals()['_vip_Process']=1");

		if (i != 0) {
			vip_debug("Init Python: an error occured\n");
		}
		else {

			__traceback = PyImport_ImportModule("traceback");
		}

		// release lock
		PyEval_SaveThread();
		pythonInit = true;
		vip_debug("Python initialized\n");
	}

public:
	// global PyInterpreterState, used to create PyThreadState objects for each thread
	PyInterpreterState* interpreterState;
	PyThreadState* threadState;

	~PythonInit()
	{
		pythonInit = false;
		__python_closed = true;

		vip_debug("Stop Python...\n");

		// stop all VipPyLocal objects
		QList<VipPyLocal*> locals = VipPyLocal::instances();
		for (int i = 0; i < locals.size(); ++i) {
			locals[i]->stop(true);
		}

		// Since Python 3.11: freeze in Py_Finalize

		// acquire GIL before Py_Finalize() (or crash)
		// bool locked = PyGILState_Check();
		// if (!locked && PyGILState_GetThisThreadState())
		//	PyEval_RestoreThread(PyGILState_GetThisThreadState());
		// Py_Finalize();

		vip_debug("Python stopped\n");
	}

	static PythonInit* instance()
	{
		static PythonInit* inst = new PythonInit();
		return inst;
	}
};

// Global Interpreter Locker mechanism
VipGILLocker::VipGILLocker()
  : state(PyGILState_Check())
{
	// create the PyThreadState for this thread if it does not exist
	if (!PyGILState_GetThisThreadState())
		PyThreadState_New(PythonInit::instance()->interpreterState);

	// lock the thread if not already locked
	if (!state)
		PyEval_RestoreThread(PyGILState_GetThisThreadState());
}

VipGILLocker::~VipGILLocker()
{
	// release GIL if previously locked
	if (!state)
		PyEval_SaveThread();
}

VipNDArray vipFromNumpyArray(void* obj)
{
	PyArrayObject* array = (PyArrayObject*)obj;
	if (obj && PyArray_Check((PyObject*)obj)) {

		int ndims = PyArray_NDIM(array);
		int type = vipNumpyToQt(PyArray_TYPE(array));
		if (type == 0)
			return VipNDArray();
		VipNDArrayShape shape;
		VipNDArrayShape strides;
		if (ndims) {
			shape.resize(ndims);
			strides.resize(ndims);
			npy_intp* sh = PyArray_SHAPE(array);
			std::copy(sh, sh + ndims, shape.data());

			sh = PyArray_STRIDES(array);
			std::copy(sh, sh + ndims, strides.data());

			int type_size = QMetaType(type).sizeOf();
			// numpy strides are in bytes
			for (int i = 0; i < ndims; ++i)
				strides[i] /= type_size;
		}

		void* opaque = PyArray_DATA(array);

		return VipNDArray::makeView(opaque, type, shape, strides).copy();
	}
	return VipNDArray();
}

void VipPyLocal::writeBytesFromProcess()
{
	if (QProcess* p = qobject_cast<QProcess*>(sender())) {
		QByteArray ar = p->readAllStandardOutput();
		QByteArray err = p->readAllStandardError();
		if (!ar.isEmpty())
			__addStandardOutput(ar);
		if (!err.isEmpty())
			__addStandardError(err);
	}
}

bool VipPyLocal::handleMagicCommand(const QString& cmd)
{
	if (cmd.startsWith("%pip") && !PythonInit::instance()->local_pip.isEmpty()) {
		QPointer<QProcess> p = new QProcess(this);
		connect(p, SIGNAL(readyReadStandardOutput()), this, SLOT(writeBytesFromProcess()));
		connect(p, SIGNAL(readyReadStandardError()), this, SLOT(writeBytesFromProcess()));
		connect(p, SIGNAL(finished(int, QProcess::ExitStatus)), p, SLOT(deleteLater()));
		QString c = cmd;
		c.replace("%pip", PythonInit::instance()->local_pip);
		setWriteToProcess(p);
		p->start(c);
		while (p && p->state() == QProcess::Running) {
			vipProcessEvents(nullptr, 20);
		}
		return true;
		/*QString c;
		c += "import subprocess;";
		QString args = cmd.mid(5); args.remove("\n");
		c += "subprocess.run([\"" + PythonInit::instance()->local_pip + "\", \"" + args + "\"], capture_output = True, shell=True)";
		int id = this->execCode(c);
		this->wait(id);
		return true;*/
	}
	return false;
}

// thread manipulation functions
static std::set<quint64> _stop_threads;
static QMutex _stop_thread_mutex;

static int stopThread(quint64 threadid)
{
	QMutexLocker lock(&_stop_thread_mutex);
	if (_stop_threads.find(threadid) == _stop_threads.end() && threadid > 0) {
		_stop_threads.insert(threadid);
		return PyThreadState_SetAsyncExc(threadid, PyExc_SystemExit);
	}
	return 0;
}

static void threadStopped(quint64 threadid)
{
	QMutexLocker lock(&_stop_thread_mutex);
	_stop_threads.erase(threadid);
}

class PyRunnable;
using PyRunPtr = QSharedPointer<VipBasePyRunnable>;

struct PyRunThread : public QThread
{
	VipPyLocal* local;
	std::deque<PyRunPtr> runnables;

	QMutex mutex;
	QWaitCondition cond;
	quint64 threadId;

	PyRunThread()
	  : local(nullptr)
	  , threadId(0)
	{
	}

	VipPyFuture add(PyRunnable* r);
	void runOneLoop(VipPyLocal*);

	bool waitForRunnable(const PyRunnable*, unsigned long time = ULONG_MAX);

	void run()
	{
		threadId = ::vipPyThreadId();
		if (VipPyLocal* tmp = local) {
			registerPyIOOperation(tmp);
			while (VipPyLocal* loc = local) {
				runOneLoop(loc);
				vipSleep(5);
			}
			unregisterPyIOOperation(tmp);
		}
		threadStopped(threadId);
		threadId = 0;
	}
};

QVariant vipExecPythonCode(const QString& code, VipPyLocal* local)
{
	PyObject* globals = (PyObject*)(local ? local->globalDict() : nullptr);
	if (!globals) {
		PyObject* __main__ = PyImport_ImportModule("__main__");
		globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);
	}
	PyObject* obj = PyRun_String(code.toLatin1().data(), Py_file_input, globals, globals);
	QVariant result;
	if (obj) {
		result = vipPythonToVariant(obj);
		Py_DECREF(obj);
	}
	else {
		VipPyError error(compute_error_t{});
		if (local)
			static_cast<VipPyIOOperation*>(local)->__addStandardError(error.traceback.toLatin1().data());
		result = QVariant::fromValue(error);
	}
	return result;
}

QVariant vipSendPythonVariable(const QString& name, const QVariant& value, VipPyLocal* local)
{
	PyObject* globals = (PyObject*)(local ? local->globalDict() : nullptr);
	if (!globals) {
		PyObject* __main__ = PyImport_ImportModule("__main__");
		globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);
	}
	PyObject* obj_object = (PyObject*)vipVariantToPython(value);
	QVariant result;
	if (obj_object) {
		PyDict_SetItemString((PyObject*)local->globalDict(), name.toLatin1().data(), obj_object);
		Py_DECREF(obj_object);
	}
	else {
		VipPyError error;
		error.traceback = "Cannot convert object to Python";
		result = QVariant::fromValue(error);
	}
	return result;
}

QVariant vipRetrievePythonVariable(const QString& name, VipPyLocal* local)
{
	PyObject* globals = (PyObject*)(local ? local->globalDict() : nullptr);
	if (!globals) {
		PyObject* __main__ = PyImport_ImportModule("__main__");
		globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);
	}
	PyObject* obj = PyDict_GetItemString((PyObject*)local->globalDict(), name.toLatin1().data());
	QVariant result;
	if (obj)
		result = vipPythonToVariant(obj);
	else {
		VipPyError error;
		error.traceback = "Cannot convert object to Python";
		result = QVariant::fromValue(error);
	}
	return result;
}

struct PyRunnable : public VipBasePyRunnable
{
	QPointer<PyRunThread> runThread;

	VipPyCommandList commands;
	VipPyCommand command;

	// For all
	QVariant result;

	std::atomic<bool> finished{ false };

	PyRunnable(PyRunThread* runThread)
	  : runThread(runThread)
	{
	}

	void run(VipPyLocal* local)
	{
		VipGILLocker lock;
		PyErr_Clear();

		if (commands.isEmpty()) {
			if (command.type == VipPyCommand::ExecCode)
				result = vipExecPythonCode(command.string, local);
			else if (command.type == VipPyCommand::SendObject)
				result = vipSendPythonVariable(command.string, command.object, local);
			else
				result = vipRetrievePythonVariable(command.string, local);
		}
		else {
			QVariantMap res;
			for (const VipPyCommand& cmd : commands) {
				QVariant r;
				if (cmd.type == VipPyCommand::ExecCode)
					r = vipExecPythonCode(cmd.string, local);
				else if (cmd.type == VipPyCommand::SendObject)
					r = vipSendPythonVariable(cmd.string, cmd.object, local);
				else
					r = vipRetrievePythonVariable(cmd.string, local);
				if (!r.value<VipPyError>().isNull()) {
					result = r;
					goto end;
				}
				res[cmd.buildId()] = r;
			}
			result = QVariant::fromValue(res);
		}

	end:
		finished.store(true);
	}

	bool isFinished() const { return finished.load(std::memory_order_relaxed); }
	bool wait(int milli) const
	{
		if (isFinished())
			return true;
		if (PyRunThread* th = runThread) {
			return th->waitForRunnable(this, milli < 0 ? ULONG_MAX : (unsigned long)milli);
		}
		return false;
	}

	QVariant value(int milli) const
	{
		if (!wait(milli))
			return QVariant::fromValue(VipPyError("Timeout"));
		return result;
	}
};

VipPyFuture PyRunThread::add(PyRunnable* r)
{
	PyRunPtr res(r);
	if (::vipPyThreadId() == this->threadId) {
		// Immediate run
		r->run(this->local);
	}
	else {
		// add the PyRunnable object and compute its ID
		QMutexLocker locker(&mutex);
		runnables.push_back(res);
	}
	return VipPyFuture(res);
}

bool PyRunThread::waitForRunnable(const PyRunnable* r, unsigned long time)
{
	qint64 start = QDateTime::currentMSecsSinceEpoch();

	mutex.lock();

	// wait for it
	while (!r->finished.load()) {
		if (QDateTime::currentMSecsSinceEpoch() - start >= time) {
			mutex.unlock();
			return false;
		}
		if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
			mutex.unlock();
			// We need to check this pointer, as it might be destroyed during vipProcessEvents().
			QPointer<PyRunThread> _this = this;
			vipProcessEvents(nullptr, 10); // main thread: process events
			if (!_this.data())
				return false;
			mutex.lock();
		}
		else {
			if (!cond.wait(&mutex, time)) {
				mutex.unlock();
				return false;
			}
		}
	}
	mutex.unlock();
	return true;
}

void PyRunThread::runOneLoop(VipPyLocal* loc)
{
	while (runnables.size()) {
		PyRunPtr current;
		{
			QMutexLocker locker(&mutex);
			current = runnables.front();
			runnables.pop_front();
		}
		static_cast<PyRunnable*>(current.data())->run(loc);
		QMutexLocker locker(&mutex);
		cond.wakeAll();
	}
}

class VipPyLocal::PrivateData
{
public:
	PrivateData()
	  : globals(nullptr)
	  , wait_for_line(false)
	{
	}
	PyObject* globals;

	PyRunThread runThread;

	// If not null, calls to write() will go to the process
	QPointer<QProcess> writeToProcess;

	QByteArray input;
	QByteArray std_output;
	QByteArray std_error;
	QMutex line_mutex;
	QWaitCondition line_cond;
	bool wait_for_line;
};

static QMutex pylocal_mutex;
static QList<VipPyLocal*> pylocal_instances;

VipPyLocal::VipPyLocal(QObject* parent)
  : VipPyIOOperation(parent)
{
	// init python
	PythonInit::instance();
	VIP_CREATE_PRIVATE_DATA(d_data);

	VipGILLocker lock;
	PyObject* __main__ = PyImport_ImportModule("__main__");
	d_data->globals = PyModule_GetDict(__main__);
	Py_DECREF(__main__);

	QMutexLocker locker(&pylocal_mutex);
	pylocal_instances.append(this);

	connect(&d_data->runThread, SIGNAL(finished()), this, SLOT(emitFinished()), Qt::DirectConnection);
}

QList<VipPyLocal*> VipPyLocal::instances()
{
	QMutexLocker locker(&pylocal_mutex);
	return pylocal_instances;
}

VipPyLocal* VipPyLocal::instance(quint64 thread_id)
{
	QMutexLocker locker(&pylocal_mutex);
	for (int i = 0; i < pylocal_instances.size(); ++i) {
		if (static_cast<PyRunThread*>(pylocal_instances[i]->thread())->threadId == thread_id)
			return pylocal_instances[i];
	}
	return nullptr;
}

void VipPyLocal::setWriteToProcess(QProcess* p)
{
	d_data->writeToProcess = p;
}
QProcess* VipPyLocal::writeToProcess() const
{
	return d_data->writeToProcess;
}

void VipPyLocal::startInteractiveInterpreter()
{
	this->execCode("import sys\n"
		       "def _prompt(text=''):\n"
		       "  sys.stdout.write(text)\n"
		       "  return sys.stdin.readline()\n"
		       "\n"
		       "import code;code.interact(None,_prompt,globals())");
}

VipPyLocal::~VipPyLocal()
{
	QMutexLocker locker(&pylocal_mutex);
	pylocal_instances.removeOne(this);

	stop(true);
}

bool VipPyLocal::isWaitingForInput() const
{
	return d_data->wait_for_line;
}

bool VipPyLocal::start()
{
	if (__python_closed)
		return false;
	if (!d_data->runThread.local) {
		d_data->runThread.local = this;
		d_data->runThread.start();
		emitStarted();
	}
	return true;
}

void VipPyLocal::stop(bool wait)
{
	if (d_data->runThread.local) {

		d_data->runThread.local = nullptr;
		int res = d_data->runThread.QThread::wait(100);

		// first, stop the current Python code being executed (if any)
		if (d_data->runThread.threadId != 0 && !res) {
			VipGILLocker locker;
			res = stopThread((qint64)d_data->runThread.threadId);
		}

		// stop the interpreter thread
		d_data->runThread.local = nullptr;
		if (wait)
			d_data->runThread.QThread::wait();
	}
}

bool VipPyLocal::__stopCodeIfNeeded()
{
	if (!d_data->runThread.local) {
		return true;
	}
	return false;
}

bool VipPyLocal::isRunning() const
{
	return d_data->runThread.threadId != 0;
}

bool VipPyLocal::isStopping() const
{
	return d_data->runThread.local == nullptr;
}

void* VipPyLocal::globalDict()
{
	return d_data->globals;
}

QThread* VipPyLocal::thread()
{
	return &d_data->runThread;
}

void VipPyLocal::__addStandardOutput(const QByteArray& data)
{
	d_data->std_output += data;
	emitReadyReadStandardOutput();
	vipProcessEvents(nullptr, 50);
}

void VipPyLocal::__addStandardError(const QByteArray& data)
{
	d_data->std_error += data;
	emitReadyReadStandardError();
	vipProcessEvents(nullptr, 50);
}

QByteArray VipPyLocal::__readinput()
{

	QMutexLocker lock(&d_data->line_mutex);
	d_data->input.clear();
	d_data->wait_for_line = true;
	while (d_data->input.isEmpty()) {
		d_data->runThread.runOneLoop(this);
		d_data->line_cond.wait(&d_data->line_mutex, 15);
		if (__stopCodeIfNeeded())
			return QByteArray("exit()\n");
	}
	d_data->wait_for_line = false;
	return d_data->input;
}

QByteArray VipPyLocal::readAllStandardOutput()
{
	QByteArray out = d_data->std_output;
	d_data->std_output.clear();
	return out;
}

QByteArray VipPyLocal::readAllStandardError()
{
	QByteArray err = d_data->std_error;
	d_data->std_error.clear();
	return err;
}

qint64 VipPyLocal::write(const QByteArray& data)
{
	QMutexLocker lock(&d_data->line_mutex);
	if (d_data->writeToProcess) {
		return d_data->writeToProcess->write(data);
	}
	d_data->input = data;
	d_data->line_cond.wakeAll();
	return data.size();
}

QVariant VipPyLocal::execCommand(const VipPyCommand& cmd)
{
	VipGILLocker lock;
	if (cmd.type == VipPyCommand::ExecCode)
		return vipExecPythonCode(cmd.string, this);
	else if (cmd.type == VipPyCommand::SendObject)
		return vipSendPythonVariable(cmd.string, cmd.object, this);
	else
		return vipRetrievePythonVariable(cmd.string, this);
}

QVariant VipPyLocal::execCommands(const VipPyCommandList& cmds)
{
	VipGILLocker lock;
	QVariantMap res;
	for (const VipPyCommand& cmd : cmds) {
		QVariant r;
		if (cmd.type == VipPyCommand::ExecCode)
			r = vipExecPythonCode(cmd.string, this);
		else if (cmd.type == VipPyCommand::SendObject)
			r = vipSendPythonVariable(cmd.string, cmd.object, this);
		else
			r = vipRetrievePythonVariable(cmd.string, this);

		if (!r.value<VipPyError>().isNull())
			return r;

		res[cmd.buildId()] = r;
	}
	return QVariant::fromValue(res);
}

VipPyFuture VipPyLocal::sendCommand(const VipPyCommand& cmd)
{
	PyRunnable* r = new PyRunnable(&d_data->runThread);
	r->command = cmd;
	return d_data->runThread.add(r);
}
VipPyFuture VipPyLocal::sendCommands(const VipPyCommandList& cmds)
{
	PyRunnable* r = new PyRunnable(&d_data->runThread);
	r->commands = cmds;
	return d_data->runThread.add(r);
}

bool VipPyLocal::wait(bool* alive, int msecs)
{
	// wait for a boolean value
	qint64 st = QDateTime::currentMSecsSinceEpoch();
	while (*alive && (msecs < 0 || (QDateTime::currentMSecsSinceEpoch() - st) < msecs)) {

		if (vipPyThreadId() == d_data->runThread.threadId) {
			// if we are in the run thread, run pending runnable
			d_data->runThread.runOneLoop(this);
		}
	}

	return (QDateTime::currentMSecsSinceEpoch() - st) < msecs;
}

void initPython()
{
	PythonInit::instance();
}

void uninitPython()
{
	delete PythonInit::instance();
}

/* VipPyCodeObject::VipPyCodeObject()
{
	code = nullptr;
}

VipPyCodeObject::VipPyCodeObject(void* code)
{
	this->code = code;
}

VipPyCodeObject::VipPyCodeObject(const QString& expr, Type type)
{
	VipGILLocker lock;
	PyErr_Clear();
	int eval = Py_file_input;
	if (type == Eval)
		eval = Py_eval_input;
	else if (type == Single)
		eval = Py_single_input;
	code = Py_CompileString(expr.toLatin1().data(), "<input>", eval);
	if (code)
		pycode = expr;
}

VipPyCodeObject::VipPyCodeObject(const VipPyCodeObject& other)
{
	code = other.code;
	if (code) {
		VipGILLocker lock;
		Py_INCREF((PyObject*)(code));
	}
	pycode = other.pycode;
}

VipPyCodeObject::VipPyCodeObject(VipPyCodeObject&& other) noexcept
  : code(other.code)
  , pycode(std::move(other.pycode))
{
	other.code = nullptr;
}

VipPyCodeObject& VipPyCodeObject::operator=(VipPyCodeObject&& other) noexcept
{
	std::swap(code, other.code);
	std::swap(pycode, other.pycode);
	return *this;
}

VipPyCodeObject::~VipPyCodeObject()
{
	if (code) {
		VipGILLocker lock;
		Py_DECREF((PyObject*)(code));
	}
}

VipPyCodeObject& VipPyCodeObject::operator=(const VipPyCodeObject& other)
{
	code = other.code;
	if (code) {
		VipGILLocker lock;
		Py_INCREF((PyObject*)(code));
	}
	pycode = other.pycode;
	return *this;
}*/

void VipPyIOOperation::startInteractiveInterpreter()
{
	this->execCode("import code;code.interact(None,None,globals())");
}

static QStringList classNames(const QString& filename)
{
	// find all Python classes starting with "Thermavip" in given Python file

	QFile fin(filename);
	fin.open(QFile::ReadOnly);

	QStringList res;
	QString ar = fin.readAll();
	QStringList lines = ar.split("\n");
	for (int i = 0; i < lines.size(); ++i) {
		if (lines[i].startsWith("class ")) {
			QStringList elems = lines[i].split(" ", QString::SkipEmptyParts);
			if (elems.size() > 1 && elems[1].startsWith("Thermavip")) {
				QString classname = elems[1];
				int index = classname.indexOf("(");
				if (index >= 0)
					classname = classname.mid(0, index);
				index = classname.indexOf(":");
				if (index >= 0)
					classname = classname.mid(0, index);

				if (classname != "ThermavipPyProcessing") // base class for all processings
					res.append(classname);
			}
		}
	}

	return res;
}

QStringList VipPyInterpreter::addProcessingFile(const QFileInfo& file, const QString& prefix, bool register_processings)
{
	QStringList classnames = classNames(file.canonicalFilePath());
	// No classes starting with 'Thermavip': do not load the file
	if (classnames.size() == 0)
		return QStringList();

	vip_debug("parsed %s, found %s\n", file.canonicalFilePath().toLatin1().data(), classnames.join(", ").toLatin1().data());

	// run the file and extract the class description
	QFile in(file.canonicalFilePath());
	in.open(QFile::ReadOnly);
	QString code = in.readAll();
	VipPyError err = this->execCode(code).value(20000).value<VipPyError>();
	if (!err.isNull()) {
		VIP_LOG_WARNING("Cannot load Python processing: " + file.baseName());
		vip_debug("Python load error: \n%s\n", err.traceback.toLatin1().data());
		return QStringList();
	}

	QString category = prefix.size() ? prefix : "Python";
	QStringList res;

	for (int j = 0; j < classnames.size(); ++j) {
		QString code = "s = " + classnames[j] +
			       ".__doc__\n"
			       "it = " +
			       classnames[j] + "().displayHint()";

		err = this->execCode(code).value(3000).value<VipPyError>();
		if (!err.isNull()) {
			VIP_LOG_WARNING("Cannot load Python processing: " + classnames[j]);
			vip_debug("Python load Python processing: \n%s\n", err.traceback.toLatin1().data());
			return res;
		}
		QVariant doc = this->retrieveObject("s").value(3000);
		QVariant input_tr = this->retrieveObject("it").value(3000);

		if (!input_tr.value<VipPyError>().isNull()) {
			VIP_LOG_WARNING("Cannot load Python processing: " + classnames[j]);
			return res;
		}

		// create the VipProcessingObject::Info object corresponding to this Python class and register it
		// so that it will be available in the video and plot player.

		if (register_processings) {
			QString cname = classnames[j];
			cname.remove("Thermavip");
			VipProcessingObject::Info info(cname, "", category, QIcon(), qMetaTypeId<PyProcessing*>());
			info.displayHint = (VipProcessingObject::DisplayHint)input_tr.toInt();
			info.init = cname;
			info.description = doc.toString();
			VipProcessingObject::registerAdditionalInfoObject(info);
			VIP_LOG_INFO("Added Python processing " + cname + " in category " + info.category);
			vip_debug("Added Python processing %s in category %s\n", cname.toLatin1().data(), info.category.toLatin1().data());
		}
		res.push_back(classnames[j]);
	}
	return res;
}

QStringList VipPyInterpreter::addProcessingDirectory(const QString& dir, bool register_processings)
{
	return addProcessingDirectoryInternal(dir, QString(), register_processings);
}

QStringList VipPyInterpreter::addProcessingDirectoryInternal(const QString& dir, const QString& prefix, bool register_processings)
{
	// Inspect a directory (recursive), find all Python files, find all classes starting with Thermavip in these files, and exec these files
	// to add the classes in the global dict.

	vip_debug("inspect %s\n", dir.toLatin1().data());

	QStringList found;

	// keep track of loaded directories for the current global Python interpreter
	QStringList lst = this->property("_vip_dirs").toStringList();
	if (lst.indexOf(QFileInfo(dir).canonicalFilePath()) >= 0)
		return found;
	lst.append(QFileInfo(dir).canonicalFilePath());
	this->setProperty("_vip_dirs", lst);

	QFileInfoList res = QDir(dir).entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files, QDir::Name);
	for (int i = 0; i < res.size(); ++i) {
		if (res[i].isDir()) {
			found += addProcessingDirectoryInternal(res[i].canonicalFilePath(), prefix.size() ? prefix + "/" + res[i].baseName() : res[i].baseName(), register_processings);
		}
		else if (res[i].suffix() == "py") {
			found += addProcessingFile(res[i], prefix, register_processings);
		}
	}
	return found;
}

class VipPyInterpreter::PrivateData
{
public:
	PrivateData()
	  : lock(QReadWriteLock::Recursive)
	  , dirty(true)
	  , type("VipPyLocal")
	  , workingDirectory("./")
	  , pyIOOperation(nullptr)
	  , launchCode(VipPyInterpreter::InIPythonInterp)
	{
		startupCode = "import numpy as np";
		python = "python";
	}
	// Lock protecting the pyIOOperation
	QReadWriteLock lock;
	bool dirty;
	QString type;
	QString python;
	QVariantMap params;
	QString workingDirectory;
	QString startupCode;
	std::unique_ptr<VipPyIOOperation> pyIOOperation;
	PyLaunchCode launchCode;
	QPointer<QObject> interp;
};

VipPyInterpreter::VipPyInterpreter(QObject* parent)
  : VipPyIOOperation(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipPyInterpreter::~VipPyInterpreter()
{
	clear();
}

void VipPyInterpreter::setPyType(const QString& type)
{
	QWriteLocker ll(&d_data->lock);
	if (type != d_data->type) {
		QObject* obj = vipCreateVariant((type + "*").toLatin1().data()).value<QObject*>();
		if (obj) {
			delete obj;
			d_data->type = type;
			d_data->dirty = true;
		}
	}
}

QString VipPyInterpreter::pyType() const
{
	return d_data->type;
}

void VipPyInterpreter::setPython(const QString& python)
{
	QWriteLocker ll(&d_data->lock);
	if (python != d_data->python) {
		d_data->python = python;
		d_data->dirty = true;
	}
}
QString VipPyInterpreter::python()
{
	return d_data->python;
}

void VipPyInterpreter::setParameters(const QVariantMap& params)
{
	QWriteLocker ll(&d_data->lock);
	if (params != d_data->params) {
		d_data->params = params;
		d_data->dirty = true;
	}
}

QVariantMap VipPyInterpreter::parameters() const
{
	return d_data->params;
}

void VipPyInterpreter::setWorkingDirectory(const QString& workingDirectory)
{
	QWriteLocker ll(&d_data->lock);
	if (QFileInfo(workingDirectory) != QFileInfo(d_data->workingDirectory) && !workingDirectory.isEmpty()) {
		d_data->workingDirectory = workingDirectory;
		d_data->workingDirectory.replace("\\", "/");
		d_data->dirty = true;
	}
}

QString VipPyInterpreter::workingDirectory() const
{
	return d_data->workingDirectory;
}

void VipPyInterpreter::setStartupCode(const QString& code)
{
	QWriteLocker ll(&d_data->lock);
	if (code != d_data->startupCode) {
		d_data->startupCode = code;
		d_data->dirty = true;
	}
}
QString VipPyInterpreter::startupCode() const
{
	return d_data->startupCode;
}

void VipPyInterpreter::setLaunchCode(PyLaunchCode l)
{
	QWriteLocker ll(&d_data->lock);
	d_data->launchCode = l;
}
VipPyInterpreter::PyLaunchCode VipPyInterpreter::launchCode() const
{
	return d_data->launchCode;
}

void VipPyInterpreter::setMainInterpreter(QObject* interp)
{
	QWriteLocker ll(&d_data->lock);
	d_data->interp = interp;
}
QObject* VipPyInterpreter::mainInterpreter() const
{
	return d_data->interp;
}

void VipPyInterpreter::clear()
{
	QWriteLocker ll(&d_data->lock);
	d_data->pyIOOperation.reset();
}

bool VipPyInterpreter::start()
{
	QReadLocker ll(&d_data->lock);
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->start();
	return false;
}

void VipPyInterpreter::stop(bool wait)
{
	QReadLocker ll(&d_data->lock);
	if (!__python_closed && pyIOOperation())
		pyIOOperation()->stop(wait);
}

bool VipPyInterpreter::isRunning() const
{
	QReadLocker ll(&d_data->lock);
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->isRunning();
	return false;
}

bool VipPyInterpreter::isWaitingForInput() const
{
	QReadLocker ll(&d_data->lock);
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->isWaitingForInput();
	return false;
}

bool VipPyInterpreter::wait(bool* alive, int msecs)
{
	QReadLocker ll(&d_data->lock);
	if (__python_closed && pyIOOperation())
		return true;
	return pyIOOperation()->wait(alive, msecs);
}

struct BoolLocker
{
	bool* value;
	BoolLocker(bool& val)
	{
		value = &val;
		val = true;
	}
	~BoolLocker() { *value = false; }
};

VipPyIOOperation* VipPyInterpreter::reset(bool create_new)
{
	if (d_data->pyIOOperation && !d_data->dirty && !create_new)
		return d_data->pyIOOperation.get();

	this->d_data->dirty = false;
	if (this->d_data->pyIOOperation) {
		// avoid a possible issue when trying to stop a VipPyLocal on its own thread (usually during a I/O operation)

		if (VipPyLocal* loc = qobject_cast<VipPyLocal*>(this->d_data->pyIOOperation.get())) {
			if (loc->thread() == QThread::currentThread()) {
				// in this, case, do NOT stop the thread (or it will try to wait on itself).
				// just return the current VipPyLocal and keep the dirty mark.
				this->d_data->dirty = true;
				return this->d_data->pyIOOperation.get();
			}
			else {
				// vip_debug("delete pyIOOperation\n");
				this->d_data->pyIOOperation.reset();
				// vip_debug("nullptr pyIOOperation\n");
			}
		}
		else {
			this->d_data->pyIOOperation.reset();
		}
	}
	this->d_data->pyIOOperation.reset();
	if (d_data->type == "VipPyLocal")
		this->d_data->pyIOOperation.reset(new VipPyLocal());
	else {
		this->d_data->pyIOOperation.reset(vipCreateVariant((d_data->type + "*").toLatin1().data()).value<VipPyIOOperation*>());
		if (!this->d_data->pyIOOperation) {
			d_data->lock.unlock();
			return nullptr;
		}
		this->setProperty("_vip_dirs", QVariant::fromValue(QStringList())); // clear inspected path to restart PyAddProcessingDirectory
	}
	if (!this->d_data->pyIOOperation->initialize(d_data->params)) {
		this->d_data->pyIOOperation.reset();
		return nullptr;
	}

	connect(this->d_data->pyIOOperation.get(), SIGNAL(readyReadStandardOutput()), this, SLOT(emitReadyReadStandardOutput()), Qt::DirectConnection);
	connect(this->d_data->pyIOOperation.get(), SIGNAL(readyReadStandardError()), this, SLOT(emitReadyReadStandardError()), Qt::DirectConnection);
	connect(this->d_data->pyIOOperation.get(), SIGNAL(started()), this, SLOT(emitStarted()), Qt::DirectConnection);
	connect(this->d_data->pyIOOperation.get(), SIGNAL(finished()), this, SLOT(emitFinished()), Qt::DirectConnection);
	this->d_data->pyIOOperation->start();
	this->d_data->pyIOOperation->execCode("import os;os.chdir('" + d_data->workingDirectory + "')").wait();
	this->d_data->pyIOOperation->execCode(d_data->startupCode).wait();

	// register all files found in the Python directory
	/* this->addProcessingDirectory(vipGetPythonDirectory(), false);
	QString std_path = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python";
	vip_debug("inspect path %s\n", std_path.toLatin1().data());
	this->addProcessingDirectory( std_path, false);*/
	return d_data->pyIOOperation.get();
}

VipPyIOOperation* VipPyInterpreter::pyIOOperation(bool create_new) const
{
	bool is_recursive = false;
	auto* op = pyIOOperationInternal(create_new, &is_recursive);
	if (op && !is_recursive)
		d_data->lock.unlock();
	return op;
}

VipPyIOOperation* VipPyInterpreter::pyIOOperationInternal(bool create_new, bool* is_recursive) const
{
	// Returns current or newly created VipPyIOOperation.
	// The lock is STILL held on return

	if (__python_closed)
		return nullptr;

	// avoid recursive calls to this function
	thread_local bool recurs = false;
	if (recurs) {
		*is_recursive = true;
		return d_data->pyIOOperation.get();
	}
	BoolLocker locker(recurs);

	// Lock for read
	VipPyInterpreter* _this = const_cast<VipPyInterpreter*>(this);
	_this->d_data->lock.lockForRead();

	if (!d_data->pyIOOperation || d_data->dirty || create_new) {
		// Potentially dirty, lock for write
		_this->d_data->lock.unlock();
		_this->d_data->lock.lockForWrite();
		try {
			return _this->reset(create_new);
		}
		catch (...) {
			_this->d_data->lock.unlock();
			return nullptr;
		}
	}

	return _this->d_data->pyIOOperation.get();
}

QByteArray VipPyInterpreter::readAllStandardOutput()
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->readAllStandardOutput();
	return QByteArray();
}

QByteArray VipPyInterpreter::readAllStandardError()
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->readAllStandardError();
	return QByteArray();
}

qint64 VipPyInterpreter::write(const QByteArray& data)
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->write(data);
	return 0;
}

QVariant VipPyInterpreter::execCommand(const VipPyCommand& cmd)
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->execCommand(cmd);
	return QVariant::fromValue(VipPyError("nullptr VipPyInterpreter"));
}
QVariant VipPyInterpreter::execCommands(const VipPyCommandList& cmds)
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->execCommands(cmds);
	return QVariant::fromValue(VipPyError("nullptr VipPyInterpreter"));
}

VipPyFuture VipPyInterpreter::sendCommand(const VipPyCommand& cmd)
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->sendCommand(cmd);
	return VipPyFuture();
}
VipPyFuture VipPyInterpreter::sendCommands(const VipPyCommandList& cmds)
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->sendCommands(cmds);
	return VipPyFuture();
}

bool VipPyInterpreter::handleMagicCommand(const QString& cmd)
{
	if (!__python_closed && pyIOOperation())
		return pyIOOperation()->handleMagicCommand(cmd);
	return false;
}

void VipPyInterpreter::printOutput()
{
	std::cout << readAllStandardOutput().data();
	std::cout.flush();
}

void VipPyInterpreter::printError()
{
	std::cerr << readAllStandardError().data();
	std::cerr.flush();
}

void VipPyInterpreter::startInteractiveInterpreter()
{
	if (!__python_closed && pyIOOperation())
		pyIOOperation()->startInteractiveInterpreter();
}

VipPyInterpreter* VipPyInterpreter::instance()
{
	static VipPyInterpreter inst;
	return &inst;
}

using EvalResultType = QPair<QString, QString>;

static const QString eval_code = "def eval_code(code) :\n"
				 "    import traceback\n"
				 "    try :\n"
				 "        res = eval(code)\n"
				 "        if res is None : return ('', '')\n"
				 "        else : return (str(res), '')\n"
				 "    except SyntaxError as e :\n"
				 "        try :\n"
				 "            exec(code, globals(), globals())\n"
				 "            return ('', '')\n"
				 "        except :\n"
				 "            return ('', traceback.format_exc())\n"
				 "\n";

static EvalResultType evalPythonCode(const QString& code)
{
	static bool init = false;
	VipGILLocker lock;
	if (!init) {
		int r = PyRun_SimpleString(eval_code.toLatin1().data());
		init = true;
	}

	QChar str;
	if (code.contains('"'))
		str = '\'';
	else
		str = '"';

	QString to_exec = QString("tmp=eval_code(") + str + code + str + QString(")");
	vip_debug("%s\n", to_exec.toLatin1().data());

	int r = PyRun_SimpleString(to_exec.toLatin1().data());

	PyObject* __main__ = PyImport_ImportModule("__main__");
	PyObject* globals = PyModule_GetDict(__main__);
	Py_DECREF(__main__);
	PyObject* obj = PyDict_GetItemString(globals, "tmp");
	QVariantList lst = vipPythonToVariant(obj).value<QVariantList>();
	if (lst.size() == 2) {
		return EvalResultType(lst[0].toString(), lst[1].toString());
	}
	return EvalResultType();
}

struct _Event : QEvent
{
	QSharedPointer<bool> alive;
	QSharedPointer<EvalResultType> result;
	QString code;
	_Event(QSharedPointer<bool> v, QSharedPointer<EvalResultType> res)
	  : QEvent(QEvent::MaxUser)
	  , alive(v)
	  , result(res)
	{
		*alive = true;
	}
	~_Event() { *alive = false; }
};
struct _EventObject : QObject
{
	virtual bool event(QEvent* evt)
	{
		if (evt->type() == QEvent::MaxUser) {
			// exec string
			*static_cast<_Event*>(evt)->result = evalPythonCode(static_cast<_Event*>(evt)->code);
			*static_cast<_Event*>(evt)->alive = false;
			return true;
		}
		return false;
	}
};
static _EventObject* object()
{
	static _EventObject inst;
	if (inst.thread() != QCoreApplication::instance()->thread())
		inst.moveToThread(QCoreApplication::instance()->thread());
	return &inst;
}

EvalResultType VipPyLocal::evalCodeMainThread(const QString& code)
{

	if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
		return evalPythonCode(code);
	}
	else {
		QSharedPointer<bool> alive(new bool(true));
		QSharedPointer<EvalResultType> result(new EvalResultType());
		_Event* evt = new _Event(alive, result);
		evt->code = code;
		QCoreApplication::instance()->postEvent(object(), evt);
		QPointer<VipPyLocal> loc = VipPyLocal::instance(vipPyThreadId());
		while (*alive) {
			if (loc) {
				loc->wait(alive.data(), 50);
				if (loc->isStopping())
					return EvalResultType("", "STOP signal received");
			}
		}
		return *result;
	}
}
