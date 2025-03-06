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

#ifndef VIP_LOGGING_H
#define VIP_LOGGING_H

#include <qdatetime.h>
#include <qfile.h>
#include <qmutex.h>
#include <qsharedmemory.h>
#include <qstringlist.h>
#include <qsystemsemaphore.h>
#include <qtextstream.h>
#include <qthread.h>

#include "VipConfig.h"

///\module Logging
///
/// The Logging library provides logging purpose functionalities.
/// Its main components are the VipLogging classe and the VIP_LOG_INFO, VIP_LOG_WARNING and VIP_LOG_ERROR macros.
///
/// The Logging  library only depends on \qt.

/// \addtogroup Logging
/// @{

class VipFileLogger;

/// \class VipLogging
/// \brief A process/thread safe logging class.
///
/// \ref VipLogging is a process/thread safe class used to log informations of different levels into one or multiple output devices.
///
/// The possible levels are:
/// <ul>
/// <li>VipLogging::Info : a standard process information.
/// <li>VipLogging::Warning : a process warning.
/// <li>VipLogging::Error : a process error.
/// <li>VipLogging::Debug : a debug information.
/// </ul>
///
/// The possible outputs are:
/// <ul>
/// <li>VipLogging::Cout : output the log text into the standard output (std::cout)
/// <li>VipLogging::SharedMemory : output the log text into a shared memory. The shared memory key is set using #VipLogging::SetIdentifier().
/// <li>VipLogging::File : output the log text into a file. The file is protected using a semaphore which key is set using #VipLogging::SetIdentifier.
/// </ul>
///
/// The #VipLogging class is ready to use, you do not necessarily have to specify the shared memory and semaphore identifier
/// nor the file name, default values are provided ('Log.txt' for output file).
///
/// The \ref VipLogging class uses a specific format for log text output:<br>
/// <i> "Level YY:MM:DD-hh:mm:ss.zzz text" </i>
/// <ul>
/// <li><i>Level</i> is a 10 length string describing the log level.
/// <li><i>YY:MM:DD-hh:mm:ss.zzz</i> is a 25 length string representing the date and time of the logging action.
/// <li><i>text</i> is the log text.
/// </ul>
///
/// A log text cannot have a newline. Each log is separated by a newline.
///
/// Since logging a text can be a time consuming task (opening a file, writting a line, closing the file), the \ref VipLogging
/// class internally uses a dedicated thread to perform the log writing. The member function \ref VipLogging::Log() only push
/// the log entry into an internall buffer (which takes less than 1 ms compared to almost 20 ms!) and returns immediately.
/// The actual writing will be left to the thread.
///
/// #VipLogging is a singleton class and can be manipulated using the static method #VipLogging::instance().
///
/// To log new entries, please consider using #VIP_LOG_INFO, #VIP_LOG_WARNING and #VIP_LOG_ERROR instead of #VipLogging::Log()
class VIP_LOGGING_EXPORT VipLogging : public QThread
{
public:
	/// Possible outputs for each log entry
	enum VipOutput
	{
		Cout = 0x0001,				///< VipOutput to the console.
		SharedMemory = 0x0002,			///< VipOutput to the shared memory. Use #VipLogging::ClearSharedMemory to retrieve the logs.
		File = 0x0004,				///< VipOutput to a file.
		AllOutputs = Cout | SharedMemory | File ///< VipOutput to all possible devices
	};
	Q_DECLARE_FLAGS(Outputs, VipOutput);

	/// Possible levels for each log entry
	enum Level
	{
		Info = 0x0001,	  ///< Standard information.
		Warning = 0x0002, ///< Warning that does not cause the process to end.
		Error = 0x0004,	  ///< Error that causes the process to end.
		Debug = 0x0008	  ///< Debug information
	};
	Q_DECLARE_FLAGS(Levels, Level);

	VipLogging();
	VipLogging(Outputs outputs, VipFileLogger* logger);
	VipLogging(Outputs outputs, const QString& identifier = QString());
	~VipLogging();

	static VipLogging& instance();

	/// Return the current VipFileLogger instance
	const VipFileLogger* logger() const;
	/// Returns the logging identifier (used for the file semaphore and the shared memory keys).
	QString identifier() const;
	/// Returns the current output filename.
	QString filename() const;
	/// Returns the current output.
	Outputs outputs() const;

	/// Enable log saving.
	/// If enabled, all log entries are pushed to an internal list.
	/// To retrieve the log list, use #savedEntries().
	/// Setting this property to false will clear the internal log list.
	void setSavingEnabled(bool enable);
	bool savingEnabled() const;
	QStringList savedEntries() const;

	/// Enable logging.
	/// If disabled, all log functions will do nothing.
	void setEnabled(bool);
	bool isEnabled() const;

