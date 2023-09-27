#include <limits>
#include <qalgorithms.h>
#include <qmath.h>
#include <float.h>

#include "VipScaleEngine.h"
#include "VipValueTransform.h"
#include "VipScaleMap.h"
#include "VipScaleDraw.h"
#include "VipAbstractScale.h"

static const vip_double _eps = sizeof(vip_double) > sizeof(double) ? 1.0e-8 : 1.0e-6;

/// Ceil a value, relative to an interval
///
/// \param value Value to vipCeil
/// \param intervalSize VipInterval size
///
/// \sa floorEps()
vip_double VipScaleArithmetic::ceilEps( vip_double value,
    vip_double intervalSize )
{
    const vip_double eps = _eps * intervalSize;

    value = ( value - eps ) / intervalSize;
    return ::vipCeil( value ) * intervalSize;
}

/// Floor a value, relative to an interval
///
/// \param value Value to vipFloor
/// \param intervalSize VipInterval size
///
/// \sa floorEps()
vip_double VipScaleArithmetic::floorEps( vip_double value, vip_double intervalSize )
{
    const vip_double eps = _eps * intervalSize;

    value = ( value + eps ) / intervalSize;
    return ::vipFloor( value ) * intervalSize;
}

/// \brief Divide an interval into steps
///
/// \f$stepSize = (intervalSize - intervalSize * 10e^{-6}) / numSteps\f$
///
/// \param intervalSize VipInterval size
/// \param numSteps Number of steps
/// \return Step size
vip_double VipScaleArithmetic::divideEps( vip_double intervalSize, vip_double numSteps )
{
    if ( numSteps == 0.0 || intervalSize == 0.0 )
        return 0.0;

    return ( intervalSize - ( _eps * intervalSize ) ) / numSteps;
}

/// Find the smallest value out of {1,2,5}*10^n with an integer number n
/// which is greater than or equal to x
///
/// \param x VipInput value
vip_double VipScaleArithmetic::ceil125( vip_double x )
{
    if ( x == 0.0 )
        return 0.0;

    const vip_double sign = ( x > 0 ) ? 1.0 : -1.0;
	const vip_double lx = ::log10(::vipAbs(x));
    const vip_double p10 = ::vipFloor( lx );

    vip_double fr = (vip_double)::pow( (vip_double)10.0, lx - p10 );
    if ( fr <= 1.0 )
        fr = 1.0;
    else if ( fr <= 2.0 )
        fr = 2.0;
    else if ( fr <= 5.0 )
        fr = 5.0;
    else
        fr = 10.0;

    return sign * fr * (vip_double)::pow((vip_double)10.0, p10 );
}

/// \brief Find the largest value out of {1,2,5}*10^n with an integer number n
/// which is smaller than or equal to x
///
/// \param x VipInput value
vip_double VipScaleArithmetic::floor125( vip_double x )
{
    if ( x == 0.0 )
        return 0.0;

    vip_double sign = ( x > 0 ) ? 1.0 : -1.0;
    const vip_double lx = ::log10( ::vipAbs( x ) );
    const vip_double p10 = ::vipFloor( lx );

    vip_double fr = (vip_double)::pow((vip_double)10.0, lx - p10 );
    if ( fr >= 10.0 )
        fr = 10.0;
    else if ( fr >= 5.0 )
        fr = 5.0;
    else if ( fr >= 2.0 )
        fr = 2.0;
    else
        fr = 1.0;

    return sign * fr * (vip_double)::pow((vip_double)10.0, p10 );
}

class VipScaleEngine::PrivateData
{
public:
    PrivateData():
        attributes( VipScaleEngine::NoAttribute ),
        lowerMargin( 0.0 ),
        upperMargin( 0.0 ),
        referenceValue( 0.0 )
    {
    }

    VipScaleEngine::Attributes attributes;       // scale attributes

    vip_double lowerMargin;      // margins
    vip_double upperMargin;

    vip_double referenceValue; // reference value

};

//! Constructor
VipScaleEngine::VipScaleEngine()
{
    d_data = new PrivateData;
}


//! Destructor
VipScaleEngine::~VipScaleEngine ()
{
    delete d_data;
}

/// \return the margin at the lower end of the scale
/// The default margin is 0.
///
/// \sa setMargins()
vip_double VipScaleEngine::lowerMargin() const
{
    return d_data->lowerMargin;
}

/// \return the margin at the upper end of the scale
/// The default margin is 0.
///
/// \sa setMargins()
vip_double VipScaleEngine::upperMargin() const
{
    return d_data->upperMargin;
}

