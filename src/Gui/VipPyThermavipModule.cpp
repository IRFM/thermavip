#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipDragWidget.h"
#include "VipIODevice.h"
#include "VipGui.h"
#include "VipDrawShape.h"
#include "VipPyOperation.h"
#include "VipPyProcessing.h"
#include "VipFunctionTraits.h"
#include "VipProcessingObjectEditor.h"
#include "VipProgress.h"
#include "VipPlayWidget.h"
#include "VipPythonManager.h"

#if VIP_WITH_VTK
#include "VipVTKPlayer.h"
#endif

#include <functional>
#include <qapplication.h>
#include <qgridlayout.h>
#include <qtoolbutton.h>

///////////////////////////////////////////////
// Thermavip module functions
///////////////////////////////////////////////

static QVariant error(const QString& str)
{
	return QVariant::fromValue(VipErrorData(str));
}

static QVariant registerPyProcessing(const QVariant& value)
{
	if (VipPyObjectPtr ptr = value.value<VipPyObjectPtr>()) {
		bool ret = vipRegisterPythonProcessingObject(*ptr);
		if (!ret)
			return error("Unable to register Python processing class");
		return {};
	}
	return error("Unable to register Python processing class: invalid argument");
}

static QVariant userInput(const QString& title, const QVariantList& values)
{
	QWidget* w = new QWidget();
	QGridLayout* lay = new QGridLayout();
	w->setLayout(lay);
	int row = 0;
	QList<QWidget*> widgets;

	for (int i = 0; i < values.size(); ++i) {
		const QVariantList v = values[i].value<QVariantList>();
		if (v.size() < 3 || v[0].userType() != QMetaType::QString || v[1].userType() != QMetaType::QString) {
			delete w;
			return error("Wrong input values");
		}

		QString label = v[0].toString();
		QString type = v[1].toString();

		if (type == "int") {
			lay->addWidget(new QLabel(label), row, 0);
			QSpinBox* spin = new QSpinBox();
			lay->addWidget(spin, row++, 1);
			spin->setValue(v[2].toInt());
			if (v.size() == 4) {
				QVariantList lst = v[3].value<QVariantList>();
				if (lst.size() != 3) {
					delete w;
					return error("'int' type: wrong input values");
				}
				spin->setRange(lst[0].toInt(), lst[1].toInt());
				spin->setSingleStep(lst[2].toInt());
			}
			widgets << spin;
		}
		else if (type == "float") {
			lay->addWidget(new QLabel(label), row, 0);
			QDoubleSpinBox* spin = new QDoubleSpinBox();
			lay->addWidget(spin, row++, 1);
			spin->setValue(v[2].toDouble());
			if (v.size() == 4) {
				QVariantList lst = v[3].value<QVariantList>();
				if (lst.size() != 3) {
					delete w;
					return error("'float' type: wrong input values");
				}
				spin->setRange(lst[0].toDouble(), lst[1].toDouble());
				spin->setSingleStep(lst[2].toDouble());
			}
			widgets << spin;
		}
		else if (type == "bool") {
			QCheckBox* check = new QCheckBox(label);
			lay->addWidget(check, row++, 0, 1, 2);
			check->setChecked(v[2].toBool());
			widgets << check;
		}
		else if (type == "str") {
			lay->addWidget(new QLabel(label), row, 0);
			QComboBox* box = new QComboBox();
			lay->addWidget(box, row++, 1);
			QString default_value = v[2].toString();
			if (v.size() == 4) {
				QVariantList lst = v[3].value<QVariantList>();
				for (int i = 0; i < lst.size(); ++i)
					box->addItem(lst[i].toString());
			}
			else
				box->setEditable(true);
			box->setCurrentText(default_value);

			widgets << box;
		}
		else if (type == "folder") {
			lay->addWidget(new QLabel(label), row, 0);
			VipFileName* w = new VipFileName();
			w->setMode(VipFileName::OpenDir);
			lay->addWidget(w, row++, 1);
			QString default_value = v[2].toString();
			w->setFilename(default_value);
			widgets << w;
		}
		else if (type == "ifile") {
			lay->addWidget(new QLabel(label), row, 0);
			VipFileName* w = new VipFileName();
			w->setMode(VipFileName::Open);
			lay->addWidget(w, row++, 1);
			QString default_value = v[2].toString();
			w->setFilename(default_value);
			if (v.size() == 4) {
				w->setFilters(v[3].toString());
			}
			widgets << w;
		}
		else if (type == "ofile") {
			lay->addWidget(new QLabel(label), row, 0);
			VipFileName* w = new VipFileName();
			w->setMode(VipFileName::Save);
			lay->addWidget(w, row++, 1);
			QString default_value = v[2].toString();
			w->setFilename(default_value);
			if (v.size() == 4) {
				w->setFilters(v[3].toString());
			}
			widgets << w;
		}
		else {
			// A type this chain does not know used to be skipped in silence: the
			// dialog opened with one field fewer, and since the answers are read
			// back by position, every field after it was read as its neighbour.
			return error("unknown input type '" + type + "' for field '" + label + "'");
		}
	}

	VipGenericDialog dialog(w, title, vipGetMainWindow());
	if (dialog.exec() == QDialog::Accepted) {
		QVariantList res;
		for (int i = 0; i < widgets.size(); ++i) {
			if (QSpinBox* s = qobject_cast<QSpinBox*>(widgets[i]))
				res.append(s->value());
			else if (QDoubleSpinBox* s = qobject_cast<QDoubleSpinBox*>(widgets[i]))
				res.append(s->value());
			else if (QCheckBox* s = qobject_cast<QCheckBox*>(widgets[i]))
				res.append(s->isChecked());
			else if (QComboBox* s = qobject_cast<QComboBox*>(widgets[i]))
				res.append(s->currentText());
			else if (VipFileName* s = qobject_cast<VipFileName*>(widgets[i]))
				res.append(s->filename());
		}
		return QVariant::fromValue(res);
	}
	else {
		return QVariant();
	}
}

static QVariant queryPulseOrDate(const QString& title, const QString& default_value)
{
	if (vipQueryFunction()) {
		return QVariant(vipQueryFunction()(title, default_value));
	}
	return error("query function is not implemented");
}

static QVariant workspaceTitle(int id)
{
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
		if (vipGetMainWindow()->displayArea()->widget(i)->id() == id) {
			return (vipGetMainWindow()->displayArea()->widget(i)->windowTitle());
		}
	}

	return error("wrong workspace id");
}

static QVariant setWorkspaceTitle(int id, const QString& title)
{
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
		if (vipGetMainWindow()->displayArea()->widget(i)->id() == id) {
			vipGetMainWindow()->displayArea()->widget(i)->setWindowTitle(title);
			return QVariant();
		}
	}
	return error("wrong workspace id");
}

template<class T>
T getArg(const QVariantMap& kwargs, const QString& name)
{
	auto it = kwargs.find(name);
	if (it == kwargs.end())
		return T{};
	return it.value().value<T>();
}

static VipAnyData toAnyData(const QVariant& p, const QVariantMap& kwargs)
{
	// Convert QVariant to VipAnyData
	VipAnyData ret;
	if (p.userType() == qMetaTypeId<VipAnyData>())
		ret = p.value<VipAnyData>();
	else
		ret.setData(p);

	auto it = kwargs.find("name");
	if (it != kwargs.end())
		ret.setName(it.value().toString());
	it = kwargs.find("x_unit");
	if (it != kwargs.end())
		ret.setXUnit(it.value().toString());
	it = kwargs.find("y_unit");
	if (it != kwargs.end())
		ret.setYUnit(it.value().toString());
	it = kwargs.find("z_unit");
	if (it != kwargs.end())
		ret.setZUnit(it.value().toString());
	it = kwargs.find("stylesheet");
	if (it != kwargs.end())
		ret.setAttribute("stylesheet", it.value());
	return ret;
}

static VipAnyResource* findAnyResource(VipAbstractPlayer* pl, const QString name)
{
	// Find a source VipAnyResource with given data name
	for (auto* display : pl->displayObjects()) {
		for (auto* res : vipListCast<VipAnyResource*>(display->allSources())) {
			if (name.isEmpty())
				return res;
			const auto data = res->outputAt(0)->data();
			if (name.isEmpty() || data.name() == name)
				return res;
		}
	}
	return nullptr;
}

