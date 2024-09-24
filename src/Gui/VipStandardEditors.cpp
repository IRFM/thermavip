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

#include <QBoxLayout>
#include <QFontDialog>
#include <QGridLayout>
#include <qapplication.h>
#include <qgroupbox.h>

#include "VipGui.h"
#include "VipPlayer.h"
#include "VipPlotCurve.h"
#include "VipPlotGrid.h"
#include "VipPlotHistogram.h"
#include "VipPlotRasterData.h"
#include "VipPlotWidget2D.h"
#include "VipScaleDraw.h"
#include "VipScaleEngine.h"
#include "VipStandardEditors.h"
#include "VipTextOutput.h"

static void applyAsStyleSheet(const VipBoxStyle& style, VipPlotItem* item)
{
	item->styleSheet().setProperty("VipPlotItem", "border", QVariant::fromValue(style.borderPen()));
	item->styleSheet().setProperty("VipPlotItem", "background", QVariant::fromValue(style.backgroundBrush().color()));
	item->updateStyleSheetString();
}

static void removeStyleSheet(VipPlotItem* item)
{
	if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(VipAbstractPlayer::findAbstractPlayer(item))) {
		pl->removeStyleSheet(item);
	}
}
static QGroupBox* createGroup(const QString& label)
{
	QGroupBox* res = new QGroupBox(label);
	res->setFlat(true);
	QFont f = res->font();
	f.setBold(true);
	res->setFont(f);
	return res;
}

QString vipComprehensiveName(QObject* obj)
{
	if (QGraphicsObject* o = qobject_cast<QGraphicsObject*>(obj))
		return vipItemName(o);

	if (qobject_cast<VipPlotGrid*>(obj)) {
		return "Axes grid";
	}
	else if (VipPlotCurve* c = qobject_cast<VipPlotCurve*>(obj)) {
		if (!c->title().isEmpty())
			return c->title().text();
		return "Curve";
	}
	else if (VipPlotHistogram* h = qobject_cast<VipPlotHistogram*>(obj)) {
		if (!h->title().isEmpty())
			return h->title().text();
		return "Histogram";
	}
	else if (VipAxisBase* ax = qobject_cast<VipAxisBase*>(obj)) {
		QString ori = ax->title().text();
		if (ori.isEmpty())
			ori = "Axis";
		if (ax->alignment() == VipAxisBase::Bottom)
			ori += " (bottom)";
		else if (ax->alignment() == VipAxisBase::Top)
			ori += " (top)";
		else if (ax->alignment() == VipAxisBase::Left)
			ori += " (left)";
		if (ax->alignment() == VipAxisBase::Right)
			ori += " (right)";
		return ori;
	}
	return vipSplitClassname(obj->metaObject()->className());
}

QString vipItemName(QGraphicsObject* obj)
{
	if (VipPlotItem* item = qobject_cast<VipPlotItem*>(obj)) {
		QString classname = vipSplitClassname(item->metaObject()->className());
		if (classname.startsWith("plot ", Qt::CaseInsensitive))
			classname = classname.mid(5);
		if (qobject_cast<VipPlotGrid*>(obj)) {
			classname += " (Y unit: " + item->axisUnit(1).text() + ")";
			return classname;
		}
		if (!item->title().isEmpty())
			classname += ": " + item->title().text();
		else {
			// unlsess grid or canvas, returns empty string
			if (!qobject_cast<VipPlotGrid*>(obj) && !qobject_cast<VipPlotCanvas*>(obj))
				return QString();
			else {
				classname += " (Y unit: " + item->axisUnit(1).text() + ")";
				return classname;
			}
		}
		return classname;
	}
	else if (VipAxisColorMap* map = qobject_cast<VipAxisColorMap*>(obj)) {
		QString res = "Colormap";
		if (!map->title().isEmpty())
			res += ": " + map->title().text();
		return res;
	}
	else if (VipAbstractScale* scale = qobject_cast<VipAbstractScale*>(obj)) {
		if (!scale->title().isEmpty())
			return "Axis: " + scale->title().text();
		else {
			QString res;
			// try to find a synchronized scale with a valid name
			QSet<VipAbstractScale*> scales = scale->synchronizedWith();
			for (QSet<VipAbstractScale*>::iterator it = scales.begin(); it != scales.end(); ++it) {
				res = *it != scale ? (*it)->title().text() : QString();
				if (!res.isEmpty()) {
					if (VipAxisBase* ax = qobject_cast<VipAxisBase*>(scale)) {
						if (ax->alignment() == VipAxisBase::Bottom)
							return "Axis: " + res + " (bottom)";
						else if (ax->alignment() == VipAxisBase::Top)
							return "Axis: " + res + " (top)";
						else if (ax->alignment() == VipAxisBase::Left)
							return "Axis: " + res + " (left)";
						if (ax->alignment() == VipAxisBase::Right)
							return "Axis: " + res + " (right)";
					}
					break;
				}
			}
		}
		return QString();
	}

	return vipSplitClassname(obj->metaObject()->className());
}

QList<VipAbstractScale*> vipAllScales(VipPlotItem* item)
{
	// NEWPLOT
	// QList<VipAbstractScale*> scales = item->axes();
	//  QList<VipAbstractScale*> res;
	//
	// for(int i=0; i < scales.size(); ++i)
	// if(scales[i])
	// {
	// scales = scales[i]->linkedScales();
	// break;
	// }
	//
	// //scales = VipAbstractScale::independentScales(scales);
	// scales = scales.toSet().toList(); //remove duplicates
	QList<VipAbstractScale*> scales, res;
	if (VipAbstractPlotArea* area = item->area()) {
		scales = area->allScales();
	}

	for (int i = 0; i < scales.size(); ++i)
		if (!scales[i]->title().isEmpty() || !scales[i]->objectName().isEmpty())
			res << scales[i];

	return res;
}

QStringList vipScaleNames(const QList<VipAbstractScale*>& scales)
{
	QStringList res;
	for (int i = 0; i < scales.size(); ++i) {
		QString name = vipItemName(scales[i]);
		if (!name.isEmpty())
			res << name;
	}

	return res;
}

int vipIndexOfScale(const QList<VipAbstractScale*>& scales, const QString& name)
{
	for (int i = 0; i < scales.size(); ++i)
		if (scales[i]->title().text() == name || scales[i]->objectName() == name)
			return i;

	return -1;
}

VipSymbolWidget::VipSymbolWidget(QWidget* parent)
  : QWidget(parent)
{
	QGridLayout* glay = new QGridLayout();
	int row = -1;

	QLabel* outer_pen = new QLabel("Outer pen:");
	outer_pen->setObjectName("outer_pen");
	QLabel* inner_brush = new QLabel("Inner brush:");
	inner_brush->setObjectName("inner_brush");

	glay->addWidget(new QLabel("Symbol style:"), ++row, 0);
	glay->addWidget(&m_style, row, 1);
	glay->addWidget(new QLabel("Symbol size:"), ++row, 0);
	glay->addWidget(&m_size, row, 1);
	glay->addWidget(outer_pen, ++row, 0);
	glay->addWidget(&m_pen_color, row, 1);
	glay->addWidget(inner_brush, ++row, 0);
	glay->addWidget(&m_brush_color, row, 1);
	setLayout(glay);

	m_style.setFrame(false);
	m_style.setToolTip("Select the symbol style");

	m_size.setFrame(false);
	m_size.setRange(1, 100);
	m_size.setValue(1);
	m_size.setToolTip("Select the symbol size (1->100)");

	m_brush_color.setMode(VipPenButton::Brush);
	// m_pen_color.setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
	// m_brush_color.setToolButtonStyle (Qt::ToolButtonTextBesideIcon);
	setSymbol(VipSymbol());

	QObject::connect(&m_style, SIGNAL(activated(int)), this, SLOT(emitSymbolChanged()));
	QObject::connect(&m_size, SIGNAL(valueChanged(int)), this, SLOT(emitSymbolChanged()));
	QObject::connect(&m_pen_color, SIGNAL(penChanged(const QPen&)), this, SLOT(emitSymbolChanged()));
	QObject::connect(&m_brush_color, SIGNAL(penChanged(const QPen&)), this, SLOT(emitSymbolChanged()));

	this->setMaximumSize(QSize(300, 250));
}

// void VipSymbolWidget::setColorOptionsVisible(bool vis)
//  {
//  m_pen_color.setVisible(vis);
//  m_brush_color.setVisible(vis);
//  findChild<QLabel*>("outer_pen")->setVisible(vis);
//  findChild<QLabel*>("inner_brush")->setVisible(vis);
//  }
//
//  bool VipSymbolWidget::colorOptionsVisible() const
//  {
//  return m_pen_color.isVisible();
//  }

void VipSymbolWidget::setSymbol(const VipSymbol& symbol)
{
	m_symbol = symbol;

	m_pen_color.blockSignals(true);
	m_brush_color.blockSignals(true);
	m_style.blockSignals(true);
	m_size.blockSignals(true);

	m_style.setCurrentIndex(symbol.style());
	m_size.setValue(symbol.size().width());
	m_pen_color.setPen(symbol.pen());
	m_brush_color.setBrush(symbol.brush());
	redraw();

	m_pen_color.blockSignals(false);
	m_brush_color.blockSignals(false);
	m_style.blockSignals(false);
	m_size.blockSignals(false);

	emit symbolChanged(m_symbol);
}

VipSymbol VipSymbolWidget::getSymbol() const
{
	return m_symbol;
}

void VipSymbolWidget::emitSymbolChanged()
{
	m_symbol.setStyle(VipSymbol::Style(m_style.currentIndex()));
	m_symbol.setSize(QSize(m_size.value(), m_size.value()));
	m_symbol.setPen(m_pen_color.pen());
	m_symbol.setBrush(m_brush_color.pen().brush());
	emit symbolChanged(m_symbol);
}

void VipSymbolWidget::redraw()
{

	// set the styles;
	int index = m_style.currentIndex();
	m_style.clear();
	m_style.setIconSize(QSize(30, 20));
	for (int i = VipSymbol::Ellipse; i <= VipSymbol::Hexagon; ++i) {
		QPixmap pix(QSize(30, 20));
		pix.fill(QColor(255, 255, 255, 0));
		QPainter p(&pix);
		p.setRenderHint(QPainter::Antialiasing, true);
		VipSymbol sym(m_symbol);
		sym.setSize(QSizeF(15, 15));
		sym.setStyle(VipSymbol::Style(i));
		sym.drawSymbol(&p, QPointF(15, 10));
		m_style.addItem(QIcon(pix), "");
	}
	if (index >= 0)
		m_style.setCurrentIndex(index);
}

