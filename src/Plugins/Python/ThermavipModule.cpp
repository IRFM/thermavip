#include "VipDisplayArea.h"
#include "VipPlayer.h"
#include "VipDragWidget.h"
#include "VipIODevice.h"
#include "VipGui.h"
#include "VipDrawShape.h"
#include "ThermavipModule.h"
#include "PyOperation.h"
#include "PyProcessing.h"


#include <functional>
#include <qapplication.h>
#include <qgridlayout.h>
#include <qtoolbutton.h>

///////////////////////////////////////////////
// HELPER FUNCTIONS/STRUCTS
///////////////////////////////////////////////


/**
* Lock GIL on construction, unlock on destruction
*/
struct GIL_Lock_Event : QEvent
{
	VipGILLocker lock;
	GIL_Lock_Event()
		:QEvent((QEvent::Type)(QEvent::MaxUser + 1)) {
		//vip_debug("lock GIL\n");fflush(stdout);
	}

	~GIL_Lock_Event() {
		//vip_debug("unlock GIL\n");fflush(stdout);
	}
};

/**
* Ensure that a QOjbect and all its children will post GIL_Lock_Event before each meta call
*/
struct PyQtWatcher : QObject
{
	QVector<QPointer<QObject> > watched;

	PyQtWatcher(QObject * parent)
		:QObject(parent)
	{
		install(parent);
	}

	~PyQtWatcher() {
		for (int i = 0; i < watched.size(); ++i)
			if (watched[i])
				watched[i]->removeEventFilter(this);
	}
	void install(QObject * w) {
		w->installEventFilter(this);
		watched.append(w);
		QList<QObject*> children = w->findChildren<QObject*>(QString(), Qt::FindDirectChildrenOnly);
		for (int i = 0; i < children.size(); ++i)
			install(children[i]);
	}
	virtual bool eventFilter(QObject * w, QEvent * evt)
	{
		if (evt->type() == QEvent::MetaCall)
		{
			//lock the GIL before calling the slot. It will be unlocked after.
			QCoreApplication::instance()->postEvent(w, new GIL_Lock_Event());
		}
		else if (evt->type() == QEvent::ChildAdded)
		{
			QObject * c = static_cast<QChildEvent*>(evt)->child();
			if (watched.indexOf(c) < 0)
				install(c);
		}
		return false;
	}
};

struct CloseButton : QToolButton
{
	CloseButton(QWidget *parent)
		:QToolButton(parent) {
		parent->installEventFilter(this);
		setToolTip("Close");
		setIcon(vipIcon("close.png"));
		setMaximumSize(QSize(16, 16));
		setStyleSheet("QToolButton {padding: 0 0 0 0;margin: 0px;}");
		connect(this, SIGNAL(clicked(bool)), parent, SLOT(close()));
		move(parent->width() - this->width(), 0);
	}

	~CloseButton() {
		if (parent())
			parent()->removeEventFilter(this);
	}
	virtual bool eventFilter(QObject * w, QEvent * evt) {
		if (evt->type() == QEvent::Resize || evt->type() == QEvent::Show) {
			move(parentWidget()->width() - this->width(), 0);
		}
		return false;
	}
};









typedef QPair<QVariant, QString> ResultType;

struct Event : QEvent
{
	QSharedPointer<bool> alive;
	QSharedPointer<ResultType> result;
	std::function<ResultType()> fun;

	Event(QSharedPointer<bool> v, QSharedPointer<ResultType> res) :QEvent(QEvent::MaxUser), alive(v), result(res) { *alive = true; }
	~Event() { *alive = false; }
};
struct EventObject : QObject
{
	virtual bool event(QEvent * evt)
	{
		if (evt->type() == QEvent::MaxUser) {
			*static_cast<Event*>(evt)->result = static_cast<Event*>(evt)->fun();
			*static_cast<Event*>(evt)->alive = false;
			return true;
		}
		return false;
	}
};
static EventObject* object()
{
	static EventObject inst;
	if (inst.thread() != QCoreApplication::instance()->thread())
	{
		inst.moveToThread(QCoreApplication::instance()->thread());
	}
	return &inst;
}

template< class T>
static ResultType execDelayFunction(const T & fun) {

	if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
		return fun();
	}
	else {
		QSharedPointer<bool> alive(new bool(true));
		QSharedPointer<ResultType> result(new ResultType());
		Event * evt = new Event(alive, result);
		evt->fun = std::function<ResultType()>(fun);
		QCoreApplication::instance()->postEvent(object(), evt);
		QPointer<VipPyLocal> loc = VipPyLocal::instance(vipPyThreadId());
		while (*alive)
		{
			if (loc) {
				loc->wait(alive.data(), 50);
				if (loc->isStopping()) {
					return ResultType(QVariant(), "STOP signal received");
				}
			}
		}
		return *result;
	}
}


static QString pyToString(PyObject * obj)
{
	PyObject* bytes = PyUnicode_AsUTF8String(obj);
	if (!bytes)
		return QString();
	std::string file_name = PyBytes_AsString(bytes);
	if (PyErr_Occurred()) {
		return QString();
	}
	Py_DECREF(bytes);
	return QString(file_name.c_str());
}





///////////////////////////////////////////////
// Thermavip module functions
///////////////////////////////////////////////



static ResultType userInput(const QString& title, const QList<QVariantList> & values)
{
	QWidget* w = new QWidget();
	QGridLayout* lay = new QGridLayout();
	w->setLayout(lay);
	int row = 0;
	QList<QWidget*> widgets;

	for (int i = 0; i < values.size(); ++i)
	{
		const QVariantList v = values[i];
		if (v.size() < 3 || v[0].userType() != QMetaType::QString || v[1].userType() != QMetaType::QString) {
			delete w;
			return ResultType(QVariant(), "Wrong input values");
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
					return ResultType(QVariant(), "'int' type: wrong input values");
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
					return ResultType(QVariant(), "'float' type: wrong input values");
				}
				spin->setRange(lst[0].toDouble(), lst[1].toDouble());
				spin->setSingleStep(lst[2].toDouble());
			}
			widgets << spin;
		}
		else if (type == "bool") {
			QCheckBox* check = new QCheckBox(label);
			lay->addWidget(check, row++, 0,1,2);
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
		return ResultType(QVariant::fromValue(res), QString());
	}
	else {
		return ResultType();
	}
}


static ResultType queryPulseOrDate(const QString & title, const QString & default_value)
{
	if (vipQueryFunction()) {
		return ResultType(QVariant(vipQueryFunction()(title, default_value)), QString());
	}
	return ResultType(QVariant(), "query function is not implemented");
}


static ResultType workspaceTitle(int id)
{
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
		if (vipGetMainWindow()->displayArea()->widget(i)->id() == id) {
			return ResultType(vipGetMainWindow()->displayArea()->widget(i)->windowTitle(), QString());
		}
	}

	return ResultType(QVariant(), "wrong workspace id");
}

static ResultType setWorkspaceTitle(int id, const QString & title)
{
	/*if (index < 0 || index >= vipGetMainWindow()->displayArea()->count())
		return ResultType(QVariant(), "wrong index number");
	if (title.isEmpty())
		return ResultType(QVariant(), "empty title!");
	vipGetMainWindow()->displayArea()->widget(index)->setWindowTitle(title);*/
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
		if (vipGetMainWindow()->displayArea()->widget(i)->id() == id) {
			vipGetMainWindow()->displayArea()->widget(i)->setWindowTitle(title);
			return ResultType();
		}
	}
	return ResultType(QVariant(), "wrong workspace id");
}


static ResultType openPath(const QVariant & p, int player = 0, const QString & side = QString())
{
	VipDragWidget * w = nullptr;
	VipPathList paths;
	if (p.userType() == qMetaTypeId<QString>())
		paths << p.toString();
	else {
		QVariantList lst = p.value<QVariantList>();
		for (int i = 0; i < lst.size(); ++i)
			paths << lst[i].toString();
	}


	if (player) {
		//get parent VipDragWidget
		w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
		if (!w)
			return ResultType(QVariant(), "Cannot find player number " + QString::number(player));
		//get VipAbstractPlayer inside
		VipAbstractPlayer * pl = qobject_cast<VipAbstractPlayer*>(w->widget());
		if (!pl)
			return ResultType(QVariant(), "Invalid player type for player number " + QString::number(player));

		int left = side.compare("left", Qt::CaseInsensitive) == 0;
		int right = side.compare("right", Qt::CaseInsensitive) == 0;
		int top = side.compare("top", Qt::CaseInsensitive) == 0;
		int bottom = side.compare("bottom", Qt::CaseInsensitive) == 0;
		int sum = left + top + right + bottom;
		if (!side.isEmpty() && sum != 1)
			return ResultType(QVariant(), "Wrong last  argument (" + QString::number(player) + "), should one of 'left', 'right', 'top' or 'bottom'");

		
		//add to left, right, top or bottom
		if (sum) {
			VipMultiDragWidget * mw = VipMultiDragWidget::fromChild(w);
			QPoint pt = mw->indexOf(w);
			//create new player
			QList<VipAbstractPlayer*> pls = vipCreatePlayersFromPaths(paths, nullptr);
			pl = pls.size() ? pls.first() : nullptr;
			if (!pl)
				return ResultType(QVariant(), "Cannot open data for given path(s)" );

			VipDragWidget * dw = qobject_cast<VipDragWidget*>(vipCreateFromWidgets(QList<QWidget*>() << pl));
			int id = VipUniqueId::id((VipBaseDragWidget*)dw);
			if (left) {
				mw->insertSub(pt.y(), pt.x(), dw);
			}
			else if (right) {
				mw->insertSub(pt.y(), pt.x() + 1, dw);
			}
			else if (top) {
				mw->insertMain(pt.y(), dw);
			}
			else {
				mw->insertMain(pt.y() + 1, dw);
			}
			return ResultType(QVariant(id), QString());
		}
		else {
			//open in existing player
			vipGetMainWindow()->openPaths(paths, pl).isEmpty();
			if (!pl)
				return ResultType(QVariant(), "Cannot open data in player number" + QString::number(player));

			return ResultType(QVariant(VipUniqueId::id(VipDragWidget::fromChild(pl))), QString());
		}

	}
	else {
		QList<VipAbstractPlayer *> pl = vipGetMainWindow()->openPaths(paths,nullptr);
		if (pl.isEmpty())
			return ResultType(QVariant(), "Cannot open data ");

		return ResultType(QVariant(VipUniqueId::id(VipDragWidget::fromChild(pl.last()))), QString());
	}
}

static ResultType closeWindow(int player)
{
	VipBaseDragWidget * w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return ResultType(QVariant(), "Cannot find window number " + QString::number(player));
	}
	w->close();
	return ResultType(QVariant(), QString());
}

static ResultType setTimeMarkers(qint64 start, qint64 end)
{
	if (VipDisplayPlayerArea* a = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		a->processingPool()->setTimeLimitsEnable(true);
		a->processingPool()->setStopBeginTime(start);
		a->processingPool()->setStopEndTime(end);
		return ResultType(QVariant(), QString());
	}
	else
		return ResultType(QVariant(), "Cannot find a valid workspace");
}
static ResultType removeTimeMarkers()
{
	if (VipDisplayPlayerArea* a = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		a->processingPool()->setTimeLimitsEnable(false);
		return ResultType(QVariant(), QString());
	}
	else
		return ResultType(QVariant(), "Cannot find a valid workspace");
}

static ResultType setRowRatio(int row, double ratio)
{
	if (ratio < 0 || ratio > 1) {
		return ResultType(QVariant(), "wrong ratio value");
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
				int to_add_or_remove = to_add ? (to_add / (heights.size()-2)) : (to_remove / (heights.size() - 2));
				for (int i = 0; i < heights.size()-1; ++i) {
					if (i != row)
					{
						if (to_add)
							heights[i] -= to_add_or_remove;
						else if (to_remove)
							heights[i] += to_add_or_remove;
					}
				}
				
				vsplitter->setSizes(heights);
				vsplitter->setOpaqueResize(true);
				vsplitter->setProperty("_vip_dirtySplitter", 0);
				return ResultType(QVariant(), QString());
			}
		}
	}
	
	return ResultType(QVariant(), "Cannot find a valid workspace");
}

