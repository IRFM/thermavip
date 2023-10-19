#include "VipUpdate.h"
#include "VipLogging.h"

#include <QProcess>
#include <QTextStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <qcoreapplication.h>
#include <iostream>



class DetachableProcess : public QProcess
{
public:
	DetachableProcess(QObject *parent = 0) : QProcess(parent) {}
	void detach()
	{
		this->waitForStarted();
		setProcessState(QProcess::NotRunning);
	}
};


class VipUpdate::PrivateData
{
public:
	PrivateData() :detachedOnQuit(false), progressed(-1) {}
	DetachableProcess process;
	bool detachedOnQuit;
	int progressed;
};




VipUpdate::VipUpdate(QObject * parent)
	:QObject(parent)
{
	m_data = new PrivateData();
}

VipUpdate::~VipUpdate()
{
	if(!m_data->detachedOnQuit)
		stop();
	else
		m_data->process.detach();
	delete m_data;
}


QString VipUpdate::getUpdateProgram()
{
	static QString update_program;
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		QString thermavipdir = QFileInfo(QCoreApplication::instance()->arguments()[0]).canonicalPath();
		//printf("path: %s\n", thermavipdir.toLatin1().data());

		QFileInfoList lst = QDir(thermavipdir).entryInfoList(QStringList() << "*.exe", QDir::Files | QDir::Executable);
		for (int i = 0; i < lst.size(); ++i) {
			QString fname = lst[i].fileName();
			//printf("%s\n", fname.toLatin1().data());
			if (fname.startsWith("vipupdate") && fname.endsWith(".exe"))
				return update_program = fname;
		}
	}
	return update_program;
}

void VipUpdate::setDetachedOnQuit(bool enable)
{
	m_data->detachedOnQuit = enable;
}

bool VipUpdate::detachedOnQuit() const
{
	return m_data->detachedOnQuit;
}

bool VipUpdate::stop()
{
	disconnect(&m_data->process,SIGNAL(readyReadStandardOutput()),this,SLOT(newOutput()));
	disconnect(&m_data->process,SIGNAL(finished(int, QProcess::ExitStatus)),this,SLOT(emitFinished()));

	m_data->process.terminate();
	if (!m_data->process.waitForFinished(10000))
		m_data->process.kill();
	return m_data->process.state() == QProcess::NotRunning;
}

int VipUpdate::hasUpdate(const QString & out_dir, bool * already_downloaded, void ** _stop)
{
	//printf("update out_dir: %s\n", out_dir.toLatin1().data());
	if (already_downloaded)
		*already_downloaded = false;

	if(!stop())
		return -1;

	//printf("start '%s'\n", (getUpdateProgram() + " -c --hide -o " + out_dir).toLatin1().data());
	//m_data->process.start(getUpdateProgram(), QStringList()<<"-c"<<"--hide"<<"-o"<<out_dir);
	m_data->process.start(getUpdateProgram() + " -c --hide -o " + out_dir);
	m_data->process.waitForStarted(3000);

	if (!_stop)
	{
		if (!m_data->process.waitForFinished(30000))
			return -1;
	}
	else
	{
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		while (*_stop)
		{
			if (m_data->process.waitForFinished(500))
				break;
			else
			{
				if (QDateTime::currentMSecsSinceEpoch() - st > 30000)
					return -1;
			}
		}
	}

	QByteArray ar = m_data->process.readAllStandardOutput();
	QByteArray err = m_data->process.readAllStandardError();
	//if(err.size())
	// VIP_LOG_ERROR(err);
	// if (ar.size())
	// VIP_LOG_INFO(ar);
	if(ar.isEmpty())
		return -1;
	QTextStream stream(ar);
	int count;
	int downloaded;
	stream >> count >> downloaded;
	if (stream.status() == QTextStream::Ok)
	{
		if (already_downloaded) * already_downloaded = downloaded;
		return count;
	}
	return 0;
}

