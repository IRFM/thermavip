/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipAbout.h"
#include "VipGui.h"
#include "VipPlugin.h"
#include "VipStandardWidgets.h"

#include <qboxlayout.h>
#include <qdesktopservices.h>
#include <qevent.h>
#include <qlistwidget.h>
#include <qpushbutton.h>
#include <qtextedit.h>

class VipAboutDialog::PrivateData
{
public:
	QLabel* thermadiag;
	QLabel* infos;
	QListWidget* products;
	QLabel* warning;
	QPushButton* ok;
};

VipAboutDialog::VipAboutDialog()
  : QDialog(vipGetMainWindow())
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->thermadiag = new QLabel();
	d_data->infos = new QLabel();
	d_data->products = new QListWidget();
	d_data->warning = new QLabel();
	d_data->ok = new QPushButton();

	d_data->thermadiag->setStyleSheet("QLabel {background: #2D2D30; }");
	d_data->thermadiag->setPixmap(vipPixmap("Thermavip_banner.png"));
	d_data->thermadiag->setAlignment(Qt::AlignCenter);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	const QPixmap pix = d_data->thermadiag->pixmap() ? *d_data->thermadiag->pixmap() : QPixmap();
#else
	const QPixmap pix = d_data->thermadiag->pixmap(Qt::ReturnByValue);
#endif
	d_data->thermadiag->setMinimumWidth(pix.width());   // +20);
	d_data->thermadiag->setMinimumHeight(pix.height()); // +20);
	d_data->thermadiag->setCursor(QCursor(Qt::PointingHandCursor));
	d_data->thermadiag->installEventFilter(this);
	// d_data->thermadiag->setProperty("href", QString("http://thermadiag.com/index.php/en/"));

	QString infos = "Thermavip ";
	if (!vipEditionVersion().isEmpty())
		infos += vipEditionVersion();
	QDateTime build_time = QDateTime::fromMSecsSinceEpoch(vipBuildTime());
	infos += "<br>Version " + QString(VIP_VERSION) + ", " + build_time.toString("dd MMM yyyy, hh:mm:ss");
	infos += "<br>&#9400; " + build_time.toString("yyyy") + " <a href='http://irfm.cea.fr/'>CEA/IRFM</a>";
	infos += "<br>All rights reserved";
	infos += "<p>Installed products :</p>";
	d_data->infos->setText(infos);
	d_data->infos->setAlignment(Qt::AlignLeft);
	d_data->infos->setTextFormat(Qt::RichText);
	d_data->infos->setTextInteractionFlags(Qt::TextBrowserInteraction);
	d_data->infos->setOpenExternalLinks(true);
	d_data->infos->setStyleSheet("QLabel {margin: 5px;}");

	QList<VipPluginInterface*> interfaces = VipLoadPlugins::instance().loadedPlugins();
	QStringList names = VipLoadPlugins::instance().loadedPluginNames();
	for (int i = 0; i < interfaces.size(); ++i) {
		QString text = names[i] + " " + interfaces[i]->pluginVersion();
		QLabel* pl = new QLabel(text);
		pl->setObjectName("label");
		pl->setToolTip(interfaces[i]->description());
		pl->setStyleSheet("QLabel#label{background: transparent;}");

		QListWidgetItem* item = new QListWidgetItem();
		item->setSizeHint(pl->sizeHint());
		d_data->products->addItem(item);
		d_data->products->setItemWidget(item, pl);
	}
	d_data->products->setStyleSheet("QListWidget {margin: 5px;}");
	// d_data->products->setMaximumWidth(500);

	d_data->warning->setText(
	  tr("This Software is protected by copyright law and international treaties. "
	     "This Software is licensed (not sold), and its use is subject to a valid WRITTEN AND SIGNED Communique(r) License Agreement. "
	     "The unauthorized use, copying or distribution of this Software may result in severe criminal or civil penalties, and will be prosecuted to the maximum extent allowed by law."));
	d_data->warning->setWordWrap(true);
	d_data->warning->setAlignment(Qt::AlignLeft);
	d_data->warning->setStyleSheet("QLabel {margin: 5px;}");

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->thermadiag);
	vlay->addWidget(d_data->infos);
	vlay->addWidget(d_data->products);
	vlay->addWidget(d_data->warning);
	// vlay->addWidget(VipLineWidget::createHLine());

	d_data->ok->setText("Ok");
	d_data->ok->setStyleSheet("QPushButton {margin: 5px;}");

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addStretch(1);
	hlay->addWidget(d_data->ok);
	vlay->addLayout(hlay);
	vlay->setContentsMargins(0, 0, 0, 0);

	if (QFileInfo("changelog.txt").exists()) {
		// display changelog
		QTabWidget* tab = new QTabWidget();
		QWidget* about = new QWidget();

		QColor c = vipWidgetTextBrush(vipGetMainWindow()).color();
		bool dark_skin = (c.redF() > 0.9 && c.greenF() > 0.9 && c.blueF() > 0.9);
		if (dark_skin) {
			about->setStyleSheet("background: #272822;");
		}
		about->setLayout(vlay);
		QTextEdit* edit = new QTextEdit();
		edit->setReadOnly(true);
		QFile fin("changelog.txt");
		fin.open(QFile::ReadOnly | QFile::Text);
		edit->setPlainText(fin.readAll());
		tab->addTab(about, "About");
		tab->addTab(edit, "Changelog");

		QVBoxLayout* l = new QVBoxLayout();
		l->setContentsMargins(0, 0, 0, 0);
		l->addWidget(tab);
		setLayout(l);
	}
	else
		setLayout(vlay);

	connect(d_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
}

VipAboutDialog::~VipAboutDialog()
{
}

bool VipAboutDialog::eventFilter(QObject* watched, QEvent* evt)
{
	if (evt->type() == QEvent::MouseButtonPress) {
		if (static_cast<QMouseEvent*>(evt)->buttons() & Qt::LeftButton) {
			QString href = watched->property("href").toString();
			if (!href.isEmpty()) {
				QDesktopServices::openUrl(QUrl(href));
			}
		}
	}

	return false;
}
