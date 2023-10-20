#include "VipPlotHistogram.h"
#include "VipAxisBase.h"
#include "VipPainter.h"
#include "VipToolTip.h"
#include "VipPlotWidget2D.h"

#include <qstring.h>
#include <qpainter.h>




static int registerHistogramKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		QMap<QByteArray, int> style;
		style["lines"] = VipPlotHistogram::Lines;
		style["outline"] = VipPlotHistogram::Outline;
		style["columns"] = VipPlotHistogram::Columns;
		
		keywords["style"] = VipParserPtr(new EnumOrParser(style));
		keywords["text-alignment"] = VipParserPtr(new EnumOrParser(VipStandardStyleSheet::alignmentEnum()));
		keywords["text-position"] = VipParserPtr(new EnumParser(VipStandardStyleSheet::regionPositionEnum()));
		keywords["text-distance"] = VipParserPtr(new DoubleParser());

		keywords["border-radius"] = VipParserPtr(new DoubleParser());

		vipSetKeyWordsForClass(&VipPlotHistogram::staticMetaObject, keywords);
	}
	return 0;
}

static int _registerHistogramKeyWords = registerHistogramKeyWords();




static inline bool isCombinable( const VipInterval &d1,
    const VipInterval &d2 )
{
    if ( d1.isValid() && d2.isValid() )
    {
        if ( d1.maxValue() == d2.minValue() )
        {
            if ( !( d1.borderFlags() & VipInterval::ExcludeMaximum
                && d2.borderFlags() & VipInterval::ExcludeMinimum ) )
            {
                return true;
            }
        }
    }

    return false;
}

class VipPlotHistogram::PrivateData
{
public:
    PrivateData():
		baseline( 0.0 ),
		style( Columns ),
		textAlignment(Qt::AlignTop|Qt::AlignHCenter),
		textPosition(Vip::Outside),
		textDistance(5)
    {
    }

	vip_double baseline;

    VipBoxStyle boxStyle;
    VipPlotHistogram::HistogramStyle style;
    QList<VipInterval> bounding;
	QList<VipInterval> boundingInterval;

    VipInterval plotInterval;
	VipInterval plotValidInterval;

    Qt::Alignment textAlignment;
	Vip::RegionPositions textPosition;
    QTransform textTransform ;
    QPointF textTransformReference ;
	double textDistance;
	VipText text;
	QSharedPointer<VipTextStyle> textStyle;
};


/// Constructor
/// \param title Title of the histogram.
VipPlotHistogram::VipPlotHistogram( const VipText &title )
:VipPlotItemDataType( title )
{
	d_data = new PrivateData();
	this->setData( QVariant::fromValue(VipIntervalSampleVector()) );
	this->boxStyle().setBackgroundBrush(QBrush(QColor(Qt::blue)));
}

//! Destructor
VipPlotHistogram::~VipPlotHistogram()
{
    delete d_data;
}


void VipPlotHistogram::setStyle( HistogramStyle style )
{
    if ( style != d_data->style )
    {
        d_data->style = style;

        emitItemChanged();
    }
}


VipPlotHistogram::HistogramStyle VipPlotHistogram::style() const
{
    return d_data->style;
}

void VipPlotHistogram::setBoxStyle(const VipBoxStyle & bs)
{
	d_data->boxStyle = bs;
	emitItemChanged();
}

const VipBoxStyle & VipPlotHistogram::boxStyle() const
{
	return d_data->boxStyle;
}

VipBoxStyle & VipPlotHistogram::boxStyle()
{
	return d_data->boxStyle;
}


void VipPlotHistogram::setBaseline(vip_double value )
{
    if ( d_data->baseline != value )
    {
        d_data->baseline = value;
		{
			dataLock()->lock();
			d_data->bounding.clear();
			dataLock()->unlock();
		}
        emitItemChanged();
    }
}

vip_double VipPlotHistogram::baseline() const
{
    return d_data->baseline;
}

void VipPlotHistogram::setTextAlignment(Qt::Alignment align)
{
	d_data->textAlignment = align;
	emitItemChanged();
}

