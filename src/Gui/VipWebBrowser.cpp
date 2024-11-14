/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#include <qboxlayout.h>

#include "VipDisplayArea.h"
#include "VipProgress.h"
#include "VipWebBrowser.h"

#ifdef __VIP_USE_WEB_ENGINE

#include <qwebenginecertificateerror.h>
#include <qwebenginesettings.h>

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
  : QToolBar(parent)
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
	  : QWebEnginePage(parent)
	{
	}

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
  : VipWidgetPlayer(makeWebBrowserWidget(), parent)
{
	this->setObjectName("VipWebBrowser");
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->view = widget()->findChild<QWebEngineView*>();
	d_data->bar = widget()->findChild<VipWebBrowserToolBar*>();

	webEngine()->setPage(new WebPage());

	connect(webEngine(), SIGNAL(titleChanged(const QString&)), this, SLOT(setWindowTitle(const QString&)));
	connect(d_data->bar->prev, SIGNAL(triggered(bool)), webEngine(), SLOT(back()));
	connect(d_data->bar->next, SIGNAL(triggered(bool)), webEngine(), SLOT(forward()));
	connect(d_data->bar->reload, SIGNAL(triggered(bool)), webEngine(), SLOT(reload()));
	connect(d_data->bar->stop, SIGNAL(triggered(bool)), webEngine(), SLOT(stop()));
	connect(&d_data->bar->url, SIGNAL(returnPressed()), this, SLOT(setUrlInternal()));
	connect(webEngine(), SIGNAL(iconChanged(const QIcon&)), &d_data->bar->url, SLOT(setIcon(const QIcon&)));
	connect(webEngine(), SIGNAL(urlChanged(const QUrl&)), this, SLOT(displayUrl(const QUrl&)));

	connect(webEngine(), SIGNAL(loadStarted()), this, SLOT(loadStarted()));
	connect(webEngine(), SIGNAL(loadProgress(int)), this, SLOT(loadProgress(int)));
	connect(webEngine(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));

	connect(webEngine(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));

	connect(webEngine()->page(), SIGNAL(featurePermissionRequested(const QUrl&, QWebEnginePage::Feature)), this, SLOT(featurePermissionRequested(const QUrl&, QWebEnginePage::Feature)));

	webEngine()->page()->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, true);
}

VipWebBrowser::~VipWebBrowser() {}

QWebEngineView* VipWebBrowser::webEngine() const
{
	return d_data->view;
}

void VipWebBrowser::featurePermissionRequested(const QUrl&, // securityOrigin,
					       QWebEnginePage::Feature feature)
{

	// grant permission
	webEngine()->page()->setFeaturePermission(webEngine()->page()->url(), feature, QWebEnginePage::PermissionGrantedByUser);
}

void VipWebBrowser::setUrlInternal()
{
	webEngine()->load(QUrl(d_data->bar->url.text()));
}
void VipWebBrowser::displayUrl(const QUrl& url)
{
	d_data->bar->url.setText(url.toString());
}

void VipWebBrowser::loadStarted()
{
	d_data->bar->load.setValue(0);
	d_data->bar->loadAction->setVisible(true);
	d_data->bar->stop->setVisible(true);
}
void VipWebBrowser::loadProgress(int progress)
{
	d_data->bar->load.setValue(progress);
	if (progress == 100)
		loadFinished(true);
}
void VipWebBrowser::loadFinished(bool ok)
{
	(void)ok;
	d_data->bar->loadAction->setVisible(false);
	d_data->bar->stop->setVisible(false);
}

QWidget* VipWebBrowser::widgetForMouseEvents() const
{
	return widget();
	// QObjectList lst = webEngine()->children();
	//  for (int i = 0; i < lst.size(); ++i) {
	//  if (QWidget* w = qobject_cast<QWidget*>(lst[i]))
	//  return w;
	//  }
	//  return nullptr;
}

QToolBar* VipWebBrowser::playerToolBar() const
{
	return d_data->bar;
}

void VipWebBrowser::setUrl(const QString& url)
{
	webEngine()->load(QUrl(url));
}

VipArchive& operator<<(VipArchive& arch, const VipWebBrowser* browser)
{
	return arch.content("url", browser->webEngine()->page()->url().toString()).content("toolBarVisible", browser->playerToolBar()->isVisible());
}

VipArchive& operator>>(VipArchive& arch, VipWebBrowser* browser)
{
	arch.save();
	QString url;
	bool toolBarVisible = true;
	arch.content("url", url);
	if (arch.content("toolBarVisible", toolBarVisible)) {
		browser->webEngine()->page()->setUrl(QUrl(url));
		browser->playerToolBar()->setVisible(toolBarVisible);
		if (!toolBarVisible)
			browser->playerToolBar()->hide();
	}
	else
		arch.restore();
	return arch;
}

static int registerArchive()
{
	vipRegisterArchiveStreamOperators<VipWebBrowser*>();
	return 0;
}
static int _registerArchive = registerArchive();

#endif // __VIP_USE_WEB_ENGINE
