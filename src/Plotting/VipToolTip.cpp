#include "VipToolTip.h"
#include "VipLegendItem.h"
#include "VipPlotItem.h"
#include "VipBorderItem.h"
#include "VipPlotWidget2D.h"
#include "VipCorrectedTip.h"
#include "VipPlotGrid.h"

#include <qmath.h>
#include <QPicture>
#include <QToolTip>
#include <qtimer.h>
#include <qapplication.h>
#include <qstyle.h>
#include <qdesktopwidget.h>





struct VipToolTip::PrivateData
{
	VipAbstractPlotArea * area;
	DisplayFlags displayFlags;
	double stickDistance;
	double distanceToPointer;
	int delayTime;
	QMargins margins;
	bool displayInsideScales;
	int areaAxis;
	int minRefreshTime;
	qint64 lastRefresh;
	QPointF pos;

	int maxLines;
	QString maxLineMessage;

	Vip::RegionPositions position;
	Qt::Alignment alignment;

	QStringList ignoreProperties;

	QSharedPointer<QPoint> offset;

	QList<VipAbstractScale*> scales;

	QPen overlayPen;
	QBrush overlayBrush;

	PrivateData()
		:area(NULL),
		displayFlags(VipToolTip::All),
		stickDistance(10),
		distanceToPointer(10),
		delayTime(2000),
		displayInsideScales(false),
		areaAxis(-1),
		minRefreshTime(100),
		lastRefresh(0),
		maxLines(INT_MAX),
		position(Vip::Automatic),
		alignment(Qt::AlignCenter)
	{
		overlayPen.setStyle(Qt::NoPen);
	}
};

VipToolTip::VipToolTip(QObject * parent)
	:QObject(parent)
{
	d_data = new PrivateData();
}

VipToolTip::~VipToolTip()
{
	delete d_data;
}

void VipToolTip::setMargins(const QMargins& m)
{
	d_data->margins = m;
}

void VipToolTip::setMargins(double m)
{
	d_data->margins = QMargins(m, m, m, m);
}

const QMargins & VipToolTip::margins() const
{
	return d_data->margins;
}

QString VipToolTip::attributeMargins() const
{
	return "margin-top:" + QString::number(margins().top()) + "px ; margin-left:" + QString::number(margins().left()) +
		"px ;margin-bottom:" + QString::number(margins().bottom()) + "px ;margin-right:" + QString::number(margins().right()) + "px ";
}

void VipToolTip::setPlotArea(VipAbstractPlotArea * area)
{
	d_data->area = area;
}

VipAbstractPlotArea * VipToolTip::plotArea() const
{
	return d_data->area;
}

void VipToolTip::setIgnoreProperties(const QStringList & names)
{
	d_data->ignoreProperties = names;
}
void VipToolTip::addIgnoreProperty(const QString & name)
{
	if (d_data->ignoreProperties.indexOf(name) < 0)
		d_data->ignoreProperties.append(name);
}
QStringList VipToolTip::ignoreProperties() const
{
	return d_data->ignoreProperties;
}
bool VipToolTip::isPropertyIgnored(const QString & name) const
{
	if (name == "stylesheet") return true;
	if (name.startsWith("_vip_")) //private properties start with '_vip_'
		return true;
	return d_data->ignoreProperties.indexOf(name) >= 0;
}

void VipToolTip::setDelayTime(int msec)
{
	d_data->delayTime = msec;
}

int VipToolTip::delayTime() const
{
	return d_data->delayTime;
}

void VipToolTip::setDisplayFlags(DisplayFlags displayFlags)
{
	d_data->displayFlags = displayFlags;
}

void VipToolTip::setDisplayFlag(DisplayFlag flag, bool on)
{
	if (d_data->displayFlags.testFlag(flag) != on)
	{
		if (on)
			d_data->displayFlags |= flag;
		else
			d_data->displayFlags &= ~flag;
	}
}

bool VipToolTip::testDisplayFlag(DisplayFlag flag) const
{
	return d_data->displayFlags.testFlag(flag);
}

VipToolTip::DisplayFlags VipToolTip::displayFlags() const
{
	return d_data->displayFlags;
}

//void VipToolTip::setAreaAxis(int axis)
// {
// d_data->areaAxis = axis;
// }
// int VipToolTip::areaAxis() const
// {
// return d_data->areaAxis;
// }

void VipToolTip::setRegionPositions(Vip::RegionPositions pos)
{
	d_data->position = pos;
}
Vip::RegionPositions VipToolTip::regionPositions() const
{
	return d_data->position;
}

