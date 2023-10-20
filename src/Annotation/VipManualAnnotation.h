#ifndef VIP_MANUAL_ANNOTATION_H
#define VIP_MANUAL_ANNOTATION_H

#include "VipStandardWidgets.h"
#include "VipPlayer.h"
#include "VipProgress.h"
#include "VipPlotMarker.h"
#include "VipSqlQuery.h"

typedef QMap<qint64, QPolygonF> MarkersType;
Q_DECLARE_METATYPE(MarkersType);


class VirtualTimeGrip;

class VIP_ANNOTATION_EXPORT VipTimeMarker : public VipPlotMarker
{
	Q_OBJECT
public:
	VirtualTimeGrip* grip;
	QPointer< VipProcessingPool> pool;
	QPointer< VipVideoPlayer> player;
	QPointer< VipPlotShape> shape;

	VipTimeMarker(VipProcessingPool* _pool, VipVideoPlayer* p, VipAbstractScale* parent, VipPlotShape* _shape, qint64 time);
	~VipTimeMarker();

	virtual void draw(QPainter* p, const VipCoordinateSystemPtr& m) const;
	double value() const;
	void setValue(double v);
};

class VIP_ANNOTATION_EXPORT VipAnnotationParameters : public QWidget
{
	Q_OBJECT

public:
	VipAnnotationParameters(const QString & device);
	~VipAnnotationParameters();

	void setCategory(const QString & cat);
	QString category() const;

	void setComment(const QString &comment);
	QString comment() const;

	void setDataset(const QString& d);
	QString dataset() const;

	void setName(const QString& name);
	QString name() const;

	void setConfidence(double);
	double confidence() const;

	void setCamera(const QString & camera);
	QString camera() const;

	void setDevice(const QString& device);
	QString device() const;

	void setPulse(Vip_experiment_id);
	Vip_experiment_id pulse() const;

	bool commentChanged() const;
	bool nameChanged() const;

	QComboBox * cameraBox() const;
	QComboBox* deviceBox() const;
	QComboBox * eventBox() const;

	void setCameraVisible(bool);
	bool cameraVisible() const;

	void setDeviceVisible(bool vis);
	bool deviceVisible() const;

	void setPulseVisible(bool);
	bool pulseVisible() const;

	virtual bool eventFilter(QObject* w, QEvent* evt);

private Q_SLOTS:
	void emitChanged();
	void deviceChanged();
Q_SIGNALS:
	void changed();

private:
	class PrivateData;
	PrivateData * m_data;
};

class VipPlayerDBAccess;
class VIP_ANNOTATION_EXPORT VipManualAnnotation : public QWidget
{
	Q_OBJECT

public:
	VipManualAnnotation(VipPlayerDBAccess * access);
	~VipManualAnnotation();

	VipVideoPlayer * player() const;

	Vip_event_list generateShapes(VipProgress * p = nullptr, QString * error = nullptr);

	/**
	* Filter key event 'K' at application level to put a key frame, and over shortcuts
	*/
	virtual bool eventFilter(QObject* w, QEvent* evt);

public Q_SLOTS:
	void addMarker(qint64);
	void addMarker();
	//void removeMarker(qint64);
	void clearMarkers();

private Q_SLOTS:
	void imageTransformChanged(const QTransform & _new);
	void computeMarkers();
	void delayComputeMarkers();
	void parametersChanged();
	void setTime(qint64);
	void emitSendToDB();
	void emitSendToJson();
Q_SIGNALS:
	void vipSendToDB();
	void sendToJson();
protected:
	virtual void keyPressEvent(QKeyEvent* key);
private:
	
	class PrivateData;
	PrivateData * m_data;
};

#endif