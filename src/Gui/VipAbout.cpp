#include "VipAbout.h"
#include "VipPlugin.h"
#include "VipStandardWidgets.h"
#include "VipGui.h"

#include <qlistwidget.h>
#include <qpushbutton.h>
#include <qboxlayout.h>
#include <qevent.h>
#include <qdesktopservices.h>
#include <qtextedit.h>

class VipAboutDialog::PrivateData
{
public:
	QLabel *thermadiag;
	QLabel *infos;
	QListWidget * products;
	QLabel * warning;
	QPushButton * ok;
};



VipAboutDialog::VipAboutDialog()
	:QDialog(vipGetMainWindow())
{
	m_data = new PrivateData();
	m_data->thermadiag = new QLabel();
	m_data->infos = new QLabel();
	m_data->products = new QListWidget();
	m_data->warning = new QLabel();
	m_data->ok = new QPushButton();

	m_data->thermadiag->setStyleSheet("QLabel {background: #2D2D30; }");
	m_data->thermadiag->setPixmap(vipPixmap("Thermavip_banner.png"));
	m_data->thermadiag->setAlignment(Qt::AlignCenter);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	const QPixmap pix = m_data->thermadiag->pixmap() ? *m_data->thermadiag->pixmap() : QPixmap();
#else
	const QPixmap pix = m_data->thermadiag->pixmap(Qt::ReturnByValue);
#endif
	m_data->thermadiag->setMinimumWidth(pix.width());			      // +20);
	m_data->thermadiag->setMinimumHeight(pix.height()); // +20);
	m_data->thermadiag->setCursor(QCursor(Qt::PointingHandCursor));
	m_data->thermadiag->installEventFilter(this);
	//m_data->thermadiag->setProperty("href", QString("http://thermadiag.com/index.php/en/"));

	QString infos = "Thermavip ";
	if(!vipEditionVersion().isEmpty())
		infos += vipEditionVersion() ;
	QDateTime build_time = QDateTime::fromMSecsSinceEpoch(vipBuildTime());
	infos += "<br>Version " + QString(VIP_VERSION) + ", " + build_time.toString("dd MMM yyyy, hh:mm:ss");
	infos += "<br>&#9400; " + build_time.toString("yyyy") + " <a href='http://irfm.cea.fr/'>CEA/IRFM</a>";
	infos += "<br>All rights reserved";
	infos += "<p>Installed products :</p>";
	m_data->infos->setText(infos);
	m_data->infos->setAlignment(Qt::AlignLeft);
	m_data->infos->setTextFormat(Qt::RichText);
	m_data->infos->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_data->infos->setOpenExternalLinks(true);
	m_data->infos->setStyleSheet("QLabel {margin: 5px;}");

	QList<VipPluginInterface*> interfaces = VipLoadPlugins::instance().loadedPlugins();
	QStringList names = VipLoadPlugins::instance().loadedPluginNames();
	for (int i = 0; i < interfaces.size(); ++i)
	{
		QString text = names[i] + " " + interfaces[i]->pluginVersion();
		QLabel *pl = new QLabel(text);
		pl->setObjectName("label");
		pl->setToolTip(interfaces[i]->description());
		pl->setStyleSheet("QLabel#label{background: transparent;}");

		QListWidgetItem * item = new QListWidgetItem();
		item->setSizeHint(pl->sizeHint());
		m_data->products->addItem(item);
		m_data->products->setItemWidget(item, pl);
	}
	m_data->products->setStyleSheet("QListWidget {margin: 5px;}");
	//m_data->products->setMaximumWidth(500);

	m_data->warning->setText(tr("This Software is protected by copyright law and international treaties. "
		"This Software is licensed (not sold), and its use is subject to a valid WRITTEN AND SIGNED Communique(r) License Agreement. "
		"The unauthorized use, copying or distribution of this Software may result in severe criminal or civil penalties, and will be prosecuted to the maximum extent allowed by law."));
	m_data->warning->setWordWrap(true);
	m_data->warning->setAlignment(Qt::AlignLeft);
	m_data->warning->setStyleSheet("QLabel {margin: 5px;}");

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addWidget(m_data->thermadiag);
	vlay->addWidget(m_data->infos);
	vlay->addWidget(m_data->products);
	vlay->addWidget(m_data->warning);
	//vlay->addWidget(VipLineWidget::createHLine());

	m_data->ok->setText("Ok");
	m_data->ok->setStyleSheet("QPushButton {margin: 5px;}");

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addStretch(1);
	hlay->addWidget(m_data->ok);
	vlay->addLayout(hlay);
	vlay->setContentsMargins(0, 0, 0, 0);

	if (QFileInfo("changelog.txt").exists())
	{
		//display changelog
		QTabWidget * tab = new QTabWidget();
		QWidget * about = new QWidget();

		QColor c = vipWidgetTextBrush(vipGetMainWindow()).color();
		bool dark_skin = (c.redF() > 0.9 && c.greenF() > 0.9 && c.blueF() > 0.9);
		if (dark_skin) {
			about->setStyleSheet("background: #272822;");
		}
		about->setLayout(vlay);
		QTextEdit * edit = new QTextEdit();
		edit->setReadOnly(true);
		QFile fin("changelog.txt");
		fin.open(QFile::ReadOnly | QFile::Text);
		edit->setPlainText(fin.readAll());
		tab->addTab(about, "About");
		tab->addTab(edit, "Changelog");

		QVBoxLayout * l = new QVBoxLayout();
		l->setContentsMargins(0, 0, 0, 0);
		l->addWidget(tab);
		setLayout(l);
	}
	else
		setLayout(vlay);

	connect(m_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
}

VipAboutDialog::~VipAboutDialog()
{
	delete m_data;
}

bool VipAboutDialog::eventFilter(QObject * watched, QEvent * evt)
{
	if (evt->type() == QEvent::MouseButtonPress)
	{
		if (static_cast<QMouseEvent*>(evt)->buttons() & Qt::LeftButton)
		{
			QString href = watched->property("href").toString();
			if (!href.isEmpty())
			{
				QDesktopServices::openUrl(QUrl(href));
			}
		}
	}

	return false;
}
