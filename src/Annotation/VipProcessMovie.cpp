/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#include "VipProcessMovie.h"

#include "VipDisplayArea.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipPlayWidget.h"
#include "VipPlayer.h"
#include "VipPolygon.h"
#include "VipProcessingObjectEditor.h"
#include "VipProgress.h"
#include "VipSet.h"
#include "VipStandardWidgets.h"
#include "VipNetwork.h"
#include "VipSleep.h"

#include <qgridlayout.h>
#include <qmessagebox.h>
#include <qstandarditemmodel.h>

static QColor eventColor(const QString& evt_name)
{
	static QMap<QString, QColor> colors;
	if (colors.isEmpty()) {
		VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);
		QStringList evts = vipEventTypesDB();
		for (qsizetype i = 0; i < evts.size(); ++i)
			colors.insert(evts[i], palette.color(i));
	}
	QMap<QString, QColor>::const_iterator it = colors.find(evt_name);
	if (it != colors.cend())
		return it.value();
	return Qt::transparent;
}

static void drawEventTimeLine(const Vip_event_list& evts, const QList<QPointer<VipPlotShape>>& shapes, const VipTimeRangeListItem* item, QPainter* painter, const VipCoordinateSystemPtr& m)
{
	if (shapes.isEmpty())
		return;

	QMap<qint64, qint64> times;
	for (qsizetype i = 0; i < shapes.size(); ++i) {
		if (VipPlotShape* sh = shapes[i]) {
			qint64 id = sh->rawData().id();
			const VipShapeList & evt_shapes = evts[id];
			for (qsizetype s = 0; s < evt_shapes.size(); ++s)
				times.insert(evt_shapes[s].attribute("timestamp_ns").toLongLong(), 0);
		}
	}

	if (!times.isEmpty()) {
		QVector<qint64> timestamps = times.keys().toVector();
		VipTimeRangeList ranges = vipToTimeRangeList(timestamps, 25000000);

		painter->setPen(Qt::white);
		painter->setBrush(Qt::white);
		painter->setRenderHints(QPainter::RenderHints());
		painter->setOpacity(0.5);
		double bottom = item->heights().first;
		double top = item->heights().second;
		for (qsizetype i = 0; i < ranges.size(); ++i) {
			double left = ranges[i].first;
			double right = ranges[i].second;
			QRectF r(left, top, right - left, bottom - top);
			r = m->transformRect(r);
			painter->drawRect(r);
		}
	}
}

VipEventDevice::VipEventDevice(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
  , m_video_sampling(20000000)
{
	outputAt(0)->setData(QVariant::fromValue(VipSceneModel()));
}

VipEventDevice::~VipEventDevice() {}

bool VipEventDevice::open(VipIODevice::OpenModes mode)
{
	if (!(mode & VipIODevice::ReadOnly))
		return false;

	QString name = m_group;
	if (m_events.size()) {
		QVariant pulse = m_events.first().first().attribute("experiment_id");
		QVariant camera = m_events.first().first().attribute("line_of_sight");
		QString _device = m_events.first().first().attribute("device").toString();

		if (!pulse.isNull())
			name += " " + QString::number(pulse.value<Vip_experiment_id>());
		if (!camera.isNull())
			name += " " + camera.toString();
	}
	setAttribute("Name", name);
	// setAttribute("Device", _device);
	setOpenMode(mode);
	return true;
}

void VipEventDevice::setEvents(const Vip_event_list& events, const QString& group)
{
	m_scenes.clear();
	m_events = events;
	m_group = group;
	// build the scene models for each frame
	for (Vip_event_list::const_iterator it = events.begin(); it != events.end(); ++it) {
		const VipShapeList& sh = it.value();
		for (qsizetype i = 0; i < sh.size(); ++i) {
			VipShape s = sh[i];
			// set the shape name
			// s.setAttribute("Name", s.attribute("ID").toString() + " " + s.group());
			if (group.isEmpty() || group == s.group()) {
				VipSceneModel sm = m_scenes[s.attribute("timestamp_ns").toLongLong()];
				sm.add(s);
			}
		}
	}

	// set the device timestamps
	if (m_scenes.size()) {
		QList<qint64> times = m_scenes.keys();
		QVector<qint64> timestamps(times.size());
		std::copy(times.begin(), times.end(), timestamps.begin());
		// setTimestamps(timestamps);
		setTimestampsWithSampling(timestamps, m_video_sampling);

		setProperty("_vip_showTimeLine", 1);
	}
	else
		setProperty("_vip_showTimeLine", 0);

	// set the color property
	QColor c = eventColor(group);
	if (c == Qt::transparent)
		setProperty("_vip_color", QVariant());
	else
		setProperty("_vip_color", QVariant::fromValue(c));
}

Vip_event_list VipEventDevice::events() const
{
	return m_events;
}
QString VipEventDevice::group() const
{
	return m_group;
}
void VipEventDevice::setVideoSamplingTime(qint64 s)
{
	m_video_sampling = s;
}

bool VipEventDevice::readData(qint64 time)
{
	// QList<qint64> times = m_scenes.keys();
	// for (qsizetype i = 0; i < times.size(); ++i) vip_debug("%s\n", QString::number(times[i]).toLatin1().data());

	QMap<qint64, VipSceneModel>::const_iterator it = m_scenes.find(time);
	VipSceneModel sm = it != m_scenes.end() ? it.value() : VipSceneModel();
	
	// Set the minimal size
	QSize s = vipGetThermalEventDBOptions().minimumSize;
	if (s.width() == 0 && s.height() == 0) {
		VipAnyData data = create(QVariant::fromValue(sm));
		outputAt(0)->setData(data);
		return true;
	}

	VipSceneModel out = sm.copy();
	if (s.width() || s.height()) {
		auto lst = out.shapes();
		for (VipShape& sh : lst) {
			QRectF r = sh.boundingRect();
			if (r.width() < s.width() || r.height() < s.height()) {
				VipShape outsh = sh.copy();
				double x_scale = r.width() < s.width() ? ((double)s.width() / r.width() - 1.) : 0;
				double y_scale = r.height() < s.height() ? ((double)s.height() / r.height() - 1.) : 0;
				QTransform tr;
				QPointF center = r.center();
				tr.translate(center.x(), center.y()).scale(1 + x_scale, 1 + y_scale).translate(-center.x(), -center.y());
				sh.transform(tr);
			}
		}
	}

	VipAnyData data = create(QVariant::fromValue(out));
	outputAt(0)->setData(data);
	return true;
}

bool VipEventDevice::readInvalidTime(qint64 time)
{
	/*VipTimeRangeList lst = computeTimeWindow();
	if (lst.size() && (time < lst.first().first || time > lst.last().second)) {
		VipAnyData data = create(QVariant::fromValue(VipSceneModel()));
			outputAt(0)->setData(data);
			return true;
	}
	return false;*/

	// TEST: replace all <= by <

	// find event at exact time or just after
	QMap<qint64, VipSceneModel>::const_iterator it = m_scenes.lowerBound(time);
	if (it != m_scenes.end()) {

		if (it == m_scenes.begin()) {
			if (it.key() - time < m_video_sampling) {

				// First time of the event, within a one frame delay, use it
			}
			else {
				it = m_scenes.end();
			}
		}
		else {
			auto it_prev = it;
			--it_prev;
			qint64 space = it.key() - it_prev.key();
			if (space < m_video_sampling) {
				// Requested time is inbetween 2 thermal_events_instances for this event,
				// select the closest event to display
				it = (time - it_prev.key()) < (it.key() - time) ? it_prev : it;
			}
			else if ((it.key() - time) < m_video_sampling)
				; // Use lower bound (it = it)
			else if ((time - it_prev.key()) < m_video_sampling)
				it = it_prev;
			else
				it = m_scenes.end();
		}
	}
	else {
		if (m_scenes.size() && (time - m_scenes.lastKey()) < m_video_sampling) {
			// Last event time,  within a one frame delay, use it
			it = --m_scenes.end();
		}
	}

	VipSceneModel sm;
	if (it != m_scenes.end())
		sm = it.value();

	VipAnyData data = create(QVariant::fromValue(sm));
	outputAt(0)->setData(data);
	return true;
}

class MyConnection : public VipNetworkConnection
{
public:
	MyConnection(QObject* parent = nullptr)
	  : VipNetworkConnection(parent)
	{
	}

protected:
	virtual void onReadyRead()
	{
		QByteArray ar = this->readAll();
		vip_debug("%i\n", (int)ar.size());
	}
};

class VipClientEventDevice::PrivateData : public QThread
{
public:
	std::atomic<VipClientEventDevice*> parent{ nullptr };
	std::atomic<int> status{ 0 }; // 0: connecting, 1: connected, -1: failed to connect
	virtual void run()
	{
		QTcpSocket connection;
		VipClientEventDevice* dev = parent.load();
		if (!dev)
			return;

		QString path = dev->removePrefix(dev->path());
		QStringList lst = path.split(";", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		if (lst.size() != 3) {
			VIP_LOG_ERROR("Wrong path format: ", path);
			return;
		}

		QString host = lst[0];
		int port = lst[1].toInt();
		QString camera = lst[2];

		connection.connectToHost(host, port);
		if (!connection.waitForConnected(2000)) {
			status = -1;
			VIP_LOG_ERROR("Unable to connect to host ", host, ":", port);
			return;
		}
		status = 1;

		qint64 last = QDateTime::currentMSecsSinceEpoch();

		while (VipClientEventDevice* dev = parent.load()) {

			if (connection.state() != QTcpSocket::ConnectedState)
				break;

			qint64 start = QDateTime::currentMSecsSinceEpoch();

			QByteArray cmd = "GET " + camera.toLatin1() + "eof";
			vip_debug("%s\n", cmd.data());
			connection.write(cmd);
			if (!connection.waitForBytesWritten(1000)) {
				VIP_LOG_ERROR("Unable to send request to event server");
				vipSleep(100);
				continue;
			}

			// read reply
			qint64 time = QDateTime::currentMSecsSinceEpoch();
			QByteArray res;
			for (;;) {
				if (!parent.load())
					break;
				if (connection.waitForReadyRead(100))
					res += connection.readAll();

				if (res.endsWith("eof"))
					break;

				qint64 el = QDateTime::currentMSecsSinceEpoch() - time;
				if (el > 500)
					break;
			}

			vip_debug("%s\n", res.data());
			if (res.endsWith("eof"))
				res.remove(res.size() - 3, 3);

			// interpret as JSON
			Vip_event_list evts = vipEventsFromJson(res);
			vip_debug("%i events\n", (int)evts.size());

			time = QDateTime::currentMSecsSinceEpoch();
			qint64 el = time - last;
			last = time;
			vip_debug("elapsed: %i ms\n", (int)el);

			// send as output

			VipSceneModel sm;
			// build the scene models for each frame
			for (Vip_event_list::const_iterator it = evts.begin(); it != evts.end(); ++it) {
				const VipShapeList& sh = it.value();
				for (qsizetype i = 0; i < sh.size(); ++i) {
					VipShape s = sh[i];
					sm.add(s);
				}
			}

			VipAnyData any = dev->create(QVariant::fromValue(sm));
			if (evts.size() && evts.first().size())
				any.setTime(evts.first().first().attribute("timestamp_ns").toLongLong());
			else
				any.setTime(QDateTime::currentMSecsSinceEpoch() * 1000000LL);
			dev->outputAt(0)->setData(any);

			qint64 full_time = QDateTime::currentMSecsSinceEpoch() - start;
			if (full_time < 20)
				vipSleep(20 - full_time);
		}
	}
};

VipClientEventDevice::VipClientEventDevice(QObject* parent)
  : VipIODevice(parent)
  , d_data(nullptr)
{
	outputAt(0)->setData(QVariant::fromValue(VipSceneModel()));
}

VipClientEventDevice::~VipClientEventDevice()
{
	VipClientEventDevice::close();
}

bool VipClientEventDevice::open(VipIODevice::OpenModes mode)
{
	resetError();
	QString path = this->removePrefix(this->path());
	QStringList lst = path.split(";", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.size() != 3) {
		setError("Wrong path format: " + path);
		return false;
	}

	setOpenMode(mode);
	return true;
}

void VipClientEventDevice::close()
{
	if (d_data) {
		d_data->parent = nullptr;
		d_data->wait();
		d_data.reset();
	}
	VipIODevice::close();
}

bool VipClientEventDevice::enableStreaming(bool enable)
{
	// stop
	if (d_data) {
		d_data->parent = nullptr;
		d_data->wait();
		d_data.reset();
	}

	if (enable) {
		// start
		VIP_CREATE_PRIVATE_DATA(d_data);
		d_data->parent = this;
		d_data->start();

		// wait for status
		while (d_data->status.load() == 0)
			vipSleep(10);
		if (d_data->status.load() < 0) {
			d_data->parent = nullptr;
			d_data->wait();
			d_data.reset();
		}
	}

	return true;
}

class UploadToDB::PrivateData
{
public:
	QComboBox userName;
	QComboBox camera;
	QComboBox device;
	QWidget* pulse;
};

UploadToDB::UploadToDB(const QString& device, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->pulse = vipFindDeviceParameters(device)->pulseEditor();

	QGridLayout* lay = new QGridLayout();
	lay->addWidget(new QLabel("User name"), 0, 0);
	lay->addWidget(&d_data->userName, 0, 1);
	lay->addWidget(new QLabel("Camera name"), 1, 0);
	lay->addWidget(&d_data->camera, 1, 1);
	lay->addWidget(new QLabel("Device name"), 2, 0);
	lay->addWidget(&d_data->device, 2, 1);
	lay->addWidget(new QLabel("Experiment id"), 3, 0);
	lay->addWidget(d_data->pulse, 3, 1);
	setLayout(lay);

	d_data->userName.addItems(vipUsersDB());
	d_data->camera.addItems(vipCamerasDB());
	d_data->device.addItems(vipDevicesDB());
	d_data->pulse->setProperty("value", 0);

	connect(&d_data->device, SIGNAL(currentIndexChanged(int)), this, SLOT(deviceChanged()));
}
UploadToDB::~UploadToDB()
{
}

void UploadToDB::setUserName(const QString& user)
{
	d_data->userName.setCurrentText(user);
}
QString UploadToDB::userName() const
{
	return d_data->userName.currentText();
}

void UploadToDB::setCamera(const QString& cam)
{
	d_data->camera.setCurrentText(cam);
}
QString UploadToDB::camera() const
{
	return d_data->camera.currentText();
}

void UploadToDB::setDevice(const QString& dev)
{
	d_data->device.setCurrentText(dev);
}
QString UploadToDB::device() const
{
	return d_data->device.currentText();
}

void UploadToDB::deviceChanged()
{
	double pulse = this->pulse();
	QWidget* p = vipFindDeviceParameters(device())->pulseEditor();
	delete d_data->pulse;
	static_cast<QGridLayout*>(layout())->addWidget(d_data->pulse = p, 3, 1);
	setPulse(pulse);
}

void UploadToDB::setPulse(Vip_experiment_id pulse)
{
	d_data->pulse->setProperty("value", pulse);
}
Vip_experiment_id UploadToDB::pulse() const
{
	return d_data->pulse->property("value").value<Vip_experiment_id>();
}

class EventInfo::PrivateData
{
public:
	QLabel userName;
	QLabel duration;

	QComboBox category;
	VipDatasetButton dataset;
	QDoubleSpinBox confidence;
	QComboBox analysisStatus;
	QCheckBox automatic;
	QComboBox method;

	QLineEdit comment;
	QLineEdit name;
	QLineEdit mergeIds;
	QAction *apply, *close;
	QAction *interp_frames, *rm_frames, *split, *undo;
	VipPlayerDBAccess* pdb;
};
EventInfo::EventInfo(VipPlayerDBAccess* pdb)
  : QToolBar()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->pdb = pdb;
	this->setIconSize(QSize(18, 18));
	d_data->close = this->addAction(vipIcon("close.png"), "Close panel");
	this->addSeparator();

	d_data->undo = addAction(vipIcon("undo.png"), "Undo last action");
	d_data->interp_frames = addAction(vipIcon("interp_frames.png"), "Interpolate polygons inside selected time range for selected events");
	d_data->rm_frames = addAction(vipIcon("rm_frames.png"), "Remove selected time range from selected events");
	d_data->split = addAction(vipIcon("split.png"), "Split selected events based on current time");
	connect(d_data->undo, SIGNAL(triggered(bool)), this, SLOT(emitUndo()));
	connect(d_data->interp_frames, SIGNAL(triggered(bool)), this, SLOT(emitInterpFrames()));
	connect(d_data->rm_frames, SIGNAL(triggered(bool)), this, SLOT(emitRemoveFrames()));
	connect(d_data->split, SIGNAL(triggered(bool)), this, SLOT(emitSplit()));

	this->addSeparator();

	d_data->apply = this->addAction(vipIcon("apply.png"), "Apply changes");

	this->addSeparator();

	this->addWidget(new QLabel());
	this->addWidget(&d_data->userName);

	this->addWidget(new QLabel());
	this->addWidget(&d_data->duration);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->category);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->dataset);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->confidence);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->analysisStatus);

	this->addWidget(new QLabel());
	this->addWidget(&d_data->method);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->automatic);

	this->addWidget(new QLabel());
	this->addWidget(&d_data->comment);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->name);
	this->addWidget(new QLabel());
	this->addWidget(&d_data->mergeIds);

	d_data->automatic.setText("Auto");
	d_data->automatic.setToolTip("Automatic detection or not");
	d_data->automatic.setTristate(true);

	d_data->method.addItems(QStringList() << "" << vipMethodsDB());
	d_data->method.setToolTip("Detection method");

	d_data->userName.setToolTip("Author name");
	d_data->duration.setToolTip("Event duration");

	d_data->category.setToolTip("Event type");
	d_data->dataset.setToolTip("Dataset names");
	d_data->confidence.setToolTip("Detection confidence value (0 -> 1)");
	d_data->analysisStatus.setToolTip("Analysis status");
	d_data->comment.setToolTip("Additional comments");
	d_data->comment.setPlaceholderText("User comment");
	d_data->name.setToolTip("Event name");
	d_data->name.setPlaceholderText("Event name");

	d_data->mergeIds.setPlaceholderText("Merge events...");
	d_data->mergeIds.setToolTip("<b>Merge events</b><br>Enter a list of event ids to merge (like '1,45,67...')");

	d_data->category.addItems(QStringList() << "" << vipEventTypesDB());
	// d_data->category.addItems(QStringList() << "" << vipDatasetsDB());

	d_data->analysisStatus.addItems(QStringList() << QString() << vipAnalysisStatusDB());

	d_data->confidence.setRange(-0.25, 1);
	d_data->confidence.setSingleStep(0.25);
	d_data->confidence.setSpecialValueText(" ");

	connect(d_data->close, SIGNAL(triggered(bool)), this, SLOT(hide()));
	connect(d_data->apply, SIGNAL(triggered(bool)), this, SLOT(apply()));
}
EventInfo::~EventInfo()
{
}