	/// Set the log identifier.
	/// This will close all previously opened shared memory and semaphore.
	/// Returns true on success, false otherwise.
	bool open(Outputs outputs, VipFileLogger* logger);
	bool open(Outputs outputs, const QString& identifier = QString());
	bool isOpen() const;
	void close();

	bool waitForWritten(int msecs = -1);

	/// Log a text.
	/// \param text text entry to log.
	/// \param levels the levels of this entry (a combination of \ref VipLogging::Level).
	/// \param outputs the outputs where this entry is redirected (a combination of \ref VipLogging::VipOutput). A value of -1 means that the current output
	/// will be used to determine the outputs.
	void log(const QString& text, Level level, Outputs outputs = Outputs(), qint64 time = -1);

	/// Directly write a log frame without using the writing thread.
	void directLog(const QString& text, Level level = Info, Outputs outputs = Outputs(), qint64 time = -1);

	static QByteArray formatLogEntry(const QString& text, VipLogging::Level level, const QDateTime& date);
	static bool splitLogEntry(const QString& entry, QString& type, QString& date, QString& text);

	/// Returns all the log entries which are currently stored in the log shared memory (log entries with the VipLogging::SharedMemory
	/// output). Also clear the content of the shared memory.
	QStringList lastLogEntries();

protected:
	virtual void run();

private:
	struct LogFrame;
	void directLog(const LogFrame& log);
	// push a new log entry to the log buffer (mLogs)
	void pushLog(const LogFrame&);
	// pops and returns the oldest LogFrame of the log buffer
	bool popLog(LogFrame*);
	// number of log entries in the buffer
	int logCount();

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipLogging::Outputs)
Q_DECLARE_OPERATORS_FOR_FLAGS(VipLogging::Levels)

class VIP_LOGGING_EXPORT VipFileLogger
{
	QString d_identifier;
	QString d_directory;

public:
	VipFileLogger(const QString& identifier = "Log", const QString& directory = "./");
	virtual ~VipFileLogger() {}

	const QString& identifier() const { return d_identifier; }
	const QString& directory() const { return d_directory; }

	virtual QString canonicalFilePath() const = 0;
	virtual void addLogEntry(const QString& text, VipLogging::Level level, const QDateTime& time) = 0;
};

class VIP_LOGGING_EXPORT VipTextLogger : public VipFileLogger
{
	QFile d_file;

public:
	VipTextLogger(const QString& identifier, const QString& directory, bool overwrite = false);
	virtual QString canonicalFilePath() const;
	virtual void addLogEntry(const QString& text, VipLogging::Level level, const QDateTime& time);
};

inline QString vipLoggingCurrentLibrary()
{
#ifdef VIP_TARGET_NAME
	static QString name = QString(VIP_TARGET_NAME) + "    ";
	return name;
#else
	return QString();
#endif
}

/// \def VIP_DISABLE_LOG
/// \brief define VIP_DISABLE_LOG to disable logging.

/// \def VIP_ENABLE_LOG_DEBUG
/// \brief define VIP_ENABLE_LOG_DEBUG to enable debug logging (i.e. \ref VIP_LOG_DEBUG(text) expands to something).

/// \def VIP_LOG_INFO
/// \brief Log an information entry
/// \param text Log entry (QString type).
///
/// Always use this macro instead of the \ref VipLogging::Log() function.
/// Expands to nothing if \ref VIP_DISABLE_LOG is defined.

/// \def VIP_LOG_WARNING
/// \brief Log a warning entry
/// \param text Log entry (QString type).
///
/// Always use this macro instead of the \ref VipLogging::Log() function.
/// Expands to nothing if \ref VIP_DISABLE_LOG is defined.

/// \def VIP_LOG_ERROR
/// \brief Log an error entry
/// \param text Log entry (QString type).
///
/// Always use this macro instead of the \ref VipLogging::Log() function.
/// Expands to nothing if \ref VIP_DISABLE_LOG is defined.
///
/// VipLogging an error entry means than a unrecoverable error leading to a program exit just appends.
/// If this error does not necessarily leads to a program exist, consider using \ref VIP_LOG_WARNING() instead.

/// \def VIP_LOG_DEBUG
/// \brief Log a debug entry
/// \param text Log entry (QString type).
///
/// Always use this macro instead of the \ref VipLogging::Log() function.
/// Expands to nothing if \ref VIP_ENABLE_LOG_DEBUG is undefined or \ref VIP_DISABLE_LOG is defined.

namespace details
{
	template<int N>
	static inline const char* __build_str(const char str[N])
	{
		return str;
	}
	static inline const char* __build_str(const char* str)
	{
		return str;
	}
	static inline const char* __build_str(const std::string& str)
	{
		return str.c_str();
	}
	static inline const char* __build_str(const QByteArray& str)
	{
		return str.data();
	}
	static inline const char* __build_str(const QString& str)
	{
		return str.toLatin1().data();
	}