static ResultType showMaximized(int player)
{
	VipBaseDragWidget * w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return ResultType(QVariant(), "Cannot find window number " + QString::number(player));
	}
	w->showMaximized();
	return ResultType(QVariant(), QString());
}
static ResultType showNormal(int player)
{
	VipBaseDragWidget * w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return ResultType(QVariant(), "Cannot find window number " + QString::number(player));
	}
	w->showNormal();
	return ResultType(QVariant(), QString());
}
static ResultType showMinimized(int player)
{
	VipBaseDragWidget * w = (VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return ResultType(QVariant(), "Cannot find window number " + QString::number(player));
	}
	w->showMinimized();
	return ResultType(QVariant(), QString());
}

static ResultType _workspace(int wks)
{
	if (wks == 0) {
		VipDisplayPlayerArea * area = new VipDisplayPlayerArea();
		vipGetMainWindow()->displayArea()->addWidget(area);
		return ResultType(QVariant(area->id()), QString());
	}

	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i) {
		if (vipGetMainWindow()->displayArea()->widget(i)->id() == wks)
		{
			vipGetMainWindow()->displayArea()->setCurrentDisplayPlayerArea(vipGetMainWindow()->displayArea()->widget(i));
			return ResultType(QVariant(vipGetMainWindow()->displayArea()->widget(i)->id()), QString());
		}
	}
	return ResultType(QVariant(), "Cannot find workspace number " + QString::number(wks));
}

typedef QList<qint64> IntegerList;
Q_DECLARE_METATYPE(IntegerList)
static ResultType _workspaces()
{
	qRegisterMetaType<IntegerList>();
	IntegerList res;
	for (int i = 0; i < vipGetMainWindow()->displayArea()->count(); ++i)
		res.append(vipGetMainWindow()->displayArea()->widget(i)->id());
	return ResultType(QVariant::fromValue(res), QString());
}

static ResultType currentWorkspaces()
{
	if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->id()), QString());
	return ResultType(QVariant(0), QString());
}

static ResultType _reorganize()
{
	//vipGetMainWindow()->rearrangeTiles();
	return ResultType();
}

static void resizeSplitter(QSplitter* splitter)
{
	QList<int> sizes;
	for (int i = 0; i < splitter->count(); ++i)
		sizes.append(1);
	splitter->setSizes(sizes);
	splitter->setOpaqueResize(true);
}
static ResultType resize_rows_columns() 
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(), "no valid workspace selected");

	if (VipMultiDragWidget* m = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->mainDragWidget()) {
		resizeSplitter(m->mainSplitter());
		for (int i = 0; i < m->mainCount(); ++i)
			resizeSplitter(m->subSplitter(i));
	}
	return ResultType(QVariant(), QString());
	
}

static ResultType currentTime()
{
	if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->time()), QString());
	return ResultType(QVariant(), "no valid workspace selected");
}

static ResultType setCurrentTime(qint64 time, const QString & type = "absolute")
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(), "no valid workspace selected");

	VipProcessingPool* pool = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool();

	if (type.compare("relative") == 0)
		time += pool->firstTime();

	
	pool->seek(time);
	//wait for leafs
	VipProcessingObjectList objects = pool->leafs(false);
	for (int i = 0; i < objects.size(); ++i)
		if (objects[i])
			objects[i]->wait();
	//return next time
	return ResultType(QVariant(pool->nextTime(pool->time())), QString());
}

static ResultType nextTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(), "no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->nextTime(time);
	return ResultType(res, QString());
}
static ResultType previousTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(), "no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->previousTime(time);
	return ResultType(res, QString());
}
static ResultType closestTime(qint64 time)
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(), "no valid workspace selected");

	qint64 res = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->closestTime(time);
	return ResultType(res, QString());
}

static ResultType timeRange()
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return ResultType(QVariant(), "no valid workspace selected");

	VipTimeRange range = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->timeLimits();
	return ResultType(QVariant::fromValue(IntegerList() << range.first << range.second), QString());
}

static ResultType clampTime(const VipNDArray & ar, qint64 min, qint64 max)
{
	if (min >= max) {
		return ResultType(QVariant(), "wrong min max time values (min >= max)");
	}
	if (ar.isEmpty())
		return ResultType(QVariant::fromValue(VipNDArray()), QString());

	int size = ar.shape(1);
	const vip_double *xptr = (const vip_double*)ar.constData();
	const vip_double *yptr = xptr + size;


	for (int i = 1; i <size; ++i) {
		if (xptr[i] <= xptr[i - 1])
			return ResultType(QVariant(), "given signal is not continuous");
	}

	QVector<vip_double> x, y;
	x.reserve(size);
	y.reserve(size);
	int i = 0;
	while (i < size && xptr[i] < min) ++i;
	while (i < size && xptr[i] <= max) {
		x.append(xptr[i]);
		y.append(yptr[i]);
		++i;
	}

	if (x.isEmpty())
		return ResultType(QVariant::fromValue(VipNDArray()), QString());

	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(2, x.size()));
	vip_double * data = (vip_double*)res.data();
	memcpy(data, x.data(), sizeof(vip_double) * x.size());
	memcpy(data + x.size(), y.data(), sizeof(vip_double) * y.size());
	return ResultType(QVariant::fromValue(res), QString());
}


static VipDisplayPlotItem * findDisplay(VipPlayer2D * pl, const QString & partial_name)
{
	//"image" is a valid name for video player
	if (partial_name.isEmpty() || (QString("image").indexOf(partial_name) >= 0 && qobject_cast<VipVideoPlayer*>(pl))) {
		VipDisplayPlotItem * item = nullptr;
		if (qobject_cast<VipVideoPlayer*>(pl))
			item = qobject_cast<VipDisplayPlotItem*>(pl->mainDisplayObject());
		else {
			//take the last one
			QList<VipDisplayObject*> disps = pl->displayObjects();
			for (int i = disps.size() - 1; i >= 0; --i)
				if (item = qobject_cast<VipDisplayPlotItem*>(disps[i]))
					break;
		}
		return item;
	}

	QList<VipDisplayPlotItem*> disps = vipListCast<VipDisplayPlotItem*>(pl->displayObjects());
	if (disps.size() == 0)
		return nullptr;

	for (int i = disps.size() - 1; i >= 0; --i) {
		VipDisplayPlotItem * item = disps[i];
		if (item->inputAt(0)->probe().name().indexOf(partial_name) >= 0 || item->item()->title().text().indexOf(partial_name) >= 0) {
			return item;
		}
	}

	//when multiple signal have the same name, it is possible to add '[index]' in the partial name to select the right one
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
			VipDisplayPlotItem * item = disps[i];
			if (item->inputAt(0)->probe().name().indexOf(pname) >= 0 || item->item()->title().text().indexOf(pname) >= 0) {
				if (++c == index)
					return item;
			}
		}
	}
	return nullptr;
}

#define PLOT_PLAYER  0 // a 2D player containing curves, histograms, ...
#define VIDEO_PLAYER  1 // a 2D player displaying an image or a video
#define _2D_PLAYER  2 // other type of 2D player
#define OTHER_PLAYER  3 // other type of player

static ResultType playerType(int player)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipAbstractPlayer * pl = qobject_cast<VipAbstractPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (qobject_cast<VipVideoPlayer*>(pl))
		return ResultType(VIDEO_PLAYER, QString());
	else if (qobject_cast<VipPlotPlayer*>(pl))
		return ResultType(PLOT_PLAYER, QString());
	else if (qobject_cast<VipPlayer2D*>(pl))
		return ResultType(_2D_PLAYER, QString());
	else
		return ResultType(OTHER_PLAYER, QString());
}

static ResultType currentPlayer()
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		if (VipDragWidget* drag = area->dragWidgetHandler()->focusWidget())
			if (VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(drag->widget()))
				return ResultType(VipUniqueId::id(static_cast<VipBaseDragWidget*>(drag)), QString());
	return ResultType(QVariant(0),QString());
}

static ResultType setSelected(int player, bool selected, const QString & partial_name)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem * item = findDisplay(pl, partial_name);
	if (!item) {
		return ResultType(QVariant(), "cannot find a valid data for name " + partial_name);
	}

	item->item()->setSelected(selected);
	return ResultType();
}

static ResultType unselectAll(int player)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	QList<QGraphicsItem *> items = pl->plotWidget2D()->scene()->selectedItems();
	for (int i = 0; i < items.size(); ++i)
	{
		items[i]->setSelected(false);
	}
	return ResultType();
}

static ResultType itemList(int player, int selection, const QString & partial_name)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(QStringList()), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(QStringList()), "cannot find a valid player for number " + QString::number(player));

	if (VipVideoPlayer * video = qobject_cast<VipVideoPlayer*>(pl)) {
		//"image" is a valid name for video player
		if (partial_name.isEmpty() || QString("image").indexOf(partial_name) >= 0) {
			if ((selection == 2) || (int)video->spectrogram()->isSelected() == selection)
				return ResultType(QVariant(QStringList() << "image"), QString());
		}
		return ResultType(QVariant(QStringList()), QString());
	}
	else {
		QList<VipDisplayPlotItem*> disps = vipListCast<VipDisplayPlotItem*>(pl->displayObjects());
		if (disps.size() == 0)
			return ResultType(QVariant(QStringList()), QString());

		QStringList res;
		QMultiMap<QString, int> names;
		for (int i = 0; i < disps.size(); ++i) {
			VipDisplayPlotItem * item = disps[i];
			if (selection == 2 || (int)item->item()->isSelected() == selection) {
				QString found;
				if (item->inputAt(0)->probe().name().indexOf(partial_name) >= 0)
					found = (item->inputAt(0)->probe().name());
				else if (item->item()->title().text().indexOf(partial_name) >= 0)
					found = (item->item()->title().text());

				int c = names.count(found);
				names.insert(found, 0);
				if (c == 0) {
					//first occurence
					res.append(found);
				}
				else {
					res.append(found + "[" + QString::number(c + 1) + "]");
					int index = res.indexOf(found);
					if (index >= 0) {
						//replace first occurence
						res[index] = res[index] + "[1]";
					}
				}
			}
		}
		return ResultType(QVariant(res), QString());
	}
}

static ResultType setStyleSheet(int player, const QString & data_name, const QString & stylesheet)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem * item = findDisplay(pl, data_name);
	if (!item)
		return ResultType(QVariant(), "cannot find data name for player " + QString::number(player));
	item->setAttribute("stylesheet", stylesheet);
	if (!PyBaseProcessing::currentProcessing())
		vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->reload();
	return ResultType(QVariant(), QString());
}


static ResultType topLevel(int player)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipBaseDragWidget * mw = w->topLevelMultiDragWidget();
	if (!mw)
		return ResultType(QVariant(), "cannot find a valid top level window for player number " + QString::number(player));

	return ResultType(QVariant(VipUniqueId::id(mw)), QString());
}



static ResultType getData(int player, const QString & data_name)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem * item = findDisplay(pl, data_name);
	if (!item)
		return ResultType(QVariant(), "cannot find data name for player " + QString::number(player));
	return ResultType(item->inputAt(0)->data().data(), QString());
}

static ResultType getDataAttribute(int player, const QString & data_name, const QString & attr_name)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem * item = findDisplay(pl, data_name);
	if (!item)
		return ResultType(QVariant(), "cannot find data name for player " + QString::number(player));
	return ResultType(item->inputAt(0)->probe().attribute(attr_name), QString());
}

static ResultType getDataAttributes(int player, const QString & data_name)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem * item = findDisplay(pl, data_name);
	if (!item)
		return ResultType(QVariant(), "cannot find data name for player " + QString::number(player));
	QVariantMap map = item->inputAt(0)->probe().attributes();
	return ResultType(QVariant::fromValue(map), QString());
}

static ResultType getROIPolygon(int player, QString yaxis, const QString & group, int roi)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotSceneModel * model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer * plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale * sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return ResultType(QVariant(), "cannot find a valid shape for given yaxis: " + yaxis);
	VipShape sh = model->sceneModel().find(group, roi);
	if (sh.isNull())
		return ResultType(QVariant(), "cannot find a valid shape for given group and id: " + group + ", " + QString::number(roi));

	QPolygonF points;
	if (sh.type() == VipShape::Polyline) points = sh.polyline();
	else if (sh.type() == VipShape::Point) points << sh.point();
	else {
		points = sh.polygon();
		//points.removeLast();
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
	return ResultType(QVariant::fromValue(QVariantList() << QVariant::fromValue(y) << QVariant::fromValue(x)), QString());
}


