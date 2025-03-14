#ifdef _WIN32
#include <Windows.h>
#include <WinUser.h>
#endif

#include "VipPyIPython.h"
#include "VipPyOperation.h"

#include "VipCore.h"
#include "VipLogging.h"

#include <qsharedmemory.h>
#include <qdatetime.h>
#include <qdatastream.h>
#include <qthread.h>
#include <qmutex.h>
#include <qfileinfo.h>
#include <qdir.h>
#include <qtoolbutton.h>
#include <qtoolbar.h>
#include <qapplication.h>
#include <qevent.h>

// Codes to communicate through the shared memory
#define SH_EXEC_FUN "SH_EXEC_FUN     "
#define SH_OBJECT "SH_OBJECT       "
#define SH_ERROR_TRACE "SH_ERROR_TRACE  "
#define SH_SEND_OBJECT "SH_SEND_OBJECT  "
#define SH_EXEC_CODE "SH_EXEC_CODE    "
#define SH_EXEC_LINE "SH_EXEC_LINE    "
#define SH_EXEC_LINE_NO_WAIT "SH_EXEC_LINE_NW "
#define SH_RESTART "SH_RESTART      "
#define SH_RUNNING "SH_RUNNING      "

// Shared memory header
typedef union MemHeader
{
	struct
	{
		int connected;	  // number of connected user (max 2)
		int size;	  // full memory size
		int max_msg_size; // max size of a message
		int offset_read;  // read offset
		int offset_write; // write offset
	};
	char reserved[64];
} MemHeader;

// Integer to (little endian) QByteArray
static QByteArray toBinary(int value)
{
	QByteArray ar;
	{
		QDataStream str(&ar, QIODevice::WriteOnly);
		str.setByteOrder(QDataStream::LittleEndian);
		str << value;
	}
	return ar;
}

// QByteArray to integer
static int readBinary(const QByteArray& ar, int offset = 0)
{
	QDataStream str(ar.mid(offset));
	str.setByteOrder(QDataStream::LittleEndian);
	int value = 0;
	str >> value;
	return value;
}

/**
Shared memory object used to communicate between processes through a very simple message queue system.
*/
class SharedMemory : public QThread
{
	QSharedMemory d_mem;
	MemHeader d_header;
	bool d_main;
	bool d_stop;
	VipPyLocal d_loc;
	QMutex d_mutex;

public:
	SharedMemory(const QString& name, int size, bool is_main = false)
	  : d_mem(name)
	  , d_main(false)
	  , d_stop(false)
	{
		if (!d_mem.attach()) {
			if (!d_mem.create(size)) {
				vip_debug("error: %s\n", d_mem.errorString().toLatin1().data());
				VIP_LOG_ERROR("error: %s\n", d_mem.errorString().toLatin1().data());
				return;
			}
			d_mem.lock();
			// create header
			d_header.connected = 1;
			d_header.size = size;
			d_header.max_msg_size = (size - sizeof(d_header) - 16) / 2;
			d_header.offset_read = sizeof(d_header);
			d_header.offset_write = sizeof(d_header) + 8 + d_header.max_msg_size;
			memcpy(d_mem.data(), &d_header, sizeof(d_header));
			d_mem.unlock();
			d_main = true;
		}
		else {

			// read an existing shared memory
			d_mem.lock();
			memcpy(&d_header, d_mem.data(), sizeof(d_header));
			if (false) { //++d_header.connected > 2) {
				d_mem.unlock();
				d_mem.detach();
				vip_debug("error: shared memory already in use");
				VIP_LOG_ERROR("error: shared memory already in use");
				return;
			}
			memcpy(d_mem.data(), &d_header, sizeof(d_header));
			// invert read and write offset if not main
			if (!is_main)
				std::swap(d_header.offset_read, d_header.offset_write);
			d_mem.unlock();
			d_main = is_main;
		}

		// start thread
		d_loc.start();
		this->start();
	}

	~SharedMemory()
	{
		if (d_mem.isAttached()) {
			// write new connected number
			--d_header.connected;
			if (!d_main)
				std::swap(d_header.offset_read, d_header.offset_write);
			d_mem.lock();
			memcpy(d_mem.data(), &d_header.connected, sizeof(int));
			d_mem.unlock();
		}

		d_stop = true;
		d_loc.stop();
		wait();
	}

	void acquire() { d_mutex.lock(); }
	void release() { d_mutex.unlock(); }

	QByteArray flags()
	{
		d_mem.lock();
		QByteArray res(44, 0);
		memcpy(res.data(), (char*)d_mem.data() + 20, 44);
		d_mem.unlock();
		return res;
	}

	QString name() const { return d_mem.nativeKey(); }

	bool isValid() const { return d_mem.isAttached(); }

	bool waitForEmptyWrite(qint64 until = -1)
	{
		// wait for the write area to be empty
		while (true) {
			d_mem.lock();
			int s = 0;
			memcpy(&s, (char*)d_mem.data() + d_header.offset_write, 4);
			d_mem.unlock();
			if (s) {
				QThread::msleep(2);
				if (until != -1 && QDateTime::currentMSecsSinceEpoch() >= until)
					return false;
			}
			else
				break;
		}
		return true;
	}

