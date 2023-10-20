#include "p_CustomInterface.h"

#include "VipDragWidget.h"
#include "VipPlotMimeData.h"
#include "VipLegendItem.h"
#include "VipDynGridLayout.h"
#include "VipMultiPlotWidget2D.h"
#include "VipDisplayArea.h"
#include "VipGui.h"
#include "VipMimeData.h"

#include <qtooltip.h>
#include <qtextdocument.h>
#include <qtoolbutton.h>
#include <qwidgetaction.h>
#include <qboxlayout.h>
#include <qrubberband.h>
#include <qapplication.h>
#include <qclipboard.h>

#define HIGHLIGHT_MARGIN 5
#define MIN_BORDER_DIST 20

typedef QPointer<QWidget> WidgetPointer;
Q_DECLARE_METATYPE(WidgetPointer)



static void anchorToArea(const Anchor& a, VipDragRubberBand& area, QWidget* widget)
{
	QPoint top_left = widget->mapToGlobal(a.highlight.topLeft());

	QRect geom = QRect(top_left.x() - HIGHLIGHT_MARGIN, top_left.y() - HIGHLIGHT_MARGIN,
		a.highlight.width() + HIGHLIGHT_MARGIN * 2, a.highlight.height() + HIGHLIGHT_MARGIN * 2);

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

	geom = QRect(vipGetMainWindow()->mapFromGlobal(geom.topLeft()), vipGetMainWindow()->mapFromGlobal(geom.bottomRight()));
	area.setGeometry(geom);
	area.text = a.text;
}




static QString mutliDragWidgetStyleSheet(const QColor & background)
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



static void restoreAndClose(QWidget * widget)
{
	VipDragWidget * drag = qobject_cast<VipDragWidget*>(VipDragWidget::fromChild(widget));
	if (drag) {
		if (drag->isMaximized())
			drag->showNormal();
		drag->close();
	}
}

static QWidget * create_player_top_toolbar(VipAbstractPlayer * player, QObject * /*o*/)
{
	QWidget * tmp = player->property("_vip_topToolBar").value<WidgetPointer>();
	if (tmp)
		return tmp;

	VipBaseDragWidget * w = VipBaseDragWidget::fromChild(player);
	if (!w)
		return nullptr;

	VipPlayer2D* pl2D = qobject_cast<VipPlayer2D*>(player);

	QHBoxLayout * hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);

	//add the main tool bar and the title tool bar
	//player->toolBar()->setCustomBehaviorEnabled(false);
//	player->toolBar()->setShowAdditionals(VipToolBar::ShowInMenu);
	hlay->addWidget(player->playerToolBar());
	QToolBar* title = new QToolBar();
	title->setIconSize(QSize(20, 20));
	hlay->addStretch(1);
	hlay->addWidget(title);


	//add status text
	if(pl2D)
		title->addWidget(pl2D->statusText());
	 

	QLabel* titleWidget = new QLabel(w->windowTitle());
	QObject::connect(w, SIGNAL(windowTitleChanged(const QString &)), titleWidget, SLOT(setText(const QString&)));
	title->addWidget(new QLabel("<b>&nbsp;&nbsp;Title</b>: "));
	title->addWidget(titleWidget);
	if (pl2D)
		title->addWidget(pl2D->afterTitleToolBar());
	
	QWidget * res = new QWidget();
	QVBoxLayout * lay = new QVBoxLayout();
	lay->setSpacing(0);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addLayout(hlay);
	res->setLayout(lay);

	//get new tool bars
	if (pl2D) {
		QList<QToolBar*> new_bars = pl2D->toolBars();
		for (int i = 1; i < new_bars.size(); ++i) {
			lay->addWidget(new_bars[i]);
			new_bars[i]->show();
			if (VipToolBar* bar = qobject_cast<VipToolBar*>(new_bars[i]))
				bar->setCustomBehaviorEnabled(false);
		}
	}
	if(player->playerToolBar())
		player->playerToolBar()->show();

	player->setProperty("_vip_topToolBar", QVariant::fromValue(WidgetPointer(res)));
	return res;
}


