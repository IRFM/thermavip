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
#include "VipLogging.h"
#include "VipEnvironment.h"
#include "VipSearchLineEdit.h"

#if VIP_WITH_VTK
#include "VipVTKPlayer.h"
#include "VipVTKGraphicsView.h"
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

static int workspaceId(VipDisplayPlayerArea* w)
{
	int ret = w->id();
	// Combine with pointer hash
	size_t wh = vipHashValue((uintptr_t)w) & ((1ull << 15ull) - 1ull);
	return (ret & ((1 << 16) - 1)) | ((int)wh << 16);
}

static VipDisplayPlayerArea* findWorkspace(int id)
{
	id = id & ((1 << 16) - 1);
	auto* disp = vipGetMainWindow()->displayArea();
	for (int i = 0; i < disp->count(); ++i)
		if (disp->widget(i)->id() == id)
			return disp->widget(i);
	return nullptr;
}

static int playerId(QWidget* w)
{
	w = qobject_cast<VipDragWidget*>(VipBaseDragWidget::fromChild(w));
	if (!w)
		return 0;
	int ret = VipUniqueId::id(static_cast<VipBaseDragWidget*>(w));
	// Combine with pointer hash
	size_t wh = vipHashValue((uintptr_t)w) & ((1ull << 15ull) - 1ull);
	return (ret & ((1 << 16) - 1)) | ((int)wh << 16);
}

static VipDragWidget* findDragWidget(int id)
{
	id = id & ((1 << 16) - 1);
	return qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(id));
}

static VipAbstractPlayer* findPlayer(int id)
{
	auto* w = findDragWidget(id);
	if (!w)
		return nullptr;
	return qobject_cast<VipAbstractPlayer*>(w->widget());
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
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	return wks->windowTitle();
}

static QVariant setWorkspaceTitle(int id, const QString& title)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	wks->setWindowTitle(title);
	return {};
}

static QVariant workspace_floating(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	return wks->isFloating();
}

static QVariant set_workspace_floating(int id, bool enable)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	wks->setFloating(enable);
	return workspaceId(wks);
}

static QVariant setWorkspaceMaxColumns(int cols)
{
	if (cols < 1)
		return error("invalid column count");
	VipDisplayPlayerArea::setDefaultMaximumWidth(cols);
	return {};
}