static QVariant display(const QVariantList& lst, const QVariantMap& kwargs) // const QVariant& p, int player = 0, const QString& side = QString())
{
	VipDragWidget* w = nullptr;

	QVariantList data_or_path;
	for (const QVariant& p : lst) {
		if (p.userType() == qMetaTypeId<QVariantList>())
			data_or_path.append(p.value<QVariantList>());
		else
			data_or_path.append(p);
	}

	if (data_or_path.isEmpty())
		return error("empty data");

	// Check for string, in which case we should display a progress widget
	std::unique_ptr<VipProgress> progress;
	for (qsizetype i = 0; i < data_or_path.size(); ++i) {
		const QVariant& p = data_or_path[i];
		if (p.userType() == QMetaType::QString) {
			if (!progress) {
				// Initialize progress bar
				progress.reset(new VipProgress());
			}

			const QString path = p.toString();
			if (path.startsWith("thermavip://") || path.endsWith(".session") || path.endsWith(".hsession")) {
				// Session file or url: open directly
				data_or_path.removeAt(i);
				--i;

				vipGetMainWindow()->openPaths(QStringList() << path);
				continue;
			}

			// Check for file handler
			QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(path, QByteArray());
			if (devices.size() == 1) {
				std::unique_ptr<VipProcessingObject> p(devices.first().create());
				if (VipFileHandler* d = qobject_cast<VipFileHandler*>(p.get())) {
					d->setPath(path);
					if (!d->open(VipIODevice::ReadOnly))
						return error(QString("failed to open file handler (%1)").arg(d->errorString()));

					data_or_path.removeAt(i);
					--i;
					continue;
				}
			}
		}
	}

	if (data_or_path.isEmpty())
		return 0;

	if (progress) {
		progress->setRange(0, data_or_path.size());
		progress->setModal(true);
		progress->setCancelable(true);
	}

	int player = getArg<int>(kwargs, "player");
	QString side = getArg<QString>(kwargs, "side");

	// Check player and side
	if (player == 0 && !side.isEmpty())
		return error("a side was given with an empty player id");

	if (player) {
		w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));

		// Check player id validity
		if (!w)
			return error("cannot find player number " + QString::number(player));

		if (side.isEmpty()) {
			// We are going to add or replace data
			if (auto* video = qobject_cast<VipVideoPlayer*>(w->widget())) {

#if VIP_WITH_VTK
				if (auto* vtk = qobject_cast<VipVTKPlayer*>(video)) {
					// VTK player only accept VTK objects
					for (qsizetype i = 0; i < data_or_path.size(); ++i) {
						if (progress) {
							progress->setValue(i);
							if (progress->canceled())
								return player;
						}

						const auto& p = data_or_path[i];

						if (p.userType() == QMetaType::QString) {
							if (vipCreatePlayersFromPaths(VipPathList() << p.toString(), vtk).isEmpty())
								return error("unable to open path into VTK player");
						}
						else {
							VipAnyData any = toAnyData(p, kwargs);
							VipVTKObject vtkobj = any.data().value<VipVTKObject>();
							if (!vtkobj)
								return error("VTK player only accepts paths or VTK objects");

							if (auto* res = findAnyResource(vtk, any.name()))
								res->setAnyData(any);
							else {
								// Create
								VipAnyResource* d = new VipAnyResource();
								d->setAnyData(any);
								if (vipCreatePlayersFromProcessing(d, vtk).isEmpty()) {
									delete d;
									return error("unable to open VTK object");
								}
							}
						}
					}
					// Success
					return player;
				}
#endif

				// Video player

				if (data_or_path.size() != 1)
					return error("video player only accept one data to display");

				const auto& p = data_or_path.first();
				if (p.userType() == QMetaType::QString)
					return error("cannot open path into an existing video player");
				if (p.userType() != qMetaTypeId<VipAnyData>())
					return error("cannot open given data into video player");
				auto* d = findAnyResource(video, {});
				if (!d)
					return error("cannot open image into existing video player");
				d->setAnyData(toAnyData(p, kwargs));

				return player;
			}

			if (auto* plot = qobject_cast<VipPlotPlayer*>(w->widget())) {

				for (qsizetype i = 0; i < data_or_path.size(); ++i) {
					if (progress) {
						progress->setValue(i);
						if (progress->canceled())
							return player;
					}

					const auto& p = data_or_path[i];

					if (p.userType() == QMetaType::QString) {
						if (vipCreatePlayersFromPaths(VipPathList() << p.toString(), plot).isEmpty())
							return error("unable to open path into plot player");
					}
					else {
						VipAnyData any = toAnyData(p, kwargs);
						int type = any.data().userType();
						if (!(type == qMetaTypeId<VipPointVector>() || type == qMetaTypeId<VipIntervalSampleVector>()))
							return error("unsupported data for plot player");

						if (auto* res = findAnyResource(plot, any.name()))
							res->setAnyData(any);
						else {
							// Create
							VipAnyResource* d = new VipAnyResource();
							d->setAnyData(any);
							if (vipCreatePlayersFromProcessing(d, plot).isEmpty()) {
								delete d;
								return error("unable to open data into existing plot player");
							}
						}
					}
				}
				// Success
				return player;
			}

			return error("unsupported player type");
		}
	}

	int left = 0, right = 0, top = 0, bottom = 0;

	VipMultiDragWidget* mw = nullptr;
	if (player)
		mw = VipMultiDragWidget::fromChild(w);

	if (!side.isEmpty()) {

		// Check that we have a multi drag widget
		if (!mw)
			return error("player number " + QString::number(player) + " is not inside a multi player");

		// Check side validity
		left = side.compare("left", Qt::CaseInsensitive) == 0;
		right = side.compare("right", Qt::CaseInsensitive) == 0;
		top = side.compare("top", Qt::CaseInsensitive) == 0;
		bottom = side.compare("bottom", Qt::CaseInsensitive) == 0;
		int sum = left + top + right + bottom;
		if (sum != 1)
			return error("invalid side value, should one of 'left', 'right', 'top' or 'bottom'");
	}
	else {
		// Create workspace if necessary
		auto* d = vipGetMainWindow()->displayArea();
		if (!d->currentDisplayPlayerArea())
			d->addWidget(new VipDisplayPlayerArea());
	}

	// Create player
	VipAbstractPlayer* new_player = nullptr;
	for (qsizetype i = 0; i < data_or_path.size(); ++i) {
		if (progress) {
			progress->setValue(i);
			if (progress->canceled())
				return player;
		}
		const auto& p = data_or_path[i];

		if (p.userType() == QMetaType::QString) {
			auto lst = vipCreatePlayersFromPaths(VipPathList() << p.toString(), new_player);
			if (lst.size() != 1) {
				qDeleteAll(lst);
				return error("unable to create player from path");
			}
			new_player = lst.first();
		}
		else {
			auto any = toAnyData(p, kwargs);
			auto* d = new VipAnyResource();
			d->setAnyData(any);
			auto lst = vipCreatePlayersFromProcessing(d, new_player);
			if (lst.size() != 1) {
				qDeleteAll(lst);
				delete d;
				return error("unable to create player from data");
			}
			new_player = lst.first();
		}
	}

	if (!side.isEmpty()) {

		// Add to a side
		QPoint pt = mw->indexOf(w);
		VipDragWidget* dw = qobject_cast<VipDragWidget*>(vipCreateFromWidgets(QList<QWidget*>() << new_player));
		if (!dw)
			// Should never happen
			return error("Cannot create a player for the given path(s)");

		if (left)
			mw->insertSub(pt.y(), pt.x(), dw);
		else if (right)
			mw->insertSub(pt.y(), pt.x() + 1, dw);
		else if (top)
			mw->insertMain(pt.y(), dw);
		else
			mw->insertMain(pt.y() + 1, dw);
	}
	else {
		// Open anywhere
		vipGetMainWindow()->openPlayers(AbstractPlayerList() << new_player);
	}
	return QVariant(VipUniqueId::id(VipDragWidget::fromChild(new_player)));
}

static QVariant make_progress(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size())
		return error("invalid arguments number of arguments");

	std::unique_ptr<VipProgress> p(new VipProgress());

	for (auto it = kwargs.begin(); it != kwargs.end(); ++it) {

		if (it.key() == "range") {
			auto lst = it.value().value<QVariantList>();
			if (lst.size() != 2)
				return error("invalide 'range' value");
			p->setRange(lst[0].toDouble(), lst[1].toDouble());
		}
		else if (it.key() == "modal") {
			p->setModal(it.value().toBool());
		}
		else if (it.key() == "cancelable") {
			p->setCancelable(it.value().toBool());
		}
		else if (it.key() == "text") {
			p->setText(it.value().toString());
		}
		else if (it.key() == "value") {
			p->setValue(it.value().toDouble());
		}
	}
	return (quint64)(uintptr_t)p.release();
}
static QVariant destroy_progress(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size() != 1)
		return error("invalid arguments number of arguments");

	auto * p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
	delete p;
	return {};
}
static QVariant progress_set_value(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size() != 2)
		return error("invalid arguments number of arguments");

	auto* p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
	p->setValue(lst[1].toDouble());
	return {};
}
static QVariant progress_value(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size() != 1)
		return error("invalid arguments number of arguments");

	auto* p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
	return p->value();
	return {};
}
static QVariant progress_set_text(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size() != 2)
		return error("invalid arguments number of arguments");

	auto* p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
	p->setText(lst[1].toString());
	return {};
}
static QVariant progress_text(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size() != 1)
		return error("invalid arguments number of arguments");

	auto* p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
	return p->text();
	return {};
}
static QVariant progress_canceled(const QVariantList& lst, const QVariantMap& kwargs)
{
	if (lst.size() != 1)
		return error("invalid arguments number of arguments");

	auto* p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
	return p->canceled();
}


static QVariant closeWindow(int player)
{
	VipBaseDragWidget* w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return error("Cannot find window number " + QString::number(player));
	}
	w->close();
	return QVariant();
}

