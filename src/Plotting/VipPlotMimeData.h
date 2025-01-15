/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_PLOT_MIME_DATA_H
#define VIP_PLOT_MIME_DATA_H

#include <QList>
#include <QMimeData>

#include "VipCoordinateSystem.h"
#include "VipGlobals.h"

/// \addtogroup Plotting
/// @{

class VipPlotItem;

/// @brief Base class for mime data involving VipPlotItem objects
///
/// VipPlotMimeData is a QMimeData used to drag and drop VipPlotItem objects.
/// VipPlotItem::startDragging() member internally create a VipPlotMimeData that might be dropped on any other VipPlotItem object.
class VIP_PLOTTING_EXPORT VipPlotMimeData : public QMimeData
{
	Q_OBJECT

public:
	VipPlotMimeData();
	virtual ~VipPlotMimeData();

	/// @brief Set the VipPlotItem objects to drag & drop
	void setPlotData(const QList<VipPlotItem*>&);

	/// @brief Returns the VipPlotItem objects to drop on a specific target
	/// @param drop_target drop VipPlotItem target, might be null
	/// @param drop_widget drop widget, might be null
	/// @return dropped/created VipPlotItem objects
	virtual QList<VipPlotItem*> plotData(VipPlotItem* drop_target, QWidget* drop_widget) const;

	/// @brief Returns the coordinate system of carried VipPlotItem objects
	virtual VipCoordinateSystem::Type coordinateSystemType() const;

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
// end Plotting

#endif
