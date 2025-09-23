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

#ifndef VIP_OPEN_HISTORY_H
#define VIP_OPEN_HISTORY_H

#include "VipStandardWidgets.h"

/// @brief Helper class used to open files/signals based on a possibly custom format.
/// This is used to open files/signals from a search line edit located on the top tool 
/// bar of the main window.
///
class VIP_GUI_EXPORT VipDeviceOpenHelper
{
public:

	/// @brief Shortcup path represented by a shortcut format string and
	/// the associated VipDeviceOpenHelper.
	struct ShortcupPath
	{
		QString format;
		const VipDeviceOpenHelper* helper;
	};

	virtual ~VipDeviceOpenHelper() {}

	/// @brief From a user input, return all possibly well formated inputs.
	/// For instance, given the start of a file path, returns all possible locations.
	virtual QStringList format(const QString& user_input) const = 0;

	/// @brief Convert a valid shortcut format to a valid path that can be passed to VipMainWindow::openPaths().
	/// Returns an aempty QString if a valid path cannot be created.
	virtual QString validPathFromFormat(const QString& format) const = 0;

	/// @brief Convert a valid path to its shortcut format.
	/// Returns an empty string if a valid format cannot be created.
	virtual QString formatFromValidPath(const QString& path) const = 0;

	/// @brief Tells if given format can be directly open or might require an additional argument
	virtual bool directOpen(const QString& format) const = 0;

	/// @brief Open from well formatted path
	virtual bool open(const QString& valid_path) const;

	/// @brief Clean a valid format string
	/// For instance, this will remove redundant ../../ from local paths.
	virtual QString cleanFormat(const QString& format) const { return format; }

	/// @brief Register a VipDeviceOpenHelper object.
	/// Note that this function takes ownership of the object.
	static void registerHelper(VipDeviceOpenHelper*);

	/// @brief Returns the VipDeviceOpenHelper that can handle given valid format
	static const VipDeviceOpenHelper* fromFormat(const QString& format);

	/// @brief Returns the VipDeviceOpenHelper that can handle given path
	static const VipDeviceOpenHelper* fromValidPath(const QString& valid_path);

	static QStringList possibleFormats(const QString& user_input);


	static bool addToHistory(const ShortcupPath& shortcut);
	static bool addToHistory(const QString& valid_path);
	static int addToHistory(const QStringList& valid_paths);
	static int addFormatsToHistory(const QStringList& valid_formats);
	static const QList<ShortcupPath>& history();
};

/// @brief A VipDeviceOpenHelper used to open local files
class VIP_GUI_EXPORT VipFileOpenHelper : public VipDeviceOpenHelper
{
public:
	virtual QStringList format(const QString& user_input) const;
	virtual QString validPathFromFormat(const QString& format) const;
	virtual QString formatFromValidPath(const QString& path) const;
	virtual bool directOpen(const QString& format) const;
	virtual QString cleanFormat(const QString& format) const;
};

/// @brief A VipDeviceOpenHelper providing GUI features shortcuts
/// from the central search line edit.
class VIP_GUI_EXPORT VipShortcutsHelper : public VipDeviceOpenHelper
{
public:
	virtual QStringList format(const QString& user_input) const;
	virtual QString validPathFromFormat(const QString& format) const;
	virtual QString formatFromValidPath(const QString& path) const;
	virtual bool directOpen(const QString& format) const { return true; }
	virtual bool open(const QString& valid_path) const;

	static bool registerShorcut(const QString& format, const std::function<void()>& fun);
};


/// @brief Global search line editor
/// 
/// VipSearchLineEdit can be used to open files,
/// or any kind of signals based on shortcuts (see VipDeviceOpenHelper class).
/// 
/// It can be used to start GUI features based on a search string using
/// VipShortcutsHelper::registerShorcut().
/// 
/// Currently, the program embedds only one VipSearchLineEdit instance
/// located on the top of the main window.
/// 
class VIP_GUI_EXPORT VipSearchLineEdit : public QLineEdit
{
	Q_OBJECT

public:
	VipSearchLineEdit(QWidget* parent = nullptr);
	~VipSearchLineEdit();

private Q_SLOTS:
	void returnPressed();
	void textEntered();
	void displayHistory();
	void showHistoryWidget(const QStringList &);

protected:
	virtual void focusOutEvent(QFocusEvent* evt);
	virtual bool event(QEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);
	virtual bool eventFilter(QObject* watched, QEvent* event);

private:
	VIP_DECLARE_PRIVATE_DATA();
};

#endif