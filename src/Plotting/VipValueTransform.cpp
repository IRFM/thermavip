
#include <cmath>

#include "VipValueTransform.h"




//! Smallest allowed value for logarithmic scales: 1.0e-150
QT_STATIC_CONST_IMPL vip_double LogTransform::LogMin = 1.0e-150;

//! Largest allowed value for logarithmic scales: 1.0e150
QT_STATIC_CONST_IMPL vip_double LogTransform::LogMax = 1.0e150;

//! Constructor
VipValueTransform::VipValueTransform()
{
}

//! Destructor
VipValueTransform::~VipValueTransform()
{
}

/// \param value Value to be bounded
/// \return value unmodified
vip_double VipValueTransform::bounded( vip_double value ) const
{
    return value;
}

//! Constructor
NullTransform::NullTransform():
    VipValueTransform()
{
}

//! Destructor
NullTransform::~NullTransform()
{
}

/// \param value Value to be transformed
/// \return value unmodified
vip_double NullTransform::transform( vip_double value ) const
{
    return value;
}

/// \param value Value to be transformed
/// \return value unmodified
vip_double NullTransform::invTransform( vip_double value ) const
{
    return value;
}

//! \return Clone of the transformation
VipValueTransform *NullTransform::copy() const
{
    return new NullTransform();
}

//! Constructor
LogTransform::LogTransform():
    VipValueTransform()
{
}

//! Destructor
LogTransform::~LogTransform()
{
}

/// \param value Value to be transformed
/// \return log( value )
vip_double LogTransform::transform( vip_double value ) const
{
	 return std::log( (value < LOG_MIN ? LOG_MIN : value) );
}

/// \param value Value to be transformed
/// \return exp( value )
vip_double LogTransform::invTransform( vip_double value ) const
{
    return std::exp( value );
}

/// \param value Value to be bounded
/// \return qBound( LogMin, value, LogMax )
vip_double LogTransform::bounded( vip_double value ) const
{
    return qBound( LogMin, value, LogMax );
}

//! \return Clone of the transformation
VipValueTransform *LogTransform::copy() const
{
    return new LogTransform();
}

/// Constructor
/// \param exponent Exponent
PowerTransform::PowerTransform( vip_double exponent ):
    VipValueTransform(),
    d_exponent( exponent )
{
}

//! Destructor
PowerTransform::~PowerTransform()
{
}

/// \param value Value to be transformed
/// \return Exponentiation preserving the sign
vip_double PowerTransform::transform( vip_double value ) const
{
    if ( value < 0.0 )
        return -std::pow( -value, 1.0 / d_exponent );
    else
        return std::pow( value, 1.0 / d_exponent );

}

/// \param value Value to be transformed
/// \return Inverse exponentiation preserving the sign
vip_double PowerTransform::invTransform( vip_double value ) const
{
    if ( value < 0.0 )
        return -std::pow( -value, d_exponent );
    else
        return std::pow( value, d_exponent );
}

//! \return Clone of the transformation
VipValueTransform *PowerTransform::copy() const
{
    return new PowerTransform( d_exponent );
}