static QVariant setTimeMarkers(qint64 start, qint64 end)
{
	if (VipDisplayPlayerArea* a = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		a->processingPool()->setTimeLimitsEnable(true);
		a->processingPool()->setStopBeginTime(start);
		a->processingPool()->setStopEndTime(end);
		return QVariant();
	}
	else
		return error("Cannot find a valid workspace");
}
static QVariant removeTimeMarkers()
{
	if (VipDisplayPlayerArea* a = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		a->processingPool()->setTimeLimitsEnable(false);
		return QVariant();
	}
	else
		return error("Cannot find a valid workspace");
}

static QVariant setRowRatio(int row, double ratio)
{
	if (ratio < 0 || ratio > 1) {
		return error("wrong ratio value");
	}
	if (VipDisplayPlayerArea* a = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		int height = a->dragWidgetArea()->height();
		int new_height = height * ratio;
		if (VipMultiDragWidget* mw = a->dragWidgetArea()->findChild<VipMultiDragWidget*>()) {
			QSplitter* vsplitter = mw->mainSplitter();
			if (row >= 0 && row < vsplitter->count()) {
				int current_h = vsplitter->widget(row)->height();
				int to_add = current_h > new_height ? 0 : new_height - current_h;
				int to_remove = current_h < new_height ? 0 : current_h - new_height;

				QList<int> heights = vsplitter->sizes();

				heights[row] = new_height;
				// The rows the difference is spread over, which is every row but this
				// one and the last. There can be none: with two rows the divisor was
				// zero, and an integer division by zero stops the process. Everything
				// else about this call is validated, this alone was not.
				const qsizetype others = heights.size() - 2;
				int to_add_or_remove = 0;
				if (others > 0)
					to_add_or_remove = static_cast<int>((to_add ? to_add : to_remove) / others);
				for (int i = 0; i < heights.size() - 1; ++i) {
					if (i != row) {
						if (to_add)
							heights[i] -= to_add_or_remove;
						else if (to_remove)
							heights[i] += to_add_or_remove;
					}
				}

				vsplitter->setSizes(heights);
				vsplitter->setOpaqueResize(true);
				vsplitter->setProperty("_vip_dirtySplitter", 0);
				return QVariant();
			}
		}
	}

	return error("Cannot find a valid workspace");
}

static QVariant showMaximized(int player)
{
	VipBaseDragWidget* w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return error("Cannot find window number " + QString::number(player));
	}
	w->showMaximized();
	return QVariant();
}
static QVariant showNormal(int player)
{
	VipBaseDragWidget* w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return error("Cannot find window number " + QString::number(player));
	}
	w->showNormal();
	return QVariant();
}
static QVariant showMinimized(int player)
{
	VipBaseDragWidget* w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return error("Cannot find window number " + QString::number(player));
	}
	w->showMinimized();
	return QVariant();
}

static QVariant _workspace(int wks)
{
	if (wks == 0) {
		VipDisplayPlayerArea* area = new VipDisplayPlayerArea();
		vipGetMainWindow()->displayArea()->addWidget(area);
		return QVariant(area->id());
	}

	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
		if (vipGetMainWindow()->displayArea()->widget(i)->id() == wks) {
			vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(vipGetMainWindow()->displayArea()->widget(i));
			return QVariant(vipGetMainWindow()->displayArea()->widget(i)->id());
		}
	}
	return error("Cannot find workspace number " + QString::number(wks));
}

typedef QList<qint64> IntegerList;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
Q_DECLARE_METATYPE(IntegerList)
#endif

static QVariant _workspaces()
{
	qRegisterMetaType<IntegerList>();
	IntegerList res;
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i)
		res.append(vipGetMainWindow()->displayArea()->widget(i)->id());
	return QVariant::fromValue(res);
}

static QVariant currentWorkspace()
{
	if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return QVariant(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->id());
	return QVariant(0);
}

static void resizeSplitter(QSplitter* splitter)
{
	QList<int> sizes;
	for (int i = 0; i < splitter->count(); ++i)
		sizes.append(1);
	splitter->setSizes(sizes);
	splitter->setOpaqueResize(true);
}
static QVariant resize_rows_columns()
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	if (VipMultiDragWidget* m = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->mainDragWidget()) {
		resizeSplitter(m->mainSplitter());
		for (int i = 0; i < m->mainCount(); ++i)
			resizeSplitter(m->subSplitter(i));
	}
	return QVariant();
}

static QVariant currentTime()
{
	if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return QVariant(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->time());
	return error("no valid workspace selected");
}

static QVariant setCurrentTime(qint64 time, const QString& type = "absolute")
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	VipProcessingPool* pool = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool();

	if (type.compare("relative") == 0)
		time += pool->firstTime();

	pool->seek(time);
	// wait for leafs
	VipProcessingObjectList objects = pool->leafs(false);
	for (int i = 0; i < objects.size(); ++i)
		if (objects[i])
			objects[i]->wait();
	// return next time
	return QVariant(pool->nextTime(pool->time()));
}

static QVariant firstTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->firstTime();
	return QVariant::fromValue(res);
}
static QVariant lastTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->lastTime();
	return QVariant::fromValue(res);
}

static QVariant nextTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->nextTime(time);
	return QVariant::fromValue(res);
}
static QVariant previousTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->previousTime(time);
	return QVariant::fromValue(res);
}
static QVariant closestTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->closestTime(time);
	return QVariant::fromValue(res);
}

static QVariant timeRange()
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return error("no valid workspace selected");

	VipTimeRange range = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->timeLimits();
	return QVariant::fromValue(IntegerList() << range.first << range.second);
}

static QVariant clampTime(const VipNDArray& ar, qint64 min, qint64 max)
{
	if (min >= max) {
		return error("wrong min max time values (min >= max)");
	}
	if (ar.isEmpty())
		return QVariant::fromValue(VipNDArray());

	// The array comes from Python. Its rank was never checked, so shape(1) on a
	// one dimensional array read past the shape vector and gave an arbitrary
	// size; and its buffer was reinterpreted as vip_double whatever it held, so
	// an array of four byte elements gave twice its length in reads past the end
	// of the block, and what was read came back to the caller. Convert, which is
	// what every other function of this file does.
	if (ar.shapeCount() != 2 || ar.shape(0) != 2)
		return error("clamp_time expects an array of shape 2xN");

	const VipNDArrayType<vip_double, 2> in = ar;
	if (in.isEmpty())
		return error("clamp_time cannot read this array as numbers");

	const qsizetype size = in.shape(1);
	const vip_double* xptr = in.ptr();
	const vip_double* yptr = xptr + size;

	for (qsizetype i = 1; i < size; ++i) {
		if (xptr[i] <= xptr[i - 1])
			return error("given signal is not continuous");
	}

	QVector<vip_double> x, y;
	x.reserve(size);
	y.reserve(size);
	qsizetype i = 0;
	while (i < size && xptr[i] < min)
		++i;
	while (i < size && xptr[i] <= max) {
		x.append(xptr[i]);
		y.append(yptr[i]);
		++i;
	}

	if (x.isEmpty())
		return QVariant::fromValue(VipNDArray());

	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(2, x.size()));
	vip_double* data = (vip_double*)res.data();
	memcpy(data, x.data(), sizeof(vip_double) * x.size());
	memcpy(data + x.size(), y.data(), sizeof(vip_double) * y.size());
	return QVariant::fromValue(res);
}

static VipDisplayPlotItem* findDisplay(VipPlayer2D* pl, const QString& partial_name)
{
	//"image" is a valid name for video player
	if (partial_name.isEmpty() || (QString("image").indexOf(partial_name) >= 0 && qobject_cast<VipVideoPlayer*>(pl))) {
		VipDisplayPlotItem* item = nullptr;
		if (qobject_cast<VipVideoPlayer*>(pl))
			item = qobject_cast<VipDisplayPlotItem*>(pl->mainDisplayObject());
		else {
			// take the last one
			QList<VipDisplayObject*> disps = pl->displayObjects();
			for (int i = disps.size() - 1; i >= 0; --i)
				if ((item = qobject_cast<VipDisplayPlotItem*>(disps[i])))
					break;
		}
		return item;
	}

	QList<VipDisplayPlotItem*> disps = vipListCast<VipDisplayPlotItem*>(pl->displayObjects());
	if (disps.size() == 0)
		return nullptr;

	for (int i = disps.size() - 1; i >= 0; --i) {
		VipDisplayPlotItem* item = disps[i];
		if (item->inputAt(0)->probe().name().indexOf(partial_name) >= 0 || item->item()->title().text().indexOf(partial_name) >= 0) {
			return item;
		}
	}

	// when multiple signal have the same name, it is possible to add '[index]' in the partial name to select the right one
	if (partial_name.contains("[") && partial_name.contains("]")) {
		int start = partial_name.lastIndexOf("[");
		int end = partial_name.indexOf("]", start);
		if (end < 0 || end != partial_name.size() - 1)
			return nullptr;
		QString num = partial_name.mid(start + 1, end - start - 1);
		bool ok;
		int index = num.toInt(&ok);
		if (!ok || index < 1)
			return nullptr;

		int c = 0;
		QString pname = partial_name.mid(0, start);
		for (int i = 0; i < disps.size(); ++i) {
			VipDisplayPlotItem* item = disps[i];
			if (item->inputAt(0)->probe().name().indexOf(pname) >= 0 || item->item()->title().text().indexOf(pname) >= 0) {
				if (++c == index)
					return item;
			}
		}
	}
	return nullptr;
}

