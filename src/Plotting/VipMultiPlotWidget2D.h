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

#ifndef VIP_MULTI_PLOT_WIDGET_2D
#define VIP_MULTI_PLOT_WIDGET_2D

#include "VipPlotWidget2D.h"

/// @brief A VipPlotArea2D displaying multiple cartesian plotting area stacked vertically and sharing their horizontal axes
///
/// Usage:
/// \code{cpp}
///
///
/// #include <qapplication.h>
/// #include <cmath>
/// #include "VipMultiPlotWidget2D.h"
/// #include "VipColorMap.h"
/// #include "VipPlotCurve.h"
/// #include "VipLegendItem.h"
///
/// void format_legend(VipLegend* l)
/// {
/// 	// Format an inner legend
///
/// 	// Internal border margin
/// 	l->setMargins(2);
/// 	// Maximum number of columns
/// 	l->setMaxColumns(1);
/// 	// Draw light box around the legend
/// 	l->boxStyle().setBorderPen(Qt::lightGray);
/// 	// Semi transparent background
/// 	l->boxStyle().setBackgroundBrush(QBrush(QColor(255, 255, 255, 200)));
/// }
///
///
/// int main(int argc, char** argv)
/// {
///		QApplication app(argc, argv);
///
/// 	// Create the VipVMultiPlotArea2D, and set it to a VipPlotWidget2D
/// 	VipVMultiPlotArea2D* area = new VipVMultiPlotArea2D();
/// 	VipPlotWidget2D w;
/// 	w.setArea(area);
///
/// 	// Enable zooming/panning
/// 	area->setMousePanning(Qt::RightButton);
/// 	area->setMouseWheelZoom(true);
///
/// 	// Add small margin around the plot area
/// 	area->setMargins(VipMargins(10, 10, 10, 10));
///
/// 	// Insert a new left axis at the top.
/// 	// The VipVMultiPlotArea2D will take care of adding the corresponding right and bottom axes.
/// 	area->setInsertionIndex(1);
/// 	area->addScale(new VipAxisBase(VipAxisBase::Left), true);
///
/// 	// Create an inner legend for the 2 areas
/// 	area->addInnerLegend(new VipLegend(), area->leftMultiAxis()->at(0), Qt::AlignTop | Qt::AlignLeft, 0);
/// 	format_legend(area->innerLegend(0));
/// 	area->addInnerLegend(new VipLegend(), area->leftMultiAxis()->at(1), Qt::AlignTop | Qt::AlignLeft, 0);
/// 	format_legend(area->innerLegend(1));
///
/// 	// Hide the global legend located at the very bottom of the area
/// 	area->legend()->setVisible(false);
///
/// 	// Color palette used to give a unique color to each curve
/// 	VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);
///
/// 	// Cos curve on the top area
/// 	VipPlotCurve* c_cos = new VipPlotCurve("cos");
/// 	c_cos->setMajorColor(palette.color(0));
/// 	c_cos->setFunction([](double x) { return std::cos(x); }, VipInterval(-M_PI, M_PI));
/// 	c_cos->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(1), VipCoordinateSystem::Cartesian);
///
/// 	// Sin curve on the top area
/// 	VipPlotCurve* c_sin = new VipPlotCurve("sin");
/// 	c_sin->setMajorColor(palette.color(1));
/// 	c_sin->setFunction([](double x) { return std::sin(x); }, VipInterval(-M_PI, M_PI));
/// 	c_sin->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(1), VipCoordinateSystem::Cartesian);
///
/// 	// Atan curve on the bottom area
/// 	VipPlotCurve* c_atan = new VipPlotCurve("atan");
/// 	c_atan->setMajorColor(palette.color(2));
/// 	c_atan->setFunction([](double x) { return std::atan(x); }, VipInterval(-M_PI, M_PI));
/// 	c_atan->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(0), VipCoordinateSystem::Cartesian);
///
/// 	// Tanh curve on the bottom area
/// 	VipPlotCurve* c_tanh = new VipPlotCurve("tanh");
/// 	c_tanh->setMajorColor(palette.color(3));
/// 	c_tanh->setFunction([](double x) { return std::tanh(x); }, VipInterval(-M_PI, M_PI));
/// 	c_tanh->setAxes(area->bottomAxis(), area->leftMultiAxis()->at(0), VipCoordinateSystem::Cartesian);
///
/// 	w.resize(500, 500);
/// 	w.show();
/// 	return app.exec();
///	}
///
/// \endcode
///
class VIP_PLOTTING_EXPORT VipVMultiPlotArea2D : public VipPlotArea2D
{
	Q_OBJECT

public:
	VipVMultiPlotArea2D(QGraphicsItem* parent = nullptr);
	virtual ~VipVMultiPlotArea2D();

	/// @brief Reimplemented from VipPlotArea2D
	virtual VipAxisBase* leftAxis() const;
	/// @brief Reimplemented from VipPlotArea2D
	virtual VipAxisBase* rightAxis() const;
	/// @brief Reimplemented from VipPlotArea2D
	virtual VipPlotGrid* grid() const;
	/// @brief Reimplemented from VipPlotArea2D
	virtual VipPlotCanvas* canvas() const;

	/// @brief Returns the left VipMultiAxisBase
	VipMultiAxisBase* leftMultiAxis() const;
	/// @brief Returns the right VipMultiAxisBase
	VipMultiAxisBase* rightMultiAxis() const;

	/// @brief Returns all VipPlotGrid objects
	QList<VipPlotGrid*> allGrids() const;
	/// @brief Returns all VipPlotCanvas objects
	QList<VipPlotCanvas*> allCanvas() const;
	/// @brief Returns all horizontal (synchronized) axes
	QList<VipAxisBase*> horizontalAxes() const;

	/// @brief Returns the plotting area in item's cordinate for given vertical axis
	QPainterPath plotArea(const VipBorderItem* vertical_scale) const;
	/// @brief Returns the full plotting rectangle in item's coordinate
	QRectF plotRect() const;

	/// @brief Set the insertion index used for the next call to VipAbstractPlotArea::addScale().
	/// After a call to VipAbstractPlotArea::addScale(), this value is resetted
	void setInsertionIndex(int index);
	int insertionIndex() const;

	/// @brief Returns the 2 axes defining the plotting area that contains pos
	virtual QList<VipAbstractScale*> scalesForPos(const QPointF& pos) const;

	virtual void recomputeGeometry(bool recompute_aligned_areas = true);
	virtual void zoomOnSelection(const QPointF& start, const QPointF& end);
	virtual void zoomOnPosition(const QPointF& pos, double factor);
	virtual void translate(const QPointF& fromPt, const QPointF& dp);

	virtual void resetInnerLegendsPosition();
public Q_SLOTS:
	void applyDefaultParameters();

protected:
	virtual bool internalAddScale(VipAbstractScale*, bool isSpatialCoordinate = true);
	virtual bool internalRemoveScale(VipAbstractScale*);

Q_SIGNALS:
	void canvasAdded(VipPlotCanvas*);
	void canvasRemoved(VipPlotCanvas*);

private:
	void applyLabelOverlapping();
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