	bool writeAscii(const char* data, int milli_timmeout = -1) { return write(data, (int)strlen(data), milli_timmeout); }
	bool write(const char* data, int size, int milli_timeout = -1)
	{
		if (!d_mem.isAttached())
			return false;

		// write a message
		// first 4 bytes is the message size
		// next 4 bytes is a flag telling if the message is finished (for message length longer than the write buffer)

		qint64 start = QDateTime::currentMSecsSinceEpoch();
		qint64 until = milli_timeout == -1 ? -1 : start + milli_timeout;

		do {
			// write for write area to be available
			if (!waitForEmptyWrite(until))
				return false;

			int flag = size > d_header.max_msg_size;
			int s = flag ? d_header.max_msg_size : size;
			// write size and flag
			d_mem.lock();
			memcpy((char*)d_mem.data() + d_header.offset_write, &s, sizeof(s));
			memcpy((char*)d_mem.data() + d_header.offset_write + 4, &flag, sizeof(flag));
			memcpy((char*)d_mem.data() + d_header.offset_write + 8, data, s);
			d_mem.unlock();
			size -= s;
			data += s;

			if (size > 0 && until != -1 && QDateTime::currentMSecsSinceEpoch() >= until) {
				// reset write area
				flag = s = 0;
				d_mem.lock();
				memcpy((char*)d_mem.data() + d_header.offset_write, &s, sizeof(s));
				memcpy((char*)d_mem.data() + d_header.offset_write + 4, &s, sizeof(flag));
				d_mem.unlock();
				return false;
			}

		} while (size > 0);

		return true;
	}

	bool read(QByteArray& data, int milli_timeout = -1)
	{
		if (!d_mem.isAttached())
			return false;

		// read message
		qint64 start = QDateTime::currentMSecsSinceEpoch();
		qint64 until = milli_timeout == -1 ? -1 : start + milli_timeout;

		data.clear();
		while (true) {
			int flag = 0, s = 0;
			d_mem.lock();
			memcpy(&s, (char*)d_mem.data() + d_header.offset_read, 4);
			memcpy(&flag, (char*)d_mem.data() + d_header.offset_read + 4, 4);
			d_mem.unlock();
			if (!s) {
				if (until != -1 && QDateTime::currentMSecsSinceEpoch() >= until)
					return false;
				QThread::msleep(15);
				continue;
			}

			int prev = data.size();
			data.resize(data.size() + s);
			d_mem.lock();
			memcpy(data.data() + prev, (char*)d_mem.data() + d_header.offset_read + 8, s);
			// set read area to 0
			int unused = 0;
			memcpy((char*)d_mem.data() + d_header.offset_read, &unused, 4);
			memcpy((char*)d_mem.data() + d_header.offset_read + 4, &unused, 4);
			d_mem.unlock();
			if (flag == 0)
				break;
		}
		return true;
	}

	/**
	 * Send python object called 'name' through the shared memory
	 */
	bool writeObject(const QString& name, qint64 timeout = -1, QString* error = nullptr)
	{
		if (error)
			error->clear();
		QString code = "import pickle\n"
			       "import struct\n"
			       "__res = b'" SH_OBJECT "' +struct.pack('i',len('" +
			       name + "')) + b'" + name + "' + pickle.dumps(" + name + ")";

		VipPyError err = d_loc.execCode(code).value().value<VipPyError>();
		if (!err.isNull()) {
			if (error)
				*error = err.traceback;
			vip_debug("%s\n", err.traceback.toLatin1().data());
			VIP_LOG_ERROR("%s\n", err.traceback.toLatin1().data());
			return false;
		}

		// send result
		QVariant v = d_loc.retrieveObject("__res").value();
		QByteArray tmp = v.toByteArray();
		if (!this->write(tmp.data(), tmp.size(), timeout)) {
			if (error)
				*error = "Error writing to shared memory";
			return false;
		}
		return true;
	}
	/**
	 * Send python object called 'name' through the shared memory
	 */
	bool writeObject(const QString& name, const QVariant& v, qint64 timeout = -1, QString* error = nullptr)
	{
		if (error)
			error->clear();

		VipPyError err = d_loc.sendObject(name, v).value().value<VipPyError>();
		if (!err.isNull()) {
			if (error)
				*error = err.traceback;
			return false;
		}

		QString code = "import pickle\n"
			       "import struct\n"
			       "__res = b'" SH_OBJECT "' +struct.pack('i',len('" +
			       name + "')) + b'" + name + "' + pickle.dumps(" + name + ")";

		err = d_loc.execCode(code).value().value<VipPyError>();
		if (!err.isNull()) {
			if (error)
				*error = err.traceback;
			vip_debug("%s\n", err.traceback.toLatin1().data());
			VIP_LOG_ERROR("%s\n", err.traceback.toLatin1().data());
			return false;
		}

		// send result
		QVariant _v = d_loc.retrieveObject("__res").value();
		QByteArray tmp = _v.toByteArray();
		if (!this->write(tmp.data(), tmp.size(), timeout)) {
			if (error)
				*error = "Error writing to shared memory";
			return false;
		}
		return true;
	}
	bool writeSendObject(const QString& name, qint64 timeout = -1, QString* error = nullptr)
	{
		QByteArray _name = name.toLatin1();
		QByteArray ar = SH_SEND_OBJECT + toBinary(_name.size()) + _name;
		if (!this->write(ar.data(), ar.size(), timeout)) {
			if (error)
				*error = "Error writing to shared memory";
			return false;
		}
		return true;
	}
	/**
	 * Send error message through the shared memory
	 */
	bool writeError(const QString& err, qint64 timeout = -1)
	{
		QByteArray _err = err.toLatin1();
		QByteArray ar = SH_ERROR_TRACE + toBinary(_err.size()) + _err;
		return this->write(ar.data(), ar.size(), timeout);
	}
	bool writeExecCode(const QString& code, qint64 timeout = -1)
	{
		QByteArray _code = code.toLatin1();
		QByteArray ar = SH_EXEC_CODE + toBinary(_code.size()) + _code;
		return this->write(ar.data(), ar.size(), timeout);
	}