static QVariant workspaceMaxColumns()
{
	return VipDisplayPlayerArea::defaultMaximumWidth();
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

static QVariant createDevice(const QVariant& p)
{
	if (p.userType() == qMetaTypeId<VipPyObjectPtr>()) {
		// We accept VipIODevice
		VipPyObjectPtr ptr = p.value<VipPyObjectPtr>();
		if (!ptr || !(*ptr))
			return error("invalid or null object passed");

		VipPyObject obj = *ptr;
		auto objType = obj.type();
		// VipGILLocker ll;
		if (vipPyIsIODevice(objType)) {
			// This is a VipIODevice
			VipPyWrapperIODevice* d = new VipPyWrapperIODevice();
			if (!d->initializeProcessing(vipToVariant(obj)).toBool()) {
				delete d;
				return error("unable to create device");
			}
			return vipToVariant(d);
		}
	}
	return {};
}

static QVariant handleShapes(const VipAnyData& any, VipPlayer2D* player = nullptr, int canvas = 0)
{
	auto* plot = qobject_cast<VipPlotPlayer*>(player);
	auto* target = plot ? plot->canvas(canvas) : nullptr;

	if (any.data().userType() == qMetaTypeId<VipShape>()) {
		// Open a shape within default video scene model (ROIs)
		VipShape sh = any.data().value<VipShape>();
		if (sh.type() == VipShape::Point)
			sh.setGroup("Points");
		else if (sh.type() == VipShape::Polyline)
			sh.setGroup("Polylines");
		else
			sh.setGroup("ROI");
		if (player) {
			if (plot) {
				plot->canvas(canvas);
				VipAbstractScale* sc = plot->leftScales()[canvas];
				if (auto* model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc))
					model->sceneModel().add(sh);
				else
					return error("unable to find scene model for given canvas index");
			}
			else
				player->plotSceneModel()->sceneModel().add(sh);
			return vipToVariant(player);
		}
		else {
			auto lst = vipCreatePlayersFromData(any, nullptr);
			if (lst.isEmpty())
				return error("unable to open VipShape object");
			return vipToVariant(lst.first());
		}
	}
	if (any.data().userType() == qMetaTypeId<VipSceneModel>()) {
		// Open a scene model with a device
		if (player) {
			auto* dev = findAnyResource(player, any.name());
			if (!dev || dev->data().userType() != qMetaTypeId<VipSceneModel>()) {
				dev = new VipAnyResource();
				dev->setAnyData(any);
				if (vipCreatePlayersFromProcessing(dev, player, nullptr, target).isEmpty()) {
					delete dev;
					return error("unable to open device on video player");
				}
			}
			else
				dev->setAnyData(any);
			return vipToVariant(player);
		}
		else {
			auto lst = vipCreatePlayersFromData(any, nullptr);
			if (lst.isEmpty())
				return error("unable to open VipSceneModel object");
			return vipToVariant(lst.first());
		}
	}
	return {};
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
		return error("empty input data");

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
	int canvas = getArg<int>(kwargs, "canvas");
	QString side = getArg<QString>(kwargs, "side");

	// Check player and side
	if (player == 0 && !side.isEmpty())
		return error("a side was given with an empty player id");

	if (player) {
		w = findDragWidget(player);

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
						auto d = createDevice(p);
						if (auto* dev = d.value<VipIODevice*>()) {
							if (vipCreatePlayersFromProcessing(dev, vtk).isEmpty()) {
								delete dev;
								return error("unable to open device on VTK player");
							}
							continue;
						}
						if (d.userType() != 0)
							return d;
						if (p.userType() == QMetaType::QString) {
							if (vipCreatePlayersFromPaths(VipPathList() << p.toString(), vtk).isEmpty())
								return error("unable to open path into VTK player");
						}
						else {
							if (QObject* obj = vipQObjectFromPython(p)) {
								return error("cannot open widget in existing player");
							}

							VipAnyData any = toAnyData(p, kwargs);
							auto ret = handleShapes(any, vtk);
							if (ret.value<VipVideoPlayer*>())
								continue;
							if (ret.userType() != 0)
								return ret;

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
				for (qsizetype i = 0; i < data_or_path.size(); ++i) {
					if (progress) {
						progress->setValue(i);
						if (progress->canceled())
							return player;
					}

					const auto& p = data_or_path[i];

					auto d = createDevice(p);
					if (auto* dev = d.value<VipIODevice*>()) {
						if (vipCreatePlayersFromProcessing(dev, video).isEmpty()) {
							delete dev;
							return error("unable to open device on video player");
						}
						continue;
					}
					if (d.userType() != 0)
						return d;
					if (p.userType() == QMetaType::QString) {
						if (vipCreatePlayersFromPaths(VipPathList() << p.toString(), video).isEmpty())
							return error("unable to open path into video player");
						else
							continue;
					}

					if (vipQObjectFromPython(p))
						return error("cannot open widget in existing player");

					auto any = toAnyData(p, kwargs);
					auto ret = handleShapes(any, video);
					if (ret.value<VipVideoPlayer*>())
						continue;
					if (ret.userType() != 0)
						return ret;

					if (any.data().userType() != qMetaTypeId<VipNDArray>())
						return error("cannot open given data into video player");
					auto* dev = findAnyResource(video, {});
					if (!dev)
						return error("cannot open image into existing video player");
					dev->setAnyData(toAnyData(p, kwargs));
				}
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

					auto d = createDevice(p);
					if (auto* dev = d.value<VipIODevice*>()) {
						if (vipCreatePlayersFromProcessing(dev, plot, nullptr, plot->canvas(canvas)).isEmpty()) {
							delete dev;
							return error("unable to open device on plot player");
						}
						continue;
					}
					if (d.userType() != 0)
						return d;
					if (vipQObjectFromPython(p)) {
						return error("cannot open widget in existing player");
					}
					if (p.userType() == QMetaType::QString) {
						if (vipCreatePlayersFromPaths(VipPathList() << p.toString(), plot, plot->canvas(canvas)).isEmpty())
							return error("unable to open path into plot player");
					}
					else {
						VipAnyData any = toAnyData(p, kwargs);

						auto ret = handleShapes(any, plot, canvas);
						if (ret.value<VipPlotPlayer*>())
							continue;
						if (ret.userType() != 0)
							return ret;

						int type = any.data().userType();
						if (!(type == qMetaTypeId<VipPointVector>() || type == qMetaTypeId<VipIntervalSampleVector>()))
							return error("unsupported data for plot player");

						if (auto* res = findAnyResource(plot, any.name()))
							res->setAnyData(any);
						else {
							// Create
							VipAnyResource* dev = new VipAnyResource();
							dev->setAnyData(any);
							if (vipCreatePlayersFromProcessing(dev, plot, nullptr, plot->canvas(canvas)).isEmpty()) {
								delete dev;
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
			continue;
		}
		if (QObject* obj = vipQObjectFromPython(p)) {
			if (QWidget* w = qobject_cast<QWidget*>(obj)) {
				if (new_player)
					return error("cannot open widget in existing player");
				VipWidgetPlayer* pl = new VipWidgetPlayer(w);
				new_player = pl;
				continue;
			}
			else
				return error("cannot open non widget QObject");
		}
		auto d = createDevice(p);
		if (auto* dev = d.value<VipIODevice*>()) {
			auto lst = vipCreatePlayersFromProcessing(dev, new_player);
			if (lst.size() != 1) {
				qDeleteAll(lst);
				delete dev;
				return error("unable to create player from data");
			}
			new_player = lst.first();
			continue;
		}
		if (d.userType() != 0)
			return d;
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
			continue;
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
	return playerId(new_player);
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

	auto* p = (VipProgress*)(uintptr_t)lst[0].toULongLong();
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
	auto* w = findDragWidget(player);
	if (!w)
		return error("invalid player id");

	w->close();
	return QVariant();
}

static QVariant setCurrentPlayer(int player)
{
	auto* w = findDragWidget(player);
	if (!w)
		return error("invalid player id");

	VipDisplayPlayerArea* a = VipDisplayPlayerArea::fromChild(w);
	if (!a)
		return error("Cannot find window number " + QString::number(player));

	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(a);
	w->setFocusWidget();
	return {};
}

static QVariant playerWorkspace(int player)
{
	auto* w = findDragWidget(player);
	if (!w)
		return error("invalid player id");
	VipDisplayPlayerArea* a = VipDisplayPlayerArea::fromChild(w);
	if (!a)
		return error("invalid player id");
	return workspaceId(a);
}

static QVariant setTimeMarkers(int id, qint64 start, qint64 end)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");

	wks->processingPool()->setTimeLimitsEnable(true);
	wks->processingPool()->setStopBeginTime(start);
	wks->processingPool()->setStopEndTime(end);
	return {};
}
static QVariant removeTimeMarkers(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	wks->processingPool()->setTimeLimitsEnable(false);
	return {};
}

static QVariant setRowRatio(int id, int row, double ratio)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	if (ratio < 0 || ratio > 1) {
		return error("wrong ratio value");
	}
	if (wks) {
		int height = wks->dragWidgetArea()->height();
		int new_height = height * ratio;
		if (VipMultiDragWidget* mw = wks->dragWidgetArea()->findChild<VipMultiDragWidget*>()) {
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
	auto* w = findDragWidget(player);
	if (!w)
		return error("invalid player id");
	w->showMaximized();
	return QVariant();
}
static QVariant showNormal(int player)
{
	auto* w = findDragWidget(player);
	if (!w)
		return error("invalid player id");
	w->showNormal();
	return QVariant();
}
static QVariant showMinimized(int player)
{
	auto* w = findDragWidget(player);
	if (!w)
		return error("invalid player id");
	w->showMinimized();
	return QVariant();
}

static QVariant source_devices(int player)
{
	auto* pl = findPlayer(player);
	if (!pl)
		return error("invalid player id");

	QVariantList ret;
	for (auto* display : pl->displayObjects()) {
		auto devices = vipListCast<VipIODevice*>(display->allSources());
		for (auto* d : devices)
			ret.push_back((quint64)(uintptr_t)d);
	}
	return vipToVariant(ret);
}

static QVariant closeWorkspace(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	wks->close();
	return {};
}

static QVariant workspacePlayers(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");

	auto lst = wks->findChildren<VipDragWidget*>();
	QVariantList ret;
	for (auto* w : lst)
		ret.push_back(playerId(w));
	return vipToVariant(ret);
}

static QVariant _workspace(int id)
{
	if (id == 0) {
		VipDisplayPlayerArea* area = new VipDisplayPlayerArea();
		vipGetMainWindow()->displayArea()->addWidget(area);
		return workspaceId(area);
	}
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");

	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(wks);
	return workspaceId(wks);
}

static QVariant workspace_pos(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	return vipGetMainWindow()->displayArea()->displayTabWidget()->indexOf(wks);
}
static QVariant set_workspace_pos(int id, int pos)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	auto* tab = vipGetMainWindow()->displayArea()->displayTabWidget();
	int idx = tab->indexOf(wks);
	if (pos < 0)
		pos = 0;
	else if (pos >= tab->count() - 1)
		pos = tab->count() - 2;
	tab->tabBar()->moveTab(idx, pos);
	return {};
}

#include "VipXmlArchive.h"

static QVariant workspace_as_session(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");

	vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(wks);

	VipXOStringArchive arch;
	vipGetMainWindow()->saveSession(arch, (VipMainWindow::SessionType)VipMainWindow::CurrentArea, VipMainWindow::All);
	return arch.toString();
}

static QVariant workspace_as_image(int id, const QString& background)
{
	auto* area = findWorkspace(id);
	if (!area)
		return error("invalid workspace id");
	// vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(this);

	QImage img;
	QColor back = QColor(background);

	VipMultiDragWidget* mdrag = area->mainDragWidget(nullptr, false);
	if (!mdrag) {
		img = QImage(area->width(), area->height(), QImage::Format_ARGB32);
		img.fill(back);
	}
	else {

		VipRenderState state;
		VipRenderObject::startRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);

		vipProcessEvents();

		QPixmap pixmap(/*area->dragWidgetArea()->viewport()->size()*/ mdrag->size());
		pixmap.fill(back);

		{
			QPainter p(&pixmap);
			p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

			VipRenderObject::renderObject(/*area->dragWidgetArea()->widget()*/ mdrag, &p, QPoint(), true, false);

			VipRenderObject::endRender(/*area->dragWidgetArea()->widget()*/ mdrag, state);
		}

		pixmap = vipRemoveColoredBorder(pixmap, back);
		VipGuiDisplayParamaters::instance()->watermark().addToPixmap(pixmap);
		img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
	}

	return vipToVariant(VipNDArray(img.bits(), qMetaTypeId<VipRGB>(), vipVector(img.height(), img.width())));
}

static QVariant workspace_orientation(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	return wks->mainDragWidget()->orientation() == Qt::Vertical ? 0 : 1;
}

static QVariant set_workspace_orientation(int id, int o)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	if (o < 0 || o > 1)
		return error("invalid orientation");
	wks->mainDragWidget()->setOrientation(o == 0 ? Qt::Vertical : Qt::Horizontal);
	return {};
}

static QVariant workspace_left_tool_bar(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	auto* ret = wks->leftTabWidget();
	if (!ret)
		wks->setLeftTabWidget(ret = new QToolBar());
	return vipToVariant(ret);
}

static QVariant workspace_right_tool_bar(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	auto* ret = wks->rightTabWidget();
	if (!ret)
		wks->setRightTabWidget(ret = new QToolBar());
	return vipToVariant(ret);
}

static QVariant _workspaces()
{
	QVariantList res;
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i)
		res.append(workspaceId(vipGetMainWindow()->displayArea()->widget(i)));
	return vipToVariant(res);
}

