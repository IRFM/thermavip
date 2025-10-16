/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VIP_MANUAL_ANNOTATION_H
#define VIP_MANUAL_ANNOTATION_H

#include "VipPlayer.h"
#include "VipPlotMarker.h"
#include "VipProgress.h"
#include "VipSqlQuery.h"
#include "VipStandardWidgets.h"

typedef QMap<qint64, QPolygonF> MarkersType;
Q_DECLARE_METATYPE(MarkersType);

class VirtualTimeGrip;

class VIP_ANNOTATION_EXPORT VipTimeMarker : public VipPlotMarker
{
	Q_OBJECT
public:
	VirtualTimeGrip* grip;
	QPointer<VipProcessingPool> pool;
	QPointer<VipVideoPlayer> player;
	QPointer<VipPlotShape> shape;

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
	VipAnnotationParameters(const QString& device);
	~VipAnnotationParameters();

	void setCategory(const QString& cat);
	QString category() const;

	void setComment(const QString& comment);
	QString comment() const;

	void setDataset(const QString& d);
	QString dataset() const;

	void setName(const QString& name);
	QString name() const;

	void setConfidence(double);
	double confidence() const;

	void setCamera(const QString& camera);
	QString camera() const;

	void setDevice(const QString& device);
	QString device() const;

	void setPulse(Vip_experiment_id);
	Vip_experiment_id pulse() const;

	bool commentChanged() const;
	bool nameChanged() const;

	QComboBox* cameraBox() const;
	QComboBox* deviceBox() const;
	QComboBox* eventBox() const;

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
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VipPlayerDBAccess;
class VIP_ANNOTATION_EXPORT VipManualAnnotation : public QWidget
{
	Q_OBJECT

public:
	VipManualAnnotation(VipPlayerDBAccess* access);
	~VipManualAnnotation();

	VipVideoPlayer* player() const;

	Vip_event_list generateShapes(VipProgress* p = nullptr, QString* error = nullptr);

	/**
	 * Filter key event 'K' at application level to put a key frame, and over shortcuts
	 */
	virtual bool eventFilter(QObject* w, QEvent* evt);

public Q_SLOTS:
	void addMarker(qint64);
	void addMarker();
	// void removeMarker(qint64);
	void clearMarkers();

private Q_SLOTS:
	void imageTransformChanged(const QTransform& _new);
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
	
	VIP_DECLARE_PRIVATE_DATA();
};

#endif