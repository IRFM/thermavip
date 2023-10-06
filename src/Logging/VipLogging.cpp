#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QSharedPointer>
#include <QFileInfo>
#include <iostream>

#include "VipLogging.h"


#define DATE_FORMAT "yy:MM:dd-hh:mm:ss.zzz"
#define DATE_SIZE 25





class VipLogging::PrivateData
{
public:
	PrivateData()
		: semaphore("Log", 1), stop(true), enable_saving(false), enabled(true)
	{}
	QList<LogFrame>					logs;
	QSystemSemaphore				semaphore;
	QSharedMemory					memory;
	QMutex							mutex;
	QSharedPointer<VipFileLogger>	file;
	Outputs							outputs;
	bool							stop;
	bool							enable_saving;
	bool							enabled;
	QStringList						saved;
};

/// Structure representing a log entry.
struct VipLogging::LogFrame
{
	QString text;
	Outputs outputs;
	Level level;
	QDateTime date;

	LogFrame(const QString& text = QString(), Level level = Info, Outputs outputs = Outputs(), const QDateTime& date = QDateTime::currentDateTime())
		:text(text),outputs(outputs),level(level), date(date) {}
};



void VipLogging::run()
{
	while(!d_data->stop)
	{
		while(logCount() > 0)
		{
			LogFrame frame;
			popLog(&frame);
			directLog(frame);
		}

		QThread::msleep(2);
	}

	while(logCount() > 0)
	{
		LogFrame frame;
		popLog(&frame);
		directLog(frame);
	}
}



VipLogging::VipLogging()
:QThread()
{
	d_data = new PrivateData();
}

VipLogging::VipLogging(Outputs outputs,VipFileLogger * logger)
:QThread()
{
	d_data = new PrivateData();
	open(outputs,logger);
}

VipLogging::VipLogging( Outputs outputs, const QString & identifier)
:QThread()
{
	d_data = new PrivateData();
	open(outputs,identifier);
}

VipLogging::~VipLogging()
{
	close();
	delete d_data;
}

VipLogging & VipLogging::instance()
{
	static VipLogging inst;
	return inst;
}

void VipLogging::pushLog(const LogFrame & l)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->logs.append(l);
}