VipPlotItemWidget::VipPlotItemWidget(QWidget* parent)
  : QWidget(parent)
  , m_item(nullptr)
  , m_xAxisLabel("X axis")
  , m_yAxisLabel("Y axis")
{
	QGridLayout* lay = new QGridLayout();
	int row = -1;
	m_titleLabel.setText("Title:");
	lay->addWidget(&m_titleLabel, ++row, 0);
	lay->addWidget(&m_title, row, 1);
	lay->addWidget(&m_visible, ++row, 0, 1, 2);
	lay->addWidget(&m_antialiazed, ++row, 0, 1, 2);
	lay->addWidget(&m_drawText, ++row, 0, 1, 2);
	lay->addWidget(&m_xAxisLabel, ++row, 0);
	lay->addWidget(&m_xAxis, row, 1);
	lay->addWidget(&m_yAxisLabel, ++row, 0);
	lay->addWidget(&m_yAxis, row, 1);
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	m_title.setToolTip("Plot item's title.<br>Press ENTER after changing the title.");
	m_drawText.setText("Draw additional text");
	m_drawText.setToolTip("If this plot item draws additional text, shows/hides it");
	m_visible.setText("Visible item");
	m_visible.setToolTip("Show/hide this plot item");
	m_antialiazed.setText("Anti-aliazed drawing");
	m_antialiazed.setToolTip("Draw this item using anti-aliasing");

	connect(&m_title,
		SIGNAL(returnPressed() // changed(const VipText&)
		       ),
		this,
		SLOT(emitPlotItemChanged()));
	connect(&m_visible, SIGNAL(clicked(bool)), this, SLOT(emitPlotItemChanged()));
	connect(&m_antialiazed, SIGNAL(clicked(bool)), this, SLOT(emitPlotItemChanged()));
	connect(&m_drawText, SIGNAL(clicked(bool)), this, SLOT(emitPlotItemChanged()));
	connect(&m_xAxis, SIGNAL(currentIndexChanged(int)), this, SLOT(emitPlotItemChanged()));
	connect(&m_yAxis, SIGNAL(currentIndexChanged(int)), this, SLOT(emitPlotItemChanged()));
}

VipPlotItemWidget::~VipPlotItemWidget() {}

void VipPlotItemWidget::setPlotItem(VipPlotItem* item)
{
	if (!item)
		return;

	m_item = item;
	m_scales = vipAllScales(item);

	m_visible.blockSignals(true);
	m_antialiazed.blockSignals(true);
	m_drawText.blockSignals(true);
	m_title.blockSignals(true);
	m_xAxis.blockSignals(true);
	m_yAxis.blockSignals(true);

	m_antialiazed.setChecked(item->renderHints() & QPainter::Antialiasing);
	m_visible.setChecked(item->isVisible());
	m_drawText.setChecked(item->drawText());
	m_title.setText(item->title().text());

	QList<VipAbstractScale*> item_scales = item->axes();
	// TEST: remove scale edition for plot items
	bool have_axis_option = false; //(item_scales.size() == 2);
	if (have_axis_option) {
		m_xAxis.clear();
		m_xAxis.addItems(vipScaleNames(m_scales));
		m_xAxis.setCurrentIndex(m_scales.indexOf(item_scales[0]));
		m_yAxis.clear();
		m_yAxis.addItems(vipScaleNames(m_scales));
		m_yAxis.setCurrentIndex(m_scales.indexOf(item_scales[1]));
	}
	m_xAxis.setVisible(have_axis_option);
	m_yAxis.setVisible(have_axis_option);
	m_xAxisLabel.setVisible(have_axis_option);
	m_yAxisLabel.setVisible(have_axis_option);

	m_visible.blockSignals(false);
	m_antialiazed.blockSignals(false);
	m_drawText.blockSignals(false);
	m_title.blockSignals(false);
	m_xAxis.blockSignals(false);
	m_yAxis.blockSignals(false);
}

VipPlotItem* VipPlotItemWidget::getPlotItem() const
{
	return const_cast<VipPlotItem*>(m_item.data());
}

void VipPlotItemWidget::setTitleVisible(bool vis)
{
	m_title.setVisible(vis);
	m_titleLabel.setVisible(vis);
}
bool VipPlotItemWidget::titleVisible() const
{
	return m_title.isVisible();
}

void VipPlotItemWidget::updatePlotItem(VipPlotItem* item)
{
	if (!item)
		return;

	// block the selectionChanged() signal beiing emitted by the scene
	// because hiding the item will trigger it, which will change the current editor,
	// delete the previous one (which is currently used) and BOOOM on m_title.text() !

	if (item->scene())
		item->scene()->blockSignals(true);
	item->setVisible(m_visible.isChecked());
	if (item->scene())
		item->scene()->blockSignals(false);

	item->setTitle(VipText(m_title.text(), item->title().textStyle()));
	item->setDrawText(m_drawText.isChecked());

	VipProcessingObject* to_reload = nullptr;
	if (sender() == &m_title) {
		// try to set the title to the source processing object
		if (VipDisplayObject* obj = item->property("VipDisplayObject").value<VipDisplayObject*>()) {
			if (VipOutput* out = obj->inputAt(0)->connection()->source())
				if (VipProcessingObject* p = out->parentProcessing())
					if (p->attribute("Name").toString() != m_title.text()) {
						p->setAttribute("Name", m_title.text());
						to_reload = p;
					}
		}
	}

	if (m_antialiazed.isChecked()) {
		item->setRenderHints(item->renderHints() | QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
	}
	else
		item->setRenderHints(QPainter::RenderHints());

	QList<VipAbstractScale*> item_scales = item->axes();
	if (item_scales.size() == 2) {
		int index = m_xAxis.currentIndex();
		if (index >= 0 && index < m_scales.size())
			item_scales[0] = m_scales[index];

		index = m_yAxis.currentIndex();
		if (index >= 0 && index < m_scales.size())
			item_scales[1] = m_scales[index];

		item->setAxes(item_scales, item->coordinateSystemType());
	}

	if (to_reload)
		to_reload->reload();
	item->update();
}

void VipPlotItemWidget::emitPlotItemChanged()
{
	if (m_item) {
		removeStyleSheet(m_item);
		updatePlotItem(m_item);

		Q_EMIT plotItemChanged(m_item);
	}
}

VipPlotGridWidget::VipPlotGridWidget(QWidget* parent)
  : QWidget(parent)
  , m_grid(nullptr)
{
	QGridLayout* glay = new QGridLayout();

	int row = -1;

	glay->addWidget(&m_item, ++row, 0, 1, 2);

	glay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	glay->addWidget(&m_enableX, ++row, 0);
	glay->addWidget(&m_enableXMin, row, 1);

	glay->addWidget(&m_enableY, ++row, 0);
	glay->addWidget(&m_enableYMin, row, 1);

	glay->addWidget(&m_maj_pen, ++row, 0);
	glay->addWidget(&m_min_pen, row, 1);

	setLayout(glay);
	glay->setContentsMargins(0, 0, 0, 0);

	m_item.setTitleVisible(false);

	m_enableX.setText("Enable X major");
	m_enableXMin.setText("Enable X minor");

	m_enableY.setText("Enable Y major");
	m_enableYMin.setText("Enable Y minor");

	m_maj_pen.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_min_pen.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	m_maj_pen.setText("Major pen");
	m_maj_pen.setToolTip("Change the grid major pen");
	m_min_pen.setText("Minor pen");
	m_min_pen.setToolTip("Change the grid minor pen");

	connect(&m_enableX, SIGNAL(clicked(bool)), this, SLOT(emitGridChanged()));
	connect(&m_enableY, SIGNAL(clicked(bool)), this, SLOT(emitGridChanged()));
	connect(&m_enableXMin, SIGNAL(clicked(bool)), this, SLOT(emitGridChanged()));
	connect(&m_enableYMin, SIGNAL(clicked(bool)), this, SLOT(emitGridChanged()));
	connect(&m_maj_pen, SIGNAL(penChanged(const QPen&)), this, SLOT(emitGridChanged()));
	connect(&m_min_pen, SIGNAL(penChanged(const QPen&)), this, SLOT(emitGridChanged()));
	connect(&m_item, SIGNAL(plotItemChanged(VipPlotItem*)), this, SLOT(emitGridChanged()));
}

void VipPlotGridWidget::setGrid(VipPlotGrid* grid)
{
	if (!grid)
		return;

	m_grid = grid;

	m_enableX.blockSignals(true);
	m_enableXMin.blockSignals(true);
	m_enableY.blockSignals(true);
	m_enableYMin.blockSignals(true);
	m_maj_pen.blockSignals(true);
	m_min_pen.blockSignals(true);
	m_item.blockSignals(true);

	m_item.setPlotItem(grid);

	m_enableX.setChecked(grid->axisEnabled(0));
	m_enableXMin.setChecked(grid->axisMinEnabled(0));

	m_enableY.setChecked(grid->axisEnabled(1));
	m_enableYMin.setChecked(grid->axisMinEnabled(1));

	m_maj_pen.setPen(grid->majorPen());
	QPen pen(grid->minorPen());

	m_min_pen.setPen(pen);

	m_enableX.blockSignals(false);
	m_enableXMin.blockSignals(false);
	m_enableY.blockSignals(false);
	m_enableYMin.blockSignals(false);
	m_maj_pen.blockSignals(false);
	m_min_pen.blockSignals(false);
	m_item.blockSignals(false);
}

VipPlotGrid* VipPlotGridWidget::getGrid() const
{
	return const_cast<VipPlotGrid*>(m_grid.data());
}

void VipPlotGridWidget::updateGrid(VipPlotGrid* grid)
{
	if (!grid)
		return;

	QColor prev = grid->majorPen().color();
	QColor _new = m_maj_pen.pen().color();

	m_item.updatePlotItem(grid);
	grid->enableAxis(0, m_enableX.isChecked());
	grid->enableAxisMin(0, m_enableXMin.isChecked());

	grid->enableAxis(1, m_enableY.isChecked());
	grid->enableAxisMin(1, m_enableYMin.isChecked());

	grid->setMajorPen(m_maj_pen.pen());
	grid->setMinorPen(m_min_pen.pen());
}

void VipPlotGridWidget::emitGridChanged()
{
	if (m_grid) {
		removeStyleSheet(m_grid);
		updateGrid(m_grid);
		Q_EMIT gridChanged(m_grid);
	}
}

VipPlotCanvasWidget::VipPlotCanvasWidget(QWidget* parent)
  : QWidget(parent)
  , m_canvas(nullptr)
{
	QGroupBox* inner = createGroup("Inner area");
	inner->setFlat(true);
	QGroupBox* outer = createGroup("Outer area");
	outer->setFlat(true);

	QVBoxLayout* lay = new QVBoxLayout();

	lay->addWidget(inner);
	lay->addWidget(&m_inner);

	lay->addWidget(outer);
	lay->addWidget(&m_outer);

	setLayout(lay);

	connect(&m_inner, SIGNAL(boxStyleChanged(const VipBoxStyle&)), this, SLOT(emitCanvasChanged()));
	connect(&m_outer, SIGNAL(boxStyleChanged(const VipBoxStyle&)), this, SLOT(emitCanvasChanged()));
}

void VipPlotCanvasWidget::setCanvas(VipPlotCanvas* canvas)
{
	if (!canvas)
		return;

	m_canvas = canvas;
	// m_area = nullptr;

	// if(QGraphicsItem * parent = canvas->parentItem())
	//	if(QGraphicsObject * obj = parent->toGraphicsObject())
	//		if(VipPlotItemArea * area = qobject_cast<VipPlotItemArea*>(obj))
	//			m_area = area;

	m_outer.setVisible(false); // m_area);

	m_inner.blockSignals(true);
	m_outer.blockSignals(true);

	m_inner.setBoxStyle(canvas->boxStyle());
	// if(m_area)
	//	m_outer.setBoxStyle(m_area->boxStyle());

	m_inner.blockSignals(false);
	m_outer.blockSignals(false);
}

VipPlotCanvas* VipPlotCanvasWidget::getCanvas() const
{
	return const_cast<VipPlotCanvas*>(m_canvas.data());
}

void VipPlotCanvasWidget::updateCanvas(VipPlotCanvas* canvas)
{
	if (!canvas)
		return;

	QColor prev = canvas->boxStyle().backgroundBrush().color();
	QColor _new = m_inner.getBoxStyle().backgroundBrush().color();

	canvas->setBoxStyle(m_inner.getBoxStyle());
	applyAsStyleSheet(m_inner.getBoxStyle(), canvas);
}

void VipPlotCanvasWidget::emitCanvasChanged()
{
	if (m_canvas) {
		removeStyleSheet(m_canvas);
		updateCanvas(m_canvas);
		Q_EMIT canvasChanged(m_canvas);
	}
}

VipPlotCurveWidget::VipPlotCurveWidget(QWidget* parent)
  : QWidget(parent)
  , m_curve(nullptr)
{
	QVBoxLayout* lay = new QVBoxLayout();

	m_draw_line.setTitle("Curve line");
	m_draw_line.setFlat(true);
	m_draw_line.setCheckable(true);
	m_draw_line.setToolTip("Check/uncheck to draw/hide the curve outline");

	m_draw_symbol.setTitle("Curve symbol");
	m_draw_symbol.setFlat(true);
	m_draw_symbol.setCheckable(true);
	m_draw_symbol.setToolTip("Check/uncheck to draw/hide the curve points");

	QGridLayout* glay = new QGridLayout();
	glay->setContentsMargins(0, 0, 0, 0);
	glay->addWidget(new QLabel("Curve style"), 0, 0);
	glay->addWidget(&m_line_style, 0, 1);
	glay->addWidget(new QLabel("Baseline"), 1, 0);
	glay->addWidget(&m_baseline, 1, 1);

	m_line_style.addItems(QStringList() << "Lines"
					    << "Sticks"
					    << "Steps");
	m_line_style.setItemData(0, "Connect the points with straight lines", Qt::ToolTipRole);
	m_line_style.setItemData(1, "Draw vertical or horizontal sticks from a baseline", Qt::ToolTipRole);
	m_line_style.setItemData(2, "Connect the points with a step function", Qt::ToolTipRole);
	m_line_style.setToolTip("Select the curve drawing style");
	m_line_style.setCurrentIndex(0);

	m_baseline.setValue(0);
	m_baseline.setToolTip("Baseline value used for the 'Sticks' style.<br>The baseline is also used when drawing the curve background.");

	lay->addWidget(&m_item);
	// lay->addWidget(VipLineWidget::createHLine());

	lay->addWidget(&m_draw_line);
	lay->addLayout(glay);
	// lay->addWidget(&m_line_style);
	//
	// QHBoxLayout * hlay = new QHBoxLayout();
	// hlay->setContentsMargins(0, 0, 0, 0);
	// m_baseline_label.setText("Baseline");
	// hlay->addWidget(&m_baseline_label);
	// hlay->addWidget(&m_baseline);
	// lay->addLayout(hlay);

	lay->addWidget(&m_style);

	lay->addWidget(&m_draw_symbol);
	lay->addWidget(&m_symbol);

	setLayout(lay);

	connect(&m_draw_line, SIGNAL(clicked(bool)), this, SLOT(emitCurveChanged()));
	connect(&m_line_style, SIGNAL(currentIndexChanged(int)), this, SLOT(emitCurveChanged()));
	connect(&m_baseline, SIGNAL(valueChanged(double)), this, SLOT(emitCurveChanged()));
	connect(&m_draw_symbol, SIGNAL(clicked(bool)), this, SLOT(emitCurveChanged()));

	connect(&m_item, SIGNAL(plotItemChanged(VipPlotItem*)), this, SLOT(emitCurveChanged()));
	connect(&m_style, SIGNAL(boxStyleChanged(const VipBoxStyle&)), this, SLOT(emitCurveChanged()));
	connect(&m_symbol, SIGNAL(symbolChanged(const VipSymbol&)), this, SLOT(emitCurveChanged()));
}

void VipPlotCurveWidget::setCurve(VipPlotCurve* curve)
{
	if (!curve)
		return;

	m_curve = curve;

	m_line = curve->pen().color();
	m_back = curve->brush().color();
	m_symbolPen = curve->symbol() ? curve->symbol()->pen().color() : m_line.darker(110);
	m_symbolBack = curve->symbol() ? curve->symbol()->brush().color() : m_line;

	m_draw_line.blockSignals(true);
	m_draw_symbol.blockSignals(true);
	m_item.blockSignals(true);
	m_style.blockSignals(true);
	m_symbol.blockSignals(true);
	m_baseline.blockSignals(true);
	m_line_style.blockSignals(true);

	if (curve->style() != VipPlotCurve::NoCurve && curve->style() < VipPlotCurve::Dots)
		m_line_style.setCurrentIndex(curve->style());
	m_baseline.setValue(curve->baseline());
	// m_baseline.setVisible(curve->style() == VipPlotCurve::Sticks);
	// m_baseline_label.setVisible(curve->style() == VipPlotCurve::Sticks);

	m_draw_line.setChecked(curve->style() != VipPlotCurve::NoCurve);
	m_draw_symbol.setChecked(curve->symbol() && curve->symbolVisible());

	m_item.setPlotItem(curve);
	m_style.setBoxStyle(curve->boxStyle());
	if (curve->symbol())
		m_symbol.setSymbol(*curve->symbol());
	else
		m_symbol.setSymbol(VipSymbol());

	m_item.blockSignals(false);
	m_style.blockSignals(false);
	m_symbol.blockSignals(false);
	m_draw_line.blockSignals(false);
	m_draw_symbol.blockSignals(false);
	m_baseline.blockSignals(false);
	m_line_style.blockSignals(false);
}

VipPlotCurve* VipPlotCurveWidget::getCurve() const
{
	return const_cast<VipPlotCurve*>(m_curve.data());
}

void VipPlotCurveWidget::updateCurve(VipPlotCurve* curve)
{
	if (!curve)
		return;

	m_item.updatePlotItem(curve);
	curve->setStyle(m_draw_line.isChecked() ? (VipPlotCurve::CurveStyle)m_line_style.currentIndex() : VipPlotCurve::NoCurve);
	curve->setBoxStyle(m_style.getBoxStyle());
	applyAsStyleSheet(m_style.getBoxStyle(), curve);
	curve->setSymbol(new VipSymbol(m_symbol.getSymbol()));
	curve->setSymbolVisible(m_draw_symbol.isChecked());
	curve->setBaseline(m_baseline.value());
}

void VipPlotCurveWidget::emitCurveChanged()
{
	if (m_curve) {
		removeStyleSheet(m_curve);
		updateCurve(m_curve);
		Q_EMIT curveChanged(m_curve);
	}
}

VipPlotHistogramWidget::VipPlotHistogramWidget(QWidget* parent)
  : QWidget(parent)
  , m_histo(nullptr)
{
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(&m_item);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&m_style);
	lay->addWidget(VipLineWidget::createHLine());

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(new QLabel("Style"));
	hlay->addWidget(&m_histStyle);
	lay->addLayout(hlay);

	setLayout(lay);

	m_histStyle.addItem("Outline");
	m_histStyle.addItem("Columns");
	m_histStyle.addItem("Lines");

	connect(&m_item, SIGNAL(plotItemChanged(VipPlotItem*)), this, SLOT(emitHistogramChanged()));
	connect(&m_style, SIGNAL(boxStyleChanged(const VipBoxStyle&)), this, SLOT(emitHistogramChanged()));
	connect(&m_histStyle, SIGNAL(currentIndexChanged(int)), this, SLOT(emitHistogramChanged()));
}