void EventInfo::setCategory(const QString& cat)
{
	d_data->category.setCurrentText(cat);
}
QString EventInfo::category() const
{
	return d_data->category.currentText();
}

void EventInfo::setDataset(const QString& dataset)
{
	d_data->dataset.setDataset(dataset);
}
QString EventInfo::dataset() const
{
	return d_data->dataset.dataset();
}

void EventInfo::setAnalysisStatus(const QString& status)
{
	d_data->analysisStatus.setCurrentText(status);
}
QString EventInfo::analysisStatus() const
{
	return d_data->analysisStatus.currentText();
}

void EventInfo::setComment(const QString& comment)
{
	d_data->comment.setText(comment);
}
QString EventInfo::comment() const
{
	return d_data->comment.text();
}

void EventInfo::setName(const QString& name)
{
	d_data->name.setText(name);
}
QString EventInfo::name() const
{
	return d_data->name.text();
}

void EventInfo::setConfidence(double value)
{
	d_data->confidence.setValue(value);
}
double EventInfo::confidence() const
{
	return d_data->confidence.value();
}

void EventInfo::setAutomaticState(Qt::CheckState st)
{
	d_data->automatic.setCheckState(st);
}
Qt::CheckState EventInfo::automaticState() const
{
	return d_data->automatic.checkState();
}

void EventInfo::setMethod(const QString& method)
{
	d_data->method.setCurrentText(method);
}
QString EventInfo::method() const
{
	return d_data->method.currentText();
}

void EventInfo::setUserName(const QString& user)
{
	d_data->userName.setText(user);
}

void EventInfo::setDuration(double duration_s)
{
	if (vipIsNan(duration_s))
		d_data->duration.setText(QString());
	else
		d_data->duration.setText(QString::number(duration_s) + "s");
}

QList<qint64> EventInfo::mergeIds() const
{
	QString ids = d_data->mergeIds.text();
	ids.replace(",", " ");
	ids.replace(":", " ");
	ids.replace(";", " ");
	QStringList lst = ids.split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	QList<qint64> res;
	for (qsizetype i = 0; i < lst.size(); ++i) {
		qint64 id;
		bool ok = false;
		id = lst[i].toLongLong(&ok);
		if (!ok)
			return QList<qint64>();
		res.append(id);
	}
	return res;
}

void EventInfo::clearMergeIds()
{
	d_data->mergeIds.setText(QString());
}

void EventInfo::setUndoToolTip(const QString& str)
{
	d_data->undo->setToolTip(str);
}

void EventInfo::apply()
{
	Q_EMIT applied();
}

void EventInfo::emitUndo()
{
	Q_EMIT undo();
}