static ResultType getROIBoundingRect(int player, QString yaxis, const QString& group, int roi)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return ResultType(QVariant(), "cannot find a valid shape for given yaxis: " + yaxis);
	VipShape sh = model->sceneModel().find(group, roi);
	if (sh.isNull())
		return ResultType(QVariant(), "cannot find a valid shape for given group and id: " + group + ", " + QString::number(roi));

	QRectF r = sh.boundingRect();
	return ResultType(QVariant::fromValue(QVariantList() << r.left()<<r.top()<<r.width()<<r.height()), QString());
}

static ResultType getROIPoints(int player, const QString & group, int roi)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

	VipPlotSceneModel * model = pl->plotSceneModel();
	if (!model)
		return ResultType(QVariant(), "cannot find a valid shape for given player");
	VipShape sh = model->sceneModel().find(group, roi);
	if (sh.isNull())
		return ResultType(QVariant(), "cannot find a valid shape for given group and id: " + group + ", " + QString::number(roi));
	QVector<QPoint> points = sh.fillPixels();
	VipNDArray y(QMetaType::Int, vipVector(points.size()));
	VipNDArray x(QMetaType::Int, vipVector(points.size()));
	for (int i = 0; i < points.size(); ++i) {
		((int*)(x.data()))[i] = (points[i].x());
		((int*)(y.data()))[i] = (points[i].y());
	}

	return ResultType(QVariant::fromValue(QVariantList() << QVariant::fromValue(y) << QVariant::fromValue(x)), QString());
}

static ResultType clearROIs(int player, const QString & yaxis)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));
	VipPlotSceneModel * model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer * plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale * sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return ResultType(QVariant(), "cannot find a valid shape for given yaxis: " + yaxis);

	model->sceneModel().clear();
	return ResultType();
}

static ResultType addROI(int player, const QVariant & v, const QString & yaxis)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPointVector points;
	VipNDArrayType<vip_double> yx = v.value<VipNDArray>().convert<vip_double>();
	if (yx.isEmpty()) {
		if (v.userType() == qMetaTypeId<QString>() || v.userType() == qMetaTypeId<QByteArray>()) {
			QString filename = v.toString();
			QList<VipShape> res = vipGetSceneModelWidgetPlayer()->editor()->openShapes(filename, pl);
			if (res.size())
			{
				QStringList lst;
				for (int i = 0; i < res.size(); ++i)
					lst.append(res[i].identifier());
				return ResultType(QVariant::fromValue(lst), QString());
			}
			else
				return ResultType(QVariant(), "unable to open file " + filename);
		}
		QVariantList lst = v.value<QVariantList>();
		if (lst.size() != 2)
			return ResultType(QVariant(), "wrong ROI value");
		if (lst[0].canConvert<double>() && lst[1].canConvert<double>())
			points.push_back(VipPoint(lst[1].toDouble(), lst[0].toDouble()));
		else if (lst[0].canConvert<QVariantList>() && lst[1].canConvert<QVariantList>()) {
			QVariantList l0 = lst[0].value<QVariantList>();
			QVariantList l1 = lst[1].value<QVariantList>();
			if (l0.size() != l1.size() || l0.size() == 0)
				return ResultType(QVariant(), "wrong ROI value");
			for (int i = 0; i < l0.size(); ++i)
				points.push_back(VipPoint(l1[i].toDouble(), l0[i].toDouble()));
		}
		else if (lst[0].canConvert<VipNDArray>() && lst[1].canConvert<VipNDArray>()) {
			VipNDArrayType<vip_double> y = lst[0].value<VipNDArray>().convert<vip_double>();
			VipNDArrayType<vip_double> x = lst[1].value<VipNDArray>().convert<vip_double>();
			if (x.shapeCount() != 1 || y.shapeCount() != 1 || x.size() != y.size() || x.isEmpty())
				return ResultType(QVariant(), "wrong ROI value");
			for (int i = 0; i < x.size(); ++i)
				points.push_back(VipPoint(x(i), y(i)));
		}
		else
			return ResultType(QVariant(), "wrong ROI value");
	}
	else {
		if (yx.shapeCount() != 2)
			return ResultType(QVariant(), "wrong ROI value");
		for (int i = 0; i < yx.shape(1); ++i)
			points.push_back(VipPoint(yx(1, i), yx(0, i)));
	}
	if (points.isEmpty())
		return ResultType(QVariant(), "wrong ROI value");

	VipPlotSceneModel * model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer * plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale * sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return ResultType(QVariant(), "cannot find a valid shape for given yaxis: " + yaxis);


	VipShape sh;
	if (points.size() == 1) {
		sh.setPoint(points.last().toPointF());
		model->sceneModel().add("Points", sh);
	}
	else if (points.last() == points.first()) {
		sh.setPolygon(points.toPointF());
		model->sceneModel().add("ROI", sh);
	}
	else {
		sh.setPolyline(points.toPointF());
		model->sceneModel().add("Polylines", sh);
	}

	return ResultType(sh.identifier(), QString());
}





static ResultType addEllipse(int player, const QVariant& v, const QString& yaxis)
{
	VipDragWidget* w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	QRectF rect; 
	if (v.userType() == qMetaTypeId<QVariantList>()) {
		const QVariantList lst = v.value<QVariantList>();
		if(lst.size() != 4)
			return ResultType(QVariant(), "wrong ROI value");
		rect = QRectF(lst[0].toDouble(), lst[1].toDouble(), lst[2].toDouble(), lst[3].toDouble());
	}
	else {
		VipNDArrayType<vip_double> yx = v.value<VipNDArray>().convert<vip_double>();
		if (yx.shapeCount() != 1 || yx.size() != 4)
			return ResultType(QVariant(), "wrong ROI value");

		rect = QRectF(yx(0), yx(1), yx(2), yx(3));
	}
	
	VipPlotSceneModel* model = pl->plotSceneModel();
	if (!yaxis.isEmpty())
		if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(pl)) {
			VipAbstractScale* sc = plot->findYScale(yaxis);
			model = plot->findPlotSceneModel(QList<VipAbstractScale*>() << plot->xScale() << sc);
		}
	if (!model)
		return ResultType(QVariant(), "cannot find a valid shape for given yaxis: " + yaxis);

	QPainterPath p;
	p.addEllipse(rect);
	VipShape sh(p);
	model->sceneModel().add("ROI", sh);
	

	return ResultType(sh.identifier(), QString());
}


static ResultType addCircle(int player, double x, double y, double r, const QString& yaxis)
{
	double left = x - r;
	double top = y - r;
	double width = r * 2;
	double height = r * 2;
	QVariantList lst;
	lst << left << top << width << height;
	return addEllipse(player, QVariant::fromValue(lst), yaxis);
}



static ResultType extractTimeTrace(int player, const QVariantList & rois, const QVariantMap & attrs)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

	VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

	VipPlotPlayer * out = nullptr;
	if (int id = attrs["player"].toInt()) {
		VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(id));
		if (!w)
			return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(id));
		out = qobject_cast<VipPlotPlayer*>(w->widget());
	}
	int skip = attrs["skip"].toInt();
	if (skip == 0)
		skip = 1;
	int multi = 2;
	if (attrs.find("multi") != attrs.end())
		multi = attrs["multi"].toInt();

	VipPlotSceneModel * sm = pl->plotSceneModel();
	if (!sm)
		return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

	VipShapeList lst;
	for (int i = 0; i < rois.size(); ++i) {
		VipShape sh = sm->sceneModel().find(rois[i].toString());
		if (!sh.isNull())
			lst.append(sh);
	}
	if (lst.size() == 0) {
		return ResultType(QVariant(), "no valid ROI given");
	}

	QString stat = attrs["statistics"].toString();
	VipShapeStatistics::Statistics stats = 0;
	if (!stat.isEmpty()) {
		if (stat.contains("min"))
			stats |= VipShapeStatistics::Minimum;
		if (stat.contains("max"))
			stats |= VipShapeStatistics::Maximum;
		if (stat.contains("mean"))
			stats |= VipShapeStatistics::Mean;
	}

	//launch
	out = vipExtractTimeTrace(lst, pl, stats, skip, multi, out);
	if (!out)
		return ResultType(QVariant(), "unable to extract time trace for given ROIs");

	return ResultType(VipUniqueId::id(VipDragWidget::fromChild(out)), QString());
}

static ResultType setDataAttribute(int player, const QString & data_name, const QString & attr_name, const QVariant & value)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipDisplayPlotItem * item = findDisplay(pl, data_name);
	if (!item)
		return ResultType(QVariant(), "cannot find data name for player " + QString::number(player));
	if (VipOutput * out = item->inputAt(0)->connection()->source()) {
		out->parentProcessing()->setAttribute(attr_name, value);

		if (!PyBaseProcessing::currentProcessing())
			out->parentProcessing()->reload();
	}

	return ResultType(QVariant(), QString());
}

static ResultType removeSignal(int player, const QString & data_name)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (data_name.isEmpty()) {
		return ResultType(QVariant(), "a valid data name must be specified");
	}
	QList<VipDisplayPlotItem*> disps = vipListCast<VipDisplayPlotItem*>(pl->displayObjects());
	if (disps.size() == 0)
		return ResultType(QVariant(), "cannot find data name for player " + QString::number(player));

	int count = 0;
	for (int i = 0; i < disps.size(); ++i) {
		VipDisplayPlotItem * item = disps[i];
		if (item->inputAt(0)->probe().name().indexOf(data_name) >= 0 || item->item()->title().text().indexOf(data_name) >= 0) {
			item->item()->deleteLater();
			++count;
		}
	}
	return ResultType(QVariant(count), QString());
}

static ResultType setTimeMarker(int player, bool enable)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));

	pl->setTimeMarkerVisible(enable);
	return ResultType(QVariant(), QString());
}

static ResultType zoomArea(int player, double x1, double x2, double y1, double y2, const QString & unit)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(w->widget()))
	{
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
		return ResultType(QVariant(), QString());
	}
	else if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(w->widget()))
	{
		VipAbstractScale * left = pl->findYScale(unit);
		VipAbstractScale * bottom = pl->xScale();
		if (!left || !bottom)
			return ResultType(QVariant(), "cannot find valid axes for player number " + QString::number(player));
		pl->setAutoScale(false);
		if (x1 != x2) {
			bottom->setScale(qMin(x1, x2), qMax(x1, x2));
		}
		if (y1 != y2) {
			left->setScale(qMin(y1, y2), qMax(y1, y2));
		}
		return ResultType(QVariant(), QString());
	}
	else
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));
}


static ResultType setColorMapScale(int player, double min, double max, double gripMin, double gripMax)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

	if (pl->spectrogram()->colorMap()) {
		pl->setAutomaticColorScale(false);
		if (min != max)
			pl->spectrogram()->colorMap()->setScale(min, max);
		if (gripMin != gripMax)
			pl->spectrogram()->colorMap()->setGripInterval(VipInterval(gripMin, gripMax));
	}
	return ResultType(QVariant(), QString());
}


static ResultType playerRange(int player)
{
	qRegisterMetaType<DoubleList>();

	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget())) {
		VipInterval inter = pl->xScale()->scaleDiv().bounds().normalized();
		return ResultType(QVariant::fromValue(DoubleList() << inter.minValue() << inter.maxValue()), QString());
	}
	if (VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(w->widget())) {
		if (VipDisplayObject * obj = pl->mainDisplayObject())
		{
			QList<VipIODevice*> dev = vipListCast<VipIODevice*>(obj->allSources());
			if (dev.size() == 1 && dev.first()->deviceType() == VipIODevice::Temporal) {
				VipTimeRange r = dev.first()->timeLimits();
				return ResultType(QVariant::fromValue(DoubleList() << r.first << r.second), QString());
			}
		}
	}

	return ResultType(QVariant(), "cannot find a valid player or device for number " + QString::number(player));

}

static ResultType autoScale(int player, bool enable)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));

	pl->setAutoScale(enable);
	return ResultType();
}
static ResultType setXScale(int player, vip_double min, vip_double max)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));

	if (VipAbstractScale *sc = pl->xScale()) {
		sc->setAutoScale(false);
		sc->setScaleDiv(sc->scaleEngine()->divideScale(min, max, sc->maxMajor(), sc->maxMinor()));
		return ResultType();
	}
	return ResultType(QVariant(), "cannot find a valid scale for player number " + QString::number(player));
}
static ResultType setYScale(int player, vip_double min, vip_double max, const QString & unit)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));

	if (VipAbstractScale *sc = pl->findYScale(unit)) {
		sc->setAutoScale(false);
		sc->setScaleDiv(sc->scaleEngine()->divideScale(min, max, sc->maxMajor(), sc->maxMinor()));
		return ResultType();
	}
	return ResultType(QVariant(), "cannot find a valid scale for player number " + QString::number(player));
}


