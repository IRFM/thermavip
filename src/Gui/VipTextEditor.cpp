#include <QFont>
#include <QFile>
#include <qpainter.h>
#include <QTextBlock>
#include <qscrollbar.h>

#include "VipGui.h"

#include "VipTextEditor.h"
#include "VipEditorFilter.h"
#include "VipTextHighlighter.h"

class LineNumberArea : public QWidget
{
public:
	LineNumberArea(VipTextEditor* editor)
	  : QWidget(editor)
	{
		codeEditor = editor;
	}

	QSize sizeHint() const { return QSize(codeEditor->lineNumberAreaWidth(), 0); }

protected:
	void paintEvent(QPaintEvent* event) { codeEditor->lineNumberAreaPaintEvent(event); }

private:
	VipTextEditor* codeEditor;
};

class VipTextEditor::PrivateData
{
public:
	QWidget* lineNumberArea;
	QColor lineAreaBackground;
	QColor lineAreaBorder;
	QColor lineNumberColor;
	QFont lineNumberFont;
	QColor currentLine;
	QColor background;
	QColor border;
	QColor text;
	QFileInfo info;
	int line;
};

static QList<VipTextEditor*>& _editors()
{
	static QList<VipTextEditor*> ed;
	return ed;
}

VipTextEditor::VipTextEditor(QWidget* parent)
  : QPlainTextEdit(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	// setFrameShape(QFrame::NoFrame);
	// setStyleSheet("border : none;");
	d_data->line = -1;
	d_data->lineNumberArea = new LineNumberArea(this);

	d_data->lineAreaBackground = Qt::white;
	d_data->lineAreaBorder = Qt::transparent;
	d_data->lineNumberColor = Qt::lightGray;
	d_data->currentLine = Qt::transparent;
	d_data->background = Qt::transparent;
	d_data->border = Qt::transparent;
	d_data->text = Qt::transparent;

	connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumberAreaWidth(int)));
	connect(this, SIGNAL(updateRequest(QRect, int)), this, SLOT(updateLineNumberArea(QRect, int)));
	connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(highlightCurrentLine()));

	/*#ifdef Q_OS_WIN
	QFont font;
	font.setFamily("Courier");
	font.setFixedPitch(true);
	font.setPointSize(10);
	setFont(font);
	d_data->lineNumberFont = font;
	#endif*/

	setFont(VipGuiDisplayParamaters::instance()->defaultEditorFont());

	this->setLineWrapMode(QPlainTextEdit::NoWrap);

	updateLineNumberAreaWidth(0);
	highlightCurrentLine();

	_editors().append(this);
}

VipTextEditor::~VipTextEditor()
{
	_editors().removeOne(this);
}

QList<VipTextEditor*> VipTextEditor::editors()
{
	return _editors();
}

QWidget* VipTextEditor::lineNumberArea() const
{
	return d_data->lineNumberArea;
}

void VipTextEditor::setLineNumberVisible(bool vis)
{
	d_data->lineNumberArea->setVisible(vis);
	if (vis) {
		setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
	}
	else {
		setViewportMargins(0, 0, 0, 0);
	}
}
bool VipTextEditor::lineNumberVisible() const
{
	return d_data->lineNumberArea->isVisible();
}

bool VipTextEditor::openFile(const QString& filename)
{
	QFile file(filename);
	if (!file.open(QFile::ReadOnly)) {
		this->document()->setModified(false);
		return false;
	}
	else {
		this->setPlainText(QString(file.readAll()));
		d_data->info = QFileInfo(filename);
		this->document()->setModified(true);
		this->document()->setModified(false);
		return true;
	}
}

bool VipTextEditor::saveToFile(const QString& filename)
{
	QFile file(filename);
	if (!file.open(QFile::WriteOnly))
		return false;
	else {
		file.write(this->toPlainText().toLatin1());
		d_data->info = QFileInfo(filename);
		this->document()->setModified(true);
		this->document()->setModified(false);

		if (const VipTextHighlighter* h = stdColorSchemeForExt(d_data->info.suffix())) {
			setColorScheme(h);
		}
		Q_EMIT saved(fileInfo().canonicalFilePath());
		return true;
	}
}