/// \brief Specify margins at the scale's endpoints
/// \param lower minimum vipDistance between the scale's lower boundary and the
///          smallest enclosed value
/// \param upper minimum vipDistance between the scale's upper boundary and the
///          greatest enclosed value
///
/// VipMargins can be used to leave a minimum amount of space between
/// the enclosed intervals and the boundaries of the scale.
///
/// \warning
/// \li VipLog10ScaleEngine measures the margins in decades.
///
/// \sa upperMargin(), lowerMargin()

void VipScaleEngine::setMargins( vip_double lower, vip_double upper )
{
    d_data->lowerMargin = qMax( lower, (vip_double)0.0 );
    d_data->upperMargin = qMax( upper, (vip_double)0.0 );
}

/// Calculate a step size for an interval size
///
/// \param intervalSize VipInterval size
/// \param numSteps Number of steps
///
/// \return Step size
vip_double VipScaleEngine::divideInterval(
    vip_double intervalSize, int numSteps ) const
{
    if ( numSteps <= 0 )
        return 0.0;

    vip_double v = VipScaleArithmetic::divideEps( intervalSize, numSteps );
    return VipScaleArithmetic::ceil125( v );
}

/// Check if an interval "contains" a value
///
/// \param interval VipInterval
/// \param value Value
///
/// \sa VipScaleArithmetic::compareEps()
bool VipScaleEngine::contains(
    const VipInterval &interval, vip_double value ) const
{
    if ( !interval.isValid() )
        return false;

    if ( vipFuzzyCompare( value, interval.minValue(), interval.width() ) < 0 )
        return false;

    if ( vipFuzzyCompare( value, interval.maxValue(), interval.width() ) > 0 )
        return false;

    return true;
}

/// Remove ticks from a list, that are not inside an interval
///
/// \param ticks Tick list
/// \param interval VipInterval
///
/// \return Stripped tick list
VipScaleDiv::TickList VipScaleEngine::strip( const VipScaleDiv::TickList& ticks,
    const VipInterval &interval ) const
{
    if ( !interval.isValid() || ticks.count() == 0 )
        return VipScaleDiv::TickList();

    if ( contains( interval, ticks.first() )
        && contains( interval, ticks.last() ) )
    {
        return ticks;
    }

	VipScaleDiv::TickList strippedTicks;
	strippedTicks.reserve(ticks.count());
    for ( int i = 0; i < ticks.count(); i++ )
    {
        if ( contains( interval, ticks[i] ) )
            strippedTicks += ticks[i];
    }
    return strippedTicks;
}

/// \brief Build an interval for a value
///
/// In case of v == 0.0 the interval is [-0.5, 0.5],
/// otherwide it is [0.5 * v, 1.5 * v]

VipInterval VipScaleEngine::buildInterval( vip_double v ) const
{
    const vip_double delta = ( v == 0.0 ) ? 0.5 : vipAbs( 0.5 * v );

    if ( DBL_MAX - delta < v )
        return VipInterval( DBL_MAX - delta, DBL_MAX );

    if ( -DBL_MAX + delta > v )
        return VipInterval( -DBL_MAX, -DBL_MAX + delta );

    return VipInterval( v - delta, v + delta );
}

/// Change a scale attribute
///
/// \param attribute Attribute to change
/// \param on On/Off
///
/// \sa Attribute, testAttribute()
void VipScaleEngine::setAttribute( Attribute attribute, bool on )
{
    if ( on )
        d_data->attributes |= attribute;
    else
        d_data->attributes &= ~attribute;
}

/// Check if a attribute is set.
///
/// \param attribute Attribute to be tested
/// \sa Attribute, setAttribute()
bool VipScaleEngine::testAttribute( Attribute attribute ) const
{
    return ( d_data->attributes & attribute );
}

/// Change the scale attribute
///
/// \param attributes Set scale attributes
/// \sa Attribute, attributes()
void VipScaleEngine::setAttributes( Attributes attributes )
{
    d_data->attributes = attributes;
}

/// Return the scale attributes
/// \sa Attribute, setAttributes(), testAttribute()
VipScaleEngine::Attributes VipScaleEngine::attributes() const
{
    return d_data->attributes;
}

/// \brief Specify a reference point
/// \param r new reference value
///
/// The reference point is needed if options IncludeReference or
/// Symmetric are active. Its default value is 0.0.
///
/// \sa Attribute
void VipScaleEngine::setReference( vip_double r )
{
    d_data->referenceValue = r;
}

/// \return the reference value
/// \sa setReference(), setAttribute()
vip_double VipScaleEngine::reference() const
{
    return d_data->referenceValue;
}

/// Return a transformation, for linear scales
VipValueTransform *VipLinearScaleEngine::transformation() const
{
	return NULL;// new NullTransform();
}

