#include "VipAxisColorMap.h"
#include "VipSliderGrip.h"
#include "VipPainter.h"
#include "VipPlotItem.h"





/// -	'color-bar-enabled': boolean value equivalent to VipAxisColorMap::setColorBarEnabled()
/// -	'color-bar-width': floating point value equivalent to VipAxisColorMap::setColorBarWidth()
/// -	'use-flat-histogram':  boolean value equivalent to VipAxisColorMap::setUseFlatHistogram()
/// -	'flat-histogram-strength': floating point value equivalent to VipAxisColorMap::setFlatHistogramStrength()
 
static int registerAxisColorMapKeywords() 
{
	VipKeyWords keys;
	keys["color-bar-enabled"] = VipParserPtr(new BoolParser());
	keys["color-bar-width"] = VipParserPtr(new DoubleParser());
	keys["use-flat-histogram"] = VipParserPtr(new BoolParser());
	keys["flat-histogram-strength"] = VipParserPtr(new DoubleParser());
	vipSetKeyWordsForClass(&VipAxisColorMap::staticMetaObject, keys);
	return 0;
}

static int _registerAxisColorMapKeywords = registerAxisColorMapKeywords();

class VipAxisColorMap::PrivateData
{
public:

	struct t_colorBar
	{
		bool isEnabled;
		double width;
		VipInterval interval;
		VipColorMap *colorMap;
	} colorBar;

	QList<VipPlotItem*> plotItems;

	VipColorMapGrip * grip_1;
	VipColorMapGrip * grip_2;
	VipInterval computedInterval;

	QList<VipColorMapGrip*> grips;
	QPixmap pixmap;

	vip_double autoScaleMin, autoScaleMax;
	bool hasAutoScaleMin, hasAutoScaleMax;
};

VipAxisColorMap::VipAxisColorMap(Alignment pos, QGraphicsItem * parent )
:VipAxisBase(pos,parent)
{
	d_data = new PrivateData();
	d_data->colorBar.colorMap = NULL;
	d_data->colorBar.width = 15;
	d_data->colorBar.isEnabled = true;
	d_data->grip_1 = new VipColorMapGrip(this);
	d_data->grip_2 = new VipColorMapGrip(this);
	d_data->autoScaleMin = d_data->autoScaleMax = vipNan();
	d_data->hasAutoScaleMin = d_data->hasAutoScaleMax = false;

	VipInterval interval  = this->scaleDiv().bounds();
	d_data->grip_1->setValue(interval.minValue());
	d_data->grip_2->setValue(interval.maxValue());

	this->setMargin(5);
	this->scaleDraw()->setComponents(VipScaleDraw::Backbone | VipScaleDraw::Ticks | VipScaleDraw::Labels);
	this->scaleDraw()->setTicksPosition(VipScaleDraw::TicksOutside);

	connect(d_data->grip_1,SIGNAL(valueChanged(double)),this,SLOT(gripValueChanged(double)));
	connect(d_data->grip_2,SIGNAL(valueChanged(double)),this,SLOT(gripValueChanged(double)));

	//when scale div changed, do not notify plot items
	//disconnect(this,SIGNAL(scaleDivChanged(bool)),this,SLOT(notifyScaleDivChanged()));
	connect(this,SIGNAL(scaleDivChanged(bool)),this,SLOT(scaleDivHasChanged()));

	this->reset(pos);
	this->setObjectName("Color scale");
}

bool VipAxisColorMap::setItemProperty(const char* name, const QVariant& value, const QByteArray& index) 
{
	if (value.userType() == 0)
		return false;

	/// -	'color-bar-enabled': boolean value equivalent to VipAxisColorMap::setColorBarEnabled()
	/// -	'color-bar-width': floating point value equivalent to VipAxisColorMap::setColorBarWidth()
	/// -	'use-flat-histogram':  boolean value equivalent to VipAxisColorMap::setUseFlatHistogram()
	/// -	'flat-histogram-strength': integer point value equivalent to VipAxisColorMap::setFlatHistogramStrength()
	/// 
	if (strcmp(name, "color-bar-enabled") == 0) {
		setColorBarEnabled(value.toBool());
		return true;
	}
	if (strcmp(name, "color-bar-width") == 0) {
		setColorBarWidth(value.toDouble());
		return true;
	}
	if (strcmp(name, "use-flat-histogram") == 0) {
		setUseFlatHistogram(value.toBool());
		return true;
	}
	if (strcmp(name, "flat-histogram-strength") == 0) {
		setFlatHistogramStrength(value.toInt());
		return true;
	}
	
	return VipAxisBase::setItemProperty(name, value, index);
}