void VipTextEditor::setColorScheme(const VipTextHighlighter* h)
{
	if (VipTextHighlighter* tmp = colorScheme())
		delete tmp;
	if (VipEditorFilter* f = editorFilter())
		delete f;
	if (h) {
		VipTextHighlighter* tmp = h->clone(document());
		tmp->updateEditor(this);
		tmp->rehighlight();
		if (auto* filter = tmp->createFilter(this)) {
			connect(filter, SIGNAL(saveTriggered()), this, SLOT(emitSaveTriggered()));
			connect(filter, SIGNAL(searchTriggered()), this, SLOT(emitSearchTriggered()));
		}
	}
}

VipTextHighlighter* VipTextEditor::colorScheme() const
{
	return this->document()->findChild<VipTextHighlighter*>();
}

VipEditorFilter* VipTextEditor::editorFilter() const
{
	return this->findChild<VipEditorFilter*>();
}

const QFileInfo& VipTextEditor::fileInfo() const
{
	return d_data->info;
}

void VipTextEditor::reload()
{
	if (fileInfo().exists()) {
		bool at_end = false;
		if (this->verticalScrollBar()->isHidden())
			at_end = true;
		else if (this->verticalScrollBar()->value() == this->verticalScrollBar()->maximum())
			at_end = true;

		int value = this->verticalScrollBar()->value();

		// reload file, try to keep the current visible line
		QFile file(fileInfo().canonicalFilePath());
		if (file.open(QFile::ReadOnly)) {
			this->setPlainText(QString(file.readAll()));
			this->document()->setModified(false);

			if (at_end && this->verticalScrollBar()->isVisible())
				this->verticalScrollBar()->setValue(this->verticalScrollBar()->maximum());
			else if (this->verticalScrollBar()->isVisible())
				this->verticalScrollBar()->setValue(value);
		}
	}
}

bool VipTextEditor::isEmpty() const
{
	return !fileInfo().exists() && toPlainText().isEmpty();
}

int VipTextEditor::lineNumberAreaWidth()
{
	int digits = 1;
	int max = qMax(1, blockCount());
	while (max >= 10) {
		max /= 10;
		++digits;
	}

#if QT_VERSION < QT_VERSION_CHECK(5, 11, 0)
	int space = 8 + fontMetrics().width(QLatin1Char('9')) * digits;
#else
	int space = 8 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
#endif	

	return space;
}

void VipTextEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
	setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void VipTextEditor::updateLineNumberArea(const QRect& rect, int dy)
{
	if (dy)
		d_data->lineNumberArea->scroll(0, dy);
	else
		d_data->lineNumberArea->update(0, rect.y(), d_data->lineNumberArea->width(), rect.height());

	if (rect.contains(viewport()->rect()))
		updateLineNumberAreaWidth(0);
}

