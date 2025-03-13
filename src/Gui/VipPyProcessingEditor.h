#ifndef PY_PROCESSING_EDITOR_H
#define PY_PROCESSING_EDITOR_H

#include "VipPyProcessing.h"
#include "VipPySignalFusionProcessing.h"
#include "VipStandardProcessing.h"
#include "VipPlayer.h"
#include "VipPyFitProcessing.h"
#include "VipPyGenerator.h"

#include <qdialog.h>

class QPushButton;
class QToolButton;
class QTreeWidgetItem;


/**
Editor for VipPyProcessing based on a Python processing class inheriting 'ThermavipPyProcessing'
*/
class VipPyParametersEditor : public QWidget
{
	Q_OBJECT
public:
	VipPyParametersEditor(VipPyProcessing*);
	~VipPyParametersEditor();

private Q_SLOTS:
	void updateProcessing();

Q_SIGNALS:
	void changed();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipPyApplyToolBar;

/**
Global editor for the VipPyProcessing class.
*/
class VipPyProcessingEditor : public QWidget
{
	Q_OBJECT
public:
	VipPyProcessingEditor();
	~VipPyProcessingEditor();

	VipPyApplyToolBar* buttons() const;
	void setPyProcessing(VipPyProcessing*);

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
/// custom processings (using VipPyRegisterProcessing), and manage registered custom
/// processings.
///
class VipPyApplyToolBar : public QWidget
{
	Q_OBJECT

public:
	VipPyApplyToolBar(QWidget* parent = nullptr);
	~VipPyApplyToolBar();

	QPushButton* applyButton() const;
	QToolButton* registerButton() const;
	QToolButton* manageButton() const;

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Manager widget for custom Python processings.
///
class VipPySignalFusionProcessingManager : public QWidget
{
	Q_OBJECT

public:
	VipPySignalFusionProcessingManager(QWidget* parent = nullptr);
	~VipPySignalFusionProcessingManager();

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
class VipPyApplyToolBar;
class QPushButton;
class QToolButton;

/**
Editor for VipPySignalFusionProcessing
*/
class VipPySignalFusionProcessingEditor : public QWidget
{
	Q_OBJECT

public:
	VipPySignalFusionProcessingEditor(QWidget* parent = nullptr);
	~VipPySignalFusionProcessingEditor();

	void setPlotPlayer(VipPlotPlayer* player);
	VipPlotPlayer* plotPlayer() const;

	void setPySignalFusionProcessing(VipPySignalFusionProcessing* proc);
	VipPySignalFusionProcessing* getPySignalFusionProcessing() const;

	VipPyApplyToolBar* buttons() const;

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
VIP_GUI_EXPORT void vipOpenProcessingManager();



class VIP_GUI_EXPORT VipPySignalGeneratorEditor : public QWidget
{
	Q_OBJECT

public:
	VipPySignalGeneratorEditor(QWidget* parent = nullptr);
	~VipPySignalGeneratorEditor();

	void setGenerator(VipPySignalGenerator* gen);
	VipPySignalGenerator* generator() const;

	static VipPySignalGenerator* createGenerator();

private Q_SLOTS:
	void updateGenerator();
	void updateWidget();
	void updateVisibility();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};



class VipPlotCurve;
class VipPyFitProcessing;

class VIP_GUI_EXPORT VipFitDialogBox : public QDialog
{
	Q_OBJECT

public:
	// fit could be empty or "Linear", "Exponential", "Polynomial", "Gaussian".
	VipFitDialogBox(VipPlotPlayer* pl, const QString& fit, QWidget* parent = nullptr);
	~VipFitDialogBox();

	VipPlotCurve* selectedCurve() const;
	int selectedFit() const;

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};


namespace detail
{
	/// @brief VipFitManage used to automatically change the fit time unit
	/// and update the fit when moving the time window
	class AttachFitToPlayer : public VipFitManage
	{
		Q_OBJECT
		QPointer<VipPlotPlayer> m_player;
	public:
		AttachFitToPlayer(VipPyFitProcessing* fit, VipPlotPlayer* player);
		VipPlotPlayer* player() const;
		virtual VipInterval xBounds() const;
	private Q_SLOTS:
		void timeUnitChanged();
	};
}

/// @brief Fit a curve inside a plot player
/// Uses a dialog box to select the curve and fit type.
/// Returns the VipPyFitProcessing object on success.
/// The result of the VipPyFitProcessing object will be displayed 
/// as a dash curve on the same player, with overlayed fit equation.
VIP_GUI_EXPORT VipPyFitProcessing* vipFitCurve(VipPlotPlayer* player, const QString& fit);

/// @brief Fit a curve inside a plot player with given fit type (one of VipPyFitProcessing::Type).
/// Returns the VipPyFitProcessing object on success.
/// The result of the VipPyFitProcessing object will be displayed
/// as a dash curve on the same player, with overlayed fit equation.
VIP_GUI_EXPORT VipPyFitProcessing* vipFitCurve(VipPlotCurve* curve, VipPlotPlayer* player, int fit_type);

#endif