static QVariant currentWorkspace()
{
	if (auto* wks = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return workspaceId(wks);
	return 0;
}

static void resizeSplitter(QSplitter* splitter)
{
	QList<int> sizes;
	for (int i = 0; i < splitter->count(); ++i)
		sizes.append(1);
	splitter->setSizes(sizes);
	splitter->setOpaqueResize(true);
}
static QVariant resize_rows_columns(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");

	if (VipMultiDragWidget* m = wks->mainDragWidget()) {
		resizeSplitter(m->mainSplitter());
		for (int i = 0; i < m->mainCount(); ++i)
			resizeSplitter(m->subSplitter(i));
	}
	return QVariant();
}

static QVariant workspace_widget(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	return vipToVariant(wks);
}

static QVariant currentTime(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	return wks->processingPool()->time();
}

static QVariant setCurrentTime(int id, qint64 time, const QString& type = "absolute")
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");

	VipProcessingPool* pool = wks->processingPool();

	if (type.compare("relative") == 0)
		time += pool->firstTime();

	pool->seek(time);
	// wait for leafs
	VipProcessingObjectList objects = pool->leafs(false);
	for (int i = 0; i < objects.size(); ++i)
		if (objects[i])
			objects[i]->wait();
	// return time
	return QVariant((pool->time()));
}

static QVariant firstTime(int id, qint64 time)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	qint64 res = wks->processingPool()->firstTime();
	return QVariant::fromValue(res);
}
static QVariant lastTime(int id, qint64 time)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	qint64 res = wks->processingPool()->lastTime();
	return QVariant::fromValue(res);
}

static QVariant nextTime(int id, qint64 time)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	qint64 res = wks->processingPool()->nextTime(time);
	return QVariant::fromValue(res);
}
static QVariant previousTime(int id, qint64 time)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	qint64 res = wks->processingPool()->previousTime(time);
	return QVariant::fromValue(res);
}
static QVariant closestTime(int id, qint64 time)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	qint64 res = wks->processingPool()->closestTime(time);
	return QVariant::fromValue(res);
}

static QVariant timeRange(int id)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("invalid workspace id");
	VipTimeRange range = wks->processingPool()->timeLimits();
	return vipToVariant(QVariantList() << range.first << range.second);
}

static QVariant plot_canvas(int player)
{
	auto* pl = findPlayer(player);

	if (auto* plot = qobject_cast<VipPlotPlayer*>(pl))
		return plot->leftScaleCount();
	return 1;
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

static QVariant playerType(int player)
{
	VipAbstractPlayer* pl = findPlayer(player);
	if (!pl)
		return error("invalid player id");
	return QString(pl->metaObject()->className());
}

static QVariant playerToolBar(int player)
{
	VipAbstractPlayer* pl = findPlayer(player);
	if (!pl)
		return error("invalid player id");
	return vipToVariant(pl->playerToolBar());
}

static QVariant currentPlayer()
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		if (VipDragWidget* drag = area->dragWidgetHandler()->focusWidget())
			if (/* VipAbstractPlayer* pl =*/qobject_cast<VipAbstractPlayer*>(drag->widget()))
				return playerId(drag);
	return 0;
}

static QVariant setSelected(int player, bool selected, const QString& partial_name)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	VipDisplayPlotItem* item = findDisplay(pl, partial_name);
	if (!item)
		return error("cannot find a valid data for name " + partial_name);

	item->item()->setSelected(selected);
	return QVariant();
}

static QVariant unselectAll(int player)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	QList<QGraphicsItem*> items = pl->plotWidget2D()->scene()->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		items[i]->setSelected(false);
	}
	return QVariant();
}

static QVariant itemList(int player, int selection, const QString& partial_name)
{
	// TODO: reimplement properly
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
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
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	if (data_name.isEmpty()) {
		pl->plotWidget2D()->area()->setStyleSheet(stylesheet);
	}
	else {
		VipDisplayPlotItem* item = findDisplay(pl, data_name);
		if (!item)
			return error("cannot find data name for player " + QString::number(player));
		item->setAttribute("stylesheet", stylesheet);
	}

	if (VipProcessingPool* pool = pl->processingPool())
		pool->reload();
	return QVariant();
}

