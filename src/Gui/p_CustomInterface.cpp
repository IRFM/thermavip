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

#include "p_CustomInterface.h"

#include "VipDisplayArea.h"
#include "VipDragWidget.h"
#include "VipDynGridLayout.h"
#include "VipGui.h"
#include "VipLegendItem.h"
#include "VipMimeData.h"
#include "VipMultiPlotWidget2D.h"
#include "VipPlotMimeData.h"

#include <qapplication.h>
#include <qboxlayout.h>
#include <qclipboard.h>
#include <qrubberband.h>
#include <qtextdocument.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qwidgetaction.h>

#define HIGHLIGHT_MARGIN 5
#define MIN_BORDER_DIST 20

typedef QPointer<QWidget> WidgetPointer;
Q_DECLARE_METATYPE(WidgetPointer)

/// @brief Custom rubber band displayed to highlight potential drop areas.
/// We use this other VipDragRubberBand as it works over native windows.
class DragRubberBand : public QWidget
{
public:
	QString text;
	QPen pen;
	QColor background;

	DragRubberBand(QWidget* parent)
	  : QWidget(parent)
	{
		setWindowFlags(Qt::Popup | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
		setAttribute(Qt::WA_AlwaysStackOnTop, true);
		QColor c = vipGetMainWindow()->palette().color(QPalette::Window);
		bool is_light = c.red() > 200 && c.green() > 200 && c.blue() > 200;
		if (!is_light)
			background = c.lighter(120);
		else
			background = c.darker(120);
		QString cs = QString("rgb(%1,%2,%3)").arg(background.red()).arg(background.green()).arg(background.blue());
		setStyleSheet("QWidget{background:" + cs + ";}");
		this->setWindowOpacity(0.7);
		pen = QPen(Qt::green, 2);
	}
protected:
	virtual void paintEvent(QPaintEvent*)
	{
		QPainter p(this);
		p.setPen(pen);
		p.setBrush(QBrush(background));
		QRect r(0, 0, width(), height());
		p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 2, 2);

		VipText t = this->text;
		t.setTextPen(QPen(VipGuiDisplayParamaters::instance()->defaultPlayerTextColor()));
		if (!t.isEmpty()) {
			QSize s = t.textSize().toSize();
			if (s.width() < r.width()) {
				t.setAlignment(Qt::AlignCenter);
				t.draw(&p, r);
			}
			else if (s.width() < r.height()) {
				QTransform tr;
				tr.translate(r.center().x(), r.center().y());
				tr.rotate(-90);
				p.setTransform(tr);
				t.setAlignment(Qt::AlignCenter);
				r = QRect(0, 0, r.height(), r.width());
				r.moveCenter(QPoint(0, 0));
				t.draw(&p, r);
			}
		}
	}
};

CloseToolBar::CloseToolBar(QWidget* parent)
  : QToolBar(parent)
{
	minimize = addAction(vipIcon("minimize.png"), "Minimize player");
	maximize = addAction(vipIcon("restore.png"), "Maximize/restore player");
	close = addAction(vipIcon("close.png"), "Close player");
	setIconSize(QSize(16, 16));
	setStyleSheet("QToolBar{background: transparent; spacing:0px; margin:0px; padding: 0px;}");
	setMouseTracking(true);
	setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
	setMaximumWidth(80);
	setHasToolTip(true);
	if (parent)
		parent->installEventFilter(this);
}

CloseToolBar::~CloseToolBar()
{
	if (parent())
		parent()->removeEventFilter(this);
}

void CloseToolBar::mouseMoveEvent(QMouseEvent* evt)
{
	update();
	QToolBar::mouseMoveEvent(evt);
}

void CloseToolBar::setHasToolTip(bool enable)
{
	if (enable == hasToolTip())
		return;
	if (enable) 
		setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
	else
		setWindowFlags(Qt::Widget);
	QWidget* p = this->parentWidget();
	if (enable && p) {
		VipAbstractPlayer* pl = qobject_cast<VipAbstractPlayer*>(p);
		QColor c = p->palette().color(QPalette::Window);
		if (pl && pl->plotWidget2D())
			c = pl->plotWidget2D()->backgroundColor();
		QString cs = QString("rgb(%1,%2,%3)").arg(c.red()).arg(c.green()).arg(c.blue());
		setStyleSheet("QToolBar{background: " + cs + "; spacing:0px; margin:0px; padding: 0px;}");
	}
	else
		setStyleSheet("QToolBar{background: transparent; spacing:0px; margin:0px; padding: 0px;}");
}

bool CloseToolBar::hasToolTip() const
{
	return (bool)(windowFlags() & Qt::Tool);
}

bool CloseToolBar::eventFilter(QObject* obj, QEvent* evt)
{
	switch (evt->type()) {
		case QEvent::Hide:
			this->hide();
			break;
		default:
			break;
	}
	return false;
}

static void moveTopRight(CloseToolBar* bar, QWidget* parent, int rightMargin = 0, int top = 0)
{
	if (!(bar->windowFlags() & Qt::ToolTip))
		bar->move(parent->width() - bar->width() - rightMargin, top);
	else {
		QWidget* p = bar->parentWidget();
		if (!p)
			parent = p;
		QPoint pt(parent->width() - bar->width() - rightMargin, top);
		pt = p->mapToGlobal(pt);
		bar->move(pt);
	}
}

static void anchorToArea(const Anchor& a, DragRubberBand& area, QWidget* widget)
{
	QPoint top_left = widget->mapToGlobal(a.highlight.topLeft());

	QRect geom = QRect(top_left.x() - HIGHLIGHT_MARGIN, top_left.y() - HIGHLIGHT_MARGIN, a.highlight.width() + HIGHLIGHT_MARGIN * 2, a.highlight.height() + HIGHLIGHT_MARGIN * 2);

	if (!a.canvas) {

		if (VipDisplayPlayerArea* workspace = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
			VipMultiDragWidget* main = workspace->mainDragWidget(QWidgetList());
			if (main->orientation() == Qt::Vertical) {
				if (a.side == Vip::Top || a.side == Vip::Bottom) {

					QPoint left = workspace->mapToGlobal(QPoint(0, 0));
					QPoint right = workspace->mapToGlobal(QPoint(workspace->width(), 0));
					geom.setLeft(left.x());
					geom.setRight(right.x());
				}
			}
			else {
				if (a.side == Vip::Left || a.side == Vip::Right) {
					QPoint top = workspace->mapToGlobal(QPoint(0, 0));
					QPoint bottom = workspace->mapToGlobal(QPoint(0, workspace->height()));
					geom.setTop(top.y());
					geom.setBottom(bottom.y());
				}
			}
		}
	}

	if (!(area.windowFlags() & Qt::ToolTip))
		// Not a tooltip: since vipGetMainWindow() is the area parent, we need coordinates in vipGetMainWindow() reference
		geom = QRect(vipGetMainWindow()->mapFromGlobal(geom.topLeft()), vipGetMainWindow()->mapFromGlobal(geom.bottomRight()));
	area.setGeometry(geom);
	area.text = a.text;
}

static QString mutliDragWidgetStyleSheet(const QColor& background)
{
	QColor darker = background.darker(130);
	QString res = "VipDragWidgetHandle {background: #" + QString::number(background.rgba(), 16) +
		      ";}\n"
		      "VipDragWidgetHandle:hover{background: #" +
		      QString::number(darker.rgba(), 16) +
		      ";}\n"
		      "VipDragWidgetSplitter {border-radius: 0px; background: #" +
		      QString::number(background.rgba(), 16) +
		      ";}\n"
		      "VipMultiDragWidget{ border-radius: 0px; border: none;}\n"
		      "VipMultiDragWidget > VipScaleWidget{qproperty-backgroundColor: #" +
		      QString::number(background.rgba(), 16) +
		      ";}\n"
		      "VipMultiDragWidget > QWidget {background: #" +
		      QString::number(background.rgba(), 16) +
		      ";}\n"
		      "VipAbstractPlotWidget2D {qproperty-backgroundColor: #" +
		      QString::number(background.rgba(), 16) + ";}\n";
	return res;
}

static void restoreAndClose(QWidget* widget)
{
	VipDragWidget* drag = qobject_cast<VipDragWidget*>(VipDragWidget::fromChild(widget));
	if (drag) {
		if (drag->isMaximized())
			drag->showNormal();
		drag->close();
	}
}

static QWidget* create_player_top_toolbar(VipAbstractPlayer* player, QObject* /*o*/)
{
	QWidget* tmp = player->property("_vip_topToolBar").value<WidgetPointer>();
	if (tmp)
		return tmp;

	VipBaseDragWidget* w = VipBaseDragWidget::fromChild(player);
	if (!w)
		return nullptr;

	VipPlayer2D* pl2D = qobject_cast<VipPlayer2D*>(player);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);

	// add the main tool bar and the title tool bar
	// player->toolBar()->setCustomBehaviorEnabled(false);
	//	player->toolBar()->setShowAdditionals(VipToolBar::ShowInMenu);
	if (QWidget* w = player->playerToolBar())
		hlay->addWidget(w);
	QToolBar* title = new QToolBar();
	title->setIconSize(QSize(20, 20));
	hlay->addStretch(1);
	hlay->addWidget(title);

	// add status text
	if (pl2D)
		title->addWidget(pl2D->statusText());

	QLabel* titleWidget = new QLabel(w->windowTitle());
	QObject::connect(w, SIGNAL(windowTitleChanged(const QString&)), titleWidget, SLOT(setText(const QString&)));
	title->addWidget(new QLabel("<b>&nbsp;&nbsp;Title</b>: "));
	title->addWidget(titleWidget);
	if (pl2D)
		title->addWidget(pl2D->afterTitleToolBar());

	QWidget* res = new QWidget();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->setSpacing(0);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addLayout(hlay);
	res->setLayout(lay);

	// get new tool bars
	if (pl2D) {
		QList<QToolBar*> new_bars = pl2D->toolBars();
		for (int i = 1; i < new_bars.size(); ++i) {
			lay->addWidget(new_bars[i]);
			new_bars[i]->show();
			if (VipToolBar* bar = qobject_cast<VipToolBar*>(new_bars[i]))
				bar->setCustomBehaviorEnabled(false);
		}
	}
	if (player->playerToolBar())
		player->playerToolBar()->show();

	player->setProperty("_vip_topToolBar", QVariant::fromValue(WidgetPointer(res)));
	return res;
}