void VipPlotHistogramWidget::setHistogram(VipPlotHistogram* curve)
{
	if (!curve)
		return;

	m_histo = curve;

	m_border = curve->pen().color();
	m_back = curve->brush().color();

	m_item.blockSignals(true);
	m_style.blockSignals(true);
	m_histStyle.blockSignals(true);

	m_item.setPlotItem(curve);
	m_style.setBoxStyle(curve->boxStyle());
	m_histStyle.setCurrentIndex(curve->style());

	m_item.blockSignals(false);
	m_style.blockSignals(false);
	m_histStyle.blockSignals(false);
}

VipPlotHistogram* VipPlotHistogramWidget::getHistogram() const
{
	return const_cast<VipPlotHistogram*>(m_histo.data());
}

void VipPlotHistogramWidget::updateHistogram(VipPlotHistogram* curve)
{
	if (!curve)
		return;

	m_item.updatePlotItem(curve);
	curve->setBoxStyle(m_style.getBoxStyle());
	applyAsStyleSheet(m_style.getBoxStyle(), curve);
	curve->setStyle(VipPlotHistogram::HistogramStyle(m_histStyle.currentIndex()));
}

void VipPlotHistogramWidget::emitHistogramChanged()
{
	if (m_histo) {
		removeStyleSheet(m_histo);
		updateHistogram(m_histo);
		Q_EMIT histogramChanged(m_histo);
	}
}

VipPlotAxisWidget::VipPlotAxisWidget(QWidget* parent)
  : QWidget(parent)
{
	QGridLayout* lay = new QGridLayout();

	int row = -1;
	lay->addWidget(&m_title, ++row, 0, 1, 2);
	lay->addWidget(new QLabel("Axis labels properties"), ++row, 0);
	lay->addWidget(&m_labels, row, 1);
	lay->addWidget(&m_labelVisible, ++row, 0, 1, 2);
	lay->addWidget(&m_visible, ++row, 0, 1, 2);
	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);
	lay->addWidget(new QLabel("Maximum value"), ++row, 0);
	lay->addWidget(&m_max, row, 1);
	lay->addWidget(new QLabel("Minimum value"), ++row, 0);
	lay->addWidget(&m_min, row, 1);
	lay->addWidget(new QLabel("Major graduations"), ++row, 0);
	lay->addWidget(&m_maj_grad, row, 1);
	lay->addWidget(new QLabel("Minor graduations"), ++row, 0);
	lay->addWidget(&m_min_grad, row, 1);
	lay->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);
	lay->addWidget(&m_log, ++row, 0, 1, 2);
	lay->addWidget(&m_auto_scale, ++row, 0, 1, 2);
	lay->addWidget(&m_manualExponent, ++row, 0);
	lay->addWidget(&m_exponent, row, 1);
	lay->addWidget(&m_pen, ++row, 0, 1, 2);

	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	m_title.edit()->setToolTip("Axis title.<br>Press ENTER to apply changes.");
	m_visible.setText("Axis visible");
	m_labelVisible.setText("Labels visible");
	m_auto_scale.setText("Automatic scaling");
	m_log.setText("Log10 scale");
	m_pen.setText("Backbone and ticks pen");
	m_pen.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	m_labels.edit()->hide();

	m_manualExponent.setText("Set scale exponent");
	m_manualExponent.setToolTip("<b>Set scale exponent</b><br>If checked, the scale exponent is manually set.<br>Otherwise, the scale exponent is automatically computed.");
	connect(&m_manualExponent, SIGNAL(clicked(bool)), &m_exponent, SLOT(setEnabled(bool)));
	m_exponent.setRange(-300, 300);
	m_exponent.setValue(0);
	m_exponent.setToolTip("Set the common exponent factor to all scale labels");
	m_exponent.setEnabled(false);

	connect(&m_title, SIGNAL(changed(const VipText&)), this, SLOT(emitAxisChanged()));
	connect(&m_labelVisible, SIGNAL(clicked(bool)), this, SLOT(emitAxisChanged()));
	connect(&m_labels, SIGNAL(changed(const VipText&)), this, SLOT(emitAxisChanged()));
	connect(&m_visible, SIGNAL(clicked(bool)), this, SLOT(emitAxisChanged()));
	connect(&m_auto_scale, SIGNAL(clicked(bool)), this, SLOT(emitAxisChanged()));
	connect(&m_max, SIGNAL(valueChanged(double)), this, SLOT(emitAxisChanged()));
	connect(&m_min, SIGNAL(valueChanged(double)), this, SLOT(emitAxisChanged()));
	connect(&m_maj_grad, SIGNAL(valueChanged(int)), this, SLOT(emitAxisChanged()));
	connect(&m_min_grad, SIGNAL(valueChanged(int)), this, SLOT(emitAxisChanged()));
	connect(&m_log, SIGNAL(clicked(bool)), this, SLOT(emitAxisChanged()));
	connect(&m_pen, SIGNAL(penChanged(const QPen&)), this, SLOT(emitAxisChanged()));
	connect(&m_exponent, SIGNAL(valueChanged(int)), this, SLOT(emitAxisChanged()));
	connect(&m_manualExponent, SIGNAL(clicked(bool)), this, SLOT(emitAxisChanged()));
}