static QVariant getData(int player, const QString& data_name)
{
	// TODO: reimplement properly (return VipAnyData)
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	VipDisplayPlotItem* item = findDisplay(pl, data_name);
	if (!item)
		return error("cannot find data name for player " + QString::number(player));
	return vipToVariant(VipPyObject(vipAnyDataToPython(item->inputAt(0)->data()), borrow_t{}));
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

static QVariant create_ellipse(double left, double top, double width, double height, const QVariantMap& attrs)
{
	QPainterPath path;
	path.addEllipse(QRectF(left, top, width, height));
	VipShape sh(path);
	sh.setAttributes(attrs);
	return vipToVariant(VipPyObject(vipVariantToPython(vipToVariant(sh)), borrow_t{}));
}

static QVariant get_shape_groups(int player, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (canvas < 0)
		return error("invalid canvas value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (canvas >= plot->leftScaleCount())
			return error("invalid canvas value");
		VipAbstractScale* sc = plot->leftScales()[canvas];
		model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
	}
	if (!model)
		return error("cannot find a valid shape for given canvas");
	return model->sceneModel().groups();
}

static QVariant get_shapes(int player, const QString& group, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (canvas < 0)
		return error("invalid canvas value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (canvas >= plot->leftScaleCount())
			return error("invalid canvas value");
		VipAbstractScale* sc = plot->leftScales()[canvas];
		model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
	}
	if (!model)
		return error("cannot find a valid shape for given canvas");

	auto lst = model->sceneModel().shapes(group);
	QVariantList ret;
	for (const auto& sh : lst)
		ret << vipToVariant(VipPyObject(vipVariantToPython(vipToVariant(sh)), borrow_t{}));

	return vipToVariant(ret);
}

static QVariant get_shape(int player, const QString& id, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (canvas < 0)
		return error("invalid canvas value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (canvas >= plot->leftScaleCount())
			return error("invalid canvas value");
		VipAbstractScale* sc = plot->leftScales()[canvas];
		model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
	}
	if (!model)
		return error("cannot find a valid shape for given canvas");
	VipShape sh = model->sceneModel().find(id);
	if (sh.isNull())
		return error("cannot find a valid shape for id '" + id + "'");
	return vipToVariant(VipPyObject(vipVariantToPython(vipToVariant(sh)), borrow_t{}));
}

static VipNDArray fill_pixels(const VipShape& sh)
{
	auto vec = sh.fillPixels();
	return VipNDArray((int*)vec.data(), vipVector(vec.size(), 2));
}

static QVariant clear_shapes(int player, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (canvas < 0)
		return error("invalid canvas value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (canvas >= plot->leftScaleCount())
			return error("invalid canvas value");
		VipAbstractScale* sc = plot->leftScales()[canvas];
		model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
	}
	if (!model)
		return error("cannot find a valid shape for given canvas");

	model->sceneModel().clear();
	return QVariant();
}

static QVariant add_shape(int player, const VipShape& sh, const QString& id, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (canvas < 0)
		return error("invalid canvas value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (plot->leftScaleCount() <= canvas)
			return error("invalid canvas value");
		VipAbstractScale* sc = plot->leftScales()[canvas];
		model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
	}
	if (!model)
		return error("cannot find a valid shape for given canvas");

	if (!id.isEmpty()) {
		auto exist = model->sceneModel().find(id);
		if (!exist.isNull()) {
			if (exist.type() != sh.type())
				return error("cannot replace a shape of different type");
			// replace
			if (sh.type() == VipShape::Point) {
				exist.mergeAttributes(sh.attributes());
				exist.setPoint(sh.point());
			}
			else if (sh.type() == VipShape::Polyline) {
				exist.mergeAttributes(sh.attributes());
				exist.setPolyline(sh.polyline());
			}
			else if (sh.type() == VipShape::Polygon) {
				exist.mergeAttributes(sh.attributes());
				exist.setPolygon(sh.polygon());
			}
			else {
				exist.mergeAttributes(sh.attributes());
				exist.setShape(sh.shape());
			}
			return id;
		}
	}

	// Create
	if (sh.type() == VipShape::Point)
		model->sceneModel().add("Points", sh);
	else if (sh.type() == VipShape::Polyline)
		model->sceneModel().add("Polylines", sh);
	else
		model->sceneModel().add("ROI", sh);
	return sh.identifier();
}

static QVariant remove_shape(int player, const QString& id, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (canvas < 0)
		return error("invalid canvas value");

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (plot->leftScaleCount() <= canvas)
			return error("invalid canvas value");
		VipAbstractScale* sc = plot->leftScales()[canvas];
		model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
	}
	if (!model)
		return error("cannot find a valid shape for given canvas");

	auto exist = model->sceneModel().find(id);
	if (!exist.isNull()) {
		model->sceneModel().remove(exist);
		return {};
	}
	return error("unable to find shape");
}

static QVariant extractTimeTrace(int player, const QVariantList& rois, const QVariantMap& attrs)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

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

static QVariant player_as_image(int player, const QString& background)
{
	auto* pl = findPlayer(player);
	if (!pl)
		return error("invalid player id");

	VipRenderState state;
	VipRenderObject::startRender(pl, state);

	vipProcessEvents();

	QColor back = QColor(background);
	QPixmap pixmap(pl->size());
	pixmap.fill(back); // Qt::transparent);

	{
		QPainter p(&pixmap);
		p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

		vipFDAboutToRender().callAllMatch(pl);

		VipRenderObject::renderObject(pl, &p, QPoint(), true, false);

		VipRenderObject::endRender(pl, state);
	}
	VipGuiDisplayParamaters::instance()->watermark().addToPixmap(pixmap);
	auto img = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);

	return vipToVariant(VipNDArray(img.bits(), qMetaTypeId<VipRGB>(), vipVector(img.height(), img.width())));
}

static QVariant player_color_map(int player)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	auto map = pl->colorMap();
	if (map == VipLinearColorMap::Unknown)
		return QString();
	else
		return QString(VipLinearColorMap::colorMapToName((VipLinearColorMap::StandardColorMap)map));
}
static QVariant set_player_color_map(int player, const QString& name)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	auto map = VipLinearColorMap::colorMapFromName(name);
	if (map == VipLinearColorMap::Unknown)
		return error("invalid color map name");
	pl->setColorMap(map);
	return {};
}

