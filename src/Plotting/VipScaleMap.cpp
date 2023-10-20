
#include "VipScaleEngine.h"
#include "VipScaleMap.h"


/// \brief Constructor
///
/// The scale and paint device intervals are both set to [0,1].
VipScaleMap::VipScaleMap():
    d_s1( 0.0 ),
    d_s2( 1.0 ),
    d_p1( 0.0 ),
    d_p2( 1.0 ),
    d_cnv( 1.0 ),
    d_abs_cnv( 1.0 ),
    d_ts1( 0.0 ),
    d_transform( nullptr )
{
}

//! Copy constructor
VipScaleMap::VipScaleMap( const VipScaleMap& other ):
    d_s1( other.d_s1 ),
    d_s2( other.d_s2 ),
    d_p1( other.d_p1 ),
    d_p2( other.d_p2 ),
    d_cnv( other.d_cnv ),
    d_abs_cnv( other.d_abs_cnv ),
    d_ts1( other.d_ts1 ),
    d_transform( nullptr )
{
    if ( other.d_transform )
        d_transform = other.d_transform->copy();
}

/// Destructor
VipScaleMap::~VipScaleMap()
{
	if(d_transform)
		delete d_transform;
}

//! Assignment operator
VipScaleMap &VipScaleMap::operator=( const VipScaleMap & other )
{
    d_s1 = other.d_s1;
    d_s2 = other.d_s2;
    d_p1 = other.d_p1;
    d_p2 = other.d_p2;
    d_cnv = other.d_cnv;
    d_abs_cnv = other.d_abs_cnv;
    d_ts1 = other.d_ts1;

	if(d_transform)
		delete d_transform;
    d_transform = nullptr;

    if ( other.d_transform )
        d_transform = other.d_transform->copy();

    return *this;
}
