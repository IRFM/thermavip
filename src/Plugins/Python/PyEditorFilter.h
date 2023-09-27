#ifndef PY_EDITOR_FILTER_H
#define PY_EDITOR_FILTER_H


#include <QObject>
#include <QKeyEvent>

#include "PyConfig.h"

class CodeEditor;

class PYTHON_EXPORT PyEditorFilter: public QObject
{
	Q_OBJECT

	CodeEditor* m_editor;

public:

	PyEditorFilter(CodeEditor * parent);
	~PyEditorFilter();

	virtual bool 	eventFilter ( QObject * watched, QEvent * event );

	void Indent	(int fromline, int toline);
	void Unindent	(int fromline, int toline);
	void Comment	(int fromline, int toline);
	void Uncomment	(int fromline, int toline);

	void IndentSelection	();
	void UnindentSelection	();
	void CommentSelection	();
	void UncommentSelection	();

Q_SIGNALS:

	void SaveTriggered();
	void SearchTriggered();

};

#endif
