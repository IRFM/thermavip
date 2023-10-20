#ifndef VIP_RECORD_TOOL_WIDGET_HVipProcessingLeafDataSelector
#define VIP_RECORD_TOOL_WIDGET_H

#include <qtoolbutton.h>
#include "VipToolWidget.h"

/// \addtogroup Gui
/// @{


class VipProcessingObject;
class VipProcessingPool;
class VipRecordToolWidget;
class VipDisplayObject;
class VipPlotItem;
class VipFileName;
class VipRecordWidget;
class VipVideoPlayer;


class VIP_GUI_EXPORT VipRecordToolBar : public VipToolWidgetToolBar
{
	Q_OBJECT

public:
	VipRecordToolBar(VipRecordToolWidget * tool);
	~VipRecordToolBar();

	VipRecordToolWidget * toolWidget() const;
	VipFileName * filename() const;
	QToolButton * record() const;
	virtual void setDisplayPlayerArea(VipDisplayPlayerArea * area);

private Q_SLOTS:
	void updateMenu();
	void itemSelected(QAction* act);
	void updateWidget();
	void updateRecorder();
	void execMenu();
private:
	class PrivateData;
	PrivateData * m_data;
};

class VIP_GUI_EXPORT VipPlotItemSelector : public QToolButton
{
	Q_OBJECT
public:
	VipPlotItemSelector(VipRecordToolWidget * parent );
	~VipPlotItemSelector();

	VipProcessingPool * processingPool() const;
	QList<VipPlotItem*> possibleItems() const;

	static QList<VipPlotItem*> possibleItems(VipDisplayPlayerArea * area, const QList<VipPlotItem*> & current_item = QList<VipPlotItem*>()) ;
	static QList<QAction*> createActions(const QList<VipPlotItem*> & items, QObject * parent = nullptr) ;

Q_SIGNALS:
	void itemSelected(VipPlotItem*);

private Q_SLOTS:
	void aboutToShow();
	void processingSelected(QAction*);

private:
	class PrivateData;
	PrivateData * m_data;
};


class VIP_GUI_EXPORT SkipFrame : public  QWidget
{
	Q_OBJECT
public:
	SkipFrame(QWidget* parent = nullptr);
	~SkipFrame();

	int value();

public Q_SLOTS:
	void setValue(int);
	void reset();
private:
	class PrivateData;
	PrivateData *m_data;
};

class VipPlotItem;
class VIP_GUI_EXPORT VipRecordToolWidget : public  VipToolWidget
{
	Q_OBJECT

	friend class ListWidget;
public:
	enum RecordType
	{
		SignalArchive,
		Movie
	};

	VipRecordToolWidget(VipMainWindow * window);
	~VipRecordToolWidget();

	void setRecordType(RecordType);
	RecordType recordType() const;

	void setBackgroundColor(const QColor &);
	QColor backgroundColor() const;

	QString currentPlayer() const;
	void setCurrentPlayer(const QString & player);

	void setFilename(const QString & filename);
	QString filename() const;

	VipVideoPlayer * selectedVideoPlayer() const;
	void setRecordSceneOnly(bool enable);
	bool recordSceneOnly() const;

	VipRecordWidget * recordWidget() const;

	bool updateFileFiltersAndDevice(bool build_connections = false, bool close_device = true);

	virtual void setDisplayPlayerArea(VipDisplayPlayerArea * area);
	virtual VipRecordToolBar * toolBar();
	VipDisplayPlayerArea * area() const;
	VipProcessingPool * processingPool() const;
	QList<VipPlotItem*> selectedItems() const;
	VipPlotItemSelector * leafSelector() const;

public Q_SLOTS:
	bool addPlotItem(VipPlotItem * item);
	bool removePlotItem(VipPlotItem * item);

private Q_SLOTS:
	void recordTypeChanged();
	void displayAvailablePlayers();
	void displayAvailablePlayers(bool update_player_pixmap);
	void playerSelected();
	void itemClicked(VipPlotItem*, int);
	void timeout();
	void launchRecord(bool record);

	void updateBuffer();

private:
	class PrivateData;
	PrivateData * m_data;
};

VipRecordToolWidget * vipGetRecordToolWidget(VipMainWindow * window = nullptr);



class VipBaseDragWidget;

class VIP_GUI_EXPORT VipRecordWidgetButton : public QToolButton
{
	Q_OBJECT

public:
	VipRecordWidgetButton(VipBaseDragWidget * widget, QWidget * parent = nullptr);
	~VipRecordWidgetButton();

	QString filename() const;
	QColor backgroundColor() const;
	int frequency() const;

public :
	virtual bool eventFilter(QObject*, QEvent* evt);

private Q_SLOTS:
	void filenameChanged();
	void newImage();
	void setStarted(bool);

private:
	class PrivateData;
	PrivateData * m_data;
};

/// @}
//end Gui

#endif
