

#ifndef VIP_TEXT_EDITOR_H
#define VIP_TEXT_EDITOR_H

#include <QPlainTextEdit>
#include <QFileInfo>
#include <QObject>

#include "VipConfig.h"
#include "VipPimpl.h"

class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;

class VipEditorFilter;
class VipTextHighlighter;

typedef QMap<QString, QString> StringMap;

/// @brief Text editor class, mainly used for Python code
///
/// VipTextEditor is a simple extension of QPlainTextEdit
/// providing the following additional features:
/// -	Saving/loading to to/from files
/// -	Support for line number widget
/// -	Support for custom syntax highlighter and keys filtering.
///
/// VipTextEditor is usually used through a VipTabEditor object.
///
class VIP_GUI_EXPORT VipTextEditor : public QPlainTextEdit
{
	Q_OBJECT
	friend class LineNumberArea;

public:
	VipTextEditor(QWidget* parent = 0);
	~VipTextEditor();

	/// @brief Returns all currently existing code editors
	static QList<VipTextEditor*> editors();

	/// @brief Returns the widget used to draw line numbers
	QWidget* lineNumberArea() const;

	bool lineNumberVisible() const;

	/// @brief Returns current QFileInfo (if any)
	const QFileInfo& fileInfo() const;

	/// @briefReturns true if the editor content is empty and if it is not saved to the disk.
	/// This means that the editor can used to load another file's content.
	bool isEmpty() const;

	/// @brief Returns the preferred LineNumberArea widget width
	int lineNumberAreaWidth();

	/// @brief Set/get line number area background color
	QColor lineAreaBackground() const;

	/// @brief Set/get line color between the line area and the editor
	QColor lineAreaBorder() const;

	/// @brief Set/get line number text color
	QColor lineNumberColor() const;

	/// @brief Set/get line number text font
	QFont lineNumberFont() const;

	/// @brief Set/get the background color of the current line
	QColor currentLineColor() const;

	/// @brief Set/get the editor background color
	QColor backgroundColor() const;

	/// @brief Set/get the editor border color
	QColor borderColor() const;

	/// @brief Set/get the default edtior text color
	QColor textColor() const;

	/// @brief Set the color scheme used by this editor.
	/// Provided color scheme is cloned, and the copy is managed by the editor.
	/// This also set the VipEditorFilter if the VipTextHighlighter
	/// can create a valid one.
	///
	void setColorScheme(const VipTextHighlighter* h);
	VipTextHighlighter* colorScheme() const;
	VipEditorFilter* editorFilter() const;

	/// @brief Returns all registered color schemes
	static QList<const VipTextHighlighter*> colorSchemes();
	/// @brief Returns all registered color schemes supporting given file suffix
	static QList<const VipTextHighlighter*> colorSchemes(const QString& extension);
	/// @brief Returns all color scheme names supporting provided type (like 'Python')
	static QStringList colorSchemesNames(const QString& type);
	/// @brief Returns the color scheme with given name and type
	static const VipTextHighlighter* colorScheme(const QString& type, const QString& name);
	/// @brief Returns the first found color scheme type supporting provided file suffix
	static QString typeForExtension(const QString& ext);
	/// @brief Register a new color scheme.
	/// An internal structure will take ownership of the VipTextHighlighter.
	static void registerColorScheme(VipTextHighlighter*);
	/// @brief Returns the supported file filters based on registered color schemes.
	/// The filters can be passed to VipFileDialog functions.
	static QString supportedFilters();

	/// @brief Set the default color scheme for given type
	static void setStdColorSchemeForType(const QString& type, const VipTextHighlighter*);
	/// @brief Set the default color scheme for given type
	static void setStdColorSchemeForType(const QString& type, const QString& name);
	/// @brief Returns the default color scheme for given type
	static const VipTextHighlighter* stdColorSchemeForType(const QString& type);
	/// @brief Returns the default color scheme for given file suffix
	static const VipTextHighlighter* stdColorSchemeForExt(const QString& extension);

	/// @brief Returns the standard color schemes as a string map
	static StringMap stdColorSchemes();
	static void setStdColorSchemes(const StringMap& map);

public Q_SLOTS:
	/// @brief Reload opened file
	void reload();
	/// @brief Open file content
	bool openFile(const QString& filename);
	/// @brief Save to file
	bool saveToFile(const QString& filename);
	void setLineAreaBackground(const QColor&);
	void setLineAreaBorder(const QColor&);
	void setLineNumberColor(const QColor&);
	void setLineNumberFont(const QFont&);
	void setCurrentLineColor(const QColor&);
	void setBackgroundColor(const QColor&);
	void setBorderColor(const QColor&);
	void setTextColor(const QColor&);
	void setLineNumberVisible(bool);

protected:
	void resizeEvent(QResizeEvent* event);

private Q_SLOTS:
	void updateLineNumberAreaWidth(int newBlockCount);
	void highlightCurrentLine();
	void updateLineNumberArea(const QRect&, int);
	void emitSaveTriggered() { Q_EMIT saveTriggered(); }
	void emitSearchTriggered() { Q_EMIT searchTriggered(); }

Q_SIGNALS:
	void saved(const QString&);
	void saveTriggered();
	void searchTriggered();

private:
	void lineNumberAreaPaintEvent(QPaintEvent* event);
	void formatStyleSheet();

	VIP_DECLARE_PRIVATE_DATA();
};

Q_DECLARE_METATYPE(StringMap)

#endif