/// Align and divide an interval
///
/// \param maxNumSteps Max. number of steps
/// \param x1 First limit of the interval (In/Out)
/// \param x2 Second limit of the interval (In/Out)
/// \param stepSize Step size (Out)
///
/// \sa setAttribute()
void VipLinearScaleEngine::autoScale( int maxNumSteps,
    vip_double &x1, vip_double &x2, vip_double &stepSize ) const
{
    VipInterval interval( x1, x2 );
    interval = interval.normalized();

    interval.setMinValue( interval.minValue() - lowerMargin() );
    interval.setMaxValue( interval.maxValue() + upperMargin() );

    if ( testAttribute( VipScaleEngine::Symmetric ) )
        interval = interval.symmetrize( reference() );

    if ( testAttribute( VipScaleEngine::IncludeReference ) )
        interval = interval.extend( reference() );

    if ( interval.width() == 0.0 )
        interval = buildInterval( interval.minValue() );

    stepSize = divideInterval( interval.width(), qMax( maxNumSteps, 1 ) );

    if ( !testAttribute( VipScaleEngine::Floating ) )
        interval = align( interval, stepSize );

    x1 = interval.minValue();
    x2 = interval.maxValue();

    if ( testAttribute( VipScaleEngine::Inverted ) )
    {
        qSwap( x1, x2 );
        stepSize = -stepSize;
    }
}

/// \brief Calculate a scale division
///
/// \param x1 First interval limit
/// \param x2 Second interval limit
/// \param maxMajSteps Maximum for the number of major steps
/// \param maxMinSteps Maximum number of minor steps
/// \param stepSize Step size. If stepSize == 0, the scaleEngine
///                calculates one.
///
/// \sa VipScaleEngine::stepSize(), VipScaleEngine::subDivide()
VipScaleDiv VipLinearScaleEngine::divideScale( vip_double x1, vip_double x2,
    int maxMajSteps, int maxMinSteps, vip_double stepSize ) const
{
    VipInterval interval = VipInterval( x1, x2 ).normalized();
    if ( interval.width() <= 0 )
        return VipScaleDiv();

    stepSize = vipAbs( stepSize );
    if ( stepSize == 0.0 )
    {
        if ( maxMajSteps < 1 )
            maxMajSteps = 1;

        stepSize = divideInterval( interval.width(), maxMajSteps );
    }

    VipScaleDiv scaleDiv;

    if ( stepSize != 0.0 )
    {
		VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes];
        buildTicks( interval, stepSize, maxMinSteps, ticks );

        scaleDiv = VipScaleDiv( interval, ticks );
    }

    if ( x1 > x2 )
        scaleDiv.invert();

    return scaleDiv;
}

/// \brief Calculate ticks for an interval
///
/// \param interval VipInterval
/// \param stepSize Step size
/// \param maxMinSteps Maximum number of minor steps
/// \param ticks Arrays to be filled with the calculated ticks
///
/// \sa buildMajorTicks(), buildMinorTicks
void VipLinearScaleEngine::buildTicks(
    const VipInterval& interval, vip_double stepSize, int maxMinSteps,
	VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes] ) const
{
    const VipInterval boundingInterval = align( interval, stepSize );

    ticks[VipScaleDiv::MajorTick] =
        buildMajorTicks( boundingInterval, stepSize );

    if ( maxMinSteps > 0 )
    {
        buildMinorTicks( ticks[VipScaleDiv::MajorTick], maxMinSteps, stepSize,
            ticks[VipScaleDiv::MinorTick], ticks[VipScaleDiv::MediumTick] );
    }

    for ( int i = 0; i < VipScaleDiv::NTickTypes; i++ )
    {
        ticks[i] = strip( ticks[i], interval );

        // ticks very close to 0.0 are
        // explicitely set to 0.0

        for ( int j = 0; j < ticks[i].count(); j++ )
        {
            if ( vipFuzzyCompare( ticks[i][j], (vip_double)0.0, stepSize ) == 0 )
                ticks[i][j] = 0.0;
        }
    }
}

/// \brief Calculate major ticks for an interval
///
/// \param interval VipInterval
/// \param stepSize Step size
///
/// \return Calculated ticks
VipScaleDiv::TickList VipLinearScaleEngine::buildMajorTicks(
    const VipInterval &interval, vip_double stepSize ) const
{
    int numTicks = qRound( interval.width() / stepSize ) + 1;
    if ( numTicks > 10000 )
        numTicks = 10000;

	VipScaleDiv::TickList ticks;
	ticks.reserve(numTicks +2);

    ticks += interval.minValue();
    for ( int i = 1; i < numTicks - 1; i++ )
        ticks += interval.minValue() + i * stepSize;
    ticks += interval.maxValue();

    return ticks;
}

