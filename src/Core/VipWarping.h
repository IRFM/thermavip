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

#ifndef VIP_WARPING_H
#define VIP_WARPING_H

#include "VipImageProcessing.h"
#include "VipMultiNDArray.h"
#include "VipNDArray.h"
#include "VipNDArrayImage.h"

VIP_CORE_EXPORT VipPointVector vipWarping(QVector<QPoint> pts1, QVector<QPoint> pts2, int width, int height);

/// @brief Image warping processing based on Delaunay triangulation
class VIP_CORE_EXPORT VipWarping : public VipSceneModelBasedProcessing
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("description", "Image warping based on Delaunay triangulation")

	VipPointVector m_warping;

public:
	VipWarping(QObject* parent = nullptr);

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipNDArray>(); }

	const VipPointVector& warping() const { return m_warping; }
	void setWarping(const VipPointVector& warp)
	{
		m_warping = warp;
		emitProcessingChanged();
	}

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipWarping*)

class VipArchive;

VIP_CORE_EXPORT VipArchive& operator<<(VipArchive&, const VipWarping* tr);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive&, VipWarping* tr);

#endif
