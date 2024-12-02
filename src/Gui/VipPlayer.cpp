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

#include "VipPlayer.h"
#include "VipAnnotationEditor.h"
#include "VipAxisBase.h"
#include "VipCorrectedTip.h"
#include "VipDisplayArea.h"
#include "VipDisplayObject.h"
#include "VipDrawShape.h"
#include "VipDynGridLayout.h"
#include "VipExtractStatistics.h"
#include "VipGenericDevice.h"
#include "VipGui.h"
#include "VipIODevice.h"
#include "VipLegendItem.h"
#include "VipLogging.h"
#include "VipMimeData.h"
#include "VipMultiNDArray.h"
#include "VipMultiPlotWidget2D.h"
#include "VipNDArrayImage.h"
#include "VipPlayWidget.h"
#include "VipPlotGrid.h"
#include "VipPlotShape.h"
#include "VipPlotSpectrogram.h"
#include "VipPlotWidget2D.h"
#include "VipProcessingObjectEditor.h"
#include "VipProcessingObjectTree.h"
#include "VipProgress.h"
#include "VipResizeItem.h"
#include "VipSet.h"
#include "VipStandardEditors.h"
#include "VipStandardProcessing.h"
#include "VipStandardWidgets.h"
#include "VipSymbol.h"
#include "VipTextOutput.h"
#include "VipToolTip.h"
#include "VipXmlArchive.h"

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QGridLayout>
#include <QLayoutItem>
#include <QMenu>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QToolTip>
#include <QWidgetAction>
#include <qclipboard.h>
#include <qradiobutton.h>

#include <functional>

#define _VIP_USE_LEFT_SCALE_ONLY

#ifdef _VIP_USE_LEFT_SCALE_ONLY
#define _VIP_PLOT_TYPE VipPlotWidget2D::VMulti
#else
#define _VIP_PLOT_TYPE VipPlotWidget2D::Simple
#endif

VipPlayerToolBar::VipPlayerToolBar(QWidget* parent)
  : VipToolBar(parent)
{
	setShowAdditionals(VipToolBar::ShowInToolBar);
	saveitem = new QToolButton(this);
	saveitem->setToolTip(tr("<b>Save current raw data</b><br>Save a raw data in ascii or other formats: an image, a curve,..."));
	saveitem->setIcon(vipIcon("save.png"));
	saveitem->setAutoRaise(true);
	saveItemMenu = new QMenu(saveitem);
	saveitem->setMenu(saveItemMenu);
	saveitem->setPopupMode(QToolButton::InstantPopup);
	saveItemAction = this->addWidget(saveitem);

	// selectionMode = new QToolButton(this);
	//  selectionMode->setToolTip(QString("Area zooming"));
	//  selectionMode->setIcon(vipIcon("zoom_area.png"));
	//  selectionMode->setAutoRaise(true);
	//  selectionMode->setCheckable(true);
	//  selectionModeAction = this->addWidget(selectionMode);
	selectionModeAction = this->addAction(vipIcon("zoom_area.png"), "Area zooming");
	selectionModeAction->setCheckable(true);
}

// class VipDefaultSceneModelDisplayOptions::PrivateData
// {
// public:
// VipTextStyle style;
// QPen pen;
// QBrush brush;
// VipPlotShape::DrawComponents components;
// };
//
// VipDefaultSceneModelDisplayOptions::~VipDefaultSceneModelDisplayOptions()
// {
// delete d_data;
// }
//
// VipDefaultSceneModelDisplayOptions::VipDefaultSceneModelDisplayOptions()
// {
// VIP_CREATE_PRIVATE_DATA(d_data);
// d_data->pen = QPen();
// d_data->brush = QBrush(QColor(255, 0, 0, 70));
// d_data->components = VipPlotShape::Background | VipPlotShape::Border | VipPlotShape::Id;
// }
//
// VipTextStyle VipDefaultSceneModelDisplayOptions::textStyle()
// {
// return instance().d_data->style;
// }
//
// QPen VipDefaultSceneModelDisplayOptions::borderPen()
// {
// return instance().d_data->pen;
// }
//
// QBrush VipDefaultSceneModelDisplayOptions::backgroundBrush()
// {
// return instance().d_data->brush;
// }
//
// VipPlotShape::DrawComponents VipDefaultSceneModelDisplayOptions::drawComponents()
// {
// return instance().d_data->components;
// }
//
// void VipDefaultSceneModelDisplayOptions::setBorderPen(const QPen & pen)
// {
// instance().d_data->pen = pen;
// applyAll();
// Q_EMIT instance().changed();
// }
//
// void VipDefaultSceneModelDisplayOptions::setBackgroundBrush(const QBrush & brush)
// {
// instance().d_data->brush = brush;
// applyAll();
// Q_EMIT instance().changed();
// }
//
// void VipDefaultSceneModelDisplayOptions::setDrawComponents(const VipPlotShape::DrawComponents & c)
// {
// instance().d_data->components = c;
// applyAll();
// Q_EMIT instance().changed();
// }
//
// void VipDefaultSceneModelDisplayOptions::setTextStyle(const VipTextStyle & s)
// {
// instance().d_data->style = s;
// applyAll();
// Q_EMIT instance().changed();
// }
//
// void VipDefaultSceneModelDisplayOptions::applyAll()
// {
// QList<VipPlayer2D*> players = VipUniqueId::objects<VipPlayer2D>();
// for (int i = 0; i < players.size(); ++i)
// {
// QList<VipPlotSceneModel*> models = players[i]->plotSceneModels();
// for (int j = 0; j < models.size(); ++j) {
//	models[j]->setPen("All", borderPen());
//	models[j]->setBrush("All", backgroundBrush());
//	models[j]->setDrawComponents("All", drawComponents());
// }
// }
// }
//
// VipDefaultSceneModelDisplayOptions & VipDefaultSceneModelDisplayOptions::instance()
// {
// static VipDefaultSceneModelDisplayOptions inst;
// return inst;
// }

class VipPlotItemClipboard::PrivateData
{
public:
	VipMimeDataDuplicatePlotItem duplicate;
};

VipPlotItemClipboard::VipPlotItemClipboard()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}
VipPlotItemClipboard::~VipPlotItemClipboard()
{
}

void VipPlotItemClipboard::copy(const PlotItemList& items)
{
	instance().d_data->duplicate.clearItems();

	PlotItemList copied;
	for (int i = 0; i < items.size(); ++i)
		if (items[i]) {
			copied.append(items[i]);
		}

	instance().d_data->duplicate.setPlotItems(copied);

	Q_EMIT instance().itemsCopied(copied);
}

PlotItemList VipPlotItemClipboard::copiedItems()
{
	return instance().d_data->duplicate.plotItems();
}

void VipPlotItemClipboard::paste(VipAbstractPlotArea* dst, QWidget* drop_widget)
{
	VipPlotItem* drop_target = nullptr;
	if (dst) {
		drop_target = dst->lastPressed();
		if (!drop_target)
			drop_target = dst->canvas();
	}
	instance().d_data->duplicate.plotData(drop_target, drop_widget);

	Q_EMIT instance().itemsPasted(instance().d_data->duplicate.plotItems());
}

bool VipPlotItemClipboard::supportDestinationPlayer(VipAbstractPlayer* pl)
{
	return instance().d_data->duplicate.supportDestinationPlayer(pl);
}

bool VipPlotItemClipboard::supportSourceItems()
{
	return VipMimeDataDuplicatePlotItem::supportSourceItems(instance().d_data->duplicate.plotItems());
}

const QMimeData* VipPlotItemClipboard::mimeData()
{
	return &(instance().d_data->duplicate);
}

VipPlotItemClipboard& VipPlotItemClipboard::instance()
{
	static VipPlotItemClipboard clipboard;
	return clipboard;
}

class VipPlayerLifeTime::PrivateData
{
public:
	QMutex mutex;
	QList<VipAbstractPlayer*> players;
};

VipPlayerLifeTime::VipPlayerLifeTime()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipPlayerLifeTime::~VipPlayerLifeTime()
{
}

VipPlayerLifeTime* VipPlayerLifeTime::instance()
{
	static VipPlayerLifeTime inst;
	return &inst;
}
QList<VipAbstractPlayer*> VipPlayerLifeTime::players()
{
	QMutexLocker lock(&instance()->d_data->mutex);
	QList<VipAbstractPlayer*> res = instance()->d_data->players;
	res.detach();
	return res;
}

void VipPlayerLifeTime::emitCreated(VipAbstractPlayer* p)
{
	{
		QMutexLocker lock(&instance()->d_data->mutex);
		if (instance()->d_data->players.indexOf(p) < 0) {
			instance()->d_data->players.push_back(p);
		}
	}
	Q_EMIT instance()->created(p);
}
void VipPlayerLifeTime::emitDestroyed(VipAbstractPlayer* p)
{
	bool emit_sig = false;
	{
		QMutexLocker lock(&instance()->d_data->mutex);
		if (instance()->d_data->players.removeOne(p))
			emit_sig = true;
	}
	if (emit_sig)
		Q_EMIT instance()->destroyed(p);
}

VipPlotWidget::VipPlotWidget(QWidget* parent)
  : QWidget(parent)
  , VipRenderObject(this)
{
	setLayout(new QGridLayout());
}

VipPlotWidget::VipPlotWidget(VipAbstractPlotWidget2D* plot, QWidget* parent)
  : QWidget(parent)
  , VipRenderObject(this)
{
	setLayout(new QGridLayout());
	setPlotWidget2D(plot);
}

void VipPlotWidget::setPlotWidget2D(VipAbstractPlotWidget2D* plot)
{
	QLayoutItem* item = gridLayout()->itemAtPosition(10, 10);
	if (item && item->widget())
		item->widget()->close();

	gridLayout()->addWidget(plot, 10, 10);
}

VipAbstractPlotWidget2D* VipPlotWidget::plotWidget2D() const
{
	QLayoutItem* item = gridLayout()->itemAtPosition(10, 10);
	if (item)
		return qobject_cast<VipAbstractPlotWidget2D*>(item->widget());
	else
		return nullptr;
}

QGridLayout* VipPlotWidget::gridLayout() const
{
	return static_cast<QGridLayout*>(layout());
}

class VipAbstractPlayer::PrivateData
{
public:
	PrivateData()
	  : inDestructor(false)
	  , automaticWindowTitle(true)
	{
	}
	QPointer<VipProcessingPool> pool;

	bool inDestructor;
	bool automaticWindowTitle;
};

VipAbstractPlayer::VipAbstractPlayer(QWidget* parent)
  : VipPlotWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	VipUniqueId::id(this);

	VipPlayerLifeTime::emitCreated(this);
}

VipAbstractPlayer::~VipAbstractPlayer()
{
	VipPlayerLifeTime::emitDestroyed(this);

	d_data->inDestructor = true;
	if (d_data->pool) {
		d_data->pool->stop();
		d_data->pool->stopStreaming();
		vipProcessEvents(nullptr, 500);
	}

}

void VipAbstractPlayer::setProcessingPool(VipProcessingPool* pool)
{
	if (pool != d_data->pool) {
		d_data->pool = pool;

		if (pool) {
			// the processing pool to all display objects and their sources
			QList<VipDisplayObject*> disps = this->displayObjects();
			for (int i = 0; i < disps.size(); ++i) {
				VipDisplayObject* disp = disps[i];
				disp->setParent(pool);
				QList<VipProcessingObject*> sources = disp->allSources();
				for (int j = 0; j < sources.size(); ++j)
					sources[j]->setParent(pool);
			}
		}
	}
}

VipProcessingPool* VipAbstractPlayer::processingPool() const
{
	return d_data->pool;
}

void VipAbstractPlayer::showEvent(QShowEvent*)
{
	QList<VipDisplayObject*> lst = this->displayObjects();
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->setEnabled(isEnabled());
}

void VipAbstractPlayer::hideEvent(QHideEvent*)
{
	QList<VipDisplayObject*> lst = this->displayObjects();
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->setEnabled(true);
}

void VipAbstractPlayer::changeEvent(QEvent* e)
{
	if (e->type() == QEvent::EnabledChange) {
		QList<VipDisplayObject*> lst = this->displayObjects();
		for (int i = 0; i < lst.size(); ++i)
			lst[i]->setEnabled(isEnabled() && isVisible());
	}
}

void VipAbstractPlayer::dragEnterEvent(QDragEnterEvent* evt)
{
	VipPlotWidget::dragEnterEvent(evt);
	// evt->accept();
}
void VipAbstractPlayer::dragLeaveEvent(QDragLeaveEvent* evt)
{
	VipPlotWidget::dragLeaveEvent(evt);
}
void VipAbstractPlayer::dragMoveEvent(QDragMoveEvent* evt)
{
	VipPlotWidget::dragMoveEvent(evt);
}
void VipAbstractPlayer::dropEvent(QDropEvent* evt)
{
	VipPlotWidget::dropEvent(evt);
}

bool VipAbstractPlayer::inDestructor() const
{
	return d_data->inDestructor;
}

void VipAbstractPlayer::setAutomaticWindowTitle(bool enable)
{
	d_data->automaticWindowTitle = enable;
}
bool VipAbstractPlayer::automaticWindowTitle() const
{
	return d_data->automaticWindowTitle;
}

VipDisplayPlayerArea* VipAbstractPlayer::parentDisplayArea() const
{
	QWidget* w = parentWidget();
	while (w) {
		if (qobject_cast<VipDisplayPlayerArea*>(w))
			return static_cast<VipDisplayPlayerArea*>(w);

		w = w->parentWidget();
	}
	return nullptr;
}

int VipAbstractPlayer::parentId() const
{
	QWidget* parent = this->parentWidget();
	while (parent) {
		if (VipBaseDragWidget* w = qobject_cast<VipBaseDragWidget*>(parent))
			return VipUniqueId::id<VipBaseDragWidget>(w);
		parent = parent->parentWidget();
	}
	return 0;
}

QSize VipAbstractPlayer::sizeHint() const
{
	QList<VipDisplayObject*> displays = displayObjects();
	QSize res;
	for (int i = 0; i < displays.size(); ++i) {
		QSize tmp = displays[i]->sizeHint();
		res.setWidth(qMax(res.width(), tmp.width()));
		res.setHeight(qMax(res.height(), tmp.height()));
	}

	if (res != QSize()) {
		// find the size of the outer widgets (around the plot area)
		// QRectF plot_area_rect = this->plotWidget2D()->area()->canvas()->mapToScene(this->plotWidget2D()->area()->canvas()->boundingRect()).boundingRect();
		//  plot_area_rect =  this->plotWidget2D()->mapFromScene(plot_area_rect).boundingRect();
		//  QSize additional = this->plotWidget2D()->size() - plot_area_rect.size().toSize();
		//  additional += QSize(10,60);//this->size() - this->plotWidget2D()->size();
		//  res += additional;
		// we want, if possible, a minimum width and height of 250
		double ratio = res.width() / double(res.height());
		if (res.width() < 250) {
			res.setWidth(250);
			res.setHeight(250 / ratio);
		}
		if (res.height() < 250) {
			res.setHeight(250);
			res.setWidth(250 * ratio);
		}

		res += QSize(150, 100);
		if (const VipVideoPlayer* pl = qobject_cast<const VipVideoPlayer*>(this))
			if (pl->isShowAxes() == false) {
				res += QSize(0, 70);
			}
	}
	else
		res = QSize(700, 500); // VipPlotWidget::sizeHint();

	// we want a maximum width and height of 800 while keeping the ratio
	double w_ratio = res.width() / 800.0;
	double h_ratio = res.height() / 800.0;
	if (w_ratio > 1 && w_ratio > h_ratio) {
		res.setWidth(res.width() / w_ratio);
		res.setHeight(res.height() / w_ratio);
	}
	else if (h_ratio > 1 && h_ratio > w_ratio) {
		res.setWidth(res.width() / h_ratio);
		res.setHeight(res.height() / h_ratio);
	}

	return res;
}

void VipAbstractPlayer::startRender(VipRenderState& state)
{
	// we render on a transparent background, so we usually need non white scales and texts
	if (plotWidget2D()) {

		// TEST: save (image, video,...) using gray-like style
		static const QString gray_style = "VipAbstractPlotArea { title-color: black; }"
						  "VipAbstractScale {pen-color: black; label-color: black; title-color: black;}"
						  "VipLegend{ color: black;}"
						  "VipPlotItem {title-color: black; color: black;}";

		state.state(this)["StyleSheet"] = QVariant::fromValue(plotWidget2D()->area()->styleSheet());
		plotWidget2D()->area()->setStyleSheet(gray_style);
		/* plotWidget2D()->save();
		if (plotWidget2D()->axisColor() == Qt::white) plotWidget2D()->setAxisColor(Qt::black);
		if (plotWidget2D()->axisTextColor() == Qt::white) plotWidget2D()->setAxisTextColor(Qt::black);
		if (plotWidget2D()->axisTitleColor() == Qt::white) plotWidget2D()->setAxisTitleColor(Qt::black);
		if (plotWidget2D()->titleColor() == Qt::white) plotWidget2D()->setTitleColor(Qt::black);
		if (plotWidget2D()->legendColor() == Qt::white) plotWidget2D()->setLegendColor(Qt::black);*/

		VipRenderObject::startRender(plotWidget2D()->scene(), state);
	}

	Q_EMIT renderStarted(state);
}

void VipAbstractPlayer::endRender(VipRenderState& state)
{
	if (plotWidget2D()) {
		// TEST
		// plotWidget2D()->restore();
		VipStyleSheet sh = state.state(this)["StyleSheet"].value<VipStyleSheet>();
		plotWidget2D()->area()->setStyleSheet(sh);

		VipRenderObject::endRender(plotWidget2D()->scene(), state);
	}
	Q_EMIT renderEnded(state);
}

VipAbstractPlayer* VipAbstractPlayer::findAbstractPlayer(QGraphicsItem* child)
{
	if (!child)
		return nullptr;
	if (!child->scene())
		return nullptr;

	QList<QGraphicsView*> v = child->scene()->views();
	if (!v.size())
		return nullptr;

	QWidget* w = v[0];
	while (w) {
		VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(w);
		if (pl)
			return pl;
		w = w->parentWidget();
	}

	return nullptr;
}

VipAbstractPlayer* VipAbstractPlayer::findAbstractPlayer(VipDisplayObject* display)
{
	if (VipTypeId* ids = VipUniqueId::typeId(&VipAbstractPlayer::staticMetaObject)) {
		QList<QObject*> objs = ids->objects();
		for (int i = 0; i < objs.size(); ++i) {
			if (VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(objs[i])) {
				if (pl->displayObjects().indexOf(display) >= 0)
					return pl;
			}
		}
	}
	return nullptr;
}

QList<VipAbstractPlayer*> VipAbstractPlayer::findOutputPlayers(VipProcessingObject* proc)
{
	if (proc->outputCount() == 0)
		return QList<VipAbstractPlayer*>();

	// find in the parent processing list
	if (VipProcessingList* lst = proc->property("VipProcessingList").value<VipProcessingList*>())
		return findOutputPlayers(lst);

	QList<VipDisplayObject*> displays = vipListCast<VipDisplayObject*>(proc->allSinks());
	QList<VipAbstractPlayer*> res;
	for (int i = 0; i < displays.size(); ++i) {
		if (VipAbstractPlayer* pl = vipFindParent<VipAbstractPlayer>(displays[i]->widget()))
			if (res.indexOf(pl) < 0)
				res.append(pl);
	}
	return res;
}

class VipWidgetPlayer::PrivateData
{
public:
	PrivateData() {}
	QPointer<QWidget> widget;
};

VipWidgetPlayer::VipWidgetPlayer(QWidget* w, QWidget* parent)
  : VipAbstractPlayer(parent)
  , d_data(new PrivateData())
{
	setWidget(w);
}

VipWidgetPlayer::~VipWidgetPlayer()
{
}

QSize VipWidgetPlayer::sizeHint() const
{
	return d_data->widget ? d_data->widget->sizeHint() : QWidget::sizeHint();
}

void VipWidgetPlayer::setWidget(QWidget* w)
{
	if (d_data->widget != w) {
		if (d_data->widget)
			delete d_data->widget;
		d_data->widget = w;
		if (w) {
			w->setParent(this);
			// if (d_data->transparent)
			// delete d_data->transparent;
			// data->transparent = new TransparentWidget(this);
		}
		resizeEvent(nullptr);
	}
}
QWidget* VipWidgetPlayer::widget() const
{
	return d_data->widget;
}

void VipWidgetPlayer::resizeEvent(QResizeEvent*)
{
	if (widget()) {
		widget()->resize(this->size());
		// d_data->transparent->resize(size());
	}
}

bool VipWidgetPlayer::renderObject(QPainter* p, const QPointF& pos, bool)
{
	this->render(p, pos.toPoint(), QRegion(), QWidget::DrawChildren);
	return true;
}

static VipArchive& operator<<(VipArchive& arch, const VipWidgetPlayer* pl)
{
	if (pl->widget())
		arch.content("widget", pl->widget());
	return arch;
}
static VipArchive& operator>>(VipArchive& arch, VipWidgetPlayer* pl)
{
	arch.save();
	QWidget* w = arch.read("widget").value<QWidget*>();
	if (!arch)
		arch.restore();
	if (w)
		pl->setWidget(w);
	return arch;
}

VipPlayerToolTip::VipPlayerToolTip() {}

VipPlayerToolTip::~VipPlayerToolTip() {}

VipPlayerToolTip& VipPlayerToolTip::instance()
{
	static VipPlayerToolTip inst;
	return inst;
}

void VipPlayerToolTip::setToolTipFlags(VipToolTip::DisplayFlags flags, const QMetaObject* meta)
{
	instance().m_flags[meta->className()] = flags;

	QList<QObject*> players = VipUniqueId::objects(meta);
	for (int i = 0; i < players.size(); ++i)
		if (VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(players[i]))
			pl->toolTipFlagsChanged(flags);
}

VipToolTip::DisplayFlags VipPlayerToolTip::toolTipFlags(const QMetaObject* meta)
{
	QMap<QString, VipToolTip::DisplayFlags>::const_iterator it = instance().m_flags.find(meta->className());
	if (it != instance().m_flags.end())
		return it.value();
	else
		return (VipToolTip::DisplayFlags)(VipToolTip::All & (~VipToolTip::SearchXAxis) & (~VipToolTip::SearchYAxis));
}

bool VipPlayerToolTip::setDefaultToolTipFlags(VipToolTip::DisplayFlags flags, const QMetaObject* meta)
{
	QMap<QString, VipToolTip::DisplayFlags>::const_iterator it = instance().m_flags.find(meta->className());
	if (it == instance().m_flags.end()) {
		instance().m_flags[meta->className()] = flags;
		return true;
	}
	else
		return false;
}

QMap<QString, VipToolTip::DisplayFlags> VipPlayerToolTip::allToolTipFlags()
{
	return instance().m_flags;
}

void VipPlayerToolTip::setAllToolTipFlags(const QMap<QString, VipToolTip::DisplayFlags>& flags)
{
	instance().m_flags = flags;
}

class VipPlayer2D::PrivateData
{
public:
	VipPlayerToolBar* toolBar;
	VipToolBar* afterTitleToolBar;
	QVBoxLayout* toolBarLayout;
	QLabel* statusText;
	QWidget* toolBarWidget;
	qint64 keyEventId;
	QPointer<VipPlotSceneModel> plotScene;
	QList<QPointer<QToolBar>> bars;
	QPoint lastMousePress;
};

VipPlayer2D::VipPlayer2D(QWidget* parent)
  : VipAbstractPlayer(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->keyEventId = 0;
	d_data->toolBar = new VipPlayerToolBar(this);
	d_data->toolBar->setIconSize(QSize(20, 20));
	d_data->toolBar->setMaximumHeight(26);

	d_data->afterTitleToolBar = new VipToolBar(this);
	d_data->afterTitleToolBar->setIconSize(QSize(20, 20));
	d_data->afterTitleToolBar->setMaximumHeight(26);

	d_data->statusText = new QLabel();
	d_data->statusText->setWordWrap(true);

	d_data->toolBarLayout = new QVBoxLayout();
	d_data->toolBarLayout->setContentsMargins(0, 0, 0, 0);
	d_data->toolBarWidget = new QWidget();
	d_data->toolBarWidget->setLayout(d_data->toolBarLayout);

	this->gridLayout()->addWidget(d_data->toolBarWidget, 9, 10);
	this->gridLayout()->addWidget(d_data->statusText, 20, 10);
	this->gridLayout()->setContentsMargins(0, 0, 0, 0);
	this->gridLayout()->setSpacing(0);

	d_data->toolBarLayout->addWidget(d_data->toolBar);
	d_data->bars.append(d_data->toolBar);

	connect(d_data->toolBar->selectionModeAction, SIGNAL(triggered(bool)), this, SLOT(selectionZoomArea(bool)), Qt::QueuedConnection);
	connect(d_data->toolBar->saveItemMenu, SIGNAL(aboutToShow()), this, SLOT(saveMenuPopup()));
	connect(d_data->toolBar->saveItemMenu, SIGNAL(triggered(QAction*)), this, SLOT(saveMenuClicked(QAction*)));

	VipUniqueId::id(this);

	VipPlayerLifeTime::emitCreated(this);
}

VipPlayer2D::~VipPlayer2D()
{
	VipPlayerLifeTime::emitDestroyed(this);
}

QMenu* VipPlayer2D::generateToolTipMenu()
{
	VipToolTip::DisplayFlags flags = VipPlayerToolTip::toolTipFlags(this->metaObject());

	QMenu* menu = new QMenu();
	menu->setToolTipsVisible(true);
	QAction* hidden = menu->addAction("Tool tip hidden");
	hidden->setData((int)VipToolTip::Hidden);
	hidden->setCheckable(true);
	QAction* axes = menu->addAction("Tool tip: show axis values");
	axes->setData((int)VipToolTip::Axes);
	axes->setCheckable(true);
	QAction* title = menu->addAction("Tool tip: show item titles");
	title->setData((int)VipToolTip::ItemsTitles);
	title->setCheckable(true);
	QAction* legend = menu->addAction("Tool tip: show item legends");
	legend->setData((int)VipToolTip::ItemsLegends);
	legend->setCheckable(true);
	QAction* position = menu->addAction("Tool tip: show item positions");
	position->setData((int)VipToolTip::ItemsPos);
	position->setCheckable(true);
	QAction* properties = menu->addAction("Tool tip: show item properties");
	properties->setData((int)VipToolTip::ItemsProperties);
	properties->setCheckable(true);

	hidden->setChecked(flags & VipToolTip::Hidden);
	axes->setChecked(flags & VipToolTip::Axes);
	title->setChecked(flags & VipToolTip::ItemsTitles);
	legend->setChecked(flags & VipToolTip::ItemsLegends);
	position->setChecked(flags & VipToolTip::ItemsPos);
	properties->setChecked(flags & VipToolTip::ItemsProperties);

	connect(hidden, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(axes, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(title, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(legend, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(position, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	connect(properties, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));

	if (qobject_cast<VipPlotPlayer*>(this)) {
		QAction* xAxis = menu->addAction("Tool tip: display temporal position");
		xAxis->setToolTip("If checked, the plotting will dislpay a vertical line representing the mouse x position.<br>"
				  "This will also display this vertical line on all other plotting area within the current workspace.");
		xAxis->setData((int)VipToolTip::SearchXAxis);
		xAxis->setCheckable(true);

		QAction* yAxis = menu->addAction("Tool tip: display vertical position");
		yAxis->setToolTip("If checked, the plotting will dislpay a horizontal line representing the mouse y position.<br>"
				  "This will also display this horizontal line on all other plotting area within the current workspace.");
		yAxis->setData((int)VipToolTip::SearchYAxis);
		yAxis->setCheckable(true);

		xAxis->setChecked(flags & VipToolTip::SearchXAxis);
		yAxis->setChecked(flags & VipToolTip::SearchYAxis);

		connect(xAxis, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
		connect(yAxis, SIGNAL(triggered(bool)), this, SLOT(toolTipChanged()));
	}
	return menu;
}

VipPlayer2D* VipPlayer2D::findPlayer2D(const VipSceneModel& scene)
{
	if (VipTypeId* ids = VipUniqueId::typeId(&VipPlayer2D::staticMetaObject)) {
		QList<QObject*> objs = ids->objects();
		for (int i = 0; i < objs.size(); ++i) {
			if (VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(objs[i])) {
				if (pl->findPlotSceneModel(scene))
					return pl;
			}
		}
	}
	return nullptr;
}

void VipPlayer2D::toolTipChanged()
{
	QAction* act = qobject_cast<QAction*>(this->sender());
	if (!act)
		return;

	VipToolTip::DisplayFlag flag = (VipToolTip::DisplayFlag)act->data().toInt();
	bool enabled = act->isChecked();

	VipToolTip::DisplayFlags flags = VipPlayerToolTip::toolTipFlags(this->metaObject());

	if (enabled)
		flags |= flag;
	else
		flags &= (~flag);

	if (enabled && flag == VipToolTip::SearchXAxis) {
		// disable search y
		flags &= (~VipToolTip::SearchYAxis);
	}
	else if (enabled && flag == VipToolTip::SearchYAxis) {
		// disable search x
		flags &= (~VipToolTip::SearchXAxis);
	}

	this->setToolTipFlags(flags);
}

VipToolTip::DisplayFlags VipPlayer2D::toolTipFlags() const
{
	return VipPlayerToolTip::toolTipFlags(this->metaObject());
}

void VipPlayer2D::setToolTipFlags(VipToolTip::DisplayFlags flags)
{
	VipPlayerToolTip::setToolTipFlags(flags, this->metaObject());
}

void VipPlayer2D::addSceneModels(const VipSceneModelList& lst, bool remove_old_shapes)
{
	if (lst.size()) {
		if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(this)) {

			QSet<VipPlotSceneModel*> cleared;

			// set the scene models for the right axes
			for (int i = 0; i < lst.size(); ++i) {
				VipSceneModel sm = lst[i];
				QString yunit = sm.attribute("YUnit").toString();
				VipAbstractScale* sc = pl->findYScale(yunit);
				if (!sc && pl->leftScales().size())
					sc = pl->leftScales().first();
				if (sc)
					if (VipPlotSceneModel* psm = pl->findPlotSceneModel(QList<VipAbstractScale*>() << pl->xScale() << sc)) {
						if (remove_old_shapes && cleared.find(psm) == cleared.end()) {
							psm->sceneModel().clear();
							cleared.insert(psm);
						}
						psm->sceneModel().add(sm);
					}
			}
		}
		else {
			if (remove_old_shapes)
				plotSceneModel()->sceneModel().clear();
			for (int i = 0; i < lst.size(); ++i)
				plotSceneModel()->sceneModel().add(lst[i]);
		}
	}
}

// void VipPlayer2D::clipboardOperationRequested(ClipboardOperation op)
//  {
//
//  VipSceneModel model = scene->sceneModel();
//
//  if (op == Copy || op == Cut)
//  {
//	//compute the list of selected VipShape
//	VipShapeList shapes;
//	QList<VipPlotShape*> plot_shapes = scene->shapes();
//	for (int i = 0; i < plot_shapes.size(); ++i)
//	{
//		if (plot_shapes[i]->isSelected())
//			shapes.append(plot_shapes[i]->rawData());
//	}
//
//	//copy to clipboard
//	vipCopyObjectsToClipboard(shapes);
//
//	//remove if cut
//	if (op == Cut)
//	{
//		for (int i = 0; i < shapes.size(); ++i)
//		{
//			model.remove(shapes[i]);
//		}
//	}
//  }
//  else
//  {
//	//paste
//	VipShapeList shapes = vipCreateFromClipboard<VipShape>();
//	model.add(shapes);
//  }
//
//
//  }

void VipPlayer2D::plotItemAdded(VipPlotItem*) {}
void VipPlayer2D::plotItemRemoved(VipPlotItem*) {}
void VipPlayer2D::plotItemSelectionChanged(VipPlotItem*) {}
void VipPlayer2D::plotItemAxisUnitChanged(VipPlotItem*) {}
void VipPlayer2D::toolTipFlagsChanged(VipToolTip::DisplayFlags flags)
{
	if (VipAbstractPlotWidget2D* w = this->plotWidget2D())
		if (VipAbstractPlotArea* a = w->area())
			if (VipToolTip* tip = a->plotToolTip())
				tip->setDisplayFlags(flags);
}

static VipPlayer2D* _drop_target = nullptr;

VipPlayer2D* VipPlayer2D::dropTarget()
{
	return _drop_target;
}

void VipPlayer2D::itemsDropped(VipPlotItem* target, QMimeData* mimeData)
{
	bool managed = false;
	if (VipPlotMimeData* mime = qobject_cast<VipPlotMimeData*>(mimeData)) {
		_drop_target = this;
		PlotItemList items = mime->plotData(target, this);
		if (items.size()) {
			managed = true;
			for (int i = 0; i < items.size(); ++i) {
				// find the drop dource
				VipPlayer2D* source = qobject_cast<VipPlayer2D*>(VipAbstractPlayer::findAbstractPlayer(items[i]));
				if (source != this) {
					// move from one player to another, set the _vip_created flag to trigger a potential axis creation
					items[i]->setProperty("_vip_created", true);
				}
			}

			VipAbstractPlayer* pl_src = VipAbstractPlayer::findAbstractPlayer(items.first());
			VipProcessingPool* pool_src = pl_src ? pl_src->processingPool() : nullptr;
			VipProcessingPool* pool_dst = this->processingPool();

			if (pool_dst == pool_src) {
				// same processing pool, just move the items
				for (int i = 0; i < items.size(); ++i)
					if (!qobject_cast<VipPlotSpectrogram*>(items[i])) // do NOT drop VipPlotSpectrogram into an existing player
						items[i]->setAxes(target->axes(), target->coordinateSystemType());
			}
			else {
				// we need to copy the items and processing pipeline (if possible)
				VipMimeDataDuplicatePlotItem duplicate(items);
				duplicate.plotData(target, this);
			}
		}
	}

	if (!managed) {
		// Manage unhandled drop

		const auto lst = VipFDDropOnPlotItem().match(this, target, mimeData);
		for (int i = 0; i < lst.size(); ++i) {
			bool ret = lst[i](this, target, mimeData);
			if (ret)
				break;
		}
	}
	_drop_target = nullptr;
}

static bool is_selectable(VipPlotItem* item)
{
	// avoid selecting through key shortcut spectrogram, plot scene model, canvas and grid
	if (!(item->flags() & QGraphicsItem::ItemIsSelectable))
		return false;
	if (!item->isVisible())
		return false;
	if (qobject_cast<VipPlotSpectrogram*>(item))
		return false;
	if (qobject_cast<VipPlotCanvas*>(item))
		return false;
	if (qobject_cast<VipPlotGrid*>(item))
		return false;
	if (qobject_cast<VipPlotSceneModel*>(item))
		return false;
	if (qobject_cast<VipResizeItem*>(item))
		return false;
	return true;
}

static void unselect_all(const QList<VipPlotItem*>& items)
{
	for (int i = 0; i < items.size(); ++i)
		items[i]->setSelected(false);
}

void VipPlayer2D::nextSelection(bool ctrl)
{
	// change selection, like the TAB key for widgets
	QList<VipPlotItem*> all_items = plotWidget2D()->area()->plotItems();
	QList<VipPlotItem*> items = all_items;
	VipPlotItem* last_selected = nullptr;
	VipPlotItem* next = nullptr;
	for (int i = 0; i < items.size(); ++i) {
		if (items[i]->isSelected()) {
			last_selected = items[i];
			if (next == last_selected)
				next = nullptr;
			next = i == items.size() - 1 ? nullptr : items[i + 1];
			items.removeAt(i);
			--i;
		}
	}
	if (!last_selected) {
		// select the first selectable item
		for (QList<VipPlotItem*>::iterator it = items.begin(); it != items.end(); ++it) {
			if (is_selectable((*it))) {
				(*it)->setSelected(true);
				return;
			}
		}
	}
	else {
		// unselect all
		if (!ctrl)
			unselect_all(all_items);
		// find the next selectable item
		int index = next ? items.indexOf(next) : 0;
		for (int i = index; i < items.size(); ++i) {
			if (is_selectable(items[i])) {
				items[i]->setSelected(true);
				return;
			}
		}
		// start from beginning
		for (int i = 0; i < index; ++i) {
			if (is_selectable(items[i])) {
				items[i]->setSelected(true);
				return;
			}
		}
	}
}

void VipPlayer2D::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Z && (evt->modifiers() & Qt::CTRL)) {
		VipSceneModelState::instance()->undo();
		return;
	}
	else if (evt->key() == Qt::Key_Y && (evt->modifiers() & Qt::CTRL)) {
		VipSceneModelState::instance()->redo();
		return;
	}
	else if (evt->key() == Qt::Key_N) {
		nextSelection(evt->modifiers() & Qt::CTRL);
		return;
	}
	

	// Apply dispatcher
	auto fun = VipFDPlayerKeyPress().match(this);
	for (auto& f : fun) {
		if (f(this, (int)evt->key(), (int)evt->modifiers())) {
			evt->accept();
			return;
		}
	}

	evt->ignore();
}

QList<VipDisplayObject*> VipPlayer2D::displayObjects() const
{
	if (VipProcessingPool* pool = this->processingPool()) {
		// grab all VipDisplayPlotItem from the player processing pool,
		// and 'touch' them: call item() member that will ensure they
		// have a valid (or not) VipPlotItem object. This might be required
		// when loading a session, the VipDisplayPlotItem might not be
		// aware of their VipPlotItem.
		QList<VipDisplayPlotItem*> ditems = pool->findChildren<VipDisplayPlotItem*>();
		for (int i = 0; i < ditems.size(); ++i)
			ditems[i]->item();
	}

	QList<VipDisplayObject*> res;
	QList<VipPlotItem*> items = this->plotWidget2D()->area()->plotItems();

	for (int i = 0; i < items.size(); ++i) {
		VipDisplayObject* obj = items[i]->property("VipDisplayObject").value<VipDisplayObject*>();
		if (obj)
			res.append(obj);
	}

	return res;
}

VipPlotSceneModel* VipPlayer2D::plotSceneModel() const
{
	if (inDestructor())
		return nullptr;
	return d_data->plotScene;
}

QList<VipPlotSceneModel*> VipPlayer2D::plotSceneModels() const
{
	if (VipPlotSceneModel* sm = plotSceneModel())
		return QList<VipPlotSceneModel*>() << sm;
	return QList<VipPlotSceneModel*>();
}

VipPlotSceneModel* VipPlayer2D::findPlotSceneModel(const VipSceneModel& scene) const
{
	if (inDestructor())
		return nullptr;

	QList<VipPlotSceneModel*> lst = this->plotWidget2D()->area()->findItems<VipPlotSceneModel*>();
	for (int i = 0; i < lst.size(); ++i)
		if (lst[i]->sceneModel() == scene)
			return lst[i];
	return nullptr;
}

VipPlotSceneModel* VipPlayer2D::findPlotSceneModel(const QList<VipAbstractScale*>& scales) const
{
	if (inDestructor())
		return nullptr;
	QList<VipPlotSceneModel*> lst = this->plotWidget2D()->area()->findItems<VipPlotSceneModel*>();
	for (int i = 0; i < lst.size(); ++i)
		if (lst[i]->axes() == scales)
			return lst[i];
	return nullptr;
}

QList<VipPlotShape*> VipPlayer2D::findSelectedPlotShapes(int selected, int visible) const
{
	if (this->plotWidget2D())
		if (this->plotWidget2D()->area())
			return this->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), selected, visible);
	return QList<VipPlotShape*>();
}

VipShapeList VipPlayer2D::findSelectedShapes(int selected, int visible) const
{
	QList<VipPlotShape*> pshapes = findSelectedPlotShapes(selected, visible);
	VipShapeList res;
	for (int i = 0; i < pshapes.size(); ++i)
		res.append(pshapes[i]->rawData());
	return res;
}

VipDisplaySceneModel* VipPlayer2D::findDisplaySceneModel(const VipSceneModel& model) const
{
	if (model.isNull())
		return nullptr;

	VipDisplaySceneModel* disp_sm = nullptr;
	QList<VipDisplayObject*> displays = this->displayObjects();
	for (int i = 0; i < displays.size(); ++i)
		if (VipDisplaySceneModel* disp = qobject_cast<VipDisplaySceneModel*>(displays[i]))
			if (disp->item()->sceneModel() == model) {
				disp_sm = disp;
				break;
			}
	return disp_sm;
}

VipDisplaySceneModel* VipPlayer2D::findDisplaySceneModel(const VipShape& sh) const
{
	return findDisplaySceneModel(sh.parent());
}

VipPlayerToolBar* VipPlayer2D::toolBar() const
{
	return d_data->toolBar;
}

QWidget* VipPlayer2D::toolBarWidget() const
{
	return d_data->toolBarWidget;
}

void VipPlayer2D::insertToolBar(int index, QToolBar* bar)
{
	d_data->toolBarLayout->insertWidget(index, bar);
	d_data->bars.insert(index, bar);
}
void VipPlayer2D::addToolBar(QToolBar* bar)
{
	d_data->toolBarLayout->addWidget(bar);
	d_data->bars.append(bar);
}
int VipPlayer2D::toolBarCount() const
{
	return d_data->bars.size();
}
QToolBar* VipPlayer2D::toolBarAt(int index)
{
	if (index < 0 || index >= d_data->bars.size())
		return nullptr;
	return d_data->bars[index];
}
int VipPlayer2D::indexOfToolBar(QToolBar* bar) const
{
	return d_data->bars.indexOf(bar);
}
QToolBar* VipPlayer2D::takeToolBar(int index) const
{
	if (index > 0 && index < d_data->bars.size()) {
		QToolBar* b = d_data->bars.takeAt(index);
		b->setParent(nullptr);
		return b;
	}
	return nullptr;
}
QList<QToolBar*> VipPlayer2D::toolBars() const
{
	QList<QToolBar*> res;
	for (int i = 0; i < d_data->bars.size(); ++i)
		res.append(d_data->bars[i]);
	return res;
}

VipToolBar* VipPlayer2D::afterTitleToolBar() const
{
	return d_data->afterTitleToolBar;
}

QLabel* VipPlayer2D::statusText() const
{
	return d_data->statusText;
}

void VipPlayer2D::draw(QPainter* p, const QRectF& dst, Qt::AspectRatioMode aspectRatioMode) const
{
	if (plotWidget2D()) {
		VipRenderState state;
		VipRenderObject::startRender(const_cast<VipPlayer2D*>(this), state);
		QCoreApplication::processEvents();
		plotWidget2D()->scene()->render(p, dst, plotWidget2D()->area()->boundingRect() /* plotAreaRect()*/, aspectRatioMode);
		VipRenderObject::endRender(const_cast<VipPlayer2D*>(this), state);
	}
}

QPixmap VipPlayer2D::currentPixmap(QPainter::RenderHints hints) const
{
	if (!plotWidget2D())
		return QPixmap();

	// get the outer rect in pixel coordinates
	QRect rect = plotWidget2D()->mapFromScene(plotWidget2D()->area()->boundingRect() /*plotAreaRect()*/).boundingRect();
	// rect.setWidth(rect.width()+100);
	QPixmap pixmap(rect.width(), rect.height());
	pixmap.fill(QColor(255, 255, 255, 0));
	QPainter painter(&pixmap);
	painter.setRenderHints(hints);
	draw(&painter, rect.translated(-rect.topLeft()), Qt::KeepAspectRatio);
	return pixmap;
}

void VipPlayer2D::startRender(VipRenderState& state)
{

	state.state(this)["toolBar_visible"] = !toolBar()->isHidden();
	toolBar()->hide();
	VipAbstractPlayer::startRender(state);
}

void VipPlayer2D::endRender(VipRenderState& state)
{
	toolBar()->setVisible(state.state(this)["toolBar_visible"].toBool());
	VipAbstractPlayer::endRender(state);
}

VipPlotSceneModel* VipPlayer2D::createPlotSceneModel(const QList<VipAbstractScale*>& scales, VipCoordinateSystem::Type type)
{
	VipPlotSceneModel* scene = new VipPlotSceneModel();
	scene->setMode(VipPlotSceneModel::Resizable);
	scene->setZValue(1000);
	scene->setIgnoreStyleSheet(true);
	// affect an identifier to the scene model
	// TODO:CHANGESCENEMODEL
	VipUniqueId::id(scene->sceneModel().shapeSignals());

	scene->setAxes(scales, type);
	scene->setBrush("All", VipGuiDisplayParamaters::instance()->shapeBackgroundBrush());
	scene->setPen("All", VipGuiDisplayParamaters::instance()->shapeBorderPen());
	scene->setDrawComponents("All", VipGuiDisplayParamaters::instance()->shapeDrawComponents());
	scene->sceneModel().setAttribute("YUnit", scales[1]->title().text());

	Q_EMIT sceneModelAdded(scene);
	connect(scene, SIGNAL(groupsChanged()), this, SLOT(emitSceneModelGroupsChanged()), Qt::AutoConnection);
	connect(scene, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged()), Qt::AutoConnection);

	VipSceneModelState::instance()->connectSceneModel(scene);

	return scene;
}

void VipPlayer2D::setPlotWidget2D(VipAbstractPlotWidget2D* plot)
{
	//
	// make sure to detect mouse release, item selection and new items in order to invoke the right VipFunctionDispatcher objects
	//

	if (plotWidget2D()) {
		disconnect(plotWidget2D()->area(), SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)));
		disconnect(plotWidget2D()->area(), SIGNAL(mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(recordLastMousePress()));
		disconnect(plotWidget2D()->area(), SIGNAL(keyPress(VipPlotItem*, qint64, int, int)), this, SLOT(keyPress(VipPlotItem*, qint64, int, int)));
		disconnect(plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem*)), this, SLOT(itemSelectionChanged(VipPlotItem*)));
		disconnect(plotWidget2D()->area(), SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(itemAdded(VipPlotItem*)));
		disconnect(plotWidget2D()->area(), SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(itemRemoved(VipPlotItem*)));
		disconnect(plotWidget2D()->area(), SIGNAL(childAxisUnitChanged(VipPlotItem*)), this, SLOT(itemAxisUnitChanged(VipPlotItem*)));
		disconnect(plotWidget2D()->area(), SIGNAL(dropped(VipPlotItem*, QMimeData*)), this, SLOT(itemsDropped(VipPlotItem*, QMimeData*)));

		disconnect(plotWidget2D()->area(), SIGNAL(mouseScaleAboutToChange()), plotWidget2D()->area(), SLOT(disableAutoScale()));

		QList<VipPlotSceneModel*> models = plotWidget2D()->area()->findItems<VipPlotSceneModel*>();
		for (int i = 0; i < models.size(); ++i) {
			Q_EMIT sceneModelRemoved(models[i]);
			disconnect(models[i], SIGNAL(groupsChanged()), this, SLOT(emitSceneModelGroupsChanged()));
			disconnect(models[i], SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged()));
		}
	}

	VipPlotWidget::setPlotWidget2D(plot);

	if (plotWidget2D()) {

		plotWidget2D()->area()->grid()->setFlag(QGraphicsItem::ItemIsSelectable, false);
		plotWidget2D()->area()->grid()->setItemAttribute(VipPlotItem::IgnoreMouseEvents, true);

		// add a VipPlotSceneModel object if there is none
		QList<VipPlotSceneModel*> models = plotWidget2D()->area()->findItems<VipPlotSceneModel*>();
		if (!models.size()) {
			QList<VipAbstractScale*> scales;
			VipCoordinateSystem::Type type = plotWidget2D()->area()->standardScales(scales);
			d_data->plotScene = createPlotSceneModel(scales, type);
		}
		else
			d_data->plotScene = models.first();

		connect(plotWidget2D()->area(),
			SIGNAL(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)),
			this,
			SLOT(mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton)),
			Qt::QueuedConnection);
		connect(plotWidget2D()->area(), SIGNAL(mouseButtonPress(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(recordLastMousePress()), Qt::DirectConnection);
		connect(plotWidget2D()->area(), SIGNAL(keyPress(VipPlotItem*, qint64, int, int)), this, SLOT(keyPress(VipPlotItem*, qint64, int, int)));
		connect(plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem*)), this, SLOT(itemSelectionChanged(VipPlotItem*)));
		connect(plotWidget2D()->area(), SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(itemAdded(VipPlotItem*)));
		connect(plotWidget2D()->area(), SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(itemRemoved(VipPlotItem*)));
		connect(plotWidget2D()->area(), SIGNAL(childAxisUnitChanged(VipPlotItem*)), this, SLOT(itemAxisUnitChanged(VipPlotItem*)));
		connect(plotWidget2D()->area(), SIGNAL(dropped(VipPlotItem*, QMimeData*)), this, SLOT(itemsDropped(VipPlotItem*, QMimeData*)));

		connect(plotWidget2D()->area(), SIGNAL(mouseScaleAboutToChange()), plotWidget2D()->area(), SLOT(disableAutoScale()));

		// invoke the VipFunctionDispatcher object used to modify the player itself
		QMetaObject::invokeMethod(this, "playerCreated", Qt::QueuedConnection);
	}

	toolTipFlagsChanged(VipPlayerToolTip::toolTipFlags(this->metaObject()));
	// this->setToolTipFlags(this->toolTipFlags());
}

void VipPlayer2D::mouseButtonRelease(VipPlotItem* item, VipPlotItem::MouseButton button)
{
	vipProcessEvents(nullptr, 100);

	if (this->plotItemClicked(item, button))
		return;

	if (button == VipPlotItem::RightButton) {
		// in some case , convert the VipPlotItem into a VipPlotShape (VipResizeItem with only one managed VipPlotShape)
		if (VipResizeItem* resize = qobject_cast<VipResizeItem*>(item)) {
			PlotItemList items = resize->managedItems();
			if (items.size() == 1)
				if (VipPlotShape* shape = qobject_cast<VipPlotShape*>(items.first()))
					item = shape;
		}
		// create the contextual menu from the dispatcher
		const auto funs = VipFDItemRightClick().match(item, this);
		QList<QAction*> actions;

		for (int i = 0; i < funs.size(); ++i) {
			actions.append(funs[i](item, this).value<QList<QAction*>>());
		}

		VipDragMenu menu;
		for (int i = 0; i < actions.size(); ++i) {
			menu.addAction(actions[i]);
			actions[i]->setParent(&menu);
		}

		menu.setToolTipsVisible(true);
		menu.exec(QCursor::pos());
	}
}

void VipPlayer2D::keyPress(VipPlotItem*, qint64 id, int key, int modifiers)
{
	// since several items might send the same key event, use its id (which is the event timestamp) to process it only once

	if (id == d_data->keyEventId)
		return;

	if (key == Qt::Key_C && (modifiers & Qt::CTRL))
		this->copySelectedItems();
	else if (key == Qt::Key_V && (modifiers & Qt::CTRL))
		this->pasteItems();

	d_data->keyEventId = id;
}

void VipPlayer2D::resetSelection()
{

	if (plotWidget2D()) {
		// get all selected items
		QList<VipPlotItem*> items = plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);

		// unselected and reselect them
		for (int i = 0; i < items.size(); ++i) {
			items[i]->setSelected(false);
			items[i]->setSelected(true);
		}
	}
}

void VipPlayer2D::resetSelection(VipDisplayPlayerArea* area)
{
	QList<VipPlayer2D*> pls = area->findChildren<VipPlayer2D*>();
	for (int i = 0; i < pls.size(); ++i)
		pls[i]->resetSelection();
}

// detect change of selection
void VipPlayer2D::itemSelectionChanged(VipPlotItem* item)
{
	// just call all valid functions in the dispatcher
	VipFDItemSelected().callAllMatch(item, this);

	QMetaObject::invokeMethod(this, "plotItemSelectionChanged", Qt::QueuedConnection, Q_ARG(VipPlotItem*, item));
}

// detect new item
void VipPlayer2D::itemAdded(VipPlotItem* item)
{
	// disable the VipDisplayObject if this widget is disabled
	if (VipDisplayObject* display = item->property("VipDisplayObject").value<VipDisplayObject*>()) {
		if (!isEnabled())
			display->setEnabled(false);

		// this display object might just be created from a drag and drop.
		// We need to add it and its sources into the current processing pool if does not belong to one.
		if (VipProcessingPool* pool = processingPool()) {
			QList<VipProcessingObject*> sources = display->allSources();
			for (int i = 0; i < sources.size(); ++i) {
				if (!sources[i]->parent())
					sources[i]->setParent(pool);
			}
			if (!display->parent())
				display->setParent(pool);
		}
	}

	// just call all valid functions in the dispatcher
	VipFDItemAddedOnPlayer().callAllMatch(item, this);

	// remove all pending deleteLater() for this item.
	// This is because on dropping, removeLeftScale is called before this function,
	// and removeLeftScale will delete the items related to a stacked canvas that is removed because empty.
	QCoreApplication::removePostedEvents(item, QEvent::DeferredDelete);

	QMetaObject::invokeMethod(this, "plotItemAdded", Qt::QueuedConnection, Q_ARG(VipPlotItem*, item));

	if (VipPlotSceneModel* sm = qobject_cast<VipPlotSceneModel*>(item)) {
		Q_EMIT sceneModelAdded(sm);
		// make sure we do not connect twice
		disconnect(sm, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged()));
		disconnect(sm, SIGNAL(groupsChanged()), this, SLOT(emitSceneModelGroupsChanged()));

		connect(sm, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged()), Qt::DirectConnection);
		connect(sm, SIGNAL(groupsChanged()), this, SLOT(emitSceneModelGroupsChanged()), Qt::DirectConnection);
	}
}

void VipPlayer2D::itemRemoved(VipPlotItem* item)
{
	// just call all valid functions in the dispatcher
	VipFDItemRemovedFromPlayer().callAllMatch(item, this);

	QMetaObject::invokeMethod(this, "plotItemRemoved", Qt::QueuedConnection, Q_ARG(VipPlotItem*, item));

	if (VipPlotSceneModel* sm = qobject_cast<VipPlotSceneModel*>(item)) {
		Q_EMIT sceneModelRemoved(sm);
		disconnect(sm, SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitSceneModelChanged()));
		disconnect(sm, SIGNAL(groupsChanged()), this, SLOT(emitSceneModelGroupsChanged()));
	}
}
void VipPlayer2D::itemAxisUnitChanged(VipPlotItem* item)
{
	// just call all valid functions in the dispatcher
	VipFDItemAxisUnitChanged().callAllMatch(item, this);

	QMetaObject::invokeMethod(this, "plotItemAxisUnitChanged", Qt::QueuedConnection, Q_ARG(VipPlotItem*, item));
}
//
void VipPlayer2D::playerCreated()
{
	// call the dispatcher that might modify the player
	vipFDPlayerCreated().callAllMatch(this);

	this->onPlayerCreated();
}

void VipPlayer2D::selectionZoomArea(bool enable)
{
	if (isSelectionZoomAreaEnabled() != enable) {
		if (qobject_cast<VipPlotPlayer*>(this)) {
			// For VipPlotPlayer, the default behavior is to zoom on selected area,
			// which is the most used feature.
			if (enable) {
				plotWidget2D()->area()->setMouseZoomSelection(Qt::LeftButton);
				plotWidget2D()->area()->setMouseItemSelection(Qt::NoButton);
			}
			else {
				plotWidget2D()->area()->setMouseZoomSelection(Qt::NoButton);
				plotWidget2D()->area()->setMouseItemSelection(Qt::NoButton);
			}
		}
		else {
			// For other players, the behavior is to select items
			if (enable) {
				plotWidget2D()->area()->setMouseItemSelection(Qt::LeftButton);
			}
			else {
				plotWidget2D()->area()->setMouseItemSelection(Qt::NoButton);
			}
		}
		d_data->toolBar->selectionModeAction->blockSignals(true);
		d_data->toolBar->selectionModeAction->setChecked(enable);
		d_data->toolBar->selectionModeAction->blockSignals(false);

		Q_EMIT mouseSelectionChanged(enable);
	}
}

bool VipPlayer2D::isSelectionZoomAreaEnabled() const
{
	return plotWidget2D()->area()->mouseZoomSelection() == Qt::LeftButton || plotWidget2D()->area()->mouseItemSelection() == Qt::LeftButton;
}

QPoint VipPlayer2D::lastMousePressScreenPos() const
{
	return d_data->lastMousePress;
}

void VipPlayer2D::copySelectedItems()
{
	if (this->plotWidget2D()) {
		PlotItemList items = this->plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);
		VipPlotItemClipboard::copy(items);
	}
}

void VipPlayer2D::pasteItems()
{
	if (plotWidget2D())
		VipPlotItemClipboard::paste(plotWidget2D()->area(), this);
}

PlotItemList VipPlayer2D::savableItems() const
{
	QList<VipPlotItem*> items = this->plotWidget2D()->area()->plotItems();
	for (int i = 0; i < items.size(); ++i) {
		VipPlotItemData* mime_data = qobject_cast<VipPlotItemData*>(items[i]);
		// we can only save VipPlotItemData with non null mime_data and having a valid title
		if (!mime_data || mime_data->data().userType() == 0 || (mime_data->title().isEmpty() && mime_data->data().userType() != qMetaTypeId<VipNDArray>())) {
			items.removeAt(i);
			--i;
		}
	}
	return items;
}

void VipPlayer2D::saveMenuPopup()
{
	QList<VipPlotItem*> items = savableItems();
	d_data->toolBar->saveItemMenu->clear();
	d_data->toolBar->saveItemMenu->addAction("Save player as image...")->setProperty("save_image", true);
	d_data->toolBar->saveItemMenu->addAction("Save player as session...")->setProperty("save_session", true);

	d_data->toolBar->saveItemMenu->addSeparator();
	d_data->toolBar->saveItemMenu->addAction("Copy image to clipboard")->setProperty("image_clipboard", true);

	d_data->toolBar->saveItemMenu->addSeparator();
	for (int i = 0; i < items.size(); ++i) {
		QString title = items[i]->title().text();
		if (title.isEmpty())
			title = vipSplitClassname(items[i]->metaObject()->className());
		QAction* act = new QAction(d_data->toolBar->saveItemMenu);
		act->setText(title);
		act->setProperty("PlotItem", QVariant::fromValue(items[i]));
		QString type = vipSplitClassname(static_cast<VipPlotItemData*>(items[i])->data().typeName());
		QString tool_tip = "<b>Name</b>: " + title + "<br><b>Type</b>: " + type;
		act->setToolTip(tool_tip);
		d_data->toolBar->saveItemMenu->addAction(act);
		d_data->toolBar->saveItemMenu->setToolTipsVisible(true);
	}

	if (items.size() > 1) {
		d_data->toolBar->saveItemMenu->addSeparator();
		d_data->toolBar->saveItemMenu->addAction(new QAction("Save all signals...", d_data->toolBar->saveItemMenu));
	}
}

void VipPlayer2D::saveMenuClicked(QAction* act)
{
	if (act->property("save_image").toBool()) {
		int supported_formats = VipRenderObject::supportedVectorFormats();
		QString filters = VipImageWriter().fileFilters() + ";;PDF file (*.pdf)";
		if (supported_formats & VipRenderObject::PS)
			filters += ";;PS file(*.ps)";
		if (supported_formats & VipRenderObject::EPS)
			filters += ";;EPS file(*.eps)";

		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save image as", filters //"Image file (*.png *.bmp *.jpg *.jpeg *.ico *.gif *.gif *.xpm)"
		);
		if (!filename.isEmpty()) {
			QFileInfo info(filename);

			if (info.suffix().compare("pdf", Qt::CaseInsensitive) == 0) {
				VipRenderObject::saveAsPdf(this, filename, nullptr);
			}
			else if (info.suffix().compare("ps", Qt::CaseInsensitive) == 0 || info.suffix().compare("eps", Qt::CaseInsensitive) == 0) {
				VipRenderObject::saveAsPs(this, filename);
			}
			else {
				VipRenderState state;
				VipRenderObject::startRender(this, state);

				vipProcessEvents();

				bool use_transparency = (QFileInfo(filename).suffix().compare("png", Qt::CaseInsensitive) == 0);

				QPixmap pixmap(this->size());
				if (use_transparency)
					pixmap.fill(QColor(255, 255, 255, 1)); // Qt::transparent);
				else
					pixmap.fill(QColor(255, 255, 255));

				QPainter p(&pixmap);
				p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

				vipFDAboutToRender().callAllMatch(this);

				VipRenderObject::renderObject(this, &p, QPoint(), true, false);

				VipRenderObject::endRender(this, state);

				if (!pixmap.save(filename)) {
					VIP_LOG_ERROR("Failed to save image " + filename);
				}
				else {
					VIP_LOG_INFO("Saved image in " + filename);
				}
			}
		}
	}
	else if (act->property("save_session").toBool()) {
		if (VipDragWidget* w = qobject_cast<VipDragWidget*>(VipDragWidget::fromChild(this))) {
			QString filename = VipFileDialog::getSaveFileName(nullptr, "Save player as", "Session file (*.session)");
			if (!filename.isEmpty()) {
				VipXOfArchive arch(filename);
				vipSaveBaseDragWidget(arch, w);
				arch.close();
			}
		}
	}
	else if (act->property("image_clipboard").toBool()) {
		VipRenderState state;
		VipRenderObject::startRender(this, state);

		vipProcessEvents();

		// Clipboard does not handle transparent image well, so fille background with white
		QPixmap pixmap(this->size());
		pixmap.fill(QColor(255, 255, 255));

		{
			QPainter p(&pixmap);
			p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

			vipFDAboutToRender().callAllMatch(this);

			VipRenderObject::renderObject(this, &p, QPoint(), true, false);
			VipRenderObject::endRender(this, state);
		}
		pixmap = vipRemoveColoredBorder(pixmap, QColor(255, 255, 255), 10);
		QGuiApplication::clipboard()->setPixmap(pixmap);
	}
	else {
		VipPlotItemData* item = act->property("PlotItem").value<VipPlotItemData*>();
		saveItemContent(item, QString());
	}
}

void VipPlayer2D::emitSceneModelGroupsChanged()
{
	Q_EMIT sceneModelGroupsChanged(qobject_cast<VipPlotSceneModel*>(sender()));
}

void VipPlayer2D::emitSceneModelChanged()
{
	Q_EMIT sceneModelChanged(qobject_cast<VipPlotSceneModel*>(sender()));
}

void VipPlayer2D::recordLastMousePress()
{
	d_data->lastMousePress = QCursor::pos();
}

void VipPlayer2D::setPlotSceneModel(VipPlotSceneModel* sm)
{
	d_data->plotScene = sm;
}

// static void saveItemContent(VipPlotItemData * item, VipPlayer2D *)
// {
// QVariant data = item->data();
// if (data.userType() == 0)
// {
// VIP_LOG_ERROR("Save item data: empty data");
// return;
// }
//
// QStringList filters = VipIODevice::possibleWriteFilters(QString(), QVariantList() << data);
// QString filename = VipFileDialog::getSaveFileName(nullptr, "Save text file", filters.join(";;"));
// if (!filename.isEmpty())
// {
// QList<int> devices = VipIODevice::possibleWriteDevices(filename, QVariantList() << data);
// VipIODevice * device = VipCreateDevice::create(devices);
// if (device)
// {
// //if the device'w input is a VipMultiInput, add an input
// if (VipMultiInput * in = device->topLevelInputAt(0)->toMultiInput())
// in->add();
// device->setPath(filename);
// device->open(VipIODevice::WriteOnly);
// device->inputAt(0)->setData(data);
// device->update();
// delete device;
// VIP_LOG_INFO("Data saved successfully");
// }
// }
// }

bool VipPlayer2D::saveItemContent(VipPlotItemData* item, const QString& path)
{
	if (!item) {
		// save all items
		QList<VipPlotItem*> items = savableItems();
		QVariantList lst_data;
		QList<VipAnyData> any_data;

		for (int i = 0; i < items.size(); ++i) {
			if (VipDisplayObject* display = items[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
				VipAnyData any = display->inputAt(0)->data();
				any_data.append(any);
				lst_data.append(any.data());
			}
		}

		if (!lst_data.size()) {
			VIP_LOG_WARNING("No valid items to save");
			return false;
		}

		QString filename = path;
		if (filename.isEmpty()) {
			QStringList filters = VipIODevice::possibleWriteFilters(QString(), lst_data);
			filename = VipFileDialog::getSaveFileName2(nullptr, item ? item->title().text() : QString(), "Save all data", filters.join(";;"));
		}

		if (!filename.isEmpty()) {
			VipProgress progress;
			progress.setText("<b>Save</b> " + QFileInfo(filename).fileName() + "...");
			progress.setModal(true);
			vipProcessEvents();
			QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(filename, lst_data);
			VipIODevice* device = VipCreateDevice::create(devices);
			if (device) {
				device->setProperty("player", QVariant::fromValue(this));

				// if the device'w input is a VipMultiInput, add an input
				if (VipMultiInput* in = device->topLevelInputAt(0)->toMultiInput())
					in->resize(lst_data.size());
				device->setPath(filename);
				if (!device->open(VipIODevice::WriteOnly)) {
					VIP_LOG_ERROR("Failed to open output file ", filename);
					delete device;
					return false;
				}
				for (int i = 0; i < lst_data.size(); ++i)
					device->inputAt(i)->setData(any_data[i]);
				device->update();
				bool res = !device->hasError();
				delete device;
				if (res)
					VIP_LOG_INFO("All items saved successfully");
				else
					VIP_LOG_ERROR("Failed to save all items");
				return res;
			}
			else {
				VIP_LOG_WARNING("No output device found for path " + filename);
			}
		}
		return false;
	}

	if (item->data().userType() == 0) {
		VIP_LOG_ERROR("Save item's content: empty item");
		return false;
	}

	VipAnyData any(item->data(), 0);
	any.setSource(1); // do NOT set a nullptr source or the data might not be loaded back
	any.setName(item->title().text());
	if (VipDisplayObject* display = item->property("VipDisplayObject").value<VipDisplayObject*>()) {
		// if the VipPlotItemData belongs to a VipDisplayObject, use the VipDisplayObject input data
		any = display->inputAt(0)->data();
	}

	// for VipRasterData, convert to VipNDArray
	if (any.data().userType() == qMetaTypeId<VipRasterData>()) {
		QVariant _data = any.data();
		any.setData(_data);
	}

	QString filename = path;
	if (filename.isEmpty()) {
		QStringList filters = VipIODevice::possibleWriteFilters(QString(), QVariantList() << any.data());
		filename = VipFileDialog::getSaveFileName2(nullptr, any.name(), "Save data", filters.join(";;"));
		vip_debug("%s\n", filename.toLatin1().data());
	}
	if (!filename.isEmpty()) {
		VipProgress progress;
		progress.setText("<b>Save</b> " + QFileInfo(filename).fileName() + "...");
		progress.setModal(true);
		vipProcessEvents();
		QList<VipIODevice::Info> devices = VipIODevice::possibleWriteDevices(filename, QVariantList() << any.data());
		VipIODevice* device = VipCreateDevice::create(devices);
		if (device) {
			device->setProperty("player", QVariant::fromValue(this));

			// if the device'w input is a VipMultiInput, add an input
			if (VipMultiInput* in = device->topLevelInputAt(0)->toMultiInput())
				in->add();
			device->setPath(filename);
			if (!device->open(VipIODevice::WriteOnly)) {
				VIP_LOG_ERROR("Failed to open output file ", filename);
				delete device;
				return false;
			}
			device->inputAt(0)->setData(any);
			device->update();
			bool res = !device->hasError();
			delete device;
			if (res)
				VIP_LOG_INFO("Item's content saved successfully");
			else
				VIP_LOG_ERROR("Failed to save item's content");
			return res;
		}
	}
	return false;
}

class VipVideoPlayer::PrivateData
{
public:
	PrivateData()
	  : currentDisplay(nullptr)
	  , previousImageDataType(0)
	  , pendingRectTrial(0)
	{
	}
	~PrivateData()
	{
		// if (recorder)
		//  delete recorder.data();
	}
	VipImageWidget2D* viewer;
	QComboBox* zoomChoice;
	QToolButton* sharedZoom;

	// for the first image, check if we should display the image properties
	QPointer<VipDisplayObject> currentDisplay;

	QSize previousImageSize;
	int previousImageDataType;
	bool isFrozen;

	QAction* frozen;

	VipExtractComponentEditor* componentChoice;
	QAction* componentAction;
	VipDisplayImageEditor* multiArrayChoice;
	QAction* multiArray;

	QAction* show_axes;
	QPointer<VipExtractComponent> extract;
	QPointer<VipProcessingList> processingList;

	QMenu* superimposeMenu;
	QSlider* superimposeSlider;
	QToolButton* superimposeButton;
	QAction* superimposeAction;
	QAction* sharedZoomAction;
	QAction* zoomChoiceAction;
	QPointer<VipVideoPlayer> superimposePlayer;
	QTimer timer;
	QRectF pendingRect;
	int pendingRectTrial;

	QTransform transform; // global image transform used for ROIs

	// widget to edit the processing list (if any)
	QAction* processing_tree_action;
	QToolButton* processing_tree_button;
	std::unique_ptr<VipProcessingObjectMenu> processing_menu;

	// recording options;
	// QAction * record;
	// QPointer<VipGenericRecorder> recorder; //use a pointer for VipGenericRecorder since it might be added (and deleted) to a VipProcessingObject
	// VipRecordWidget recordWidget;

	// when showing/hidding axes
	double canvasLevel;

	QList<int> rates; // processing rate, make the avergae of the last ones
};

static QList<VipDisplayObject*> getSelectedDisplays(VipVideoPlayer* pl)
{
	// get all selected items
	QList<VipDisplayObject*> displays;
	QList<VipPlotItemData*> items = vipCastItemListOrdered<VipPlotItemData*>(pl->plotWidget2D()->area()->findItems<VipPlotItemData*>(QString(), 1, 1));
	for (int i = 0; i < items.size(); ++i) {
		VipPlotItemData* it = items[i];
		if (VipDisplayObject* disp = it->property("VipDisplayObject").value<VipDisplayObject*>()) {
			if (displays.indexOf(disp) < 0)
				displays.append(disp);
		}
		else {
			// for VipPlotShape, find the scene model
			if (VipPlotShape* sh = qobject_cast<VipPlotShape*>(it))
				if (VipPlotSceneModel* psm = sh->property("VipPlotSceneModel").value<VipPlotSceneModel*>())
					if (VipDisplayObject* d = psm->property("VipDisplayObject").value<VipDisplayObject*>()) {
						if (displays.indexOf(d) < 0)
							displays.append(d);
					}
		}
	}
	if (displays.isEmpty()) {
		// add the video display
		if (VipDisplayObject* disp = pl->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>()) {
			if (displays.indexOf(disp) < 0)
				displays.append(disp);
		}
	}
	return displays;
}

static QList<VipProcessingObject*> applyVideoProcessingOnDrop(VipVideoPlayer* pl, const VipProcessingObject::Info& proc)
{
	QList<VipProcessingObject*> res;
	QList<VipDisplayObject*> displays = getSelectedDisplays(pl);
	if (displays.isEmpty())
		return res;

	if (VipDisplayObject* display = displays.last()) {
		if (VipOutput* out = display->inputAt(0)->connection()->source()) {
			if (VipProcessingObject* obj = proc.create()) {
				// check input count
				if (obj->inputCount() != 1) {
					if (VipMultiInput* multi = obj->topLevelInputAt(0)->toMultiInput()) {
						if (!multi->resize(1))
							return res;
					}
					else
						return res;
				}
				// Input transform: add the processing in a new VipProcessingList
				if (proc.displayHint == VipProcessingObject::InputTransform) {
					if (pl == VipPlayer2D::dropTarget()) {
						// try to drop a InputTransform processing on itself:
						// add the processing into the processing list
						pl->addSelectedProcessing(proc);
						return res;
					}
					VipProcessingList* lst = new VipProcessingList();
					lst->setOverrideName(pl->spectrogram()->title().text() + " (" + vipSplitClassname(proc.classname) + ")");
					lst->setDeleteOnOutputConnectionsClosed(true);
					lst->append(obj);
					lst->inputAt(0)->setData(out->data());
					lst->update();
					lst->setScheduleStrategy(VipProcessingObject::Asynchronous);
					lst->inputAt(0)->setConnection(out);
					res.append(lst);
				}
				// other kind of processing
				else {
					if (VipProcessingObject* tmp = vipCreateProcessing(out, proc))
						res.append(tmp);
					delete obj;
				}
			}
		}
	}

	if (res.size()) {
		// update the processing editor
		vipGetProcessingEditorToolWidget()->setProcessingObject(res.last());
		if (VipProcessingListEditor* editor = vipGetProcessingEditorToolWidget()->editor()->processingEditor<VipProcessingListEditor*>(res.last())) {
			if (VipProcessingList* lst = qobject_cast<VipProcessingList*>(res.last()))
				if (lst->size())
					editor->selectObject(lst->processings().last());
			vipGetProcessingEditorToolWidget()->editor()->setProcessingObjectVisible(res.last(), true);
			vipGetProcessingEditorToolWidget()->show();
			vipGetProcessingEditorToolWidget()->raise();
		}
	}
	return vipListCast<VipProcessingObject*>(res);
}

static void __create_video_processing_menu(VipProcessingObjectMenu* menu, VipVideoPlayer* pl)
{
	// get all selected items
	QList<VipDisplayObject*> displays = getSelectedDisplays(pl);
	if (displays.isEmpty()) {
		menu->clear();
		return;
	}

	menu->setProcessingInfos(VipProcessingObject::validProcessingObjects(QVariantList() << displays.last()->inputAt(0)->probe().data(), 1, VipDisplayObject::DisplayOnDifferentSupport).values());

	// make the processing menu draggable and droppable
	QList<QAction*> acts = menu->processingActions();
	for (int i = 0; i < acts.size(); ++i) {
		const auto lst = VipFDAddProcessingAction().match(acts[i], pl);
		bool applied = false;
		for (int j = 0; j < lst.size(); ++j)
			applied = applied || lst[j](acts[i], pl).value<bool>();
		if (!applied) {
			// make the menu action droppable
			acts[i]->setProperty("QMimeData",
					     QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipProcessingObject*>>(
					       std::bind(&applyVideoProcessingOnDrop, pl, acts[i]->property("Info").value<VipProcessingObject::Info>()), VipCoordinateSystem::Cartesian, acts[i])));
		}
		else
			acts[i]->setProperty("_vip_notrigger", true);
	}
}

ImageAndText::ImageAndText()
{
	QHBoxLayout* lay = new QHBoxLayout();
	lay->addWidget(&image);
	lay->addWidget(&text);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);
}

VipVideoPlayer::VipVideoPlayer(VipImageWidget2D* img, QWidget* parent)
  : VipPlayer2D(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	// d_data->recorder = new VipGenericRecorder(this);
	d_data->viewer = img ? img : new VipImageWidget2D(this);
	d_data->viewer->setMinimumSize(100, 100);
	d_data->viewer->setStyleSheet("VipImageWidget2D {background-color:transparent;}");
	d_data->viewer->area()->boxStyle().setBackgroundBrush(QBrush(Qt::transparent));
	d_data->viewer->setBackgroundBrush(QBrush(Qt::transparent));
	d_data->viewer->scene()->setBackgroundBrush(QBrush(Qt::transparent));
	d_data->viewer->area()->setMousePanning(Qt::RightButton);
	// d_data->viewer->area()->setMouseZoomSelection(Qt::LeftButton);
	d_data->viewer->area()->setMouseWheelZoom(true);
	d_data->viewer->area()->grid()->setHoverEffect();
	d_data->viewer->area()->grid()->setFlag(QGraphicsItem::ItemIsSelectable, false);
	d_data->viewer->area()->canvas()->setFlag(QGraphicsItem::ItemIsSelectable, false);
	d_data->viewer->area()->titleAxis()->setVisible(false);

	d_data->isFrozen = false;
	toolBar()->insertSeparator(toolBar()->actions().first());
	toolBar()->insertAction(toolBar()->actions().first(),
				d_data->frozen = new QAction(vipIcon("freeze.png"),
							     "<b>Freeze player</b>"
							     "<br>Avoid to update the player's content anymore.<br>"
							     "This will also disable any other processing relying on this player like time trace, values along a segment,...",
							     toolBar()));
	d_data->frozen->setCheckable(true);

	toolBar()->selectionModeAction->setToolTip("<b>Area selection</b><br>Select all Regions Of Interest that intersect the drawn area");

	// specific tool bar actions for images
	d_data->show_axes = new QAction(this);
	d_data->show_axes->setIcon(vipIcon("show_axes.png"));
	d_data->show_axes->setText(QString("Show/hide axes"));
	d_data->show_axes->setCheckable(true);
	d_data->show_axes->setChecked(true);
	toolBar()->addAction(d_data->show_axes);

	// d_data->record = toolBar()->addAction(vipIcon("record_icon.png"), "Show/hide record movie panel");
	//  d_data->record->setCheckable(true);
	//  d_data->record->setVisible(false);
	//  connect(d_data->record, SIGNAL(triggered(bool)), &d_data->recordWidget, SLOT(setVisible(bool)));

	toolBar()->addSeparator();

	d_data->componentChoice = new VipExtractComponentEditor();
	d_data->componentChoice->setToolTip("Extract a component");
	d_data->componentAction = this->toolBar()->addWidget(d_data->componentChoice);
	d_data->componentAction->setVisible(false);
	d_data->componentChoice->choices()->setToolTip("<b>Extract a component</b><br>Extract a component BEFORE applying the other processings (if any).<br>"
						       "It could be the real or imaginary part of a complex image, the red component of a color image, ...");

	d_data->multiArrayChoice = new VipDisplayImageEditor();
	d_data->multiArray = this->toolBar()->addWidget(d_data->multiArrayChoice);
	d_data->multiArrayChoice->editor()->choices()->setToolTip("<b>Select the component or channel to display</b><br>"
								  "This image provides several components or channels, select the one you wish to display.<br>"
								  "It could be for instance the real or imaginary part of a complex image.");
	d_data->multiArray->setVisible(false);

	d_data->sharedZoom = new QToolButton();
	d_data->sharedZoom->setAutoRaise(true);
	d_data->sharedZoom->setIcon(vipIcon("zoom.png"));
	d_data->sharedZoom->setToolTip("<b>Shared zoom</b><br>Zooming or panning within a video will apply the same zoom/panning to other videos in this workspace.");
	d_data->sharedZoom->setCheckable(true);
	connect(d_data->sharedZoom, SIGNAL(clicked(bool)), this, SLOT(setSharedZoom(bool)));

	d_data->zoomChoice = new VipComboBox();
	d_data->zoomChoice->setFrame(false);
	d_data->zoomChoice->setEditable(true);
	d_data->zoomChoice->setToolTip("Selected zoom factor");
	QStringList zooms;
	zooms << "Expand"
	      << "3200%"
	      << "2400%"
	      << "1600%"
	      << "1200%"
	      << "800%"
	      << "700%"
	      << "600%"
	      << "500%"
	      << "400%"
	      << "300%"
	      << "200%"
	      << "100%"
	      << "66%"
	      << "50%"
	      << "33%"
	      << "25%"
	      << "16%"
	      << "12%";
	d_data->zoomChoice->addItems(zooms);
	d_data->zoomChoice->setEditable(true);

	// create the superimpose menu and tool button
	d_data->superimposeMenu = new QMenu();
	d_data->superimposeSlider = new QSlider(Qt::Horizontal);
	QWidgetAction* slider = new QWidgetAction(d_data->superimposeMenu);
	slider->setToolTip("Superimpose opacity");
	d_data->superimposeSlider->setRange(0, 100);
	d_data->superimposeSlider->setValue(50);
	slider->setDefaultWidget(d_data->superimposeSlider);
	d_data->superimposeMenu->addAction(slider);
	connect(d_data->superimposeSlider, SIGNAL(valueChanged(int)), this, SLOT(setSuperimposeOpacity(int)));
	connect(d_data->superimposeMenu, SIGNAL(triggered(QAction*)), this, SLOT(superimposeTriggered(QAction*)));
	connect(d_data->superimposeMenu, SIGNAL(aboutToShow()), this, SLOT(computeSuperimposeMenu()));

	d_data->superimposeButton = new QToolButton();
	d_data->superimposeButton->setAutoRaise(true);
	d_data->superimposeButton->setIcon(vipIcon("superp.png"));
	d_data->superimposeButton->setToolTip("Superimpose an image");
	d_data->superimposeButton->setObjectName("Superimpose");
	d_data->superimposeButton->setMenu(d_data->superimposeMenu);
	d_data->superimposeButton->setPopupMode(QToolButton::InstantPopup);
	d_data->superimposeButton->setMinimumWidth(25);
	d_data->superimposeAction = toolBar()->addWidget(d_data->superimposeButton);

	//
	// tool button to add new processing
	//

	d_data->processing_tree_button = new QToolButton();
	d_data->processing_tree_button->setAutoRaise(true);
	d_data->processing_tree_button->setToolTip("<b>Apply an image processing</b><br>This will display the processing editor panel");
	d_data->processing_tree_button->setIcon(vipIcon("PROCESSING.png"));
	d_data->processing_tree_button->setPopupMode(QToolButton::InstantPopup);

	d_data->processing_menu.reset( new VipProcessingObjectMenu());
	d_data->processing_tree_button->setMenu(d_data->processing_menu.get());
	// d_data->processing_menu->setProcessingInfos(VipProcessingObject::validProcessingObjects(QVariantList() << QVariant::fromValue(VipNDArray(QMetaType::Int,vipVector(3,3))),
	// 1,VipDisplayObject::DisplayOnDifferentSupport).values());
	//  QList<QAction*> acts = d_data->processing_menu->processingActions();
	//  for(int i=0; i < acts.size(); ++i) {
	//  QList<VipFunction> lst = VipFDAddProcessingAction().match(acts[i],this);
	//  bool applied = false;
	//  for(int j=0; j < lst.size(); ++j) applied = applied || lst[j](acts[i],this).value<bool>();
	//  if(applied)
	//  acts[i]->setProperty("_vip_notrigger",true);
	//  }
	connect(d_data->processing_menu.get(), SIGNAL(aboutToShow()), this, SLOT(updateProcessingMenu()));
	d_data->processing_tree_action = toolBar()->addWidget(d_data->processing_tree_button);
	connect(d_data->processing_menu.get(), SIGNAL(selected(const VipProcessingObject::Info&)), this, SLOT(addSelectedProcessing(const VipProcessingObject::Info&)));

	this->setPlotWidget2D(d_data->viewer);
	this->setSpectrogram(this->spectrogram());

	// Disable drawing selection order
	d_data->viewer->area()->setDrawSelectionOrder(nullptr);

	// set the default color map
	d_data->viewer->area()->colorMapAxis()->setUseBorderDistHintForLayout(true);
	d_data->viewer->area()->colorMapAxis()->scaleDraw()->enableLabelOverlapping(true);
	d_data->viewer->area()->colorMapAxis()->setColorMap(d_data->viewer->area()->colorMapAxis()->gripInterval(),
							    VipLinearColorMap::createColorMap(VipGuiDisplayParamaters::instance()->playerColorScale()));

	d_data->viewer->area()->colorMapAxis()->scaleDraw()->valueToText()->setAutomaticExponent(true);
	d_data->viewer->area()->colorMapAxis()->scaleDraw()->valueToText()->setMaxLabelSize(4);

	// Set a better slider handle
	d_data->viewer->area()->colorMapAxis()->grip1()->setImage(vipPixmap("slider_handle.png").toImage());
	d_data->viewer->area()->colorMapAxis()->grip2()->setImage(vipPixmap("slider_handle.png").toImage());

	// d_data->viewer->area()->colorMap()->grip1()->setZValue(-std::numeric_limits<double>::max());
	// d_data->viewer->area()->colorMap()->grip2()->setZValue(-std::numeric_limits<double>::max());
	d_data->viewer->area()->colorMapAxis()->setZValue(std::numeric_limits<double>::max());

	// this->gridLayout()->addWidget(&d_data->recordWidget, 11, 10);
	// d_data->recordWidget.hide();
	// d_data->recordWidget.setGenericRecorder(d_data->recorder);
	// d_data->recordWidget.updateFileFilters(QVariantList() << QVariant::fromValue(VipNDArray()));
	// d_data->recorder->setScheduleStrategy(VipProcessingObject::Asynchronous, true);
	// d_data->recorder->setEnabled(false);
	// connect(&d_data->recordWidget,SIGNAL(recordingChanged(bool)),this,SLOT(setRecording(bool)));

	// timer used to display the status info
	d_data->timer.setSingleShot(false);
	d_data->timer.setInterval(20);

	d_data->canvasLevel = d_data->viewer->area()->canvas()->zValue();

	statusText()->setToolTip("Display frame rate (frames/s)");

	// add a button for quick ROI drawing
	toolBar()->addWidget(vipGetSceneModelWidgetPlayer()->createPlayerButton(this));

	// Add zoom options to tool bar
	d_data->sharedZoomAction = toolBar()->addWidget(d_data->sharedZoom);
	d_data->zoomChoiceAction = toolBar()->addWidget(d_data->zoomChoice);

	// TODO: uncomment
	connect(d_data->viewer->area()->colorMapAxis(), SIGNAL(valueChanged(double)), this, SLOT(disableAutomaticColorScale()));
	connect(this->spectrogram(), SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(showColorScaleParameters()));
	connect(d_data->viewer->area()->colorMapAxis(), SIGNAL(mouseButtonDoubleClick(VipAbstractScale*, VipPlotItem::MouseButton, double)), this, SLOT(showColorScaleParameters()));
	connect(d_data->viewer->area()->colorMapAxis(), SIGNAL(colorMapChanged(int)), this, SLOT(receivedColorMapChanged(int)));
	connect(d_data->zoomChoice, SIGNAL(currentTextChanged(const QString&)), this, SLOT(toolBarZoomChanged()));
	connect(d_data->viewer->area(), SIGNAL(visualizedAreaChanged()), this, SLOT(viewerZoomChanged()), Qt::QueuedConnection);
	connect(d_data->viewer->area(), SIGNAL(visualizedAreaChanged()), this, SLOT(visualizedAreaChanged()), Qt::QueuedConnection);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(updateStatusInfo()));
	connect(d_data->show_axes, SIGNAL(triggered(bool)), this, SLOT(showAxes(bool)), Qt::QueuedConnection);
	connect(d_data->frozen, SIGNAL(triggered(bool)), this, SLOT(setFrozen(bool)));

	connect(this, SIGNAL(sceneModelAdded(VipPlotSceneModel*)), this, SLOT(updateImageTransform()), Qt::QueuedConnection);
	connect(this, SIGNAL(sceneModelRemoved(VipPlotSceneModel*)), this, SLOT(updateImageTransform()), Qt::QueuedConnection);

	d_data->timer.start();

	VipUniqueId::id(this);

	// polish the player to be sure that the stylesheet properties are applied
	QStyle* style = (qApp->style());
	style->polish(this);

	// set the tool tip
	VipPlayerToolTip::setDefaultToolTipFlags(
	  VipToolTip::DisplayFlags(VipToolTip::All & (~VipToolTip::Axes) & (~VipToolTip::SearchXAxis) & (~VipToolTip::SearchYAxis) & (~VipToolTip::ItemsProperties)),
	  &VipVideoPlayer::staticMetaObject);
	VipToolTip* tip = new VipToolTip();
	tip->setDistanceToPointer(20);
	tip->setDisplayFlags(VipPlayerToolTip::toolTipFlags(&VipVideoPlayer::staticMetaObject));
	d_data->viewer->area()->setPlotToolTip(tip);
	toolTipFlagsChanged(VipPlayerToolTip::toolTipFlags(&VipVideoPlayer::staticMetaObject));

	tip->setMaxLines(20);
	tip->setMaxLineMessage("See more in the 'Player properties' panel");

	// TEST
	// tip->setRegionPositions( Vip::RegionPositions(Vip::XInside | Vip::YInside));
	// tip->setAlignment(Qt::AlignLeft | Qt::AlignTop);
	tip->setDelayTime(5000);

	// default video player parameters
	VipGuiDisplayParamaters::instance()->apply(this);

	VipPlayerLifeTime::emitCreated(this);
}

VipVideoPlayer::~VipVideoPlayer()
{
	d_data->timer.stop();
	QCoreApplication::instance()->removePostedEvents(this, QEvent::MetaCall);
	VipPlayerLifeTime::emitDestroyed(this);
}

void VipVideoPlayer::setZoomFeaturesVisible(bool vis)
{
	d_data->sharedZoomAction->setVisible(vis);
	d_data->zoomChoiceAction->setVisible(vis);
}
bool VipVideoPlayer::zoomFeaturesVisible() const
{
	return d_data->sharedZoomAction->isVisible();
}

QToolButton* VipVideoPlayer::superimposeButton() const
{
	return d_data->superimposeButton;
}

QAction* VipVideoPlayer::superimposeAction() const
{
	return d_data->superimposeAction;
}

QComboBox* VipVideoPlayer::zoomWidget() const
{
	return d_data->zoomChoice;
}

QAction* VipVideoPlayer::frozenAction() const
{
	return d_data->frozen;
}

VipImageWidget2D* VipVideoPlayer::viewer() const
{
	return const_cast<VipImageWidget2D*>(d_data->viewer);
}

QAction* VipVideoPlayer::showAxesAction() const
{
	return d_data->show_axes;
}

void VipVideoPlayer::setProcessingPool(VipProcessingPool* pool)
{
	VipProcessingPool* prev = processingPool();
	VipPlayer2D::setProcessingPool(pool);
	if (pool && pool != prev) {
		// set the color map
		if (VipDisplayPlayerArea* a = VipDisplayPlayerArea::fromChildWidget(this))
			a->setColorMapToPlayer(this, a->useGlobalColorMap());
	}
}

void VipVideoPlayer::computeSuperimposeMenu()
{
	QList<QAction*> acts = d_data->superimposeMenu->actions();
	for (int i = 1; i < acts.size(); ++i)
		delete acts[i];

	// get all other VipVideoPlayer in this tab
	VipBaseDragWidget* parent = VipBaseDragWidget::fromChild(this);
	QList<VipVideoPlayer*> players;
	if (parent)
		parent = parent->validTopLevelMultiDragWidget();
	if (parent) {
		QWidget* top_level = parent->parentWidget();
		if (top_level)
			players = top_level->findChildren<VipVideoPlayer*>();
	}

	// add the players into the superimpose menu
	for (int i = 0; i < players.size(); ++i) {
		if (players[i] != this) {
			VipBaseDragWidget* p = VipBaseDragWidget::fromChild(players[i]);
			QString title = p ? p->windowTitle() : players[i]->windowTitle();
			QAction* act = d_data->superimposeMenu->addAction(title);
			act->setCheckable(true);
			if (d_data->superimposePlayer == players[i])
				act->setChecked(true);
			act->setProperty("player", QVariant::fromValue(QObjectPointer(players[i])));
		}
	}
}

void VipVideoPlayer::setSuperimposeOpacity(int value)
{
	double opacity = value / 100.0;
	this->spectrogram()->setSuperimposeOpacity(opacity);
}

void VipVideoPlayer::superimposeTriggered(QAction* act)
{
	if (!act->isChecked()) {
		this->spectrogram()->setSuperimposeImage(QImage());
		d_data->superimposePlayer = nullptr;
	}
	else if (VipVideoPlayer* player = qobject_cast<VipVideoPlayer*>(act->property("player").value<QObjectPointer>())) {
		this->spectrogram()->setSuperimposeImage(player->spectrogram()->image());
		d_data->superimposePlayer = player;
	}
}

void VipVideoPlayer::addSelectedProcessing(const VipProcessingObject::Info& info)
{
	if (!d_data->processingList)
		return;

	if (info.displayHint == VipProcessingObject::DisplayOnSameSupport) {
		VipProcessingObject* last = nullptr;
		// create a new pipeline and display it in this player
		if (VipDisplayObject* obj = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>())
			if (VipOutput* out = obj->inputAt(0)->connection()->source()) {
				if (VipProcessingObject* tmp = vipCreateProcessing(out, info)) {
					if (vipCreatePlayersFromProcessing(obj, this).size())
						last = tmp;
				}
			}
		if (last) {
			vipGetProcessingEditorToolWidget()->setProcessingObject(last);
			QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
		}
	}
	else if (info.displayHint == VipProcessingObject::DisplayOnDifferentSupport) {
		VipProcessingObject* last = nullptr;
		VipAbstractPlayer* pl = nullptr;
		// create a new player and display all new pipelines in this new player
		// first, create the new player
		if (VipDisplayObject* obj = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>())
			if (VipOutput* out = obj->inputAt(0)->connection()->source()) {
				if ((last = vipCreateProcessing(out, info))) {
					QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessing(obj, nullptr);
					if (pls.size())
						pl = pls[0];
				}
			}
		if (!pl)
			return;
		if (last) {
			vipGetProcessingEditorToolWidget()->setProcessingObject(last);
			QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
		}
	}
	else if (info.displayHint == VipProcessingObject::InputTransform) {
		// add the selected processings
		QList<VipProcessingObject::Info> infos = QList<VipProcessingObject::Info>() << info;

		if (infos.size()) {
			vipGetProcessingEditorToolWidget()->setProcessingObject(this->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>());
			if (VipProcessingListEditor* editor = vipGetProcessingEditorToolWidget()->editor()->processingEditor<VipProcessingListEditor*>(d_data->processingList)) {
				editor->addProcessings(infos);
				if (d_data->processingList->size())
					editor->selectObject(d_data->processingList->processings().last());

				vipGetProcessingEditorToolWidget()->editor()->setProcessingObjectVisible(d_data->processingList, true);
				vipGetProcessingEditorToolWidget()->show();
				vipGetProcessingEditorToolWidget()->raise();
				QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
			}
		}
	}
}

double VipVideoPlayer::zoomFactor() const
{
	return d_data->viewer->area()->zoom();
}

void VipVideoPlayer::setZoomFactor(double value)
{
	QPointF center = d_data->viewer->area()->visualizedImageRect().center();
	QRectF inner_rect = d_data->viewer->area()->innerRect();

	QTransform tr;
	tr.scale(1 / value, 1 / value);
	inner_rect = tr.mapRect(inner_rect);
	inner_rect.moveCenter(center);

	d_data->viewer->area()->setVisualizedImageRect(inner_rect);
}

QTransform VipVideoPlayer::computeImageTransform() const
{
	if (spectrogram())
		if (VipDisplayImage* disp = spectrogram()->property("VipDisplayObject").value<VipDisplayImage*>())
			return disp->globalImageTransform();
	return QTransform();
}

QTransform VipVideoPlayer::imageTransform() const
{
	return d_data->transform;
}

// VipNDArray VipVideoPlayer::currentImage() const
//  {
//  if (VipDisplayImage * disp = spectrogram()->property("VipDisplayObject").value<VipDisplayImage*>())
//  {
//  return disp->inputAt(0)->probe().value<VipNDArray>();
//  }
//  return VipNDArray();
//  }
//
//  VipNDArray VipVideoPlayer::initialImage() const
//  {
//  //we need the source VipIODevice with the initial image
//
//  if (VipDisplayImage * disp = spectrogram()->property("VipDisplayObject").value<VipDisplayImage*>())
//  {
//  QList<VipProcessingObject*> srcs = disp->allSources();
//  srcs.prepend(disp);
//
//  if (srcs.size() > 1)
//  {
//  //we the processing after the source to get the image
//  return srcs[srcs.size() - 2]->inputAt(0)->connection()->source()->data().value<VipNDArray>();
//  }
//  else
//  return disp->inputAt(0)->connection()->source()->data().value<VipNDArray>();
//  }
//
//  return VipNDArray();
//  }

QRectF VipVideoPlayer::visualizedImageRect() const
{
	return d_data->viewer->area()->visualizedImageRect();
}

void VipVideoPlayer::setVisualizedImageRect(const QRectF& r)
{
	d_data->viewer->area()->setVisualizedImageRect(r);
}

void VipVideoPlayer::setPendingVisualizedImageRect(const QRectF& r)
{
	d_data->pendingRect = r;
	d_data->pendingRectTrial = 0;
}

VipNDArray VipVideoPlayer::array() const
{
	if (d_data->currentDisplay)
		return d_data->currentDisplay->inputAt(0)->probe().value<VipNDArray>();
	else
		return d_data->viewer->area()->array();
}

QGraphicsObject* VipVideoPlayer::defaultEditableObject() const
{
	if (spectrogram())
		return d_data->viewer->area()->colorMapAxis();
	return d_data->viewer->area()->grid();
}

// void VipVideoPlayer::setRecording(bool record)
//  {
//  //retrieve the last data
//  VipAnyData last;
//  if (VipDisplayObject * display = this->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>())
//  last = display->inputAt(0)->probe();
//
//  d_data->recorder->setEnabled(record);
//  if (record && !last.isEmpty())
//  d_data->recorder->inputAt(0)->setData(last);
//  }
//
//  VipGenericRecorder * VipVideoPlayer::recorder() const
//  {
//  return d_data->recorder;
//  }
//
//  VipRecordWidget * VipVideoPlayer::recordWidget() const
//  {
//  return &d_data->recordWidget;
//  }

VipProcessingList* VipVideoPlayer::sourceProcessingList() const
{
	if (d_data->processingList)
		return d_data->processingList;

	// Try to find the first source VipProcessingList
	if (VipDisplayObject* disp = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>()) {
		QList<VipProcessingList*> lst = vipListCast<VipProcessingList*>(disp->allSources());
		if (lst.size())
			return lst.first();
	}
	return nullptr;
}

void VipVideoPlayer::updateContent()
{
	if (!spectrogram())
		return;

	// if (spectrogram()->backgroundimage().isNull())
	//  spectrogram()->setBackgroundImage(QImage("C:/Users/VM213788/Documents/W7X/2021/55567_WA_CAD.png"));
	// spectrogram()->setSuperimposeImage(QImage());

	VipDisplayObject* disp = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();

	if (disp) {
		// if the recorder is not connected, connect it
		if (VipOutput* out = disp->inputAt(0)->connection()->source()) {
			if (disp != d_data->currentDisplay) {
				// TEST: moved to here
				disp->setSourceProperty("VipSceneModel", QVariant::fromValue(this->plotSceneModel()->sceneModel()));

				d_data->frozen->setVisible(false);
				// if the source VipDisplayObject changed, check if we should display the image properties (do NOT display if there are too many properties)
				// VipAnyData data = out->data();
				if (out->data().data().userType() == 0) {
					d_data->currentDisplay = nullptr;
				}
				else {
					if (d_data->currentDisplay)
						disconnect(d_data->currentDisplay, SIGNAL(imageTransformChanged(VipProcessingObject*)), this, SLOT(updateImageTransform()));

					d_data->currentDisplay = disp;
				}

				if (d_data->currentDisplay) {
					QList<VipIODevice*> devices = vipListCast<VipIODevice*>(d_data->currentDisplay->allSources());
					if (devices.size() == 1)
						d_data->frozen->setVisible(true);

					connect(d_data->currentDisplay, SIGNAL(imageTransformChanged(VipProcessingObject*)), this, SLOT(updateImageTransform()), Qt::QueuedConnection);

					if (d_data->isFrozen && d_data->currentDisplay->isEnabled())
						d_data->currentDisplay->setEnabled(false);
					else if (!d_data->isFrozen && !d_data->currentDisplay->isEnabled())
						d_data->currentDisplay->setEnabled(true);
				}

				// update the ROI transform
				updateImageTransform();
			}
		}

		// find a source VipProcessingList and an VipExtractComponent inside it
		// if a VipProcessingList is found, also set the VipProcessingList to the processing editor menu and show it in the tool bar

		if (!d_data->extract || !d_data->processingList) {
			QList<VipProcessingList*> lst = vipListCast<VipProcessingList*>(disp->allSources());
			if (lst.size()) {
				d_data->processingList = lst.first();

				// set the VipProcessingList to the editor menu
				if (!d_data->processing_tree_action->isVisible())
					d_data->processing_tree_action->setVisible(true);

				QList<VipExtractComponent*> extracts = vipListCast<VipExtractComponent*>(d_data->processingList->processings());
				if (extracts.size()) {
					d_data->extract = extracts.back();
					d_data->componentChoice->setExtractComponent(d_data->extract);
				}
			}
			if (!d_data->processingList && d_data->processing_tree_action->isVisible())
				d_data->processing_tree_action->setVisible(false);
		}

		if (d_data->multiArrayChoice->displayImage() != disp)
			d_data->multiArrayChoice->setDisplayImage(qobject_cast<VipDisplayImage*>(disp));

		if (!d_data->pendingRect.isNull()) {
			// Set the visualizedimage rect after a session loading
			// Do it several times until almost good or until pendingRectTrial is > 10
			QRectF visualized = visualizedImageRect();
			QPointF top_left = visualized.topLeft() - d_data->pendingRect.topLeft();
			QPointF bottom_right = visualized.bottomRight() - d_data->pendingRect.bottomRight();
			if ((top_left.manhattanLength() < 10 && bottom_right.manhattanLength() < 10) || d_data->pendingRectTrial > 10)
				d_data->pendingRect = QRectF();
			else {
				this->setVisualizedImageRect(d_data->pendingRect);
				d_data->pendingRectTrial++;
			}
		}
	}
}

void VipVideoPlayer::updateProcessingMenu()
{
	__create_video_processing_menu(d_data->processing_menu.get(), this);
}

void VipVideoPlayer::updateImageTransform()
{
	// apply the global image transform (extracted from the pipeline) to all ROIs
	//QTransform old = d_data->transform;
	QTransform tr = computeImageTransform();

	QList<VipPlotSceneModel*> scenes = this->plotWidget2D()->area()->findItems<VipPlotSceneModel*>(QString(), 2, 1);

	for (int i = 0; i < scenes.size(); ++i) {
		VipPlotSceneModel* p_scene = scenes[i];
		// check if it has a source
		if (VipDisplaySceneModel* obj = p_scene->property("VipDisplayObject").value<VipDisplaySceneModel*>()) {
			// set the transform to the source
			obj->setTransform(tr);
			continue;
		}

		VipSceneModel sm = p_scene->sceneModel();
		VipShapeList shapes = sm.shapes();

		// first, revert back the current shapes
		if (!d_data->transform.isIdentity()) {
			QTransform inv_tr = d_data->transform.inverted();
			for (int j = 0; j < shapes.size(); ++j)
				shapes[j].transform(inv_tr);
		}

		// apply the global transform to all shapes
		for (int j = 0; j < shapes.size(); ++j)
			shapes[j].transform(tr);
	}

	d_data->transform = tr;

	Q_EMIT imageTransformChanged(tr);

	if (this->processingPool())
		this->processingPool()->reload();
	this->update();
}

void VipVideoPlayer::colorMapClicked(VipAbstractScale*, VipPlotItem::MouseButton button, double value)
{
	// display menu to manage contour levels
	if (button != VipPlotItem::RightButton)
		return;

	QMenu menu;
	QAction* add = menu.addAction("Add contour level");
	menu.addSeparator();
	QAction* rem = menu.addAction("Remove all contour level");

	VipPenButton* pen = new VipPenButton();
	pen->setMode(VipPenButton::Pen);
	QWidgetAction* act_pen = new QWidgetAction(this);
	act_pen->setDefaultWidget(pen);
	pen->setPen(defaultContourPen());
	menu.addAction(act_pen);
	connect(pen, SIGNAL(penChanged(const QPen&)), this->spectrogram(), SLOT(setDefaultContourPen(const QPen&)));

	rem->setProperty("remove_all", true);
	add->setProperty("value", value);

	connect(&menu, SIGNAL(triggered(QAction*)), this, SLOT(handleContour(QAction*)));
	menu.exec(QCursor::pos());
}

void VipVideoPlayer::contourClicked(VipSliderGrip* grip, VipPlotItem::MouseButton button)
{
	// display menu to manage contour levels
	if (button != VipPlotItem::RightButton)
		return;

	QMenu menu;
	QAction* rem = menu.addAction("Remove this contour level");
	menu.addSeparator();
	QAction* rem_all = menu.addAction("Remove all contour level");

	VipPenButton* pen = new VipPenButton();
	pen->setMode(VipPenButton::Pen);
	pen->setPen(defaultContourPen());
	QWidgetAction* act_pen = new QWidgetAction(this);
	act_pen->setDefaultWidget(pen);
	menu.addAction(act_pen);
	connect(pen, SIGNAL(penChanged(const QPen&)), this->spectrogram(), SLOT(setDefaultContourPen(const QPen&)));

	rem->setProperty("remove", QVariant::fromValue(grip));
	rem_all->setProperty("remove_all", true);

	connect(&menu, SIGNAL(triggered(QAction*)), this, SLOT(handleContour(QAction*)));
	menu.exec(QCursor::pos());
}

void VipVideoPlayer::handleContour(QAction* act)
{
	if (!act)
		return;
	if (act->property("remove_all").toBool())
		removeAllContourLevels();
	else if (VipSliderGrip* grip = act->property("remove").value<VipSliderGrip*>())
		removeContourLevel(grip->value());
	else
		addContourLevel(act->property("value").toDouble());
}

void VipVideoPlayer::setSpectrogram(VipPlotSpectrogram* spectrogram)
{
	if (this->spectrogram()) {
		// reset the VipSceneModel of the display object
		if (VipDisplayObject* display = this->spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>())
			display->setSourceProperty("VipSceneModel", QVariant());

		disconnect(this->spectrogram(), SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(showColorScaleParameters()));
		disconnect(d_data->viewer->area()->colorMapAxis(), SIGNAL(mouseButtonDoubleClick(VipAbstractScale*, VipPlotItem::MouseButton, double)), this, SLOT(showColorScaleParameters()));
		disconnect(this->spectrogram(), SIGNAL(dataChanged()), this, SLOT(imageChanged()));
		disconnect(d_data->viewer->area()->colorMapAxis(), SIGNAL(colorMapChanged(int)), this, SLOT(receivedColorMapChanged(int)));

		// disconnect signals used to manage contour levels
		if (d_data->viewer->area()->colorMapAxis())
			disconnect(d_data->viewer->area()->colorMapAxis(),
				   SIGNAL(mouseButtonPress(VipAbstractScale*, VipPlotItem::MouseButton, double)),
				   this,
				   SLOT(colorMapClicked(VipAbstractScale*, VipPlotItem::MouseButton, double)));

		QList<VipColorMapGrip*> grips = this->spectrogram()->contourGrips();
		for (int i = 0; i < grips.size(); ++i)
			disconnect(grips[i], SIGNAL(mouseButtonPress(VipSliderGrip*, VipPlotItem::MouseButton)), this, SLOT(contourClicked(VipSliderGrip*, VipPlotItem::MouseButton)));
	}

	d_data->viewer->area()->setSpectrogram(spectrogram);

	if (this->spectrogram()) {
		connect(this->spectrogram(), SIGNAL(mouseButtonDoubleClick(VipPlotItem*, VipPlotItem::MouseButton)), this, SLOT(showColorScaleParameters()));
		connect(d_data->viewer->area()->colorMapAxis(), SIGNAL(mouseButtonDoubleClick(VipAbstractScale*, VipPlotItem::MouseButton, double)), this, SLOT(showColorScaleParameters()));
		connect(this->spectrogram(), SIGNAL(dataChanged()), this, SLOT(imageChanged()), Qt::QueuedConnection);
		connect(d_data->viewer->area()->colorMapAxis(), SIGNAL(colorMapChanged(int)), this, SLOT(receivedColorMapChanged(int)));

		if (this->spectrogram()->toolTipText().isEmpty())
			this->spectrogram()->setToolTipText("<b>X</b>: #avalue0%i<br><b>Y</b>: #avalue1%i<br><b>Value</b>: #value");
		this->spectrogram()->setItemAttribute(VipPlotItem::VisibleLegend, false);
		this->spectrogram()->setItemAttribute(VipPlotItem::HasLegendIcon, false);

		if (automaticWindowTitle())
			this->setWindowTitle(this->spectrogram()->title().text());

		int data_type = this->spectrogram()->rawData().dataType();
		d_data->viewer->area()->colorMapAxis()->setVisible(!(data_type == qMetaTypeId<QImage>() || data_type == qMetaTypeId<QPixmap>()));

		d_data->viewer->area()->colorMapAxis()->scaleDraw()->setTicksPosition(VipScaleDraw::TicksOutside);

		d_data->viewer->area()->colorMapAxis()->grip1()->setToolTipText("#value");
		d_data->viewer->area()->colorMapAxis()->grip2()->setToolTipText("#value");

		d_data->viewer->area()->colorMapAxis()->grip1()->setHandleDistance(0);
		d_data->viewer->area()->colorMapAxis()->grip2()->setHandleDistance(0);

		d_data->viewer->area()->colorMapAxis()->grip1()->setDisplayToolTipValue(Qt::AlignRight | Qt::AlignVCenter);
		d_data->viewer->area()->colorMapAxis()->grip2()->setDisplayToolTipValue(Qt::AlignRight | Qt::AlignVCenter);

		// connect signals used to manage contour levels
		connect(d_data->viewer->area()->colorMapAxis(),
			SIGNAL(mouseButtonPress(VipAbstractScale*, VipPlotItem::MouseButton, double)),
			this,
			SLOT(colorMapClicked(VipAbstractScale*, VipPlotItem::MouseButton, double)));

		QList<VipColorMapGrip*> grips = this->spectrogram()->contourGrips();
		for (int i = 0; i < grips.size(); ++i) {
			disconnect(grips[i], SIGNAL(mouseButtonPress(VipSliderGrip*, VipPlotItem::MouseButton)), this, SLOT(contourClicked(VipSliderGrip*, VipPlotItem::MouseButton)));
			connect(grips[i], SIGNAL(mouseButtonPress(VipSliderGrip*, VipPlotItem::MouseButton)), this, SLOT(contourClicked(VipSliderGrip*, VipPlotItem::MouseButton)));
		}
	}

	// expand image
	QRectF rect = d_data->viewer->area()->imageBoundingRect();
	d_data->viewer->area()->setVisualizedImageRect(rect);

	// update image size and component choice
	d_data->extract = nullptr;
	d_data->processingList = nullptr;
	d_data->processing_tree_action->setVisible(false);

	imageChanged();
}

VipPlotSpectrogram* VipVideoPlayer::spectrogram() const
{
	return d_data->viewer->area()->spectrogram();
}

void VipVideoPlayer::startRender(VipRenderState& state)
{
	// save scroll bar policy
	d_data->viewer->setScrollBarEnabled(false);

	// state.state(this)["recordWidgetVisible"] = d_data->recordWidget.isVisible();
	// d_data->recordWidget.hide();

	VipPlayer2D::startRender(state);
}

void VipVideoPlayer::endRender(VipRenderState& state)
{
	// reset scroll bar policy
	d_data->viewer->setScrollBarEnabled(true);
	// d_data->recordWidget.setVisible(state.state(this)["recordWidgetVisible"].toBool());
	VipPlayer2D::endRender(state);
}

bool VipVideoPlayer::increaseContour()
{
	DoubleList contours = this->contourLevels();
	if (contours.size() == 1) {
		contours.first() = std::round(contours.first() + 1);
		setContourLevels(contours);
		return true;
	}
	return false;
}

bool VipVideoPlayer::decreaseContour()
{
	DoubleList contours = this->contourLevels();
	if (contours.size() == 1) {
		contours.first() = std::round(contours.first() - 1);
		setContourLevels(contours);
		return true;
	}
	return false;
}

void VipVideoPlayer::keyPressEvent(QKeyEvent* evt)
{

	// Use Z and S to move contour level
	evt->ignore();

	if (!evt->modifiers()) {
		if (evt->key() == Qt::Key_Z && !(evt->modifiers() & Qt::CTRL)) {
			if (increaseContour())
				evt->accept();
		}
		else if (evt->key() == Qt::Key_S && !(evt->modifiers() & Qt::CTRL)) {
			if (decreaseContour())
				evt->accept();
		}
		else if (evt->key() == Qt::Key_I && !(evt->modifiers() & Qt::CTRL)) {
			updateSelectedShapesFromIsoLine();
			evt->accept();
		}

		// Shortcuts to draw ROI
		else if (evt->key() == Qt::Key_R) {
			QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "addRect");
			evt->accept();
		}
		else if (evt->key() == Qt::Key_E) {
			QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "addEllipse");
			evt->accept();
		}
		else if (evt->key() == Qt::Key_P) {
			QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "addPolygon");
			evt->accept();
		}
		else if (evt->key() == Qt::Key_F) {
			QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "addMask");
			evt->accept();
		}
		else if (evt->key() == Qt::Key_L) {
			QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "addPolyline");
			evt->accept();
		}
		else if (evt->key() == '.' || evt->key() == ';') {
			QMetaObject::invokeMethod(vipGetSceneModelWidgetPlayer(), "addPixel");
			evt->accept();
		}
		else if (evt->key() == Qt::Key_H) {
			// Switch flat histogram on/off
			setFlatHistogramColorScale(!isFlatHistogramColorScale());
			evt->accept();
		}
	}

	if (!evt->isAccepted())
		VipPlayer2D::keyPressEvent(evt);
}

VipDisplayObject* VipVideoPlayer::mainDisplayObject() const
{
	if (d_data->currentDisplay)
		return d_data->currentDisplay;
	QList<VipDisplayObject*> objs = displayObjects();
	if (objs.size())
		return objs.first();
	return nullptr;
}

void VipVideoPlayer::refreshToolTip()
{
	if (d_data->viewer->area()->plotToolTip() && VipCorrectedTip::isVisible())
		d_data->viewer->area()->plotToolTip()->refresh();
}

void VipVideoPlayer::showAxes(bool show)
{
	if (show != isShowAxes()) {
		d_data->show_axes->blockSignals(true);
		d_data->show_axes->setChecked(show);
		d_data->show_axes->blockSignals(false);

		d_data->viewer->area()->leftAxis()->setVisible(show);
		d_data->viewer->area()->topAxis()->setVisible(show);
		d_data->viewer->area()->rightAxis()->setVisible(show);
		d_data->viewer->area()->bottomAxis()->setVisible(show);
		d_data->viewer->area()->grid()->setVisible(show);

		// if (show)
		//  {
		//  plotWidget2D()->area()->canvas()->setZValue(d_data->canvasLevel);
		//  }
		//  else
		//  {
		//  d_data->canvasLevel = plotWidget2D()->area()->canvas()->zValue();
		//  plotWidget2D()->area()->canvas()->setZValue(spectrogram()->zValue() + 1);
		//  }

		d_data->viewer->area()->recomputeGeometry();
		viewerZoomChanged();
		update();
	}
}

bool VipVideoPlayer::isShowAxes() const
{
	return d_data->viewer->area()->leftAxis()->isVisible();
}

void VipVideoPlayer::toolBarZoomChanged()
{
	d_data->zoomChoice->blockSignals(true);

	QString value = d_data->zoomChoice->currentText();

	if (value == "Expand") {
		QRectF rect = d_data->viewer->area()->imageBoundingRect();
		d_data->viewer->area()->setVisualizedImageRect(rect);
	}
	else {
		value.replace("%", "");
		bool ok;
		double zoom = value.toDouble(&ok);
		if (ok) {
			setZoomFactor(zoom / 100);
			QCoreApplication::processEvents();
			setZoomFactor(zoom / 100);
		}
	}

	QCoreApplication::processEvents();

	d_data->zoomChoice->blockSignals(false);
}

void VipVideoPlayer::updateStatusInfo()
{
	if (!d_data->viewer->area()->spectrogram())
		return;
	// display the position
	QPoint pos = QCursor::pos();
	pos = d_data->viewer->mapFromGlobal(pos);
	QPointF scene_pos = d_data->viewer->mapToScene(pos);
	QPointF scale_pos = d_data->viewer->area()->spectrogram()->sceneMap()->invTransform(scene_pos);
	QPoint int_pos((int)scale_pos.x(), (int)scale_pos.y());

	// display the frame rate
	VipDisplayObject* disp = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (disp) {
		double rate = disp->processingRate();

		// compute the average rate
		d_data->rates.append(rate);
		if (d_data->rates.size() > 3)
			d_data->rates.pop_front();
		double avg = 0;
		for (int i = 0; i < d_data->rates.size(); ++i)
			avg += d_data->rates[i];
		avg /= d_data->rates.size();

		statusText()->setText("<b>Rate</b>: " + QString::number((int)avg) + "/s");
	}
}

void VipVideoPlayer::showColorScaleParameters()
{
	vipGetPlotToolWidgetPlayer()->setItem(d_data->viewer->area()->colorMapAxis());
	vipGetPlotToolWidgetPlayer()->show();
	vipGetPlotToolWidgetPlayer()->resetSize();
}

void VipVideoPlayer::setColorScaleVisible(bool vis)
{
	VipAxisColorMap* map = d_data->viewer->area()->colorMapAxis();
	if (map->isVisible() != vis) {
		map->setVisible(vis);
		if (vis) {
			map->grip1()->setVisible(true);
			map->grip2()->setVisible(true);
		}
		this->plotWidget2D()->recomputeGeometry();
	}
}

bool VipVideoPlayer::isAutomaticColorScale() const
{
	return d_data->viewer->area()->colorMapAxis()->isAutoScale();
}

bool VipVideoPlayer::isColorScaleVisible() const
{
	return d_data->viewer->area()->colorMapAxis()->isVisible();
}

bool VipVideoPlayer::isFrozen() const
{
	return d_data->isFrozen;
}

void VipVideoPlayer::setFrozen(bool enable)
{
	if (d_data->isFrozen != enable) {
		d_data->isFrozen = enable;
		if (d_data->currentDisplay) {

			// disable the processing before (usually processinglist)
			QList<VipDisplayObject*> disp = this->displayObjects();
			for (VipDisplayObject* d : disp) {
				if (d->inputAt(0)->connection()->source())
					if (VipProcessingObject* o = d->inputAt(0)->connection()->source()->parentProcessing())
						o->setEnabled(!enable);
			}
			// if (d_data->currentDisplay->inputAt(0)->connection()->source())
			//	if (VipProcessingObject* o = d_data->currentDisplay->inputAt(0)->connection()->source()->parentProcessing())
			//		o->setEnabled(!enable);
		}

		if (!enable && processingPool())
			processingPool()->reload();
	}
	d_data->frozen->blockSignals(true);
	d_data->frozen->setChecked(enable);
	d_data->frozen->blockSignals(false);
}

bool VipVideoPlayer::isSharedZoom() const
{
	bool res = false;
	if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChildWidget(const_cast<VipVideoPlayer*>(this))) {
		res = area->property("_vip_sharedZoom").toBool();
	}
	else
		res = d_data->sharedZoom->isChecked();

	if (d_data->sharedZoom->isChecked() != res) {
		d_data->sharedZoom->blockSignals(true);
		d_data->sharedZoom->setChecked(res);
		d_data->sharedZoom->blockSignals(false);
	}
	return res;
}
void VipVideoPlayer::setSharedZoom(bool enable)
{
	d_data->sharedZoom->blockSignals(true);
	d_data->sharedZoom->setChecked(enable);
	d_data->sharedZoom->blockSignals(false);
	if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChildWidget(const_cast<VipVideoPlayer*>(this))) {
		area->setProperty("_vip_sharedZoom", enable);
	}
}

void VipVideoPlayer::setZoomFeaturesEnabled(bool enable)
{
	setProperty("_vip_zoomFeatures", enable);
}
bool VipVideoPlayer::zoomFeaturesEnabled() const
{
	QVariant v = property("_vip_zoomFeatures");
	if (v.userType() == 0)
		return true;
	return v.toBool();
}

void VipVideoPlayer::visualizedAreaChanged()
{
	static QMap<VipDisplayPlayerArea*, qint64> _times;

	// apply zoom to other
	if (isSharedZoom() && zoomFeaturesEnabled()) {
		// get all video players in workspace
		if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChildWidget(this)) {

			// get last update time
			QMap<VipDisplayPlayerArea*, qint64>::iterator it = _times.find(area);
			if (it != _times.end() && (QDateTime::currentMSecsSinceEpoch() - it.value()) < 100)
				return;
			_times[area] = QDateTime::currentMSecsSinceEpoch();
			QList<VipVideoPlayer*> players = area->findChildren<VipVideoPlayer*>();
			players.removeOne(this);

			// set zoom to all players

			// first remove transform to zoom
			QRectF rect = visualizedImageRect();
			QTransform tr = imageTransform();
			rect = tr.inverted().map(rect).boundingRect();

			for (int i = 0; i < players.size(); ++i) {
				// apply player transform
				VipVideoPlayer* pl = players[i];
				players.removeAt(i);
				--i;
				QRectF r = pl->imageTransform().map(rect).boundingRect();

				disconnect(pl->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), pl, SLOT(visualizedAreaChanged()));
				disconnect(pl->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), pl->plotWidget2D(), SLOT(computeScrollBars()));
				pl->setVisualizedImageRect(r);
				QMetaObject::invokeMethod(pl->plotWidget2D(), "computeScrollBars", Qt::DirectConnection);
				connect(pl->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), pl, SLOT(visualizedAreaChanged()));
				connect(pl->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), pl->plotWidget2D(), SLOT(computeScrollBars()), Qt::QueuedConnection);
			}
		}
	}
}

void VipVideoPlayer::viewerZoomChanged()
{
	d_data->zoomChoice->blockSignals(true);
	double factor = zoomFactor();
	d_data->zoomChoice->setCurrentText(QString::number(qRound((factor * 100))) + "%");
	d_data->zoomChoice->blockSignals(false);
}

static void setActionVisible(QAction* a, bool vis)
{
	a->setVisible(vis);
	if (QWidgetAction* wa = qobject_cast<QWidgetAction*>(a))
		wa->defaultWidget()->setVisible(vis);
}
/*static QAction* findAction(QWidget* w, const char* name)
{
	const QList<QAction*> acts = w->actions();
	for (int i = 0; i < acts.size(); ++i)
		if (acts[i]->objectName() == name)
			return acts[i];
	return nullptr;
}*/
void VipVideoPlayer::setColorMapOptionsVisible(bool visible)
{
	if (QAction* a = this->property("show_scale").value<QAction*>())
		setActionVisible(a, visible);
	if (QAction* a = this->property("auto_scale").value<QAction*>())
		setActionVisible(a, visible);
	if (QAction* a = this->property("fit_to_grip").value<QAction*>())
		setActionVisible(a, visible);
	if (QAction* a = this->property("histo_scale").value<QAction*>())
		setActionVisible(a, visible);
	if (QAction* a = this->property("scale_sep").value<QAction*>())
		setActionVisible(a, visible);
	if (QAction* a = this->property("scale_params").value<QAction*>())
		setActionVisible(a, visible);
	if (QAction* a = this->property("scale").value<QAction*>())
		setActionVisible(a, visible);
}

void VipVideoPlayer::updateContourLevels()
{
	// update grips
	QList<VipColorMapGrip*> grips = this->spectrogram()->contourGrips();
	for (int i = 0; i < grips.size(); ++i) {
		grips[i]->setHandleDistance(3);
		grips[i]->setToolTipText("#value");
		grips[i]->setDisplayToolTipValue(Qt::AlignRight | Qt::AlignVCenter);
		disconnect(grips[i], SIGNAL(mouseButtonPress(VipSliderGrip*, VipPlotItem::MouseButton)), this, SLOT(contourClicked(VipSliderGrip*, VipPlotItem::MouseButton)));
		connect(grips[i], SIGNAL(mouseButtonPress(VipSliderGrip*, VipPlotItem::MouseButton)), this, SLOT(contourClicked(VipSliderGrip*, VipPlotItem::MouseButton)));
	}
}

void VipVideoPlayer::addContourLevel(double value)
{
	QList<vip_double> lines = this->spectrogram()->contourLevels();
	lines.append(value);
	this->spectrogram()->setContourLevels(lines, true, vipPixmap("slider_here.png"));

	// update grips
	updateContourLevels();
}

void VipVideoPlayer::removeContourLevel(double value)
{
	QList<vip_double> lines = this->spectrogram()->contourLevels();
	lines.removeOne(value);
	this->spectrogram()->setContourLevels(lines, true, vipPixmap("slider_here.png"));

	// update grips
	updateContourLevels();
}

void VipVideoPlayer::setContourLevels(const DoubleList& levels)
{
	this->spectrogram()->setContourLevels(levels, true, vipPixmap("slider_here.png"));

	// update grips
	updateContourLevels();
}

void VipVideoPlayer::removeAllContourLevels()
{
	this->spectrogram()->setContourLevels(QList<vip_double>());
}

QList<vip_double> VipVideoPlayer::contourLevels() const
{
	return this->spectrogram()->contourLevels();
}

QPen VipVideoPlayer::defaultContourPen() const
{
	return this->spectrogram()->defaultContourPen();
}
void VipVideoPlayer::setDefaultContourPen(const QPen& p)
{
	this->spectrogram()->setDefaultContourPen(p);
}

int VipVideoPlayer::colorMap() const
{
	if (d_data->viewer->area()->colorMapAxis()->colorMap()->mapType() == VipColorMap::Linear)
		return static_cast<VipLinearColorMap*>(d_data->viewer->area()->colorMapAxis()->colorMap())->type();
	return VipLinearColorMap::Unknown;
}

void VipVideoPlayer::setColorMap(int map)
{
	bool is_flat_histo = isFlatHistogramColorScale();
	d_data->viewer->area()->colorMapAxis()->setColorMap(VipLinearColorMap::StandardColorMap(map));
	setFlatHistogramColorScale(is_flat_histo);
}
void VipVideoPlayer::receivedColorMapChanged(int map)
{
	Q_EMIT colorMapChanged(map);
}

bool VipVideoPlayer::isFlatHistogramColorScale() const
{
	return d_data->viewer->area()->colorMapAxis()->useFlatHistogram();
}
void VipVideoPlayer::setFlatHistogramColorScale(bool enable)
{
	d_data->viewer->area()->colorMapAxis()->setUseFlatHistogram(enable);
	this->spectrogram()->update();
}

int VipVideoPlayer::flatHistogramStrength() const
{
	return d_data->viewer->area()->colorMapAxis()->flatHistogramStrength();
}
void VipVideoPlayer::setFlatHistogramStrength(int strength)
{
	d_data->viewer->area()->colorMapAxis()->setFlatHistogramStrength(strength);
	this->spectrogram()->update();
}

void VipVideoPlayer::setAutomaticColorScale(bool auto_scale)
{
	d_data->viewer->area()->colorMapAxis()->setAutoScale(auto_scale);
}

void VipVideoPlayer::enableAutomaticColorScale()
{
	setAutomaticColorScale(true);
}
void VipVideoPlayer::disableAutomaticColorScale()
{
	setAutomaticColorScale(false);
}

void VipVideoPlayer::fitColorScaleToGrips()
{
	VipInterval inter = d_data->viewer->area()->colorMapAxis()->gripInterval();
	d_data->viewer->area()->colorMapAxis()->setAutoScale(false);
	d_data->viewer->area()->colorMapAxis()->divideAxisScale(inter.minValue(), inter.maxValue());
}

void VipVideoPlayer::onPlayerCreated()
{
	imageChanged();
}

bool VipVideoPlayer::isColorImage() const
{
	int data_type = spectrogram()->rawData().dataType();
	return ((data_type == qMetaTypeId<QImage>() || data_type == qMetaTypeId<QPixmap>()));
}

void VipVideoPlayer::imageChanged()
{
	// vip_debug("image changed\n");
	// update the image size display
	QSize size = d_data->viewer->area()->imageBoundingRect().size().toSize();
	if (size != d_data->previousImageSize) {
		d_data->previousImageSize = size;
		// recompute the zoom after all the plotting geometry stuff has been performed
		QMetaObject::invokeMethod(this, "viewerZoomChanged", Qt::QueuedConnection);
	}

	// update the different source processings if necessary
	updateContent();

	// update the tool tip
	refreshToolTip();

	// update the array choice combo box for VipMultiNDArray
	// bool is_multi_array = false;
	//  if (VipDisplayImage  * img = qobject_cast<VipDisplayImage*>(d_data->currentDisplay))
	//  {
	//  //update multiArrayChoice visibility
	//  const VipNDArray ar = img->inputAt(0)->probe().value<VipNDArray>();
	//  is_multi_array = vipIsMultiNDArray(ar) || !VipDisplayImage::canDisplayImageAsIs(ar);
	//  if (!d_data->multiArray->isVisible() && is_multi_array)
	//  d_data->multiArray->setVisible(true);
	//  else if (!is_multi_array && d_data->multiArray->isVisible())
	//  d_data->multiArray->setVisible(false);
	//  }
	//
	// d_data->componentAction->setVisible((bool)d_data->extract && d_data->extract->supportedComponents().size() > 1);
	if (VipDisplayImage* img = qobject_cast<VipDisplayImage*>(d_data->currentDisplay)) {
		// Check for multi component images (VipMultiNDArray of complex image or RGB image...)
		const VipNDArray ar = img->inputAt(0)->probe().value<VipNDArray>();
		bool multi_component = (bool)d_data->extract && d_data->extract->supportedComponents().size() > 1;
		if (multi_component) {
			// favor d_data->extract if possible
			if (!d_data->componentAction->isVisible())
				d_data->componentAction->setVisible(true);
			if (d_data->multiArray->isVisible())
				d_data->multiArray->setVisible(false);
		}
		else {
			bool is_multi_array = vipIsMultiNDArray(ar) || !VipDisplayImage::canDisplayImageAsIs(ar);
			if (is_multi_array) {
				if (d_data->extract && d_data->componentAction->isVisible())
					d_data->componentAction->setVisible(false);
				if (!d_data->multiArray->isVisible())
					d_data->multiArray->setVisible(true);
			}
			else {
				if (d_data->extract && d_data->componentAction->isVisible())
					d_data->componentAction->setVisible(false);
				if (d_data->multiArray->isVisible())
					d_data->multiArray->setVisible(false);
			}
		}
	}

	// update the color map visibility
	int data_type = spectrogram()->rawData().dataType();
	if ((data_type == qMetaTypeId<QImage>() || data_type == qMetaTypeId<QPixmap>())) {
		if (d_data->viewer->area()->colorMapAxis()->isVisible())
			d_data->viewer->area()->colorMapAxis()->setVisible(false);
	}
	else if ((d_data->previousImageDataType == qMetaTypeId<QImage>() || d_data->previousImageDataType == qMetaTypeId<QPixmap>()) && !d_data->viewer->area()->colorMapAxis()->isVisible())
		d_data->viewer->area()->colorMapAxis()->setVisible(true);
	d_data->previousImageDataType = data_type;

	// change the player title if necessary
	if (automaticWindowTitle() && (this->windowTitle() != this->spectrogram()->title().text() && !this->spectrogram()->title().isEmpty()))
		this->setWindowTitle(this->spectrogram()->title().text());

	Q_EMIT displayImageChanged();
}

void VipVideoPlayer::resizeEvent(QResizeEvent* evt)
{
	VipPlayer2D::resizeEvent(evt);
	// recompute the zoom after all the plotting geometry stuff has been performed
	QMetaObject::invokeMethod(this, "viewerZoomChanged", Qt::QueuedConnection);
}

static QString __vip_getExtractMethod(const VipNDArray& ar)
{
	if (ar.canConvert<double>())
		return QString();

	QStringList lst = vipPossibleComponents(ar);
	if (!lst.size())
		return QString();

	QStringList methods;
	for (int i = 0; i < lst.size(); ++i)
		methods.append(vipMethodDescription(lst[i]));

	QComboBox* box = new QComboBox();
	box->addItems(methods);
	box->setCurrentIndex(0);
	box->setToolTip("Select the way the image will be splitted into multiple components");
	VipGenericDialog dialog(box, "Components choice");
	if (dialog.exec() == QDialog::Accepted)
		return lst[box->currentIndex()];
	return QString();
}

// more tricky: extract the pixels along a polyline
QList<VipDisplayCurve*> VipVideoPlayer::extractPolylines(const VipShapeList& shs, const QString& method) const
{
	VipDisplayObject* display = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (!display)
		return QList<VipDisplayCurve*>();

	// try to retrieve the source VipOutput and VipProcessingObject of this VipDisplayObject
	VipOutput* src_output = nullptr;
	VipProcessingObject* src_proc = nullptr;

	if (VipInput* input = display->inputAt(0))
		if (VipConnectionPtr con = input->connection())
			if (VipOutput* source = con->source())
				if (VipProcessingObject* tmp = source->parentProcessing()) {
					src_output = source;
					src_proc = tmp;
				}

	if (!src_output || !src_proc)
		return QList<VipDisplayCurve*>();

	QList<VipDisplayCurve*> res;

	for (int s = 0; s < shs.size(); ++s) {

		VipShape sh = shs[s];
		const VipNDArray ar = src_output->value<VipNDArray>(); // d_data->viewer->area()->array();
		QString _method = method;
		if (_method.isEmpty() && !ar.canConvert<double>())
			_method = __vip_getExtractMethod(ar);

		// connect the VipOutput to a VipExtractPolyline, a (list of) VipProcessingList  and a (list of) VipDisplayObject

		VipExtractPolyline* extract = new VipExtractPolyline();
		extract->propertyName("method")->setData(_method);
		extract->setParent(display->parent());
		extract->setScheduleStrategies(VipProcessingObject::Asynchronous);
		extract->setDeleteOnOutputConnectionsClosed(true);

		// set output name
		if (VipAnnotation* annot = vipLoadAnnotation(sh.attribute("_vip_annotation").toByteArray()))
			if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
				VipSimpleAnnotation* a = static_cast<VipSimpleAnnotation*>(annot);
				QString curve_name = a->text().text();
				if (!curve_name.isEmpty())
					extract->propertyName("output_name")->setData(curve_name + " polyline");
			}

		// the shape might come from a scene model which is generated from a processing.
		// If this is the case, find the generator, connect the output which generate the scene model to the extractor property
		if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(sh)) {
			// we've got the VipDisplaySceneModel which display the scene model. Now grab its input, connect the corresponding output to the extract property
			if (VipOutput* src = disp_sm->inputAt(0)->connection()->source()) {
				src->setConnection(extract->propertyAt(0));
				extract->propertyAt(1)->setData(sh.identifier());
			}
			else
				extract->setShape(sh);
			// set the additional transform
			extract->setShapeTransform(disp_sm->transform());
		}
		else
			extract->setShape(sh);

		// set the first data to get the number of output

		extract->inputAt(0)->setData(ar);
		extract->wait();
		VipMultiOutput* out = extract->topLevelOutputAt(0)->toMultiOutput();
		src_output->setConnection(extract->inputAt(0));

		QList<QPen> pen;
		if (vipIsImageArray(ar))
			pen << QPen(Qt::red) << QPen(Qt::green) << QPen(Qt::blue) << QPen(Qt::yellow);
		else
			for (int i = 0; i < out->count(); ++i)
				pen << QPen();

		for (int o = 0; o < out->count(); ++o) {
			VipProcessingList* lst = new VipProcessingList(display->parent());
			lst->setScheduleStrategies(VipProcessingObject::Asynchronous);
			lst->setDeleteOnOutputConnectionsClosed(true);
			out->at(o)->setConnection(lst->inputAt(0));

			VipDisplayCurve* curve = new VipDisplayCurve(display->parent());
			curve->setScheduleStrategies(VipProcessingObject::Asynchronous);
			curve->item()->setTitle(VipText(out->at(o)->data().name())); // sh.group() + " " +QString::number(sh.id()) + " " + extract->components()[o]));
			curve->item()->boxStyle().setBorderPen(pen[o]);
			if (curve->item()->symbol()) {
				curve->item()->symbol()->setBrush(pen[o].color());
				curve->item()->symbol()->setPen(pen[o].color().darker(110));
			}
			curve->item()->setRenderHints(QPainter::Antialiasing);
			curve->setItemSuppressable(true);
			lst->outputAt(0)->setConnection(curve->inputAt(0));

			res.append(curve);
		}

		// reset the first data to update the display objects
		extract->inputAt(0)->setData(ar);
	}

	if (src_proc->parentObjectPool())
		src_proc->parentObjectPool()->reload();

	return res;
}

QList<VipDisplayHistogram*> VipVideoPlayer::extractHistograms(const VipShape& sh, const QString& method) const
{
	VipDisplayObject* display = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (!display)
		return QList<VipDisplayHistogram*>();

	// try to retrieve the source VipOutput and VipProcessingObject of this VipDisplayObject
	VipOutput* src_output = nullptr;
	VipProcessingObject* src_proc = nullptr;

	if (VipInput* input = display->inputAt(0))
		if (VipConnectionPtr con = input->connection())
			if (VipOutput* source = con->source())
				if (VipProcessingObject* tmp = source->parentProcessing()) {
					src_output = source;
					src_proc = tmp;
				}

	if (!src_output || !src_proc)
		return QList<VipDisplayHistogram*>();

	const VipNDArray ar = src_output->value<VipNDArray>(); // d_data->viewer->area()->array();
	QString _method = method;
	if (_method.isEmpty() && !ar.canConvert<double>())
		_method = __vip_getExtractMethod(ar);

	// connect the VipOutput to a VipExtractPolyline, a (list of) VipProcessingList  and a (list of) VipDisplayObject

	VipExtractHistogram* extract = new VipExtractHistogram();
	extract->propertyName("method")->setData(_method);
	extract->setParent(display->parent());
	extract->setScheduleStrategies(VipProcessingObject::Asynchronous);
	extract->setDeleteOnOutputConnectionsClosed(true);

	// set output name
	if (VipAnnotation* annot = vipLoadAnnotation(sh.attribute("_vip_annotation").toByteArray()))
		if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
			VipSimpleAnnotation* a = static_cast<VipSimpleAnnotation*>(annot);
			QString curve_name = a->text().text();
			if (!curve_name.isEmpty())
				extract->propertyName("output_name")->setData(curve_name + " histogram");
		}

	// the shape might come from a scene model which is generated from a processing.
	// If this is the case, find the generator, connect the output which generate the scene model to the extractor property
	if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(sh)) {
		// we've got the VipDisplaySceneModel which display the scene model. Now grab its input, connect the corresponding output to the extract property
		if (VipOutput* src = disp_sm->inputAt(0)->connection()->source()) {
			src->setConnection(extract->propertyAt(0));
			extract->propertyAt(1)->setData(sh.identifier());
		}
		else
			extract->setShape(sh);

		extract->setShapeTransform(disp_sm->transform());
	}
	else
		extract->setShape(sh);

	// VipNDArray array = d_data->viewer->area()->array();

	if (sh.isNull()) {
		VipShape full_shape(QRectF(0, 0, ar.shape(1), ar.shape(0)));
		extract->setShape(full_shape);
	}

	// set the first data to get the number of output
	VipAnyData in(QVariant::fromValue(ar), 0);
	in.setName(spectrogram()->title().text());
	extract->inputAt(0)->setData(in);
	extract->wait();
	src_output->setConnection(extract->inputAt(0));

	VipMultiOutput* out = extract->topLevelOutputAt(0)->toMultiOutput();

	QList<VipDisplayHistogram*> res;
	QList<QBrush> brush;
	if (vipIsImageArray(ar) && _method == "Color ARGB")
		brush << QBrush(Qt::red) << QBrush(Qt::green) << QBrush(Qt::blue) << QBrush(Qt::yellow);
	else
		for (int i = 0; i < out->count(); ++i)
			brush << QBrush(Qt::red);

	QString title = sh.group() + " " + QString::number(sh.id());
	if (sh.isNull())
		title = spectrogram()->title().text();

	for (int o = 0; o < out->count(); ++o) {
		VipProcessingList* lst = new VipProcessingList(display->parent());
		lst->setScheduleStrategies(VipProcessingObject::Asynchronous);
		lst->setDeleteOnOutputConnectionsClosed(true);
		out->at(o)->setConnection(lst->inputAt(0));

		VipDisplayHistogram* curve = new VipDisplayHistogram(display->parent());
		curve->setScheduleStrategies(VipProcessingObject::Asynchronous);
		curve->item()->setTitle(VipText(out->at(o)->data().name())); // title + " " + extract->components()[o]));
		curve->item()->boxStyle().setBackgroundBrush(brush[o]);
		// curve->item()->boxStyle().setBorderPen(brush[o].color());
		curve->setItemSuppressable(true);
		lst->outputAt(0)->setConnection(curve->inputAt(0));

		res.append(curve);
	}

	// reset the first data to update the display objects
	extract->inputAt(0)->setData(ar);

	if (src_proc->parentObjectPool())
		src_proc->parentObjectPool()->reload();

	return res;
}

QPoint VipVideoPlayer::globalPosToImagePos(const QPoint& global)
{
	QPoint pos = plotWidget2D()->mapFromGlobal(global);
	QPointF scene_pos = plotWidget2D()->mapToScene(pos);
	QPointF spec_pos = spectrogram()->mapFromScene(scene_pos);
	QPointF img_posF = spectrogram()->sceneMap()->invTransform(spec_pos);
	QPoint img_pos((int)img_posF.x(), (int)img_posF.y());
	return img_pos;
}

#include "VipPolygon.h"

static VipNDArrayType<int> label(const VipNDArray& img)
{
	VipNDArrayType<int> out(img.shape());
	VipNDArrayType<int> im = img.toInt32();
	vipLabelImage(im, out, 0);
	return out;
}

void VipVideoPlayer::createShapeFromIsoLine(const QPoint& img_pos)
{
	QList<vip_double> contours = contourLevels();
	if (contours.size() != 1)
		return;

	double value = contours.first();
	VipNDArray img = array(); // spectrogram()->rawData().extract();
	// segment
	img = img >= value;
	img = label(img);
	int foreground = img.value(vipVector(img_pos.y(), img_pos.x())).toInt();
	double epsilon = 0.1;
	QPolygonF poly = vipExtractMaskPolygon(img, foreground, 0.1, img_pos);
	while (poly.size() > 64) {
		epsilon *= 2;
		poly = vipExtractMaskPolygon(img, foreground, epsilon, img_pos);
	}
	if (poly.size()) {
		VipShape sh(poly);
		plotSceneModel()->sceneModel().add("ROI", sh);
	}
}

static void UpdateShapeFromIsoLine(VipShape& sh, VipNDArray label_img, const QPoint& img_pos)
{
	int foreground = label_img.value(vipVector(img_pos.y(), img_pos.x())).toInt();
	double epsilon = 0.1;
	QPolygonF poly = vipExtractMaskPolygon(label_img, foreground, 0.1, img_pos);
	while (poly.size() > 64) {
		epsilon *= 2;
		poly = vipExtractMaskPolygon(label_img, foreground, epsilon, img_pos);
	}
	if (poly.size()) {
		sh.setPolygon(poly);
	}
}

void VipVideoPlayer::updateShapeFromIsoLine(const QPoint& img_pos)
{
	QList<vip_double> contours = contourLevels();
	if (contours.size() != 1)
		return;

	QList<VipPlotShape*> shapes = plotSceneModel()->shapes("ROI", 1);
	if (shapes.isEmpty())
		return;

	VipShape sh;
	if (shapes.size() == 1)
		sh = shapes.first()->rawData();
	else {
		for (int i = shapes.size() - 1; i >= 0; --i) {
			if (shapes[i]->rawData().shape().contains(img_pos)) {
				sh = shapes[i]->rawData();
				break;
			}
		}
	}
	if (sh.isNull())
		sh = shapes.last()->rawData();

	VipNDArray img = img >= contours.first();
	img = label(img);
	UpdateShapeFromIsoLine(sh, img, img_pos);
}

void VipVideoPlayer::updateSelectedShapesFromIsoLine()
{
	/// More tricky: update all selected ROI based on the current iso contour (if any).
	/// For that we segment the image, label it, and compute the intersection of the result with each shape.
	/// If several blob intersect the shape, we take the largest intersection one.

	QList<vip_double> contours = contourLevels();
	if (contours.size() != 1)
		return;

	QList<VipPlotShape*> shapes = plotSceneModel()->shapes("ROI", 1);
	if (shapes.isEmpty()) {
		// create a new ROI
		createShapeFromIsoLine(globalPosToImagePos(lastMousePressScreenPos()));
		return;
	}

	VipShapeList shs;
	for (int i = 0; i < shapes.size(); ++i) {
		shs.append(shapes[i]->rawData());
	}

	VipNDArray img = array();
	img = img >= contours.first();
	VipNDArrayType<int> labelled = label(img);

	for (int i = 0; i < shs.size(); ++i) {
		// compute intersection of shape with labelled image
		QMap<int, QPair<int, QPoint>> inter; // map of label -> pixel count, first point
		const QRegion reg = shs[i].region();
		for (const QRect& r : reg) {
			for (int y = r.top(); y < r.top() + r.height(); ++y)
				for (int x = r.left(); x < r.left() + r.width(); ++x) {
					int l = labelled(y, x);
					if (l) {
						QMap<int, QPair<int, QPoint>>::iterator it = inter.find(l);
						if (it == inter.end())
							inter[l] = QPair<int, QPoint>(1, QPoint(x, y));
						else
							it.value().first++;
					}
				}
		}

		if (inter.size()) {
			// find maximum intersection
			int max = 0;
			QPoint start;
			for (QMap<int, QPair<int, QPoint>>::iterator it = inter.begin(); it != inter.end(); ++it) {
				if (it.value().first > max) {
					max = it.value().first;
					start = it.value().second;
				}
			}
			// update shape
			UpdateShapeFromIsoLine(shs[i], labelled, start);
		}
	}
}

struct TimeEvolutionOptions : public QWidget
{
	QRadioButton shape_union;
	QRadioButton shape_inter;
	QRadioButton shape_multi;
	QCheckBox max, min, mean, std, pixCount, entropy, kurtosis, skewness;
	QSpinBox skip;
	TimeEvolutionOptions(bool has_multi_shapes)
	  : max("Maximum")
	  , min("Minimum")
	  , mean("Mean")
	  , std("Standard deviation")
	  , pixCount("Pixel count")
	  , entropy("Entropy")
	  , kurtosis("Kurtosis")
	  , skewness("Skewness")
	{
		int row = 0;
		QGridLayout* grid = new QGridLayout();

		if (has_multi_shapes) {
			shape_union.setText("Use the union of selected shapes");
			shape_inter.setText("Use the intersection of selected shapes");
			shape_multi.setText("Extract time trace for all selected shapes");
			grid->addWidget(&shape_union, row++, 0, 1, 4);
			grid->addWidget(&shape_inter, row++, 0, 1, 4);
			grid->addWidget(&shape_multi, row++, 0, 1, 4);
			grid->addWidget(VipLineWidget::createHLine(), row++, 0, 1, 4);
		}

		grid->addWidget(&max, row, 0);
		grid->addWidget(&min, row, 1);
		grid->addWidget(&mean, row, 2);
		grid->addWidget(&std, row, 3);
		++row;
		grid->addWidget(&pixCount, row, 0);
		grid->addWidget(&entropy, row, 1);
		grid->addWidget(&kurtosis, row, 2);
		grid->addWidget(&skewness, row, 3);
		++row;
		grid->addWidget(new QLabel("Take one frame out of "), row, 0, 1, 3);
		grid->addWidget(&skip, row, 3);
		max.setChecked(true);
		skip.setRange(1, 1000);
		setLayout(grid);

		shape_multi.setChecked(true);
	}
};

QList<VipProcessingObject*> VipVideoPlayer::extractTimeEvolution(const ShapeInfo& infos, VipShapeStatistics::Statistics stats, int one_frame_out_of, int multi_shape, const QVector<double>& quantiles)
{

	// the displayed image cannot be a QImage or QPixmap or complex array
	VipNDArray array = viewer()->area()->array();
	if (!array.canConvert<double>()) {
		VIP_LOG_ERROR("Cannot extract time trace on color or complex images");
		return QList<VipProcessingObject*>();
	}
	if (infos.shapes.isEmpty() && infos.identifiers.isEmpty()) {
		return QList<VipProcessingObject*>();
	}

	// retrieve the processing pool

	VipDisplayObject* display = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (!display)
		return QList<VipProcessingObject*>();

	// try to retrieve the source VipOutput this VipDisplayObject
	VipOutput* src_output = nullptr;
	if (VipInput* input = display->inputAt(0))
		if (VipConnectionPtr con = input->connection())
			if (VipOutput* source = con->source())
				src_output = source;

	// a VipProcessingPool which is not of type Resource is mandatory

	VipProcessingPool* pool = display->parentObjectPool();
	if (!pool || !src_output)
		return QList<VipProcessingObject*>();

	if (pool->deviceType() == VipIODevice::Resource)
		return QList<VipProcessingObject*>();

	// check if wa can enable union or intersection (only for multiple standard ROIs)
	bool can_merge = infos.shapes.size() > 1;
	if (can_merge) {
		for (int i = 0; i < infos.shapes.size(); ++i) {
			if (findDisplaySceneModel(infos.shapes[i])) {
				can_merge = false;
				break;
			}
		}
	}
	if (!can_merge)
		multi_shape = 2; // force extract independantly for all shapes

	// display time trace option if no stats given or if we don't know how to handle multiple shapes
	if (stats == 0 || (can_merge && multi_shape < 0)) {
		// compute the statistics the user wants
		TimeEvolutionOptions* options = new TimeEvolutionOptions(can_merge);
		if (multi_shape == 0)
			options->shape_union.setChecked(true);
		else if (multi_shape == 1)
			options->shape_inter.setChecked(true);
		else if (multi_shape == 2)
			options->shape_multi.setChecked(true);

		VipGenericDialog dialog(options, "Time trace options");
		if (dialog.exec() != QDialog::Accepted)
			return QList<VipProcessingObject*>();

		if (options->min.isChecked())
			stats |= VipShapeStatistics::Minimum;
		if (options->max.isChecked())
			stats |= VipShapeStatistics::Maximum;
		if (options->mean.isChecked())
			stats |= VipShapeStatistics::Mean;
		if (options->std.isChecked())
			stats |= VipShapeStatistics::Std;
		if (options->pixCount.isChecked())
			stats |= VipShapeStatistics::PixelCount;
		if (options->entropy.isChecked())
			stats |= VipShapeStatistics::Entropy;
		if (options->kurtosis.isChecked())
			stats |= VipShapeStatistics::Kurtosis;
		if (options->skewness.isChecked())
			stats |= VipShapeStatistics::Skewness;
		one_frame_out_of = options->skip.value();

		if (options->shape_union.isChecked())
			multi_shape = 0;
		else if (options->shape_inter.isChecked())
			multi_shape = 1;
		else if (options->shape_multi.isChecked())
			multi_shape = 2;
	}
	if (stats == 0)
		return QList<VipProcessingObject*>();

	// compute the actual used shaped depending on the multi_shape parameter
	VipShape sh_merged;
	QString sh_name;
	if (multi_shape == 0) {
		sh_name = "union " + QString::number(infos.shapes.first().id());
		sh_merged = infos.shapes.first().copy();
		if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(infos.shapes.first()))
			sh_merged.transform(disp_sm->transform());
		for (int i = 1; i < infos.shapes.size(); ++i) {
			VipShape tmp = infos.shapes[i];
			// apply the additional transform
			if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(tmp))
				tmp = tmp.copy().transform(disp_sm->transform());
			sh_merged.unite(tmp);
			sh_name += "," + QString::number(infos.shapes[i].id());
		}
	}
	else if (multi_shape == 1) {
		sh_name = "intersection " + QString::number(infos.shapes.first().id());
		sh_merged = infos.shapes.first().copy();
		if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(infos.shapes.first()))
			sh_merged.transform(disp_sm->transform());
		for (int i = 1; i < infos.shapes.size(); ++i) {
			VipShape tmp = infos.shapes[i];
			// apply the additional transform
			if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(tmp))
				tmp = tmp.copy().transform(disp_sm->transform());
			sh_merged.intersect(tmp);
			sh_name += "," + QString::number(infos.shapes[i].id());
		}
	}

	// find all displays within this players, and all their sources
	QList<VipDisplayObject*> displays = this->displayObjects();
	QList<VipProcessingObject*> sources; // all sources
	QList<VipProcessingObject*> leafs;   // last sources before the display
	for (int i = 0; i < displays.size(); ++i) {
		if (VipOutput* src = displays[i]->inputAt(0)->connection()->source())
			if (VipProcessingObject* obj = src->parentProcessing())
				leafs.append(obj);
		sources += displays[i]->allSources();
	}

	QVector<VipExtractStatistics*> extracts;
	QStringList sh_names;

	QList<VipSourceROI> s_shapes;
	QTransform tr = this->imageTransform();
	tr = tr.inverted();

	if (!sh_merged.isNull()) {
		VipSourceROI s;
		s.player = this;
		s.polygon = sh_merged.copy().transform(tr).polygon();
		s_shapes.append(s);
		VipExtractStatistics* extract = new VipExtractStatistics();
		extract->setStatistics(stats);
		extract->setShapeQuantiles(quantiles);
		extracts.append(extract);
		extract->setShape(sh_merged);

		sh_names << sh_merged.group() + " " + sh_name;
	}
	else if (infos.shapes.size()) {
		for (int i = 0; i < infos.shapes.size(); ++i) {
			VipExtractStatistics* extract = new VipExtractStatistics();
			extract->setStatistics(stats);
			extract->setShapeQuantiles(quantiles);
			extracts.append(extract);

			// compute shape name
			const VipShape sh = infos.shapes[i];
			QString curve_name = sh.group() + " " + QString::number(sh.id());
			if (!sh.attribute("_vip_annotation").toByteArray().isEmpty()) {
				// use annotation to get the name
				if (VipAnnotation* annot = vipLoadAnnotation(sh.attribute("_vip_annotation").toByteArray()))
					if (strcmp(annot->name(), "VipSimpleAnnotation") == 0) {
						VipSimpleAnnotation* a = static_cast<VipSimpleAnnotation*>(annot);
						curve_name = a->text().text();
					}
			}
			else if (!sh.attribute("Name").toString().isEmpty())
				curve_name = sh.attribute("Name").toString();
			sh_names << curve_name;

			VipSourceROI s;
			s.player = this;
			s.polygon = sh.copy().transform(tr).polygon();
			s_shapes.append(s);

			// the shape might come from a scene model which is generated from a processing.
			// If this is the case, find the generator, connect the output which generate the scene model to the extractor property

			if (VipDisplaySceneModel* disp_sm = findDisplaySceneModel(infos.shapes[i])) {
				// we've got the VipDisplaySceneModel which display the scene model. Now grab its input, connect the corresponding output to the extract property
				if (VipOutput* src = disp_sm->inputAt(0)->connection()->source()) {
					src->setConnection(extract->propertyAt(0));
					extract->propertyAt(1)->setData(infos.shapes[i].identifier());

					// new in 2.2.16
					// add the VipDisplaySceneModel source processing into the leafs and sources
					leafs.push_back(src->parentProcessing());
					sources << src->parentProcessing()->allSources() << src->parentProcessing();
				}
				else
					extract->setShape(infos.shapes[i]);

				extract->setShapeTransform(disp_sm->transform());
			}
			else
				extract->setShape(infos.shapes[i]);
		}
	}
	else {
		for (int i = 0; i < infos.identifiers.size(); ++i) {
			if (VipDisplaySceneModel* disp_sm = infos.identifiers[i].first) {
				if (VipOutput* src = disp_sm->inputAt(0)->connection()->source()) {
					VipExtractStatistics* extract = new VipExtractStatistics();
					extract->setStatistics(stats);
					extract->setShapeQuantiles(quantiles);
					extracts.append(extract);

					QStringList names = infos.identifiers[i].second.split(":");
					if (names.size() == 2)
						sh_names << names[0] + " " + names[1];
					else
						sh_names << infos.identifiers[i].second;

					src->setConnection(extract->propertyAt(0));
					extract->propertyAt(1)->setData(infos.identifiers[i].second);

					// add the VipDisplaySceneModel source processing into the leafs and sources
					leafs.push_back(src->parentProcessing());
					sources << src->parentProcessing()->allSources() << src->parentProcessing();

					extract->setShapeTransform(disp_sm->transform());
				}
			}
		}
	}

	// make sure sources and leafs are unique
	sources = vipToSet(sources).values();
	leafs = vipToSet(leafs).values();

	// look into the display object sources for VipIODevice, find if the source is Temporal, Sequential or a Resource
	QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
	VipTimeRange intersect_time = VipInvalidTimeRange;
	VipIODevice::DeviceType type = VipIODevice::Resource;
	for (int i = 0; i < devices.size(); ++i)
		if (devices[i]->deviceType() == VipIODevice::Sequential) {
			// Sequential device has the priority
			type = VipIODevice::Sequential;
			break;
		}
		else if (devices[i]->deviceType() == VipIODevice::Temporal) {
			type = VipIODevice::Temporal;
			// compute devices union time range
			VipTimeRange range = devices[i]->timeLimits();
			if (intersect_time == VipInvalidTimeRange)
				intersect_time = range;
			else
				intersect_time = vipUnionRange(intersect_time, range);
		}

	if (type == VipIODevice::Resource)
		return QList<VipProcessingObject*>();

	// for Sequential device only:

	if (type == VipIODevice::Sequential) {
		QList<VipProcessingObject*> result;

		for (int i = 0; i < extracts.size(); ++i) {
			VipExtractStatistics* extract = extracts[i];

			// create the pipeline: Extractor -> ConvertToPointVector -> ProcessingList
			extract->setScheduleStrategies(VipProcessingObject::Asynchronous);
			extract->setDeleteOnOutputConnectionsClosed(true);
			extract->setParent(pool);

			for (int j = 0; j < extract->outputCount(); ++j) {
				if (!extract->outputAt(j)->isEnabled())
					continue;

				VipNumericValueToPointVector* ConvertToPointVector = new VipNumericValueToPointVector(pool);
				ConvertToPointVector->setScheduleStrategies(VipProcessingObject::Asynchronous);
				ConvertToPointVector->setDeleteOnOutputConnectionsClosed(true);
				ConvertToPointVector->inputAt(0)->setConnection(extract->outputAt(j));

				VipProcessingList* ProcessingList = new VipProcessingList(pool);
				ProcessingList->setScheduleStrategies(VipProcessingObject::Asynchronous);
				ProcessingList->setDeleteOnOutputConnectionsClosed(true);
				ProcessingList->inputAt(0)->setConnection(ConvertToPointVector->outputAt(0));

				if (src_output->data().isValid())
					extract->inputAt(0)->setData(src_output->data());
				else
					extract->inputAt(0)->setData(viewer()->area()->array());
				ProcessingList->wait(true);

				VipAnyData any = ProcessingList->outputAt(0)->data();
				result.append(ProcessingList);

				src_output->setConnection(extract->inputAt(0));

				// QString curve_name = sh_names[i];
				// ProcessingList->setAttribute("Name",curve_name + " max");
			}
		}

		return result;
	}

	// for Temporal device only:
	pool->stop(); // stop playing

	VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);
	progress.setText("Extract time trace...");

	// now, save the current VipProcessingPool state, because we are going to modify it heavily
	pool->save();

	// disable all processing except the sources, remove the Automatic flag from the sources
	pool->disableExcept(sources);
	foreach (VipProcessingObject* obj, sources) {
		obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	}

	// create the VipExtractStatistics object and connect it to the display source object
	for (int i = 0; i < extracts.size(); ++i) {
		VipExtractStatistics* extract = extracts[i];
		extract->setLogErrors(QSet<int>());
		src_output->setConnection(extract->inputAt(0));
		extract->inputAt(0)->setData(viewer()->area()->array());
		extract->update();
	}

	// extract the values

	// vector of vector of VipPointVector
	// first vector has size of extracts (one per shape)
	// second vector has a size of 8 (one per stat)
	QVector<QVector<VipPointVector>> stats_values(extracts.size());
	for (int i = 0; i < extracts.size(); ++i)
		stats_values[i].resize(8);
	QVector<VipTimestampedRectListVector> quantiles_values;
	quantiles_values.resize(extracts.size());

	QVector<VipPointVector> maxPos(extracts.size());
	QVector<VipPointVector> minPos(extracts.size());

	qint64 pool_time = pool->time();
	qint64 time = pool->firstTime();
	if (time < intersect_time.first)
		time = intersect_time.first;
	qint64 end_time = pool->lastTime();
	if (end_time > intersect_time.second)
		end_time = intersect_time.second;
	// qint64 current_time = pool->time();
	int skip = one_frame_out_of;
	progress.setRange(time, end_time);

	// block signals
	pool->blockSignals(true);
	for (int i = 0; i < infos.shapes.size(); ++i)
		if (infos.shapes[i].shapeSignals())
			infos.shapes[i].shapeSignals()->blockSignals(true);

	// TEST: Asynchronous strategy
	//  We launch the pipeline in a synchronous way, except the VipExtractStatistics objects
	//  which are Asynchronous.
	//
	//  We tell the outputs of each VipExtractStatistics to bufferize their data.
	//  Then, every 20 frames, we collect the buffered outputs and add them to the final
	//  curves.
	//
	//  This way we fasten the statistics extraction by using each VipExtractStatistics own
	//  internal task pool. This is especially relevant when using multiple ROIs (and therefore
	//  multiple VipExtractStatistics objects).
	//
	//  The previous synchronous version is commented below.

	{
		// Asynchronous strategy with buffered outputs
		for (int i = 0; i < extracts.size(); ++i) {
			VipExtractStatistics* extract = extracts[i];
			extract->setScheduleStrategy(VipProcessingObject::Asynchronous);
			extract->inputAt(0)->setListType(VipDataList::FIFO, VipDataList::Number);
			for (int j = 0; j < extract->outputCount(); ++j)
				extract->outputAt(j)->setBufferDataEnabled(true);
		}

		int count = 0;
		while (time != VipInvalidTime && time <= end_time) {
			progress.setValue(time);

			pool->read(time, true);
			// update all leafs
			for (int i = 0; i < leafs.size(); ++i)
				leafs[i]->update();

			// update statistics every 10 frames
			if (count % 20 == 0)
				for (int i = 0; i < extracts.size(); ++i) {
					VipExtractStatistics* extract = extracts[i];
					extract->wait();
					if (!extract->hasError()) {

						VipAnyDataList lst = extract->outputAt(0)->clearBufferedData();
						for (const VipAnyData& any : lst) {
							minPos[i].push_back(any.attribute("Pos").toPoint());
							stats_values[i][0].append(QPointF(any.time(), any.value<double>()));
						}
						lst = extract->outputAt(1)->clearBufferedData();
						for (const VipAnyData& any : lst) {
							maxPos[i].push_back(any.attribute("Pos").toPoint());
							stats_values[i][1].append(QPointF(any.time(), any.value<double>()));
						}
						for (int index = 2; index < 8; ++index) {
							lst = extract->outputAt(index)->clearBufferedData();
							for (const VipAnyData& any : lst)
								stats_values[i][index].append(QPointF(any.time(), any.value<double>()));
						}
						lst = extract->outputAt(8)->clearBufferedData();
						for (const VipAnyData& any : lst)
							quantiles_values[i].append(VipTimestampedRectList(any.time(), any.value<VipRectList>()));
					}
				}

			// skip frames
			bool end_loop = false;
			for (int i = 0; i < skip; ++i) {
				qint64 next = pool->nextTime(time);
				if (next == time || progress.canceled() || next == VipInvalidTime) {
					end_loop = true;
					break;
				}
				time = next;
			}
			if (end_loop)
				break;

			++count;
		}

		// finish
		for (int i = 0; i < extracts.size(); ++i) {
			VipExtractStatistics* extract = extracts[i];
			extract->wait();
			if (!extract->hasError()) {

				VipAnyDataList lst = extract->outputAt(0)->clearBufferedData();
				for (const VipAnyData& any : lst) {
					minPos[i].push_back(any.attribute("Pos").toPoint());
					stats_values[i][0].append(QPointF(any.time(), any.value<double>()));
				}
				lst = extract->outputAt(1)->clearBufferedData();
				for (const VipAnyData& any : lst) {
					maxPos[i].push_back(any.attribute("Pos").toPoint());
					stats_values[i][1].append(QPointF(any.time(), any.value<double>()));
				}
				for (int index = 2; index < 8; ++index) {
					lst = extract->outputAt(index)->clearBufferedData();
					for (const VipAnyData& any : lst)
						stats_values[i][index].append(QPointF(any.time(), any.value<double>()));
				}
				lst = extract->outputAt(8)->clearBufferedData();
				for (const VipAnyData& any : lst)
					quantiles_values[i].append(VipTimestampedRectList(any.time(), any.value<VipRectList>()));
			}
		}
	}

	/* while (time != VipInvalidTime && time <= end_time)
	{
		progress.setValue(time);

		pool->read(time, true);

		//update all leafs
		for (int i = 0; i < leafs.size(); ++i)
			leafs[i]->update();

		//update statistics
		for (int i = 0; i < extracts.size(); ++i)
		{
			VipExtractStatistics * extract = extracts[i];
			extract->update();
			if (!extract->hasError()) {
				maxPos[i].push_back(extract->outputAt(1)->data().attribute("Pos").toPoint());
				minPos[i].push_back(extract->outputAt(0)->data().attribute("Pos").toPoint());

				stats_values[i][0].append(QPointF(time, extract->outputAt(0)->data().value<double>()));
				stats_values[i][1].append(QPointF(time, extract->outputAt(1)->data().value<double>()));
				stats_values[i][2].append(QPointF(time, extract->outputAt(2)->data().value<double>()));
				stats_values[i][3].append(QPointF(time, extract->outputAt(3)->data().value<double>()));
				stats_values[i][4].append(QPointF(time, extract->outputAt(4)->data().value<double>()));
				stats_values[i][5].append(QPointF(time, extract->outputAt(5)->data().value<double>()));
				stats_values[i][6].append(QPointF(time, extract->outputAt(6)->data().value<double>()));
				stats_values[i][7].append(QPointF(time, extract->outputAt(7)->data().value<double>()));
				quantiles_values[i].append(VipTimestampedRectList(time, extract->outputAt(8)->data().value<VipRectList>()));
			}
		}


		//skip frames
		bool end_loop = false;
		for (int i = 0; i < skip; ++i)
		{
			qint64 next = pool->nextTime(time);
			if (next == time || progress.canceled() || next == VipInvalidTime)
			{
				end_loop = true;
				break;
			}
			time = next;
		}
		if (end_loop)
			break;
	}*/

	// Unblock signals
	pool->blockSignals(false);
	for (int i = 0; i < infos.shapes.size(); ++i)
		if (infos.shapes[i].shapeSignals())
			infos.shapes[i].shapeSignals()->blockSignals(false);

	// store the result
	QList<VipProcessingObject*> res;

	QString y_unit;
	if (d_data->viewer->area()->colorMapAxis())
		y_unit = d_data->viewer->area()->colorMapAxis()->title().text();

	for (int i = 0; i < extracts.size(); ++i) {
		QString curve_name = sh_names[i];

		VipSourceROI s = i < s_shapes.size() ? s_shapes[i] : VipSourceROI();

		if (stats & VipShapeStatistics::Maximum) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			if (!y_unit.isEmpty())
				any->setAttribute("YUnit", y_unit);
			any->setPath(curve_name + " max");
			// set the position of the maximum as a private attribute
			any->setAttribute("_vip_Pos", QVariant::fromValue(maxPos[i]));
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][1]));

			res << any;
		}

		if (stats & VipShapeStatistics::Minimum) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			if (!y_unit.isEmpty())
				any->setAttribute("YUnit", y_unit);
			any->setPath(curve_name + " min");
			// set the position of the minimum as a private attribute
			any->setAttribute("_vip_Pos", QVariant::fromValue(minPos[i]));
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][0]));
			res << any;
		}

		if (stats & VipShapeStatistics::Mean) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			if (!y_unit.isEmpty())
				any->setAttribute("YUnit", y_unit);
			any->setPath(curve_name + " mean");
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][2]));
			res << any;
		}

		if (stats & VipShapeStatistics::Std) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			if (!y_unit.isEmpty())
				any->setAttribute("YUnit", y_unit);
			any->setPath(curve_name + " std");
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][3]));
			res << any;
		}
		if (stats & VipShapeStatistics::PixelCount) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			any->setPath(curve_name + " pixel count");
			any->setData(QVariant::fromValue(stats_values[i][4]));
			res << any;
		}
		if (stats & VipShapeStatistics::Entropy) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			any->setPath(curve_name + " entropy");
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][5]));
			res << any;
		}
		if (stats & VipShapeStatistics::Kurtosis) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			any->setPath(curve_name + " kurtosis");
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][6]));
			res << any;
		}
		if (stats & VipShapeStatistics::Skewness) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			any->setPath(curve_name + " skewness");
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(stats_values[i][7]));
			res << any;
		}
		if (quantiles.size() > 0) {
			VipAnyResource* any = new VipAnyResource(pool);
			any->setAttribute("XUnit", QString("Time"));
			any->setPath(curve_name + " quantiles");
			if (s.player)
				any->setAttribute("_vip_sourceROI", QVariant::fromValue(s));
			any->setData(QVariant::fromValue(quantiles_values[i]));
			res << any;
		}

		delete extracts[i];
	}

	// restore the VipProcessingPool
	pool->restore();
	pool->read(pool_time);

	return res;
}

#include "VipNDArrayOperations.h"

VipProcessingObject* VipVideoPlayer::extractTimeStatistics()
{
	// the displayed image cannot be a QImage or QPixmap or complex array
	VipNDArrayType<double> array = viewer()->area()->array().toDouble();
	if (array.isEmpty()) {
		VIP_LOG_ERROR("Cannot extract time statistics on color or complex images");
		return nullptr;
	}

	// retrieve the processing pool

	VipDisplayObject* display = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (!display)
		return nullptr;

	// try to retrieve the source VipOutput for this VipDisplayObject
	VipOutput* src_output = nullptr;
	if (VipInput* input = display->inputAt(0))
		if (VipConnectionPtr con = input->connection())
			if (VipOutput* source = con->source())
				src_output = source;

	// a VipProcessingPool which is not of type Resource is mandatory

	VipProcessingPool* pool = display->parentObjectPool();
	if (!pool || !src_output) {
		VIP_LOG_ERROR("Canno find processing pool");
		return nullptr;
	}

	if (pool->deviceType() != VipIODevice::Temporal) {
		VIP_LOG_ERROR("Cannot extract time statistics on non temporal device");
		return nullptr;
	}

	// find all displays within this players, and all their sources
	QList<VipDisplayObject*> displays = this->displayObjects();
	QList<VipProcessingObject*> sources; // all sources
	QList<VipProcessingObject*> leafs;   // last sources before the display
	for (int i = 0; i < displays.size(); ++i) {
		if (VipOutput* src = displays[i]->inputAt(0)->connection()->source())
			if (VipProcessingObject* obj = src->parentProcessing())
				leafs.append(obj);
		sources += displays[i]->allSources();
	}

	// make sure sources and leafs are unique
	sources = vipToSet(sources).values();
	leafs = vipToSet(leafs).values();

	// look into the display object sources for VipIODevice, find if the source is Temporal, Sequential or a Resource
	QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
	// qint64 max_time = VipInvalidTime, min_time = VipInvalidTime;
	VipTimeRange intersect_time = VipInvalidTimeRange;
	VipIODevice::DeviceType type = VipIODevice::Resource;
	for (int i = 0; i < devices.size(); ++i)
		if (devices[i]->deviceType() == VipIODevice::Sequential) {
			// forbidden
			VIP_LOG_ERROR("Cannot extract time statistics on non temporal device");
			return nullptr;
		}
		else if (devices[i]->deviceType() == VipIODevice::Temporal) {
			type = VipIODevice::Temporal;
			// compute devices union time range
			VipTimeRange range = devices[i]->timeLimits();
			if (intersect_time == VipInvalidTimeRange)
				intersect_time = range;
			else
				intersect_time = vipIntersectRange(intersect_time, range);
		}

	if (type != VipIODevice::Temporal) {
		VIP_LOG_ERROR("Cannot extract time statistics on non temporal device");
		return nullptr;
	}

	pool->stop(); // stop playing

	VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);
	progress.setText("Extract time statistics...");

	// now, save the current VipProcessingPool state, because we are going to modify it heavily
	pool->save();

	// disable all processing except the sources, remove the Automatic flag from the sources
	pool->disableExcept(sources);
	foreach (VipProcessingObject* obj, sources) {
		obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	}

	qint64 pool_time = pool->time();
	qint64 time = pool->firstTime();
	if (time < intersect_time.first)
		time = intersect_time.first;
	qint64 end_time = pool->lastTime();
	if (end_time > intersect_time.second)
		end_time = intersect_time.second;

	progress.setRange(time, end_time);

	// block signals
	pool->blockSignals(true);

	// create output arrays
	VipNDArrayType<double> min(array.shape());
	VipNDArrayType<double> max(array.shape());
	VipNDArrayType<double> sum(array.shape());
	VipNDArrayType<double> sum2(array.shape());
	min.fill(std::numeric_limits<double>::max());
	max.fill(-std::numeric_limits<double>::max());
	sum.fill(0);
	sum2.fill(0);
	int count = 0;

	while (time != VipInvalidTime && time <= end_time) {
		progress.setValue(time);

		pool->read(time, true);

		// update all leafs
		for (int i = 0; i < leafs.size(); ++i)
			leafs[i]->update();

		// update statistics
		VipNDArray img = src_output->data().value<VipNDArray>().toDouble();
		if (img.isEmpty() || img.shape() != array.shape()) {
			// empty image or different shape
			VIP_LOG_ERROR("Wrong image at time " + QString::number(time));
			break;
		}

		++count;
		min = vipMin(min, img);
		max = vipMax(max, img);
		sum = sum + img;
		sum2 = sum2 + img * img;

		qint64 next = pool->nextTime(time);
		if (next == time || progress.canceled() || next == VipInvalidTime) {
			break;
		}
		time = next;
	}

	pool->blockSignals(false);

	// compute mean and std

	VipNDArrayType<double> mean = sum / (double)count;
	VipNDArrayType<double> std;
	if (count > 1) {
		VipNDArrayType<double> var = (sum2 - count * mean * mean) / (count - 1);
		std = vipSqrt(var);
	}

	VipMultiNDArray multi;

	multi.addArray("Min", min);
	multi.addArray("Max", max);
	multi.addArray("Mean", mean);
	if (count > 1)
		multi.addArray("Std", std);

	VipAnyResource* any = new VipAnyResource();
	any->setAttribute("Name", this->windowTitle() + " statistics");
	any->setData(QVariant::fromValue(VipNDArray(multi)));

	// restore the VipProcessingPool
	pool->restore();
	pool->read(pool_time);

	return any;
}

static VipValueToTime::TimeType findBestTimeUnit(VipPlotPlayer* w)
{
	// Check if on of the curves have a source sequential device, in which case use Seconds Since Epoch
	QList<VipPlotCurve*> curves = w->plotWidget2D()->area()->findItems<VipPlotCurve*>();
	for (int i = 0; i < curves.size(); ++i) {
		if (VipDisplayObject* display = curves[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QList<VipIODevice*> devices = vipListCast<VipIODevice*>(display->allSources());
			for (int j = 0; j < devices.size(); ++j) {
				if (devices[j]->deviceType() == VipIODevice::Sequential)
					return VipValueToTime::SecondsSE;
			}
		}
	}

	VipScaleDiv div = static_cast<VipPlotArea2D*>(w->plotWidget2D()->area())->bottomAxis()->scaleDiv();
	// VipValueToTime::TimeType current = w->timeType();
	return VipValueToTime::findBestTimeUnit(div.bounds().normalized());
	// return current;
}

/// Function called when the user drag and drop a processing from a VipProcessingObjectMenu in a VipPlotPlayer
static QList<VipProcessingObject*> applyProcessingOnDrop(VipPlotPlayer* pl, const VipProcessingObject::Info& proc)
{
	QList<VipProcessingObject*> res;
	QList<VipPlotItemData*> data = vipCastItemList<VipPlotItemData*>(pl->plotWidget2D()->area()->plotItems(), QString(), 1, 1);

	// check if the processing is a data fusion one
	const QMetaObject* meta = QMetaType(proc.metatype).metaObject();

	while (meta) {
		if (strcmp(meta->className(), "VipBaseDataFusion") == 0)
			break;
		else
			meta = meta->superClass();
	}
	if (meta && !proc.displayHint == VipProcessingObject::InputTransform) {
		// apply data fusion processing if NOT an input transform one
		if (VipProcessingObject* obj = vipCreateDataFusionProcessing(vipCastItemListOrdered<VipPlotItem*>(data), proc))
			res.append(obj);
		else
			return res;
	}
	else {
		// apply the processing to all selected curves
		for (int i = 0; i < data.size(); ++i) {
			VipPlotItemData* item = data[i];
			if (VipDisplayObject* display = item->property("VipDisplayObject").value<VipDisplayObject*>()) {
				if (VipOutput* out = display->inputAt(0)->connection()->source()) {
					if (VipProcessingObject* obj = proc.create()) {
						// check input count
						if (obj->inputCount() != 1) {
							if (VipMultiInput* multi = obj->topLevelInputAt(0)->toMultiInput()) {
								if (!multi->resize(1))
									break;
							}
							else
								break;
						}

						// Input transform: add the processing in a new VipProcessingList
						if (proc.displayHint == VipProcessingObject::InputTransform) {
							VipProcessingList* lst = new VipProcessingList();
							lst->setOverrideName(item->title().text() + " (" + vipSplitClassname(proc.classname) + ")");
							lst->setDeleteOnOutputConnectionsClosed(true);
							lst->append(obj);
							lst->inputAt(0)->setData(out->data());
							lst->update();
							lst->setScheduleStrategy(VipProcessingObject::Asynchronous);
							lst->inputAt(0)->setConnection(out);
							res.append(lst);
						}
						// other kind of processing
						else {
							if (VipProcessingObject* tmp = vipCreateProcessing(out, proc))
								res.append(tmp);
							delete obj;
						}
					}
				}
			}
		}
	}

	if (res.size()) {
		// update the processing editor
		vipGetProcessingEditorToolWidget()->setProcessingObject(res.last());
		if (VipProcessingListEditor* editor = vipGetProcessingEditorToolWidget()->editor()->processingEditor<VipProcessingListEditor*>(res.last())) {
			if (VipProcessingList* lst = qobject_cast<VipProcessingList*>(res.last()))
				if (lst->size())
					editor->selectObject(lst->processings().last());

			vipGetProcessingEditorToolWidget()->editor()->setProcessingObjectVisible(res.last(), true);
			vipGetProcessingEditorToolWidget()->show();
			vipGetProcessingEditorToolWidget()->raise();
		}
	}

	return vipListCast<VipProcessingObject*>(res);
}

VipAnyResource* VipVideoPlayer::extractPolylineValuesAlongTime(const VipShape& shape) const
{

	// the displayed image cannot be a QImage or QPixmap or complex array
	VipNDArray array = viewer()->area()->array();
	if (!array.canConvert<double>()) {
		VIP_LOG_ERROR("Cannot extract time trace on color or complex images");
		return nullptr;
	}
	if (shape.type() != VipShape::Polyline) {
		return nullptr;
	}

	// retrieve the processing pool

	VipDisplayObject* display = spectrogram()->property("VipDisplayObject").value<VipDisplayObject*>();
	if (!display)
		return nullptr;

	// try to retrieve the source VipOutput this VipDisplayObject
	VipOutput* src_output = nullptr;
	if (VipInput* input = display->inputAt(0))
		if (VipConnectionPtr con = input->connection())
			if (VipOutput* source = con->source())
				src_output = source;

	// a VipProcessingPool which is not of type Resource is mandatory

	VipProcessingPool* pool = display->parentObjectPool();
	if (!pool || !src_output)
		return nullptr;

	if (pool->deviceType() == VipIODevice::Resource)
		return nullptr;

	// find all displays within this players, and all their sources
	QList<VipDisplayObject*> displays = this->displayObjects();
	QList<VipProcessingObject*> sources; // all sources
	QList<VipProcessingObject*> leafs;   // last sources before the display
	for (int i = 0; i < displays.size(); ++i) {
		if (VipOutput* src = displays[i]->inputAt(0)->connection()->source())
			if (VipProcessingObject* obj = src->parentProcessing())
				leafs.append(obj);
		sources += displays[i]->allSources();
	}

	// make sure sources and leafs are unique
	sources = vipToSet(sources).values();
	leafs = vipToSet(leafs).values();

	// look into the display object sources for VipIODevice, find if the source is Temporal, Sequential or a Resource
	QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
	VipTimeRange intersect_time = VipInvalidTimeRange;
	VipIODevice::DeviceType type = VipIODevice::Resource;
	for (int i = 0; i < devices.size(); ++i)
		if (devices[i]->deviceType() == VipIODevice::Sequential) {
			// Sequential device has the priority
			type = VipIODevice::Sequential;
			break;
		}
		else if (devices[i]->deviceType() == VipIODevice::Temporal) {
			type = VipIODevice::Temporal;
			// compute devices union time range
			VipTimeRange range = devices[i]->timeLimits();
			if (intersect_time == VipInvalidTimeRange)
				intersect_time = range;
			else
				intersect_time = vipUnionRange(intersect_time, range);
		}

	if (type != VipIODevice::Temporal)
		return nullptr;

	// for Sequential device only:

	// for Temporal device only:
	pool->stop(); // stop playing

	VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);
	progress.setText("Extract time trace...");

	// now, save the current VipProcessingPool state, because we are going to modify it heavily
	pool->save();

	// disable all processing except the sources, remove the Automatic flag from the sources
	pool->disableExcept(sources);
	foreach (VipProcessingObject* obj, sources) {
		obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	}

	// create the VipExtractStatistics object and connect it to the display source object
	VipExtractPolyline* extract = new VipExtractPolyline();
	extract->setShape(shape);
	extract->setLogErrors(QSet<int>());
	src_output->setConnection(extract->inputAt(0));
	extract->inputAt(0)->setData(viewer()->area()->array());
	extract->update();

	// extract the values

	qint64 pool_time = pool->time();
	qint64 time = pool->firstTime();
	if (time < intersect_time.first)
		time = intersect_time.first;
	qint64 end_time = pool->lastTime();
	if (end_time > intersect_time.second)
		end_time = intersect_time.second;
	// qint64 current_time = pool->time();
	int skip = 1;
	progress.setRange(time, end_time);

	// block signals
	// TODO:CHANGESCENEMODEL
	pool->blockSignals(true);

	QVector<VipPointVector> curves;

	while (time != VipInvalidTime && time <= end_time) {
		progress.setValue(time);

		pool->read(time, true);

		// update all leafs
		for (int i = 0; i < leafs.size(); ++i)
			leafs[i]->update();

		// update statistics
		extract->update();
		curves.push_back(extract->outputAt(0)->value<VipPointVector>());

		// skip frames
		bool end_loop = false;
		for (int i = 0; i < skip; ++i) {
			qint64 next = pool->nextTime(time);
			if (next == time || progress.canceled() || next == VipInvalidTime) {
				end_loop = true;
				break;
			}
			time = next;
		}
		if (end_loop)
			break;
	}

	pool->blockSignals(false);
	// restore the VipProcessingPool
	pool->restore();
	pool->read(pool_time);

	// build the result
	if (curves.size()) {
		VipNDArrayType<double> ar(vipVector(curves.first().size(), curves.size()));
		for (int i = 0; i < curves.size(); ++i) {
			double* ptr = ar.ptr(vipVector(0, i));
			const VipPointVector& pts = curves[i];
			for (int y = 0; y < pts.size(); ++y)
				ptr[y * curves.size()] = pts[y].y();
		}

		VipAnyResource* res = new VipAnyResource();
		QString name = shape.name();
		if (name.isEmpty())
			name = shape.identifier();
		res->setAttribute("Name", "Time trace - " + windowTitle() + " - " + name);
		res->setData(QVariant::fromValue(VipNDArray(ar)));
		return res;
	}

	return nullptr;
}

VipShape vipCopyVideoShape(const VipShape& shape, const VipVideoPlayer* src_player, const VipVideoPlayer* dst_player)
{
	VipShape res = shape.copy();

	if (src_player) {
		QTransform tr = src_player->imageTransform();
		if (!tr.isIdentity()) {
			tr = tr.inverted();
			res.transform(tr);
		}
	}
	if (dst_player) {
		QTransform tr = dst_player->imageTransform();
		if (!tr.isIdentity()) {
			res.transform(tr);
		}
	}
	return res;
}

VipSceneModel vipCopyVideoSceneModel(const VipSceneModel& sm, const VipVideoPlayer* src_player, const VipVideoPlayer* dst_player)
{
	QTransform src = src_player ? src_player->imageTransform() : QTransform();
	if (!src.isIdentity())
		src = src.inverted();
	QTransform dst = dst_player ? dst_player->imageTransform() : QTransform();

	VipSceneModel res = sm.copy();

	if (!src.isIdentity() || !dst.isIdentity()) {
		res.shapeSignals()->blockSignals(true);

		VipShapeList lst = res.shapes();
		if (!src.isIdentity()) {
			for (int i = 0; i < lst.size(); ++i)
				lst[i].transform(src);
		}
		if (!dst.isIdentity()) {
			for (int i = 0; i < lst.size(); ++i)
				lst[i].transform(dst);
		}

		res.shapeSignals()->blockSignals(false);
	}
	return res;
}

class VipPlotPlayer::PrivateData
{
public:
	VipAbstractPlotWidget2D* viewer;
	QList<VipAbstractScale*> stdScales;
	VipCoordinateSystem::Type stdType;
	VipValueToTimeButton* timeUnit;
	QCheckBox* displayTimeAsInteger;
	QAction* timeUnitAction;
	QAction* timeMarkerVisible;

	QAction* show_axes_and_grid_action;
	QToolButton* show_axes_and_grid;
	QAction* show_axes;
	QList<QAction*> legendActions;
	// VipLegend * innerLegend;
	Vip::PlayerLegendPosition legendPosition;

	QMenu* deleteItemMenu;
	QToolButton* deleteItem;
	VipDragMenu* selectionItemMenu;
	QToolButton* selectionItem;
	QToolButton* autoScale;
	QToolButton* advancedTools;
	QToolButton* displayVerticalWindow;
	QAction* autoScaleAction;
	QMenu* autoScaleMenu;
	QAction* undoScale;
	QAction* redoScale;
	QAction* normalize;
	QAction* start_zero;
	QAction* start_y_zero;
	QAction* zoom_h;
	QAction* zoom_v;
	QAction* zoom;
	QAction* autoX;
	QAction* autoY;
	QPointer<VipProcessingPool> pool;
	VipPlotMarker* timeMarker;
	VipPlotMarker* xMarker;
	VipPlotShape* verticalWindow;
	VipResizeItem* verticalWindowResize;
	VipSceneModel verticalWindowModel; // the vertical window shape must belong to a VipSceneModel to emit signals
	VipDoubleEdit* plotDuration;
	QAction* plotDurationAction;
	QSpinBox* histBins;
	QAction* histBinsAction;
	VipDisplayCurveEditor* curveEditor;
	QAction* curveEditorAction;

	// list of previous scales, updated with wheel zoom, mouse panning and mouse zooming
	typedef QList<QPair<QPointer<VipAbstractScale>, VipInterval>> scale_state;
	QList<scale_state> prev_scales;

	bool needComputeStartDate;
	bool timeMarkerAlwaysVisible;

	// widget to edit the processing list (if any)
	QAction* processing_tree_action;
	QToolButton* processing_tree_button;
	std::unique_ptr<VipProcessingObjectMenu> processing_menu;

	QAction* fusion_processing_tree_action;
	QToolButton* fusion_processing_tree_button;
	std::unique_ptr<VipProcessingObjectMenu> fusion_processing_menu;
};

VipPlotPlayer::VipPlotPlayer(VipAbstractPlotWidget2D* viewer, QWidget* parent)
  : VipPlayer2D(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->needComputeStartDate = true;
	d_data->timeMarkerAlwaysVisible = VipGuiDisplayParamaters::instance()->alwaysShowTimeMarker();
	// vip_debug("VipPlotPlayer::setTimeMarkerAlwaysVisible :%i\n", (int)d_data->timeMarkerAlwaysVisible);
	d_data->viewer = viewer ? viewer : new VipPlotWidget2D(nullptr, nullptr, _VIP_PLOT_TYPE);
	d_data->viewer->setMinimumSize(100, 100);
	QString className = d_data->viewer->metaObject()->className();
	d_data->viewer->setStyleSheet(className + " {background-color:transparent;}");
	d_data->viewer->area()->boxStyle().setBackgroundBrush(QBrush(Qt::transparent));
	d_data->viewer->setBackgroundBrush(QBrush(Qt::transparent));
	d_data->viewer->scene()->setBackgroundBrush(QBrush(Qt::transparent));
	d_data->viewer->area()->setMousePanning(Qt::RightButton);
	d_data->viewer->area()->setMouseWheelZoom(true);
	// TEST
	d_data->viewer->area()->setMouseSelectionAndZoom(true);
	// d_data->viewer->area()->setMouseZoomSelection(Qt::LeftButton);
	//
	d_data->viewer->area()->setPlotToolTip(new VipToolTip());
	d_data->viewer->area()->grid()->setHoverEffect();
	d_data->viewer->area()->canvas()->setFlag(QGraphicsItem::ItemIsSelectable, false);
	d_data->viewer->setMouseTracking(true);

	if (VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer)) {
		pl->area()->bottomAxis()->scaleDraw()->enableLabelOverlapping(false);
		pl->area()->topAxis()->scaleDraw()->enableLabelOverlapping(false);
	}

	// Get standard scales and coordinate system
	QList<VipAbstractScale*> stdScales;
	VipCoordinateSystem::Type stdType = d_data->viewer->area()->standardScales(stdScales);
	d_data->stdScales = stdScales;
	d_data->stdType = stdType;

	// specific tool bar actions for plot area

	d_data->show_axes_and_grid = new QToolButton();
	d_data->show_axes_and_grid->setIcon(vipIcon("show_legend.png"));
	d_data->show_axes_and_grid->setToolTip("Show/hide grid and/or legend");
	d_data->show_axes_and_grid->setAutoRaise(true);
	QMenu* menu = new QMenu();
	d_data->show_axes_and_grid->setMenu(menu);
	d_data->show_axes_and_grid->setPopupMode(QToolButton::InstantPopup);
	this->toolBar()->addWidget(d_data->show_axes_and_grid);

	d_data->show_axes = menu->addAction("Show/hide axes grid");
	d_data->show_axes->setCheckable(true);
	d_data->show_axes->setChecked(true);
	menu->addSeparator();

	d_data->legendPosition = Vip::LegendBottom;
	d_data->legendActions << menu->addAction("Hide legend");
	d_data->legendActions << menu->addAction(vipIcon("blegend.png"), "Show legend bottom");
	d_data->legendActions << menu->addAction(vipIcon("inner_tllegend.png"), "Show inner legend top left");
	d_data->legendActions << menu->addAction(vipIcon("inner_trlegend.png"), "Show inner legend top right");
	d_data->legendActions << menu->addAction(vipIcon("inner_bllegend.png"), "Show inner legend bottom left");
	d_data->legendActions << menu->addAction(vipIcon("inner_brlegend.png"), "Show inner legend bottom right");

	for (int i = 0; i < d_data->legendActions.size(); ++i) {
		d_data->legendActions[i]->setCheckable(true);
		if (i == 1)
			d_data->legendActions[i]->setChecked(true);
		d_data->legendActions[i]->setProperty("position", i);
		connect(d_data->legendActions[i], SIGNAL(triggered(bool)), this, SLOT(legendTriggered()));
	}

	d_data->deleteItemMenu = new QMenu();
	d_data->deleteItem = new QToolButton();
	d_data->deleteItem->setToolTip(tr("<b>Remove item</b><br>Remove one or more plot items"));
	d_data->deleteItem->setIcon(vipIcon("del.png"));
	d_data->deleteItem->setAutoRaise(true);
	d_data->deleteItem->setMenu(d_data->deleteItemMenu);
	d_data->deleteItem->setPopupMode(QToolButton::InstantPopup);
	this->toolBar()->addWidget(d_data->deleteItem);

	connect(d_data->deleteItemMenu, SIGNAL(aboutToShow()), this, SLOT(computeDeleteMenu()));
	connect(d_data->deleteItemMenu, SIGNAL(triggered(QAction*)), this, SLOT(deleteItem(QAction*)));

	d_data->selectionItemMenu = new VipDragMenu();
	d_data->selectionItem = new QToolButton();
	d_data->selectionItem->setToolTip(tr("Show/hide item"));
	d_data->selectionItem->setIcon(vipIcon("select.png"));
	d_data->selectionItem->setAutoRaise(true);
	d_data->selectionItem->setMenu(d_data->selectionItemMenu);
	d_data->selectionItem->setPopupMode(QToolButton::InstantPopup);
	this->toolBar()->addWidget(d_data->selectionItem);

	connect(d_data->selectionItemMenu, SIGNAL(aboutToShow()), this, SLOT(computeSelectionMenu()));
	connect(d_data->selectionItemMenu, SIGNAL(triggered(QAction*)), this, SLOT(selectItem(QAction*)));

	d_data->autoScale = new QToolButton();
	d_data->autoScale->setIcon(vipIcon("axises.png"));
	d_data->autoScale->setToolTip("<b>Auto scale</b><br>Adjust the scales so that all plot items (curves, histograms, ...) fit into the window.");
	d_data->autoScale->setAutoRaise(true);
	d_data->autoScale->setCheckable(true);
	d_data->autoScale->setChecked(true);
	d_data->autoScaleMenu = new QMenu();
	d_data->undoScale = d_data->autoScaleMenu->addAction(vipIcon("undo.png"), "Undo last scale change");
	d_data->redoScale = d_data->autoScaleMenu->addAction(vipIcon("redo.png"), "Redo last scale change");

	d_data->autoScaleMenu->addSeparator();
	d_data->autoX = d_data->autoScaleMenu->addAction(vipIcon("x_axis.png"), "Auto scale X axis only");
	d_data->autoY = d_data->autoScaleMenu->addAction(vipIcon("y_axis.png"), "Auto scale Y axis only");

	d_data->autoScale->setMenu(d_data->autoScaleMenu);
	d_data->autoScale->setPopupMode(QToolButton::MenuButtonPopup);
	d_data->autoScaleAction = this->toolBar()->addWidget(d_data->autoScale);
	connect(d_data->autoScaleMenu, SIGNAL(aboutToShow()), this, SLOT(undoMenuShow()));
	// enable undo/redo scale
	d_data->viewer->area()->setTrackScalesStateEnabled(true);

	QToolButton* applyScaleToAll = new QToolButton();
	applyScaleToAll->setIcon(vipIcon("axises_to_all.png"));
	applyScaleToAll->setAutoRaise(true);
	applyScaleToAll->setToolTip("Apply this player x/y scale to all other players within the current workspace");
	applyScaleToAll->setPopupMode(QToolButton::InstantPopup);
	applyScaleToAll->setMenu(new QMenu());
	toolBar()->addWidget(applyScaleToAll);
	QAction* x_scale = applyScaleToAll->menu()->addAction("Apply X scale to all");
	x_scale->setToolTip("Apply the x (or time) scale to all other plot players within this workspace");
	QAction* y_scale = applyScaleToAll->menu()->addAction("Apply Y scale to all");
	y_scale->setToolTip("Apply the y scale to all other plot players within this workspace.\n"
			    "Note that this will only work for other scale with the same unit.");
	connect(x_scale, SIGNAL(triggered(bool)), this, SLOT(xScaleToAll()));
	connect(y_scale, SIGNAL(triggered(bool)), this, SLOT(yScaleToAll()));

	QToolButton* bzoom = new QToolButton();
	bzoom->setAutoRaise(true);
	bzoom->setIcon(vipIcon("zoom.png"));
	bzoom->setToolTip("Zoom options");
	bzoom->setMenu(new QMenu());
	bzoom->setPopupMode(QToolButton::InstantPopup);
	d_data->zoom = this->toolBar()->addWidget(bzoom);
	d_data->zoom->setToolTip("Zoom options");
	bzoom->menu()->setToolTipsVisible(true);
	d_data->zoom_h = bzoom->menu()->addAction( // vipIcon("zoom_h.png"),
	  "Allow X zooming");
	d_data->zoom_h->setToolTip("Enable zooming on the x (time) axis using the wheel.<br>This will also enable mouse panning on the x (time) axis.");
	d_data->zoom_h->setCheckable(true);
	d_data->zoom_h->setChecked(true);
	d_data->zoom_v = bzoom->menu()->addAction( // vipIcon("zoom_v.png"),
	  "Allow Y zooming");
	d_data->zoom_v->setToolTip("Enable zooming on the y axis using the wheel.<br>This will also enable mouse panning on the y axis.");
	d_data->zoom_v->setCheckable(true);
	d_data->zoom_v->setChecked(true);

	this->toolBar()->addSeparator();

	// Detect dark skin
	QColor c = vipWidgetTextBrush(vipGetMainWindow()).color();
	bool dark_skin = (c.redF() > 0.9 && c.greenF() > 0.9 && c.blueF() > 0.9);

	d_data->timeMarker = new VipPlotMarker();
	d_data->timeMarker->setLineStyle(VipPlotMarker::VLine);
	d_data->timeMarker->setLinePen(Qt::red, 1);
	d_data->timeMarker->setIgnoreStyleSheet(true);
	if (dark_skin) {
		QColor light_red = VipColorPalette(VipLinearColorMap::ColorPaletteRandom).lighter(VipGuiDisplayParamaters::instance()->itemPaletteFactor()).color(1);
		d_data->timeMarker->setLinePen(light_red, 1);
	}
	d_data->timeMarker->setZValue(1000);
	d_data->timeMarker->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
	d_data->timeMarker->setItemAttribute(VipPlotItem::AutoScale, false);
	d_data->timeMarker->setRenderHints(QPainter::TextAntialiasing);
	VipText label;
	label.setBackgroundBrush(Qt::red);
	label.setTextPen(QPen(Qt::white));
	d_data->timeMarker->setLabel(label);

	d_data->timeMarker->setAxes(stdScales, stdType);

	d_data->timeMarker->setVisible(false);
	// disable ClipToScaleRect for stacked plots
	d_data->timeMarker->setItemAttribute(VipPlotItem::ClipToScaleRect, false);

	d_data->xMarker = new VipPlotMarker();
	d_data->xMarker->setLineStyle(VipPlotMarker::VLine);
	d_data->xMarker->setIgnoreStyleSheet(true);
	QPen xpen(Qt::red);
	xpen.setWidth(1);
	if (dark_skin) {
		QColor light_red = VipColorPalette(VipLinearColorMap::ColorPaletteRandom).lighter(VipGuiDisplayParamaters::instance()->itemPaletteFactor()).color(1);
		xpen.setColor(light_red);
	}
	d_data->xMarker->setLinePen(xpen);
	d_data->xMarker->setZValue(1000);
	d_data->xMarker->setRenderHints(QPainter::TextAntialiasing);
	d_data->xMarker->setItemAttribute(VipPlotItem::AutoScale, false);
	d_data->xMarker->setAxes(stdScales, stdType);
	d_data->xMarker->setVisible(false);
	d_data->xMarker->setItemAttribute(VipPlotItem::IgnoreMouseEvents);
	// disable ClipToScaleRect for stacked plots
	d_data->xMarker->setItemAttribute(VipPlotItem::ClipToScaleRect, false);

	d_data->timeUnit = new VipValueToTimeButton();
	d_data->timeUnitAction = toolBar()->addWidget(d_data->timeUnit);

	d_data->timeMarkerVisible =
	  this->toolBar()->addAction(vipIcon("time.png"), "<b>Show/hide time marker</b><br>Display a vertical time marker that represents the current time in this workspace.");
	d_data->timeMarkerVisible->setCheckable(true);
	d_data->timeMarkerVisible->setVisible(false);

	this->toolBar()->addSeparator();

	d_data->advancedTools = new QToolButton();
	d_data->advancedTools->setMenu(new VipDragMenu(d_data->advancedTools));
	d_data->advancedTools->setAutoRaise(true);
	d_data->advancedTools->setToolTip("Advanced tools");
	d_data->advancedTools->setPopupMode(QToolButton::InstantPopup);
	d_data->advancedTools->setIcon(vipIcon("scaletools.png"));
	d_data->advancedTools->menu()->setToolTipsVisible(true);
	this->toolBar()->addWidget(d_data->advancedTools);

	d_data->normalize = d_data->advancedTools->menu()->addAction(vipIcon("normalize.png"), "Normalize curves");
	d_data->normalize->setToolTip("<b>Normalize curves</b><br>Normalize all curves/histograms between 0 and 1.");
	d_data->normalize->setCheckable(true);
	d_data->start_zero = d_data->advancedTools->menu()->addAction(vipIcon("align_left.png"), "Align curves to zero");
	d_data->start_zero->setToolTip("<b>Align curves to zero</b><br>Set all curves minimum abscissa to 0.");
	d_data->start_zero->setCheckable(true);
	d_data->start_zero->setObjectName("x_zero");
	d_data->start_y_zero = d_data->advancedTools->menu()->addAction(vipIcon("align_bottom.png"), "Align curves Y values to zero");
	d_data->start_y_zero->setToolTip("<b>Align curves Y values to zero</b><br>Set all curves minimum ordinate to 0.");
	d_data->start_y_zero->setCheckable(true);
	d_data->start_y_zero->setObjectName("y_zero");

	//
	// tool button to add new processing
	//

	d_data->processing_tree_button = new QToolButton();
	d_data->processing_tree_button->setAutoRaise(true);
	d_data->processing_tree_button->setToolTip("Add a new processing for the selected curves");
	d_data->processing_tree_button->setIcon(vipIcon("PROCESSING.png"));
	d_data->processing_tree_button->setPopupMode(QToolButton::InstantPopup);

	d_data->processing_menu.reset(new VipProcessingObjectMenu());
	d_data->processing_tree_button->setMenu(d_data->processing_menu.get());
	// d_data->processing_menu->setProcessingInfos(VipProcessingList::validProcessingObjects(QVariant::fromValue(VipPointVector())));
	d_data->processing_tree_action = toolBar()->addWidget(d_data->processing_tree_button);
	d_data->processing_tree_action->setVisible(false);
	connect(d_data->processing_menu.get(), SIGNAL(selected(const VipProcessingObject::Info&)), this, SLOT(addSelectedProcessing(const VipProcessingObject::Info&)));

	d_data->fusion_processing_tree_button = new QToolButton();
	d_data->fusion_processing_tree_button->setAutoRaise(true);
	d_data->fusion_processing_tree_button->setToolTip("Create a data fusion processing for the selected curves");
	d_data->fusion_processing_tree_button->setIcon(vipIcon("processing_merge.png"));
	d_data->fusion_processing_tree_button->setPopupMode(QToolButton::InstantPopup);

	d_data->fusion_processing_menu.reset( new VipProcessingObjectMenu());
	d_data->fusion_processing_tree_button->setMenu(d_data->fusion_processing_menu.get());
	d_data->fusion_processing_tree_action = toolBar()->addWidget(d_data->fusion_processing_tree_button);
	d_data->fusion_processing_tree_action->setVisible(false);
	connect(d_data->fusion_processing_menu.get(), SIGNAL(selected(const VipProcessingObject::Info&)), this, SLOT(addSelectedProcessing(const VipProcessingObject::Info&)));

	d_data->displayVerticalWindow = new QToolButton();
	d_data->displayVerticalWindow->setIcon(vipIcon("vwindow.png"));
	d_data->displayVerticalWindow->setToolTip("Display/hide the vertical window");
	d_data->displayVerticalWindow->setAutoRaise(true);
	d_data->displayVerticalWindow->setCheckable(true);
	d_data->displayVerticalWindow->setChecked(false);
	QMenu* vwindow_menu = new QMenu();
	QAction* reset_vertical_window = vwindow_menu->addAction("Reset vertical window");
	d_data->displayVerticalWindow->setMenu(vwindow_menu);
	d_data->displayVerticalWindow->setPopupMode(QToolButton::MenuButtonPopup);
	this->toolBar()->addWidget(d_data->displayVerticalWindow);
	connect(d_data->displayVerticalWindow, SIGNAL(clicked(bool)), this, SLOT(setDisplayVerticalWindow(bool)));
	connect(reset_vertical_window, SIGNAL(triggered(bool)), this, SLOT(resetVerticalWindow()));

	d_data->plotDuration = new VipDoubleEdit();
	d_data->plotDuration->setSuffix(" s ");
	d_data->plotDuration->setMaximumWidth(80);
	d_data->plotDuration->setToolTip("<b>Sliding time window (s)</b><br>"
					 "For streaming curves, set their sliding time window in seconds. The curves will be clamped in order to never exceed this maximum duration. Set a negative "
					 "value to disable the sliding window.");
	d_data->plotDurationAction = toolBar()->addWidget(d_data->plotDuration);
	d_data->plotDurationAction->setVisible(false);
	connect(d_data->plotDuration, SIGNAL(valueChanged(double)), this, SLOT(setSlidingTimeWindow()));

	d_data->histBins = new QSpinBox();
	d_data->histBins->setRange(0, INT_MAX);
	d_data->histBins->setToolTip("Histogram bins number");
	d_data->histBinsAction = toolBar()->addWidget(d_data->histBins);
	d_data->histBinsAction->setVisible(false);
	connect(d_data->histBins, SIGNAL(valueChanged(int)), this, SLOT(histBinsChanged(int)));

	d_data->curveEditor = new VipDisplayCurveEditor();
	d_data->curveEditorAction = this->toolBar()->addWidget(d_data->curveEditor);
	d_data->curveEditorAction->setVisible(false);

	this->setPlotWidget2D(d_data->viewer);

	d_data->viewer->area()->legend()->setCheckState(VipLegend::CheckableSelection);

	// add inner legend
	VipLegend* innerLegend = new VipLegend();
	innerLegend->layout()->setMaxColumns(1);
	innerLegend->setProperty("position", 1);
	innerLegend->setVisible(false);

	// Replace with stdScales
	d_data->viewer->area()->addInnerLegend(innerLegend, stdScales[1], Qt::AlignTop | Qt::AlignRight, 5);

	// hide top labels
	if (VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer))
		pl->area()->topAxis()->scaleDraw()->enableComponent(VipAbstractScaleDraw::Labels, false);

	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(d_data->viewer->area()))
		area->leftMultiAxis()->setItemIntervalFactor(0.05);
	stdScales[1]->scaleDraw()->valueToText()->setAutomaticExponent(true);
	stdScales[1]->scaleDraw()->valueToText()->setMaxLabelSize(3);
	stdScales[0]->setMaxMajor(15); // set more ticks to the bottom (time) axis
	stdScales[0]->setOptimizeFromStreaming(true);

	// Setup the vertical window
	VipPlotShape* sh = new VipPlotShape();
	sh->setIgnoreStyleSheet(true);
	sh->setDrawComponent(VipPlotShape::Title, false);
	VipText sh_text = sh->title();
	sh_text.textStyle().setTextPen(Qt::NoPen);
	sh->setTitle(sh_text);
	sh->setPen(QPen(Qt::transparent));
	QColor cinner = QColor(255, 0, 0).lighter(170);
	cinner.setAlpha(50);
	sh->setBrush(QBrush(cinner));
	cinner.setAlpha(255);
	sh->setPen(QPen(cinner));
	VipShape _sh = VipShape(QRectF());
	_sh.setName("Vertical window");
	d_data->verticalWindowModel.add(_sh);
	sh->setRawData(_sh);
	sh->setTextPosition(Vip::Inside);
	sh->setTextAlignment(Qt::AlignBottom | Qt::AlignHCenter);
	sh->setTextDistance(10);
	sh->setItemAttribute(VipPlotItem::HasToolTip, false);
	VipResizeItem* r = new VipResizeItem();
	r->setIgnoreStyleSheet(true);
	r->setManagedItems(PlotItemList() << sh);
	{
		QList<VipAbstractScale*> scales;
		auto system = this->plotWidget2D()->area()->standardScales(scales);
		r->setAxes(scales, system);
	}
	r->setLibertyDegrees(VipResizeItem::HorizontalMove | VipResizeItem::HorizontalResize | VipResizeItem::ExpandVertical);
	r->setExpandToFullArea(true);
	r->setPen(QPen(Qt::transparent));
	r->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	sh->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	r->setZValue(1000);
	sh->setZValue(1000);
	r->setItemAttribute(VipPlotItem::IsSuppressable, false);
	sh->setItemAttribute(VipPlotItem::IsSuppressable, false);
	sh->setVisible(false);
	d_data->verticalWindow = sh;
	d_data->verticalWindowResize = r;
	// Set resizer shape
	QPainterPath resizer;
	resizer.addPolygon(QPolygonF() << QPointF(-10, 0) << QPointF(0, 10) << QPointF(0, -10));
	r->setCustomLeftResizer(resizer);
	r->setCustomRightResizer(QTransform().rotate(180).map(resizer));
	r->setResizerBrush(QBrush(cinner));
	cinner.setAlpha(100);
	r->setResizerPen(QPen(cinner));

	r->setProperty("_vip_no_serialize", true);
	sh->setProperty("_vip_no_serialize", true);

	VipPlayerToolTip::setDefaultToolTipFlags((VipToolTip::DisplayFlags)VipToolTip::All & VipToolTip::DisplayFlags((~VipToolTip::SearchXAxis) & (~VipToolTip::SearchYAxis) & (~VipToolTip::Axes)),
						 &VipPlotPlayer::staticMetaObject);
	VipToolTip* tip = new VipToolTip();
	tip->setDistanceToPointer(20);
	tip->setDisplayFlags(VipPlayerToolTip::toolTipFlags(&VipPlotPlayer::staticMetaObject));
	d_data->viewer->area()->setPlotToolTip(tip);
	tip->setDelayTime(5000);
	tip->addIgnoreProperty("Date");
	tip->addIgnoreProperty("Name");

	toolTipFlagsChanged(VipPlayerToolTip::toolTipFlags(&VipPlotPlayer::staticMetaObject));

	this->plotSceneModel()->setDrawComponent("All", VipPlotShape::FillPixels, false);

	// optimize plotting
	// NEWPLOT
	// d_data->viewer->area()->setUpdater(new VipPlotAreaUpdater());

	connect(d_data->show_axes, SIGNAL(triggered(bool)), this, SLOT(showGrid(bool)));
	connect(d_data->autoScale, SIGNAL(clicked(bool)), this, SLOT(setAutoScale(bool)));
	connect(d_data->viewer->area(), SIGNAL(autoScaleChanged(bool)), this, SLOT(setAutoScale(bool)));
	connect(d_data->undoScale, SIGNAL(triggered(bool)), d_data->viewer->area(), SLOT(undoScalesState()));
	connect(d_data->redoScale, SIGNAL(triggered(bool)), d_data->viewer->area(), SLOT(redoScalesState()));

	connect(d_data->normalize, SIGNAL(triggered(bool)), this, SLOT(normalize(bool)));
	connect(d_data->start_zero, SIGNAL(triggered(bool)), this, SLOT(startAtZero(bool)));
	connect(d_data->start_y_zero, SIGNAL(triggered(bool)), this, SLOT(startYAtZero(bool)));
	connect(d_data->zoom_h, SIGNAL(triggered(bool)), this, SLOT(computeZoom()));
	connect(d_data->zoom_v, SIGNAL(triggered(bool)), this, SLOT(computeZoom()));
	connect(d_data->timeUnit, SIGNAL(timeUnitChanged()), this, SLOT(timeUnitChanged()));
	connect(d_data->timeMarkerVisible, SIGNAL(triggered(bool)), this, SLOT(setTimeMarkerVisible(bool)));
	connect(d_data->autoX, SIGNAL(triggered(bool)), this, SLOT(autoScaleX()));
	connect(d_data->autoY, SIGNAL(triggered(bool)), this, SLOT(autoScaleY()));

	connect(d_data->viewer->area(), SIGNAL(toolTipStarted(const QPointF&)), this, SLOT(toolTipStarted(const QPointF&)));
	connect(d_data->viewer->area(), SIGNAL(toolTipMoved(const QPointF&)), this, SLOT(toolTipMoved(const QPointF&)));
	connect(d_data->viewer->area(), SIGNAL(toolTipEnded(const QPointF&)), this, SLOT(toolTipEnded(const QPointF&)));
	connect(d_data->viewer->area(), SIGNAL(itemDataChanged(VipPlotItem*)), this, SLOT(refreshToolTip(VipPlotItem*)));
	// connect(d_data->viewer->area(), SIGNAL(itemDataChanged(VipPlotItem*)), this, SLOT(delayedComputeStartDate()), Qt::DirectConnection);

	VipUniqueId::id(this);

	QStyle* style = (qApp->style());
	style->polish(this);

	// apply the default settings
	VipGuiDisplayParamaters::instance()->apply(this);

	VipPlayerLifeTime::emitCreated(this);
}

VipPlotPlayer::~VipPlotPlayer()
{
	QCoreApplication::instance()->removePostedEvents(this, QEvent::MetaCall);
	// disconnect this signal as it could be sent longer after the player is destroyed
	disconnect(d_data->viewer->area(), SIGNAL(toolTipEnded(const QPointF&)), this, SLOT(toolTipEnded(const QPointF&)));
	VipPlayerLifeTime::emitDestroyed(this);
}

void VipPlotPlayer::timeUnitChanged()
{
	VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer);
	if (!pl)
		return;

	// find the time unit
	VipValueToTime::TimeType type = d_data->timeUnit->valueToTime();
	if (d_data->timeUnit->automaticUnit()) {
		this->setProperty("_vip_forceTimeUnit", false);
		type = VipValueToTime::findBestTimeUnit(itemsInterval());
		d_data->timeUnit->setValueToTime(type);
		return;
	}
	else
		this->setProperty("_vip_forceTimeUnit", true);

	// create the VipValueToTime for top and bottom axes
	VipValueToTime* bottom = d_data->timeUnit->currentValueToTime().copy();
	VipValueToTime* top = bottom->copy();
	bottom->type = type;
	top->type = type;
	top->drawAdditionalText = false;

	// modify scale draw if displaying times as integers or absolute date time
	if ((bottom->displayType == VipValueToTime::Integer) || bottom->displayType == VipValueToTime::AbsoluteDateTime) {
		VipTextStyle st = pl->area()->bottomAxis()->scaleDraw()->textStyle();
		st.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		pl->area()->bottomAxis()->scaleDraw()->setTextStyle(st);
		pl->area()->bottomAxis()->scaleDraw()->setlabelRotation(45, VipScaleDiv::MajorTick);
	}
	else {
		VipTextStyle st = pl->area()->bottomAxis()->scaleDraw()->textStyle();
		st.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		pl->area()->bottomAxis()->scaleDraw()->setTextStyle(st);
		pl->area()->bottomAxis()->scaleDraw()->setlabelRotation(0, VipScaleDiv::MajorTick);
	}

	// import current text styles
	// top->setTextStyles(*pl->area()->topAxis()->scaleDraw()->valueToText());
	// bottom->setTextStyles(*pl->area()->bottomAxis()->scaleDraw()->valueToText());

	// set the new VipValueToTime to top and bottom axes
	pl->area()->bottomAxis()->scaleDraw()->setValueToText(bottom);
	pl->area()->topAxis()->scaleDraw()->setValueToText(top);

	// change the scale engine
	if (d_data->timeUnit->currentValueToTime().type % 2) {
		VipDateTimeScaleEngine* b_engine = new VipDateTimeScaleEngine();
		b_engine->setValueToTime(bottom);
		VipDateTimeScaleEngine* t_engine = new VipDateTimeScaleEngine();
		t_engine->setValueToTime(top);
		pl->area()->bottomAxis()->setScaleEngine(b_engine);
		pl->area()->topAxis()->setScaleEngine(t_engine);
	}
	else {
		pl->area()->bottomAxis()->setScaleEngine(new VipLinearScaleEngine());
		pl->area()->topAxis()->setScaleEngine(new VipLinearScaleEngine());
	}

	// TEST: added this instead of inside computeStartDate()
	// change value to time
	// VipValueToTime * vt = static_cast<VipValueToTime*>(d_data->viewer->area()->bottomAxis()->constScaleDraw()->valueToText());
	//  if (d_data->timeUnit->currentValueToTime().type % 2 &&
	//  vt->valueToTextType() == VipValueToText::ValueToTime &&
	//  vt->displayType != VipValueToTime::AbsoluteDateTime)
	//  {
	//  if (d_data->timeUnit->currentValueToTime().fixedStartValue) {
	//  vt->fixedStartValue = true;
	//  vt->startValue = d_data->viewer->area()->bottomAxis()->itemsInterval().minValue();
	//  }
	//  else
	//  {
	//  vt->fixedStartValue = false;
	//  VipInterval inter = d_data->viewer->area()->bottomAxis()->scaleDiv().bounds();
	//  vt->startValue = inter.minValue();
	//  }
	//  //reset scale
	//  if (!isAutoScale())
	//  {
	//  VipInterval inter = d_data->viewer->area()->bottomAxis()->scaleDiv().bounds();
	//  d_data->viewer->area()->bottomAxis()->setScale(inter.minValue(), inter.maxValue());
	//  }
	//  }
	//  else
	//  {
	//  if (vt->valueToTextType() == VipValueToText::ValueToTime)
	//  {
	//  vt->fixedStartValue = false;
	//  VipInterval inter = d_data->viewer->area()->bottomAxis()->scaleDiv().bounds();
	//  vt->startValue = inter.minValue();
	//  }
	//  }

	timeChanged();
	computeStartDate();

	// if auto scale, force to recompute the scale
	if (viewer()->area()->isAutoScale()) {
		viewer()->area()->setAutoScale(false);
		viewer()->area()->setAutoScale(true);
	}

	viewer()->recomputeGeometry();

	Q_EMIT timeUnitChanged(timeUnit());
}

void VipPlotPlayer::delayedComputeStartDate()
{
	computeStartDate();
	// if (d_data->needComputeStartDate)
	//  {
	//  d_data->needComputeStartDate = false;
	//  QMetaObject::invokeMethod(this, "computeStartDate", Qt::QueuedConnection);
	//  }
}

QString VipPlotPlayer::formatXValue(vip_double value) const
{
	VipValueToText* v = defaultXAxis()->constScaleDraw()->valueToText();
	return v->convert(value, VipScaleDiv::MajorTick);
}

QString VipPlotPlayer::timeUnit() const
{
	if (!haveTimeUnit())
		return QString();
	else {
		VipValueToText* v = defaultXAxis()->scaleDraw()->valueToText();
		if (v->valueToTextType() == VipValueToText::ValueToTime) {
			VipValueToTime* vt = static_cast<VipValueToTime*>(v);
			if (vt->displayType == VipValueToTime::AbsoluteDateTime)
				return QString();
			return vt->timeUnit();
		}
		return QString();
	}
}

qint64 VipPlotPlayer::timeFactor() const
{
	if (!haveTimeUnit())
		return 1;
	else {
		VipValueToText* v = defaultXAxis()->scaleDraw()->valueToText();
		if (v->valueToTextType() == VipValueToText::ValueToTime) {
			VipValueToTime* vt = static_cast<VipValueToTime*>(v);
			if (vt->displayType == VipValueToTime::AbsoluteDateTime)
				return 1;
			else {
				if (vt->type == VipValueToTime::NanoSeconds || vt->type == VipValueToTime::NanoSecondsSE)
					return 1;
				else if (vt->type == VipValueToTime::MicroSeconds || vt->type == VipValueToTime::MicroSecondsSE)
					return 1000;
				else if (vt->type == VipValueToTime::MilliSeconds || vt->type == VipValueToTime::MilliSecondsSE)
					return 1000000;
				else if (vt->type == VipValueToTime::Seconds || vt->type == VipValueToTime::SecondsSE)
					return 1000000000;
			}
		}
		else
			return 1;
	}
	return 1;
}

void VipPlotPlayer::computeStartDate()
{
	// For date time x axis (since epoch), compute the start date of the union of all plot items.
	// Then, set this date to the bottom and top VipValueToTime.
	// This will prevent the start date (displayed at the bottom left corner) to change
	// when zooming or mouse panning.

	VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer);
	if (!pl)
		return;

	VipValueToText* v = pl->area()->bottomAxis()->constScaleDraw()->valueToText();
	VipValueToTime* vt = static_cast<VipValueToTime*>(pl->area()->bottomAxis()->constScaleDraw()->valueToText());
	VipValueToTime* vb = static_cast<VipValueToTime*>(pl->area()->topAxis()->constScaleDraw()->valueToText());

	if (d_data->timeUnit->currentValueToTime().type % 2 && v->valueToTextType() == VipValueToText::ValueToTime && vt->displayType != VipValueToTime::AbsoluteDateTime) {
		if (d_data->timeUnit->currentValueToTime().fixedStartValue) {
			// vt->fixedStartValue = true;
			vb->fixedStartValue = true;
			VipInterval inter = pl->area()->bottomAxis()->itemsInterval();
			// vt->startValue = inter.minValue();
			vb->startValue = inter.minValue();
		}
		else {
			// vt->fixedStartValue = false;
			vb->fixedStartValue = false;
			VipInterval inter = pl->area()->bottomAxis()->scaleDiv().bounds();
			// vt->startValue = inter.minValue();
			vb->startValue = inter.minValue();
		}

		// reset scale
		if (!isAutoScale()) {
			VipInterval inter = pl->area()->bottomAxis()->scaleDiv().bounds();
			pl->area()->bottomAxis()->setScale(inter.minValue(), inter.maxValue());
		}
	}
	else {
		if (v->valueToTextType() == VipValueToText::ValueToTime) {
			vt->fixedStartValue = false;
			vb->fixedStartValue = false;
			VipInterval inter = pl->area()->bottomAxis()->scaleDiv().bounds();
			vt->startValue = inter.minValue();
			vb->startValue = inter.minValue();
		}
	}
	d_data->needComputeStartDate = true;
}

bool VipPlotPlayer::isAutoScale() const
{
	return viewer()->area()->isAutoScale();
}

bool VipPlotPlayer::isHZoomEnabled() const
{
	return d_data->zoom_h->isChecked();
}
bool VipPlotPlayer::isVZoomEnabled() const
{
	return d_data->zoom_v->isChecked();
}

void VipPlotPlayer::enableHZoom(bool enable)
{
	d_data->zoom_h->blockSignals(true);
	d_data->zoom_h->setChecked(enable);
	d_data->zoom_h->blockSignals(false);
	computeZoom();
}
void VipPlotPlayer::enableVZoom(bool enable)
{
	d_data->zoom_v->blockSignals(true);
	d_data->zoom_v->setChecked(enable);
	d_data->zoom_v->blockSignals(false);
	computeZoom();
}

void VipPlotPlayer::xScaleToAll()
{
	// get the player x scale
	VipInterval interval = xScale()->scaleDiv().bounds().normalized();
	if (VipDisplayPlayerArea* workspace = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		// grab all VipPlotPlayer within this workspace
		QList<VipPlotPlayer*> pls = workspace->findChildren<VipPlotPlayer*>();
		for (int i = 0; i < pls.size(); ++i) {
			VipPlotPlayer* pl = pls[i];
			if (pl != this && pl->haveTimeUnit()) {
				pl->setAutoScale(false);
				pl->xScale()->setScale(interval.minValue(), interval.maxValue());
			}
		}
	}
}
void VipPlotPlayer::yScaleToAll()
{
	// get the player y scales
	QMap<QString, VipInterval> intervals;
	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(d_data->viewer->area())) {
		for (int i = 0; i < area->leftMultiAxis()->count(); ++i) {
			intervals[area->leftMultiAxis()->at(i)->title().text()] = area->leftMultiAxis()->at(i)->scaleDiv().bounds().normalized();
		}
	}
	if (intervals.size()) {
		if (VipDisplayPlayerArea* workspace = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
			// grab all VipPlotPlayer within this workspace
			QList<VipPlotPlayer*> pls = workspace->findChildren<VipPlotPlayer*>();
			for (int i = 0; i < pls.size(); ++i) {
				VipPlotPlayer* pl = pls[i];
				if (pl != this && pl->haveTimeUnit()) {
					pl->setAutoScale(false);
					if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(pl->plotWidget2D()->area())) {
						for (int j = 0; j < area->leftMultiAxis()->count(); ++j) {
							QMap<QString, VipInterval>::iterator it = intervals.find(area->leftMultiAxis()->at(j)->title().text());
							if (it != intervals.end())
								area->leftMultiAxis()->at(j)->setScale(it.value().minValue(), it.value().maxValue());
						}
					}
				}
			}
		}
	}
}

void VipPlotPlayer::computeZoom()
{
	QList<VipAbstractScale*> scales;
	VipCoordinateSystem::Type t = d_data->viewer->area()->standardScales(scales);

	if (t == VipCoordinateSystem::Cartesian) {
		// currently only cartesian systems are supported
		scales = d_data->viewer->area()->scales();
		for (int i = 0; i < scales.size(); ++i) {
			if (VipBorderItem* it = qobject_cast<VipBorderItem*>(scales[i])) {
				if (it->orientation() == Qt::Horizontal) {
					d_data->viewer->area()->setZoomEnabled(it, isHZoomEnabled());
				}
				else {
					d_data->viewer->area()->setZoomEnabled(it, isVZoomEnabled());
				}
			}
		}
	}
}

void VipPlotPlayer::setAutoScale(bool enable)
{
	if (enable != viewer()->area()->isAutoScale()) {
		viewer()->area()->setAutoScale(enable);
		computeStartDate();
	}
	d_data->autoScale->blockSignals(true);
	d_data->autoScale->setChecked(enable);
	d_data->autoScale->blockSignals(false);
}

bool VipPlotPlayer::displayVerticalWindow() const
{
	return d_data->verticalWindow->isVisible();
}

void VipPlotPlayer::setDisplayVerticalWindow(bool enable)
{
	if (enable != displayVerticalWindow()) {
		if (enable) {
			QRectF r = verticalWindow()->rawData().polygon().boundingRect();
			if (r.isEmpty()) {
				VipInterval inter = verticalWindow()->axes().first()->scaleDiv().bounds().normalized();
				VipInterval yinter = verticalWindow()->axes().last()->scaleDiv().bounds().normalized();
				VipShape sh = verticalWindow()->rawData();
				sh.setRect(QRectF(QPointF(inter.minValue() + 0.05 * inter.width(), yinter.maxValue()), QPointF(inter.maxValue() - 0.05 * inter.width(), yinter.minValue())));
				verticalWindow()->setRawData(sh);
			}
		}
		d_data->verticalWindow->setVisible(enable);
		// d_data->verticalWindowResize->setVisible(enable);
	}
	d_data->displayVerticalWindow->blockSignals(true);
	d_data->displayVerticalWindow->setChecked(enable);
	d_data->displayVerticalWindow->blockSignals(false);
}

void VipPlotPlayer::resetVerticalWindow()
{
	VipShape sh = verticalWindow()->rawData();
	if (verticalWindow()->isVisible()) {
		VipInterval inter = verticalWindow()->axes().first()->scaleDiv().bounds().normalized();
		VipInterval yinter = verticalWindow()->axes().last()->scaleDiv().bounds().normalized();
		sh.setRect(QRectF(QPointF(inter.minValue() + 0.05 * inter.width(), yinter.maxValue()), QPointF(inter.maxValue() - 0.05 * inter.width(), yinter.minValue())));
	}
	else
		sh.setRect(QRectF());
	verticalWindow()->setRawData(sh);
}

void VipPlotPlayer::autoScaleX()
{
	if (viewer()->area()->isAutoScale())
		return;
	viewer()->area()->bufferScalesState();
	QList<VipBorderItem*> items = vipListCast<VipBorderItem*>(viewer()->area()->scales());
	for (int i = 0; i < items.size(); ++i)
		if (items[i]->alignment() == VipBorderItem::Bottom) {
			items[i]->setAutoScale(false);
			items[i]->setAutoScale(true);
		}
}

void VipPlotPlayer::autoScaleY()
{
	if (viewer()->area()->isAutoScale())
		return;
	viewer()->area()->bufferScalesState();
	QList<VipBorderItem*> items = vipListCast<VipBorderItem*>(viewer()->area()->scales());
	for (int i = 0; i < items.size(); ++i)
		if (items[i]->alignment() == VipBorderItem::Left) {
			items[i]->setAutoScale(false);
			items[i]->setAutoScale(true);
		}
}

void VipPlotPlayer::autoScale()
{
	setAutoScale(true);
}

void VipPlotPlayer::undoMenuShow()
{
	d_data->undoScale->setEnabled(d_data->viewer->area()->undoStates().size());
	d_data->redoScale->setEnabled(d_data->viewer->area()->redoStates().size());
}

void VipPlotPlayer::setLegendPosition(Vip::PlayerLegendPosition pos)
{
	if (pos == Vip::LegendHidden) {
		// d_data->innerLegend->setVisible(false);
		for (int i = 0; i < d_data->viewer->area()->innerLegendCount(); ++i)
			d_data->viewer->area()->innerLegend(i)->setVisible(false);
		plotWidget2D()->area()->borderLegend()->setVisible(false);
	}
	else if (pos == Vip::LegendBottom) {
		// d_data->innerLegend->setVisible(false);
		for (int i = 0; i < d_data->viewer->area()->innerLegendCount(); ++i)
			d_data->viewer->area()->innerLegend(i)->setVisible(false);
		plotWidget2D()->area()->borderLegend()->setVisible(true);
	}
	else if (pos == Vip::LegendInnerBottomLeft) {
		// d_data->innerLegend->setVisible(true);
		plotWidget2D()->area()->borderLegend()->setVisible(false);
		// d_data->viewer->area()->setInnerLegendAlignment(0, Qt::AlignBottom | Qt::AlignLeft);
		for (int i = 0; i < d_data->viewer->area()->innerLegendCount(); ++i) {
			d_data->viewer->area()->innerLegend(i)->setVisible(true);
			d_data->viewer->area()->setInnerLegendAlignment(i, Qt::AlignBottom | Qt::AlignLeft);
		}
	}
	else if (pos == Vip::LegendInnerBottomRight) {
		// d_data->innerLegend->setVisible(true);
		plotWidget2D()->area()->borderLegend()->setVisible(false);
		// d_data->viewer->area()->setInnerLegendAlignment(0, Qt::AlignBottom | Qt::AlignRight);
		for (int i = 0; i < d_data->viewer->area()->innerLegendCount(); ++i) {
			d_data->viewer->area()->innerLegend(i)->setVisible(true);
			d_data->viewer->area()->setInnerLegendAlignment(i, Qt::AlignBottom | Qt::AlignRight);
		}
	}
	else if (pos == Vip::LegendInnerTopLeft) {
		// d_data->innerLegend->setVisible(true);
		plotWidget2D()->area()->borderLegend()->setVisible(false);
		// d_data->viewer->area()->setInnerLegendAlignment(0, Qt::AlignTop | Qt::AlignLeft);
		for (int i = 0; i < d_data->viewer->area()->innerLegendCount(); ++i) {
			d_data->viewer->area()->innerLegend(i)->setVisible(true);
			d_data->viewer->area()->setInnerLegendAlignment(i, Qt::AlignTop | Qt::AlignLeft);
		}
	}
	else if (pos == Vip::LegendInnerTopRight) {
		// d_data->innerLegend->setVisible(true);
		plotWidget2D()->area()->borderLegend()->setVisible(false);
		// d_data->viewer->area()->setInnerLegendAlignment(0, Qt::AlignTop | Qt::AlignRight);
		for (int i = 0; i < d_data->viewer->area()->innerLegendCount(); ++i) {
			d_data->viewer->area()->innerLegend(i)->setVisible(true);
			d_data->viewer->area()->setInnerLegendAlignment(i, Qt::AlignTop | Qt::AlignRight);
		}
	}
	d_data->legendPosition = pos;

	for (int i = 0; i <= Vip::LegendInnerBottomRight; ++i) {
		d_data->legendActions[i]->blockSignals(true);
		d_data->legendActions[i]->setChecked(false);
		d_data->legendActions[i]->blockSignals(false);
	}
	d_data->legendActions[pos]->blockSignals(true);
	d_data->legendActions[pos]->setChecked(true);
	d_data->legendActions[pos]->blockSignals(false);
}
Vip::PlayerLegendPosition VipPlotPlayer::legendPosition() const
{
	return d_data->legendPosition;
}
VipLegend* VipPlotPlayer::innerLegend() const
{
	return nullptr; // d_data->innerLegend;
}

void VipPlotPlayer::legendTriggered()
{
	if (QAction* act = qobject_cast<QAction*>(sender()))
		setLegendPosition((Vip::PlayerLegendPosition)act->property("position").toInt());
}

VipAbstractScale* VipPlotPlayer::addLeftScale()
{
	return insertLeftScale(leftScaleCount());
}

VipAbstractScale* VipPlotPlayer::addLeftScale(VipAbstractScale* scale)
{
	return insertLeftScale(leftScaleCount(), scale);
}

VipAbstractScale* VipPlotPlayer::insertLeftScale(int index, VipAbstractScale* scale)
{
	if (VipAxisBase* axis = qobject_cast<VipAxisBase*>(scale)) {
		if (axis->alignment() != VipAxisBase::Left)
			return nullptr;

		axis->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
		axis->setRenderHints(QPainter::TextAntialiasing);
		axis->setMargin(0);
		axis->setMaxBorderDist(0, 0);
		axis->setZValue(20);
		axis->setExpandToCorners(true);

		plotWidget2D()->area()->blockSignals(true);
		if (VipVMultiPlotArea2D* a = qobject_cast<VipVMultiPlotArea2D*>(plotWidget2D()->area()))
			a->setInsertionIndex(index);
		plotWidget2D()->area()->addScale(axis, true);
		plotWidget2D()->area()->blockSignals(false);

		// add legend
		VipLegend* l = new VipLegend();
		l->setVisible(plotWidget2D()->area()->innerLegend(0)->isVisible());
		l->layout()->setMaxColumns(plotWidget2D()->area()->innerLegend(0)->layout()->maxColumns());
		l->setProperty("position", plotWidget2D()->area()->innerLegend(0)->property("position"));
		qreal le, r, t, b;
		plotWidget2D()->area()->innerLegend(0)->layout()->getContentsMargins(&le, &t, &r, &b);
		l->layout()->setContentsMargins(le, t, r, b);
		// l->layout()->setMargins(plotWidget2D()->area()->innerLegend(0)->layout()->setMargins);
		l->layout()->setSpacing(plotWidget2D()->area()->innerLegend(0)->layout()->spacing());
		l->setDrawCheckbox(plotWidget2D()->area()->innerLegend(0)->drawCheckbox());

		plotWidget2D()->area()->addInnerLegend(l, axis, plotWidget2D()->area()->innerLegendAlignment(0), plotWidget2D()->area()->innerLegendMargin(0));

		return axis;
	}
	return nullptr;
}

VipAbstractScale* VipPlotPlayer::insertLeftScale(int index)
{
	// create axis
	VipAxisBase* axis = new VipAxisBase(VipBorderItem::Left);
	axis->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	axis->setRenderHints(QPainter::TextAntialiasing);
	axis->setMargin(0);
	axis->setMaxBorderDist(0, 0);
	axis->setZValue(20);
	axis->setExpandToCorners(true);
	axis->scaleDraw()->valueToText()->setAutomaticExponent(true);
	axis->scaleDraw()->valueToText()->setMaxLabelSize(3);
	plotWidget2D()->area()->blockSignals(true);
	if (VipVMultiPlotArea2D* a = qobject_cast<VipVMultiPlotArea2D*>(plotWidget2D()->area()))
		a->setInsertionIndex(index);
	plotWidget2D()->area()->addScale(axis, true);
	plotWidget2D()->area()->blockSignals(false);
	// create a new VipPlotSceneModel for this scale
	VipPlotSceneModel* sm = createPlotSceneModel(QList<VipAbstractScale*>() << xScale() << axis, VipCoordinateSystem::Cartesian);
	sm->setDrawComponent("All", VipPlotShape::FillPixels, false);

	// add legend
	VipLegend* l = new VipLegend();
	l->setVisible(plotWidget2D()->area()->innerLegend(0)->isVisible());
	l->layout()->setMaxColumns(plotWidget2D()->area()->innerLegend(0)->layout()->maxColumns());
	l->setProperty("position", plotWidget2D()->area()->innerLegend(0)->property("position"));
	qreal le, r, t, b;
	plotWidget2D()->area()->innerLegend(0)->layout()->getContentsMargins(&le, &t, &r, &b);
	l->layout()->setContentsMargins(le, t, r, b);
	// l->layout()->setMargins(plotWidget2D()->area()->innerLegend(0)->layout()->setMargins);
	l->layout()->setSpacing(plotWidget2D()->area()->innerLegend(0)->layout()->spacing());
	l->setDrawCheckbox(plotWidget2D()->area()->innerLegend(0)->drawCheckbox());

	plotWidget2D()->area()->addInnerLegend(l, axis, plotWidget2D()->area()->innerLegendAlignment(0), plotWidget2D()->area()->innerLegendMargin(0));

	return axis;
}

int VipPlotPlayer::leftScaleCount() const
{
	if (VipVMultiPlotArea2D* a = qobject_cast<VipVMultiPlotArea2D*>(this->plotWidget2D()->area()))
		return a->leftMultiAxis()->count();
	return 1;
}

bool VipPlotPlayer::removeLeftScale(VipAbstractScale* scale)
{
	VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer);
	if (!pl)
		// Only works for VipPlotWidget2D
		return false;

	// find another left scale
	QList<VipAxisBase*> axes = vipListCast<VipAxisBase*>(plotWidget2D()->area()->scales());
	VipAxisBase* new_left_scale = nullptr;
	for (int i = 0; i < axes.size(); ++i)
		if (axes[i]->alignment() == VipBorderItem::Left && axes[i] != scale) {
			new_left_scale = axes[i];
			break;
		}
	if (!new_left_scale)
		return false;

	// remove the plot scene model
	VipPlotSceneModel* ps = plotSceneModel();
	if (VipPlotSceneModel* model = findPlotSceneModel(QList<VipAbstractScale*>() << xScale() << scale)) {
		// delete the default plot scene model for this axis
		model->blockSignals(true);
		model->setAxes(QList<VipAbstractScale*>(), VipCoordinateSystem::Cartesian);
		model->blockSignals(false);
		model->deleteLater();

		if (model == ps) {
			// we removed the "default" scene model, take the bottom most plot scene model and make it the default
			setPlotSceneModel(findPlotSceneModel(QList<VipAbstractScale*>() << xScale() << new_left_scale));
			// we also need to move the markers
			d_data->timeMarker->setAxes(pl->area()->bottomAxis(), new_left_scale, VipCoordinateSystem::Cartesian);
			d_data->xMarker->setAxes(pl->area()->bottomAxis(), new_left_scale, VipCoordinateSystem::Cartesian);
		}
	}

	// Move the vertical window if necessary
	if (d_data->verticalWindow->axes().indexOf(scale) >= 0) {
		d_data->verticalWindow->setAxes(pl->area()->bottomAxis(), new_left_scale, VipCoordinateSystem::Cartesian);
		d_data->verticalWindowResize->setAxes(pl->area()->bottomAxis(), new_left_scale, VipCoordinateSystem::Cartesian);
	}

	plotWidget2D()->area()->removeScale(scale);

	// remove other plot items based on this scale
	PlotItemList lst = scale->plotItems();
	for (int i = 0; i < lst.size(); ++i) {
		VipPlotItem* it = lst[i];
		if (it != d_data->timeMarker && it != d_data->xMarker) {
			it->setAxes(QList<VipAbstractScale*>(), VipCoordinateSystem::Null);
			it->deleteLater();
		}
	}

	scale->deleteLater();
	return true;
}

VipProcessingList* VipPlotPlayer::currentProcessingList() const
{
	if (VipPlotItemData* current = currentPlotItem()) {
		if (VipDisplayObject* obj = current->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QList<VipProcessingList*> lst = vipListCast<VipProcessingList*>(obj->allSources());
			if (lst.size())
				return lst.first();
		}
	}
	return nullptr;
}

VipPlotItemData* VipPlotPlayer::currentPlotItem() const
{
	QList<VipPlotItemData*> lst = d_data->viewer->area()->findItems<VipPlotItemData*>(QString(), 1, 1);
	if (lst.size() == 1)
		return lst.first();
	return nullptr;
}

bool VipPlotPlayer::isNormalized() const
{
	return d_data->normalize->isChecked();
}

bool VipPlotPlayer::isStartAtZero() const
{
	return d_data->start_zero->isChecked();
}

bool VipPlotPlayer::isStartYAtZero() const
{
	return d_data->start_y_zero->isChecked();
}

void VipPlotPlayer::computeDeleteMenu()
{
	d_data->deleteItemMenu->clear();

	QList<VipPlotItem*> items = viewer()->area()->findItems<VipPlotItem*>();
	QList<VipPlotItem*> removableItems;

	for (int i = 0; i < items.size(); ++i) {
		if (items[i]->testItemAttribute(VipPlotItem::IsSuppressable)) {
			QPixmap icon(20, 20);
			icon.fill(Qt::transparent);
			if (items[i]->testItemAttribute(VipPlotItem::HasLegendIcon) && items[i]->legendNames().size() == 1) {
				QPainter p(&icon);
				items[i]->drawLegend(&p, QRectF(0, 0, 20, 20), 0);
			}
			QAction* act = d_data->deleteItemMenu->addAction(icon, items[i]->title().text());
			act->setProperty("VipPlotItem", QVariant::fromValue(items[i]));
		}
	}
}

void VipPlotPlayer::deleteItem(QAction* act)
{
	if (VipPlotItem* item = act->property("VipPlotItem").value<VipPlotItem*>()) {
		item->deleteLater();

		vipProcessEvents();
		// show again the delete menu
		// computeSelectionMenu();
		QMetaObject::invokeMethod(d_data->deleteItem, "showMenu", Qt::QueuedConnection);
		// d_data->deleteItem->showMenu();
	}
}

void VipPlotPlayer::startRender(VipRenderState& state)
{
	// save the legend check state
	state.state(this)["checkState"] = viewer()->area()->legend()->checkState();
	viewer()->area()->legend()->setCheckState(VipLegend::NonCheckable);
	// d_data->innerLegend->setCheckState(VipLegend::NonCheckable);

	VipPlayer2D::startRender(state);
}

void VipPlotPlayer::endRender(VipRenderState& state)
{
	viewer()->area()->legend()->setCheckState(VipLegend::CheckState(state.state(this)["checkState"].toInt()));
	// d_data->innerLegend->setCheckState(VipLegend::CheckState(state.state(this)["checkState"].toInt()));

	VipPlayer2D::endRender(state);
}

void VipPlotPlayer::hideAllItems()
{
	QList<VipPlotItem*> items = viewer()->area()->findItems<VipPlotItem*>();
	for (int i = 0; i < items.size(); ++i) {
		// only consider non grid items with a title
		if (!qobject_cast<VipPlotGrid*>(items[i]) && !items[i]->title().isEmpty()) {
			items[i]->setVisible(false);
		}
	}
	this->plotWidget2D()->recomputeGeometry();
	this->computeSelectionMenu();
}

void VipPlotPlayer::computeSelectionMenu()
{
	// despite its name, this function create the menu to set the visibility of the plot items, not the selection

	d_data->selectionItemMenu->clear();

	QList<VipPlotItem*> items = viewer()->area()->findItems<VipPlotItem*>();

	QWidget* w = new QWidget();
	w->setLayout(new QVBoxLayout());

	QToolButton* hide_all = new QToolButton();
	hide_all->setText("Hide all items");
	hide_all->setAutoRaise(true);
	w->layout()->addWidget(hide_all);
	w->layout()->addWidget(VipLineWidget::createSunkenHLine());
	connect(hide_all, SIGNAL(clicked(bool)), this, SLOT(hideAllItems()));

	for (int i = 0; i < items.size(); ++i) {
		// only consider non grid items with a title
		if (!qobject_cast<VipPlotGrid*>(items[i]) && !items[i]->title().isEmpty()) {
			// QPixmap icon(20, 20);
			//  icon.fill(Qt::transparent);
			//  if (items[i]->testItemAttribute(VipPlotItem::HasLegendIcon) && items[i]->legendNames().size() == 1)
			//  {
			//  QPainter p(&icon);
			//  items[i]->drawLegend(&p, QRectF(0, 0, 20, 20), 0);
			//  }

			QCheckBox* box = new QCheckBox();
			box->setChecked(items[i]->isVisible());
			box->setText(items[i]->title().text());
			connect(box, SIGNAL(clicked(bool)), items[i], SLOT(setVisible(bool)));
			// make sure to recompute the widget geometry as the legend size might change
			connect(box, SIGNAL(clicked(bool)), this->plotWidget2D(), SLOT(recomputeGeometry()));
			w->layout()->addWidget(box);
		}
	}

	w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	d_data->selectionItemMenu->setWidget(w);
}

void VipPlotPlayer::selectItem(QAction* act)
{
	// unselect all items
	QList<VipPlotItem*> items = d_data->viewer->area()->findItems<VipPlotItem*>();
	for (int i = 0; i < items.size(); ++i)
		items[i]->setSelected(false);

	if (VipPlotItem* item = act->property("VipPlotItem").value<VipPlotItem*>())
		item->setSelected(true);
}

void VipPlotPlayer::addSelectedProcessing(const VipProcessingObject::Info& info)
{
	QList<VipPlotItemData*> items = d_data->viewer->area()->findItems<VipPlotItemData*>(QString(), 1, 1);

	// check if this is a data fusion processing
	const QMetaObject* meta = QMetaType(info.metatype).metaObject();
	while (meta) {
		if (strcmp(meta->className(), "VipBaseDataFusion") == 0)
			break;
		else
			meta = meta->superClass();
	}
	if (meta && info.displayHint != VipProcessingObject::InputTransform) {
		// apply the data fusion algorithm if NOT an input tranform one
		// note that selection order matters when it comes to signal fusion processing, therefore sort them first
		VipProcessingObject* obj = vipCreateDataFusionProcessing(vipCastItemListOrdered<VipPlotItem*>(items), info);
		if (obj) {
			if (info.displayHint == VipProcessingObject::DisplayOnDifferentSupport) {
				QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessing(obj, nullptr);
				if (pls.size())
					vipGetMainWindow()->openPlayers(pls);
			}
			else
				vipCreatePlayersFromProcessing(obj, this);

			vipGetProcessingEditorToolWidget()->setProcessingObject(obj);
			QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
		}
		computeStartDate();
		return;
	}

	if (info.displayHint == VipProcessingObject::DisplayOnSameSupport) {
		VipProcessingObject* last = nullptr;
		// create a new pipeline for each item, and display them in this player
		for (int i = 0; i < items.size(); ++i) {
			if (VipDisplayObject* obj = items[i]->property("VipDisplayObject").value<VipDisplayObject*>())
				if (VipOutput* out = obj->inputAt(0)->connection()->source()) {
					if (VipProcessingObject* tmp = vipCreateProcessing(out, info)) {
						if (vipCreatePlayersFromProcessing(obj, this).size() && i == items.size() - 1)
							last = tmp;
					}
				}
		}
		if (last) {
			vipGetProcessingEditorToolWidget()->setProcessingObject(last);
			QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
		}
	}
	else if (info.displayHint == VipProcessingObject::DisplayOnDifferentSupport) {
		VipProcessingObject* last = nullptr;
		// create a new player and display all new pipelines in this new player

		// first, create the new player
		VipAbstractPlayer* pl = nullptr;
		if (VipDisplayObject* obj = items[0]->property("VipDisplayObject").value<VipDisplayObject*>())
			if (VipOutput* out = obj->inputAt(0)->connection()->source()) {
				if (VipProcessingObject* tmp = vipCreateProcessing(out, info)) {
					QList<VipAbstractPlayer*> pls = vipCreatePlayersFromProcessing(obj, nullptr);
					if (items.size() == 1)
						last = tmp;
					if (pls.size())
						pl = pls[0];
				}
			}
		if (!pl)
			return;
		// then, add the other pipelines to the player
		for (int i = 1; i < items.size(); ++i) {
			if (VipDisplayObject* obj = items[i]->property("VipDisplayObject").value<VipDisplayObject*>())
				if (VipOutput* out = obj->inputAt(0)->connection()->source()) {
					if (VipProcessingObject* tmp = vipCreateProcessing(out, info)) {
						if (vipCreatePlayersFromProcessing(obj, pl).size() && i == items.size() - 1)
							last = tmp;
					}
				}
		}

		vipGetMainWindow()->openPlayers(QList<VipAbstractPlayer*>() << pl);
		if (last) {
			vipGetProcessingEditorToolWidget()->setProcessingObject(last);
			QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
		}
	}
	else if (info.displayHint == VipProcessingObject::InputTransform) {
		// apply the processing to all selected items
		for (int i = 0; i < items.size(); ++i) {
			// get the processing list
			VipProcessingList* lst = nullptr;
			if (VipDisplayObject* obj = items[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
				QList<VipProcessingList*> all_lst = vipListCast<VipProcessingList*>(obj->allSources());
				if (all_lst.size())
					lst = all_lst.first();
			}
			if (!lst)
				continue;

			// add the selected processings
			QList<VipProcessingObject::Info> infos = QList<VipProcessingObject::Info>() << info;

			if (infos.size()) {
				vipGetProcessingEditorToolWidget()->setProcessingObject(items[i]->property("VipDisplayObject").value<VipDisplayObject*>());
				if (VipProcessingListEditor* editor = vipGetProcessingEditorToolWidget()->editor()->processingEditor<VipProcessingListEditor*>(lst)) {
					editor->addProcessings(infos);
					if (lst->size())
						editor->selectObject(lst->processings().last());

					vipGetProcessingEditorToolWidget()->editor()->setProcessingObjectVisible(lst, true);
					vipGetProcessingEditorToolWidget()->show();
					vipGetProcessingEditorToolWidget()->raise();
					QMetaObject::invokeMethod(vipGetProcessingEditorToolWidget(), "resetSize", Qt::QueuedConnection);
				}
			}
		}
	}

	computeStartDate();
}

void VipPlotPlayer::normalize(bool apply)
{
	d_data->normalize->blockSignals(true);
	d_data->normalize->setChecked(apply);
	d_data->normalize->blockSignals(false);

	QList<VipDisplayObject*> lst = this->displayObjects();
	VipNormalize test;

	for (int i = 0; i < lst.size(); ++i) {
		if (!lst[i]->inputAt(0)->connection()->source())
			continue;

		// get the processing just before
		VipOutput* src = lst[i]->inputAt(0)->connection()->source();
		if (!src)
			continue;

		VipProcessingObject* obj = src->parentProcessing();
		if (!obj)
			continue;

		// check if VipNormalize can be applied on this data
		if (!test.acceptInput(0, lst[i]->inputAt(0)->connection()->source()->data().data()))
			continue;

		VipProcessingList* plist = qobject_cast<VipProcessingList*>(obj);
		if (!plist) {
			// insert a processing list between the display and its source
			plist = new VipProcessingList(obj->parent());
			plist->setScheduleStrategy(VipProcessingList::Asynchronous);
			plist->setDeleteOnOutputConnectionsClosed(true);

			obj->setDeleteOnOutputConnectionsClosed(false);
			lst[i]->inputAt(0)->setConnection(plist->outputAt(0));
			src->setConnection(plist->inputAt(0));
			obj->setDeleteOnOutputConnectionsClosed(true);
		}

		QList<VipNormalize*> norms = vipListCast<VipNormalize*>(plist->processings());

		if (apply) {
			// apply normalization
			if (norms.size())
				continue;
			else {
				// insert a VipNormalize object
				plist->append(new VipNormalize());
			}
		}
		else {
			// remove any VipNormalize object from the processing list
			for (int n = 0; n < norms.size(); ++n)
				plist->remove(norms[n]);
		}

		plist->reload();
	}

	if (sender() == d_data->normalize) {
		// setAutoScale(false);
		// setAutoScale(true);
		this->autoScaleY();
	}
}

void VipPlotPlayer::startAtZero(bool apply)
{
	d_data->start_zero->blockSignals(true);
	d_data->start_zero->setChecked(apply);
	d_data->start_zero->blockSignals(false);

	QList<VipDisplayObject*> lst = this->displayObjects();
	VipStartAtZero test;

	for (int i = 0; i < lst.size(); ++i) {
		if (!lst[i]->inputAt(0)->connection()->source())
			continue;

		VipOutput* src = lst[i]->inputAt(0)->connection()->source();
		if (!src)
			continue;

		VipProcessingObject* obj = src->parentProcessing();
		if (!obj)
			continue;

		// check if VipNormalize can be applied on this data
		if (!test.acceptInput(0, lst[i]->inputAt(0)->connection()->source()->data().data()))
			continue;

		VipProcessingList* plist = qobject_cast<VipProcessingList*>(obj);
		if (!plist) {
			// insert a processing list between the display and its source
			plist = new VipProcessingList(obj->parent());
			plist->setScheduleStrategy(VipProcessingList::Asynchronous);
			plist->setDeleteOnOutputConnectionsClosed(true);

			obj->setDeleteOnOutputConnectionsClosed(false);
			lst[i]->inputAt(0)->setConnection(plist->outputAt(0));
			src->setConnection(plist->inputAt(0));
			obj->setDeleteOnOutputConnectionsClosed(true);
		}

		QList<VipStartAtZero*> starts = vipListCast<VipStartAtZero*>(plist->processings());

		if (apply) {
			// apply
			if (starts.size())
				continue;
			else {
				// insert a VipStartAtZero object
				plist->append(new VipStartAtZero());
			}
		}
		else {
			// remove any VipStartAtZero object from the processing list
			for (int n = 0; n < starts.size(); ++n)
				plist->remove(starts[n]);
		}
		plist->reload();
	}

	if (sender() == d_data->start_zero) {
		setAutoScale(false);
		setAutoScale(true);
	}
	computeStartDate();
}

void VipPlotPlayer::startYAtZero(bool apply)
{
	d_data->start_y_zero->blockSignals(true);
	d_data->start_y_zero->setChecked(apply);
	d_data->start_y_zero->blockSignals(false);

	QList<VipDisplayObject*> lst = this->displayObjects();
	VipStartYAtZero test;

	for (int i = 0; i < lst.size(); ++i) {
		if (!lst[i]->inputAt(0)->connection()->source())
			continue;

		VipOutput* src = lst[i]->inputAt(0)->connection()->source();
		if (!src)
			continue;

		VipProcessingObject* obj = src->parentProcessing();
		if (!obj)
			continue;

		// check if VipStartYAtZero can be applied on this data
		if (!test.acceptInput(0, lst[i]->inputAt(0)->connection()->source()->data().data()))
			continue;

		VipProcessingList* plist = qobject_cast<VipProcessingList*>(obj);
		if (!plist) {
			// insert a processing list between the display and its source
			plist = new VipProcessingList(obj->parent());
			plist->setScheduleStrategy(VipProcessingList::Asynchronous);
			plist->setDeleteOnOutputConnectionsClosed(true);

			obj->setDeleteOnOutputConnectionsClosed(false);
			lst[i]->inputAt(0)->setConnection(plist->outputAt(0));
			src->setConnection(plist->inputAt(0));
			obj->setDeleteOnOutputConnectionsClosed(true);
		}

		QList<VipStartYAtZero*> starts = vipListCast<VipStartYAtZero*>(plist->processings());

		if (apply) {
			// apply
			if (starts.size())
				continue;
			else {
				// insert a VipStartYAtZero object
				plist->append(new VipStartYAtZero());
			}
		}
		else {
			// remove any VipStartYAtZero object from the processing list
			for (int n = 0; n < starts.size(); ++n)
				plist->remove(starts[n]);
		}
		plist->reload();
	}

	if (sender() == d_data->start_y_zero) {
		// setAutoScale(false);
		// setAutoScale(true);
		this->autoScaleY();
	}
}

bool VipPlotPlayer::gridVisible() const
{
	return plotWidget2D()->area()->grid()->isVisible();
}
bool VipPlotPlayer::legendVisible() const
{
	return plotWidget2D()->area()->borderLegend()->isVisible();
}

void VipPlotPlayer::showGrid(bool sh)
{
	plotWidget2D()->area()->grid()->setVisible(sh);
	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(plotWidget2D()->area())) {
		QList<VipPlotGrid*> grids = area->findItems<VipPlotGrid*>(QString());
		for (int i = 0; i < grids.size(); ++i)
			grids[i]->setVisible(sh);
	}
	d_data->show_axes->blockSignals(true);
	d_data->show_axes->setChecked(sh);
	d_data->show_axes->blockSignals(false);
}

void VipPlotPlayer::timeChanged()
{
	// update the time marker
	if (d_data->timeMarker->isVisible() && d_data->pool) {
		d_data->timeMarker->setRawData(QPointF(d_data->pool->time(), 0));
		VipText t = d_data->timeMarker->label();
		t.setText(defaultXAxis()->scaleDraw()->valueToText()->convert(d_data->pool->time()));
		d_data->timeMarker->setLabel(t);
	}
}

QToolButton* VipPlotPlayer::advancedTools() const
{
	return d_data->advancedTools;
}

VipPlotMarker* VipPlotPlayer::timeMarker() const
{
	return d_data->timeMarker;
}

VipPlotMarker* VipPlotPlayer::xMarker() const
{
	return d_data->xMarker;
}

VipPlotShape* VipPlotPlayer::verticalWindow() const
{
	return d_data->verticalWindow;
}

void VipPlotPlayer::setTimeMarkerVisible(bool visible)
{
	d_data->timeMarkerVisible->blockSignals(true);
	d_data->timeMarkerVisible->setChecked(visible);
	d_data->timeMarkerVisible->blockSignals(false);
	d_data->timeMarker->setVisible(visible);
	if (!visible && processingPool() && processingPool()->deviceType() == VipIODevice::Temporal)
		d_data->timeMarkerAlwaysVisible = false;
	timeChanged();
}

void VipPlotPlayer::setProcessingPool(VipProcessingPool* pool)
{
	VipAbstractPlayer::setProcessingPool(pool);

	if (d_data->pool) {
		disconnect(d_data->pool, SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged()));
		disconnect(d_data->pool, SIGNAL(deviceTypeChanged()), this, SLOT(poolTypeChanged()));
	}

	if (pool) {
		d_data->pool = pool;
		connect(pool, SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged()));
		connect(d_data->pool, SIGNAL(deviceTypeChanged()), this, SLOT(poolTypeChanged()));
	}

	poolTypeChanged();

	// when we set the processing pool, force time marker
	if (d_data->timeMarkerAlwaysVisible)
		setTimeMarkerVisible(processingPool()->deviceType() == VipIODevice::Temporal);
}

QGraphicsObject* VipPlotPlayer::defaultEditableObject() const
{
	QGraphicsObject* current = nullptr;
	QList<VipPlotItem*> items = plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);
	if (items.size())
		current = items.first();
	else {
		// select the first VipPlotCurve or VipPlotHistogram
		items = plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 0, 1);
		for (int i = 0; i < items.size(); ++i) {
			if (qobject_cast<VipPlotCurve*>(items[i]) || qobject_cast<VipPlotHistogram*>(items[i])) {
				current = items[i];
				break;
			}
		}
	}
	if (!current)
		current = d_data->viewer->area()->grid();
	return current;
}

VipDisplayObject* VipPlotPlayer::mainDisplayObject() const
{
	QList<VipDisplayObject*> disps = this->displayObjects();
	return disps.size() ? disps.first() : nullptr;
}

QList<VipPlotSceneModel*> VipPlotPlayer::plotSceneModels() const
{
	QList<VipAbstractScale*> left = leftScales();
	QList<VipPlotSceneModel*> res;

	for (int i = 0; i < left.size(); ++i) {
		if (VipPlotSceneModel* sm = findPlotSceneModel(QList<VipAbstractScale*>() << xScale() << left[i]))
			res << sm;
	}

	if (plotSceneModel()) {
		res.removeOne(plotSceneModel());
		res.push_front(plotSceneModel());
	}
	return res;
}

void VipPlotPlayer::showParameters()
{
	// select on which item to edit
	VipPlotItem* current = currentPlotItem();
	if (!current)
		current = qobject_cast<VipPlotItem*>(defaultEditableObject());

	vipGetPlotToolWidgetPlayer()->setItem(current);
	vipGetPlotToolWidgetPlayer()->show();
	vipGetPlotToolWidgetPlayer()->resetSize();
}

void VipPlotPlayer::setTimeMarkerAlwaysVisible(bool enable)
{
	d_data->timeMarkerAlwaysVisible = enable;
	// vip_debug("setTimeMarkerAlwaysVisible :%i\n", (int)enable);
	poolTypeChanged();
	if (!enable)
		setTimeMarkerVisible(false);
}

void VipPlotPlayer::poolTypeChanged()
{
	if (VipProcessingPool* pool = processingPool()) {
		if (d_data->timeMarkerAlwaysVisible)
			setTimeMarkerVisible(pool->deviceType() == VipIODevice::Temporal);
		else
			setTimeMarkerVisible(false);

		d_data->timeMarkerVisible->setVisible(pool->deviceType() == VipIODevice::Temporal);
	}
}

void VipPlotPlayer::toolTipStarted(const QPointF&)
{
	// remove the tool tip offset if necessary
	if (!(this->plotWidget2D()->area()->plotToolTip()->displayFlags() & VipToolTip::SearchXAxis) && !(this->plotWidget2D()->area()->plotToolTip()->displayFlags() & VipToolTip::SearchYAxis))
		plotWidget2D()->area()->plotToolTip()->removeToolTipOffset();
}

void VipPlotPlayer::toolTipMoved(const QPointF& pos)
{
	if (this->plotWidget2D()->area()->plotToolTip()->displayFlags() & VipToolTip::SearchXAxis || this->plotWidget2D()->area()->plotToolTip()->displayFlags() & VipToolTip::SearchYAxis) {
		VipPoint scale = this->plotWidget2D()->area()->positionToScale(pos);
		vip_double time = scale.x();
		vip_double y = scale.y();

		// we display a vertical line representing the current time for this player and ALL other players in the current workspace
		if (VipDisplayPlayerArea* workspace = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
			QList<VipPlotPlayer*> pls = workspace->findChildren<VipPlotPlayer*>();
			for (int i = 0; i < pls.size(); ++i) {
				VipPlotPlayer* pl = pls[i];
				bool is_time_based = pl->haveTimeUnit();
				if (!is_time_based && (this->plotWidget2D()->area()->plotToolTip()->displayFlags() & VipToolTip::SearchXAxis)) {
					pl->xMarker()->setVisible(false);
					pl->plotWidget2D()->area()->plotToolTip()->removeToolTipOffset();
				}
				else {
					if (!pl->xMarker()->isVisible())
						pl->xMarker()->setVisible(true);
					if (this->plotWidget2D()->area()->plotToolTip()->displayFlags() & VipToolTip::SearchXAxis) {
						pl->xMarker()->setRawData(VipPoint(time, 0));
						pl->xMarker()->setLineStyle(VipPlotMarker::VLine);
					}
					else {
						pl->xMarker()->setRawData(VipPoint(0, y));
						pl->xMarker()->setLineStyle(VipPlotMarker::HLine);
					}
					pl->plotWidget2D()->area()->plotToolTip()->setToolTipOffset(QPoint(20, 0));
				}
			}
		}
	}
}
void VipPlotPlayer::toolTipEnded(const QPointF&)
{
	// TODO: might be received at closing: crash!

	// hide the time marker for all players
	if (VipDisplayPlayerArea* workspace = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		QList<VipPlotPlayer*> pls = workspace->findChildren<VipPlotPlayer*>();
		for (int i = 0; i < pls.size(); ++i) {
			VipPlotPlayer* pl = pls[i];
			pl->xMarker()->setVisible(false);
		}
	}
}

void VipPlotPlayer::refreshToolTip(VipPlotItem* item)
{
	if (qobject_cast<VipPlotMarker*>(item))
		return;

	if (d_data->viewer->area()->plotToolTip() && VipCorrectedTip::isVisible()) {
		if (xMarker()->isVisible()) {
			// convert mouse pos to scale value
			QPointF pos = screenToSceneCoordinates(d_data->viewer->scene(), QCursor::pos());
			pos = defaultXAxis()->mapFromScene(pos);
			double time = defaultXAxis()->value(pos);

			xMarker()->blockSignals(true);
			xMarker()->setRawData(QPointF(time, 0));
			xMarker()->blockSignals(false);
		}

		d_data->viewer->area()->plotToolTip()->refresh();
	}
}

VipAbstractPlotWidget2D* VipPlotPlayer::viewer() const
{
	return d_data->viewer;
}

VipPlotPlayer* VipPlotPlayer::createEmpty() const
{
	VipPlotPlayer* res = new VipPlotPlayer();
	// res->setPlotWidget2D(plotWidget2D()->createEmpty());
	return res;
}

VipAbstractScale* VipPlotPlayer::defaultXAxis() const
{
	if (VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer))
		return pl->area()->bottomAxis();
	return d_data->stdScales.first();
}
VipAbstractScale* VipPlotPlayer::defaultYAxis() const
{
	if (VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(d_data->viewer))
		return pl->area()->leftAxis();
	return d_data->stdScales[1];
}

VipCoordinateSystem::Type VipPlotPlayer::defaultCoordinateSystem() const
{
	return d_data->stdType;
}

VipValueToTimeButton* VipPlotPlayer::valueToTimeButton() const
{
	return d_data->timeUnit;
}

void VipPlotPlayer::setTimeType(VipValueToTime::TimeType type)
{
	if (type != timeType()) {
		d_data->timeUnit->setValueToTime(type);
	}
}

VipValueToTime::TimeType VipPlotPlayer::timeType() const
{
	return d_data->timeUnit->valueToTime();
}

void VipPlotPlayer::setDisplayType(VipValueToTime::DisplayType type)
{
	if (type != displayType()) {
		d_data->timeUnit->setDisplayType(type);
	}
}

VipValueToTime::DisplayType VipPlotPlayer::displayType() const
{
	return d_data->timeUnit->displayType();
}

bool VipPlotPlayer::haveTimeUnit() const
{
	QList<VipPlotItemData*> lst = viewer()->area()->findItems<VipPlotItemData*>();
	for (int i = 0; i < lst.size(); ++i) {
		if (lst[i]->axisUnit(0).text().indexOf("time", 0, Qt::CaseInsensitive) >= 0)
			return true;
	}
	return false;
}

// void VipPlotPlayer::setDisplayTimeAsInteger(bool enable)
//  {
//  d_data->displayTimeAsInteger->setChecked(enable);
//  }
//  bool VipPlotPlayer::displayTimeAsInteger() const
//  {
//  return d_data->displayTimeAsInteger->isChecked();
//  }

QList<VipAbstractScale*> VipPlotPlayer::leftScales() const
{
	QList<VipAbstractScale*> scales = d_data->viewer->area()->scales();
	QList<VipAbstractScale*> res;
	if (qobject_cast<VipPlotArea2D*>(d_data->viewer->area())) {
		for (int i = 0; i < scales.size(); ++i) {
			if (VipAxisBase* ax = qobject_cast<VipAxisBase*>(scales[i]))
				if (ax->alignment() == VipBorderItem::Left)
					res << ax;
		}
	}
	return res;
}

VipAbstractScale* VipPlotPlayer::findYScale(const QString& title) const
{
	QList<VipAbstractScale*> scales = d_data->viewer->area()->scales();

	if (VipPlotArea2D* area = qobject_cast<VipPlotArea2D*>(d_data->viewer->area())) {
		if (title.isEmpty())
			return area->leftAxis();
		for (int i = 0; i < scales.size(); ++i) {
			if (VipAxisBase* ax = qobject_cast<VipAxisBase*>(scales[i]))
				if (ax->orientation() == Qt::Vertical && ax->title().text() == title)
					return ax;
		}
		return nullptr;
	}
	return nullptr;
}

VipAbstractScale* VipPlotPlayer::xScale() const
{
	QList<VipAbstractScale*> scales;
	d_data->viewer->area()->standardScales(scales);
	if (scales.size())
		return scales.first();
	return nullptr;
}

VipInterval VipPlotPlayer::itemsInterval() const
{
	VipInterval res;

	QList<VipPlotItem*> items = d_data->viewer->area()->findItems<VipPlotItem*>(QString(), 2, 1);
	for (int i = 0; i < items.size(); ++i) {
		QList<VipInterval> tmp = items[i]->plotBoundingIntervals();
		if (tmp.isEmpty() || tmp.first().isNull())
			continue;

		if (!res.isValid())
			res = tmp.first();
		else
			res = res.unite(tmp.first());
	}
	return res;
}

static VipPlotPlayer::function_type _function = findBestTimeUnit;

void VipPlotPlayer::setTimeUnitFunction(function_type fun)
{
	_function = fun;

	QList<VipPlotPlayer*> widgets = VipUniqueId::objects<VipPlotPlayer>();
	for (int i = 0; i < widgets.size(); ++i)
		widgets[i]->plotItemAdded(nullptr);
}

VipPlotPlayer::function_type VipPlotPlayer::timeUnitFunction()
{
	return _function;
}

void VipPlotPlayer::removeStyleSheet(VipPlotItem* item)
{
	if (item->styleSheet().size()) {
		item->setStyleSheet(QString());
		// remove also symbol condition
		if (VipPlotCurve* c = qobject_cast<VipPlotCurve*>(item))
			c->setSymbolCondition(QString());
		if (VipDisplayObject* obj = item->property("VipDisplayObject").value<VipDisplayObject*>()) {
			// remove it from source
			QList<VipIODevice*> devs = vipListCast<VipIODevice*>(obj->allSources());
			for (int i = 0; i < devs.size(); ++i) {
				devs[i]->setAttribute("stylesheet", QString());
				devs[i]->reload();
			}
		}
	}
}
void VipPlotPlayer::removeStyleSheet()
{
	// remove style sheet from selected items
	QList<VipPlotItem*> items = plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);
	for (int i = 0; i < items.size(); ++i)
		removeStyleSheet(items[i]);
}

static bool _newItemBehaviorEnabled = true;
void VipPlotPlayer::setNewItemBehaviorEnabled(bool enable)
{
	_newItemBehaviorEnabled = enable;
}
bool VipPlotPlayer::newItemBehaviorEnabled()
{
	return _newItemBehaviorEnabled;
}

void VipPlotPlayer::computePlayerTitle()
{
	if (!automaticWindowTitle())
		return;

	QList<VipPlotItemData*> lst = viewer()->area()->findItems<VipPlotItemData*>();
	if (lst.size()) {
		QString title = lst.first()->title().text();
		QString title2 = lst.first()->property("PlayerName").toString();
		if (!title2.isEmpty())
			title = title2;
		if (title != this->windowTitle())
			this->setWindowTitle(title);
	}
}

void VipPlotPlayer::updateSlidingTimeWindow()
{
	bool visible = false;
	QSet<double> values;

	QList<VipPlotCurve*> curves = plotWidget2D()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
	for (int i = 0; i < curves.size(); ++i) {
		if (VipDisplayObject* disp = curves[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QList<VipProcessingObject*> sources = disp->allSources();
			QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
			bool has_sequential_device = false;
			for (int d = 0; d < devices.size(); ++d) {
				if (devices[d]->deviceType() == VipIODevice::Sequential) {
					has_sequential_device = true;
					break;
				}
			}
			if (has_sequential_device) {
				visible = true;
				QList<VipNumericValueToPointVector*> converts = vipListCast<VipNumericValueToPointVector*>(sources);
				if (converts.size()) {
					double val = converts.last()->propertyAt(0)->value<double>();
					if (val != -1)
						values.insert(converts.last()->propertyAt(0)->value<double>());
				}
				else {
					values.insert(disp->propertyName("Sliding_time_window")->value<double>());
				}
			}
		}
	}

	d_data->plotDurationAction->setVisible(visible);
	if (values.size() == 1) {
		d_data->plotDuration->setValue(*values.begin());
		// set this value to all other curves
		for (int i = 0; i < curves.size(); ++i) {
			if (VipDisplayObject* disp = curves[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
				QList<VipNumericValueToPointVector*> converts = vipListCast<VipNumericValueToPointVector*>(disp->allSources());
				if (converts.size()) {
					converts.last()->propertyAt(0)->setData(*values.begin());
				}
			}
		}
	}
	else
		d_data->plotDuration->setText(QString());
}

void VipPlotPlayer::setSlidingTimeWindow()
{
	// retrieve sliding time value
	double value;
	if (d_data->plotDuration->text().isEmpty())
		value = -1;
	else
		value = d_data->plotDuration->value();
	if (vipIsNan(value))
		value = -1;

	if (!vipIsNan(value)) {
		d_data->plotDuration->blockSignals(true);
		d_data->plotDuration->setValue(value);
		d_data->plotDuration->blockSignals(false);
	}

	QList<VipPlotCurve*> curves = plotWidget2D()->area()->findItems<VipPlotCurve*>(QString(), 2, 1);
	for (int i = 0; i < curves.size(); ++i) {
		if (VipDisplayObject* disp = curves[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QList<VipProcessingObject*> sources = disp->allSources();
			QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
			bool has_sequential_device = false;
			for (int d = 0; d < devices.size(); ++d) {
				if (devices[d]->deviceType() == VipIODevice::Sequential) {
					has_sequential_device = true;
					break;
				}
			}
			if (has_sequential_device) {
				QList<VipNumericValueToPointVector*> converts = vipListCast<VipNumericValueToPointVector*>(sources);
				if (converts.size()) {
					converts.first()->propertyAt(0)->setData(value);
				}
				else {
					disp->propertyName("Sliding_time_window")->setData(value);
				}
			}
		}
	}
}

void VipPlotPlayer::onPlayerCreated()
{
	plotItemAxisUnitChanged(nullptr);
	computePlayerTitle();
	updateSlidingTimeWindow();
	computeStartDate();
	// setProperty("_vip_forceTimeUnit", false);
}

bool VipPlotPlayer::plotItemClicked(VipPlotItem*, VipPlotItem::MouseButton button)
{
	if ( // VipPlotCurve* c = qobject_cast<VipPlotCurve*>(item)
	  true) {
		if (button == VipPlotItem::LeftButton)
			if (QGuiApplication::keyboardModifiers() & Qt::ShiftModifier)
				if (haveTimeUnit())
					if (VipProcessingPool* pool = this->processingPool()) {
						QPoint p = QCursor::pos();
						p = this->plotWidget2D()->mapFromGlobal(p);

						QPointF pf = this->plotWidget2D()->mapToScene(p);
						QList<VipPointVector> points;
						VipBoxStyleList styles;
						QList<int> legends;

						// Retrieve items points close to the mouse
						PlotItemList items = this->plotWidget2D()->area()->plotItems(pf, -1, 10, points, styles, legends);
						// search closest point
						QPointF closest;
						double closest_dist = -1;
						for (int i = 0; i < points.size(); ++i) {
							const VipPointVector vec = points[i];
							for (int j = 0; j < vec.size(); ++j) {
								QPointF ip = items[i]->mapToScene(vec[j]);
								double dist = (ip - pf).manhattanLength();
								if (closest_dist == -1 || dist < closest_dist) {
									closest_dist = dist;
									closest = ip;
								}
							}
						}
						if (closest_dist == -1)
							closest = pf;

						VipPoint scale = this->plotWidget2D()->area()->positionToScale(closest);
						pool->seek(static_cast<qint64>(scale.x()));

						return true;
					}
	}
	return false;
}

void VipPlotPlayer::plotItemAdded(VipPlotItem* item)
{
	if (qobject_cast<VipPlotShape*>(item) || qobject_cast<VipResizeItem*>(item)) {
		plotItemSelectionChanged(nullptr);
		return;
	}
	// reapply normalization and start at zero for the new plot item
	this->normalize(this->isNormalized());
	this->startAtZero(this->isStartAtZero());
	this->startYAtZero(this->isStartYAtZero());

	if (timeUnitFunction() && d_data->timeUnit->automaticUnit() && !this->property("_vip_forceTimeUnit").toBool()) {
		this->setTimeType(timeUnitFunction()(this));
		d_data->timeUnit->setAutomaticUnit(true);
	}
	plotItemAxisUnitChanged(nullptr);

	computePlayerTitle();
	updateSlidingTimeWindow();

	// set the tool tip for histograms
	if (VipPlotHistogram* hist = qobject_cast<VipPlotHistogram*>(item)) {
		hist->setToolTipText("<b>From</b> #min<br><b>To</b> #max<br><b>Values</b>: #value");
	}

	plotItemSelectionChanged(nullptr);
	computeStartDate();
	computeZoom();
}
void VipPlotPlayer::plotItemRemoved(VipPlotItem*)
{
	if (timeUnitFunction())
		this->setTimeType(timeUnitFunction()(this));

	plotItemSelectionChanged(nullptr);
	computePlayerTitle();
	updateSlidingTimeWindow();
	computeStartDate();
	computeZoom();
}

static void __create_processing_menu(VipProcessingObjectMenu* menu, VipPlayer2D* pl, VipPlotItemData* selected)
{
	if (!selected || selected->data().userType() == 0)
		return;
	menu->setProcessingInfos(VipProcessingObject::validProcessingObjects(QVariantList() << selected->data(), 1, VipProcessingObject::DisplayOnDifferentSupport).values());
	// make the processing menu draggable and droppable
	QList<QAction*> acts = menu->processingActions();
	for (int i = 0; i < acts.size(); ++i) {
		const auto lst = VipFDAddProcessingAction().match(acts[i], pl);
		bool applied = false;
		for (int j = 0; j < lst.size(); ++j)
			applied = applied || lst[j](acts[i], pl).value<bool>();
		if (!applied) {
			// make the menu action droppable
			if (VipPlotPlayer* ppl = qobject_cast<VipPlotPlayer*>(pl))
				acts[i]->setProperty("QMimeData",
						     QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipProcessingObject*>>(
						       std::bind(&applyProcessingOnDrop, ppl, acts[i]->property("Info").value<VipProcessingObject::Info>()), VipCoordinateSystem::Cartesian, acts[i])));
		}
		else
			acts[i]->setProperty("_vip_notrigger", true);
	}
}
static void __create_fusion_processing_menu(VipProcessingObjectMenu* menu, VipPlayer2D* pl, QList<VipPlotItemData*> selected)
{
	if (selected.isEmpty())
		return;

	// create the input data list
	QVariantList inputs;
	for (int i = 0; i < selected.size(); ++i) {
		if (VipDisplayObject* display = selected[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			inputs.append(display->inputAt(0)->data().data());
		}
		else
			return;
	}

	menu->setProcessingInfos(VipProcessingObject::validProcessingObjects<VipBaseDataFusion*>(inputs, 1, VipProcessingObject::DisplayOnDifferentSupport).values());
	// make the processing menu draggable and droppable
	QList<QAction*> acts = menu->processingActions();
	for (int i = 0; i < acts.size(); ++i) {
		const auto lst = VipFDAddProcessingAction().match(acts[i], pl);
		bool applied = false;
		for (int j = 0; j < lst.size(); ++j)
			applied = applied || lst[j](acts[i], pl).value<bool>();
		if (!applied) {
			// make the menu action droppable
			if (VipPlotPlayer* ppl = qobject_cast<VipPlotPlayer*>(pl))
				acts[i]->setProperty("QMimeData",
						     QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipProcessingObject*>>(
						       std::bind(&applyProcessingOnDrop, ppl, acts[i]->property("Info").value<VipProcessingObject::Info>()), VipCoordinateSystem::Cartesian, acts[i])));
		}
		else {
			acts[i]->setProperty("_vip_notrigger", true);
		}
	}
}

void VipPlotPlayer::plotItemSelectionChanged(VipPlotItem*)
{
	// get all selected items
	QList<VipPlotItemData*> items = d_data->viewer->area()->findItems<VipPlotItemData*>(QString(), 1, 1);

	// currently only VipPlotCurve supports processings
	int has_valid_items = items.size();
	if (has_valid_items) {
		has_valid_items = 0;
		for (int i = 0; i < items.size(); ++i) {
			if (qobject_cast<VipPlotCurve*>(items[i])) {
				has_valid_items++;
			}
		}
	}

	if (!has_valid_items) {
		d_data->processing_tree_action->setVisible(false);
		d_data->fusion_processing_tree_action->setVisible(false);
	}
	else {
		// check they are all of the same type
		int user_type = items.first()->data().userType();
		for (int i = 1; i < items.size(); ++i)
			if (items[i]->data().userType() != user_type) {
				d_data->processing_tree_action->setVisible(false);
				user_type = 0;
				break;
			}

		if (user_type) {
			// show the processing button and update it
			d_data->processing_tree_action->setVisible(true);
			// make the processing menu draggable and droppable
			__create_processing_menu(d_data->processing_menu.get(), this, items.first());

			if (has_valid_items > 1) {
				// same thing for data fusion processings
				// show the processing button and update it
				d_data->fusion_processing_tree_action->setVisible(true);
				// make the processing menu draggable and droppable
				__create_fusion_processing_menu(d_data->fusion_processing_menu.get(), this, items);
			}
			else
				d_data->fusion_processing_tree_action->setVisible(false);
		}
	}

	// show the hist bins spin box
	QList<VipPlotHistogram*> lst = d_data->viewer->area()->findItems<VipPlotHistogram*>(QString(), 1, 1);
	QList<VipExtractHistogram*> extractHist = QList<VipExtractHistogram*>();
	for (int i = 0; i < lst.size(); ++i)
		if (VipDisplayObject* disp = lst[i]->property("VipDisplayObject").value<VipDisplayObject*>())
			extractHist += vipListCast<VipExtractHistogram*>(disp->allSources());
	if (extractHist.size()) {
		d_data->histBinsAction->setVisible(true);
		d_data->histBins->blockSignals(true);
		d_data->histBins->setValue(extractHist.back()->propertyName("bins")->data().value<int>());
		d_data->histBins->blockSignals(false);
	}
	else
		d_data->histBinsAction->setVisible(false);

	// update the curve editor
	updateCurveEditor();

	// display if needed the source ROI
	QMap<VipVideoPlayer*, QPicture> pictures;
	QMap<VipVideoPlayer*, QPainter*> all_painters;
	for (int i = 0; i < items.size(); ++i)
		if (VipDisplayObject* disp = items[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QVariant v = disp->inputAt(0)->probe().attribute("_vip_sourceROI");
			VipSourceROI s = v.value<VipSourceROI>();
			if (s.player) {
				VipPlotSpectrogram* sp = s.player->spectrogram();
				QPolygonF poly = sp->sceneMap()->transform(s.player->imageTransform().map(s.polygon));

				if (poly.size()) {
					QPicture& pic = pictures[s.player];
					QPainter* p = nullptr;
					if (all_painters.find(s.player) == all_painters.end()) {
						p = all_painters[s.player] = new QPainter();
						p->begin(&pic);
					}
					else
						p = all_painters[s.player];
					p->setPen(Qt::black);
					if (poly.first() == poly.last())
						p->drawPolygon(poly);
					else
						p->drawPolyline(poly);
				}
			}
		}

	auto pit = all_painters.begin();
	for (auto it = pictures.begin(); it != pictures.end(); ++it, ++pit) {
		pit.value()->end();
		it.key()->plotWidget2D()->area()->rubberBand()->setAdditionalPaintCommands(it.value());
		delete pit.value();
	}
}

void VipPlotPlayer::updateCurveEditor()
{
	// show/hide the component choice
	VipDisplayCurve* has_component = nullptr;
	VipDisplayCurve* curve = nullptr;
	if (VipPlotItemData* it = currentPlotItem())
		if (VipDisplayCurve* disp = it->property("VipDisplayObject").value<VipDisplayCurve*>()) {
			curve = disp;
			if (disp->extractComponent()->supportedComponents().size() > 1)
				has_component = disp;
		}

	// set the curve to the curve editor and connect it
	if (d_data->curveEditor->display() && d_data->curveEditor->display() != curve)
		disconnect(d_data->curveEditor->display(), SIGNAL(displayed(const VipAnyDataList&)), this, SLOT(updateCurveEditor()));
	if (curve && d_data->curveEditor->display() != curve) {
		d_data->curveEditor->setDisplay(curve);
		connect(d_data->curveEditor->display(), SIGNAL(displayed(const VipAnyDataList&)), this, SLOT(updateCurveEditor()));
	}
	else if (!curve && d_data->curveEditor->display())
		d_data->curveEditor->setDisplay(nullptr);

	if (has_component) {
		if (!d_data->curveEditorAction->isVisible())
			d_data->curveEditorAction->setVisible(has_component);
	}
	else {
		if (d_data->curveEditorAction->isVisible())
			d_data->curveEditorAction->setVisible(false);
	}
}

void VipPlotPlayer::plotItemAxisUnitChanged(VipPlotItem*)
{
	bool time_unit = this->haveTimeUnit();
	d_data->timeUnitAction->setVisible(time_unit);

	if (!time_unit) {
		d_data->timeUnit->setAutomaticUnit(false);
		this->setTimeType(VipValueToTime::NanoSeconds);
	}
	else if (timeUnitFunction() && !this->property("_vip_forceTimeUnit").toBool()) {
		this->setTimeType(timeUnitFunction()(this));
		d_data->timeUnit->setAutomaticUnit(true);
		this->setProperty("_vip_forceTimeUnit", false);
	}
}

void VipPlotPlayer::histBinsChanged(int value)
{
	QList<VipPlotHistogram*> lst = d_data->viewer->area()->findItems<VipPlotHistogram*>(QString(), 1, 1);
	QList<VipExtractHistogram*> extractHist = QList<VipExtractHistogram*>();
	for (int i = 0; i < lst.size(); ++i)
		if (VipDisplayObject* disp = lst[i]->property("VipDisplayObject").value<VipDisplayObject*>())
			extractHist += vipListCast<VipExtractHistogram*>(disp->allSources());
	for (int i = 0; i < extractHist.size(); ++i) {
		extractHist[i]->propertyName("bins")->setData(value);
		extractHist[i]->reload();
	}
}

VipFunctionDispatcher<1>& vipFDPlayerCreated()
{
	static VipFunctionDispatcher<1> disp;
	return disp;
}

VipFunctionDispatcher<2>& VipFDItemAddedOnPlayer()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& VipFDItemRemovedFromPlayer()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& VipFDItemAxisUnitChanged()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& VipFDItemSelected()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& VipFDItemRightClick()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<2>& VipFDAddProcessingAction()
{
	static VipFunctionDispatcher<2> disp;
	return disp;
}

VipFunctionDispatcher<3>& VipFDDropOnPlotItem()
{
	static VipFunctionDispatcher<3> disp;
	return disp;
}

VipFunctionDispatcher<3>& VipFDPlayerKeyPress()
{
	static VipFunctionDispatcher<3> disp;
	return disp;
}

#include "VipDisplayArea.h"

// default action on right click: menu with actions move to foreground/background.
// if the item is a VipPlotShape, add extract statistics, histogram, polyline

static void moveSelectionToForeground(VipAbstractPlotArea* area)
{
	QList<VipPlotItem*> items = area->findItems<VipPlotItem*>(QString(), 1, 1);
	QList<VipPlotItem*> all = area->plotItems();

	items.removeOne(area->canvas());
	items.removeOne(area->grid());
	all.removeOne(area->canvas());
	all.removeOne(area->grid());
	if (VipImageArea2D* a = qobject_cast<VipImageArea2D*>(area)) {
		items.removeOne(a->spectrogram());
		all.removeOne(a->spectrogram());
	}
	if (items.isEmpty())
		return;

	// compute max z value for all items, excluding selected ones
	double allMaxZ = vipNan();
	for (int i = 0; i < all.size(); ++i)
		if (items.indexOf(all[i]) < 0) {
			if (vipIsNan(allMaxZ))
				allMaxZ = all[i]->zValue();
			else
				allMaxZ = std::max(allMaxZ, all[i]->zValue());
		}

	if (vipIsNan(allMaxZ))
		return;

	// sort items by z value
	QMap<double, VipPlotItem*> sorted;
	for (int i = 0; i < items.size(); ++i)
		sorted.insert(items[i]->zValue(), items[i]);

	allMaxZ += 0.01;

	for (QMap<double, VipPlotItem*>::iterator it = sorted.begin(); it != sorted.end(); ++it, allMaxZ += 0.01)
		it.value()->setZValue(allMaxZ);
}

// default action on right click: menu with actions move to foreground/background
static void moveSelectionToBackground(VipAbstractPlotArea* area)
{
	QList<VipPlotItem*> items = area->findItems<VipPlotItem*>(QString(), 1, 1);
	QList<VipPlotItem*> all = area->plotItems();

	double minZ = area->canvas()->zValue();

	items.removeOne(area->canvas());
	items.removeOne(area->grid());
	all.removeOne(area->canvas());
	all.removeOne(area->grid());
	if (VipImageArea2D* a = qobject_cast<VipImageArea2D*>(area)) {
		items.removeOne(a->spectrogram());
		all.removeOne(a->spectrogram());
		minZ = std::max(minZ, a->spectrogram()->zValue());
	}
	if (items.isEmpty())
		return;

	// compute min z value for all items, excluding selected ones
	double allMinZ = vipNan();
	for (int i = 0; i < all.size(); ++i)
		if (items.indexOf(all[i]) < 0) {
			if (vipIsNan(allMinZ))
				allMinZ = all[i]->zValue();
			else
				allMinZ = std::min(allMinZ, all[i]->zValue());
		}

	if (vipIsNan(allMinZ))
		return;

	// sort items by z value
	QMap<double, VipPlotItem*> sorted;
	for (int i = 0; i < items.size(); ++i)
		sorted.insert(items[i]->zValue(), items[i]);

	allMinZ -= 0.01 + items.size() * 0.01;
	if (allMinZ < minZ)
		allMinZ = minZ;
	for (QMap<double, VipPlotItem*>::iterator it = sorted.begin(); it != sorted.end(); ++it, allMinZ += 0.01)
		it.value()->setZValue(allMinZ);
}

static void extractHistogram(VipPlotShape* shape, VipVideoPlayer* pl)
{
	VipShape sh = shape ? shape->rawData() : VipShape();
	QList<VipDisplayHistogram*> curves = pl->extractHistograms(sh, QString());

	QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessings(vipListCast<VipProcessingObject*>(curves), nullptr);

	if (players.size()) {
		vipGetMainWindow()->openPlayers(players);
	}
	// VipBaseDragWidget * drag = vipCreateFromWidgets(vipListCast<QWidget*>(players));
	//
	//
	// if (VipMultiDragWidget * w = vipCreateFromBaseDragWidget(drag))
	// {
	// if (VipDisplayPlayerArea * disp = VipDisplayPlayerArea::fromChildWidget(pl))
	// {
	// disp->addWidget(w);
	// }
	// }
}

static void extractPolyline(QList<VipPlotShape*> shapes, VipVideoPlayer* pl)
{
	VipShapeList shs; // = shape->rawData();

	for (int i = 0; i < shapes.size(); ++i) {
		VipShape sh = shapes[i]->rawData();
		if (sh.type() == VipShape::Polyline)
			shs.append(sh);
	}

	QList<VipDisplayCurve*> curves = pl->extractPolylines(shs, QString());

	QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessings(vipListCast<VipProcessingObject*>(curves), nullptr);

	if (players.size()) {
		vipGetMainWindow()->openPlayers(players);
	}
}

static void extractPolylineValuesAlongTime(VipPlotShape* shape, VipVideoPlayer* pl)
{

	VipAnyResource* res = pl->extractPolylineValuesAlongTime(shape->rawData());

	QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessings(QList<VipProcessingObject*>() << res, nullptr);

	if (players.size()) {
		vipGetMainWindow()->openPlayers(players);
	}
}

static void extractPixelsCoordinates(VipPlotShape* shape, VipVideoPlayer* pl)
{
	VipShape sh = shape->rawData();
	const QVector<QPoint> tmp = sh.fillPixels();
	QRectF bound;
	const VipNDArray ar = pl->spectrogram()->rawData().extract(pl->spectrogram()->rawData().boundingRect(), &bound);
	const QVector<QPoint> pixels = tmp; // sh.clip(tmp, QRect(bound.toRect().topLeft(), QSize(ar.shape(1), ar.shape(0))));

	QString filename = VipFileDialog::getOpenFileName(nullptr, "Save pixels coordinates", "TEXT file (*.txt)");
	if (!filename.isEmpty()) {
		QFile out(filename);
		if (out.open(QFile::WriteOnly)) {
			QTextStream stream(&out);
			for (int i = 0; i < pixels.size(); ++i) {
				stream << pixels[i].x() << "\t" << pixels[i].y() << "\n";
			}
		}
		else
			VIP_LOG_ERROR("Cannot open file " + filename);
	}
}

VipPlotPlayer* vipExtractTimeTrace(const VipShapeList& shs, VipVideoPlayer* pl, VipShapeStatistics::Statistics stats, int one_frame_out_of, int multi_shapes, VipPlotPlayer* out)
{
	QList<VipProcessingObject*> curves = pl->extractTimeEvolution(shs, stats, one_frame_out_of, multi_shapes);

	QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessings((curves), out);
	if (out) {
		if (players.size())
			return qobject_cast<VipPlotPlayer*>(players.first());
		else
			return nullptr;
	}

	if (players.size()) {
		vipGetMainWindow()->openPlayers(players);
		return qobject_cast<VipPlotPlayer*>(players.first());
	}
	return nullptr;
}

VipPlotPlayer* vipExtractTimeStatistics(VipVideoPlayer* pl)
{
	VipProcessingObject* obj = pl->extractTimeStatistics();
	if (!obj)
		return nullptr;
	QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(obj, nullptr);
	if (players.size()) {
		vipGetMainWindow()->openPlayers(players);
		return qobject_cast<VipPlotPlayer*>(players.first());
	}
	return nullptr;
}

// standard entries on the menu when right clicking on a Plot Item
static QList<QAction*> standardActions(VipPlotItem* item, VipAbstractPlayer* player)
{
	QList<QAction*> actions;

	VipAbstractPlotArea* area = player->plotWidget2D()->area();
	if (item != area->canvas()) {
		QAction* foreground = new QAction(vipIcon("foreground.png"), "Move selection to foreground", nullptr);
		QAction* background = new QAction(vipIcon("background.png"), "Move selection to background", nullptr);

		QObject::connect(foreground, &QAction::triggered, std::bind(moveSelectionToForeground, area));
		QObject::connect(background, &QAction::triggered, std::bind(moveSelectionToBackground, area));
		actions << foreground << background;

		if (VipPlayer2D* p = qobject_cast<VipPlayer2D*>(player)) {
			QList<VipPlotItem*> items = p->plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);
			if (VipMimeDataDuplicatePlotItem::supportSourceItems(items)) {
				QAction* copy = new QAction(vipIcon("copy.png"), "Copy selected items (curves, image, ROIs)", nullptr);
				copy->setProperty("QMimeData", QVariant::fromValue(new VipMimeDataDuplicatePlotItem(items, copy)));
				QObject::connect(copy, SIGNAL(triggered(bool)), player, SLOT(copySelectedItems()));
				actions << copy;
			}
			if (VipPlotItemClipboard::supportDestinationPlayer(p)) {
				QAction* paste = new QAction(vipIcon("paste.png"), "Paste items", nullptr);
				QObject::connect(paste, SIGNAL(triggered(bool)), player, SLOT(pasteItems()));
				actions << paste;
			}

			if (VipPlotItemData* ditem = qobject_cast<VipPlotItemData*>(item))
				if (!qobject_cast<VipPlotShape*>(item)) // do NOT save ROI this way
				{
					QAction* save = new QAction(vipIcon("save.png"), "Save item's content in file...", nullptr);
					QObject::connect(save, &QAction::triggered, std::bind(&VipPlayer2D::saveItemContent, p, ditem, QString()));
					actions << save;
				}

			if (qobject_cast<VipPlotShape*>(item)) {
				QAction* sep = new QAction(nullptr);
				sep->setSeparator(true);
				actions << sep;

				QAction* annot = new QAction("Create annotation...", nullptr);
				QAction* clear_annot = new QAction("Remove annotations", nullptr);
				actions << annot << clear_annot;
				QObject::connect(annot, &QAction::triggered, std::bind(vipEditAnnotations, p));
				QObject::connect(clear_annot, &QAction::triggered, std::bind(vipRemoveAnnotations, p));
			}
		}

		{
			QAction* sep = new QAction(nullptr);
			sep->setSeparator(true);

			actions << sep;
		}

		if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(player)) {
			if (VipPlotShape* shape = qobject_cast<VipPlotShape*>(item)) {
				if (shape->rawData().type() != VipShape::Point) {
					QAction* histogram = new QAction("Extract the shape histogram", nullptr);
					QObject::connect(histogram, &QAction::triggered, std::bind(extractHistogram, shape, pl));
					actions << histogram;

					// make the menu action droppable
					histogram->setProperty("QMimeData",
							       QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipDisplayHistogram*>>(
								 std::bind(&VipVideoPlayer::extractHistograms, pl, shape->rawData(), QString()), VipCoordinateSystem::Cartesian, histogram)));
				}
				if (shape->rawData().type() == VipShape::Polyline) {
					QAction* polyline = new QAction("Extract values along the polyline", nullptr);
					QList<VipPlotShape*> shapes = pl->viewer()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
					QObject::connect(polyline, &QAction::triggered, std::bind(extractPolyline, shapes, pl));
					actions << polyline;

					if (shapes.size() == 1) {
						QAction* time_polyline = new QAction("Extract time trace of values along the polyline", nullptr);
						QObject::connect(time_polyline, &QAction::triggered, std::bind(extractPolylineValuesAlongTime, shapes.first(), pl));
						actions << time_polyline;

						// make the menu action droppable
						time_polyline->setProperty(
						  "QMimeData",
						  QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipAnyResource*>(
						    std::bind(&VipVideoPlayer::extractPolylineValuesAlongTime, pl, shapes.first()->rawData()), VipCoordinateSystem::Cartesian, time_polyline)));
					}

					QAction* extract_coordinates = new QAction("Extract pixels coordinates along the polyline", nullptr);
					QObject::connect(extract_coordinates, &QAction::triggered, std::bind(extractPixelsCoordinates, shape, pl));
					actions << extract_coordinates;

					VipShapeList shs;
					for (int i = 0; i < shapes.size(); ++i) {
						VipShape sh = shapes[i]->rawData();
						if (sh.type() == VipShape::Polyline)
							shs.append(sh);
					}

					// make the menu action droppable
					polyline->setProperty("QMimeData",
							      QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipDisplayCurve*>>(
								std::bind(&VipVideoPlayer::extractPolylines, pl, shs, QString()), VipCoordinateSystem::Cartesian, polyline)));
				}

				if (shape->type() != VipShape::Unknown && pl->array().canConvert<double>()) {
					QAction* time_trace = new QAction("Extract the shape time trace", nullptr);
					QObject::connect(time_trace,
							 &QAction::triggered,
							 std::bind(vipExtractTimeTrace, pl->findSelectedShapes(1, 1), pl, VipShapeStatistics::Statistics(), 1, 2, (VipPlotPlayer*)nullptr));
					actions << time_trace;

					// make the menu action droppable
					time_trace->setProperty(
					  "QMimeData",
					  QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipProcessingObject*>>(std::bind(&VipVideoPlayer::extractTimeEvolution,
																	       pl,
																	       pl->findSelectedShapes(1, 1) // shape->rawData()
																	       ,
																	       VipShapeStatistics::Statistics(),
																	       1,
																	       2,
																	       QVector<double>()),
																     VipCoordinateSystem::Cartesian,
																     time_trace)));

					if (pl->contourLevels().size() == 1) {
						const VipNDArray img = pl->array();
						if (img.isNumeric()) {

							QPoint img_pos = pl->globalPosToImagePos(QCursor::pos());
							if (img_pos.x() >= 0 && img_pos.y() >= 0 && img_pos.x() < img.shape(1) && img_pos.y() < img.shape(0)) {
								// QMetaObject::invokeMethod(pl,"createShapeFromIsoLine",Qt::DirectConnection,Q_ARG(QPoint,img_pos));
								QAction* _shape = new QAction("Update ROI from iso line", nullptr);
								QObject::connect(_shape, &QAction::triggered, std::bind(&VipVideoPlayer::updateShapeFromIsoLine, pl, img_pos));
								actions << _shape;
							}
						}
					}
				}
			}
			else {
				QAction* histogram = new QAction("Extract the full image histogram", nullptr);
				QObject::connect(histogram, &QAction::triggered, std::bind(extractHistogram, (VipPlotShape*)nullptr, pl));
				actions << histogram;

				// make the menu action droppable
				histogram->setProperty("QMimeData",
						       QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<QList<VipDisplayHistogram*>>(
							 std::bind(&VipVideoPlayer::extractHistograms, pl, VipShape(), QString()), VipCoordinateSystem::Cartesian, histogram)));

				if (pl->contourLevels().size() == 1) {
					const VipNDArray img = pl->array();
					if (img.isNumeric()) {
						QPoint img_pos = pl->globalPosToImagePos(QCursor::pos());
						if (img_pos.x() >= 0 && img_pos.y() >= 0 && img_pos.x() < img.shape(1) && img_pos.y() < img.shape(0)) {
							// QMetaObject::invokeMethod(pl,"createShapeFromIsoLine",Qt::DirectConnection,Q_ARG(QPoint,img_pos));
							QAction* _shape = new QAction("Create ROI from iso line", nullptr);
							QObject::connect(_shape, &QAction::triggered, std::bind(&VipVideoPlayer::createShapeFromIsoLine, pl, img_pos));
							actions << _shape;
						}
					}
				}
			}

			QAction* time_stat = new QAction("Extract the cumulated maximum image", nullptr);
			QObject::connect(time_stat, &QAction::triggered, std::bind(vipExtractTimeStatistics, pl));
			actions << time_stat;

			// make the menu action droppable
			time_stat->setProperty("QMimeData",
					       QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipProcessingObject*>(
						 std::bind(&VipVideoPlayer::extractTimeStatistics, pl), VipCoordinateSystem::Cartesian, time_stat)));
		}
	}
	else if (VipPlayer2D* p = qobject_cast<VipPlayer2D*>(player))
		if (VipPlotItemClipboard::supportDestinationPlayer(p)) {
			QAction* paste = new QAction(vipIcon("paste.png"), "Paste items", nullptr);
			QObject::connect(paste, SIGNAL(triggered(bool)), player, SLOT(pasteItems()));
			actions << paste;
		}

	// special plot player actions
	if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(player)) {
		QAction* x_scale = new QAction("Apply x scale to all other players", nullptr);
		QAction* y_scale = new QAction("Apply y scale to all other players", nullptr);
		actions << x_scale << y_scale;
		// separator
		actions << new QAction(nullptr);
		actions.last()->setSeparator(true);
		QAction* undo = new QAction(vipIcon("undo.png"), "Undo last scale change", nullptr);
		QAction* redo = new QAction(vipIcon("redo.png"), "Redo last scale change", nullptr);
		actions << undo << redo;
		// separator
		actions << new QAction(nullptr);
		actions.last()->setSeparator(true);
		QAction* autoscale = new QAction(vipIcon("axises.png"), "Automatic scaling", nullptr);
		actions << autoscale;
		// separator
		actions << new QAction(nullptr);
		actions.last()->setSeparator(true);
		QAction* x_zoom = new QAction( // vipIcon("zoom_h.png"),
		  "Allow x zooming",
		  nullptr);
		QAction* y_zoom = new QAction( // vipIcon("zoom_v.png"),
		  "Allow y zooming",
		  nullptr);
		QAction* selectionZoomArea = new QAction(vipIcon("zoom_area.png"), "Selection zooming", nullptr);
		actions << x_zoom << y_zoom << selectionZoomArea;
		x_zoom->setCheckable(true);
		y_zoom->setCheckable(true);
		selectionZoomArea->setCheckable(true);
		selectionZoomArea->setChecked(pl->isSelectionZoomAreaEnabled());
		x_zoom->setChecked(pl->isHZoomEnabled());
		y_zoom->setChecked(pl->isVZoomEnabled());

		// add remove style sheet
		bool has_stylesheet = false;
		QList<VipPlotItem*> lst = player->plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);
		for (int i = 0; i < lst.size(); ++i)
			if (lst[i]->styleSheet().size()) {
				has_stylesheet = true;
				break;
			}
		if (has_stylesheet) {
			QAction* st = new QAction("Remove item(s) style sheet", nullptr);
			QObject::connect(st, SIGNAL(triggered(bool)), player, SLOT(removeStyleSheet()));
			actions << st;
		}

		QAction::connect(x_scale, SIGNAL(triggered(bool)), pl, SLOT(xScaleToAll()));
		QAction::connect(y_scale, SIGNAL(triggered(bool)), pl, SLOT(yScaleToAll()));
		QAction::connect(undo, SIGNAL(triggered(bool)), pl->plotWidget2D()->area(), SLOT(undoScalesState()));
		QAction::connect(redo, SIGNAL(triggered(bool)), pl->plotWidget2D()->area(), SLOT(redoScalesState()));
		QAction::connect(autoscale, SIGNAL(triggered(bool)), pl, SLOT(autoScale()));
		QAction::connect(x_zoom, SIGNAL(triggered(bool)), pl, SLOT(enableHZoom(bool)));
		QAction::connect(y_zoom, SIGNAL(triggered(bool)), pl, SLOT(enableVZoom(bool)));
		QAction::connect(selectionZoomArea, SIGNAL(triggered(bool)), pl, SLOT(selectionZoomArea(bool)));
	}

	// add the tool tip menu
	if (VipPlayer2D* p = qobject_cast<VipPlayer2D*>(player)) {
		QAction* tool_tip = new QAction("Tool tip management", nullptr);
		tool_tip->setMenu(p->generateToolTipMenu());
		actions << tool_tip;
	}

	return actions;
}

static QList<QAction*> applyDataProcessing(VipPlotItemData* item, VipPlayer2D* player)
{
	// add a submenu displaying a list of processings to apply to the item's data
	if (item->data().userType() && item->isSelected()) {
		VipProcessingObjectMenu* menu = new VipProcessingObjectMenu();
		if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(player))
			// specific menu for VipVideoPlayer
			__create_video_processing_menu(menu, pl);
		else
			__create_processing_menu(menu, player, item);

		QObject::connect(menu, SIGNAL(selected(const VipProcessingObject::Info&)), player, SLOT(addSelectedProcessing(const VipProcessingObject::Info&)));

		QAction* act = new QAction("Apply simple processing", nullptr);
		act->setToolTip("Add a new processing to this item's processing list");
		act->setMenu(menu);
		return QList<QAction*>() << act;
	}
	return QList<QAction*>();
}

#include "VipCore.h"
#include <iostream>

static void setAxesTitle(VipPlotItem* // item
			 ,
			 VipPlotPlayer* player,
			 VipPlotItem* exclude)
{
	if (!VipPlotPlayer::newItemBehaviorEnabled())
		return;
	//'exclude' is nullptr if the item is added, non nullptr otherwise

	// block signals to avoid emitting itemRemoved() and itemAdded() which lead to an infinite loop
	// player->plotWidget2D()->area()->blockSignals(true);

	// retrieve all vertical scales
	QList<VipAxisBase*> axes = vipListCast<VipAxisBase*>(player->plotWidget2D()->area()->scales());
	QList<VipAxisBase*> left_axes;
	QList<VipAxisBase*> right_axes;
	QMap<QString, VipAxisBase*> named_left_scales;
	for (int i = 0; i < axes.size(); ++i) {
		if (axes[i]->orientation() != Qt::Vertical) {
			axes.removeAt(i);
			--i;
		}
		else {
			if (axes[i]->alignment() == VipBorderItem::Left) {
				left_axes << axes[i];
				named_left_scales[axes[i]->title().text()] = axes[i];
			}
			else
				right_axes << axes[i];
		}
	}

	QList<VipPlotCurve*> items = vipListCast<VipPlotCurve*>(player->plotWidget2D()->area()->plotItems());
	for (int i = 0; i < items.size(); ++i) {
		VipPlotCurve* it = items[i];
		if (it == exclude)
			continue;

		// get the y unit
		QString y_unit = it->axisUnit(1).text();
		if (VipDisplayObject* obj = it->property("VipDisplayObject").value<VipDisplayObject*>()) {
			QString tmp = obj->inputAt(0)->probe().attribute("YUnit").toString();
			if (!tmp.isEmpty())
				y_unit = tmp;
		}
		if (y_unit.isEmpty())
			// empty y unit, do nothing for this item
			continue;

		if (it->axes()[1]->title().text() == y_unit)
			// already good
			continue;
		if (it->axes()[1]->title().text().isEmpty()) {
			// axis with no title, just change it
			it->setAxisUnit(1, y_unit);
			it->axes()[1]->setTitle(y_unit);
			continue;
		}

		// For now, NEVER rename an axis once it has a title
		// find related curves (sharing the same axes)
		// QList<VipPlotCurve*> related_curves = vipListCast<VipPlotCurve*>(
		//  it->axes()[0]->plotItems().toSet().intersect(it->axes()[1]->plotItems().toSet()).toList());
		//  related_curves.removeOne(it);
		//  if (related_curves.isEmpty()) {
		//  //there are no other curves sharing the same axis, we can rename it safely
		//  it->setAxisUnit(1, y_unit);
		//  it->axes()[1]->setTitle(y_unit);
		//  continue;
		//  }

		if (it->property("_vip_created").toBool()) {
			it->setProperty("_vip_created", false);
			// the item was just dropped or created, it can trigger an axis creation or can be moved
			// find left scale with this unit
			QMap<QString, VipAxisBase*>::const_iterator found = named_left_scales.find(y_unit);
			if (found != named_left_scales.end()) {
				// move to existing left axis
				it->setAxes(QList<VipAbstractScale*>() << it->axes()[0] << found.value(), it->coordinateSystemType());
			}
			else {
				// create axis
				VipAxisBase* axis = new VipAxisBase(VipBorderItem::Left);
				axis->setTitle(VipText(y_unit));
				axis->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
				axis->setRenderHints(QPainter::TextAntialiasing);
				axis->setMargin(0);
				axis->setMaxBorderDist(0, 0);
				axis->setZValue(20);
				axis->setExpandToCorners(true);
				player->plotWidget2D()->area()->blockSignals(true);
				player->plotWidget2D()->area()->addScale(axis, true);
				player->plotWidget2D()->area()->blockSignals(false);
				// create a new VipPlotSceneModel for this scale
				VipPlotSceneModel* sm = player->createPlotSceneModel(QList<VipAbstractScale*>() << player->xScale() << axis, VipCoordinateSystem::Cartesian);
				sm->setDrawComponent("All", VipPlotShape::FillPixels, false);
				left_axes << axis;
				it->setAxes(QList<VipAbstractScale*>() << it->axes()[0] << axis, it->coordinateSystemType());
			}
		}
	}

	// remove all unused axes
#ifndef _VIP_USE_LEFT_SCALE_ONLY
	for (int i = 0; i < axes.size(); ++i) {
		if (!axes[i]->plotItems().size() && !axes[i]->synchronizedWith().size()) {
			right_axes.removeOne(axes[i]);
			player->plotWidget2D()->area()->removeScale(axes[i]);
			axes[i]->deleteLater();
		}
	}
#else
	// remove unused left scale
	if (left_axes.size() > 1) {
		for (int i = 0; i < left_axes.size(); ++i) {
			// for stacked plots:
			// if a plot area just has 4 items (the grid, canvas, plot scene model and item being deleted), remove it.
			// not that 'exclude' is not nullptr only when removing an item.

			// get all items belonging to this scale and remove the VipPlotMarker, VipPlotShape and VipResizeItem
			QList<VipPlotItem*> tmp_items = left_axes[i]->plotItems();
			for (int j = 0; j < tmp_items.size(); ++j)
				if (qobject_cast<VipPlotMarker*>(tmp_items[j]) || qobject_cast<VipPlotShape*>(tmp_items[j]) || qobject_cast<VipResizeItem*>(tmp_items[j])) {
					tmp_items.removeAt(j);
					--j;
				}

			if (tmp_items.size() <= 4 && tmp_items.indexOf(exclude) >= 0) {
				player->removeLeftScale(left_axes[i]);
			}
		}
	}

#endif

	// hide the first right axis (the default one synchronized with the left axis)
#ifndef _VIP_USE_LEFT_SCALE_ONLY
	right_axes.first()->setVisible(right_axes.size() == 1);
#endif

	// player->plotWidget2D()->area()->blockSignals(false);

	player->plotWidget2D()->area()->recomputeGeometry();
}

static void setAxesTitleUnitChanged(VipPlotItem* item, VipPlotPlayer* player)
{
	setAxesTitle(item, player, nullptr);
}

static void setAxesTitleItemAdded(VipPlotItem* item, VipPlotPlayer* player)
{
	setAxesTitle(item, player, nullptr);
}

static void setAxesTitleItemRemoved(VipPlotItem* item, VipPlotPlayer* player)
{
	setAxesTitle(item, player, item);
}

static void setVideoPlayer(VipDragWidget*, VipVideoPlayer* player)
{

	VipAxisColorMap* map = static_cast<VipImageArea2D*>(player->plotWidget2D()->area())->colorMapAxis();

	QAction* show_scale = new QAction(vipIcon("scalevisible.png"), "Show/hide color scale", player);
	show_scale->setCheckable(true);
	show_scale->setChecked(map->isVisible());
	player->setProperty("show_scale", QVariant::fromValue(show_scale));
	QAction* auto_scale = new QAction(vipIcon("scaleauto.png"), "Toogle auto scaling", player);
	auto_scale->setCheckable(true);
	auto_scale->setChecked(map->isAutoScale());
	player->setProperty("auto_scale", QVariant::fromValue(auto_scale));
	QAction* fit_to_grip = new QAction(vipIcon("fit_to_scale.png"), "Fit color scale to grips", player);
	player->setProperty("fit_to_grip", QVariant::fromValue(fit_to_grip));
	QAction* histo_scale = new QAction(vipIcon("scalehisto.png"), "Adjust color scale to have the best dynamic", player);
	histo_scale->setCheckable(true);
	histo_scale->setChecked(map->useFlatHistogram());
	player->setProperty("histo_scale", QVariant::fromValue(histo_scale));
	QAction* scale_params = new QAction(vipIcon("scaletools.png"), "Display color scale parameters", player);
	player->setProperty("scale_params", QVariant::fromValue(scale_params));

	VipColorScaleButton* scale = new VipColorScaleButton();
	scale->setColorPalette(player->colorMap());

	QAction* sep;
	if (player->afterTitleToolBar()->actions().size())
		sep = player->afterTitleToolBar()->insertSeparator(player->afterTitleToolBar()->actions().first());
	else
		sep = player->afterTitleToolBar()->addSeparator();
	player->setProperty("scale_sep", QVariant::fromValue(sep));
	QAction* sc = player->afterTitleToolBar()->insertWidget(player->afterTitleToolBar()->actions().first(), scale);
	player->setProperty("scale", QVariant::fromValue(sc));
	player->afterTitleToolBar()->insertAction(sc, scale_params);
	player->afterTitleToolBar()->insertAction(scale_params, histo_scale);
	player->afterTitleToolBar()->insertAction(histo_scale, fit_to_grip);
	player->afterTitleToolBar()->insertAction(fit_to_grip, auto_scale);
	player->afterTitleToolBar()->insertAction(auto_scale, show_scale);

	QObject::connect(show_scale, SIGNAL(triggered(bool)), player, SLOT(setColorScaleVisible(bool)));
	QObject::connect(auto_scale, SIGNAL(triggered(bool)), player, SLOT(setAutomaticColorScale(bool)));
	QObject::connect(fit_to_grip, SIGNAL(triggered(bool)), player, SLOT(fitColorScaleToGrips()));
	QObject::connect(histo_scale, SIGNAL(triggered(bool)), player, SLOT(setFlatHistogramColorScale(bool)));
	QObject::connect(scale_params, SIGNAL(triggered(bool)), player, SLOT(showColorScaleParameters()));

	QObject::connect(map, SIGNAL(visibilityChanged(bool)), show_scale, SLOT(setChecked(bool)));
	QObject::connect(map, SIGNAL(autoScaleChanged(bool)), auto_scale, SLOT(setChecked(bool)));
	QObject::connect(map, SIGNAL(useFlatHistogramChanged(bool)), histo_scale, SLOT(setChecked(bool)));

	QObject::connect(scale, SIGNAL(colorPaletteChanged(int)), player, SLOT(setColorMap(int)));
	QObject::connect(player, SIGNAL(colorMapChanged(int)), scale, SLOT(setColorPalette(int)));
}

static void setPlotPlayer(VipDragWidget*, VipPlotPlayer* player)
{

	QAction* params = new QAction(vipIcon("scaletools.png"), "Display curve parameters", player);
	if (player->afterTitleToolBar()->actions().size())
		player->afterTitleToolBar()->insertAction(player->afterTitleToolBar()->actions().first(), params);
	else
		player->afterTitleToolBar()->addAction(params);
	QObject::connect(params, SIGNAL(triggered(bool)), player, SLOT(showParameters()));
}

static int registerStandardFunctions()
{
	VipFDItemRightClick().append<QList<QAction*>(VipPlotItem*, VipAbstractPlayer*)>(standardActions);
	VipFDItemRightClick().append<QList<QAction*>(VipPlotItemData*, VipPlayer2D*)>(applyDataProcessing);
	VipFDItemAddedOnPlayer().append<void(VipPlotItem*, VipPlotPlayer*)>(setAxesTitleItemAdded);
	VipFDItemAxisUnitChanged().append<void(VipPlotItem*, VipPlotPlayer*)>(setAxesTitleUnitChanged);
	VipFDItemRemovedFromPlayer().append<void(VipPlotItem*, VipPlotPlayer*)>(setAxesTitleItemRemoved);
	vipSetDragWidget().append<void(VipDragWidget*, VipVideoPlayer*)>(setVideoPlayer);
	vipSetDragWidget().append<void(VipDragWidget*, VipPlotPlayer*)>(setPlotPlayer);
	return 0;
}

static int _registerStandardFunctions = vipAddInitializationFunction(registerStandardFunctions);

static void saveVipPlayerToolTip(VipArchive& arch)
{
	arch.start("VipPlayerToolTip");
	const QMap<QString, VipToolTip::DisplayFlags> flags = VipPlayerToolTip::allToolTipFlags();
	arch.content("count", flags.size());
	for (QMap<QString, VipToolTip::DisplayFlags>::const_iterator it = flags.begin(); it != flags.end(); ++it) {
		arch.content("name", it.key());
		arch.content("value", (int)it.value());
	}
	arch.end();
}

static void loadVipPlayerToolTip(VipArchive& arch)
{
	if (arch.start("VipPlayerToolTip")) {
		QMap<QString, VipToolTip::DisplayFlags> flags;
		int count = arch.read("count").toInt();
		for (int i = 0; i < count; ++i) {
			QString name = arch.read("name").toString();
			VipToolTip::DisplayFlags fgs = (VipToolTip::DisplayFlags)arch.read("value").toInt();
			flags[name] = fgs;
		}
		VipPlayerToolTip::setAllToolTipFlags(flags);
		arch.end();
	}
}

static VipArchive& operator<<(VipArchive& arch, const VipAbstractPlayer* value)
{
	vipSaveCustomProperties(arch, value);

	arch.content("automaticWindowTitle", value->automaticWindowTitle());
	arch.content("windowTitle", value->windowTitle());
	return arch;
}
static VipArchive& operator>>(VipArchive& arch, VipAbstractPlayer* value)
{
	vipLoadCustomProperties(arch, value);

	arch.save();
	bool automaticWindowTitle;
	QString windowTitle;
	if (arch.content("automaticWindowTitle", automaticWindowTitle)) {
		arch.content("windowTitle", windowTitle);
		if (!automaticWindowTitle) {
			value->setAutomaticWindowTitle(automaticWindowTitle);
			value->setWindowTitle(windowTitle);
		}
	}
	else
		arch.restore();

	return arch;
}

static VipArchive& operator<<(VipArchive& arch, VipPlayer2D* value)
{
	// TODO:CHANGESCENEMODEL
	// serialize the scene model Id to set it back when loading the archive
	return arch.content("scene_model_id", VipUniqueId::id(value->plotSceneModel()->sceneModel().shapeSignals()));
}

static VipArchive& operator>>(VipArchive& arch, VipPlayer2D* value)
{
	// TODO:CHANGESCENEMODEL
	int id = arch.read("scene_model_id").toInt();
	if (id >= 0) {
		VipUniqueId::setId(value->plotSceneModel()->sceneModel().shapeSignals(), id);
	}
	return arch;
}

struct DownsampledImage
{
	VipNDArrayShape shape;
	VipNDArray downsampled;
	VipNDArray toArray() const
	{
		if (!downsampled.isEmpty())
			return downsampled.resize(shape, Vip::LinearInterpolation);
		return VipNDArray();
	}
	static DownsampledImage fromArray(const VipNDArray& ar)
	{
		DownsampledImage res;
		res.shape = ar.shape();
		VipNDArrayShape down = res.shape;
		for (int i = 0; i < down.size(); ++i) {
			if (down[i] > 8)
				down[i] /= 8;
			else
				down[i] = 1;
		}
		res.downsampled = ar.resize(down, Vip::LinearInterpolation);
		return res;
	}
};
Q_DECLARE_METATYPE(DownsampledImage)

static QDataStream& operator<<(QDataStream& stream, const DownsampledImage& img)
{
	return stream << img.shape << img.downsampled;
}
static QDataStream& operator>>(QDataStream& stream, DownsampledImage& img)
{
	return stream >> img.shape >> img.downsampled;
}
static QTextStream& operator<<(QTextStream& stream, const DownsampledImage& img)
{
	for (int i = 0; i < img.shape.size(); ++i)
		stream << img.shape[i] << "\t";
	stream << "\n";
	return stream << img.downsampled;
}
static QTextStream& operator>>(QTextStream& stream, DownsampledImage& img)
{
	QString line = stream.readLine();
	{
		QTextStream str(line.toLatin1());
		img.shape.clear();
		while (true) {
			int v;
			str >> v;
			if (str.status() != QTextStream::Ok)
				break;
			img.shape.push_back(v);
		}
	}
	return stream >> img.downsampled;
}
VipNDArray toArray(const DownsampledImage& img)
{
	return img.toArray();
}

static int registerDownsampledImage()
{
	qRegisterMetaType<VipSourceROI>();
	qRegisterMetaType<DownsampledImage>();
	qRegisterMetaTypeStreamOperators<DownsampledImage>();
	QMetaType::registerConverter<DownsampledImage, VipNDArray>(toArray);
	QMetaType::registerConverter<DownsampledImage, QString>(detail::typeToString<DownsampledImage>);
	QMetaType::registerConverter<DownsampledImage, QByteArray>(detail::typeToByteArray<DownsampledImage>);
	QMetaType::registerConverter<QString, DownsampledImage>(detail::stringToType<DownsampledImage>);
	QMetaType::registerConverter<QByteArray, DownsampledImage>(detail::byteArrayToType<DownsampledImage>);
	return 0;
}
static int _registerDownsampledImage = registerDownsampledImage();

static VipArchive& operator<<(VipArchive& arch, VipVideoPlayer* value)
{
	// save the standard scales (already allocated in VipPlayer2D constructor)
	QList<VipAbstractScale*> std_scales = QList<VipAbstractScale*>() << value->viewer()->area()->bottomAxis() << value->viewer()->area()->topAxis() << value->viewer()->area()->leftAxis()
									 << value->viewer()->area()->rightAxis();

	for (int i = 0; i < std_scales.size(); ++i)
		arch.content("scale", std_scales[i]);

	// save canvas, grid and plotSceneModel
	arch.content("canvas", value->plotWidget2D()->area()->canvas());
	arch.content("grid", value->plotWidget2D()->area()->grid());
	arch.content("plotSceneModel", value->plotSceneModel());
	arch.content("imageTransform", value->imageTransform()); // save the image tansform

	// mark all items from the scene model with "_vip_no_serialize"
	value->plotSceneModel()->setProperty("_vip_no_serialize", true);
	for (int i = 0; i < value->plotSceneModel()->count(); ++i) {
		if (VipPlotShape* sh = qobject_cast<VipPlotShape*>(value->plotSceneModel()->at(i))) {
			sh->setProperty("_vip_no_serialize", true);
			if (VipResizeItem* re = (sh->property("VipResizeItem").value<VipResizeItemPtr>()))
				re->setProperty("_vip_no_serialize", true);
		}
	}

	// save show axes
	arch.content("showAxes", value->isShowAxes());

	// save the color map
	arch.content("colorMap", value->viewer()->area()->colorMapAxis());
	// save the spectrogram
	arch.content("spectrogram", value->spectrogram());

	// save the spectrogram image
	arch.content("image", DownsampledImage::fromArray(value->viewer()->area()->array()));

	// save auto scaling
	arch.content("auto_scale", value->isAutomaticColorScale());
	// save color scale visible
	arch.content("color_scale_visible", value->isColorScaleVisible());

	// save contour levels
	arch.content("contour_levels", value->contourLevels());

	// Since 3.8.0
	arch.content("isFlatHistogramColorScale", value->isFlatHistogramColorScale());
	arch.content("flatHistogramStrength", value->flatHistogramStrength());

	// save all VipPlotItem except canvas and grid
	QList<VipPlotItem*> items = value->viewer()->area()->plotItems();
	items.removeOne(value->viewer()->area()->canvas());
	items.removeOne(value->viewer()->area()->grid());
	items.removeOne(value->spectrogram());
	items.removeOne(value->plotSceneModel());

	arch.start("items");

	for (int i = 0; i < items.size(); ++i) {
		if (VipPlotItemComposite* it = items[i]->property("VipPlotItemComposite").value<VipPlotItemComposite*>()) {
			if (!it->property("_vip_no_serialize").toBool())
				arch.content(items[i]);
		}
		else if (!items[i]->property("_vip_no_serialize").toBool())
			arch.content(items[i]);
	}

	arch.end();

	arch.content("rect", value->visualizedImageRect());

	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipVideoPlayer* value)
{

	// load standard axes
	QList<VipAbstractScale*> std_scales = QList<VipAbstractScale*>() << value->viewer()->area()->bottomAxis() << value->viewer()->area()->topAxis() << value->viewer()->area()->leftAxis()
									 << value->viewer()->area()->rightAxis();

	for (int i = 0; i < std_scales.size(); ++i)
		arch.content("scale", std_scales[i]);

	// load canvas, grid and plotSceneModel
	arch.content("canvas", value->viewer()->area()->canvas());
	arch.content("grid", value->viewer()->area()->grid());
	arch.content("plotSceneModel", value->plotSceneModel());

	// new in 2.2.17
	arch.save();
	QTransform imageTransform;
	// read the image transform, invert it and apply it to the scene model
	if (arch.content("imageTransform", imageTransform)) {
		imageTransform = imageTransform.inverted();
		value->plotSceneModel()->sceneModel().transform(imageTransform);
	}
	else
		arch.restore();

	value->showAxes(arch.read("showAxes").toBool());

	arch.save();
	// load the color map
	bool ok = (bool)arch.content("colorMap", value->viewer()->area()->colorMapAxis());
	// load the spectrogram
	ok = ok && (bool)arch.content("spectrogram", value->spectrogram());
	if (!ok) {
		// 2.2.16 and below: read spectrogram before
		arch.restore();
		ok = arch.content("spectrogram", value->spectrogram());
		ok = ok && arch.content("colorMap", value->viewer()->area()->colorMap());
	}

	value->spectrogram()->setColorMap(value->viewer()->area()->colorMapAxis());

	value->viewer()->area()->setArray(arch.read("image").value<VipNDArray>(), QPointF(0, 0));

	arch.save();
	bool auto_scale = true;
	bool color_scale_visible = true;
	if (arch.content("auto_scale", auto_scale))
		value->setAutomaticColorScale(auto_scale);
	else
		arch.restore();
	arch.save();
	if (arch.content("color_scale_visible", color_scale_visible))
		value->setColorScaleVisible(color_scale_visible);
	else
		arch.restore();

	// load contour levels
	DoubleList contour_levels;
	arch.save();
	if (arch.content("contour_levels", contour_levels))
		value->setContourLevels(contour_levels);
	else
		arch.restore();

	// Since 3.8.0
	arch.save();
	bool isFlatHistogramColorScale;
	int flatHistogramStrength;
	if (arch.content("isFlatHistogramColorScale", isFlatHistogramColorScale)) {
		arch.content("flatHistogramStrength", flatHistogramStrength);
		value->setFlatHistogramColorScale(isFlatHistogramColorScale);
		value->setFlatHistogramStrength(flatHistogramStrength);
	}
	else
		arch.restore();

	// load additional items
	QList<VipPlotItem*> items;

	if (arch.start("items")) {

		while (true) {
			VipPlotItem* item = arch.read().value<VipPlotItem*>();
			if (item)
				items << item;
			else
				break;
		}
		arch.end();
		value->setPendingVisualizedImageRect(arch.read("rect").toRectF());
	}

	arch.resetError();
	return arch;
}

static void markSceneModelNoSerialize(VipPlotSceneModel* sm)
{
	sm->setProperty("_vip_no_serialize", true);
	// mark all items from the scene model with "_vip_no_serialize"
	for (int i = 0; i < sm->count(); ++i) {
		if (VipPlotShape* sh = qobject_cast<VipPlotShape*>(sm->at(i))) {
			sh->setProperty("_vip_no_serialize", true);
			if (VipResizeItem* re = (sh->property("VipResizeItem").value<VipResizeItemPtr>()))
				re->setProperty("_vip_no_serialize", true);
		}
	}
}

static VipArchive& operator<<(VipArchive& arch, VipPlotPlayer* value)
{
	// save the standard scales (already allocated in VipPlayer2D constructor)

	if (VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(value->viewer())) {
		QList<VipAbstractScale*> std_scales = QList<VipAbstractScale*>() << pl->area()->bottomAxis() << pl->area()->topAxis() << pl->area()->leftAxis() << pl->area()->rightAxis();

		for (int i = 0; i < std_scales.size(); ++i)
			arch.content("scale", std_scales[i]);
	}

	// save the additional scales
	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(value->plotWidget2D()->area())) {
		arch.start("MultiAxes");
		VipMultiAxisBase* left = area->leftMultiAxis();
		for (int i = 1; i < left->count(); ++i) {
			arch.content(left->at(i));
		}

		arch.end();
	}

	// save canvas, grid and plotSceneModel, time marker
	arch.content("canvas", value->viewer()->area()->canvas());
	arch.content("grid", value->viewer()->area()->grid());
	arch.content("plotSceneModel", value->plotSceneModel());
	arch.content("timeMarker", value->timeMarker());
	arch.content("legendPosition", (int)value->legendPosition());
	arch.content("autoScale", value->isAutoScale());
	arch.content("isHZoomEnabled", value->isHZoomEnabled());
	arch.content("isVZoomEnabled", value->isVZoomEnabled());

	// mark the xMarker as not serializable
	value->xMarker()->setProperty("_vip_no_serialize", true);

	// mark all items from the scene model with "_vip_no_serialize"
	markSceneModelNoSerialize(value->plotSceneModel());

	// New in 2.2.17
	// do the same for all other VipPlotSceneModel and save them
	QList<VipAbstractScale*> left = value->leftScales();
	arch.start("sceneModels");
	for (int i = 0; i < left.size(); ++i) {
		if (VipPlotSceneModel* sm = value->findPlotSceneModel(QList<VipAbstractScale*>() << value->xScale() << left[i]))
			if (sm != value->plotSceneModel()) {
				arch.content(sm);
				markSceneModelNoSerialize(sm);
			}
	}
	arch.end();

	// save the vertical window status
	arch.content("verticalWindowVisible", value->displayVerticalWindow());
	arch.content("verticalWindowVisibleRect", value->verticalWindow()->rawData().polygon().boundingRect());

	// save the time scale
	bool auto_unit = value->valueToTimeButton()->automaticUnit();
	arch.content("automaticUnit", auto_unit);
	arch.content("timeUnit", (int)value->valueToTimeButton()->valueToTime());
	arch.content("displayUnitType", (int)value->valueToTimeButton()->displayType());
	arch.content("displayTimeOffset", (int)value->valueToTimeButton()->displayTimeOffset());

	// new in 2.2.14
	arch.content("gridVisible", value->gridVisible());
	arch.content("timeMarkerVisible", value->timeMarker()->isVisible());

	// save all VipPlotItem except canvas, grid, scene model and marker
	QList<VipPlotItem*> items = value->viewer()->area()->plotItems();
	items.removeOne(value->viewer()->area()->canvas());
	items.removeOne(value->viewer()->area()->grid());
	items.removeOne(value->plotSceneModel());
	items.removeOne(value->timeMarker());

	arch.start("items");

	for (int i = 0; i < items.size(); ++i) {
		if (VipPlotItemComposite* it = items[i]->property("VipPlotItemComposite").value<VipPlotItemComposite*>()) {
			if (!it->property("_vip_no_serialize").toBool())
				arch.content(items[i]);
		}
		else if (!items[i]->property("_vip_no_serialize").toBool())
			arch.content(items[i]);
	}

	arch.end();

	return arch;
}

static bool hasNullAxes(VipPlotItem* item)
{
	QList<VipAbstractScale*> scales = item->axes();
	for (int i = 0; i < scales.size(); ++i)
		if (!scales[i])
			return true;
	return false;
}

static VipArchive& operator>>(VipArchive& arch, VipPlotPlayer* value)
{
	// load standard axes
	if (VipPlotWidget2D* pl = qobject_cast<VipPlotWidget2D*>(value->viewer())) {
		QList<VipAbstractScale*> std_scales = QList<VipAbstractScale*>() << pl->area()->bottomAxis() << pl->area()->topAxis() << pl->area()->leftAxis() << pl->area()->rightAxis();

		for (int i = 0; i < std_scales.size(); ++i)
			arch.content("scale", std_scales[i]);
	}

	// load additional axes
	if (qobject_cast<VipVMultiPlotArea2D*>(value->plotWidget2D()->area())) {
		arch.save();
		if (arch.start("MultiAxes")) {
			// VipMultiAxisBase * left = area->leftMultiAxis();
			while (VipBorderItem* it = arch.read().value<VipBorderItem*>()) {
				// area->addScale(it, true);
				value->addLeftScale(it);
			}
			arch.end();
		}
		else
			arch.restore();
	}

	// save scale state
	QByteArray scale_state = value->plotWidget2D()->area()->saveSpatialScaleState();

	// load canvas, grid, plotSceneModel and time marker
	arch.content("canvas", value->viewer()->area()->canvas());
	arch.content("grid", value->viewer()->area()->grid());
	arch.content("plotSceneModel", value->plotSceneModel());
	arch.content("timeMarker", value->timeMarker());

	arch.save();
	int legend_pos;
	;
	if (!arch.content("legendPosition", legend_pos))
		arch.restore();
	else
		value->setLegendPosition((Vip::PlayerLegendPosition)legend_pos);

	value->setAutoScale(arch.read("autoScale").toBool());
	value->enableHZoom(arch.read("isHZoomEnabled").toBool());
	value->enableVZoom(arch.read("isVZoomEnabled").toBool());
	value->setTimeMarkerVisible(false); // timeMarker()->setVisible(false);

	// New in 2.2.17
	// Load the left scales scene models
	arch.save();
	if (arch.start("sceneModels")) {
		while (VipPlotSceneModel* sm = arch.read().value<VipPlotSceneModel*>()) {
			sm->setBrush("All", VipGuiDisplayParamaters::instance()->shapeBackgroundBrush());
			sm->setPen("All", VipGuiDisplayParamaters::instance()->shapeBorderPen());
			sm->setDrawComponents("All", VipGuiDisplayParamaters::instance()->shapeDrawComponents());
			sm->setDrawComponent("All", VipPlotShape::FillPixels, false);
		}
		arch.end();
	}
	else
		arch.restore();

	// New in 3.8.0
	arch.save();
	bool auto_unit; // = d_data->timeUnit->automaticUnit();
	bool displayVerticalWindow;
	QRectF verticalWindowVisibleRect;
	if (arch.content("verticalWindowVisible", displayVerticalWindow)) {
		arch.content("verticalWindowVisibleRect", verticalWindowVisibleRect);
		arch.content("automaticUnit", auto_unit);

		VipShape sh = value->verticalWindow()->rawData();
		sh.setRect(verticalWindowVisibleRect);
		value->verticalWindow()->setRawData(sh);
		value->setDisplayVerticalWindow(displayVerticalWindow);
		value->setProperty("_vip_forceTimeUnit", !auto_unit);
	}
	else
		arch.restore();

	value->valueToTimeButton()->setValueToTime(VipValueToTime::TimeType(arch.read("timeUnit").toInt()));
	value->valueToTimeButton()->setDisplayType(VipValueToTime::DisplayType(arch.read("displayUnitType").toInt()));
	value->valueToTimeButton()->setDisplayTimeOffset(arch.read("displayTimeOffset").toBool());

	// new in 2.2.14
	arch.save();
	bool gridVisible, timeMarkerVisible;
	if (!arch.content("gridVisible", gridVisible))
		arch.restore();
	else if (!arch.content("timeMarkerVisible", timeMarkerVisible))
		arch.restore();
	else {
		value->showGrid(gridVisible);
		value->setTimeMarkerVisible(timeMarkerVisible);
	}

	// load additional items
	QList<VipPlotItem*> items;

	arch.start("items");

	while (true) {
		VipPlotItem* item = arch.read().value<VipPlotItem*>();
		if (item) {
			items << item;
		}
		else
			break;
	}

	// add the items with invalid axes
	QList<VipAbstractScale*> std_scales;
	VipCoordinateSystem::Type t = value->plotWidget2D()->area()->standardScales(std_scales);
	for (int i = 0; i < items.size(); ++i) {
		VipPlotItem* it = items[i];
		if (!qobject_cast<VipPlotGrid*>(it) && !qobject_cast<VipPlotCanvas*>(it)) {
			bool has_null_axes = hasNullAxes(it);
			if (has_null_axes) {
				it->setAxes(std_scales, t);
			}
		}
	}

	arch.end();

	// restore scale state
	QMetaObject::invokeMethod(value->plotWidget2D()->area(), "restoreSpatialScaleState", Qt::QueuedConnection, Q_ARG(QByteArray, scale_state));

	return arch;
}

#include <qmessagebox.h>

static bool handleDropROIFileOnVideo(VipVideoPlayer* pl, VipPlotItem* sp, QMimeData* mime)
{
	(void)sp;
	// Handle drop of ROI xml files on a video player
	QStringList files;
	if (mime->hasFormat("VipMimeDataMapFile"))
		files = static_cast<VipMimeDataMapFile*>(mime)->paths().paths();
	QList<QUrl> urls = mime->urls();
	for (int i = 0; i < urls.size(); ++i)
		files << urls[i].toLocalFile();
	QStringList roi_files;
	for (int i = 0; i < files.size(); ++i) {
		QString fname = files[i];
		std::string f = fname.toLatin1().data();
		if (QFileInfo(fname).suffix() == "xml" || QFileInfo(fname).suffix() == "json") {

			// check thta this is a valid ROI file
			VipShapeReader reader;
			reader.setPath(fname);
			if (reader.open(VipIODevice::ReadOnly))
				roi_files.append(fname);
		}
	}

	if (roi_files.isEmpty())
		return false;

	bool remove_old = true;
	if (pl->plotSceneModel()->shapes().size()) {
		if (QMessageBox::question(pl, "Keep old shapes?", "Do you want to keep the previous shapes?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
			remove_old = false;
	}

	bool res = false;
	for (int i = 0; i < roi_files.size(); ++i) {
		if (vipGetSceneModelWidgetPlayer()->editor()->openShapes(roi_files[i], pl, remove_old).size() > 0)
			res = true;
	}
	return res;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipAbstractPlayer*>();
	vipRegisterArchiveStreamOperators<VipPlayer2D*>();
	vipRegisterArchiveStreamOperators<VipVideoPlayer*>();
	vipRegisterArchiveStreamOperators<VipPlotPlayer*>();
	vipRegisterArchiveStreamOperators<VipWidgetPlayer*>();
	// vipRegisterSettingsArchiveFunctions(serialize_VipDefaultSceneModelDisplayOptions, serialize_VipDefaultSceneModelDisplayOptions);
	vipRegisterSettingsArchiveFunctions(saveVipPlayerToolTip, loadVipPlayerToolTip);
	VipFDDropOnPlotItem().append<bool(VipVideoPlayer*, VipPlotItem*, QMimeData*)>(handleDropROIFileOnVideo);
	return 0;
}
static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);