	bool writeExecLine(const QString& code, qint64 timeout = -1)
	{
		QByteArray _code = code.toLatin1();
		QByteArray ar = SH_EXEC_LINE + toBinary(_code.size()) + _code;
		return this->write(ar.data(), ar.size(), timeout);
	}

	bool writeExecLineNoWait(const QString& code, qint64 timeout = -1)
	{
		QByteArray _code = code.toLatin1();
		QByteArray ar = SH_EXEC_LINE_NO_WAIT + toBinary(_code.size()) + _code;
		return this->write(ar.data(), ar.size(), timeout);
	}

	bool writeRestart(qint64 timeout = -1)
	{
		QByteArray ar = SH_RESTART;
		return this->write(ar.data(), ar.size(), timeout);
	}

	bool writeIsRunningCode(qint64 timeout = -1)
	{
		QByteArray ar = SH_RUNNING;
		return this->write(ar.data(), ar.size(), timeout);
	}

	bool readObject(QByteArray ar, QVariant& v, QString* error = nullptr)
	{
		if (error)
			error->clear();

		if (!ar.startsWith(SH_OBJECT)) {
			if (error)
				*error = "wrong start code";
			return false;
		}
		ar = ar.mid(strlen(SH_OBJECT));
		int len = readBinary(ar);
		ar = ar.mid(4);
		QByteArray name = ar.mid(0, len);
		ar = ar.mid(len);

		// load object with pickle
		VipPyError err = d_loc.sendObject("__ar", QVariant::fromValue(ar)).value().value<VipPyError>();
		if (!err.isNull()) {
			if (error)
				*error = err.traceback;
			return false;
		}

		QString code = "import pickle\n"
			       "__res =  pickle.loads(__ar)";
		err = d_loc.execCode(code).value().value<VipPyError>();
		if (!err.isNull()) {
			if (error)
				*error = err.traceback;
			return false;
		}

		v = d_loc.retrieveObject("__res").value();
		if (!v.value<VipPyError>().isNull()) {
			if (error)
				*error = v.value<VipPyError>().traceback;
			return false;
		}
		return true;
	}

	bool readError(QByteArray ar, QString& error)
	{
		if (!ar.startsWith(SH_ERROR_TRACE))
			return false;
		ar = ar.mid(strlen(SH_ERROR_TRACE));
		/* int len =*/readBinary(ar);
		ar = ar.mid(4);
		error = ar;
		return true;
	}

protected:
	virtual void run()
	{
		qint64 timeout = 100;

		while (!d_stop) {

			QByteArray ar;
			// lock the reading part to avoid collision with other commands
			acquire();
			bool r = this->read(ar, 5);
			release();
			if (ar.isEmpty()) {
				QThread::msleep(5);
				continue;
			}

			if (r && ar.size()) {

				// interpret read value

				if (ar.startsWith(SH_EXEC_FUN)) {
					// execute internal python function
					ar = ar.mid(strlen(SH_EXEC_FUN));
					QDataStream str(ar);
					str.setByteOrder(QDataStream::LittleEndian);
					int s1 = 0, s2 = 0, s3 = 0;
					str >> s1 >> s2 >> s3; // read function name len, tuple args len, dict args len
					if (!s1 || !s2 || !s3) {
						continue;
					}
					// send pickle versions of variables. name is already the ascii function name.
					QByteArray name(s1, 0), targs(s2, 0), dargs(s3, 0);
					str.readRawData(name.data(), name.size());
					str.readRawData(targs.data(), targs.size());
					str.readRawData(dargs.data(), dargs.size());
					d_loc.sendObject("__targs", targs);
					d_loc.sendObject("__dargs", dargs);

					QString code = "import pickle\n"
						       "import struct\n"
						       "__targs = pickle.loads(__targs)\n"
						       "__dargs = pickle.loads(__dargs)\n"
						       //"__res = " + name + "(*__targs, **__dargs)\n";
						       "__res = builtins.internal.call_internal_func('" +
						       name + "', *__targs, **__dargs)";
					VipPyError err = d_loc.execCode(code).value().value<VipPyError>();
					if (!err.isNull()) {
						vip_debug("%s\n", err.traceback.toLatin1().data());
						VIP_LOG_ERROR("%s\n", err.traceback.toLatin1().data());
						writeError(err.traceback, timeout);
						continue;
					}
					QString error;
					if (!writeObject("__res", timeout, &error)) {
						writeError(error, timeout);
					}
				}
				else {
					// vip_debug("%s\n", ar.data());
				}
			}
		}
	}
};

QString vipPyGlobalSharedMemoryName()
{
	// Create the global shared memory 'Thermavip' so that external Python process can communicate with thermavip
	static QString str = VipIPythonShellProcess::findNextMemoryName();
	static SharedMemory _global_mem(str, 50000000, true);
	return str;
}

static int _fontSize{ 10 };
static QString _style;

void vipSetIPythonFontSize(int size)
{
	_fontSize = size;
}
int vipIPythonFontSize()
{
	return _fontSize;
}