struct EditTitle : QLineEdit
{
	EditTitle(QWidget* parent)
	  : QLineEdit(parent)
	{
		connect(this, SIGNAL(returnPressed()), this, SLOT(deleteLater()));
		setToolTip("<b>Edit title</b><br>Press ENTER to finish");
	}
	virtual void focusOutEvent(QFocusEvent*) { this->deleteLater(); }
};
static QPointer<EditTitle> _title_editor;
static void finishEditingTitle(VipPlayer2D* player)
{
	player->setAutomaticWindowTitle(false);
	player->setWindowTitle(_title_editor->text());
}
static void editTitle(VipPlayer2D* player)
{
	// setTopWidgetsVisible(false);
	_title_editor = new EditTitle(player);
	_title_editor->resize(player->width(), _title_editor->height());
	_title_editor->setText(player->windowTitle());
	_title_editor->setSelection(0, _title_editor->text().size());
	_title_editor->move(0, 0);
	_title_editor->raise();
	_title_editor->show();
	_title_editor->setFocus();
	QObject::connect(_title_editor.data(), &EditTitle::returnPressed, player, std::bind(finishEditingTitle, player));
}

class NavigatePlayers::PrivateData
{
public:
	QPointer<VipDragWidget> parent;
};

NavigatePlayers::NavigatePlayers(VipDragWidget* p)
  : QToolBar(p->widget())
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->parent = p;
	p->installEventFilter(this);
	updatePos();

	connect(this->addAction(vipIcon("previous.png"), "Go to previous player"), SIGNAL(triggered(bool)), this, SLOT(goPrev()));
	connect(this->addAction(vipIcon("next.png"), "Go to next player"), SIGNAL(triggered(bool)), this, SLOT(goNext()));
	connect(p, SIGNAL(visibilityChanged(VisibilityState)), this, SLOT(updatePos()));
	this->setStyleSheet("QToolBar {background: transparent;}");
}

NavigatePlayers::~NavigatePlayers()
{
	if (d_data->parent)
		d_data->parent->removeEventFilter(this);
}

VipDragWidget* NavigatePlayers::parentDragWidget() const
{
	return d_data->parent;
}
VipDragWidget* NavigatePlayers::next() const
{
	if (!d_data->parent)
		return nullptr;

	VipDragWidget* w = d_data->parent->next();
	for (;;) {
		if (!w)
			w = d_data->parent->topLevelMultiDragWidget()->firstVisibleDragWidget();
		if (w == d_data->parent)
			return nullptr;
		if (qobject_cast<VipPlayer2D*>(w->widget()))
			return w;
		w = w->next();
	}
	return w;
}
VipDragWidget* NavigatePlayers::prev() const
{
	if (!d_data->parent)
		return nullptr;

	{
		VipDragWidget* w = d_data->parent->prev();
		for (;;) {
			if (!w)
				w = d_data->parent->topLevelMultiDragWidget()->lastVisibleDragWidget();
			if (w == d_data->parent)
				return nullptr;
			if (qobject_cast<VipPlayer2D*>(w->widget()))
				return w;
			w = w->prev();
		}
		return w;
	}

	VipMultiDragWidget* m = d_data->parent->topLevelMultiDragWidget();
	if (!m)
		return nullptr;

	QList<VipDragWidget*> children = m->findChildren<VipDragWidget*>();
	if (children.size() == 0)
		return nullptr;

	int idx = children.indexOf(d_data->parent);
	if (idx < 0)
		// impossible
		return nullptr;

	int _idx = idx;
	for (;;) {
		--idx;
		if (idx < 0)
			idx = children.size() - 1;
		if (idx == _idx)
			return nullptr;
		if (qobject_cast<VipPlayer2D*>(children[idx]->widget()))
			break;
	}

	return children[idx];
}

bool NavigatePlayers::eventFilter(QObject* obj, QEvent* evt)
{
	(void)obj;
	if (evt->type() == QEvent::Resize || evt->type() == QEvent::Show || evt->type() == QEvent::Hide) {
		updatePos();
	}
	else if (evt->type() == QEvent::KeyPress) {
		// if (this->isVisible()) {
		QKeyEvent* e = static_cast<QKeyEvent*>(evt);
		if (e->key() == Qt::Key_Down) {
			goNext();
			return true;
		}
		else if (e->key() == Qt::Key_Up) {
			goPrev();
			return true;
		}
		//}
	}
	return false;
}

void NavigatePlayers::goNext()
{
	if (VipDragWidget* n = this->next()) {
		if (this->isVisible())
			n->showMaximized();
		n->setFocusWidget();
	}
}
void NavigatePlayers::goPrev()
{
	if (VipDragWidget* p = this->prev()) {
		if (this->isVisible())
			p->showMaximized();
		p->setFocusWidget();
	}
}

void NavigatePlayers::visibilityChanged()
{
	updatePos();
}

void NavigatePlayers::updatePos()
{
	if (!d_data->parent) {
		this->setVisible(false);
		return;
	}

	bool vis = d_data->parent->isMaximized() && d_data->parent->isVisible();
	if (vis) {

		VipMultiDragWidget* m = d_data->parent->topLevelMultiDragWidget();
		if (!m)
			vis = false;
		else {

			auto c = m->findChildren<VipDragWidget*>();

			if (c.size() <= 1)
				vis = false;
			else {
				for (VipDragWidget* w : c) {
					if (w != d_data->parent && w->isVisible()) {
						if (!w->testSupportedOperation(VipDragWidget::NoHideOnMaximize)) {

							vis = false;
							break;
						}
					}
				}
			}
		}
	}

	this->setVisible(vis);
	// this->move(0, 0);
}

void BaseCustomPlayer::closePlayer()
{
	if (QWidget* w = qobject_cast<QWidget*>(sender()))
		w->setEnabled(false);
	if (sender()) {
		disconnect(sender(), SIGNAL(clicked(bool)), this, SLOT(closePlayer()));
		QCoreApplication::instance()->removePostedEvents(sender(), QEvent::MetaCall);
	}
	QCoreApplication::instance()->removePostedEvents(this, QEvent::MetaCall);
	if (dragWidget())
		restoreAndClose(dragWidget());
}

void BaseCustomPlayer::maximizePlayer()
{
	if (dragWidget()) {
		if (dragWidget()->isMaximized()) {
			if (VipMultiDragWidget* md = dragWidget()->parentMultiDragWidget())
				if (md->count() > 1)
					dragWidget()->showNormal();
		}
		else if (dragWidget()->isMinimized()) {
			dragWidget()->showNormal();
		}
		else {
			dragWidget()->showMaximized();
		}
	}
}

void BaseCustomPlayer::minimizePlayer()
{
	if (dragWidget()) {
		if (!dragWidget()->isMinimized()) {
			if (VipMultiDragWidget* md = dragWidget()->parentMultiDragWidget()) {
				QList<VipDragWidget*> ws = md->findChildren<VipDragWidget*>();
				int vis_count = 0;
				for (int i = 0; i < ws.size(); ++i)
					if (ws[i] != dragWidget() && ws[i]->isVisible())
						++vis_count;
				if (vis_count) {
					dragWidget()->showMinimized();
				}
			}
		}
	}
}

void BaseCustomPlayer2D::unselectAll()
{
	if (player() && player()->plotWidget2D()) {
		QList<QGraphicsItem*> items = player()->plotWidget2D()->area()->scene()->items();
		QList<QGraphicsObject*> oitems = vipCastItemList<QGraphicsObject*>(items, QString(), 1, 1);
		QList<QPointer<QGraphicsObject>> pitems;
		for (int i = 0; i < oitems.size(); ++i)
			pitems.append(oitems[i]);
		for (int i = 0; i < pitems.size(); ++i) {
			if (pitems[i])
				pitems[i]->setSelected(false);
		}
	}
}

void BaseCustomPlayer2D::updateTitle()
{
	VipText t = player()->plotWidget2D()->area()->title();
	t.setText(player()->windowTitle());
	player()->plotWidget2D()->area()->setTitle(t);
	// player()->plotWidget2D()->area()->titleAxis()->setVisible(!t.isEmpty());
	// static_cast<VipPlotWidget2D*>(player()->plotWidget2D())->area()->topAxis()->setTitle(t);
}
void BaseCustomPlayer2D::editTitle()
{
	::editTitle(player());
}
QPointF BaseCustomPlayer2D::scenePos(const QPoint& viewport_pos) const
{
	QPoint view_pt = player()->plotWidget2D()->viewport()->mapTo(player()->plotWidget2D(), viewport_pos);
	QPointF scene_pt = player()->plotWidget2D()->mapToScene(view_pt);
	return scene_pt;
}
QGraphicsItem* BaseCustomPlayer2D::firstVisibleItem(const QPointF& scene_pos) const
{
	QList<QGraphicsItem*> items = player()->plotWidget2D()->area()->scene()->items(scene_pos);
	for (int i = 0; i < items.size(); ++i) {
		if (!items[i]->isVisible())
			continue;
		if (qobject_cast<VipPlotGrid*>(items[i]->toGraphicsObject()) ||
			qobject_cast<VipRubberBand*>(items[i]->toGraphicsObject()) ||
			qobject_cast<VipPlotMarker*>(items[i]->toGraphicsObject())/*||
																	  qobject_cast<VipDrawSelectionOrder*>(items[i]->toGraphicsObject())*/)
			continue;
		return items[i];
	}
	return nullptr;
}

class CustomizeVideoPlayer::PrivateData
{
public:
	VipVideoPlayer* player;
	VipDragWidget* dragWidget;
	CloseToolBar* closeBar;
	QPoint mousePress;
	Anchor anchor;
	QPointer<DragRubberBand> area;
	bool closeRequested;
	PrivateData()
	  : mousePress(-1, -1)
	  , closeRequested(false)
	{
	}
};