void VipToolTip::setAlignment(Qt::Alignment align)
{
	d_data->alignment = align;
}
Qt::Alignment VipToolTip::alignment() const
{
	return d_data->alignment;
}

void VipToolTip::setToolTipOffset(const QPoint & offset)
{
	d_data->offset.reset(new QPoint(offset));
}
QPoint VipToolTip::toolTipOffset() const
{
	return d_data->offset ? *d_data->offset : QPoint();
}
void VipToolTip::removeToolTipOffset()
{
	d_data->offset = QSharedPointer<QPoint>();
}
bool VipToolTip::hasToolTipOffset() const
{
	return d_data->offset;
}

void VipToolTip::setDisplayInsideScales(bool enable)
{
	d_data->displayInsideScales = enable;
}

bool VipToolTip::displayInsideScales() const
{
	return d_data->displayInsideScales;
}

void VipToolTip::setStickDistance(double vipDistance)
{
	d_data->stickDistance = vipDistance;
}

double VipToolTip::stickDistance() const
{
	return d_data->stickDistance;
}

void VipToolTip::setDistanceToPointer(double d)
{
	d_data->distanceToPointer = d;
}

double VipToolTip::distanceToPointer() const
{
	return d_data->distanceToPointer;
}

void VipToolTip::setOverlayPen(const QPen & p)
{
	d_data->overlayPen = p;
}
QPen VipToolTip::overlayPen() const
{
	return d_data->overlayPen;
}

void VipToolTip::setOverlayBrush(const QBrush & b)
{
	d_data->overlayBrush = b;
}
QBrush VipToolTip::overlayBrush() const
{
	return d_data->overlayBrush;
}

void VipToolTip::setMinRefreshTime(int milli)
{
	d_data->minRefreshTime = milli;
}
int VipToolTip::minRefreshTime() const
{
	return d_data->minRefreshTime;
}

void VipToolTip::setMaxLines(int max_lines)
{
	d_data->maxLines = max_lines;
}
int VipToolTip::maxLines() const
{
	return d_data->maxLines;
}

void VipToolTip::setMaxLineMessage(const QString & line_msg)
{
	d_data->maxLineMessage = line_msg;
}
QString VipToolTip::maxLineMessage() const
{
	return d_data->maxLineMessage;
}

void VipToolTip::setScales(const QList<VipAbstractScale*> & scales)
{
	d_data->scales = scales;
}
const QList<VipAbstractScale*> & VipToolTip::scales() const
{
	return d_data->scales;
}

void VipToolTip::refresh()
{	
	//check refresh time
	qint64 current = QDateTime::currentMSecsSinceEpoch();
	if (current - d_data->lastRefresh < d_data->minRefreshTime)
		return;

	d_data->lastRefresh = current;
	if (d_data->pos == QPointF())
		return;

	if (auto* v = plotArea()->view()) {
		if (!v->isVisible() || v->isHidden())
			return;
		if (!v->underMouse())
			return;
	}
	else
		return;

	//check that the mouse is inside the canvas
	QPainterPath p = plotArea()->canvas()->shape();

	QPointF pos = screenToSceneCoordinates(plotArea()->scene(), QCursor::pos());
	pos = plotArea()->canvas()->mapFromScene(pos);
	if (!p.contains(pos))
		return;

	QPointF saved = d_data->pos;
	setPlotAreaPos(QPointF(d_data->pos));
	d_data->pos = saved;
}

