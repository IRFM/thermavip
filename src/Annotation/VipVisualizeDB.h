#ifndef VIP_VISUALIZE_DB_H
#define VIP_VISUALIZE_DB_H

#include <qtablewidget.h>

#include "VipStandardWidgets.h"
#include "VipToolWidget.h"
#include "VipProcessingObject.h"
#include "VipSqlQuery.h"

//Add options
// - show selected events on: 1) new player, 2) list of existing players
// - for selected events, plot time trace of: max Y, centroid X/Y (gather all columns in one widget with check boxes,
//and one checkbox to merge pulses. If merged, split them with  nan values. Make the action droppable.

class VipPlotItem;
class VIP_ANNOTATION_EXPORT VisualizeDB : public QWidget
{
	Q_OBJECT

public:
	VisualizeDB(QWidget * parent = nullptr);
	~VisualizeDB();

	static VisualizeDB * fromChild(QWidget * w);

	VipQueryDBWidget * queryWidget() const;
	QPushButton * launchQueryButton() const;
	QPushButton * resetQueryButton() const;
	QTableWidget * tableWidget() const;
	VipEventQueryResults events() const;
	
	void displayEventResult(const VipEventQueryResults & res, VipProgress * p = nullptr);

	virtual bool eventFilter(QObject* watched, QEvent * evt);
	//virtual bool viewportEvent(QEvent *event);
public Q_SLOTS:
	void suppressSelectedLines();
	void editSelectedColumn();
	void launchQuery();
	void resetQueryParameters();

	void saveToCSV();
	void copyToClipBoard();
	
private Q_SLOTS:
	void displaySelectedEvents(QAction*);
	void findRelatedEvents();
	void plotTimeTrace();

private:
	QByteArray dumpSelection();

	class PrivateData;
	PrivateData * m_data;
};



class VIP_ANNOTATION_EXPORT VisualizeDBToolWidget : public VipToolWidget
{
	Q_OBJECT

public:
	VisualizeDBToolWidget(VipMainWindow* = nullptr);
	VisualizeDB * getVisualizeDB() const;
};

VIP_ANNOTATION_EXPORT VisualizeDBToolWidget* vipGetVisualizeDBToolWidget(VipMainWindow* win = nullptr);

/// @brief Initialize VisualizeDBToolWidget, must be called in Thermavip entry point
VIP_ANNOTATION_EXPORT bool vipInitializeVisualizeDBWidget();

#endif