void VipAxisColorMap::scaleDivHasChanged()
{
	//smart grip update

	QPointF p1 = grip1()->pos();
	QPointF p2 = grip2()->pos();
	bool update_grip = false;

	if(this->orientation() == Qt::Horizontal)
	{
		VipInterval scale = VipInterval(this->scalePosition().x(),this->scaleEndPosition().x()).normalized();
		update_grip = ( ((p1.x() <= scale.minValue() && p2.x() >= scale.maxValue()) ||
				(p2.x() <= scale.minValue() && p1.x() >= scale.maxValue())) );
	}
	else
	{
		VipInterval scale = VipInterval(this->scalePosition().y(),this->scaleEndPosition().y()).normalized();
		update_grip = ( ((p1.y() <= scale.minValue() && p2.y() >= scale.maxValue()) ||
				(p2.y() <= scale.minValue() && p1.y() >= scale.maxValue())) );
	}

	if(update_grip)
	{
		grip1()->blockSignals(true);
		grip2()->blockSignals(true);

		VipInterval interval = scaleDiv().bounds().normalized();
		setColorMapInterval(interval);
		grip1()->setValue(interval.minValue());
		grip2()->setValue(interval.maxValue());

		grip1()->blockSignals(false);
		grip2()->blockSignals(false);
	}


}

VipAxisColorMap::~VipAxisColorMap()
{
	if(d_data->colorBar.colorMap)
		delete d_data->colorBar.colorMap;

	delete d_data;
}

void VipAxisColorMap::reset( Alignment align )
{
	Q_UNUSED(align)

	if(d_data->colorBar.colorMap)
		delete d_data->colorBar.colorMap;

	d_data->colorBar.colorMap = new VipLinearColorMap();
	d_data->colorBar.isEnabled = true;
	d_data->colorBar.width = 15;
	emitScaleNeedUpdate();
}

double VipAxisColorMap::additionalSpace() const
{
	if ( d_data->colorBar.isEnabled  && d_data->colorBar.interval.isValid() )
	{
		return d_data->colorBar.width;
	}
	else
		return 0.;
}

const QList<VipPlotItem*> VipAxisColorMap::itemList() const
{
	return d_data->plotItems;
}

VipInterval VipAxisColorMap::validInterval() const
{
	VipInterval res(0,0,VipInterval::IncludeBorders);
	if (hasAutoScaleMin())
		res.setMinValue(autoScaleMin());
	else
		res.setMinValue(-std::numeric_limits<vip_double>::infinity());
	if (hasAutoScaleMax())
		res.setMaxValue(autoScaleMax());
	else
		res.setMaxValue(std::numeric_limits<vip_double>::infinity());
	return res;
}

void VipAxisColorMap::computeScaleDiv()
{
	//update scale div according to the items with ColorMapAutoScale
	if (isAutoScale())
	{
		if (d_data->plotItems.size())
		{
			VipInterval interval;
			VipInterval valid = validInterval();

			for (int i = 0; i < d_data->plotItems.size(); ++i)
			{
				if (d_data->plotItems[i]->testItemAttribute(VipPlotItem::ColorMapAutoScale))
				{
					if (!interval.isValid())
					{
						interval = d_data->plotItems[i]->plotInterval(valid);
					}
					else
					{
						interval = interval.unite(d_data->plotItems[i]->plotInterval(valid));
					}
				}
			}

			if (interval.isValid() && interval != d_data->computedInterval)
			{
				d_data->computedInterval = interval;

				//reset the grip before the scale
				bool inside1 = grip1()->gripAlwaysInsideScale();
				bool inside2 = grip2()->gripAlwaysInsideScale();
				grip1()->setGripAlwaysInsideScale(false);
				grip2()->setGripAlwaysInsideScale(false);
				setGripInterval(interval);

				//reset scale div
				vip_double stepSize = 0.;
				vip_double x1 = interval.minValue(), x2 = interval.maxValue();
				scaleEngine()->autoScale(maxMajor(), x1, x2, stepSize);
				this->setScaleDiv(scaleEngine()->divideScale(x1, x2, maxMajor(), maxMinor(), stepSize));

				grip1()->setGripAlwaysInsideScale(inside1);
				grip2()->setGripAlwaysInsideScale(inside2);
			}
		}
		else
		{
			//compute the scale div the standard way in case
			//plot items are actually using this color map as an axis.
			d_data->computedInterval = VipInterval();
			VipAbstractScale::computeScaleDiv();
		}
	}
	else
		d_data->computedInterval = VipInterval();
}

