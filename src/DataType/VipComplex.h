#pragma once

#include <complex>
#include <qmetatype.h>

/// \addtogroup DataType
/// @{

typedef std::complex<float> complex_f;
typedef std::complex<double> complex_d;

Q_DECLARE_METATYPE(complex_f)
Q_DECLARE_METATYPE(complex_d)

/// @}
//end DataType
