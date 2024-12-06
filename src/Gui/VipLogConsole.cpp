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

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QLabel>
#include <QMutex>
#include <QToolBar>
#include <QToolButton>
#include <iostream>

#include "VipGui.h"
#include "VipLogConsole.h"
#include "VipStandardWidgets.h"
#include "VipTextOutput.h"

class OutLogDevice : public QIODevice
{
	VipLogConsole* out;
	QString line;

public:
	OutLogDevice(VipLogConsole* parent)
	  : QIODevice(parent)
	  , out(parent)
	{
		this->setOpenMode(WriteOnly);
	}

	virtual bool atEnd() const { return false; }
	virtual void close() {}
	virtual bool isSequential() const { return false; }
	virtual bool open(OpenMode) { return true; }
	virtual qint64 pos() const { return 0; }
	virtual bool reset() { return true; }
	virtual bool seek(qint64) { return false; }
	virtual qint64 size() const { return 0; }

	QColor color() const;

protected:
	virtual qint64 readData(char*, qint64) { return 0; }
	virtual qint64 writeData(const char* data, qint64 maxSize)
	{
		// switch QueuedConnection to AutoConnection
		QString str(data);
		QMetaObject::invokeMethod(out, "printLogEntry", Qt::AutoConnection, Q_ARG(QString, data));
		return maxSize;
	}
};

struct Entry
{
	VipLogging::Level level;
	QString line;
	Entry(VipLogging::Level level = VipLogging::Info, const QString& line = QString())
	  : level(level)
	  , line(line)
	{
	}
};

class VipLogConsole::PrivateData
{
public:
	PrivateData()
	  : sections(All)
	  , lastColor(Qt::black)
	  , infoColor(Qt::black)
	  , debugColor(Qt::black)
	  , warningColor("#ffb000")
	  , errorColor(Qt::red)
	{
	}
	QMutex mutex;
	VipLogging::Levels levels;
	LogSections sections;
	QList<Entry> logs;
	OutLogDevice* device;
	QTextStream stream;
	VipStreambufToQTextStream* redirect;
	std::streambuf* oldcout;

	QColor lastColor;

	QColor infoColor;
	QColor debugColor;
	QColor warningColor;
	QColor errorColor;
};

VipLogConsole::VipLogConsole(QWidget* parent)
  : QTextEdit(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->errorColor = vipDefaultTextErrorColor(this);
	d_data->levels = VipLogging::Info | VipLogging::Warning | VipLogging::Debug | VipLogging::Error;
	d_data->device = new OutLogDevice(this);
	d_data->stream.setDevice(d_data->device);
	d_data->redirect = new VipStreambufToQTextStream(&d_data->stream);
	d_data->oldcout = std::cout.rdbuf();
	std::cout.rdbuf(d_data->redirect);

	setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());
	setLineWrapMode(QTextEdit::NoWrap);
	setReadOnly(true);

	// print the previous log entries
	QStringList entries = VipLogging::instance().savedEntries();
	for (int i = 0; i < entries.size(); ++i)
		printLogEntry(entries[i]);
}

VipLogConsole::~VipLogConsole()
{
	{
		QMutexLocker lock(&d_data->mutex);
		std::cout.rdbuf(d_data->oldcout);
		delete d_data->redirect;
		delete d_data->device;
	}
}

void VipLogConsole::setVisibleSections(LogSections sections)
{
	if (d_data->sections != sections) {
		d_data->sections = sections;
		QTextEdit::clear();
		for (int i = 0; i < d_data->logs.size(); ++i) {
			Entry entry = d_data->logs[i];
			if (entry.level & d_data->levels) {
				this->printMessage(entry.level, entry.line);
			}
		}
	}
}

VipLogConsole::LogSections VipLogConsole::visibleSections() const
{
	return d_data->sections;
}

