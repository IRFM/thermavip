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

#ifndef VIP_PY_SIGNAL_FUSION_PROCESSING_H
#define VIP_PY_SIGNAL_FUSION_PROCESSING_H

#include "VipStandardProcessing.h"
#include "VipPyProcessing.h"


/**
Data fusion processing that takes as input multiple VipPointVector signals,
and applies a Python processing to the x components and y components.

Within these Python scripts, 'x' and 'y' variables refer to the output x and y values,
and variables 'x0', 'x1',...,'y0','y1',... refer to the input signals x and y.

The processing applies a different Python script to the x and y components.
*/
class VIP_CORE_EXPORT VipPySignalFusionProcessing : public VipPyBaseProcessing
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty x_algo)
	VIP_IO(VipProperty y_algo)
	VIP_IO(VipProperty output_title)
	VIP_IO(VipProperty output_unit)
	VIP_IO(VipProperty output_x_unit)
	Q_CLASSINFO("description",
		    "Apply a python script based on given input signals.\n"
		    "This processing only takes 1D + time signals as input, and create a new output using\n"
		    "a Python script for the x components and the y components.")
	Q_CLASSINFO("category", "Miscellaneous")
public:
	VipPySignalFusionProcessing(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return DisplayOnSameSupport; }
	virtual QVariant initializeProcessing(const QVariant& v);
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return qMetaTypeId<VipPointVector>() == v.userType(); }
	virtual bool useEventLoop() const { return true; }
	bool registerThisProcessing(const QString& category, const QString& name, const QString& description, bool overwrite = true);

protected:
	virtual void mergeData(int, int);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPySignalFusionProcessing*)
typedef QSharedPointer<VipPySignalFusionProcessing> VipPySignalFusionProcessingPtr;
Q_DECLARE_METATYPE(VipPySignalFusionProcessingPtr);

#endif
