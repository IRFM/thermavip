#ifndef VIP_TEXT_HIGHLIGHTER_H
#define VIP_TEXT_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include "PyConfig.h"
#include "VipEditorFilter.h"
#include "VipConfig.h"

class QTextDocument;
class VipTextEditor;

/// @brief Base highlighter class for VipTextEditor widget.
///
/// VipTextHighlighter is a QSyntaxHighlighter with additional features:
/// -	Virtual function updateEditor() to directly modify the parent VipTextEditor
/// -	Virtual function createFilter() to create a custom VipEditorFilter for the parent VipTextEditor
/// -	Each VipTextHighlighter has a name (like 'PyDev'), a type (like 'Python' or 'Text') and a list
///		of supported suffixes (like 'py' or 'txt').
///
/// New VipTextHighlighter instance are registered using VipTextEditor::registerColorScheme().
///
class PYTHON_EXPORT VipTextHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	QString name;		//! Highlighter name, like 'Pydev'
	QString type;		//! Highlighter type, like 'Python'
	QStringList extensions; //! Supported file suffixes (like 'py')

	/// @brief Construct from the highlighter name, type and suffixes
	VipTextHighlighter(const QString& name, const QString& type, const QStringList& extensions, QTextDocument* parent = 0)
	  : QSyntaxHighlighter(parent)
	  , name(name)
	  , type(type)
	  , extensions(extensions)
	  , d_enableRehighlight(true)
	  , d_dirtyRehighlight(true)
	{
	}

	/// @brief Update the code editor: set the background color line number color...
	virtual void updateEditor(VipTextEditor*) const = 0;
	/// @brief Clone the highlighter
	virtual VipTextHighlighter* clone(QTextDocument* parent = nullptr) const = 0;
	/// @brief Returns the default text background color
	virtual QColor backgroundColor() const = 0;
	/// @brief Create a filter used by the editor
	virtual VipEditorFilter* createFilter(VipTextEditor* parent) const { return new VipEditorFilter(parent); }

protected Q_SLOTS:
	void rehighlightDelayed()
	{
		if (d_enableRehighlight) {
			if (d_dirtyRehighlight) {
				QMetaObject::invokeMethod(this, "rehighlightInternal", Qt::QueuedConnection);
				d_dirtyRehighlight = false;
			}
		}
	}

private Q_SLOTS:
	void rehighlightInternal()
	{
		QSyntaxHighlighter::rehighlight();
		d_dirtyRehighlight = true;
	}

private:
	bool d_enableRehighlight;
	bool d_dirtyRehighlight;
};

/// @brief Base class for Python syntax highlighter classes
///
class PYTHON_EXPORT VipPyBaseHighlighter : public VipTextHighlighter
{
	Q_OBJECT

public:
	QTextCharFormat keywordFormat;
	QTextCharFormat predefineFormat;
	QTextCharFormat singleLineCommentFormat;
	QTextCharFormat multiLineCommentFormat;
	QTextCharFormat quotationFormat;
	QTextCharFormat functionFormat;
	QTextCharFormat numberFormat;

	VipPyBaseHighlighter(const QString& name, const QString& type, const QStringList& extensions, QTextDocument* parent = 0);
	~VipPyBaseHighlighter();

	virtual VipEditorFilter* createFilter(VipTextEditor*) const;

protected:
	virtual void highlightBlock(const QString& text);
	void updateRules();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);

	QByteArray removeStringsAndComments(const QString& code, int start, int& inside_string_start, QList<QPair<int, int>>& strings, QList<QPair<int, int>>& comments);
};

struct PYTHON_EXPORT VipPyDevScheme : VipPyBaseHighlighter
{
	VipPyDevScheme(QTextDocument* parent = nullptr)
	  : VipPyBaseHighlighter("Pydev", "Python", QStringList() << "py", parent)
	{
		keywordFormat.setForeground(QColor(0x0000FF));
		keywordFormat.setFontWeight(QFont::Normal);
		predefineFormat.setFontWeight(QFont::Normal);
		predefineFormat.setForeground(QColor(0x900090));
		quotationFormat.setForeground(QColor(0x00AA00));
		singleLineCommentFormat.setForeground(QColor(0xC0C0C0));
		functionFormat.setFontWeight(QFont::Bold);
		functionFormat.setFontItalic(false);
		functionFormat.setForeground(Qt::black);
		numberFormat.setForeground(QColor(0x800066));
		updateRules();
	}

