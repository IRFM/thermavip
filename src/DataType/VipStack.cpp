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

#include "VipStack.h"

bool vipStack(VipNDArray& dst, const VipNDArray& v1, const VipNDArray& v2, qsizetype axis)
{
	// test same dim count
	if (v1.shapeCount() != v2.shapeCount())
		return false;
	// test same shape except for given axis
	for (qsizetype i = 0; i < v1.shapeCount(); ++i) {
		if (i != axis && (v1.shape(i) != v2.shape(i) || v1.shape(i) != v2.shape(i)))
			return false;
	}

	// start position of each array
	VipNDArrayShape pos1, pos2;
	pos1.resize(v1.shapeCount());
	pos1.fill(0);
	pos2.resize(v1.shapeCount());
	pos2.fill(0);
	pos2[axis] = v1.shape(axis);

	VipNDArray view1 = dst.mid(pos1, v1.shape());
	VipNDArray view2 = dst.mid(pos2, v2.shape());
	if (!v1.convert(view1))
		return false;
	if (!v2.convert(view2))
		return false;
	return true;
}

#include <qimage.h>
VipNDArray vipStack(const VipNDArray& v1, const VipNDArray& v2, qsizetype axis)
{
	VipNDArrayShape sh = v1.shape();
	sh[axis] = v1.shape()[axis] + v2.shape()[axis];

	int t1 = v1.dataType();
	int t2 = v2.dataType();
	if (t1 == qMetaTypeId<QImage>() && t2 == qMetaTypeId<QImage>()) {
		VipNDArray res = VipNDArray(vipCreateArrayHandle(VipNDArrayHandle::Image, qMetaTypeId<QImage>(), sh));
		if (!vipStack(res, v1, v2, axis))
			res.clear();
		return res;
	}
	int type = t1 != t2 ? vipHigherArrayType(v1.dataType(), v2.dataType()) : t1;
	VipNDArray res = VipNDArray(type, sh);
	if (!vipStack(res, v1, v2, axis))
		res.clear();
	return res;
}
