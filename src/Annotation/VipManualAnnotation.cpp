/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, L�o Dubus, Erwan Grelier
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

#include "VipManualAnnotation.h"
#include "VipDisplayArea.h"
#include "VipDrawShape.h"
#include "VipIODevice.h"
#include "VipPlayWidget.h"
#include "VipPolygon.h"
#include "VipProcessMovie.h"
#include "VipSet.h"
#include "VipSliderGrip.h"
#include "VipSqlQuery.h"

#include <qapplication.h>
#include <qboxlayout.h>
#include <qgraphicssceneevent.h>
#include <qmessagebox.h>
#include <qset.h>
#include <qstandarditemmodel.h>
#include <qtooltip.h>

static int registerMetaTypes()
{
	qRegisterMetaType<MarkersType>();
	qRegisterMetaTypeStreamOperators<MarkersType>();
	return 0;
}
static int _registerMetaTypes = registerMetaTypes();

class VipTimeMarker;

class VirtualTimeGrip : public VipSliderGrip
{

public:
	VirtualTimeGrip(VipTimeMarker* marker, VipAbstractScale* parent);

protected:
	virtual double closestValue(double v);
	virtual bool sceneEventFilter(QGraphicsItem* watched, QEvent* event);

private:
	VipTimeMarker* d_marker;
	bool d_aboutTo;
};
VipTimeMarker::VipTimeMarker(VipProcessingPool* _pool, VipVideoPlayer* p, VipAbstractScale* parent, VipPlotShape* _shape, qint64 time)
  : VipPlotMarker()
  , pool(_pool)
  , player(p)
  , shape(_shape)
{
	grip = new VirtualTimeGrip(this, parent);
	grip->setImage(QImage(1, 1, QImage::Format_ARGB32));
	grip->setVisible(false);
	this->setLineStyle(VipPlotMarker::VLine);
	this->setExpandToFullArea(true);
	this->setFlag(VipPlotItem::ItemIsSelectable, true);
	this->setItemAttribute(VipPlotItem::AutoScale, false);
	this->setItemAttribute(VipPlotItem::IsSuppressable, true);
	this->setRenderHints(QPainter::Antialiasing);
	this->setRawData(QPointF(time, 0));
	this->setLabelAlignment(Qt::AlignVCenter | Qt::AlignRight);
	this->setLabel(QString::number(_shape->rawData().id()));
	grip->setValue(time);
	connect(grip, &VipSliderGrip::valueChanged, this, &VipTimeMarker::setValue);
	connect(_shape, SIGNAL(destroyed(VipPlotItem*)), this, SLOT(deleteLater()));
}
VipTimeMarker::~VipTimeMarker()
{
	if (shape) {
		// push current state before removing time marker
		if (player) {
			VipSceneModelState::instance()->pushState(player, player->plotSceneModel());
		}

		// remove this time from shape
		MarkersType m = shape->rawData().attribute("_vip_markers").value<MarkersType>();
		m.remove(value());
		shape->rawData().setAttribute("_vip_markers", QVariant::fromValue(m));
	}
	delete grip;
}
void VipTimeMarker::draw(QPainter* p, const VipCoordinateSystemPtr& m) const
{
	if (isSelected())
		const_cast<VipTimeMarker*>(this)->setPen(QPen(Qt::red, 1.5));
	else
		const_cast<VipTimeMarker*>(this)->setPen(QPen(Qt::green, 1.5));
	VipPlotMarker::draw(p, m);
}
double VipTimeMarker::value() const
{
	return rawData().x();
}
void VipTimeMarker::setValue(double v)
{
	qint64 prev = value();
	setRawData(QPointF(v, 0));
	if (v != grip->value())
		grip->setValue(v);

	// set the new time value to the shape
	if (shape) {
		MarkersType m = shape->rawData().attribute("_vip_markers").value<MarkersType>();
		MarkersType::iterator it = m.find(prev);
		if (it != m.end()) {
			QPolygonF p = it.value();
			m.erase(it);
			m.insert(v, p);
			shape->rawData().setAttribute("_vip_markers", QVariant::fromValue(m));
		}
	}
}

VirtualTimeGrip::VirtualTimeGrip(VipTimeMarker* marker, VipAbstractScale* parent)
  : VipSliderGrip(parent)
  , d_marker(marker)
  , d_aboutTo(false)
{
	setValue(0);
	setDisplayToolTipValue(Qt::AlignHCenter | Qt::AlignBottom);
}

double VirtualTimeGrip::closestValue(double v)
{
	if (d_marker->pool) {
		qint64 tmp = d_marker->pool->closestTime(v);
		if (tmp != VipInvalidTime)
			return tmp;
		else
			return v;
	}
	else
		return v;
}

bool VirtualTimeGrip::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
	if (qobject_cast<VipPlotMarker*>(watched->toGraphicsObject())) {
		if (event->type() == QEvent::GraphicsSceneMouseMove) {
			if (d_aboutTo) {
				d_aboutTo = false;
				// push state before moving time marker
				VipSceneModelState::instance()->pushState(d_marker->player, d_marker->player->plotSceneModel());
			}
			double prev = this->value();
			this->mouseMoveEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
			double after = this->value();

			// apply the motion to other selected TimerMarker
			QList<VipTimeMarker*> markers = vipCastItemList<VipTimeMarker*>(d_marker->linkedItems(), QString(), 1, 1);
			markers.removeOne(d_marker);
			double diff = after - prev;
			for (int i = 0; i < markers.size(); ++i)
				markers[i]->setValue(markers[i]->value() + diff);

			return true;
		}
		else if (event->type() == QEvent::GraphicsSceneMousePress) {
			// watched->setSelected(true);
			// Call VipTimeMarker::mousePressEvent to handle selection with CTRL
			static_cast<VipPlotItem*>(watched)->mousePressEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
			d_aboutTo = true;
			this->mousePressEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
			return true;
		}
		else if (event->type() == QEvent::GraphicsSceneMouseRelease) {
			d_aboutTo = false;
			this->mouseReleaseEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
			return true;
		}
	}
	return false;
}

