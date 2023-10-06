#include "VipPolarWidgets.h"
#include "VipPolarAxis.h"
#include "VipPlotMarker.h"
#include "VipPlotGrid.h"
#include "VipPieChart.h"
#include "VipSymbol.h"

#include <qapplication.h>

class OpaqueBackgroundPie : public VipPieItem
{
	QPointer< VipPlotPolarArea2D> area;
public:
	OpaqueBackgroundPie(VipPlotPolarArea2D * a)
		:VipPieItem(), area(a){}

	virtual void draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
	{
		if (const VipPlotPolarArea2D* a = area) {
			QColor background = view()->palette().color(QPalette::Window);
			QBrush areaBackground = a->boxStyle().backgroundBrush();
			if (areaBackground.style() != Qt::NoBrush && areaBackground.color().alpha() != 0)
				background = areaBackground.color();
			if (background != this->boxStyle().backgroundBrush().color()) {
				const_cast<VipBoxStyle&>(this->boxStyle()).setBackgroundBrush(QBrush(background));
				const_cast<VipBoxStyle&>(this->boxStyle()).setBorderPen(QPen(background));
			}

			VipPieItem::draw(painter, m);
		}
	}
};

#include <QGraphicsSceneWheelEvent>

class VipCustomPolarArea : public VipPlotPolarArea2D
{
	QPointer< VipPolarValueGauge> value;
public:
	VipCustomPolarArea(VipPolarValueGauge * w)
		:VipPlotPolarArea2D(), value(w)
	{}

protected:
	virtual void	wheelEvent(QGraphicsSceneWheelEvent* event)
	{
		VipInterval bounds = value->range();
		double val = value->value() + (event->delta() > 0 ? 1 : -1);
		if (val > bounds.maxValue())
			val = bounds.maxValue();
		else if (val < bounds.minValue())
			val = bounds.minValue();
		value->setValue(val);
	}
};


class VipPolarValueGauge::PrivateData
{
public:
	VipPieItem* polarGradiant;
	VipPieItem* shadow;
	VipPieItem* light;
	VipPieItem* clipValue;
	VipPieItem* background;
	VipPlotMarker* centralText;
	VipPlotMarker* bottomText;
	QString textFormat;

	double value;
	double radialWidth;
	double shadowSize;
	double lightSize;
};