struct EditTitle : QLineEdit
{
	EditTitle(QWidget * parent)
		:QLineEdit(parent) {
		connect(this, SIGNAL(returnPressed()), this, SLOT(deleteLater()));
		setToolTip("<b>Edit title</b><br>Press ENTER to finish");
	}
	virtual void	focusOutEvent(QFocusEvent *) {
		this->deleteLater();
	}
};
static QPointer<EditTitle> _title_editor;
static void finishEditingTitle(VipPlayer2D * player)
{
	player->setAutomaticWindowTitle(false);
	player->setWindowTitle(_title_editor->text());
}
static void editTitle(VipPlayer2D * player)
{
	//setTopWidgetsVisible(false);
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
	if (dragWidget())
	{
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
	if (dragWidget())
	{
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
	if(player() && player()->plotWidget2D()) {
		QList<QGraphicsItem*> items = player()->plotWidget2D()->area()->scene()->items();
		QList<QGraphicsObject*> oitems = vipCastItemList< QGraphicsObject*>(items,QString(),1,1);
		QList<QPointer<QGraphicsObject> > pitems;
		for (int i = 0; i < oitems.size(); ++i)
			pitems.append(oitems[i]);
		for (int i = 0; i < pitems.size(); ++i) {
			if(pitems[i])
				pitems[i]->setSelected(false);
		}
	}
}


void BaseCustomPlayer2D::updateTitle()
{
	VipText t = player()->plotWidget2D()->area()->title();
	t.setText(player()->windowTitle());
	player()->plotWidget2D()->area()->setTitle(t);
	//player()->plotWidget2D()->area()->titleAxis()->setVisible(!t.isEmpty());
	//static_cast<VipPlotWidget2D*>(player()->plotWidget2D())->area()->topAxis()->setTitle(t);
}
void BaseCustomPlayer2D::editTitle()
{
	::editTitle(player());
}
QPointF BaseCustomPlayer2D::scenePos(const QPoint & viewport_pos) const
{
	QPoint view_pt = player()->plotWidget2D()->viewport()->mapTo(player()->plotWidget2D(), viewport_pos);
	QPointF scene_pt = player()->plotWidget2D()->mapToScene(view_pt);
	return scene_pt;
}
QGraphicsItem * BaseCustomPlayer2D::firstVisibleItem(const QPointF & scene_pos) const
{
	QList<QGraphicsItem*> items = player()->plotWidget2D()->area()->scene()->items(scene_pos);
	for (int i = 0; i < items.size(); ++i) {
		if (!items[i]->isVisible()) continue;
		if (qobject_cast<VipPlotGrid*>(items[i]->toGraphicsObject()) ||
			qobject_cast<VipRubberBand*>(items[i]->toGraphicsObject()) ||
			qobject_cast<VipPlotMarker*>(items[i]->toGraphicsObject())/*||
																	  qobject_cast<VipDrawSelectionOrder*>(items[i]->toGraphicsObject())*/) continue;
		return items[i];
	}
	return nullptr;
}



class CustomizeVideoPlayer::PrivateData
{
public:
	VipVideoPlayer * player;
	VipDragWidget * dragWidget;
	QToolButton * close;
	QToolButton * maximize;
	QToolButton * minimize;
	QPoint mousePress;
	Anchor anchor;
	QPointer<VipDragRubberBand> area;
	bool closeRequested;
	PrivateData()
		:mousePress(-1, -1), closeRequested(false)
	{}
};

CustomizeVideoPlayer::CustomizeVideoPlayer(VipVideoPlayer * player)
	:BaseCustomPlayer2D(player)
{
	m_data = new PrivateData();
	m_data->player = player;
	m_data->dragWidget = nullptr;

	//m_data->area->SetBrush(QBrush(Qt::NoBrush));
	QWidget * parent = player->parentWidget();
	VipMultiDragWidget * top = nullptr;
	while (parent) {
		if (!m_data->dragWidget) m_data->dragWidget = qobject_cast<VipDragWidget*>(parent);
		if ((top = qobject_cast<VipMultiDragWidget*>(parent))) {
			break;
		}
		parent = parent->parentWidget();
	}
	if (!m_data->dragWidget)
		return;

	m_data->area = new VipDragRubberBand(vipGetMainWindow());
	m_data->player->plotWidget2D()->viewport()->installEventFilter(this);

	//TEST: comment
	//player->plotWidget2D()->setBackgroundColor(defaultPlayerBackground());

	m_data->close = new QToolButton(m_data->player);
	m_data->close->setAutoRaise(true);
	m_data->close->setToolTip("Close video");
	m_data->close->setIcon(vipIcon("close.png"));
	m_data->close->resize(20, 20);
	connect(m_data->close, SIGNAL(clicked(bool)), this, SLOT(closePlayer()));

	m_data->maximize = new QToolButton(m_data->player);
	m_data->maximize->setAutoRaise(true);
	m_data->maximize->setToolTip("Maximize/restore video");
	m_data->maximize->setIcon(vipIcon("restore.png"));
	m_data->maximize->resize(20, 20);
	connect(m_data->maximize, SIGNAL(clicked(bool)), this, SLOT(maximizePlayer()));

	m_data->minimize = new QToolButton(m_data->player);
	m_data->minimize->setAutoRaise(true);
	m_data->minimize->setToolTip("Maximize/restore video");
	m_data->minimize->setIcon(vipIcon("minimize.png"));
	m_data->minimize->resize(20, 20);
	connect(m_data->minimize, SIGNAL(clicked(bool)), this, SLOT(minimizePlayer()));

	connect(m_data->player->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), this, SLOT(reorganizeCloseButton()));
	reorganizeCloseButton();

	//title management
	VipText t = m_data->player->plotWidget2D()->area()->title();
	QFont f = t.font();
	//f.setBold(true);
	t.setFont(f);
	//t.setLayoutAttribute(VipText::MinimumLayout);
	//static_cast<VipPlotWidget2D*>(m_data->player->plotWidget2D())->area()->topAxis()->setTitle(t);
	m_data->player->plotWidget2D()->area()->setTitle(t);
	updateTitle();
	connect(player, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(updateTitle()));


	


	//moveTitleBarActionsToToolBar(qobject_cast<VipDragWidgetHeader*>(m_data->dragWidget->Header()), player->toolBar(), this);
	create_player_top_toolbar(player, this);

	connect(player, SIGNAL(renderEnded(const VipRenderState&)), this, SLOT(endRender()));
	/*
	printf("area: %s\n", QString::number((qint64)m_data->area.data(),16).toLatin1().data());
	printf("close: %s\n", QString::number((qint64)m_data->close, 16).toLatin1().data());
	printf("status: %s\n", QString::number((qint64)m_data->player->statusBar(), 16).toLatin1().data());
	printf("viewport: %s\n", QString::number((qint64)m_data->player->plotWidget2D()->viewport(), 16).toLatin1().data());*/
}
CustomizeVideoPlayer::~CustomizeVideoPlayer()
{
	if (m_data->area)
		m_data->area->deleteLater();
	delete m_data;
}

VipDragWidget * CustomizeVideoPlayer::dragWidget() const
{
	return m_data->dragWidget;
}

void CustomizeVideoPlayer::updateViewport(QWidget* viewport)
{
	viewport->installEventFilter(this);
}

QToolButton * CustomizeVideoPlayer::maximizeButton() const
{
	return m_data->maximize;
}
QToolButton * CustomizeVideoPlayer::minimizeButton() const
{
	return m_data->minimize;
}
QToolButton * CustomizeVideoPlayer::closeButton() const
{
	return m_data->close;
}


void CustomizeVideoPlayer::endRender()
{
	//TEST: comment
	//m_data->player->plotWidget2D()->setBackgroundColor(defaultPlayerBackground());
}
#include <qscrollbar.h>
void CustomizeVideoPlayer::reorganizeCloseButton()
{
	if (!m_data->player || !m_data->dragWidget || m_data->dragWidget->isDestroying())
		return;

	QScrollBar * bar = m_data->player->plotWidget2D()->verticalScrollBar();
	m_data->close->move(m_data->player->width() - m_data->close->width() - (bar->isVisible() ? bar->width() : 0), 0);
	m_data->maximize->move(m_data->player->width() - m_data->close->width() - m_data->maximize->width() - (bar->isVisible() ? bar->width() : 0), 0);
	m_data->minimize->move(m_data->player->width() - m_data->close->width() - m_data->maximize->width() - m_data->minimize->width() - (bar->isVisible() ? bar->width() : 0), 0);

	QPoint pt = m_data->player->plotWidget2D()->mapFromGlobal(QCursor::pos());
	//update visibility
	QRect r = m_data->player->plotWidget2D()->rect();
	if (r.contains(pt)) {
		m_data->close->setVisible(true);
		m_data->maximize->setVisible(true);
		m_data->minimize->setVisible(true);
	}
	else {
		m_data->close->setVisible(false);
		m_data->maximize->setVisible(false);
		m_data->minimize->setVisible(false);
	}
}


Anchor CustomizeVideoPlayer::anchor(const QPoint & viewport_pos, const QMimeData * mime)
{
	QPointF scene_pt = scenePos(viewport_pos); //mouse in scene coordinates
	QPoint pt = m_data->player->plotWidget2D()->viewport()->mapTo(m_data->player->plotWidget2D(), viewport_pos); //mouse in view coordinates
	bool is_drag_widget = mime && !mime->data("application/dragwidget").isEmpty();
	/*
	Highlight areas
	We define several areas:
	- Inside a canvas (add curve on existing canvas)
	- Inside a canvas but on a border (add curve on a new stacked panel)
	- Outside canvas (add plot in a new player)
	*/


	//Buils scene path
	VipPlotCanvas * canvas = m_data->player->plotWidget2D()->area()->canvas();
	QRectF r = canvas->mapToScene(canvas->boundingRect()).boundingRect();

	//find area to highlight
	Anchor res;
	//Viewport rect
	QRect viewport_rect = m_data->player->plotWidget2D()->viewport()->geometry();
	QRect canvas_rect = m_data->player->plotWidget2D()->mapFromScene(r).boundingRect();

	int w_l = canvas_rect.left() - viewport_rect.left();
	int w_r = viewport_rect.right() - canvas_rect.right();
	int h_t = canvas_rect.top() - viewport_rect.top();
	int h_b = viewport_rect.bottom() - canvas_rect.bottom();

	if (w_l < MIN_BORDER_DIST) w_l = MIN_BORDER_DIST;
	if (w_r < MIN_BORDER_DIST) w_r = MIN_BORDER_DIST;
	if (h_t < MIN_BORDER_DIST) h_t = MIN_BORDER_DIST;
	if (h_b < MIN_BORDER_DIST) h_b = MIN_BORDER_DIST;

	if (viewport_rect.width() < w_l * 2) w_l = viewport_rect.width() / 2;
	if (viewport_rect.width() < w_r * 2) w_r = viewport_rect.width() / 2;
	if (viewport_rect.height() < h_b * 2) h_b = viewport_rect.height() / 2;
	if (viewport_rect.height() < h_t * 2) h_t = viewport_rect.height() / 2;

	
	//test borders
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
		res.highlight = m_data->player->plotWidget2D()->mapFromScene(r).boundingRect();
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

void CustomizeVideoPlayer::addToolBarWidget(QWidget * w)
{
	if (m_data->player)
		m_data->player->toolBar()->addWidget(w);
}


static void unselectAll(QGraphicsScene * scene)
{
	QList<QGraphicsItem*> items = scene->items();
	foreach(QGraphicsItem *item, items) {
		item->setSelected(false);
	}
}

bool CustomizeVideoPlayer::eventFilter(QObject * , QEvent * evt)
{
	if (!m_data->player || !m_data->dragWidget || m_data->dragWidget->isDestroying())
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

	if (evt->type() == QEvent::MouseButtonPress)
	{
		//ignore the mouse event if there is a filter installed (like drawing ROI), let the filter do its job
		if (m_data->player->plotWidget2D()->area()->filter())
			return false;

		QMouseEvent * event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton) {

			//Get plot items behind the mouse,
			//check if there are only canvas or grids
			QPointF scene_pt = scenePos(event->pos());
			bool ok = true;
			if (QGraphicsItem * item = firstVisibleItem(scene_pt)) {
				if (!item->toGraphicsObject()) ok = false;
				else {
					QGraphicsObject * obj = item->toGraphicsObject();
					if (!qobject_cast<VipPlotCanvas*>(obj) && !qobject_cast<VipPlotSpectrogram*>(obj)) ok = false;
				}
			}
			if (ok) {
				//check potential key modifiers
				int key_modifiers = m_data->player->property("_vip_moveKeyModifiers").toInt();
				if (key_modifiers && !(key_modifiers & event->modifiers()))
					return false;
				//yes, only canvas or grids
				m_data->mousePress = event->pos();
				return false;//true;
			}
		}
	}
	else if (evt->type() == QEvent::MouseButtonRelease)
	{
		//disable further dragging
		if (m_data->mousePress != QPoint(-1, -1)) {
			bool same_pos = (m_data->mousePress - QPointF(static_cast<QMouseEvent*>(evt)->pos())).manhattanLength() < 10;
			m_data->mousePress = QPoint(-1, -1);
			//we need to unselect all items since this is a simple click
			if (same_pos)
				//this->unselectAll();
				QMetaObject::invokeMethod(this, "unselectAll"/*std::bind(unselectAll, m_data->player->plotWidget2D()->area()->scene())*/, Qt::QueuedConnection);
			return false;
		}
	}
	else if (evt->type() == QEvent::MouseMove)
	{
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton)
			//disable dragging the spectrogram to make sure we keep receiving mouse events
			m_data->player->spectrogram()->setItemAttribute(VipPlotItem::Droppable, false);
			if (m_data->mousePress != QPoint(-1, -1) && !m_data->player->plotWidget2D()->area()->mouseInUse()) {
				//drag
				if ((event->pos() - m_data->mousePress).manhattanLength() > 10) {
					VipBaseDragWidget* w = VipDragWidget::fromChild(m_data->player);
					QPoint pt = m_data->mousePress;
					m_data->mousePress = QPoint(-1, -1);
					return  w->dragThisWidget(m_data->player->plotWidget2D()->viewport(), pt);
				}

			}
	}
	

	/**
	Now we want to manage drop events
	*/

	if (evt->type() == QEvent::DragEnter)
	{
		QDragEnterEvent * event = static_cast<QDragEnterEvent*>(evt);

		m_data->anchor = anchor(event->pos(), event->mimeData());

		if (m_data->anchor.side != Vip::NoSide && !m_data->anchor.canvas && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->acceptProposedAction();
			return true;
		}

		//Do not accept drag widgets inside
		if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->setAccepted(false);
			return false;
		}
		//Do not accept VipPlotMimeData objects
		if (qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
			event->setAccepted(false);
			return false;
		}

	}
	else if (evt->type() == QEvent::DragMove)
	{
		
		QDragMoveEvent * event = static_cast<QDragMoveEvent*>(evt);

		m_data->anchor = anchor(event->pos(), event->mimeData());
		if (m_data->anchor.side != Vip::NoSide) {
			//higlight area
			anchorToArea(m_data->anchor, *m_data->area, m_data->player->plotWidget2D());
			m_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else if (m_data->anchor.side == Vip::AllSides && !m_data->anchor.canvas && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			//higlight area
			anchorToArea(m_data->anchor, *m_data->area, m_data->player->plotWidget2D());
			m_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else {
			event->setAccepted(false);
			m_data->area->hide();
		}
		return false;
	}
	else if (evt->type() == QEvent::DragLeave)
	{
		//if(!m_data->area->geometry().contains(QCursor::pos()) || !m_data->area->isVisible())
		m_data->area->hide();
	}
	else if (evt->type() == QEvent::Drop)
	{
		m_data->area->hide();
		QDropEvent * event = static_cast<QDropEvent*>(evt);

		if (m_data->anchor.side != Vip::NoSide && !m_data->anchor.canvas)
		{
			if (VipMultiDragWidget * mw = m_data->dragWidget->parentMultiDragWidget()) {
				QPoint pos = mw->indexOf(m_data->dragWidget);
				VipDragWidgetHandle * h = nullptr;

				if (mw->orientation() == Qt::Vertical) {

					if (m_data->anchor.side == Vip::Right)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (m_data->anchor.side == Vip::Left)
						h = mw->subSplitterHandle(pos.y(), pos.x());
					else if (m_data->anchor.side == Vip::Bottom)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (m_data->anchor.side == Vip::Top)
						h = mw->mainSplitterHandle(pos.y());
				}
				else {
					if (m_data->anchor.side == Vip::Right)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (m_data->anchor.side == Vip::Left)
						h = mw->mainSplitterHandle(pos.y());
					else if (m_data->anchor.side == Vip::Bottom)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (m_data->anchor.side == Vip::Top)
						h = mw->subSplitterHandle(pos.y(), pos.x());
				}

				bool res = h->dropMimeData(event->mimeData());
				if (!res)
					event->setDropAction(Qt::IgnoreAction);
				else
					event->acceptProposedAction();

				//VipDragWidgetHandler::find(m_data->dragWidget->topLevelMultiDragWidget()->parentWidget())->reorganizeMinimizedChildren();
				QMetaObject::invokeMethod(m_data->dragWidget->topLevelMultiDragWidget(), "reorganizeMinimizedChildren");
				return true;
			}

		}
		else if (m_data->anchor.side == Vip::AllSides  && m_data->anchor.canvas)
		{
			if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
				//TODO: player swapping

				VipBaseDragWidget * base = (VipBaseDragWidget*)(event->mimeData()->data("application/dragwidget").toULongLong());
				if (VipDragWidget * d = qobject_cast<VipDragWidget*>(base)) {
					d->parentMultiDragWidget()->swapWidgets(d, m_data->dragWidget);
				}
				event->setDropAction(Qt::IgnoreAction);
				return true;
			}
			else {
				m_data->anchor.canvas->dropMimeData(event->mimeData());
				event->acceptProposedAction();
				return true;
			}
			//return false;
		}

		//ignore drag widget to be sure that the widget will be moved
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
	QToolButton* close;
	QToolButton* maximize;
	QToolButton* minimize;
	QPoint mousePress;
	Anchor anchor;
	QPointer<VipDragRubberBand> area;
	bool closeRequested;
	PrivateData()
		:mousePress(-1, -1), closeRequested(false)
	{}
};

CustomWidgetPlayer::CustomWidgetPlayer(VipWidgetPlayer* player)
	:BaseCustomPlayer(player)
{
	m_data = new PrivateData();
	m_data->player = player;
	m_data->dragWidget = nullptr;

	//m_data->area->SetBrush(QBrush(Qt::NoBrush));
	QWidget* parent = player->parentWidget();
	VipMultiDragWidget* top = nullptr;
	while (parent) {
		if (!m_data->dragWidget) m_data->dragWidget = qobject_cast<VipDragWidget*>(parent);
		if ((top = qobject_cast<VipMultiDragWidget*>(parent))) {
			break;
		}
		parent = parent->parentWidget();
	}
	if (!m_data->dragWidget)
		return;

	m_data->area = new VipDragRubberBand(vipGetMainWindow());
	if(m_data->player->widgetForMouseEvents())
		m_data->player->widgetForMouseEvents()->installEventFilter(this);

	m_data->close = new QToolButton(m_data->player);
	m_data->close->setAutoRaise(true);
	m_data->close->setToolTip("Close widget");
	m_data->close->setIcon(vipIcon("close.png"));
	m_data->close->resize(20, 20);
	connect(m_data->close, SIGNAL(clicked(bool)), this, SLOT(closePlayer()));

	m_data->maximize = new QToolButton(m_data->player);
	m_data->maximize->setAutoRaise(true);
	m_data->maximize->setToolTip("Maximize/restore widget");
	m_data->maximize->setIcon(vipIcon("restore.png"));
	m_data->maximize->resize(20, 20);
	connect(m_data->maximize, SIGNAL(clicked(bool)), this, SLOT(maximizePlayer()));

	m_data->minimize = new QToolButton(m_data->player);
	m_data->minimize->setAutoRaise(true);
	m_data->minimize->setToolTip("Maximize/restore widget");
	m_data->minimize->setIcon(vipIcon("minimize.png"));
	m_data->minimize->resize(20, 20);
	connect(m_data->minimize, SIGNAL(clicked(bool)), this, SLOT(minimizePlayer()));

	//connect(m_data->player->plotWidget2D()->area(), SIGNAL(visualizedAreaChanged()), this, SLOT(reorganizeCloseButton()));
	reorganizeCloseButton();


	create_player_top_toolbar(player, this);
}
CustomWidgetPlayer::~CustomWidgetPlayer()
{
	if (m_data->area)
		m_data->area->deleteLater();
	delete m_data;
}

VipDragWidget* CustomWidgetPlayer::dragWidget() const
{
	return m_data->dragWidget;
}

void CustomWidgetPlayer::reorganizeCloseButton()
{
	if (!m_data->player || !m_data->dragWidget || m_data->dragWidget->isDestroying())
		return;

	QWidget* w = m_data->player->widget();

	m_data->close->move(w->width() - m_data->close->width() , 0);
	m_data->maximize->move(w->width() - m_data->close->width() - m_data->maximize->width() , 0);
	m_data->minimize->move(w->width() - m_data->close->width() - m_data->maximize->width() - m_data->minimize->width() , 0);

	QPoint pt = w->mapFromGlobal(QCursor::pos());
	//update visibility
	QRect r = w->rect();
	if (r.contains(pt)) {
		m_data->close->setVisible(true);
		m_data->maximize->setVisible(true);
		m_data->minimize->setVisible(true);
	}
	else {
		m_data->close->setVisible(false);
		m_data->maximize->setVisible(false);
		m_data->minimize->setVisible(false);
	}
}

Anchor CustomWidgetPlayer::anchor(const QPoint& viewport_pos, const QMimeData* mime)
{
	QPoint pt = viewport_pos;
	bool is_drag_widget = mime && !mime->data("application/dragwidget").isEmpty();
	
	//find area to highlight
	Anchor res;
	//Viewport rect
	QRect viewport_rect = m_data->player->geometry();
	QRect canvas_rect = viewport_rect.adjusted(20, 20, -20, -20);

	int w_l = canvas_rect.left() - viewport_rect.left();
	int w_r = viewport_rect.right() - canvas_rect.right();
	int h_t = canvas_rect.top() - viewport_rect.top();
	int h_b = viewport_rect.bottom() - canvas_rect.bottom();

	if (w_l < MIN_BORDER_DIST) w_l = MIN_BORDER_DIST;
	if (w_r < MIN_BORDER_DIST) w_r = MIN_BORDER_DIST;
	if (h_t < MIN_BORDER_DIST) h_t = MIN_BORDER_DIST;
	if (h_b < MIN_BORDER_DIST) h_b = MIN_BORDER_DIST;

	if (viewport_rect.width() < w_l * 2) w_l = viewport_rect.width() / 2;
	if (viewport_rect.width() < w_r * 2) w_r = viewport_rect.width() / 2;
	if (viewport_rect.height() < h_b * 2) h_b = viewport_rect.height() / 2;
	if (viewport_rect.height() < h_t * 2) h_t = viewport_rect.height() / 2;

	//test borders
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


bool CustomWidgetPlayer::eventFilter(QObject* , QEvent* evt)
{
	if (!m_data->player || !m_data->dragWidget || m_data->dragWidget->isDestroying())
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

	if (evt->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton) {

			//check potential key modifiers
			int key_modifiers = m_data->player->property("_vip_moveKeyModifiers").toInt();
			if (!key_modifiers)
				key_modifiers = (int)Qt::AltModifier;
			if (key_modifiers && !(key_modifiers & event->modifiers()))
				return false;
			//yes, only canvas or grids
			m_data->mousePress = event->pos();
			return false;//true;
		}
	}
	else if (evt->type() == QEvent::MouseButtonRelease)
	{
		//disable further dragging
		if (m_data->mousePress != QPoint(-1, -1)) {
			//bool same_pos = (m_data->mousePress - QPointF(static_cast<QMouseEvent*>(evt)->pos())).manhattanLength() < 10;
			m_data->mousePress = QPoint(-1, -1);
			return false;
		}
	}
	else if (evt->type() == QEvent::MouseMove)
	{
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);
		if (m_data->mousePress != QPoint(-1, -1) ) {
			//drag
			if ((event->pos() - m_data->mousePress).manhattanLength() > 10) {
				VipBaseDragWidget* w = VipDragWidget::fromChild(m_data->player);
				QPoint pt = m_data->mousePress;
				m_data->mousePress = QPoint(-1, -1);
				return  w->dragThisWidget(m_data->player, pt);
			}

		}
	}


	/**
	Now we want to manage drop events
	*/

	if (evt->type() == QEvent::DragEnter)
	{
		QDragEnterEvent* event = static_cast<QDragEnterEvent*>(evt);

		m_data->anchor = anchor(event->pos(), event->mimeData());

		if (m_data->anchor.side != Vip::NoSide && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->acceptProposedAction();
			return true;
		}

		//Do not accept drag widgets inside
		if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->setAccepted(false);
			return false;
		}
		//Do not accept VipPlotMimeData objects
		if (qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
			event->setAccepted(false);
			return false;
		}

	}
	else if (evt->type() == QEvent::DragMove)
	{
		QDragMoveEvent* event = static_cast<QDragMoveEvent*>(evt);

		m_data->anchor = anchor(event->pos(), event->mimeData());
		if (m_data->anchor.side != Vip::NoSide) {
			//higlight area
			anchorToArea(m_data->anchor, *m_data->area, m_data->player);
			m_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else if (m_data->anchor.side == Vip::AllSides &&  !event->mimeData()->data("application/dragwidget").isEmpty()) {
			//higlight area
			anchorToArea(m_data->anchor, *m_data->area, m_data->player);
			m_data->area->show();
			event->setAccepted(true);
			return true;
		}
		else {
			event->setAccepted(false);
			m_data->area->hide();
		}
		return false;
	}
	else if (evt->type() == QEvent::DragLeave)
	{
		//if(!m_data->area->geometry().contains(QCursor::pos()) || !m_data->area->isVisible())
		m_data->area->hide();
	}
	else if (evt->type() == QEvent::Drop)
	{
		m_data->area->hide();
		QDropEvent* event = static_cast<QDropEvent*>(evt);

		if (m_data->anchor.side == Vip::AllSides)
		{
			if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
				//TODO: player swapping

				VipBaseDragWidget* base = (VipBaseDragWidget*)(event->mimeData()->data("application/dragwidget").toULongLong());
				if (VipDragWidget* d = qobject_cast<VipDragWidget*>(base)) {
					d->parentMultiDragWidget()->swapWidgets(d, m_data->dragWidget);
				}
				event->setDropAction(Qt::IgnoreAction);
				return true;
			}
			else {
				/*m_data->anchor.canvas->dropMimeData(event->mimeData());
				event->acceptProposedAction();
				return true;*/
				return false;
			}
			//return false;
		}
		else if (m_data->anchor.side != Vip::NoSide )
		{
			if (VipMultiDragWidget* mw = m_data->dragWidget->parentMultiDragWidget()) {
				QPoint pos = mw->indexOf(m_data->dragWidget);
				VipDragWidgetHandle* h = nullptr;

				if (mw->orientation() == Qt::Vertical) {

					if (m_data->anchor.side == Vip::Right)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (m_data->anchor.side == Vip::Left)
						h = mw->subSplitterHandle(pos.y(), pos.x());
					else if (m_data->anchor.side == Vip::Bottom)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (m_data->anchor.side == Vip::Top)
						h = mw->mainSplitterHandle(pos.y());
				}
				else {
					if (m_data->anchor.side == Vip::Right)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (m_data->anchor.side == Vip::Left)
						h = mw->mainSplitterHandle(pos.y());
					else if (m_data->anchor.side == Vip::Bottom)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (m_data->anchor.side == Vip::Top)
						h = mw->subSplitterHandle(pos.y(), pos.x());
				}
				

				bool res = h->dropMimeData(event->mimeData());
				if (!res)
					event->setDropAction(Qt::IgnoreAction);
				else
					event->acceptProposedAction();

				//VipDragWidgetHandler::find(m_data->dragWidget->topLevelMultiDragWidget()->parentWidget())->reorganizeMinimizedChildren();
				QMetaObject::invokeMethod(m_data->dragWidget->topLevelMultiDragWidget(), "reorganizeMinimizedChildren");
				return true;
			}

		}
		
		//ignore drag widget to be sure that the widget will be moved
		if (!event->mimeData()->data("application/dragwidget").isEmpty())
			event->setDropAction(Qt::IgnoreAction);
	}

	return false;
}







