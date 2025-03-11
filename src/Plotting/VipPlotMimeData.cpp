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

#include "VipPlotMimeData.h"
#include "VipPlotItem.h"
#include <QPointer>

class VipPlotMimeData::PrivateData
{
public:
	QList<QPointer<VipPlotItem>> plotData;
};

VipPlotMimeData::VipPlotMimeData()
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	setText("VipPlotMimeData");
}

VipPlotMimeData::~VipPlotMimeData()
{
}

void VipPlotMimeData::setPlotData(const QList<VipPlotItem*>& items)
{
	d_data->plotData.clear();
	for (int i = 0; i < items.size(); ++i)
		d_data->plotData << items[i];
}

QList<VipPlotItem*> VipPlotMimeData::plotData(VipPlotItem* drop_target, QWidget*) const
{
	Q_UNUSED(drop_target)
	QList<VipPlotItem*> res;
	for (int i = 0; i < d_data->plotData.size(); ++i)
		if (d_data->plotData[i])
			res << d_data->plotData[i].data();
	return res;
}

VipCoordinateSystem::Type VipPlotMimeData::coordinateSystemType() const
{
	for (int i = 0; i < d_data->plotData.size(); ++i)
		if (d_data->plotData[i])
			return d_data->plotData[i]->coordinateSystemType();
	return VipCoordinateSystem::Null;
}