void VipTextEditor::resizeEvent(QResizeEvent* e)
{
	QPlainTextEdit::resizeEvent(e);

	QRect cr = contentsRect();
	d_data->lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void VipTextEditor::highlightCurrentLine()
{
	/*QList<QTextEdit::ExtraSelection> extraSelections;

	if (!isReadOnly()) {
	QTextEdit::ExtraSelection selection;

	QColor lineColor = d_data->currentLine;//QColor(0x2691AF);//.lighter(160);

	selection.format.setBackground(lineColor);
	selection.format.setProperty(QTextFormat::FullWidthSelection, true);
	selection.cursor = textCursor();
	selection.cursor.clearSelection();
	extraSelections.append(selection);
	}

	setExtraSelections(extraSelections);*/

	if (!isReadOnly()) {
		QList<QTextEdit::ExtraSelection> selections = extraSelections();
		// remove previous

		for (int i = 0; i < selections.size(); ++i)
			if (selections[i].format.property(QTextFormat::UserProperty + 1).toBool()) {
				selections.removeAt(i);
				--i;
			}

		QColor lineColor = d_data->currentLine; // QColor(0x2691AF);//.lighter(160);
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
	if (d_data->line >= 0) {
	QTextBlock b = document()->findBlockByLineNumber(d_data->line);
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
	d_data->line = b.firstLineNumber();
	QVector<QTextLayout::FormatRange> ranges = b.layout()->formats();
	QTextLayout::FormatRange f;
	f.format = b.charFormat();
	f.format.setProperty(QTextFormat::FullWidthSelection, true);
	QColor lineColor = d_data->currentLine;//QColor(0x2691AF);//.lighter(160);
	f.format.setBackground(lineColor);
	f.format.setProperty(QTextFormat::UserProperty + 1, true);
	ranges.append(f);
	b.layout()->setFormats(ranges);
	document()->markContentsDirty(b.position(), b.length());
	}
	}*/
}

QColor VipTextEditor::lineAreaBackground() const
{
	return d_data->lineAreaBackground;
}
void VipTextEditor::setLineAreaBackground(const QColor& c)
{
	d_data->lineAreaBackground = c;
	update();
}

QColor VipTextEditor::lineAreaBorder() const
{
	return d_data->lineAreaBorder;
}
void VipTextEditor::setLineAreaBorder(const QColor& c)
{
	d_data->lineAreaBorder = c;
	update();
}

QColor VipTextEditor::lineNumberColor() const
{
	return d_data->lineNumberColor;
}
void VipTextEditor::setLineNumberColor(const QColor& c)
{
	d_data->lineNumberColor = c;
	update();
}

QFont VipTextEditor::lineNumberFont() const
{
	return d_data->lineNumberFont;
}
void VipTextEditor::setLineNumberFont(const QFont& f)
{
	d_data->lineNumberFont = f;
	update();
}

void VipTextEditor::setCurrentLineColor(const QColor& c)
{
	d_data->currentLine = c;
	update();
	highlightCurrentLine();
}
QColor VipTextEditor::currentLineColor() const
{
	return d_data->currentLine;
}

void VipTextEditor::setBackgroundColor(const QColor& c)
{
	d_data->background = c;
	formatStyleSheet();
}
QColor VipTextEditor::backgroundColor() const
{
	return d_data->background;
}

void VipTextEditor::setBorderColor(const QColor& c)
{
	d_data->border = c;
	formatStyleSheet();
}
QColor VipTextEditor::borderColor() const
{
	return d_data->border;
}

void VipTextEditor::setTextColor(const QColor& c)
{
	d_data->text = c;
	formatStyleSheet();
}
QColor VipTextEditor::textColor() const
{
	return d_data->text;
}

void VipTextEditor::formatStyleSheet()
{
	QString background, border, text;
	if (d_data->background != Qt::transparent)
		background = "background-color: rgb(" + QString::number(d_data->background.red()) + ", " + QString::number(d_data->background.green()) + ", " +
			     QString::number(d_data->background.blue()) + ");\n";
	if (d_data->border != Qt::transparent)
		border = "border-color: rgb(" + QString::number(d_data->border.red()) + ", " + QString::number(d_data->border.green()) + ", " + QString::number(d_data->border.blue()) + ");\n";
	if (d_data->text != Qt::transparent)
		text = "color: rgb(" + QString::number(d_data->text.red()) + ", " + QString::number(d_data->text.green()) + ", " + QString::number(d_data->text.blue()) + ");\n";

	if (background.isEmpty() && border.isEmpty() && text.isEmpty())
		setStyleSheet("");
	else
		setStyleSheet("VipTextEditor {\n" + background + border + text + "}");
}

void VipTextEditor::lineNumberAreaPaintEvent(QPaintEvent* event)
{
	QPainter painter(d_data->lineNumberArea);
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
			painter.drawText(-3, top, d_data->lineNumberArea->width(), QFontMetrics(this->lineNumberFont()).height(), Qt::AlignRight, number);
		}

		block = block.next();
		top = bottom;
		bottom = top + (int)blockBoundingRect(block).height();
		++blockNumber;
	}
}

struct __ColorScheme
{
	QList<const VipTextHighlighter*> schemes;
	QMap<QString, const VipTextHighlighter*> stdSchemes;
	~__ColorScheme()
	{
		for (int i = 0; i < schemes.size(); i++)
			delete const_cast<VipTextHighlighter*>(schemes[i]);
	}
};

__ColorScheme& __schemes()
{
	static __ColorScheme sh;
	return sh;
}