VipPlotAxisWidget::~VipPlotAxisWidget()
{
	// bool stop = true;
}

void VipPlotAxisWidget::setAxis(VipAbstractScale* scale)
{
	if (!scale)
		return;

	m_title.blockSignals(true);
	m_labels.blockSignals(true);
	m_visible.blockSignals(true);
	m_labelVisible.blockSignals(true);
	m_auto_scale.blockSignals(true);
	m_max.blockSignals(true);
	m_min.blockSignals(true);
	m_maj_grad.blockSignals(true);
	m_min_grad.blockSignals(true);
	m_log.blockSignals(true);
	m_pen.blockSignals(true);
	m_exponent.blockSignals(true);
	m_manualExponent.blockSignals(true);

	m_scale = scale;
	m_title.setText(scale->title());
	m_labels.setText(VipText("", scale->constScaleDraw()->textStyle()));
	m_visible.setChecked(scale->isVisible());
	m_labelVisible.setChecked(scale->scaleDraw()->hasComponent(VipAbstractScaleDraw::Labels));
	m_auto_scale.setChecked(scale->isAutoScale());
	m_max.setValue(scale->scaleDiv().bounds().maxValue());
	m_min.setValue(scale->scaleDiv().bounds().minValue());
	m_maj_grad.setValue(scale->maxMajor());
	m_min_grad.setValue(scale->maxMinor());
	m_log.setChecked(scale->scaleEngine()->scaleType() == VipScaleEngine::Log10);
	m_pen.setPen(scale->scaleDraw()->componentPen(VipScaleDraw::Backbone));
	m_max.setEnabled(!m_auto_scale.isChecked());
	m_min.setEnabled(!m_auto_scale.isChecked());

	// if there is at least one VipPlotRasterData, do NOT enable the log widget
	QList<VipPlotItem*> items = m_scale->synchronizedPlotItems();
	m_log.setEnabled(true);
	for (int i = 0; i < items.size(); ++i)
		if (qobject_cast<VipPlotRasterData*>(items[i])) {
			m_log.setEnabled(false);
			break;
		}

	m_exponent.setValue(scale->constScaleDraw()->valueToText()->exponent());
	m_manualExponent.setChecked(!scale->constScaleDraw()->valueToText()->automaticExponent());
	m_exponent.setEnabled(m_manualExponent.isChecked());

	m_title.blockSignals(false);
	m_labels.blockSignals(false);
	m_visible.blockSignals(false);
	m_labelVisible.blockSignals(false);
	m_auto_scale.blockSignals(false);
	m_max.blockSignals(false);
	m_min.blockSignals(false);
	m_maj_grad.blockSignals(false);
	m_min_grad.blockSignals(false);
	m_log.blockSignals(false);
	m_pen.blockSignals(false);
	m_exponent.blockSignals(false);
	m_manualExponent.blockSignals(false);
}

VipAbstractScale* VipPlotAxisWidget::getAxis() const
{
	return const_cast<VipAbstractScale*>(m_scale.data());
}

void VipPlotAxisWidget::emitAxisChanged()
{
	if (m_scale) {
		updateAxis(m_scale);

		Q_EMIT axisChanged(m_scale);
	}
}

void VipPlotAxisWidget::updateAxis(VipAbstractScale* scale)
{
	if (!scale)
		return;

	// int custom = scale->customDisplayFlags();
	// if (scale->scaleDraw()->componentPen(VipScaleDraw::Backbone).color() != m_pen.pen().color())
	//	custom |= VIP_CUSTOM_BACKBONE_TICKS_COLOR;
	// if (scale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).font() != m_labels.getText().textStyle().font())
	//	custom |= VIP_CUSTOM_LABEL_FONT;
	// if (scale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).textPen().color() != m_labels.getText().textStyle().textPen().color())
	//	custom |= VIP_CUSTOM_LABEL_PEN;
	// if (scale->title().textStyle().font() != m_title.getText().textStyle().font())
	//	custom |= VIP_CUSTOM_TITLE_FONT;
	// if (scale->title().textStyle().textPen().color() != m_title.getText().textStyle().textPen().color())
	//	custom |= VIP_CUSTOM_TITLE_PEN;
	// scale->setCustomDisplayFlags(custom);

	// TEST: set properties through the scale style sheet
	if (scale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).font() != m_labels.getText().textStyle().font())
		scale->styleSheet().setProperty("VipAbstractScale", "label-font", QVariant::fromValue(m_labels.getText().textStyle().font()));
	if (scale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).textPen() != m_labels.getText().textStyle().textPen())
		scale->styleSheet().setProperty("VipAbstractScale", "label-color", QVariant::fromValue(m_labels.getText().textStyle().textPen().color()));
	if (scale->title().textStyle().font() != m_title.getText().textStyle().font())
		scale->styleSheet().setProperty("VipAbstractScale", "title-font", QVariant::fromValue(m_title.getText().textStyle().font()));
	if (scale->title().textStyle().textPen() != m_title.getText().textStyle().textPen())
		scale->styleSheet().setProperty("VipAbstractScale", "title-color", QVariant::fromValue(m_title.getText().textStyle().textPen().color()));
	if (scale->scaleDraw()->componentPen(VipScaleDraw::Backbone) != m_pen.pen())
		scale->styleSheet().setProperty("VipAbstractScale", "pen", QVariant::fromValue(m_pen.pen()));
	scale->updateStyleSheetString();

	scale->scaleDraw()->valueToText()->setExponent(m_exponent.value());
	scale->scaleDraw()->valueToText()->setAutomaticExponent(!m_manualExponent.isChecked());
	scale->setScaleDiv(scale->scaleDiv(), true); // reset the scale div for exponent parameters

	scale->setTitle(m_title.getText());
	scale->scaleDraw()->setTextStyle(m_labels.getText().textStyle());
	scale->setVisible(m_visible.isChecked());
	scale->scaleDraw()->enableComponent(VipAbstractScaleDraw::Labels, m_labelVisible.isChecked());
	if (!m_auto_scale.isChecked()) {
		scale->setScale(m_min.value(), m_max.value());
	}
	scale->setMaxMajor(m_maj_grad.value());
	scale->setMaxMinor(m_min_grad.value());
	bool need_autoscale = false;
	if (m_log.isChecked() && scale->scaleEngine()->scaleType() == VipScaleEngine::Linear) {
		scale->setScaleEngine(new VipLog10ScaleEngine());
		need_autoscale = true;
	}
	else if (!m_log.isChecked() && scale->scaleEngine()->scaleType() == VipScaleEngine::Log10) {
		scale->setScaleEngine(new VipLinearScaleEngine());
		need_autoscale = true;
	}
	scale->setAutoScale(m_auto_scale.isChecked() || need_autoscale);

	scale->scaleDraw()->setComponentPen(VipScaleDraw::Backbone | VipScaleDraw::Ticks, m_pen.pen());

	m_max.setEnabled(!m_auto_scale.isChecked());
	m_min.setEnabled(!m_auto_scale.isChecked());

	scale->scaleDraw()->invalidateCache();
	scale->computeScaleDiv();
	// NEWPLOT
	// scale->recomputeGeometry();

	if (need_autoscale) {
		if (VipPlotPlayer* pl = qobject_cast<VipPlotPlayer*>(VipPlotPlayer::findAbstractPlayer(scale))) {
			pl->setAutoScale(false);
			pl->setAutoScale(true);
		}
	}
}

#include "VipAxisColorMap.h"
#include "VipColorMap.h"
#include "VipPainter.h"
#include "VipSliderGrip.h"
#include "VipStandardWidgets.h"
#include <QCheckBox>
#include <QGridLayout>

class VipColorScaleWidget::PrivateData
{
public:
	PrivateData()
	  : thisColorScale(nullptr)
	  , colorScale(nullptr)
	{
	}

	VipTextWidget title;
	VipTextWidget labels;
	QComboBox colorMaps;
	QCheckBox externalColor;
	VipColorWidget externalColorChoice;
	VipDoubleEdit maximum;
	VipDoubleEdit minimum;
	VipDoubleEdit gripMaximum;
	VipDoubleEdit gripMinimum;
	QSpinBox maxMajor;
	QSpinBox maxMinor;

	// interval validity
	QCheckBox hasMax;
	QCheckBox hasMin;
	VipDoubleEdit minValue;
	VipDoubleEdit maxValue;

	QCheckBox manualExponent;
	QSpinBox exponent;

	QCheckBox visibleScale;
	QCheckBox autoScale;
	QCheckBox logScale;
	QCheckBox applyAll;
	VipScaleWidget scaleWidget;
	VipAxisColorMap* thisColorScale;

	QPointer<VipAxisColorMap> colorScale;
};