VipPlayerDBAccess::VipPlayerDBAccess(VipVideoPlayer* player)
  : QObject(player)
  , m_player(player)
{
	m_db = new QToolButton();
	m_db->setIcon(vipIcon("database.png"));
	m_db->setToolTip("Event DataBase options");
	m_db->setMenu(new QMenu());
	m_db->setAutoRaise(true);
	m_db->setPopupMode(QToolButton::InstantPopup);

	m_infos = new EventInfo(this);
	m_player->gridLayout()->addWidget(m_infos, 18, 10);
	m_infos->hide();

	player->toolBar()->addWidget(m_db);
	player->setProperty("VipPlayerDBAccess", true);
	connect(m_db->menu(), SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
	connect(m_player->viewer()->area(), SIGNAL(childSelectionChanged(VipPlotItem*)), this, SLOT(itemSelected(VipPlotItem*)));
	connect(m_infos, SIGNAL(applied()), this, SLOT(applyChangesToSelection()));
	connect(m_infos, SIGNAL(undo()), this, SLOT(undo()));
	connect(m_infos, SIGNAL(split()), this, SLOT(splitEvents()));
	connect(m_infos, SIGNAL(removeFrames()), this, SLOT(removeFramesToEvents()));
	connect(m_infos, SIGNAL(interpFrames()), this, SLOT(interpolateFrames()));
}

Vip_experiment_id VipPlayerDBAccess::pulse() const
{
	if (VipDisplayObject* disp = m_player->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		if (in.hasAttribute("Pulse"))
			// TOD: use stringToPulse() ?
			return (in.attribute("Pulse").value<Vip_experiment_id>());
		if (in.hasAttribute("experiment_id"))
			// TOD: use stringToPulse() ?
			return (in.attribute("experiment_id").value<Vip_experiment_id>());
		if (in.hasAttribute("Experiment_Id"))
			// TOD: use stringToPulse() ?
			return (in.attribute("Experiment_Id").value<Vip_experiment_id>());
	}
	return 0;
}
QString VipPlayerDBAccess::camera() const
{
	if (VipDisplayObject* disp = m_player->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		return in.attribute("Camera").toString();
	}
	return QString();
}
QString VipPlayerDBAccess::device() const
{
	if (VipDisplayObject* disp = m_player->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		return in.attribute("Device").toString();
	}
	return QString();
}

VipVideoPlayer* VipPlayerDBAccess::player() const
{
	return m_player;
}

void VipPlayerDBAccess::aboutToShow()
{
	// build tool button menu actions
	m_db->menu()->clear();

	// if(vipHasWriteRightsDB())
	//	connect(m_db->menu()->addAction("Detect thermal events..."), SIGNAL(triggered(bool)), this, SLOT(computeEvents()));
	connect(m_db->menu()->addAction("Show thermal events from DB..."), SIGNAL(triggered(bool)), this, SLOT(displayFromDataBase()));
	connect(m_db->menu()->addAction("Open thermal events file..."), SIGNAL(triggered(bool)), this, SLOT(displayFromJsonFile()));

	if (displayEvents().size() || m_actions.size()) {
		m_db->menu()->addSeparator();
		if (vipHasWriteRightsDB())
			connect(m_db->menu()->addAction("Upload new/modified events to DB"), SIGNAL(triggered(bool)), this, SLOT(upload()));
		if (displayEvents().size()) {
			connect(m_db->menu()->addAction("Save events to JSON..."), SIGNAL(triggered(bool)), this, SLOT(saveToJson()));
			m_db->menu()->addSeparator();
			connect(m_db->menu()->addAction(vipIcon("del.png"), "Remove thermal events from player"), SIGNAL(triggered(bool)), this, SLOT(clear()));
		}
	}

	if (m_actions.size()) {
		m_db->menu()->addSeparator();
		if (vipHasWriteRightsDB())
			connect(m_db->menu()->addAction(vipIcon("undo.png"), "Undo last change"), SIGNAL(triggered(bool)), this, SLOT(undo()));
	}

	// show/hide manual annotation widget
	if (vipHasReadRightsDB()) {
		m_db->menu()->addSeparator();
		QAction* act = m_db->menu()->addAction("Manual annotation panel");
		act->setCheckable(true);
		act->setChecked(m_annotation && m_annotation->isVisible());
		connect(act, SIGNAL(triggered(bool)), this, SLOT(showManualAnnotation(bool)));
	}

	if (m_player && m_player->processingPool() && m_player->processingPool()->deviceType() == VipIODevice::Sequential) {

		// connection to event server
		m_db->menu()->addSeparator();
		QAction* act = m_db->menu()->addAction("Connect to event server...");
		connect(act, SIGNAL(triggered(bool)), this, SLOT(connectToEventServerEdit()));
	}

	// chakib option
	// if (displayEvents().size() && ( vipUserName() == "CB246620" || vipUserName() == "VM213788")) {
	//	connect(m_db->menu()->addAction("Save events as CSV"), SIGNAL(triggered(bool)), this, SLOT(saveCSV()));
	// }
	// if ((vipUserName() == "CB246620" || vipUserName() == "VM213788") && !camera().isEmpty()) {
	//	connect(m_db->menu()->addAction("Convert ROI file to SQL"), SIGNAL(triggered(bool)), this, SLOT(roiFileToSQL()));
	// }

	// scene model display
	// loadSceneModel();
	// if (!m_plotSM.isNull()) {
	//	QAction * a = m_db->menu()->addAction("Show camera scene model");
	//	a->setCheckable(true);
	//	a->setChecked(m_plotSM->groupVisible("Scene Model"));
	//	connect(a, SIGNAL(triggered(bool)), this, SLOT(setSceneModelVisible(bool)));
	// }
}

void VipPlayerDBAccess::setSceneModelVisible(bool vis)
{
	if (m_plotSM)
		m_plotSM->setGroupVisible("Scene Model", vis);
}

void VipPlayerDBAccess::showManualAnnotation(bool vis)
{
	if (vis) {
		if (m_annotation)
			m_annotation->setVisible(true);
		else {
			m_annotation = new VipManualAnnotation(this);
			m_player->gridLayout()->addWidget(m_annotation, 19, 10);
			// m_annotation->hide();
			connect(m_annotation, SIGNAL(vipSendToDB()), this, SLOT(sendManualAnnotation()));
			connect(m_annotation, SIGNAL(sendToJson()), this, SLOT(sendManualAnnotationToJson()));
		}
	}
	else {
		if (m_annotation)
			m_annotation->deleteLater();
	}
}

void VipPlayerDBAccess::remove(qint64 id)
{
	// remove an event by adding a corresponding action (that can be undone)
	Action act;
	act.ids << id;
	act.type = Action::Remove;
	m_actions.append(act);

	QMap<qint64, qint64>::iterator it = m_modifications.find(id);
	if (it != m_modifications.end())
		it.value()++;
	else
		m_modifications[id] = 1;

	updateUndoToolTip();
	applyActions();
}

void VipPlayerDBAccess::changeSelectedPolygons()
{
	qint64 time = VipInvalidTime;
	QList<qint64> ids;
	QList<QPolygonF> polygons;

	// get player transform (if any)
	QTransform tr = m_player->imageTransform().inverted();

	for (qsizetype i = 0; i < m_selection.size(); ++i) {
		if (VipPlotShape* shape = m_selection[i]) {
			VipShape sh = shape->rawData();
			// get group, id, polygon and timestamp
			QString group = sh.group();
			qsizetype id = sh.id();
			QPolygonF p = sh.polygon();
			if (time == VipInvalidTime)
				time = sh.attribute("timestamp_ns").toLongLong();

			// get the corresponding shape in current events
			Vip_event_list::iterator it = m_events.find(id);
			if (it == m_events.end())
				continue;

			// find shape with right timestamp
			const VipShape* found = nullptr;
			const VipShapeList& shs = it.value();
			for (qsizetype s = 0; s < shs.size(); ++s) {
				if (shs[s].attribute("timestamp_ns").toLongLong() == time) {
					found = &shs[s];
					break;
				}
			}

			if (!found)
				continue;

			// make sure polygons are different
			if (found->polygon() == p)
				continue;

			p = tr.map(p);
			ids.append(found->id());
			polygons.append(p);
		}
	}

	if (ids.size()) {
		Action act;
		act.ids = ids;
		act.type = Action::ChangePolygon;
		act.time = time;
		act.polygons = polygons;
		m_actions.append(act);

		for (qsizetype i = 0; i < ids.size(); ++i) {
			qint64 id = ids[i];
			QMap<qint64, qint64>::iterator it = m_modifications.find(id);
			if (it != m_modifications.end())
				it.value()++;
			else
				m_modifications[id] = 1;
		}

		updateUndoToolTip();
		applyActions();
	}
}

void VipPlayerDBAccess::changeCategory(const QString& new_type, const QList<qint64>& ids)
{
	Action act;
	act.ids = ids;
	act.type = Action::ChangeType;
	act.value = new_type;
	m_actions.append(act);

	for (qsizetype i = 0; i < ids.size(); ++i) {
		qint64 id = ids[i];
		QMap<qint64, qint64>::iterator it = m_modifications.find(id);
		if (it != m_modifications.end())
			it.value()++;
		else
			m_modifications[id] = 1;
	}

	updateUndoToolTip();
	applyActions();
}
void VipPlayerDBAccess::changeValue(const QString& name, const QString& value, const QList<qint64>& ids)
{
	Action act;
	act.ids = ids;
	act.type = Action::ChangeValue;
	act.name = name;
	act.value = value;
	m_actions.append(act);

	for (qsizetype i = 0; i < ids.size(); ++i) {
		qint64 id = ids[i];
		QMap<qint64, qint64>::iterator it = m_modifications.find(id);
		if (it != m_modifications.end())
			it.value()++;
		else
			m_modifications[id] = 1;
	}

	updateUndoToolTip();
	applyActions();
}

void VipPlayerDBAccess::mergeIds(const QList<qint64>& ids)
{
	if (!ids.size())
		return;

	// check merge validity
	QString category = m_events[ids.first()].first().group();
	QVector<VipTimeRange> ranges;
	qsizetype count = 0;
	for (qsizetype k = 0; k < ids.size(); ++k) {
		qint64 id = ids[k];
		const VipShapeList shs = m_events[id];
		count += shs.size();
		if (shs.size()) {
			QString cat = shs.first().group();
			if (cat != category) {
				VIP_LOG_ERROR("Cannot merge events with different event types");
				return;
			}
			/*ranges.append(VipTimeRange(shs.first().attribute("timestamp").toLongLong(), shs.last().attribute("timestamp").toLongLong()));
			//check potential overlap
			bool valid = true;
			for (qsizetype l = ranges.size() - 2; l >= 0; --l) {
				if (vipIntersectRange(ranges.last(), ranges[l]) != VipInvalidTimeRange) {
					VIP_LOG_ERROR("Cannot merge ids: ROIs overlap!");
					return;
				}
			}*/
		}
	}

	if (!count) {
		VIP_LOG_ERROR("Nothing to merge!");
		return;
	}
	Action act;
	act.ids = ids;
	act.type = Action::MergeEvents;
	m_actions.append(act);

	for (qsizetype i = 0; i < ids.size(); ++i) {
		qint64 id = ids[i];
		QMap<qint64, qint64>::iterator it = m_modifications.find(id);
		if (it != m_modifications.end())
			it.value()++;
		else
			m_modifications[id] = 1;
	}

	updateUndoToolTip();
	applyActions();
	resetDrawEventTimeLine();
}

void VipPlayerDBAccess::splitEvents()
{
	QList<qint64> ids;
	qint64 time = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->time();

	for (qsizetype i = 0; i < m_selection.size(); ++i) {
		if (VipPlotShape* shape = m_selection[i]) {
			qsizetype id = shape->rawData().id();
			Vip_event_list::iterator it = m_events.find(id);
			if (it == m_events.end())
				continue;
			const VipShapeList& shs = it.value();
			if (shs.size()) {
				if (shs.first().attribute("timestamp_ns").toLongLong() < time && shs.last().attribute("timestamp_ns").toLongLong() > time) {
					ids.append(id);
				}
			}
		}
	}

	if (ids.size()) {
		Action act;
		act.ids = ids;
		act.time = time;
		act.type = Action::SplitEvents;
		m_actions.append(act);

		for (qsizetype i = 0; i < ids.size(); ++i) {
			qint64 id = ids[i];
			QMap<qint64, qint64>::iterator it = m_modifications.find(id);
			if (it != m_modifications.end())
				it.value()++;
			else
				m_modifications[id] = 1;
		}

		updateUndoToolTip();
		applyActions();
	}
	else {
		VIP_LOG_ERROR("No valid selected events to split!");
	}

	resetDrawEventTimeLine();
}

void VipPlayerDBAccess::interpolateFrames()
{
	VipTimeRange range = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->playWidget()->area()->selectionTimeRange();
	if (range.second < range.first)
		std::swap(range.first, range.second);

	if (range.first == VipInvalidTime || range.second == VipInvalidTime) {
		VIP_LOG_ERROR("Cannot remove frames: invalid time range!");
		return;
	}

	QList<qint64> ids;
	VipTimeRangeList ranges;

	for (qsizetype i = 0; i < m_selection.size(); ++i) {
		if (VipPlotShape* shape = m_selection[i]) {
			qsizetype id = shape->rawData().id();
			Vip_event_list::iterator it = m_events.find(id);
			if (it == m_events.end())
				continue;

			// find the first and last frames inside which we are going to interpolate the polygons
			/*qint64 firstValid = VipInvalidTime;
			qint64 lastValid = VipInvalidTime;
			const VipShapeList& shs = it.value();
			qsizetype frame_count = 0;
			for (qsizetype j = 0; j < shs.size(); ++j) {
				qint64 time = shs[j].attribute("timestamp").toLongLong();
				if (firstValid == VipInvalidTime && vipIsInside(range, time) && j > 0) {
					firstValid = shs[j - 1].attribute("timestamp").toLongLong();
					frame_count++;
				}
				else if (firstValid != VipInvalidTime && !vipIsInside(range, time) ) {
					lastValid = time;
					break;
				}
				else if (firstValid != VipInvalidTime) {
					++frame_count;
				}
			}
			if (firstValid == VipInvalidTime || frame_count == 0)
				continue;
			if(lastValid == VipInvalidTime)
				lastValid = shs.last().attribute("timestamp").toLongLong();
				*/
			ids.push_back(id);
			ranges.push_back(range); // VipTimeRange(firstValid, lastValid));
		}
	}

	if (ids.size()) {
		Action act;
		act.ids = ids;
		act.ranges = ranges;
		act.type = Action::InterpolateFrames;
		m_actions.append(act);

		for (qsizetype i = 0; i < ids.size(); ++i) {
			qint64 id = ids[i];
			QMap<qint64, qint64>::iterator it = m_modifications.find(id);
			if (it != m_modifications.end())
				it.value()++;
			else
				m_modifications[id] = 1;
		}

		updateUndoToolTip();
		applyActions();
	}
	else {
		VIP_LOG_ERROR("No valid selected events for polygon interpolation!");
	}

	resetDrawEventTimeLine();
}

void VipPlayerDBAccess::removeFramesToEvents()
{
	VipTimeRange range = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->playWidget()->area()->selectionTimeRange();
	if (range.second < range.first)
		std::swap(range.first, range.second);
	QList<qint64> ids;

	if (range.first == VipInvalidTime || range.second == VipInvalidTime) {
		VIP_LOG_ERROR("Cannot remove frames: invalid time range!");
		return;
	}

	for (qsizetype i = 0; i < m_selection.size(); ++i) {
		if (VipPlotShape* shape = m_selection[i]) {
			qsizetype id = shape->rawData().id();
			Vip_event_list::iterator it = m_events.find(id);
			if (it == m_events.end())
				continue;
			const VipShapeList& shs = it.value();
			if (shs.size()) {
				VipTimeRange r(shs.first().attribute("timestamp_ns").toLongLong(), shs.last().attribute("timestamp_ns").toLongLong());
				if (vipIsValid(vipIntersectRange(r, range))) {
					ids.append(id);
				}
			}
		}
	}

	if (ids.size()) {
		Action act;
		act.ids = ids;
		act.range = range;
		act.type = Action::RemoveFrames;
		m_actions.append(act);

		for (qsizetype i = 0; i < ids.size(); ++i) {
			qint64 id = ids[i];
			QMap<qint64, qint64>::iterator it = m_modifications.find(id);
			if (it != m_modifications.end())
				it.value()++;
			else
				m_modifications[id] = 1;
		}

		updateUndoToolTip();
		applyActions();
		resetDrawEventTimeLine();
	}
	else {
		VIP_LOG_ERROR("No valid selected events to split!");
	}
}

void VipPlayerDBAccess::setPulse(Vip_experiment_id p)
{
	if (VipDisplayObject* disp = m_player->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		if (!in.hasAttribute("Pulse"))
			if (VipOutput* out = disp->inputAt(0)->connection()->source())
				out->parentProcessing()->setAttribute("Pulse", p);
	}
}
void VipPlayerDBAccess::setCamera(const QString& cam)
{
	if (VipDisplayObject* disp = m_player->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		if (!in.hasAttribute("Camera"))
			if (VipOutput* out = disp->inputAt(0)->connection()->source())
				out->parentProcessing()->setAttribute("Camera", cam);
	}
}
void VipPlayerDBAccess::setDevice(const QString& name)
{
	if (VipDisplayObject* disp = m_player->mainDisplayObject()) {
		VipAnyData in = disp->inputAt(0)->probe();
		if (!in.hasAttribute("Device"))
			if (VipOutput* out = disp->inputAt(0)->connection()->source())
				out->parentProcessing()->setAttribute("Device", name);
	}
}

static void mergeEvents(Vip_event_list& evts, const QList<qint64>& ids)
{
	/**
	 * Merge several events.
	 * After this operation , these events will have the same id in evts (ids.first()), and the remaining ids will be removed.
	 *
	 * Note that events that overlap in time cannot be merged in theory.
	 * The way to handle overlapping frames is to keep only one polygon out of all ids. The kept polygon is always the first possible polygon in ids.
	 */

	QMap<qint64, VipShape> shapes; // map of time -> shape
	for (qsizetype k = 0; k < ids.size(); ++k) {
		qint64 id = ids[k];
		VipShapeList shs = evts[id];
		for (qsizetype i = 0; i < shs.size(); ++i) {
			qint64 time = shs[i].attribute("timestamp_ns").toLongLong();
			if (shapes.find(time) == shapes.end()) {
				shapes.insert(time, shs[i]);
				shs[i].setId(ids.first());
			}
		}
	}

	evts[ids.first()] = VipShapeList(shapes.values().toVector());
	for (qsizetype i = 1; i < ids.size(); ++i)
		evts.remove(ids[i]);
}

Vip_event_list VipPlayerDBAccess::applyActions(const Vip_event_list& events)
{

	Vip_event_list res = vipCopyEvents(events);
	for (qsizetype i = 0; i < m_actions.size(); ++i) {
		Action act = m_actions[i];
		if (act.type == Action::Remove) {
			// remove id from events
			for (qsizetype k = 0; k < act.ids.size(); ++k)
				res.remove(act.ids[k]);
		}
		else if (act.type == Action::ChangeType) {
			for (qsizetype k = 0; k < act.ids.size(); ++k) {
				qint64 id = act.ids[k];
				VipShapeList shs = res[id];
				for (qsizetype j = 0; j < shs.size(); ++j)
					shs[j].setGroup(act.value);
				res[id] = shs;
			}
		}
		else if (act.type == Action::ChangeValue) {
			for (qsizetype k = 0; k < act.ids.size(); ++k) {
				qint64 id = act.ids[k];
				VipShapeList shs = res[id];
				for (qsizetype j = 0; j < shs.size(); ++j) {
					QVariant v = shs[j].attribute(act.name);
					QVariant val(act.value);
					if (v.userType())
						val.convert(VIP_META(v.userType()));
					shs[j].setAttribute(act.name, val);
				}
			}
		}
		else if (act.type == Action::MergeEvents) {

			mergeEvents(res, act.ids);
		}
		else if (act.type == Action::ChangePolygon) {
			for (qsizetype k = 0; k < act.ids.size(); ++k) {
				qint64 id = act.ids[k];
				VipShapeList& shs = res[id];
				// find time
				for (qsizetype s = 0; s < shs.size(); ++s) {
					if (shs[s].attribute("timestamp_ns").toLongLong() == act.time) {
						shs[s].setPolygon(act.polygons[k]);
						break;
					}
				}
			}
		}
		else if (act.type == Action::SplitEvents) {
			for (qsizetype k = 0; k < act.ids.size(); ++k) {
				qint64 id = act.ids[k];
				VipShapeList& shs = res[id];
				// find time
				for (qsizetype s = 0; s < shs.size(); ++s) {
					if (shs[s].attribute("timestamp_ns").toLongLong() > act.time) {
						VipShapeList news = shs.mid(s);
						res[id] = shs.mid(0, s);
						qsizetype new_id = res.lastKey() + 1;
						// set new id
						for (qsizetype n = 0; n < news.size(); ++n)
							news[n].setId(new_id);
						res[new_id] = news;
						break;
					}
				}
			}
		}
		else if (act.type == Action::RemoveFrames) {
			for (qsizetype k = 0; k < act.ids.size(); ++k) {
				qint64 id = act.ids[k];
				VipShapeList& shs = res[id];
				for (qsizetype s = 0; s < shs.size(); ++s) {
					qint64 time = shs[s].attribute("timestamp_ns").toLongLong();
					if (time >= act.range.first && time <= act.range.second) {
						shs.removeAt(s);
						--s;
					}
				}
			}
		}
		else if (act.type == Action::InterpolateFrames) {
			for (qsizetype k = 0; k < act.ids.size(); ++k) {
				qint64 id = act.ids[k];
				VipTimeRange range = act.ranges[k];
				VipShapeList& shs = res[id];

				// get a map of time -> polygon
				QMap<qint64, VipShape> polygons;
				for (qsizetype s = 0; s < shs.size(); ++s) {
					polygons[shs[s].attribute("timestamp_ns").toLongLong()] = shs[s];
				}

				// get the source IR device
				VipIODevice* dev = vipListCast<VipIODevice*>(m_player->mainDisplayObject()->allSources()).first();

				// get time before and after
				qint64 start_t = dev->previousTime(range.first);
				if (start_t == VipInvalidTime)
					start_t = range.first;
				qint64 end_t = dev->nextTime(range.second);
				if (end_t == VipInvalidTime)
					end_t = range.second;

				// save the first shape for its attributes
				VipShape first = polygons.first().copy();
				// find start and end polygon
				QPolygonF start, end;
				if (range.first <= polygons.firstKey())
					start = polygons.first().polygon();
				else if (range.first >= polygons.lastKey())
					start = polygons.last().polygon();
				else {
					QMap<qint64, VipShape>::iterator it = polygons.upperBound(range.first - 1);
					--it;
					start = it.value().polygon();
				}
				if (range.second <= polygons.firstKey())
					end = polygons.first().polygon();
				else if (range.second >= polygons.lastKey())
					end = polygons.last().polygon();
				else {
					QMap<qint64, VipShape>::iterator it = polygons.upperBound(range.second + 1);
					end = it.value().polygon();
				}

				// expand to device first time if range.first == dev->firstTime()
				if (start_t == dev->firstTime()) {
					VipShape tmp = first.copy();
					tmp.setAttribute("timestamp_ns", start_t);
					tmp.setPolygon(start);
					polygons.insert(start_t, tmp);
				}
				if (end_t == dev->lastTime()) {
					VipShape tmp = first.copy();
					tmp.setAttribute("timestamp_ns", end_t);
					tmp.setPolygon(end);
					polygons.insert(end_t, tmp);
				}
				// get time
				qint64 time = dev->nextTime(start_t);
				do {
					double advance = (time - start_t) / (double)(end_t - start_t);
					QPolygonF p = vipInterpolatePolygons(start, end, advance);
					p = vipSimplifyPolygonDB(p, VIP_DB_MAX_FRAME_POLYGON_POINTS);
					VipShape tmp = first.copy();
					tmp.setPolygon(p);
					tmp.setAttribute("timestamp_ns", time);
					polygons.insert(time, tmp);
					time = dev->nextTime(time);
				} while (time != VipInvalidTime && time < end_t);

				shs = polygons.values().toVector();
			}
		}
	}
	return res;
}

bool VipPlayerDBAccess::connectToEventServer(const QString& host)
{
	if (!m_player)
		return false;

	VipClientEventDevice* dev = new VipClientEventDevice();
	dev->setPath(host);
	if (!dev->open(VipIODevice::ReadOnly)) {
		VIP_LOG_ERROR(dev->errorString());
		delete dev;
		return false;
	}

	if (m_sequential_events) {
		m_sequential_events->close();
		delete m_sequential_events;
	}

	m_sequential_events = dev;
	m_sequential_events->setParent(m_player->processingPool());
	if (!m_player->processingPool()->isStreamingEnabled())
		m_player->processingPool()->startStreaming();
	if (vipCreatePlayersFromProcessings(QList<VipProcessingObject*>() << m_sequential_events.data(), m_player.data()).size() == 0) {
		VIP_LOG_ERROR("Unable to display events on player");
		return false;
	}
	return true;
}
void VipPlayerDBAccess::connectToEventServerEdit()
{
	QLineEdit* edit = new QLineEdit();
	edit->setMinimumWidth(200);
	edit->setPlaceholderText("Format: 'host:ip:optional_camera'");
	VipGenericDialog dial(edit, "Connect to event server", m_player);
	if (dial.exec() == QDialog::Accepted) {

		QStringList lst = edit->text().split(":", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		QString path;
		if (lst.size() == 2) {
			path = lst[0] + ";" + lst[1] + ";" + this->camera();
		}
		else if (lst.size() == 3) {
			path = lst[0] + ";" + lst[1] + ";" + lst[2];
		}
		else {
			VIP_LOG_ERROR("Wrong connection format: ", edit->text());
			return;
		}
		connectToEventServer(path);
	}
}

void VipPlayerDBAccess::updateUndoToolTip()
{
	if (m_actions.size()) {

		QString tp = "<b>Undo last action</b>";
		const Action& last = m_actions.last();
		if (last.type == Action::Remove) {
			tp += "<br>(Remove events)";
		}
		else if (last.type == Action::ChangeType) {
			tp += "<br>(Change event type)";
		}
		else if (last.type == Action::ChangeValue) {
			tp += "<br>(Change event attribute:" + last.name + ")";
		}
		else if (last.type == Action::MergeEvents) {
			tp += "<br>(Merge events)";
		}
		else if (last.type == Action::SplitEvents) {
			tp += "<br>(Split events)";
		}
		else if (last.type == Action::RemoveFrames) {
			tp += "<br>(Remove frames from events)";
		}
		else if (last.type == Action::ChangePolygon) {
			tp += "<br>(Change event polygon)";
		}
		else if (last.type == Action::InterpolateFrames) {
			tp += "<br>(Interpolate polygons)";
		}

		m_infos->setUndoToolTip(tp);
	}
	else
		m_infos->setUndoToolTip("<b>Undo last action</b>");
}

void VipPlayerDBAccess::undo()
{
	if (m_actions.size()) {
		const QList<qint64> ids = m_actions.last().ids;
		for (qsizetype i = 0; i < ids.size(); ++i) {
			qint64 id = ids[i];
			QMap<qint64, qint64>::iterator it = m_modifications.find(id);
			if (it != m_modifications.end())
				if (--it.value() == 0) {
					m_modifications.erase(it);
				}
		}
		m_actions.pop_back();
		applyActions();
		// reset info panel
		itemSelected(m_selected_item);
	}

	updateUndoToolTip();
}

void VipPlayerDBAccess::applyActions()
{
	m_events = applyActions(m_initial_events);
	// get groups
	QSet<QString> groups;
	for (Vip_event_list::const_iterator it = m_events.begin(); it != m_events.end(); ++it)
		if (it.value().size())
			groups.insert(it.value().first().group());

	// apply the changes for each VipEventDevice (sorted by group).
	// Create a new VipEventDevice if the group is new.

	for (QSet<QString>::const_iterator it = groups.constBegin(); it != groups.constEnd(); ++it) {
		VipEventDevice* dev = device(*it);
		if (!dev)
			createDevice(m_events, *it);
		else
			dev->setEvents(m_events, *it);
	}

	// remove devices and displays with a group that does not exists anymore
	QList<VipDisplayObject*> disps = this->displayEvents();
	QList<VipEventDevice*> devs = this->devices();
	for (qsizetype i = 0; i < devs.size(); ++i) {
		if (groups.find(devs[i]->group()) == groups.end()) {
			disps[i]->deleteLater();
			devs[i]->deleteLater();
		}
	}

	// reload events
	m_player->processingPool()->reload();
}

void VipPlayerDBAccess::upload()
{
	uploadInternal(true);
}
void VipPlayerDBAccess::uploadNoMessage()
{
	uploadInternal(false);
}

void VipPlayerDBAccess::saveToJson()
{
	saveToJsonInternal(true);
}

/*static QByteArray rectToByteArray(const QRect& r)
{
	return QString::asprintf("%i %i %i %i", r.left(), r.top(), r.width(), r.height()).toLatin1();
}*/

void VipPlayerDBAccess::saveToJsonInternal(bool show_messages)
{
	QString filename = VipFileDialog::getSaveFileName(vipGetMainWindow(), "Create JSON file", "JSON file (*.json)");
	if (filename.isEmpty())
		return;

	if (m_events.size()) {
		// find PPO, pulse, camera
		QString PPO = m_events.first().first().attribute("user").toString();
		QString camera = m_events.first().first().attribute("line_of_sight").toString();
		QString device = m_events.first().first().attribute("device").toString();
		// Vip_experiment_id pulse = m_events.first().first().attribute("experiment_id").value< Vip_experiment_id>();

		if (PPO.isEmpty()) {
			if (show_messages)
				QMessageBox::warning(nullptr, "Warning", "Invalid user name");
			VIP_LOG_WARNING("Invalid user name");
			return;
		}
		if (camera.isEmpty()) {
			if (show_messages)
				QMessageBox::warning(nullptr, "Warning", "Invalid camera name");
			VIP_LOG_WARNING("Invalid camera name");
			return;
		}
		if (device.isEmpty()) {
			if (show_messages)
				QMessageBox::warning(nullptr, "Warning", "Invalid device name");
			VIP_LOG_WARNING("Invalid device name");
			return;
		}

		// At this point, we might need to recompute the temperature stats inside some events.
		// This is true if polygons has been modified or interpolated.
		// Therefore, fins event ids with this kind of modifications
		QSet<qint64> ids;
		for (qsizetype i = 0; i < m_actions.size(); ++i) {
			if (m_actions[i].type == Action::ChangePolygon || m_actions[i].type == Action::InterpolateFrames) {
				ids.unite(vipToSet(m_actions[i].ids));
			}
		}
		QList<QPair<VipDisplaySceneModel*, QString>> to_recompute;
		QList<qint64> to_recomputeIds;
		// for each event id, find the VipDisplaySceneModel
		QList<VipDisplayObject*> displays = displayEvents();
		QList<VipEventDevice*> events = this->devices();
		for (QSet<qint64>::const_iterator it = ids.begin(); it != ids.end(); ++it) {
			qint64 id = *it;
			// find the group
			QString group = m_events[id].first().group();
			// find the corresponding VipEventDevice
			VipDisplaySceneModel* disp = nullptr;
			for (qsizetype i = 0; i < events.size(); ++i) {
				if (events[i]->group() == group) {
					disp = qobject_cast<VipDisplaySceneModel*>(displays[i]);
					break;
				}
			}
			if (disp) {
				to_recompute.push_back(QPair<VipDisplaySceneModel*, QString>(disp, group + ":" + QString::number(id)));
				to_recomputeIds.push_back(id);
			}
		}

		VipProgress p;

		if (to_recompute.size()) {

			p.setText("Recompute temporal statistics for modified events...");

			// recompute stats
			QList<VipProcessingObject*> stats = m_player->extractTimeEvolution(to_recompute, VipShapeStatistics::Minimum | VipShapeStatistics::Maximum | VipShapeStatistics::Mean, 1, 2);
			qsizetype c = 0;
			for (qsizetype i = 0; i < to_recomputeIds.size(); ++i) {
				VipAnyResource* max = static_cast<VipAnyResource*>(stats[c++]);
				vip_debug("%s\n", max->path().toLatin1().data());
				VipAnyResource* min = static_cast<VipAnyResource*>(stats[c++]);
				vip_debug("%s\n", min->path().toLatin1().data());
				VipAnyResource* mean = static_cast<VipAnyResource*>(stats[c++]);
				vip_debug("%s\n", mean->path().toLatin1().data());

				VipPointVector max_vals = max->outputAt(0)->value<VipPointVector>();
				VipPointVector max_vals_pos = max->outputAt(0)->data().attribute("_vip_Pos").value<VipPointVector>();

				VipPointVector min_vals = min->outputAt(0)->value<VipPointVector>();
				VipPointVector min_vals_pos = min->outputAt(0)->data().attribute("_vip_Pos").value<VipPointVector>();

				VipPointVector mean_vals = mean->outputAt(0)->value<VipPointVector>();

				VipShapeList& shs = m_events[to_recomputeIds[i]];

				// transform max and min positions based on player transform
				QTransform tr = m_player->imageTransform().inverted();
				max_vals_pos = tr.map(max_vals_pos.toPointF());
				min_vals_pos = tr.map(min_vals_pos.toPointF());

				if (max_vals.size() == max_vals_pos.size() && max_vals.size() == min_vals.size() && max_vals.size() == min_vals_pos.size() && max_vals.size() == mean_vals.size() &&
				    max_vals.size() == shs.size()) {

					for (qsizetype j = 0; j < max_vals.size(); ++j) {
						VipShape& sh = shs[j];
						sh.setAttribute("max_temperature_C", max_vals[j].y()); // TODO
						sh.setAttribute("max_T_image_position_x", qRound(max_vals_pos[j].x()));
						sh.setAttribute("max_T_image_position_y", qRound(max_vals_pos[j].y()));
						sh.setAttribute("min_temperature_C", min_vals[j].y());
						sh.setAttribute("min_T_image_position_x", qRound(min_vals_pos[j].x()));
						sh.setAttribute("min_T_image_position_y", qRound(min_vals_pos[j].y()));
						sh.setAttribute("average_temperature_C", mean_vals[j].y());
					}
				}
			}
		}

		if (m_events.size() &&
		    !vipEventsToJsonFile(filename, m_events, &p)) { //! sendToJSON(filename,PPO, camera,device, pulse, m_events, before_send_list_type() << computeEventPolygons, &p)) {
			if (show_messages)
				QMessageBox::warning(nullptr, "Warning", "Failed to create JSON file!");
			VIP_LOG_WARNING("Failed to create JSON file!");
			return;
		}
	}
}

void VipPlayerDBAccess::uploadInternal(bool show_messages)
{
	Vip_event_list to_send;
	QList<qint64> to_remove_from_DB;

	// TEST
	/*std::map<qint64, qint64> modifs;
	for (QMap<qint64, qint64>::const_iterator it = m_modifications.begin(); it != m_modifications.end(); ++it)
		modifs[it.key()] = it.value();

	std::map<qint64, EventFlag> evts;
	for (Vip_event_list::const_iterator it = m_events.begin(); it != m_events.end(); ++it)
		evts[it.key()] = (EventFlag)it.value().first().attribute("origin").toInt();
	bool stop = true;

	*/

	// compute the list of ids that needs to be removed from the DB
	for (QMap<qint64, qint64>::const_iterator it = m_modifications.begin(); it != m_modifications.end(); ++it) {
		if (m_initial_events.find(it.key()) != m_initial_events.end()) {
			// get flag
			int origin = m_initial_events[it.key()].first().attribute("origin").toInt();
			if (origin == DB) {
				// compute the actual ID in the database
				if (qint64 DB_id = m_initial_events[it.key()].first().attribute("id").toLongLong())
					if (to_remove_from_DB.indexOf(DB_id) < 0)
						to_remove_from_DB.append(DB_id);
			}
		}
	}

	// compute the list of events that needs to be sent
	for (Vip_event_list::const_iterator it = m_events.begin(); it != m_events.end(); ++it) {
		if (m_modifications.find(it.key()) != m_modifications.end() && it.value().first().attribute("confidence").toDouble() > 0)
			// this event was modified, we need to send it
			to_send.insert(it.key(), it.value());
		else if (it.value().first().attribute("origin").toInt() == New && it.value().first().attribute("confidence").toDouble() > 0) {
			// this event is new (computed just now), we need to send it
			to_send.insert(it.key(), it.value());
		}
	}

	{
		if (to_remove_from_DB.isEmpty() && to_send.isEmpty()) {
			if (show_messages)
				QMessageBox::information(nullptr, "Uploading", "No modifications to upload!");
			VIP_LOG_INFO("No modifications to upload!");
			return;
		}

		// send all events

		if (m_events.size()) {
			// find PPO, pulse, camera
			QString PPO = m_events.first().first().attribute("user").toString();
			QString camera = m_events.first().first().attribute("line_of_sight").toString();
			QString device = m_events.first().first().attribute("device").toString();
			Vip_experiment_id pulse = m_events.first().first().attribute("experiment_id").value<Vip_experiment_id>();

			if (PPO.isEmpty()) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Invalid user name");
				VIP_LOG_WARNING("Invalid user name");
				return;
			}
			if (camera.isEmpty()) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Invalid camera name");
				VIP_LOG_WARNING("Invalid camera name");
				return;
			}
			if (device.isEmpty()) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Invalid device name");
				VIP_LOG_WARNING("Invalid device name");
				return;
			}
			if (pulse <= 0) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Invalid experiment id value");
				VIP_LOG_WARNING("Invalid experiment id value");
				return;
			}

			// At this point, we might need to recompute the temperature stats inside some events.
			// This is true if polygons has been modified or interpolated.
			// Therefore, find event ids with this kind of modifications
			QSet<qint64> ids;
			for (qsizetype i = 0; i < m_actions.size(); ++i) {
				if (m_actions[i].type == Action::ChangePolygon || m_actions[i].type == Action::InterpolateFrames) {
					ids.unite(vipToSet(m_actions[i].ids));
				}
			}
			QList<QPair<VipDisplaySceneModel*, QString>> to_recompute;
			QList<qint64> to_recomputeIds;
			// for each event id, find the VipDisplaySceneModel
			QList<VipDisplayObject*> displays = displayEvents();
			QList<VipEventDevice*> events = this->devices();
			for (QSet<qint64>::const_iterator it = ids.begin(); it != ids.end(); ++it) {
				qint64 id = *it;
				if (to_send.find(id) == to_send.end())
					continue;
				// find the group
				QString group = to_send[id].first().group();
				// find the corresponding VipEventDevice
				VipDisplaySceneModel* disp = nullptr;
				for (qsizetype i = 0; i < events.size(); ++i) {
					if (events[i]->group() == group) {
						disp = qobject_cast<VipDisplaySceneModel*>(displays[i]);
						break;
					}
				}
				if (disp) {
					to_recompute.push_back(QPair<VipDisplaySceneModel*, QString>(disp, group + ":" + QString::number(id)));
					to_recomputeIds.push_back(id);
				}
			}

			VipProgress p;

			if (to_recompute.size()) {

				p.setText("Recompute temporal statistics for modified events...");

				// recompute stats
				QList<VipProcessingObject*> stats =
				  m_player->extractTimeEvolution(to_recompute, VipShapeStatistics::Minimum | VipShapeStatistics::Maximum | VipShapeStatistics::Mean, 1, 2);
				qsizetype c = 0;
				for (qsizetype i = 0; i < to_recomputeIds.size(); ++i) {
					VipAnyResource* max = static_cast<VipAnyResource*>(stats[c++]);
					vip_debug("%s\n", max->path().toLatin1().data());
					VipAnyResource* min = static_cast<VipAnyResource*>(stats[c++]);
					vip_debug("%s\n", min->path().toLatin1().data());
					VipAnyResource* mean = static_cast<VipAnyResource*>(stats[c++]);
					vip_debug("%s\n", mean->path().toLatin1().data());

					VipPointVector max_vals = max->outputAt(0)->value<VipPointVector>();
					VipPointVector max_vals_pos = max->outputAt(0)->data().attribute("_vip_Pos").value<VipPointVector>();

					VipPointVector min_vals = min->outputAt(0)->value<VipPointVector>();
					VipPointVector min_vals_pos = min->outputAt(0)->data().attribute("_vip_Pos").value<VipPointVector>();

					VipPointVector mean_vals = mean->outputAt(0)->value<VipPointVector>();

					VipShapeList& shs = to_send[to_recomputeIds[i]];

					// transform max and min positions based on player transform
					QTransform tr = m_player->imageTransform().inverted();
					max_vals_pos = tr.map(max_vals_pos.toPointF());
					min_vals_pos = tr.map(min_vals_pos.toPointF());

					if (max_vals.size() == max_vals_pos.size() && max_vals.size() == min_vals.size() && max_vals.size() == min_vals_pos.size() &&
					    max_vals.size() == mean_vals.size() && max_vals.size() == shs.size()) {

						for (qsizetype j = 0; j < max_vals.size(); ++j) {
							VipShape& sh = shs[j];
							sh.setAttribute("max_temperature_C", max_vals[j].y()); // TODO
							sh.setAttribute("max_T_image_position_x", qRound(max_vals_pos[j].x()));
							sh.setAttribute("max_T_image_position_y", qRound(max_vals_pos[j].y()));
							sh.setAttribute("min_temperature_C", min_vals[j].y());
							sh.setAttribute("min_T_image_position_x", qRound(min_vals_pos[j].x()));
							sh.setAttribute("min_T_image_position_y", qRound(min_vals_pos[j].y()));
							sh.setAttribute("average_temperature_C", mean_vals[j].y());
						}
					}
				}
			}

			p.setText("Remove modified events from DB...");

			// remove from db
			if (to_remove_from_DB.size() && !vipRemoveFromDB(to_remove_from_DB)) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Unable to remove events from DB");
				VIP_LOG_WARNING("Unable to remove events from DB");
				return;
			}
			if (to_remove_from_DB.size() > 1)
				VIP_LOG_INFO(to_remove_from_DB.size(), " events removed from DB");
			else if (to_remove_from_DB.size() == 1)
				VIP_LOG_INFO(to_remove_from_DB.size(), " event removed from DB");

			p.setText("Send events to DB...");
			if (to_send.size() && vipSendToDB(PPO, camera, device, pulse, to_send).size() == 0) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Failed to upload events!");
				VIP_LOG_WARNING("Failed to upload events!");
				return;
			}

			if (to_send.size() > 1)
				VIP_LOG_INFO(to_send.size(), " events sent to DB");
			else if (to_send.size() == 1)
				VIP_LOG_INFO(to_send.size(), " event sent to DB");
		}
		else {
			// remove from db
			if (to_remove_from_DB.size() && !vipRemoveFromDB(to_remove_from_DB)) {
				if (show_messages)
					QMessageBox::warning(nullptr, "Warning", "Unable to remove events from DB");
				VIP_LOG_WARNING("Unable to remove events from DB");
				return;
			}
			if (to_remove_from_DB.size() > 1)
				VIP_LOG_INFO(to_remove_from_DB.size(), " events removed from DB");
			else if (to_remove_from_DB.size() == 1)
				VIP_LOG_INFO(to_remove_from_DB.size(), " event removed from DB");
		}

		Vip_event_list tmp = m_events;
		clear();
		// m_initial_events = m_events = tmp;
		// showEvents();
		addEvents(tmp, true);

		return;
	}
}