VipPolarValueGauge::VipPolarValueGauge(QWidget* parent)
	:VipPlotPolarWidget2D(parent)
{
	d_data = new PrivateData();
	this->setArea(new VipCustomPolarArea(this));
	this->area()->polarAxis()->setStartAngle(-15);
	this->area()->polarAxis()->setEndAngle(180 + 15);
	this->area()->titleAxis()->setVisible(true);
	this->area()->radialAxis()->setStartRadius(0, this->area()->polarAxis());
	this->area()->radialAxis()->setEndRadius(1, this->area()->polarAxis());
	this->area()->radialAxis()->setVisible(false);
	this->area()->radialAxis()->setAngle(90);
	this->area()->radialAxis()->setAutoScale(false);
	this->area()->polarAxis()->setAutoScale(false);
	this->area()->grid()->enableAxis(0, false);
	QColor white_transparent(255, 255, 255, 100);
	this->area()->grid()->setMajorPen(white_transparent);
	this->area()->grid()->setMinorPen(white_transparent);
	this->area()->grid()->enableAxisMin(1, true);

	//add a left scale from 0 to 100
	VipAxisBase* left = new VipAxisBase();
	left->setAutoScale(false);
	left->setScale(0, 100);
	this->area()->addScale(left);
	left->setVisible(false);
	this->area()->titleAxis()->setAutoScale(false);
	this->area()->titleAxis()->setScale(0, 100);


	VipScaleDiv div;
	VipScaleDiv::TickList major, minor;
	for (double i = 0; i < 100; i += 2.5)
		minor.append(i);
	for (double i = 0; i < 100; i += 10)
		major.append(i);
	major.append(100);
	div.setTicks(VipScaleDiv::MajorTick, major);
	div.setTicks(VipScaleDiv::MinorTick, minor);
	div.setInterval(0, 100);
	d_data->value = 0;
	this->area()->polarAxis()->setScaleDiv(div);
	this->area()->polarAxis()->scaleDraw()->setTickLength(VipScaleDiv::MinorTick, this->area()->polarAxis()->scaleDraw()->tickLength(VipScaleDiv::MajorTick));
	this->area()->polarAxis()->setScaleInverted(true);

	QList<VipAbstractScale*> scales;
	this->area()->standardScales(scales);

	d_data->polarGradiant = new VipPieItem();
	d_data->polarGradiant->setRawData(VipPie(0, 100, 60, 100, 0));
	d_data->radialWidth = 40;
	VipAdaptativeGradient grad;
	grad.setConical();
	grad.setGradientStops(QGradientStops() << QGradientStop(0, QColor(0xD02128)) << QGradientStop(0.5, QColor(0xFDF343)) << QGradientStop(1, QColor(0x11B34C)));
	d_data->polarGradiant->boxStyle().setAdaptativeGradientBrush(grad);
	//VipText t = "this is a long text";
	// t.setAlignment(Qt::AlignLeft | Qt::AlignTop);
	// p->setText(t);
	// p->setTextPosition(Vip::Inside);
	// p->setCurved(true);
	// p->setRotation(20);
	// p->setTextDirection(VipText::AutoDirection);
	d_data->polarGradiant->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->polarGradiant->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	d_data->polarGradiant->setRenderHints(QPainter::Antialiasing );

	d_data->clipValue = new VipPieItem();
	d_data->clipValue->setRawData(VipPie(0, 100, 60, 100, 0));
	d_data->clipValue->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->clipValue->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	d_data->clipValue->setRenderHints(QPainter::Antialiasing );
	d_data->clipValue->boxStyle().setBorderPen(QPen(Qt::white));
	d_data->polarGradiant->setClipTo(d_data->clipValue);

	d_data->background = new VipPieItem();
	d_data->background->setRawData(VipPie(0, 100, 60, 100, 0));
	d_data->background->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->background->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	d_data->background->setRenderHints(QPainter::Antialiasing);
	d_data->background->boxStyle().setBorderPen(QPen(Qt::white));

	this->area()->grid()->setClipTo(d_data->polarGradiant);

	d_data->shadowSize = 2.5;
	d_data->shadow = new VipPieItem();
	d_data->shadow->setRawData(VipPie(-d_data->shadowSize, 100 + d_data->shadowSize, 100 - d_data->radialWidth - d_data->shadowSize, 100));
	QColor shadow = QColor(0x141720);
	shadow.setAlpha(70);
	d_data->shadow->boxStyle().setBackgroundBrush(QBrush(shadow));
	d_data->shadow->boxStyle().setBorderPen(QPen(shadow));
	d_data->shadow->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->shadow->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	d_data->shadow->setZValue(d_data->polarGradiant->zValue()-0.1);// area()->grid()->zValue() + 2);


	d_data->lightSize = 10;
	d_data->light = new VipPieItem();
	d_data->light->setRawData(VipPie(0, 100, 90, 100));
	QColor c(//0x141720
Qt::white);
	c.setAlpha(50);
	d_data->light->boxStyle().setBackgroundBrush(QBrush(c));
	d_data->light->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->light->setItemAttribute(VipPlotItem::ClipToScaleRect, false);


	d_data->centralText = new VipPlotMarker();
	VipText t;
	VipTextStyle textStyle = t.textStyle();
	VipAdaptativeGradient gradText;
	gradText.setLinear(Qt::Vertical);
	gradText.setGradientStops(QGradientStops() << QGradientStop(0, QColor(0xF8DA46)) << QGradientStop(1, QColor(0xDD901E)) );
	textStyle.textBoxStyle().setAdaptativeGradientBrush(gradText);
	textStyle.textBoxStyle().setBorderPen(QPen(Qt::NoPen));
	t.setTextStyle(textStyle);

	d_data->centralText->setLabel(t);
	d_data->centralText->setExpandToFullArea(false);
	d_data->centralText->setRawData(QPointF(-40, 50));
	d_data->centralText->setRelativeFontSize(40, 0);
	d_data->centralText->setLabelAlignment(Qt::AlignTop | Qt::AlignHCenter);
	d_data->centralText->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->centralText->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	d_data->centralText->setItemAttribute(VipPlotItem::AutoScale, false);

	d_data->bottomText = new VipPlotMarker();
	d_data->bottomText->setLabel(VipText());
	d_data->bottomText->setExpandToFullArea(false);
	d_data->bottomText->setRawData(QPointF(-40, 50));
	d_data->bottomText->setRelativeFontSize(10, 0);
	d_data->bottomText->setLabelAlignment(Qt::AlignBottom | Qt::AlignHCenter);
	d_data->bottomText->setAxes(scales, VipCoordinateSystem::Polar);
	d_data->bottomText->setItemAttribute(VipPlotItem::ClipToScaleRect, false);
	d_data->bottomText->setItemAttribute(VipPlotItem::AutoScale, false);
}

