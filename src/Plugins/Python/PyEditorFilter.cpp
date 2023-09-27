#include <QTextCursor>
#include <QTextBlock>

#include "CodeEditor.h"
#include "PyEditorFilter.h"




static bool haveTwoPoint(const QString & str)
{
	for(int i = str.length()-1; i>=0 ; i--)
		if( str[i] != ' ' )
		{
			if( str[i] == ':')
				return true;
			else
				return false;
		}
	return false;
}

static int leftSpace(const QString & str)
{
	for( int i =0; i < str.length(); i++)
		if( str[i] != ' ' )
			return i;

	return str.length();
}
/*
static QTextCursor getCursor(QPlainTextEdit  * doc, int line)
{
	return QTextCursor( doc->document()->findBlockByLineNumber(line) );
}

static QString getLine(QPlainTextEdit  * doc, int line)
{
	QTextCursor c( getCursor(doc,line) );
	return c.block().text();
}
*/
static int getLineFromPos(QPlainTextEdit * doc, int pos)
{
	//return QTextCursor(doc->document()->findBlock(pos)).blockNumber();
	return doc->document()->findBlock(pos).blockNumber();
}
/*
static QTextBlock getBlock(QTextEdit * doc, const QPoint & pt)
{
	return doc->cursorForPosition(pt).block();
}

static int getLineFromPoint(QPlainTextEdit * doc, const QPoint & pt)
{
	//QRectF  r = doc->cursorForPosition(pt).block().layout()->boundingRect();
	return doc->cursorForPosition(pt).blockNumber();
}
*/





PyEditorFilter::PyEditorFilter(CodeEditor * parent)
:QObject(parent),m_editor(parent)
{
	m_editor->installEventFilter(this);
}

PyEditorFilter::~PyEditorFilter()
{
	m_editor->removeEventFilter(this);
}

void PyEditorFilter::Indent	(int fromline, int toline)
{
	for(int i=fromline; i<= toline; i++)
	{
		QTextCursor c( m_editor->document()->findBlockByLineNumber(i) );
		c.beginEditBlock();
		c.insertText(QString("    "));
		c.endEditBlock();
	}
}

void PyEditorFilter::Unindent	(int fromline, int toline)
{
	for(int i=fromline; i<= toline; i++)
	{
		QString s = QTextCursor( m_editor->document()->findBlockByLineNumber(i) ).block().text();

		int j;
		for(j=0; j<s.length(); j++)
			if( s[j] != ' ' )
				break;

		if( j > 4 ) j = 4;
		if( j <= s.length())
		{
			QTextCursor c(m_editor->document()->findBlockByLineNumber(i));
			c.beginEditBlock();
			for(int k=0; k<j; k++)
				c.deleteChar();
			c.endEditBlock();
		}
	}
}

void PyEditorFilter::Comment	(int fromline, int toline)
{
	for(int i=fromline; i<= toline; i++)
	{
		QTextCursor c(m_editor->document()->findBlockByLineNumber(i));
		c.beginEditBlock();
		c.insertText(QString("#"));
		c.endEditBlock();
	}
}

void PyEditorFilter::Uncomment	(int fromline, int toline)
{
	for(int i=fromline; i<= toline; i++)
	{
		QString s = QTextCursor( m_editor->document()->findBlockByLineNumber(i) ).block().text();

		int j;
		for(j=0; j<s.length(); j++)
			if( s[j] == '#' )
				break;

		if( j != s.length())
		{
			QTextCursor c(m_editor->document()->findBlockByLineNumber(i));
			c.setPosition( c.position() + j );
			c.beginEditBlock();
			c.deleteChar();
			c.endEditBlock();
		}
	}
}


void PyEditorFilter::IndentSelection		()
{
	QTextCursor cursor      = m_editor->textCursor();

	int to = getLineFromPos(m_editor,cursor.position());//cursor.blockNumber();
	int from = getLineFromPos(m_editor,cursor.anchor());

	if(to < from)
		std::swap(from,to);

	Indent(from,to);
	cursor.setPosition( m_editor->document()->findBlockByLineNumber ( from).position() );
	cursor.setPosition( m_editor->document()->findBlockByLineNumber ( to).position(),QTextCursor::KeepAnchor );
}