Qt::Alignment VipPlotHistogram::textAlignment() const
{
	return d_data->textAlignment;
}

void VipPlotHistogram::setTextPosition(Vip::RegionPositions pos)
{
	d_data->textPosition = pos;
	emitItemChanged();
}

Vip::RegionPositions VipPlotHistogram::textPosition() const
{
	return d_data->textPosition;
}


void VipPlotHistogram::setTextTransform(const QTransform& tr, const QPointF& ref)
{
	d_data->textTransform = tr;
	d_data->textTransformReference = ref;
	emitItemChanged();
}
const QTransform& VipPlotHistogram::textTransform() const
{
	return d_data->textTransform;
}
const QPointF& VipPlotHistogram::textTransformReference() const
{
	return d_data->textTransformReference;
}


void VipPlotHistogram::setTextDistance(double vipDistance)
{
	d_data->textDistance = vipDistance;
	emitItemChanged();
}

double VipPlotHistogram::textDistance() const
{
	return d_data->textDistance;
}

void VipPlotHistogram::setText(const VipText& text)
{
	d_data->text = text;
	if (d_data->textStyle)
		d_data->text.setTextStyle(*d_data->textStyle);
	// no need to mark style sheet dirty
	emitItemChanged(true, true, true, false);
}

const VipText & VipPlotHistogram::text() const
{
	return d_data->text;
}

VipText & VipPlotHistogram::text()
{
	return d_data->text;
}

QList<VipInterval> VipPlotHistogram::dataBoundingIntervals(const VipIntervalSampleVector & data, vip_double baseline)
{
	if(!data.size())
		return QList<VipInterval>();

	const VipIntervalSample first = data[0];
	vip_double x_min = first.interval.minValue();
	vip_double x_max = first.interval.maxValue();
	vip_double y_min = baseline;
	vip_double y_max = first.value;
	if (first.value < baseline)
		std::swap(y_min, y_max);

	for (int i = 1; i < data.size(); ++i)
	{
		const VipIntervalSample s_interval = data[i];
		x_min = std::min(s_interval.interval.minValue(), x_min);
		x_max = std::max(s_interval.interval.maxValue(), x_max);
		if (s_interval.value > baseline)
			y_max = std::max(y_max, s_interval.value);
		else
			y_min = std::min(y_min, s_interval.value);
	}
	return QList<VipInterval>() << VipInterval(x_min, x_max) << VipInterval(y_min, y_max);
}

VipInterval VipPlotHistogram::plotInterval(const VipInterval & interval) const
{
	if(!d_data->plotInterval.isValid() || d_data->plotValidInterval != interval)
	{
		Locker locker(dataLock());
		const VipIntervalSampleVector data = rawData();
		if(!data.size())
			return VipInterval();

		VipInterval inter = VipInterval(data[0].value,data[0].value);
		for(int i=1; i < data.size(); ++i)
			inter = inter.extend(data[i].value);

		const_cast<PrivateData*>(d_data)->plotInterval = inter;
		const_cast<PrivateData*>(d_data)->plotValidInterval = interval;
	}

	return d_data->plotInterval;
}

QList<VipInterval> VipPlotHistogram::plotBoundingIntervals() const
{
	Locker locker(dataLock());
	QList<VipInterval> res = d_data->bounding;
	if(res.isEmpty())
	{
		res = const_cast<PrivateData*>(d_data)->bounding = dataBoundingIntervals(rawData(),baseline());
	}
	res.detach();
	return res;
}

void VipPlotHistogram::setData( const QVariant & data)
{
	VipPlotItemDataType::setData(data);
	Locker locker(dataLock());
	d_data->bounding = dataBoundingIntervals(rawData(), baseline());//.clear();
	const VipIntervalSampleVector vec = data.value<VipIntervalSampleVector>();
	if (vec.size()) {
		VipInterval inter = VipInterval(vec[0].value, vec[0].value);
		for (int i = 1; i < vec.size(); ++i)
			inter = inter.extend(vec[i].value);
		(d_data)->plotInterval = inter;
	}
	//d_data->plotInterval = VipInterval();
	//d_data->plotValidInterval = VipInterval();
}