#define PLOT_PLAYER 0  // a 2D player containing curves, histograms, ...
#define VIDEO_PLAYER 1 // a 2D player displaying an image or a video
#define _2D_PLAYER 2   // other type of 2D player
#define OTHER_PLAYER 3 // other type of player

static QVariant playerType(int player)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	if (qobject_cast<VipVideoPlayer*>(pl))
		return QVariant::fromValue(VIDEO_PLAYER);
	else if (qobject_cast<VipPlotPlayer*>(pl))
		return QVariant::fromValue(PLOT_PLAYER);
	else if (qobject_cast<VipPlayer2D*>(pl))
		return QVariant::fromValue(_2D_PLAYER);
	else
		return QVariant::fromValue(OTHER_PLAYER);
}

static QVariant currentPlayer()
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		if (VipDragWidget* drag = area->dragWidgetHandler()->focusWidget())
			if (/* VipAbstractPlayer* pl =*/qobject_cast<VipAbstractPlayer*>(drag->widget()))
				return QVariant::fromValue(VipUniqueId::id(static_cast<VipBaseDragWidget*>(drag)));
	return QVariant(0);
}

static QVariant setSelected(int player, bool selected, const QString& partial_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem* item = findDisplay(pl, partial_name);
	if (!item) {
		return error("cannot find a valid data for name " + partial_name);
	}

	item->item()->setSelected(selected);
	return QVariant();
}

static QVariant unselectAll(int player)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	QList<QGraphicsItem*> items = pl->plotWidget2D()->scene()->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		items[i]->setSelected(false);
	}
	return QVariant();
}

static QVariant itemList(int player, int selection, const QString& partial_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	if (VipVideoPlayer* video = qobject_cast<VipVideoPlayer*>(pl)) {
		//"image" is a valid name for video player
		if (partial_name.isEmpty() || QString("image").indexOf(partial_name) >= 0) {
			if ((selection == 2) || (int)video->spectrogram()->isSelected() == selection)
				return (QVariant(QStringList() << "image"));
		}
		return (QVariant(QStringList()));
	}
	else {
		QList<VipDisplayPlotItem*> disps = vipListCast<VipDisplayPlotItem*>(pl->displayObjects());
		if (disps.size() == 0)
			return (QVariant(QStringList()));

		QStringList res;
		QMultiMap<QString, int> names;
		for (int i = 0; i < disps.size(); ++i) {
			VipDisplayPlotItem* item = disps[i];
			if (selection == 2 || (int)item->item()->isSelected() == selection) {
				QString found;
				if (item->inputAt(0)->probe().name().indexOf(partial_name) >= 0)
					found = (item->inputAt(0)->probe().name());
				else if (item->item()->title().text().indexOf(partial_name) >= 0)
					found = (item->item()->title().text());

				// An item that matches neither used to be counted, named and returned
				// as an empty string, so the caller could not tell no result from all
				// of them rejected, and the second such item was published as "[2]",
				// a name nothing else in this file can read back.
				if (found.isEmpty())
					continue;

				int c = names.count(found);
				names.insert(found, 0);
				if (c == 0) {
					// first occurence
					res.append(found);
				}
				else {
					res.append(found + "[" + QString::number(c + 1) + "]");
					int index = res.indexOf(found);
					if (index >= 0) {
						// replace first occurence
						res[index] = res[index] + "[1]";
					}
				}
			}
		}
		return QVariant(res);
	}
}

// The order the Python wrapper sends: it passes positionally, and the two
// parameters are both strings, so the swap could only show up as "cannot find
// data name", which is what every call produced.
static QVariant setStyleSheet(int player, const QString& stylesheet, const QString& data_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem* item = findDisplay(pl, data_name);
	if (!item)
		return error("cannot find data name for player " + QString::number(player));
	item->setAttribute("stylesheet", stylesheet);
	// The pool of the player this call names, not the one of whatever workspace
	// happens to be in front: the identifier is global, so a player of another
	// workspace had its attribute set and someone else's pipeline reloaded. And
	// the current workspace can be none, which eleven other places in this file
	// already test for.
	if (VipProcessingPool* pool = pl->processingPool())
		pool->reload();
	return QVariant();
}

static QVariant topLevel(int player)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipBaseDragWidget* mw = w->topLevelMultiDragWidget();
	if (!mw)
		return error("cannot find a valid top level window for player number " + QString::number(player));

	return (QVariant(VipUniqueId::id(mw)));
}

static QVariant getData(int player, const QString& data_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem* item = findDisplay(pl, data_name);
	if (!item)
		return error("cannot find data name for player " + QString::number(player));
	return (item->inputAt(0)->data().data());
}

#include "VipProcessingObjectEditor.h"

static QVariant loadData(const QString& path, const QVariant& time)
{
	std::unique_ptr<VipIODevice> device(VipCreateDevice::create(path));
	if (!device)
		return error("no suitable device found for given path");

	if (!device->open(VipIODevice::ReadOnly)) {
		return error("unable to open device of type " + QString(device->metaObject()->className()));
	}
	if (device->outputCount() == 0)
		return error("device has no output");

	if (device->deviceType() == VipIODevice::Temporal) {
		qint64 load_time = device->firstTime();
		if (!time.isNull())
			load_time = time.toLongLong();
		if (!device->read(load_time))
			return error("cannot read first time of device");
	}

	VipAnyData any = device->outputAt(0)->data();
	auto ret = any.attributes();
	ret["data"] = any.data();
	ret["time"] = any.time();
	return QVariant::fromValue(ret);
}

static QVariant clickedPoint()
{
	auto pt = VipAbstractPlotArea::lastGlobalClickedPoint();
	if (pt.item) {
		QVariantList lst;
		lst.push_back(pt.point.x());
		lst.push_back(pt.point.y());
		return QVariant::fromValue(lst);
	}
	return QVariant();
}

static QVariant getDataAttribute(int player, const QString& data_name, const QString& attr_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem* item = findDisplay(pl, data_name);
	if (!item)
		return error("cannot find data name for player " + QString::number(player));
	return item->inputAt(0)->probe().attribute(attr_name);
}

static QVariant getDataAttributes(int player, const QString& data_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem* item = findDisplay(pl, data_name);
	if (!item)
		return error("cannot find data name for player " + QString::number(player));
	QVariantMap map = item->inputAt(0)->probe().attributes();
	return QVariant::fromValue(map);
}

static QVariant getROIPolygon(int player, QString yaxis, const QString& group, int roi)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return error("cannot find a valid shape for given yaxis: " + yaxis);
	VipShape sh = model->sceneModel().find(group, roi);
	if (sh.isNull())
		return error("cannot find a valid shape for given group and id: " + group + ", " + QString::number(roi));

	QPolygonF points;
	if (sh.type() == VipShape::Polyline)
		points = sh.polyline();
	else if (sh.type() == VipShape::Point)
		points << sh.point();
	else {
		points = sh.polygon();
		// points.removeLast();
	}
	VipNDArray y, x;
	if (qobject_cast<VipVideoPlayer*>(pl)) {
		y = VipNDArray(QMetaType::Int, vipVector(points.size()));
		x = VipNDArray(QMetaType::Int, vipVector(points.size()));
		for (int i = 0; i < points.size(); ++i) {
			((int*)(x.data()))[i] = qRound(points[i].x());
			((int*)(y.data()))[i] = qRound(points[i].y());
		}
	}
	else {
		y = VipNDArray(QMetaType::Double, vipVector(points.size()));
		x = VipNDArray(QMetaType::Double, vipVector(points.size()));
		for (int i = 0; i < points.size(); ++i) {
			((double*)(x.data()))[i] = (points[i].x());
			((double*)(y.data()))[i] = (points[i].y());
		}
	}
	return QVariant::fromValue(QVariantList() << QVariant::fromValue(y) << QVariant::fromValue(x));
}

static QVariant getROIBoundingRect(int player, QString yaxis, const QString& group, int roi)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return error("cannot find a valid shape for given yaxis: " + yaxis);
	VipShape sh = model->sceneModel().find(group, roi);
	if (sh.isNull())
		return error("cannot find a valid shape for given group and id: " + group + ", " + QString::number(roi));

	QRectF r = sh.boundingRect();
	return QVariant::fromValue(QVariantList() << r.left() << r.top() << r.width() << r.height());
}

static QVariant getROIPoints(int player, const QString& group, int roi)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid video player for number " + QString::number(player));

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!model)
		return error("cannot find a valid shape for given player");
	VipShape sh = model->sceneModel().find(group, roi);
	if (sh.isNull())
		return error("cannot find a valid shape for given group and id: " + group + ", " + QString::number(roi));
	QVector<QPoint> points = sh.fillPixels();
	VipNDArray y(QMetaType::Int, vipVector(points.size()));
	VipNDArray x(QMetaType::Int, vipVector(points.size()));
	for (int i = 0; i < points.size(); ++i) {
		((int*)(x.data()))[i] = (points[i].x());
		((int*)(y.data()))[i] = (points[i].y());
	}

	return QVariant::fromValue(QVariantList() << QVariant::fromValue(y) << QVariant::fromValue(x));
}

