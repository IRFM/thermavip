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

#ifndef VIP_TOOL_TIP_H
#define VIP_TOOL_TIP_H

#include "VipBoxStyle.h"
#include "VipText.h"

#include <QMargins>
#include <qlabel.h>

class VipAbstractPlotArea;
class VipAbstractScale;
class VipPlotItem;

/// \addtogroup Plotting
/// @{

/// @brief Class controlling the way tool tips are displayed in a VipAbstractPlotArea.
///
/// See VipAbstractPlotArea::setPlotToolTip()
/// 
class VIP_PLOTTING_EXPORT VipToolTip : public QObject
{
	Q_OBJECT

public:
	enum DisplayFlag
	{
		PlotArea = 0x0001,	  // VipAbstractPlotArea tool tip text as returned by #VipAbstractPlotArea::formatToolTip()
		Axes = 0x0002,		  //  Axes titles and values (only if axis has a title)
		ItemsTitles = 0x0004,	  // Title of each VipPlotItem
		ItemsLegends = 0x0008,	  // Legends of each VipPlotItem (if any)
		ItemsPos = 0x0020,	  // Item's position from #VipPlotItem::areaOfInterest() (if different from mouse position)
		ItemsProperties = 0x0040, // Item's dynamic properties (if any)
		ItemsToolTips = 0x0080,	  // Item's custom tool tip text, as returned by #VipPlotItem::formatToolTip() (replace all the previous item's tooltips)
		SearchXAxis = 0x0100,
		SearchYAxis = 0x0200,
		Hidden = 0x0400,
		All = PlotArea | Axes | ItemsTitles | ItemsLegends | ItemsPos | ItemsProperties | ItemsToolTips | SearchXAxis | SearchYAxis
	};
	Q_DECLARE_FLAGS(DisplayFlags, DisplayFlag);

	VipToolTip(QObject* parent = nullptr);
	virtual ~VipToolTip();

	/// @brief Set the parent VipAbstractPlotArea.
	/// This is automatically called in VipAbstractPlotArea::setPlotToolTip().
	void setPlotArea(VipAbstractPlotArea*);
	VipAbstractPlotArea* plotArea() const;

	/// @brief Set the minimum time between calls to VipToolTip::refresh().
	/// This is usefull for streaming curves/images/..., when the tool tip
	/// should be updated even if the mouse is not moving.
	void setMinRefreshTime(int milli);
	int minRefreshTime() const;

	/// @brief Set the maximum number of lines the tool tip can display.
	void setMaxLines(int max_lines);
	int maxLines() const;

	/// @brief Set the message to be displayed at the end of the tool tip
	/// if we reach the maximum number of lines.
	void setMaxLineMessage(const QString& line_msg);
	QString maxLineMessage() const;

	/// @brief Set the margins in pixels between the tooltip borders and the inner text.
	void setMargins(const QMargins&);
	void setMargins(double);
	const QMargins& margins() const;


	void setDelayTime(int msec);
	int delayTime() const;

	void setDisplayFlags(DisplayFlags);
	void setDisplayFlag(DisplayFlag, bool on = true);
	bool testDisplayFlag(DisplayFlag) const;
	DisplayFlags displayFlags() const;

	/// Set the scales that should appear in the tool tip.
	///  Note that the scales are ignored if 'Axes' flag is not set.
	void setScales(const QList<VipAbstractScale*>& scales);
	const QList<VipAbstractScale*>& scales() const;

	/// Set the tool tip position. Default is Automatic (the tool tip follow the mouse position).
	/// If the region position is not automatic, the tool tip position will be fixed (inside or outside the plot area).
	/// Use #VipToolTip::setAlignment to have a better control on the tool tip position.
	void setRegionPositions(Vip::RegionPositions pos);
	Vip::RegionPositions regionPositions() const;

	/// Set the tool tip alignment. The result will depend on the region position.
	/// This alignment is ignored if the region position is set to automatic.
	void setAlignment(Qt::Alignment);
	Qt::Alignment alignment() const;

	/// Set the tool tip top left corner offset based on the mouse position.
	/// If you set this option, the tool tip will always have a fixed position depending the mouse one,
	/// no matter what are the values of regionPositions() and alignment().
	/// Use removeToolTipOffset() to revert back to the standard tool tip position managment (based on regionPositions() and alignment()).
	void setToolTipOffset(const QPoint&);
	QPoint toolTipOffset() const;
	void removeToolTipOffset();
	bool hasToolTipOffset() const;

	/// If the region position is not automatic and enable is true, the area used to compute the tool tip position is defined by the plot area scales.
	/// Otherwise, the area is defined by the plot area bounding rect (which is usually around the scales area).
	void setDisplayInsideScales(bool enable);
	bool displayInsideScales() const;

	/// @brief Set the stick distance in scene coordinates.
	/// When moving the mouse, the stick distance will be used to select the closest 
	/// point (for curves) or bar (for histogram like classes) in order to display
	/// information related to this object.
	void setStickDistance(double);
	double stickDistance() const;

	/// @brief Set the distance between the tooltip and the mouse pointer in pixels
	void setDistanceToPointer(double);
	double distanceToPointer() const;

	/// @brief Set the overlay pen used to highlight a plot item (point in a curve, bar in a histogram...)
	void setOverlayPen(const QPen&);
	QPen overlayPen() const;

	/// @brief Set the overlay brush used to highlight a plot item (point in a curve, bar in a histogram...)
	void setOverlayBrush(const QBrush&);
	QBrush overlayBrush() const;

	/// @brief Set the property names to be ignored.
	/// When ItemsProperties is set, the tool tip will display 
	/// the QObject properties of hovered VipPlotItem.
	/// Some properties can be ignored using this function.
	void setIgnoreProperties(const QStringList& names);
	void addIgnoreProperty(const QString& name);
	QStringList ignoreProperties() const;
	bool isPropertyIgnored(const QString& name) const;

	/// @brief Set the tool tip position in VipAbstractPlotArea coordinates.
	/// This will recompute the tool tip content and display it.
	/// This function is automatically called by the parent VipAbstractPlotArea.
	virtual void setPlotAreaPos(const QPointF& pos);

public Q_SLOTS:
	/// @brief Refresh the tool tip content.
	/// This function is automatically called by the parent VipAbstractPlotArea.
	void refresh();

private:
	QString attributeMargins() const;
	QPoint toolTipPosition(VipText& text, const QPointF& pos, Vip::RegionPositions position, Qt::Alignment align);

	struct PrivateData;
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipToolTip::DisplayFlags)

/// @}
// end Plotting

#endif