static ResultType xRange(int player)
{
	qRegisterMetaType<DoubleList>();

	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));

	QList<VipPlotCurve*> curves = pl->viewer()->area()->findItems<VipPlotCurve*>();
	if (curves.isEmpty())
		return ResultType(QVariant::fromValue(DoubleList()), QString());

	DoubleList res;
	for (int i = 0; i < curves.size(); ++i) {
		if (!curves[i]->isVisible())
			continue;
		vip_double min, max;
		if (curves[i]->rawData().size()) {
			const VipPointVector v = curves[i]->rawData();
			min = max = v.first().x();
			for (int i = 1; i < v.size(); ++i) {
				if (v[i].x() < min) min = v[i].x();
				if (v[i].x() > max) max = v[i].x();
			}
			if (res.isEmpty())
				res << min << max;
			else {
				res[0] = qMin(min, res[0]);
				res[1] = qMax(max, res[1]);
			}
		}
	}

	return ResultType(QVariant::fromValue(res), QString());
}


static ResultType setPlayerTitle(int player, const QString & title)
{
	VipBaseDragWidget * base = VipUniqueId::find<VipBaseDragWidget>(player);
	if (!base)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (VipMultiDragWidget * w = qobject_cast<VipMultiDragWidget*>(base)) {
		w->setWindowTitle(title);
		return ResultType();
	}

	VipDragWidget * w = qobject_cast<VipDragWidget*>(base);
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipAbstractPlayer * pl = qobject_cast<VipAbstractPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (!title.isEmpty()) {
		pl->setAutomaticWindowTitle(false);
		pl->setWindowTitle(title);
	}
	else {
		pl->setAutomaticWindowTitle(true);
		if (pl->processingPool() && (!PyBaseProcessing::currentProcessing()))
			pl->processingPool()->reload();
	}
	return ResultType();
}



//annotation functions

//uniquely identifiy each annotation with a map of id -> shape identifier ('player_id:yaxis:group:shape_id')
static QMap<int, QString> _annotations;
static int _createId() {
	int start = 1;
	for (QMap<int, QString>::const_iterator it = _annotations.begin(); it != _annotations.end(); ++it, ++start) {
		if (it.key() != start)
			return start;
	}
	return start;
}

#include "VipAnnotationEditor.h"	

static ResultType createAnnotation(int player, const QString & type, const QString & text, const QList<double> & pos, const QVariantMap & attributes)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	if (!(pos.size() == 2 || pos.size() == 4))
		return ResultType(QVariant(), "wrong position (should a list of 2 or 4 values, start coordinates and optional end coordinates)");

	QPointF start(pos[0], pos[1]);
	QPointF end = pos.size() == 4 ? QPointF(pos[2], pos[3]) : QPointF();
	QString error;
	QString yaxis = attributes["yaxis"].toString();

	VipAnnotation * a = vipAnnotation(pl, type, text, start, end, attributes, yaxis, &error);
	if (!a) {
		return ResultType(QVariant(), error);
	}

	VipShape sh = a->parentShape()->rawData();
	QString sh_id = QString::number(player) + ":" + yaxis + ":" + sh.group() + ":" + QString::number(sh.id());
	int id = _createId();
	sh.setAttribute("_vip_annotation_id", id);
	_annotations[id] = sh_id;

	return ResultType(id, QString());
}

static ResultType clearAnnotation(int id)
{

	QMap<int, QString>::const_iterator it = _annotations.find(id);
	if (it == _annotations.end())
		return ResultType(QVariant(), "wrong annotation identifier");
	QStringList lst = it.value().split(":");
	if (lst.size() != 4)
		return ResultType(QVariant(), "wrong annotation identifier");

	int player = lst[0].toInt();
	QString yaxis = lst[1];
	QString group = lst[2];
	int sh_id = lst[3].toInt();

	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlotSceneModel * model = pl->plotSceneModel();
	if (!yaxis.isEmpty()) {
		if (VipPlotPlayer * p = qobject_cast<VipPlotPlayer*>(pl)) {
			if (VipAbstractScale * scale = p->findYScale(yaxis)) {
				model = p->findPlotSceneModel(QList<VipAbstractScale*>() << p->xScale() << scale);
			}
		}
	}
	if (!model)
		return ResultType(QVariant(), "wrong annotation identifier");

	VipShape sh = model->sceneModel().find(group, sh_id);
	if (sh.isNull())
		return ResultType(QVariant(), "wrong annotation identifier");

	_annotations.remove(id);
	model->sceneModel().remove(sh);
	return ResultType();
}

static ResultType clearAnnotations(int player, bool all)
{
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	//only remove python annotations (ones with _vip_annotation_id attribute)
	QList<VipPlotSceneModel*> models = pl->plotSceneModels();
	for (int i = 0; i < models.size(); ++i) {
		QList<VipPlotShape*> shapes = models[i]->shapes();
		QList<VipShape> to_remove;
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


	//vipRemoveAllAnnotations(pl);
	return ResultType();
}


static ResultType imShow(const VipNDArray & array, const QVariantMap & attributes)
{
	if (array.shapeCount() != 2 || array.size() < 4) {
		return ResultType(QVariant(), "wrong input array shape");
	}

	VipDragWidget * w = nullptr;
	VipVideoPlayer * pl = nullptr;
	VipAnyResource * res = nullptr;
	int player = attributes["player"].toInt();
	if (player) {
		w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
		if (!w)
			return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

		pl = qobject_cast<VipVideoPlayer*>(w->widget());
		if (!pl)
			return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

		QList<VipIODevice*> devices = vipListCast<VipIODevice*>(pl->mainDisplayObject()->allSources());
		if (devices.size() != 1 || !qobject_cast<VipAnyResource*>(devices.first()))
			return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

		res = static_cast<VipAnyResource*>(devices.first());
	}

	QString name = attributes["title"].toString();
	if (name.isEmpty())
		name = "image";
	QString zunit = attributes["unit"].toString();

	if (!res) {

		VipDisplayPlayerArea * area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
		if (!area)
			return ResultType(QVariant(), "no current valid workspace!");

		res = new VipAnyResource();
		res->setAttribute("Name", name);
		res->setAttribute("ZUnit", zunit);
		res->setData(QVariant::fromValue(array));
		QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessing(res, nullptr);
		if (!pls.size())
			return ResultType(QVariant(), "unable to show image");

		w = qobject_cast<VipDragWidget*>(vipCreateFromWidgets(QList<QWidget*>() << pls.first()));
		area->addWidget(vipCreateFromBaseDragWidget(w));
	}
	else
	{
		res->setAttribute("Name", name);
		res->setAttribute("ZUnit", zunit);
		res->setData(QVariant::fromValue(array));
	}

	return ResultType(VipUniqueId::id((VipBaseDragWidget*)w), QString());
}


static ResultType plotData(const VipPointVector & vector, const QVariantMap & attributes)
{
	VipDragWidget * w = nullptr;
	VipPlotPlayer * pl = nullptr;
	VipAnyResource * res = nullptr;
	int player = attributes["player"].toInt();
	QString name = attributes["title"].toString();
	if (name.isEmpty())
		name = "curve";
	QString yunit = attributes["unit"].toString();

	if (player) {
		w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
		if (!w)
			return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

		pl = qobject_cast<VipPlotPlayer*>(w->widget());
		if (!pl)
			return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

		QList<VipPlotCurve*> curves = vipCastItemListTitle<VipPlotCurve*>(pl->viewer()->area()->childItems(), name, 2, 1);
		if (curves.size() && curves.last()->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QList<VipIODevice*> devices = vipListCast<VipIODevice*>(curves.last()->property("VipDisplayObject").value<VipDisplayObject*>()->allSources());
			if (devices.size() == 1 && qobject_cast<VipAnyResource*>(devices.first()))
				res = static_cast<VipAnyResource*>(devices.first());
		}
	}

	//build stylesheet
	QString symbol = attributes["symbol"].toString();
	QString symbolsize = attributes["symbolsize"].toString();
	QString symbolborder = attributes["symbolborder"].toString();
	QString symbolbackground = attributes["symbolbackground"].toString();
	QString border = attributes["border"].toString();
	QString background = attributes["background"].toString();
	QString style = attributes["style"].toString();
	QString baseline = attributes["baseline"].toString();
	QString color = attributes["color"].toString();
	QString xunit = attributes["xunit"].toString();

	QString stylesheet;
	if (!symbol.isEmpty()) stylesheet += "symbol: " + symbol + ";";
	if (!symbolsize.isEmpty()) stylesheet += "symbolsize: " + symbolsize + ";";
	if (!symbolborder.isEmpty()) stylesheet += "symbolborder: " + symbolborder + ";";
	if (!symbolbackground.isEmpty()) stylesheet += "symbolbackground: " + symbolbackground + ";";
	if (!border.isEmpty()) stylesheet += "border: " + border + ";";
	if (!background.isEmpty()) stylesheet += "background: " + background + ";";
	if (!style.isEmpty()) stylesheet += "style: " + style + ";";
	if (!baseline.isEmpty()) stylesheet += "baseline: " + baseline + ";";
	if (!color.isEmpty()) stylesheet += "color: " + color + ";";


	if (!res) {

		VipDisplayPlayerArea * area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
		if (!area)
			return ResultType(QVariant(), "no current valid workspace!");

		res = new VipAnyResource();
		res->setAttribute("Name", name);
		res->setAttribute("YUnit", yunit);
		res->setAttribute("XUnit", xunit.isEmpty() ? "Time" : xunit);
		if (!stylesheet.isEmpty()) res->setAttribute("stylesheet", stylesheet);
		res->setData(QVariant::fromValue(vector));
		QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessing(res, pl);
		if (!pls.size())
			return ResultType(QVariant(), "unable to plot data");

		if (!pl) {
			w = qobject_cast<VipDragWidget*>(vipCreateFromWidgets(QList<QWidget*>() << pls.first()));
			area->addWidget(vipCreateFromBaseDragWidget(w));
		}
	}
	else
	{
		res->setAttribute("Name", name);
		res->setAttribute("YUnit", yunit);
		res->setAttribute("XUnit", xunit.isEmpty() ? "Time" : xunit);
		if (!stylesheet.isEmpty()) res->setAttribute("stylesheet", stylesheet);
		res->setData(QVariant::fromValue(vector));
		w = qobject_cast<VipDragWidget*>(VipBaseDragWidget::fromChild(pl));
	}

	return ResultType(VipUniqueId::id((VipBaseDragWidget*)w), QString());
}



static QVariant getAttribute(const QVariantMap & attributes, const QString & key, int index)
{
	const QVariantList lst = attributes[key].value<QVariantList>();
	if (index < lst.size())
		return lst[index];
	return QVariant();
}
static ResultType plotsData(const QList<VipPointVector> & vectors, const QVariantMap & attributes)
{
	QVariantList result;
	for (int i = 0; i < vectors.size(); ++i)
	{
		const VipPointVector vector = vectors[i];

		VipDragWidget * w = nullptr;
		VipPlotPlayer * pl = nullptr;
		VipAnyResource * res = nullptr;
		int player = getAttribute(attributes,"player",i).toInt();
		QString name = getAttribute(attributes, "title", i).toString();
		if (name.isEmpty())
			name = "curve";
		QString yunit = getAttribute(attributes, "unit", i).toString();

		if (player) {
			w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
			if (!w)
				return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

			pl = qobject_cast<VipPlotPlayer*>(w->widget());
			if (!pl)
				return ResultType(QVariant(), "cannot find a valid video player for number " + QString::number(player));

			QList<VipPlotCurve*> curves = vipCastItemListTitle<VipPlotCurve*>(pl->viewer()->area()->childItems(), name, 2, 1);
			if (curves.size() && curves.last()->property("VipDisplayObject").value<VipDisplayObject*>()) {
				QList<VipIODevice*> devices = vipListCast<VipIODevice*>(curves.last()->property("VipDisplayObject").value<VipDisplayObject*>()->allSources());
				if (devices.size() == 1 && qobject_cast<VipAnyResource*>(devices.first()))
					res = static_cast<VipAnyResource*>(devices.first());
			}
		}

		//build stylesheet
		QString symbol = getAttribute(attributes, "symbol", i).toString();
		QString symbolsize = getAttribute(attributes, "symbolsize", i).toString();
		QString symbolborder = getAttribute(attributes, "symbolborder", i).toString(); 
		QString symbolbackground = getAttribute(attributes, "symbolbackground", i).toString();
		QString border = getAttribute(attributes, "border", i).toString(); 
		QString background = getAttribute(attributes, "background", i).toString();
		QString style = getAttribute(attributes, "style", i).toString(); 
		QString baseline = getAttribute(attributes, "baseline", i).toString(); 
		QString color = getAttribute(attributes, "color", i).toString();
		QString xunit = getAttribute(attributes, "xunit", i).toString();

		QString stylesheet;
		if (!symbol.isEmpty()) stylesheet += "symbol: " + symbol + ";";
		if (!symbolsize.isEmpty()) stylesheet += "symbolsize: " + symbolsize + ";";
		if (!symbolborder.isEmpty()) stylesheet += "symbolborder: " + symbolborder + ";";
		if (!symbolbackground.isEmpty()) stylesheet += "symbolbackground: " + symbolbackground + ";";
		if (!border.isEmpty()) stylesheet += "border: " + border + ";";
		if (!background.isEmpty()) stylesheet += "background: " + background + ";";
		if (!style.isEmpty()) stylesheet += "style: " + style + ";";
		if (!baseline.isEmpty()) stylesheet += "baseline: " + baseline + ";";
		if (!color.isEmpty()) stylesheet += "color: " + color + ";";


		if (!res) {

			VipDisplayPlayerArea * area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
			if (!area)
				return ResultType(QVariant(), "no current valid workspace!");

			res = new VipAnyResource();
			res->setAttribute("Name", name);
			res->setAttribute("YUnit", yunit);
			res->setAttribute("XUnit", xunit.isEmpty() ? "Time" : xunit);
			if (!stylesheet.isEmpty()) res->setAttribute("stylesheet", stylesheet);
			res->setData(QVariant::fromValue(vector));
			QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessing(res, pl);
			if (!pls.size())
				return ResultType(QVariant(), "unable to plot data");

			if (!pl) {
				w = qobject_cast<VipDragWidget*>(vipCreateFromWidgets(QList<QWidget*>() << pls.first()));
				area->addWidget(vipCreateFromBaseDragWidget(w));
			}
		}
		else
		{
			res->setAttribute("Name", name);
			res->setAttribute("YUnit", yunit);
			res->setAttribute("XUnit", xunit.isEmpty() ? "Time" : xunit);
			if (!stylesheet.isEmpty()) res->setAttribute("stylesheet", stylesheet);
			res->setData(QVariant::fromValue(vector));
			w = qobject_cast<VipDragWidget*>(VipBaseDragWidget::fromChild(pl));
		}

		result << QVariant(VipUniqueId::id((VipBaseDragWidget*)w));
		//return ResultType(VipUniqueId::id((VipBaseDragWidget*)w), QString());
	}
	return ResultType(QVariant::fromValue(result),QString());
}


static ResultType addFunction(int player, PyObject * fun, const QString & fun_name, const QString & item_name)
{
	//find the player
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));
	}

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl) {
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));
	}

	VipDisplayPlotItem * item = findDisplay(pl, item_name);
	if (!item) {
		return ResultType(QVariant(), "cannot find a valid data for name " + item_name);
	}

	QList<VipProcessingList *> lst = vipListCast<VipProcessingList*>(item->allSources());
	if (!lst.size()) {
		return ResultType(QVariant(), "cannot find a valid data for name " + item_name);
	}

	VipProcessingList * p = lst.first();

	//find a PyFunctionProcessing with given name
	bool found = false;
	for (int i = 0; i < p->size(); ++i) {
		if (PyFunctionProcessing * proc = qobject_cast<PyFunctionProcessing*>(p->at(i)))
			if (proc->property("_vip_processingName").toString() == fun_name) {
				proc->setFunction(fun);
				found = true;
				break;
			}
	}
	if (!found) {
		PyFunctionProcessing * proc = new PyFunctionProcessing();
		proc->setFunction(fun);
		proc->setProperty("_vip_processingName", fun_name);
		p->append(proc);
	}

	if (!PyBaseProcessing::currentProcessing())
		vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->reload();
	return ResultType();
}