void VipPlayerDBAccess::displayFromJsonFile()
{
	QString filename = VipFileDialog::getOpenFileName(vipGetMainWindow(), "Open JSON file", "JSON file (*.json)");
	if (filename.isEmpty())
		return;

	QFile fin(filename);
	if (!fin.open(QFile::ReadOnly)) {
		QMessageBox::warning(nullptr, "Warning", "Failed to open JSON file!");
		return;
	}

	QByteArray ar = fin.readAll();
	Vip_event_list evts = vipEventsFromJson(ar);
	if (!evts.size()) {
		QMessageBox::warning(nullptr, "Warning", "Unable to load events from JSON file!");
		return;
	}

	addEvents(evts, false);
}

void VipPlayerDBAccess::displayFromDataBaseQuery(const VipEventQuery& query, bool clear_previous)
{
	VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);

	VipEventQueryResults res = vipQueryDB(query, &progress);
	if (!res.isValid()) {
		QMessageBox::warning(nullptr, "Warning", "Failed to retrieve events!");
		return;
	}

	VipFullQueryResult fres = vipFullQueryDB(res, &progress);
	if (!fres.isValid()) {
		QMessageBox::warning(nullptr, "Warning", "Failed to retrieve events!");
		return;
	}

	progress.setText("Update visual events...");

	// clear previous events
	if (clear_previous)
		clear();

	res = fres.result[this->pulse()].cameras[this->camera()].events;

	Vip_event_list result;
	for (QMap<qint64, VipEventQueryResult>::const_iterator it = res.events.cbegin(); it != res.events.cend(); ++it) {
		const VipEventQueryResult evt = it.value();
		if (evt.shapes.size())
			result[it.key()] = evt.shapes;
	}
	addEvents(result, true);
}

