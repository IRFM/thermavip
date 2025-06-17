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

#include "VipVisualizeDB.h"
#include "VipDisplayArea.h"
#include "VipDragWidget.h"
#include "VipLogging.h"
#include "VipPlotShape.h"
#include "VipProcessMovie.h"
#include "VipSet.h"

#include <qapplication.h>
#include <qboxlayout.h>
#include <qclipboard.h>
#include <qmessagebox.h>
#include <qpainter.h>
#include <qtooltip.h>

// Find ALL video players withing the current workspace
static QList<VipVideoPlayer*> findPlayers()
{
	QList<VipVideoPlayer*> players;
	if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		players = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->findChildren<VipVideoPlayer*>();
	return players;
}
// Returns the player title with its Id
static QString titleFromPlayer(VipVideoPlayer* pl)
{
	if (VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(pl)) {
		return QString::number(VipUniqueId::id(parent)) + " " + pl->windowTitle();
	}
	return QString();
}

struct DBItem : public QTableWidgetItem
{
	enum Type
	{
		String,
		Bool,
		Double,
		Time,
		Date
	};

	qint64 eventId;
	QString column;
	QVariant value;
	QString tooltip;
	Type type;

	DBItem(qint64 event_id, const QString& column, const QVariant& value, Type value_type)
	  : eventId(event_id)
	  , column(column)
	  , value(value)
	  , type(value_type)
	{
		switch (type) {
			case String:
				setText(value.toString());
				break;
			case Bool:
				setText(value.toInt() ? "yes" : "no");
				break;
			case Double:
				setText(value.toString());
				break;
			case Time:
				setText(QString::number(value.toLongLong() / 1000000000.0));
				break;
			case Date:
				setText(QDateTime::fromMSecsSinceEpoch(value.toLongLong() / 1000000).toString("dd/MM/yyyy\nhh:mm:ss"));
				break;
		}
		// disable edition
		setFlags(flags() & ~Qt::ItemIsEditable);
	}

	virtual QVariant data(int role) const
	{
		if (role == Qt::ToolTipRole) {
			// generate the item tool tip
			if (tooltip.isEmpty()) {
				if (VisualizeDB* table = VisualizeDB::fromChild(this->tableWidget())) {
					const VipEventQueryResult evt = table->events().events[eventId];
					if (evt.eventId == eventId) {

						// read event polygon
						/* double scale = 4.0;
						QPolygonF poly;
						QTextStream str(evt.polygon);
						while (true) {
							QPointF pt;
							str >> pt.rx() >> pt.ry();
							if (str.status() != QTextStream::Ok)
								break;
							poly.push_back(pt / scale);
						}*/

						QString device = evt.device;
						/* QSize size = vipFindDeviceParameters(device)->defaultVideoSize();

						QImage img(size.width() / scale, size.height() / scale, QImage::Format_ARGB32);
						img.fill(Qt::transparent);
						{
							QPainter p(&img);
							p.setPen(Qt::gray);
							p.setBrush(QBrush());
							p.drawRect(0, 0, img.width() - 1, img.height() - 1);
							p.setRenderHints(QPainter::Antialiasing);
							p.setPen(Qt::magenta);
							p.setBrush(Qt::magenta);
							p.drawPolygon(poly);
						}*/

						QString& _tooltip = const_cast<QString&>(tooltip);
						_tooltip +=
						  QString::number(evt.experiment_id) + " " + evt.camera + " " + evt.device + " " + evt.eventName + " (" + QString::number(evt.confidence) + "/1)";
						_tooltip += "<br>duration: " + QString::number(evt.duration / 1000000000.0) + "s";
						//_tooltip += "<br>" + vipToHtml(img, "align=\"middle\"");
						return tooltip;
					}
				}
			}
			return tooltip;
		}
		return QTableWidgetItem::data(role);
	}
};

class VisualizeDB::PrivateData
{
public:
	VipQueryDBWidget* query;
	QPushButton* launch;
	QPushButton* reset;
	QTableWidget* table;
	VipEventQueryResults events;
	// ExtractOption extract;

	QPointer<VipPlotShape> selectedShape;
};