CustomizeVideoPlayer::CustomizeVideoPlayer(VipVideoPlayer* player)
  : BaseCustomPlayer2D(player)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->player = player;
	d_data->dragWidget = nullptr;

	// d_data->area->SetBrush(QBrush(Qt::NoBrush));
	QWidget* parent = player->parentWidget();
	VipMultiDragWidget* top = nullptr;
	while (parent) {
		if (!d_data->dragWidget)
			d_data->dragWidget = qobject_cast<VipDragWidget*>(parent);
		if ((top = qobject_cast<VipMultiDragWidget*>(parent))) {
			break;
		}
		parent = parent->parentWidget();
	}
	if (!d_data->dragWidget)
		return;

	// Add a navigation widget that manages itself
	new NavigatePlayers(d_data->dragWidget);

	d_data->area = new DragRubberBand(vipGetMainWindow());
	d_data->player->plotWidget2D()->viewport()->installEventFilter(this);
	d_data->closeBar = new CloseToolBar(d_data->player);
	connect(d_data->closeBar->close, SIGNAL(triggered(bool)), this, SLOT(closePlayer()));
	connect(d_data->closeBar->maximize, SIGNAL(triggered(bool)), this, SLOT(maximizePlayer()));
	connect(d_data->closeBar->minimize, SIGNAL(triggered(bool)), this, SLOT(minimizePlayer()));

	connect(d_data->player->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), this, SLOT(reorganizeCloseButton()));
	reorganizeCloseButton();

	// title management
	VipText t = d_data->player->plotWidget2D()->area()->title();
	QFont f = t.font();
	// f.setBold(true);
	t.setFont(f);
	// t.setLayoutAttribute(VipText::MinimumLayout);
	// static_cast<VipPlotWidget2D*>(d_data->player->plotWidget2D())->area()->topAxis()->setTitle(t);
	d_data->player->plotWidget2D()->area()->setTitle(t);
	updateTitle();
	connect(player, SIGNAL(windowTitleChanged(const QString&)), this, SLOT(updateTitle()));

	// moveTitleBarActionsToToolBar(qobject_cast<VipDragWidgetHeader*>(d_data->dragWidget->Header()), player->toolBar(), this);
	create_player_top_toolbar(player, this);

	connect(player, SIGNAL(renderEnded(const VipRenderState&)), this, SLOT(endRender()));
}
CustomizeVideoPlayer::~CustomizeVideoPlayer()
{
	if (d_data->area)
		d_data->area->deleteLater();
}

VipDragWidget* CustomizeVideoPlayer::dragWidget() const
{
	return d_data->dragWidget;
}

void CustomizeVideoPlayer::updateViewport(QWidget* viewport)
{
	viewport->installEventFilter(this);
}

CloseToolBar* CustomizeVideoPlayer::closeToolBar() const
{
	return d_data->closeBar;
}

void CustomizeVideoPlayer::endRender()
{
	// TEST: comment
	// d_data->player->plotWidget2D()->setBackgroundColor(defaultPlayerBackground());
}
#include <qscrollbar.h>
void CustomizeVideoPlayer::reorganizeCloseButton()
{
	if (!d_data->player || !d_data->dragWidget || d_data->dragWidget->isDestroying())
		return;

	QScrollBar* bar = d_data->player->plotWidget2D()->verticalScrollBar();
	moveTopRight(d_data->closeBar, d_data->player, (bar->isVisible() ? bar->width() : 0));

	QPoint pt = d_data->player->plotWidget2D()->mapFromGlobal(QCursor::pos());
	// update visibility
	QRect r = d_data->player->plotWidget2D()->rect();
	if (r.contains(pt) && d_data->player->isVisible()) {
		d_data->closeBar->setVisible(true);
	}
	else {
		d_data->closeBar->setVisible(false);
	}
}

Anchor CustomizeVideoPlayer::anchor(const QPoint& viewport_pos, const QMimeData* mime)
{
	QPointF scene_pt = scenePos(viewport_pos);								     // mouse in scene coordinates
	QPoint pt = d_data->player->plotWidget2D()->viewport()->mapTo(d_data->player->plotWidget2D(), viewport_pos); // mouse in view coordinates
	bool is_drag_widget = mime && !mime->data("application/dragwidget").isEmpty();
	/*
	Highlight areas
	We define several areas:
	- Inside a canvas (add curve on existing canvas)
	- Inside a canvas but on a border (add curve on a new stacked panel)
	- Outside canvas (add plot in a new player)
	*/

	// Buils scene path
	VipPlotCanvas* canvas = d_data->player->plotWidget2D()->area()->canvas();
	QRectF r = canvas->mapToScene(canvas->boundingRect()).boundingRect();

	// find area to highlight
	Anchor res;
	// Viewport rect
	QRect viewport_rect = d_data->player->plotWidget2D()->viewport()->geometry();
	QRect canvas_rect = d_data->player->plotWidget2D()->mapFromScene(r).boundingRect();

	int w_l = canvas_rect.left() - viewport_rect.left();
	int w_r = viewport_rect.right() - canvas_rect.right();
	int h_t = canvas_rect.top() - viewport_rect.top();
	int h_b = viewport_rect.bottom() - canvas_rect.bottom();

	if (w_l < MIN_BORDER_DIST)
		w_l = MIN_BORDER_DIST;
	if (w_r < MIN_BORDER_DIST)
		w_r = MIN_BORDER_DIST;
	if (h_t < MIN_BORDER_DIST)
		h_t = MIN_BORDER_DIST;
	if (h_b < MIN_BORDER_DIST)
		h_b = MIN_BORDER_DIST;

	if (viewport_rect.width() < w_l * 2)
		w_l = viewport_rect.width() / 2;
	if (viewport_rect.width() < w_r * 2)
		w_r = viewport_rect.width() / 2;
	if (viewport_rect.height() < h_b * 2)
		h_b = viewport_rect.height() / 2;
	if (viewport_rect.height() < h_t * 2)
		h_t = viewport_rect.height() / 2;

	// test borders
	res.side = Vip::NoSide;

	if (pt.x() < viewport_rect.left() + w_l) {
		res.side = Vip::Left;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.top(), w_l, viewport_rect.height());
		res.text = "Create new plot area on the left";
	}
	else if (pt.x() > viewport_rect.right() - w_r) {
		res.side = Vip::Right;
		res.highlight = QRect(viewport_rect.right() - w_r, viewport_rect.top(), w_r, viewport_rect.height());
		res.text = "Create new plot area on the right";
	}
	else if (pt.y() < viewport_rect.top() + h_t) {
		res.side = Vip::Top;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.top(), viewport_rect.width(), h_t);
		res.text = "Create new plot area on the top";
	}
	else if (pt.y() > viewport_rect.bottom() - h_b) {
		res.side = Vip::Bottom;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.bottom() - h_b, viewport_rect.width(), h_b);
		res.text = "Create new plot area on the bottom";
	}
	else if (r.contains(scene_pt)) {
		res.side = Vip::AllSides;
		res.canvas = canvas;
		res.highlight = d_data->player->plotWidget2D()->mapFromScene(r).boundingRect();
		if (is_drag_widget)
			res.text = "Swap players";
		else
			res.text = "Add to this video";
	}

	if (res.side != Vip::NoSide) {
		return res;
	}
	return Anchor();
}

void CustomizeVideoPlayer::addToolBarWidget(QWidget* w)
{
	if (d_data->player)
		d_data->player->toolBar()->addWidget(w);
}

/*static void unselectAll(QGraphicsScene* scene)
{
	QList<QGraphicsItem*> items = scene->items();
	foreach (QGraphicsItem* item, items) {
		item->setSelected(false);
	}
}*/