static ResultType getFunction(int player, const QString & fun_name, const QString & item_name)
{
	//find the player
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w) {
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));
	}

	VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(w->widget());
	if (!pl) {
		return ResultType(QVariant(), "cannot find a valid plot player for number " + QString::number(player));
	}

	VipDisplayPlotItem * item = findDisplay(pl, item_name);
	if (!item) {
		return ResultType(QVariant(), "cannot find a valid data for name " + item_name);
	}

	QList<VipProcessingList *> lst = vipListCast<VipProcessingList*>(item->allSources());
	if (!lst.size()) {
		return ResultType(QVariant(), "cannot find a valid data for name " + item_name);
	}

	VipProcessingList * p = lst.first();

	//find a PyFunctionProcessing with given name
	PyFunctionProcessing* found = nullptr;
	for (int i = 0; i < p->size(); ++i) {
		if (PyFunctionProcessing * proc = qobject_cast<PyFunctionProcessing*>(p->at(i)))
			if (proc->property("_vip_processingName").toString() == fun_name) {
				found = proc;
				break;
			}
	}
	if (!found) {
		return ResultType(QVariant(), "unable to find processing " + fun_name);
	}
	return ResultType(QVariant::fromValue((VipProcessingObject*)found), QString());
}


#include <qwindow.h>
static ResultType testPid(qint64 pid)
{
	/*QWindow *l_container = QWindow::fromWinId(pid);
	l_container->requestActivate();
	QWidget *l_widget = QWidget::createWindowContainer(l_container,nullptr, Qt::FramelessWindowHint);
	l_widget->setAutoFillBackground(true);
	l_widget->setMinimumSize(50, 50);

	QMainWindow* l_main_win = new QMainWindow();
	l_main_win->setWindowTitle("DOLPHIN EMBEDDED IN QT APPLICATION!");
	l_main_win->setCentralWidget(l_widget);
	l_main_win->show();*/
	VipGILLocker lock;
	int r = PyRun_SimpleString("import PyQt5\nfrom PyQt5.QtWidgets import QLabel\nl=QLabel('toto')\nl.show()");
	return ResultType();
}



static ResultType addWidgetToPlayer(int player, const QString & side, const QString & widget_name, const QString & old_name)
{
	//get parent VipDragWidget
	VipDragWidget * w = qobject_cast<VipDragWidget*>(VipUniqueId::find<VipBaseDragWidget>(player));
	if (!w)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	VipAbstractPlayer * pl = qobject_cast<VipAbstractPlayer*>(w->widget());
	if (!pl)
		return ResultType(QVariant(), "cannot find a valid player for number " + QString::number(player));

	int left = side.compare("left", Qt::CaseInsensitive) == 0;
	int right = side.compare("right", Qt::CaseInsensitive) == 0;
	int top = side.compare("top", Qt::CaseInsensitive) == 0;
	int bottom = side.compare("bottom", Qt::CaseInsensitive) == 0;
	int sum = left + top + right + bottom;
	if (!side.isEmpty() && sum != 1)
		return ResultType(QVariant(), "Wrong last  argument (" + QString::number(player) + "), should one of 'left', 'right', 'top' or 'bottom'");

	//find widget
	QList<QWidget*> ws = qApp->topLevelWidgets();
	QWidget * found = nullptr;
	for (int i = 0; i < ws.size(); ++i) {
		if (ws[i]->objectName() == widget_name) {
			found = ws[i];
			break;
		}
	}
	if (!found) {
		return ResultType(QVariant(), "unable to find given widget");
	}
	found->setObjectName(old_name);

	if (left) {
		int left = 9;
		while (pl->gridLayout()->itemAtPosition(10, left) && left >= 0)
			--left;
		if (left < 0)
			return ResultType(QVariant(), "cannot add more widgets to the left side of player");
		pl->gridLayout()->addWidget(found, 10, left);
	}
	if (right) {
		int right = 11;
		while (pl->gridLayout()->itemAtPosition(10, right))
			++right;
		pl->gridLayout()->addWidget(found, 10, right);
	}
	if (top) {
		int top = 9;
		while (pl->gridLayout()->itemAtPosition(top, 10) && top >= 0)
			--top;
		if (top < 0)
			return ResultType(QVariant(), "cannot add more widgets to the top side of player");
		pl->gridLayout()->addWidget(found, top, 10);
	}
	if (bottom) {
		int bottom = 11;
		while (pl->gridLayout()->itemAtPosition(bottom, 10))
			++bottom;
		pl->gridLayout()->addWidget(found, bottom, 10);
	}

	PyQtWatcher * watcher = new PyQtWatcher(found);
	CloseButton * cl = new CloseButton(found);

	return ResultType();
}



static ResultType callRegisteredFunction(const VipFunctionObject & fun, const QVariantList& args)
{
	QVariant v = fun.function(args);
	return ResultType(v, QString());
}


///////////////////////////////////////////////
// Thermavip module functions in python
///////////////////////////////////////////////






static PyObject* current_player(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "current_player: wrong number of argument (should be 0)");
		return nullptr;
	}
	 
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(currentPlayer));
	Py_END_ALLOW_THREADS
		 
		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}


static PyObject* player_type(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "player_type: wrong number of argument (should be 1)");
		return nullptr;
	}
	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(playerType, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* item_list(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "item_list: wrong number of argument (should be 3)");
		return nullptr;
	}
	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();
	int selection = vipPythonToVariant(PyTuple_GetItem(args, 1)).toInt();
	QString name = pyToString(PyTuple_GetItem(args, 2));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(itemList, player, selection, name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* set_selected(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "set_selected: wrong number of argument (should be 3)");
		return nullptr;
	}
	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();
	bool selection = vipPythonToVariant(PyTuple_GetItem(args, 1)).toBool();
	QString name = pyToString(PyTuple_GetItem(args, 2));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setSelected, player, selection, name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* unselect_all(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "unselect_all: wrong number of argument (should be 1)");
		return nullptr;
	}
	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(unselectAll, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* query(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "query: wrong number of argument (should be 2)");
		return nullptr;
	}
	QString title = pyToString(PyTuple_GetItem(args, 0));
	QString default_value = pyToString(PyTuple_GetItem(args, 1));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(queryPulseOrDate, title, default_value));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyUnicode_FromString(result.first.toString().toLatin1().data());
}

static PyObject* open_path(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "open: wrong number of argument (should be 3)");
		return nullptr;
	}
	QVariant path = vipPythonToVariant(PyTuple_GetItem(args, 0));
	if (path.userType() != qMetaTypeId<QString>() && path.userType() != qMetaTypeId<QVariantList>()) {
		PyErr_SetString(PyExc_RuntimeError, "wrong path value");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 1));
	QString side = pyToString(PyTuple_GetItem(args, 2));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(openPath, path, player, side));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}

static PyObject* close_window(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "close_window: wrong number of argument (should be 1)");
		return nullptr;
	}
	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(closeWindow, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* show_maximized(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "show_maximized: wrong number of argument (should be 1)");
		return nullptr;
	}
	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(showMaximized, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject* show_normal(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "show_normal: wrong number of argument (should be 1)");
		return nullptr;
	}
	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(showNormal, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject* show_minimized(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "show_minimized: wrong number of argument (should be 1)");
		return nullptr;
	}
	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(showMinimized, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* workspace(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "workspace: wrong number of argument (should be 1)");
		return nullptr;
	}
	int wks = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(_workspace, wks));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}

static PyObject* workspaces(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "workspaces: wrong number of argument (should be 0)");
		return nullptr;
	}
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(_workspaces));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	IntegerList lst = result.first.value<IntegerList>();
	PyObject * res = PyList_New(lst.size());
	for (int i = 0; i < lst.size(); ++i)
		PyList_SetItem(res, i, PyLong_FromLong(lst[i]));
	return res;
}

static PyObject* current_workspace(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "current_workspace: wrong number of argument (should be 0)");
		return nullptr;
	}
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(currentWorkspaces));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}