class CustomizePlotPlayer::PrivateData
{
public:
	VipPlotPlayer * player;
	VipDragWidget * dragWidget;
	QList<QWidget*> topWidgets;
	QToolBar * toolBar;
	QToolButton * title;
	QRect topWidgetsRect;
	QPoint mousePress;
	QPointer<VipDragRubberBand> area;
	Anchor anchor;

	PrivateData()
		:player(nullptr), dragWidget(nullptr), mousePress(-1, -1)
	{}
};

CustomizePlotPlayer::CustomizePlotPlayer(VipPlotPlayer * player)
	:BaseCustomPlayer2D(player)
{
	m_data = new PrivateData();
	m_data->player = player;
	m_data->dragWidget = nullptr;

	QWidget * parent = player->parentWidget();
	VipMultiDragWidget * top = nullptr;
	while (parent) {
		if (!m_data->dragWidget) m_data->dragWidget = qobject_cast<VipDragWidget*>(parent);
		if ((top = qobject_cast<VipMultiDragWidget*>(parent))) {
			break;
		}
		parent = parent->parentWidget();
	}
	if (!m_data->dragWidget)
		return;

	m_data->player->plotWidget2D()->viewport()->installEventFilter(this);

	m_data->area = new VipDragRubberBand(vipGetMainWindow());
	m_data->area->setAttribute(Qt::WA_TransparentForMouseEvents);
	m_data->area->setEnabled(false);
	m_data->area->setFocusPolicy(Qt::NoFocus);

	//remove plot area margins if any
	m_data->player->plotWidget2D()->area()->setMargins(VipMargins());





	//title management
	VipText t = m_data->player->plotWidget2D()->area()->title();
	QFont f = t.font();
	//f.setBold(true);
	t.setFont(f);
	//t.setLayoutAttribute(VipText::MinimumLayout);
	//static_cast<VipPlotWidget2D*>(m_data->player->plotWidget2D())->area()->topAxis()->setTitle(t);
	m_data->player->plotWidget2D()->area()->setTitle(t);
	updateTitle();
	connect(player, SIGNAL(windowTitleChanged(const QString &)), this, SLOT(updateTitle()));

	//rendering management
	//connect(player, SIGNAL(renderStarted(const VipRenderState&)), this, SLOT(startRender()));
	//connect(player, SIGNAL(renderEnded(const VipRenderState&)), this, SLOT(endRender()));

	//legend management
	m_data->player->plotWidget2D()->area()->legend()->layout()->setContentsMargins(20, 0, 20, 0);
	m_data->player->plotWidget2D()->area()->legend()->layout()->setMargins(0);
	m_data->player->plotWidget2D()->area()->legend()->layout()->setSpacing(0);
	m_data->player->plotWidget2D()->area()->legend()->setDrawCheckbox(false);

	for (int i = 0; i < m_data->player->plotWidget2D()->area()->innerLegendCount(); ++i) {
		if (VipLegend * l = m_data->player->plotWidget2D()->area()->innerLegend(i)) {
			l->layout()->setContentsMargins(5, 5, 5, 5);
			l->layout()->setMargins(0);
			l->layout()->setSpacing(0);
			l->setDrawCheckbox(false);
		}
	}

	if (VipVMultiPlotArea2D * area = qobject_cast<VipVMultiPlotArea2D*>(m_data->player->plotWidget2D()->area())) {
		connect(area, SIGNAL(canvasAdded(VipPlotCanvas*)), this, SLOT(reorganizeCloseButtons()));
		connect(area, SIGNAL(canvasRemoved(VipPlotCanvas*)), this, SLOT(reorganizeCloseButtons()));
	}


	//moveTitleBarActionsToToolBar(qobject_cast<VipDragWidgetHeader*>(m_data->dragWidget->Header()), player->toolBar(), this);
	create_player_top_toolbar(player, this);
}
CustomizePlotPlayer::~CustomizePlotPlayer()
{
	if (m_data->area)
		m_data->area->deleteLater();
	delete m_data;
}

