#include <QFont>
#include <QFile>
#include <qpainter.h>
#include <QTextBlock>

#include "VipGui.h"

#include "CodeEditor.h"
#include "PyHighlighter.h"


static QList<CodeEditor*> & _editors()
{
	static QList<CodeEditor*> ed;
	return ed;
}

CodeEditor::CodeEditor(QWidget *parent) : QPlainTextEdit(parent)
{
	//setFrameShape(QFrame::NoFrame);
	//setStyleSheet("border : none;");
	m_line = -1;
	m_lineNumberArea = new LineNumberArea(this);

	m_lineAreaBackground = Qt::white;
	m_lineAreaBorder = Qt::transparent;
	m_lineNumberColor = Qt::lightGray;
	m_currentLine = Qt::transparent;
	m_background = Qt::transparent;
	m_border = Qt::transparent;
	m_text = Qt::transparent;

	connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumberAreaWidth(int)));
	connect(this, SIGNAL(updateRequest(QRect, int)), this, SLOT(updateLineNumberArea(QRect, int)));
	connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(highlightCurrentLine()));

	/*#ifdef Q_OS_WIN
	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	setFont(font);
	m_lineNumberFont = font;
	#endif*/

	setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());

	this->setLineWrapMode(QPlainTextEdit::NoWrap);

	updateLineNumberAreaWidth(0);
	highlightCurrentLine();

	_editors().append(this);
}

CodeEditor::~CodeEditor()
{
	_editors().removeOne(this);
}

QList<CodeEditor*> CodeEditor::editors()
{
	return _editors();
}

bool CodeEditor::OpenFile(const QString & filename)
{
	QFile file(filename);
	if (!file.open(QFile::ReadOnly))
	{
		this->document()->setModified(false);
		return false;
	}
	else
	{
		this->setPlainText(QString(file.readAll()));
		m_info = QFileInfo(filename);
		this->document()->setModified(true);
		this->document()->setModified(false);
		return true;
	}
}

bool CodeEditor::SaveToFile(const QString & filename)
{
	QFile file(filename);
	if (!file.open(QFile::WriteOnly))
		return false;
	else
	{
		file.write(this->toPlainText().toLatin1());
		m_info = QFileInfo(filename);
		this->document()->setModified(true);
		this->document()->setModified(false);

		if (BaseHighlighter * h = stdColorSchemeForExt(m_info.suffix()))
		{
			BaseHighlighter * tmp = this->findChild<BaseHighlighter*>();
			if (tmp) delete tmp;
			h = h->clone(document());
			h->updateEditor(this);
			h->rehighlight();
		}
		Q_EMIT saved(FileInfo().canonicalFilePath());
		return true;
	}
}

void CodeEditor::setColorScheme(BaseHighlighter * h)
{
	BaseHighlighter * tmp = this->findChild<BaseHighlighter*>();
	if (tmp) delete tmp;
	h = h->clone(document());
	h->updateEditor(this);
	h->rehighlight();
}

BaseHighlighter * CodeEditor::colorScheme() const
{
	return this->findChild<BaseHighlighter*>();
}

const QFileInfo & CodeEditor::FileInfo() const
{
	return m_info;
}

#include <qscrollbar.h>
void CodeEditor::reload()
{
	if (FileInfo().exists()) {
		bool at_end = false;
		if (this->verticalScrollBar()->isHidden())
			at_end = true;
		else if (this->verticalScrollBar()->value() == this->verticalScrollBar()->maximum())
			at_end = true;

		int value = this->verticalScrollBar()->value();

		//reload file, try to keep the current visible line
		QFile file(FileInfo().canonicalFilePath());
		if (file.open(QFile::ReadOnly))
		{
			this->setPlainText(QString(file.readAll()));
			this->document()->setModified(false);

			if (at_end && this->verticalScrollBar()->isVisible())
				this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
			else if (this->verticalScrollBar()->isVisible())
				this->verticalScrollBar()->setValue(value);
		}
	}
}