VipPolarValueGauge::~VipPolarValueGauge()
{
	delete d_data;
}

void VipPolarValueGauge::recomputeFullGeometry()
{
	VipInterval bounds = area()->polarAxis()->scaleDiv().bounds().normalized();
	d_data->polarGradiant->setRawData(VipPie(bounds.minValue(), bounds.maxValue(), 100 - d_data->radialWidth, 100, 0));
	d_data->background->setRawData(VipPie(bounds.minValue(), bounds.maxValue(), 100 - d_data->radialWidth, 100, 0));
	d_data->clipValue->setRawData(VipPie(bounds.minValue(), d_data->value, 100 - d_data->radialWidth, 100, 0));
	double shadow_width = bounds.width() * (d_data->shadowSize / 100) /2.;
	d_data->shadow->setRawData(VipPie(bounds.minValue() - shadow_width, bounds.maxValue() + shadow_width, 100 - d_data->radialWidth - d_data->shadowSize, 100 - d_data->shadowSize));
	d_data->light->setRawData(VipPie(bounds.minValue(), bounds.maxValue(), 100 - d_data->lightSize, 100));
	d_data->centralText->setRawData(QPointF(d_data->centralText->rawData().x(), (bounds.maxValue() - bounds.minValue())/2));
	d_data->bottomText->setRawData(QPointF(d_data->bottomText->rawData().x(), (bounds.maxValue() - bounds.minValue()) / 2));
}

void VipPolarValueGauge::setRange(double min, double max, double step)
{
	VipScaleDiv div;
	VipScaleDiv::TickList major, minor;
	if (step == 0)
		step = (max - min) / 40;
	for (double i = min; i <= max; i += step)
		minor.append(i);
	double major_step = step * 4;
	for (double i = min; i <= max; i += major_step)
		major.append(i);
	if(major.last() != max)
		major.append(max);
	div.setTicks(VipScaleDiv::MajorTick, major);
	div.setTicks(VipScaleDiv::MinorTick, minor);
	div.setInterval(min,max);
	this->area()->polarAxis()->setScaleDiv(div);

	if (d_data->value < min)
		d_data->value = min;
	else if (d_data->value > max)
		d_data->value = max;

	recomputeFullGeometry();
}
VipInterval VipPolarValueGauge::range() const
{
	return this->area()->polarAxis()->scaleDiv().bounds().normalized();
}

void VipPolarValueGauge::setAngles(double start, double end)
{
	if (start != area()->polarAxis()->startAngle() || end != area()->polarAxis()->endAngle()) {
		area()->polarAxis()->setStartAngle(start);
		area()->polarAxis()->setEndAngle(end);
		recomputeFullGeometry();
	}
}
VipInterval VipPolarValueGauge::angles() const
{
	return VipInterval(area()->polarAxis()->startAngle(), area()->polarAxis()->endAngle());
}

void VipPolarValueGauge::setRadialWidth(double width)
{
	if (width != d_data->radialWidth) {

		d_data->radialWidth = width;
		recomputeFullGeometry();
	}
}
double VipPolarValueGauge::radialWidth()
{
	return d_data->radialWidth;
}

