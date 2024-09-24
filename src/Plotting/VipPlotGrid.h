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

#ifndef VIP_PLOT_GRID_H
#define VIP_PLOT_GRID_H

#include "VipPlotItem.h"
#include "VipScaleDiv.h"

/// \addtogroup Plotting
/// @{

/// \brief A class which draws a coordinate grid
///
/// The VipPlotGrid class can be used to draw a coordinate grid.
/// A coordinate grid consists of major and minor vertical
/// and horizontal grid lines for cartesian coordinate system.
/// The locations of the grid lines are determined by the X and Y scale divisions.
///
/// VipPlotGrid supports polar coordinate systems.
///
/// VipPlotGrid supports stylesheets and define the following attributes:
/// -	'major-pen': pen used to draw the grid for major ticks (might be 'none')
/// -	'minor-pen': pen used to draw the grid for minor ticks (might be 'none')
/// -	'major-axis': Enable/disable drawing the grid for the major ticks of given axis index. Usage: 'major-axis[0] : true;'
/// -	'minor-axis': Enable/disable drawing the grid for the minor ticks of given axis index. Usage: 'minor-axis[0] : false;'
/// -	'above': if true (default), the grid is drawn on top of all other items, otherwise the grid is drawn just above the canvas
///
/// In addition, VipPlotGrid supports the following selectors: 'cartesian' and 'polar' depending on its coordinate system.
///
class VIP_PLOTTING_EXPORT VipPlotGrid : public VipPlotItem
{
	Q_OBJECT

public:
	VipPlotGrid();
	~VipPlotGrid();

	/// @brief Enable/disable drawing the grid for the major ticks of given axis index
	/// @param axis axis index, 0 for X, 1 for Y
	/// @param tf true to enable the grid
	void enableAxis(int axis, bool tf);
	bool axisEnabled(int axis) const;

	/// @brief Enable/disable drawing the grid for the minor ticks of given axis index
	/// @param axis axis index, 0 for X, 1 for Y
	/// @param tf true to enable the grid
	void enableAxisMin(int axis, bool tf);
	bool axisMinEnabled(int axis) const;

	/// @brief Reimplemented from QGraphicsItem
	virtual QPainterPath shape() const;

	/// @brief Reimplemented from VipPlotItem, set the grid pen used for major ticks
	virtual void setPen(const QPen&);
	/// @brief Reimplemented from VipPlotItem return the grid pen used for major ticks
	virtual QPen pen() const { return majorPen(); }
	/// @brief Reimplemented from VipPlotItem, return the pen color used for major ticks
	virtual QColor majorColor() const { return majorPen().color(); }
	/// @brief Reimplemented from VipPlotItem, set the pen color used for both major and minor ticks
	virtual void setMajorColor(const QColor& c)
	{
		majorPen().setColor(c);
		minorPen().setColor(c);
	}
	/// @brief Reimplemented from VipPlotItem, does nothing
	virtual void setBrush(const QBrush&) {}
	/// @brief Reimplemented from VipPlotItem, returns a default constructed QBrush
	virtual QBrush brush() const { return QBrush(); }

	/// @brief Set/get the pen used to draw the major ticks lines
	void setMajorPen(const QPen&);
	const QPen& majorPen() const;
	QPen& majorPen();

	/// @brief Set/get the pen used to draw the minor ticks lines
	void setMinorPen(const QPen& p);
	const QPen& minorPen() const;
	QPen& minorPen();

	virtual void draw(QPainter* p, const VipCoordinateSystemPtr&) const;

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	virtual bool hasState(const QByteArray& state, bool enable) const;

private:
	void drawCartesian(QPainter* painter, const VipCoordinateSystem&) const;
	void drawPolar(QPainter* painter, const VipPolarSystem&) const;

	void drawRadius(QPainter* painter, const VipScaleDiv::TickList& angles, const QPen& pen, const VipPolarSystem&) const;
	void drawArc(QPainter* painter, const VipScaleDiv::TickList& radiuses, const QPen& pen, const VipPolarSystem&) const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief A VipPlotItem that fills the space defined by 2 axes.
/// VipPlotCanvas is 'just' used to fill the inner space of a plotting area.
///
class VIP_PLOTTING_EXPORT VipPlotCanvas : public VipPlotItem
{
	Q_OBJECT

public:
	VipPlotCanvas();
	~VipPlotCanvas();

	virtual QPainterPath shape() const;
	virtual QRectF boundingRect() const { return shape().boundingRect(); }
	virtual void draw(QPainter* p, const VipCoordinateSystemPtr&) const;

	void setBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& boxStyle() const;
	VipBoxStyle& boxStyle();

	virtual QColor majorColor() const
	{
		if (boxStyle().borderPen().style() == Qt::NoPen || boxStyle().borderPen().color().alpha() == 0)
			return boxStyle().backgroundBrush().color();
		return boxStyle().borderPen().color();
	}
	virtual void setMajorColor(const QColor& c)
	{
		QPen p = boxStyle().borderPen();
		p.setColor(c);
		QBrush b = boxStyle().backgroundBrush();
		b.setColor(c);
		boxStyle().setBorderPen(p);
		boxStyle().setBackgroundBrush(b);
	}

	virtual void setPen(const QPen& p) { boxStyle().setBorderPen(p); }
	virtual QPen pen() const { return boxStyle().borderPen(); }

	virtual void setBrush(const QBrush& b) { boxStyle().setBackgroundBrush(b); }
	virtual QBrush brush() const { return boxStyle().backgroundBrush(); }

protected:
	bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotGrid*)
VIP_REGISTER_QOBJECT_METATYPE(VipPlotCanvas*)

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotGrid* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotGrid* value);

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotCanvas* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotCanvas* value);

/// @}
// end Plotting

#endif