bool CodeEditor::isEmpty() const
{
	return !FileInfo().exists() && toPlainText().isEmpty();
}


int CodeEditor::lineNumberAreaWidth()
{
	int digits = 1;
	int max = qMax(1, blockCount());
	while (max >= 10) {
		max /= 10;
		++digits;
	}

	int space = 8 + fontMetrics().width(QLatin1Char('9')) * digits;

	return space;
}



void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
	setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}



void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
	if (dy)
		m_lineNumberArea->scroll(0, dy);
	else
		m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

	if (rect.contains(viewport()->rect()))
		updateLineNumberAreaWidth(0);
}



void CodeEditor::resizeEvent(QResizeEvent *e)
{
	QPlainTextEdit::resizeEvent(e);

	QRect cr = contentsRect();
	m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}



void CodeEditor::highlightCurrentLine()
{
	/*QList<QTextEdit::ExtraSelection> extraSelections;

	if (!isReadOnly()) {
	QTextEdit::ExtraSelection selection;

	QColor lineColor = m_currentLine;//QColor(0x2691AF);//.lighter(160);

	selection.format.setBackground(lineColor);
	selection.format.setProperty(QTextFormat::FullWidthSelection, true);
	selection.cursor = textCursor();
	selection.cursor.clearSelection();
	extraSelections.append(selection);
	}

	setExtraSelections(extraSelections);*/



	if (!isReadOnly()) {
		QList<QTextEdit::ExtraSelection> selections = extraSelections();
		//remove previous

		for (int i = 0; i < selections.size(); ++i)
			if (selections[i].format.property(QTextFormat::UserProperty + 1).toBool()) {
				selections.removeAt(i);
				--i;
			}

		QColor lineColor = m_currentLine;//QColor(0x2691AF);//.lighter(160);
		QTextEdit::ExtraSelection selection;
		selection.format.setBackground(lineColor);
		selection.format.setProperty(QTextFormat::UserProperty + 1, true);
		selection.format.setProperty(QTextFormat::FullWidthSelection, true);
		selection.cursor = textCursor();
		selection.cursor.clearSelection();
		selections.insert(0, selection);
		setExtraSelections(selections);
	}




	/*if (!isReadOnly()) {
	//remove previous
	if (m_line >= 0) {
	QTextBlock b = document()->findBlockByLineNumber(m_line);
	if (b.isValid()) {
	QVector<QTextLayout::FormatRange> ranges = b.layout()->formats();
	for (int i = 0; i < ranges.size(); ++i) {
	if (ranges[i].format.property(QTextFormat::UserProperty +1).toBool()) {
	ranges.removeAt(i);
	--i;
	}
	}
	b.layout()->setFormats(ranges);
	document()->markContentsDirty(b.position(), b.length());
	}
	}
	QTextBlock b = textCursor().block();
	if (b.isValid()) {
	m_line = b.firstLineNumber();
	QVector<QTextLayout::FormatRange> ranges = b.layout()->formats();
	QTextLayout::FormatRange f;
	f.format = b.charFormat();
	f.format.setProperty(QTextFormat::FullWidthSelection, true);
	QColor lineColor = m_currentLine;//QColor(0x2691AF);//.lighter(160);
	f.format.setBackground(lineColor);
	f.format.setProperty(QTextFormat::UserProperty + 1, true);
	ranges.append(f);
	b.layout()->setFormats(ranges);
	document()->markContentsDirty(b.position(), b.length());
	}
	}*/
}

QColor CodeEditor::lineAreaBackground() const
{
	return m_lineAreaBackground;
}
void CodeEditor::setLineAreaBackground(const QColor & c)
{
	m_lineAreaBackground = c;
	update();
}

QColor CodeEditor::lineAreaBorder() const
{
	return m_lineAreaBorder;
}
void CodeEditor::setLineAreaBorder(const QColor & c)
{
	m_lineAreaBorder = c;
	update();
}