static PyObject* workspace_title(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "workspace_title: wrong number of argument (should be 1)");
		return nullptr;
	}
	int wks = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(workspaceTitle, wks));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* set_workspace_title(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "set_workspace_title: wrong number of argument (should be 2)");
		return nullptr;
	}
	int wks = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString title = pyToString(PyTuple_GetItem(args, 1));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setWorkspaceTitle, wks, title));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}



static PyObject* reorganize(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "reorganize: wrong number of argument (should be 0)");
		return nullptr;
	}
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(_reorganize));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* time(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "time: wrong number of argument (should be 0)");
		return nullptr;
	}
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(currentTime));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLongLong(result.first.toLongLong());
}

static PyObject* set_time(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "set_time: wrong number of argument (should be 2)");
		return nullptr;
	}

	qint64 time = PyLong_AsLongLong(PyTuple_GetItem(args, 0));
	QString type = pyToString(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setCurrentTime, time, type));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	//Py_INCREF(Py_None);
	//return Py_None;
	return PyLong_FromLongLong(result.first.toLongLong());
}

static PyObject* next_time(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "next_time: wrong number of argument (should be 1)");
		return nullptr;
	}

	qint64 time = PyLong_AsLongLong(PyTuple_GetItem(args, 0));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = nextTime(time);//execDelayFunction(std::bind(setCurrentTime, time, type));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLongLong(result.first.toLongLong());
}
static PyObject* previous_time(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "previous_time: wrong number of argument (should be 1)");
		return nullptr;
	}

	qint64 time = PyLong_AsLongLong(PyTuple_GetItem(args, 0));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = previousTime(time);//execDelayFunction(std::bind(setCurrentTime, time, type));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLongLong(result.first.toLongLong());
}
static PyObject* closest_time(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "closest_time: wrong number of argument (should be 1)");
		return nullptr;
	}

	qint64 time = PyLong_AsLongLong(PyTuple_GetItem(args, 0));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = closestTime(time);//execDelayFunction(std::bind(setCurrentTime, time, type));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLongLong(result.first.toLongLong());
}

static PyObject* time_range(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "time_range: wrong number of argument (should be 0)");
		return nullptr;
	}
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(timeRange));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	IntegerList range = result.first.value<IntegerList>();
	if (range.isEmpty()) {
		PyErr_SetString(PyExc_RuntimeError, "cannot retrieve time range");
		return nullptr;
	}
	PyObject * res = PyList_New(2);
	PyList_SetItem(res, 0, PyLong_FromLongLong(range[0]));
	PyList_SetItem(res, 1, PyLong_FromLongLong(range[1]));
	return res;
}


static PyObject* clamp_time(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "clamp_time: wrong number of argument (should be 3)");
		return nullptr;
	}


	VipNDArray ar = vipPythonToVariant(PyTuple_GetItem(args, 0)).value<VipNDArray>().convert<vip_double>();
	qint64 min = PyLong_AsLongLong(PyTuple_GetItem(args, 1));
	qint64 max = PyLong_AsLongLong(PyTuple_GetItem(args, 2));

	if (ar.isEmpty() || ar.shapeCount() != 2 || ar.shape(0) != 2) {
		PyErr_SetString(PyExc_RuntimeError, "clamp_time: wrong input array size");
		return nullptr;
	}

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = clampTime(ar, min, max);
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* set_stylesheet(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "set_stylesheet: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString stylesheet = pyToString(PyTuple_GetItem(args, 1));
	QString data_name = pyToString(PyTuple_GetItem(args, 2));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setStyleSheet, player, data_name, stylesheet));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* resize_workspace(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "resize_workspace: wrong number of argument (should be 0)");
		return nullptr;
	}

	ResultType result;
	Py_BEGIN_ALLOW_THREADS result = execDelayFunction(std::bind(resize_rows_columns));
	Py_END_ALLOW_THREADS

	  if (!result.second.isEmpty())
	{
		PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
		return nullptr;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* top_level(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "top_level: wrong number of argument (should be 1)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(topLevel, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	return PyLong_FromLong(result.first.toInt());
}


static PyObject* get(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "get: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString name = pyToString(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getData, player, name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;// PyLong_FromLong(0);
		}

	PyObject * res = (PyObject*)vipVariantToPython(result.first);
	if (!res) {
		PyErr_SetString(PyExc_RuntimeError, "unable to convert data to a valid Python object");
		return nullptr;
	}
	return res;
}

static PyObject* get_attribute(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "get_attribute: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString attr = pyToString(PyTuple_GetItem(args, 1));
	QString name = pyToString(PyTuple_GetItem(args, 2));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getDataAttribute, player, name, attr));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	PyObject * res = (PyObject*)vipVariantToPython(result.first);
	if (!res) {
		PyErr_SetString(PyExc_RuntimeError, "unable to convert data to a valid Python object");
		return nullptr;
	}
	return res;
}

static PyObject* get_attributes(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "get_attributes: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString name = pyToString(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getDataAttributes, player, name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	PyObject * res = (PyObject*)vipVariantToPython(result.first);
	if (!res) {
		PyErr_SetString(PyExc_RuntimeError, "unable to convert data to a valid Python object");
		return nullptr;
	}
	return res;
}

static PyObject* set_attribute(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 4) {
		PyErr_SetString(PyExc_RuntimeError, "set_attribute: wrong number of argument (should be 4)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString attr = pyToString(PyTuple_GetItem(args, 1));
	QVariant value = vipPythonToVariant(PyTuple_GetItem(args, 2));
	QString name = pyToString(PyTuple_GetItem(args, 3));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setDataAttribute, player, name, attr, value));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* roi(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 4) {
		PyErr_SetString(PyExc_RuntimeError, "roi: wrong number of argument (should be 4)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString group = pyToString(PyTuple_GetItem(args, 1));
	int id = PyLong_AsLong(PyTuple_GetItem(args, 2));
	QString axis = pyToString(PyTuple_GetItem(args, 3));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getROIPolygon, player, axis, group, id));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	PyObject * res = (PyObject*)vipVariantToPython(result.first);
	if (!res) {
		PyErr_SetString(PyExc_RuntimeError, "unable to convert data to a valid Python object");
		return nullptr;
	}
	return res;
}

static PyObject* roi_filled_points(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "roi_filled_points: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString group = pyToString(PyTuple_GetItem(args, 1));
	int id = PyLong_AsLong(PyTuple_GetItem(args, 2));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getROIPoints, player, group, id));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	PyObject * res = (PyObject*)vipVariantToPython(result.first);
	if (!res) {
		PyErr_SetString(PyExc_RuntimeError, "unable to convert data to a valid Python object");
		return nullptr;
	}
	return res;
}


static PyObject* get_roi_bounding_rect(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 4) {
		PyErr_SetString(PyExc_RuntimeError, "get_roi_bounding_rect: wrong number of argument (should be 4)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString group = pyToString(PyTuple_GetItem(args, 1));
	int id = PyLong_AsLong(PyTuple_GetItem(args, 2));
	QString axis = pyToString(PyTuple_GetItem(args, 3));


	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getROIBoundingRect, player,axis, group, id));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	PyObject* res = (PyObject*)vipVariantToPython(result.first);
	if (!res) {
		PyErr_SetString(PyExc_RuntimeError, "unable to convert data to a valid Python object");
		return nullptr;
	}
	return res;
}


static PyObject* clear_roi(PyObject *, PyObject* args)
{

	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "clear_roi: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString axis = pyToString(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(clearROIs, player, axis));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject* add_roi(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "add_roi: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QVariant roi = vipPythonToVariant(PyTuple_GetItem(args, 1));
	QString axis = pyToString(PyTuple_GetItem(args, 2));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(addROI, player, roi, axis));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* add_ellipse(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "add_ellipse: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QVariant roi = vipPythonToVariant(PyTuple_GetItem(args, 1));
	QString axis = pyToString(PyTuple_GetItem(args, 2));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(addEllipse, player, roi, axis));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* add_circle(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 5) {
		PyErr_SetString(PyExc_RuntimeError, "add_circle: wrong number of argument (should be 5)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	double y = vipPythonToVariant(PyTuple_GetItem(args, 1)).toDouble();
	double x = vipPythonToVariant(PyTuple_GetItem(args, 2)).toDouble();
	double radius = vipPythonToVariant(PyTuple_GetItem(args, 3)).toDouble();
	QString axis = pyToString(PyTuple_GetItem(args, 4));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(addCircle, player, x,y,radius, axis));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return (PyObject*)vipVariantToPython(result.first);
}


static PyObject* time_trace(PyObject *, PyObject* args)
{
	/*
	args contains:
	- the player id
	- the style
	- the text
	- the position as a list
	- the atributes dictionary
	*/

	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "time_trace: wrong number of argument (should be 3)");
		return nullptr;
	}

	QVariantList rois;
	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QVariant tmp = vipPythonToVariant(PyTuple_GetItem(args, 1));
	QVariantMap attributes = vipPythonToVariant(PyTuple_GetItem(args, 2)).value<QVariantMap>();
	if (tmp.userType() == qMetaTypeId<QVariantList>())
		rois = tmp.value<QVariantList>();
	else
		rois.append(tmp.toString());

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(extractTimeTrace, player, rois, attributes));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}


static PyObject* remove(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "remove: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString name = pyToString(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(removeSignal, player, name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	return PyLong_FromLong(result.first.toInt());
}

