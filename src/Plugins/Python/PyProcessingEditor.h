#ifndef PY_PROCESSING_EDITOR_H
#define PY_PROCESSING_EDITOR_H

#include "PyProcessing.h"
#include "PySignalFusionProcessing.h"
#include "VipStandardProcessing.h"
#include "qwidget.h"

/**
Edit a small 2D array
*/
class PyArrayEditor : public QWidget
{
	Q_OBJECT
public:
	PyArrayEditor();
	~PyArrayEditor();

	VipNDArray array() const;
	void setText(const QString& text);
	void setArray(const VipNDArray& ar);

private Q_SLOTS:
	void textEntered();
	void finished();
Q_SIGNALS:
	void changed();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/**
Edit a data which either a 2D array manually edited (through PyArrayEditor),
or a data comming from another player (through VipOtherPlayerDataEditor)
*/
class PyDataEditor : public QWidget
{
	Q_OBJECT
public:
	PyDataEditor();
	~PyDataEditor();

	VipOtherPlayerData value() const;
	void setValue(const VipOtherPlayerData& data);
	void displayVLines(bool before, bool after);

private Q_SLOTS:
	void emitChanged() { Q_EMIT changed(); }
Q_SIGNALS:
	void changed();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/**
Editor for PyProcessing based on a Python processing class inheriting 'ThermavipPyProcessing'
*/
class PyParametersEditor : public QWidget
{
	Q_OBJECT
public:
	PyParametersEditor(PyProcessing*);
	~PyParametersEditor();

private Q_SLOTS:
	void updateProcessing();

Q_SIGNALS:
	void changed();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class PyApplyToolBar;

/**
Global editor for the PyProcessing class.
*/
class PyProcessingEditor : public QWidget
{
	Q_OBJECT
public:
	PyProcessingEditor();
	~PyProcessingEditor();

	PyApplyToolBar* buttons() const;
	void setPyProcessing(PyProcessing*);

private Q_SLOTS:
	void updatePyProcessing();
	void applyRequested();
	void uninitRequested();
	void registerProcessing();
	void manageProcessing();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief A simple horizontal widget used by the different Python processing editors.
/// This tool bar is used to apply the processing, save the processing to the list of
/// custom processings (using PyRegisterProcessing), and manage registered custom
/// processings.
///
class PyApplyToolBar : public QWidget
{
	Q_OBJECT

public:
	PyApplyToolBar(QWidget* parent = nullptr);
	~PyApplyToolBar();

	QPushButton* applyButton() const;
	QToolButton* registerButton() const;
	QToolButton* manageButton() const;

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Manager widget for custom Python processings.
///
class PySignalFusionProcessingManager : public QWidget
{
	Q_OBJECT

public:
	PySignalFusionProcessingManager(QWidget* parent = nullptr);
	~PySignalFusionProcessingManager();

	QString name() const;
	QString category() const;
	QString description() const;

	void setName(const QString&);
	void setCategory(const QString&);
	void setDescription(const QString&);

	void setManagerVisible(bool);
	bool managerVisible() const;

	void setCreateNewVisible(bool);
	bool createNewVisible() const;

public Q_SLOTS:
	void updateWidget();
	bool applyChanges();
private Q_SLOTS:
	void removeSelection();
	void itemClicked(QTreeWidgetItem*, int);
	void itemDoubleClicked(QTreeWidgetItem*, int);
	void descriptionChanged();
	void selectionChanged();
	void showMenu(const QPoint&);

protected:
	virtual void keyPressEvent(QKeyEvent*);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipPlotPlayer;
class PyApplyToolBar;
class QPushButton;
class QToolButton;

/**
Editor for PySignalFusionProcessing
*/
class PySignalFusionProcessingEditor : public QWidget
{
	Q_OBJECT

public:
	PySignalFusionProcessingEditor(QWidget* parent = nullptr);
	~PySignalFusionProcessingEditor();

	void setPlotPlayer(VipPlotPlayer* player);
	VipPlotPlayer* plotPlayer() const;

	void setPySignalFusionProcessing(PySignalFusionProcessing* proc);
	PySignalFusionProcessing* getPySignalFusionProcessing() const;

	PyApplyToolBar* buttons() const;

public Q_SLOTS:
	bool updateProcessing();
	void updateWidget();
	bool apply();
	void nameTriggered(QAction* a);
	void registerProcessing();
	void manageProcessing();
	void showError(const QPoint& pos, const QString& error);
	void showErrorDelayed(const QPoint& pos, const QString& error);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Open the processing manager widget
void openProcessingManager();

#endif