void PyEditorFilter::UnindentSelection	()
{
	QTextCursor cursor      = m_editor->textCursor();

	int to = getLineFromPos(m_editor,cursor.position());//cursor.blockNumber();
	int from = getLineFromPos(m_editor,cursor.anchor());

	if(to < from)
		std::swap(from,to);

	Unindent(from,to);
	cursor.setPosition( m_editor->document()->findBlockByLineNumber ( from).position() );
	cursor.setPosition( m_editor->document()->findBlockByLineNumber ( to).position(),QTextCursor::KeepAnchor );
}

void PyEditorFilter::CommentSelection		()
{
	QTextCursor cursor      = m_editor->textCursor();

	int to = getLineFromPos(m_editor,cursor.position());//cursor.blockNumber();
	int from = getLineFromPos(m_editor,cursor.anchor());

	if(to < from)
		std::swap(from,to);

	Comment(from,to);

}

void PyEditorFilter::UncommentSelection	()
{
	QTextCursor cursor      = m_editor->textCursor();

	int to = getLineFromPos(m_editor,cursor.position());//cursor.blockNumber();
	int from = getLineFromPos(m_editor,cursor.anchor());

	if(to < from)
		std::swap(from,to);

	Uncomment(from,to);

}

bool 	PyEditorFilter::eventFilter ( QObject * , QEvent * event )
{
	if(event->type() == QEvent::KeyPress)
	{
		QKeyEvent * key = static_cast<QKeyEvent*>(event);

		if( key->key() == Qt::Key_Tab )
		{
			IndentSelection();
			return true;
		}

		else if( key->key() == Qt::Key_Backtab )
		{
			UnindentSelection();
			return true;
		}

		else if( key->key() == Qt::Key_Enter || key->key() == Qt::Key_Return )
		{
			QString m_left;
			QTextCursor cursor      = m_editor->textCursor();
			QString cur_line_text   = cursor.block().text();

			//int pos_line = m_editor->document()->characterCount() - cursor.position();

			if( cur_line_text.indexOf(m_left) == 0 )
				cur_line_text.replace(0,m_left.length(),"");

			int space = leftSpace(cur_line_text);

			//QTextEdit::keyPressEvent(event);
			m_editor->insertPlainText("\n");

			bool tp = haveTwoPoint(cur_line_text);

			QString left = m_left + QString( space, ' ');
			if( tp ) left += QString( 4, ' ');

			cursor.beginEditBlock();
			cursor.insertText(left);
			cursor.endEditBlock();

			return true;
		}

		else if(key->key() == Qt::Key_Backspace)
		{
			QString m_left;

			QTextCursor cursor      = m_editor->textCursor();
			QString cur_line_text   = cursor.block().text();
			int space = leftSpace(cur_line_text);

			if( cursor.columnNumber() <= m_left.length() && m_left.length() != 0)
				return true;



			if( (space) % 4 == 0 && space > 0 && cursor.selectedText().length() == 0 && cursor.columnNumber() <= space)
			{
				cursor.beginEditBlock();
				cursor.deletePreviousChar();
				cursor.deletePreviousChar();
				cursor.deletePreviousChar();
				cursor.deletePreviousChar();
				cursor.endEditBlock();

			}
			else
				cursor.deletePreviousChar();

			//QTextEdit::keyPressEvent(event);

			return true;

		}

		else if( key->key() == Qt::Key_S && (key->modifiers() & Qt::ControlModifier))
		{
			Q_EMIT SaveTriggered();
			return true;
		}
		else if (key->key() == Qt::Key_F && (key->modifiers() & Qt::ControlModifier))
		{
			Q_EMIT SearchTriggered();
			return true;
		}
	}

	return false;
}


