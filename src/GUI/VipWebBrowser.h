#pragma once

#ifdef __VIP_USE_WEB_ENGINE

#define VIP_USE_WEB_ENGINE

#include <qwebengineview.h>
#include <qprogressbar.h>

#include "VipStandardWidgets.h"
#include "VipIODevice.h"
#include "VipPlayer.h"

static QRegExp& regExpForIp()
{
	static QRegExp inst("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})");
	return inst;
}

/// Manage HTML files as well as http addresses when opening a file from Thermavip tool bar
class VipHTTPFileHandler : public VipFileHandler
{
	Q_OBJECT
public:
	virtual bool open(const QString& path, QString* error);
	virtual QString fileFilters() const { return "HTML files (*.html)"; }
	bool probe(const QString& filename, const QByteArray&) const
	{
		QString file(removePrefix(filename));
		return (QFileInfo(file).suffix().compare("html", Qt::CaseInsensitive) == 0 || file.startsWith("http://") || file.startsWith("https://") || regExpForIp().indexIn(file) == 0);
	}
};
VIP_REGISTER_QOBJECT_METATYPE(VipHTTPFileHandler*)


class VipWebBrowserToolBar : public QToolBar
{
	Q_OBJECT
public:
	QAction* prev;
	QAction* next;
	QAction* reload;
	QAction* urlAction;
	QAction* loadAction;

	VipLineEditIcon url;
	QProgressBar load;
	QAction* stop;

	VipWebBrowserToolBar(QWidget * parent = NULL);

public Q_SLOTS:
	void setIcon(const QIcon& icon);
};

class VipWebBrowser : public VipWidgetPlayer
{
	Q_OBJECT

public:
	VipWebBrowser(QWidget* parent = NULL);

	QWebEngineView* webEngine() const;
	virtual QWidget* widgetForMouseEvents() const;
	virtual QToolBar* playerToolBar() const;
public Q_SLOTS:
	void setUrl(const QString& url);

private Q_SLOTS:
	void setUrlInternal();
	void displayUrl(const QUrl& url);
	void loadStarted();
	void loadProgress(int progress);
	void loadFinished(bool ok);
	void featurePermissionRequested(const QUrl& securityOrigin,
		QWebEnginePage::Feature feature);
private:
	class PrivateData;
	PrivateData* m_data;

};
VIP_REGISTER_QOBJECT_METATYPE(VipWebBrowser*)

#endif //__VIP_USE_WEB_ENGINE
