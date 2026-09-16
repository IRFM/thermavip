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

#include "VipLogging.h"

#include <QByteArray>
#include <QDateTime>
#include <QFileInfo>
#include <QSharedPointer>
#include <QStringList>

#include <atomic>
#include <iostream>

#define DATE_FORMAT "yy:MM:dd-hh:mm:ss.zzz"
#define DATE_SIZE 25

namespace vip_log_detail
{
	static std::atomic<bool>& _vip_debug()
	{
		static std::atomic<bool> enable{ false };
		return enable;
	}
	bool _vip_enable_debug()
	{
		return _vip_debug().load(std::memory_order_relaxed);
	}
	void _vip_set_enable_debug(bool en)
	{
		_vip_debug() = en;
	}
}

class VipLogging::PrivateData
{
public:
	PrivateData()
	  : stop(true)
	  , enable_saving(false)
	  , enabled(true)
	{
	}
	QList<LogFrame> logs;
	QSharedMemory memory;
	// The queue and the configuration.
	QMutex mutex;
	// The outputs themselves. Neither the file logger nor the shared memory object
	// has any exclusion of its own, so writing needs one, but it must not be the
	// mutex above: holding that one across a write blocks every thread that only
	// wants to queue an entry.
	QMutex outputMutex;
	QSharedPointer<VipFileLogger> file;
	Outputs outputs;
	// Atomic: the writing thread reads it in its loop without the mutex, and
	// close() writes it under the mutex. A plain bool leaves the thread free never
	// to observe the write, and the wait that follows never returns.
	std::atomic<bool> stop{ true };
	bool enable_saving;
	bool enabled;
	QStringList saved;
};

/// Structure representing a log entry.
struct VipLogging::LogFrame
{
	QString text;
	Outputs outputs;
	Level level;
	QDateTime date;

	LogFrame(const QString& text = QString(), Level level = Info, Outputs outputs = Outputs(), const QDateTime& date = QDateTime::currentDateTime())
	  : text(text)
	  , outputs(outputs)
	  , level(level)
	  , date(date)
	{
	}
};


void VipLogging::run()
{
	while (!d_data->stop) {
		while (logCount() > 0) {
			LogFrame frame;
			popLog(&frame);
			directLog(frame);
		}

		QThread::msleep(15);
	}

	while (logCount() > 0) {
		LogFrame frame;
		popLog(&frame);
		directLog(frame);
	}
}

VipLogging::VipLogging()
  : QThread()
{
	VIP_CREATE_PRIVATE_DATA();
}

VipLogging::VipLogging(Outputs outputs, std::unique_ptr<VipFileLogger> logger)
  : QThread()
{
	VIP_CREATE_PRIVATE_DATA();
	open(outputs, std::move(logger));
}

VipLogging::VipLogging(Outputs outputs, const QString& identifier)
  : QThread()
{
	VIP_CREATE_PRIVATE_DATA();
	open(outputs, identifier);
}

VipLogging::~VipLogging()
{
	close();
}

VipLogging& VipLogging::instance()
{
	static VipLogging inst;
	return inst;
}

void VipLogging::pushLog(const LogFrame& l)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->logs.append(l);
}

bool VipLogging::popLog(LogFrame* ret)
{
	QMutexLocker lock(&d_data->mutex);
	if (d_data->logs.size() > 0) {
		*ret = d_data->logs[0];
		d_data->logs.pop_front();
		return true;
	}

	return false;
}

int VipLogging::logCount()
{
	QMutexLocker lock(&d_data->mutex);
	return d_data->logs.size();
}

const VipFileLogger* VipLogging::logger() const
{
	return d_data->file.data();
}

QString VipLogging::identifier() const
{
	return d_data->memory.key();
}

QString VipLogging::filename() const
{
	if (d_data->file)
		return d_data->file->canonicalFilePath();
	else
		return QString();
}

VipLogging::Outputs VipLogging::outputs() const
{
	return d_data->outputs;
}

void VipLogging::setSavingEnabled(bool enable)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->enable_saving = enable;
	if (!enable)
		d_data->saved.clear();
}

bool VipLogging::savingEnabled() const
{
	return d_data->enable_saving;
}

QStringList VipLogging::savedEntries() const
{
	QMutexLocker lock(&d_data->mutex);
	return d_data->saved;
}

void VipLogging::setEnabled(bool enable)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->enabled = enable;
}
bool VipLogging::isEnabled() const
{
	return d_data->enabled;
}

namespace
{
	/// Holds the inter process lock of a shared memory segment for a scope. The
	/// lock is named and system wide: released by hand on two paths, an allocation
	/// that throws between them left it taken for every process of the session,
	/// including those started afterwards.
	class SharedMemoryLocker
	{
		QSharedMemory* m_memory;