class TimeMarkersManager : public QObject
{
public:
	QPointer<VipVideoPlayer> player;
	QList<QPointer<VipTimeMarker>> plotMarkers;

	TimeMarkersManager(VipVideoPlayer* player, QObject* parent = nullptr)
	  : QObject(parent)
	  , player(player)
	{
	}

	~TimeMarkersManager()
	{
		// remove time markers from time line
		for (int i = 0; i < plotMarkers.size(); ++i) {
			if (plotMarkers[i]) {
				plotMarkers[i]->shape = nullptr;
				plotMarkers[i]->deleteLater();
			}
		}
	}

	void computeTimeMarkers(const QList<VipPlotShape*>& selected)
	{
		// remove time markers from time line
		for (int i = 0; i < plotMarkers.size(); ++i) {
			if (plotMarkers[i]) {
				plotMarkers[i]->shape = nullptr;
				plotMarkers[i]->deleteLater();
			}
		}
		plotMarkers.clear();

		// We are going to add VipPlotMarker objects on the area time line, so find the area first
		// get parent VipDisplayPlayerArea
		VipDisplayPlayerArea* area = nullptr;
		QWidget* p = (player->parentWidget());
		while (p) {
			area = qobject_cast<VipDisplayPlayerArea*>(p);
			if (area)
				break;
			p = p->parentWidget();
		}
		if (!area)
			return;

		// build new markers
		for (int i = 0; i < selected.size(); ++i) {
			const MarkersType m = selected[i]->rawData().attribute("_vip_markers").value<MarkersType>();
			for (MarkersType::const_iterator it = m.begin(); it != m.end(); ++it) {
				qint64 time = it.key();
				VipTimeMarker* tm = new VipTimeMarker(player->processingPool(), player, area->playWidget()->area()->timeScale(), selected[i], time);
				tm->setAxes(area->playWidget()->area()->timeScale(), area->playWidget()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
				tm->setZValue(area->playWidget()->area()->timeMarker()->zValue() - 0.01);
				tm->installSceneEventFilter(tm->grip);
				plotMarkers.append(tm);
			}
		}
	}

	void clearMarkers()
	{
		QList<VipPlotShape*> shapes = player->plotSceneModel()->shapes(1);
		for (int i = 0; i < shapes.size(); ++i) {
			shapes[i]->rawData().setAttribute("_vip_markers", QVariant());
		}
		computeTimeMarkers(shapes);
	}

	QMap<VipPlotShape*, VipTimeRange> timeRanges(VipTimeRange* union_range = nullptr) const
	{
		VipTimeRange _union = VipInvalidTimeRange;
		QMap<VipPlotShape*, VipTimeRange> res;
		QList<VipPlotShape*> shapes = player->plotSceneModel()->shapes(1);
		for (int i = 0; i < shapes.size(); ++i) {
			MarkersType m = shapes[i]->rawData().attribute("_vip_markers").value<MarkersType>();
			if (m.size()) {
				res[shapes[i]] = VipTimeRange(m.firstKey(), m.lastKey());
				if (_union == VipInvalidTimeRange)
					_union = res[shapes[i]];
				else {
					_union.first = qMin(_union.first, m.firstKey());
					_union.second = qMax(_union.second, m.lastKey());
				}
			}
		}
		if (union_range)
			*union_range = _union;
		return res;
	}

	QPolygonF createPolygon(qint64 time, const MarkersType& markers) const
	{
		if (markers.isEmpty())
			return QPolygonF();

		int min_count = markers.begin()->size();
		// check minimum number of points
		for (MarkersType::const_iterator it = markers.begin(); it != markers.end(); ++it) {
			min_count = qMin(min_count, it->size());
		}
		if (min_count == 0)
			return QPolygonF();

		// try to interpolate the shape
		qint64 first = VipInvalidTime;
		qint64 second = VipInvalidTime;
		for (MarkersType::const_iterator it = markers.begin(); it != markers.end(); ++it) {
			if (it.key() == time)
				return it.value();
			else if (it.key() < time)
				first = it.key();
			else if (it.key() > time) {
				second = it.key();
				break;
			}
		}

		if (first != VipInvalidTime && second != VipInvalidTime) {
			// interpolate
			QPolygonF r1 = markers[first];
			QPolygonF r2 = markers[second];
			/*if (r1.size() > min_count)
				r1 = r1.mid(0, min_count);
			if (r2.size() > min_count)
				r2 = r2.mid(0, min_count);
			if (second == first) {
				return r1;
			}
			else {
				double factor = (time - first) / (double)(second - first);
				QPolygonF res(min_count);
				for (int i = 0; i < min_count; ++i) {
					res[i] = r1[i] * (1 - factor) + r2[i] * factor;
				}
				return res;
			}*/
			double factor = (time - first) / (double)(second - first);
			QPolygonF res = vipInterpolatePolygons(r1, r2, factor);
			res = vipSimplifyPolygonDB(res, VIP_DB_MAX_FRAME_POLYGON_POINTS);
			return res;
		}
		else if (first != VipInvalidTime) {
			// take the last rect
			return (markers.last());
		}
		else {
			// take the first rect
			return (markers.first());
		}
		VIP_UNREACHABLE();
		// return QPolygonF();
	}

	void setTime(qint64 time, bool update_processing_pool)
	{
		// set the pool time
		if (update_processing_pool && player->processingPool()->time() != time)
			player->processingPool()->seek(time);

		bool in_main_thread = (QThread::currentThread() == qApp->thread());

		// we need to update ALL shapes
		QList<VipPlotShape*> shapes = player->plotSceneModel()->shapes();
		for (int i = 0; i < shapes.size(); ++i) {
			MarkersType markers = shapes[i]->rawData().attribute("_vip_markers").value<MarkersType>();
			if (markers.size() > 1) {
				QPolygonF r = createPolygon(time, markers);
				QRectF tmp;
				if (vipIsRect(r, &tmp))
					shapes[i]->rawData().setRect(tmp);
				else
					shapes[i]->rawData().setPolygon(r);
				if (in_main_thread)
					shapes[i]->update();
				else
					QMetaObject::invokeMethod(shapes[i], "update", Qt::QueuedConnection);
			}
		}
	}

	/*void remove(qint64 time)
	{
		if (time == VipInvalidTime)
			return;
		//remove from shapes
		QList<VipPlotShape*> shapes = player->plotSceneModel()->shapes(1);
		for (int i = 0; i < shapes.size(); ++i) {
			MarkersType m = shapes[i]->rawData().attribute("_vip_markers").value<MarkersType>();
			m.remove(time);
			shapes[i]->rawData().setAttribute("_vip_markers", QVariant::fromValue(m));
		}

		computeTimeMarkers(shapes);
	}*/

	void add(qint64 time)
	{
		if (time == VipInvalidTime)
			return;

		// push state before adding marker
		VipSceneModelState::instance()->pushState(player, player->plotSceneModel());

		QList<VipPlotShape*> shapes = player->plotSceneModel()->shapes(1);
		for (int i = 0; i < shapes.size(); ++i) {
			MarkersType m = shapes[i]->rawData().attribute("_vip_markers").value<MarkersType>();
			MarkersType::iterator it = m.find(time);
			if (it == m.end())
				m.insert(time, shapes[i]->rawData().polygon());
			else
				it.value() = shapes[i]->rawData().polygon();
			shapes[i]->rawData().setAttribute("_vip_markers", QVariant::fromValue(m));
		}
		computeTimeMarkers(shapes);
	}
};

class VipAnnotationParameters::PrivateData
{
public:
	QWidget* pulse;
	QComboBox camera;
	QComboBox device;
	QComboBox category;
	VipDatasetButton dataset;
	QDoubleSpinBox confidence;
	QLineEdit comment;
	QLineEdit name;
	bool commentChanged;
	bool nameChanged;
	QAction* camAction;
	QAction* pulseAction;
};
VipAnnotationParameters::VipAnnotationParameters(const QString& device)
  : QWidget()
{
	// setStyleSheet("QToolBar > QToolButton{ margin: 0px 10px; }");
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->commentChanged = false;
	d_data->nameChanged = false;

	d_data->camera.addItems(QStringList() << QString() << vipCamerasDB());
	d_data->camera.setToolTip("Camera name");

	d_data->device.addItems(QStringList() << QString() << vipDevicesDB());
	d_data->device.setToolTip("Device name");

	d_data->pulse = vipFindDeviceParameters(device)->pulseEditor();

	d_data->category.addItems(QStringList() << QString() << vipEventTypesDB());
	d_data->category.setToolTip("Event type");
	d_data->confidence.setRange(-0.25, 1);
	d_data->confidence.setSpecialValueText(" ");
	d_data->confidence.setValue(1);
	d_data->confidence.setSingleStep(0.25);
	d_data->confidence.setToolTip("confidence (0->1)");
	d_data->comment.setPlaceholderText("User comments");
	d_data->comment.setToolTip("<b>User comments (optional)</b><br>Press ENTER to validate");
	d_data->name.setPlaceholderText("Event name");
	d_data->comment.setToolTip("<b>Event name (optional)</b><br>Press ENTER to validate");

	/*d_data->pulseAction = addWidget(&d_data->pulse);
	d_data->camAction = addWidget(&d_data->camera);

	addWidget(&d_data->category);
	addWidget(&d_data->dataset);
	addWidget(&d_data->confidence);
	addWidget(&d_data->comment);
	addWidget(&d_data->name);*/
	QHBoxLayout* lay = new QHBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->pulse);
	lay->addWidget(&d_data->camera);
	lay->addWidget(&d_data->device);
	lay->addWidget(&d_data->category);
	lay->addWidget(&d_data->dataset);
	lay->addWidget(&d_data->confidence);
	lay->addWidget(&d_data->comment);
	lay->addWidget(&d_data->name);
	setLayout(lay);