/// \brief Calculate minor/medium ticks for major ticks
///
/// \param majorTicks Major ticks
/// \param maxMinSteps Maximum number of minor steps
/// \param stepSize Step size
/// \param minorTicks Array to be filled with the calculated minor ticks
/// \param mediumTicks Array to be filled with the calculated medium ticks
///
void VipLinearScaleEngine::buildMinorTicks(
    const VipScaleDiv::TickList& majorTicks,
    int maxMinSteps, vip_double stepSize,
	VipScaleDiv::TickList &minorTicks,
	VipScaleDiv::TickList &mediumTicks ) const
{
    vip_double minStep = divideInterval( stepSize, maxMinSteps );
    if ( minStep == 0.0 )
        return;

    // # ticks per interval
    int numTicks = ::vipCeil( vipAbs( stepSize / minStep ) ) - 1;

    // Do the minor steps fit into the interval?
    if ( vipFuzzyCompare( ( numTicks +  1 ) * vipAbs( minStep ),
        vipAbs( stepSize ), stepSize ) > 0 )
    {
        numTicks = 1;
        minStep = stepSize * 0.5;
    }

    int medIndex = -1;
    if ( numTicks % 2 )
        medIndex = numTicks / 2;

    // calculate minor ticks
	mediumTicks.reserve(majorTicks.count() * numTicks);
	minorTicks.reserve(majorTicks.count() * numTicks);

    for ( int i = 0; i < majorTicks.count(); i++ )
    {
        vip_double val = majorTicks[i];
        for ( int k = 0; k < numTicks; k++ )
        {
            val += minStep;

            vip_double alignedValue = val;
            if ( vipFuzzyCompare( val, (vip_double)0.0, stepSize ) == 0 )
                alignedValue = 0.0;

            if ( k == medIndex )
                mediumTicks += alignedValue;
            else
                minorTicks += alignedValue;
        }
    }
}

/// \brief Align an interval to a step size
///
/// The limits of an interval are aligned that both are integer
/// multiples of the step size.
///
/// \param interval VipInterval
/// \param stepSize Step size
///
/// \return Aligned interval
VipInterval VipLinearScaleEngine::align(
    const VipInterval &interval, vip_double stepSize ) const
{
    vip_double x1 = interval.minValue();
    vip_double x2 = interval.maxValue();

    if ( -DBL_MAX + stepSize <= x1 )
    {
        const vip_double x = VipScaleArithmetic::floorEps( x1, stepSize );
        if ( vipFuzzyCompare( x1, x, stepSize ) != 0 )
            x1 = x;
    }

    if ( DBL_MAX - stepSize >= x2 )
    {
        const vip_double x = VipScaleArithmetic::ceilEps( x2, stepSize );
        if ( vipFuzzyCompare( x2, x, stepSize ) != 0 )
            x2 = x;
    }

    return VipInterval( x1, x2 );
}

/// Return a transformation, for logarithmic (base 10) scales
VipValueTransform *VipLog10ScaleEngine::transformation() const
{
    return new LogTransform(  );
}