VisualizeDB::VisualizeDB(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->query = new VipQueryDBWidget(QString());
	d_data->launch = new QPushButton();
	d_data->reset = new QPushButton();
	d_data->table = new QTableWidget();

	d_data->launch->setIcon(vipIcon("apply.png"));
	d_data->launch->setText("Launch query");
	d_data->reset->setIcon(vipIcon("reset.png"));
	d_data->reset->setToolTip("Reset search parameters");

	d_data->query->enablePulseRange(true);
	d_data->query->setRemovePreviousVisible(false);

	// columns: Pulse, CameraName, initial_timestamp, duration, thermal_event,maximum,
	// is_automatic_detection, method, confidence, user, comments, name
	int col_count = 14;

	d_data->table->setColumnCount(col_count);
	d_data->table->setHorizontalHeaderLabels(QStringList() << "Experiment id"
							       << "Camera"
							       << "Device"
							       << "Start(s)"
							       << "Duration(s)"
							       << "Type"
							       << "MaxT(C)"
							       << "Automatic"
							       << "Method"
							       << "Confidence"
							       << "User"
							       << "Comments"
							       << "Name");
	// for now hide pulse date
	d_data->table->setSortingEnabled(true);
	d_data->table->installEventFilter(this);
	d_data->table->viewport()->installEventFilter(this);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addWidget(d_data->query);

	vlay->addWidget(VipLineWidget::createHLine());

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addWidget(d_data->launch, 1);
	hlay->addWidget(d_data->reset);

	vlay->addWidget(VipLineWidget::createHLine());
	vlay->addLayout(hlay);
	vlay->addStretch(1);

	QHBoxLayout* lay = new QHBoxLayout();
	lay->addLayout(vlay);
	lay->addWidget(d_data->table, 1);
	setLayout(lay);

	connect(d_data->launch, SIGNAL(clicked(bool)), this, SLOT(launchQuery()));
	connect(d_data->reset, SIGNAL(clicked(bool)), this, SLOT(resetQueryParameters()));
}
VisualizeDB::~VisualizeDB()
{
}

bool VisualizeDB::eventFilter(QObject* watched, QEvent* evt)
{
	if (watched == d_data->table || watched == d_data->table->viewport()) {
		if (evt->type() == QEvent::KeyPress && (vipHasWriteRightsDB())) {
			int key = static_cast<QKeyEvent*>(evt)->key();
			if (key == Qt::Key_Delete) {
				this->suppressSelectedLines();
				return true;
			}
		}
		else if (evt->type() == QEvent::MouseButtonRelease) {
			if (static_cast<QMouseEvent*>(evt)->button() == Qt::RightButton) {

				// check only one column selected
				QList<QTableWidgetItem*> items = d_data->table->selectedItems();
				QSet<int> lines;
				int col = -1;
				for (int i = 0; i < items.size(); ++i)
					lines.insert(items[i]->row());
				for (int i = 0; i < items.size(); ++i) {
					if (col < 0)
						col = items[i]->column();
					else if (col != items[i]->column()) {
						col = -1;
						break;
					}
				}

				VipDragMenu menu;
				if (col >= 0) {
					if (vipHasWriteRightsDB())
						connect(menu.addAction("Edit column for selected events..."), SIGNAL(triggered(bool)), this, SLOT(editSelectedColumn()));
				}
				if (items.size()) {

					if (vipHasWriteRightsDB())
						connect(menu.addAction("Remove selected events..."), SIGNAL(triggered(bool)), this, SLOT(suppressSelectedLines()));
					if (lines.size() == 1) {
						// menu.addSeparator();
						// connect(menu.addAction("Find related events..."), SIGNAL(triggered(bool)), this, SLOT(findRelatedEvents()));
					}

					// menu.addSeparator();
					// connect(menu.addAction("Extract time trace for selected events..."), SIGNAL(triggered(bool)), this, SLOT(plotTimeTrace()));

					menu.addSeparator();
					QList<VipVideoPlayer*> pls = findPlayers();
					// add display on player options
					QMenu* plmenu = new QMenu(&menu);
					plmenu->addAction("Display on new player");
					if (pls.size()) {
						plmenu->addSeparator();
						for (int i = 0; i < pls.size(); ++i) {
							QAction* a = plmenu->addAction(titleFromPlayer(pls[i]));
							a->setProperty("player", QVariant::fromValue(pls[i]));
						}
					}
					menu.addAction("Display selected events on")->setMenu(plmenu);
					connect(plmenu, SIGNAL(triggered(QAction*)), this, SLOT(displaySelectedEvents(QAction*)));

					menu.addSeparator();
					connect(menu.addAction(vipIcon("save.png"), "Save selection to CSV file"), SIGNAL(triggered(bool)), this, SLOT(saveToCSV()));
					connect(menu.addAction("Copy selection to clipboard"), SIGNAL(triggered(bool)), this, SLOT(copyToClipBoard()));
				}

				if (menu.actions().size() > 0) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
					menu.exec(static_cast<QMouseEvent*>(evt)->globalPos());
#else
					menu.exec(static_cast<QMouseEvent*>(evt)->globalPosition().toPoint());
#endif
				}

				return true;
			}
		}
	}

	return false;
}

