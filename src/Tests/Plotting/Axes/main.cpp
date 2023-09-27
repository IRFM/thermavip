
#include <qapplication.h>
#include <cmath>
#include "VipMultiPlotWidget2D.h"
#include "VipColorMap.h"
#include "VipPlotCurve.h"
#include "VipPolarAxis.h"
#include "VipSliderGrip.h"

#include "VipStyleSheet.h"
#include <iostream>

int main(int argc, char** argv)
{
	QApplication app(argc, argv);

	// Rotated texts are ugly with raster engine... let's cache it through QPixmap
	VipText::setCacheTextWhenPossible(true);

	VipPlotPolarWidget2D w;
	VipPlotPolarArea2D* area = w.area();
	area->setInnerMargin(20);
	area->setMargins(10);
	area->setTitle("<b>Example of plotting area with multiple axes");
	// add a space between title and plotting area
	area->titleAxis()->setMargin(20);

	
	// The default VipPlotPolarArea2D polar axis shows text outside with TextHorizontal transform.
	// Now let's add additional axes with other parameters
	
	// Polar axis with TextInside and TicksInside
	VipPolarAxis* p = new VipPolarAxis();
	p->setCenterProximity(2);
	p->setStartAngle(0);
	p->setEndAngle(100);
	p->scaleDraw()->setTextPosition(VipScaleDraw::TextInside);
	p->scaleDraw()->setTicksPosition(VipScaleDraw::TicksInside);
	area->addScale(p);

	/// Polar axis with TextPerpendicular
	VipPolarAxis* p2 = new VipPolarAxis();
	p2->setCenterProximity(1);
	p2->setStartAngle(110);
	p2->setEndAngle(210);
	p2->scaleDraw()->setTextTransform(VipScaleDraw::TextPerpendicular, VipScaleDiv::MajorTick);
	area->addScale(p2);

	/// Polar axis with TextParallel
	VipPolarAxis* p3 = new VipPolarAxis();
	p3->setCenterProximity(1);
	p3->setStartAngle(220);
	p3->setEndAngle(350);
	p3->scaleDraw()->setTextTransform(VipScaleDraw::TextParallel, VipScaleDiv::MajorTick);
	area->addScale(p3);

	/// Polar axis with TextCurved
	VipPolarAxis* p4 = new VipPolarAxis();
	p4->setCenterProximity(2);
	p4->setStartAngle(120);
	p4->setEndAngle(350);
	p4->setScale(100000, 1000000);
	p4->scaleDraw()->setTextTransform(VipScaleDraw::TextCurved, VipScaleDiv::MajorTick);
	area->addScale(p4);

	// Show how to add a grip to an axis
	VipSliderGrip* grip = new VipSliderGrip(p);
	grip->setToolTipText("#value");


	VipAxisBase* l = new VipAxisBase(VipAxisBase::Left);
	l->scaleDraw()->textStyle().setAlignment(Qt::AlignTop);
	l->setTitle("<b>Text aligned top");
	area->addScale(l);

	VipAxisBase* r = new VipAxisBase(VipAxisBase::Right);
	r->scaleDraw()->setSpacing(5);
	r->scaleDraw()->setLabelTransform(QTransform().rotate(45), VipScaleDiv::MajorTick);
	r->scaleDraw()->setLabelTransformReference(QPointF(0, 0.5), VipScaleDiv::MajorTick);
	r->setTitleInverted(true);
	r->setTitle("<b>Text rotation of 45 degrees, title inverted");
	area->addScale(r);

	VipAxisBase* t = new VipAxisBase(VipAxisBase::Top);
	area->addScale(t);

	VipAxisBase* b = new VipAxisBase(VipAxisBase::Bottom);
	b->scaleDraw()->textStyle().setAlignment(Qt::AlignRight);
	b->setTitle("<b>Text aligned right, title inside");
	b->setTitleInside(true);
	area->addScale(b);

	VipAxisColorMap* map;
	{
		VipAxisBase* l = new VipAxisBase(VipAxisBase::Left);
		l->setCanvasProximity(1);
		l->setMargin(20);
		l->scaleDraw()->setTextTransform(VipScaleDraw::TextParallel, VipScaleDiv::MajorTick);
		l->scaleDraw()->setTextPosition(VipScaleDraw::TextInside);
		l->setTitle("<b>Text parallel to the backbone and inside");
		area->addScale(l);

		map = new VipAxisColorMap();
		map->setColorMap(VipInterval(0, 100), VipLinearColorMap::Sunset);
		map->scaleDraw()->setSpacing(5);
		map->setTitle("<b>Sunset color map");
		area->addScale(map);

		VipAxisBase* t = new VipAxisBase(VipAxisBase::Top);
		t->setCanvasProximity(1);
		t->setMargin(30);
		t->scaleDraw()->setTextTransform(VipScaleDraw::TextPerpendicular, VipScaleDiv::MajorTick);
		t->scaleDraw()->setTextPosition(VipScaleDraw::TextInside);
		t->setTitle("<b>Text perpendicular to the backbone and inside");
		area->addScale(t);

		VipAxisBase* b = new VipAxisBase(VipAxisBase::Bottom);
		b->setCanvasProximity(1);
		b->setMargin(10);
		b->scaleDraw()->setSpacing(5);
		b->scaleDraw()->textStyle().boxStyle().setBackgroundBrush(QBrush(Qt::blue));
		b->scaleDraw()->textStyle().boxStyle().setBorderPen(QPen(Qt::darkBlue));
		b->scaleDraw()->textStyle().setTextPen(QPen(Qt::white));
		b->scaleDraw()->textStyle().setMargin(3);
		b->setSpacing(5);
		b->setTitle("<b>Custom text box style, additional margin to the center");
		area->addScale(b);
		
	}


	{

		VipMultiAxisBase* multi = new VipMultiAxisBase();
		multi->setCanvasProximity(2);
		multi->setMargin(20);
		multi->setScaleSpacing(10);
		multi->setTitle("<b>Multi vertical axis with fixed space between scales");
		VipAxisBase* first = new VipAxisBase();
		VipAxisBase* second = new VipAxisBase();
		VipAxisBase* third = new VipAxisBase();
		multi->addScale(first);
		multi->addScale(second);
		multi->addScale(third);
		area->addScale(multi);
	}
	{

		VipMultiAxisBase* multi = new VipMultiAxisBase(VipBorderItem::Top);
		multi->setCanvasProximity(2);
		multi->setMargin(20);
		multi->setTitle("<b>Multi horizontal axis with automatic spacing");
		VipAxisBase* first = new VipAxisBase();
		first->setUseBorderDistHintForLayout(true);
		VipAxisBase* second = new VipAxisBase();
		second->setUseBorderDistHintForLayout(true);
		VipAxisBase* third = new VipAxisBase();
		third->setUseBorderDistHintForLayout(true);
		multi->addScale(first);
		multi->addScale(second);
		multi->addScale(third);
		area->addScale(multi);
	}

	// Enable zooming/panning
	area->setMousePanning(Qt::RightButton);
	area->setMouseWheelZoom(true);


	//add curve in polar coordinate system

	VipPointVector vec;
	for (int i=0; i < 400; ++i) {
		VipPoint p(i/10., std::cos(i * 0.1 * M_PI));
		vec.push_back(p);
	}
	VipPlotCurve* c = new VipPlotCurve();
	c->setRawData(vec);
	c->setPen(QPen(Qt::blue));
	c->setAxes(area->polarAxis(), area->radialAxis(), VipCoordinateSystem::Polar);
	

	w.resize(700, 700);
	w.show();
	return app.exec();
}