void VipLogConsole::printMessage(VipLogging::Level level, const QString& msg)
{
	QColor color = d_data->lastColor;
	if (level == VipLogging::Info) {
		color = d_data->infoColor;
	}
	else if (level == VipLogging::Debug) {
		color = d_data->debugColor;
	}
	else if (level == VipLogging::Warning) {
		color = d_data->warningColor; // orange
	}
	else if (level == VipLogging::Error) {
		color = d_data->errorColor;
	}
	d_data->lastColor = color;

	// QMutexLocker lock(&d_data->mutex);
	if ((level & d_data->levels) && isEnabled()) {
		moveCursor(QTextCursor::End);
		setTextColor(color);

		// split sections
		if (d_data->sections != All) {
			QString type, date, text, entry;
			VipLogging::splitLogEntry(msg, type, date, text);
			if (d_data->sections & Type)
				entry += type;
			if (d_data->sections & DateTime)
				entry += date;
			if (d_data->sections & Text)
				entry += text;
			if (!entry.endsWith("\n"))
				entry += "\n";
			insertPlainText(entry);
		}
		else
			insertPlainText(msg);
	}
}

void VipLogConsole::printLogEntry(const QString& msg)
{
	QColor color = d_data->lastColor;
	VipLogging::Level level = VipLogging::Info;
	if (msg.startsWith("Info")) {
		color = d_data->infoColor;
		level = VipLogging::Info;
	}
	else if (msg.startsWith("Debug")) {
		color = d_data->debugColor;
		level = VipLogging::Debug;
	}
	else if (msg.startsWith("Warning")) {
		color = d_data->warningColor;
		level = VipLogging::Warning;
	}
	else if (msg.startsWith("Error")) {
		color = d_data->errorColor;
		level = VipLogging::Error;
	}
	d_data->lastColor = color;

	// QMutexLocker lock(&d_data->mutex);
	d_data->logs.append(Entry(level, msg));
	if (d_data->logs.size() > 10000)
		d_data->logs.pop_front();

	if ((level & d_data->levels) && isEnabled()) {
		moveCursor(QTextCursor::End);
		setTextColor(color);
		// split sections
		if (d_data->sections != All) {
			QString type, date, text, entry;
			VipLogging::splitLogEntry(msg, type, date, text);
			if (d_data->sections & Type)
				entry += type;
			if (d_data->sections & DateTime)
				entry += date;
			if (d_data->sections & Text)
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
	d_data->logs.clear();
}

void VipLogConsole::setVisibleLogLevels(VipLogging::Levels levels)
{
	// QMutexLocker lock(&d_data->mutex);
	if (d_data->levels != levels) {
		d_data->levels = levels;

		QTextEdit::clear();
		for (int i = 0; i < d_data->logs.size(); ++i) {
			Entry entry = d_data->logs[i];
			if (entry.level & levels) {
				this->printMessage(entry.level, entry.line);
			}
		}
	}
}

VipLogging::Levels VipLogConsole::visibleLogLevels() const
{
	return d_data->levels;
}

const QColor& VipLogConsole::infoColor() const
{
	return d_data->infoColor;
}
const QColor& VipLogConsole::warningColor() const
{
	return d_data->warningColor;
}
const QColor& VipLogConsole::errorColor() const
{
	return d_data->errorColor;
}
const QColor& VipLogConsole::debugColor() const
{
	return d_data->debugColor;
}

void VipLogConsole::setInfoColor(const QColor& color)
{
	if (d_data->infoColor != color) {
		d_data->infoColor = color;
		d_data->lastColor = color;
		setVisibleLogLevels(d_data->levels);
	}
}
void VipLogConsole::setWarningColor(const QColor& color)
{
	if (d_data->warningColor != color) {
		d_data->warningColor = color;
		setVisibleLogLevels(d_data->levels);
	}
}
void VipLogConsole::setErrorColor(const QColor& color)
{
	if (d_data->errorColor != color) {
		d_data->errorColor = color;
		setVisibleLogLevels(d_data->levels);
	}
}
void VipLogConsole::setDebugColor(const QColor& color)
{
	if (d_data->debugColor != color) {
		d_data->debugColor = color;
		setVisibleLogLevels(d_data->levels);
	}
}

class VipConsoleWidget::PrivateData
{
public:
	VipLogConsole* console;
	QWidget* widget;
	QLabel* text;
	QToolBar* toolBar;
	QToolButton* levelVisibility;
};

VipConsoleWidget::VipConsoleWidget(VipMainWindow* window)
  : VipToolWidget(window)
{
	this->setKeepFloatingUserSize(true);
	this->setObjectName("Console");
	this->setWindowTitle("Console");
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->console = new VipLogConsole();
	d_data->text = new QLabel();
	d_data->toolBar = new QToolBar();
	d_data->levelVisibility = new QToolButton();

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(d_data->text);
	hlay->addStretch(2);
	hlay->addWidget(d_data->toolBar);
	hlay->setContentsMargins(0, 0, 0, 0);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	hlay->setMargin(0);
#endif

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(hlay);
	vlay->addWidget(d_data->console);
	vlay->setContentsMargins(0, 0, 0, 0);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	vlay->setMargin(0);
#endif

	d_data->widget = new QWidget();
	d_data->widget->setLayout(vlay);
	setWidget(d_data->widget, Qt::Horizontal);

	d_data->text->setText("Console [All]");
	d_data->text->setStyleSheet(""
				    "padding:0px;"
				    "text-indent : 0px;"
				    "margin:0px;"
				    "border: none;");

	d_data->toolBar->setIconSize(QSize(18, 18));
	QAction* copy = d_data->toolBar->addAction(vipIcon("copy.png"), "Copy content to clipboard");
	QAction* save = d_data->toolBar->addAction(vipIcon("save.png"), "Save content to file...");
	QAction* disable = d_data->toolBar->addAction(vipIcon("cancel.png"), "Stop/Resume the console");
	QAction* clear = d_data->toolBar->addAction(vipIcon("close.png"), "Clear the console");
	disable->setCheckable(true);
	d_data->toolBar->addSeparator();
	d_data->toolBar->addWidget(d_data->levelVisibility);
	d_data->levelVisibility->setIcon(vipIcon("console.png"));
	d_data->levelVisibility->setText("Display selected outputs");
	d_data->levelVisibility->setAutoRaise(true);
	d_data->levelVisibility->setPopupMode(QToolButton::InstantPopup);
	d_data->levelVisibility->setIconSize(QSize(25, 18));
	d_data->levelVisibility->setMinimumWidth(35);

	QMenu* menu = new QMenu(d_data->levelVisibility);
	QAction* info = menu->addAction("Display Informations");
	info->setCheckable(true);
	info->setChecked(true);
	QAction* deb = menu->addAction("Display Debug info");
	deb->setCheckable(true);
	deb->setChecked(true);
	QAction* warning = menu->addAction("Display Warnings");
	warning->setCheckable(true);
	warning->setChecked(true);
	QAction* error = menu->addAction("Display Errors");
	error->setCheckable(true);
	error->setChecked(true);
	menu->addSeparator();
	QAction* date = menu->addAction("Display log date");
	date->setCheckable(true);
	date->setChecked(true);
	QAction* type = menu->addAction("Display log type");
	type->setCheckable(true);
	type->setChecked(true);
	d_data->levelVisibility->setMenu(menu);

	connect(info, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(deb, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(warning, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(error, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(date, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));
	connect(type, SIGNAL(triggered(bool)), this, SLOT(setVisibleLogLevel()));

	connect(clear, SIGNAL(triggered(bool)), d_data->console, SLOT(clear()));
	connect(disable, SIGNAL(triggered(bool)), this, SLOT(disable(bool)));
	connect(copy, SIGNAL(triggered(bool)), this, SLOT(copy()));
	connect(save, SIGNAL(triggered(bool)), this, SLOT(save()));

	this->setMinimumWidth(250);
}

VipConsoleWidget::~VipConsoleWidget()
{
}

void VipConsoleWidget::removeConsole()
{
	d_data->console->setParent(nullptr);
}
void VipConsoleWidget::resetConsole()
{
	d_data->widget->layout()->addWidget(d_data->console);
}

VipLogConsole* VipConsoleWidget::logConsole() const
{
	return const_cast<VipLogConsole*>(d_data->console);
}

void VipConsoleWidget::clear()
{
	d_data->console->clear();
}

void VipConsoleWidget::save()
{
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save to file", "TEXT file (*.txt)");
	if (!filename.isEmpty()) {
		QFile fout(filename);
		if (fout.open(QFile::WriteOnly | QFile::Text))
			fout.write(d_data->console->toPlainText().toLatin1());
	}
}

void VipConsoleWidget::copy()
{
	QClipboard* clipboard = QApplication::clipboard();
	clipboard->setText(d_data->console->toPlainText());
}

void VipConsoleWidget::disable(bool dis)
{
	d_data->console->setEnabled(!dis);
	if (!dis)
		setVisibleLogLevel();
}

void VipConsoleWidget::setVisibleLogLevel()
{
	QList<QAction*> actions = d_data->levelVisibility->menu()->actions();
	VipLogging::Levels levels;
	QStringList consoles;

	if (actions[0]->isChecked()) {
		levels |= VipLogging::Info;
		consoles.append("Info");
	}

	if (actions[1]->isChecked()) {
		levels |= VipLogging::Debug;
		consoles.append("Debug");
	}

	if (actions[2]->isChecked()) {
		levels |= VipLogging::Warning;
		consoles.append("Warning");
	}

	if (actions[3]->isChecked()) {
		levels |= VipLogging::Error;
		consoles.append("Error");
	}

	VipLogConsole::LogSections sections = VipLogConsole::Text;
	if (actions[5]->isChecked()) {
		sections |= VipLogConsole::DateTime;
	}

	if (actions[6]->isChecked()) {
		sections |= VipLogConsole::Type;
	}

	if (consoles.size() == 3)
		d_data->text->setText("Console [All]");
	else if (consoles.size() == 0)
		d_data->text->setText("Console [None]");
	else
		d_data->text->setText("Console [" + consoles.join("|") + "]");

	d_data->console->setVisibleLogLevels(levels);
	d_data->console->setVisibleSections(sections);
}

void VipConsoleWidget::updateVisibleMenu()
{
	VipLogging::Levels levels = visibleLogLevels();
	VipLogConsole::LogSections sections = visibleSections();
	QList<QAction*> actions = d_data->levelVisibility->menu()->actions();
	for (int i = 0; i < actions.size(); ++i)
		actions[i]->blockSignals(true);

	actions[0]->setChecked(levels & VipLogging::Info);
	actions[1]->setChecked(levels & VipLogging::Debug);
	actions[2]->setChecked(levels & VipLogging::Warning);
	actions[3]->setChecked(levels & VipLogging::Error);
	actions[5]->setChecked(sections & VipLogConsole::DateTime);
	actions[6]->setChecked(sections & VipLogConsole::Type);

	for (int i = 0; i < actions.size(); ++i)
		actions[i]->blockSignals(false);
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

VipConsoleWidget* vipGetConsoleWidget(VipMainWindow* window)
{
	static VipConsoleWidget* console = new VipConsoleWidget(window);
	return console;
}

#include "VipArchive.h"

VipArchive& operator<<(VipArchive& arch, VipConsoleWidget* console)
{
	arch.content("levels", (int)console->visibleLogLevels());
	arch.content("sections", (int)console->visibleSections());
	return arch;
}
VipArchive& operator>>(VipArchive& arch, VipConsoleWidget* console)
{
	int levels = arch.read("levels").toInt();
	int sections = arch.read("sections").toInt();
	if (arch) {
		console->setVisibleLogLevels(VipLogging::Levels(levels));
		console->setVisibleSections(VipLogConsole::LogSections(sections));
	}
	return arch;
}

static int register1 = vipRegisterArchiveStreamOperators<VipConsoleWidget*>();
