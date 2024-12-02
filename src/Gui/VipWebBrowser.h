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

#ifndef VIP_WEB_BROWSER_H
#define VIP_WEB_BROWSER_H

#ifdef __VIP_USE_WEB_ENGINE

#define VIP_USE_WEB_ENGINE

#include <qprogressbar.h>
#include <qwebengineview.h>

#include "VipIODevice.h"
#include "VipPlayer.h"
#include "VipStandardWidgets.h"


/// @brief Manage HTML files as well as http addresses when opening a file from Thermavip tool bar.
/// 
/// HTTP/IP addresses and HTML files are opened in a VipWebBrowser player.
/// 
class VIP_GUI_EXPORT VipHTTPFileHandler : public VipFileHandler
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

	static inline QRegExp& regExpForIp()
	{
		static QRegExp inst("(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})");
		return inst;
	}
};
VIP_REGISTER_QOBJECT_METATYPE(VipHTTPFileHandler*)

/// @brief Tool bar of a VipWebBrowser
class VIP_GUI_EXPORT VipWebBrowserToolBar : public QToolBar
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

	VipWebBrowserToolBar(QWidget* parent = nullptr);

public Q_SLOTS:
	void setIcon(const QIcon& icon);
};

/// @brief A player used to display a lightweight web browser
///
class VIP_GUI_EXPORT VipWebBrowser : public VipWidgetPlayer
{
	Q_OBJECT

public:
	VipWebBrowser(QWidget* parent = nullptr);
	~VipWebBrowser();

	QWebEngineView* webEngine() const;
	virtual QWidget* widgetForMouseEvents() const;
	virtual QToolBar* playerToolBar() const;

	/// @brief Helper function, open a web browser in a new workspace
	static void openWebBrowser(const QString& url);

public Q_SLOTS:
	void setUrl(const QString& url);

private Q_SLOTS:
	void setUrlInternal();
	void displayUrl(const QUrl& url);
	void loadStarted();
	void loadProgress(int progress);
	void loadFinished(bool ok);
	void featurePermissionRequested(const QUrl& securityOrigin, QWebEnginePage::Feature feature);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};
VIP_REGISTER_QOBJECT_METATYPE(VipWebBrowser*)

#endif //__VIP_USE_WEB_ENGINE

#endif // VIP_WEB_BROWSER_H