VipDragWidget * CustomizePlotPlayer::dragWidget() const
{
	return m_data->dragWidget;
}

void CustomizePlotPlayer::updateViewport(QWidget* viewport)
{
	viewport->installEventFilter(this);
}

Anchor CustomizePlotPlayer::anchor(const QPoint & viewport_pos, const QMimeData * mime)
{
	QPointF scene_pt = scenePos(viewport_pos); //mouse in scene coordinates
	QPoint pt = m_data->player->plotWidget2D()->viewport()->mapTo(m_data->player->plotWidget2D(), viewport_pos); //mouse in view coordinates
	bool is_drag_widget = mime && !mime->data("application/dragwidget").isEmpty();
	/*
	Highlight areas
	We define several areas:
	- Inside a canvas (add curve on existing canvas)
	- Inside a canvas but on a border (add curve on a new stacked panel)
	- Outside canvas (add plot in a new player)
	*/


	//Buils scene path
	QRectF scene = m_data->player->plotWidget2D()->sceneRect();
	QPainterPath scene_path;
	scene_path.addRect(scene);

	//Build canvas paths and remove them from scene path
	QList<VipPlotCanvas*> all_canvas = m_data->player->plotWidget2D()->area()->findItems<VipPlotCanvas*>();
	QMap<VipPlotCanvas*, QPainterPath> canvas_path;
	for (int i = 0; i < all_canvas.size(); ++i) {
		QPainterPath p = all_canvas[i]->mapToScene(all_canvas[i]->shape());
		canvas_path[all_canvas[i]] = p;
		scene_path = scene_path.subtracted(p);
	}

	//find area to highlight
	Anchor res;

	//test cansvas
	for (QMap<VipPlotCanvas*, QPainterPath>::const_iterator it = canvas_path.begin(); it != canvas_path.end(); ++it) {
		if (it.value().contains(scene_pt)) {
			//Mouse inside canvas
			QRect canvas_rect = m_data->player->plotWidget2D()->mapFromScene(it.value().boundingRect()).boundingRect();

			int w = MIN_BORDER_DIST;
			if (canvas_rect.width() < w * 2) w = canvas_rect.width() / 2;
			int h = MIN_BORDER_DIST;
			if (canvas_rect.height() < h * 2) h = canvas_rect.height() / 2;

			//test borders
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
			else*/ if (pt.y() < canvas_rect.top() + h) {
				if (is_drag_widget)
					return res; //cannot drop drag widget for stacked plot
				res.side = Vip::Top;
				res.canvas = it.key();
				res.highlight = QRect(canvas_rect.left(), canvas_rect.top(), canvas_rect.width(), h);
				res.text = "Stacked plot on the top";
				return res;
			}
			else if (pt.y() > canvas_rect.bottom() - h) {
				if (is_drag_widget)
					return res; //cannot drop drag widget for stacked plot
				res.side = Vip::Bottom;
				res.canvas = it.key();
				res.highlight = QRect(canvas_rect.left(), canvas_rect.bottom() - h, canvas_rect.width(), h);
				res.text = "Stacked plot on the bottom";
				return res;
			}
			else {
				//inside canvas
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

	//Viewport rect
	QRect viewport_rect = m_data->player->plotWidget2D()->viewport()->geometry();

	int w = MIN_BORDER_DIST * 2;
	if (viewport_rect.width() < w * 2) w = viewport_rect.width() / 2;
	int h = MIN_BORDER_DIST * 2;
	if (viewport_rect.height() < h * 2) h = viewport_rect.height() / 2;

	//test borders
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
		//higlight area
		return res;
	}
	return Anchor();
}


#include <qrubberband.h>
bool CustomizePlotPlayer::eventFilter(QObject * w, QEvent * evt)
{
	if (!m_data->player || !m_data->dragWidget || m_data->dragWidget->isDestroying())
		return false;

	if (evt->type() == QEvent::Destroy)
		return false;

	if (!qobject_cast<VipPlotPlayer*>(m_data->player) || w != m_data->player->plotWidget2D()->viewport())
		return false;

	if (evt->type() == QEvent::Resize)
		reorganizeCloseButtons();
	else if (evt->type() == QEvent::Leave)
		reorganizeCloseButtons();

	/**
	handle double click to edit title
	*/
	if (evt->type() == QEvent::MouseButtonDblClick)
	{
		QMouseEvent * event = static_cast<QMouseEvent*>(evt);
		//convert to scene pos
		QPointF pt = m_data->player->plotWidget2D()->mapToScene(event->pos());
		QRectF b = m_data->player->plotWidget2D()->area()->titleAxis()->boundingRect();
		if (b.contains(pt)) {
			editTitle();
			return true;
		}
		return false;
	}

	/*
	We want to move a VipPlotPlayer through the internal canvas
	*/

	if (evt->type() == QEvent::MouseButtonPress)
	{
		//ignore the mouse event if there is a filter installed (like drawing ROI), let the filter do its job
		if (m_data->player->plotWidget2D()->area()->filter())
			return false;

		QMouseEvent * event = static_cast<QMouseEvent*>(evt);

		if (event->buttons() & Qt::LeftButton) {

			//Get plot items behind the mouse,
			//check if there are only canvas or grids
			QPointF scene_pt = scenePos(event->pos());
			bool ok = true;
			if (QGraphicsItem * item = firstVisibleItem(scene_pt)) {
				if (!item->toGraphicsObject()) ok = false;
				else {
					QGraphicsObject * obj = item->toGraphicsObject();
					if (!qobject_cast<VipPlotCanvas*>(obj)) ok = false;
				}
			}
			if (ok) {
				//yes, only canvas or grids
				m_data->mousePress = event->pos();
				return false;
			}
		}
	}
	else if (evt->type() == QEvent::MouseButtonRelease)
	{
		//disable further dragging
		if (m_data->mousePress != QPoint(-1, -1)) {
			m_data->mousePress = QPoint(-1, -1);
			return false;
		}
	}
	else if (evt->type() == QEvent::MouseMove)
	{
		reorganizeCloseButtons();
		QMouseEvent* event = static_cast<QMouseEvent*>(evt);
		if (m_data->mousePress != QPoint(-1, -1) && !m_data->player->plotWidget2D()->area()->mouseInUse()) {
			//drag
			if (event->buttons() & Qt::LeftButton) {
				if ((event->pos() - m_data->mousePress).manhattanLength() > 10) {
					VipBaseDragWidget* _w = VipDragWidget::fromChild(m_data->player);
					QPoint pt = m_data->mousePress;
					m_data->mousePress = QPoint(-1, -1);
					return _w->dragThisWidget(m_data->player->plotWidget2D()->viewport(), pt);
				}
			}

		}
	}


	/**
	Now we want to manage drop events
	*/

	if (evt->type() == QEvent::DragEnter)
	{
		QDragEnterEvent * event = static_cast<QDragEnterEvent*>(evt);

		m_data->anchor = anchor(event->pos(), event->mimeData());

		if (m_data->anchor.side != Vip::NoSide && !m_data->anchor.canvas && !event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->acceptProposedAction();
			return true;
		}

		//Do not accept drag widgets inside
		if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
			event->setAccepted(false);
			return false;
		}
		//Accept VipPlotMimeData objects
		if (qobject_cast<const VipPlotMimeData*>(event->mimeData())) {
			event->accept();
		}

	}
	else if (evt->type() == QEvent::DragMove)
	{
		QDragMoveEvent * event = static_cast<QDragMoveEvent*>(evt);

		m_data->anchor = anchor(event->pos(), event->mimeData());
		if (m_data->anchor.side != Vip::NoSide) {
			event->acceptProposedAction();
			//higlight area
			anchorToArea(m_data->anchor, *m_data->area, m_data->player->plotWidget2D());
			m_data->area->show();
			return true;
		}
		else {
			event->setAccepted(false);
			m_data->area->hide();
		}
		return false;
	}
	else if (evt->type() == QEvent::DragLeave)
	{
		//if(!m_data->area->geometry().contains(QCursor::pos()) || !m_data->area->isVisible())
		m_data->area->hide();
	}
	else if (evt->type() == QEvent::Drop)
	{
		m_data->area->hide();
		QDropEvent * event = static_cast<QDropEvent*>(evt);

		if (m_data->anchor.side != Vip::NoSide && !m_data->anchor.canvas)
		{
			if (VipMultiDragWidget * mw = m_data->dragWidget->parentMultiDragWidget()) {
				QPoint pos = mw->indexOf(m_data->dragWidget);
				VipDragWidgetHandle * h = nullptr;
				if (mw->orientation() == Qt::Vertical) {

					if (m_data->anchor.side == Vip::Right)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (m_data->anchor.side == Vip::Left)
						h = mw->subSplitterHandle(pos.y(), pos.x());
					else if (m_data->anchor.side == Vip::Bottom)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (m_data->anchor.side == Vip::Top)
						h = mw->mainSplitterHandle(pos.y());
				}
				else {
					if (m_data->anchor.side == Vip::Right)
						h = mw->mainSplitterHandle(pos.y() + 1);
					else if (m_data->anchor.side == Vip::Left)
						h = mw->mainSplitterHandle(pos.y());
					else if (m_data->anchor.side == Vip::Bottom)
						h = mw->subSplitterHandle(pos.y(), pos.x() + 1);
					else if (m_data->anchor.side == Vip::Top)
						h = mw->subSplitterHandle(pos.y(), pos.x());
				}

				bool res = h->dropMimeData(event->mimeData());
				if (!res)
					event->setDropAction(Qt::IgnoreAction);
				else
					event->acceptProposedAction();

				//VipDragWidgetHandler::find(m_data->dragWidget->topLevelMultiDragWidget()->parentWidget())->reorganizeMinimizedChildren();
				QMetaObject::invokeMethod(m_data->dragWidget->topLevelMultiDragWidget(), "reorganizeMinimizedChildren");

				return true;
			}

		}
		else if (m_data->anchor.side == Vip::AllSides  && m_data->anchor.canvas)
		{
			if (!event->mimeData()->data("application/dragwidget").isEmpty()) {
				//TODO: player swapping

				VipBaseDragWidget * base = (VipBaseDragWidget*)(event->mimeData()->data("application/dragwidget").toULongLong());
				if (VipDragWidget * d = qobject_cast<VipDragWidget*>(base)) {
					d->parentMultiDragWidget()->swapWidgets(d, m_data->dragWidget);
				}
				event->setDropAction(Qt::IgnoreAction);
				return false;
			}
			else {
				m_data->anchor.canvas->dropMimeData(event->mimeData());
				event->acceptProposedAction();
				return true;
			}
		}
		else if (m_data->anchor.canvas)
		{
			if (VipVMultiPlotArea2D * a = qobject_cast<VipVMultiPlotArea2D*>(m_data->player->plotWidget2D()->area())) {
				//find canvas index
				VipAbstractScale * left = m_data->anchor.canvas->axes()[1];
				int index = a->leftMultiAxis()->indexOf(static_cast<VipBorderItem*>(left));
				//find insertion index
				if (m_data->anchor.side == Vip::Top)
					++index;

				left = m_data->player->insertLeftScale(index);

				//find canvas
				VipPlotCanvas * canvas = vipListCast<VipPlotCanvas*>(left->plotItems()).first();
				canvas->dropMimeData(event->mimeData());
			}
		}

		//ignore drag widget to be sure that the widget will be moved
		if (!event->mimeData()->data("application/dragwidget").isEmpty())
			event->setDropAction(Qt::IgnoreAction);
	}

	return false;
}

void CustomizePlotPlayer::finishEditingTitle()
{
	//m_data->player->setAutomaticWindowTitle(false);
	//m_data->player->setWindowTitle(_title_editor->text());
}
void CustomizePlotPlayer::titleChanged()
{
	//m_data->title->setText(QString::number(VipUniqueId::id<VipBaseDragWidget>(m_data->dragWidget)));
}

void CustomizePlotPlayer::reorganizeCloseButtons()
{
	if (!m_data->player || !m_data->dragWidget || m_data->dragWidget->isDestroying())
		return;

	//Get mouse pos 
	QPoint pt = m_data->player->plotWidget2D()->mapFromGlobal(QCursor::pos());
	QList<VipPlotCanvas*> canvas;

	if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(m_data->player->plotWidget2D()->area())) {
		canvas = area->allCanvas();
	}
	else
		canvas << m_data->player->plotWidget2D()->area()->canvas();
		
		
		//QList<VipPlotCanvas*> canvas = area->allCanvas();
		for (int i = 0; i < canvas.size(); ++i) {
			QToolButton * close = canvas[i]->property("_vip_close").value<QToolButton*>();
			if (!close) {
				close = new QToolButton(m_data->player);
				close->setAutoRaise(true);
				close->setMaximumSize(QSize(20, 20));
				close->setAutoFillBackground(true);
				close->setIcon(vipIcon("close.png"));
				close->setProperty("_vip_canvas", QVariant::fromValue(canvas[i]));
				canvas[i]->setProperty("_vip_close", QVariant::fromValue(close));
				connect(close, SIGNAL(clicked(bool)), this, SLOT(closeCanvas()));
				connect(canvas[i], SIGNAL(destroyed(VipPlotItem*)), close, SLOT(deleteLater()));
			}
			close->setToolTip(canvas.size() == 1 ? "Close window" : "Close this stacked plot area");

			QToolButton * maximize = canvas[i]->property("_vip_maximize").value<QToolButton*>();
			if (!maximize) {
				maximize = new QToolButton(m_data->player);
				maximize->setAutoRaise(true);
				maximize->setMaximumSize(QSize(20, 20));
				maximize->setAutoFillBackground(true);
				maximize->setIcon(vipIcon("restore.png"));
				maximize->setProperty("_vip_canvas", QVariant::fromValue(canvas[i]));
				maximize->setToolTip("Maximize/restore window");
				canvas[i]->setProperty("_vip_maximize", QVariant::fromValue(maximize));
				connect(maximize, SIGNAL(clicked(bool)), this, SLOT(maximizePlayer()));
			}

			QToolButton * minimize = canvas[i]->property("_vip_minimize").value<QToolButton*>();
			if (!minimize) {
				minimize = new QToolButton(m_data->player);
				minimize->setAutoRaise(true);
				minimize->setMaximumSize(QSize(20, 20));
				minimize->setAutoFillBackground(true);
				minimize->setIcon(vipIcon("minimize.png"));
				minimize->setProperty("_vip_canvas", QVariant::fromValue(canvas[i]));
				minimize->setToolTip("Minimize window");
				canvas[i]->setProperty("_vip_minimize", QVariant::fromValue(minimize));
				connect(minimize, SIGNAL(clicked(bool)), this, SLOT(minimizePlayer()));
			}
			
			//update visibility
			QRectF r;
			if (VipVMultiPlotArea2D* area = qobject_cast<VipVMultiPlotArea2D*>(m_data->player->plotWidget2D()->area())) {
				r = area->plotArea((VipBorderItem*)canvas[i]->axes()[1]).boundingRect();
				r = m_data->player->plotWidget2D()->mapFromScene(area->mapToScene(r)).boundingRect();
			}
			else {
				r = m_data->player->plotWidget2D()->mapFromScene(m_data->player->plotWidget2D()->sceneRect()).boundingRect();
			}
			if (r.contains(pt)) {
				close->setVisible(true);
				maximize->setVisible(true);
				minimize->setVisible(true);
			}
			else {
				close->setVisible(false);
				maximize->setVisible(false);
				minimize->setVisible(false);
			}

			QRect crect = m_data->player->plotWidget2D()->mapFromScene(canvas[i]->mapToScene(canvas[i]->boundingRect())).boundingRect();
			close->move(crect.right() - close->width(), crect.top());
			maximize->move(crect.right() - maximize->width() - close->width(), crect.top());
			minimize->move(crect.right() - maximize->width() - close->width() - minimize->width(), crect.top());
		}
}