bool CustomizeVideoPlayer::eventFilter(QObject*, QEvent* evt)
{
	if (!d_data->player || !d_data->dragWidget || d_data->dragWidget->isDestroying())
		return false;
	if (evt->type() == QEvent::Destroy)
		return false;

	if (evt->type() == QEvent::Resize) {
		reorganizeCloseButton();
	}
	else if (evt->type() == QEvent::Enter) {
		reorganizeCloseButton();
	}
	else if (evt->type() == QEvent::Leave) {
		reorganizeCloseButton();
	}

	/*
	We want to move a VipPlotPlayer through the internal canvas
	*/

	if (evt->type() == QEvent::MouseButtonPress) {
		// ignore the mouse event if there is a filter installed (like drawing ROI), let the filter do its job
		if (d_data->player->plotWidget2D()->area()->filter())
			return false;

		QMouseEvent* event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton) {

			// Get plot items behind the mouse,
			// check if there are only canvas or grids
			QPointF scene_pt = scenePos(event->VIP_EVT_POSITION());
			bool ok = true;
			if (QGraphicsItem* item = firstVisibleItem(scene_pt)) {
				if (!item->toGraphicsObject())
					ok = false;
				else {
					QGraphicsObject* obj = item->toGraphicsObject();
					if (!qobject_cast<VipPlotCanvas*>(obj) && !qobject_cast<VipPlotSpectrogram*>(obj))
						ok = false;
				}
			}
			if (ok) {
				// check potential key modifiers
				int key_modifiers = d_data->player->property("_vip_moveKeyModifiers").toInt();
				if (key_modifiers && !(key_modifiers & event->modifiers()))
					return false;
				// yes, only canvas or grids
				d_data->mousePress = event->VIP_EVT_POSITION();
				return false; // true;
			}
		}
	}
	else if (evt->type() == QEvent::MouseButtonRelease) {
		// disable further dragging
		if (d_data->mousePress != QPoint(-1, -1)) {
			bool same_pos = (d_data->mousePress - QPointF(static_cast<QMouseEvent*>(evt)->VIP_EVT_POSITION())).manhattanLength() < 10;
			d_data->mousePress = QPoint(-1, -1);
			// we need to unselect all items since this is a simple click
			if (same_pos)
				// this->unselectAll();
				QMetaObject::invokeMethod(this, "unselectAll" /*std::bind(unselectAll, d_data->player->plotWidget2D()->area()->scene())*/, Qt::QueuedConnection);
			return false;
		}
	}
	else if (evt->type() == QEvent::MouseMove) {
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton)
			// disable dragging the spectrogram to make sure we keep receiving mouse events
			d_data->player->spectrogram()->setItemAttribute(VipPlotItem::Droppable, false);
		if (d_data->mousePress != QPoint(-1, -1) && !d_data->player->plotWidget2D()->area()->mouseInUse()) {
			// drag
			if ((event->VIP_EVT_POSITION() - d_data->mousePress).manhattanLength() > 10) {
				VipBaseDragWidget* w = VipDragWidget::fromChild(d_data->player);
				QPoint pt = d_data->mousePress;
				d_data->mousePress = QPoint(-1, -1);
				return w->dragThisWidget(d_data->player->plotWidget2D()->viewport(), pt);
			}
		}
	}

	/**
	Now we want to manage drop events
	*/

	if (evt->type() == QEvent::DragEnter) {
		QDragEnterEvent* event = static_cast<QDragEnterEvent*>(evt);

		d_data->anchor = anchor(event->VIP_EVT_POSITION(), event->mimeData());

		if (d_data->anchor.side != Vip::NoSide && !d_data->anchor.canvas && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->acceptProposedAction();
			return true;
		}

		// Do not accept drag widgets inside
		if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->setAccepted(false);
			return false;
		}
		// Do not accept VipPlotMimeData objects
		if (qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
			event->setAccepted(false);
			return false;
		}
	}
	else if (evt->type() == QEvent::DragMove) {

		QDragMoveEvent* event = static_cast<QDragMoveEvent*>(evt);

		d_data->anchor = anchor(event->VIP_EVT_POSITION(), event->mimeData());
		if (d_data->anchor.side != Vip::NoSide) {
			// higlight area
			anchorToArea(d_data->anchor, *d_data->area, d_data->player->plotWidget2D());
			d_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else if (d_data->anchor.side == Vip::AllSides && !d_data->anchor.canvas && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			// higlight area
			anchorToArea(d_data->anchor, *d_data->area, d_data->player->plotWidget2D());
			d_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else {
			event->setAccepted(false);
			d_data->area->hide();
		}
		return false;
	}
	else if (evt->type() == QEvent::DragLeave) {
		d_data->area->hide();
	}
	else if (evt->type() == QEvent::Drop) {
		d_data->area->hide();
		QDropEvent* event = static_cast<QDropEvent*>(evt);

		if (d_data->anchor.side != Vip::NoSide && !d_data->anchor.canvas) {
			if (VipMultiDragWidget* mw = d_data->dragWidget->parentMultiDragWidget()) {
				QPoint pos = mw->indexOf(d_data->dragWidget);
				VipDragWidgetHandle* h = nullptr;

				if (mw->orientation() == Qt::Vertical) {

					if (d_data->anchor.side == Vip::Right)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (d_data->anchor.side == Vip::Left)
						h = mw->subSplitterHandle(pos.y(), pos.x());
					else if (d_data->anchor.side == Vip::Bottom)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (d_data->anchor.side == Vip::Top)
						h = mw->mainSplitterHandle(pos.y());
				}
				else {
					if (d_data->anchor.side == Vip::Right)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (d_data->anchor.side == Vip::Left)
						h = mw->mainSplitterHandle(pos.y());
					else if (d_data->anchor.side == Vip::Bottom)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (d_data->anchor.side == Vip::Top)
						h = mw->subSplitterHandle(pos.y(), pos.x());
				}

				bool res = h->dropMimeData(event->mimeData());
				if (!res)
					event->setDropAction(Qt::IgnoreAction);
				else
					event->acceptProposedAction();

				// VipDragWidgetHandler::find(d_data->dragWidget->topLevelMultiDragWidget()->parentWidget())->reorganizeMinimizedChildren();
				QMetaObject::invokeMethod(d_data->dragWidget->topLevelMultiDragWidget(), "reorganizeMinimizedChildren");
				return true;
			}
		}
		else if (d_data->anchor.side == Vip::AllSides && d_data->anchor.canvas) {
			if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
				// TODO: player swapping

				VipBaseDragWidget* base = (VipBaseDragWidget*)(event->mimeData()->data("application/dragwidget").toULongLong());
				if (VipDragWidget* d = qobject_cast<VipDragWidget*>(base)) {
					d->parentMultiDragWidget()->swapWidgets(d, d_data->dragWidget);
				}
				event->setDropAction(Qt::IgnoreAction);
				return true;
			}
			else {
				d_data->anchor.canvas->dropMimeData(event->mimeData());
				event->acceptProposedAction();
				return true;
			}
			// return false;
		}

		// ignore drag widget to be sure that the widget will be moved
		if (!event->mimeData()->data("application/dragwidget").isEmpty())
			event->setDropAction(Qt::IgnoreAction);
	}

	return false;
}

class CustomWidgetPlayer::PrivateData
{
public:
	VipWidgetPlayer* player;
	VipDragWidget* dragWidget;
	CloseToolBar* closeBar;
	QPoint mousePress;
	Anchor anchor;
	QPointer<DragRubberBand> area;
	bool closeRequested;
	PrivateData()
	  : mousePress(-1, -1)
	  , closeRequested(false)
	{
	}
};

CustomWidgetPlayer::CustomWidgetPlayer(VipWidgetPlayer* player)
  : BaseCustomPlayer(player)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->player = player;
	d_data->dragWidget = nullptr;

	// d_data->area->SetBrush(QBrush(Qt::NoBrush));
	QWidget* parent = player->parentWidget();
	VipMultiDragWidget* top = nullptr;
	while (parent) {
		if (!d_data->dragWidget)
			d_data->dragWidget = qobject_cast<VipDragWidget*>(parent);
		if ((top = qobject_cast<VipMultiDragWidget*>(parent))) {
			break;
		}
		parent = parent->parentWidget();
	}
	if (!d_data->dragWidget)
		return;

	d_data->area = new DragRubberBand(vipGetMainWindow());
	if (d_data->player->widgetForMouseEvents())
		d_data->player->widgetForMouseEvents()->installEventFilter(this);

	d_data->closeBar = new CloseToolBar(d_data->player);

	connect(d_data->closeBar->close, SIGNAL(triggered(bool)), this, SLOT(closePlayer()));
	connect(d_data->closeBar->maximize, SIGNAL(triggered(bool)), this, SLOT(maximizePlayer()));
	connect(d_data->closeBar->minimize, SIGNAL(triggered(bool)), this, SLOT(minimizePlayer()));

	// connect(d_data->player->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), this, SLOT(reorganizeCloseButton()));
	reorganizeCloseButton();

	create_player_top_toolbar(player, this);
}
CustomWidgetPlayer::~CustomWidgetPlayer()
{
	if (d_data->area)
		d_data->area->deleteLater();
}

VipDragWidget* CustomWidgetPlayer::dragWidget() const
{
	return d_data->dragWidget;
}

void CustomWidgetPlayer::reorganizeCloseButton()
{
	if (!d_data->player || !d_data->dragWidget || d_data->dragWidget->isDestroying())
		return;

	if (QWidget* w = d_data->player->widget()) {

		moveTopRight(d_data->closeBar, w);

		QPoint pt = w->mapFromGlobal(QCursor::pos());
		// update visibility
		QRect r = w->rect();
		if (r.contains(pt) && d_data->player->isVisible()) {
			d_data->closeBar->setVisible(true);
		}
		else {
			d_data->closeBar->setVisible(false);
		}
	}
}

Anchor CustomWidgetPlayer::anchor(const QPoint& viewport_pos, const QMimeData* mime)
{
	QPoint pt = viewport_pos;
	bool is_drag_widget = mime && !mime->data("application/dragwidget").isEmpty();

	// find area to highlight
	Anchor res;
	// Viewport rect
	QRect viewport_rect = d_data->player->geometry();
	QRect canvas_rect = viewport_rect.adjusted(20, 20, -20, -20);

	int w_l = canvas_rect.left() - viewport_rect.left();
	int w_r = viewport_rect.right() - canvas_rect.right();
	int h_t = canvas_rect.top() - viewport_rect.top();
	int h_b = viewport_rect.bottom() - canvas_rect.bottom();

	if (w_l < MIN_BORDER_DIST)
		w_l = MIN_BORDER_DIST;
	if (w_r < MIN_BORDER_DIST)
		w_r = MIN_BORDER_DIST;
	if (h_t < MIN_BORDER_DIST)
		h_t = MIN_BORDER_DIST;
	if (h_b < MIN_BORDER_DIST)
		h_b = MIN_BORDER_DIST;

	if (viewport_rect.width() < w_l * 2)
		w_l = viewport_rect.width() / 2;
	if (viewport_rect.width() < w_r * 2)
		w_r = viewport_rect.width() / 2;
	if (viewport_rect.height() < h_b * 2)
		h_b = viewport_rect.height() / 2;
	if (viewport_rect.height() < h_t * 2)
		h_t = viewport_rect.height() / 2;

	// test borders
	res.side = Vip::NoSide;

	if (pt.x() < viewport_rect.left() + w_l) {
		res.side = Vip::Left;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.top(), w_l, viewport_rect.height());
		res.text = "Create new area on the left";
	}
	else if (pt.x() > viewport_rect.right() - w_r) {
		res.side = Vip::Right;
		res.highlight = QRect(viewport_rect.right() - w_r, viewport_rect.top(), w_r, viewport_rect.height());
		res.text = "Create new area on the right";
	}
	else if (pt.y() < viewport_rect.top() + h_t) {
		res.side = Vip::Top;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.top(), viewport_rect.width(), h_t);
		res.text = "Create new area on the top";
	}
	else if (pt.y() > viewport_rect.bottom() - h_b) {
		res.side = Vip::Bottom;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.bottom() - h_b, viewport_rect.width(), h_b);
		res.text = "Create new area on the bottom";
	}
	else if (canvas_rect.contains(pt)) {
		res.side = Vip::AllSides;
		res.highlight = canvas_rect;
		if (is_drag_widget)
			res.text = "Swap players";
		else
			res.text = "Add to this widget";
	}

	if (res.side != Vip::NoSide) {
		return res;
	}
	return Anchor();
}