void vipSetIPythonStyle(const QString& style)
{
	_style = style;
}
QString vipIPythonStyle()
{
	return _style;
}

class VipIPythonShellProcess::PrivateData
{
public:
	QString sharedMemoryName;
	SharedMemory* mem;
	QString lastError;
	qint64 pid;
	int timeout;
	bool embedded;
};

VipIPythonShellProcess::VipIPythonShellProcess(QObject* parent)
  : QProcess(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->mem = nullptr;
	d_data->timeout = 3000;
	d_data->embedded = false;
	d_data->pid = 0;
}

VipIPythonShellProcess::~VipIPythonShellProcess()
{
	if (state() == Running) {
		terminate();
		if (!waitForFinished(1000)) {
			kill();
			waitForFinished(1000);
		}
	}
	if (d_data->mem)
		delete d_data->mem;
}

void VipIPythonShellProcess::setTimeout(int milli_timeout)
{
	d_data->timeout = milli_timeout;
}
int VipIPythonShellProcess::timeout() const
{
	return d_data->timeout;
}

void VipIPythonShellProcess::setEmbedded(bool enable)
{
	d_data->embedded = enable;
}
bool VipIPythonShellProcess::embedded() const
{
	return d_data->embedded;
}

qint64 VipIPythonShellProcess::start(int font_size, const QString& _style, const QString& _shared_memory_name)
{
	d_data->lastError.clear();

	QString style = _style;
	if (style.isEmpty())
		style = vipIPythonStyle();
	if (style.isEmpty())
		style = "default";
	if (font_size < 0)
		font_size = vipIPythonFontSize();

	// kill running process
	if (state() == Running) {
		terminate();
		if (!waitForFinished(1000))
			kill();
		waitForFinished();
	}

	// initialize shared memory
	if (d_data->mem) {
		delete d_data->mem;
		d_data->mem = nullptr;
	}

	QString shared_memory_name = _shared_memory_name;
	if (shared_memory_name.isEmpty()) {
		shared_memory_name = d_data->sharedMemoryName;
		if (shared_memory_name.isEmpty() || !isFreeName(shared_memory_name))
			shared_memory_name = findNextMemoryName();
	}

	if (!d_data->mem) {
		d_data->mem = new SharedMemory(shared_memory_name, 50000000, true);
		if (!d_data->mem->isValid()) {
			delete d_data->mem;
			d_data->mem = nullptr;
			d_data->lastError = "cannot create shared memory object";
			return 0;
		}
	}

	d_data->sharedMemoryName = shared_memory_name;

	QString current = QDir::currentPath();
	current.replace("\\", "/");
	QString path = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python/qtconsole_widget.py";
	QString sys_path = QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/Python";
	path.replace("\\", "/");
	sys_path.replace("\\", "/");
	QString python = VipPyInterpreter::instance()->python();
	vip_debug("Start IPython with %s\n", python.toLatin1().data());
	python.replace("\\", "/");
	QString cmd = python + " " + path + " " + QString::number(font_size) + " " + style + " \"import sys; sys.path.append('" + sys_path + "');import Thermavip; Thermavip.setSharedMemoryName('" +
		      shared_memory_name +
		      "'); Thermavip._ipython_interp = __interp \""
		      " \"" +
		      current + "\" " + QString::number(qApp->applicationPid());

	QStringList args;
	args << path << QString::number(font_size) << style
	     << "import sys; sys.path.append('" + sys_path + "');import Thermavip; Thermavip.setSharedMemoryName('" + shared_memory_name + "'); Thermavip._ipython_interp = __interp" << current
	     << QString::number(qApp->applicationPid());

	if (d_data->embedded) {

		cmd += " 1";
		args << "1";
	}
	vip_debug("IPython shell cmd: %s\n", cmd.toLatin1().data());

	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

#ifdef _WIN32
	// For windows, we must add some paths to PATH in case of anaconda install
	// First, we need the python path
	QProcess p;
	QStringList _args;
	_args << "-c" << "import sys; print(sys.executable)";
	p.start(python, _args);
	p.waitForStarted();
	p.waitForFinished();

	QByteArray ar = p.readAllStandardOutput();
	QString err = p.errorString();
	if (!ar.isEmpty()) {
		vip_debug("found Python at %s\n", ar.data());
		VIP_LOG_INFO("Found Python at %s\n", ar.data());
		QString pdir = QFileInfo(QString(ar)).absolutePath();
		QStringList lst;
		lst << pdir + "/Library/bin" << pdir + "/bin" << pdir + "/condabin" << pdir + "/Scripts";

		QString path = env.value("PATH");
		if (!path.endsWith(";"))
			path += ";";
		path = lst.join(";");
		env.insert("PATH", path);
		vip_debug("path: %s\n", path.toLatin1().data());
	}

#else
#ifndef VIP_PYTHONHOME
	// remove PYTHONHOME and PYTHONPATH set by thermavip
	env.remove("PYTHONHOME");
	env.remove("PYTHONPATH");
#endif
#endif
	this->setProcessEnvironment(env);

#ifdef _WIN32
	QDir::setCurrent(env.value("USERPROFILE"));
#else
	QDir::setCurrent(env.value("HOME"));
#endif

	this->QProcess::start(python, args);
	this->waitForStarted(5000);

	QDir::setCurrent(current);

	// read pid
	qint64 pid = 0;
	while (state() == Running) {
		if (waitForReadyRead(timeout())) {
			QByteArray tmp = readAllStandardOutput() + readAllStandardError();
			vip_debug("%s\n", tmp.data());
			pid = tmp.split('\n').first().toLongLong();
			break;
		}
	}

	if (pid == 0 && state() == Running) {
		// kill
		terminate();
		if (!waitForFinished(1000))
			kill();
		d_data->lastError = this->errorString() + "\n" + readAllStandardError();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return 0;
	}

	if (pid == 0) {
		d_data->lastError = this->errorString() + "\n" + readAllStandardError();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
	}

	return d_data->pid = pid;
}

