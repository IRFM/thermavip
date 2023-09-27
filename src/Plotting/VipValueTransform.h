#ifndef VIP_VALUE_TRANSFORM_H
#define VIP_VALUE_TRANSFORM_H

#include "VipGlobals.h"
#include "VipDataType.h"

/// \addtogroup Plotting
/// @{

/// \brief A transformation between coordinate systems
///
/// VipValueTransform manipulates values, when being mapped between
/// the scale and the paint device coordinate system.
///
/// A transformation consists of 2 methods:
///
/// - transform
/// - invTransform
///
/// where one is is the inverse function of the other.
///
/// When p1, p2 are the boundaries of the paint device coordinates
/// and s1, s2 the boundaries of the scale, QwtScaleMap uses the
/// following calculations:
///
/// - p = p1 + ( p2 - p1 ) * ( T( s ) - T( s1 ) / ( T( s2 ) - T( s1 ) );
/// - s = invT ( T( s1 ) + ( T( s2 ) - T( s1 ) ) * ( p - p1 ) / ( p2 - p1 ) );
class  VipValueTransform
{
public:

	enum Type {
		Null,
		Log,
		Power,
		UserType = 100
	};

    VipValueTransform();
    virtual ~VipValueTransform();

    /// Returns the transformation type.
    virtual Type type() const = 0;

    /// Modify value to be a valid value for the transformation.
    /// The default implementation does nothing.
    virtual vip_double bounded( vip_double value ) const;

    /// Transformation function
///
    /// \param value Value
    /// \return Modified value
///
    /// \sa invTransform()
    virtual vip_double transform( vip_double value ) const = 0;

    /// Inverse transformation function
///
    /// \param value Value
    /// \return Modified value
///
    /// \sa transform()
    virtual vip_double invTransform( vip_double value ) const = 0;

    //! Virtualized copy operation
    virtual VipValueTransform *copy() const = 0;
};

/// \brief Null transformation
///
/// NullTransform returns the values unmodified.
///
class  VIP_PLOTTING_EXPORT NullTransform: public VipValueTransform
{
public:
    NullTransform();
    virtual ~NullTransform();

    virtual Type type() const {return Null;}
    virtual vip_double transform( vip_double value ) const;
    virtual vip_double invTransform( vip_double value ) const;

    virtual VipValueTransform *copy() const;
};
/// \brief Logarithmic transformation
///
/// LogTransform modifies the values using log() and exp().
///
/// \note In the calculations of QwtScaleMap the base of the log function
///      has no effect on the mapping. So LogTransform can be used
///      for log2(), log10() or any other logarithmic scale.
class  VIP_PLOTTING_EXPORT LogTransform: public VipValueTransform
{
public:
    LogTransform();
    virtual ~LogTransform();

    virtual Type type() const {return Log;}
    virtual vip_double transform( vip_double value ) const;
    virtual vip_double invTransform( vip_double value ) const;

    virtual vip_double bounded( vip_double value ) const;

    virtual VipValueTransform *copy() const;

    QT_STATIC_CONST vip_double LogMin;
    QT_STATIC_CONST vip_double LogMax;
};

/// \brief A transformation using pow()
///
/// PowerTransform preserves the sign of a value.
/// F.e. a transformation with a factor of 2
/// transforms a value of -3 to -9 and v.v. Thus PowerTransform
/// can be used for scales including negative values.
class  VIP_PLOTTING_EXPORT PowerTransform: public VipValueTransform
{
public:
    PowerTransform( vip_double exponent );
    virtual ~PowerTransform();

    virtual Type type() const {return Power;}
    virtual vip_double transform( vip_double value ) const;
    virtual vip_double invTransform( vip_double value ) const;

    virtual VipValueTransform *copy() const;

private:
    const vip_double d_exponent;
};


/// @}
//end Plotting

#endif
