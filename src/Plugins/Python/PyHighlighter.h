#ifndef PY_HIGHLIGHTER_H
#define PY_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QHash>
#include <QTextCharFormat>

#include "PyConfig.h"


class QTextDocument;
class CodeEditor;

class BaseHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	QString name;
	QString type;
	QStringList extensions;

	BaseHighlighter(const QString & name, const QString & type, const QStringList & extensions, QTextDocument *parent = 0)
		:QSyntaxHighlighter(parent), name(name), type(type), extensions(extensions), enableRehighlight(true), dirtyRehighlight(true)
	{}

	virtual void updateEditor(CodeEditor*) const = 0;
	virtual BaseHighlighter * clone(QTextDocument * parent = nullptr) const = 0;
	virtual QColor backgroundColor() const = 0;

protected Q_SLOTS:
void rehighlightDelayed()
{
	if (enableRehighlight) {
		if (dirtyRehighlight) {
			QMetaObject::invokeMethod(this, "rehighlightInternal", Qt::QueuedConnection);
			dirtyRehighlight = false;
		}
	}
}

private Q_SLOTS:
void rehighlightInternal() {
	QSyntaxHighlighter::rehighlight();
	dirtyRehighlight = true;
}

private:
	bool enableRehighlight;
	bool dirtyRehighlight;
};

class PYTHON_EXPORT PyBaseHighlighter : public BaseHighlighter
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

	PyBaseHighlighter(const QString & name, const QString & type, const QStringList & extensions, QTextDocument *parent = 0);
	void Enable(bool e);

protected:
    void highlightBlock(const QString &text);
	void updateRules();
private:
    struct HighlightingRule
    {
        QRegExp pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QRegExp commentStartExpression;
    QRegExp commentEndExpression;
	QRegExp real;

	int m_current_str_number;
	char m_token[4];
	bool m_enable;

	QByteArray removeStringsAndComments(const QString & code, int start, int & inside_string_start,
		QList<QPair<int, int> > & strings, QList<QPair<int, int> > & comments);
};


struct PyDevScheme : PyBaseHighlighter
{
	PyDevScheme(QTextDocument * parent = nullptr) 
		:PyBaseHighlighter("Pydev", "Python", QStringList() << "py", parent) 
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

	virtual PyDevScheme * clone(QTextDocument * parent = nullptr) const {
		return new PyDevScheme(parent);
	}

	virtual void updateEditor(CodeEditor * editor) const;
	virtual QColor backgroundColor() const { return Qt::white; }
};

struct PyDarkScheme : PyBaseHighlighter
{
	PyDarkScheme(QTextDocument * parent = nullptr)
		:PyBaseHighlighter("Dark", "Python", QStringList() << "py", parent)
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

	virtual PyDarkScheme * clone(QTextDocument * parent = nullptr) const {
		return new PyDarkScheme(parent);
	}

	virtual void updateEditor(CodeEditor * editor) const;
	virtual QColor backgroundColor() const { return QColor(0x272822); }
};

struct SpyderDarkScheme : PyBaseHighlighter
{
	SpyderDarkScheme(QTextDocument* parent = nullptr)
		:PyBaseHighlighter("Spyder Dark", "Python", QStringList() << "py", parent)
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

	virtual SpyderDarkScheme* clone(QTextDocument* parent = nullptr) const {
		return new SpyderDarkScheme(parent);
	}

	virtual void updateEditor(CodeEditor* editor) const;
	virtual QColor backgroundColor() const { return QColor(0x272822); }
};

struct PyZenburnScheme : PyBaseHighlighter
{
	PyZenburnScheme(QTextDocument * parent = nullptr)
		:PyBaseHighlighter("Zenburn", "Python", QStringList() << "py", parent)
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

	virtual PyZenburnScheme * clone(QTextDocument * parent = nullptr) const {
		return new PyZenburnScheme(parent);
	}

	virtual void updateEditor(CodeEditor * editor) const;
	virtual QColor backgroundColor() const { return QColor(0x3F3F3F); }
};


struct TextScheme : BaseHighlighter
{
	TextScheme(QTextDocument * parent = nullptr)
		:BaseHighlighter("Text", "Text", QStringList() << "txt", parent)
	{
	}

	virtual TextScheme * clone(QTextDocument * parent = nullptr) const {
		return new TextScheme(parent);
	}

	virtual void updateEditor(CodeEditor * ) const {}

	virtual void highlightBlock(const QString &) {}
	virtual QColor backgroundColor() const {
		return Qt::white;
	}
};

#endif