static QVariant clearROIs(int player, const QString& yaxis)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));
	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return error("cannot find a valid shape for given yaxis: " + yaxis);

	model->sceneModel().clear();
	return QVariant();
}

static QVariant addROI(int player, const QVariant& v, const QString& yaxis)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPointVector points;
	VipNDArrayType<vip_double> yx = v.value<VipNDArray>().convert<vip_double>();
	if (yx.isEmpty()) {
		if (v.userType() == qMetaTypeId<QString>() || v.userType() == qMetaTypeId<QByteArray>()) {
			QString filename = v.toString();
			VipShapeList res = vipGetSceneModelWidgetPlayer()->editor()->openShapes(filename, pl);
			if (res.size()) {
				QStringList lst;
				for (int i = 0; i < res.size(); ++i)
					lst.append(res[i].identifier());
				return QVariant::fromValue(lst);
			}
			else
				return error("unable to open file " + filename);
		}
		QVariantList lst = v.value<QVariantList>();
		if (lst.size() != 2)
			return error("wrong ROI value");
		if (lst[0].canConvert<double>() && lst[1].canConvert<double>())
			points.push_back(VipPoint(lst[1].toDouble(), lst[0].toDouble()));
		else if (lst[0].canConvert<QVariantList>() && lst[1].canConvert<QVariantList>()) {
			QVariantList l0 = lst[0].value<QVariantList>();
			QVariantList l1 = lst[1].value<QVariantList>();
			if (l0.size() != l1.size() || l0.size() == 0)
				return error("wrong ROI value");
			for (int i = 0; i < l0.size(); ++i)
				points.push_back(VipPoint(l1[i].toDouble(), l0[i].toDouble()));
		}
		else if (lst[0].canConvert<VipNDArray>() && lst[1].canConvert<VipNDArray>()) {
			VipNDArrayType<vip_double> y = lst[0].value<VipNDArray>().convert<vip_double>();
			VipNDArrayType<vip_double> x = lst[1].value<VipNDArray>().convert<vip_double>();
			if (x.shapeCount() != 1 || y.shapeCount() != 1 || x.size() != y.size() || x.isEmpty())
				return error("wrong ROI value");
			for (int i = 0; i < x.size(); ++i)
				points.push_back(VipPoint(x(i), y(i)));
		}
		else
			return error("wrong ROI value");
	}
	else {
		if (yx.shapeCount() != 2)
			return error("wrong ROI value");
		for (int i = 0; i < yx.shape(1); ++i)
			points.push_back(VipPoint(yx(1, i), yx(0, i)));
	}
	if (points.isEmpty())
		return error("wrong ROI value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return error("cannot find a valid shape for given yaxis: " + yaxis);

	VipShape sh;
	if (points.size() == 1) {
		sh.setPoint(points.last().toPointF());
		model->sceneModel().add("Points", sh);
	}
	else if (points.last() == points.first()) {
		sh.setPolygon(vipToPointF(points));
		model->sceneModel().add("ROI", sh);
	}
	else {
		sh.setPolyline(vipToPointF(points));
		model->sceneModel().add("Polylines", sh);
	}

	return QVariant(sh.identifier());
}

static QVariant addEllipse(int player, const QVariant& v, const QString& yaxis)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	QRectF rect;
	if (v.userType() == qMetaTypeId<QVariantList>()) {
		const QVariantList lst = v.value<QVariantList>();
		if (lst.size() != 4)
			return error("wrong ROI value");
		rect = QRectF(lst[0].toDouble(), lst[1].toDouble(), lst[2].toDouble(), lst[3].toDouble());
	}
	else {
		VipNDArrayType<vip_double> yx = v.value<VipNDArray>().convert<vip_double>();
		if (yx.shapeCount() != 1 || yx.size() != 4)
			return error("wrong ROI value");

		rect = QRectF(yx(0), yx(1), yx(2), yx(3));
	}

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return error("cannot find a valid shape for given yaxis: " + yaxis);

	QPainterPath p;
	p.addEllipse(rect);
	VipShape sh(p);
	model->sceneModel().add("ROI", sh);

	return QVariant(sh.identifier());
}

static QVariant addCircle(int player, double x, double y, double r, const QString& yaxis)
{
	double left = x - r;
	double top = y - r;
	double width = r * 2;
	double height = r * 2;
	QVariantList lst;
	lst << left << top << width << height;
	return addEllipse(player, QVariant::fromValue(lst), yaxis);
}

static QVariant extractTimeTrace(int player, const QVariantList& rois, const QVariantMap& attrs)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid video player for number " + QString::number(player));

	VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid video player for number " + QString::number(player));

	VipPlotPlayer* out = nullptr;
	if (int id = attrs["player"].toInt()) {
		VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(id));
		if (!w)
			return error("cannot find a valid plot player for number " + QString::number(id));
		out = qobject_cast<VipPlotPlayer*>(w->widget());
	}
	int skip = attrs["skip"].toInt();
	if (skip == 0)
		skip = 1;
	int multi = 2;
	if (attrs.find("multi") != attrs.end())
		multi = attrs["multi"].toInt();

	VipPlotSceneModel* sm = pl->plotSceneModel();
	if (!sm)
		return error("cannot find a valid video player for number " + QString::number(player));

	VipShapeList lst;
	for (int i = 0; i < rois.size(); ++i) {
		VipShape sh = sm->sceneModel().find(rois[i].toString());
		if (!sh.isNull())
			lst.append(sh);
	}
	if (lst.size() == 0) {
		return error("no valid ROI given");
	}

	QString stat = attrs["statistics"].toString();
	Vip::ArrayStatistics stats{};
	if (!stat.isEmpty()) {
		if (stat.contains("min"))
			stats |= Vip::Min;
		if (stat.contains("max"))
			stats |= Vip::Max;
		if (stat.contains("mean"))
			stats |= Vip::Mean;
	}

	// launch
	out = vipExtractTimeTrace(lst, pl, stats, skip, multi, out);
	if (!out)
		return error("unable to extract time trace for given ROIs");

	return QVariant(VipUniqueId::id(VipDragWidget::fromChild(out)));
}

static QVariant setDataAttribute(int player, const QString& data_name, const QString& attr_name, const QVariant& value)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem* item = findDisplay(pl, data_name);
	if (!item)
		return error("cannot find data name for player " + QString::number(player));
	if (VipOutput* out = item->inputAt(0)->connection()->source()) {
		out->parentProcessing()->setAttribute(attr_name, value);

		out->parentProcessing()->reload();
	}

	return QVariant();
}

static QVariant removeSignal(int player, const QString& data_name)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	if (data_name.isEmpty()) {
		return error("a valid data name must be specified");
	}
	QList<VipDisplayPlotItem*> disps = vipListCast<VipDisplayPlotItem*>(pl->displayObjects());
	if (disps.size() == 0)
		return error("cannot find data name for player " + QString::number(player));

	int count = 0;
	for (int i = 0; i < disps.size(); ++i) {
		VipDisplayPlotItem* item = disps[i];
		if (item->inputAt(0)->probe().name().indexOf(data_name) >= 0 || item->item()->title().text().indexOf(data_name) >= 0) {
			item->item()->deleteLater();
			++count;
		}
	}
	return QVariant(count);
}

static QVariant setTimeMarker(int player, bool enable)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid plot player for number " + QString::number(player));

	pl->setTimeMarkerVisible(enable);
	return QVariant();
}

static QVariant zoomArea(int player, double x1, double x2, double y1, double y2, const QString& unit)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w->widget())) {
		QRectF rect = pl->visualizedImageRect();
		if (x1 != x2) {
			rect.setLeft(x1);
			rect.setRight(x2);
		}
		if (y1 != y2) {
			rect.setTop(y1);
			rect.setBottom(y2);
		}
		pl->setVisualizedImageRect(rect.normalized());
		return QVariant();
	}
	else if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget())) {
		VipAbstractScale* left = pl->findYScale(unit);
		VipAbstractScale* bottom = pl->xScale();
		if (!left || !bottom)
			return error("cannot find valid axes for player number " + QString::number(player));
		pl->setAutoScale(false);
		if (x1 != x2) {
			bottom->setScale(qMin(x1, x2), qMax(x1, x2));
		}
		if (y1 != y2) {
			left->setScale(qMin(y1, y2), qMax(y1, y2));
		}
		return QVariant();
	}
	else
		return error("cannot find a valid plot player for number " + QString::number(player));
}

static QVariant setColorMapScale(int player, double min, double max, double gripMin, double gripMax)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid video player for number " + QString::number(player));

	if (pl->spectrogram()->colorMap()) {
		pl->setAutomaticColorScale(false);
		if (min != max)
			pl->spectrogram()->colorMap()->setScale(min, max);
		if (gripMin != gripMax)
			pl->spectrogram()->colorMap()->setGripInterval(VipInterval(gripMin, gripMax));
	}
	return QVariant();
}

