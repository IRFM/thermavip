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

#include "VipTextHighlighter.h"
#include "VipTextEditor.h"

class VipPyBaseHighlighter::PrivateData
{
public:
	struct HighlightingRule
	{
		QRegExp pattern;
		QTextCharFormat format;
	};
	QVector<HighlightingRule> highlightingRules;

	QRegExp commentStartExpression;
	QRegExp commentEndExpression;
	QRegExp real;

	int current_str_number;
	char token[4];
};

VipPyBaseHighlighter::VipPyBaseHighlighter(const QString& name, const QString& type, const QStringList& extensions, QTextDocument* parent)
  : VipTextHighlighter(name, type, extensions, parent)
{
	VIP_CREATE_PRIVATE_DATA();

	using HighlightingRule = PrivateData::HighlightingRule;

	HighlightingRule rule;

	keywordFormat.setForeground(Qt::darkBlue);
	keywordFormat.setFontWeight(QFont::Bold);
	predefineFormat.setFontWeight(QFont::Bold);
	predefineFormat.setForeground(Qt::darkMagenta);
	quotationFormat.setForeground(Qt::darkGreen);
	singleLineCommentFormat.setForeground(Qt::red);
	functionFormat.setFontItalic(true);
	functionFormat.setForeground(Qt::blue);
	d_data->real = QRegExp("((\\+|-)?([0-9]+)(\\.[0-9]+)?)|((\\+|-)?\\.?[0-9]+)");
	updateRules();
}

VipPyBaseHighlighter::~VipPyBaseHighlighter() {}

VipEditorFilter* VipPyBaseHighlighter::createFilter(VipTextEditor* editor) const
{
	return new VipPyEditorFilter(editor);
}

void VipPyBaseHighlighter::updateRules()
{
	using HighlightingRule = PrivateData::HighlightingRule;
	HighlightingRule rule;

	QStringList keywordPatterns;
	keywordPatterns << "\\band\\b"
			<< "\\bassert\\b"
			<< "\\bbreak\\b"
			<< "\\bclass\\b"
			<< "\\bcontinue\\b"
			<< "\\bdef\\b"
			<< "\\belif\\b"
			<< "\\belse\\b"
			<< "\\bexcept\\b"
			<< "\\bexec\\b"
			<< "\\bfinally\\b"
			<< "\\bfor\\b"
			<< "\\bfrom\\b"
			<< "\\bglobal\\b"
			<< "\\bif\\b"
			<< "\\bimport\\b"
			<< "\\bin\\b"
			<< "\\bis\\b"
			<< "\\blambda\\b"
			<< "\\bnot\\b"
			<< "\\bor\\b"
			<< "\\bpass\\b"
			<< "\\braise\\b"
			<< "\\breturn\\b"
			<< "\\btry\\b"
			<< "\\bwhile\\b"
			<< "\\byield\\b"
			<< "\\bas\\b"
			<< "\\bself\\b";

	d_data->highlightingRules.clear();
	foreach (const QString& pattern, keywordPatterns) {
		rule.pattern = QRegExp(pattern);
		rule.format = keywordFormat;
		d_data->highlightingRules.append(rule);
	}

	QStringList predefinePatterns;
	predefinePatterns << "\\bstr\\b"
			  << "\\blen\\b"
			  << "\\bmax\\b"
			  << "\\bmin\\b"
			  << "\\bint\\b"
			  << "\\blong\\b"
			  << "\\bfloat\\b"
			  << "\\bbool\\b"
			  << "\\bstr\\b"
			  << "\\bhelp\\b"
			  << "\\bdir\\b"
			  << "\\bcallable\\b"
			  << "\\blist\\b"
			  << "\\btuple\\b"
			  << "\\bNameError\\b"
			  << "\\bBytesWarning\\b"
			  << "\\bdict\\b"
			  << "\\binput\\b"
			  << "\\boct\\b"
			  << "\\bbin\\b"
			  << "\\bSystemExit\\b"
			  << "\\bStandardError\\b"
			  << "\\bformat\\b"
			  << "\\brepr\\b"
			  << "\\bsorted\\b"
			  << "\\bFalse\\b"
			  << "\\bset\\b"
			  << "\\bbytes\\b"
			  << "\\breduce\\b"
			  << "\\bintern\\b"
			  << "\\bissubclass\\b"
			  << "\\bEllipsis\\b"
			  << "\\bEOFError\\b"
			  << "\\blocals\\b"
			  << "\\bBufferError\\b"
			  << "\\bWarning\\b"
			  << "\\b__package__\\b"
			  << "\\bround\\b"
			  << "\\bRuntimeWarning\\b"
			  << "\\biter\\b"
			  << "\\bcmp\\b"
			  << "\\bslice\\b"
			  << "\\bFloatingPointError\\b"
			  << "\\bsum\\b"
			  << "\\bgetattr\\b"
			  << "\\babs\\b"
			  << "\\bexit\\b"
			  << "\\bTrue\\b"
			  << "\\bFutureWarning\\b"
			  << "\\bImportWarning\\b"
			  << "\\bNone\\b"
			  << "\\bhash\\b"
			  << "\\bReferenceError\\b"
			  << "\\bcredits\\b"
			  << "\\bdel\\b"
			  << "\\bglobals\\b"
			  << "\\brange\\b"
			  << "\\bprint\\b"
			  << "\\bobject\\b";

	foreach (const QString& pattern, predefinePatterns) {
		rule.pattern = QRegExp(pattern);
		rule.format = predefineFormat;
		d_data->highlightingRules.append(rule);
	}
}

