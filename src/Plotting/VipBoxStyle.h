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

#ifndef VIP_BOX_STYLE_H
#define VIP_BOX_STYLE_H

#include <QBrush>
#include <QList>
#include <QMetaType>
#include <QPen>

#include "VipAdaptativeGradient.h"
#include "VipPie.h"

/// \addtogroup Plotting
/// @{

class QPainter;
typedef QPair<QPainterPath, QPainterPath> PainterPaths;

/// VipBoxStyle represents drawing parameters used to represent boxes, polygons or any kind of shape within the Plotting library.
/// Uses Copy On Write internally.
class VIP_PLOTTING_EXPORT VipBoxStyle
{
public:
	VipBoxStyle();
	/// @brief Construct from a border pen, a background brush and a border radius
	VipBoxStyle(const QPen& b_pen, const QBrush& b_brush = QBrush(), double radius = 0);
	VIP_DEFAULT_MOVE(VipBoxStyle);

	/// @brief Returns whether the box style is null (uninitialized) or not
	VIP_ALWAYS_INLINE bool isNull() const noexcept { return !d_data; }
	/// @brief Returns !isNull()
	VIP_ALWAYS_INLINE bool isValid() const noexcept { return d_data; }
	/// @brief Returns whether the box style is null or has nothing to draw
	VIP_ALWAYS_INLINE bool isEmpty() const noexcept { return isNull() || (d_data->paths.first.isEmpty() && d_data->paths.second.isEmpty()); }

	/// @brief Set the border pen
	void setBorderPen(const QPen&);
	void setBorderPen(const QColor& c);
	void setBorderPen(const QColor& c, double width);
	const QPen& borderPen() const noexcept;
	QPen& borderPen();

	/// @brief Set the background brush
	void setBackgroundBrush(const QBrush&);
	const QBrush &backgroundBrush() const noexcept;
	QBrush& backgroundBrush();

	/// @brief Set the border and background color
	void setColor(const QColor& c);

	/// @brief Set the adaptative gradient brush
	void setAdaptativeGradientBrush(const VipAdaptativeGradient&);
	const VipAdaptativeGradient &adaptativeGradientBrush() const noexcept;
	/// @brief Remove the QGradient from the background brush, but keep the brush itself
	void unsetBrushGradient();

	/// @brief Set the adaptative gradient pen
	void setAdaptativeGradientPen(const VipAdaptativeGradient&);
	const VipAdaptativeGradient &adaptativeGradientPen() const noexcept;
	/// @brief Remove the QGradient from the border pen, but keep the pen itself
	void unsetPenGradient();

	/// @brief Set the border radius, valid for all kind of shapes except QPainterPath
	void setBorderRadius(double);
	double borderRadius() const noexcept;

	/// @brief Set the borders to be drawn. Valid for quadrilateral shapes and pies
	void setDrawLines(Vip::Sides draw_lines);
	void setDrawLine(Vip::Side draw_line, bool on);
	bool testDrawLines(Vip::Side draw_line) const;
	Vip::Sides drawLines() const noexcept;

	/// @brief Set the corners to be rounded. Valid for quadrilateral shapes and pies
	void setRoundedCorners(Vip::Corners rounded_corners);
	void setRoundedCorner(Vip::Corner rounded_corner, bool on);
	bool testRoundedCorner(Vip::Corner rounded_corner) const;
	Vip::Corners roundedCorners() const noexcept;

	/// @brief Returns true is the brush is transparent
	bool isTransparentBrush() const noexcept;
	/// @brief Returns true if the border pen is transparent
	bool isTransparentPen() const noexcept;
	/// @brief Returns true if the full shape is transparent
	bool isTransparent() const noexcept;

	/// @brief Returns the background shape
	const QPainterPath& background() const noexcept;
	/// @brief Returns the border shape
	const QPainterPath& border() const noexcept;
	/// @brief Returns the background and border shape
	const PainterPaths& paths() const noexcept;
	/// @brief Returns the shape bounding rect
	QRectF boundingRect() const;

	/// @brief Set the shape (background and border) to path.
	void computePath(const QPainterPath& path);
	/// @brief Set the background and border shapes
	void computePath(const PainterPaths& paths);
	/// @brief Build the shape based on given quadrilateral
	void computeQuadrilateral(const QPolygonF&);
	/// @brief Build the shape based on given (possibliy closed) polyline
	void computePolyline(const QPolygonF&);
	/// @brief Build the shape based on given rectangle
	void computeRect(const QRectF&);
	/// @brief Build the shape based on given pie
	void computePie(const QPointF& center, const VipPie& pie, double spacing = 0);

	
	/// @brief Returns true if the brush uses an adaptative gradient
	bool hasBrushGradient() const noexcept;
	/// @brief Returns true if the border pen uses an adaptative gradient
	bool hasPenGradient() const noexcept;

	/// @brief Draw the background
	void drawBackground(QPainter*) const;
	void drawBackground(QPainter* painter, const QBrush&) const;
	/// @brief Draw the borders
	void drawBorder(QPainter*) const;
	void drawBorder(QPainter*, const QPen&) const;
	/// @brief Draw background and borders
	void draw(QPainter*) const;
	void draw(QPainter* painter, const QBrush& brush) const;
	void draw(QPainter* painter, const QBrush& brush, const QPen& pen) const;

	/// @brief Compare 2 VipBoxStyle for equality. Only test drawing style (pen, brush, adaptative gradients, draw lines, draw corners, radius...),
	/// but not the shape itself.
	bool operator==(const VipBoxStyle& other) const noexcept;
	bool operator!=(const VipBoxStyle& other) const noexcept;

private:
	/// @brief Create the background brush based on the shape and the given brush or gradient.
	/// Only rectangles and pies can use adaptative gradients.
	QBrush createBackgroundBrush() const;
	QPen createBorderPen(const QPen& pen) const;

	void update();

	struct PrivateData : QSharedData
	{
		PrivateData()
		  : pen(Qt::NoPen)
		  , radius(0)
		  , drawLines(Vip::AllSides)
		{
		}

		QPen pen;
		double radius;
		Vip::Sides drawLines;
		Vip::Corners roundedCorners;

		VipAdaptativeGradient brushGradient;
		VipAdaptativeGradient penGradient;

		PainterPaths paths;

		// pie values
		VipPie pie;
		QPointF center;

		// other shapes
		QRectF rect;
		QVector<QPointF> polygon;
	};

	QSharedDataPointer<PrivateData> d_data;
};

typedef QList<VipBoxStyle> VipBoxStyleList;
Q_DECLARE_METATYPE(VipBoxStyle)

class QDataStream;
VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipBoxStyle& style);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipBoxStyle& style);

/// @}
// end Plotting

#endif