void VipPlayerDBAccess::displayFromDataBase()
{
	VipQueryDBWidget* db = new VipQueryDBWidget(this->device());
	db->enableAllCameras(false);
	db->enableAllDevices(false);
	db->enablePulseRange(false);
	// db->setRemovePreviousVisible(true);
	// db->setRemovePrevious(true);

	QString camera = this->camera();
	if (!camera.isEmpty())
		db->setCamera(camera);

	QString device = this->device();
	if (!device.isEmpty())
		db->setDevice(device);

	if (pulse() >= 0)
		db->setPulse(pulse());

	VipGenericDialog dial(db, "Search events");
	if (dial.exec() != QDialog::Accepted)
		return;

	// set the pulse and camera if not already available
	setPulse(db->pulseRange().first);
	setCamera(db->camera());
	setDevice(db->device());

	VipEventQuery query;
	query.automatic = db->automatic();
	if (db->camera().size())
		query.cameras << db->camera();
	if (db->device().size())
		query.devices << db->device();
	if (db->thermalEvent().size())
		query.event_types << db->thermalEvent();
	query.in_comment = db->inComment();
	query.in_name = db->inName();
	query.min_duration = db->durationRange().first;
	query.max_duration = db->durationRange().second;
	query.min_temperature = db->maxTemperatureRange().first;
	query.max_temperature = db->maxTemperatureRange().second;
	query.min_confidence = db->minConfidence();
	query.max_confidence = db->maxConfidence();
	query.dataset = db->dataset();
	query.method = db->method();
	if (db->userName().size())
		query.users << db->userName();

	query.min_pulse = db->pulseRange().first;
	query.max_pulse = db->pulseRange().second;

	displayFromDataBaseQuery(query, true);
	/*VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);

	VipEventQueryResults res = vipQueryDB(query, &progress);
	if (!res.isValid()) {
		QMessageBox::warning(nullptr, "Warning", "Failed to retrieve events!");
		return;
	}

	FullQuery fquery;
	VipFullQueryResult fres = vipQueryDB(fquery, res, &progress);
	if (!fres.isValid()) {
		QMessageBox::warning(nullptr, "Warning", "Failed to retrieve events!");
		return;
	}

	progress.setText("Update visual events...");

	//clear previous events
	clear();

	res = fres.result[db->pulseRange().first].cameras[db->camera()].events;

	Vip_event_list result;
	for (QMap<qint64, Event>::const_iterator it = res.events.cbegin(); it != res.events.cend(); ++it)
	{
		const Event evt = it.value();
		if (evt.shapes.size())
			result[it.key()] = evt.shapes;
	}
	addEvents(result,true);
*/
}