qint64 VipIPythonShellProcess::windowId() const
{
	return d_data->pid;
}

QString VipIPythonShellProcess::sharedMemoryName() const
{
	return d_data->sharedMemoryName;
}

bool VipIPythonShellProcess::sendObject(const QString& name, const QVariant& obj)
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	QString error;
	d_data->mem->acquire();

	// write object
	bool r = d_data->mem->writeObject(name, obj, timeout(), &error);
	if (!r) {
		d_data->lastError = error;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	// read reply
	QByteArray res;
	if (!d_data->mem->read(res, timeout())) {
		QByteArray r = readAllStandardError();
		d_data->lastError = "Timeout";
		if (!r.isEmpty())
			d_data->lastError += "\n" + r;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}
	d_data->mem->release();

	if (!d_data->mem->readError(res, error)) {
		d_data->lastError = "error while interpreting reply";
		return false;
	}

	if (error.isEmpty())
		return true;

	d_data->lastError = error;
	return false;
}

QVariant VipIPythonShellProcess::retrieveObject(const QString& name)
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return QVariant::fromValue(VipPyError(d_data->lastError));
	}

	QString error;
	d_data->mem->acquire();

	// write object
	bool r = d_data->mem->writeSendObject(name, timeout(), &error);
	if (!r) {
		d_data->lastError = error;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return QVariant::fromValue(VipPyError(error + " "));
	}

	// read reply
	QByteArray ar;
	r = d_data->mem->read(ar, timeout());
	d_data->mem->release();
	if (!r) {
		QByteArray r = readAllStandardError();
		d_data->lastError = "Timeout";
		if (!r.isEmpty())
			d_data->lastError += "\n" + r;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return QVariant::fromValue(VipPyError(d_data->lastError));
	}

	QVariant v;
	if (!d_data->mem->readObject(ar, v, &error)) {
		QString saved = error;
		if (d_data->mem->readError(ar, error)) {
			d_data->lastError = error;
			vip_debug("%s\n", d_data->lastError.toLatin1().data());
			return QVariant::fromValue(VipPyError(error + " "));
		}
		d_data->lastError = saved;
		return QVariant::fromValue(VipPyError(saved + " "));
	}

	return v;
}