bool CustomWidgetPlayer::eventFilter(QObject*, QEvent* evt)
{
	if (!d_data->player || !d_data->dragWidget || d_data->dragWidget->isDestroying())
		return false;
	if (evt->type() == QEvent::Destroy)
		return false;

	if (evt->type() == QEvent::Resize)
		reorganizeCloseButton();
	else if (evt->type() == QEvent::Enter)
		reorganizeCloseButton();
	else if (evt->type() == QEvent::Leave)
		reorganizeCloseButton();

	/*
	We want to move a VipPlotPlayer through the internal canvas
	*/

	if (evt->type() == QEvent::MouseButtonPress) {
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton) {

			// check potential key modifiers
			int key_modifiers = d_data->player->property("_vip_moveKeyModifiers").toInt();
			if (!key_modifiers)
				key_modifiers = (int)Qt::AltModifier;
			if (key_modifiers && !(key_modifiers & event->modifiers()))
				return false;
			// yes, only canvas or grids
			d_data->mousePress = event->VIP_EVT_POSITION();
			return false; // true;
		}
	}
	else if (evt->type() == QEvent::MouseButtonRelease) {
		// disable further dragging
		if (d_data->mousePress != QPoint(-1, -1)) {
			// bool same_pos = (d_data->mousePress - QPointF(static_cast<QMouseEvent*>(evt)->VIP_EVT_POSITION())).manhattanLength() < 10;
			d_data->mousePress = QPoint(-1, -1);
			return false;
		}
	}
	else if (evt->type() == QEvent::MouseMove) {
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);
		if (d_data->mousePress != QPoint(-1, -1)) {
			// drag
			if ((event->VIP_EVT_POSITION() - d_data->mousePress).manhattanLength() > 10) {
				VipBaseDragWidget* w = VipDragWidget::fromChild(d_data->player);
				QPoint pt = d_data->mousePress;
				d_data->mousePress = QPoint(-1, -1);
				return w->dragThisWidget(d_data->player, pt);
			}
		}
	}

	/**
	Now we want to manage drop events
	*/

	if (evt->type() == QEvent::DragEnter) {
		QDragEnterEvent* event = static_cast<QDragEnterEvent*>(evt);

		d_data->anchor = anchor(event->VIP_EVT_POSITION(), event->mimeData());

		if (d_data->anchor.side != Vip::NoSide && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->acceptProposedAction();
			return true;
		}

		// Do not accept drag widgets inside
		if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->setAccepted(false);
			return false;
		}
		// Do not accept VipPlotMimeData objects
		if (qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
			event->setAccepted(false);
			return false;
		}
	}
	else if (evt->type() == QEvent::DragMove) {
		QDragMoveEvent* event = static_cast<QDragMoveEvent*>(evt);

		d_data->anchor = anchor(event->VIP_EVT_POSITION(), event->mimeData());
		if (d_data->anchor.side != Vip::NoSide) {
			// higlight area
			anchorToArea(d_data->anchor, *d_data->area, d_data->player);
			d_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else if (d_data->anchor.side == Vip::AllSides && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			// higlight area
			anchorToArea(d_data->anchor, *d_data->area, d_data->player);
			d_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else {
			event->setAccepted(false);
			d_data->area->hide();
		}
		return false;
	}
	else if (evt->type() == QEvent::DragLeave) {
		d_data->area->hide();
	}
	else if (evt->type() == QEvent::Drop) {
		d_data->area->hide();
		QDropEvent* event = static_cast<QDropEvent*>(evt);

		if (d_data->anchor.side == Vip::AllSides) {
			if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
				// TODO: player swapping

				VipBaseDragWidget* base = (VipBaseDragWidget*)(event->mimeData()->data("application/dragwidget").toULongLong());
				if (VipDragWidget* d = qobject_cast<VipDragWidget*>(base)) {
					d->parentMultiDragWidget()->swapWidgets(d, d_data->dragWidget);
				}
				event->setDropAction(Qt::IgnoreAction);
				return true;
			}
			else {
				/*d_data->anchor.canvas->dropMimeData(event->mimeData());
				event->acceptProposedAction();
				return true;*/
				return false;
			}
			// return false;
		}
		else if (d_data->anchor.side != Vip::NoSide) {
			if (VipMultiDragWidget* mw = d_data->dragWidget->parentMultiDragWidget()) {
				QPoint pos = mw->indexOf(d_data->dragWidget);
				VipDragWidgetHandle* h = nullptr;

				if (mw->orientation() == Qt::Vertical) {

					if (d_data->anchor.side == Vip::Right)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (d_data->anchor.side == Vip::Left)
						h = mw->subSplitterHandle(pos.y(), pos.x());
					else if (d_data->anchor.side == Vip::Bottom)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (d_data->anchor.side == Vip::Top)
						h = mw->mainSplitterHandle(pos.y());
				}
				else {
					if (d_data->anchor.side == Vip::Right)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (d_data->anchor.side == Vip::Left)
						h = mw->mainSplitterHandle(pos.y());
					else if (d_data->anchor.side == Vip::Bottom)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (d_data->anchor.side == Vip::Top)
						h = mw->subSplitterHandle(pos.y(), pos.x());
				}

				bool res = h->dropMimeData(event->mimeData());
				if (!res)
					event->setDropAction(Qt::IgnoreAction);
				else
					event->acceptProposedAction();

				// VipDragWidgetHandler::find(d_data->dragWidget->topLevelMultiDragWidget()->parentWidget())->reorganizeMinimizedChildren();
				QMetaObject::invokeMethod(d_data->dragWidget->topLevelMultiDragWidget(), "reorganizeMinimizedChildren");
				return true;
			}
		}

		// ignore drag widget to be sure that the widget will be moved
		if (!event->mimeData()->data("application/dragwidget").isEmpty())
			event->setDropAction(Qt::IgnoreAction);
	}

	return false;
}

class CustomizePlotPlayer::PrivateData
{
public:
	VipPlotPlayer* player;
	VipDragWidget* dragWidget;
	QList<QWidget*> topWidgets;
	QToolBar* toolBar;
	QToolButton* title;
	QRect topWidgetsRect;
	QPoint mousePress;
	QPointer<DragRubberBand> area;
	Anchor anchor;

	PrivateData()
	  : player(nullptr)
	  , dragWidget(nullptr)
	  , mousePress(-1, -1)
	{
	}
};

CustomizePlotPlayer::CustomizePlotPlayer(VipPlotPlayer* player)
  : BaseCustomPlayer2D(player)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->player = player;
	d_data->dragWidget = nullptr;

	QWidget* parent = player->parentWidget();
	VipMultiDragWidget* top = nullptr;
	while (parent) {
		if (!d_data->dragWidget)
			d_data->dragWidget = qobject_cast<VipDragWidget*>(parent);
		if ((top = qobject_cast<VipMultiDragWidget*>(parent))) {
			break;
		}
		parent = parent->parentWidget();
	}
	if (!d_data->dragWidget)
		return;

	// Add a navigation widget that manages itself
	new NavigatePlayers(d_data->dragWidget);

	d_data->player->plotWidget2D()->viewport()->installEventFilter(this);

	d_data->area = new DragRubberBand(vipGetMainWindow());
	d_data->area->setAttribute(Qt::WA_TransparentForMouseEvents);
	d_data->area->setEnabled(false);
	d_data->area->setFocusPolicy(Qt::NoFocus);

	// remove plot area margins if any
	d_data->player->plotWidget2D()->area()->setMargins(VipMargins());

	// title management
	VipText t = d_data->player->plotWidget2D()->area()->title();
	QFont f = t.font();
	// f.setBold(true);
	t.setFont(f);
	// t.setLayoutAttribute(VipText::MinimumLayout);
	// static_cast<VipPlotWidget2D*>(d_data->player->plotWidget2D())->area()->topAxis()->setTitle(t);
	d_data->player->plotWidget2D()->area()->setTitle(t);
	updateTitle();
	connect(player, SIGNAL(windowTitleChanged(const QString&)), this, SLOT(updateTitle()));

	// rendering management
	// connect(player, SIGNAL(renderStarted(const VipRenderState&)), this, SLOT(startRender()));
	// connect(player, SIGNAL(renderEnded(const VipRenderState&)), this, SLOT(endRender()));

	// legend management
	d_data->player->plotWidget2D()->area()->legend()->layout()->setContentsMargins(20, 0, 20, 0);
	d_data->player->plotWidget2D()->area()->legend()->layout()->setMargins(0);
	d_data->player->plotWidget2D()->area()->legend()->layout()->setSpacing(0);
	d_data->player->plotWidget2D()->area()->legend()->setDrawCheckbox(false);

	for (int i = 0; i < d_data->player->plotWidget2D()->area()->innerLegendCount(); ++i) {
		if (VipLegend* l = d_data->player->plotWidget2D()->area()->innerLegend(i)) {
			l->layout()->setContentsMargins(5, 5, 5, 5);
			l->layout()->setMargins(0);
			l->layout()->setSpacing(0);
			l->setDrawCheckbox(false);
		}
	}

	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(d_data->player->plotWidget2D()->area())) {
		connect(area, SIGNAL(canvasAdded(VipPlotCanvas*)), this, SLOT(reorganizeCloseButtons()));
		connect(area, SIGNAL(canvasRemoved(VipPlotCanvas*)), this, SLOT(reorganizeCloseButtons()));
	}

	// moveTitleBarActionsToToolBar(qobject_cast<VipDragWidgetHeader*>(d_data->dragWidget->Header()), player->toolBar(), this);
	create_player_top_toolbar(player, this);
}
CustomizePlotPlayer::~CustomizePlotPlayer()
{
	if (d_data->area)
		d_data->area->deleteLater();
}

VipDragWidget* CustomizePlotPlayer::dragWidget() const
{
	return d_data->dragWidget;
}

void CustomizePlotPlayer::updateViewport(QWidget* viewport)
{
	viewport->installEventFilter(this);
}