void VipAxisColorMap::divideAxisScale(vip_double min, vip_double max, vip_double stepSize)
{
	grip1()->blockSignals(true);
	grip2()->blockSignals(true);
	this->blockSignals(true);

	vip_double mi = min, ma = max;

	this->scaleEngine()->autoScale(this->maxMajor(),min,max,stepSize);
	this->setScaleDiv(this->scaleEngine()->divideScale(min,max,this->maxMajor(),this->maxMinor(),stepSize));
	grip1()->setValue(mi);
	grip2()->setValue(ma);

	grip1()->blockSignals(false);
	grip2()->blockSignals(false);
	this->blockSignals(false);

	//this->emitScaleDivChanged();
}

/// Draw the color bar of the scale widget
///
/// \param painter VipPainter
/// \param rect Bounding rectangle for the color bar
///
/// \sa setColorBarEnabled()
void VipAxisColorMap::drawColorBar( QPainter *painter, const QRectF& rect ) const
{
    if ( !d_data->colorBar.interval.isValid() )
        return;

    const VipScaleDraw* sd = this->constScaleDraw();

    VipPainter::drawColorBar( painter, *d_data->colorBar.colorMap,
        d_data->colorBar.interval.normalized(), sd->scaleMap(),
        sd->orientation(), rect, &d_data->pixmap );
}

double VipAxisColorMap::extentForLength(double length) const
{
	//return d_data->length;
	return  VipAxisBase::extentForLength( length) +5;
}

void VipAxisColorMap::itemGeometryChanged(const QRectF & r)
{
	VipAxisBase::itemGeometryChanged(r);

	//reset grip items positions

	for(int i= 0; i < d_data->grips.size(); ++i)
	{
		d_data->grips[i]->setValue(d_data->grips[i]->value());
	}

	d_data->grip_1->setValue(d_data->grip_1->value());
	d_data->grip_2->setValue(d_data->grip_2->value());
}

void VipAxisColorMap::draw ( QPainter * painter, QWidget * widget  )
{
	const bool has_backbone = this->scaleDraw()->hasComponent(VipScaleDraw::Backbone);
	bool draw_backbone = has_backbone;
	if ( d_data->colorBar.isEnabled && d_data->colorBar.width > 0 &&
			d_data->colorBar.interval.isValid() )
	{
		draw_backbone = false;

		this->scaleDraw()->setComponents(this->scaleDraw()->components() & ~VipScaleDraw::Backbone);
		VipAxisBase::draw(painter, widget);

		QRectF rect = colorBarRect();
		//rect = rect.adjusted(0, 0, 0, 0);
		drawColorBar(painter, rect.toAlignedRect()); // has_backbone ? rect.adjusted(0, 0, 0, 0) : rect);

		if (has_backbone) {
			painter->setPen(this->scaleDraw()->componentPen(VipScaleDraw::Backbone));
			QPainter::RenderHints hint = painter->renderHints();
			if (!painter->transform().isRotating()) {
				painter->setRenderHints(QPainter::Antialiasing,false);
				painter->setRenderHints(QPainter::HighQualityAntialiasing, false);
			}
			painter->drawRect(rect.toAlignedRect());
			if (!painter->transform().isRotating())
				painter->setRenderHints(hint);
		}
	}
	else {
		if (!draw_backbone)
			this->scaleDraw()->setComponents(this->scaleDraw()->components() & ~VipScaleDraw::Backbone);
		VipAxisBase::draw(painter, widget);
	}
	
	if (has_backbone)
		this->scaleDraw()->setComponents(this->scaleDraw()->components() | VipScaleDraw::Backbone);
}



QRectF VipAxisColorMap::colorBarRect() const
{
	return colorBarRect(this->boundingRectNoCorners());
}

