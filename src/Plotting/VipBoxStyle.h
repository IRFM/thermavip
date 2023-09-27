#ifndef VIP_BOX_STYLE_H
#define VIP_BOX_STYLE_H

#include <QPen>
#include <QBrush>
#include <QList>
#include <QMetaType>

#include "VipPie.h"
#include "VipAdaptativeGradient.h"

/// \addtogroup Plotting
/// @{

class QPainter;
typedef QPair<QPainterPath,QPainterPath> PainterPaths;

/// VipBoxStyle represents drawing parameters used to represent boxes, polygons or any kind of shape within the Plotting library.
/// Uses Copy On Write.
class VIP_PLOTTING_EXPORT VipBoxStyle
{
public:

	VipBoxStyle();
	/// @brief Construct from a border pen, a background brush and a border radius
	VipBoxStyle(const QPen& b_pen , const QBrush& b_brush = QBrush(), double radius = 0);
	VipBoxStyle(const VipBoxStyle &);

	VipBoxStyle & operator=(const VipBoxStyle&);

	/// @brief Returns whether the box style is null (uninitialized) or not
	bool isNull() const;
	/// @brief Returns !isNull()
	bool isValid() const;
	/// @brief Returns whether the box style is null or has nothing to draw
	bool isEmpty() const;

	/// @brief Set the border pen
	void setBorderPen(const QPen & );
	void setBorderPen(const QColor & c);
	void setBorderPen(const QColor & c, double width);
	QPen borderPen() const;
	QPen& borderPen();

	/// @brief Set the background brush
	void setBackgroundBrush(const QBrush &);
	QBrush backgroundBrush() const;
	QBrush& backgroundBrush();

	/// @brief Set the border and background color
	void setColor(const QColor & c);

	/// @brief Set the adaptative gradient brush
	void setAdaptativeGradientBrush(const VipAdaptativeGradient&);
	VipAdaptativeGradient adaptativeGradientBrush() const;
	/// @brief Remove the QGradient from the background brush, but keep the brush itself
	void unsetBrushGradient();

	/// @brief Set the adaptative gradient pen
	void setAdaptativeGradientPen(const VipAdaptativeGradient&);
	VipAdaptativeGradient adaptativeGradientPen() const;
	/// @brief Remove the QGradient from the border pen, but keep the pen itself
	void unsetPenGradient();

	/// @brief Set the border radius, valid for all kind of shapes except QPainterPath
	void setBorderRadius(double);
	double borderRadius() const;

	/// @brief Set the borders to be drawn. Valid for quadrilateral shapes and pies
	void setDrawLines(Vip::Sides draw_lines);
	void setDrawLine(Vip::Side draw_line, bool on);
	bool testDrawLines(Vip::Side draw_line) const;
	Vip::Sides drawLines() const;

	/// @brief Set the corners to be rounded. Valid for quadrilateral shapes and pies
	void setRoundedCorners(Vip::Corners rounded_corners);
	void setRoundedCorner(Vip::Corner rounded_corner, bool on);
	bool testRoundedCorner(Vip::Corner rounded_corner) const;
	Vip::Corners roundedCorners() const;

	/// @brief Returns true is the brush is transparent
	bool isTransparentBrush() const;
	/// @brief Returns true if the border pen is transparent
	bool isTransparentPen() const;
	/// @brief Returns true if the full shape is transparent
	bool isTransparent() const;

	/// @brief Returns the background shape
	QPainterPath background() const;
	/// @brief Returns the border shape
	QPainterPath border() const;
	/// @brief Returns the background and border shape
	PainterPaths paths() const;
	/// @brief Returns the shape bounding rect
	QRectF boundingRect() const;

	/// @brief Set the shape (background and border) to path.
	void computePath(const QPainterPath & path);
	/// @brief Set the background and border shapes
	void computePath(const PainterPaths & paths);
	/// @brief Build the shape based on given quadrilateral
	void computeQuadrilateral(const QPolygonF &);
	/// @brief Build the shape based on given (possibliy closed) polyline
	void computePolyline(const QPolygonF &);
	/// @brief Build the shape based on given rectangle
	void computeRect(const QRectF &);
	/// @brief Build the shape based on given pie
	void computePie(const QPointF & center, const VipPie & pie, double spacing = 0);

	/// @brief Create the background brush based on the shape and the given brush or gradient.
	/// Only rectangles and pies can use adaptative gradients.
	QBrush createBackgroundBrush() const;
	QPen createBorderPen(const QPen & pen) const;

	/// @brief Returns true if the brush uses an adaptative gradient
	bool hasBrushGradient() const;
	/// @brief Returns true if the border pen uses an adaptative gradient
	bool hasPenGradient() const;

	/// @brief Draw the background
	void drawBackground(QPainter *) const;
	void drawBackground(QPainter* painter, const QBrush& ) const;
	/// @brief Draw the borders
	void drawBorder(QPainter *) const;
	void drawBorder(QPainter*, const QPen &) const;
	/// @brief Draw background and borders
	void draw(QPainter *) const;
	void draw(QPainter* painter, const QBrush& brush) const;
	void draw(QPainter* painter, const QBrush& brush, const QPen& pen) const;

	/// @brief Compare 2 VipBoxStyle for equality. Only test drawinf style (pen, brush, adaptative gradients, draw lines, draw corners, radius...),
	/// but not the shape itself.
	bool operator==(const VipBoxStyle & other) const;
	bool operator!=(const VipBoxStyle & other) const;

private:

	void update();

	struct PrivateData: QSharedData
	{
		PrivateData()
		:pen(Qt::NoPen), radius(0), drawLines(Vip::AllSides), roundedCorners(0)
		{}

		QPen pen;
		double radius;
		Vip::Sides drawLines;
		Vip::Corners roundedCorners;

		VipAdaptativeGradient brushGradient;
		VipAdaptativeGradient penGradient;

		PainterPaths paths;

		//pie values
		VipPie pie;
		QPointF center;

		//other shapes
		QRectF rect;
		QVector<QPointF> polygon;
	};

	QSharedDataPointer<PrivateData> d_data;
};


typedef QList<VipBoxStyle> VipBoxStyleList;
Q_DECLARE_METATYPE(VipBoxStyle)

class QDataStream;
VIP_PLOTTING_EXPORT QDataStream & operator<<(QDataStream & stream, const VipBoxStyle & style);
VIP_PLOTTING_EXPORT QDataStream & operator>>(QDataStream & stream, VipBoxStyle & style);

/// @}
//end Plotting

#endif