bool VipUpdate::isDownloadFinished()
{
	if(!stop())
		return false;

	//m_data->process.start(getUpdateProgram(), QStringList()<< "-w"<< "--hide");
	m_data->process.start(getUpdateProgram() + " -w --hide");
	if(!m_data->process.waitForFinished(30000))
		return false;

	QByteArray ar = m_data->process.readAllStandardOutput();
	if(ar.isEmpty())
		return false;
	QTextStream stream(ar);
	int count;
	stream >> count;
	if(stream.status() == QTextStream::Ok)
		return count;
	return false;
}

bool VipUpdate::startDownload(const QString & out_dir)
{
	if(!stop())
		return false;

	m_data->progressed=-1;
	connect(&m_data->process,SIGNAL(readyReadStandardOutput()),this,SLOT(newOutput()),Qt::DirectConnection);
	connect(&m_data->process,SIGNAL(finished(int, QProcess::ExitStatus)),this,SLOT(emitFinished()),Qt::DirectConnection);
	
	//m_data->process.start(getUpdateProgram() ,QStringList()<< "-u"<< "-d" << "--hide" << "-o" << out_dir);
	m_data->process.start(getUpdateProgram() + " -u -d --hide -o " + out_dir);
	return m_data->process.waitForStarted(10000);
}

bool VipUpdate::startUpdate(const QString & out_dir)
{
	if(!stop())
		return false;

	m_data->progressed=-1;
	connect(&m_data->process,SIGNAL(readyReadStandardOutput()),this,SLOT(newOutput()),Qt::DirectConnection);
	connect(&m_data->process,SIGNAL(finished(int, QProcess::ExitStatus)),this,SLOT(emitFinished()),Qt::DirectConnection);
	
	//m_data->process.start(getUpdateProgram() ,QStringList()<< "-u" <<"--hide"<< "-o" << out_dir);
	m_data->process.start(getUpdateProgram() + " -u --hide -o " + out_dir);
	return m_data->process.waitForStarted(30000);
}

QProcess * VipUpdate::process() {return &m_data->process;}

void VipUpdate::emitFinished()
{
	Q_EMIT finished();
}

void VipUpdate::newOutput()
{
	QByteArray ar = m_data->process.readAllStandardOutput();
	QList<QByteArray> lines = ar.split(' ');
	if(lines.size() > 1)
	{
		QByteArray value = lines[lines.size()-2];
		value.replace(' ',"");
		bool ok;
		int count = qRound(value.toDouble(&ok));
		if(ok && count != m_data->progressed)
		{
			m_data->progressed = count;
			Q_EMIT updateProgressed(count);
		}
	}
}


#include <qdiriterator.h>
// #include "qt_windows.h"
// static std::string GetLastErrorAsString()
// {
// //Get the error message, if any.
// DWORD errorMessageID = ::GetLastError();
// if (errorMessageID == 0)
// return std::string(); //No error message has been recorded
//
// LPSTR messageBuffer = nullptr;
// size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
// NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
//
// std::string message(messageBuffer, size);
//
// //Free the buffer.
// LocalFree(messageBuffer);
//
// return message;
// }
bool VipUpdate::renameNewFiles(const QString & dir_name)
{
	QStringList files;

	bool has_opened_files = false;

	if (QDir(dir_name).exists())
	{
		QString m_inDir = dir_name;
		m_inDir.replace("\\", "/");
		if (!m_inDir.endsWith("/")) m_inDir += "/";
		QDirIterator it(m_inDir, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString next = it.next();
			QFileInfo info(next);
			if (info.exists() && !info.isDir() && info.suffix() == "vipnewfile")
			{
				QString file = info.canonicalFilePath();
				files.append(file);

				QFile in(file);
				if (!in.open(QFile::WriteOnly | QFile::Append))
				{
					VIP_LOG_WARNING("Cannot rename file " + QFileInfo(file).fileName() );
					has_opened_files = true;
				}
			}
		}

	}

	for (int i = 0; i < files.size(); ++i)
	{

		QString new_name = files[i];
		new_name.remove(".vipnewfile");


		if (QFileInfo(new_name).exists())
		{

			int cr = remove(new_name.toLatin1().data());
			if (cr != 0)
			{
				//VIP_LOG_WARNING("Cannot remove file " + QFileInfo(new_name).fileName());
			}
			else if (!QFile::rename(files[i], new_name))
				return false;
		}
	}

	return !has_opened_files;
}




