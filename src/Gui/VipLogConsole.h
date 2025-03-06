/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_LOG_CONSOLE_H
#define VIP_LOG_CONSOLE_H

#include <qtextedit.h>
#include <qthread.h>

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
	VipLogConsole(QWidget* parent = nullptr);
	~VipLogConsole();

public:
	enum LogSection
	{
		DateTime = 0x001,
		Type = 0x002,
		Text = 0x004,
		All = DateTime | Type | Text
	};
	typedef QFlags<LogSection> LogSections;

	void setVisibleLogLevels(VipLogging::Levels levels);
	VipLogging::Levels visibleLogLevels() const;

	void setVisibleSections(LogSections sections);
	LogSections visibleSections() const;

	const QColor& infoColor() const;
	const QColor& warningColor() const;
	const QColor& errorColor() const;
	const QColor& debugColor() const;

public Q_SLOTS:
	/// Print given log entry.
	void printLogEntry(const QString& msg);
	void clear();

	void setInfoColor(const QColor& color);
	void setWarningColor(const QColor& color);
	void setErrorColor(const QColor& color);
	void setDebugColor(const QColor& color);

private:
	void printMessage(VipLogging::Level level, const QString& msg);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipLogConsole::LogSections)

class VIP_GUI_EXPORT VipConsoleWidget : public VipToolWidget
{
	Q_OBJECT
public:
	VipConsoleWidget(VipMainWindow* window = nullptr);
	~VipConsoleWidget();

	VipLogConsole* logConsole() const;

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
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipConsoleWidget*)

VIP_GUI_EXPORT VipConsoleWidget* vipGetConsoleWidget(VipMainWindow* window = nullptr);

VIP_GUI_EXPORT VipArchive& operator<<(VipArchive& arch, VipConsoleWidget* console);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive& arch, VipConsoleWidget* console);

/// @}
// end Gui

#endif