void VipToolTip::setPlotAreaPos(const QPointF & pos)
{
	d_data->pos = QPointF();

	if (!plotArea() || !plotArea()->scene() || testDisplayFlag(Hidden))
	{
		return;
	}

	QStringList text;
	int line = 0;

	//compute axes text (only for independant axes)
	if (testDisplayFlag(Axes))
	{
		QStringList axis_text;
		const QList<VipAbstractScale*> scales = d_data->scales;//VipAbstractScale::independentScales(plotArea()->scales());

		for (int i = 0; i < scales.size(); ++i)
		{
			VipAbstractScale * scale = scales[i];
			if (!scale->isVisible() || scale->property("_vip_ignoreToolTip").toBool())
				continue;

			VipText title = scale->title();

			QPointF axis_pos = plotArea()->mapToScene(pos);
			axis_pos = scale->mapFromScene(axis_pos);

			vip_double value = scale->constScaleDraw()->value(axis_pos);
			if (title.text().isEmpty())
				axis_text << scale->constScaleDraw()->label(value, VipScaleDiv::MajorTick).text();
			else
				axis_text << "<b>" + title.text() + "</b> = " + scale->constScaleDraw()->label(value, VipScaleDiv::MajorTick).text();

			if (++line >= d_data->maxLines)
				break;
		}

		text << "<p>" + axis_text.join("<br>") + "</p>";
	}

	//compute items text

	PlotItemList 	items;
	QList<VipPointVector> points;
	VipBoxStyleList 	styles;
	QList<int> legends;

	int axis = -1;
	if ((d_data->displayFlags & SearchXAxis) && !(d_data->displayFlags & SearchYAxis))
		axis = 0;
	else if ((d_data->displayFlags & SearchYAxis) && !(d_data->displayFlags & SearchXAxis))
		axis = 1;

	items = plotArea()->plotItems(pos, axis, stickDistance(), points, styles, legends);

	QPicture additional;
	QPainter* pa = nullptr;
	



	for (int i = 0; i < items.size(); ++i)
	{
		VipPlotItem * item = items[i];
		if (!item->isVisible() || item->property("_vip_ignoreToolTip").toBool())
			continue;

		QString custom_tooltip;

		if (line >= d_data->maxLines)
			break;

		if (!item->testItemAttribute(VipPlotItem::HasToolTip))
			continue;

		if (!styles[i].isEmpty())
		{
			if (!pa) {
				pa = new QPainter();
				pa->begin(&additional);
			}
			if (d_data->overlayBrush.style() != Qt::NoBrush || d_data->overlayPen.style() != Qt::NoPen) {
				VipBoxStyle st = styles[i];
				if (d_data->overlayBrush.style() != Qt::NoBrush)
					st.setBackgroundBrush(d_data->overlayBrush);
				if (d_data->overlayPen.style() != Qt::NoPen)
					st.setBorderPen(d_data->overlayPen);
				
				st.draw(pa);
			}
			else
				styles[i].draw(pa);
		}

		const VipPointVector points_of_intereset =
			points[i].isEmpty() ?
			(VipPointVector() << plotArea()->mapToItem(item, pos)) :
			points[i];

		//compute custom tool tip
		if (testDisplayFlag(ItemsToolTips))
		{
			for (int p = 0; p < points_of_intereset.size(); ++p)
			{
				QString tooltip = item->formatToolTip((QPointF)points_of_intereset[p]);
				if (!tooltip.isEmpty())
				{
					custom_tooltip += "<p>" + tooltip + "</p>";
					if (++line >= d_data->maxLines)
						break;
				}
			}
		}

		QStringList item_title;
		QStringList item_text;

		//compute title
		if (legends[i] >= 0)
		{
			int legend = legends[i];
			VipText name = item->legendNames()[legend];
			if (testDisplayFlag(ItemsLegends))
				item_title << QString(vipToHtml(item->legendPixmap(QSize(20, 16), legend), "vertical-align:\"middle\"")) + (testDisplayFlag(ItemsTitles) ? "<b>" + name.text() + "</b>" : QString());
			else if (testDisplayFlag(ItemsTitles))
				item_title << "<b>" + name.text() + "</b>";

			if (++line >= d_data->maxLines)
				break;
		}

		if (!item->testItemAttribute(VipPlotItem::CustomToolTipOnly))
		{
			//compute item position
			if (testDisplayFlag(ItemsPos) && !points[i].isEmpty())
			{
				QStringList axis_text;
				const QList<VipAbstractScale*> scales = item->axes();
				for (int p = 0; p < points_of_intereset.size(); ++p)
				{
					for (int s = 0; s < scales.size(); ++s)
					{
						if (!scales[s]) continue;

						VipText title = scales[s]->title();
						QPointF axis_pos = scales[s]->mapFromItem(item, (QPointF)points_of_intereset[p]);

						vip_double value = scales[s]->constScaleDraw()->value(axis_pos);
						if (title.isEmpty())
							axis_text << scales[s]->constScaleDraw()->label(value, VipScaleDiv::MajorTick).text() + " " + scales[s]->constScaleDraw()->valueToText()->exponentText();
						else
							axis_text << "<b>" + title.text() + "</b> = " + scales[s]->constScaleDraw()->label(value, VipScaleDiv::MajorTick).text() + " " + scales[s]->constScaleDraw()->valueToText()->exponentText();

						if (++line >= d_data->maxLines)
							break;
					}
				}

				if (axis_text.size())
					item_text << axis_text.join("<br>");
			}

			//compute item properties
			if (testDisplayFlag(ItemsProperties))
			{
				QList<QByteArray> props = item->dynamicPropertyNames();
				for (int p = 0; p < props.size(); ++p)
				{
					if (!isPropertyIgnored(props[p]))
					{
						QString t_value = item->property(props[p].data()).toString();
						if (!t_value.isEmpty())
						{
							item_text << "<b>" + QString(props[p]) + "</b> = " + t_value;
							if (++line >= d_data->maxLines)
								break;
						}
					}
				}
			}

		}

		if (item_title.size() && item_text.size())
			text << "<p>" + item_title.join("<br>") + "<br><span margin-left:\"10px\">" + custom_tooltip + item_text.join("<br>") + "</span></p>";
		else if (item_title.size())
			text << "<p>" + item_title.join("<br>") + custom_tooltip;
		else if (item_text.size())
			text << "<p>" + custom_tooltip + item_text.join("<br>");
		else if (custom_tooltip.size())
			text << "<p>" + custom_tooltip;

	}

	if (line >= d_data->maxLines)
	{
		text.append("<br>...");
		if (!d_data->maxLineMessage.isEmpty())
			text.append("<br>" + d_data->maxLineMessage);
	}

	//set the additional VipBoxStyle to draw
	if (pa) {
		pa->end();
		delete pa;
	}
	//printf("setAdditionalPaintCommands %i\n", (int)additional.isNull());
	plotArea()->rubberBand()->setAdditionalPaintCommands(additional);


	QString tool_tip;
	if (text.size())
	{
		tool_tip = text.join("");
		tool_tip = "<p style=\"" + attributeMargins() + "\">" + tool_tip + "</p>";
	}

	QWidget * parent = NULL;
	if (QGraphicsView * v = plotArea()->view())
		parent = v->viewport();

	if (!tool_tip.isEmpty())
	{
		//remove empty paragraph or newline at the end
		while (true)
		{
			if (tool_tip.endsWith("<br>"))
				tool_tip.remove(tool_tip.size() - 4, 4);
			if (tool_tip.endsWith("<p>"))
				tool_tip.remove(tool_tip.size() - 3, 3);
			else if (tool_tip.endsWith("<p></p>"))
				tool_tip.remove(tool_tip.size() - 7, 7);
			else
				break;
		}

		tool_tip = ("<div style = \"white-space:nowrap;\"><p align='left' style = \"white-space:nowrap; width: 1200px;\">" + tool_tip + "</p></div>");
		VipText text = tool_tip;
		QPoint this_pos = toolTipPosition(text, pos, d_data->position, d_data->alignment);
		VipCorrectedTip::showText(this_pos, text.text(), parent, QRect(), d_data->delayTime);
		d_data->pos = pos;
	}
	else {
		VipText t;
		VipCorrectedTip::showText(toolTipPosition(t, pos, d_data->position, d_data->alignment), QString(), parent, QRect(), d_data->delayTime);
	}
}