	public:
		explicit SharedMemoryLocker(QSharedMemory& memory)
		  : m_memory(memory.lock() ? &memory : nullptr)
		{
		}
		~SharedMemoryLocker()
		{
			if (m_memory)
				m_memory->unlock();
		}
		SharedMemoryLocker(const SharedMemoryLocker&) = delete;
		SharedMemoryLocker& operator=(const SharedMemoryLocker&) = delete;
		explicit operator bool() const noexcept { return m_memory != nullptr; }
	};
}

bool VipLogging::open(Outputs outputs, const QString& identifier)
{
	std::unique_ptr<VipFileLogger> logger;
	if (!identifier.isEmpty() && (outputs & File))
		logger.reset(new VipTextLogger(identifier, "./"));
	return open(outputs, std::move(logger));
}

bool VipLogging::open(Outputs outputs, std::unique_ptr<VipFileLogger> logger)
{
	close();

	QString identifier = "Log";
	if (logger)
		identifier = logger->identifier();

	QMutexLocker lock(&d_data->mutex);
	d_data->outputs = outputs;

	if (d_data->memory.isAttached())
		d_data->memory.detach();

	if (outputs & SharedMemory) {
		d_data->memory.setKey(identifier);
		if (!d_data->memory.create(10000)) {
			if (!d_data->memory.attach())
				return false;
		}

		if (SharedMemoryLocker locker{ d_data->memory })
			memset(d_data->memory.data(), 0, d_data->memory.size());
	}

	if (outputs & File) {
		d_data->file = QSharedPointer<VipFileLogger>(logger.release());
	}

	d_data->stop = false;
	this->start();
	return true;
}

bool VipLogging::isOpen() const
{
	return !d_data->stop;
}

void VipLogging::close()
{
	if (isOpen()) {
		{
			QMutexLocker lock(&d_data->mutex);
			d_data->stop = true;
		}
		this->wait();
	}

	// Both, in this order and only here: a writer holds one at a time, so the two
	// cannot be taken in the opposite order anywhere.
	QMutexLocker lock(&d_data->mutex);
	QMutexLocker outputs(&d_data->outputMutex);
	if (d_data->memory.isAttached())
		d_data->memory.detach();

	d_data->logs.clear();
	d_data->file.reset();
	d_data->outputs = Outputs();
}

bool VipLogging::waitForWritten(int msecs)
{
	qint64 start = QDateTime::currentMSecsSinceEpoch();
	while (logCount() > 0) {
		QThread::msleep(1);
		if (msecs >= 0 && QDateTime::currentMSecsSinceEpoch() - start > msecs)
			return false;
		;
	}
	return true;
}

void VipLogging::log(const QString& text, Level level, Outputs outputs, qint64 time)
{
	if (!isEnabled())
		return;

	if (time < 0)
		pushLog(LogFrame(text, level, outputs));
	else
		pushLog(LogFrame(text, level, outputs, QDateTime::fromMSecsSinceEpoch(time)));
}

void VipLogging::directLog(const QString& text, Level level, Outputs outputs, qint64 time)
{
	if (!isEnabled())
		return;

	if (time < 0)
		directLog(LogFrame(text, level, outputs));
	else
		directLog(LogFrame(text, level, outputs, QDateTime::fromMSecsSinceEpoch(time)));
}

void VipLogging::directLog(const LogFrame& frame)
{
	if (!isEnabled())
		return;

	Outputs out = frame.outputs;

	// Read the configuration under the queue mutex, then let it go: the writes
	// below are what took the longest, and every call that only wants to queue an
	// entry was waiting behind them.
	QSharedPointer<VipFileLogger> file;
	bool saving = false;
	{
		QMutexLocker lock(&d_data->mutex);
		if (out < Cout)
			out = d_data->outputs;
		file = d_data->file;
		saving = d_data->enable_saving;
	}

	QByteArray log;

	if (out & Cout) {
		if (log.isEmpty())
			log = formatLogEntry(frame.text, frame.level, frame.date);
		std::cout << log.data();
		std::cout.flush();
	}

	QMutexLocker lock(&d_data->outputMutex);

	if ((out & File) && file) {
		// No cross process exclusion here: two instances writing to the same file
		// interleave their entries. The semaphore that was meant to prevent it was
		// built and keyed on every open but both of its uses were commented out, so
		// it protected nothing while still costing a system object. Restoring it
		// means acquiring it outside the mutex below, which is a change to the
		// locking order and belongs with the concurrency work.
		file->addLogEntry(frame.text, frame.level, frame.date);
	}
	if (out & SharedMemory) {
		if (log.isEmpty())
			log = formatLogEntry(frame.text, frame.level, frame.date);

		if (SharedMemoryLocker locker{ d_data->memory }) {
			// The segment is named, so any process of the session can write this
			// header. It is the destination offset of the copy below, and only the
			// sum used to be tested; the sum itself was computed in 32 bits, where
			// a value near the maximum wraps negative and passes that test.
			qint32 size = 0;
			memcpy(&size, d_data->memory.data(), sizeof(qint32));
			const qsizetype capacity = d_data->memory.size() - (qsizetype)sizeof(qint32);
			if (size < 0 || (qsizetype)size > capacity) {
				memset(d_data->memory.data(), 0, d_data->memory.size());
				size = 0;
			}

			const qsizetype new_size = (qsizetype)size + log.size();
			if (new_size <= capacity) {
				const qint32 written = (qint32)new_size;
				memcpy(d_data->memory.data(), &written, sizeof(qint32));
				memcpy(static_cast<char*>(d_data->memory.data()) + size + sizeof(qint32), log.data(), log.size());
			}
		}
	}
	if (saving) {
		if (log.isEmpty())
			log = formatLogEntry(frame.text, frame.level, frame.date);
		QMutexLocker queue(&d_data->mutex);
		d_data->saved.append(log);
	}
}

