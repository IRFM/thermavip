#ifndef FFMPEG_PLUGIN_H
#define FFMPEG_PLUGIN_H


#include "VipPlugin.h"
#include "RecordWindow.h"

#include <qpointer.h>
#include <qtoolbutton.h>
#include <qcombobox.h>


class OpenStream : public QWidget
{
	Q_OBJECT

public:
	OpenStream();

	QString path() const;
	QStringList recentPaths() const;
	void setRencentPaths(const QStringList & lst);
private Q_SLOTS:
	void openFilePath();
	void open();
private:
	QComboBox m_paths;
	QToolButton m_open;
};



class QAction;

class FfmpegInterface : public QObject, public VipPluginInterface
{
	Q_OBJECT
		Q_PLUGIN_METADATA(IID "thermadiag.thermavip.VipPluginInterface")
		//Q_PLUGIN_METADATA(IID "InfraTechPluginh")
		Q_INTERFACES(VipPluginInterface)
public:

	virtual LoadResult load();
	virtual QByteArray pluginVersion() { return "2.1.0"; }
	virtual void unload() {}
	virtual QString author() { return "Victor Moncada(victor.moncada@cea.fr)"; }
	virtual QString description() { return "Defines interfaces to read/create video files and manage the webcam"; }
	virtual QString link() { return QString(); }
	virtual bool hasExtraCommands() { return true; }
	virtual void save(VipArchive &);
	virtual void restore(VipArchive &);
public Q_SLOTS:
	void openVideoStream(QAction * action);

	void setRecording(bool);

private:
	QPointer<QToolButton> m_rec;
	QPointer<RecordWindow> m_rec_win;
	QPointer<OpenStream> m_open_stream;
};

#endif