void CustomizePlotPlayer::closeCanvas()
{
	if (VipVMultiPlotArea2D * area = qobject_cast<VipVMultiPlotArea2D*>(m_data->player->plotWidget2D()->area())) {
		if (QToolButton * tool = qobject_cast<QToolButton*>(sender())) {
			if (VipPlotCanvas * c = tool->property("_vip_canvas").value<VipPlotCanvas*>()) {
				if (area->allCanvas().size() == 1)
					closePlayer();
				else
					m_data->player->removeLeftScale(c->axes()[1]);
			}
		}
	}
	else {
		if (QToolButton* tool = qobject_cast<QToolButton*>(sender())) {
			if (VipPlotCanvas* c = tool->property("_vip_canvas").value<VipPlotCanvas*>()) {
				closePlayer();
			}
		}
	}
}






static void updatePlotPlayer(VipPlotPlayer* pl)
{
	if (!VipBaseDragWidget::fromChild(pl))
		return;
	QObject * obj = pl->findChild<BaseCustomPlayer2D*>(QString(), Qt::FindDirectChildrenOnly);
	if (!obj)
		new CustomizePlotPlayer(pl);
}
static void updateVideoPlayer(VipVideoPlayer* pl)
{
	if (!VipBaseDragWidget::fromChild(pl))
		return;
	QObject * obj = pl->findChild<BaseCustomPlayer2D*>(QString(), Qt::FindDirectChildrenOnly);
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







static void resizeSplitter(QSplitter * splitter)
{
	QList<int> sizes;
	for (int i = 0; i < splitter->count(); ++i)
		sizes.append(1);
	splitter->setSizes(sizes);
	splitter->setOpaqueResize(true);
	// TEST: comment
	//splitter->setProperty("_vip_dirtySplitter", 0);
}


static QAction * createSeparator()
{
	QAction * sep = new QAction(nullptr);
	sep->setSeparator(true);
	return sep;
}

static QList<QAction*> additionalActions(VipPlotItem* /*item*/, VipPlayer2D* player)
{
	QList<QAction*> res;

	if (QObject * obj = player->findChild<BaseCustomPlayer2D*>(QString(), Qt::FindDirectChildrenOnly)) {
		QAction * title = new QAction(nullptr);
		title->setText("Edit title");
		res << title << createSeparator();
		QObject::connect(title, SIGNAL(triggered(bool)), obj, SLOT(editTitle()));
	}

	//get parent drag widget and multi drag widget
	if (VipDragWidget *dw = qobject_cast<VipDragWidget*>(VipBaseDragWidget::fromChild(player)))
		if (VipMultiDragWidget * mw = dw->parentMultiDragWidget()) {
			int count = 0;
			QPoint pt = mw->indexOf(dw);
			if (pt != QPoint(-1, -1)) {
				int s = mw->subCount(pt.y());
				if (s > 1) {
					QAction * actc = new QAction(nullptr);
					actc->setText("Resize columns");
					QObject::connect(actc, &QAction::triggered, actc, std::bind(resizeSplitter, mw->subSplitter(pt.y())));
					res << actc;
					++count;
				}
			}

			if (mw->mainCount() > 1) {
				QAction * actr = new QAction(nullptr);
				actr->setText("Resize rows");
				QObject::connect(actr, &QAction::triggered, actr, std::bind(resizeSplitter, mw->mainSplitter()));
				res << actr;
				++count;
			}

			if (count)
				res << createSeparator();

			QAction * savei = new QAction("Save as image...", nullptr);
			QAction * saves = new QAction("Save as session...", nullptr);
			res << savei << saves;
			QObject::connect(savei, &QAction::triggered, std::bind(vipSaveImage, dw)); // dw, SLOT(saveImage()));
			QObject::connect(saves, &QAction::triggered, std::bind(vipSaveSession, dw)); // dw, SLOT(saveSession()));

			if (mw->count() > 1) {
				res << createSeparator();
				if (!dw->isMaximized()) {
					QAction * maximize = new QAction("Maximize window", nullptr);
					QObject::connect(maximize, SIGNAL(triggered(bool)), dw, SLOT(showMaximized()));
					res << maximize;
				}
				if (dw->isMaximized()) {
					QAction * restore = new QAction("Restore window", nullptr);
					QObject::connect(restore, SIGNAL(triggered(bool)), dw, SLOT(showNormal()));
					res << restore;
				}

				QAction * close = new QAction("Close window", nullptr);
				QObject::connect(close, &QAction::triggered, dw, std::bind(restoreAndClose, player));
				res << close;
			}
		}
	return res;
}






#include <qboxlayout.h>
struct CustomizePlayer : public QObject
{
	void customize(VipAbstractPlayer * p) {
		//hide tool bars and status bar
		if (VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(p)) {
			QList<QToolBar*> ws = pl->toolBars();
			for (int i = 0; i < ws.size(); ++i)
				ws[i]->hide();

			
			//apply mouse selection to all
			if (qobject_cast<VipPlotPlayer*>(p))
				QObject::connect(pl, &VipPlayer2D::mouseSelectionChanged, this, &CustomizePlayer::mousePlotSelectionChanged);
			else if (/*VipVideoPlayer *vp = */qobject_cast<VipVideoPlayer*>(p)) {
				QObject::connect(pl, &VipPlayer2D::mouseSelectionChanged, this, &CustomizePlayer::mouseVideoSelectionChanged);
			}
		}
	}
	void destroyed(VipAbstractPlayer * p)
	{
		// destroy the tool bars
		if (VipPlayer2D * player = qobject_cast<VipPlayer2D*>(p)) {
			QWidget * tmp = player->property("_vip_topToolBar").value<WidgetPointer>();
			if (tmp) {
				player->setProperty("_vip_topToolBar", QVariant());
				if (tmp)
					tmp->deleteLater();
			}
		}
	}

	void focusWidgetChanged(VipDragWidget * w)
	{
		static QPointer<VipDragWidget> prev_focus;

		if (prev_focus) {
			prev_focus->setStyleSheet("VipDragWidget{ border: none;}");
		}
		prev_focus = w;

		if (!w) {
			//empty workspace, hide all tool bars
			QWidget * top = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->topWidget();
			QList<QWidget *> toolbars = top->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
			for (int i = 0; i < toolbars.size(); ++i)
				toolbars[i]->hide();
			return;
		}

		//make sure to create the tool bars before
		if (VipVideoPlayer * video = qobject_cast<VipVideoPlayer*>(w->widget()))
			updateVideoPlayer(video);
		else if (VipPlotPlayer * plot = qobject_cast<VipPlotPlayer*>(w->widget()))
			updatePlotPlayer(plot);
		else if (VipWidgetPlayer* widget = qobject_cast<VipWidgetPlayer*>(w->widget()))
			updateWidgetPlayer(widget);

		//hide all tool bars
		QWidget * top = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->topWidget();
		QList<QWidget *> toolbars = top->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
		for (int i = 0; i < toolbars.size(); ++i)
			toolbars[i]->hide();

		if (VipAbstractPlayer * p = qobject_cast<VipAbstractPlayer*>(w->widget())) {
			QWidget * toolbar = p->property("_vip_topToolBar").value<WidgetPointer>();
			if (toolbar) {
				QLayout * lay = top->layout();
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


static CustomizePlayer * customizePlayer()
{
	static CustomizePlayer * inst = new CustomizePlayer();
	return inst;
}

static void customizeMultiDragWidget(VipMultiDragWidget * w)
{
	w->setSupportedOperation(VipBaseDragWidget::DragWidgetExtract, false);
	w->setSupportedOperation(VipBaseDragWidget::Minimize, false);
	w->setMaximumHandleWidth(5);
	QColor c = VipGuiDisplayParamaters::instance()->defaultPlayerBackgroundColor();
	w->setStyleSheet(mutliDragWidgetStyleSheet(c));

	// TEST: comment
	//QObject::connect(w, &VipMultiDragWidget::splitterMoved, customizePlayer(), &CustomizePlayer::receivedSplitterMoved);
}



//
// Registered functions for python wrapper
//


static QVariant setCurrentWorkspaceMaxColumns(const QVariantList& lst)
{
	if (lst.size() != 1 || !lst.first().canConvert<int>()) {
		return QVariant::fromValue(VipErrorData("setCurrentWorkspaceMaxColumns: wrong input argument (should be an integer value)"));
	}
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
	{
		int value = lst.first().toInt();
		if(value <=0)
			return QVariant::fromValue(VipErrorData("setCurrentWorkspaceMaxColumns: wrong input value (" + QString::number(value) +")"));
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

static QVariant currentWorkspaceMaxColumns(const QVariantList& )
{
	if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
	{
		return QVariant::fromValue(area->property("_vip_customNumCols").toInt());
	}
	else
		return QVariant::fromValue(VipErrorData("currentWorkspaceMaxColumns: no valid workspace available"));
}

static QVariant reorganizeCurrentWorkspace(const QVariantList& )
{
	VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	if(!area)
		return QVariant::fromValue(VipErrorData("reorganizeCurrentWorkspace: no valid workspace available"));
	
	int max_cols = area->property("_vip_customNumCols").toInt();
	if (max_cols <= 0) 
		return QVariant::fromValue(VipErrorData("reorganizeCurrentWorkspace: wrong maximum columns (" + QString::number(max_cols) + ")"));
	
	//build the list of all available players
	QList<VipDragWidget*> players;
	VipMultiDragWidget* main = area->mainDragWidget(QList<QWidget*>(), false);
	if(!main)
		return  QVariant::fromValue(VipErrorData("reorganizeCurrentWorkspace: no valid workspace available"));

	for(int y=0; y < main->mainCount(); ++y)
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

	//focus widget changed: update the common tool bar
	QObject::connect(vipGetMainWindow()->displayArea(), &VipDisplayArea::focusWidgetChanged, customizePlayer(), &CustomizePlayer::focusWidgetChanged);

	//customize players
	//make sure to hide the tool bars BEFORE showing the plot player
	QObject::connect(VipPlayerLifeTime::instance(), &VipPlayerLifeTime::created, customizePlayer(), &CustomizePlayer::customize);
	QObject::connect(VipPlayerLifeTime::instance(), &VipPlayerLifeTime::destroyed, customizePlayer(), &CustomizePlayer::destroyed);
	vipFDPlayerCreated().append<void(VipPlotPlayer*)>(updatePlotPlayer);
	vipFDPlayerCreated().append<void(VipVideoPlayer*)>(updateVideoPlayer);
	vipFDPlayerCreated().append<void(VipWidgetPlayer*)>(updateWidgetPlayer);

	//additional actions on right click
	VipFDItemRightClick().append<QList<QAction*>(VipPlotItem*, VipPlayer2D*)>(additionalActions);

	//disable the creation of stacked plot based on the curve unit
	VipPlotPlayer::setNewItemBehaviorEnabled(false);

	//set title visible by default
	//QMetaObject::invokeMethod(VipGuiDisplayParamaters::instance()->defaultPlotArea()->titleAxis(), "setVisible", Qt::QueuedConnection, Q_ARG(bool, true));

	return 0;
}

static int register_functions = vipAddGuiInitializationFunction(registerCustomPlotPlayer);