QStringList VipLogging::lastLogEntries()
{
	QMutexLocker lock(&d_data->mutex);

	QStringList lst;

	if (SharedMemoryLocker locker{ d_data->memory }) {
		// Same header, same reason to distrust it: here it is the length of the read.
		qint32 size = 0;
		memcpy(&size, d_data->memory.data(), sizeof(qint32));
		const qsizetype capacity = d_data->memory.size() - (qsizetype)sizeof(qint32);

		if (size <= 0 || (qsizetype)size > capacity) {
			if (size != 0)
				memset(d_data->memory.data(), 0, d_data->memory.size());
			return lst;
		}

		QString str(QByteArray(static_cast<char*>(d_data->memory.data()) + sizeof(qint32), size));
		lst = str.split("\n", VIP_SKIP_BEHAVIOR::SkipEmptyParts);

		memset(d_data->memory.data(), 0, d_data->memory.size());
	}

	return lst;
}

bool VipLogging::splitLogEntry(const QString& entry, QString& type, QString& date, QString& text)
{
	if (entry.size() < DATE_SIZE + 10)
		return false;

	type = entry.mid(0, 10);
	date = entry.mid(10, DATE_SIZE);
	text = entry.mid(DATE_SIZE + 10);
	return true;
}

QByteArray VipLogging::formatLogEntry(const QString& text, VipLogging::Level level, const QDateTime& date)
{
	QByteArray log;

	if (level == Info)
		log = "Info";
	else if (level == Warning)
		log = "Warning";
	else if (level == Error)
		log = "Error";
	else if (level == Debug)
		log = "Debug";

	log.append(QByteArray(10 - log.size(), ' '));

	QByteArray time = date.toString(DATE_FORMAT).toLatin1();
	time.append(QByteArray(DATE_SIZE - time.size(), ' '));

	log += time;

	int size = log.size();
	QStringList lst = text.split("\n");
	if (lst.size() > 1) {
		QByteArray prefix(size, char(' '));
		for (int i = 0; i < lst.size(); ++i) {
			if (i > 0)
				log += prefix;
			log += lst[i].toLatin1() + "\n";
		}
	}
	else {
		log += text.toLatin1();
		log += "\n";
	}

	return log;
}

VipFileLogger::VipFileLogger(const QString& identifier, const QString& directory)
  : d_identifier(identifier)
  , d_directory(directory)
{
	d_directory.replace("\\", "/");
	if (d_directory.endsWith("/"))
		d_directory.remove(d_directory.size() - 1, 1);
}

VipTextLogger::VipTextLogger(const QString& id, const QString& dir, bool overwrite)
  : VipFileLogger(id, dir)
{
	QString filename = directory() + "/" + identifier() + ".txt";
	d_file.setFileName(filename);
	if (!d_file.open(QFile::ReadOnly) || overwrite) {
		d_file.close();
		// bool res =
		d_file.open(QFile::WriteOnly);
	}
	d_file.close();
}

QString VipTextLogger::canonicalFilePath() const
{
	return QFileInfo(d_file.fileName()).canonicalFilePath();
}

void VipTextLogger::addLogEntry(const QString& text, VipLogging::Level level, const QDateTime& date)
{
	// Nothing here was checked: a failed open dropped the entry without a word, a
	// short write truncated it, and the close that flushes it hid any deferred
	// error. A missing line in a log is read as the event not having happened. A
	// logger cannot log its own failure, so it says so on the error stream.
	const QByteArray log = VipLogging::formatLogEntry(text, level, date);
	if (!d_file.open(QFile::WriteOnly | QFile::Text | QFile::Append)) {
		std::cerr << "VipLogging: cannot open " << qPrintable(d_file.fileName()) << std::endl;
		return;
	}
	const qint64 written = d_file.write(log);
	const bool flushed = d_file.flush();
	d_file.close();
	if (written != log.size() || !flushed || d_file.error() != QFile::NoError)
		std::cerr << "VipLogging: incomplete write to " << qPrintable(d_file.fileName()) << std::endl;
}
