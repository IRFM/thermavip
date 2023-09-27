#include <iostream>
#include <QMutex>
#include <QClipboard>
#include <QApplication>
#include <QToolBar>
#include <QLabel>
#include <QToolButton>
#include <QBoxLayout>

#include "VipTextOutput.h"
#include "VipStandardWidgets.h"
#include "VipLogConsole.h"
#include "VipGui.h"


class OutLogDevice : public QIODevice
{
	VipLogConsole * out;
	QString line;

public:
	OutLogDevice(VipLogConsole * parent)
		:QIODevice(parent), out(parent) {
		this->setOpenMode(WriteOnly);
	}

	virtual bool 	atEnd() const { return false; }
	virtual void 	close() {}
	virtual bool 	isSequential() const { return false; }
	virtual bool 	open(OpenMode) { return true; }
	virtual qint64 	pos() const { return 0; }
	virtual bool 	reset() { return true; }
	virtual bool 	seek(qint64) { return false; }
	virtual qint64 	size() const { return 0; }

	QColor color() const;

protected:

	virtual qint64 	readData(char *, qint64) { return 0; }
	virtual qint64 	writeData(const char * data, qint64 maxSize)
	{
		//switch QueuedConnection to AutoConnection
		QString str(data);
		QMetaObject::invokeMethod(out, "printLogEntry", Qt::AutoConnection, Q_ARG(QString, data));
		return maxSize;
	}
};

struct Entry
{
	VipLogging::Level level;
	QString line;
	Entry(VipLogging::Level level = VipLogging::Info, const QString & line = QString())
		:level(level), line(line) {}
};


class VipLogConsole::PrivateData
{
public:
	PrivateData() : sections(All), lastColor(Qt::black), infoColor(Qt::black), debugColor(Qt::black), warningColor("#ffb000"), errorColor(Qt::red) {}
	QMutex mutex;
	VipLogging::Levels levels;
	LogSections sections;
	QList<Entry> logs;
	OutLogDevice * device;
	QTextStream stream;
	VipStreambufToQTextStream * redirect;
	std::streambuf * oldcout;

	QColor lastColor;

	QColor infoColor;
	QColor debugColor;
	QColor warningColor;
	QColor errorColor;
};

VipLogConsole::VipLogConsole(QWidget * parent)
	:QTextEdit(parent)
{
	m_data = new PrivateData();
	m_data->errorColor = vipDefaultTextErrorColor(this);
	m_data->levels = VipLogging::Info | VipLogging::Warning | VipLogging::Debug | VipLogging::Error;
	m_data->device = new OutLogDevice(this);
	m_data->stream.setDevice(m_data->device);
	m_data->redirect = new VipStreambufToQTextStream(&m_data->stream);
	m_data->oldcout = std::cout.rdbuf();
	std::cout.rdbuf(m_data->redirect);

	setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());
	setLineWrapMode(QTextEdit::NoWrap);
	setReadOnly(true);

	//print the previous log entries
	QStringList entries = VipLogging::instance().savedEntries();
	for (int i = 0; i < entries.size(); ++i)
		printLogEntry(entries[i]);
}

VipLogConsole::~VipLogConsole()
{
	{
		QMutexLocker lock(&m_data->mutex);
		std::cout.rdbuf(m_data->oldcout);
		delete m_data->redirect;
		delete m_data->device;
	}
	delete m_data;
}

void VipLogConsole::setVisibleSections(LogSections sections)
{
	if (m_data->sections != sections)
	{
		m_data->sections = sections;
		QTextEdit::clear();
		for (int i = 0; i < m_data->logs.size(); ++i)
		{
			Entry entry = m_data->logs[i];
			if (entry.level & m_data->levels)
			{
				this->printMessage(entry.level, entry.line);
			}
		}
	}
}

VipLogConsole::LogSections VipLogConsole::visibleSections() const
{
	return m_data->sections;
}