	virtual VipPyDevScheme* clone(QTextDocument* parent = nullptr) const { return new VipPyDevScheme(parent); }
	virtual void updateEditor(VipTextEditor* editor) const;
	virtual QColor backgroundColor() const { return Qt::white; }
};

struct PYTHON_EXPORT VipPyDarkScheme : VipPyBaseHighlighter
{
	VipPyDarkScheme(QTextDocument* parent = nullptr)
	  : VipPyBaseHighlighter("Dark", "Python", QStringList() << "py", parent)
	{
		keywordFormat.setForeground(QColor(0x558EFF));
		keywordFormat.setFontWeight(QFont::Normal);
		predefineFormat.setFontWeight(QFont::Bold);
		predefineFormat.setForeground(QColor(0xAA00AA));
		quotationFormat.setForeground(QColor(0x11A642));
		singleLineCommentFormat.setForeground(QColor(0x7F7F7F));
		functionFormat.setFontWeight(QFont::Bold);
		functionFormat.setFontItalic(false);
		functionFormat.setForeground(Qt::white);
		numberFormat.setForeground(QColor(0xC80000));
		updateRules();
	}

	virtual VipPyDarkScheme* clone(QTextDocument* parent = nullptr) const { return new VipPyDarkScheme(parent); }

	virtual void updateEditor(VipTextEditor* editor) const;
	virtual QColor backgroundColor() const { return QColor(0x272822); }
};

struct PYTHON_EXPORT VipSpyderDarkScheme : VipPyBaseHighlighter
{
	VipSpyderDarkScheme(QTextDocument* parent = nullptr)
	  : VipPyBaseHighlighter("Spyder Dark", "Python", QStringList() << "py", parent)
	{
		keywordFormat.setForeground(QColor(0xC670E0));
		keywordFormat.setFontWeight(QFont::Normal);
		predefineFormat.setFontWeight(QFont::Bold);
		predefineFormat.setForeground(QColor(0xFAB16C));
		quotationFormat.setForeground(QColor(0xB0E686));
		singleLineCommentFormat.setForeground(QColor(0x999999));
		functionFormat.setFontWeight(QFont::Bold);
		functionFormat.setFontItalic(false);
		functionFormat.setForeground(QColor(0x57D6E4));
		numberFormat.setForeground(QColor(0xFAED5C));
		updateRules();
	}

	virtual VipSpyderDarkScheme* clone(QTextDocument* parent = nullptr) const { return new VipSpyderDarkScheme(parent); }

	virtual void updateEditor(VipTextEditor* editor) const;
	virtual QColor backgroundColor() const { return QColor(0x272822); }
};

struct PYTHON_EXPORT VipPyZenburnScheme : VipPyBaseHighlighter
{
	VipPyZenburnScheme(QTextDocument* parent = nullptr)
	  : VipPyBaseHighlighter("Zenburn", "Python", QStringList() << "py", parent)
	{
		keywordFormat.setForeground(QColor(0xDFAF8F));
		keywordFormat.setFontWeight(QFont::Bold);
		predefineFormat.setFontWeight(QFont::Bold);
		predefineFormat.setForeground(QColor(0xEFEF8F));
		quotationFormat.setForeground(QColor(0xCC9393));
		singleLineCommentFormat.setForeground(QColor(0x7F9F7F));
		functionFormat.setForeground(QColor(0xEFEF8F));
		functionFormat.setFontItalic(false);
		numberFormat.setForeground(QColor(0x8CD0D3));
		updateRules();
	}

	virtual VipPyZenburnScheme* clone(QTextDocument* parent = nullptr) const { return new VipPyZenburnScheme(parent); }

	virtual void updateEditor(VipTextEditor* editor) const;
	virtual QColor backgroundColor() const { return QColor(0x3F3F3F); }
};

struct PYTHON_EXPORT VipTextScheme : VipTextHighlighter
{
	VipTextScheme(QTextDocument* parent = nullptr)
	  : VipTextHighlighter("Text", "Text", QStringList() << "txt", parent)
	{
	}

	virtual VipTextScheme* clone(QTextDocument* parent = nullptr) const { return new VipTextScheme(parent); }

	virtual void updateEditor(VipTextEditor*) const;
	virtual void highlightBlock(const QString&) {}
	virtual QColor backgroundColor() const { return Qt::white; }
};

#endif