VisualizeDB* VisualizeDB::fromChild(QWidget* w)
{
	while (w) {
		if (qobject_cast<VisualizeDB*>(w))
			return static_cast<VisualizeDB*>(w);
		w = w->parentWidget();
	}
	return nullptr;
}

VipQueryDBWidget* VisualizeDB::queryWidget() const
{
	return d_data->query;
}
QPushButton* VisualizeDB::launchQueryButton() const
{
	return d_data->launch;
}
QPushButton* VisualizeDB::resetQueryButton() const
{
	return d_data->reset;
}
QTableWidget* VisualizeDB::tableWidget() const
{
	return d_data->table;
}
VipEventQueryResults VisualizeDB::events() const
{
	return d_data->events;
}

void VisualizeDB::plotTimeTrace()
{
	/* if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return;

	ExtractOption extr = extractOptions(d_data->extract);
	if (extr.hasError)
		return;

	d_data->extract = extr;
	VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);

	//get selected events
	VipEventQueryResults events;
	QSet<int> lines;
	QList<QTableWidgetItem*> items = d_data->table->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		int row = items[i]->row();
		if (lines.find(row) == lines.end()) {
			lines.insert(row);
			qint64 eventId = static_cast<DBItem*>(items[i])->eventId;
			events.events[eventId] = d_data->events.events[eventId];
		}
	}

	QString device;
	if (events.events.size()) {
		device = events.events.first().device;
	}

	QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector> > > > res =
		extractParameters(extr, events,&progress);

	//map of camera -> io devices
	QMap<QString, QList<VipAnyResource*> > curves;

	QStringList cameras = res.keys();
	for (int c = 0; c < cameras.size(); ++c) {
		const QString camera = cameras[c];
		const QList<Vip_experiment_id> pulses = res[camera].keys();
		for (int p = 0; p < pulses.size(); ++p) {
			const Vip_experiment_id pulse = pulses[p];
			const QStringList rois = res[camera][pulse].keys();
			for (int r = 0; r < rois.size(); ++r) {
				const QString roi = rois[r];
				const QStringList params = res[camera][pulse][roi].keys();
				for (int a = 0; a < params.size(); ++a) {
					const QString & param = params[a];
					const VipPointVector points = res[camera][pulse][roi][param];
					const QString & name = (pulse == -1 ? QString() : vipFindDeviceParameters(device)->pulseToString(pulse) + " ") +
						(camera == "All" ? QString() : camera + " ") +
						(roi == "All" ? QString() : roi + " ") +
						param;

					VipAnyResource * any = new VipAnyResource();
					any->setAttribute("Name", name);
					any->setAttribute("XUnit", "Time");
					any->setData(QVariant::fromValue(points));
					curves[camera].push_back(any);
				}
			}
		}
	}

	//create players
	QList<QWidget*> players;
	for (QMap<QString, QList<VipAnyResource*> >::iterator it = curves.begin(); it != curves.end(); ++it)
	{
		QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessings(it.value(), nullptr);
		if (pls.size())
			players.push_back(pls.first());
	}

	VipBaseDragWidget * base = vipCreateFromWidgets(players);
	VipMultiDragWidget * multi = vipCreateFromBaseDragWidget(base);
	vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->addWidget(multi);
	*/
}

void VisualizeDB::suppressSelectedLines()
{
	if (!vipHasWriteRightsDB()) {
		QMessageBox::warning(nullptr, "Error", "You do not have the rights to perform this action!");
		return;
	}

	QList<QTableWidgetItem*> items = d_data->table->selectedItems();
	// compute selected event ids and lines
	QMap<int, qint64> ids;
	for (int i = 0; i < items.size(); ++i)
		ids.insert(items[i]->row(), static_cast<DBItem*>(items[i])->eventId);

	// ask confirmation
	if (ids.size()) {
		QMessageBox::StandardButton b =
		  QMessageBox::question(nullptr, "Confirmation", "Are you sure to delete selected items (" + QString::number(ids.size()) + ") ?", QMessageBox::Yes | QMessageBox::Cancel);
		if (b != QMessageBox::Yes)
			return;
	}

	VipProgress p;

	// remove items from DB
	if (vipRemoveFromDB(ids.values(), &p)) {

		// remove from table
		QList<int> lines = ids.keys();
		for (int i = lines.size() - 1; i >= 0; --i) {
			d_data->table->removeRow(lines[i]);
		}
	}
}