static QVariant playerRange(int player)
{
	qRegisterMetaType<DoubleList>();

	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget())) {
		VipInterval inter = pl->xScale()->scaleDiv().bounds().normalized();
		return QVariant::fromValue(DoubleList() << inter.minValue() << inter.maxValue());
	}
	if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w->widget())) {
		if (VipDisplayObject* obj = pl->mainDisplayObject()) {
			QList<VipIODevice*> dev = vipListCast<VipIODevice*>(obj->allSources());
			if (dev.size() == 1 && dev.first()->deviceType() == VipIODevice::Temporal) {
				VipTimeRange r = dev.first()->timeLimits();
				return QVariant::fromValue(DoubleList() << r.first << r.second);
			}
		}
	}

	return error("cannot find a valid player or device for number " + QString::number(player));
}

static QVariant autoScale(int player, bool enable)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid plot player for number " + QString::number(player));

	pl->setAutoScale(enable);
	return QVariant();
}
static QVariant setXScale(int player, vip_double min, vip_double max)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid plot player for number " + QString::number(player));

	if (VipAbstractScale* sc = pl->xScale()) {
		sc->setAutoScale(false);
		sc->setScaleDiv(sc->scaleEngine()->divideScale(min, max, sc->maxMajor(), sc->maxMinor()));
		return QVariant();
	}
	return error("cannot find a valid scale for player number " + QString::number(player));
}
static QVariant setYScale(int player, vip_double min, vip_double max, const QString& unit)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid plot player for number " + QString::number(player));

	if (VipAbstractScale* sc = pl->findYScale(unit)) {
		sc->setAutoScale(false);
		sc->setScaleDiv(sc->scaleEngine()->divideScale(min, max, sc->maxMajor(), sc->maxMinor()));
		return QVariant();
	}
	return error("cannot find a valid scale for player number " + QString::number(player));
}

static QVariant xRange(int player)
{
	qRegisterMetaType<DoubleList>();

	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid plot player for number " + QString::number(player));

	QList<VipPlotCurve*> curves = pl->viewer()->area()->findItems<VipPlotCurve*>();
	if (curves.isEmpty())
		return QVariant::fromValue(DoubleList());

	DoubleList res;
	for (int i = 0; i < curves.size(); ++i) {
		if (!curves[i]->isVisible())
			continue;
		vip_double min, max;
		if (curves[i]->rawData().size()) {
			const VipPointVector v = curves[i]->rawData();
			min = max = v.first().x();
			for (int i = 1; i < v.size(); ++i) {
				if (v[i].x() < min)
					min = v[i].x();
				if (v[i].x() > max)
					max = v[i].x();
			}
			if (res.isEmpty())
				res << min << max;
			else {
				res[0] = qMin(min, res[0]);
				res[1] = qMax(max, res[1]);
			}
		}
	}

	return QVariant::fromValue(res);
}

static QVariant setPlayerTitle(int player, const QString& title)
{
	VipBaseDragWidget* base = VipUniqueId::find<VipBaseDragWidget>(player);
	if (!base)
		return error("cannot find a valid player for number " + QString::number(player));

	if (VipMultiDragWidget* w = qobject_cast<VipMultiDragWidget*>(base)) {
		w->setWindowTitle(title);
		return QVariant();
	}

	VipDragWidget* w = qobject_cast<VipDragWidget*>(base);
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	if (!title.isEmpty()) {
		pl->setAutomaticWindowTitle(false);
		pl->setWindowTitle(title);
	}
	else {
		pl->setAutomaticWindowTitle(true);
		if (pl->processingPool())
			pl->processingPool()->reload();
	}
	return QVariant();
}

// annotation functions

// uniquely identifiy each annotation with a map of id -> shape identifier ('player_id:yaxis:group:shape_id')
//  What an annotation identifier refers to. It used to be the four fields pasted
//  into one string with a colon between them, while one of the four comes from
//  the caller and may hold a colon of its own: the split then gave five pieces,
//  removal answered "wrong annotation identifier" for ever, the annotation stayed
//  on screen and the entry was never freed.
struct AnnotationRef
{
	int player = 0;
	QString yaxis;
	QString group;
	int shapeId = 0;
};

static QMap<int, AnnotationRef> _annotations;
static int _createId()
{
	int start = 1;
	for (QMap<int, AnnotationRef>::const_iterator it = _annotations.begin(); it != _annotations.end(); ++it, ++start) {
		if (it.key() != start)
			return start;
	}
	return start;
}

#include "VipAnnotationEditor.h"

static QVariant createAnnotation(int player, const QString& type, const QString& text, const QList<double>& pos, const QVariantMap& attributes)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	if (!(pos.size() == 2 || pos.size() == 4))
		return error("wrong position (should a list of 2 or 4 values, start coordinates and optional end coordinates)");

	QPointF start(pos[0], pos[1]);
	QPointF end = pos.size() == 4 ? QPointF(pos[2], pos[3]) : QPointF();
	QString err;
	QString yaxis = attributes["yaxis"].toString();

	VipAnnotation* a = vipAnnotation(pl, type, text, start, end, attributes, yaxis, &err);
	if (!a) {
		return error(err);
	}

	// Tested like the annotation above it: the shape it hangs from is a pointer
	// like any other.
	VipPlotShape* parent_shape = a->parentShape();
	if (!parent_shape)
		return error("the annotation has no shape");

	VipShape sh = parent_shape->rawData();
	int id = _createId();
	sh.setAttribute("_vip_annotation_id", id);
	_annotations[id] = AnnotationRef{ player, yaxis, sh.group(), (int)sh.id() };

	return QVariant(id);
}

static QVariant clearAnnotation(int id)
{

	QMap<int, AnnotationRef>::const_iterator it = _annotations.find(id);
	if (it == _annotations.end())
		return error("wrong annotation identifier");

	const AnnotationRef ref = it.value();
	const int player = ref.player;
	const QString yaxis = ref.yaxis;
	const QString group = ref.group;
	const int sh_id = ref.shapeId;

	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty()) {
		if (VipPlotPlayer* p = qobject_cast<VipPlotPlayer*>(pl)) {
			if (VipAbstractScale* scale = p->findYScale(yaxis)) {
				model = p->findPlotSceneModel(QList<VipAbstractScale*>() << p->xScale() << scale);
			}
		}
	}
	if (!model)
		return error("wrong annotation identifier");

	VipShape sh = model->sceneModel().find(group, sh_id);
	if (sh.isNull())
		return error("wrong annotation identifier");

	_annotations.remove(id);
	model->sceneModel().remove(sh);
	return QVariant();
}

static QVariant clearAnnotations(int player, bool all)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return error("cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return error("cannot find a valid player for number " + QString::number(player));

	// only remove python annotations (ones with _vip_annotation_id attribute)
	QList<VipPlotSceneModel*> models = pl->plotSceneModels();
	for (int i = 0; i < models.size(); ++i) {
		QList<VipPlotShape*> shapes = models[i]->shapes();
		VipShapeList to_remove;
		for (int j = 0; j < shapes.size(); ++j) {
			if (shapes[j]->annotation()) {
				int id = shapes[j]->rawData().attribute("_vip_annotation_id").toInt();
				if (id)
					_annotations.remove(id);
				if (id || all)
					to_remove << shapes[j]->rawData();
			}
		}
		models[i]->sceneModel().remove(to_remove);
	}

	// vipRemoveAllAnnotations(pl);
	return QVariant();
}

static QVariant play(bool enable)
{
	auto* wks = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (!wks)
		return error("no workspace available");

	if (enable)
		wks->processingPool()->play();
	else
		wks->processingPool()->stop();
	return {};
}

static QVariant repeat(bool enable)
{
	auto* wks = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (!wks)
		return error("no workspace available");

	wks->processingPool()->setRepeat(enable);
	return {};
}

static QVariant stream(bool enable)
{
	auto* wks = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (!wks)
		return error("no workspace available");

	wks->processingPool()->setStreamingEnabled(enable);
	return {};
}

static QVariant get_generate_menu()
{
	return vipToVariant(vipGetMainWindow()->generateMenu());
}

static QVariant get_main_window()
{
	return vipToVariant(vipGetMainWindow());
}

static QVariant get_tool_widgets()
{

	QVariantList ret;
	for (auto* tool : vipGetMainWindow()->findChildren<VipToolWidget*>())
		ret.push_back(vipToVariant(tool));
	return vipToVariant(ret);
}

static QVariant get_tool_widget(const QString& name)
{
	auto* tool = vipGetMainWindow()->findChild<VipToolWidget*>(name);
	if (tool)
		return vipToVariant(tool);
	return {};
}