void VipPlayerDBAccess::addEvents(const Vip_event_list& events, bool fromDB)
{
	// add new events.
	// update m_initial_events
	// update devices (create new ones if new group)
	// set the right flag to events (New or DB)
	// set the right id to events (from DB, keep ids and set ThermaEventInfo_ID, overwise use last id(group) +1)
	// update display

	QSet<QString> groups;

	qint64 start_id = m_initial_events.size() ? m_initial_events.lastKey() + 1 : 1;
	for (Vip_event_list::const_iterator it = events.begin(); it != events.end(); ++it) {
		// update list of groups
		groups.insert(it.value().first().group());

		// set the right flag
		VipShape(it.value().first()).setAttribute("origin", fromDB ? (int)DB : (int)New);

		// get the device for the group
		//  NEW: do it after this loop or we might end up with id collisions
		// VipEventDevice * dev = device(it.value().first().group());
		// if (!dev) {
		//	dev = createDevice(events, it.value().first().group());
		// }

		// insert in m_initial_events with the new id
		m_initial_events[start_id] = it.value();
		// set the id to all shapes
		VipShapeList& lst = m_initial_events[start_id];
		for (qsizetype i = 0; i < lst.size(); ++i)
			lst[i].setId(start_id);
		++start_id;
	}

	// build devices for each group
	for (auto it = groups.begin(); it != groups.end(); ++it) {
		VipEventDevice* dev = device(*it);
		if (!dev) {
			createDevice(m_initial_events, *it);
		}
	}

	// reapply actions
	applyActions();
}

