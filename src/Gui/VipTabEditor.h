#ifndef VIP_TAB_EDITOR_H
#define VIP_TAB_EDITOR_H

#include "VipTextEditor.h"
#include "VipConfig.h"

#include <QTabWidget>
#include <QWidgetAction>
#include <QToolBar>

class QLineEdit;

/// @brief Small tool bar used for searching in a VipTextEditor
class VIP_GUI_EXPORT VipTextSearchBar : public QToolBar
{
	Q_OBJECT

public:
	VipTextSearchBar(QWidget* parent = nullptr);
	~VipTextSearchBar();

	bool exactMatch() const;
	bool caseSensitive() const;
	bool regExp() const;

	void setEditor(QTextEdit*);
	void setEditor(QPlainTextEdit*);
	void setDocument(QTextDocument*);
	QTextDocument* document() const;
	QLineEdit* search() const;
	QPair<int, int> lastFound(int* line) const;

public Q_SLOTS:
	void searchNext();
	void searchPrev();
	void setExactMatch(bool);
	void setRegExp(bool);
	void setCaseSensitive(bool);
	void close(bool);
	void restartFromCursor();
	void search(bool forward);

Q_SIGNALS:
	void closeRequested();

protected:
	virtual void showEvent(QShowEvent*);
	virtual void hideEvent(QHideEvent*);

private:
	void removePreviousFormat();
	void format(const QTextBlock& b, int start, int end);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Default tool bar for all text editors
class VIP_GUI_EXPORT VipDefaultTextBar : public QToolBar
{

public:
	VipDefaultTextBar(QWidget* parent = nullptr);

	QWidgetAction* open;
	QWidgetAction* save;
	QWidgetAction* saveAs;
	QWidgetAction* saveAll;
	QWidgetAction* newfile;

	QWidgetAction* comment;
	QWidgetAction* uncomment;
	QWidgetAction* indent;
	QWidgetAction* unindent;
};

/// @brief Tab widget used by VipTabEditor
class VipTextEditorTabWidget : public QTabWidget
{
public:
	VipTextEditorTabWidget(QWidget* parent = nullptr)
	  : QTabWidget(parent)
	{
		setStyleSheet("QTabWidget::pane { border: 0px; } QTabWidget{padding:0px; margin: 0px; }");
	}

	QTabBar* tabBar() { return this->QTabWidget::tabBar(); }
};

/// @brief Text editor that organizes its editors by tabs.
///
class VIP_GUI_EXPORT VipTabEditor : public QWidget
{
	Q_OBJECT

public:
	VipTabEditor(Qt::Orientation tool_bar_orientation = Qt::Horizontal, QWidget* parent = nullptr);
	~VipTabEditor();

	/// @brief Returns the tab widget
	VipTextEditorTabWidget* tabWidget() const;
	/// @brief Returns the tab bar
	VipDefaultTextBar* tabBar() const;
	/// @brief Returns the current editor
	VipTextEditor* currentEditor() const;
	/// @brief Returns editor at given pos
	VipTextEditor* editor(int pos) const;
	/// @brief Returns editor count
	int count() const;
	/// @brief Returns the current editor index
	int currentIndex() const;
	/// @brief Returns true if this editor is in unique file mode.
	/// In this case, the tab bar is hidden and options like
	/// opening a new file or creating a new editor are disabled.
	bool uniqueFile() const;

	/// @brief Returns the editor's filename (if any)
	QString filename(VipTextEditor* ed) const;
	/// @brief Returns the editor's canonical filename (if any)
	QString canonicalFilename(VipTextEditor* ed) const;

	/// @brief Returns the default color scheme used for files without extension
	/// or new files.
	QString defaultColorSchemeType() const;

	/// @brief Returns the default saving/opening directory
	QString defaultSaveDirectory() const;

	/// @brief Returns the default text displayed by new editors
	QString defaultText() const;

public Q_SLOTS:

	void setDefaultText(const QString& code);
	void setCurrentIndex(int index);
	void setUniqueFile(bool unique);
	void setDefaultColorSchemeType(const QString& type);
	void setDefaultSaveDirectory(const QString&);

	/// @brief Open a new text editor with a file dialog prompting to open one or more files.
	QList<VipTextEditor*> open();
	/// @brief Open a text file.
	VipTextEditor* openFile(const QString& filename);
	/// @brief Create a new editor with default text content.
	VipTextEditor* newFile();

	/// @brief Save to current editor file,
	/// and prompt for a filename if non was set.
	void save();
	/// @brief Save current editor to another file.
	void saveAs();
	/// @brief Save all to files.
	void saveAll();

	/// @brief Comment selection in current editor.
	void comment();
	/// @brief Uncomment selection in current editor.
	void uncomment();
	/// @brief Indent selection in current editor.
	void indent();
	/// @brief Unindent selection in current editor.
	void unindent();

	/// @brief Close the search bar
	void closeSearch();
	/// @brief Show the search bar
	void showSearch();
	/// @brief Show the search bar and, the focus to the search text,
	/// and fill the search text with current selection.
	void showSearchAndFocus();
	/// @brief Show/hide the search bar
	void setSearchVisible(bool);

	/// @brief Save state: editors order, opened files...
	QByteArray saveState() const;
	/// @brief Restore previously saved state.
	void restoreState(const QByteArray& state);

private Q_SLOTS:

	void aboutToClose(int);
	void setHeaderBarVisibility();
	void modificationChanged(bool modify);
	void currentChanged(int);

Q_SIGNALS:
	/// @brief Emitted whenever an editor's text changed, or when creating/closing editors.
	void modified(bool);

private:
	void setEditorFilename(VipTextEditor* editor, const QString& filename);
	int nextId() const;
	bool save(VipTextEditor* editor);
	int createEditor(const QString& filename = QString());

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