	connect(d_data->pulse, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&d_data->camera, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
	connect(&d_data->device, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
	connect(&d_data->device, SIGNAL(currentIndexChanged(int)), this, SLOT(deviceChanged()));
	connect(&d_data->category, SIGNAL(currentIndexChanged(int)), this, SLOT(emitChanged()));
	connect(&d_data->dataset, SIGNAL(changed()), this, SLOT(emitChanged()));
	connect(&d_data->confidence, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&d_data->comment, SIGNAL(returnPressed()), this, SLOT(emitChanged()));
	connect(&d_data->name, SIGNAL(returnPressed()), this, SLOT(emitChanged()));
}
VipAnnotationParameters::~VipAnnotationParameters()
{
}

void VipAnnotationParameters::deviceChanged()
{
	double pulse = this->pulse();
	QWidget* p = vipFindDeviceParameters(device())->pulseEditor();
	bool hidden = d_data->pulse->isHidden();
	delete d_data->pulse;
	static_cast<QHBoxLayout*>(layout())->insertWidget(0, d_data->pulse = p);
	p->setVisible(!hidden);
	setPulse(pulse);
}

QComboBox* VipAnnotationParameters::cameraBox() const
{
	return &d_data->camera;
}
QComboBox* VipAnnotationParameters::deviceBox() const
{
	return &d_data->device;
}
QComboBox* VipAnnotationParameters::eventBox() const
{
	return &d_data->category;
}

void VipAnnotationParameters::setCameraVisible(bool vis)
{
	// d_data->camAction->setVisible(vis);
	d_data->camera.setVisible(vis);
}
bool VipAnnotationParameters::cameraVisible() const
{
	// return d_data->camAction->isVisible();
	return d_data->camera.isVisible();
}

void VipAnnotationParameters::setDeviceVisible(bool vis)
{
	// d_data->camAction->setVisible(vis);
	d_data->device.setVisible(vis);
}
bool VipAnnotationParameters::deviceVisible() const
{
	// return d_data->camAction->isVisible();
	return d_data->device.isVisible();
}

void VipAnnotationParameters::setPulseVisible(bool vis)
{
	// d_data->pulseAction->setVisible(vis);
	d_data->pulse->setVisible(vis);
}
bool VipAnnotationParameters::pulseVisible() const
{
	//	return d_data->pulseAction->isVisible();
	return d_data->pulse->isVisible();
}

bool VipAnnotationParameters::eventFilter(QObject*, QEvent* evt)
{
	// filter key events on cameras, pulse, event type, and confidence to catch CTRL+Z or CTRL+Y (undo/redo) and let them got the to upper widget
	if (evt->type() == QEvent::KeyPress) {
		QKeyEvent* k = static_cast<QKeyEvent*>(evt);
		if (k->key() == Qt::Key_Z && (k->modifiers() & Qt::CTRL)) {
			k->ignore();
			return true;
		}
		else if (k->key() == Qt::Key_Y && (k->modifiers() & Qt::CTRL)) {
			k->ignore();
			return true;
		}
	}
	return false;
}

bool VipAnnotationParameters::commentChanged() const
{
	return d_data->commentChanged;
}

bool VipAnnotationParameters::nameChanged() const
{
	return d_data->nameChanged;
}

void VipAnnotationParameters::emitChanged()
{
	d_data->commentChanged = sender() == &d_data->comment;
	d_data->nameChanged = sender() == &d_data->name;
	Q_EMIT changed();
}

void VipAnnotationParameters::setCategory(const QString& cat)
{
	d_data->category.setCurrentText(cat);
}
QString VipAnnotationParameters::category() const
{
	return d_data->category.currentText();
}

void VipAnnotationParameters::setComment(const QString& comment)
{
	d_data->comment.setText(comment);
}
QString VipAnnotationParameters::comment() const
{
	return d_data->comment.text();
}

void VipAnnotationParameters::setDataset(const QString& dataset)
{
	d_data->dataset.setDataset(dataset);
}
QString VipAnnotationParameters::dataset() const
{
	return d_data->dataset.dataset();
}

void VipAnnotationParameters::setName(const QString& name)
{
	d_data->name.setText(name);
}
QString VipAnnotationParameters::name() const
{
	return d_data->name.text();
}

void VipAnnotationParameters::setConfidence(double c)
{
	d_data->confidence.setValue(c);
}
double VipAnnotationParameters::confidence() const
{
	return d_data->confidence.value();
}

void VipAnnotationParameters::setCamera(const QString& camera)
{
	d_data->camera.setCurrentText(camera);
}
QString VipAnnotationParameters::camera() const
{
	return d_data->camera.currentText();
}

void VipAnnotationParameters::setDevice(const QString& device)
{
	d_data->device.setCurrentText(device);
}
QString VipAnnotationParameters::device() const
{
	return d_data->device.currentText();
}
void VipAnnotationParameters::setPulse(Vip_experiment_id p)
{
	// vip_debug("set pulse: %f\n",(double)p);fflush(stdout);
	d_data->pulse->setProperty("value", p);
	// vip_debug("set: %f\n",(double)d_data->pulse.value());fflush(stdout);
}
Vip_experiment_id VipAnnotationParameters::pulse() const
{
	return d_data->pulse->property("value").value<Vip_experiment_id>();
}

#include <functional>

class VipManualAnnotation::PrivateData
{
public:
	QPointer<VipVideoPlayer> player;
	QPointer<VipPlayerDBAccess> dbAccess;
	QToolButton* close;
	QToolButton* create;
	QToolButton* remove;
	QToolButton* send;
	TimeMarkersManager* drawMarkers;
	VipAnnotationParameters* params;
	VipDragMenu* menu;
	QTransform playerTr;
	QPointer<VipProcessingPool> pool;
	int callback_id;
	QTimer timer;
	QList<QByteArray> states;
};

VipManualAnnotation::VipManualAnnotation(VipPlayerDBAccess* access)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->dbAccess = access;
	d_data->player = access->player();
	d_data->timer.setSingleShot(true);

	d_data->playerTr = player()->imageTransform();

	d_data->close = new QToolButton();
	d_data->close->setIcon(vipIcon("close.png"));
	d_data->close->setToolTip("Close annotation panel.\nThis will NOT remove the annotations you defined.");
	d_data->close->setAutoRaise(true);
	d_data->create = new QToolButton();
	d_data->create->setIcon(vipIcon("new.png"));
	d_data->create->setToolTip("Create a new time marker for the selected shapes");
	d_data->create->setAutoRaise(true);
	d_data->create->setEnabled(false);
	d_data->send = new QToolButton();
	d_data->send->setIcon(vipIcon("database.png"));
	d_data->send->setToolTip("<b>Send selected annotations to DataBase or to JSON file</b><br>Only selected events will be sent.");
	d_data->send->setAutoRaise(true);
	d_data->send->setEnabled(false);
	d_data->send->setPopupMode(QToolButton::InstantPopup);
	d_data->send->setMenu(new QMenu());
	connect(d_data->send->menu()->addAction("Send to DB"), SIGNAL(triggered(bool)), this, SLOT(emitSendToDB()));
	connect(d_data->send->menu()->addAction("Send to Json file..."), SIGNAL(triggered(bool)), this, SLOT(emitSendToJson()));

	d_data->remove = new QToolButton();
	d_data->remove->setIcon(vipIcon("del.png"));
	d_data->remove->setToolTip("Remove all time markers for the selected shapes");
	d_data->remove->setAutoRaise(true);
	d_data->remove->setEnabled(false);
	d_data->drawMarkers = new TimeMarkersManager(d_data->player, this);

	d_data->params = new VipAnnotationParameters(d_data->dbAccess->device());
	/*d_data->menu = new VipDragMenu();
	d_data->menu->setWidget(d_data->params);
	d_data->send->setMenu(d_data->menu);
	d_data->send->setPopupMode(QToolButton::MenuButtonPopup);*/
	d_data->params->setDataset(QString());
	d_data->params->setCategory(QString()); //"localized heat flux");//"hotspot"
	d_data->params->setCamera(d_data->dbAccess->camera());
	d_data->params->setDevice(d_data->dbAccess->device());
	d_data->params->setPulse(d_data->dbAccess->pulse());
	d_data->params->setConfidence(1);

	if (d_data->dbAccess->camera().size() > 0)
		d_data->params->setCameraVisible(false);
	if (d_data->dbAccess->device().size() > 0)
		d_data->params->setDeviceVisible(false);
	if (d_data->dbAccess->pulse() > 0)
		d_data->params->setPulseVisible(false);

	QHBoxLayout* lay = new QHBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->close);
	lay->addWidget(VipLineWidget::createVLine());
	lay->addWidget(d_data->create);
	lay->addWidget(d_data->send);
	lay->addWidget(d_data->remove);
	lay->addWidget(VipLineWidget::createVLine());
	lay->addWidget(d_data->params, 1);
	setLayout(lay);