/// Calculate the the rectangle for the color bar
///
/// \param rect Bounding rectangle for all components of the scale
/// \return Rectangle for the color bar
QRectF VipAxisColorMap::colorBarRect( const QRectF& rect ) const
{
    QRectF cr = rect;
    double margin = this->margin();
    //double borderDist[2];
    // borderDist[0] = this->startBorderDist();
    // borderDist[1] = this->endBorderDist();

    if ( constScaleDraw()->orientation() == Qt::Horizontal )
    {
        cr.setLeft( this->constScaleDraw()->pos().x() );////cr.left() + borderDist[0] );
        cr.setWidth( this->constScaleDraw()->length() );//cr.width() - borderDist[1] + 1 );
    }
    else
    {
        cr.setTop( this->constScaleDraw()->pos().y() );//cr.top() + borderDist[0] );
        cr.setHeight( this->constScaleDraw()->length() );//cr.height() - borderDist[1] + 1 );
    }

    switch ( this->alignment() )
    {
        case Left:
        {
            cr.setLeft( cr.right() - margin
                - d_data->colorBar.width );
            cr.setWidth( d_data->colorBar.width );
            break;
        }

        case Right:
        {
            cr.setLeft( cr.left() + margin );
            cr.setWidth( d_data->colorBar.width );
            break;
        }

        case Bottom:
        {
            cr.setTop( cr.top() + margin );
            cr.setHeight( d_data->colorBar.width );
            break;
        }

        case Top:
        {
            cr.setTop( cr.bottom() - margin
                - d_data->colorBar.width );
            cr.setHeight( d_data->colorBar.width );
            break;
        }
    }

    return cr;
}

void VipAxisColorMap::addItem(VipPlotItem * item)
{
	if(d_data->plotItems.indexOf(item) < 0)
	{
		d_data->plotItems.append(item);
		computeScaleDiv();
	}
}

void VipAxisColorMap::removeItem(VipPlotItem * item)
{
	if(d_data->plotItems.removeAll(item))
	{
		computeScaleDiv();
	}
}


void VipAxisColorMap::setColorBarEnabled( bool on )
{
    if ( on != d_data->colorBar.isEnabled )
    {
        d_data->colorBar.isEnabled = on;
        layoutScale();
    }
}


bool VipAxisColorMap::isColorBarEnabled() const
{
    return d_data->colorBar.isEnabled;
}


void VipAxisColorMap::setColorBarWidth( double width )
{
    if ( width != d_data->colorBar.width )
    {
        d_data->colorBar.width = width;
        if ( isColorBarEnabled() )
            layoutScale();
    }
}


double VipAxisColorMap::colorBarWidth() const
{
    return d_data->colorBar.width;
}

void VipAxisColorMap::setColorMapInterval(const VipInterval & interval)
{
	d_data->colorBar.interval = interval;
}


VipInterval VipAxisColorMap::colorMapInterval() const
{
    return d_data->colorBar.interval;
}

void VipAxisColorMap::setUseFlatHistogram(bool enable)
{
	if (d_data->colorBar.colorMap->mapType() == VipColorMap::Linear)
	{
		VipLinearColorMap * map = static_cast<VipLinearColorMap*>(d_data->colorBar.colorMap);
		if (map->useFlatHistogram() != enable)
		{
			map->setUseFlatHistogram(enable);
			Q_EMIT useFlatHistogramChanged(enable);
		}
	}
}
bool VipAxisColorMap::useFlatHistogram() const
{
	if (d_data->colorBar.colorMap->mapType() == VipColorMap::Linear)
		return static_cast<VipLinearColorMap*>(d_data->colorBar.colorMap)->useFlatHistogram();
	return false;
}

void VipAxisColorMap::setFlatHistogramStrength(int strength)
{
	if (d_data->colorBar.colorMap->mapType() == VipColorMap::Linear)
	{
		VipLinearColorMap* map = static_cast<VipLinearColorMap*>(d_data->colorBar.colorMap);
		if (map->flatHistogramStrength() != strength)
		{
			map->setFlatHistogramStrength(strength);
			Q_EMIT useFlatHistogramChanged(useFlatHistogram());
		}
	}
}
int VipAxisColorMap::flatHistogramStrength() const
{
	if (d_data->colorBar.colorMap->mapType() == VipColorMap::Linear)
		return static_cast<VipLinearColorMap*>(d_data->colorBar.colorMap)->flatHistogramStrength();
	return false;
}


void VipAxisColorMap::setColorMap(
    const VipInterval &interval, VipColorMap *colorMap )
{
    d_data->colorBar.interval = interval;

    if ( colorMap != d_data->colorBar.colorMap )
    {
        delete d_data->colorBar.colorMap;
        d_data->colorBar.colorMap = colorMap;
    }

    if ( isColorBarEnabled() )
        layoutScale();

	emitScaleNeedUpdate();

	int type = (d_data->colorBar.colorMap->mapType() == VipColorMap::Linear) ?
		static_cast<VipLinearColorMap*>(d_data->colorBar.colorMap)->type() :
		VipLinearColorMap::Unknown;

	Q_EMIT colorMapChanged(type);
}

