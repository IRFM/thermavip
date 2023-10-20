/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

#include "VipLongDouble.h"
#include "VipNDArrayImage.h"
#include "VipSceneModel.h"

/// @}
// end DataType

#endif