	connect(d_data->close, SIGNAL(clicked(bool)), this, SLOT(deleteLater()));
	connect(d_data->create, SIGNAL(clicked(bool)), this, SLOT(addMarker()));
	connect(d_data->remove, SIGNAL(clicked(bool)), this, SLOT(clearMarkers()));
	// connect(d_data->send, SIGNAL(clicked(bool)), this, SLOT(emitSendToDB()));
	connect(player()->plotSceneModel(), SIGNAL(shapeSelectionChanged(VipPlotShape*)), this, SLOT(delayComputeMarkers()));
	connect(player()->plotSceneModel(), SIGNAL(shapeDestroyed(VipPlotShape*)), this, SLOT(delayComputeMarkers()));
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(computeMarkers()));
	connect(player(), SIGNAL(imageTransformChanged(const QTransform&)), this, SLOT(imageTransformChanged(const QTransform&)));
	connect(d_data->params, SIGNAL(changed()), this, SLOT(parametersChanged()));

	// connect(player()->processingPool(), SIGNAL(timeChanged(qint64)), this, SLOT(setTime(qint64)),Qt::QueuedConnection);
	// d_data->pool = player()->processingPool();
	QObject* obj = player()->processingPool()->addReadDataCallback(std::bind(&VipManualAnnotation::setTime, this, std::placeholders::_1));
	obj->setParent(this);

	computeMarkers();

	qApp->installEventFilter(this);
}
VipManualAnnotation::~VipManualAnnotation()
{
	qApp->removeEventFilter(this);
}