VipEventDevice* VipPlayerDBAccess::createDevice(const Vip_event_list& events, const QString& group)
{
	// First, retrieve the underlying video sampling time
	qint64 sampling = 0;
	QList<VipIODevice*> devices = vipListCast<VipIODevice*>(m_player->mainDisplayObject()->allSources());
	if (devices.size() == 1) {
		VipIODevice* d = devices.first();
		if (d->deviceType() == VipIODevice::Temporal && d->size() > 1) {
			qint64 first = d->firstTime();
			qint64 second = d->nextTime(first);
			sampling = second - first;
			if (sampling < 0)
				sampling = 0;
		}
	}

	// create processing pipeline to display the scene models
	VipEventDevice* dev = new VipEventDevice(m_player->processingPool());
	dev->setVideoSamplingTime(sampling);
	dev->setEvents(events, group);
	dev->open(VipIODevice::ReadOnly);
	if (VipDisplayObject* disp = vipCreateDisplayFromData(dev->outputAt(0)->data(), m_player)) {
		disp->setParent(m_player->processingPool());
		dev->setDeleteOnOutputConnectionsClosed(true);
		dev->outputAt(0)->setConnection(disp->inputAt(0));
		disp->setScheduleStrategy(VipDisplayObject::Asynchronous, true);
		m_displays.append(disp);
		m_devices.append(dev);
		// connect(disp, SIGNAL(destroyed(QObject*)), this, SLOT(clear()));
		VipDisplaySceneModel* disp_sm = static_cast<VipDisplaySceneModel*>(disp);
		if (vipHasWriteRightsDB()) {
			// enable the removing of events
			disp_sm->item()->setItemAttribute(VipPlotItem::IsSuppressable, true);
			connect(disp_sm->item(), SIGNAL(shapeDestroyed(VipPlotShape*)), this, SLOT(shapeDestroyed(VipPlotShape*)), Qt::DirectConnection);
		}
		// destroy the plot item if the display is destroyed
		connect(disp_sm, SIGNAL(destroyed(QObject*)), disp_sm->item(), SLOT(deleteLater()), Qt::DirectConnection);
		// set the shape colors
		QColor c = eventColor(group);
		if (c != Qt::transparent) {
			VipTextStyle style = disp_sm->item()->textStyle(group);
			style.setTextPen(QPen(c));
			disp_sm->item()->setTextStyle(group, style);
			c.setAlpha(100);
			disp_sm->item()->setBrush(group, QBrush(c));
		}
		// disable serialization for the plot scene model, the display scene model and the event device
		disp_sm->item()->setProperty("_vip_no_serialize", true);
		disp_sm->setProperty("_vip_no_serialize", true);
		dev->setProperty("_vip_no_serialize", true);

		// add to player
		vipCreatePlayersFromProcessing(disp, m_player);

		if (vipHasWriteRightsDB()) {

			// enable polygon modifications
			disp_sm->item()->setMode(VipPlotSceneModel::Resizable);

			QObject::connect(disp_sm->item(), SIGNAL(finishedChange(VipResizeItem*)), this, SLOT(changeSelectedPolygons()));
		}
	}
	return dev;
}

void VipPlayerDBAccess::showEvents()
{
	if (!m_player)
		return;
	if (!m_player->processingPool())
		return;

	QSet<QString> groups;
	// extract groups
	for (Vip_event_list::const_iterator it = m_initial_events.begin(); it != m_initial_events.end(); ++it) {
		const VipShapeList &lst = it.value();
		if (lst.size())
			groups.insert(lst.first().group());
	}

	for (QSet<QString>::const_iterator it = groups.begin(); it != groups.end(); ++it) {
		createDevice(m_initial_events, *it);
	}

	m_player->processingPool()->reload();
}

void VipPlayerDBAccess::shapeDestroyed(VipPlotShape* sh)
{
	// Shape destroyed manually, remove it from events
	VipShape shape = sh->rawData();
	if (VipPlotSceneModel* plot = sh->property("VipPlotSceneModel").value<VipPlotSceneModel*>()) {
		if (VipDisplayObject* obj = plot->property("VipDisplayObject").value<VipDisplayObject*>())
			if (/*VipEventDevice* dev = */vipListCast<VipEventDevice*>(obj->allSources()).first())
				this->remove(/*shape.attribute("ID").toInt()*/ shape.id());
	}

	// remove ALL drawn time line
	if (VipDisplayPlayerArea* a = VipDisplayPlayerArea::fromChildWidget(m_player)) {
		QList<VipTimeRangeListItem*> items = a->playWidget()->area()->findItems<VipTimeRangeListItem*>(QString(), 2, 1);
		for (qsizetype i = 0; i < items.size(); ++i) {
			if (/*VipEventDevice* dev =*/ qobject_cast<VipEventDevice*>(items[i]->device())) {
				items[i]->setAdditionalDrawFunction(VipTimeRangeListItem::draw_function());
			}
		}
	}
}