static QVariant add_tool_widget(const VipPyObjectPtr& widget, bool floating, int orientation, int area, int allowed_areas)
{
	if (!widget || !(*widget))
		return error("invalid 'widget' argument");

	QWidget* w = qobject_cast<QWidget*>(vipQObjectFromPython(*widget));
	if (!w)
		return error("invalid 'widget' argument");

	auto a = (Qt::DockWidgetArea)area;
	auto allowed = (Qt::DockWidgetAreas)allowed_areas;
	if (!(allowed & a))
		allowed = a;

	QString title = w->windowTitle();
	VipToolWidget* tool = new VipToolWidget(vipGetMainWindow());
	tool->setWindowTitle(title);
	tool->setObjectName(title);
	tool->setWindowIcon(w->windowIcon());
	tool->setWidget(w, (Qt::Orientation)orientation);
	tool->setAllowedAreas((Qt::DockWidgetAreas)allowed);
	vipGetMainWindow()->addDockWidget((Qt::DockWidgetArea)a, tool);
	tool->setFloating(floating);
	return vipToVariant(VipPyObjectPtr(new VipPyObject(vipPythonFromQObject(tool))));
}


/* static QVariant addFunction(int player, PyObject* fun, const QString& fun_name, const QString& item_name)
{
	//find the player
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return error( "cannot find a valid player for number " + QString::number(player));
	}

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl) {
		return error( "cannot find a valid plot player for number " + QString::number(player));
	}

	VipDisplayPlotItem * item = findDisplay(pl, item_name);
	if (!item) {
		return error( "cannot find a valid data for name " + item_name);
	}

	QList<VipProcessingList *> lst = vipListCast<VipProcessingList*>(item->allSources());
	if (!lst.size()) {
		return error( "cannot find a valid data for name " + item_name);
	}

	VipProcessingList * p = lst.first();

	//find a VipPyFunctionProcessing with given name
	bool found = false;
	for (int i = 0; i < p->size(); ++i) {
		if (VipPyFunctionProcessing * proc = qobject_cast<VipPyFunctionProcessing*>(p->at(i)))
			if (proc->property("_vip_processingName").toString() == fun_name) {
				proc->setFunction(fun);
				found = true;
				break;
			}
	}
	if (!found) {
		VipPyFunctionProcessing * proc = new VipPyFunctionProcessing();
		proc->setFunction(fun);
		proc->setProperty("_vip_processingName", fun_name);
		p->append(proc);
	}

	vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->reload();
	return QVariant();
}

static QVariant getFunction(int player, const QString & fun_name, const QString & item_name)
{
	//find the player
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return error( "cannot find a valid player for number " + QString::number(player));
	}

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl) {
		return error( "cannot find a valid plot player for number " + QString::number(player));
	}

	VipDisplayPlotItem * item = findDisplay(pl, item_name);
	if (!item) {
		return error( "cannot find a valid data for name " + item_name);
	}

	QList<VipProcessingList *> lst = vipListCast<VipProcessingList*>(item->allSources());
	if (!lst.size()) {
		return error( "cannot find a valid data for name " + item_name);
	}

	VipProcessingList * p = lst.first();

	//find a VipPyFunctionProcessing with given name
	VipPyFunctionProcessing* found = nullptr;
	for (int i = 0; i < p->size(); ++i) {
		if (VipPyFunctionProcessing * proc = qobject_cast<VipPyFunctionProcessing*>(p->at(i)))
			if (proc->property("_vip_processingName").toString() == fun_name) {
				found = proc;
				break;
			}
	}
	if (!found) {
		return error( "unable to find processing " + fun_name);
	}
	return ResultType(QVariant::fromValue((VipProcessingObject*)found), QString());
}*/

#include <QLibraryInfo>

static QVariant _can_load_qtpy()
{
	bool ret = !QLibraryInfo::isDebugBuild();
	return ret;
}

static QVariant _update_device_output(quint64 addr, const QString & name, int index)
{
	VipPyWrapperIODevice::updateDeviceOutput(addr, name, index);
	return {};
}