void VipManualAnnotation::delayComputeMarkers()
{
	d_data->timer.start(20);
}

static QString getParam(const QList<VipPlotShape*>& selected, const QString& name)
{
	// returns the attribute 'name' if it has the same value for all shapes, or returns an empty string
	QSet<QString> res;
	QString attr = "_vip_" + name;
	for (int i = 0; i < selected.size(); ++i) {
		res.insert(selected[i]->rawData().attribute(attr).toString());
	}
	if (res.size() == 1)
		return *res.begin();
	return QString();
}

void VipManualAnnotation::parametersChanged()
{
	QList<VipPlotShape*> selected = d_data->player->plotSceneModel()->shapes(1);
	if (selected.size() && d_data->drawMarkers->player) {
		// push state before changing shapes parameters
		VipSceneModelState::instance()->pushState(d_data->drawMarkers->player, d_data->drawMarkers->player->plotSceneModel());
	}
	for (int i = 0; i < selected.size(); ++i) {
		if (d_data->params->pulse() >= 0)
			selected[i]->rawData().setAttribute("_vip_Pulse", d_data->params->pulse());
		if (d_data->params->confidence() >= 0)
			selected[i]->rawData().setAttribute("_vip_Confidence", d_data->params->confidence());
		if (d_data->params->camera().size())
			selected[i]->rawData().setAttribute("_vip_Camera", d_data->params->camera());
		if (d_data->params->device().size())
			selected[i]->rawData().setAttribute("_vip_Device", d_data->params->device());
		if (d_data->params->category().size())
			selected[i]->rawData().setAttribute("_vip_Event", d_data->params->category());
		// if (d_data->params->dataset().size())
		selected[i]->rawData().setAttribute("_vip_Dataset", d_data->params->dataset());
		if (d_data->params->commentChanged())
			selected[i]->rawData().setAttribute("_vip_Comment", d_data->params->comment());
		if (d_data->params->nameChanged())
			selected[i]->rawData().setAttribute("_vip_Name", d_data->params->name());
	}
}

static QStringList comboBoxList(QComboBox* box)
{
	QStringList lst;
	for (int i = 0; i < box->count(); ++i)
		lst.append(box->itemText(i));
	return lst;
}