static PyObject* set_time_marker(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "set_time_marker: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	bool enable = PyLong_AsLong(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setTimeMarker, player, (bool)enable));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* zoom(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 6) {
		PyErr_SetString(PyExc_RuntimeError, "zoom: wrong number of argument (should be 6)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	double x1 = PyFloat_AsDouble(PyTuple_GetItem(args, 1));
	double x2 = PyFloat_AsDouble(PyTuple_GetItem(args, 2));
	double y1 = PyFloat_AsDouble(PyTuple_GetItem(args, 3));
	double y2 = PyFloat_AsDouble(PyTuple_GetItem(args, 4));
	QString unit = pyToString(PyTuple_GetItem(args, 5));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(zoomArea, player, x1, x2, y1, y2, unit));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject* set_color_map_scale(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 5) {
		PyErr_SetString(PyExc_RuntimeError, "set_color_map_scale: wrong number of argument (should be 5)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	double min = PyFloat_AsDouble(PyTuple_GetItem(args, 1));
	double max = PyFloat_AsDouble(PyTuple_GetItem(args, 2));
	double gripMin = PyFloat_AsDouble(PyTuple_GetItem(args, 3));
	double gripMax = PyFloat_AsDouble(PyTuple_GetItem(args, 4));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setColorMapScale, player, min, max, gripMin, gripMax));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* x_range(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "x_range: wrong number of argument (should be 1)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(xRange, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	DoubleList lst = result.first.value<DoubleList>();
	if (lst.isEmpty())
		return PyList_New(0);
	else {
		PyObject * res = PyList_New(2);
		PyList_SetItem(res, 0, PyFloat_FromDouble(lst[0]));
		PyList_SetItem(res, 1, PyFloat_FromDouble(lst[1]));
		return res;
	}
}

static PyObject* player_range(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "player_range: wrong number of argument (should be 1)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(playerRange, player));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	DoubleList lst = result.first.value<DoubleList>();
	if (lst.isEmpty())
		return PyList_New(0);
	else {
		PyObject * res = PyList_New(2);
		PyList_SetItem(res, 0, PyFloat_FromDouble(lst[0]));
		PyList_SetItem(res, 1, PyFloat_FromDouble(lst[1]));
		return res;
	}
}

static PyObject* auto_scale(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "auto_scale: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	int enable = PyLong_AsLong(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(autoScale, player, (bool)enable));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* set_x_scale(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "set_x_scale: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QVariant min = vipPythonToVariant(PyTuple_GetItem(args, 1));
	QVariant max = vipPythonToVariant(PyTuple_GetItem(args, 2));
	vip_double _min, _max;
	if (min.userType() == QMetaType::LongLong) _min = min.toLongLong();
	else if (min.userType() == qMetaTypeId<vip_double>()) _min = min.value<vip_double>();
	else _min = min.toDouble();
	if (max.userType() == QMetaType::LongLong) _max = max.toLongLong();
	else if (max.userType() == qMetaTypeId<vip_double>()) _max = max.value<vip_double>();
	else _max = max.toDouble();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setXScale, player, _min, _max));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* set_y_scale(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 4) {
		PyErr_SetString(PyExc_RuntimeError, "set_y_scale: wrong number of argument (should be 4)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QVariant min = vipPythonToVariant(PyTuple_GetItem(args, 1));
	QVariant max = vipPythonToVariant(PyTuple_GetItem(args, 2));
	QString unit = pyToString(PyTuple_GetItem(args, 3));
	vip_double _min, _max;
	if (min.userType() == QMetaType::LongLong) _min = min.toLongLong();
	else if (min.userType() == qMetaTypeId<vip_double>()) _min = min.value<vip_double>();
	else _min = min.toDouble();
	if (max.userType() == QMetaType::LongLong) _max = max.toLongLong();
	else if (max.userType() == qMetaTypeId<vip_double>()) _max = max.value<vip_double>();
	else _max = max.toDouble();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setYScale, player, _min, _max, unit));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* set_title(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "set_title: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString title = pyToString(PyTuple_GetItem(args, 1));

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setPlayerTitle, player, title));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject* annotation(PyObject *, PyObject* args)
{
	/*
	args contains:
	- the player id
	- the style
	- the text
	- the position as a list
	- the atributes dictionary
	*/

	size_t size = PyTuple_GET_SIZE(args);
	if (size != 5) {
		PyErr_SetString(PyExc_RuntimeError, "annotation: wrong number of argument (should be 5)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	QString style = pyToString(PyTuple_GetItem(args, 1));
	QString text = pyToString(PyTuple_GetItem(args, 2));
	QVariantList pos = vipPythonToVariant(PyTuple_GetItem(args, 3)).value<QVariantList>();
	QVariantMap attributes = vipPythonToVariant(PyTuple_GetItem(args, 4)).value<QVariantMap>();

	if (!(pos.size() == 2 || pos.size() == 4)) {
		PyErr_SetString(PyExc_RuntimeError, "wrong position format (should be a list of 2 or 4 values)");
		return nullptr;
	}
	QList<double> positions;
	//invert x/y
	positions << pos[1].toDouble() << pos[0].toDouble();
	if (pos.size() == 4)
		positions << pos[3].toDouble() << pos[2].toDouble();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(createAnnotation, player, style, text, positions, attributes));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}


static PyObject* remove_annotation(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "remove_annotation: wrong number of argument (should be 1)");
		return nullptr;
	}

	int id = PyLong_AsLong(PyTuple_GetItem(args, 0));
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(clearAnnotation, id));
	Py_END_ALLOW_THREADS
		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* clear_annotations(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "clear_annotations: wrong number of argument (should be 2)");
		return nullptr;
	}

	int player = PyLong_AsLong(PyTuple_GetItem(args, 0));
	bool all = PyTuple_GetItem(args, 1) == Py_True ? true : false;
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(clearAnnotations, player, all));
	Py_END_ALLOW_THREADS
		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* set_time_markers(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "set_time_markers: wrong number of argument (should be 2)");
		return nullptr;
	}

	qint64 start = vipPythonToVariant(PyTuple_GetItem(args, 0)).toLongLong();
	qint64 end = vipPythonToVariant(PyTuple_GetItem(args, 1)).toLongLong();
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setTimeMarkers, start, end));
	Py_END_ALLOW_THREADS
		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* remove_time_markers(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 0) {
		PyErr_SetString(PyExc_RuntimeError, "remove_time_markers: wrong number of argument (should be 2)");
		return nullptr;
	}

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(removeTimeMarkers));
	Py_END_ALLOW_THREADS
		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* set_row_ratio(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "set_row_ratio: wrong number of argument (should be 2)");
		return nullptr;
	}

	int row = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();
	double ratio = vipPythonToVariant(PyTuple_GetItem(args, 1)).toDouble();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(setRowRatio,row,ratio));
	Py_END_ALLOW_THREADS
		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* imshow(PyObject *, PyObject* args)
{
	/*
	args contains:
	- the array
	- the attributes dictionary
	*/

	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "imshow: wrong number of argument (should be 2)");
		return nullptr;
	}

	VipNDArray ar = vipPythonToVariant(PyTuple_GetItem(args, 0)).value<VipNDArray>();
	QVariantMap attributes = vipPythonToVariant(PyTuple_GetItem(args, 1)).value<QVariantMap>();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(imShow, ar, attributes));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}



static VipNDArray __to1DArray(const QVariant & v)
{
	if (v.userType() == qMetaTypeId<VipNDArray>()) {
		return v.value<VipNDArray>().convert<vip_double>();
	}
	else if (v.userType() == qMetaTypeId<QVariantList>()) {
		QVariantList lst = v.value<QVariantList>();
		VipNDArrayType<vip_double> res(vipVector(lst.size()));
		for (int i = 0; i < lst.size(); ++i) {
			const QVariant t = lst[i];
			if (t.userType() == qMetaTypeId<vip_double>()) res(i) = t.value<vip_double>();
			else res(i) = t.toDouble();
		}
		return res;
	}
	return VipNDArray();
}
static PyObject* plot(PyObject *, PyObject* args)
{
	/*
	args contains:
	- the array
	- the attributes dictionary
	*/

	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "plot: wrong number of argument (should be 2)");
		return nullptr;
	}

	QVariant data = vipPythonToVariant(PyTuple_GetItem(args, 0));
	QVariantMap attributes = vipPythonToVariant(PyTuple_GetItem(args, 1)).value<QVariantMap>();

	VipPointVector vector;
	if (data.userType() == qMetaTypeId<VipNDArray>()) {
		const VipNDArrayType<vip_double> r = data.value<VipNDArray>().convert<vip_double>();
		if (r.isEmpty() || r.shapeCount() != 2 || r.shape(0) != 2) {
			PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
			return nullptr;
		}
		vector.resize(r.shape(1));
		for (int i = 0; i < vector.size(); ++i)
			vector[i] = VipPoint(r(0, i), r(1, i));

	}
	else if (data.userType() == qMetaTypeId<QVariantList>()) {
		QVariantList lst = data.value<QVariantList>();
		if (lst.size() != 2) {
			PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
			return nullptr;
		}
		VipNDArrayType<vip_double> x = __to1DArray(lst[0]);
		VipNDArrayType<vip_double> y = __to1DArray(lst[1]);
		if (x.isEmpty() || y.isEmpty() || x.size() != y.size() || x.shapeCount() != 1 || y.shapeCount() != 1) {
			PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
			return nullptr;
		}
		vector.resize(x.size());
		for (int i = 0; i < vector.size(); ++i)
			vector[i] = VipPoint(x(i), y(i));
	}
	else {
		PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
		return nullptr;
	}

	if (vector.isEmpty()) {
		PyErr_SetString(PyExc_RuntimeError, "empty input data");
		return nullptr;
	}

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(plotData, vector, attributes));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	return PyLong_FromLong(result.first.toInt());
}

static PyObject* plots(PyObject *, PyObject* args)
{
	/*
	args contains:
	- the array
	- the attributes dictionary
	*/

	size_t size = PyTuple_GET_SIZE(args);
	if (size != 2) {
		PyErr_SetString(PyExc_RuntimeError, "plot: wrong number of argument (should be 2)");
		return nullptr;
	}

	const QVariantList datas = vipPythonToVariant(PyTuple_GetItem(args, 0)).value<QVariantList>();
	QVariantMap attributes = vipPythonToVariant(PyTuple_GetItem(args, 1)).value<QVariantMap>();

	

	QList<VipPointVector> vectors;
	for (int i = 0; i < datas.size(); ++i) {
		VipPointVector vector;
		QVariant data = datas[i];
		if (data.userType() == qMetaTypeId<VipNDArray>()) {
			const VipNDArrayType<vip_double> r = data.value<VipNDArray>().convert<vip_double>();
			if (r.isEmpty() || r.shapeCount() != 2 || r.shape(0) != 2) {
				PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
				return nullptr;
			}
			vector.resize(r.shape(1));
			for (int i = 0; i < vector.size(); ++i)
				vector[i] = VipPoint(r(0, i), r(1, i));

		}
		else if (data.userType() == qMetaTypeId<QVariantList>()) {
			QVariantList lst = data.value<QVariantList>();
			if (lst.size() != 2) {
				PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
				return nullptr;
			}
			VipNDArrayType<vip_double> x = __to1DArray(lst[0]);
			VipNDArrayType<vip_double> y = __to1DArray(lst[1]);
			if (x.isEmpty() || y.isEmpty() || x.size() != y.size() || x.shapeCount() != 1 || y.shapeCount() != 1) {
				PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
				return nullptr;
			}
			vector.resize(x.size());
			for (int i = 0; i < vector.size(); ++i)
				vector[i] = VipPoint(x(i), y(i));
		}
		else {
			PyErr_SetString(PyExc_RuntimeError, "wrong input data, should be either a 2D array or a list of 2 1D array");
			return nullptr;
		}

		if (vector.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, "empty input data");
			return nullptr;
		}

		vectors << vector;
	}

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(plotsData, vectors, attributes));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		} 
	return (PyObject*)vipVariantToPython(result.first);
}

static PyObject* resample(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 4) {
		PyErr_SetString(PyExc_RuntimeError, "resample: wrong number of argument (should be 4)");
		return nullptr;
	}

	const QVariantList datas = vipPythonToVariant(PyTuple_GetItem(args, 0)).value<QVariantList>();
	const QString strategy = pyToString(PyTuple_GetItem(args, 1));
	double step = vipPythonToVariant(PyTuple_GetItem(args, 2)).toDouble();
	double padd = vipPythonToVariant(PyTuple_GetItem(args, 3)).toDouble();
	

	if (datas.size() % 2 || datas.size() == 0) {
		PyErr_SetString(PyExc_RuntimeError, "resample: wrong number of input array");
		return nullptr;
	}
	QList<VipPointVector> vectors;
	for (int i = 0; i < datas.size(); i += 2) {
		const VipNDArrayType<double> x = datas[i].value<VipNDArray>().toDouble();
		const VipNDArrayType<double> y = datas[i+1].value<VipNDArray>().toDouble();
		if(x.isEmpty() || y.size() != x.size()) {
			PyErr_SetString(PyExc_RuntimeError, "resample: x and y arrays have different sizes");
			return nullptr;
		}
		if(x.shapeCount() != y.shapeCount()) {
			PyErr_SetString(PyExc_RuntimeError, "resample: x and y arrays have different shapes");
			return nullptr;
		}
		VipPointVector r(x.size());
		for (int i = 0; i < r.size(); ++i)
			r[i] = VipPoint(x[i], y[i]);
		vectors.append(r);
	}

	bool ok;
	if (step) {
		if (strategy == "union") {
			if(vipIsNan(padd))
				ok = vipResampleVectors(vectors, step, ResampleUnion | ResampleInterpolation, padd);
			else
				ok = vipResampleVectors(vectors, step, ResampleUnion | ResampleInterpolation |ResamplePadd0, padd);
		}
		else {
			if (vipIsNan(padd))
				ok = vipResampleVectors(vectors, step, ResampleIntersection | ResampleInterpolation, padd);
			else
				ok = vipResampleVectors(vectors, step, ResampleIntersection | ResampleInterpolation | ResamplePadd0, padd);
		}
	}
	else {
		if (strategy == "union") {
			if (vipIsNan(padd))
				ok = vipResampleVectors(vectors, ResampleUnion | ResampleInterpolation, padd);
			else
				ok = vipResampleVectors(vectors, ResampleUnion | ResampleInterpolation | ResamplePadd0, padd);
		}
		else {
			if (vipIsNan(padd))
				ok = vipResampleVectors(vectors, ResampleIntersection | ResampleInterpolation, padd);
			else
				ok = vipResampleVectors(vectors, ResampleIntersection | ResampleInterpolation | ResamplePadd0, padd);
		}
	}
	if (!ok) {
		PyErr_SetString(PyExc_RuntimeError, "resample: cannot resample input arrays");
		return nullptr;
	}

	//store results
	PyObject * res = PyTuple_New(datas.size());
	for (int i = 0; i < vectors.size(); ++i) {
		VipNDArray x = vipExtractXValues(vectors[i]);
		VipNDArray y = vipExtractYValues(vectors[i]);
		PyTuple_SetItem(res, i * 2, (PyObject*)vipVariantToPython(QVariant::fromValue(x)));
		PyTuple_SetItem(res, i * 2 +1, (PyObject*)vipVariantToPython(QVariant::fromValue(y)));
	}
	return res;
}