Anchor CustomizePlotPlayer::anchor(const QPoint& viewport_pos, const QMimeData* mime)
{
	QPointF scene_pt = scenePos(viewport_pos);								     // mouse in scene coordinates
	QPoint pt = d_data->player->plotWidget2D()->viewport()->mapTo(d_data->player->plotWidget2D(), viewport_pos); // mouse in view coordinates
	bool is_drag_widget = mime && !mime->data("application/dragwidget").isEmpty();
	/*
	Highlight areas
	We define several areas:
	- Inside a canvas (add curve on existing canvas)
	- Inside a canvas but on a border (add curve on a new stacked panel)
	- Outside canvas (add plot in a new player)
	*/

	// Buils scene path
	QRectF scene = d_data->player->plotWidget2D()->sceneRect();
	QPainterPath scene_path;
	scene_path.addRect(scene);

	// Build canvas paths and remove them from scene path
	QList<VipPlotCanvas*> all_canvas = d_data->player->plotWidget2D()->area()->findItems<VipPlotCanvas*>();
	QMap<VipPlotCanvas*, QPainterPath> canvas_path;
	for (int i = 0; i < all_canvas.size(); ++i) {
		QPainterPath p = all_canvas[i]->mapToScene(all_canvas[i]->shape());
		canvas_path[all_canvas[i]] = p;
		scene_path = scene_path.subtracted(p);
	}

	// find area to highlight
	Anchor res;

	// test cansvas
	for (QMap<VipPlotCanvas*, QPainterPath>::const_iterator it = canvas_path.begin(); it != canvas_path.end(); ++it) {
		if (it.value().contains(scene_pt)) {
			// Mouse inside canvas
			QRect canvas_rect = d_data->player->plotWidget2D()->mapFromScene(it.value().boundingRect()).boundingRect();

			int w = MIN_BORDER_DIST;
			if (canvas_rect.width() < w * 2)
				w = canvas_rect.width() / 2;
			int h = MIN_BORDER_DIST;
			if (canvas_rect.height() < h * 2)
				h = canvas_rect.height() / 2;

			// test borders
			/*if (pt.x() < canvas_rect.left() + w) {
			res.side = Vip::Left;
			res.canvas = it.key();
			res.highlight = QRect(canvas_rect.left(), canvas_rect.top(), w, canvas_rect.height());
			res.text = ""
			return res;
			}
			else if (pt.x() > canvas_rect.right() - w) {
			res.side = Vip::Right;
			res.canvas = it.key();
			res.highlight = QRect(canvas_rect.right() - w, canvas_rect.top(), w, canvas_rect.height());
			return res;;
			}
			else*/
			if (pt.y() < canvas_rect.top() + h) {
				if (is_drag_widget)
					return res; // cannot drop drag widget for stacked plot
				res.side = Vip::Top;
				res.canvas = it.key();
				res.highlight = QRect(canvas_rect.left(), canvas_rect.top(), canvas_rect.width(), h);
				res.text = "Stacked plot on the top";
				return res;
			}
			else if (pt.y() > canvas_rect.bottom() - h) {
				if (is_drag_widget)
					return res; // cannot drop drag widget for stacked plot
				res.side = Vip::Bottom;
				res.canvas = it.key();
				res.highlight = QRect(canvas_rect.left(), canvas_rect.bottom() - h, canvas_rect.width(), h);
				res.text = "Stacked plot on the bottom";
				return res;
			}
			else {
				// inside canvas
				res.side = Vip::AllSides;
				res.canvas = it.key();
				res.highlight = canvas_rect.adjusted(w, h, -w, -h);
				if (is_drag_widget)
					res.text = "Swap players";
				else
					res.text = "Add curve to this area";
				return res;
			}
		}
	}

	// Viewport rect
	QRect viewport_rect = d_data->player->plotWidget2D()->viewport()->geometry();

	int w = MIN_BORDER_DIST * 2;
	if (viewport_rect.width() < w * 2)
		w = viewport_rect.width() / 2;
	int h = MIN_BORDER_DIST * 2;
	if (viewport_rect.height() < h * 2)
		h = viewport_rect.height() / 2;

	// test borders
	res.side = Vip::NoSide;

	if (pt.x() < viewport_rect.left() + w) {
		res.side = Vip::Left;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.top(), w, viewport_rect.height());
		res.text = "Create new plot area on the left";
	}
	else if (pt.x() > viewport_rect.right() - w) {
		res.side = Vip::Right;
		res.highlight = QRect(viewport_rect.right() - w, viewport_rect.top(), w, viewport_rect.height());
		res.text = "Create new plot area on the right";
	}
	else if (pt.y() < viewport_rect.top() + h) {
		res.side = Vip::Top;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.top(), viewport_rect.width(), h);
		res.text = "Create new plot area on the top";
	}
	else if (pt.y() > viewport_rect.bottom() - h) {
		res.side = Vip::Bottom;
		res.highlight = QRect(viewport_rect.left(), viewport_rect.bottom() - h, viewport_rect.width(), h);
		res.text = "Create new plot area on the bottom";
	}

	if (res.side != Vip::NoSide) {
		// higlight area
		return res;
	}
	return Anchor();
}

#include <qrubberband.h>
bool CustomizePlotPlayer::eventFilter(QObject* w, QEvent* evt)
{
	if (!d_data->player || !d_data->dragWidget || d_data->dragWidget->isDestroying())
		return false;

	if (evt->type() == QEvent::Destroy)
		return false;

	if (!qobject_cast<VipPlotPlayer*>(d_data->player) || w != d_data->player->plotWidget2D()->viewport())
		return false;

	if (evt->type() == QEvent::Resize)
		QMetaObject::invokeMethod(this, "reorganizeCloseButtons", Qt::QueuedConnection);
	else if (evt->type() == QEvent::Leave)
		reorganizeCloseButtons();

	/**
	handle double click to edit title
	*/
	if (evt->type() == QEvent::MouseButtonDblClick) {
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);
		// convert to scene pos
		QPointF pt = d_data->player->plotWidget2D()->mapToScene(event->VIP_EVT_POSITION());
		QRectF b = d_data->player->plotWidget2D()->area()->titleAxis()->boundingRect();
		if (b.contains(pt)) {
			editTitle();
			return true;
		}
		return false;
	}

	/*
	We want to move a VipPlotPlayer through the internal canvas
	*/

	if (evt->type() == QEvent::MouseButtonPress) {
		// ignore the mouse event if there is a filter installed (like drawing ROI), let the filter do its job
		if (d_data->player->plotWidget2D()->area()->filter())
			return false;

		QMouseEvent* event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton) {

			// Get plot items behind the mouse,
			// check if there are only canvas or grids
			QPointF scene_pt = scenePos(event->VIP_EVT_POSITION());
			bool ok = true;
			if (QGraphicsItem* item = firstVisibleItem(scene_pt)) {
				if (!item->toGraphicsObject())
					ok = false;
				else {
					QGraphicsObject* obj = item->toGraphicsObject();
					if (!qobject_cast<VipPlotCanvas*>(obj))
						ok = false;
				}
			}
			if (ok) {
				// yes, only canvas or grids
				d_data->mousePress = event->VIP_EVT_POSITION();
				return false;
			}
		}
	}
	else if (evt->type() == QEvent::MouseButtonRelease) {
		// disable further dragging
		if (d_data->mousePress != QPoint(-1, -1)) {
			d_data->mousePress = QPoint(-1, -1);
			return false;
		}
	}
	else if (evt->type() == QEvent::Show) {
		QMetaObject::invokeMethod(this, "reorganizeCloseButtons", Qt::QueuedConnection);
	}
	else if (evt->type() == QEvent::MouseMove) {
		reorganizeCloseButtons();
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);
		if (d_data->mousePress != QPoint(-1, -1) && !d_data->player->plotWidget2D()->area()->mouseInUse()) {
			// drag
			if (event->buttons() & Qt::LeftButton) {
				if ((event->VIP_EVT_POSITION() - d_data->mousePress).manhattanLength() > 10) {
					VipBaseDragWidget* _w = VipDragWidget::fromChild(d_data->player);
					QPoint pt = d_data->mousePress;
					d_data->mousePress = QPoint(-1, -1);
					return _w->dragThisWidget(d_data->player->plotWidget2D()->viewport(), pt);
				}
			}
		}
	}

	/**
	Now we want to manage drop events
	*/

	if (evt->type() == QEvent::DragEnter) {
		QDragEnterEvent* event = static_cast<QDragEnterEvent*>(evt);

		d_data->anchor = anchor(event->VIP_EVT_POSITION(), event->mimeData());

		if (d_data->anchor.side != Vip::NoSide && !d_data->anchor.canvas && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->acceptProposedAction();
			return true;
		}

		// Do not accept drag widgets inside
		if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->setAccepted(false);
			return false;
		}
		// Accept VipPlotMimeData objects
		if (qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
			event->accept();
		}
	}
	else if (evt->type() == QEvent::DragMove) {
		QDragMoveEvent* event = static_cast<QDragMoveEvent*>(evt);

		d_data->anchor = anchor(event->VIP_EVT_POSITION(), event->mimeData());
		if (d_data->anchor.side != Vip::NoSide) {
			event->acceptProposedAction();
			// higlight area
			anchorToArea(d_data->anchor, *d_data->area, d_data->player->plotWidget2D());
			d_data->area->show();
			return true;
		}
		else {
			event->setAccepted(false);
			d_data->area->hide();
		}
		return false;
	}
	else if (evt->type() == QEvent::DragLeave) {
		d_data->area->hide();
	}
	else if (evt->type() == QEvent::Drop) {
		d_data->area->hide();
		QDropEvent* event = static_cast<QDropEvent*>(evt);

		if (d_data->anchor.side != Vip::NoSide && !d_data->anchor.canvas) {
			if (VipMultiDragWidget* mw = d_data->dragWidget->parentMultiDragWidget()) {
				QPoint pos = mw->indexOf(d_data->dragWidget);
				VipDragWidgetHandle* h = nullptr;
				if (mw->orientation() == Qt::Vertical) {

					if (d_data->anchor.side == Vip::Right)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (d_data->anchor.side == Vip::Left)
						h = mw->subSplitterHandle(pos.y(), pos.x());
					else if (d_data->anchor.side == Vip::Bottom)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (d_data->anchor.side == Vip::Top)
						h = mw->mainSplitterHandle(pos.y());
				}
				else {
					if (d_data->anchor.side == Vip::Right)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (d_data->anchor.side == Vip::Left)
						h = mw->mainSplitterHandle(pos.y());
					else if (d_data->anchor.side == Vip::Bottom)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (d_data->anchor.side == Vip::Top)
						h = mw->subSplitterHandle(pos.y(), pos.x());
				}

				bool res = h->dropMimeData(event->mimeData());
				if (!res)
					event->setDropAction(Qt::IgnoreAction);
				else
					event->acceptProposedAction();

				// VipDragWidgetHandler::find(d_data->dragWidget->topLevelMultiDragWidget()->parentWidget())->reorganizeMinimizedChildren();
				QMetaObject::invokeMethod(d_data->dragWidget->topLevelMultiDragWidget(), "reorganizeMinimizedChildren");

				return true;
			}
		}
		else if (d_data->anchor.side == Vip::AllSides && d_data->anchor.canvas) {
			if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
				// TODO: player swapping

				VipBaseDragWidget* base = (VipBaseDragWidget*)(event->mimeData()->data("application/dragwidget").toULongLong());
				if (VipDragWidget* d = qobject_cast<VipDragWidget*>(base)) {
					d->parentMultiDragWidget()->swapWidgets(d, d_data->dragWidget);
				}
				event->setDropAction(Qt::IgnoreAction);
				return false;
			}
			else {
				d_data->anchor.canvas->dropMimeData(event->mimeData());
				event->acceptProposedAction();
				return true;
			}
		}
		else if (d_data->anchor.canvas) {
			if (VipVMultiPlotArea2D* a = qobject_cast<VipVMultiPlotArea2D*>(d_data->player->plotWidget2D()->area())) {
				// find canvas index
				VipAbstractScale* left = d_data->anchor.canvas->axes()[1];
				int index = a->leftMultiAxis()->indexOf(static_cast<VipBorderItem*>(left));
				// find insertion index
				if (d_data->anchor.side == Vip::Top)
					++index;

				left = d_data->player->insertLeftScale(index);

				// find canvas
				VipPlotCanvas* canvas = vipListCast<VipPlotCanvas*>(left->plotItems()).first();
				canvas->dropMimeData(event->mimeData());
			}
		}

		// ignore drag widget to be sure that the widget will be moved
		if (!event->mimeData()->data("application/dragwidget").isEmpty())
			event->setDropAction(Qt::IgnoreAction);
	}

	return false;
}