QByteArray VipPyBaseHighlighter::removeStringsAndComments(const QString& code, int start, int& inside_string_start, QList<QPair<int, int>>& strings, QList<QPair<int, int>>& comments)
{
	QByteArray ar = code.toLatin1();
	QByteArray copy = ar;
	inside_string_start = -1;

	for (int i = start; i < ar.size(); ++i) {
		if (ar[i] == '"' || ar[i] == (char)39) {
			// find full comment
			char token[4] = { ar[i], 0, 0, 0 };
			// check for triple quotes
			if (ar[i] == '"' && i < ar.size() - 2 && ar[i + 1] == '"' && ar[i + 2] == '"')
				token[1] = token[2] = token[0] = '"';
			int end = ar.indexOf(token, i + 1);
			if (end < 0) {
				inside_string_start = i;
				memcpy(d_data->token, token, 4);
				for (int j = i + (int)strlen(token); j < ar.size(); ++j)
					copy[j] = ' ';
				return copy; // unbalanced string
			}
			end += (int)strlen(token);
			if (end > ar.size())
				end = ar.size();
			// replace string content by spaces
			for (int j = i + (int)strlen(token); j < end - (int)strlen(token); ++j)
				copy[j] = ' ';
			strings.push_back(QPair<int, int>(i, end));
			i = end;
		}
		else if (ar[i] == '#') {
			// find full comment (until EOL)
			int end = ar.indexOf('\n');
			if (end < 0)
				end = ar.size();
			// replace string content by spaces
			for (int j = i; j < end; ++j) {
				copy[j] = ' ';
			}
			comments.push_back(QPair<int, int>(i, end));
			i = end - 1;
		}
	}
	return copy;
}

