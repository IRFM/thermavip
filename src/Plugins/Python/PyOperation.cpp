#include <iostream>
#include <cmath>

 
//#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#ifdef VIP_ENABLE_PYTHON_LINK

//undef _DEBUG or pyconfig.h will automatically try to link to python34_d.lib
#undef _DEBUG
extern "C" {

#include "Python.h"
#include "numpy/arrayobject.h"
#include <frameobject.h>
#include <traceback.h>

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#include <dlfcn.h>
#endif

} 

#include <iostream>

#include <QApplication>
#include <QFileInfo> 
#include <QDir>
#include <QMutex> 
#include <QWaitCondition>
#include <QDateTime>
#include <QThread>
#include <QProcess>
#include <QLibrary>

#include "VipDataType.h"
#include "VipLogging.h"
#include "PyOperation.h"
#include "VipEnvironment.h"
#include "VipProcessingObject.h"
#include "VipCore.h"
#include "ThermavipModule.h"

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#include <pthread.h>
qint64 threadId()
{
	return (qint64)pthread_self();
}
#else
#include "Windows.h"
qint64 threadId()
{
	return (qint64)GetCurrentThreadId();
}
#endif


#if PY_MINOR_VERSION    < 4
int PyGILState_Check(void) {
	PyThreadState * tstate = (PyThreadState*)_PyThreadState_Current._value;
	return tstate && (tstate == PyGILState_GetThisThreadState());
}
#endif





#else //no VIP_ENABLE_PYTHON_LINK

#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QMutex>
#include <QWaitCondition>
#include <QDateTime>
#include <QThread>

#include "VipDataType.h"
#include "VipLogging.h"
#include "PyOperation.h"
#include "VipEnvironment.h"
#include "VipProcessingObject.h"
#endif

#include "PyProcessing.h"

