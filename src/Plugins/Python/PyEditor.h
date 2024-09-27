#ifndef PY_EDITOR_H
#define PY_EDITOR_H


#include "VipTabEditor.h"
#include "PyOperation.h"
#include "IOOperationWidget.h"

#include "VipToolWidget.h"

/// @brief A VipTabEditor dedicated to Python file edition
/// and execution.
/// 
class PYTHON_EXPORT PyEditor : public VipTabEditor
{
	Q_OBJECT

public:

	PyEditor(QWidget * parent = nullptr);
	~PyEditor();

	/// @brief Returns the shell that runs the script, either a IOOperationWidget or a IPythonWidget.
	/// Note that the shell widget is null until launching a Python file. 
	QWidget * shellWidget() const;

	/// @brief Returns the object running the code, either a VipPyIOOperation or a IPythonConsoleProcess
	/// Note that the object is null until launching a Python file. 
	QObject * interpreter() const;

	/// @brief Returns true if a file is currently running.
	bool isFileRunning() const;
	/// @brief Returns true if a file is currently running in debug mode.
	bool isDebugging() const;

public Q_SLOTS:
	void execFile();
	void debugFile();
	void stopFile(bool wait);
	void stopFile() { stopFile(true); }
	void nextStep();
	void stepIn();
	void stepOut();
	void pause();
	void _continue();
	/// @brief Start or restart the interpreter if it is a VipPyIOOperation
	void startInteractiveInterpreter();
	/// @brief Next file executions will be performed in the internal Python shell
	void execInInternal();
	/// @brief Next file executions will be performed in the external IPython shell if available
	void execInIPython();

Q_SIGNALS:
	/// @brief Emitted when a file execution finished
	void fileFinished();

private Q_SLOTS:
	void aboutToDisplayLaunchMode();
	void check();

protected:
	virtual void keyPressEvent(QKeyEvent * evt);
private:
	bool isRunning();
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};




/// @brief Global PyEditor tool widget class
class PYTHON_EXPORT PyEditorToolWidget : public VipToolWidget
{
	Q_OBJECT
public:

	PyEditorToolWidget(VipMainWindow * parent = nullptr)
		:VipToolWidget(parent)
	{
		PyEditor* editor = new PyEditor();
		editor->setDefaultSaveDirectory(vipGetPythonScriptsDirectory());
		this->setWidget(editor);
		this->setWindowTitle("Python code editor");
		this->setObjectName("Python code editor");
		this->setKeepFloatingUserSize(true);
		this->connect(editor->tabWidget(), SIGNAL(currentChanged(int)), this, SLOT(currentFileChanged()));
		this->connect(editor, SIGNAL(modified(bool)), this, SLOT(currentFileChanged()));
		this->resize(500, 700);
		this->currentFileChanged();
	} 

	PyEditor * editor() const {
		return static_cast<PyEditor*>(widget());
	}

public Q_SLOTS:
	void currentFileChanged()
	{
		QString title = "Python code editor";
		if (VipTextEditor * ed = editor()->currentEditor())
			title += " - " + QString(ed->document()->isModified() ? "*" : "") +editor()->filename(ed) ;
		setWindowTitle(title);
	}
};

VIP_REGISTER_QOBJECT_METATYPE(PyEditorToolWidget*)

PYTHON_EXPORT PyEditorToolWidget * vipGetPyEditorToolWidget();

#endif