QColor CodeEditor::lineNumberColor() const
{
	return m_lineNumberColor;
}
void CodeEditor::setLineNumberColor(const QColor & c)
{
	m_lineNumberColor = c;
	update();
}

QFont CodeEditor::lineNumberFont() const
{
	return m_lineNumberFont;
}
void CodeEditor::setLineNumberFont(const QFont & f)
{
	m_lineNumberFont = f;
	update();
}

void CodeEditor::setCurrentLineColor(const QColor & c)
{
	m_currentLine = c;
	update();
	highlightCurrentLine();
}
QColor CodeEditor::currentLineColor() const
{
	return m_currentLine;
}

void CodeEditor::setBackgroundColor(const QColor & c)
{
	m_background = c;
	formatStyleSheet();
}
QColor CodeEditor::backgroundColor() const
{
	return m_background;
}

void CodeEditor::setBorderColor(const QColor & c)
{
	m_border = c;
	formatStyleSheet();
}
QColor CodeEditor::borderColor() const
{
	return m_border;
}

void CodeEditor::setTextColor(const QColor & c)
{
	m_text = c;
	formatStyleSheet();
}
QColor CodeEditor::textColor() const
{
	return m_text;
}

void CodeEditor::formatStyleSheet()
{
	QString background, border, text;
	if (m_background != Qt::transparent)
		background = "background-color: rgb(" + QString::number(m_background.red()) + ", " + QString::number(m_background.green()) + ", " + QString::number(m_background.blue()) + ");\n";
	if (m_border != Qt::transparent)
		border = "border-color: rgb(" + QString::number(m_border.red()) + ", " + QString::number(m_border.green()) + ", " + QString::number(m_border.blue()) + ");\n";
	if (m_text != Qt::transparent)
		text = "color: rgb(" + QString::number(m_text.red()) + ", " + QString::number(m_text.green()) + ", " + QString::number(m_text.blue()) + ");\n";

	if (background.isEmpty() && border.isEmpty() && text.isEmpty())
		setStyleSheet("");
	else
		setStyleSheet("CodeEditor {\n" + background + border + text + "}");
}

void CodeEditor::setDefaultStyle(const QString & type_and_name)
{
	QStringList lst = type_and_name.split(":");
	if (lst.size() != 2)
		return;

	if (BaseHighlighter * sh = colorScheme(lst[0], lst[1]))
		setStdColorSchemeForType(lst[0], sh);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
	QPainter painter(m_lineNumberArea);
	painter.fillRect(event->rect(), this->lineAreaBackground());
	painter.setPen(this->lineAreaBorder());
	painter.drawLine(event->rect().topRight(), event->rect().bottomRight());


	QTextBlock block = firstVisibleBlock();
	int blockNumber = block.blockNumber();
	int top = (int)blockBoundingGeometry(block).translated(contentOffset()).top();
	int bottom = top + (int)blockBoundingRect(block).height();

	while (block.isValid() && top <= event->rect().bottom()) {
		if (block.isVisible() && bottom >= event->rect().top()) {
			QString number = QString::number(blockNumber + 1);
			painter.setPen(this->lineNumberColor());
			painter.setFont(this->lineNumberFont());
			painter.drawText(-3, top, m_lineNumberArea->width(), QFontMetrics(this->lineNumberFont()).height(),
				Qt::AlignRight, number);
		}

		block = block.next();
		top = bottom;
		bottom = top + (int)blockBoundingRect(block).height();
		++blockNumber;
	}
}



struct __ColorScheme
{
	QList<BaseHighlighter*> schemes;
	QMap<QString, BaseHighlighter*> stdSchemes;
	~__ColorScheme()
	{
		for (int i = 0; i < schemes.size(); i++)
			delete schemes[i];
	}
};

__ColorScheme & __schemes() {
	static __ColorScheme sh;
	return sh;
}