static QPoint findPosition(Vip::RegionPositions position, Qt::Alignment alignment, const QSize & size, const QRect & //screen
, const QRect & area_screen, double distanceToPointer)
{
	QPoint this_pos;
	if (position & Vip::XInside)
	{
		if (alignment & Qt::AlignLeft)
			this_pos.setX(area_screen.left() + distanceToPointer);
		else if (alignment & Qt::AlignRight)
			this_pos.setX(area_screen.right() - size.width() - distanceToPointer);
		else
			this_pos.setX(area_screen.center().x() - size.width() / 2);
	}
	else
	{
		if (alignment & Qt::AlignLeft)
			this_pos.setX(area_screen.left() - size.width() - distanceToPointer);
		else if (alignment & Qt::AlignRight)
			this_pos.setX(area_screen.right() + distanceToPointer);
		else
			this_pos.setX(area_screen.center().x() - size.width() / 2);
	}

	if (position & Vip::YInside)
	{
		if (alignment & Qt::AlignTop)
			this_pos.setY(area_screen.top() + distanceToPointer);
		else if (alignment & Qt::AlignBottom)
			this_pos.setY(area_screen.bottom() - size.height() - distanceToPointer);
		else
			this_pos.setY(area_screen.center().y() - size.height() / 2);
	}
	else
	{
		if (alignment & Qt::AlignTop)
			this_pos.setY(area_screen.top() - size.height() - distanceToPointer);
		else if (alignment & Qt::AlignBottom)
			this_pos.setY(area_screen.bottom() + distanceToPointer);
		else
			this_pos.setY(area_screen.center().y() - size.height() / 2);
	}

	//this_pos -= QPoint(5, 15);
	return this_pos;
}

