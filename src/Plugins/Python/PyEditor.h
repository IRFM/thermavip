#ifndef PY_EDITOR_H
#define PY_EDITOR_H

#include "CodeEditor.h"

#include <QTabWidget>
#include <QWidgetAction>
#include <QToolBar>

class QLineEdit;
class PYTHON_EXPORT PySearch : public QToolBar
{
	Q_OBJECT

public:
	PySearch(QWidget * parent = NULL);
	~PySearch();

	bool exactMatch() const;
	bool caseSensitive() const;
	bool regExp() const;

	void setEditor(QTextEdit  *);
	void setEditor(QPlainTextEdit  *);
	void setDocument(QTextDocument *);
	QTextDocument  *document() const;
	QLineEdit * search() const;
	QPair<int, int> lastFound(int * line) const;

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
	virtual void showEvent(QShowEvent *);
	virtual void hideEvent(QHideEvent *);

private:
	void removePreviousFormat();
	void format(const QTextBlock & b, int start, int end);
	class PrivateData;
	PrivateData * m_data;
};

class PYTHON_EXPORT PyToolBar : public QToolBar
{

public:

	PyToolBar( QWidget * parent = NULL );

	QWidgetAction *open;
	QWidgetAction *save;
	QWidgetAction *saveAs;
	QWidgetAction *saveAll;
	QWidgetAction *newfile;

	QWidgetAction *comment;
	QWidgetAction *uncomment;
	QWidgetAction *indent;
	QWidgetAction *unindent;
};


class TabWidget : public QTabWidget
{
public:

	TabWidget(QWidget * parent = NULL) : QTabWidget(parent) {
		setStyleSheet("QTabWidget::pane { border: 0px; } QTabWidget{padding:0px; margin: 0px; }");
	}

	QTabBar * TabBar() { return this->tabBar(); }
};


class PYTHON_EXPORT PyEditor : public QWidget
{
	Q_OBJECT

	TabWidget m_tab;
	PyToolBar m_bar;
	PySearch *m_search;
	QAction * m_searchAction;
	QString m_default_code;
	bool m_unique;
	QMap<int, int> m_ids;

	bool Save(CodeEditor* editor);
	int CreateEditor(const QString & filename = QString());

public:

	PyEditor(Qt::Orientation tool_bar_orientation = Qt::Horizontal, QWidget * parent = NULL);

	TabWidget * GetTabWidget() {return &m_tab;}
	PyToolBar * TabBar() const;
	CodeEditor * CurrentEditor()const;
	CodeEditor * Editor(int pos)const;
	int EditorCount() const;
	int CurrentIndex() const;

	void setUniqueFile(bool unique);
	bool uniqueFile() const;

	QString filename(CodeEditor * ed) const;
	QString canonicalFilename(CodeEditor * ed) const;

public slots:

	void SetDefaultCode(const QString & code);
	void SetCurrentIndex(int index);

	CodeEditor * Load();
	CodeEditor * OpenFile(const QString & filename);
	CodeEditor * NewFile();

	void Save();
	void SaveAs();
	void SaveAll();

	void Comment();
	void Uncomment();
	void Indent();
	void Unindent();

	void CloseSearch();
	void ShowSearch();
	void ShowSearchAndFocus();
	void SetSearchVisible(bool);

	QByteArray SaveState() const;
	void RestoreState(const QByteArray & state) ;
	
private Q_SLOTS:

	void AboutToClose(int);
	void SetHeaderBarVisibility();
	void ModificationChanged (bool modify);
	void CurrentChanged(int);
	//void ReceivedSaved(const QString &);

Q_SIGNALS:
	void modified(bool);

private:
	int nextId() const;
	
};


#endif