VipColorScaleWidget::VipColorScaleWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	QGridLayout* grid = new QGridLayout();

	int row = -1;
	grid->addWidget(new QLabel("Title"), ++row, 0);
	grid->addWidget(&d_data->title, row, 1);

	grid->addWidget(new QLabel("Color map labels"), ++row, 0);
	grid->addWidget(&d_data->labels, row, 1);
	d_data->labels.edit()->hide();

	grid->addWidget(new QLabel("Color map"), ++row, 0);
	grid->addWidget(&d_data->colorMaps, row, 1);

	grid->addWidget(&d_data->externalColor, ++row, 0);
	grid->addWidget(&d_data->externalColorChoice, row, 1);

	grid->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	grid->addWidget(new QLabel("Scale max"), ++row, 0);
	grid->addWidget(&d_data->maximum, row, 1);

	grid->addWidget(new QLabel("Scale min"), ++row, 0);
	grid->addWidget(&d_data->minimum, row, 1);

	grid->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	grid->addWidget(new QLabel("Grip max"), ++row, 0);
	grid->addWidget(&d_data->gripMaximum, row, 1);

	grid->addWidget(new QLabel("Grip min"), ++row, 0);
	grid->addWidget(&d_data->gripMinimum, row, 1);

	grid->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	grid->addWidget(&d_data->hasMax, ++row, 0);
	grid->addWidget(&d_data->maxValue, row, 1);

	grid->addWidget(&d_data->hasMin, ++row, 0);
	grid->addWidget(&d_data->minValue, row, 1);

	grid->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	grid->addWidget(new QLabel("Major graduations"), ++row, 0);
	grid->addWidget(&d_data->maxMajor, row, 1);

	grid->addWidget(new QLabel("Minor graduations"), ++row, 0);
	grid->addWidget(&d_data->maxMinor, row, 1);

	grid->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	grid->addWidget(&d_data->visibleScale, ++row, 0, 1, 2);
	grid->addWidget(&d_data->autoScale, ++row, 0, 1, 2);
	grid->addWidget(&d_data->logScale, ++row, 0, 1, 2);
	grid->addWidget(&d_data->manualExponent, ++row, 0);
	grid->addWidget(&d_data->exponent, row, 1);

	grid->addWidget(VipLineWidget::createHLine(), ++row, 0, 1, 2);

	grid->addWidget(&d_data->applyAll, ++row, 0, 1, 2);

	grid->addWidget(&d_data->scaleWidget, 0, 2, ++row, 1);

	// add a stretch
	QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	grid->addWidget(empty, ++row, 0, 1, 2);

	setLayout(grid);

	d_data->externalColor.setText("Fixed color outside");
	d_data->thisColorScale = new VipAxisColorMap(VipAxisColorMap::Right);
	d_data->thisColorScale->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	d_data->thisColorScale->setRenderHints(QPainter::TextAntialiasing);
	d_data->thisColorScale->setColorBarEnabled(true);
	d_data->thisColorScale->setBorderDist(5, 5);
	d_data->thisColorScale->setExpandToCorners(true);
	d_data->thisColorScale->setColorMap(VipInterval(0, 100), VipLinearColorMap::createColorMap(VipLinearColorMap::Jet));
	d_data->thisColorScale->setUseBorderDistHintForLayout(true);
	d_data->thisColorScale->setIgnoreStyleSheet(true);
	d_data->scaleWidget.setScale(d_data->thisColorScale);
	d_data->scaleWidget.setMaximumWidth(100);
	d_data->scaleWidget.setStyleSheet("background-color:transparent;");

	// d_data-> labels.edit()->hide();
	d_data->labels.edit()->setText("Label example");
	d_data->visibleScale.setText("Color scale visible");
	d_data->autoScale.setText("Automatic scaling");
	d_data->logScale.setText("Log10 scale");
	d_data->logScale.hide();
	d_data->applyAll.setText("Apply to all color scales");

	d_data->hasMin.setText("Clamp min");
	d_data->hasMax.setText("Clamp max");
	d_data->hasMin.setToolTip("Set the minimum value under which values are ignored when computing the automatic color scale");
	d_data->hasMax.setToolTip("Set the maximum value above which values are ignored when computing the automatic color scale");
	d_data->minValue.setToolTip(d_data->hasMin.toolTip());
	d_data->maxValue.setToolTip(d_data->hasMax.toolTip());

	d_data->manualExponent.setText("Set scale exponent");
	d_data->manualExponent.setToolTip("<b>Set scale exponent</b><br>If checked, the scale exponent is manually set.<br>Otherwise, the scale exponent is automatically computed.");
	connect(&d_data->manualExponent, SIGNAL(clicked(bool)), &d_data->exponent, SLOT(setEnabled(bool)));

	d_data->exponent.setRange(-300, 300);
	d_data->exponent.setValue(0);
	d_data->exponent.setToolTip("Set the common exponent factor to all scale labels");
	d_data->exponent.setEnabled(false);

	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Autumn, QSize(20, 20)), "Autumn");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Bone, QSize(20, 20)), "Bone");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::BuRd, QSize(20, 20)), "BuRd");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Cool, QSize(20, 20)), "Cool");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Copper, QSize(20, 20)), "Copper");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Gray, QSize(20, 20)), "Gray");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Hot, QSize(20, 20)), "Hot");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Hsv, QSize(20, 20)), "Hsv");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Jet, QSize(20, 20)), "Jet");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Fusion, QSize(20, 20)), "Fusion");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Pink, QSize(20, 20)), "Pink");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Rainbow, QSize(20, 20)), "Rainbow");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Spring, QSize(20, 20)), "Spring");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Summer, QSize(20, 20)), "Summer");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Sunset, QSize(20, 20)), "Sunset");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Viridis, QSize(20, 20)), "Viridis");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::White, QSize(20, 20)), "White");
	d_data->colorMaps.addItem(colorMapPixmap(VipLinearColorMap::Winter, QSize(20, 20)), "Winter");
	d_data->colorMaps.setCurrentIndex(VipLinearColorMap::Jet);

	connect(&d_data->externalColor, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->externalColorChoice, SIGNAL(colorChanged(const QColor&)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->title, SIGNAL(changed(const VipText&)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->labels, SIGNAL(changed(const VipText&)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->colorMaps, SIGNAL(currentIndexChanged(int)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->maximum, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->minimum, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->gripMaximum, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->gripMinimum, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->maxMajor, SIGNAL(valueChanged(int)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->maxMinor, SIGNAL(valueChanged(int)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->visibleScale, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->autoScale, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->logScale, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->applyAll, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(d_data->thisColorScale, SIGNAL(scaleDivChanged(bool)), this, SLOT(emitColorScaleChanged()));
	connect(d_data->thisColorScale, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));

	connect(&d_data->hasMin, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->hasMax, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->minValue, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->maxValue, SIGNAL(valueChanged(double)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->exponent, SIGNAL(valueChanged(int)), this, SLOT(emitColorScaleChanged()));
	connect(&d_data->manualExponent, SIGNAL(clicked(bool)), this, SLOT(emitColorScaleChanged()));

	this->setMaximumHeight(470);
	this->setMaximumWidth(450);
}

VipColorScaleWidget::~VipColorScaleWidget()
{
}

QPixmap VipColorScaleWidget::colorMapPixmap(int color_map, const QSize& size, const QPen& pen)
{
	VipLinearColorMap* map = VipLinearColorMap::createColorMap(VipLinearColorMap::StandardColorMap(color_map));
	if (map) {
		QPixmap pix(size);
		QPainter p(&pix);
		VipScaleMap sc;
		sc.setScaleInterval(0, size.height());
		VipPainter::drawColorBar(&p, *map, VipInterval(0, size.height()), sc, Qt::Vertical, QRectF(0, 0, size.width(), size.height()));
		if (pen.style() != Qt::NoPen) {
			p.setPen(pen);
			p.drawRect(QRect(0, 0, size.width() - 1, size.height() - 1));
		}
		delete map;
		return pix;
	}
	return QPixmap();
}

VipAxisColorMap* VipColorScaleWidget::colorScale() const
{
	return d_data->colorScale;
}

void VipColorScaleWidget::updateColorScale()
{
	setColorScale(d_data->colorScale);
}

void VipColorScaleWidget::setColorScale(VipAxisColorMap* scale)
{
	if (d_data->colorScale) {
		disconnect(d_data->colorScale, SIGNAL(scaleDivChanged(bool)), this, SLOT(updateColorScale()));
		disconnect(d_data->colorScale, SIGNAL(valueChanged(double)), this, SLOT(updateColorScale()));
	}

	d_data->colorScale = scale;
	if (!scale)
		return;

	connect(d_data->colorScale, SIGNAL(scaleDivChanged(bool)), this, SLOT(updateColorScale()));
	connect(d_data->colorScale, SIGNAL(valueChanged(double)), this, SLOT(updateColorScale()));

	d_data->title.blockSignals(true);
	d_data->labels.blockSignals(true);
	d_data->colorMaps.blockSignals(true);
	d_data->externalColor.blockSignals(true);
	d_data->externalColorChoice.blockSignals(true);
	d_data->maximum.blockSignals(true);
	d_data->minimum.blockSignals(true);
	d_data->gripMaximum.blockSignals(true);
	d_data->gripMinimum.blockSignals(true);
	d_data->maxMajor.blockSignals(true);
	d_data->maxMinor.blockSignals(true);
	d_data->visibleScale.blockSignals(true);
	d_data->autoScale.blockSignals(true);
	d_data->logScale.blockSignals(true);
	d_data->applyAll.blockSignals(true);
	d_data->scaleWidget.blockSignals(true);
	d_data->thisColorScale->blockSignals(true);

	d_data->hasMin.blockSignals(true);
	d_data->hasMax.blockSignals(true);
	d_data->minValue.blockSignals(true);
	d_data->maxValue.blockSignals(true);
	d_data->exponent.blockSignals(true);
	d_data->manualExponent.blockSignals(true);

	d_data->colorScale = scale;
	d_data->title.setText(scale->title());
	d_data->labels.setText(VipText("", scale->scaleDraw()->textStyle()));
	int index = static_cast<VipLinearColorMap*>(scale->colorMap())->type();
	if (index >= 0) {
		d_data->colorMaps.setCurrentIndex(index);
		d_data->thisColorScale->setColorMap(scale->gripInterval(), VipLinearColorMap::createColorMap(VipLinearColorMap::StandardColorMap(index)));
	}
	d_data->externalColor.setChecked(scale->colorMap()->externalValue() == VipLinearColorMap::ColorFixed);
	QRgb ext = scale->colorMap()->externalColor();
	d_data->externalColorChoice.setColor(QColor(qRed(ext), qGreen(ext), qBlue(ext), qAlpha(ext)));
	d_data->maximum.setValue(scale->scaleDiv().bounds().normalized().maxValue());
	d_data->minimum.setValue(scale->scaleDiv().bounds().normalized().minValue());
	d_data->gripMaximum.setValue(scale->gripInterval().normalized().maxValue());
	d_data->gripMinimum.setValue(scale->gripInterval().normalized().minValue());
	d_data->maxMajor.setValue(scale->maxMajor());
	d_data->maxMinor.setValue(scale->maxMinor());
	d_data->thisColorScale->setScaleDiv(scale->scaleDiv());
	d_data->thisColorScale->setTitle(scale->title());
	d_data->thisColorScale->grip1()->setValue(d_data->gripMinimum.value());
	d_data->thisColorScale->grip2()->setValue(d_data->gripMaximum.value());
	d_data->thisColorScale->scaleDraw()->setTicksPosition(scale->scaleDraw()->ticksPosition());
	d_data->thisColorScale->setRenderHints(scale->renderHints());
	d_data->thisColorScale->setColorBarEnabled(scale->isColorBarEnabled());
	d_data->thisColorScale->setColorBarWidth(scale->colorBarWidth());
	d_data->thisColorScale->setMaxMajor(scale->maxMajor());
	d_data->thisColorScale->setMaxMinor(scale->maxMinor());
	d_data->thisColorScale->scaleDraw()->setTextStyle(scale->scaleDraw()->textStyle());
	d_data->thisColorScale->scaleDraw()->setComponentPen(VipScaleDraw::Backbone, scale->scaleDraw()->componentPen(VipScaleDraw::Backbone));
	d_data->thisColorScale->scaleDraw()->setComponentPen(VipScaleDraw::Ticks, scale->scaleDraw()->componentPen(VipScaleDraw::Ticks));
	if (scale->transformation())
		d_data->thisColorScale->setTransformation(scale->transformation()->copy());
	double min, max;
	scale->getBorderDistHint(min, max);
	d_data->thisColorScale->setBorderDist(min, max);
	d_data->thisColorScale->setExpandToCorners(scale->expandToCorners());
	d_data->autoScale.setChecked(scale->isAutoScale());
	d_data->visibleScale.setChecked(scale->isVisible());
	d_data->logScale.setChecked(scale->scaleEngine()->scaleType() == VipScaleEngine::Log10);

	d_data->exponent.setValue(scale->constScaleDraw()->valueToText()->exponent());
	d_data->manualExponent.setChecked(!scale->constScaleDraw()->valueToText()->automaticExponent());
	d_data->exponent.setEnabled(d_data->manualExponent.isChecked());

	d_data->gripMaximum.setEnabled(!scale->isAutoScale());
	d_data->gripMinimum.setEnabled(!scale->isAutoScale());
	d_data->thisColorScale->grip1()->setVisible(!scale->isAutoScale());
	d_data->thisColorScale->grip2()->setVisible(!scale->isAutoScale());

	d_data->hasMin.setChecked(scale->hasAutoScaleMin());
	d_data->hasMax.setChecked(scale->hasAutoScaleMax());
	d_data->minValue.setValue(scale->autoScaleMin());
	d_data->maxValue.setValue(scale->autoScaleMax());

	d_data->title.blockSignals(false);
	d_data->labels.blockSignals(false);
	d_data->colorMaps.blockSignals(false);
	d_data->externalColor.blockSignals(false);
	d_data->externalColorChoice.blockSignals(false);
	d_data->maximum.blockSignals(false);
	d_data->minimum.blockSignals(false);
	d_data->gripMaximum.blockSignals(false);
	d_data->gripMinimum.blockSignals(false);
	d_data->maxMajor.blockSignals(false);
	d_data->maxMinor.blockSignals(false);
	d_data->visibleScale.blockSignals(false);
	d_data->autoScale.blockSignals(false);
	d_data->logScale.blockSignals(false);
	d_data->applyAll.blockSignals(false);
	d_data->scaleWidget.blockSignals(false);
	d_data->thisColorScale->blockSignals(false);
	d_data->hasMin.blockSignals(false);
	d_data->hasMax.blockSignals(false);
	d_data->minValue.blockSignals(false);
	d_data->maxValue.blockSignals(false);
	d_data->exponent.blockSignals(false);
	d_data->manualExponent.blockSignals(false);

	d_data->scaleWidget.setMinimumWidth(d_data->thisColorScale->minimumLengthHint() + 15);
	d_data->scaleWidget.setMaximumWidth(d_data->scaleWidget.minimumWidth());
}

void VipColorScaleWidget::emitColorScaleChanged()
{
	// update this color scale scale div, grips and color map that can be modified by the VipDoubleEdit widgets and the VipComboBox
	d_data->thisColorScale->blockSignals(true);
	d_data->thisColorScale->setScale(qMin(d_data->minimum.value(), d_data->maximum.value()), qMax(d_data->minimum.value(), d_data->maximum.value()));
	if (sender() != d_data->thisColorScale) {
		d_data->thisColorScale->grip1()->setValue(d_data->gripMinimum.value());
		d_data->thisColorScale->grip2()->setValue(d_data->gripMaximum.value());
	}

	d_data->thisColorScale->setTitle(d_data->title.getText());
	d_data->thisColorScale->colorMap()->setExternalValue(d_data->externalColor.isChecked() ? VipColorMap::ColorFixed : VipColorMap::ColorBounds, d_data->externalColorChoice.color().rgba());

	if (static_cast<VipLinearColorMap*>(d_data->thisColorScale->colorMap())->type() != d_data->colorMaps.currentIndex())
		d_data->thisColorScale->setColorMap(d_data->thisColorScale->gripInterval(), VipLinearColorMap::createColorMap(VipLinearColorMap::StandardColorMap(d_data->colorMaps.currentIndex())));

	d_data->thisColorScale->setMaxMajor(d_data->maxMajor.value());
	d_data->thisColorScale->setMaxMinor(d_data->maxMinor.value());
	d_data->thisColorScale->scaleDraw()->setTextStyle(d_data->labels.getText().textStyle());
	d_data->thisColorScale->scaleDraw()->valueToText()->setExponent(d_data->exponent.value());
	d_data->thisColorScale->scaleDraw()->valueToText()->setAutomaticExponent(!d_data->manualExponent.isChecked());
	d_data->thisColorScale->setScaleDiv(d_data->thisColorScale->scaleDiv(), true);

	// change scale engine
	if (d_data->logScale.isChecked() && d_data->thisColorScale->scaleEngine()->scaleType() == VipScaleEngine::Linear)
		d_data->thisColorScale->setScaleEngine(new VipLog10ScaleEngine());
	else if (!d_data->logScale.isChecked() && d_data->thisColorScale->scaleEngine()->scaleType() == VipScaleEngine::Log10)
		d_data->thisColorScale->setScaleEngine(new VipLinearScaleEngine());

	d_data->thisColorScale->blockSignals(false);
	d_data->thisColorScale->scaleDraw()->invalidateCache();
	d_data->thisColorScale->computeScaleDiv();
	// NEWPLOT
	// d_data-> thisColorScale->recomputeGeometry();

	d_data->gripMaximum.setEnabled(!d_data->autoScale.isChecked());
	d_data->gripMinimum.setEnabled(!d_data->autoScale.isChecked());
	d_data->thisColorScale->grip1()->setVisible(!d_data->autoScale.isChecked());
	d_data->thisColorScale->grip2()->setVisible(!d_data->autoScale.isChecked());

	// compute the list of players
	// QList<VipVideoPlayer*> players;
	//  if(d_data->applyAll.isChecked() && displayArea()->currentDisplayPlayerArea())
	//  players = displayArea()->currentDisplayPlayerArea()->findChildren<VipVideoPlayer*>();
	//  else if(VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(currentPlayer()))
	//  players << pl;

	QList<VipAxisColorMap*> scales;
	if (d_data->applyAll.isChecked()) {
		QList<VipAbstractPlayer*> players = VipFindChidren::findChildren<VipAbstractPlayer*>();
		for (int i = 0; i < players.size(); ++i) {
			if (players[i]->plotWidget2D())
				scales.append(players[i]->plotWidget2D()->area()->findItems<VipAxisColorMap*>());
		}
	}
	else if (d_data->colorScale)
		scales << d_data->colorScale;

	// apply modifications
	for (int i = 0; i < scales.size(); ++i) {
		// VipVideoPlayer * player = players[i];
		VipAxisColorMap* scale = scales[i]; // player->viewer()->area()->colorMap();

		if (scale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).font() != d_data->thisColorScale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).font())
			scale->styleSheet().setProperty("VipAbstractScale", "label-font", QVariant::fromValue(d_data->thisColorScale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).font()));
		if (scale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).textPen() != d_data->thisColorScale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).textPen())
			scale->styleSheet().setProperty(
			  "VipAbstractScale", "label-color", QVariant::fromValue(d_data->thisColorScale->scaleDraw()->textStyle(VipScaleDiv::MajorTick).textPen().color()));
		if (scale->title().textStyle().font() != d_data->thisColorScale->title().textStyle().font())
			scale->styleSheet().setProperty("VipAbstractScale", "title-font", QVariant::fromValue(d_data->thisColorScale->title().textStyle().font()));
		if (scale->title().textStyle().textPen() != d_data->thisColorScale->title().textStyle().textPen())
			scale->styleSheet().setProperty("VipAbstractScale", "title-color", QVariant::fromValue(d_data->thisColorScale->title().textStyle().textPen().color()));
		if (scale->scaleDraw()->componentPen(VipScaleDraw::Backbone) != d_data->thisColorScale->scaleDraw()->componentPen(VipScaleDraw::Backbone))
			scale->styleSheet().setProperty("VipAbstractScale", "pen", QVariant::fromValue(d_data->thisColorScale->scaleDraw()->componentPen(VipScaleDraw::Backbone)));
		scale->updateStyleSheetString();

		// update title
		if (scale == d_data->colorScale)
			scale->setTitle(d_data->title.getText());

		// update auto scaling
		scale->setAutoScale(d_data->autoScale.isChecked());
		if (scale == d_data->colorScale)
			scale->setVisible(d_data->visibleScale.isChecked());

		// update scale div and grip values
		if (!d_data->autoScale.isChecked()) {
			if (scale->scaleDiv() != d_data->thisColorScale->scaleDiv())
				scale->setScaleDiv(d_data->thisColorScale->scaleDiv());
			scale->grip1()->setValue(d_data->thisColorScale->grip1()->value());
			scale->grip2()->setValue(d_data->thisColorScale->grip2()->value());
		}

		if (scale == d_data->colorScale)
			disconnect(scale, SIGNAL(scaleDivChanged(bool)), this, SLOT(updateColorScale()));

		scale->setMaxMajor(d_data->maxMajor.value());
		scale->setMaxMinor(d_data->maxMinor.value());
		scale->scaleDraw()->setTextStyle(d_data->labels.getText().textStyle());

		scale->scaleDraw()->valueToText()->setExponent(d_data->exponent.value());
		scale->scaleDraw()->valueToText()->setAutomaticExponent(!d_data->manualExponent.isChecked());
		scale->setScaleDiv(scale->scaleDiv(), true);

		// change scale engine
		if (d_data->logScale.isChecked() && scale->scaleEngine()->scaleType() == VipScaleEngine::Linear)
			scale->setScaleEngine(new VipLog10ScaleEngine());
		else if (!d_data->logScale.isChecked() && scale->scaleEngine()->scaleType() == VipScaleEngine::Log10)
			scale->setScaleEngine(new VipLinearScaleEngine());

		if (scale == d_data->colorScale)
			connect(scale, SIGNAL(scaleDivChanged(bool)), this, SLOT(updateColorScale()));

		// update color map
		if (static_cast<VipLinearColorMap*>(scale->colorMap())->type() != d_data->colorMaps.currentIndex())
			scale->setColorMap(d_data->thisColorScale->gripInterval(), VipLinearColorMap::createColorMap(VipLinearColorMap::StandardColorMap(d_data->colorMaps.currentIndex())));

		// update external color
		scale->colorMap()->setExternalValue(d_data->externalColor.isChecked() ? VipColorMap::ColorFixed : VipColorMap::ColorBounds, d_data->externalColorChoice.color().rgba());

		// update min/max valid values
		if (d_data->hasMin.isChecked()) {
			scale->setAutoScaleMin(d_data->minValue.value());
			scale->setHasAutoScaleMin(true);
		}
		else {
			scale->setAutoScaleMin(vipNan());
			scale->setHasAutoScaleMin(false);
		}
		if (d_data->hasMax.isChecked()) {
			scale->setAutoScaleMax(d_data->maxValue.value());
			scale->setHasAutoScaleMax(true);
		}
		else {
			scale->setAutoScaleMax(vipNan());
			scale->setHasAutoScaleMax(false);
		}

		scale->scaleDraw()->invalidateCache();
		scale->computeScaleDiv();
		// NEWPLOT
		// scale->recomputeGeometry();
	}

	d_data->scaleWidget.setMinimumWidth(d_data->thisColorScale->minimumLengthHint() + 15);
	d_data->scaleWidget.setMaximumWidth(d_data->scaleWidget.minimumWidth());

	Q_EMIT colorScaleChanged(d_data->colorScale);
}

QMenu* VipColorScaleButton::generateColorScaleMenu()
{
	QMenu* menu = new QMenu();
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Autumn, QSize(20, 16), QPen()), "Autumn");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Bone, QSize(20, 16), QPen()), "Bone");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::BuRd, QSize(20, 16), QPen()), "BuRd");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Cool, QSize(20, 16), QPen()), "Cool");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Copper, QSize(20, 16), QPen()), "Copper");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Gray, QSize(20, 16), QPen()), "Gray");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Hot, QSize(20, 16), QPen()), "Hot");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Hsv, QSize(20, 16), QPen()), "Hsv");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Jet, QSize(20, 16), QPen()), "Jet");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Fusion, QSize(20, 16), QPen()), "Fusion");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Pink, QSize(20, 16), QPen()), "Pink");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Rainbow, QSize(20, 16), QPen()), "Rainbow");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Spring, QSize(20, 16), QPen()), "Spring");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Summer, QSize(20, 16), QPen()), "Summer");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Sunset, QSize(20, 16), QPen()), "Sunset");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Viridis, QSize(20, 16), QPen()), "Viridis");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::White, QSize(20, 16), QPen()), "White");
	menu->addAction(VipColorScaleWidget::colorMapPixmap(VipLinearColorMap::Winter, QSize(20, 16), QPen()), "Winter");

	for (int i = 0; i < menu->actions().size(); ++i)
		menu->actions()[i]->setProperty("colorMap", i);
	return menu;
}
VipColorScaleButton::VipColorScaleButton(QWidget* parent)
  : QToolButton(parent)
  , m_colorPalette(-1)
{
	setToolTip("Change color palette");
	QMenu* menu = generateColorScaleMenu();
	setMenu(menu);
	setPopupMode(QToolButton::InstantPopup);
	connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(menuTriggered(QAction*)));
	setColorPalette(VipLinearColorMap::Jet);
}