QPoint VipToolTip::toolTipPosition(VipText & text, const QPointF & pos, Vip::RegionPositions position, Qt::Alignment alignment)
{
	QDesktopWidget *w = qApp->desktop();
	int screen_number = -1;
	if (this->plotArea() && this->plotArea()->view())
		screen_number = w->screenNumber(this->plotArea()->view());
	QRect screen = w->screenGeometry(screen_number);
	QRect tip_rect = VipCorrectedTip::textGeometry(QPoint(0, 0), text.text(), plotArea()->view(), QRect());
	QSize tip_size = tip_rect.size();
	QPoint tip_offset = tip_rect.topLeft();

	if (d_data->offset)
	{
		QPoint this_pos = sceneToScreenCoordinates(plotArea()->scene(), plotArea()->mapToScene(pos));

		int factor = 1;
		QRect rect = QRect(QPoint(0, 0), tip_size).translated(this_pos);
		rect.setBottom(rect.bottom() + 50 + d_data->offset->y());
		//rect.setRight(rect.right() + 50 + d_data->offset->x());
		if ((rect & screen) != rect)
			factor = -1;
		//static int c = 0;
		this_pos += *d_data->offset * factor;
		//printf("offset:%i,  %i\n",++c,(int) d_data->offset->x());
		return this_pos;
	}


	QRectF geometry = plotArea()->boundingRect();
	if (d_data->displayInsideScales)
		if (VipPlotCanvas * c = plotArea()->canvas())
			if (VipCoordinateSystemPtr ptr = c->sceneMap())
				geometry = ptr->clipPath(c).boundingRect();

	if (position == Vip::Automatic)
	{
		QPoint this_pos = sceneToScreenCoordinates(plotArea()->scene(), plotArea()->mapToScene(pos));
		QPoint mouse_pos = this_pos;
		this_pos.setY(this_pos.y() + d_data->distanceToPointer);
		this_pos.setX(this_pos.x() - tip_size.width()* (pos.x() - geometry.left()) / geometry.width());

		//make sure the tool tip fits within the screen in height
		QRect rect = QRect(QPoint(0, 0), tip_size).translated(this_pos);

		rect.setBottom(rect.bottom() + 50);

		if ((rect & screen) != rect)
		{
			if (rect.bottom() > screen.bottom())
			{
				if (qAbs(mouse_pos.x() - rect.left()) < qAbs(mouse_pos.x() - rect.right()))
				{
					this_pos.setX(mouse_pos.x() - rect.width() - d_data->distanceToPointer);
				}
				else
				{
					this_pos.setX(mouse_pos.x() + d_data->distanceToPointer);
				}

			}
		}
		return this_pos - tip_offset;
	}
	else
	{
		QRectF tmp = plotArea()->mapToScene(geometry).boundingRect();
		QRect area_screen = QRect(sceneToScreenCoordinates(plotArea()->scene(), tmp.topLeft()), sceneToScreenCoordinates(plotArea()->scene(), tmp.bottomRight())).normalized();
		QPoint this_pos = findPosition(position, alignment, tip_size, screen, area_screen, d_data->distanceToPointer);
		QRect this_rect(this_pos, tip_size);

		//be sure the tool tip is not above the mouse
		QPoint mouse_pos = QCursor::pos();//sceneToScreenCoordinates(plotArea()->scene(), plotArea()->mapToScene(pos));
		if (this_rect.contains(mouse_pos) && position != Vip::Automatic)
		{
			Qt::Alignment align = 0;;
			if (alignment & Qt::AlignRight)
				align |= Qt::AlignLeft;
			else if (alignment & Qt::AlignLeft)
				align |= Qt::AlignRight;
			if (alignment & Qt::AlignTop)
				align |= Qt::AlignBottom;
			else if (alignment & Qt::AlignBottom)
				align |= Qt::AlignTop;

			this_pos = findPosition(position, align, tip_size, screen, area_screen, d_data->distanceToPointer);
			this_rect = QRect(this_pos, tip_size);
		}

		//tool tip outside the screen
		if (this_rect.top() < screen.top())
			this_rect.moveTop(screen.top());
		if (this_rect.bottom() > screen.bottom())
			this_rect.moveBottom(screen.bottom());
		if (this_rect.left() < screen.left())
			this_rect.moveLeft(screen.left());
		if (this_rect.right() > screen.right())
			this_rect.moveRight(screen.right());

		return this_rect.topLeft() - tip_offset;
	}
}