bool VipPlotHistogram::areaOfInterest(const QPointF & pos, int //axis
, double maxDistance, VipPointVector & //out_pos
, VipBoxStyle & style, int & legend) const
{
	Locker locker(dataLock());

	QRectF found;
	int index = -1;
	double dist = std::numeric_limits<double>::max();
	legend = 0;

	//try to find a sample at distance < maxDistance (in item's coordinates)
	const VipIntervalSampleVector data = rawData();
	for (int i = 0; i < data.size(); ++i)
	{
		QRectF rect(sceneMap()->transform(QPointF(data[i].interval.minValue(), baseline())),
			sceneMap()->transform(QPointF(data[i].interval.maxValue(), data[i].value)));
		rect = rect.normalized();
		QRectF adjusted = rect.adjusted(-maxDistance, -maxDistance, maxDistance, maxDistance);
		if (!adjusted.contains(pos))
			continue;

		double d = 0;
		if (adjusted.height() > adjusted.width()) d = qAbs(adjusted.center().x() - pos.x());
		else d = qAbs(adjusted.center().y() - pos.y());

		if (d < maxDistance && d < dist)
		{
			dist = d;
			index = i;
			found = rect;
		}
	}

	if (index >= 0)
	{
		QPainterPath p;
		p.addRect(found);
		p = p.intersected(sceneMap()->clipPath(this));
		p.closeSubpath();
		style.computePath(p);
		QColor color(Qt::red);
		style.setBorderPen(QPen(color, 2));
		color.setAlpha(125);
		style.setBackgroundBrush(QBrush(color));
		return true;
	}

	return false;
}

QString VipPlotHistogram::formatSampleText(const QString & str, const VipIntervalSample & s) const
{
	QString res = VipText::replace(str, "#value", s.value);
	res = VipText::replace(res, "#max", s.interval.maxValue());
	res = VipText::replace(res, "#min", s.interval.minValue());
	return res;
}

QString VipPlotHistogram::formatText(const QString & str, const QPointF & pos) const
{
	VipText res = VipPlotItem::formatText(str, pos);

	const double dist = area() ? area()->plotToolTip()->distanceToPointer() : 0;

	//we need to replace #min, #max and #value

	//try to find a sample at distance < maxDistance (in item's coordinates)
	Locker lock(dataLock());
	const VipIntervalSampleVector data = rawData();
	for (int i = 0; i < data.size(); ++i)
	{
		QRectF rect(sceneMap()->transform(QPointF(data[i].interval.minValue(), baseline())),
			sceneMap()->transform(QPointF(data[i].interval.maxValue(), data[i].value)));
		rect = rect.normalized().adjusted(-dist,-dist,dist,dist);
		if (rect.contains(pos))
		{
			res = formatSampleText(res.text(), data[i]);
			break;
		}
	}

	return res.text();
}


/// Draw a subset of the histogram samples
///
/// \param painter VipPainter
/// \param xMap Maps x-values into pixel coordinates.
/// \param yMap Maps y-values into pixel coordinates.
/// \param canvasRect Contents rectangle of the canvas
/// \param from Index of the first sample to be painted
/// \param to Index of the last sample to be painted. If to < 0 the
///      series will be painted to its last sample.
///
/// \sa drawOutline(), drawLines(), drawColumns
void VipPlotHistogram::draw(QPainter * painter, const VipCoordinateSystemPtr & m) const
{
    if ( !painter )
        return;

    switch ( d_data->style )
    {
        case Outline:
            drawOutline( painter, m );
            break;
        case Lines:
            drawLines( painter, m );
            break;
        case Columns:
            drawColumns( painter, m );
            break;
        default:
            break;
    }

    //draw the texts
	if(! d_data->text.isEmpty())
	{
		VipIntervalSampleVector data = this->rawData();

		for ( int i = 0; i < data.size(); ++i )
		{
			const VipIntervalSample sample = data[i];
			if ( !sample.interval.isNull() )
			{
				
				VipText t = formatSampleText(d_data->text.text(), sample);// d_data->text.formatText(//d_data->text.sprintf((double)sample.value);
				t.setTextStyle(d_data->text.textStyle());
				t.setLayoutAttributes(d_data->text.layoutAttributes());
				//draw text
				QPointF p1(sample.interval.minValue(), baseline());
				QPointF p2(sample.interval.maxValue(), sample.value);
				QRectF geom = QRectF(m->transform(p1), m->transform(p2)).normalized(); //m->transform(QRectF(xLeft, yTop, qAbs(xRight - xLeft), qAbs(yBottom - yTop))).boundingRect();
				//geom = m->transformRect(geom);
				VipPainter::drawText(painter, t, textTransform(), textTransformReference(), textDistance(), textPosition(), textAlignment(), geom);															   
				

			}
		}
	}
}