void VipColorScaleButton::setColorPalette(int color_palette)
{
	if (m_colorPalette != color_palette && color_palette >= 0 && color_palette < menu()->actions().size()) {
		m_colorPalette = color_palette;

		QPixmap pix = VipColorScaleWidget::colorMapPixmap(color_palette, QSize(20, 16), QPen());
		this->setIcon(pix);
		this->setToolTip("Change color palette (current: " + menu()->actions()[color_palette]->text() + ")");

		Q_EMIT colorPaletteChanged(color_palette);
	}
}
int VipColorScaleButton::colorPalette() const
{
	return m_colorPalette;
}

void VipColorScaleButton::menuTriggered(QAction* act)
{
	setColorPalette(menu()->actions().indexOf(act));
}

class VipAbstractPlayerWidget::PrivateData
{
public:
	bool inDelayedSelection{ false };
	QPointer<VipAbstractPlayer> player;
	QComboBox selection;

	QList<QPointer<QGraphicsObject>> items;
	QPointer<QGraphicsObject> selected;

	QGridLayout* grid;
};

VipAbstractPlayerWidget::VipAbstractPlayerWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->grid = new QGridLayout();
	d_data->grid->addWidget(new QLabel("Available items"), 0, 0);
	d_data->grid->addWidget(&d_data->selection, 0, 1);
	d_data->grid->addWidget(VipLineWidget::createHLine(), 1, 0, 1, 2);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(d_data->grid);
	vlay->addStretch(2);
	setLayout(vlay);

	d_data->selection.setEditable(true);
	// d_data->selection.setSizeAdjustPolicy(QComboBox::AdjustToContentsOnFirstShow);
	d_data->selection.setMaximumWidth(200);
	d_data->selection.setToolTip("Select an to edit among the list of all available items in the current player");

	// TEST: hide item selection for more visibility
	// d_data->selection.setVisible(false);

	connect(&d_data->selection, SIGNAL(currentIndexChanged(int)), this, SLOT(itemChoiceChanged()));
}