void VipLogConsole::printMessage(VipLogging::Level level, const QString & msg)
{
	QColor color = m_data->lastColor;
	if (level == VipLogging::Info)
	{
		color = m_data->infoColor;
	}
	else if (level == VipLogging::Debug)
	{
		color = m_data->debugColor;
	}
	else if (level == VipLogging::Warning)
	{
		color = m_data->warningColor; //orange
	}
	else if (level == VipLogging::Error)
	{
		color = m_data->errorColor;
	}
	m_data->lastColor = color;

	//QMutexLocker lock(&m_data->mutex);
	if ((level & m_data->levels) && isEnabled())
	{
		moveCursor(QTextCursor::End);
		setTextColor(color);

		//split sections
		if (m_data->sections != All)
		{
			QString type, date, text, entry;
			VipLogging::splitLogEntry(msg, type, date, text);
			if (m_data->sections & Type)
				entry += type;
			if (m_data->sections & DateTime)
				entry += date;
			if (m_data->sections & Text)
				entry += text;
			if (!entry.endsWith("\n"))
				entry += "\n";
			insertPlainText(entry);
		}
		else
			insertPlainText(msg);
	}
}

void VipLogConsole::printLogEntry(const QString & msg)
{
	QColor color = m_data->lastColor;
	VipLogging::Level level = VipLogging::Info;
	if (msg.startsWith("Info"))
	{
		color = m_data->infoColor;
		level = VipLogging::Info;
	}
	else if (msg.startsWith("Debug"))
	{
		color = m_data->debugColor;
		level = VipLogging::Debug;
	}
	else if (msg.startsWith("Warning"))
	{
		color = m_data->warningColor;
		level = VipLogging::Warning;
	}
	else if (msg.startsWith("Error"))
	{
		color = m_data->errorColor;
		level = VipLogging::Error;
	}
	m_data->lastColor = color;

	//QMutexLocker lock(&m_data->mutex);
	m_data->logs.append(Entry(level, msg));
	if (m_data->logs.size() > 10000)
		m_data->logs.pop_front();

	if ((level & m_data->levels) && isEnabled())
	{
		moveCursor(QTextCursor::End);
		setTextColor(color);
		//split sections
		if (m_data->sections != All)
		{
			QString type, date, text, entry;
			VipLogging::splitLogEntry(msg, type, date, text);
			if (m_data->sections & Type)
				entry += type;
			if (m_data->sections & DateTime)
				entry += date;
			if (m_data->sections & Text)
				entry += text;
			if (!entry.endsWith("\n"))
				entry += "\n";
			insertPlainText(entry);
		}
		else
			insertPlainText(msg);
	}
}

void VipLogConsole::clear()
{
	QTextEdit::clear();
	m_data->logs.clear();
}

void VipLogConsole::setVisibleLogLevels(VipLogging::Levels levels)
{
	//QMutexLocker lock(&m_data->mutex);
	if (m_data->levels != levels) {
		m_data->levels = levels;

		QTextEdit::clear();
		for (int i = 0; i < m_data->logs.size(); ++i){
			Entry entry = m_data->logs[i];
			if (entry.level & levels){
				this->printMessage(entry.level, entry.line);
			}
		}
	}
}

VipLogging::Levels VipLogConsole::visibleLogLevels() const
{
	return m_data->levels;
}

const QColor & VipLogConsole::infoColor() const
{
	return m_data->infoColor;
}
const QColor & VipLogConsole::warningColor() const
{
	return m_data->warningColor;
}
const QColor & VipLogConsole::errorColor() const
{
	return m_data->errorColor;
}
const QColor & VipLogConsole::debugColor() const
{
	return m_data->debugColor;
}

void VipLogConsole::setInfoColor(const QColor & color)
{
	if (m_data->infoColor != color) {
		m_data->infoColor = color;
		m_data->lastColor = color;
		setVisibleLogLevels(m_data->levels);
	}
}
void VipLogConsole::setWarningColor(const QColor & color)
{
	if (m_data->warningColor != color) {
		m_data->warningColor = color;
		setVisibleLogLevels(m_data->levels);
	}
}
void VipLogConsole::setErrorColor(const QColor & color)
{
	if (m_data->errorColor != color) {
		m_data->errorColor = color;
		setVisibleLogLevels(m_data->levels);
	}
}
void VipLogConsole::setDebugColor(const QColor & color)
{
	if (m_data->debugColor != color) {
		m_data->debugColor = color;
		setVisibleLogLevels(m_data->levels);
	}
}