void VipManualAnnotation::computeMarkers()
{

	QList<VipPlotShape*> selected = d_data->player->plotSceneModel()->shapes(1);
	d_data->drawMarkers->computeTimeMarkers(selected);
	d_data->create->setEnabled(selected.size());
	d_data->send->setEnabled(selected.size());
	d_data->remove->setEnabled(selected.size());

	if (selected.size()) {
		Vip_experiment_id current_pulse = d_data->dbAccess->pulse();
		QString current_camera = d_data->dbAccess->camera();
		QString current_device = d_data->dbAccess->device();

		if (current_camera.size() >= 0) {

			// Force the camera name
			QStringList cameras = (QStringList() << current_camera);
			if (comboBoxList(d_data->params->cameraBox()) != cameras) {
				d_data->params->cameraBox()->clear();
				d_data->params->cameraBox()->addItems(cameras);
			}
			// Force the list of events
			QStringList events = vipEventTypesDB(); // current_camera);
			if (events.size() > 1)
				events.insert(0, QString());
			if (comboBoxList(d_data->params->eventBox()) != events) {
				QString text = d_data->params->eventBox()->currentText();
				d_data->params->eventBox()->clear();
				d_data->params->eventBox()->addItems(events);
				d_data->params->eventBox()->setCurrentText(text);
			}
		}

		if (current_device.size() >= 0) {

			// Force the device name
			QStringList devices = (QStringList() << current_device);
			if (comboBoxList(d_data->params->deviceBox()) != devices) {
				d_data->params->deviceBox()->clear();
				d_data->params->deviceBox()->addItems(devices);
			}
		}

		// set the default values if needed
		for (int i = 0; i < selected.size(); ++i) {

			if (current_pulse >= 0 && !current_camera.isEmpty()) {
				// reset pulse and camera at least
				if (selected[i]->rawData().attribute("_vip_Pulse").value<Vip_experiment_id>() != current_pulse ||
				    selected[i]->rawData().attribute("_vip_Camera").toString() != current_camera || selected[i]->rawData().attribute("_vip_Device").toString() != current_device) {
					// reset all
					selected[i]->rawData().setAttribute("_vip_Event", QString(/*"localized heat flux"*/));
					selected[i]->rawData().setAttribute("_vip_Confidence", 1);
					selected[i]->rawData().setAttribute("_vip_Comment", QString());
					selected[i]->rawData().setAttribute("_vip_Dataset", QString());
					selected[i]->rawData().setAttribute("_vip_Name", QString());
				}
				selected[i]->rawData().setAttribute("_vip_Pulse", current_pulse);
				selected[i]->rawData().setAttribute("_vip_Camera", current_camera);
				selected[i]->rawData().setAttribute("_vip_Device", current_device);
				continue;
			}

			QVariant v;
			v = selected[i]->rawData().attribute("_vip_Pulse");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Pulse", d_data->dbAccess->pulse());
			v = selected[i]->rawData().attribute("_vip_Camera");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Camera", d_data->dbAccess->camera());
			v = selected[i]->rawData().attribute("_vip_Device");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Device", d_data->dbAccess->device());
			v = selected[i]->rawData().attribute("_vip_Event");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Event", QString(/*"localized heat flux"*/)); // hotspot
			v = selected[i]->rawData().attribute("_vip_Confidence");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Confidence", 1);
			v = selected[i]->rawData().attribute("_vip_Comment");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Comment", QString());
			v = selected[i]->rawData().attribute("_vip_Dataset");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Dataset", QString());
			v = selected[i]->rawData().attribute("_vip_Name");
			if (v.userType() == 0)
				selected[i]->rawData().setAttribute("_vip_Name", QString());
		}

		// set the VipAnnotationParameters values (if possible)
		QString p = getParam(selected, "Pulse");
		QString c = getParam(selected, "Camera");
		QString d = getParam(selected, "Device");
		QString e = getParam(selected, "Event");
		QString conf = getParam(selected, "Confidence");
		QString comment = getParam(selected, "Comment");
		QString dataset = getParam(selected, "Dataset");
		QString name = getParam(selected, "Name");
		d_data->params->blockSignals(true);
		d_data->params->setCategory(e);
		d_data->params->setCamera(c);
		d_data->params->setDevice(d);
		d_data->params->setPulse(p.isEmpty() ? -1 : p.toLongLong());
		d_data->params->setConfidence(conf.isEmpty() ? -1 : conf.toDouble());
		d_data->params->setComment(comment);
		d_data->params->setDataset(dataset);
		d_data->params->setName(name);
		d_data->params->blockSignals(false);
	}
}

void VipManualAnnotation::imageTransformChanged(const QTransform& _new)
{
	// apply the image transform to the markers of all shapes
	QTransform inv = d_data->playerTr.inverted();
	QList<VipPlotShape*> shapes = player()->plotSceneModel()->shapes();
	for (int i = 0; i < shapes.size(); ++i) {
		MarkersType m = shapes[i]->rawData().attribute("_vip_markers").value<MarkersType>();
		for (MarkersType::iterator it = m.begin(); it != m.end(); ++it) {
			QPolygonF p = inv.map(it.value());
			p = _new.map(p);
			it.value() = p;
		}
		shapes[i]->rawData().setAttribute("_vip_markers", QVariant::fromValue(m));
	}
	d_data->playerTr = _new;
}

VipVideoPlayer* VipManualAnnotation::player() const
{
	return d_data->player;
}

void VipManualAnnotation::addMarker(qint64 time)
{
	d_data->drawMarkers->add(time);
}
void VipManualAnnotation::addMarker()
{
	d_data->drawMarkers->add(d_data->player->processingPool()->time());
}
/*void VipManualAnnotation::removeMarker(qint64 time)
{
	d_data->drawMarkers->remove(time);
}*/
void VipManualAnnotation::setTime(qint64 time)
{
	d_data->drawMarkers->setTime(time, false);
}
void VipManualAnnotation::clearMarkers()
{
	d_data->drawMarkers->clearMarkers();
}

void VipManualAnnotation::emitSendToDB()
{

	// check that all shapes have a valid event type, camera name and pulse number
	QList<VipPlotShape*> selected = d_data->player->plotSceneModel()->shapes(1);
	bool has_one_marker = false;

	for (int i = 0; i < selected.size(); ++i) {

		const MarkersType m = selected[i]->rawData().attribute("_vip_markers").value<MarkersType>();
		if (m.size() == 1)
			has_one_marker = true;
		else if (m.size() == 0) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to DB:</b><br>No key frame defined.");
			return;
		}

		QString event_type = selected[i]->rawData().attribute("_vip_Event").toString();
		if (event_type.isEmpty()) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to DB:</b><br>one or more shapes do not define a valid event type.");
			return;
		}
		if (selected[i]->rawData().attribute("_vip_Pulse").value<Vip_experiment_id>() <= 0) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to DB:</b><br>one or more shapes do not define a valid pulse number.");
			return;
		}
		if (selected[i]->rawData().attribute("_vip_Camera").toString().isEmpty()) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to DB:</b><br>one or more shapes do not define a valid camera name.");
			return;
		}
		if (selected[i]->rawData().attribute("_vip_Device").toString().isEmpty()) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to DB:</b><br>one or more shapes do not define a valid device name.");
			return;
		}
	}

	if (has_one_marker) {
		if (QMessageBox::question(vipGetMainWindow(),
					  "Send to DB?",
					  "A shape only has one time marker\n(start time is equal to end time).\n"
					  "Do you wish to send it anyway?",
					  QMessageBox::Yes,
					  QMessageBox::No) == QMessageBox::No)
			return;
	}

	Q_EMIT vipSendToDB();
}