static QVariant edit(QWidget* editor, const QString& name)
{
	VipGenericDialog dial(editor, "Edit " + name);
	if (dial.exec() == QDialog::Accepted) {
		return editor->property("value");
	}
	return QVariant();
}

QByteArray VisualizeDB::dumpSelection()
{
	QList<QTableWidgetItem*> items = d_data->table->selectedItems();
	int min_row = -1, max_row = -1;
	int min_col = -1, max_col = -1;
	// Sort items by rows and columns
	for (int i = 0; i < items.size(); ++i) {
		int col = items[i]->column();
		int row = items[i]->row();
		if (min_row == -1)
			min_row = row;
		if (max_row == -1)
			max_row = row;
		if (min_col == -1)
			min_col = col;
		if (max_col == -1)
			max_col = col;
		min_row = qMin(min_row, row);
		max_row = qMax(max_row, row);
		min_col = qMin(min_col, col);
		max_col = qMax(max_col, col);
	}

	QByteArray ar;
	{
		QTextStream str(&ar, QIODevice::WriteOnly);
		for (int y = min_row; y <= max_row; ++y) {
			for (int x = min_col; x <= max_col; ++x) {
				DBItem* item = static_cast<DBItem*>(d_data->table->item(y, x));
				QString text = item ? item->text() : QString();
				// if (item->type == DBItem::String)
				//	text = "\"" + text + "\"";

				str << text << "\t";
			}
			str << "\n";
		}
		str.flush();
	}
	return ar;
}

void VisualizeDB::saveToCSV()
{
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Create CSV file", "CSV file (*.csv)");
	if (filename.isEmpty())
		return;

	QByteArray ar = dumpSelection();
	ar = "\"sep=\t\"\n" + ar;

	QFile fout(filename);
	if (!fout.open(QFile::WriteOnly)) {
		QMessageBox::warning(nullptr, "Error while saving CSV file", "Unable to save to file " + QFileInfo(filename).fileName(), QMessageBox::Ok);
		return;
	}
	fout.write(ar);
}
void VisualizeDB::copyToClipBoard()
{
	QByteArray ar = dumpSelection();
	QApplication::clipboard()->setText(ar);
}

void VisualizeDB::editSelectedColumn()
{
	if (!vipHasWriteRightsDB()) {
		QMessageBox::warning(nullptr, "Error", "You do not have the rights to perform this action!");
		return;
	}

	// check only one column selected
	QList<QTableWidgetItem*> items = d_data->table->selectedItems();
	QList<qint64> ids;
	int col = -1;
	for (int i = 0; i < items.size(); ++i) {
		if (col < 0)
			col = items[i]->column();
		else if (col != items[i]->column()) {
			col = -1;
			break;
		}
		ids.append(static_cast<DBItem*>(items[i])->eventId);
	}
	if (col < 0)
		return;

	// columns: Pulse, CameraName, initial_timestamp, duration, thermal_event,maximum,
	// is_automatic_detection, method, confidence, user, comments, name
	DBItem* item = static_cast<DBItem*>(items.first());
	QString name = item->column;
	QVariant value = item->value;

	// edit value
	if (name == "category") {
		VipComboBox* box = new VipComboBox();
		box->addItems(vipEventTypesDB());
		box->setCurrentText(value.toString());
		value = edit(box, "event type");
		if (value.userType() != 0)
			value = QString("'" + value.toString() + "'");
	}
	else if (name == "is_automatic_detection") {
		VipComboBox* box = new VipComboBox();
		box->addItems(QStringList() << "no"
					    << "yes");
		box->setCurrentIndex(value.toBool());
		value = edit(box, "is automatic");
		if (value.userType() != 0)
			value = (bool)(value.toString() == "yes" ? true : false);
	}
	else if (name == "method") {
		VipComboBox* ed = new VipComboBox();
		ed->addItems(vipMethodsDB());
		ed->setCurrentText(value.toString());
		value = edit(ed, "method");
		if (value.userType() != 0)
			value = QString("'" + value.toString() + "'");
	}
	else if (name == "confidence") {
		QDoubleSpinBox* ed = new QDoubleSpinBox();
		ed->setRange(0, 1);
		ed->setSingleStep(0.25);
		ed->setValue(value.toDouble());
		value = edit(ed, "confidence");
	}
	else if (name == "user") {
		VipComboBox* ed = new VipComboBox();
		ed->addItems(vipUsersDB());
		ed->setCurrentText(value.toString());
		value = edit(ed, "User name");
		if (value.userType() != 0)
			value = QString("'" + value.toString() + "'");
	}
	else if (name == "comments") {
		VipLineEdit* ed = new VipLineEdit();
		ed->setText(value.toString());
		value = edit(ed, "comments");
		if (value.userType() != 0)
			value = QString("'" + value.toString() + "'");
	}
	else if (name == "name") {
		VipLineEdit* ed = new VipLineEdit();
		ed->setText(value.toString());
		value = edit(ed, "name");
		if (value.userType() != 0)
			value = QString("'" + value.toString() + "'");
	}
	else {
		QMessageBox::warning(nullptr, "Warning", "This column is not editable");
		return;
	}

	if (value.userType() == 0)
		return;

	VipProgress p;
	if (!vipChangeColumnInfoDB(ids, name, value.toString(), &p))
		QMessageBox::warning(nullptr, "Error", "Unable to change values!");
	else
		launchQuery();
}