/// Align and divide an interval
///
/// \param maxNumSteps Max. number of steps
/// \param x1 First limit of the interval (In/Out)
/// \param x2 Second limit of the interval (In/Out)
/// \param stepSize Step size (Out)
///
/// \sa VipScaleEngine::setAttribute()
void VipLog10ScaleEngine::autoScale( int maxNumSteps,
    vip_double &x1, vip_double &x2, vip_double &stepSize ) const
{
    if ( x1 > x2 )
        qSwap( x1, x2 );

    if (x1 <= 0) {
		if(x2 > 0)
			x1 = LOG_MIN;
		else {
			x1 = LOG_MIN;
			x2 = 10;
		}
	}

    VipInterval interval( x1 / (vip_double)::pow((vip_double)10.0, lowerMargin() ),
        x2 * (vip_double)::pow((vip_double)10.0, upperMargin() ) );

    if ( interval.maxValue() / interval.minValue() < (vip_double)10.0 )
    {
        // scale width is less than one decade -> build linear scale

        VipLinearScaleEngine linearScaler;
        linearScaler.setAttributes( attributes() );
        linearScaler.setReference( reference() );
        linearScaler.setMargins( lowerMargin(), upperMargin() );

        linearScaler.autoScale( maxNumSteps, x1, x2, stepSize );

        if ( stepSize < 0.0 )
            stepSize = -::log10( vipAbs( stepSize ) );
        else
            stepSize = ::log10( stepSize );

        return;
    }

    vip_double logRef = 1.0;
    if ( reference() > LOG_MIN / 2 )
        logRef = qMin( reference(), (vip_double)LOG_MAX / 2 );

    if ( testAttribute( VipScaleEngine::Symmetric ) )
    {
        const vip_double delta = qMax( interval.maxValue() / logRef,
            logRef / interval.minValue() );
        interval.setInterval( logRef / delta, logRef * delta );
    }

    if ( testAttribute( VipScaleEngine::IncludeReference ) )
        interval = interval.extend( logRef );

    interval = interval.limited( LOG_MIN, LOG_MAX );

    if ( interval.width() == 0.0 )
        interval = buildInterval( interval.minValue() );

    //find the best minimum value if 0 (avoid LOG_MIN which ends up messing the whole scale)
    if(interval.minValue() == LOG_MIN && interval.maxValue() > 0)
    {
    	int p = ::vipCeil( ::log10(interval.maxValue()) ); //upper power of 10
    	int missing = qMax(maxNumSteps - p, 0); //missing power of 10
    	vip_double min = (vip_double)::pow((vip_double)10.0,-missing);
    	interval.setMinValue(min);
    }

    if (interval.maxValue() >= 1000.) interval.setMinValue(qMax(interval.minValue(), (vip_double)0.1));
	else if (interval.maxValue() >= 100.) interval.setMinValue( qMax(interval.minValue(), (vip_double)0.01));
	else if (interval.maxValue() >= 10.) interval.setMinValue( qMax(interval.minValue(), (vip_double)0.001));

    stepSize = divideInterval( log10( interval ).width(), qMax( maxNumSteps, 1 ) );
    if ( stepSize < 1.0 )
        stepSize = 1.0;

    if ( !testAttribute( VipScaleEngine::Floating ) )
        interval = align( interval, stepSize );

    x1 = interval.minValue();
    x2 = interval.maxValue();

    if ( testAttribute( VipScaleEngine::Inverted ) )
    {
        qSwap( x1, x2 );
        stepSize = -stepSize;
    }
}

/// \brief Calculate a scale division
///
/// \param x1 First interval limit
/// \param x2 Second interval limit
/// \param maxMajSteps Maximum for the number of major steps
/// \param maxMinSteps Maximum number of minor steps
/// \param stepSize Step size. If stepSize == 0, the scaleEngine
///                calculates one.
///
/// \sa VipScaleEngine::stepSize(), VipLog10ScaleEngine::subDivide()
VipScaleDiv VipLog10ScaleEngine::divideScale( vip_double x1, vip_double x2,
    int maxMajSteps, int maxMinSteps, vip_double stepSize ) const
{
	if (x1 <= 0) {
		if(x2 > 0)
			x1 = LOG_MIN;
		else {
			x1 = LOG_MIN;
			x2 = 10;
		}
	}

    VipInterval interval = VipInterval( x1, x2 ).normalized();
    interval = interval.limited( LOG_MIN, LOG_MAX );

    if ( interval.width() <= 0 )
        return VipScaleDiv();

    //find the best minimum value if 0 (avoid LOG_MIN which ends up messing the whole scale)
	if(interval.minValue() == LOG_MIN && interval.maxValue() > 0)
	{
		int p = ::vipCeil( ::log10(interval.maxValue()) ); //upper power of 10
		int missing = qMax(maxMajSteps - p, 0); //missing power of 10
		vip_double min = (vip_double)::pow((vip_double)10.0,-missing);
		interval.setMinValue(min);
	}

    if ( interval.maxValue() / interval.minValue() < 10.0 )
    {
        // scale width is less than one decade -> build linear scale

        VipLinearScaleEngine linearScaler;
        linearScaler.setAttributes( attributes() );
        linearScaler.setReference( reference() );
        linearScaler.setMargins( lowerMargin(), upperMargin() );

        if ( stepSize != 0.0 )
        {
            if ( stepSize < 0.0 )
                stepSize = -(vip_double)::pow((vip_double)10.0, -stepSize );
            else
                stepSize = (vip_double)::pow((vip_double)10.0, stepSize );
        }

        return linearScaler.divideScale( x1, x2,
            maxMajSteps, maxMinSteps, stepSize );
    }

    stepSize = vipAbs( stepSize );
    if ( stepSize == 0.0 )
    {
        if ( maxMajSteps < 1 )
            maxMajSteps = 1;

        stepSize = divideInterval( log10( interval ).width(), maxMajSteps );
        if ( stepSize < 1.0 )
            stepSize = 1.0; // major step must be >= 1 decade
    }

    VipScaleDiv scaleDiv;
    if ( stepSize != 0.0 )
    {
		VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes];
        buildTicks( interval, stepSize, maxMinSteps, ticks );

        scaleDiv = VipScaleDiv( interval, ticks );
    }

    if ( x1 > x2 )
        scaleDiv.invert();

    return scaleDiv;
}

