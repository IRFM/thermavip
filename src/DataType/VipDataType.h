#ifndef VIP_DATA_TYPE_H
#define VIP_DATA_TYPE_H

///\defgroup DataType DataType
///
/// The DataType library defines the data structures that are commonly manipulated within Thermavip.
/// These structures are:
/// <ul>
/// <li>complex_f: a typedef for std::complex<float>
/// <li>complex_d: a typedef for std::complex<double>
/// <li>VipInterval: a simple class representing a data interval
/// <li>VipIntervalSample: a simple class representing a data interval and an associated value
/// <li>VipPointVector: a vector of QPointF, used to represent 2D curves
/// <li>VipIntervalSampleVector: a vector of VipIntervalSample, used to represent histograms
/// <li>VipNDArray and its derived classes (VipNDArrayView, VipNDArrayType and VipNDArrayTypeView) representing a dynamic N-dimension array of any type.
/// Standard images are represented with the VipNDArray class.
/// <li>VipShape and VipSceneModel representing arbitray 2D shapes mainly used to extract statistical data inside sub-part of images.
/// <\ul>
///
/// All of these types can be stored into QVariant objects. Most of them are serializable through QDataStream and QTextStream, and convertible to QString
/// or QByteArray.
///
/// The DataType library only depends on \qt.

/// \addtogroup DataType
/// @{

#include "VipNDArrayImage.h"
#include "VipSceneModel.h"
#include "VipLongDouble.h"

/// @}
//end DataType

#endif