void VipPlotHistogram::setTextStyle(const VipTextStyle& st)
{
	d_data->textStyle.reset(new VipTextStyle(st));
	d_data->text.setTextStyle(st);
	emitItemChanged();
}


QList<VipText> VipPlotHistogram::legendNames() const
{
	return QList<VipText>() << title();
}

QRectF VipPlotHistogram::drawLegend(QPainter * painter, const QRectF & r, int //index
) const
{
	QRectF square = vipInnerSquare(r);
	
	VipBoxStyle bstyle = d_data->boxStyle;
	bstyle.setBorderRadius(0);
	if (style() != Lines)
		bstyle.computeRect(square);
	else
		bstyle.computePolyline(QPolygonF() << QPointF(square.left(), square.center().y()) << QPointF(square.right(),square.center().y()));
	if (style() != Columns)
		bstyle.setBackgroundBrush(QBrush());
	bstyle.draw(painter);
	
	return square;
}


/// Draw a histogram in Outline style()
///
/// \param painter VipPainter
/// \param xMap Maps x-values into pixel coordinates.
/// \param yMap Maps y-values into pixel coordinates.
/// \param from Index of the first sample to be painted
/// \param to Index of the last sample to be painted. If to < 0 the
///      histogram will be painted to its last point.
///
/// \sa setStyle(), style()
/// \warning The outline style requires, that the intervals are in increasing
///        order and not overlapping.
void VipPlotHistogram::drawOutline( QPainter *painter, const VipCoordinateSystemPtr & m) const
{
	const VipIntervalSampleVector data = this->rawData();
	if(!data.size())
		return;

    VipPointVector polygon;

    VipIntervalSample previous = data[0];
    polygon << VipPoint(previous.interval.minValue(),baseline())
    		<< VipPoint(previous.interval.minValue(),previous.value);

    for ( int i = 1; i < data.size(); ++i )
    {
    	const VipIntervalSample sample = data[i];

    	//same side of the baseline
    	if( (previous.value >= baseline() && sample.value >= baseline()) ||
    			(previous.value < baseline() && sample.value < baseline()) )
    	{
    		//disjoint bart
    		if(previous.interval.maxValue() < sample.interval.minValue())
    		{
    			polygon << VipPoint( previous.interval.maxValue(), previous.value  )
						<< VipPoint( previous.interval.maxValue(), baseline() )
						<< VipPoint( sample.interval.minValue(), baseline() )
						<< VipPoint( sample.interval.minValue(), sample.value );
    		}
    		//current sample contained in previous with higher value
    		//else if(sample.interval.maxValue() <= previous.interval.maxValue())
    		// {
    		// if(sample.value > previous.value && previous.value > baseline())
    		// {
    		// polygon << QPointF(sample.interval.minValue(), previous.value)
						// << QPointF(sample.interval.minValue(), sample.value)
						// << QPointF(sample.interval.maxValue(), sample.value)
    		// }
    		// }



    		else if( (sample.value >= previous.value && previous.value >= baseline()) ||
    				(sample.value <= previous.value && previous.value <= baseline()) )
    		{
    			polygon << VipPoint( sample.interval.minValue(), previous.value  )
    					<< VipPoint( sample.interval.minValue(), sample.value );
    		}
    		else
    		{
    			polygon << VipPoint( previous.interval.maxValue(), previous.value  )
    			    	<< VipPoint( previous.interval.maxValue(), sample.value );
    		}
    	}
    	else
    	{
			polygon << VipPoint( previous.interval.maxValue(), previous.value  )
					<< VipPoint( previous.interval.maxValue(), baseline() )
					<< VipPoint( sample.interval.minValue(), baseline() )
					<< VipPoint( sample.interval.minValue(), sample.value );

    	}

    	previous = sample;

    }

    polygon << VipPoint(data.last().interval.maxValue(),data.last().value)
    		<< VipPoint(data.last().interval.maxValue(),baseline());

	VipBoxStyle bstyle = d_data->boxStyle;
	bstyle.computePolyline(m->transform(polygon));
	bstyle.draw(painter);
}

