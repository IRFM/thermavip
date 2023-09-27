#ifndef LOG_CONSOLE_H
#define LOG_CONSOLE_H

#include <qthread.h>
#include <qtextedit.h>

#include "VipLogging.h"
#include "VipToolWidget.h"

/// \addtogroup Gui
/// @{


/// \brief VipLogConsole widget used to display logged informations.
///
/// The VipLogConsole class will continuously watch for available log entries using #VipLogging::LastLogEntries()
/// and display them. An information will be printed in black, a warning in orange and an error in red.
class VIP_GUI_EXPORT VipLogConsole : public QTextEdit
{
	Q_OBJECT
	Q_PROPERTY(QColor InfoColor READ infoColor WRITE setInfoColor)
	Q_PROPERTY(QColor WarningColor READ warningColor WRITE setWarningColor)
	Q_PROPERTY(QColor ErrorColor READ errorColor WRITE setErrorColor)
	Q_PROPERTY(QColor DebugColor READ debugColor WRITE setDebugColor)

public:
	VipLogConsole(QWidget * parent = NULL);
	~VipLogConsole();

public:
	enum LogSection
	{
		DateTime = 0x001,
		Type = 0x002,
		Text = 0x004,
		All = DateTime|Type|Text
	};
	typedef QFlags<LogSection> LogSections;

	void setVisibleLogLevels(VipLogging::Levels levels);
	VipLogging::Levels visibleLogLevels() const;

	void setVisibleSections(LogSections sections);
	LogSections visibleSections() const;

	const QColor & infoColor() const;
	const QColor & warningColor() const;
	const QColor & errorColor() const;
	const QColor & debugColor() const;

public Q_SLOTS:
	///Print given log entry.
	void printLogEntry(const QString & msg);
	void clear();

	void setInfoColor(const QColor & color);
	void setWarningColor(const QColor & color);
	void setErrorColor(const QColor & color);
	void setDebugColor(const QColor & color);

private:
	void printMessage(VipLogging::Level level, const QString & msg);

	class PrivateData;
	PrivateData * m_data;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipLogConsole::LogSections)

class VIP_GUI_EXPORT VipConsoleWidget : public VipToolWidget
{
	Q_OBJECT
public:

	VipConsoleWidget(VipMainWindow * window = NULL);
	~VipConsoleWidget();

	VipLogConsole * logConsole() const;

	void setVisibleLogLevels(VipLogging::Levels levels);
	VipLogging::Levels visibleLogLevels() const;

	void setVisibleSections(VipLogConsole::LogSections sections);
	VipLogConsole::LogSections visibleSections() const;

	void removeConsole();
	void resetConsole();

public Q_SLOTS:
	void clear();
	void save();
	void copy();
	void disable(bool);
	void setVisibleLogLevel();

private:
	void updateVisibleMenu();
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipConsoleWidget*)

VIP_GUI_EXPORT VipConsoleWidget * vipGetConsoleWidget(VipMainWindow * window = NULL);

VIP_GUI_EXPORT VipArchive & operator << (VipArchive & arch, VipConsoleWidget * console);
VIP_GUI_EXPORT VipArchive & operator >> (VipArchive & arch, VipConsoleWidget * console);

/// @}
//end Gui

#endif