void CustomizePlotPlayer::finishEditingTitle()
{
	// d_data->player->setAutomaticWindowTitle(false);
	// d_data->player->setWindowTitle(_title_editor->text());
}
void CustomizePlotPlayer::titleChanged()
{
	// d_data->title->setText(QString::number(VipUniqueId::id<VipBaseDragWidget>(d_data->dragWidget)));
}

void CustomizePlotPlayer::reorganizeCloseButtons()
{
	if (!d_data->player || !d_data->dragWidget || d_data->dragWidget->isDestroying())
		return;

	// Get mouse pos
	QPoint pt = d_data->player->plotWidget2D()->mapFromGlobal(QCursor::pos());
	QList<VipPlotCanvas*> canvas;

	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(d_data->player->plotWidget2D()->area())) {
		canvas = area->allCanvas();
	}
	else
		canvas << d_data->player->plotWidget2D()->area()->canvas();

	// bool just_created = false;

	// QList<VipPlotCanvas*> canvas = area->allCanvas();
	for (int i = 0; i < canvas.size(); ++i) {
		CloseToolBar* closeBar = static_cast<CloseToolBar*>(canvas[i]->property("_vip_close").value<QToolBar*>());
		if (!closeBar) {
			// just_created = true;
			closeBar = new CloseToolBar(d_data->player);
			closeBar->close->setProperty("_vip_canvas", QVariant::fromValue(canvas[i]));
			closeBar->maximize->setProperty("_vip_canvas", QVariant::fromValue(canvas[i]));
			closeBar->minimize->setProperty("_vip_canvas", QVariant::fromValue(canvas[i]));
			canvas[i]->setProperty("_vip_close", QVariant::fromValue(closeBar));
			connect(closeBar->close, SIGNAL(triggered(bool)), this, SLOT(closeCanvas()));
			connect(closeBar->maximize, SIGNAL(triggered(bool)), this, SLOT(maximizePlayer()));
			connect(closeBar->minimize, SIGNAL(triggered(bool)), this, SLOT(minimizePlayer()));
			connect(canvas[i], SIGNAL(destroyed(VipPlotItem*)), closeBar, SLOT(deleteLater()));
		}

		// update visibility
		QRectF r;
		if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(d_data->player->plotWidget2D()->area())) {
			r = area->plotArea((VipBorderItem*)canvas[i]->axes()[1]).boundingRect();
			r = d_data->player->plotWidget2D()->mapFromScene(area->mapToScene(r)).boundingRect();
		}
		else {
			r = d_data->player->plotWidget2D()->mapFromScene(d_data->player->plotWidget2D()->sceneRect()).boundingRect();
		}
		if (r.contains(pt) && d_data->player->isVisible()) {
			closeBar->setVisible(true);
		}
		else {
			closeBar->setVisible(false);
		}

		QRect crect = d_data->player->plotWidget2D()->mapFromScene(canvas[i]->mapToScene(canvas[i]->boundingRect())).boundingRect();
		int margin = d_data->player->plotWidget2D()->width() - crect.right();
		moveTopRight(closeBar, d_data->player, margin + 1, crect.top() + 1);
	}
}

void CustomizePlotPlayer::closeCanvas()
{
	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(d_data->player->plotWidget2D()->area())) {
		if (sender()) {
			if (VipPlotCanvas* c = sender()->property("_vip_canvas").value<VipPlotCanvas*>()) {
				if (area->allCanvas().size() == 1)
					closePlayer();
				else
					d_data->player->removeLeftScale(c->axes()[1]);
			}
		}
	}
	else {
		if (sender()) {
			if (/*VipPlotCanvas* c = */ sender()->property("_vip_canvas").value<VipPlotCanvas*>()) {
				closePlayer();
			}
		}
	}
}

static void updatePlotPlayer(VipPlotPlayer* pl)
{
	if (!VipBaseDragWidget::fromChild(pl))
		return;
	QObject* obj = pl->findChild<BaseCustomPlayer2D*>(QString(), Qt::FindDirectChildrenOnly);
	if (!obj)
		new CustomizePlotPlayer(pl);
}
static void updateVideoPlayer(VipVideoPlayer* pl)
{
	if (!VipBaseDragWidget::fromChild(pl))
		return;
	QObject* obj = pl->findChild<BaseCustomPlayer2D*>(QString(), Qt::FindDirectChildrenOnly);
	if (!obj)
		new CustomizeVideoPlayer(pl);
}
static void updateWidgetPlayer(VipWidgetPlayer* pl)
{
	if (!VipBaseDragWidget::fromChild(pl))
		return;
	QObject* obj = pl->findChild<CustomWidgetPlayer*>(QString(), Qt::FindDirectChildrenOnly);
	if (!obj)
		new CustomWidgetPlayer(pl);
}

static void resizeSplitter(QSplitter* splitter)
{
	QList<int> sizes;
	for (int i = 0; i < splitter->count(); ++i)
		sizes.append(1);
	splitter->setSizes(sizes);
	splitter->setOpaqueResize(true);
}

static QAction* createSeparator()
{
	QAction* sep = new QAction(nullptr);
	sep->setSeparator(true);
	return sep;
}

static QList<QAction*> additionalActions(VipPlotItem* /*item*/, VipPlayer2D* player)
{
	QList<QAction*> res;

	if (QObject* obj = player->findChild<BaseCustomPlayer2D*>(QString(), Qt::FindDirectChildrenOnly)) {
		QAction* title = new QAction(nullptr);
		title->setText("Edit title");
		res << title << createSeparator();
		QObject::connect(title, SIGNAL(triggered(bool)), obj, SLOT(editTitle()));
	}

	// get parent drag widget and multi drag widget
	if (VipDragWidget* dw = qobject_cast<VipDragWidget*>(VipBaseDragWidget::fromChild(player)))
		if (VipMultiDragWidget* mw = dw->parentMultiDragWidget()) {
			int count = 0;
			QPoint pt = mw->indexOf(dw);
			if (pt != QPoint(-1, -1)) {
				int s = mw->subCount(pt.y());
				if (s > 1) {
					QAction* actc = new QAction(nullptr);
					actc->setText("Resize columns");
					QObject::connect(actc, &QAction::triggered, actc, std::bind(resizeSplitter, mw->subSplitter(pt.y())));
					res << actc;
					++count;
				}
			}

			if (mw->mainCount() > 1) {
				QAction* actr = new QAction(nullptr);
				actr->setText("Resize rows");
				QObject::connect(actr, &QAction::triggered, actr, std::bind(resizeSplitter, mw->mainSplitter()));
				res << actr;
				++count;
			}

			if (count)
				res << createSeparator();

			QAction* savei = new QAction("Save as image...", nullptr);
			QAction* saves = new QAction("Save as session...", nullptr);
			res << savei << saves;
			QObject::connect(savei, &QAction::triggered, std::bind(vipSaveImage, dw));   // dw, SLOT(saveImage()));
			QObject::connect(saves, &QAction::triggered, std::bind(vipSaveSession, dw)); // dw, SLOT(saveSession()));

			if (mw->count() > 1) {
				res << createSeparator();
				if (!dw->isMaximized()) {
					QAction* maximize = new QAction("Maximize window", nullptr);
					QObject::connect(maximize, SIGNAL(triggered(bool)), dw, SLOT(showMaximized()));
					res << maximize;
				}
				if (dw->isMaximized()) {
					QAction* restore = new QAction("Restore window", nullptr);
					QObject::connect(restore, SIGNAL(triggered(bool)), dw, SLOT(showNormal()));
					res << restore;
				}
				QAction* minimize = new QAction("Minimize window", nullptr);
				QObject::connect(minimize, SIGNAL(triggered(bool)), dw, SLOT(showMinimized()));
				res << minimize;
			}
			QAction* close = new QAction("Close window", nullptr);
			QObject::connect(close, &QAction::triggered, dw, std::bind(restoreAndClose, player));
			res << close;
		}
	return res;
}

