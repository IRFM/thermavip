#ifndef PY_EDITOR_H
#define PY_EDITOR_H


#include "VipTabEditor.h"
#include "VipPyOperation.h"
#include "VipPyShellWidget.h"

#include "VipToolWidget.h"

/// @brief A VipTabEditor dedicated to Python file edition
/// and execution.
/// 
class VIP_GUI_EXPORT VipPyEditor : public VipTabEditor
{
	Q_OBJECT

public:

	VipPyEditor(QWidget * parent = nullptr);
	~VipPyEditor();

	/// @brief Returns the shell that runs the script, either a VipPyShellWidget or a VipIPythonShellWidget.
	/// Note that the shell widget is null until launching a Python file. 
	QWidget * shellWidget() const;

	/// @brief Returns the object running the code, either a VipPyIOOperation or a VipIPythonShellProcess
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




/// @brief Global VipPyEditor tool widget class
class VIP_GUI_EXPORT VipPyEditorToolWidget : public VipToolWidget
{
	Q_OBJECT
public:

	VipPyEditorToolWidget(VipMainWindow * parent = nullptr)
		:VipToolWidget(parent)
	{
		VipPyEditor* editor = new VipPyEditor();
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

	VipPyEditor * editor() const {
		return static_cast<VipPyEditor*>(widget());
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

VIP_REGISTER_QOBJECT_METATYPE(VipPyEditorToolWidget*)

VIP_GUI_EXPORT VipPyEditorToolWidget * vipGetPyEditorToolWidget();

#endif