void VipManualAnnotation::emitSendToJson()
{

	// check that all shapes have a valid event type, camera name and pulse number
	QList<VipPlotShape*> selected = d_data->player->plotSceneModel()->shapes(1);
	bool has_one_marker = false;

	for (int i = 0; i < selected.size(); ++i) {

		const MarkersType m = selected[i]->rawData().attribute("_vip_markers").value<MarkersType>();
		if (m.size() == 1)
			has_one_marker = true;
		else if (m.size() == 0) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to JSON:</b><br>No key frame defined.");
			return;
		}

		QString event_type = selected[i]->rawData().attribute("_vip_Event").toString();
		if (event_type.isEmpty()) {
			selected[i]->rawData().setAttribute("_vip_Event", QString("hot spot"));
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Warning:</b><br>one or more shapes do not define a valid event type.");
			// return;
		}
		// Having a pulse of 0 is now allowed
		/*if (selected[i]->rawData().attribute("_vip_Pulse").value< db_pulse_type>() <= 0) {
			QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
			QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to JSON:</b><br>one or more shapes do not define a valid pulse number.");
			return;
		}*/
		if (selected[i]->rawData().attribute("_vip_Camera").toString().isEmpty()) {

			// If no camera defined, try to use the device path as camera
			QString file;
			if (player()) {
				VipIODeviceList lst = vipListCast<VipIODevice*>(player()->mainDisplayObject()->allSources());
				if (lst.size() == 1) {
					file = lst[0]->path();
					file = lst[0]->removePrefix(file);
				}
			}
			if (file.isEmpty()) {
				QPoint pos = d_data->send->mapToGlobal(QPoint(0, 0));
				QToolTip::showText(pos - QPoint(50, 0), "<b>Cannot send to JSON:</b><br>one or more shapes do not define a valid camera name.");
				return;
			}
			selected[i]->rawData().setAttribute("_vip_Camera", file);
		}

		if (selected[i]->rawData().attribute("_vip_Device").toString().isEmpty()) {
			// TODO: better way?
			//  If no device defined, set it to WEST
			selected[i]->rawData().setAttribute("_vip_Device", "WEST");
		}
	}

	if (has_one_marker) {
		if (QMessageBox::question(vipGetMainWindow(),
					  "Send to JSON?",
					  "A shape only has one time marker\n(start time is equal to end time).\n"
					  "Do you wish to send it anyway?",
					  QMessageBox::Yes,
					  QMessageBox::No) == QMessageBox::No)
			return;
	}

	Q_EMIT sendToJson();
}

static VipVideoPlayer* currentPlayer()
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		if (VipDragWidget* drag = area->dragWidgetHandler()->focusWidget())
			if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(drag->widget()))
				return pl;
	return nullptr;
}

bool VipManualAnnotation::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::KeyPress) {
		// avoid infinit recusrion when sending event to player

		QWidget* focus = qApp->focusWidget();
		if (qobject_cast<QLineEdit*>(focus))
			return false;

		QKeyEvent* key = static_cast<QKeyEvent*>(evt);
		if (key->key() == Qt::Key_K) {
			if (currentPlayer() == d_data->player) {
				addMarker();
				return true;
			}
		}
		else if (key->key() == Qt::Key_Z && !(key->modifiers() & Qt::CTRL)) {
			if (currentPlayer() == d_data->player) {
				if (d_data->player->increaseContour())
					return true;
			}
		}
		else if (key->key() == Qt::Key_S && !(key->modifiers() & Qt::CTRL)) {
			if (currentPlayer() == d_data->player) {
				if (d_data->player->decreaseContour())
					return true;
			}
		}
		else if (key->key() == Qt::Key_N) {
			if (currentPlayer() == d_data->player) {
				d_data->player->nextSelection(key->modifiers() & Qt::CTRL);
				return true;
			}
		}
		else if (key->key() == Qt::Key_U && !(key->modifiers() & Qt::CTRL)) {
			if (currentPlayer() == d_data->player) {
				d_data->player->updateSelectedShapesFromIsoLine();
				return true;
			}
		}
	}
	return false;
}

void VipManualAnnotation::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Z && (evt->modifiers() & Qt::CTRL)) {
		VipSceneModelState::instance()->undo();
		return;
	}
	else if (evt->key() == Qt::Key_Y && (evt->modifiers() & Qt::CTRL)) {
		VipSceneModelState::instance()->undo();
		return;
	}
	else if (evt->key() == Qt::Key_Z || evt->key() == Qt::Key_S) {
		// move contour level
		qApp->sendEvent(d_data->player, evt);
		return;
	}
	else if (evt->key() == Qt::Key_N) {
		// move contour level
		qApp->sendEvent(d_data->player, evt);
		return;
	}

	evt->ignore();
}

static QByteArray rectToByteArray(const QRect& r)
{
	return QString::asprintf("%i %i %i %i", r.left(), r.top(), r.width(), r.height()).toLatin1();
}