/// \brief Calculate ticks for an interval
///
/// \param interval VipInterval
/// \param maxMinSteps Maximum number of minor steps
/// \param stepSize Step size
/// \param ticks Arrays to be filled with the calculated ticks
///
/// \sa buildMajorTicks(), buildMinorTicks
void VipLog10ScaleEngine::buildTicks(
    const VipInterval& interval, vip_double stepSize, int maxMinSteps,
	VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes] ) const
{
    const VipInterval boundingInterval = align( interval, stepSize );

    ticks[VipScaleDiv::MajorTick] =
        buildMajorTicks( boundingInterval, stepSize );

    if ( maxMinSteps > 0 )
    {
        ticks[VipScaleDiv::MinorTick] = buildMinorTicks(
            ticks[VipScaleDiv::MajorTick], maxMinSteps, stepSize );
    }

    for ( int i = 0; i < VipScaleDiv::NTickTypes; i++ )
        ticks[i] = strip( ticks[i], interval );
}

/// \brief Calculate major ticks for an interval
///
/// \param interval VipInterval
/// \param stepSize Step size
///
/// \return Calculated ticks
VipScaleDiv::TickList VipLog10ScaleEngine::buildMajorTicks(
    const VipInterval &interval, vip_double stepSize ) const
{
    vip_double width = log10( interval ).width();

    int numTicks = qRound( width / stepSize ) + 1;
    if ( numTicks > 10000 )
        numTicks = 10000;

    const vip_double lxmin = ::log( interval.minValue() );
    const vip_double lxmax = ::log( interval.maxValue() );
    const vip_double lstep = ( lxmax - lxmin ) / vip_double( numTicks - 1 );

	VipScaleDiv::TickList ticks;
	ticks.reserve(numTicks + 2);

    ticks += interval.minValue();

    for ( int i = 1; i < numTicks - 1; i++ )
        ticks += ::exp( lxmin + vip_double( i ) * lstep );

    ticks += interval.maxValue();

    return ticks;
}

/// \brief Calculate minor/medium ticks for major ticks
///
/// \param majorTicks Major ticks
/// \param maxMinSteps Maximum number of minor steps
/// \param stepSize Step size
VipScaleDiv::TickList VipLog10ScaleEngine::buildMinorTicks(
    const VipScaleDiv::TickList &majorTicks,
    int maxMinSteps, vip_double stepSize ) const
{
    if ( stepSize < 1.1 )          // major step width is one decade
    {
        if ( maxMinSteps < 1 )
            return VipScaleDiv::TickList();

        int k0, kstep, kmax;

        if ( maxMinSteps >= 8 )
        {
            k0 = 2;
            kmax = 9;
            kstep = 1;
        }
        else if ( maxMinSteps >= 4 )
        {
            k0 = 2;
            kmax = 8;
            kstep = 2;
        }
        else if ( maxMinSteps >= 2 )
        {
            k0 = 2;
            kmax = 5;
            kstep = 3;
        }
        else
        {
            k0 = 5;
            kmax = 5;
            kstep = 1;
        }

		VipScaleDiv::TickList minorTicks;
		if (kstep)
			minorTicks.reserve(majorTicks.count() * (kmax - k0) / kstep + 1);

        for ( int i = 0; i < majorTicks.count(); i++ )
        {
            const vip_double v = majorTicks[i];
            for ( int k = k0; k <= kmax; k += kstep )
                minorTicks += v * vip_double( k );
        }

        return minorTicks;
    }
    else  // major step > one decade
    {
        vip_double minStep = divideInterval( stepSize, maxMinSteps );
        if ( minStep == 0.0 )
            return VipScaleDiv::TickList();

        if ( minStep < 1.0 )
            minStep = 1.0;

        // # subticks per interval
        int nMin = qRound( stepSize / minStep ) - 1;

        // Do the minor steps fit into the interval?

        if ( vipFuzzyCompare( ( nMin +  1 ) * minStep,
            vipAbs( stepSize ), stepSize ) > 0 )
        {
            nMin = 0;
        }

        if ( nMin < 1 )
            return VipScaleDiv::TickList();      // no subticks

        // substep factor = 10^substeps
        const vip_double minFactor = qMax( (vip_double)::pow((vip_double)10.0, minStep ), (vip_double)( 10.0 ) );

		VipScaleDiv::TickList minorTicks;
		minorTicks.reserve(majorTicks.count() *nMin);

        for ( int i = 0; i < majorTicks.count(); i++ )
        {
            vip_double val = majorTicks[i];
            for ( int k = 0; k < nMin; k++ )
            {
                val *= minFactor;
                minorTicks += val;
            }
        }
        return minorTicks;
    }
}