static QVariant color_map_flat_histogram(int player)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	return pl->isFlatHistogramColorScale() ? pl->flatHistogramStrength() : 0;
}
static QVariant set_color_map_flat_histogram(int player, int strength)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (strength < 0)
		strength = 0;
	else if (strength > 5)
		strength = 5;
	if (strength == 0)
		pl->setFlatHistogramColorScale(false);
	else {
		pl->setFlatHistogramColorScale(true);
		pl->setFlatHistogramStrength(strength);
	}
	return {};
}

static QVariant legend_position(int player)
{
	auto* pl = qobject_cast<VipPlotPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	return (int)pl->legendPosition();
}
static QVariant set_legend_position(int player, int pos)
{
	auto* pl = qobject_cast<VipPlotPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");
	if (pos < 0)
		pos = 0;
	else if (pos > Vip::LegendInnerBottomRight)
		pos = Vip::LegendInnerBottomRight;

	pl->setLegendPosition((Vip::PlayerLegendPosition)pos);
	return {};
}

static QVariant grid_visible(int player, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

#ifdef VIP_WITH_VTK
	if (auto* vtk = qobject_cast<VipVTKPlayer*>(pl)) {
		return vtk->view()->axesVisible();
	}
#endif

	if (auto* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (canvas < 0 || canvas >= plot->leftScaleCount())
			return error("invalid canvas value");
		auto* sc = plot->leftScales()[canvas];
		auto grids = pl->plotWidget2D()->area()->findItems<VipPlotGrid*>();
		for (auto* gr : grids) {
			if (gr->axes().contains(sc)) {
				return gr->isVisible();
			}
		}
	}

	return pl->plotWidget2D()->area()->grid()->isVisible();
}
static QVariant set_grid_visible(int player, bool visible, int canvas)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

#ifdef VIP_WITH_VTK
	if (auto* vtk = qobject_cast<VipVTKPlayer*>(pl)) {
		vtk->view()->setAxesVisible(visible);
		return {};
	}
#endif

	if (auto* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		if (canvas < 0 || canvas >= plot->leftScaleCount())
			return error("invalid canvas value");
		auto* sc = plot->leftScales()[canvas];
		auto grids = pl->plotWidget2D()->area()->findItems<VipPlotGrid*>();
		for (auto* gr : grids) {
			if (gr->axes().contains(sc)) {
				gr->setVisible(visible);
				return {};
			}
		}
	}

	pl->plotWidget2D()->area()->grid()->setVisible(visible);
	return {};
}

static QVariant auto_color_map(int player)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	return pl->isAutomaticColorScale();
}
static QVariant set_auto_color_map(int player, bool enable)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	pl->setAutomaticColorScale(enable);
	return {};
}

static QVariant removeSignal(int player, const QString& data_name)
{
	auto* pl = qobject_cast<VipPlotPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

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
	return count;
}

static QVariant getTimeMarker(int player)
{
	auto* pl = qobject_cast<VipPlotPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	return pl->timeMarker()->isVisible();
}
static QVariant setTimeMarker(int player, bool enable)
{
	auto* pl = qobject_cast<VipPlotPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	pl->setTimeMarkerVisible(enable);
	return QVariant();
}

static QVariant x_scale(int player)
{
	auto* w = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!w)
		return error("invalid player id");

	if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w)) {
		auto* x = pl->xScale();
		return vipToVariant(QVariantList() << x->scaleDiv().bounds().minValue() << x->scaleDiv().bounds().maxValue());
	}
	if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w)) {
		auto* x = static_cast<VipImageArea2D*>(pl->plotWidget2D()->area())->bottomAxis();
		return vipToVariant(QVariantList() << x->scaleDiv().bounds().minValue() << x->scaleDiv().bounds().maxValue());
	}
	return error("player does not support x scale");
}
static QVariant y_scale(int player, int canvas)
{
	auto* w = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!w)
		return error("invalid player id");

	if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w)) {
		if (canvas < 0 || canvas >= pl->leftScaleCount())
			return error("invalid canvas value");
		auto* y = pl->leftScales()[canvas];
		return vipToVariant(QVariantList() << y->scaleDiv().bounds().minValue() << y->scaleDiv().bounds().maxValue());
	}
	if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w)) {
		auto* y = static_cast<VipImageArea2D*>(pl->plotWidget2D()->area())->bottomAxis();
		return vipToVariant(QVariantList() << y->scaleDiv().bounds().minValue() << y->scaleDiv().bounds().maxValue());
	}
	return error("player does not support y scale");
}

static QVariant zoomArea(int player, double x1, double x2, double y1, double y2, int canvas)
{
	auto* w = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!w)
		return error("invalid player id");

	if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w)) {
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
	else if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w)) {
		if (canvas < 0 || canvas >= pl->leftScaleCount())
			return error("invalid canvas value");
		VipAbstractScale* left = pl->leftScales()[canvas];
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
		return error("player does not support zooming");
}

static QVariant setColorMapScale(int player, double min, double max, double gripMin, double gripMax)
{
	auto* pl = qobject_cast<VipVideoPlayer*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	if (pl->spectrogram()->colorMap()) {
		pl->setAutomaticColorScale(false);
		if (min != max)
			pl->spectrogram()->colorMap()->setScale(min, max);
		if (gripMin != gripMax)
			pl->spectrogram()->colorMap()->setGripInterval(VipInterval(gripMin, gripMax));
	}
	return QVariant();
}

static QVariant autoScale(int player, bool enable)
{
	// TODO: reimplement and make it work with ALL players
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	if (auto* plot = qobject_cast<VipPlotPlayer*>(pl)) {
		plot->setAutoScale(enable);
		return {};
	}
#if VIP_WITH_VTK
	if (auto* vtk = qobject_cast<VipVTKPlayer*>(pl)) {
		if (enable)
			vtk->view()->resetCamera();
		return {};
	}
#endif
	if (auto* video = qobject_cast<VipVideoPlayer*>(pl)) {
		if (enable) {
			QRectF rect = static_cast<VipImageArea2D*>(video->plotWidget2D()->area())->imageBoundingRect();
			static_cast<VipImageArea2D*>(video->plotWidget2D()->area())->setVisualizedImageRect(rect);
		}
		return {};
	}
	return error("player does not support auto scaling");
}