VipAbstractPlayerWidget::~VipAbstractPlayerWidget()
{
}

VipAbstractPlayer* VipAbstractPlayerWidget::abstractPlayer() const
{
	return const_cast<VipAbstractPlayer*>(d_data->player.data());
}

void VipAbstractPlayerWidget::setEditor(QWidget* editor)
{
	// delete the previous editor if needed
	QLayoutItem* item = d_data->grid->itemAtPosition(2, 0);
	if (item && item->widget()) {
		d_data->grid->removeItem(item);
		delete item->widget();
		delete item;
		// item->widget()->deleteLater();
	}

	// add the new one
	if (editor) {
		d_data->grid->addWidget(editor, 2, 0, 1, 2);
		editor->show();
	}
}

void VipAbstractPlayerWidget::showEvent(QShowEvent*)
{
	setPlayerInternal();
}

void VipAbstractPlayerWidget::hideEvent(QHideEvent*)
{
	// Delete current editor,
	// but make sure it is not hidden because of a dialog box triggered by the current editor
	if (!QApplication::modalWindow())
		setEditor(nullptr);
}

void VipAbstractPlayerWidget::setPlayerInternal()
{
	if (!d_data->player)
		return;
	d_data->selection.blockSignals(true);
	d_data->selection.clear();
	d_data->items.clear();

	VipAbstractPlayer* player = d_data->player;

	// retrieve all editable QGraphicsItem in the player, add the ones with a title or name to the combo box
	QList<QGraphicsObject*> items;
	if (d_data->player->plotWidget2D())
		items = d_data->player->plotWidget2D()->area()->findItems<QGraphicsObject*>();

	// find all editable items
	for (int i = 0; i < items.size(); ++i) {
		// we do NOT want to edit VipPlotShape objects, this is for the ROI panel
		if (qobject_cast<VipPlotShape*>(items[i]) || qobject_cast<VipResizeItem*>(items[i]))
			continue;

		if (vipHasObjectEditor(QVariant::fromValue(items[i]))) {
			QString name = vipItemName(items[i]);

			// only add items with a name to the combo box
			if (!name.isEmpty()) {
				d_data->selection.addItem(name);
				d_data->items.append(items[i]);
			}

			if (items[i]->isSelected()) {
				// std::cout<<"selected "<<items[i]->metaObject()->className()<<std::endl;
				d_data->selected = items[i];
			}
		}
	}

	d_data->selection.blockSignals(false);

	// if no editable items, return
	if (d_data->items.size() == 0)
		return;

	// if no selection, take the default one
	if (!d_data->selected)
		if (VipPlayer2D* p = qobject_cast<VipPlayer2D*>(player))
			d_data->selected = p->defaultEditableObject();
	// if no selection, take the last one
	if (!d_data->selected)
		d_data->selected = d_data->items.back();

	// set the selection combo box index
	int index = d_data->items.indexOf(d_data->selected);
	d_data->selection.blockSignals(true);
	if (index >= 0)
		d_data->selection.setCurrentIndex(index);
	else {
		// d_data->selected = d_data->items.back();
		// d_data->selection.setCurrentIndex(d_data->items.size()-1);
		d_data->selection.setCurrentText(vipComprehensiveName(d_data->selected));
	}

	// set the selection combo box tool tip
	// if (d_data->selected)
	//  d_data->selection.setToolTip("<b>Current selected item: </b>" +
	//  vipComprehensiveName(d_data->selected) +
	//  "<br><b>Type: </b>" + vipSplitClassname(d_data->selected->metaObject()->className()));
	//  else
	//  d_data->selection.setToolTip("Select a plot item to edit among the list of all available items in the current player");

	d_data->selection.blockSignals(false);
}

#include <iostream>
void VipAbstractPlayerWidget::setAbstractPlayer(VipAbstractPlayer* player)
{
	VipAbstractPlayer* previous_player = d_data->player;
	QGraphicsObject* previous_selected = d_data->selected;

	if (d_data->player && d_data->player->plotWidget2D())
		disconnect(d_data->player->plotWidget2D()->scene(), SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));

	if (!player)
		return;

	d_data->player = player;

	if (d_data->player && d_data->player->plotWidget2D())
		connect(d_data->player->plotWidget2D()->scene(), SIGNAL(selectionChanged()), this, SLOT(selectionChanged()));

	if (!isHidden())
		setPlayerInternal();

	// find editor
	QLayoutItem* item = d_data->grid->itemAtPosition(2, 0);
	bool has_editor = (item && item->widget());

	if (d_data->player != previous_player || d_data->selected != previous_selected || !has_editor) {
		if (d_data->selected != previous_selected)
			Q_EMIT itemChanged(d_data->selected);

		// add the editor
		if (!isHidden()) {
			QWidget* editor = vipObjectEditor(QVariant::fromValue(d_data->selected.data()));
			setEditor(editor);
		}

		if (d_data->player != previous_player)
			Q_EMIT abstractPlayerChanged(d_data->player);
	}
}

void VipAbstractPlayerWidget::itemChoiceChanged()
{
	int index = d_data->selection.currentIndex();
	if (index < d_data->items.size()) {
		d_data->selected = d_data->items[index];

		// add the editor
		QWidget* editor = vipObjectEditor(QVariant::fromValue(d_data->selected.data()));
		setEditor(editor);

		Q_EMIT itemChanged(d_data->selected);
	}
}

void VipAbstractPlayerWidget::delayedSelectionChanged()
{
	d_data->inDelayedSelection = false;
	setAbstractPlayer(d_data->player);
}

void VipAbstractPlayerWidget::selectionChanged()
{
	if (qobject_cast<VipAbstractPlayer*>(d_data->player.data())) {
		// setAbstractPlayer(d_data->player);
		// TEST
		if (!d_data->inDelayedSelection) {
			d_data->inDelayedSelection = true;
			QMetaObject::invokeMethod(this, "delayedSelectionChanged", Qt::QueuedConnection);
		}
	}
}

class VipDefaultPlotAreaSettings::PrivateData
{
public:
	QCheckBox m_leftAxis;
	QCheckBox m_rightAxis;
	QCheckBox m_topAxis;
	QCheckBox m_bottomAxis;
	QCheckBox m_majorGrid;
	QCheckBox m_minorGrid;
	VipPenButton m_majorPen;
	VipPenButton m_minorPen;
	VipPenButton m_backgroundBrush;
	QCheckBox m_drawAntialize;
	VipPlotCurveWidget m_curveEditor;
	VipPlotWidget2D m_plotWidget;
	QCheckBox m_applyToExistingOnes;

	QPointer<VipPlotCurve> m_curve;
};

