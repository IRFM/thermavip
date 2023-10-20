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

#ifndef VIP_SIMPLE_ANNOTATION_H
#define VIP_SIMPLE_ANNOTATION_H

#include "VipPlotShape.h"
#include "VipSceneModel.h"

/// Simple annotation class, used to annotate a #VipPlotShape.
///
/// When a #VipPlotShape owns an annotation object, it uses its shape() and draw() functions instead of the standard VipPlotShape ones.
///
/// \sa #VipPlotShape::setAnnotation
class VIP_PLOTTING_EXPORT VipAnnotation
{
	friend class VipPlotShape;

public:
	virtual ~VipAnnotation() {}
	/// Returns the annotation class name
	virtual const char* name() const = 0;
	/// Save the annotation content into a QDataStream
	virtual void save(QDataStream& stream) const = 0;
	/// Load the annotation content from a QDataStream
	virtual bool load(QDataStream& stream) = 0;
	/// Returns the annotation shape based on a VipShape and coordinate system
	virtual QPainterPath shape(const VipShape&, const VipCoordinateSystemPtr&) const = 0;
	/// Draw the annotation
	virtual void draw(const VipShape&, QPainter*, const VipCoordinateSystemPtr&) const = 0;
	/// Returns the parent #VipPlotShape (if any)
	VipPlotShape* parentShape() const;

private:
	void setParentShape(VipPlotShape* sh);
	QPointer<VipPlotShape> m_shape;
};

namespace detail
{
	template<class T>
	static VipAnnotation* createAnnotation()
	{
		return new T();
	}
	typedef VipAnnotation* (*annot_func)();
	VIP_PLOTTING_EXPORT void registerAnnotationFunction(const char* name, annot_func fun);
}

/// Register an annotation class.
/// This is used to serialize/deserialize annotation classes based on #vipSaveAnnotation and #vipLoadAnnotation.
template<class T>
void vipRegisterAnnotationClass(const char* name)
{
	registerAnnotationFunction(name, detail::createAnnotation<T>);
}

/// Create an annotation class based on its classname.
/// Returns nullptr on error.
VIP_PLOTTING_EXPORT VipAnnotation* vipCreateAnnotation(const char* name);
/// Returns the registered annotation classes.
VIP_PLOTTING_EXPORT QStringList vipAnnotations();
/// Save an annotation object to a QByteArray.
VIP_PLOTTING_EXPORT QByteArray vipSaveAnnotation(const VipAnnotation*);
/// Create an annotation object based on a QByteArray previously created with #vipSaveAnnotation.
VIP_PLOTTING_EXPORT VipAnnotation* vipLoadAnnotation(const QByteArray& ar);

/// A simple annotation class.
/// This class can create 3 types of annotation, depending on given VipShape:
/// - A path annotation (text inside/around a path) for VipShape::Path and VipShape::Polygon
/// - A point annotation (text around a symbol) for VipShape::Point
/// - An arrow annotation for VipShape::Polyline.
///
/// Note that some member functions are specific to certain types of annotation.
class VIP_PLOTTING_EXPORT VipSimpleAnnotation : public VipAnnotation
{
public:
	enum EndStyle
	{
		//! No symbol
		None = -1,

		//! Ellipse or circle
		Ellipse,

		//! Rectangle
		Rect,

		//!  Diamond
		Diamond,

		//! Triangle pointing upwards
		Triangle,

		//! Triangle pointing downwards
		DTriangle,

		//! Triangle pointing upwards
		UTriangle,

		//! Triangle pointing left
		LTriangle,

		//! Triangle pointing right
		RTriangle,

		//! Cross (+)
		Cross,

		//! Diagonal cross (X)
		XCross,

		//! Horizontal line
		HLine,

		//! Vertical line
		VLine,

		//! X combined with +
		Star1,

		//! Six-pointed star
		Star2,

		//! Hexagon
		Hexagon,

		Arrow
	};

	VipSimpleAnnotation();
	~VipSimpleAnnotation();

	void setPen(const QPen&);
	const QPen& pen() const;

	void setBrush(const QBrush&);
	const QBrush& brush() const;

	void setEndStyle(int style);
	int endStyle() const;

	void setEndSize(double);
	double endSize() const;

	void setText(const QString&);
	const VipText& text() const;
	VipText& text();

	void setTextDistance(double);
	double textDistance() const;

	void setArrowAngle(double angle);
	double arrowAngle() const;

	void setTextAlignment(Qt::Alignment);
	Qt::Alignment textAlignment() const;

	void setTextPosition(Vip::RegionPositions);
	Vip::RegionPositions textPosition() const;

	virtual const char* name() const { return "VipSimpleAnnotation"; }
	virtual void save(QDataStream& stream) const;
	virtual bool load(QDataStream& stream);

	virtual void draw(const VipShape&, QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QPainterPath shape(const VipShape&, const VipCoordinateSystemPtr&) const;

private:
	void drawShape(const VipShape&, QPainter*, const VipCoordinateSystemPtr&) const;
	void drawArrow(const VipShape&, QPainter*, const VipCoordinateSystemPtr&) const;
	void drawPoint(const VipShape&, QPainter*, const VipCoordinateSystemPtr&) const;

	class PrivateData;
	PrivateData* m_data;
};

/// Helper function.
/// Create an annotation object.
///
/// @param type annotation type: 'line', 'arrow', 'rectangle', 'ellipse', or 'textbox'
/// @param text annotation text
/// @param start start position of the line, arrow, rectangle, ellipse (bounding rectangle) or text box.
/// @param end end position for line, arrow, rectangle or ellipse
/// @param attributes additional annotation attributes:
/// - "textcolor" : annotation text pen given as a QPen, QColor or a QString (uses #PenParser from stylesheet mechanism)
/// - "textbackground" : annotation text background color given as a QColor or QString
/// - "textborder" : annotation text outline (border box pen)
/// - "textradius" : annotation text border radius of the border box
/// - "textsize" : size in points of the text font
/// - "bold" : use bold font for the text
/// - "italic" : use italic font for the text
/// - "fontfamilly": font familly for the text
/// - "border" : shape pen
/// - "background" : shape brush
/// - "distance" : distance between the annotation text and the shape
/// - "alignment" : annotation text alignment around the shape (combination of 'left', 'right', 'top', 'bottom', 'hcenter', vcenter', 'center')
/// - "position" : text position around the shape (combination of 'xinside', 'yinside', 'inside', 'outside')
/// - "symbol" : for 'line' only, symbol for the end point (one of 'none', 'ellipse', 'rect', 'diamond', 'triangle', 'dtriangle', 'utriangle', 'ltriangle', 'rtriangle', 'cross', 'xcross', 'hline',
/// 'vline', 'star1', 'star2', 'hexagon')
/// - "symbolsize" : for 'line' and 'arrow', symbol size for the end point
///
/// @param error is not nullptr and one of the attributes has a wrong syntax, this string gives an error message
///
/// @return created shape and annotation
VIP_PLOTTING_EXPORT QPair<VipShape, VipSimpleAnnotation*> vipAnnotation(const QString& type,
									const QString& text,
									const QPointF& start,
									const QPointF& end = QPointF(),
									const QVariantMap& attributes = QVariantMap(),
									QString* error = nullptr);

#endif