/// \brief Align an interval to a step size
///
/// The limits of an interval are aligned that both are integer
/// multiples of the step size.
///
/// \param interval VipInterval
/// \param stepSize Step size
///
/// \return Aligned interval
VipInterval VipLog10ScaleEngine::align(
    const VipInterval &interval, vip_double stepSize ) const
{
    const VipInterval intv = log10( interval );

    vip_double x1 = VipScaleArithmetic::floorEps( intv.minValue(), stepSize );
    if ( vipFuzzyCompare( interval.minValue(), x1, stepSize ) == 0 )
        x1 = interval.minValue();

    vip_double x2 = VipScaleArithmetic::ceilEps( intv.maxValue(), stepSize );
    if ( vipFuzzyCompare( interval.maxValue(), x2, stepSize ) == 0 )
        x2 = interval.maxValue();

    return pow10( VipInterval( x1, x2 ) );
}

/// Return the interval [log10(interval.minValue(), log10(interval.maxValue]

VipInterval VipLog10ScaleEngine::log10( const VipInterval &interval ) const
{
    return VipInterval( ::log10( interval.minValue() ),
            ::log10( interval.maxValue() ) );
}

/// Return the interval [pow10(interval.minValue(), pow10(interval.maxValue]
VipInterval VipLog10ScaleEngine::pow10( const VipInterval &interval ) const
{
    return VipInterval( (vip_double)::pow((vip_double)10.0, interval.minValue() ),
    		(vip_double)::pow((vip_double)10.0, interval.maxValue() ) );
}








VipFixedScaleEngine::VipFixedScaleEngine(VipFixedValueToText* vt)
  : d_vt(vt)
  , d_maxIntervalWidth(vipNan())
{
}

void VipFixedScaleEngine::setMaxIntervalWidth(double v) {
	d_maxIntervalWidth = v;
}
double VipFixedScaleEngine::maxIntervalWidth() const noexcept
{
	return d_maxIntervalWidth;
}

void VipFixedScaleEngine::onComputeScaleDiv(VipAbstractScale* scale, const VipInterval& items_interval)
{
	if (d_vt) {
		if (d_vt->startValue() == items_interval.minValue()) 
			scale->setOptimizeFromStreaming(false);
		else
			scale->setOptimizeFromStreaming(true,0.1);

        double start = items_interval.minValue();
		if (!vipIsNan(d_maxIntervalWidth) && items_interval.width() > d_maxIntervalWidth) 
            start = items_interval.maxValue() - d_maxIntervalWidth;
	    d_vt->setStartValue(start);		
	}
}

void VipFixedScaleEngine::autoScale(int maxSteps, vip_double& x1, vip_double& x2, vip_double& stepSize) const
{
	if (!d_vt)
		VipLinearScaleEngine::autoScale(maxSteps, x1, x2, stepSize);
	else {

		vip_double x = x1;
		vip_double _x1 = 0, _x2 = x2 - x1;
		if (!vipIsNan(d_maxIntervalWidth) && _x2 > d_maxIntervalWidth) 
			_x2 = d_maxIntervalWidth;
		VipLinearScaleEngine::autoScale(maxSteps, _x1, _x2, stepSize);
		
		x1 = x;
		x2 = _x2 + x;
	}
}
VipScaleDiv VipFixedScaleEngine::divideScale(vip_double x1, vip_double x2, int maxMajSteps, int maxMinSteps, vip_double stepSize ) const
{
	if (!d_vt)
		return VipLinearScaleEngine::divideScale(x1 , x2 , maxMajSteps, maxMinSteps, stepSize);

    double _x1 = x1 - d_vt->startValue();
	double _x2 = x2 - d_vt->startValue();
    if (!vipIsNan(d_maxIntervalWidth) && (x2 - x1) > d_maxIntervalWidth)
	    _x1 = _x2 - d_maxIntervalWidth;

	VipScaleDiv div = VipLinearScaleEngine::divideScale(_x1,_x2, maxMajSteps, maxMinSteps, stepSize);

	VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes];
	ticks[0] = div.ticks(VipScaleDiv::MinorTick);
	ticks[1] = div.ticks(VipScaleDiv::MediumTick);
	ticks[2] = div.ticks(VipScaleDiv::MajorTick);
	VipInterval interval = div.bounds();

	for (int i = 0; i < ticks[0].size(); ++i)
		ticks[0][i] += d_vt->startValue();
	for (int i = 0; i < ticks[1].size(); ++i)
		ticks[1][i] += d_vt->startValue();
	for (int i = 0; i < ticks[2].size(); ++i)
		ticks[2][i] += d_vt->startValue();
	interval.setMaxValue(interval.maxValue() + d_vt->startValue());
	interval.setMinValue(interval.minValue() + d_vt->startValue());

	return VipScaleDiv(interval, ticks);
}