class VipConsoleWidget::PrivateData
{
public:
	VipLogConsole *console;
	QWidget * widget;
	QLabel *text;
	QToolBar *toolBar;
	QToolButton *levelVisibility;
};

VipConsoleWidget::VipConsoleWidget(VipMainWindow * window)
	:VipToolWidget(window)
{
	this->setKeepFloatingUserSize(true);
	this->setObjectName("Console");
	this->setWindowTitle("Console");
	m_data = new PrivateData();
	m_data->console = new VipLogConsole();
	m_data->text = new QLabel();
	m_data->toolBar = new QToolBar();
	m_data->levelVisibility = new QToolButton();

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->addWidget(m_data->text);
	hlay->addStretch(2);
	hlay->addWidget(m_data->toolBar);
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->setMargin(0);

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(m_data->console);
	vlay->setContentsMargins(0, 0, 0, 0);
	vlay->setMargin(0);

	m_data->widget = new QWidget();
	m_data->widget->setLayout(vlay);
	setWidget(m_data->widget, Qt::Horizontal);

	m_data->text->setText("Console [All]");
	m_data->text->setStyleSheet(""
		"padding:0px;"
		"text-indent : 0px;"
		"margin:0px;"
		"border: none;");

	m_data->toolBar->setIconSize(QSize(18, 18));
	QAction * copy = m_data->toolBar->addAction(vipIcon("copy.png"), "Copy content to clipboard");
	QAction * save = m_data->toolBar->addAction(vipIcon("save.png"), "Save content to file...");
	QAction * disable = m_data->toolBar->addAction(vipIcon("cancel.png"), "Stop/Resume the console");
	QAction * clear = m_data->toolBar->addAction(vipIcon("close.png"), "Clear the console");
	disable->setCheckable(true);
	m_data->toolBar->addSeparator();
	m_data->toolBar->addWidget(m_data->levelVisibility);
	m_data->levelVisibility->setIcon(vipIcon("console.png"));
	m_data->levelVisibility->setText("Display selected outputs");
	m_data->levelVisibility->setAutoRaise(true);
	m_data->levelVisibility->setPopupMode(QToolButton::InstantPopup);
	m_data->levelVisibility->setIconSize(QSize(25, 18));
	m_data->levelVisibility->setMinimumWidth(35);

	QMenu * menu = new QMenu(m_data->levelVisibility);
	QAction * info = menu->addAction("Display Informations");
	info->setCheckable(true);
	info->setChecked(true);
	QAction * deb = menu->addAction("Display Debug info");
	deb->setCheckable(true);
	deb->setChecked(true);
	QAction * warning = menu->addAction("Display Warnings");
	warning->setCheckable(true);
	warning->setChecked(true);
	QAction *error = menu->addAction("Display Errors");
	error->setCheckable(true);
	error->setChecked(true);
	menu->addSeparator();
	QAction * date = menu->addAction("Display log date");
	date->setCheckable(true);
	date->setChecked(true);
	QAction * type = menu->addAction("Display log type");
	type->setCheckable(true);
	type->setChecked(true);
	m_data->levelVisibility->setMenu(menu);

	connect(info, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(deb, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(warning, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(error, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(date, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(type, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));

	connect(clear, SIGNAL(triggered(bool)), m_data->console, SLOT(clear()));
	connect(disable, SIGNAL(triggered(bool)), this, SLOT(disable(bool)));
	connect(copy, SIGNAL(triggered(bool)), this, SLOT(copy()));
	connect(save, SIGNAL(triggered(bool)), this, SLOT(save()));

	this->setMinimumWidth(250);
}

VipConsoleWidget::~VipConsoleWidget()
{
	delete m_data;
}

void VipConsoleWidget::removeConsole()
{
	m_data->console->setParent(NULL);
}
void VipConsoleWidget::resetConsole()
{
	m_data->widget->layout()->addWidget(m_data->console);
}

VipLogConsole *VipConsoleWidget::logConsole() const
{
	return const_cast<VipLogConsole*>(m_data->console);
}

void VipConsoleWidget::clear()
{
	m_data->console->clear();
}

void VipConsoleWidget::save()
{
	QString filename = VipFileDialog::getSaveFileName(NULL, "Save to file", "TEXT file (*.txt)");
	if (!filename.isEmpty())
	{
		QFile fout(filename);
		if (fout.open(QFile::WriteOnly | QFile::Text))
			fout.write(m_data->console->toPlainText().toLatin1());
	}
}

void VipConsoleWidget::copy()
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(m_data->console->toPlainText());
}

void VipConsoleWidget::disable(bool dis)
{
	m_data->console->setEnabled(!dis);
	if (!dis)
		setVisibleLogLevel();
}

void VipConsoleWidget::setVisibleLogLevel()
{
	QList<QAction*> actions = m_data->levelVisibility->menu()->actions();
	VipLogging::Levels levels = 0;
	QStringList consoles;


	if (actions[0]->isChecked())
	{
		levels |= VipLogging::Info;
		consoles.append("Info");
	}

	if (actions[1]->isChecked())
	{
		levels |= VipLogging::Debug;
		consoles.append("Debug");
	}

	if (actions[2]->isChecked())
	{
		levels |= VipLogging::Warning;
		consoles.append("Warning");
	}

	if (actions[3]->isChecked())
	{
		levels |= VipLogging::Error;
		consoles.append("Error");
	}

	VipLogConsole::LogSections sections = VipLogConsole::Text;
	if (actions[5]->isChecked())
	{
		sections |= VipLogConsole::DateTime;
	}

	if (actions[6]->isChecked())
	{
		sections |= VipLogConsole::Type;
	}

	if (consoles.size() == 3)
		m_data->text->setText("Console [All]");
	else if (consoles.size() == 0)
		m_data->text->setText("Console [None]");
	else
		m_data->text->setText("Console [" + consoles.join("|") + "]");

	m_data->console->setVisibleLogLevels(levels);
	m_data->console->setVisibleSections(sections);
}

void VipConsoleWidget::updateVisibleMenu()
{
	VipLogging::Levels levels = visibleLogLevels();
	VipLogConsole::LogSections sections = visibleSections();
	QList<QAction*> actions = m_data->levelVisibility->menu()->actions();
	for (int i = 0; i < actions.size(); ++i) actions[i]->blockSignals(true);

	actions[0]->setChecked(levels & VipLogging::Info);
	actions[1]->setChecked(levels & VipLogging::Debug);
	actions[2]->setChecked(levels & VipLogging::Warning);
	actions[3]->setChecked(levels & VipLogging::Error);
	actions[5]->setChecked(sections & VipLogConsole::DateTime);
	actions[6]->setChecked(sections & VipLogConsole::Type);

	for (int i = 0; i < actions.size(); ++i) actions[i]->blockSignals(false);
}

void VipConsoleWidget::setVisibleLogLevels(VipLogging::Levels levels)
{
	logConsole()->setVisibleLogLevels(levels);
	updateVisibleMenu();
}

VipLogging::Levels VipConsoleWidget::visibleLogLevels() const
{
	return logConsole()->visibleLogLevels();
}

void VipConsoleWidget::setVisibleSections(VipLogConsole::LogSections sections)
{
	logConsole()->setVisibleSections(sections);
	updateVisibleMenu();
}

VipLogConsole::LogSections VipConsoleWidget::visibleSections() const
{
	return logConsole()->visibleSections();
}


VipConsoleWidget * vipGetConsoleWidget(VipMainWindow * window)
{
	static VipConsoleWidget * console = new VipConsoleWidget(window);
	return console;
}

#include "VipArchive.h"

VipArchive & operator << (VipArchive & arch, VipConsoleWidget * console)
{
	arch.content("levels", (int)console->visibleLogLevels());
	arch.content("sections", (int)console->visibleSections());
	return arch;
}
VipArchive & operator >> (VipArchive & arch, VipConsoleWidget * console)
{
	int levels = arch.read("levels").toInt();
	int sections = arch.read("sections").toInt();
	if (arch)
	{
		console->setVisibleLogLevels(VipLogging::Levels(levels));
		console->setVisibleSections(VipLogConsole::LogSections(sections));
	}
	return arch;
}


static int register1 = vipRegisterArchiveStreamOperators<VipConsoleWidget*>();