static QVariant setPlayerTitle(int player, const QString& title)
{
	VipAbstractPlayer* pl = findPlayer(player);
	if (!pl)
		return error("invalid player id");

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

static QVariant playerTitle(int player)
{
	VipAbstractPlayer* pl = findPlayer(player);
	if (!pl)
		return error("invalid player id");
	return pl->windowTitle();
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

/*static QMap<int, AnnotationRef> _annotations;
static int _createId()
{
	int start = 1;
	for (QMap<int, AnnotationRef>::const_iterator it = _annotations.begin(); it != _annotations.end(); ++it, ++start) {
		if (it.key() != start)
			return start;
	}
	return start;
}*/

#include "VipAnnotationEditor.h"

/* static QVariant createAnnotation(int player, const QString& type, const QString& text, const QList<double>& pos, const QVariantMap& attributes)
{
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

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

	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

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
	auto* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

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
*/

static QVariant play(int id, bool enable)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("no workspace available");

	if (enable)
		wks->processingPool()->play();
	else
		wks->processingPool()->stop();
	return {};
}

static QVariant repeat(int id, bool enable)
{
	auto* wks = findWorkspace(id);
	if (!wks)
		return error("no workspace available");

	wks->processingPool()->setRepeat(enable);
	return {};
}

static QVariant stream(int id, bool enable)
{
	auto* wks = findWorkspace(id);
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

static QVariant main_as_session()
{
	VipXOStringArchive arch;
	vipGetMainWindow()->saveSession(arch);
	return arch.toString();
}

static QVariant main_child_widget(const QString& name)
{
	QWidget* w = vipGetMainWindow()->findChild<QWidget*>(name);
	if (!w)
		return error("cannot find widget child with given name");
	return vipToVariant(vipPythonFromQObject(w));
}

static QVariant add_page_option(const VipPyObjectPtr& widget, const QString& name, const QString& icon)
{
	if (!widget || !(*widget))
		return error("invalid 'widget' argument");
	QWidget* w = qobject_cast<QWidget*>(vipQObjectFromPython(*widget));
	if (!w)
		return error("invalid 'widget' argument");

	if (!widget->hasMember("applyPage") || !widget->hasMember("updatePage"))
		return error("widget does not have members applyPage and updatePage");

	PyPageOption* page = new PyPageOption(*widget);
	vipGetOptions()->addPage(name, page, QIcon(icon));
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

	VipToolWidget* tool = nullptr;
	if (widget->hasMember("focusPlayerChanged"))
		tool = new PyToolWidgetPlayer(*widget, vipGetMainWindow());
	else
		tool = new VipToolWidget(vipGetMainWindow());

	QString title = w->windowTitle();
	tool->setWindowTitle(title);
	tool->setObjectName(title);
	tool->setWindowIcon(w->windowIcon());
	tool->setWidget(w, (Qt::Orientation)orientation);
	tool->setAllowedAreas((Qt::DockWidgetAreas)allowed);
	vipGetMainWindow()->addDockWidget((Qt::DockWidgetArea)a, tool);
	tool->setFloating(floating);
	return vipToVariant(VipPyObjectPtr(new VipPyObject(vipPythonFromQObject(tool))));
}

static QVariant add_processing(int player, const VipPyObjectPtr& proc, const QString& fun_name, const QString& item_name)
{
	if (!proc || !(*proc))
		return error("invalid processing");

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	VipDisplayPlotItem* item = findDisplay(pl, item_name);
	if (!item)
		return error("cannot find a valid data for name " + item_name);

	QList<VipProcessingList*> lst = vipListCast<VipProcessingList*>(item->allSources());
	if (!lst.size())
		return error("cannot find a valid data for name " + item_name);

	VipProcessingList* p = lst.first();

	VipGILUnlocker unlock;

	// find a VipPyWrapperProcessing with given name
	bool found = false;
	for (int i = 0; i < p->size(); ++i) {
		if (VipPyWrapperProcessing* proc = qobject_cast<VipPyWrapperProcessing*>(p->at(i)))
			if (proc->property("_vip_processingName").toString() == fun_name) {
				proc->initializeProcessing(vipToVariant(proc));
				found = true;
				break;
			}
	}
	if (!found) {
		VipPyWrapperProcessing* processing = new VipPyWrapperProcessing();
		if (!processing->initializeProcessing(vipToVariant(proc)).toBool()) {
			delete processing;
			return error("cannot create processing object");
		}
		processing->setProperty("_vip_processingName", fun_name);
		p->append(processing);
	}

	vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->reload();
	return QVariant();
}

static QVariant get_processing(int player, const QString& fun_name, const QString& item_name)
{
	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	VipDisplayPlotItem* item = findDisplay(pl, item_name);
	if (!item)
		return error("cannot find a valid data for name " + item_name);

	VipPyWrapperProcessing* found = nullptr;

	{
		// We have the VipProcessingList and the GIL that might be locked by one of the list processings.
		// These 2 locks can lead to a deadlock situation: unlock the GIL for the scope of this function.
		VipGILUnlocker unlock;

		QList<VipProcessingList*> lst = vipListCast<VipProcessingList*>(item->allSources());
		if (!lst.size()) {
			return error("cannot find a valid data for name " + item_name);
		}

		VipProcessingList* p = lst.first();

		// find a VipPyFunctionProcessing with given name

		qsizetype size = p->size();
		for (int i = 0; i < size; ++i) {
			if (VipPyWrapperProcessing* proc = qobject_cast<VipPyWrapperProcessing*>(p->at(i)))
				if (proc->property("_vip_processingName").toString() == fun_name) {
					found = proc;
					break;
				}
		}
		if (!found)
			return error("unable to find processing " + fun_name);
	}
	return vipToVariant(found->processingObject());
}

static QVariant remove_processing(int player, const QString& fun_name, const QString& item_name)
{
	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(findPlayer(player));
	if (!pl)
		return error("invalid player id");

	VipDisplayPlotItem* item = findDisplay(pl, item_name);
	if (!item)
		return error("cannot find a valid data for name " + item_name);

	VipPyWrapperProcessing* found = nullptr;
	{
		VipGILUnlocker unlock;

		QList<VipProcessingList*> lst = vipListCast<VipProcessingList*>(item->allSources());
		if (!lst.size()) {
			return error("cannot find a valid data for name " + item_name);
		}

		VipProcessingList* p = lst.first();

		// find a VipPyFunctionProcessing with given name

		for (int i = 0; i < p->size(); ++i) {
			if (VipPyWrapperProcessing* proc = qobject_cast<VipPyWrapperProcessing*>(p->at(i)))
				if (proc->property("_vip_processingName").toString() == fun_name) {
					found = proc;
					p->take(i);
					break;
				}
		}
		if (!found)
			return {};
	}

	VipPyObject obj = found->processingObject();
	delete found;

	return vipToVariant(obj);
}

#include <QLibraryInfo>

static QVariant _can_load_qtpy()
{
	bool ret = !QLibraryInfo::isDebugBuild();
	return ret;
}

static QVariant _update_device_output(quint64 addr, const QString& name, int index)
{
	VipPyWrapperIODevice::updateDeviceOutput(addr, name, index);
	return {};
}

static QVariant log_info(const QString& txt)
{
	VIP_LOG_INFO(txt);
	return {};
}
static QVariant log_warning(const QString& txt)
{
	VIP_LOG_WARNING(txt);
	return {};
}
static QVariant log_error(const QString& txt)
{
	VIP_LOG_ERROR(txt);
	return {};
}

static QVariant env_dir(const QString& category)
{
	if (category == "data")
		return vipGetDataDirectory();
	if (category == "log")
		return vipGetLogDirectory();
	if (category == "plugins_log")
		return vipGetLogPluginsDirectory();
	if (category == "tmp")
		return vipGetTempDirectory();
	if (category == "pers")
		return vipGetPerspectiveDirectory();
	if (category == "user_pers")
		return vipGetUserPerspectiveDirectory();
	if (category == "plugins")
		return vipGetPluginsDirectory();
	if (category == "user_plugins")
		return vipGetUserPluginsDirectory();

	return error("unknown category");
}

static QVariant unregister_io_device(const QString& name)
{
	VipIODevice::unregisterDeviceForPossibleReadWrite(name);
	return {};
}

static QVariant register_shortcut(const QString& name, const VipPyObjectPtr& fun)
{
	if (!fun || !(*fun) || !fun->hasMember("__call__"))
		return error("invalid function object");

	VipShortcutsHelper::registerShorcut(name, [fun]() {
		VipGILLocker lock;
		fun->callMember("__call__");
	});
	return {};
}

static QVariant extend_help_menu(const VipPyObjectPtr& fun)
{
	vipExtendHelpMenu([fun](QMenu* menu) {
		VipGILLocker lock;
		fun->callMember("__call__", QVariantList() << vipToVariant(vipPythonFromQObject(menu)));
	});
	return {};
}

static QVariant workspace_callback(const VipPyObjectPtr& fun, bool call_now)
{
	VipDisplayPlayerArea::addCustomizeFunction([fun](auto* wks) {
		  VipGILLocker lock;
		  fun->callMember("__call__", QVariantList() << workspaceId(wks));
		}, call_now);
	return {};
}

static QVariant player_callback(const VipPyObjectPtr& fun)
{
	auto f = [fun](VipIODevice*, VipAbstractPlayer* pl) {
		VipGILLocker lock;
		fun->callMember("__call__", QVariantList() << playerId(pl));
	};
	VipFDDeviceAddedOnPlayer().append<void(VipIODevice*, VipAbstractPlayer*)>(f);
	return {};
}

static int registerFunctions()
{
	// Private functions
	vipRegisterFunction(vipMakeFunctionObject(_can_load_qtpy, "_can_load_qtpy", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(_update_device_output, "_update_device_output", {}, false));

	// Logging
	vipRegisterFunction(vipMakeFunctionObject(log_info, "log_info", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(log_warning, "log_warning", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(log_error, "log_error", {}, false));


	vipRegisterFunction(vipMakeFunctionObject(env_dir, "env_dir", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(unregister_io_device, "unregister_io_device", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(register_shortcut, "register_shortcut"));
	vipRegisterFunction(vipMakeFunctionObject(extend_help_menu, "extend_help_menu"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_callback, "workspace_callback"));
	vipRegisterFunction(vipMakeFunctionObject(player_callback, "player_callback"));

	// VipProgress wrapper functions
	vipRegisterFunction(make_progress, "make_progress", {}, false);
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

	vipRegisterFunction(vipMakeFunctionObject(fill_pixels, "fill_pixels", {}, false));
	vipRegisterFunction(vipMakeFunctionObject(create_ellipse, "create_ellipse", {}, false));

	vipRegisterFunction(vipMakeFunctionObject(add_processing, "add_processing"));
	vipRegisterFunction(vipMakeFunctionObject(get_processing, "get_processing"));
	vipRegisterFunction(vipMakeFunctionObject(remove_processing, "remove_processing"));

	vipRegisterFunction(vipMakeFunctionObject(setCurrentPlayer, "set_current_player"));
	vipRegisterFunction(vipMakeFunctionObject(playerWorkspace, "player_workspace"));
	vipRegisterFunction(vipMakeFunctionObject(closeWindow, "close"));
	vipRegisterFunction(vipMakeFunctionObject(showMaximized, "show_maximized"));
	vipRegisterFunction(vipMakeFunctionObject(showMinimized, "show_minimized"));
	vipRegisterFunction(vipMakeFunctionObject(showNormal, "show_normal"));
	vipRegisterFunction(vipMakeFunctionObject(_workspace, "workspace"));
	vipRegisterFunction(vipMakeFunctionObject(closeWorkspace, "close_workspace"));
	vipRegisterFunction(vipMakeFunctionObject(workspacePlayers, "workspace_players"));
	vipRegisterFunction(vipMakeFunctionObject(_workspaces, "workspaces"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_pos, "workspace_pos"));
	vipRegisterFunction(vipMakeFunctionObject(set_workspace_pos, "set_workspace_pos"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_as_session, "workspace_as_session"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_as_image, "workspace_as_image"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_orientation, "workspace_orientation"));
	vipRegisterFunction(vipMakeFunctionObject(set_workspace_orientation, "set_workspace_orientation"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_left_tool_bar, "workspace_left_tool_bar"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_right_tool_bar, "workspace_right_tool_bar"));
	vipRegisterFunction(vipMakeFunctionObject(currentWorkspace, "current_workspace"));
	vipRegisterFunction(vipMakeFunctionObject(workspaceTitle, "workspace_title"));
	vipRegisterFunction(vipMakeFunctionObject(setWorkspaceTitle, "set_workspace_title"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_floating, "workspace_floating"));
	vipRegisterFunction(vipMakeFunctionObject(set_workspace_floating, "set_workspace_floating"));
	vipRegisterFunction(vipMakeFunctionObject(setWorkspaceMaxColumns, "setWorkspaceMaxColumns"));
	vipRegisterFunction(vipMakeFunctionObject(workspaceMaxColumns, "workspaceMaxColumns"));
	vipRegisterFunction(vipMakeFunctionObject(currentTime, "time"));
	vipRegisterFunction(vipMakeFunctionObject(setCurrentTime, "set_time"));
	vipRegisterFunction(vipMakeFunctionObject(firstTime, "first_time"));
	vipRegisterFunction(vipMakeFunctionObject(lastTime, "last_time"));
	vipRegisterFunction(vipMakeFunctionObject(nextTime, "next_time"));
	vipRegisterFunction(vipMakeFunctionObject(previousTime, "previous_time"));
	vipRegisterFunction(vipMakeFunctionObject(closestTime, "closest_time"));
	vipRegisterFunction(vipMakeFunctionObject(timeRange, "time_range"));
	vipRegisterFunction(vipMakeFunctionObject(setStyleSheet, "set_stylesheet"));

	vipRegisterFunction(vipMakeFunctionObject(resize_rows_columns, "resize_workspace"));
	vipRegisterFunction(vipMakeFunctionObject(playerToolBar, "player_tool_bar"));
	vipRegisterFunction(vipMakeFunctionObject(player_as_image, "player_as_image"));
	vipRegisterFunction(vipMakeFunctionObject(player_color_map, "player_color_map"));
	vipRegisterFunction(vipMakeFunctionObject(set_player_color_map, "set_player_color_map"));
	vipRegisterFunction(vipMakeFunctionObject(color_map_flat_histogram, "color_map_flat_histogram"));
	vipRegisterFunction(vipMakeFunctionObject(set_color_map_flat_histogram, "set_color_map_flat_histogram"));
	vipRegisterFunction(vipMakeFunctionObject(getData, "get"));
	vipRegisterFunction(vipMakeFunctionObject(clickedPoint, "clicked_point"));
	vipRegisterFunction(vipMakeFunctionObject(get_shape, "get_shape"));
	vipRegisterFunction(vipMakeFunctionObject(clear_shapes, "clear_shapes"));
	vipRegisterFunction(vipMakeFunctionObject(get_shape_groups, "get_shape_groups"));
	vipRegisterFunction(vipMakeFunctionObject(get_shapes, "get_shapes"));
	vipRegisterFunction(vipMakeFunctionObject(source_devices, "source_devices"));

	vipRegisterFunction(vipMakeFunctionObject(add_shape, "add_shape"));
	vipRegisterFunction(vipMakeFunctionObject(remove_shape, "remove_shape"));
	vipRegisterFunction(vipMakeFunctionObject(extractTimeTrace, "time_trace"));
	vipRegisterFunction(vipMakeFunctionObject(legend_position, "legend_position"));
	vipRegisterFunction(vipMakeFunctionObject(set_legend_position, "set_legend_position"));
	vipRegisterFunction(vipMakeFunctionObject(grid_visible, "grid_visible"));
	vipRegisterFunction(vipMakeFunctionObject(set_grid_visible, "set_grid_visible"));
	vipRegisterFunction(vipMakeFunctionObject(auto_color_map, "auto_color_map"));
	vipRegisterFunction(vipMakeFunctionObject(set_auto_color_map, "set_auto_color_map"));
	vipRegisterFunction(vipMakeFunctionObject(removeSignal, "remove"));
	vipRegisterFunction(vipMakeFunctionObject(setTimeMarker, "set_time_marker"));
	vipRegisterFunction(vipMakeFunctionObject(getTimeMarker, "time_marker"));
	vipRegisterFunction(vipMakeFunctionObject(plot_canvas, "plot_canvas"));
	vipRegisterFunction(vipMakeFunctionObject(zoomArea, "zoom"));
	vipRegisterFunction(vipMakeFunctionObject(x_scale, "x_scale"));
	vipRegisterFunction(vipMakeFunctionObject(y_scale, "y_scale"));
	vipRegisterFunction(vipMakeFunctionObject(setColorMapScale, "set_color_map_scale"));
	vipRegisterFunction(vipMakeFunctionObject(currentPlayer, "current_player"));
	vipRegisterFunction(vipMakeFunctionObject(setTimeMarkers, "set_time_markers"));
	vipRegisterFunction(vipMakeFunctionObject(removeTimeMarkers, "remove_time_markers"));

	vipRegisterFunction(vipMakeFunctionObject(setRowRatio, "set_row_ratio"));
	vipRegisterFunction(vipMakeFunctionObject(setPlayerTitle, "set_title"));
	vipRegisterFunction(vipMakeFunctionObject(playerTitle, "title"));
	vipRegisterFunction(vipMakeFunctionObject(autoScale, "auto_scale"));
	vipRegisterFunction(vipMakeFunctionObject(userInput, "user_input"));

	vipRegisterFunction(vipMakeFunctionObject(play, "play"));
	vipRegisterFunction(vipMakeFunctionObject(stream, "stream"));
	vipRegisterFunction(vipMakeFunctionObject(repeat, "repeat"));

	// GUI functions
	vipRegisterFunction(vipMakeFunctionObject(main_as_session, "main_as_session"));
	vipRegisterFunction(vipMakeFunctionObject(main_child_widget, "main_child_widget"));
	vipRegisterFunction(vipMakeFunctionObject(workspace_widget, "workspace_widget"));
	vipRegisterFunction(vipMakeFunctionObject(get_generate_menu, "get_generate_menu"));
	vipRegisterFunction(vipMakeFunctionObject(get_main_window, "get_main_window"));
	vipRegisterFunction(vipMakeFunctionObject(get_tool_widgets, "get_tool_widgets"));
	vipRegisterFunction(vipMakeFunctionObject(get_tool_widget, "get_tool_widget"));
	vipRegisterFunction(vipMakeFunctionObject(add_tool_widget, "add_tool_widget"));
	vipRegisterFunction(vipMakeFunctionObject(add_page_option, "add_page_option"));

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
#include "VipLock.h"

static VipSpinlock _device_lock;
static std::map<uint64_t, QObjectPointer> _devices;

static uint64_t from_ptr(QObject* p)
{
	uint64_t key = (uint64_t)(uintptr_t)p;
	std::scoped_lock<VipSpinlock> guard(_device_lock);
	_devices.insert({ key, p });
	return key;
}
template<class T>
static T* to_ptr(uint64_t addr)
{
	std::scoped_lock<VipSpinlock> guard(_device_lock);
	auto it = _devices.find(addr);
	return it == _devices.end() ? nullptr : qobject_cast<T*>(it->second.data());
}

static void erase_ptr(QObject* p)
{
	std::scoped_lock<VipSpinlock> guard(_device_lock);
	_devices.erase((uint64_t)(uintptr_t)p);
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
	erase_ptr(dev);
	delete dev;
}

void _py_register(uint64_t addr, bool enable)
{
	VipIODevice* dev = (VipIODevice*)((uintptr_t)addr);
	if (enable) {
		from_ptr(dev);
	}
	else {
		erase_ptr(dev);
	}
}
int _py_device_mode(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return VipIODevice::NotOpen;
	}
	return (int)dev->openMode();
}

void* _py_device_type_name(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return nullptr;
	}
	return vipVariantToPython(dev->info().classname);
}

void* _py_device_path(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return nullptr;
	}
	return vipVariantToPython(QString(dev->path()));
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
int64_t _py_device_time(uint64_t addr)
{
	auto* dev = to_ptr<VipIODevice>(addr);
	if (!dev) {
		VipPyError::set("invalid device descriptor");
		return 0;
	}
	return dev->time();
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