bool VipIPythonShellProcess::execCode(const QString& code)
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	d_data->mem->acquire();

	// write object
	bool r = d_data->mem->writeExecCode(code, timeout());
	if (!r) {
		d_data->lastError = "error while sending code to execute";
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	QByteArray res;
	if (!d_data->mem->read(res, timeout())) {
		QByteArray r = readAllStandardError();
		d_data->lastError = "Timeout";
		if (!r.isEmpty())
			d_data->lastError += "\n" + r;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}
	d_data->mem->release();

	QString error;

	if (!d_data->mem->readError(res, error)) {
		d_data->lastError = "error while interpreting reply";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	if (error.isEmpty())
		return true;

	d_data->lastError = error;
	return false;
}

bool VipIPythonShellProcess::execLine(const QString& code)
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	d_data->mem->acquire();

	// write object
	bool r = d_data->mem->writeExecLine(code, timeout());
	if (!r) {
		d_data->lastError = "error while sending code to execute";
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	QByteArray res;
	if (!d_data->mem->read(res, timeout())) {
		QByteArray r = readAllStandardError();
		d_data->lastError = "Timeout";
		if (!r.isEmpty())
			d_data->lastError += "\n" + r;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}
	d_data->mem->release();

	QString error;
	vip_debug("%s\n", res.data());
	if (!d_data->mem->readError(res, error)) {
		d_data->lastError = "error while interpreting reply";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	if (error.isEmpty())
		return true;

	d_data->lastError = error;
	vip_debug("%s\n", d_data->lastError.toLatin1().data());
	return false;
}

bool VipIPythonShellProcess::execLineNoWait(const QString& code)
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	// write line
	d_data->mem->acquire();
	bool r = d_data->mem->writeExecLineNoWait(code, timeout());
	d_data->mem->release();

	if (!r) {
		d_data->lastError = "error while sending code to execute";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}
	return true;
}

bool VipIPythonShellProcess::restart()
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	d_data->mem->acquire();

	// write object
	bool r = d_data->mem->writeRestart(timeout());
	if (!r) {
		d_data->lastError = "error while sending 'restart' command";
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	QByteArray res;
	if (!d_data->mem->read(res, timeout())) {
		QByteArray r = readAllStandardError();
		d_data->lastError = "Timeout";
		if (!r.isEmpty())
			d_data->lastError += "\n" + r;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}
	d_data->mem->release();

	QString error;
	vip_debug("%s\n", res.data());
	if (!d_data->mem->readError(res, error)) {
		d_data->lastError = "error while interpreting reply";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	if (error.isEmpty())
		return true;

	d_data->lastError = error;
	vip_debug("%s\n", d_data->lastError.toLatin1().data());
	return false;
}

bool VipIPythonShellProcess::isRunningCode()
{
	d_data->lastError.clear();
	if (state() != Running || !d_data->mem || !d_data->mem->isValid()) {
		d_data->lastError = "VipIPythonShellProcess not running";
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	// read flag
	char c = d_data->mem->flags()[0];
	if (c)
		return true;
	else
		return false;

	d_data->mem->acquire();

	// write object
	bool r = d_data->mem->writeIsRunningCode(timeout());
	if (!r) {
		d_data->lastError = "error while sending 'running' command";
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return false;
	}

	// read reply
	QByteArray res;
	if (!d_data->mem->read(res, timeout())) {

		waitForReadyRead(100);
		QByteArray r = readAllStandardError();
		vip_debug("'%s' '%s'\n", readAllStandardOutput().data(), r.data());
		d_data->lastError = "Timeout";
		if (!r.isEmpty())
			d_data->lastError += "\n" + r;
		d_data->mem->release();
		vip_debug("%s\n", d_data->lastError.toLatin1().data());
		return true;
	}
	d_data->mem->release();
	return (bool)res.toInt();
}

QString VipIPythonShellProcess::lastError() const
{
	return d_data->lastError;
}

void VipIPythonShellProcess::setStyleSheet(const QString& st)
{

	// send style sheet
	QByteArray stylesheet = "SH_STYLE_SHEET  " + qApp->styleSheet().toLatin1();
	d_data->mem->write(stylesheet.data(), stylesheet.size());
}

QString VipIPythonShellProcess::findNextMemoryName()
{
	int count = 1;
	while (true) {
		QSharedMemory mem("Thermavip-" + QString::number(count));
		if (!mem.attach())
			return "Thermavip-" + QString::number(count);
		++count;
	}
	return QString();
}

bool VipIPythonShellProcess::isFreeName(const QString& name)
{
	QSharedMemory mem(name);
	if (mem.attach())
		return false;
	return true;
}

#include <qwindow.h>
#include <qlayout.h>

class VipIPythonShellWidget::PrivateData
{
public:
	VipIPythonShellProcess process;
	int font_size;
	QString style;
	QWidget* widget;
	QWindow* window;
	QVBoxLayout* layout;
	qint64 wid;
};

VipIPythonShellWidget::VipIPythonShellWidget(int font_size, const QString& style, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->font_size = font_size;
	d_data->style = style;
	d_data->widget = nullptr;
	d_data->process.setEmbedded(true);
	qint64 pid = d_data->wid = d_data->process.start(font_size, style);

	if (pid) {
		WId handle = (WId)pid;

		// TEST
		/* #ifdef _WIN32
				SetParent((HWND)handle, (HWND)this->window()->winId());
				SetWindowLongPtrW((HWND)handle, GWL_STYLE, WS_POPUP);
		#endif*/

		d_data->window = QWindow::fromWinId((WId)handle);
		d_data->widget = QWidget::createWindowContainer(d_data->window, this);
		d_data->widget->setObjectName("VipIPythonShellWidget");
		d_data->layout = new QVBoxLayout();
		// d_data->layout->setContentsMargins(0, 0, 0, 0);
		d_data->layout->setContentsMargins(5, 5, 5, 5);
		d_data->layout->addWidget(d_data->widget);
		setLayout(d_data->layout);

		d_data->process.setStyleSheet(qApp->styleSheet());
		// launch startup code
		d_data->process.execCode(VipPyInterpreter::instance()->startupCode());

#ifdef _WIN32
		SetFocus((HWND)handle);
#endif
	}
	else {
		vip_debug("IPython error: %s\n", d_data->process.lastError().toLatin1().data());
	}

	connect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(focusChanged(QWidget*, QWidget*)));
}

VipIPythonShellWidget::~VipIPythonShellWidget()
{
	disconnect(qApp, SIGNAL(focusChanged(QWidget*, QWidget*)), this, SLOT(focusChanged(QWidget*, QWidget*)));
}

VipIPythonShellProcess* VipIPythonShellWidget::process() const
{
	return &d_data->process;
}

bool VipIPythonShellWidget::restart()
{
	return d_data->process.restart();
}

bool VipIPythonShellWidget::restartProcess()
{
	if (d_data->widget) {
		delete d_data->widget;
		d_data->widget = nullptr;
	}
	qint64 pid = d_data->wid = d_data->process.start(d_data->font_size, d_data->style);
	if (pid) {
		WId handle = (WId)pid;
		d_data->window = QWindow::fromWinId((WId)handle);
		d_data->widget = QWidget::createWindowContainer(d_data->window);
		d_data->layout->addWidget(d_data->widget);
		d_data->process.setStyleSheet(qApp->styleSheet());
		// launch startup code
		d_data->process.execCode(VipPyInterpreter::instance()->startupCode());
		return true;
	}
	else {
		vip_debug("IPython error: %s\n", d_data->process.lastError().toLatin1().data());
		return false;
	}
}

void VipIPythonShellWidget::focusChanged(QWidget* old, QWidget* now)
{
#ifdef _WIN32
	if (GetFocus() == (HWND)d_data->wid)
		vipGetIPythonToolWidget()->setFocus();
#endif
}

#include <qtimer.h>

class VipIPythonTabBar::PrivateData
{
public:
	PrivateData()
	  : tabWidget(nullptr)
	  , dragIndex(-1)
	  , hoverIndex(-1)
	  , closeIcon(vipIcon("close.png"))
	  , restartIcon(vipIcon("restart.png"))
	  , hoverCloseIcon(vipIcon("close.png"))
	  , hoverRestartIcon(vipIcon("restart.png"))
	  , selectedCloseIcon(vipIcon("close.png"))
	  , selectedRestartIcon(vipIcon("restart.png"))
	{
	}
	VipIPythonTabWidget* tabWidget;
	int dragIndex;
	int hoverIndex;

	QIcon closeIcon;
	QIcon restartIcon;
	QIcon hoverCloseIcon;
	QIcon hoverRestartIcon;
	QIcon selectedCloseIcon;
	QIcon selectedRestartIcon;
};

VipIPythonTabBar::VipIPythonTabBar(VipIPythonTabWidget* parent)
  : QTabBar(parent)
{
	setIconSize(QSize(18, 18));
	setMouseTracking(true);

	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->tabWidget = parent;

	connect(this, SIGNAL(currentChanged(int)), this, SLOT(updateIcons()));
	addTab("+"); // vipIcon("add_page.png"),QString());
}

VipIPythonTabBar::~VipIPythonTabBar() {}

QIcon VipIPythonTabBar::closeIcon() const
{
	return d_data->closeIcon;
}
void VipIPythonTabBar::setCloseIcon(const QIcon& i)
{
	d_data->closeIcon = i;
	updateIcons();
}

QIcon VipIPythonTabBar::restartIcon() const
{
	return d_data->restartIcon;
}
void VipIPythonTabBar::setRestartIcon(const QIcon& i)
{
	d_data->restartIcon = i;
	updateIcons();
}

QIcon VipIPythonTabBar::hoverCloseIcon() const
{
	return d_data->hoverCloseIcon;
}
void VipIPythonTabBar::setHoverCloseIcon(const QIcon& i)
{
	d_data->hoverCloseIcon = i;
	updateIcons();
}

QIcon VipIPythonTabBar::hoverRestartIcon() const
{
	return d_data->hoverRestartIcon;
}
void VipIPythonTabBar::setHoverRestartIcon(const QIcon& i)
{
	d_data->hoverRestartIcon = i;
	updateIcons();
}

QIcon VipIPythonTabBar::selectedCloseIcon() const
{
	return d_data->selectedCloseIcon;
}
void VipIPythonTabBar::setSelectedCloseIcon(const QIcon& i)
{
	d_data->selectedCloseIcon = i;
	updateIcons();
}

QIcon VipIPythonTabBar::selectedRestartIcon() const
{
	return d_data->selectedRestartIcon;
}
void VipIPythonTabBar::setSelectedRestartIcon(const QIcon& i)
{
	d_data->selectedRestartIcon = i;
	updateIcons();
}

VipIPythonTabWidget* VipIPythonTabBar::ipythonTabWidget() const
{
	return const_cast<VipIPythonTabWidget*>(d_data->tabWidget);
}

void VipIPythonTabBar::tabInserted(int index)
{
	if (index < count() - 1) {
		VipIPythonShellWidget* area = qobject_cast<VipIPythonShellWidget*>(ipythonTabWidget()->widget(index));
		if (area) {

			QToolBar* bar = new QToolBar();
			bar->setIconSize(QSize(18, 18));
			bar->setParent(this);
			// bar->setMinimumSize(bar->sizeHint());

			QToolButton* restart = new QToolButton();
			restart->setProperty("widget", QVariant::fromValue(ipythonTabWidget()->widget(index)));
			restart->setIcon(restartIcon());
			restart->setAutoRaise(true);
			restart->setToolTip("Restart interpreter");
			restart->setStyleSheet("QToolButton {background-color : transparent;} QToolButton:hover{background-color: #3399FF;}");
			// restart->setMaximumSize(16, 16);
			restart->setMaximumWidth(18);
			restart->setObjectName("restart");

			QToolButton* restartP = new QToolButton();
			restartP->setProperty("widget", QVariant::fromValue(ipythonTabWidget()->widget(index)));
			restartP->setIcon(vipIcon("stop.png"));
			restartP->setAutoRaise(true);
			restartP->setToolTip("Restart process");
			restartP->setStyleSheet("QToolButton {background-color : transparent;} QToolButton:hover{background-color: #3399FF;}");
			// restart->setMaximumSize(16, 16);
			restartP->setMaximumWidth(18);
			restartP->setObjectName("restartP");

			// set the close button
			QToolButton* close = new QToolButton();
			close->setProperty("widget", QVariant::fromValue(ipythonTabWidget()->widget(index)));
			close->setIcon(closeIcon());
			close->setAutoRaise(true);
			close->setToolTip("Close interpreter");
			close->setStyleSheet("QToolButton {background-color : transparent;} QToolButton:hover{background-color: #3399FF;}");
			// close->setMaximumSize(16, 16);
			close->setMaximumWidth(18);
			close->setObjectName("close");

			bar->addWidget(restart);
			bar->addWidget(restartP);
			bar->addWidget(close);

			setTabButton(index, QTabBar::RightSide, bar);
			bar->show();

			connect(close, SIGNAL(clicked(bool)), this, SLOT(closeTab()));
			connect(restart, SIGNAL(clicked(bool)), this, SLOT(restartTab()));
			connect(restartP, SIGNAL(clicked(bool)), this, SLOT(restartTabProcess()));
		}
	}

	if (currentIndex() == count() - 1 && count() > 1)
		ipythonTabWidget()->setCurrentIndex(count() - 2);

	updateIcons();
}

void VipIPythonTabBar::leaveEvent(QEvent*)
{
	d_data->hoverIndex = -1;
	updateIcons();
}

void VipIPythonTabBar::mouseMoveEvent(QMouseEvent* event)
{
	// if(event->buttons())
	QTabBar::mouseMoveEvent(event);
	if (tabAt(event->pos()) != d_data->hoverIndex) {
		d_data->hoverIndex = tabAt(event->pos());
		updateIcons();
	}
}

void VipIPythonTabBar::mouseDoubleClickEvent(QMouseEvent* evt)
{
	if ((evt->buttons() & Qt::RightButton)) {
		QTabBar::mouseDoubleClickEvent(evt);
		return;
	}

	int index = tabAt(evt->pos());
	if (index < 0)
		return;
	/*VipIPythonShellWidget* area = qobject_cast<VipDisplayPlayerArea*>(displayTabWidget()->widget(index));
	if (!area)
		return;
	displayTabWidget()->setProperty("_vip_index", index);
	displayTabWidget()->renameWorkspace();*/
}

void VipIPythonTabBar::mousePressEvent(QMouseEvent* event)
{
	// id we press on the last tab, insert a new one
	if (tabAt(event->pos()) == count() - 1) {
		ipythonTabWidget()->addInterpreter();
	}
	else {
		QTabBar::mousePressEvent(event);
	}
}

void VipIPythonTabBar::closeTab()
{
	QWidget* w = sender()->property("widget").value<QWidget*>();
	int index = ipythonTabWidget()->indexOf(w);
	if (index >= 0)
		ipythonTabWidget()->closeTab(index);
	else if (w) {
		// close the current workspace
		delete w;
	}
}

void VipIPythonTabBar::restartTab()
{
	if (VipIPythonShellWidget* area = sender()->property("widget").value<VipIPythonShellWidget*>())
		area->restart();
}

void VipIPythonTabBar::restartTabProcess()
{
	if (VipIPythonShellWidget* area = sender()->property("widget").value<VipIPythonShellWidget*>())
		area->restartProcess();
}

void VipIPythonTabBar::updateIcons()
{
	int current = currentIndex();
	int hover = d_data->hoverIndex;
	for (int i = 0; i < count(); ++i) {
		if (QWidget* buttons = tabButton(i, QTabBar::RightSide)) {
			QToolButton* _close = buttons->findChild<QToolButton*>("close");
			QToolButton* _restart = buttons->findChild<QToolButton*>("restart");
			if (i == current) {
				if (_close)
					_close->setIcon(selectedCloseIcon());
				if (_restart)
					_restart->setIcon(selectedRestartIcon());
			}
			else if (i == hover) {
				if (_close)
					_close->setIcon(hoverCloseIcon());
				if (_restart)
					_restart->setIcon(hoverRestartIcon());
			}
			else {
				if (_close)
					_close->setIcon(closeIcon());
				if (_restart)
					_restart->setIcon(restartIcon());
			}
		}
	}
}

#include <qcoreapplication.h>

class VipIPythonTabWidget::PrivateData
{
public:
	QTimer timer;
};

VipIPythonTabWidget::VipIPythonTabWidget(QWidget* parent)
  : QTabWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->timer.setSingleShot(true);
	d_data->timer.setInterval(500);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(updateTab()));

	setTabBar(new VipIPythonTabBar(this));
	tabBar()->setIconSize(QSize(16, 16));
}
VipIPythonTabWidget::~VipIPythonTabWidget() {}