static void updateEditors(const VipTextHighlighter* sh)
{
	QList<VipTextEditor*> editors = VipTextEditor::editors();
	for (int i = 0; i < editors.size(); ++i) {
		VipTextHighlighter* tmp = editors[i]->document()->findChild<VipTextHighlighter*>();

		if (sh->extensions.indexOf(editors[i]->fileInfo().suffix()) >= 0) {
			editors[i]->setColorScheme(sh);
		}
		else if (tmp) {
			// check if the editor has a highlighter with the right type()
			if (tmp->type == sh->type) {
				// remove highlighter and install new one
				editors[i]->setColorScheme(sh);
			}
		}
	}
}

QList<const VipTextHighlighter*> VipTextEditor::colorSchemes()
{
	return __schemes().schemes;
}

QList<const VipTextHighlighter*> VipTextEditor::colorSchemes(const QString& extension)
{
	const QList<const VipTextHighlighter*> sh = colorSchemes();
	QList<const VipTextHighlighter*> res;
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->extensions.indexOf(extension) >= 0)
			res.append(sh[i]);
	return res;
}

QStringList VipTextEditor::colorSchemesNames(const QString& type)
{
	QStringList res;
	const QList<const VipTextHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->type == type)
			res.append(sh[i]->name);
	return res;
}

const VipTextHighlighter* VipTextEditor::colorScheme(const QString& type, const QString& name)
{
	const QList<const VipTextHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->type == type && sh[i]->name == name)
			return sh[i];
	return nullptr;
}

QString VipTextEditor::typeForExtension(const QString& ext)
{
	const QList<const VipTextHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i)
		if (sh[i]->extensions.contains(ext, Qt::CaseInsensitive))
			return sh[i]->type;
	return QString();
}

void VipTextEditor::registerColorScheme(VipTextHighlighter* sh)
{
	__schemes().schemes.append(sh);
	if (__schemes().stdSchemes.find(sh->type) == __schemes().stdSchemes.end()) {
		__schemes().stdSchemes[sh->type] = sh;
		updateEditors(sh);
	}
}

QString VipTextEditor::supportedFilters()
{
	QStringList filters;
	QStringList all_suffixes;
	for (auto it = __schemes().stdSchemes.begin(); it != __schemes().stdSchemes.end(); ++it) {
		QString type = it.key();
		QStringList suffixes = it.value()->extensions;
		for (QString& s : suffixes) {
			all_suffixes << "*." + s;
			s = "*." + s;
		}

		filters << type + " files (" + suffixes.join(" ") + ")";
	}
	filters.insert(0, "All files (" + all_suffixes.join(" ") + ")");
	return filters.join(";;");
}

void VipTextEditor::setStdColorSchemeForType(const QString& type, const VipTextHighlighter* sh)
{
	__schemes().stdSchemes[type] = sh;
	updateEditors(sh);
}

void VipTextEditor::setStdColorSchemeForType(const QString& type, const QString& name)
{
	const QList<const VipTextHighlighter*> sh = colorSchemes();
	for (int i = 0; i < sh.size(); ++i) {
		if (sh[i]->type == type && sh[i]->name == name) {
			setStdColorSchemeForType(type, sh[i]);
			break;
		}
	}
}

const VipTextHighlighter* VipTextEditor::stdColorSchemeForType(const QString& type)
{
	auto it = __schemes().stdSchemes.find(type);
	if (it == __schemes().stdSchemes.end())
		return nullptr;
	else
		return it.value();
}

const VipTextHighlighter* VipTextEditor::stdColorSchemeForExt(const QString& extension)
{
	QString type = typeForExtension(extension);
	if (type.isEmpty())
		return nullptr;
	return stdColorSchemeForType(type);
}

StringMap VipTextEditor::stdColorSchemes()
{
	QMap<QString, QString> res;
	for (auto it = __schemes().stdSchemes.begin(); it != __schemes().stdSchemes.end(); ++it)
		res[it.key()] = it.value()->name;
	return res;
}

void VipTextEditor::setStdColorSchemes(const StringMap& map)
{
	if (!map.isEmpty()) {
		__schemes().stdSchemes.clear();
		for (auto it = map.begin(); it != map.end(); ++it)
			if (const VipTextHighlighter* sh = colorScheme(it.key(), it.value())) {
				__schemes().stdSchemes[it.key()] = sh;
				updateEditors(sh);
			}
	}
}