void VisualizeDB::displayEventResult(const VipEventQueryResults& res, VipProgress* p)
{
	if (p) {
		p->setText("Update table...");
		p->setRange(0, res.events.size());
	}

	d_data->events = res;

	// fill table
	d_data->table->setRowCount(0);
	d_data->table->setRowCount(res.events.size());
	int row = 0;
	for (QMap<qint64, VipEventQueryResult>::const_iterator it = res.events.begin(); it != res.events.end(); ++it, ++row) {
		if (p)
			p->setValue(row);
		// columns: Pulse, CameraName, initial_timestamp, duration, thermal_event,maximum,
		//  method, confidence, user, comments, name

		int col = 0;
		const VipEventQueryResult& evt = it.value();
		QTableWidgetItem* Pulse = new DBItem(evt.eventId, "experiment_id", QString::number(evt.experiment_id), DBItem::String);
		d_data->table->setItem(row, col++, Pulse);
		QTableWidgetItem* CameraName = new DBItem(evt.eventId, "line_of_sight", evt.camera, DBItem::String);
		d_data->table->setItem(row, col++, CameraName);
		QTableWidgetItem* Device = new DBItem(evt.eventId, "device", evt.device, DBItem::String);
		d_data->table->setItem(row, col++, Device);
		QTableWidgetItem* initial_timestamp = new DBItem(evt.eventId, "initial_timestamp_ns", evt.initialTimestamp, DBItem::Time);
		d_data->table->setItem(row, col++, initial_timestamp);
		QTableWidgetItem* duration = new DBItem(evt.eventId, "duration_ns", evt.duration, DBItem::Time);
		d_data->table->setItem(row, col++, duration);
		QTableWidgetItem* thermal_event = new DBItem(evt.eventId, "category", evt.eventName, DBItem::String);
		d_data->table->setItem(row, col++, thermal_event);
		QTableWidgetItem* maximum = new DBItem(evt.eventId, "max_temperature_C", evt.maximum, DBItem::Double);
		d_data->table->setItem(row, col++, maximum);
		QTableWidgetItem* is_automatic_detection = new DBItem(evt.eventId, "is_automatic_detection", evt.automatic, DBItem::Bool);
		d_data->table->setItem(row, col++, is_automatic_detection);
		QTableWidgetItem* method = new DBItem(evt.eventId, "method", evt.method, DBItem::String);
		d_data->table->setItem(row, col++, method);
		QTableWidgetItem* confidence = new DBItem(evt.eventId, "confidence", evt.confidence, DBItem::Double);
		d_data->table->setItem(row, col++, confidence);
		QTableWidgetItem* user = new DBItem(evt.eventId, "user", evt.user, DBItem::String);
		d_data->table->setItem(row, col++, user);
		QTableWidgetItem* comments = new DBItem(evt.eventId, "comments", evt.comment, DBItem::String);
		d_data->table->setItem(row, col++, comments);
		QTableWidgetItem* name = new DBItem(evt.eventId, "name", evt.name, DBItem::String);
		d_data->table->setItem(row, col++, name);
	}

	d_data->table->resizeColumnsToContents();
	d_data->table->resizeRowsToContents();
}

