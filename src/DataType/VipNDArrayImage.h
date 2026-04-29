/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_NDARRAY_IMAGE_H
#define VIP_NDARRAY_IMAGE_H

#include "VipNDArray.h"

/// \addtogroup DataType
/// @{

/// Returns an array containing a QImage.
/// VipNDArray::dataType() will return qMetaTypeId<QImage>(). The image is always converted to the format QImage::Format_ARGB32.
VIP_DATA_TYPE_EXPORT VipNDArray vipToArray(const QImage& image);
/// Returns the QImage object contained in \a array if the array contains a QImage.
/// Returns a null image if the array does not contain a QImage.
/// Returned image always has the format QImage::Format_ARGB32.
VIP_DATA_TYPE_EXPORT QImage vipToImage(const VipNDArray& array);
/// Returns true if \a array contains a QImage.
VIP_DATA_TYPE_EXPORT bool vipIsImageArray(const VipNDArray& ar);

inline VipNDArrayTypeView<VipRGB> vipQImageView(QImage& img)
{
	img = img.convertToFormat(QImage::Format_ARGB32);
	return VipNDArrayTypeView<VipRGB>((VipRGB*)(img.bits()), vipVector(img.height(), img.width()));
}

/// @}
// end DataType

#endif