/// Draw a histogram in Columns style()
///
/// \param painter VipPainter
/// \param xMap Maps x-values into pixel coordinates.
/// \param yMap Maps y-values into pixel coordinates.
/// \param from Index of the first sample to be painted
/// \param to Index of the last sample to be painted. If to < 0 the
///      histogram will be painted to its last point.
///
/// \sa setStyle(), style(), setSymbol(), drawColumn()
void VipPlotHistogram::drawColumns( QPainter *painter, const VipCoordinateSystemPtr & m) const
{
    const VipIntervalSampleVector data = this->rawData();
    VipBoxStyle bs = d_data->boxStyle;
	QPen pen = bs.borderPen();

    for ( int i = 0; i < data.size(); ++i )
    {
        const VipIntervalSample sample = data[i];
        if ( sample.interval.isValid() )
        {
        	const VipPointVector sampleRect = columnRect( sample );
            const QPolygonF rect = m->transform(sampleRect);
            bs.computeQuadrilateral(rect);

			QBrush brush = bs.backgroundBrush();

			if (sample.interval.width() == 0 && (pen.style() == Qt::NoPen || pen.color().alpha() == 0))
			{
				bs.setBorderPen(brush.color());
				bs.drawBorder(painter);
			}
			else
			{
				//bs.setBorderPen(pen);
				if (colorMap()) {
					brush.setColor(color(sample.value, brush.color()));
					bs.setBackgroundBrush(brush);
				}
				bs.drawBackground(painter);
				bs.drawBorder(painter);
			}

        }
    }
}

/// Draw a histogram in Lines style()
///
/// \param painter VipPainter
/// \param xMap Maps x-values into pixel coordinates.
/// \param yMap Maps y-values into pixel coordinates.
/// \param from Index of the first sample to be painted
/// \param to Index of the last sample to be painted. If to < 0 the
///      histogram will be painted to its last point.
///
/// \sa setStyle(), style(), setPen()
void VipPlotHistogram::drawLines( QPainter *painter, const VipCoordinateSystemPtr & m) const
{
    const VipIntervalSampleVector data = this->rawData();
    VipBoxStyle bstyle = d_data->boxStyle;
    QPainterPath path;

    for ( int i = 0; i < data.size(); ++i )
    {
        const VipIntervalSample sample = data[i];
        if ( !sample.interval.isNull() )
        {
        	QPolygonF p(2);
        	p[0] = m->transform(VipPoint(sample.interval.minValue(),sample.value));
        	p[1] = m->transform(VipPoint(sample.interval.maxValue(),sample.value));

        	path.addPolygon(p);
        }
    }

    bstyle.computePath(path);
    bstyle.drawBorder(painter);

}



bool VipPlotHistogram::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;
	
	if (strcmp(name, "text-alignment") == 0) {
		setTextAlignment((Qt::Alignment)value.toInt());
		return true;
	}
	if (strcmp(name, "text-position") == 0) {
		setTextPosition((Vip::RegionPositions)value.toInt());
		return true;
	}
	if (strcmp(name, "text-distance") == 0) {
		setTextDistance(value.toDouble());
		return true;
	}
	if (strcmp(name, "border-radius") == 0) {
		boxStyle().setBorderRadius(value.toDouble());
		boxStyle().setRoundedCorners(Vip::AllCorners);
		return true;
	}
	if (strcmp(name, "style") == 0) {
		setStyle((HistogramStyle)value.toInt());
		return true;
	}
	
	return VipPlotItem::setItemProperty(name, value, index);
}