void VipPyBaseHighlighter::highlightBlock(const QString& _text)
{
	// if( ! m_enable )
	//	return;
	if (_text.isEmpty()) {
		// empy text, just check for inside string
		int st = previousBlockState() - 1;
		int cur = currentBlockState();
		int _new = st >= 0 ? 1 : 0;
		setCurrentBlockState(_new);
		if (_new != cur && currentBlock().blockNumber() != document()->blockCount()) {
			// we change the state inside the document, rehighlight all
			rehighlightDelayed();
		}
		return;
	}

	QString text = _text;

	// finish previous string
	int st = previousBlockState() - 1;
	int current = currentBlockState();
	// int bcount = currentBlock().blockNumber();
	int start_check_string = 0;
	if (st >= 0) {
		// we are inside a multi line comment
		// find string token
		int index = _text.indexOf(d_data->token);
		if (index >= 0) {
			// end of multi line comment
			setFormat(0, index + (int)strlen(d_data->token), quotationFormat);
			for (int i = 0; i < index; ++i)
				text[i] = QChar(' '); // remove comment part
			setCurrentBlockState(0);
			if (current != 0 && currentBlock().blockNumber() != document()->blockCount()) {
				// we change the state inside the document, rehighlight all
				rehighlightDelayed();
			}
			start_check_string = index + (int)strlen(d_data->token);
		}
		else {
			// still inside string
			setFormat(0, _text.size(), quotationFormat);
			setCurrentBlockState(1);
			if (current != 1 && currentBlock().blockNumber() != document()->blockCount()) {
				// we change the state inside the document, rehighlight all
				rehighlightDelayed();
			}
			return;
		}
	}
	else {
		setCurrentBlockState(0);
		if (current != 0 && currentBlock().blockNumber() != document()->blockCount()) {
			// we change the state inside the document, rehighlight all
			rehighlightDelayed();
		}
	}

	// str and comment
	QList<QPair<int, int>> strings, comments;
	int inside_string_start = -1;
	text = removeStringsAndComments(text, start_check_string, inside_string_start, strings, comments);
	if (inside_string_start >= 0) {
		// unfinished string, probably multi-line one
		setCurrentBlockState(1);
		setFormat(inside_string_start, text.size() - inside_string_start, quotationFormat);
		// replace string
		for (int i = inside_string_start + 1; i < text.size(); ++i)
			text[i] = QChar(' ');

		if (current != 1 && currentBlock().blockNumber() != document()->blockCount()) {
			// we change the state inside the document, rehighlight all
			rehighlightDelayed();
		}
	}
	else {
		setCurrentBlockState(0);
		if (current != 0 && currentBlock().blockNumber() != document()->blockCount()) {
			// we change the state inside the document, rehighlight all
			rehighlightDelayed();
		}
	}

	for (int i = 0; i < strings.size(); ++i) {
		setFormat(strings[i].first, strings[i].second - strings[i].first, quotationFormat);
	}
	for (int i = 0; i < comments.size(); ++i) {
		setFormat(comments[i].first, comments[i].second - comments[i].first, singleLineCommentFormat);
	}

	// function
	QString par("(");
	QRegExp def("\\bdef\\b");
	int idef = def.indexIn(text);
	if (idef != -1) {
		int to = text.indexOf(par, idef + 3);
		if (to >= 0) {
			setFormat(idef + 3, to, functionFormat);
			setFormat(to, text.length(), QTextCharFormat());
		}
		else
			setFormat(idef + 3, text.length(), functionFormat);
	}

	// class
	QString tp(":");
	QRegExp cl("\\bclass\\b");
	int iclass = cl.indexIn(text);
	if (iclass != -1) {
		int to = text.indexOf(par, iclass + 5);
		if (to < 0)
			to = text.indexOf(tp, iclass + 5);
		if (to >= 0) {
			setFormat(iclass + 5, to, functionFormat);
			setFormat(to, text.length(), QTextCharFormat());
		}
		else
			setFormat(iclass + 5, text.length(), functionFormat);
	}

	// str and comment

	/*const QString simple_str( "'" );
	const QString double_str ( 1,34 );
	const QString comment ( "#" );

	int index_comment = text.indexOf(comment);

	bool in_str = false;
	int index = text.indexOf(double_str);
	QString current = double_str;
	if( (text.indexOf(simple_str) < index && text.indexOf(simple_str) >= 0) || index == -1)
	{
		index = text.indexOf(simple_str);
		current = simple_str;
	}

	int from = 0;
	//int from_no_str = 0;

	if( index_comment != -1 && ( index_comment < index || index == -1) )
		setFormat(index_comment, text.length(), singleLineCommentFormat);
	else
		while (index >= 0) {

			if( !in_str)
			{

				from = index;
				in_str = true;
			}
			else
			{
				setFormat(from, index, quotationFormat);
				setFormat(index+1, text.length(),QTextCharFormat());
				in_str = false;
				//from_no_str = index;
			}

			index_comment = text.indexOf(comment,index+1);

			if( in_str )
				index = text.indexOf(current,index+1);
			else
			{
				int si = text.indexOf(simple_str,index+1);
				int di = text.indexOf(double_str,index+1);
				if( si < di && si >= 0 )
				{
					index = si;
					current = simple_str;
				}
				else
				{
					index = di;
					current = double_str;
				}
			}


			if( index_comment != -1 && index_comment < index && in_str == false )
			{
				setFormat(index_comment, text.length(), singleLineCommentFormat);
				break;
			}

			//index_comment = -1;
		}

	if(in_str)
		setFormat(from, text.length(), quotationFormat);
	else if( index_comment != -1 )
		setFormat(index_comment, text.length(), singleLineCommentFormat);

	//VipFunction and class doc using """
	const QString doc(3,34 );
	int index_doc1;
	int index_doc2;
	index_doc1 = text.indexOf(doc);
	index_doc2 = text.indexOf(doc,index_doc1);
	if(index_doc1 >= 0)
	{
		if( index_doc2 >=0 )
			setFormat(index_doc1, index_doc2+3, quotationFormat);
		else
			setFormat(index_doc1, text.length(), quotationFormat);
	}
	*/
	// keywords
	for (const PrivateData::HighlightingRule& rule : d_data->highlightingRules) {
		QRegExp expression(rule.pattern);
		int index = expression.indexIn(text);
		while (index >= 0) {
			int length = expression.matchedLength();
			setFormat(index, length, rule.format);
			index = expression.indexIn(text, index + length);
		}
	}

	// numbers
	int index = d_data->real.indexIn(text);
	while (index >= 0) {
		int length = d_data->real.matchedLength();
		setFormat(index, length, numberFormat);
		index = d_data->real.indexIn(text, index + length);
	}
}

