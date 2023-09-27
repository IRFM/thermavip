#pragma once


#include "PyEditor.h"
#include "PyOperation.h"
#include "IOOperationWidget.h"

#include "VipToolWidget.h"

class PYTHON_EXPORT CodeEditorWidget : public QWidget
{
	Q_OBJECT

public:

	CodeEditorWidget(QWidget * parent = NULL);
	~CodeEditorWidget();

	PyEditor * editor() const;

	QWidget * shellWidget() const;
	QObject * interpreter() const;

	bool isFileRunning() const;
	bool isDebugging() const;

public Q_SLOTS:
	void execFile(bool show_progress);
	void debugFile();
	void stopFile(bool wait);
	void stopFile() { stopFile(true); }
	void nextStep();
	void stepIn();
	void stepOut();
	void pause();
	void _continue();
	void startInteractiveInterpreter();
	void execInInternal();
	void execInIPython();

private Q_SLOTS:
	void aboutToDisplayLaunchMode();
	void check();

Q_SIGNALS:
	void fileFinished();

protected:
	virtual void keyPressEvent(QKeyEvent * evt);
private:
	bool isRunning();
	class PrivateData;
	PrivateData * m_data;
};





class PYTHON_EXPORT CodeEditorToolWidget : public VipToolWidget
{
	Q_OBJECT
public:

	CodeEditorToolWidget(VipMainWindow * parent = NULL)
		:VipToolWidget(parent)
	{
		this->setWidget(new CodeEditorWidget());
		this->setWindowTitle("Python code editor");
		this->setObjectName("Python code editor");
		this->setKeepFloatingUserSize(true);
		this->connect(editor()->editor()->GetTabWidget(), SIGNAL(currentChanged(int)), this, SLOT(currentFileChanged()));
		this->connect(editor()->editor(), SIGNAL(modified(bool)), this, SLOT(currentFileChanged()));
		this->resize(500, 700);
		this->currentFileChanged();
	} 

	CodeEditorWidget * editor() const {
		return static_cast<CodeEditorWidget*>(widget());
	}

public Q_SLOTS:
	void currentFileChanged()
	{
		QString title = "Python code editor";
		if (CodeEditor * ed = editor()->editor()->CurrentEditor())
			title += " - " + QString(ed->document()->isModified() ? "*" : "") +editor()->editor()->filename(ed) ;
		setWindowTitle(title);
	}
};

VIP_REGISTER_QOBJECT_METATYPE(CodeEditorToolWidget*)

PYTHON_EXPORT CodeEditorToolWidget * GetCodeEditorToolWidget();