VipDefaultPlotAreaSettings::VipDefaultPlotAreaSettings(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	d_data->m_curveEditor.baseItemEditor()->setVisible(false);
	d_data->m_curveEditor.styleEditor()->backgroundEditor()->setColorOptionVisible(false);
	d_data->m_curveEditor.styleEditor()->borderEditor()->setColorOptionVisible(false);
	d_data->m_curveEditor.symbolEditor()->penEditor()->setColorOptionVisible(false);
	d_data->m_curveEditor.symbolEditor()->brushEditor()->setColorOptionVisible(false);

	d_data->m_leftAxis.setText("Show left axis");
	d_data->m_leftAxis.setChecked(true);
	d_data->m_rightAxis.setText("Show right axis");
	d_data->m_rightAxis.setChecked(true);
	d_data->m_topAxis.setText("Show top axis");
	d_data->m_topAxis.setChecked(true);
	d_data->m_bottomAxis.setText("Show bottom axis");
	d_data->m_bottomAxis.setChecked(true);
	d_data->m_majorGrid.setText("Show major grid");
	d_data->m_majorGrid.setChecked(true);
	d_data->m_minorGrid.setText("Show minor grid");
	d_data->m_minorGrid.setChecked(false);
	d_data->m_majorPen.setText("Major grid pen");
	d_data->m_majorPen.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d_data->m_minorPen.setText("Minor grid pen");
	d_data->m_minorPen.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d_data->m_backgroundBrush.setText("Background brush");
	d_data->m_backgroundBrush.setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	d_data->m_drawAntialize.setText("Draw with anti-aliasing");
	d_data->m_applyToExistingOnes.setText("Apply changes to all existing players");
	d_data->m_applyToExistingOnes.setToolTip("If checked, apply the parameters to ALL existing players instead of just the new ones");
	d_data->m_plotWidget.setMinimumHeight(160);
	d_data->m_plotWidget.setAttribute(Qt::WA_TransparentForMouseEvents);

	QGroupBox* bcurve = createGroup("Default curve options");
	QVBoxLayout* clay = new QVBoxLayout();
	clay->addWidget(&d_data->m_drawAntialize);
	clay->addWidget(&d_data->m_curveEditor);
	bcurve->setLayout(clay);

	QGroupBox* barea = createGroup("Default plot area options");
	QVBoxLayout* alay = new QVBoxLayout();
	alay->addWidget(&d_data->m_leftAxis);
	alay->addWidget(&d_data->m_rightAxis);
	alay->addWidget(&d_data->m_bottomAxis);
	alay->addWidget(&d_data->m_topAxis);
	alay->addWidget(&d_data->m_majorGrid);
	alay->addWidget(&d_data->m_minorGrid);
	alay->addWidget(&d_data->m_majorPen);
	alay->addWidget(&d_data->m_minorPen);
	alay->addWidget(&d_data->m_backgroundBrush);
	alay->addStretch(1);
	barea->setLayout(alay);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->setContentsMargins(0, 0, 0, 0);
	hlay->addWidget(bcurve);
	hlay->addWidget(barea);

	QVBoxLayout* vlay = new QVBoxLayout();
	// vlay->setContentsMargins(0, 0, 0, 0);
	vlay->addLayout(hlay);
	vlay->addWidget(&d_data->m_plotWidget);
	vlay->addWidget(&d_data->m_applyToExistingOnes);

	setLayout(vlay);

	d_data->m_curve = new VipPlotCurve();
	d_data->m_curve->setPen(QPen(Qt::blue));
	d_data->m_curve->setBrush(QBrush(QColor(0, 0, 255, 200), Qt::NoBrush));
	d_data->m_curve->setRawData(VipPointVector() << QPointF(3, 3) << QPointF(6, 6) << QPointF(9, 4) << QPointF(12, 7));
	VipSymbol* s = new VipSymbol();
	s->setSize(QSizeF(9, 9));
	s->setStyle(VipSymbol::Ellipse);
	s->setBrush(QBrush(Qt::blue));
	s->setPen(QPen(QColor(Qt::blue).darker(120)));
	d_data->m_curve->setSymbol(s);
	d_data->m_curve->setAxes(defaultPlotArea()->bottomAxis(), defaultPlotArea()->leftAxis(), VipCoordinateSystem::Cartesian);

	d_data->m_curveEditor.setCurve(d_data->m_curve);

	setDefaultPlotArea(VipGuiDisplayParamaters::instance()->defaultPlotArea());
	setDefaultCurve(VipGuiDisplayParamaters::instance()->defaultCurve());

	connect(&d_data->m_leftAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	connect(&d_data->m_rightAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	connect(&d_data->m_topAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	connect(&d_data->m_bottomAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	connect(&d_data->m_majorGrid, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	connect(&d_data->m_minorGrid, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	connect(&d_data->m_majorPen, SIGNAL(penChanged(const QPen&)), this, SLOT(updateItems()));
	connect(&d_data->m_minorPen, SIGNAL(penChanged(const QPen&)), this, SLOT(updateItems()));
	connect(&d_data->m_backgroundBrush, SIGNAL(penChanged(const QPen&)), this, SLOT(updateItems()));
	connect(&d_data->m_drawAntialize, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	// connect(&d_data->m_leftAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	//  connect(&d_data->m_leftAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
	//  connect(&d_data->m_leftAxis, SIGNAL(clicked(bool)), this, SLOT(updateItems()));
}
VipDefaultPlotAreaSettings::~VipDefaultPlotAreaSettings()
{
}

VipPlotCurve* VipDefaultPlotAreaSettings::defaultCurve() const
{
	return d_data->m_curve;
}
void VipDefaultPlotAreaSettings::setDefaultCurve(VipPlotCurve* c)
{
	// import the curve parameters

	// copy parameters to d_data->m_curve
	if (!c)
		return;

	// apply the curve parameters, but keep the pen and brush color unchanged, as well as the symbol colors
	d_data->m_curve->setBaseline(c->baseline());
	d_data->m_curve->setStyle(c->style());
	d_data->m_curve->setRenderHints(c->renderHints());
	d_data->m_curve->setPen(c->pen());
	d_data->m_curve->setPenColor(Qt::blue);
	d_data->m_curve->setBrush(c->brush());
	d_data->m_curve->setBrushColor(QColor(0, 0, 255, 200));
	d_data->m_curve->setSymbolVisible(c->symbolVisible());
	if (c->symbol()) {
		d_data->m_curve->symbol()->setStyle(c->symbol()->style());
		d_data->m_curve->symbol()->setSize(c->symbol()->size());
		d_data->m_curve->symbol()->setPen(c->symbol()->pen());
		d_data->m_curve->symbol()->setPenColor(QColor(Qt::blue).darker(120));
		d_data->m_curve->symbol()->setBrush(c->symbol()->brush());
		d_data->m_curve->symbol()->setPenColor(QColor(Qt::blue));
	}
	d_data->m_curveEditor.setCurve(d_data->m_curve);

	d_data->m_drawAntialize.blockSignals(true);
	d_data->m_drawAntialize.setChecked(c->renderHints() & QPainter::Antialiasing);
	d_data->m_drawAntialize.blockSignals(false);
}

VipPlotArea2D* VipDefaultPlotAreaSettings::defaultPlotArea() const
{
	return d_data->m_plotWidget.area();
}

void VipDefaultPlotAreaSettings::setDefaultPlotArea(VipPlotArea2D* area)
{
	d_data->m_leftAxis.blockSignals(true);
	d_data->m_rightAxis.blockSignals(true);
	d_data->m_topAxis.blockSignals(true);
	d_data->m_bottomAxis.blockSignals(true);
	d_data->m_majorGrid.blockSignals(true);
	d_data->m_minorGrid.blockSignals(true);
	d_data->m_majorPen.blockSignals(true);
	d_data->m_minorPen.blockSignals(true);
	d_data->m_backgroundBrush.blockSignals(true);

	d_data->m_leftAxis.setChecked(area->leftAxis()->isVisible());
	d_data->m_rightAxis.setChecked(area->rightAxis()->isVisible());
	d_data->m_topAxis.setChecked(area->topAxis()->isVisible());
	d_data->m_bottomAxis.setChecked(area->bottomAxis()->isVisible());
	d_data->m_majorGrid.setChecked(area->grid()->axisEnabled(0));
	d_data->m_minorGrid.setChecked(area->grid()->axisMinEnabled(0));
	d_data->m_majorPen.setPen(area->grid()->majorPen());
	d_data->m_minorPen.setPen(area->grid()->minorPen());
	d_data->m_backgroundBrush.setBrush(area->canvas()->boxStyle().backgroundBrush());
	applyToArea(defaultPlotArea());

	d_data->m_leftAxis.blockSignals(false);
	d_data->m_rightAxis.blockSignals(false);
	d_data->m_topAxis.blockSignals(false);
	d_data->m_bottomAxis.blockSignals(false);
	d_data->m_majorGrid.blockSignals(false);
	d_data->m_minorGrid.blockSignals(false);
	d_data->m_majorPen.blockSignals(false);
	d_data->m_minorPen.blockSignals(false);
	d_data->m_backgroundBrush.blockSignals(false);
}

void VipDefaultPlotAreaSettings::applyToCurve(VipPlotCurve* c)
{
	if (!c)
		return;

	// apply the curve parameters, but keep the pen and brush color unchanged, as well as the symbol colors
	QColor border = c->pen().color();
	QColor brush = c->brush().color();

	QColor s_border = c->symbol() ? c->symbol()->pen().color() : QColor();
	QColor s_brush = c->symbol() ? c->symbol()->brush().color() : QColor();

	// save the title and reapply it after
	VipText title = c->title();
	d_data->m_curveEditor.updateCurve(c);
	c->setTitle(title);

	// reset colors
	c->setPenColor(border);
	c->setBrushColor(brush);
	if (c->symbol()) {
		c->symbol()->setPenColor(s_border);
		c->symbol()->setBrushColor(s_brush);
	}

	// apply antialiazing
	if (d_data->m_drawAntialize.isChecked())
		c->setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform | QPainter::TextAntialiasing);
	else
		c->setRenderHints(QPainter::RenderHints());
}
void VipDefaultPlotAreaSettings::applyToArea(VipPlotArea2D* area)
{
	if (!area)
		return;

	area->leftAxis()->setVisible(d_data->m_leftAxis.isChecked());
	area->rightAxis()->setVisible(d_data->m_rightAxis.isChecked());
	area->topAxis()->setVisible(d_data->m_topAxis.isChecked());
	area->bottomAxis()->setVisible(d_data->m_bottomAxis.isChecked());
	area->grid()->enableAxis(0, d_data->m_majorGrid.isChecked());
	area->grid()->enableAxis(1, d_data->m_majorGrid.isChecked());
	area->grid()->enableAxisMin(0, d_data->m_minorGrid.isChecked());
	area->grid()->enableAxisMin(1, d_data->m_minorGrid.isChecked());
	area->grid()->setMajorPen(d_data->m_majorPen.pen());
	area->grid()->setMinorPen(d_data->m_minorPen.pen());
	area->canvas()->boxStyle().setBackgroundBrush(d_data->m_backgroundBrush.pen().brush());
}

void VipDefaultPlotAreaSettings::updateItems()
{
	applyToCurve(d_data->m_curve);
	applyToArea(defaultPlotArea());
	d_data->m_plotWidget.update();
}

bool VipDefaultPlotAreaSettings::shouldApplyToAllPlayers() const
{
	return d_data->m_applyToExistingOnes.isChecked();
}
void VipDefaultPlotAreaSettings::setShouldApplyToAllPlayers(bool apply)
{
	d_data->m_applyToExistingOnes.setChecked(apply);
}

static VipPlotItemWidget* editPlotItem(VipPlotItem* item)
{
	VipPlotItemWidget* w = new VipPlotItemWidget();
	w->setPlotItem(item);
	return w;
}

static VipPlotGridWidget* editPlotGrid(VipPlotGrid* grid)
{
	VipPlotGridWidget* w = new VipPlotGridWidget();
	w->setGrid(grid);
	return w;
}

static VipPlotCanvasWidget* editPlotCanvas(VipPlotCanvas* canvas)
{
	VipPlotCanvasWidget* w = new VipPlotCanvasWidget();
	w->setCanvas(canvas);
	return w;
}

static VipPlotCurveWidget* editPlotCurve(VipPlotCurve* curve)
{
	VipPlotCurveWidget* w = new VipPlotCurveWidget();
	w->setCurve(curve);
	return w;
}

static VipPlotHistogramWidget* editPlotHistogram(VipPlotHistogram* curve)
{
	VipPlotHistogramWidget* w = new VipPlotHistogramWidget();
	w->setHistogram(curve);
	return w;
}

static VipPlotAxisWidget* editAbstractScale(VipAbstractScale* scale)
{
	VipPlotAxisWidget* w = new VipPlotAxisWidget();
	w->setAxis(scale);
	return w;
}

static VipColorScaleWidget* editColorMap(VipAxisColorMap* scale)
{
	VipColorScaleWidget* w = new VipColorScaleWidget();
	w->setColorScale(scale);
	return w;
}

static VipAbstractPlayerWidget* editAbstractPlayer(VipAbstractPlayer* player)
{
	VipAbstractPlayerWidget* w = new VipAbstractPlayerWidget();
	w->setAbstractPlayer(player);
	return w;
}

static int registerStandardEditors()
{
	vipFDObjectEditor().append<VipPlotAxisWidget*(VipAbstractScale*)>(editAbstractScale);
	vipFDObjectEditor().append<VipPlotItemWidget*(VipPlotItem*)>(editPlotItem);
	vipFDObjectEditor().append<VipPlotGridWidget*(VipPlotGrid*)>(editPlotGrid);
	vipFDObjectEditor().append<VipPlotCanvasWidget*(VipPlotCanvas*)>(editPlotCanvas);
	vipFDObjectEditor().append<VipPlotCurveWidget*(VipPlotCurve*)>(editPlotCurve);
	vipFDObjectEditor().append<VipPlotHistogramWidget*(VipPlotHistogram*)>(editPlotHistogram);
	vipFDObjectEditor().append<VipColorScaleWidget*(VipAxisColorMap*)>(editColorMap);
	vipFDObjectEditor().append<VipAbstractPlayerWidget*(VipAbstractPlayer*)>(editAbstractPlayer);
	return 0;
}

static int _registerStandardEditors = vipAddInitializationFunction(registerStandardEditors);