	template<int N>
	static inline QString __build_qstr(const char str[N])
	{
		return QString(str);
	}
	static inline QString __build_qstr(const char* str)
	{
		return QString(str);
	}
	static inline QString __build_qstr(const std::string& str)
	{
		return QString(str.c_str());
	}
	static inline QString __build_qstr(const QByteArray& str)
	{
		return QString(str);
	}
	static inline QString __build_qstr(const QString& str)
	{
		return str;
	}

	template<class T1>
	static inline QString _format(const T1& str)
	{
		return __build_qstr(str);
	}
	// template< class T1, class ...Args>
	//  static inline QString _format(const T1 & str, Args... args) { return QString::asprintf(details::__build_str(str), args...); }
	template<class T1, class T2>
	static inline QString _format(const T1& str, const T2& a2)
	{
		return QString::asprintf(details::__build_str(str), a2);
	}
	template<class T1, class T2, class T3>
	static inline QString _format(const T1& str, const T2& a2, const T3& a3)
	{
		return QString::asprintf(details::__build_str(str), a2, a3);
	}
	template<class T1, class T2, class T3, class T4>
	static inline QString _format(const T1& str, const T2& a2, const T3& a3, const T4& a4)
	{
		return QString::asprintf(details::__build_str(str), a2, a3, a4);
	}
	template<class T1, class T2, class T3, class T4, class T5>
	static inline QString _format(const T1& str, const T2& a2, const T3& a3, const T4& a4, const T5& a5)
	{
		return QString::asprintf(details::__build_str(str), a2, a3, a4, a5);
	}
	template<class T1, class T2, class T3, class T4, class T5, class T6>
	static inline QString _format(const T1& str, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6)
	{
		return QString::asprintf(details::__build_str(str), a2, a3, a4, a5, a6);
	}
	template<class T1, class T2, class T3, class T4, class T5, class T6, class T7>
	static inline QString _format(const T1& str, const T2& a2, const T3& a3, const T4& a4, const T5& a5, const T6& a6, const T7& a7)
	{
		return QString::asprintf(details::__build_str(str), a2, a3, a4, a5, a6, a7);
	}

	template<class... Args>
	static inline void concat_internal(QTextStream*)
	{
	}
	template<class T, class... Args>
	static inline void concat_internal(QTextStream* str, const T& val, Args... args)
	{
		(*str) << val;
		concat_internal<Args...>(str, std::forward<Args>(args)...);
	}
	template<class... Args>
	static inline QString concatenate(const QString& current_lib, Args... args)
	{
		QString res;
		{
			QTextStream str(&res, QIODevice::WriteOnly);
			str << current_lib;
			concat_internal(&str, std::forward<Args>(args)...);
		}
		return res;
	}
}

#ifdef VIP_DISABLE_LOG
#define VIP_LOG_INFO(...)
#define VIP_LOG_WARNING(...)
#define VIP_LOG_ERROR(...)
#else
#ifdef VIP_USE_OLD_LOGGING_SCHEME
#define VIP_LOG_INFO(...) VipLogging::instance().log(vipLoggingCurrentLibrary() + details::_format(__VA_ARGS__), VipLogging::Info)
#define VIP_LOG_WARNING(...) VipLogging::instance().log(vipLoggingCurrentLibrary() + details::_format(__VA_ARGS__), VipLogging::Warning)
#define VIP_LOG_ERROR(...) VipLogging::instance().log(vipLoggingCurrentLibrary() + details::_format(__VA_ARGS__), VipLogging::Error)
#else
#define VIP_LOG_INFO(...) VipLogging::instance().log(details::concatenate(vipLoggingCurrentLibrary(), __VA_ARGS__), VipLogging::Info)
#define VIP_LOG_WARNING(...) VipLogging::instance().log(details::concatenate(vipLoggingCurrentLibrary(), __VA_ARGS__), VipLogging::Warning)
#define VIP_LOG_ERROR(...) VipLogging::instance().log(details::concatenate(vipLoggingCurrentLibrary(), __VA_ARGS__), VipLogging::Error)
#endif
#endif

#if defined(VIP_ENABLE_LOG_DEBUG) && !defined(VIP_DISABLE_LOG)
#ifdef VIP_USE_OLD_LOGGING_SCHEME
#define VIP_LOG_DEBUG(...) VipLogging::instance().log(vipLoggingCurrentLibrary() + details::_format(__VA_ARGS__), VipLogging::Debug);
#else
#define VIP_LOG_DEBUG(...) VipLogging::instance().log(details::concatenate(vipLoggingCurrentLibrary(), __VA_ARGS__), VipLogging::Debug);
#endif
#else
#define VIP_LOG_DEBUG(...)
#endif

/// @}
// end Logging

#endif