static int registerFunctions()
{
	// Private functions
	vipRegisterFunction(vipMakeFunctionObject(_can_load_qtpy, "_can_load_qtpy", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(_update_device_output, "_update_device_output", {}, false));

	// VipProgress wrapper functions
	vipRegisterFunction(make_progress, "make_progress",{},false);
	vipRegisterFunction(destroy_progress, "destroy_progress", {}, false);
	vipRegisterFunction(progress_set_value, "progress_set_value", {}, false);
	vipRegisterFunction(progress_value, "progress_value", {}, false);
	vipRegisterFunction(progress_set_text, "progress_set_text", {}, false);
	vipRegisterFunction(progress_text, "progress_text", {}, false);
	vipRegisterFunction(progress_canceled, "progress_canceled", {}, false);


	vipRegisterFunction(vipMakeFunctionObject(registerPyProcessing, "register_processing"));
	vipRegisterFunction(vipMakeFunctionObject(playerType, "player_type"));
	vipRegisterFunction(vipMakeFunctionObject(itemList, "item_list"));
	vipRegisterFunction(vipMakeFunctionObject(setSelected, "set_selected"));
	vipRegisterFunction(vipMakeFunctionObject(unselectAll, "unselect_all"));
	vipRegisterFunction(vipMakeFunctionObject(queryPulseOrDate, "query"));
	vipRegisterFunction(display, "open");
	vipRegisterFunction(vipMakeFunctionObject(closeWindow, "close"));
	vipRegisterFunction(vipMakeFunctionObject(showMaximized, "show_maximized"));
	vipRegisterFunction(vipMakeFunctionObject(showMinimized, "show_minimized"));
	vipRegisterFunction(vipMakeFunctionObject(showNormal, "show_normal"));
	vipRegisterFunction(vipMakeFunctionObject(_workspace, "workspace"));
	vipRegisterFunction(vipMakeFunctionObject(_workspaces, "workspaces"));
	vipRegisterFunction(vipMakeFunctionObject(currentWorkspace, "current_workspace"));
	vipRegisterFunction(vipMakeFunctionObject(workspaceTitle, "workspace_title"));
	vipRegisterFunction(vipMakeFunctionObject(setWorkspaceTitle, "set_workspace_title"));
	vipRegisterFunction(vipMakeFunctionObject(currentTime, "time"));
	vipRegisterFunction(vipMakeFunctionObject(setCurrentTime, "set_time"));
	vipRegisterFunction(vipMakeFunctionObject(firstTime, "first_time"));
	vipRegisterFunction(vipMakeFunctionObject(lastTime, "last_time"));
	vipRegisterFunction(vipMakeFunctionObject(nextTime, "next_time"));
	vipRegisterFunction(vipMakeFunctionObject(previousTime, "previous_time"));
	vipRegisterFunction(vipMakeFunctionObject(closestTime, "closest_time"));
	vipRegisterFunction(vipMakeFunctionObject(timeRange, "time_range"));
	vipRegisterFunction(vipMakeFunctionObject(setStyleSheet, "set_stylesheet"));
	vipRegisterFunction(vipMakeFunctionObject(clampTime, "clamp_time"));

	vipRegisterFunction(vipMakeFunctionObject(topLevel, "top_level"));
	vipRegisterFunction(vipMakeFunctionObject(resize_rows_columns, "resize_workspace"));
	vipRegisterFunction(vipMakeFunctionObject(getData, "get"));
	vipRegisterFunction(vipMakeFunctionObject(loadData, "load"));
	vipRegisterFunction(vipMakeFunctionObject(clickedPoint, "clicked_point"));
	vipRegisterFunction(vipMakeFunctionObject(getDataAttribute, "get_attribute"));
	vipRegisterFunction(vipMakeFunctionObject(getDataAttributes, "get_attributes"));
	vipRegisterFunction(vipMakeFunctionObject(setDataAttribute, "set_attribute"));
	vipRegisterFunction(vipMakeFunctionObject(getROIPolygon, "get_roi"));
	vipRegisterFunction(vipMakeFunctionObject(getROIBoundingRect, "get_roi_bounding_rect"));
	vipRegisterFunction(vipMakeFunctionObject(getROIPoints, "get_roi_filled_points"));
	vipRegisterFunction(vipMakeFunctionObject(clearROIs, "clear_roi"));

	vipRegisterFunction(vipMakeFunctionObject(addROI, "add_roi"));
	vipRegisterFunction(vipMakeFunctionObject(addEllipse, "add_ellipse"));
	vipRegisterFunction(vipMakeFunctionObject(addCircle, "add_circle"));
	vipRegisterFunction(vipMakeFunctionObject(extractTimeTrace, "time_trace"));
	vipRegisterFunction(vipMakeFunctionObject(removeSignal, "remove"));
	vipRegisterFunction(vipMakeFunctionObject(setTimeMarker, "set_time_marker"));
	vipRegisterFunction(vipMakeFunctionObject(zoomArea, "zoom"));
	vipRegisterFunction(vipMakeFunctionObject(setColorMapScale, "set_color_map_scale"));
	vipRegisterFunction(vipMakeFunctionObject(xRange, "x_range"));
	vipRegisterFunction(vipMakeFunctionObject(playerRange, "player_range"));
	vipRegisterFunction(vipMakeFunctionObject(currentPlayer, "current_player"));
	vipRegisterFunction(vipMakeFunctionObject(setTimeMarkers, "set_time_markers"));
	vipRegisterFunction(vipMakeFunctionObject(removeTimeMarkers, "remove_time_markers"));

	vipRegisterFunction(vipMakeFunctionObject(setRowRatio, "set_row_ratio"));
	vipRegisterFunction(vipMakeFunctionObject(setPlayerTitle, "set_title"));
	vipRegisterFunction(vipMakeFunctionObject(createAnnotation, "annotation"));
	vipRegisterFunction(vipMakeFunctionObject(clearAnnotation, "remove_annotation"));
	vipRegisterFunction(vipMakeFunctionObject(clearAnnotations, "clear_annotations"));
	vipRegisterFunction(vipMakeFunctionObject(autoScale, "auto_scale"));
	vipRegisterFunction(vipMakeFunctionObject(setXScale, "set_x_scale"));
	vipRegisterFunction(vipMakeFunctionObject(setYScale, "set_y_scale"));
	vipRegisterFunction(vipMakeFunctionObject(setXScale, "set_x_scale"));
	vipRegisterFunction(vipMakeFunctionObject(setXScale, "set_x_scale"));
	vipRegisterFunction(vipMakeFunctionObject(userInput, "user_input"));

	vipRegisterFunction(vipMakeFunctionObject(play, "play"));
	vipRegisterFunction(vipMakeFunctionObject(stream, "stream"));
	vipRegisterFunction(vipMakeFunctionObject(repeat, "repeat"));

	// GUI functions

	vipRegisterFunction(vipMakeFunctionObject(get_generate_menu, "get_generate_menu"));
	vipRegisterFunction(vipMakeFunctionObject(get_main_window, "get_main_window"));
	vipRegisterFunction(vipMakeFunctionObject(get_tool_widgets, "get_tool_widgets"));
	vipRegisterFunction(vipMakeFunctionObject(get_tool_widget, "get_tool_widget"));
	vipRegisterFunction(vipMakeFunctionObject(add_tool_widget, "add_tool_widget"));

	return 0;
}

static int _registerFunction = vipStaticInit("registerFunctions", registerFunctions);

static QWidget* editVipPyWrapperProcessing(VipPyWrapperProcessing* obj)
{
	VipGILLocker lock;
	auto proc = obj->processingObject();
	if (!proc)
		return nullptr;

	auto editor = proc.callMember("editor");
	if (QObject* ed = vipQObjectFromPython(editor)) {
		return qobject_cast<QWidget*>(ed);
	}
	return nullptr;
}

static QWidget* editVipPyWrapperIODevice(VipPyWrapperIODevice* obj)
{
	VipGILLocker lock;
	auto proc = obj->processingObject();
	if (!proc)
		return nullptr;

	auto editor = proc.callMember("editor");
	if (QObject* ed = vipQObjectFromPython(editor))
		return qobject_cast<QWidget*>(ed);
	return nullptr;
}

static void customizePlayerPyIODevice(VipPlayer2D* player)
{
	if (player->property("_vip_PyIODevice").toBool())
		return;

	QSet<VipPyWrapperIODevice*> dev;
	for (VipDisplayObject* display : player->displayObjects()) {
		auto src = vipListCast<VipPyWrapperIODevice*>(display->allSources());
		for (auto* d : src)
			dev.insert(d);
	}
	if (dev.size() == 1) {

		if (QWidget* toolbar = qobject_cast<QWidget*>((*dev.begin())->toolBar()))
			player->toolBar()->addWidget(toolbar);

		player->setProperty("_vip_PyIODevice", true);
	}
}

static int registerWrapperEditors()
{
	vipFDObjectEditor().append<QWidget*(VipPyWrapperProcessing*)>(editVipPyWrapperProcessing);
	vipFDObjectEditor().append<QWidget*(VipPyWrapperIODevice*)>(editVipPyWrapperIODevice);
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(customizePlayerPyIODevice);
	return 0;
}

static int _registerEditors = vipStaticInit("registerWrapperEditors", registerWrapperEditors);

#include "VipProcessingObjectEditor.h"

static uint64_t from_ptr(void* p)
{
	return (uint64_t)(uintptr_t)p;
}
template<class T>
static T* to_ptr(uint64_t addr)
{
	return (T*)(uintptr_t)addr;
}

uint64_t _py_create(const char* path, int mode)
{
	VipProcessingObjectInfoList lst;
	if (mode == VipIODevice::ReadOnly)
		lst = VipIODevice::possibleReadDevices(QString(path), {});
	else
		lst = VipIODevice::possibleWriteDevices(QString(path), {});
	if (lst.size() > 1)
		lst = lst.mid(0, 1);
	else if (lst.isEmpty()) {
		VipPyError::set("no device found for given path");
		return 0;
	}
	auto dev = static_cast<VipIODevice*>(lst.front().create());
	dev->setPath(path);
	return from_ptr(dev);
}

void _py_open(uint64_t addr, int mode)
{
	if (mode != VipIODevice::ReadOnly && mode != VipIODevice::WriteOnly)
		return VipPyError::set("invalid 'mode' argument");
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");

	if (!dev->open(VipIODevice::OpenModes(mode)))
		VipPyError::set("unable to open given path: " + (dev ? dev->errorString() : QString()));
}

void _py_close(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");

	dev->close();
}

void _py_destroy(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return;
	dev->close();
	delete dev;
}

int _py_input_count(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->inputCount();
}

int _py_output_count(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->outputCount();
}
int _py_property_count(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->propertyCount();
}

void _py_set_input_count(uint64_t addr, int count)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");
	if (dev->inputCount() == count)
		return;
	if (dev->topLevelInputCount() == 1 && dev->topLevelInputAt(0)->toMultiInput())
		dev->topLevelInputAt(0)->toMultiInput()->resize(count);
	else
		VipPyError::set("cannot resize inputs");
}
void _py_set_property_count(uint64_t addr, int count)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");
	if (dev->propertyCount() == count)
		return;
	if (dev->topLevelPropertyCount() == 1 && dev->topLevelPropertyAt(0)->toMultiProperty())
		dev->topLevelPropertyAt(0)->toMultiProperty()->resize(count);
	else
		VipPyError::set("cannot resize properties");
}

void _py_set_output_count(uint64_t addr, int count)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");
	if (dev->outputCount() == count)
		return;
	if (dev->topLevelOutputCount() == 1 && dev->topLevelOutputAt(0)->toMultiOutput())
		dev->topLevelOutputAt(0)->toMultiOutput()->resize(count);
	else
		VipPyError::set("cannot resize outputs");
}

void* _py_input_name(uint64_t addr, int i)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->inputCount()) {
		VipPyError::set("invalid device descriptor or input index");
		return nullptr;
	}
	return vipVariantToPython(dev->inputAt(i)->name());
}
void* _py_property_name(uint64_t addr, int i)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->propertyCount()) {
		VipPyError::set("invalid device descriptor or property index");
		return nullptr;
	}
	return vipVariantToPython(dev->propertyAt(i)->name());
}
void* _py_output_name(uint64_t addr, int i)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->outputCount()) {
		VipPyError::set("invalid device descriptor or output index");
		return nullptr;
	}
	return vipVariantToPython(dev->outputAt(i)->name());
}

void _py_set_input(uint64_t addr, int i, void* value)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->inputCount())
		return VipPyError::set("invalid device descriptor or IO index");
	dev->inputAt(i)->setData(vipPythonToVariant(value).value<VipAnyData>());
}
void _py_set_property(uint64_t addr, int i, void* value)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->propertyCount())
		return VipPyError::set("invalid device descriptor or IO index");
	dev->propertyAt(i)->setData(vipPythonToVariant(value).value<VipAnyData>());
}
void* _py_get_input(uint64_t addr, int i)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->inputCount()) {
		VipPyError::set("invalid device descriptor or IO index");
		return nullptr;
	}
	return vipAnyDataToPython(dev->inputAt(i)->data());
}
void* _py_get_output(uint64_t addr, int i)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->outputCount()) {
		VipPyError::set("invalid device descriptor or IO index");
		return nullptr;
	}
	return vipAnyDataToPython(dev->outputAt(i)->data());
}
void* _py_get_property(uint64_t addr, int i)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev || i >= dev->propertyCount()) {
		VipPyError::set("invalid device descriptor or IO index");
		return nullptr;
	}
	return vipAnyDataToPython(dev->propertyAt(i)->data());
}

void _py_apply(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");
	dev->update();
}

void _py_read(uint64_t addr, int64_t time)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev)
		return VipPyError::set("invalid device descriptor");
	dev->read(time, true);
}

int64_t _py_pos_to_time(uint64_t addr, int64_t pos)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->posToTime(pos);
}

int64_t _py_time_to_pos(uint64_t addr, int64_t time)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->timeToPos(time);
}
int64_t _py_closest_time(uint64_t addr, int64_t time)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->closestTime(time);
}
int64_t _py_get_size(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->size();
}
void* _py_time_window(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return nullptr;
	}
	auto lst = dev->timeWindow();
	if (lst.isEmpty())
		return vipVariantToPython(vipToVariant(QVariantList() << (QVariantList() << VipInvalidTime << VipInvalidTime)));

	return vipVariantToPython(vipToVariant(QVariantList() << (QVariantList() << lst.front().first << lst.back().second)));
}

int _py_device_type(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return (int)dev->deviceType();
}