VipIPythonShellWidget* VipIPythonTabWidget::widget(int index) const
{
	return qobject_cast<VipIPythonShellWidget*>(this->QTabWidget::widget(index));
}

void VipIPythonTabWidget::closeTab(int index)
{
	widget(index)->deleteLater();
}

void VipIPythonTabWidget::addInterpreter()
{
	VipIPythonShellWidget* w = new VipIPythonShellWidget();
	addTab(w, w->process()->sharedMemoryName());
	setCurrentIndex(count() - 2);
	d_data->timer.start();
}

void VipIPythonTabWidget::updateTab()
{
	QSize s = size();
	resize(s + QSize(10, 10));
	resize(s);
}

void VipIPythonTabWidget::closeEvent(QCloseEvent*) {}

VipIPythonToolWidget::VipIPythonToolWidget(VipMainWindow* win)
  : VipToolWidget(win)
{
	m_tabs = new VipIPythonTabWidget();
	// this->setWidget(m_tabs, Qt::Horizontal);
	this->QDockWidget::setWidget(m_tabs); // With Qt6, embedding an external window inside a QScrollArea causes display bugs
	this->setWindowTitle("IPython external consoles");
	this->setObjectName("IPython external consoles");
	m_tabs->setStyleSheet("VipIPythonTabWidget{padding: 3px;}");
	this->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
}

VipIPythonToolWidget::~VipIPythonToolWidget() {}

VipIPythonTabWidget* VipIPythonToolWidget::widget() const
{
	return m_tabs;
}

VipIPythonToolWidget* vipGetIPythonToolWidget(VipMainWindow* win)
{
	static bool initialised = 0;
	static QPointer<VipIPythonToolWidget> inst;
	if (!initialised) {
		initialised = 1;
		if (!win)
			return nullptr;
		VipIPythonToolWidget* w = new VipIPythonToolWidget(win);
		w->widget()->addInterpreter();
		if (w->widget()->widget(0)->process()->state() != QProcess::Running) {
			delete w;
			return inst = nullptr;
		}

		inst = w;
	}
	return inst;
}