#include <qboxlayout.h>
struct CustomizePlayer : public QObject
{
	void customize(VipAbstractPlayer* p)
	{
		// hide tool bars and status bar
		if (VipPlayer2D* pl = qobject_cast<VipPlayer2D*>(p)) {
			QList<QToolBar*> ws = pl->toolBars();
			for (int i = 0; i < ws.size(); ++i)
				ws[i]->hide();

			// apply mouse selection to all
			if (qobject_cast<VipPlotPlayer*>(p))
				QObject::connect(pl, &VipPlayer2D::mouseSelectionChanged, this, &CustomizePlayer::mousePlotSelectionChanged);
			else if (/*VipVideoPlayer *vp = */ qobject_cast<VipVideoPlayer*>(p)) {
				QObject::connect(pl, &VipPlayer2D::mouseSelectionChanged, this, &CustomizePlayer::mouseVideoSelectionChanged);
			}
		}
	}
	void destroyed(VipAbstractPlayer* p)
	{
		// destroy the tool bars
		if (VipPlayer2D* player = qobject_cast<VipPlayer2D*>(p)) {
			QWidget* tmp = player->property("_vip_topToolBar").value<WidgetPointer>();
			if (tmp) {
				player->setProperty("_vip_topToolBar", QVariant());
				if (tmp)
					tmp->deleteLater();
			}
		}
	}

	void focusWidgetChanged(VipDragWidget* w)
	{
		static QPointer<VipDragWidget> prev_focus;

		if (prev_focus) {
			prev_focus->setStyleSheet("VipDragWidget{ border: none;}");
		}
		prev_focus = w;

		if (!w) {
			// empty workspace, hide all tool bars
			QWidget* top = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->topWidget();
			QList<QWidget*> toolbars = top->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
			for (int i = 0; i < toolbars.size(); ++i)
				toolbars[i]->hide();
			return;
		}

		// make sure to create the tool bars before
		if (VipVideoPlayer* video = qobject_cast<VipVideoPlayer*>(w->widget()))
			updateVideoPlayer(video);
		else if (VipPlotPlayer* plot = qobject_cast<VipPlotPlayer*>(w->widget()))
			updatePlotPlayer(plot);
		else if (VipWidgetPlayer* widget = qobject_cast<VipWidgetPlayer*>(w->widget()))
			updateWidgetPlayer(widget);

		// hide all tool bars
		QWidget* top = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->topWidget();
		QList<QWidget*> toolbars = top->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
		for (int i = 0; i < toolbars.size(); ++i)
			toolbars[i]->hide();

		if (VipAbstractPlayer* p = qobject_cast<VipAbstractPlayer*>(w->widget())) {
			QWidget* toolbar = p->property("_vip_topToolBar").value<WidgetPointer>();
			if (toolbar) {
				QLayout* lay = top->layout();
				if (!lay) {
					lay = new QVBoxLayout();
					lay->setSpacing(0);
					lay->setContentsMargins(0, 0, 0, 0);
					top->setLayout(lay);
				}
				if (toolbar->parent() != top)
					lay->addWidget(toolbar);
				toolbar->show();
				top->show();
			}
		}

		QColor c = VipGuiDisplayParamaters::instance()->defaultPlayerBackgroundColor();
		c = c.darker(120);
		w->setStyleSheet("VipDragWidget{ border: 1px solid #" + QString::number(c.rgba(), 16) + ";}");

		return;
	}

	void mousePlotSelectionChanged(bool enable)
	{
		QList<VipPlotPlayer*> pls = vipGetMainWindow()->displayArea()->findChildren<VipPlotPlayer*>();
		for (int i = 0; i < pls.size(); ++i) {
			pls[i]->blockSignals(true);
			pls[i]->selectionZoomArea(enable);
			pls[i]->blockSignals(false);
		}
	}

	void mouseVideoSelectionChanged(bool enable)
	{
		QList<VipVideoPlayer*> pls = vipGetMainWindow()->displayArea()->findChildren<VipVideoPlayer*>();
		for (int i = 0; i < pls.size(); ++i) {
			pls[i]->blockSignals(true);
			pls[i]->selectionZoomArea(enable);
			pls[i]->blockSignals(false);
		}
	}

	// TEST: comment
	/* void splitterChildChanged(QSplitter* s, QWidget*, bool)
	{
		//TODO: at this point, the splitter might have been destroyed, check!
		if (s->property("_vip_dirtySplitter").toInt() == 0)
			resizeSplitter(s);
	}

	void receivedSplitterMoved(QSplitter* s, int, int)
	{
		s->setProperty("_vip_dirtySplitter", 1);
		if (VipDragWidgetSplitter * splitter = qobject_cast<VipDragWidgetSplitter*>(s)) {
			QObject::disconnect(splitter, &VipDragWidgetSplitter::childChanged, this, &CustomizePlayer::splitterChildChanged);
			QObject::connect(splitter, &VipDragWidgetSplitter::childChanged, this, &CustomizePlayer::splitterChildChanged, Qt::QueuedConnection);
		}

	}*/
};

static CustomizePlayer* customizePlayer()
{
	static CustomizePlayer* inst = new CustomizePlayer();
	return inst;
}

static void customizeMultiDragWidget(VipMultiDragWidget* w)
{
	w->setSupportedOperation(VipBaseDragWidget::DragWidgetExtract, false);
	w->setSupportedOperation(VipBaseDragWidget::Minimize, false);
	w->setMaximumHandleWidth(5);
	QColor c = VipGuiDisplayParamaters::instance()->defaultPlayerBackgroundColor();
	w->setStyleSheet(mutliDragWidgetStyleSheet(c));

	// TEST: comment
	// QObject::connect(w, &VipMultiDragWidget::splitterMoved, customizePlayer(), &CustomizePlayer::receivedSplitterMoved);
}

//
// Registered functions for python wrapper
//

static QVariant setCurrentWorkspaceMaxColumns(const QVariantList& lst)
{
	if (lst.size() != 1 || !lst.first().canConvert<int>()) {
		return QVariant::fromValue(VipErrorData("setCurrentWorkspaceMaxColumns: wrong input argument (should be an integer value)"));
	}
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		int value = lst.first().toInt();
		if (value <= 0)
			return QVariant::fromValue(VipErrorData("setCurrentWorkspaceMaxColumns: wrong input value (" + QString::number(value) + ")"));
		if (QSpinBox* box = vipGetMainWindow()->closeBar()->findChild<QSpinBox*>("_vip_maxCols")) {
			area->setProperty("_vip_customNumCols", value);
			if (box->value() != value)
				box->setValue(value);
		}
	}
	else
		return QVariant::fromValue(VipErrorData("setCurrentWorkspaceMaxColumns: no valid workspace available"));

	return QVariant();
}

static QVariant currentWorkspaceMaxColumns(const QVariantList&)
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		return QVariant::fromValue(area->property("_vip_customNumCols").toInt());
	}
	else
		return QVariant::fromValue(VipErrorData("currentWorkspaceMaxColumns: no valid workspace available"));
}

static QVariant reorganizeCurrentWorkspace(const QVariantList&)
{
	VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if (!area)
		return QVariant::fromValue(VipErrorData("reorganizeCurrentWorkspace: no valid workspace available"));

	int max_cols = area->property("_vip_customNumCols").toInt();
	if (max_cols <= 0)
		return QVariant::fromValue(VipErrorData("reorganizeCurrentWorkspace: wrong maximum columns (" + QString::number(max_cols) + ")"));

	// build the list of all available players
	QList<VipDragWidget*> players;
	VipMultiDragWidget* main = area->mainDragWidget(QList<QWidget*>(), false);
	if (!main)
		return QVariant::fromValue(VipErrorData("reorganizeCurrentWorkspace: no valid workspace available"));

	for (int y = 0; y < main->mainCount(); ++y)
		for (int x = 0; x < main->subCount(y); ++x) {
			if (VipDragWidget* w = qobject_cast<VipDragWidget*>(main->widget(y, x, 0))) {
				players.append(w);
				w->setParent(nullptr);
			}
		}

	int width = max_cols;
	int height = players.size() / width;
	if (players.size() % width)
		++height;

	if (main->mainCount() > height)
		main->mainResize(height);

	for (int i = 0; i < players.size(); ++i) {
		int y = i / width;
		int x = i % width;

		if (y + 1 > main->mainCount())
			main->mainResize(y + 1);
		if (x + 1 > main->subCount(y))
			main->subResize(y, x + 1);
		else if (main->subCount(y) > width)
			main->subResize(y, width);

		main->setWidget(y, x, players[i]);
	}

	return QVariant();
}

static int registerCustomPlotPlayer()
{
	vipRegisterFunction(setCurrentWorkspaceMaxColumns, "setCurrentWorkspaceMaxColumns", QString());
	vipRegisterFunction(currentWorkspaceMaxColumns, "currentWorkspaceMaxColumns", QString());
	vipRegisterFunction(reorganizeCurrentWorkspace, "reorganizeCurrentWorkspace", QString());

	qRegisterMetaType<WidgetPointer>();

	VipMultiDragWidget::onCreated(customizeMultiDragWidget);

	// focus widget changed: update the common tool bar
	QObject::connect(vipGetMainWindow()->displayArea(), &VipDisplayArea::focusWidgetChanged, customizePlayer(), &CustomizePlayer::focusWidgetChanged);

	// customize players
	// make sure to hide the tool bars BEFORE showing the plot player
	QObject::connect(VipPlayerLifeTime::instance(), &VipPlayerLifeTime::created, customizePlayer(), &CustomizePlayer::customize);
	QObject::connect(VipPlayerLifeTime::instance(), &VipPlayerLifeTime::destroyed, customizePlayer(), &CustomizePlayer::destroyed);
	vipFDPlayerCreated().append<void(VipPlotPlayer*)>(updatePlotPlayer);
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(updateVideoPlayer);
	vipFDPlayerCreated().append<void(VipWidgetPlayer*)>(updateWidgetPlayer);

	// additional actions on right click
	VipFDItemRightClick().append<QList<QAction*>(VipPlotItem*, VipPlayer2D*)>(additionalActions);

	// disable the creation of stacked plot based on the curve unit
	VipPlotPlayer::setNewItemBehaviorEnabled(false);

	// set title visible by default
	// QMetaObject::invokeMethod(VipGuiDisplayParamaters::instance()->defaultPlotArea()->titleAxis(), "setVisible", Qt::QueuedConnection, Q_ARG(bool, true));

	return 0;
}

static int register_functions = vipAddGuiInitializationFunction(registerCustomPlotPlayer);