void VipPolarValueGauge::setShadowSize(double value)
{
	if (value != d_data->shadowSize) {
		d_data->shadowSize = value;
		recomputeFullGeometry();
	}
}
double VipPolarValueGauge::shadowSize() const
{
	return d_data->shadowSize;
	//return d_data->value->rawData().minRadius() - d_data->shadow->rawData().minRadius();
}

void VipPolarValueGauge::setLightSize(double value)
{
	if (value != d_data->lightSize) {
		d_data->lightSize = value;
		recomputeFullGeometry();
	}
}
double VipPolarValueGauge::lightSize() const
{
	return d_data->lightSize;//d_data->value->rawData().maxRadius() - d_data->light->rawData().minRadius();
}

void VipPolarValueGauge::setShadowColor(const QColor& c)
{
	d_data->shadow->boxStyle().setBackgroundBrush(QBrush(c));
	update();
}
QColor VipPolarValueGauge::shadowColor() const
{
	return d_data->shadow->boxStyle().backgroundBrush().color();
}

void VipPolarValueGauge::setLightColor(const QColor& c)
{
	d_data->light->boxStyle().setBackgroundBrush(QBrush(c));
	update();
}
QColor VipPolarValueGauge::lightColor() const
{
	return d_data->light->boxStyle().backgroundBrush().color();
}

void VipPolarValueGauge::setTextFormat(const QString& format)
{
	d_data->textFormat = format;
	update();
}
QString VipPolarValueGauge::textFormat() const
{
	return d_data->textFormat;
}

VipValueToText* VipPolarValueGauge::scaleValueToText() const
{
	return area()->polarAxis()->scaleDraw()->valueToText();
}
void VipPolarValueGauge::setScaleValueToText(VipValueToText* v)
{
	area()->polarAxis()->scaleDraw()->setValueToText(v);
	update();
}

void VipPolarValueGauge::setTextVerticalPosition(double pos)
{
	VipInterval bounds = area()->polarAxis()->scaleDiv().bounds().normalized();
	d_data->centralText->setRawData(QPointF(pos, (bounds.maxValue() - bounds.minValue()) / 2));
}
double VipPolarValueGauge::textVerticalPosition() const
{
	return d_data->centralText->rawData().x();
}

void VipPolarValueGauge::setBottomTextVerticalPosition(double pos)
{
	VipInterval bounds = area()->polarAxis()->scaleDiv().bounds().normalized();
	d_data->bottomText->setRawData(QPointF(pos, (bounds.maxValue() - bounds.minValue()) / 2));
}
double VipPolarValueGauge::bottomTextVerticalPosition() const
{
	return d_data->bottomText->rawData().x();
}

VipPlotMarker* VipPolarValueGauge::bottomText() const
{
	return d_data->bottomText;
}

VipPlotMarker* VipPolarValueGauge::centralText() const
{
	return d_data->centralText;
}

VipPieItem* VipPolarValueGauge::gradientPie() const
{
	return d_data->polarGradiant;
}
VipPieItem* VipPolarValueGauge::valuePie() const
{
	return d_data->clipValue;
}
VipPieItem* VipPolarValueGauge::backgroundPie() const
{
	return d_data->background;
}
VipPieItem* VipPolarValueGauge::shadowPie() const
{
	return d_data->shadow;
}
VipPieItem* VipPolarValueGauge::lightPie() const
{
	return d_data->light;
}


double VipPolarValueGauge::value() const
{
	return d_data->value;
}

void VipPolarValueGauge::setValue(double value)
{
	if (value != d_data->value) {
		d_data->value = value;
		VipText t = d_data->centralText->label();
		if (!d_data->textFormat.isEmpty())
		{
			t.setText(d_data->textFormat);
			t.sprintf(value);
		}
		else
			t.setText(QString::number(value));
		d_data->centralText->setLabel(t);

		VipInterval bounds = area()->polarAxis()->scaleDiv().bounds().normalized();
		d_data->clipValue->setRawData(VipPie(bounds.minValue(), d_data->value, 100 - d_data->radialWidth, 100, 0));
	}
}
