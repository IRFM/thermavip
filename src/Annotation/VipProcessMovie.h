#pragma once

#include "VipIODevice.h"
#include "VipDisplayObject.h"
#include "VipSqlQuery.h"
#include "VipManualAnnotation.h"

#include <qtoolbar.h>

class VipVideoPlayer;
class VipProgress;
class QToolButton;


/// @brief VipIODevice that outputs events on the form of a VipSceneModel
class VIP_ANNOTATION_EXPORT VipEventDevice : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput scene_model)

public:
	VipEventDevice(QObject * parent = NULL);
	~VipEventDevice();

	virtual bool open(VipIODevice::OpenModes mode);

	/// @brief Set the events and group (event type) to display
	void setEvents(const Vip_event_list & events, const QString & group);
	Vip_event_list events() const;
	QString group() const;

	/// @brief Set the sampling time used by readInvalidTime to avoid event flickering
	void setVideoSamplingTime(qint64 s);

protected:
	virtual bool readData(qint64 time);
	virtual bool readInvalidTime(qint64 time);
public:
	Vip_event_list m_events;
	QMap<qint64, VipSceneModel> m_scenes;
	QString m_group;
	qint64 m_video_sampling;
};


/// @brief Defines several widget used to modify events parameters before upload to DB
class VIP_ANNOTATION_EXPORT UploadToDB : public QWidget
{
	Q_OBJECT

public:
	UploadToDB(const QString & device, QWidget * parent = NULL);
	~UploadToDB();

	void setUserName(const QString &);
	QString userName() const;

	void setCamera(const QString &);
	QString camera() const;

	void setDevice(const QString&);
	QString device() const;

	void setPulse(Vip_experiment_id pulse);
	Vip_experiment_id pulse() const;

private Q_SLOTS:
	void deviceChanged();

private:
	class PrivateData;
	PrivateData * m_data;
};

class VipPlayerDBAccess;

/// @brief Display selected events information
class VIP_ANNOTATION_EXPORT EventInfo : public QToolBar
{
	Q_OBJECT

public:
	EventInfo(VipPlayerDBAccess * pdb);
	~EventInfo();

	void setCategory(const QString & cat);
	QString category() const;

	void setDataset(const QString& d);
	QString dataset() const;

	void setAnalysisStatus(const QString& cat);
	QString analysisStatus() const;

	void setComment(const QString &comment);
	QString comment() const;

	void setName(const QString &name);
	QString name() const;

	void setConfidence(double);
	double confidence() const;

	void setAutomaticState(Qt::CheckState st);
	Qt::CheckState automaticState() const;

	void setMethod(const QString& method);
	QString method() const;

	void setUserName(const QString & user);
	void setDuration(double duration_s);
	
	QList<qint64> mergeIds() const;
	void clearMergeIds();

	void setUndoToolTip(const QString& str);

private Q_SLOTS:
	void apply();
	void emitUndo();
	void emitSplit() { Q_EMIT split(); }
	void emitRemoveFrames() { Q_EMIT removeFrames(); }
	void emitInterpFrames() { Q_EMIT interpFrames(); }
Q_SIGNALS:
	void applied();
	void undo();
	void split();
	void removeFrames();
	void interpFrames();
private:
	
	class PrivateData;
	PrivateData * m_data;
};

/// @brief Class allowing interaction between a VipVideoPlayer and a connection to an event database
class VIP_ANNOTATION_EXPORT VipPlayerDBAccess : public QObject
{
	Q_OBJECT

public:

	//Flag set to each event
	enum EventFlag {
		New, //New computed event
		DB //event retrieved from DB
		//Modified //event retrieved from DB and modified
	};

	struct Action {
		enum Type {
			Remove,
			ChangeType,
			ChangeValue,
			MergeEvents,
			ChangePolygon,
			SplitEvents,
			RemoveFrames,
			InterpolateFrames
		};
		QList<qint64> ids;
		QString value;
		QString name;
		qint64 time; //for ChangePolygon, polygons time. For SplitEvents, split time.
		QList<QPolygonF> polygons; //for ChangePolygon only, polygons for all ids
		VipTimeRange range; //for RemoveFrames , time range to be removed
		VipTimeRangeList ranges; //for InterpolateFrames only, time range for each event event id inside which we must interpolate polygons
		Type type;
	};

	VipPlayerDBAccess(VipVideoPlayer * player);

	static VipPlayerDBAccess * fromPlayer(VipVideoPlayer * pl);

	void remove(qint64 id);
	void changeCategory(const QString & new_type, const QList<qint64> & ids);
	void changeValue(const QString & name, const QString & value, const QList<qint64> & ids);
	void mergeIds(const QList<qint64>& ids);
	Vip_experiment_id pulse() const;
	QString camera() const;
	QString device() const;
	VipVideoPlayer * player() const;
	QList<VipDisplayObject*> displayEvents();
	QList<VipEventDevice*> devices();
	const QList<Action>& actionsStack() const;
	VipEventDevice * device(const QString & group);

	void addEvents(const Vip_event_list & events, bool fromDB);

public Q_SLOTS:
	void upload();
	void saveToJson();
	void uploadNoMessage();
	void displayFromDataBase();
	void displayFromDataBaseQuery(const VipEventQuery& query, bool clear_previous);
	void displayFromJsonFile();
	void undo();
	void clear();
	void showManualAnnotation(bool);
	void setSceneModelVisible(bool vis);
	void changeSelectedPolygons();
	void splitEvents();
	void removeFramesToEvents();
	void interpolateFrames();
	void updateUndoToolTip();
private Q_SLOTS:
	void aboutToShow();
	void showEvents();
	void shapeDestroyed(VipPlotShape*);
	void itemSelected(VipPlotItem*);
	void applyActions();
	void applyChangesToSelection();
	void sendManualAnnotation();
	void sendManualAnnotationToJson();
	void saveCSV();
	VipEventDevice* createDevice(const Vip_event_list & events, const QString & group);
private:
	void resetDrawEventTimeLine();
	Vip_event_list applyActions(const Vip_event_list & events);
	void setPulse(Vip_experiment_id);
	void setCamera(const QString &);
	void setDevice(const QString& );
	void uploadInternal(bool show_messages);
	void saveToJsonInternal(bool show_messages);
	Vip_event_list m_initial_events;
	Vip_event_list m_events;
	QList<QPointer<VipDisplayObject> > m_displays;
	QList<QPointer<VipEventDevice> > m_devices;
	QPointer<VipVideoPlayer> m_player;
	QPointer<VipPlotShape> m_selected_item;
	QList< QPointer<VipPlotShape> > m_selection;
	QList<Action> m_actions;
	QToolButton * m_db;
	EventInfo * m_infos;
	QPointer<VipManualAnnotation> m_annotation;
	qint64 m_recordTime;

	QMap<qint64, qint64> m_modifications;

	VipSceneModel m_sceneModel;
	QPointer<VipPlotSceneModel> m_plotSM;
};


