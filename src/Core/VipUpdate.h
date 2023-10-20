#ifndef VIP_UPDATE_H
#define VIP_UPDATE_H



#include "VipConfig.h"
#include <qobject.h>

class QProcess;

/// VipUpdate is used to update a Thermavip copy based on the viptools process.
/// For VipUpdate to work properly, you need the viptools process in the same directory as your Thermavip installation.
class VIP_CORE_EXPORT VipUpdate : public QObject
{
	Q_OBJECT

public:
	VipUpdate(QObject * parent =nullptr);
	~VipUpdate();

	static QString getUpdateProgram();

public Q_SLOTS:

	///Stops the current process (download or update)
	bool stop();

	///Checks if updates are available based on given output directory containing the Thermavip installation.
	int hasUpdate(const QString & out_dir, bool * already_downloaded = nullptr,  void ** stop = nullptr);

	///Tells if all updates has been downloaded
	bool isDownloadFinished();

	///Download the updates based on given output directory, but do not copy or remove files into the output directory
	bool startDownload(const QString & out_dir);

	///Updates output directory. This will download all needed files (if required) and copy them to the output directory.
	bool startUpdate(const QString & out_dir);

	///When this object is destroyed, the current process (update or download) will keep going if detachedOnQuit is true
	void setDetachedOnQuit(bool enable);
	bool detachedOnQuit() const;

	///Returns the underlying QProcess object
	QProcess * process();

	///When updating Thermavip, some new files might not be copied into the installation directory, mainly because Thermavip is still running.
	/// In this case, the files are still copied to the installation directory, but with a name ending with '.vipnewfile'.
	/// This function recursively walks through the installation directory and tries to rename all new files by removing the trailing '.vipnewfile'.
	bool renameNewFiles(const QString & dir_name);

private Q_SLOTS:
	void emitFinished();
	void newOutput();

Q_SIGNALS:
	void updateProgressed(int);
	void finished();

private:
	class PrivateData;
	PrivateData * m_data;
};



#endif