void VipPyDevScheme::updateEditor(VipTextEditor* editor) const
{
	editor->setLineAreaBackground(QColor(0xEFEFEF));
	editor->setLineAreaBorder(Qt::transparent);
	editor->setCurrentLineColor(QColor(0xEFF8FE));
	editor->setLineNumberColor(QColor(0x808080));
	editor->setBackgroundColor(Qt::white);
	editor->setBorderColor(QColor(0xEFEFEF));
	editor->setTextColor(Qt::black);
}

void VipPyDarkScheme::updateEditor(VipTextEditor* editor) const
{
	editor->setLineAreaBackground(QColor(0x282828));
	editor->setLineAreaBorder(Qt::transparent);
	editor->setCurrentLineColor(QColor(0x31314E));
	editor->setLineNumberColor(QColor(0x808080));
	editor->setBackgroundColor(QColor(0x272822));
	editor->setBorderColor(QColor(0x31314E));
	editor->setTextColor(Qt::white);
}

void VipSpyderDarkScheme::updateEditor(VipTextEditor* editor) const
{
	editor->setLineAreaBackground(QColor(0x35342D));
	editor->setLineAreaBorder(Qt::transparent);
	editor->setCurrentLineColor(QColor(0x49483E));
	editor->setLineNumberColor(QColor(0x808080));
	editor->setBackgroundColor(QColor(0x272822));
	editor->setBorderColor(QColor(0x31314E));
	editor->setTextColor(Qt::white);
}

void VipPyZenburnScheme::updateEditor(VipTextEditor* editor) const
{
	editor->setLineAreaBackground(QColor(0x3F3F3F));
	editor->setLineAreaBorder(Qt::transparent);
	editor->setCurrentLineColor(QColor(0x2C2C2C));
	editor->setLineNumberColor(QColor(0x808080));
	editor->setBackgroundColor(QColor(0x3F3F3F));
	editor->setBorderColor(QColor(0x808080));
	editor->setTextColor(Qt::white);
}

#include "VipDisplayArea.h"
#include "VipGui.h"

static bool isDarkColor(const QColor& c)
{
	return c.lightness() < 128;
}

void VipTextScheme::updateEditor(VipTextEditor* editor) const
{
	QColor c = vipWidgetTextBrush(vipGetMainWindow()).color();
	if (isDarkColor(c)) {
		// Dark text color: use a light theme
		VipPyDevScheme s;
		s.updateEditor(editor);
	}
	else {
		// Use a dark theme
		VipPyZenburnScheme s;
		s.updateEditor(editor);
	}
}

static int registerFormats()
{
	qRegisterMetaType<StringMap>();
	qRegisterMetaTypeStreamOperators<StringMap>();

	VipTextEditor::registerColorScheme(new VipPyDevScheme());
	VipTextEditor::registerColorScheme(new VipPyDarkScheme());
	VipTextEditor::registerColorScheme(new VipPyZenburnScheme());
	VipTextEditor::registerColorScheme(new VipSpyderDarkScheme());
	VipTextEditor::registerColorScheme(new VipTextScheme());
	VipTextEditor::setStdColorSchemeForType("Python", VipTextEditor::colorScheme("Python", "Pydev"));
	VipTextEditor::setStdColorSchemeForType("Text", VipTextEditor::colorScheme("Text", "Text"));

	return 0;
}
static int _registerFormats = registerFormats();