void VipDateTimeScaleEngine::onComputeScaleDiv(VipAbstractScale * scale, const VipInterval & items_interval)
{
	//set start date

	if (!m_vt)
		return;
	if (m_vt->type % 2 &&
		m_vt->valueToTextType() == VipValueToText::ValueToTime &&
		m_vt->displayType != VipValueToTime::AbsoluteDateTime)
	{
		if (m_vt->fixedStartValue)
		{
			m_vt->startValue = items_interval.minValue();
		}
		else
		{
			VipInterval inter = scale->scaleDiv().bounds();
			m_vt->startValue = inter.minValue();
		}
	}
	else  {
		m_vt->startValue = items_interval.minValue();
	}
}

//static qint64 _year_2000 = QDateTime::fromString("2000", "yyyy").toMSecsSinceEpoch();
void VipDateTimeScaleEngine::autoScale(int maxSteps, vip_double &x1, vip_double &x2, vip_double &stepSize) const
{
	if (m_vt)
	{
		if (m_vt->type % 2)
		{
			//case Since Epoch:
			//Make the scale start exactly at the minimum value

			vip_double x = x1;

			vip_double _x1 = 0, _x2 = x2 - x1;
			VipLinearScaleEngine::autoScale(maxSteps, _x1, _x2, stepSize);
			x1 = x;
			x2 = _x2 + x;
			return;

		}
	}
	VipLinearScaleEngine::autoScale(maxSteps, x1, x2, stepSize);
}

VipScaleDiv VipDateTimeScaleEngine::divideScale(vip_double x1, vip_double x2,
	int maxMajSteps, int maxMinSteps,
	vip_double stepSize ) const
{
	if (m_vt)
	{
		if (m_vt->type % 2 )
		{
			if (m_vt->fixedStartValue)
			{
				VipScaleDiv div = VipLinearScaleEngine::divideScale(x1 - m_vt->startValue, x2 - m_vt->startValue, maxMajSteps, maxMinSteps, stepSize);

				//printf("sc: %f to %f, in %f, ti %i\n", (double)(x1 - m_vt->startValue), (double)(x2 - m_vt->startValue), (double(x2 - x1)), div.ticks(2).size());

				VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes];
				ticks[0] = div.ticks(VipScaleDiv::MinorTick);
				ticks[1] = div.ticks(VipScaleDiv::MediumTick);
				ticks[2] = div.ticks(VipScaleDiv::MajorTick);
				VipInterval interval = div.bounds();

				for (int i = 0; i < ticks[0].size(); ++i)
					ticks[0][i] += m_vt->startValue;
				for (int i = 0; i < ticks[1].size(); ++i)
					ticks[1][i] += m_vt->startValue;
				for (int i = 0; i < ticks[2].size(); ++i)
					ticks[2][i] += m_vt->startValue;
				interval.setMaxValue(interval.maxValue() + m_vt->startValue);
				interval.setMinValue(interval.minValue() + m_vt->startValue);

				return VipScaleDiv(interval, ticks);
			}
			else
			{
				VipScaleDiv div = VipLinearScaleEngine::divideScale(0, x2 - x1, maxMajSteps, maxMinSteps, stepSize);
				VipScaleDiv::TickList ticks[VipScaleDiv::NTickTypes];
				ticks[0] = div.ticks(VipScaleDiv::MinorTick);
				ticks[1] = div.ticks(VipScaleDiv::MediumTick);
				ticks[2] = div.ticks(VipScaleDiv::MajorTick);
				VipInterval interval = div.bounds();

				for (int i = 0; i < ticks[0].size(); ++i)
					ticks[0][i] += x1;
				for (int i = 0; i < ticks[1].size(); ++i)
					ticks[1][i] += x1;
				for (int i = 0; i < ticks[2].size(); ++i)
					ticks[2][i] += x1;
				interval.setMaxValue(interval.maxValue() + x1);
				interval.setMinValue(interval.minValue() + x1);

				return VipScaleDiv(interval, ticks);
			}
		}
	}

	return VipLinearScaleEngine::divideScale(x1, x2, maxMajSteps, maxMinSteps, stepSize);
}
