#include <qboxlayout.h>


#include "VipWebBrowser.h"
#include "VipDisplayArea.h"
#include "VipProgress.h"

#ifdef __VIP_USE_WEB_ENGINE

#include <qwebenginesettings.h>
#include <qwebenginecertificateerror.h>

bool VipHTTPFileHandler::open(const QString& path, QString* error)
{
	(void)error;
	VipDisplayArea* area = vipGetMainWindow()->displayArea();
	VipDisplayPlayerArea* tab = new VipDisplayPlayerArea();
	area->addWidget(tab);
	VipWebBrowser* browser = new VipWebBrowser();
	browser->setUrl(path);

	VipBaseDragWidget* w = vipCreateFromBaseDragWidget(vipCreateFromWidgets(QWidgetList() << browser));
	tab->addWidget(w);


	return true;
}




VipWebBrowserToolBar::VipWebBrowserToolBar(QWidget* parent)
	:QToolBar(parent)
{
	prev = addAction(vipIcon("rotate_left.png"), "Previous page");
	next = addAction(vipIcon("rotate_right.png"), "Next page");
	reload = addAction(vipIcon("refresh.png"), "Reload page");
	QSizePolicy sp = url.sizePolicy();
	sp.setHorizontalStretch(1);
	url.setSizePolicy(sp);
	urlAction = addWidget(&url);

	stop = addAction(vipIcon("stop.png"), "Stop loading content");
	load.setRange(0, 100);
	load.setMaximumHeight(20);
	loadAction = addWidget(&load);


	loadAction->setVisible(false);
	stop->setVisible(false);

	sp = sizePolicy();
	sp.setHorizontalStretch(1);
	setSizePolicy(sp);
}

void VipWebBrowserToolBar::setIcon(const QIcon& icon)
{
	url.setIcon(icon);
}




class WebPage : public QWebEnginePage
{
public:
	explicit WebPage(QWidget* parent = 0)
		:QWebEnginePage(parent) {}

protected:
	virtual bool certificateError(const QWebEngineCertificateError& error) override
	{
		QWebEngineCertificateError& mutableError = const_cast<QWebEngineCertificateError&>(error);
		mutableError.ignoreCertificateError();
		return true;
	}
};



class VipWebBrowser::PrivateData
{
public:
	QWebEngineView* view;
	VipWebBrowserToolBar* bar;
};

static QWidget* makeWebBrowserWidget()
{
	QWidget* w = new QWidget();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(new VipWebBrowserToolBar());
	lay->addWidget(new QWebEngineView());
	w->setLayout(lay);
	return w;
}

VipWebBrowser::VipWebBrowser(QWidget* parent)
	:VipWidgetPlayer(makeWebBrowserWidget(), parent)
{
	this->setObjectName("VipWebBrowser");
	m_data = new PrivateData();
	m_data->view = widget()->findChild< QWebEngineView*>();
	m_data->bar = widget()->findChild< VipWebBrowserToolBar*>();

	webEngine()->setPage(new WebPage());

	connect(webEngine(),SIGNAL(titleChanged(const QString&)), this, SLOT(setWindowTitle(const QString&)));
	connect(m_data->bar->prev, SIGNAL(triggered(bool)), webEngine(), SLOT(back()));
	connect(m_data->bar->next, SIGNAL(triggered(bool)), webEngine(), SLOT(forward()));
	connect(m_data->bar->reload, SIGNAL(triggered(bool)), webEngine(), SLOT(reload()));
	connect(m_data->bar->stop, SIGNAL(triggered(bool)), webEngine(), SLOT(stop()));
	connect(&m_data->bar->url, SIGNAL(returnPressed()), this, SLOT(setUrlInternal()));
	connect(webEngine(), SIGNAL(iconChanged(const QIcon&)), &m_data->bar->url, SLOT(setIcon(const QIcon &)));
	connect(webEngine(), SIGNAL(urlChanged(const QUrl&)), this, SLOT(displayUrl(const QUrl&)));

	connect(webEngine(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
	connect(webEngine(), SIGNAL(loadProgress(int)), this, SLOT(loadProgress(int)));
	connect(webEngine(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));

	connect(webEngine(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));

	connect(webEngine()->page(),
		SIGNAL(featurePermissionRequested(const QUrl&, QWebEnginePage::Feature)), this,
		SLOT(featurePermissionRequested(const QUrl&, QWebEnginePage::Feature)));


	webEngine()->page()->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
}

QWebEngineView* VipWebBrowser::webEngine() const
{
	return m_data->view;
}

void VipWebBrowser::featurePermissionRequested(const QUrl& ,//securityOrigin,
	QWebEnginePage::Feature feature)
{

	// grant permission
	webEngine()->page()->setFeaturePermission(webEngine()->page()->url(),
		feature, QWebEnginePage::PermissionGrantedByUser);
}


void VipWebBrowser::setUrlInternal()
{
	webEngine()->load(QUrl(m_data->bar->url.text()));
}
void VipWebBrowser::displayUrl(const QUrl& url)
{
	m_data->bar->url.setText(url.toString());
}

void VipWebBrowser::loadStarted()
{
	m_data->bar->load.setValue(0);
	m_data->bar->loadAction->setVisible(true);
	m_data->bar->stop->setVisible(true);
}
void VipWebBrowser::loadProgress(int progress)
{
	m_data->bar->load.setValue(progress);
	if (progress == 100)
		loadFinished(true);
}
void VipWebBrowser::loadFinished(bool ok)
{
	(bool)ok;
	m_data->bar->loadAction->setVisible(false);
	m_data->bar->stop->setVisible(false);
}

QWidget* VipWebBrowser::widgetForMouseEvents() const
{
	return widget();
	//QObjectList lst = webEngine()->children();
	// for (int i = 0; i < lst.size(); ++i) {
	// if (QWidget* w = qobject_cast<QWidget*>(lst[i]))
	// return w;
	// }
	// return NULL;
}

QToolBar* VipWebBrowser::playerToolBar() const
{
	return m_data->bar;
}

void VipWebBrowser::setUrl(const QString& url)
{
	webEngine()->load(QUrl(url));
}



VipArchive& operator<<(VipArchive& arch, const VipWebBrowser* browser)
{
	return arch.content("url", browser->webEngine()->page()->url().toString()).content("toolBarVisible", browser->playerToolBar()->isVisible());
}

VipArchive& operator >>(VipArchive& arch, VipWebBrowser* browser)
{
	arch.save();
	QString url;
	bool toolBarVisible = true;
	arch.content("url", url);
	if (arch.content("toolBarVisible", toolBarVisible)) {
		browser->webEngine()->page()->setUrl(QUrl(url));
		browser->playerToolBar()->setVisible(toolBarVisible);
		if (!toolBarVisible) browser->playerToolBar()->hide();
	}
	else
		arch.restore();
	return arch;
}

static int registerArchive()
{
	vipRegisterArchiveStreamOperators< VipWebBrowser*>();
	return 0;
}
static int _registerArchive = registerArchive();

#endif // __VIP_USE_WEB_ENGINE