void VisualizeDB::launchQuery()
{
	VipEventQuery query;
	query.automatic = d_data->query->automatic();
	if (d_data->query->camera().size())
		query.cameras << d_data->query->camera();
	if (d_data->query->device().size())
		query.devices << d_data->query->device();
	if (d_data->query->thermalEvent().size())
		query.event_types << d_data->query->thermalEvent();
	query.in_comment = d_data->query->inComment();
	query.in_name = d_data->query->inName();

	query.min_duration = d_data->query->durationRange().first;
	query.max_duration = d_data->query->durationRange().second;
	query.min_temperature = d_data->query->maxTemperatureRange().first;
	query.max_temperature = d_data->query->maxTemperatureRange().second;

	query.min_confidence = d_data->query->minConfidence();
	query.max_confidence = d_data->query->maxConfidence();
	if (d_data->query->userName().size())
		query.users << d_data->query->userName();

	query.method = d_data->query->method();
	query.dataset = d_data->query->dataset();
	query.min_pulse = d_data->query->pulseRange().first;
	query.max_pulse = d_data->query->pulseRange().second;

	if (d_data->query->idThermalEventInfo() > 0)
		query.eventIds.append(d_data->query->idThermalEventInfo());

	VipProgress p;
	VipEventQueryResults res = vipQueryDB(query, &p);
	if (!res.isValid()) {
		QMessageBox::warning(nullptr, "Warning", "Failed to retrieve events!");
		return;
	}

	displayEventResult(res, &p);
}
void VisualizeDB::resetQueryParameters()
{
	d_data->query->setIDThermalEventInfo(0);
	d_data->query->setUserName("All");
	d_data->query->setCamera("All");
	d_data->query->setDevice("All");
	d_data->query->setInComment(QString());
	d_data->query->setInName(QString());

	d_data->query->setDataset(QString());
	d_data->query->setMethod(QString());
	d_data->query->setDurationRange(VipTimeRange(0, 1000000000000ULL));

	d_data->query->setMaxTemperatureRange(QPair<double, double>(0, 5000));
	d_data->query->setAutomatic(-1);
	d_data->query->setMinConfidence(0);
	d_data->query->setThermalEvent("All");
}

void VisualizeDB::displaySelectedEvents(QAction* a)
{
	VipProgress progress;

	// extract the ids of selected events
	QList<QTableWidgetItem*> items = d_data->table->selectedItems();
	QList<qint64> ids;
	for (int i = 0; i < items.size(); ++i)
		ids.append(static_cast<DBItem*>(items[i])->eventId);
	ids = vipToSet(ids).values();

	// query events in db
	VipEventQuery q;
	q.eventIds = ids;
	VipEventQueryResults r = vipQueryDB(q, &progress);

	// query all shapes
	VipFullQueryResult fr = vipFullQueryDB(r, &progress);

	// extract events
	Vip_event_list events = vipExtractEvents(fr);

	// get the different pulses and  cameras
	QMap<Vip_experiment_id, Vip_experiment_id> pulses;
	QSet<QString> cameras;
	QSet<QString> devices;
	for (QMap<Vip_experiment_id, VipPulseResult>::const_iterator itp = fr.result.begin(); itp != fr.result.end(); ++itp) {
		const VipPulseResult p = itp.value();
		pulses.insert(p.experiment_id, p.experiment_id);
		for (QMap<QString, VipCameraResult>::const_iterator itc = p.cameras.begin(); itc != p.cameras.end(); ++itc) {
			cameras.insert(itc.value().cameraName);
			devices.insert(itc.value().device);
		}
	}

	VipVideoPlayer* pl = a->property("player").value<VipVideoPlayer*>();
	if (!pl) {
		// create a new player
		QPair<Vip_experiment_id, QString> pulse;

		if (pulses.size() == 0 || cameras.size() == 0 || devices.size() != 1) {
			VIP_LOG_ERROR("Invalid experiment id, camera of device");
			return;
		}

		// TODO: find a way to put back
		/*if (pulses.size() != 1 || cameras.size() != 1)
			pulse = cameraAndPulse(pulses.firstKey(), pulses.lastKey(), cameras.toList());
		else*/
		pulse = QPair<Vip_experiment_id, QString>(pulses.firstKey(), *cameras.begin());
		if (pulse.first >= 0) {

			QString device = *devices.begin();

			// open player
			QList<VipAbstractPlayer*> pls = vipGetMainWindow()->openPaths(QStringList() << vipFindDeviceParameters(device)->createDevicePath(pulse.first, pulse.second));
			if (pls.size())
				pl = qobject_cast<VipVideoPlayer*>(pls.last());
		}
	}
	if (pl) {
		vipProcessEvents(nullptr, 1000);
		if (VipPlayerDBAccess* db = /*VipPlayerDBAccess::fromPlayer(pl)*/ pl->findChild<VipPlayerDBAccess*>()) {
			db->addEvents(events, true);
		}
	}
}