bool VipLogging::popLog(LogFrame * ret)
{
	QMutexLocker lock(&d_data->mutex);
	if(d_data->logs.size() > 0)
	{
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

const VipFileLogger * VipLogging::logger() const
{
	return d_data->file.data();
}

QString VipLogging::identifier() const
{
	return d_data->memory.key();
}

QString VipLogging::filename() const
{
	if(d_data->file)
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

bool VipLogging::open(Outputs outputs, const QString & identifier)
{
	VipFileLogger * logger = NULL;
	if(!identifier.isEmpty() && (outputs & File))
		logger = new VipTextLogger(identifier,"./");
	return open(outputs,logger);
}

bool VipLogging::open(Outputs outputs, VipFileLogger * logger)
{
	close();

	QString identifier = "Log";
	if(logger)
		identifier = logger->identifier();

	QMutexLocker lock(&d_data->mutex);
	d_data->semaphore.setKey( identifier , 1);
	d_data->outputs = outputs;

	if(d_data->memory.isAttached())
		d_data->memory.detach();

	if(outputs & SharedMemory)
	{
		d_data->memory.setKey(identifier);
		if(!d_data->memory.create(10000))
		{
			if(!d_data->memory.attach())
				return false;
		}

		d_data->memory.lock();
		memset(d_data->memory.data(),0,d_data->memory.size());
		d_data->memory.unlock();
	}

	if(outputs & File)
	{
		d_data->file = QSharedPointer<VipFileLogger>(logger);
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
	if (isOpen())
	{
		{
			QMutexLocker lock(&d_data->mutex);
			d_data->stop = true;
		}
		this->wait();
	}

	QMutexLocker lock(&d_data->mutex);
	if(d_data->memory.isAttached())
			d_data->memory.detach();

	d_data->logs.clear();
	d_data->file.reset();
	d_data->outputs = Outputs();
}

bool VipLogging::waitForWritten(int msecs )
{
	qint64 start = QDateTime::currentMSecsSinceEpoch() ;
	while(logCount() > 0)
	{
		QThread::msleep(1);
		if(msecs >= 0 && QDateTime::currentMSecsSinceEpoch() - start > msecs)
			return false;;
	}
	return true;
}

void VipLogging::log(const QString & text, Level level, Outputs outputs , qint64 time)
{
	if (!isEnabled())
		return;

	if(time < 0)
		pushLog(LogFrame(text,level,outputs));
	else
		pushLog(LogFrame(text, level, outputs, QDateTime::fromMSecsSinceEpoch(time)));
}

void VipLogging::directLog(const QString & text, Level level , Outputs outputs, qint64 time)
{
	if (!isEnabled())
		return;

	if (time < 0)
		directLog(LogFrame(text, level, outputs));
	else
		directLog(LogFrame(text, level, outputs, QDateTime::fromMSecsSinceEpoch(time)));
}

void VipLogging::directLog(const LogFrame & frame)
{
	if (!isEnabled())
		return;

	Outputs out = frame.outputs;
	if(out < Cout)
		out = d_data->outputs;

	QByteArray log;

	QMutexLocker lock(&d_data->mutex);

	if(out & Cout)
	{
		if(log.isEmpty())
			log = formatLogEntry(frame.text,frame.level, frame.date);
		std::cout << log.data() ;
		std::cout.flush();
	}
	if((out & File) && d_data->file)
	{
		//if(d_data->semaphore.acquire())
		{
			d_data->file->addLogEntry(frame.text,frame.level, frame.date);
			//d_data->semaphore.release();
		}
	}
	if(out & SharedMemory)
	{
		if(log.isEmpty())
			log = formatLogEntry(frame.text,frame.level, frame.date);

		if(d_data->memory.lock())
		{
			qint32 size;
			memcpy(&size,d_data->memory.data(),sizeof(qint32));
			qint32 new_size = size + log.size();

			if(new_size+4 < d_data->memory.size())
			{
				memcpy(d_data->memory.data(),&new_size,sizeof(qint32));
				memcpy(static_cast<char*>(d_data->memory.data())+ size + sizeof(qint32),log.data(),log.size());
			}

			d_data->memory.unlock();
		}
	}
	if (d_data->enable_saving) {
		if (log.isEmpty())
			log = formatLogEntry(frame.text, frame.level, frame.date);
		d_data->saved.append(log);
	}
}

QStringList VipLogging::lastLogEntries()
{
	QMutexLocker lock(&d_data->mutex);

	QStringList lst;

	if(d_data->memory.lock())
	{
		qint32 size;
		memcpy(&size,d_data->memory.data(),sizeof(qint32));

		if(!size)
		{
			d_data->memory.unlock();
			return lst;
		}

		QString str(QByteArray(static_cast<char*>(d_data->memory.data()) + sizeof(qint32), size));
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
		lst = str.split("\n",VIP_SKIP_BEHAVIOR::SkipEmptyParts);
#else
		lst = str.split("\n", Qt::SkipEmptyParts);
#endif

		memset(d_data->memory.data(),0,d_data->memory.size());

		d_data->memory.unlock();
	}

	return lst;
}

bool VipLogging::splitLogEntry(const QString & entry, QString & type, QString & date, QString & text)
{
	if (entry.size() < DATE_SIZE + 10)
		return false;

	type = entry.mid(0, 10);
	date = entry.mid(10, DATE_SIZE);
	text = entry.mid(DATE_SIZE + 10);
	return true;
}

QByteArray VipLogging::formatLogEntry(const QString & text, VipLogging::Level level , const QDateTime & date)
{
	QByteArray log;

	if(level == Info)
		log = "Info";
	else if(level == Warning)
		log = "Warning";
	else if(level == Error)
		log = "Error";
	else if(level == Debug)
		log = "Debug";

	log.append(QByteArray(10-log.size(),' '));

	QByteArray time = date.toString(DATE_FORMAT).toLatin1();
	time.append(QByteArray(DATE_SIZE-time.size(),' '));

	log += time;

	int size = log.size();
	QStringList lst = text.split("\n");
	if(lst.size() > 1)
	{
		QByteArray prefix(size, char(' '));
		for(int i=0; i < lst.size(); ++i)
		{
			if(i > 0)
				log += prefix;
			log +=  lst[i].toLatin1() + "\n";
		}
	}
	else
	{
		log +=  text.toLatin1();
		log +=  "\n";
	}

	return log;
}






VipFileLogger::VipFileLogger(const QString & identifier, const QString & directory)
:d_identifier(identifier), d_directory(directory)
{
	d_directory.replace("\\","/");
	if(d_directory.endsWith("/"))
		d_directory.remove(d_directory.size()-1,1);
}


VipTextLogger::VipTextLogger(const QString & id, const QString & dir, bool overwrite)
:VipFileLogger(id,dir)
{
	QString filename = directory() + "/" + identifier() + ".txt";
	d_file.setFileName(filename);
	if(!d_file.open(QFile::ReadOnly) || overwrite)
	{
		d_file.close();
		//bool res = 
d_file.open(QFile::WriteOnly);

	}
	d_file.close();
}

QString VipTextLogger::canonicalFilePath() const
{
	return QFileInfo(d_file.fileName()).canonicalFilePath();
}

void VipTextLogger::addLogEntry(const QString & text, VipLogging::Level level , const QDateTime & date)
{
	QByteArray log = VipLogging::formatLogEntry(text,level,date);
	if(d_file.open(QFile::WriteOnly|QFile::Text|QFile::Append))
	{
		d_file.write(log);
		d_file.close();
	}
}