static PyObject* test_pid(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 1) {
		PyErr_SetString(PyExc_RuntimeError, "test_pid: wrong number of argument (should be 1)");
		return nullptr;
	}

	qint64 pid = vipPythonToVariant(PyTuple_GetItem(args, 0)).toLongLong();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(testPid, pid));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}


static PyObject* add_function(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 4) {
		PyErr_SetString(PyExc_RuntimeError, "add_function: wrong number of argument (should be 4)");
		return nullptr;
	}

	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();
	PyObject * fun = PyTuple_GetItem(args, 1);
	QString function_name = vipPythonToVariant(PyTuple_GetItem(args, 2)).toString();
	QString item_name = vipPythonToVariant(PyTuple_GetItem(args, 3)).toString();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(addFunction, player, fun, function_name, item_name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject* get_function(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 3) {
		PyErr_SetString(PyExc_RuntimeError, "get_function: wrong number of argument (should be 3)");
		return nullptr;
	}

	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();
	QString function_name = vipPythonToVariant(PyTuple_GetItem(args, 1)).toString();
	QString item_name = vipPythonToVariant(PyTuple_GetItem(args, 2)).toString();

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(getFunction, player, function_name, item_name));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	VipProcessingObject * obj = result.first.value<VipProcessingObject*>();
	PyFunctionProcessing * fun = static_cast<PyFunctionProcessing*>(obj);
	if (fun) {
		PyObject * res = (PyObject*)fun->function();
		if (res) {
			Py_INCREF(res);
			return res;
		}
	}
	PyErr_SetString(PyExc_RuntimeError, ("cannot retrieve function object " + function_name + ": nullptr object").toLatin1().data());
	return nullptr;
}



static PyObject* user_input(PyObject*, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size < 2) {
		PyErr_SetString(PyExc_RuntimeError, "user_input: wrong number of argument (should be at least 2)");
		return nullptr;
	}

	QString title = vipPythonToVariant(PyTuple_GetItem(args, 0)).toString();
	QList<QVariantList> lst;
	for (size_t i = 1; i < size; ++i) {
		lst.append(vipPythonToVariant(PyTuple_GetItem(args, i)).value<QVariantList>());
	}
	 
	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(userInput, title, lst));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}

	return (PyObject*)vipVariantToPython(result.first);
}


static PyObject* add_widget_to_player(PyObject *, PyObject* args)
{
	size_t size = PyTuple_GET_SIZE(args);
	if (size != 5) {
		PyErr_SetString(PyExc_RuntimeError, "add_widget_to_player: wrong number of argument (should be 5)");
		return nullptr;
	}

	int player = vipPythonToVariant(PyTuple_GetItem(args, 0)).toInt();
	QString side = vipPythonToVariant(PyTuple_GetItem(args, 1)).toString();
	QString wname = vipPythonToVariant(PyTuple_GetItem(args, 2)).toString();
	QString oname = vipPythonToVariant(PyTuple_GetItem(args, 3)).toString();
	PyObject * widget = PyTuple_GetItem(args, 4);

	ResultType result;
	Py_BEGIN_ALLOW_THREADS
		result = execDelayFunction(std::bind(addWidgetToPlayer, player, side, wname, oname));
	Py_END_ALLOW_THREADS

		if (!result.second.isEmpty()) {
			PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
			return nullptr;
		}
	Py_INCREF(widget);
	Py_INCREF(Py_None);
	return Py_None;
}



static PyObject* callRegisteredFun(PyObject*, PyObject* args)
{
	QVariantList lst = vipPythonToVariant(args).value<QVariantList>();
	QString name = lst[0].toString();
	lst = lst.mid(1);
	 
	ResultType result;
	VipFunctionObject obj = vipFindFunction(name);
	if (!obj.isValid()) {
		PyErr_SetString(PyExc_RuntimeError, ("Cannot find function " + name).toLatin1().data());
		return nullptr;
	}

	if (obj.mainThread) {

		Py_BEGIN_ALLOW_THREADS
			result = execDelayFunction(std::bind(callRegisteredFunction, obj, lst));
		Py_END_ALLOW_THREADS
	}
	else { 
		result.first = obj.function(lst);
	}
	 
	if (!result.second.isEmpty()) {
		PyErr_SetString(PyExc_RuntimeError, result.second.toLatin1().data());
		return nullptr;
	}
	if (result.first.userType() == 0) {
		Py_INCREF(Py_None);
		return Py_None;
	}
	else {
		if (result.first.userType() == qMetaTypeId<VipErrorData>()) {
			PyErr_SetString(PyExc_RuntimeError, result.first.value<VipErrorData>().errorString().toLatin1().data());
			return nullptr;
		}
		PyObject * res =(PyObject*)vipVariantToPython(result.first);
		if (!res) {
			PyErr_SetString(PyExc_RuntimeError, ("Cannot interpret result of function " + name + ", type is " + QString(result.first.typeName())).toLatin1().data());
			return nullptr;
		}
		return res;
	}
}



/*
static PyObject* open_path(PyObject *self, PyObject* args)
static PyObject* close_window(PyObject *self, PyObject* args)
static PyObject* show_maximized(PyObject *self, PyObject* args)
static PyObject* show_normal(PyObject *self, PyObject* args)
static PyObject* show_minimized(PyObject *self, PyObject* args)
static PyObject* workspace(PyObject *self, PyObject* args)
static PyObject* workspaces(PyObject *self, PyObject* args)
static PyObject* current_workspace(PyObject *self, PyObject* args)
static PyObject* reorganize(PyObject *self, PyObject* args)
static PyObject* time(PyObject *self, PyObject* args)
static PyObject* set_time(PyObject *self, PyObject* args)
static PyObject* time_range(PyObject *self, PyObject* args)
static PyObject* set_stylesheet(PyObject *self, PyObject* args)
*/



static PyMethodDef Thermavip_methods[] =
{
	{ "player_type", player_type, METH_VARARGS, "internal.player_type" },
	{ "item_list", item_list, METH_VARARGS, "internal.item_list" },
	{ "set_selected", set_selected, METH_VARARGS, "internal.set_selected" },
	{ "unselect_all", unselect_all, METH_VARARGS, "internal.unselect_all" },
	{ "query", query, METH_VARARGS, "internal.query" },
	{ "open", open_path, METH_VARARGS, "internal.open" },
	{ "close", close_window, METH_VARARGS, "internal.close" },
	{ "show_maximized", show_maximized, METH_VARARGS, "internal.show_maximized" },
	{ "show_normal", show_normal, METH_VARARGS, "internal.show_normal" },
	{ "show_minimized", show_minimized, METH_VARARGS, "internal.show_minimized" },
	{ "workspace", workspace, METH_VARARGS, "internal.workspace" },
	{ "workspaces", workspaces, METH_VARARGS, "internal.workspaces" },
	{ "current_workspace", current_workspace, METH_VARARGS, "internal.current_workspace" },
	{ "workspace_title", workspace_title, METH_VARARGS, "internal.workspace_title" },
	{ "set_workspace_title", set_workspace_title, METH_VARARGS, "internal.set_workspace_title" },
	{ "reorganize", reorganize, METH_VARARGS, "internal.reorganize" },
	{ "time", time, METH_VARARGS, "internal.time" },
	{ "set_time", set_time, METH_VARARGS, "internal.set_time" },
	{ "next_time", next_time, METH_VARARGS, "internal.next_time" },
	{ "previous_time", previous_time, METH_VARARGS, "internal.previous_time" },
	{ "closest_time", closest_time, METH_VARARGS, "internal.closest_time" },
	{ "time_range", time_range, METH_VARARGS, "internal.time_range" },
	{ "set_stylesheet", set_stylesheet, METH_VARARGS, "internal.set_stylesheet" },
	{ "clamp_time", clamp_time, METH_VARARGS, "internal.clamp_time" },
	{ "top_level", top_level, METH_VARARGS, "internal.top_level" },
	{ "resize_workspace", resize_workspace, METH_VARARGS, "internal.resize_workspace" },
	{ "get", get, METH_VARARGS, "internal.get" },
	{ "get_attribute", get_attribute, METH_VARARGS, "internal.get_attribute" },
	{ "get_attributes", get_attributes, METH_VARARGS, "internal.get_attributes" },
	{ "set_attribute", set_attribute, METH_VARARGS, "internal.set_attribute" },
	{ "get_roi", roi, METH_VARARGS, "internal.get_roi" },
	{ "get_roi_bounding_rect", get_roi_bounding_rect, METH_VARARGS, "internal.get_roi_bounding_rect" },
	{ "get_roi_filled_points", roi_filled_points, METH_VARARGS, "internal.get_roi_filled_points" },
	{ "clear_roi", clear_roi, METH_VARARGS, "internal.clear_roi" },
	{ "add_roi", add_roi, METH_VARARGS, "internal.add_roi" },
	{ "add_ellipse", add_ellipse, METH_VARARGS, "internal.add_ellipse" },
	{ "add_circle", add_circle, METH_VARARGS, "internal.add_circle" },
	{ "time_trace", time_trace, METH_VARARGS, "internal.time_trace" },
	{ "remove", remove, METH_VARARGS, "internal.remove" },
	{ "set_time_marker", set_time_marker, METH_VARARGS, "internal.set_time_marker" },
	{ "zoom", zoom, METH_VARARGS, "internal.zoom" },
	{ "set_color_map_scale", set_color_map_scale, METH_VARARGS, "internal.set_color_map_scale" },
	{ "x_range", x_range, METH_VARARGS, "internal.x_range" },
	{ "player_range", player_range, METH_VARARGS, "internal.player_range" },
	{ "current_player", current_player, METH_VARARGS, "internal.current_player" },
	{ "set_time_markers", set_time_markers, METH_VARARGS, "internal.set_time_markers" },
	{ "remove_time_markers", remove_time_markers, METH_VARARGS, "internal.remove_time_markers" },
	{ "set_row_ratio", set_row_ratio, METH_VARARGS, "internal.set_row_ratio" },
	{ "set_title", set_title, METH_VARARGS, "internal.set_title" },
	{ "annotation", annotation, METH_VARARGS, "internal.annotation" },
	{ "remove_annotation", remove_annotation, METH_VARARGS, "internal.remove_annotation" },
	{ "clear_annotations", clear_annotations, METH_VARARGS, "internal.clear_annotations" },
	{ "imshow", imshow, METH_VARARGS, "internal.imshow" },
	{ "plot", plot, METH_VARARGS, "internal.plot" },
	{ "plots", plots, METH_VARARGS, "internal.plots" },
	{ "auto_scale", auto_scale, METH_VARARGS, "internal.auto_scale" },
	{ "set_x_scale", set_x_scale, METH_VARARGS, "internal.set_x_scale" },
	{ "set_y_scale", set_y_scale, METH_VARARGS, "internal.set_y_scale" }, 
	{ "resample", resample, METH_VARARGS, "internal.resample" },
	{ "add_function", add_function, METH_VARARGS, "internal.add_function" },
	{ "get_function", get_function, METH_VARARGS, "internal.get_function" },
	{ "user_input", user_input, METH_VARARGS, "internal.user_input" },
	{ "test_pid", test_pid, METH_VARARGS, "internal.test_pid" },
	{ "add_widget_to_player", add_widget_to_player, METH_VARARGS, "internal.add_widget_to_player" },
	{ "call_internal_func", callRegisteredFun, METH_VARARGS, "internal.call_internal_func" }, 
	{ 0, 0, 0, 0 } // sentinel 
};
 
static struct PyModuleDef Thermavip_module = {
	PyModuleDef_HEAD_INIT,
	"internal",   /* name of module */
	nullptr, /* module documentation, may be nullptr */
	-1,       /* size of per-interpreter state of the module,
			  or -1 if the module keeps state in global variables. */
	Thermavip_methods
};

// Export the module that defines the redirection functions for standard output, error and input
PyObject* PyInit_thermavip()
{
	static QBasicAtomicInt reg = Q_BASIC_ATOMIC_INITIALIZER(0);
	static PyObject* thermavip = nullptr;
	if (reg.loadAcquire())
		return thermavip;
	else
	{
		reg.storeRelease(1);
		thermavip = PyModule_Create(&Thermavip_module);
		return thermavip;
	}
}