Vip_event_list VipManualAnnotation::generateShapes(VipProgress* p, QString* error)
{
	VipTimeRange union_range;
	QList<VipPlotShape*> selected = d_data->player->plotSceneModel()->shapes(1);
	QMap<VipPlotShape*, VipTimeRange> markers = d_data->drawMarkers->timeRanges(&union_range);

	VipDisplayObject* display = player()->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (!display) {
		if (error)
			*error = "Wrong movie type!";
		return Vip_event_list();
	}

	// try to retrieve the source VipOutput this VipDisplayObject
	VipOutput* src_output = nullptr;
	if (VipInput* input = display->inputAt(0))
		if (VipConnectionPtr con = input->connection())
			if (VipOutput* source = con->source())
				src_output = source;

	// a VipProcessingPool which is not of type Resource is mandatory
	VipProcessingPool* pool = display->parentObjectPool();
	if (!pool || !src_output) {
		if (error)
			*error = "Wrong movie type!";
		return Vip_event_list();
	}

	if (pool->deviceType() == VipIODevice::Resource) {
		if (error)
			*error = "Wrong movie type!";
		return Vip_event_list();
	}

	// find all displays within this players, and all their sources
	QList<VipDisplayObject*> displays = d_data->player->displayObjects();
	QList<VipProcessingObject*> sources; // all sources
	QList<VipProcessingObject*> leafs;   // last sources before the display
	for (int i = 0; i < displays.size(); ++i) {
		if (VipOutput* src = displays[i]->inputAt(0)->connection()->source())
			if (VipProcessingObject* obj = src->parentProcessing())
				leafs.append(obj);
		sources += displays[i]->allSources();
	}
	// make sure sources are unique
	sources = vipToSet(sources).values();

	// for Temporal device only:
	pool->stop(); // stop playing
	// now, save the current VipProcessingPool state, because we are going to modify it heavily
	pool->save();

	// disable all processing except the sources, remove the Automatic flag from the sources
	pool->disableExcept(sources);
	foreach (VipProcessingObject* obj, sources) {
		obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	}

	qint64 time = union_range.first;
	qint64 end_time = union_range.second;

	if (p) {
		p->setText("Extract shape parameters...");
		p->setRange(time, end_time);
		p->setCancelable(true);
	}

	// block signals
	pool->blockSignals(true);

	Vip_event_list res;
	QString user_name = vipUserName();

	// remove image transform from player
	QTransform tr = d_data->player->imageTransform().inverted();

	QStringList status = vipAnalysisStatusDB();
	QString default_status = "Analyzed (OK)"; // status.size() ? vipAnalysisStatusDB().first() : QString("To Analyze");

	while (time != VipInvalidTime && time <= end_time) {
		if (p) {
			p->setValue(time);
			if (p->canceled())
				return res;
		}

		pool->read(time, true);

		// update all leafs
		for (int i = 0; i < leafs.size(); ++i)
			leafs[i]->update();

		for (QMap<VipPlotShape*, VipTimeRange>::const_iterator it = markers.cbegin(); it != markers.cend(); ++it) {
			if (!vipIsInside(it.value(), time))
				continue;

			MarkersType m = it.key()->rawData().attribute("_vip_markers").value<MarkersType>();

			qint64 duration = it.value().second - it.value().first;
			qint64 initial_time = it.value().first;
			qint64 last_time = it.value().second;

			// create shape
			VipShape sh(vipSimplifyPolygonDB(d_data->drawMarkers->createPolygon(time, m), VIP_DB_MAX_FRAME_POLYGON_POINTS));
			const VipNDArray ar = src_output->data().value<VipNDArray>();
			VipShapeStatistics st = sh.statistics(ar, QPoint(0, 0), nullptr, VipShapeStatistics::All);

			// reset shape initial geometry (without transform) after extracting statistics
			if (!tr.isIdentity()) {
				sh.transform(tr);
				// apply to the max and min coordinates
				st.maxPoint = tr.map(QPointF(st.maxPoint)).toPoint();
				st.minPoint = tr.map(QPointF(st.minPoint)).toPoint();
			}

			QVariantMap attrs;
			attrs.insert("comments", it.key()->rawData().attribute("_vip_Comment").toString());
			attrs.insert("name", it.key()->rawData().attribute("_vip_Name").toString());
			attrs.insert("dataset", it.key()->rawData().attribute("_vip_Dataset").toString());
			attrs.insert("experiment_id", it.key()->rawData().attribute("_vip_Pulse"));
			attrs.insert("initial_timestamp_ns", initial_time);
			attrs.insert("final_timestamp_ns", last_time);
			attrs.insert("duration_ns", duration);
			attrs.insert("is_automatic_detection", false);
			attrs.insert("method", QString("manual annotation (bbox)"));
			attrs.insert("confidence", it.key()->rawData().attribute("_vip_Confidence").toDouble());
			attrs.insert("analysis_status", default_status);
			attrs.insert("user", user_name);
			attrs.insert("line_of_sight", it.key()->rawData().attribute("_vip_Camera").toString());
			attrs.insert("device", it.key()->rawData().attribute("_vip_Device").toString());

			// set attributes from realtime table
			attrs.insert("timestamp_ns", time);
			QRect bounding = sh.boundingRect().toRect();
			attrs.insert("bbox_x", bounding.left());
			attrs.insert("bbox_y", bounding.top());
			attrs.insert("bbox_width", bounding.width());
			attrs.insert("bbox_height", bounding.height());
			attrs.insert("max_temperature_C", st.max); // TODO
			attrs.insert("max_T_image_position_x", st.maxPoint.x());
			attrs.insert("max_T_image_position_y", st.maxPoint.y());
			attrs.insert("min_temperature_C", st.min);
			attrs.insert("min_T_image_position_x", st.minPoint.x());
			attrs.insert("min_T_image_position_y", st.minPoint.y());
			attrs.insert("average_temperature_C", st.average);
			attrs.insert("pixel_area", bounding.width() * bounding.height());
			attrs.insert("centroid_image_position_x", st.maxPoint.x());
			attrs.insert("centroid_image_position_y", st.maxPoint.y());

			// set the event flag
			attrs.insert("origin", (int)VipPlayerDBAccess::New);
			sh.setAttributes(attrs);
			sh.setId((qint64)it.key());
			sh.setGroup(it.key()->rawData().attribute("_vip_Event").toString());

			res[(qint64)it.key()].append(sh);
		}

		// max = qMax(max, st.max);

		qint64 next = pool->nextTime(time);
		if (next == time || next == VipInvalidTime)
			break;

		time = next;
	}

	pool->restore();
	pool->blockSignals(false);

	// set maximum value to all shapes
	// for (int i = 0; i < res.size(); ++i)
	//	res[i].setAttribute("maximum", max);

	return res;
}