bool VipPlotHistogram::hasState(const QByteArray& state, bool enable) const
{
	// 'lines', 'columns', 'outline'.
	if (state == "lines")
		return (style() == Lines) == enable;
	if (state == "columns")
		return (style() == Columns) == enable;
	if (state == "outline")
		return (style() == Outline) == enable;
	
	return VipPlotItem::hasState(state, enable);
}



//! Internal, used by the Outline style.
//void VipPlotHistogram::flushPolygon( QPainter *, const VipCoordinateSystemPtr & m,
//  double baseLine, QPolygonF &polygon, QPolygonF & path ) const
// {
//  if ( polygon.size() == 0 )
//      return;
//
//  polygon += QPointF( polygon.last().x(), baseLine );
// polygon += QPointF( baseLine, polygon.last().y() );
// polygon += QPointF( baseLine, polygon.first().y() );
//
// path += m->transform(polygon);
//
//  polygon.clear();
// }

/// Calculate the area that is covered by a sample
///
/// \param sample Sample
/// \param xMap Maps x-values into pixel coordinates.
/// \param yMap Maps y-values into pixel coordinates.
///
/// \return Rectangle, that is covered by a sample
VipPointVector VipPlotHistogram::columnRect( const VipIntervalSample &sample, QRectF * r) const
{
	VipPointVector rect;

    const VipInterval &iv = sample.interval;
    if ( !iv.isValid() )
        return rect;

    rect.resize(4);
    rect[0] = VipPoint(iv.minValue(),sample.value);
    rect[1] = VipPoint(iv.maxValue(),sample.value);
    rect[2] = VipPoint(iv.maxValue(),baseline());
    rect[3] = VipPoint(iv.minValue(),baseline());

	if (r) {
		*r = QRectF(iv.minValue(), sample.value, iv.width(), sample.value - baseline()).normalized();
	}

    return rect;
}




VipArchive& operator<<(VipArchive& arch, const VipPlotHistogram* value)
{
	arch.content("boxStyle", value->boxStyle())
	  .content("textPosition", (int)value->textPosition())
	  .content("textDistance", value->textDistance())
	  .content("text", value->text())
	  .content("baseline", value->baseline())
	  .content("style", (int)value->style());

	// new in 4.2.0

	arch.content("textTransform", value->textTransform());
	arch.content("textTransformReference", value->textTransformReference());
	arch.content("textAlignment", value->textAlignment());
	arch.content("textDistance", value->textDistance());
	arch.content("text", value->text());

	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotHistogram* value)
{
	value->setBoxStyle(arch.read("boxStyle").value<VipBoxStyle>());
	value->setTextPosition((Vip::RegionPositions)arch.read("textPosition").value<int>());
	value->setTextDistance(arch.read("textDistance").value<double>());
	value->setText(arch.read("text").value<VipText>());
	value->setBaseline(arch.read("baseline").value<double>());
	value->setStyle((VipPlotHistogram::HistogramStyle)arch.read("style").value<int>());

	arch.save();

	QTransform textTransform = arch.read("textTransform").value<QTransform>();
	QPointF textTransformReference = arch.read("textTransformReference").value<QPointF>();
	if (arch) {

		value->setTextTransform(textTransform, textTransformReference);
		value->setTextAlignment((Qt::Alignment)arch.read("textAlignment").value<int>());
		value->setTextDistance(arch.read("textDistance").value<double>());
		value->setText(arch.read("text").value<VipText>());
	}
	else
		arch.restore();

	return arch;
}


static int registerStreamOperators()
{
	qRegisterMetaType<VipPlotHistogram*>();
	vipRegisterArchiveStreamOperators<VipPlotHistogram*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