QString vipGetPythonDirectory(const QString & suffix)
{
	QString path = vipGetDataDirectory(suffix) + "Python/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetPythonScriptsDirectory(const QString & suffix)
{
	QString path = vipGetPythonDirectory(suffix) + "Scripts/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetPythonScriptsPlayerDirectory(const QString & suffix)
{
	QString path = vipGetPythonScriptsDirectory(suffix) + "Player/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}


static QStringList listPyDir(const QString & dir)
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

QStringList vipGetPythonPlayerScripts(const QString & suffix)
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



//tells if PyFinalize() has been called
static bool __python_closed = false;

#ifdef VIP_ENABLE_PYTHON_LINK

static int numpyToQt(int type)
{
	switch (type)
	{
	case NPY_BOOL: return QMetaType::Bool;
	case NPY_BYTE: return QMetaType::Char;
	case NPY_UBYTE: return QMetaType::UChar;
	case NPY_SHORT: return QMetaType::Short;
	case NPY_USHORT: return QMetaType::UShort;
	case NPY_INT: return QMetaType::Int;
	case NPY_UINT: return QMetaType::UInt;
	case NPY_LONG: return QMetaType::Long;
	case NPY_ULONG: return QMetaType::ULong;
	case NPY_LONGLONG: return QMetaType::LongLong;
	case NPY_ULONGLONG: return QMetaType::ULongLong;
	case NPY_FLOAT: return QMetaType::Float;
	case NPY_DOUBLE: return QMetaType::Double;
	case NPY_CFLOAT: return qMetaTypeId<complex_f>();
	case NPY_CDOUBLE: return qMetaTypeId<complex_d>();
	case NPY_LONGDOUBLE: return qMetaTypeId<vip_long_double>();
	}
	return 0;
}

static int qtToNumpy(int type)
{
	switch (type)
	{
	case QMetaType::Bool: return NPY_BOOL;
	case QMetaType::Char: return NPY_BYTE;
	case QMetaType::UChar: return NPY_UBYTE;
	case QMetaType::Short: return NPY_SHORT;
	case QMetaType::UShort: return NPY_USHORT;
	case QMetaType::Int: return NPY_INT;
	case QMetaType::UInt: return NPY_UINT;
	case QMetaType::Long: return NPY_LONG;
	case QMetaType::ULong: return NPY_ULONG;
	case QMetaType::LongLong: return NPY_LONGLONG;
	case QMetaType::ULongLong: return NPY_ULONGLONG;
	case QMetaType::Float: return NPY_FLOAT;
	case QMetaType::Double: return NPY_DOUBLE;
	}

	if (type == qMetaTypeId<complex_f>())
		return NPY_CFLOAT;
	else if (type == qMetaTypeId<complex_d>())
		return NPY_CDOUBLE;
	else if (type == qMetaTypeId<vip_long_double>())
		return NPY_LONGDOUBLE;

	return -1;
}

static QVector<python_to_variant> & getToVariant()
{
	static QVector<python_to_variant> instance;
	return instance;
}

static QVector<variant_to_python> & getToPython()
{
	static QVector<variant_to_python> instance;
	return instance;
}

void registerToVariantConverter(python_to_variant fun)
{
	getToVariant().push_back(fun);
}

void registerToPythonConverter(variant_to_python fun)
{
	getToPython().push_back(fun);
}

QVariant pythonToVariant(void * pyobject)
{
	if (!pyobject)
		return QVariant();
	else if (pyobject == Py_None)
		return QVariant();

	for (int i = 0; i < getToVariant().size(); ++i)
	{
		QVariant tmp = getToVariant()[i](pyobject);
		if (!tmp.isNull())
			return tmp;
	}
	return QVariant();
}

void * variantToPython(const QVariant & variant)
{
	if (variant.userType() == 0)
	{
		Py_RETURN_NONE;
	}

	for (int i = 0; i < getToPython().size(); ++i)
	{
		void * tmp = getToPython()[i](variant);
		if (tmp)
			return tmp;
	}

	Py_RETURN_NONE;
}

static QVariant stdToVariant(void * pyobject)
{
	QVariant res;
	PyObject * res_object = (PyObject *)pyobject;

	if (PyByteArray_Check(res_object))
	{
		int size = PyByteArray_Size(res_object);
		char * data = PyByteArray_AsString(res_object);
		res = QVariant(QByteArray(data, size));
	}
	else if (PyBytes_Check(res_object))
	{
		int size = PyBytes_Size(res_object);
		char * data = PyBytes_AsString(res_object);
		res = QVariant(QByteArray(data, size));
	}
	else if (PyUnicode_Check(res_object))
	{
		Py_ssize_t size=0; 
		wchar_t* data = PyUnicode_AsWideCharString(res_object, &size);
		res = QVariant(QString::fromWCharArray(data, size));
		PyMem_Free(data);
	}
	else if (PyLong_Check(res_object))
	{
		res = QVariant(PyLong_AsLongLong(res_object));
	}
	else if (PyBool_Check(res_object))
	{
		res = QVariant(res_object == Py_True);
	}
	else if (PyFloat_Check(res_object))
	{
		res = QVariant(PyFloat_AsDouble(res_object));
	}
	else if (PyArray_Check(res_object))
	{
		Py_INCREF(res_object);
		VipNDArray ar = PyNDArray(res_object).copy();
		if (ar.shapeCount() == 3 && ar.shape(2) == 3) {
			//RGB image
			QImage img(ar.shape(1), ar.shape(0),QImage::Format_ARGB32);
			uint * bits = (uint*)img.bits();
			
			if (ar.dataType() != QMetaType::UChar) {
				VipNDArrayType<double> ard = ar.toDouble();
				vipEval(ard, vipClamp(ard, 0., 255.));
				ar = vipCast<uchar>(ard);
			}
			
			const uchar * pix = (const uchar * )ar.constData();
			int size = img.width()*img.height();
			for (int i = 0; i < size; ++i, pix += 3) {
				bits[i] = qRgb(pix[0], pix[1], pix[2]);
			}
			ar = vipToArray(img);
		}
		res = QVariant::fromValue(ar);
	}
	else if (PyComplex_Check(res_object))
	{
		Py_complex comp = PyComplex_AsCComplex(res_object);
		res = QVariant::fromValue(complex_d(comp.real, comp.imag));
	}
	else if (PyList_Check(res_object))
	{
		int len = PyList_GET_SIZE(res_object);
		QVariantList tmp;
		for (int i = 0; i < len; ++i)
		{
			PyObject * obj = PyList_GetItem(res_object, i);
			tmp.append(pythonToVariant(obj));
		}
		res = QVariant::fromValue(tmp);
	}
	else if (PyTuple_Check(res_object))
	{
		int len = PyTuple_GET_SIZE(res_object);
		QVariantList tmp;
		for (int i = 0; i < len; ++i)
		{
			PyObject * obj = PyTuple_GetItem(res_object, i);
			tmp.append(pythonToVariant(obj));
		}
		res = QVariant::fromValue(tmp);
	}
	else if (PyDict_Check(res_object))
	{
		PyObject *key, *value;
		Py_ssize_t pos = 0;
		QVariantMap tmp;

		while (PyDict_Next(res_object, &pos, &key, &value)) {
			/* do something interesting with the values... */
			if (key && value)
			{
				QString k = pythonToVariant(key).toString();
				QVariant v = pythonToVariant(value);
				tmp[k] = v;
			}
		}
		res = QVariant::fromValue(tmp);
	}
	else if (PyArray_IsIntegerScalar(res_object))//strcmp(res_object->ob_type->tp_name,"numpy.int64")==0)
	{
		long long value = PyLong_AsLongLong(res_object);
		res = QVariant(value);
	}
	else if (PyArray_IsAnyScalar(res_object))//strcmp(res_object->ob_type->tp_name,"numpy.int64")==0)
	{
		vip_double value = PyFloat_AsDouble(res_object);
		res = QVariant::fromValue(value);
	}
	else
	{
		PyObject * new_object = PyArray_FromAny(res_object, NULL, 0, 0, NPY_ARRAY_ENSUREARRAY | NPY_ARRAY_C_CONTIGUOUS, NULL);
		if (new_object)
			res = QVariant::fromValue(PyNDArray(new_object).copy());
	}

	return res;
}


static void * stdToPython(const QVariant & obj)
{
	PyObject * obj_object = NULL;
	switch (obj.userType())
	{
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
	case QMetaType::QString:
	{
		std::wstring str = obj.toString().toStdWString();
		obj_object = PyUnicode_FromWideChar(str.c_str(), str.size());
		break;
	}
	case QMetaType::QByteArray:
	{
		QByteArray ar = obj.toByteArray();
		obj_object = PyByteArray_FromStringAndSize(ar.data(), ar.size());
	}
	}

	//other types (complex and array)
	if (!obj_object)
	{
		if (obj.userType() == qMetaTypeId<vip_long_double>())
		{
			obj_object = PyFloat_FromDouble(obj.value<vip_long_double>());
		}
		else if (obj.userType() == qMetaTypeId<complex_d>())
		{
			complex_d comp = obj.value<complex_d>();
			obj_object = PyComplex_FromDoubles(comp.real(), comp.imag());
		}
		else if (obj.userType() == qMetaTypeId<complex_f>())
		{
			complex_f comp = obj.value<complex_f>();
			obj_object = PyComplex_FromDoubles(comp.real(), comp.imag());
		}
		else if (obj.userType() == qMetaTypeId<QVariantList>())
		{
			QVariantList lst = obj.value<QVariantList>();
			obj_object = PyList_New(lst.size());
			for (int i = 0; i < lst.size(); ++i)
			{
				PyObject * item = (PyObject*)variantToPython(lst[i]);
				if (item)
					PyList_SetItem(obj_object, i, item);
			}
		}
		else if (obj.userType() == qMetaTypeId<QStringList>())
		{
			QStringList lst = obj.value<QStringList>();
			obj_object = PyList_New(lst.size());
			for (int i = 0; i < lst.size(); ++i)
			{
				PyObject * item = (PyObject*)variantToPython(lst[i]);
				if (item)
					PyList_SetItem(obj_object, i, item);
			}
		}
		else if (obj.userType() == qMetaTypeId<QVariantMap>())
		{
			QVariantMap map = obj.value<QVariantMap>();
			obj_object = PyDict_New();
			for (QVariantMap::iterator it = map.begin(); it != map.end(); ++it)
			{
				PyObject * item = (PyObject*)variantToPython(it.value());
				if (item)
				{
					std::wstring str = it.key().toStdWString();
					PyDict_SetItem(obj_object, PyUnicode_FromWideChar(str.c_str(), str.size()), item);
				}
			}
		}
		else if (obj.userType() == qMetaTypeId<VipPointVector>())
		{
			VipPointVector vec = obj.value<VipPointVector>();
			//array of 2 rows (x and y)
			VipNDArrayType<vip_double> ar(vipVector(2, vec.size()));
			for (int i = 0; i < vec.size(); ++i)
			{
				ar(vipVector(0, i)) = vec[i].x();
				ar(vipVector(1, i)) = vec[i].y();
			}
			obj_object = (PyObject*)variantToPython(QVariant::fromValue(VipNDArray(ar)));
		}
		else if (obj.userType() == qMetaTypeId<VipComplexPointVector>())
		{
			VipComplexPointVector vec = obj.value<VipComplexPointVector>();
			//array of 2 rows (x and y)
			VipNDArrayType<complex_d> ar(vipVector(2, vec.size()));
			for (int i = 0; i < vec.size(); ++i)
			{
				ar(vipVector(0, i)) = vec[i].x();
				ar(vipVector(1, i)) = vec[i].y();
			}
			obj_object = (PyObject*)variantToPython(QVariant::fromValue(VipNDArray(ar)));
		}
		else if (obj.userType() == qMetaTypeId<VipIntervalSampleVector>())
		{
			VipIntervalSampleVector vec = obj.value<VipIntervalSampleVector>();
			//list of 2 arrays: values and intervals
			VipNDArrayType<double> values(vipVector(vec.size()));
			VipNDArrayType<double> intervals(vipVector(vec.size() * 2));
			for (int i = 0; i < vec.size(); ++i)
			{
				values(vipVector(i)) = vec[i].value;
				intervals(vipVector(i * 2)) = vec[i].interval.minValue();
				intervals(vipVector(i * 2 + 1)) = vec[i].interval.maxValue();
			}
			QVariantList tmp;
			tmp.append(QVariant::fromValue(VipNDArray(values)));
			tmp.append(QVariant::fromValue(VipNDArray(intervals)));
			obj_object = (PyObject*)variantToPython(tmp);
		}
		else if (obj.userType() == qMetaTypeId<VipNDArray>())
		{
			const VipNDArray info = obj.value<VipNDArray>();

			if (vipIsImageArray(info))
			{
				//convert QImage to 3 dims array
				const QImage img = vipToImage(info);
				npy_intp shape[20] = { img.height(),img.width(),3 };
				std::vector<uchar> image(img.height()* img.width() * 3);
				int size = img.height()* img.width();
				const uint * pixels = (const uint *)img.bits();
				for (int i = 0; i < size; ++i) {
					image[i * 3] = qRed(pixels[i]);
					image[i * 3 + 1] = qGreen(pixels[i]);
					image[i * 3 + 2] = qBlue(pixels[i]);
				}
				PyObject * ref = PyArray_SimpleNewFromData(3, shape, NPY_UBYTE, (void*)image.data());
				obj_object = PyArray_NewCopy((PyArrayObject*)ref, NPY_CORDER);
				Py_DECREF(ref);
			}
			else
			{
				int numpy_type = qtToNumpy(info.dataType());
				if (numpy_type >= 0 && info.data())
				{
					npy_intp shape[20];
					std::copy(info.shape().begin(), info.shape().end(), shape);
					int nd = info.shape().size();
					PyObject * ref = PyArray_SimpleNewFromData(nd, shape, numpy_type, (void*)info.constData());
					obj_object = PyArray_NewCopy((PyArrayObject*)ref, NPY_CORDER);
					Py_DECREF(ref);
				}
			}
		}
	}

	return obj_object;
}


#endif //end VIP_ENABLE_PYTHON_LINK


void PyDelayExec::execDelay()
{
	m_done = false;
	QMetaObject::invokeMethod(this, "execInternal", Qt::QueuedConnection);
}
bool PyDelayExec::finished() const
{
	return m_done;
}
void PyDelayExec::execInternal()
{
	m_fun();
	m_done = true;
}

bool PyDelayExec::waitForFinished(int msecs)
{
	qint64 st = QDateTime::currentMSecsSinceEpoch();
	QPointer<PyLocal> loc = PyLocal::instance(threadId());
	bool alive = true;
	while (!m_done && (msecs < 0 || (QDateTime::currentMSecsSinceEpoch() - st < msecs)))
	{
		if (loc)
			loc->wait(&alive, 5);
	}
	return (QDateTime::currentMSecsSinceEpoch() - st < msecs);
}

#ifdef VIP_ENABLE_PYTHON_LINK
static PyObject * __traceback = NULL;
#endif


static QString pyToString(PyObject* obj)
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

PyError::PyError(bool compute)
	:line(0)
{
	if (!compute)
		return;
#ifdef VIP_ENABLE_PYTHON_LINK
	if (PyErr_Occurred())
	{
		PyObject *ptype, *pvalue, *ptraceback;
		PyErr_Fetch(&ptype, &pvalue, &ptraceback);
		if (!pvalue)
			return;

		if (PyTuple_Check(pvalue) && ptype == PyExc_SyntaxError)
		{
			//if the error is a SyntaxError, use a different method to retrieve the error info

			char *msg;
			char *file;
			int offset;
			char *text;
			int res = PyArg_ParseTuple(pvalue, "s(siis)", &msg, &file, &line, &offset, &text);
			if (res)
			{
				//build the filename and traceback

				filename = QString(file);
				traceback = "\tFile '" + filename + ", line " + QString::number(line) + "\n";
				traceback += "\t\t" + QString(text);
				traceback += "\t\t" + QString(offset - 1, QChar(' ')) + "^\n";
				traceback += "SyntaxError: " + QString(msg);
			}
		}
		/**/
		else
		{
			//build traceback from pvalue
			Py_ssize_t s;
			wchar_t* data = PyUnicode_AsWideCharString(pvalue, &s);
			if (data)
				traceback += QString::fromWCharArray(data, s);

			PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

			//try to build the traceback from "traceback.format_exception" function

			bool failed = true;
			if (/*PyObject * trace = PyImport_ImportModule("traceback")*/__traceback)
			{
				if (PyObject * format_exc = PyObject_GetAttrString(__traceback, "format_exception"))
				{
					if (PyObject * lst = PyObject_CallFunctionObjArgs(format_exc, ptype, pvalue, ptraceback, NULL))
					{
						traceback.clear();
						Py_ssize_t size = PyList_Size(lst);
						for (int i = 0; i < size; ++i)
						{
							PyObject * str = PyList_GetItem(lst, i);
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
			traceback = pythonToVariant(v).toString();
			Py_DECREF(v);
			failed = false;
			}
			}
			}*/
			if (failed && strcmp(pvalue->ob_type->tp_name, "SystemError") == 0)
			{
				if (PyObject * o = PyObject_Str(pvalue)) {
					traceback = "SystemError: " + pythonToVariant(o).toString();
					Py_DECREF(o);
				}

			}
		}



		//retrieve the other info based on the traceback

		PyTracebackObject *tstate = (PyTracebackObject*)ptraceback;
		if (NULL != tstate && NULL != tstate->tb_frame) {
			PyFrameObject *frame = tstate->tb_frame;
			line = frame->f_lineno;
			Py_ssize_t size;
			wchar_t *fname = PyUnicode_AsWideCharString(frame->f_code->co_filename, &size);
			filename = QString::fromWCharArray(fname, size);
			wchar_t *funcname = PyUnicode_AsWideCharString(frame->f_code->co_name, &size);
			functionName = QString::fromWCharArray(funcname, size);
		}

		if (traceback.isEmpty()) {
			traceback = pyToString(pvalue);
			/*QByteArray v1 = pyToString(ptype).toLatin1();
			QByteArray v2 = pyToString(pvalue).toLatin1();
			QByteArray v3 = pyToString(ptraceback).toLatin1();
			printf("'%s' '%s' '%s'\n", v1.data(), v2.data(), v3.data());
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

#endif //end VIP_ENABLE_PYTHON_LINK
}


PyError::PyError(void * tuple)
	:line(0)
{
#ifdef VIP_ENABLE_PYTHON_LINK
	//create the error object from a tuple.
	//The tuple must contain 5 elements: 'Error', traceback, filename, function name, line number.
	PyObject * t = (PyObject*)(tuple);

	if (PyTuple_Check(t))
	{
		int size = PyTuple_GET_SIZE(t);
		if (size != 5)
			return;

		PyObject * error = PyTuple_GET_ITEM(t, 0);
		if (!PyUnicode_Check(error))
			return;

		Py_ssize_t s;
		wchar_t* data = PyUnicode_AsWideCharString(error, &s);
		if (QString::fromWCharArray(data, s) != "Error")
			return;

		//retrieve all information
		PyObject *_trace = PyTuple_GET_ITEM(t, 1);
		PyObject *_filename = PyTuple_GET_ITEM(t, 2);
		PyObject *_funname = PyTuple_GET_ITEM(t, 3);
		PyObject *_line = PyTuple_GET_ITEM(t, 4);

		data = PyUnicode_AsWideCharString(_trace, &s);
		traceback = QString::fromWCharArray(data, s);

		data = PyUnicode_AsWideCharString(_filename, &s);
		filename = QString::fromWCharArray(data, s);

		data = PyUnicode_AsWideCharString(_funname, &s);
		functionName = QString::fromWCharArray(data, s);

		line = PyLong_AsLong(_line);
	}

#endif
}

bool PyError::isNull()
{
	return traceback.isEmpty() && line == 0;
}


void PyError::printDebug(std::ostream & oss)
{
	oss << "filename: " << filename.toLatin1().data() << std::endl;
	oss << "functionName: " << functionName.toLatin1().data() << std::endl;
	oss << "line: " << line << std::endl;
	oss << "traceback: \n" << traceback.toLatin1().data() << std::endl;
}







#ifdef VIP_ENABLE_PYTHON_LINK


//Associate each PyLocal instance to a QThread.
//Used to select the right PyLocal instance for I/O operations based on sys.stdin, sys.stderr and sys.stdout
static QMap<qint64, PyIOOperation*> PyLocalThreads;
static QMutex PyLocalThreadsMutex;

static void registerPyIOOperation(PyIOOperation * local)
{
	QMutexLocker locker(&PyLocalThreadsMutex);
	PyLocalThreads[/*(qint64)QThread::currentThreadId()*/threadId()] = local;
}

/*static void registerPyIOOperation(PyIOOperation * local, qint64 id)
{
QMutexLocker locker(&PyLocalThreadsMutex);
PyLocalThreads[id] = local;
}*/

static void unregisterPyIOOperation(PyIOOperation *)
{
	QMutexLocker locker(&PyLocalThreadsMutex);
	PyLocalThreads.remove(/*(qint64)QThread::currentThreadId()*/threadId());
}

static PyIOOperation * getCurrentPyIOOperation()
{
	QMutexLocker locker(&PyLocalThreadsMutex);
	QMap<qint64, PyIOOperation*> ::iterator it = PyLocalThreads.find(/*(qint64)QThread::currentThreadId()*/threadId());
	if (it != PyLocalThreads.end())
		return it.value();
	else
		return NULL;
}


static PyObject* Stdout_write(PyObject *self, PyObject* args)
{
	Q_UNUSED(self)
		char* data;
	if (!PyArg_ParseTuple(args, "s", &data))
		return 0;

	PyIOOperation * loc = getCurrentPyIOOperation();
	int size = 0;
	Py_BEGIN_ALLOW_THREADS
		size = (int)strlen(data);
	if (loc)
		loc->__addStandardOutput(QByteArray(data, size));
	else {
		if (GetPyOptions() && GetPyOptions()->pyIOOperation())
			GetPyOptions()->pyIOOperation()->__addStandardOutput(QByteArray(data, size));
	}
	Py_END_ALLOW_THREADS

		if (loc) loc->__stopCodeIfNeeded();
	return PyLong_FromSize_t(size);
}

static PyObject* Stderr_write(PyObject *self, PyObject* args)
{
	Q_UNUSED(self)
		char* data;
	if (!PyArg_ParseTuple(args, "s", &data))
		return 0;

	PyIOOperation * loc = getCurrentPyIOOperation();
	int size = 0;
	Py_BEGIN_ALLOW_THREADS
		size = (int)strlen(data);
	if (loc)
		loc->__addStandardError(QByteArray(data, size));
	else {
		if (GetPyOptions() && GetPyOptions()->pyIOOperation())
			GetPyOptions()->pyIOOperation()->__addStandardError(QByteArray(data, size));
	}
	Py_END_ALLOW_THREADS

		if (loc) loc->__stopCodeIfNeeded();
	return PyLong_FromSize_t(size);
}

static PyObject* Stdin_readline(PyObject *self, PyObject* args)
{
	Q_UNUSED(self)
		Q_UNUSED(args)
		QByteArray data;
	PyIOOperation * loc = getCurrentPyIOOperation();
	Py_BEGIN_ALLOW_THREADS
		if (loc)
			data = loc->__readinput();
		else {
			if (GetPyOptions() && GetPyOptions()->pyIOOperation())
				data = GetPyOptions()->pyIOOperation()->__readinput();
		}
		Py_END_ALLOW_THREADS

			if (loc) loc->__stopCodeIfNeeded();
		PyErr_Clear();
		return PyUnicode_FromString(data.data());
}

static PyMethodDef Redirect_methods[] =
{
	{ "out_write", Stdout_write, METH_VARARGS, "sys.stdout.write" },
	{ "err_write", Stderr_write, METH_VARARGS, "sys.stderr.write" },
	{ "in_readline", Stdin_readline, METH_VARARGS, "sys.stdin.readline" },
	{ 0, 0, 0, 0 } // sentinel
};

static struct PyModuleDef Redirect_module = {
	PyModuleDef_HEAD_INIT,
	"redirect",   /* name of module */
	NULL, /* module documentation, may be NULL */
	-1,       /* size of per-interpreter state of the module,
			  or -1 if the module keeps state in global variables. */
	Redirect_methods
};

// Export the "redirect" module that defines the redirection functions for standard output, error and input
static PyObject* PyInit_redirect()
{
	static QBasicAtomicInt reg = Q_BASIC_ATOMIC_INITIALIZER(0);
	static PyObject* redirect = NULL;
	if (reg.loadAcquire())
		return redirect;
	else
	{
		reg.storeRelease(1);
		redirect = PyModule_Create(&Redirect_module);
		return redirect;
	}
}


static bool import_numpy_internal()
{
#ifdef _MSC_VER
	//on Windows, use the standard approach that works most of the time
	if (_import_array() < 0) {
		PyErr_Print();
		return false;
	}
	return true;
#else
	//on Linux, PyCapsule_CheckExact failed for no valid reason.
	//we do the same as _import_array() but without PyCapsule_CheckExact.
	printf("Initialize numpy...\n");
	int numpy = PyRun_SimpleString("import numpy");
	if (numpy == 0)
		printf("numpy module imported\n");
	else {
		printf("Error while importing numpy");
		return false;
	}
	//
	PyObject *_numpy = PyImport_ImportModule("numpy.core.multiarray");
	if (!_numpy) {
		printf("error, NULL module numpy.core.multiarray\n");
		return false;
	}
	PyObject * c_api = PyObject_GetAttrString(_numpy, "_ARRAY_API");
	Py_DECREF(_numpy);
	if (c_api == NULL) {
		printf("_ARRAY_API not found\n");
		return false;
	}
	//PyObject* name= PyObject_Bytes(Py_TYPE(c_api));
	//printf("_ARRAY_API type: %s\n",Py_TYPE(c_api)->tp_name);
	/*if (!PyCapsule_CheckExact(c_api)) {
	printf("_ARRAY_API is not PyCapsule object");
	PyErr_SetString(PyExc_RuntimeError, "_ARRAY_API is not PyCapsule object");
	Py_DECREF(c_api);
	return ;
	}*/
	PyArray_API = (void **)PyCapsule_GetPointer(c_api, NULL);
	printf("numpy properly imported\n");
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
		:pythonInit(false)
	{
		QString python;
		QByteArray python_path;
		//QStringList additional_lib_paths;
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
		/* UNIX-style OS. ------------------------------------------- */
		//try to find a Python3 installation and use it
		{
#ifdef VIP_PYTHONHOME
			//use Python installation defined at compilation
			python_path = VIP_PYTHONHOME;
			printf("Use Python installation at %s\n", python_path.data());
			/*void * tmp =*/ dlopen(VIP_PYTHONDYNLIB, RTLD_GLOBAL | RTLD_NOW);
#else
			printf("Try to find Python installation...\n");
			QProcess p;
			p.start("python3 -c \"import os;print(os.path.dirname(os.__file__))\"");
			bool started = p.waitForStarted();
			if (!started)
			{
				p.start("./python3 -c \"import os;print(os.path.dirname(os.__file__))\"");
				started = p.waitForStarted();
				if (!started)
				{
					printf("Error with command 'python3', try with 'python'...");
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

			//the expected python version
			QString py_version = QString::number(PY_MAJOR_VERSION) + "." + QString::number(PY_MINOR_VERSION);
			QString found_version; //the found one
			if (started && finished)
			{
				QByteArray path = p.readAllStandardOutput();
				path.replace("'", "");
				path.replace("\n", "");
				printf("Python command output: '%s'\n", path.data());

				if (QDir(path).exists())
				{
					python_path = path;
					printf("Found Python in '%s'\n", path.data());
				}

				//retrieve lib paths
				//p.start(python + " -c \"import sys;print('\\n'.join(sys.path))\"");
				//p.waitForStarted();
				//p.waitForFinished();
				//QString out = p.readAllStandardOutput() + p.readAllStandardError();;
				//additional_lib_paths = out.split("\n",QString::SkipEmptyParts);

				//get the version and load the library
				if (path.size() > 3)
				{
#ifdef VIP_PYTHON_SHARED_LIB
					printf("Load library %s...\n", VIP_PYTHON_SHARED_LIB);
					void* tmp = dlopen(VIP_PYTHON_SHARED_LIB, RTLD_GLOBAL | RTLD_NOW);
#endif
					found_version = path;
					int start = found_version.lastIndexOf("/");
					if (start >= 0)
						found_version = found_version.mid(start+1);
					found_version.replace("python", "");
					/* found_version = path.mid(path.size() - 3);
					if (found_version == py_version)
					{
						QString libPython = "/usr/local/lib/libpython" + found_version + "m.so.1.0";
						printf("Try %s ...\n", libPython.toLatin1().data());
						void * tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
						if (!tmp)
						{
							libPython = "/usr/local/lib/libpython" + found_version + "m.so";
							printf("Try %s ...\n", libPython.toLatin1().data());
							tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
							if (!tmp)
							{
								libPython = path + "/../../bin/libpython" + found_version + "m.so.1.0";
								printf("Try %s ...\n", libPython.toLatin1().data());
								tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
								if (!tmp)
								{
									libPython = path + "/../../bin/libpython" + found_version + "m.so";
									printf("Try %s ...\n", libPython.toLatin1().data());
									tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
								}
							}

						}
						if (tmp)
							printf("load '%s'\n", libPython.toLatin1().data());
						if (tmp == NULL) {
							printf("Cannot find python shared library'\n");
						}
					}*/

				}
			}
			if (py_version != found_version)
			{
				printf("No valid Python installation found (got %s, expected %s), use the local one\n", found_version.toLatin1().data(), py_version.toLatin1().data());
				//no valid python 3 installation found: use the local one
				QFileInfo info(qApp->arguments()[0]);
				QString dirname = info.canonicalPath();
				if (dirname.endsWith("/")) dirname = dirname.mid(0, dirname.size() - 1);
				QString libPython = dirname + "/libpython" + py_version + "m.so";
				printf("load '%s'\n", libPython.toLatin1().data());
				void * tmp = dlopen(libPython.toLatin1().data(), RTLD_GLOBAL | RTLD_NOW);
				if (tmp == NULL) {
					fprintf(stderr, "%s\n", dlerror());
				}
				QString ver = py_version;
				ver.remove(".");
				python_path = (dirname + "/python" + ver).toLatin1();
			}
#endif
		}

#endif
		printf("python path: %s\n",python_path.data());fflush(stdout);
		//init Python
		if (python_path.size())
		{
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
			setenv("PYTHONPATH", python_path.data(), 1);
			setenv("PYTHONHOME", python_path.data(), 1);

			/*size_t size = python_path.size();
			wchar_t* home = Py_DecodeLocale(python_path.data(), &size);
			Py_SetPythonHome(home);
			if(home)
			PyMem_RawFree(home);*/
			printf("Set Python path to %s\n", python_path.data());
#endif
		}
		else
		{
			char * env = getenv("PYTHONHOME");
#ifdef VIP_PYTHONHOME
			if(!env)
				env = VIP_PYTHONHOME;
#endif
printf("python env: %s\n",env);fflush(stdout);
			if(env) {
				python_path = env;
				std::string str = env;
				std::wstring ws(str.begin(),str.end());
				Py_SetPythonHome((wchar_t*)ws.c_str());
				Py_SetPath(ws.c_str());
			}
			else {
				QString miniconda = QFileInfo(qApp->arguments()[0]).canonicalPath() + "/miniconda";
				if (QFileInfo(miniconda).exists()) {
					python_path = miniconda.toLatin1();
					printf("found miniconda at %s\n", python_path.data());
					local_pip = miniconda + "/Scripts/pip";

					// For some miniconda version (and specific numpy version), we need to manually import mkl dll (?)
					/* static QLibrary lib(miniconda + "/Library/bin/mkl_rt.2.dll");
					if (!lib.isLoaded()) {
						bool ok = lib.load();
						printf("Load mkl_rt.2.dll: %i\n", (int)ok);
					}*/

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

		//init threading and acquire lock
		PyEval_InitThreads();
		threadState = PyThreadState_Get();
		interpreterState = threadState->interp;
		//init path
		const wchar_t *argv[2] = { L"",L"./" };
		PySys_SetArgv(2, const_cast<wchar_t**>(argv));
		//import numpy
		PyRun_SimpleString("import sys");
		if (python_path.size())
		{ 
			if (python_path == "./") {
				QString miniconda = QFileInfo(qApp->arguments()[0]).canonicalPath() + "/miniconda";
				if (QFileInfo(miniconda).exists()) {
					local_pip = miniconda + "/Scripts/pip";

					python_path = QFileInfo(qApp->arguments()[0]).canonicalPath().toLatin1() + "/miniconda/Lib";

					//for miniconda, add path to Library/bin to PATH
					QByteArray PATH = qgetenv("PATH");
					PATH += ";" + miniconda + "/Library/bin";
					qputenv("PATH", PATH);

				}
				else
					python_path = QFileInfo(qApp->arguments()[0]).canonicalPath().toLatin1() + "/Lib";
			}
			else {
				if (python_path.endsWith("miniconda")) {

					//for miniconda, add path to Library/bin to PATH
					QByteArray PATH = qgetenv("PATH");
					PATH += ";" + python_path + "/Library/bin";
					qputenv("PATH", PATH);
					python_path += "/Lib";
				}
			}
			//printf("%s\n", python_path.data());
			//for linux only, we need to add manually the site-packages and lib-dynload folders
			PyRun_SimpleString(("sys.path.append('" + python_path + "/site-packages')").data());
			PyRun_SimpleString(("sys.path.append('" + python_path + "/lib-dynload')").data());
			PyRun_SimpleString(("sys.path.append('" + python_path + "')").data());
			//configure matplot lib thermavip backend
			PyRun_SimpleString(("sys.path.append('" + python_path + "/site-packages/matplotlib/backends')").data());
			//add the Python folder in Thermavip directory
			QByteArray pypath = (QFileInfo(qApp->arguments()[0]).canonicalPath() + "/Python").toLatin1();
			PyRun_SimpleString(("sys.path.append('" + pypath + "')").data());
		}
		else
		{
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

		printf("Initialize numpy with python %s...\n", python_path.data());
		printf("env PATH: %s\n", qgetenv("PATH").data());
		//import and configure numpy
		import_numpy_internal();
		printf("Done\n");

		//import and configure matplotlib
		/*PyRun_SimpleString(
			"import matplotlib; matplotlib.use('module://backend_thermavipagg')"
		);*/

		//register standard from/to python converters
		registerToPythonConverter(stdToPython);
		registerToVariantConverter(stdToVariant);

		PyObject* __main__ = PyImport_ImportModule("__main__");
		PyObject *globals = PyModule_GetDict(__main__);
		Py_DECREF(__main__);

		PyObject * redirect = PyInit_redirect();
		PyDict_SetItemString(globals, "redirect", redirect);
		PyObject * thermavip = PyInit_thermavip();
		PyDict_SetItemString(globals, "internal", thermavip);
		int i = PyRun_SimpleString("import builtins;builtins.internal = internal");
		i = PyRun_SimpleString(("import sys; sys.path.append('" + QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python')").toLatin1().data());
		i = PyRun_SimpleString(
			"class _Redirect:\n"
			"    def fileno(self): return 0\n"
			"    def clear(self): pass\n"
			"    def flush(self): pass\n"
			"    def flush(self): return 0\n");



		i = PyRun_SimpleString("import sys");
		i = PyRun_SimpleString("sys.stdout = _Redirect()");
		i = PyRun_SimpleString("sys.stderr = _Redirect()");
		i = PyRun_SimpleString("sys.stdin = _Redirect();sys.stdin.encoding='cp1252';sys.stdin.errors='strict';");

		i = PyRun_SimpleString(
			"sys.stdout.write = redirect.out_write;"
			"sys.stderr.write = redirect.err_write;"
			"sys.stdin.readline = redirect.in_readline;"
		);
		i = PyRun_SimpleString("globals()['_vip_Process']=1");

		if (i != 0)
		{
			printf("Init Python: an error occured\n");
		}
		else {

			__traceback = PyImport_ImportModule("traceback");
		}

		//release lock
		PyEval_SaveThread();
		pythonInit = true;
		printf("Python initialized\n");
	}

public:
	//global PyInterpreterState, used to create PyThreadState objects for each thread
	PyInterpreterState* interpreterState;
	PyThreadState *threadState;

	~PythonInit()
	{
		pythonInit = false;
		__python_closed = true;

		printf("Stop Python...\n");

		//stop all PyLocal objects
		QList<PyLocal*> locals = PyLocal::instances();
		for (int i = 0; i < locals.size(); ++i)
		{
			locals[i]->stop(true);
		}

		//acquire GIL before Py_Finalize() (or crash)
		bool locked = PyGILState_Check();
		if (!locked && PyGILState_GetThisThreadState())
			PyEval_RestoreThread(PyGILState_GetThisThreadState());

		Py_Finalize();
		printf("Python stopped\n");
	}

	static PythonInit * instance()
	{
		static PythonInit * inst = new PythonInit();
		return inst;
	}

};



// Global Interpreter Locker mechanism
GIL_Locker::GIL_Locker()
	: state(PyGILState_Check())
{
	//create the PyThreadState for this thread if it does not exist
	if (!PyGILState_GetThisThreadState())
		PyThreadState_New(PythonInit::instance()->interpreterState);

	//lock the thread if not already locked
	if (!state) PyEval_RestoreThread(PyGILState_GetThisThreadState());
}

GIL_Locker::~GIL_Locker()
{
	//release GIL if previously locked
	if (!state) PyEval_SaveThread();
}







template< class OUTTYPE>
struct SimpleCastTransform
{
	template< class INTYPE> OUTTYPE operator()(const INTYPE & v) const { return static_cast<OUTTYPE>(v); }
};

struct PyHandle : public VipNDArrayHandle
{
	PyObject* array;
	int type;

	PyHandle()
		: VipNDArrayHandle(), array(NULL), type(0)
	{
		initPython();
	}

	PyHandle(PyObject * obj)
		:array(NULL), type(0)
	{
		initPython();
		GIL_Locker lock;
		if (obj && PyArray_Check((PyObject*)obj))
		{
			if (!PyArray_ISCONTIGUOUS(obj))
			{
				//copy the non contiguous array to flatten it
				array = PyArray_NewCopy((PyArrayObject*)obj, NPY_CORDER);
				Py_DECREF(obj);
				obj = array;
			}

			PyArrayObject * ar = (PyArrayObject*)(obj);
			int ndims = PyArray_NDIM(ar);
			if (ndims)
			{
				shape.resize(ndims);
				npy_intp * sh = PyArray_SHAPE(ar);
				std::copy(sh, sh + ndims, shape.data());
			}

			opaque = PyArray_DATA(ar);
			//double * ptr = static_cast<double*>(opaque);
			type = numpyToQt(PyArray_TYPE(ar));
			array = obj;
			size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides);
		}
	}

	PyHandle(const PyHandle & other)
		:array(NULL), type(0)
	{
		initPython();
		shape = other.shape;
		strides = other.strides;
		size = other.size;
		type = other.type;
		if (other.array)
		{
			GIL_Locker locker;
			array = PyArray_NewCopy((PyArrayObject*)other.array, NPY_CORDER);
			opaque = PyArray_DATA(array);
		}
	}

	virtual ~PyHandle()
	{
		if (array)
		{
			GIL_Locker lock;
			Py_DECREF(array);
		}
	}

	virtual PyHandle* copy() const {
		return new PyHandle(*this);
	}

	virtual void * dataPointer(const VipNDArrayShape & pos) const {
		return pos.isEmpty() ? (uchar*)opaque : (uchar*)opaque + vipFlatOffset<false>(strides, pos) * QMetaType(type).sizeOf();
	}

	virtual void * opaqueForPos(void * , const VipNDArrayShape & pos) const {
		return  (uchar*)opaque + vipFlatOffset<false>(strides, pos) * QMetaType(type).sizeOf();
	}

	virtual bool isDense() const { return true; }

	virtual int handleType() const { return Standard; }

	virtual bool reshape(const VipNDArrayShape &) { return false; }

	virtual bool realloc(const VipNDArrayShape & new_shape)
	{
		if (array)
		{
			GIL_Locker lock;
			Py_DECREF(array);
		}

		QVector<npy_intp> sh(new_shape.size());
		std::copy(new_shape.begin(), new_shape.end(), sh.begin());

		if (type == 0)
			type = QMetaType::Double;

		GIL_Locker locker;
		int nd = sh.size();
		npy_intp  *dims = sh.data();
		int numpy_type = qtToNumpy(type);
		PyObject * obj = PyArray_SimpleNew(nd, dims, numpy_type);
		if (obj)
		{
			array = (obj);
			opaque = PyArray_DATA(array);
			shape = new_shape;
			size = vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides);
		}
		return true;
	}

	virtual bool resize(const VipNDArrayShape &, const VipNDArrayShape &, VipNDArrayHandle *, Vip::InterpolationType, const VipNDArrayShape &, const VipNDArrayShape &) const { return false; }

	virtual const char* dataName() const { return QMetaType::typeName(type); }
	virtual int dataSize() const { return QMetaType(type).sizeOf(); }
	virtual int dataType() const { return type; }

	virtual bool canExport(int data_type) const { return data_type == dataType(); }
	virtual bool canImport(int) const { return false; }

	virtual bool exportData(const VipNDArrayShape & this_start, const VipNDArrayShape & this_shape, VipNDArrayHandle * dst, const VipNDArrayShape & dst_start, const VipNDArrayShape & dst_shape) const
	{
		if (!dst->opaque || !opaque)
			return false;

		switch (dataType())
		{
		case QMetaType::Bool:  return vipArrayTransform(static_cast<const bool*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<bool*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<bool>());
		case QMetaType::Char:  return vipArrayTransform(static_cast<const quint8*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<quint8*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<qint8>());
		case QMetaType::UChar: return  vipArrayTransform(static_cast<const quint8*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<quint8*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<quint8>());
		case QMetaType::Short:  return vipArrayTransform(static_cast<const qint16*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<qint16*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<qint16>());
		case QMetaType::UShort:  return vipArrayTransform(static_cast<const quint16*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<quint16*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<quint16>());
		case QMetaType::Int:  return vipArrayTransform(static_cast<const qint32*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<qint32*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<qint32>());
		case QMetaType::UInt:  return vipArrayTransform(static_cast<const quint32*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<quint32*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<quint32>());
		case QMetaType::Long:  return vipArrayTransform(static_cast<const long*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<long*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<long>());
		case QMetaType::ULong:  return vipArrayTransform(static_cast<const unsigned long*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<unsigned long*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<unsigned long>());
		case QMetaType::LongLong:  return vipArrayTransform(static_cast<const qint64*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<qint64*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<qint64>());
		case QMetaType::ULongLong:  return vipArrayTransform(static_cast<const quint64*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<quint64*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<quint64>());
		case QMetaType::Float:  return vipArrayTransform(static_cast<const float*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<float*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<float>());
		case QMetaType::Double:  return vipArrayTransform(static_cast<const double*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<double*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<double>());
		}

		if (dataType() == qMetaTypeId<complex_f>())
			return vipArrayTransform(static_cast<const complex_f*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<complex_f*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<complex_f>());
		else if (dataType() == qMetaTypeId<complex_d>())
			return vipArrayTransform(static_cast<const complex_d*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<complex_d*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<complex_d>());
		else if (dataType() == qMetaTypeId<vip_long_double>())
			return vipArrayTransform(static_cast<const vip_long_double*>(opaque) + vipFlatOffset<false>(this_start, strides), this_shape, strides, static_cast<vip_long_double*>(dst->opaque) + vipFlatOffset<false>(dst_start, dst->strides), dst_shape, dst->strides, SimpleCastTransform<vip_long_double>());

		return false;
	}

	virtual bool importData(const VipNDArrayShape &, const VipNDArrayShape &, const VipNDArrayHandle *, const VipNDArrayShape &, const VipNDArrayShape &)
	{
		return false;
	}

	virtual bool fill(const VipNDArrayShape &, const VipNDArrayShape &, const QVariant & value)
	{
		QVariant tmp = value;
		tmp.convert(dataType());
		const void * data = tmp.constData();
		quint8 * this_data = (quint8 *)opaque;
		int data_size = dataSize();

		if (data && data_size && this_data)
		{
			for (int i = 0; i < size; ++i)
			{
				memcpy(this_data, data, data_size);
				this_data += data_size;
			}

			return true;
		}
		else
			return false;
	}

	virtual QVariant toVariant(const VipNDArrayShape & pos) const
	{
		void * data = ((quint8*)opaque) + dataSize() * vipFlatOffset<false>(strides, pos);
		return QVariant(dataType(), data);
	}

	virtual void fromVariant(const VipNDArrayShape & pos, const QVariant & val)
	{
		void * data = ((quint8*)opaque) + dataSize() * vipFlatOffset<false>(strides, pos);
		QMetaType(dataType()).construct(data, val.data());
	}

	virtual QDataStream & ostream(const VipNDArrayShape &, const VipNDArrayShape &, QDataStream & o) const
	{
		return o;
	}

	virtual QDataStream & istream(const VipNDArrayShape &, const VipNDArrayShape &, QDataStream & i)
	{
		i.setStatus(QDataStream::ReadPastEnd);
		return i;
	}

	virtual QTextStream & oTextStream(const VipNDArrayShape &, const VipNDArrayShape &, QTextStream & o, const QString & separator) const
	{
		(void)separator;
		return o;
	}
};



PyNDArray::PyNDArray()
	:VipNDArray()
{}

PyNDArray::PyNDArray(const VipNDArray & other)
	: VipNDArray(other)
{}

PyNDArray::PyNDArray(void * py_array)
	: VipNDArray()
{
	this->setSharedHandle(SharedHandle(new PyHandle((PyObject*)py_array)));
}

PyNDArray::PyNDArray(int type, const VipNDArrayShape & shape)
	: VipNDArray()
{
	PyHandle * h = new PyHandle();
	h->type = type;
	h->realloc(shape);
	this->setSharedHandle(SharedHandle(h));
}

void * PyNDArray::array() const
{
	if (handle())
		return const_cast<PyObject*>(static_cast<const PyHandle*>(handle())->array);
	return NULL;
}

PyNDArray & PyNDArray::operator=(const VipNDArray & other)
{
	static_cast<VipNDArray&>(*this) = other;
	return *this;
}




QVariant PyLocal::evalCode(const CodeObject & code, bool * ok)
{
	if (!code.code)
	{
		if (ok) * ok = false;
		return QVariant::fromValue(PyError());
	}

	GIL_Locker lock;
	PyObject * res = PyEval_EvalCode((PyObject*)code.code, (PyObject*)globalDict(), NULL);
	if (!res)
	{
		if (ok) * ok = false;
		PyError error(true);
		__addStandardError(error.traceback.toLatin1().data());
		return QVariant::fromValue(error);
	}
	else
	{
		QVariant var = pythonToVariant(res);
		if (ok) *ok = true;
		Py_DECREF(res);
		return var;
	}
}

QVariant PyLocal::evalCode(const QString & code, bool * ok)
{
	GIL_Locker lock;
	PyObject *globals = (PyObject *)globalDict();
	PyObject *obj = PyRun_String(code.toLatin1().data(), Py_file_input, globals, globals);
	if (obj)
	{
		QVariant var = pythonToVariant(obj);
		Py_DECREF(obj);
		if (ok) *ok = true;
		return var;
	}
	else
	{
		if (ok) * ok = false;
		PyError error(true);
		__addStandardError(error.traceback.toLatin1().data());
		return QVariant::fromValue(error);
	}
}

QVariant PyLocal::setObject(const QString & name, const QVariant & var, bool *ok)
{
	GIL_Locker lock;
	PyObject * obj_object = (PyObject *)variantToPython(var);
	if (obj_object)
	{
		PyDict_SetItemString((PyObject *)globalDict(), name.toLatin1().data(), obj_object);
		Py_DECREF(obj_object);
		if (ok) *ok = true;
		return QVariant();
	}
	else
	{
		if (ok) *ok = false;
		PyError error(false);
		error.traceback = "Cannot convert object to Python";
		return (QVariant::fromValue(error));
	}
}

QVariant PyLocal::getObject(const QString & name, bool *ok)
{
	GIL_Locker lock;
	PyObject * obj = PyDict_GetItemString((PyObject *)globalDict(), name.toLatin1().data());
	if (obj)
	{
		if (ok) *ok = true;
		return pythonToVariant(obj);
	}
	else
	{
		if (ok) *ok = false;
		PyError error(false);
		error.traceback = "Cannot convert object to Python";
		return (QVariant::fromValue(error));
	}
}

void PyLocal::writeBytesFromProcess()
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


bool PyLocal::handleMagicCommand(const QString& cmd)
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
			vipProcessEvents(NULL, 20);
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



//thread manipulation functions
#include <set>
static std::set<qint64> _stop_threads;
static QMutex _stop_thread_mutex;

static int stopThread(qint64 threadid)
{
	QMutexLocker lock(&_stop_thread_mutex);
	if (_stop_threads.find(threadid) == _stop_threads.end() && threadid > 0)
	{
		_stop_threads.insert(threadid);
		return PyThreadState_SetAsyncExc(threadid, PyExc_SystemExit);
	}
	return 0;
}

static void threadStopped(qint64 threadid)
{
	QMutexLocker lock(&_stop_thread_mutex);
	_stop_threads.erase(threadid);
}


struct PyRunnable;
struct PyRunThread : public QThread
{
	struct RunResult {
		PyLocal::command_type c;
		QVariant res;
	};

	PyLocal * local;
	QList<PyRunnable*> runnables;
	QList<RunResult> results;
	QMutex mutex;
	QWaitCondition cond;
	PyRunnable * current;
	qint64 threadId;
	int id;

	PyRunThread() : local(NULL), current(NULL), threadId(0), id(1) {}


	PyLocal::command_type add(PyRunnable *r);
	void runOneLoop(PyLocal *);

	void setResult(PyLocal::command_type c, const QVariant & v)
	{
		//set the result for given command
		//the result list cannot exceed 20 elements (20 parallel python codes)
		RunResult res;
		res.c = c;
		res.res = v;
		QMutexLocker locker(&mutex);
		results.push_back(res);
		while (results.size() > 20)
			results.pop_front();
	}

	QVariant getResult(PyLocal::command_type c)
	{
		QMutexLocker locker(&mutex);
		for (QList<RunResult>::iterator it = results.begin(); it != results.end(); ++it)
		{
			if (it->c == c)
			{
				QVariant res = it->res;
				results.erase(it);
				return res;
			}
		}
		return QVariant();
	}

	int findIndex(PyLocal::command_type c);

	bool waitForRunnable(PyLocal::command_type c, unsigned long time = ULONG_MAX);

	void run()
	{
		threadId = ::threadId();//(Qt::HANDLE)QThread::currentThreadId();
		if (PyLocal * tmp = local)
		{
			registerPyIOOperation(tmp);
			while (PyLocal * loc = local)
			{
				runOneLoop(loc);
				QThread::msleep(1);
			}
			unregisterPyIOOperation(tmp);
		}
		threadStopped((qint64)threadId);
		threadId = 0;
	}
};

struct PyRunnable
{
	enum Instruction
	{
		EXEC_CODE,
		SEND_OBJECT,
		RETRIEVE_OBJECT
	};

	QString string;
	QVariant object;
	Instruction type;
	PyLocal::command_type id;
	PyRunThread * runThread;

	PyRunnable(PyRunThread * runThread)
		:type(EXEC_CODE), id(0), runThread(runThread) {}

	void run(PyLocal * local)
	{
		GIL_Locker lock;
		PyErr_Clear();

		if (type == EXEC_CODE)
		{
			PyObject *globals = (PyObject *)local->globalDict();
			PyObject *obj = PyRun_String(string.toLatin1().data(), Py_file_input, globals, globals);
			if (obj)
			{
				runThread->setResult(id, pythonToVariant(obj));
				Py_DECREF(obj);
			}
			else
			{
				PyError error(true);
				local->__addStandardError(error.traceback.toLatin1().data());
				runThread->setResult(id, QVariant::fromValue(error));
			}
		}
		else if (type == SEND_OBJECT)
		{
			PyObject * obj_object = (PyObject *)variantToPython(object);
			if (obj_object)
			{
				PyDict_SetItemString((PyObject *)local->globalDict(), string.toLatin1().data(), obj_object);
				Py_DECREF(obj_object);
			}
			else
			{
				PyError error(false);
				error.traceback = "Cannot convert object to Python";
				runThread->setResult(id, QVariant::fromValue(error));
			}
		}
		else
		{
			PyObject * obj = PyDict_GetItemString((PyObject *)local->globalDict(), string.toLatin1().data());
			if (obj)
				runThread->setResult(id, pythonToVariant(obj));
			else
			{
				PyError error(false);
				error.traceback = "Cannot convert object to Python";
				runThread->setResult(id, QVariant::fromValue(error));
			}
		}
	}
};


PyLocal::command_type PyRunThread::add(PyRunnable *r)
{
	if (::threadId() == this->threadId)
	{
		int res = 0;
		{
			QMutexLocker locker(&mutex);
			r->id = id;
			res = id;
			++id;
			if (id == 20)
				id = 1;
			//remove possible result with this new id
			for (int i = 0; i < results.size(); ++i)
				if ((int)results[i].c == id) {
					results.removeAt(i);
					break;
				}
		}
		r->run(this->local);
		return res;
	}
	else
	{
		//add the PyRunnable object and compute its ID
		QMutexLocker locker(&mutex);
		r->id = id;
		runnables << r;
		int res = id;
		++id;
		if (id == 20)
			id = 1;

		//remove possible result with this new id
		for (int i = 0; i < results.size(); ++i)
			if ((int)results[i].c == id) {
				results.removeAt(i);
				break;
			}

		return res;
	}
}

int PyRunThread::findIndex(PyLocal::command_type c)
{
	//find command index
	int index = -1;
	for (int i = 0; i < runnables.size(); ++i)
		if (runnables[i]->id == c)
		{
			index = i;
			break;
		}
	return index;
}

bool PyRunThread::waitForRunnable(PyLocal::command_type c, unsigned long time)
{
	qint64 start = QDateTime::currentMSecsSinceEpoch();

	mutex.lock();
	int index = findIndex(c);

	//wait for it
	while (index >= 0 || (current && current->id == c))
	{
		if (QDateTime::currentMSecsSinceEpoch() - start >= time) {
			mutex.unlock();
			return false;
		}
		if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
			mutex.unlock();
			//We need to check this pointer, as it might be destroyed during vipProcessEvents().
			QPointer< PyRunThread> _this = this;
			vipProcessEvents(NULL, 10); //main thread: process events
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
		index = findIndex(c);
	}
	mutex.unlock();
	return true;
}

/*bool PyRunThread::waitForRunnable(PyLocal::command_type c, unsigned long time )
{
QMutexLocker locker(&mutex);

int index = findIndex(c);


//wait for it
while (index >= 0 || (current && current->id == c))
{
if (!cond.wait(&mutex, time))
return false;
index = findIndex(c);
}

return true;
}*/

void PyRunThread::runOneLoop(PyLocal * loc)
{
	while (runnables.size())
	{
		{
			QMutexLocker locker(&mutex);
			current = runnables.front();
			runnables.pop_front();
		}
		current->run(loc);
		PyRunnable * run = current;
		QMutexLocker locker(&mutex);
		current = NULL;
		delete run;
		cond.wakeAll();
	}
}


class PyLocal::PrivateData
{
public:
	PrivateData() : globals(NULL), wait_for_line(false) {}
	PyObject * globals;

	PyRunThread runThread;
	QList<PyRunnable> runnables;

	//If not null, calls to write() will go to the process
	QPointer<QProcess> writeToProcess;

	QByteArray input;
	QByteArray std_output;
	QByteArray std_error;
	QMutex line_mutex;
	QWaitCondition line_cond;
	bool wait_for_line;
};


static QMutex pylocal_mutex;
static QList<PyLocal*> pylocal_instances;

PyLocal::PyLocal(QObject * parent)
	:PyIOOperation(parent)
{
	//init python
	PythonInit::instance();
	d_data = new PrivateData();

	GIL_Locker lock;
	PyObject * __main__ = PyImport_ImportModule("__main__");
	d_data->globals = PyModule_GetDict(__main__);
	Py_DECREF(__main__);

	QMutexLocker locker(&pylocal_mutex);
	pylocal_instances.append(this);

	connect(&d_data->runThread, SIGNAL(finished()), this, SLOT(emitFinished()), Qt::DirectConnection);
}

QList<PyLocal*> PyLocal::instances()
{
	QMutexLocker locker(&pylocal_mutex);
	return pylocal_instances;
}

PyLocal * PyLocal::instance(qint64 thread_id)
{
	QMutexLocker locker(&pylocal_mutex);
	for (int i = 0; i < pylocal_instances.size(); ++i) {
		if (static_cast<PyRunThread*>(pylocal_instances[i]->thread())->threadId == thread_id)
			return pylocal_instances[i];
	}
	return NULL;
}

void PyLocal::setWriteToProcess(QProcess* p)
{
	d_data->writeToProcess = p;
}
QProcess* PyLocal::writeToProcess() const
{
	return d_data->writeToProcess;
}



void PyLocal::startInteractiveInterpreter()
{
	this->execCode(
		"import sys\n"
		"def _prompt(text=''):\n"
		"  sys.stdout.write(text)\n"
		"  return sys.stdin.readline()\n"
		"\n"
		"import code;code.interact(None,_prompt,globals())"
	);
}

PyLocal::~PyLocal()
{
	QMutexLocker locker(&pylocal_mutex);
	pylocal_instances.removeOne(this);

	stop(true);
	delete d_data;
}

bool PyLocal::isWaitingForInput() const
{
	return d_data->wait_for_line;
}

bool PyLocal::start()
{
	if (__python_closed)
		return false;
	if (!d_data->runThread.local)
	{
		d_data->runThread.local = this;
		d_data->runThread.start();
		emitStarted();
	}
	return true;
}

void PyLocal::stop(bool wait)
{
	if (d_data->runThread.local)
	{

		d_data->runThread.local = NULL;
		int res = d_data->runThread.QThread::wait(1000);

		//first, stop the current Python code being executed (if any)
		if (d_data->runThread.threadId != 0 && !res)
		{
			GIL_Locker locker;
			res = stopThread((qint64)d_data->runThread.threadId);
		}

		//stop the interpreter thread
		d_data->runThread.local = NULL;
		if (wait)
			d_data->runThread.QThread::wait();

	}
}


bool PyLocal::__stopCodeIfNeeded()
{
	if (!d_data->runThread.local)
	{
		//GIL_Locker locker;
		//stopThread((qint64)d_data->runThread.threadId);
		return true;
	}
	return false;
}

bool PyLocal::isRunning() const
{
	return d_data->runThread.threadId != 0;
}

bool PyLocal::isStopping() const
{
	return d_data->runThread.local == NULL;
}

void * PyLocal::globalDict()
{
	return d_data->globals;
}

QThread * PyLocal::thread()
{
	return &d_data->runThread;
}

void PyLocal::__addStandardOutput(const QByteArray & data)
{
	d_data->std_output += data;
	emitReadyReadStandardOutput();
	vipProcessEvents(NULL, 50);
}

void PyLocal::__addStandardError(const QByteArray & data)
{
	d_data->std_error += data;
	emitReadyReadStandardError();
	vipProcessEvents(NULL, 50);
}

QByteArray PyLocal::__readinput()
{

	QMutexLocker lock(&d_data->line_mutex);
	d_data->input.clear();
	d_data->wait_for_line = true;
	while (d_data->input.isEmpty())
	{
		d_data->runThread.runOneLoop(this);
		d_data->line_cond.wait(&d_data->line_mutex, 5);
		if (__stopCodeIfNeeded())
			return QByteArray("exit()\n");
	}
	d_data->wait_for_line = false;
	return d_data->input;
}

QByteArray PyLocal::readAllStandardOutput()
{
	QByteArray out = d_data->std_output;
	d_data->std_output.clear();
	return out;
}

QByteArray PyLocal::readAllStandardError()
{
	QByteArray err = d_data->std_error;
	d_data->std_error.clear();
	return err;
}

qint64 PyLocal::write(const QByteArray & data)
{
	QMutexLocker lock(&d_data->line_mutex);
	if (d_data->writeToProcess) {
		return d_data->writeToProcess->write(data);
	}
	d_data->input = data;
	d_data->line_cond.wakeAll();
	return data.size();
}

PyLocal::command_type PyLocal::execCode(const QString & code)
{
	PyRunnable * r = new PyRunnable(&d_data->runThread);
	r->type = PyRunnable::EXEC_CODE;
	r->string = code;
	return d_data->runThread.add(r);
}

PyLocal::command_type PyLocal::sendObject(const QString & name, const QVariant & obj)
{
	PyRunnable * r = new PyRunnable(&d_data->runThread);
	r->type = PyRunnable::SEND_OBJECT;
	r->string = name;
	r->object = obj;
	return d_data->runThread.add(r);
}

PyLocal::command_type PyLocal::retrieveObject(const QString & name)
{
	PyRunnable * r = new PyRunnable(&d_data->runThread);
	r->type = PyRunnable::RETRIEVE_OBJECT;
	r->string = name;
	return d_data->runThread.add(r);
}

QVariant PyLocal::wait(command_type command, int milli)
{
	bool res = d_data->runThread.waitForRunnable(command, milli > 0 ? (unsigned long)milli : ULONG_MAX);
	if (!res)
	{
		PyError err(false);
		err.traceback = "Timeout";
		return QVariant::fromValue(err);
	}
	return d_data->runThread.getResult(command);
}

bool PyLocal::wait(bool * alive, int msecs)
{
	//wait for a boolean value
	qint64 st = QDateTime::currentMSecsSinceEpoch();
	while (*alive && (msecs < 0 || (QDateTime::currentMSecsSinceEpoch() - st) < msecs)) {

		if (threadId() == d_data->runThread.threadId) {
			//if we are in the run thread, run pending runnable
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


#else //no VIP_ENABLE_PYTHON_LINK

PyLocal::PyLocal(QObject * parent) :PyIOOperation(parent) {}
PyLocal::~PyLocal() {}
void * PyLocal::globalDict() { return NULL; }
QThread * PyLocal::thread() { return NULL; }
QByteArray PyLocal::readAllStandardOutput() { return QByteArray(); }
QByteArray PyLocal::readAllStandardError() { return QByteArray(); }
qint64 PyLocal::write(const QByteArray &) { return 0; }
PyLocal::command_type PyLocal::execCode(const QString &) { return 0; }
PyLocal::command_type PyLocal::sendObject(const QString &, const QVariant & obj) { return 0; }
PyLocal::command_type PyLocal::retrieveObject(const QString &) { return 0; }
QVariant PyLocal::wait(command_type, int) { return QVariant(); }
bool PyLocal::start() { return false; }
void PyLocal::stop(bool) {}
bool PyLocal::isRunning() const { return false; }
bool PyLocal::__stopCodeIfNeeded() { return true; }
void PyLocal::__addStandardOutput(const QByteArray &) {}
void PyLocal::__addStandardError(const QByteArray &) {}
QByteArray PyLocal::__readinput() { return QByteArray(); }
void PyLocal::startInteractiveInterpreter() {}
QVariant PyLocal::evalCode(const CodeObject &, bool *) { return QVariant(); }
QVariant PyLocal::evalCode(const QString &, bool *) { return QVariant(); }
QVariant PyLocal::setObject(const QString &, const QVariant &, bool *) { return QVariant(); }
QVariant PyLocal::getObject(const QString &, bool *ok) { return QVariant(); }

void initPython()
{
}

void uninitPython()
{
}


#endif


CodeObject::CodeObject()
{
#ifdef VIP_ENABLE_PYTHON_LINK
	code = NULL;
#endif
}

CodeObject::CodeObject(void * code)
{
#ifdef VIP_ENABLE_PYTHON_LINK
	this->code = code;
#endif
}

CodeObject::CodeObject(const QString & expr, Type type)
{
#ifdef VIP_ENABLE_PYTHON_LINK
	GIL_Locker lock;
	PyErr_Clear();
	int eval = Py_file_input;
	if (type == Eval)
		eval = Py_eval_input;
	else if (type == Single)
		eval = Py_single_input;
	code = Py_CompileString(expr.toLatin1().data(), "<input>", eval);
	if (code)
		pycode = expr;
#else
	pycode = expr;
#endif
}

CodeObject::CodeObject(const CodeObject & other)
{
#ifdef VIP_ENABLE_PYTHON_LINK
	code = other.code;
	if (code)
	{
		GIL_Locker lock;
		Py_INCREF((PyObject*)(code));
	}
#endif
	pycode = other.pycode;
}

CodeObject::~CodeObject()
{
#ifdef VIP_ENABLE_PYTHON_LINK
	if (code)
	{
		GIL_Locker lock;
		Py_DECREF((PyObject*)(code));
	}
#endif
}

CodeObject & CodeObject::operator=(const CodeObject & other)
{
#ifdef VIP_ENABLE_PYTHON_LINK
	code = other.code;
	if (code)
	{
		GIL_Locker lock;
		Py_INCREF((PyObject*)(code));
	}
#endif
	pycode = other.pycode;
	return *this;
}



void PyIOOperation::startInteractiveInterpreter()
{
	this->execCode("import code;code.interact(None,None,globals())");
}











static QStringList classNames(const QString & filename)
{
	//find all Python classes starting with "Thermavip" in given Python file

	QFile fin(filename);
	fin.open(QFile::ReadOnly);

	QStringList res;
	QString ar = fin.readAll();
	QStringList lines = ar.split("\n");
	for (int i = 0; i < lines.size(); ++i)
	{
		if (lines[i].startsWith("class "))
		{
			QStringList elems = lines[i].split(" ", QString::SkipEmptyParts);
			if (elems.size() > 1 && elems[1].startsWith("Thermavip"))
			{
				QString classname = elems[1];
				int index = classname.indexOf("(");
				if (index >= 0)
					classname = classname.mid(0, index);
				index = classname.indexOf(":");
				if (index >= 0)
					classname = classname.mid(0, index);

				if (classname != "ThermavipPyProcessing") //base class for all processings
					res.append(classname);
			}
		}
	}

	return res;
}



void PyAddProcessingDirectory(const QString & dir, const QString & prefix, bool register_processings)
{
	//Inspect a directory (recursive), find all Python files, find all classes starting with Thermavip in these files, and exec these files 
	//to add the classes in the global dict.

	//keep track of loaded directories for the current global Python interpreter
	QStringList lst = GetPyOptions()->property("_vip_dirs").toStringList();
	if (lst.indexOf(QFileInfo(dir).canonicalFilePath()) >= 0)
		return;
	lst.append(QFileInfo(dir).canonicalFilePath());
	GetPyOptions()->setProperty("_vip_dirs", lst);

	QFileInfoList res = QDir(dir).entryInfoList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files, QDir::Name);
	for (int i = 0; i < res.size(); ++i)
	{
		if (res[i].isDir())
		{
			PyAddProcessingDirectory(res[i].canonicalFilePath(), prefix.size() ? prefix + "/" + res[i].baseName() : res[i].baseName());
		}
		else if (res[i].suffix() == "py")
		{
			QStringList classnames = classNames(res[i].canonicalFilePath());
			//No classes starting with 'Thermavip': do not load the file
			if (classnames.size() == 0)
				continue;

			printf("parsed %s, found %s\n", res[i].canonicalFilePath().toLatin1().data(), classnames.join(", ").toLatin1().data());

			//run the file and extract the class description
			QFile in(res[i].canonicalFilePath());
			in.open(QFile::ReadOnly);
			QString code = in.readAll();
			PyIOOperation::command_type c = GetPyOptions()->execCode(code);
			PyError err = GetPyOptions()->wait(c, 20000).value<PyError>();
			if (!err.isNull())
			{
				VIP_LOG_WARNING("Cannot load Python processing: " + res[i].baseName());
				printf("Python load error: \n%s\n", err.traceback.toLatin1().data());
				continue;
			}


			QString category = prefix.size() ? prefix : "Python";

			for (int j = 0; j < classnames.size(); ++j)
			{
				QString code =
					"s = " + classnames[j] + ".__doc__\n"
					"it = " + classnames[j] + "().displayHint()";
				c = GetPyOptions()->execCode(code);
				err = GetPyOptions()->wait(c, 3000).value<PyError>();
				if (!err.isNull())
				{
					VIP_LOG_WARNING("Cannot load Python processing: " + classnames[j]);
					printf("Python load Python processing: \n%s\n", err.traceback.toLatin1().data());
					continue;
				}
				c = GetPyOptions()->retrieveObject("s");
				QVariant doc = GetPyOptions()->wait(GetPyOptions()->retrieveObject("s"), 3000);
				QVariant input_tr = GetPyOptions()->wait(GetPyOptions()->retrieveObject("it"), 3000);
				if (!input_tr.value<PyError>().isNull())
				{
					VIP_LOG_WARNING("Cannot load Python processing: " + classnames[j]);
					continue;
				}

				//create the VipProcessingObject::Info object corresponding to this Python class and register it
				//so that it will be available in the video and plot player.

				if (register_processings)
				{
					QString cname = classnames[j];
					cname.remove("Thermavip");
					VipProcessingObject::Info info(cname, "", category, QIcon(), qMetaTypeId<PyProcessing*>());
					info.displayHint = (VipProcessingObject::DisplayHint)input_tr.toInt();
					info.init = cname;
					info.description = doc.toString();
					VipProcessingObject::registerAdditionalInfoObject(info);
					VIP_LOG_INFO("Added Python processing " + cname + " in category " + info.category);
					printf("Added Python processing %s in category %s\n", cname.toLatin1().data(), info.category.toLatin1().data());
				}
			}


		}
	}
}








#include "PyProcess.h"

class PyOptions::PrivateData
{
public:
	PrivateData()
		:dirty(true), recurs(false), type(Local), python("python"), workingDirectory("./"), pyIOOperation(NULL), launchCode(PyOptions::InIPythonInterp)
	{
#ifndef VIP_ENABLE_PYTHON_LINK
		type = Distant;
#endif
		startupCode = "import numpy as np";
	}
	bool dirty;
	bool recurs;
	PyType type;
	QString python;
	QString workingDirectory;
	QString startupCode;
	PyIOOperation * pyIOOperation;
	PyLaunchCode launchCode;
};

PyOptions::PyOptions(QObject * parent)
	:PyIOOperation(parent)
{

	d_data = new PrivateData();

	//TEST
	//d_data->type = Distant;
	//d_data->python ="C:/WinPython-64bit-3.3.5.9/python-3.3.5.amd64/python";
}

PyOptions::~PyOptions()
{
	clear();
	delete d_data;
}

void PyOptions::setPyType(PyType type)
{
#ifndef VIP_ENABLE_PYTHON_LINK
	type = Distant;
#endif
	if (type != d_data->type)
	{
		d_data->type = type;
		d_data->dirty = true;
	}
}

PyOptions::PyType PyOptions::pyType() const
{
	return d_data->type;
}

void PyOptions::setPython(const QString & python)
{
	if (QFileInfo(python) != QFileInfo(d_data->python))
	{
		d_data->python = python;
		d_data->python.replace("\\", "/");
		d_data->dirty = true;
	}
}

const QString & PyOptions::python() const
{
	return d_data->python;
}

void PyOptions::setWorkingDirectory(const QString & workingDirectory)
{
	if (QFileInfo(workingDirectory) != QFileInfo(d_data->workingDirectory) && !workingDirectory.isEmpty())
	{
		d_data->workingDirectory = workingDirectory;
		d_data->workingDirectory.replace("\\", "/");
		d_data->dirty = true;
	}
}

const QString & PyOptions::workingDirectory() const
{
	return d_data->workingDirectory;
}

void PyOptions::setStartupCode(const QString & code)
{
	d_data->startupCode = code;
}
QString PyOptions::startupCode() const
{
	return d_data->startupCode;
}

void PyOptions::setLaunchCode(PyLaunchCode l)
{
	d_data->launchCode = l;
}
PyOptions::PyLaunchCode PyOptions::launchCode() const
{
	return d_data->launchCode;
}

void PyOptions::clear()
{
	if (d_data->pyIOOperation)
	{
		delete d_data->pyIOOperation;
		d_data->pyIOOperation = NULL;
	}
}

bool PyOptions::start()
{
	if (!__python_closed)
		return pyIOOperation()->start();
	return false;
}

void PyOptions::stop(bool wait)
{
	if (!__python_closed)
		pyIOOperation()->stop(wait);
}

bool PyOptions::isRunning() const
{
	if (!__python_closed)
		return pyIOOperation()->isRunning();
	return false;
}

bool PyOptions::isWaitingForInput()const
{
	if (!__python_closed)
		return pyIOOperation()->isWaitingForInput();
	return false;
}


struct BoolLocker
{
	bool *value;
	BoolLocker(bool & val) { value = &val; val = true; }
	~BoolLocker() { *value = false; }
};
PyIOOperation * PyOptions::pyIOOperation(bool create_new) const
{
	if (__python_closed)
		return NULL;

	//avoid recursicve call to this function
	if (d_data->recurs)
		return d_data->pyIOOperation;
	BoolLocker locker(d_data->recurs);

	PyOptions * _this = const_cast<PyOptions*>(this);

	if (!d_data->pyIOOperation || d_data->dirty || create_new)
	{

		_this->d_data->dirty = false;
		if (_this->d_data->pyIOOperation)
		{
			//avoid a possible issue when trying to stop a PyLocal on its own thread (usually during a I/O operation)

			if (PyLocal * loc = qobject_cast<PyLocal*>(_this->d_data->pyIOOperation))
			{
				if (loc->thread() == QThread::currentThread())
				{
					//in this, case, do NOT stop the thread (or it will try to wait on itself).
					//just return the current PyLocal and keep the dirty mark.
					_this->d_data->dirty = true;
					return _this->d_data->pyIOOperation;
				}
				else {
					PyProcessingLocker lock;
					//printf("delete pyIOOperation\n");
					delete _this->d_data->pyIOOperation;
					//printf("NULL pyIOOperation\n");
					_this->d_data->pyIOOperation = NULL;
				}

			}
			else {
				PyProcessingLocker lock;
				delete _this->d_data->pyIOOperation;
				_this->d_data->pyIOOperation = NULL;
			}

		}
		PyProcessingLocker lock;
		if (_this->d_data->pyIOOperation)
			delete _this->d_data->pyIOOperation;
		if (d_data->type == Local)
			_this->d_data->pyIOOperation = new PyLocal();
		else {
			_this->d_data->pyIOOperation = new PyProcess(d_data->python);
			_this->setProperty("_vip_dirs", QVariant::fromValue(QStringList())); //clear inspected path to restart PyAddProcessingDirectory
		}

		connect(_this->d_data->pyIOOperation, SIGNAL(readyReadStandardOutput()), this, SLOT(emitReadyReadStandardOutput()), Qt::DirectConnection);
		connect(_this->d_data->pyIOOperation, SIGNAL(readyReadStandardError()), this, SLOT(emitReadyReadStandardError()), Qt::DirectConnection);
		connect(_this->d_data->pyIOOperation, SIGNAL(started()), this, SLOT(emitStarted()), Qt::DirectConnection);
		connect(_this->d_data->pyIOOperation, SIGNAL(finished()), this, SLOT(emitFinished()), Qt::DirectConnection);
		_this->d_data->pyIOOperation->start();
		_this->d_data->pyIOOperation->wait(_this->d_data->pyIOOperation->execCode("import os;os.chdir('" + d_data->workingDirectory + "')"));
		_this->d_data->pyIOOperation->wait(_this->d_data->pyIOOperation->execCode(d_data->startupCode));

		//register all files found in the Python directory
		PyAddProcessingDirectory(vipGetPythonDirectory(), QString(), false);
		QString std_path = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python";
		printf("inspect path %s\n", std_path.toLatin1().data());
		PyAddProcessingDirectory(/*"./Python"*/std_path, QString(), false);
	}

	return _this->d_data->pyIOOperation;
}

QByteArray PyOptions::readAllStandardOutput()
{
	if (!__python_closed)
		return pyIOOperation()->readAllStandardOutput();
	return QByteArray();
}

QByteArray PyOptions::readAllStandardError()
{
	if (!__python_closed)
		return pyIOOperation()->readAllStandardError();
	return QByteArray();
}

qint64 PyOptions::write(const QByteArray & data)
{
	if (!__python_closed)
		return pyIOOperation()->write(data);
	return 0;
}

PyOptions::command_type PyOptions::execCode(const QString & code)
{
	if (!__python_closed)
		return pyIOOperation()->execCode(code);
	return 0;
}

QVariant PyOptions::evalCode(const CodeObject & code, bool * ok)
{
	if (!__python_closed)
		return pyIOOperation()->evalCode(code, ok);
	return QVariant::fromValue(PyError("NULL PyOptions"));
}

PyOptions::command_type PyOptions::sendObject(const QString & name, const QVariant & obj)
{
	if (!__python_closed)
		return pyIOOperation()->sendObject(name, obj);
	return 0;
}

PyOptions::command_type PyOptions::retrieveObject(const QString & name)
{
	if (!__python_closed)
		return pyIOOperation()->retrieveObject(name);
	return 0;
}

QVariant PyOptions::wait(command_type c, int milli)
{
	if (!__python_closed)
		return pyIOOperation()->wait(c, milli);
	return QVariant();
}

bool PyOptions::handleMagicCommand(const QString& cmd)
{
	if (!__python_closed)
		return pyIOOperation()->handleMagicCommand(cmd);
	return false;
}

void PyOptions::printOutput()
{
	std::cout << readAllStandardOutput().data();
	std::cout.flush();
}

void PyOptions::printError()
{
	std::cerr << readAllStandardError().data();
	std::cerr.flush();
}

void PyOptions::startInteractiveInterpreter()
{
	if (!__python_closed)
		pyIOOperation()->startInteractiveInterpreter();
}



PyIOOperation * PyOptions::createNew(QObject * parent)
{
	if (__python_closed)
		return NULL;

	PyIOOperation * res = GetPyOptions()->pyType() == PyOptions::Local ?
		(PyIOOperation*)new PyLocal(parent) :
		(PyIOOperation*)new PyProcess(GetPyOptions()->python(), parent);
	if (!res->start()) {
		delete res;
		return NULL;
	}

	res->wait(res->execCode("import os;os.chdir('" + GetPyOptions()->workingDirectory() + "')"));
	res->wait(res->execCode(GetPyOptions()->startupCode()));
	return res;
}

QMutex * PyOptions::operationMutex()
{
	static QMutex mutex(QMutex::Recursive);
	return &mutex;
}

PyOptions * GetPyOptions()
{
	static PyOptions *inst = new PyOptions();
	return inst;
}









static const QString eval_code =
"def eval_code(code) :\n"
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

static EvalResultType evalCode(const QString & code)
{
	static bool init = false;
	GIL_Locker lock;
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
	printf("%s\n", to_exec.toLatin1().data());

	int r = PyRun_SimpleString(to_exec.toLatin1().data());

	PyObject * __main__ = PyImport_ImportModule("__main__");
	PyObject * globals = PyModule_GetDict(__main__);
	Py_DECREF(__main__);
	PyObject * obj = PyDict_GetItemString(globals, "tmp");
	QVariantList lst = pythonToVariant(obj).value<QVariantList>();
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
		:QEvent(QEvent::MaxUser), alive(v), result(res)
	{
		*alive = true; 
	}
	~_Event() { *alive = false; }
};
struct _EventObject : QObject
{
	virtual bool event(QEvent * evt)
	{
		if (evt->type() == QEvent::MaxUser) {
			//exec string
			*static_cast<_Event*>(evt)->result = evalCode(static_cast<_Event*>(evt)->code);
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
EvalResultType evalCodeMainThread(const QString & code) {

	if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
		return evalCode(code);
	}
	else {
		QSharedPointer<bool> alive(new bool(true));
		QSharedPointer<EvalResultType> result(new EvalResultType());
		_Event * evt = new _Event(alive, result);
		evt->code = code;
		QCoreApplication::instance()->postEvent(object(), evt);
		QPointer<PyLocal> loc = PyLocal::instance(threadId());
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