void VisualizeDB::findRelatedEvents()
{
	/* DBItem* item = static_cast<DBItem*>(d_data->table->selectedItems().first());
	const VipEventQueryResult & evt = d_data->events.events[item->eventId];

	//read event polygon
	QPolygonF poly;
	QTextStream str(evt.polygon);
	while (true) {
		QPointF pt;
		str >> pt.rx() >> pt.ry();
		if (str.status() != QTextStream::Ok)
			break;
		poly.push_back(pt);
	}

	//find all events within polygon
	VipProgress p;
	VipEventQuery q;
	q.cameras.append(evt.camera);
	q.devices.append(evt.device);
	q.region = VipShape(poly);
	q.min_region_overlap = 0.5;
	VipEventQueryResults r = vipQueryDB(q,&p);
	if (r.events.size() < 2) {
		QMessageBox::warning(nullptr,"Warning","No related events found!");
		return;
	}
	displayEventResult(r, &p);*/
}

/*bool VisualizeDB::viewportEvent(QEvent *event)
{
	if (event->type() == QEvent::ToolTip)
	{
		if (underMouse())
		{
			QPoint pos = static_cast<QHelpEvent*>(event)->pos();
			QPoint global = static_cast<QHelpEvent*>(event)->globalPos();
			const QModelIndex index = indexAt(
				this->mapFromGlobal(pos));
			if (index.isValid())
			{
				QToolTip::showText(global, "toto", this, QRect());
			}
		}
	}
	return QTableWidget::viewportEvent(event);
}*/

VisualizeDBToolWidget::VisualizeDBToolWidget(VipMainWindow* win)
  : VipToolWidget(win)
{
	//this->setWidget(new VisualizeDB());
	this->setObjectName("Event database");
	this->setWindowTitle("Event database");
	this->setKeepFloatingUserSize(true);
	this->setMinimumSize(700, 600);
	// setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
}

void VisualizeDBToolWidget::showEvent(QShowEvent*)
{
	static bool init = false;
	if (!init) {
		// Set the inner widget at first show.
		// This avoids any SQL query at startup, when the event loop is not running.
		init = true;
		this->setWidget(new VisualizeDB());
	}
}

VisualizeDB* VisualizeDBToolWidget::getVisualizeDB() const
{
	return static_cast<VisualizeDB*>(this->widget());
}

VisualizeDBToolWidget* vipGetVisualizeDBToolWidget(VipMainWindow* win)
{
	static VisualizeDBToolWidget* instance = new VisualizeDBToolWidget(win);
	return instance;
}

bool vipInitializeVisualizeDBWidget()
{
	if (QFileInfo("./.env").exists()) {

		if (vipHasReadRightsDB()) {

			/*QToolButton* button = new QToolButton();
			button->setAutoRaise(true);
			button->setText("Thermal events DB");
			button->setObjectName("Thermal events DB");
			button->setIcon(vipIcon("database.png"));
			button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
			button->setCheckable(true);
			QAction* act = vipGetMainWindow()->toolsToolBar()->addWidget(button);
			act->setObjectName("Thermal events DB");
			vipGetVisualizeDBToolWidget(vipGetMainWindow())->setButton(button);*/

			// TEST
			QAction* db = vipGetMainWindow()->toolsToolBar()->addAction(vipIcon("DB.png"), "<b>Thermal events DB");
			db->setObjectName("Thermal events DB");
			vipGetVisualizeDBToolWidget(vipGetMainWindow())->setAction(db);

			return true;
		}
	}
	return false;
}