void VipPlayerDBAccess::resetDrawEventTimeLine()
{
	QList<VipPlotShape*> shapes = vipCastItemList<VipPlotShape*>(m_player->plotWidget2D()->area()->plotItems(), QString(), 1, 1);

	// set the draw function to draw time ranges for selected events
	QMap<QString, QList<QPointer<VipPlotShape>>> pshapes;
	for (qsizetype i = 0; i < shapes.size(); ++i) {
		pshapes[shapes[i]->rawData().group()].append(shapes[i]);
	}
	if (VipDisplayPlayerArea* a = VipDisplayPlayerArea::fromChildWidget(m_player)) {
		QList<VipTimeRangeListItem*> items = a->playWidget()->area()->findItems<VipTimeRangeListItem*>(QString(), 2, 1);
		for (qsizetype i = 0; i < items.size(); ++i) {
			if (VipEventDevice* dev = qobject_cast<VipEventDevice*>(items[i]->device())) {
				if (pshapes.find(dev->group()) != pshapes.end())
					items[i]->setAdditionalDrawFunction(
					  std::bind(drawEventTimeLine, m_events, pshapes[dev->group()], std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				else
					items[i]->setAdditionalDrawFunction(VipTimeRangeListItem::draw_function());
			}
		}
	}
}

void VipPlayerDBAccess::itemSelected(VipPlotItem*)
{
	QList<VipPlotShape*> shapes = vipCastItemList<VipPlotShape*>(m_player->plotWidget2D()->area()->plotItems(), QString(), 1, 1);

	// set the draw function to draw time ranges for selected events
	QMap<QString, QList<QPointer<VipPlotShape>>> pshapes;
	for (qsizetype i = 0; i < shapes.size(); ++i) {
		pshapes[shapes[i]->rawData().group()].append(shapes[i]);
	}
	if (VipDisplayPlayerArea* a = VipDisplayPlayerArea::fromChildWidget(m_player)) {
		QList<VipTimeRangeListItem*> items = a->playWidget()->area()->findItems<VipTimeRangeListItem*>(QString(), 2, 1);
		for (qsizetype i = 0; i < items.size(); ++i) {
			if (VipEventDevice* dev = qobject_cast<VipEventDevice*>(items[i]->device())) {
				if (pshapes.find(dev->group()) != pshapes.end())
					items[i]->setAdditionalDrawFunction(
					  std::bind(drawEventTimeLine, m_events, pshapes[dev->group()], std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
				else
					items[i]->setAdditionalDrawFunction(VipTimeRangeListItem::draw_function());
			}
		}
	}

	if (!vipHasReadRightsDB())
		return;

	for (qsizetype i = 0; i < shapes.size(); ++i) {
		if (!shapes[i]->rawData().hasAttribute("max_temperature_C")) {
			shapes.removeAt(i);
			--i;
		}
	}

	m_selection.clear();
	QString category, comment, name, method, userName, dataset, status;
	double duration = vipNan();
	double confidence = -1;
	Qt::CheckState automatic = Qt::PartiallyChecked;

	if (!shapes.isEmpty()) {
		m_selection.append(shapes.first());
		category = shapes.first()->rawData().group();
		comment = shapes.first()->rawData().attribute("comments").toString();
		name = shapes.first()->rawData().attribute("name").toString();
		method = shapes.first()->rawData().attribute("method").toString();
		dataset = shapes.first()->rawData().attribute("dataset").toString();
		status = shapes.first()->rawData().attribute("analysis_status").toString();
		userName = shapes.first()->rawData().attribute("user").toString();
		duration = shapes.first()->rawData().attribute("duration_ns").toDouble() / 1000000000.0;
		automatic = shapes.first()->rawData().attribute("is_automatic_detection").toBool() ? Qt::Checked : Qt::Unchecked;
		confidence = shapes.first()->rawData().attribute("confidence").toDouble();
		for (qsizetype i = 1; i < shapes.size(); ++i) {
			VipPlotShape* shape = shapes[i];
			m_selection.append(shape);
			QString _category = shape->rawData().group();
			QString _comment = shape->rawData().attribute("comments").toString();
			QString _name = shape->rawData().attribute("name").toString();
			QString _method = shape->rawData().attribute("method").toString();
			QString _dataset = shape->rawData().attribute("dataset").toString();
			QString _status = shape->rawData().attribute("analysis_status").toString();
			QString _userName = shape->rawData().attribute("user").toString();
			double _duration = shape->rawData().attribute("duration_ns").toDouble() / 1000000000.0;
			int _automatic = shape->rawData().attribute("is_automatic_detection").toBool() ? Qt::Checked : Qt::Unchecked;
			double _confidence = shape->rawData().attribute("confidence").toDouble();
			if (!category.isEmpty() && _category != category)
				category = QString();
			if (!comment.isEmpty() && _comment != comment)
				comment = QString();
			if (!name.isEmpty() && _name != name)
				name = QString();
			if (!method.isEmpty() && _method != method)
				method = QString();
			if (!dataset.isEmpty() && _dataset != dataset)
				dataset = QString();
			if (!status.isEmpty() && _status != status)
				status = QString();
			if (!userName.isEmpty() && _userName != userName)
				userName = QString();
			if (!vipIsNan(duration) && duration != _duration)
				duration = vipNan();
			if (automatic != Qt::PartiallyChecked && automatic != _automatic)
				automatic = Qt::PartiallyChecked;
			if (confidence >= 0 && confidence != _confidence)
				confidence = -1;
		}
		m_infos->show();
	}

	m_infos->setCategory(category);
	m_infos->setDataset(dataset);
	m_infos->setAnalysisStatus(status);
	m_infos->setComment(comment);
	m_infos->setName(name);
	m_infos->setConfidence(confidence);
	m_infos->setMethod(method);
	m_infos->setAutomaticState(automatic);
	m_infos->setUserName(userName);
	m_infos->setDuration(duration);
	m_infos->clearMergeIds();
}

void VipPlayerDBAccess::applyChangesToSelection()
{
	// build ids first
	QList<qint64> ids;
	QList<qint64> group_ids;
	VipPlotShape* first = nullptr;
	QSet<QString> groups;
	for (qsizetype i = 0; i < m_selection.size(); ++i) {
		if (VipPlotShape* sh = m_selection[i]) {
			if (!first)
				first = sh;
			ids.append(sh->rawData().id());
			if (!m_infos->category().isEmpty() && m_infos->category() != sh->rawData().group())
				group_ids.append(sh->rawData().id());
			groups.insert(sh->rawData().group());
		}
	}

	if (first) {
		if (!m_infos->comment().isEmpty()) // && first->rawData().attribute("comments").toString() != m_infos->comment())
			this->changeValue("comments", m_infos->comment(), ids);
		if (!m_infos->dataset().isEmpty()) // && first->rawData().attribute("comments").toString() != m_infos->comment())
			this->changeValue("dataset", m_infos->dataset(), ids);
		if (!m_infos->analysisStatus().isEmpty())
			this->changeValue("analysis_status", m_infos->analysisStatus(), ids);
		if (!m_infos->name().isEmpty())
			this->changeValue("name", m_infos->name(), ids);
		if (!m_infos->method().isEmpty()) //&& first->rawData().attribute("method").toString() != m_infos->method())
			this->changeValue("method", m_infos->method(), ids);
		if (m_infos->confidence() >= 0) //&& first->rawData().attribute("confidence").toInt() != m_infos->confidence())
			this->changeValue("confidence", QString::number(m_infos->confidence()), ids);
		if (m_infos->automaticState() != Qt::PartiallyChecked) // && first->rawData().attribute("is_automatic_detection").toBool() != (m_infos->automaticState() == Qt::Checked))
			this->changeValue("is_automatic_detection", QString::number((int)(m_infos->automaticState() == Qt::Checked)), ids);
		if (!m_infos->category().isEmpty() && group_ids.size())
			this->changeCategory(m_infos->category(), group_ids);
	}

	// get ids to merge
	QList<qint64> merged = m_infos->mergeIds();
	if (merged.size())
		mergeIds(merged);
	// clear ids from info panel
	m_infos->clearMergeIds();

	// reload to take group changes into account
	m_player->processingPool()->reload();
}

void VipPlayerDBAccess::clear()
{
	// reset all
	// remove all display objects (and their sources)
	QObject* s = sender();
	QList<VipDisplayObject*> lst = displayEvents();
	for (qsizetype i = 0; i < lst.size(); ++i) {
		if (lst[i] != s) {
			// disconnect(lst[i], SIGNAL(destroyed(QObject*)), this, SLOT(clear()));
			lst[i]->deleteLater();
			m_devices[i]->deleteLater();
		}
	}
	m_displays.clear();
	m_devices.clear();
	m_events.clear();
	m_initial_events.clear();
	m_actions.clear();
	m_modifications.clear();
}

QList<VipDisplayObject*> VipPlayerDBAccess::displayEvents()
{
	QList<VipDisplayObject*> res;
	for (qsizetype i = 0; i < m_displays.size(); ++i) {
		if (m_displays[i])
			res.append(m_displays[i]);
		else {
			m_displays.removeAt(i);
			m_devices.removeAt(i);
			--i;
		}
	}
	return res;
}
QList<VipEventDevice*> VipPlayerDBAccess::devices()
{
	QList<VipEventDevice*> res;
	for (qsizetype i = 0; i < m_displays.size(); ++i) {
		if (m_displays[i])
			res.append(m_devices[i]);
		else {
			m_displays.removeAt(i);
			m_devices.removeAt(i);
			--i;
		}
	}
	return res;
}

const QList<VipPlayerDBAccess::Action>& VipPlayerDBAccess::actionsStack() const
{
	return m_actions;
}

VipEventDevice* VipPlayerDBAccess::device(const QString& group)
{
	QList<VipEventDevice*> devs = devices();
	for (qsizetype i = 0; i < devs.size(); ++i)
		if (devs[i]->group() == group)
			return devs[i];
	return nullptr;
}

VipManualAnnotation* VipPlayerDBAccess::manualAnnotationPanel() const
{
	return m_annotation;
}

void VipPlayerDBAccess::sendManualAnnotationToJson()
{
	if (!m_annotation) {
		VIP_LOG_ERROR("No available annotations!");
		return;
	}

	VipProgress p;
	QString error;

	Vip_event_list to_send = m_annotation->generateShapes(&p, &error);
	if (to_send.isEmpty()) {
		if (error.size())
			QMessageBox::warning(nullptr, "Warning", error);
		return;
	}

	QString filename = VipFileDialog::getSaveFileName(vipGetMainWindow(), "Save events to JSON", "JSON file (*.json)");
	if (filename.isEmpty())
		return;

	if (!vipEventsToJsonFile(filename, to_send, &p))
	/* if (!sendToJSON(filename,
			   to_send.first().first().attribute("user").toString(),
	to_send.first().first().attribute("line_of_sight").toString(),
	to_send.first().first().attribute("device").toString(),
	to_send.first().first().attribute("experiment_id").value< Vip_experiment_id>(),
	to_send, before_send_list_type() << computeEventPolygons, &p))*/
	{
		QMessageBox::warning(nullptr, "Error", "An error occured while saving manual annotation");
		return;
	}
}

void VipPlayerDBAccess::sendManualAnnotation()
{
	if (!m_annotation) {
		VIP_LOG_ERROR("No available annotations!");
		return;
	}

	VipProgress p;
	QString error;

	Vip_event_list to_send = m_annotation->generateShapes(&p, &error);
	if (to_send.isEmpty()) {
		if (error.size())
			QMessageBox::warning(nullptr, "Warning", error);
		return;
	}

	QList<qint64> ids = vipSendToDB(to_send.first().first().attribute("user").toString(),
					to_send.first().first().attribute("line_of_sight").toString(),
					to_send.first().first().attribute("device").toString(),
					to_send.first().first().attribute("experiment_id").value<Vip_experiment_id>(),
					to_send,
					&p);
	if (ids.size() == 0) {
		QMessageBox::warning(nullptr, "Error", "An error occured while sending manual annotation");
		return;
	}

	// remove the selected shapes
	QList<VipPlotShape*> shapes = m_player->plotSceneModel()->shapes(1);
	VipShapeList lst;
	for (qsizetype i = 0; i < shapes.size(); ++i)
		lst.push_back(shapes[i]->rawData());
	for (qsizetype i = 0; i < lst.size(); ++i) {
		MarkersType m = lst[i].attribute("_vip_markers").value<MarkersType>();
		if (m.size()) {
			m_player->plotSceneModel()->sceneModel().remove(lst[i]);
		}
	}

	this->addEvents(to_send, true);
}

void VipPlayerDBAccess::saveCSV()
{
	struct Info
	{
		double min, max, mean, deltaT;
		double X, Y;
		double duration;
		double pixel_area;
		double elongation;

		int camera;
	};

	if (m_events.size() == 0)
		return;

	QString pulse = QString::number(m_events.first().first().attribute("experiment_id").value<Vip_experiment_id>());
	QString csv_name = pulse + "-" + m_events.first().first().attribute("line_of_sight").toString();

	QString filename = VipFileDialog::getSaveFileName2(nullptr, csv_name, "Save events as CSV file", "CSV file (*.csv)");
	if (filename.isEmpty())
		return;

	QFile fout(filename);
	if (!fout.open(QFile::WriteOnly | QFile::Text))
		return;

	QStringList cams = vipCamerasDB();

	QList<Info> infos;
	for (Vip_event_list::const_iterator it = m_events.begin(); it != m_events.end(); ++it) {
		Info info;
		const VipShapeList& lst = it.value();

		info.min = lst.first().attribute("max_temperature_C").toDouble();
		info.max = lst.first().attribute("max_temperature_C").toDouble();
		info.mean = lst.first().attribute("max_temperature_C").toDouble();
		info.X = lst.first().attribute("max_T_image_position_x").toDouble();
		info.Y = lst.first().attribute("max_T_image_position_y").toDouble();
		info.pixel_area = lst.first().attribute("pixel_area").toDouble();
		info.elongation = lst.first().attribute("bbox_width").toDouble() / lst.first().attribute("bbox_height").toDouble();
		info.duration = (lst.last().attribute("timestamp_ns").toDouble() - lst.first().attribute("timestamp_ns").toDouble()) / 1000000000.;
		info.deltaT = lst.last().attribute("max_temperature_C").toDouble() - lst.first().attribute("max_temperature_C").toDouble();
		info.camera = cams.indexOf(lst.first().attribute("line_of_sight").toString());

		for (qsizetype i = 1; i < lst.size(); ++i) {
			info.min = qMin(info.min, lst[i].attribute("max_temperature_C").toDouble());
			info.max = qMax(info.max, lst[i].attribute("max_temperature_C").toDouble());
			info.X += lst[i].attribute("max_T_image_position_x").toDouble();
			info.Y += lst[i].attribute("max_T_image_position_y").toDouble();
			info.pixel_area += lst[i].attribute("pixel_area").toDouble();
			info.elongation += lst[i].attribute("bbox_width").toDouble() / lst[i].attribute("bbox_height").toDouble();
			info.mean += lst[i].attribute("max_temperature_C").toDouble();
		}

		info.X /= lst.size();
		info.Y /= lst.size();
		info.pixel_area /= lst.size();
		info.elongation /= lst.size();
		info.mean /= lst.size();

		infos.push_back(info);
	}

	QTextStream str(&fout);

	QStringList names = QStringList() << "Min (deg C)"
					  << "Max (deg C)"
					  << "Mean (deg C)"
					  << "DeltaT (deg C)"
					  << "X"
					  << "Y";
	names << "duration (s)"
	      << "pixel_area (px)"
	      << "Elongation";

	// write separator
	str << "\"sep=\t\"\n";
	// write the header
	str << names.join("\t") << "\n";

	// write values
	for (qsizetype i = 0; i < infos.size(); ++i) {
		str << infos[i].min << "\t" << infos[i].max << "\t" << infos[i].mean << "\t" << infos[i].deltaT << "\t" << infos[i].X << "\t" << infos[i].Y << "\t" << infos[i].duration << "\t"
		    << infos[i].pixel_area << "\t" << infos[i].elongation << "\t";

		str << "\n";
	}

	fout.close();
}

VipPlayerDBAccess* VipPlayerDBAccess::fromPlayer(VipVideoPlayer* pl)
{
	if (!pl)
		return nullptr;
	if (!pl->property("VipPlayerDBAccess").toBool())
		return new VipPlayerDBAccess(pl);
	return pl->findChild<VipPlayerDBAccess*>();
}

static void onPlayerCreated(VipVideoPlayer* pl)
{
	if (!vipHasReadRightsDB())
		return;
	// only display for raw video player (or possible collision with tokida plugin)
	if (pl->metaObject() != &VipVideoPlayer::staticMetaObject)
		return;
	if (!pl->property("VipPlayerDBAccess").toBool() && pl->processingPool())
		new VipPlayerDBAccess(pl);
}
static int registerPlayerDBAccess()
{
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(onPlayerCreated);
	return 0;
}
static int _registerPlayerDBAccess = registerPlayerDBAccess();