void VipAxisColorMap::setColorMap(
	const VipInterval &interval, VipLinearColorMap::StandardColorMap map)
{
	bool flat = useFlatHistogram();
	int str = flatHistogramStrength();
	setColorMap(interval, VipLinearColorMap::createColorMap(map));
	setUseFlatHistogram(flat);
	setFlatHistogramStrength(str);
}

void VipAxisColorMap::setColorMap(VipLinearColorMap::StandardColorMap map)
{
	setColorMap(this->gripInterval(), map);
}

void VipAxisColorMap::setColorMap(VipColorMap* map)
{
	setColorMap(this->gripInterval(), map);
}


VipColorMap *VipAxisColorMap::colorMap() const
{
    return const_cast<VipColorMap*>(d_data->colorBar.colorMap);
}

void VipAxisColorMap::setGripInterval(const VipInterval & interval)
{
	grip1()->blockSignals(true);
	grip2()->blockSignals(true);

	grip1()->setValue(interval.minValue());
	grip2()->setValue(interval.maxValue());
	d_data->colorBar.interval = gripInterval();
	this->update();

	grip1()->blockSignals(false);
	grip2()->blockSignals(false);
}

VipInterval VipAxisColorMap::gripInterval() const
{
	return VipInterval( grip1()->value(), grip2()->value() ).normalized();
}

VipColorMapGrip * VipAxisColorMap::grip1()
{
	return d_data->grip_1;
}

const VipColorMapGrip * VipAxisColorMap::grip1() const
{
	return d_data->grip_1;
}

VipColorMapGrip * VipAxisColorMap::grip2()
{
	return d_data->grip_2;
}

const VipColorMapGrip * VipAxisColorMap::grip2() const
{
	return d_data->grip_2;
}

VipColorMapGrip * VipAxisColorMap::addGrip(const QImage & img)
{
	VipColorMapGrip *grip = new VipColorMapGrip(this);
	if(!img.isNull())
		grip->setImage(img);
	return addGrip(grip);
}

VipColorMapGrip * VipAxisColorMap::addGrip(VipColorMapGrip * grip)
{
	removeGrip(grip);
	connect(grip,SIGNAL(valueChanged(double)),this,SLOT(gripValueChanged(double)));
	d_data->grips.append( grip );
	return grip;
}

void VipAxisColorMap::removeGrip(VipColorMapGrip* grip)
{
	if( d_data->grips.removeAll( grip ) )
		disconnect(grip,SIGNAL(valueChanged(double)),this,SLOT(gripValueChanged(double)));
}

QList<VipColorMapGrip*> VipAxisColorMap::grips() const
{
	return d_data->grips;
}

void VipAxisColorMap::gripValueChanged(double value)
{
	if(sender() == grip1() || sender() == grip2())
	{
		d_data->colorBar.interval = gripInterval();
		this->update();
	}
	Q_EMIT valueChanged(value);
}

void VipAxisColorMap::setAutoScaleMax(vip_double value)
{
	d_data->autoScaleMax = value;
	emitScaleDivNeedUpdate();
}
vip_double VipAxisColorMap::autoScaleMax() const
{
	return d_data->autoScaleMax;
}
bool VipAxisColorMap::hasAutoScaleMax() const
{
	return d_data->hasAutoScaleMax;
}
void VipAxisColorMap::setHasAutoScaleMax(bool enable)
{
	d_data->hasAutoScaleMax = enable;
	emitScaleDivNeedUpdate();
}
void VipAxisColorMap::setAutoScaleMin(vip_double value)
{
	d_data->autoScaleMin = value;
	emitScaleDivNeedUpdate();
}
vip_double VipAxisColorMap::autoScaleMin() const
{
	return d_data->autoScaleMin;
}
bool VipAxisColorMap::hasAutoScaleMin() const
{
	return d_data->hasAutoScaleMin;
}
void VipAxisColorMap::setHasAutoScaleMin(bool enable)
{
	d_data->hasAutoScaleMin = enable;
	emitScaleDivNeedUpdate();
}

void VipAxisColorMap::startRender(VipRenderState & state)
{
	//save the grip visibility
	state.state(this)["grip1"] = grip1()->isVisible();
	state.state(this)["grip2"] = grip2()->isVisible();

	//hide grips
	grip1()->setVisible(false);
	grip2()->setVisible(false);
}

void VipAxisColorMap::endRender(VipRenderState & state)
{
	//restore the grip visibility
	grip1()->setVisible(state.state(this)["grip1"].value<bool>());
	grip2()->setVisible(state.state(this)["grip2"].value<bool>());
}