static void updateEditors(BaseHighlighter* sh)
{
	QList<CodeEditor*> editors = CodeEditor::editors();
	for (int i = 0; i < editors.size(); ++i)
	{
		BaseHighlighter * tmp = editors[i]->document()->findChild<BaseHighlighter*>();

		if (sh->extensions.indexOf(editors[i]->FileInfo().suffix()) >= 0) {
			if (tmp) delete tmp;
			sh = sh->clone(editors[i]->document());
			sh->updateEditor(editors[i]);
			sh->rehighlight();
		}
		else if (tmp) {
			//check if the editor has a highlighter with the right type()
			if (tmp->type == sh->type) {
				//remove highlighter and install new one
				delete tmp;
				sh = sh->clone(editors[i]->document());
				sh->updateEditor(editors[i]);
				sh->rehighlight();
			}
		}
	}

}

QList<BaseHighlighter*> CodeEditor::colorSchemes()
{
	return __schemes().schemes;
}

QList<BaseHighlighter*> CodeEditor::colorSchemes(const QString & extension)
{
	const QList<BaseHighlighter*> sh = colorSchemes();
	QList<BaseHighlighter*> res;
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->extensions.indexOf(extension) >= 0)
			res.append(sh[i]);
	return res;
}

QStringList CodeEditor::colorSchemesNames(const QString & type)
{
	QStringList res;
	const QList<BaseHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->type == type)
			res.append(sh[i]->name);
	return res;
}

BaseHighlighter * CodeEditor::colorScheme(const QString & type, const QString & name)
{
	const QList<BaseHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->type == type && sh[i]->name == name)
			return sh[i];
	return NULL;
}

QString CodeEditor::typeForExtension(const QString & ext)
{
	const QList<BaseHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->extensions.indexOf(ext) >= 0)
			return sh[i]->type;
	return QString();
}

void CodeEditor::registerColorScheme(BaseHighlighter* sh)
{
	__schemes().schemes.append(sh);
	if (__schemes().stdSchemes.find(sh->type) == __schemes().stdSchemes.end())
	{
		__schemes().stdSchemes[sh->type] = sh;
		updateEditors(sh);
	}
}

void CodeEditor::setStdColorSchemeForType(const QString & type, BaseHighlighter* sh)
{
	__schemes().stdSchemes[type] = sh;
	updateEditors(sh);
}

void CodeEditor::setStdColorSchemeForType(const QString & type, const QString & name)
{
	const QList<BaseHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
	{
		if (sh[i]->type == type && sh[i]->name == name)
		{
			setStdColorSchemeForType(type, sh[i]);
			break;
		}
	}
}

BaseHighlighter* CodeEditor::stdColorSchemeForType(const QString & type)
{
	QMap<QString, BaseHighlighter*>::iterator it = __schemes().stdSchemes.find(type);
	if (it == __schemes().stdSchemes.end())
		return NULL;
	else
		return it.value();
}

BaseHighlighter* CodeEditor::stdColorSchemeForExt(const QString & extension)
{
	QString type = typeForExtension(extension);
	if (type.isEmpty())
		return NULL;
	return stdColorSchemeForType(type);
}

StringMap CodeEditor::stdColorSchemes()
{
	QMap<QString, QString> res;
	for (QMap<QString, BaseHighlighter*>::iterator it = __schemes().stdSchemes.begin(); it != __schemes().stdSchemes.end(); ++it)
		res[it.key()] = it.value()->name;
	return res;
}

void CodeEditor::setStdColorSchemes(const StringMap & map)
{
	if (!map.isEmpty())
	{
		__schemes().stdSchemes.clear();
		for (QMap<QString, QString>::const_iterator it = map.begin(); it != map.end(); ++it)
			if (BaseHighlighter * sh = colorScheme(it.key(), it.value()))
			{
				__schemes().stdSchemes[it.key()] = sh;
				updateEditors(sh);
			}
	}
}
