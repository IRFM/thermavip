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

#ifndef VIP_PLOT_CURVE_H
#define VIP_PLOT_CURVE_H

#include "VipDataType.h"
#include "VipPlotItem.h"

#include <QPen>

#include <functional>

/// \addtogroup Plotting
/// @{

class QPainter;
class VipSymbol;

/// @brief A plot item that represents a serie of points
///
/// A curve is the representation of a series of points in the x-y plane.
/// It supports different display styles, interpolation ( f.e. spline )
/// and symbols.
///
/// When a curve is created, it is configured to draw black solid lines
/// with in VipPlotCurve::Lines style and no symbols.
///
/// VipPlotCurve gets its points using a VipPointVector object offering
/// a bridge to the real storage of the points ( like QAbstractItemModel ).
///
/// VipPlotCurve can also display continuous functions using  VipPlotCurve::setFunction().
///
/// When calling VipPlotCurve::setData() or VipPlotCurve::setRawData(), the passed VipPointVector
/// is splitted in multiple smaller VipPointVector based on X NaN values. Each VipPointVector
/// will then be rendered independently, and the space between each curve might be filled based on
/// given inner brush (VipPlotCurve::setSubBrush()).
///
/// VipPlotCurve is optimized to render millions of points per seconds as long a given VipPointVector is ordered by X values.
///
/// VipPlotCurve supports stylesheets, and add the following properties:
/// -   'curve-style': curve style, one of 'none', 'lines', 'sticks', dots', 'steps'
/// -   'curve-attribute': combination of 'inverted|closePolyline|fillMultiCurves'
/// -   'legend': legend attribute, one of 'legendNoAttribute', 'legendShowLine', 'legendShowSymbol', 'legendShowBrush'
/// -   'symbol': symbol style, one of 'none', 'ellipse', 'rect', 'diamond', ....
/// -   'symbol-size': symbol size in item's coordinates, with width==height
/// -   'symbol-border': symbol border pen
/// -   'symbol-background': symbol background color
/// -   'baseline': curve baseline value
/// -   'symbol-condition': string that defines the symbols visibility condition (see VipPlotCurve::setSymbolCondition())
/// -   'optimize-large-pen-drawing': boolean value, enable/disable large pen drawing optimization
///
/// In addition, VipPlotCurve defines the following selectors: 'none', 'lines', 'sticks', dots', 'steps'.
///
///
/// Usage:
/// \code{cpp}
///
/// #include <cmath>
/// #include <qapplication.h>
/// #include "VipColorMap.h"
/// #include "VipPlotCurve.h"
/// #include "VipSymbol.h"
/// #include "VipPlotWidget2D.h"
///
/// int main(int argc, char** argv)
/// {
///
///     QApplication app(argc, argv);
///
/// 	VipPlotWidget2D w;
///     w.area()->setMouseWheelZoom(true);
///     w.area()->setMousePanning(Qt::RightButton);
///
///     // generate curve
///     VipPointVector vec;
///     for (int i = 0; i < 50; ++i)
/// 	    vec.push_back(VipPoint(i * 0.15, std::cos(i * 0.15)));
///
///     VipPoint yoffset(0, 0);
///
///     VipColorPalette p(VipLinearColorMap::ColorPaletteRandom);
///
///     {
/// 	    VipPlotCurve* c = new VipPlotCurve("Lines");
/// 	    c->setRawData(vec);
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///     }
///     yoffset += VipPoint(0, 1);
///    {
/// 	    VipPlotCurve* c = new VipPlotCurve("Sticks");
/// 	    c->setRawData(offset(vec, yoffset));
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setStyle(VipPlotCurve::Sticks);
/// 	    c->setBaseline(yoffset.y());
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///     }
///     yoffset += VipPoint(0, 1);
///     {
/// 	    VipPlotCurve* c = new VipPlotCurve("Steps");
/// 	    c->setRawData(offset(vec, yoffset));
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setStyle(VipPlotCurve::Steps);
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///    }
///     yoffset += VipPoint(0, 1);
///     {
/// 	    VipPlotCurve* c = new VipPlotCurve("Dots");
/// 	    c->setRawData(offset(vec, yoffset));
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setStyle(VipPlotCurve::Dots);
/// 	    c->setPen(QPen(p.color((int)yoffset.y()), 3));
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///     }
///     yoffset += VipPoint(0, 1);
///     {
/// 	    VipPlotCurve* c = new VipPlotCurve("Baseline Filled");
/// 	    c->setRawData(offset(vec, yoffset));
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setBrush(QBrush(c->majorColor().lighter()));
/// 	    c->setBaseline(yoffset.y());
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///     }
///     yoffset += VipPoint(0, 1);
///     {
///
/// 	    VipPlotCurve* c = new VipPlotCurve("Inner Filled");
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setSubBrush(0, QBrush(c->majorColor().lighter()));
/// 	    c->setCurveAttribute(VipPlotCurve::FillMultiCurves);
/// 	    VipPointVector v = offset(vec, yoffset);
/// 	    v += Vip::InvalidPoint;
/// 	    yoffset += VipPoint(0, 1);
/// 	    v += offset(vec, yoffset);
/// 	    c->setRawData(v);
///
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
///    }
///    yoffset += VipPoint(0, 1);
///     for (int i = VipSymbol::Ellipse; i <= VipSymbol::Hexagon; ++i) {
/// 	    VipPlotCurve* c = new VipPlotCurve(VipSymbol::nameForStyle(VipSymbol::Style(i)));
/// 	    c->setMajorColor(p.color((int)yoffset.y()));
/// 	    c->setSymbol(new VipSymbol(VipSymbol::Style(i)));
/// 	    c->symbol()->setPen(p.color((int)yoffset.y()));
/// 	    c->symbol()->setBrush(QBrush(p.color((int)yoffset.y()).lighter()));
/// 	    c->symbol()->setSize(13, 13);
/// 	    c->setRawData(offset(vec, yoffset));
/// 	    c->setSymbolVisible(true);
/// 	    c->setStyle(VipPlotCurve::NoCurve);
/// 	    c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
/// 	    yoffset += VipPoint(0, 1);
///     }
///
///     w.show();
///    return app.exec();
/// }
///
/// \endcode
class VIP_PLOTTING_EXPORT VipPlotCurve : public VipPlotItemDataType<VipPointVector, VipPoint>
{
	Q_OBJECT

public:
	/// Curve styles.
	/// \sa setStyle(), style()
	enum CurveStyle
	{
		/// Don't draw a curve. Note: This doesn't affect the symbols.
		NoCurve = -1,

		/// Connect the points with straight lines. The lines might
		/// be interpolated depending on the 'Fitted' attribute. Curve
		/// fitting can be configured using setCurveFitter().
		Lines,

		/// Draw vertical or horizontal sticks from a baseline which is defined by setBaseline().
		Sticks,

		/// Connect the points with a step function. The step function
		/// is drawn from the left to the right or vice versa,
		/// depending on the VipPlotCurve::Inverted attribute.
		Steps,

		/// Draw dots at the locations of the data points. Note:
		/// This is different from a dotted line (see setPen()), and faster
		/// as a curve in VipPlotCurve::NoStyle style and a symbol
		/// painting a point.
		Dots,

		/// Styles >= VipPlotCurve::UserCurve are reserved for derived
		/// classes of VipPlotCurve that overload drawCurve() with
		/// additional application specific curve types.
		UserCurve = 100
	};

	/// Attribute for drawing the curve
	/// \sa setCurveAttribute(), testCurveAttribute(), curveFitter()
	enum CurveAttribute
	{
		/// For VipPlotCurve::Steps only.
		/// Draws a step function from the right to the left.
		Inverted = 0x01,

		/// For VipPlotCurve::Lines and VipPlotCurve::Steps only.
		/// Close the curve polygon using the baseline.
		ClosePolyline = 0x02,

		/// If the VipPlotCurve consists of several curves (a VipPointVector containing nan value(s) ),
		/// the brush will be used to fill the space between the curves and the basline won't be used.
		/// This only works if #isSubContinuous() returns true and the different curves overlapp in x axis.
		/// The filling is applied 2 curves by 2 curves, in the order of the original VipPointVector.
		/// The sub brush (as returned by #subBrush()) will be used if possible.
		FillMultiCurves = 0x04
	};

	//! Curve attributes
	typedef QFlags<CurveAttribute> CurveAttributes;

	/// Attributes how to represent the curve on the legend
	///
	/// \sa setLegendAttribute(), testLegendAttribute()
	enum LegendAttribute
	{
		/// VipPlotCurve tries to find a color representing the curve
		/// and paints a rectangle with it.
		LegendNoAttribute = 0x00,

		/// If the style() is not VipPlotCurve::NoCurve a line
		/// is painted with the curve pen().
		LegendShowLine = 0x01,

		/// If the curve has a valid symbol it is painted.
		LegendShowSymbol = 0x02,

		/// If the curve has a brush a rectangle filled with the
		/// curve brush() is painted.
		LegendShowBrush = 0x04
	};

	//! VipLegend attributes
	typedef QFlags<LegendAttribute> LegendAttributes;

	/// @brief Construct from title
	explicit VipPlotCurve(const VipText& title = VipText());
	virtual ~VipPlotCurve();

	/// @brief Set/get legend attribute(s)
	void setLegendAttribute(LegendAttribute, bool on = true);
	bool testLegendAttribute(LegendAttribute) const;
	LegendAttributes legendAttributes() const;
	void setLegendAttributes(LegendAttributes attributes);

	/// @brief Reimplemented from VipPlotItemSampleDataType, set the VipPointVector data as a QVariant.
	/// Use VipPlotCurve::setRawData() to directly set a VipPointVector object.
	virtual void setData(const QVariant&);

	void addSample(const VipPoint& pt) { addSamples(&pt, 1); }
	void addSamples(const VipPointVector& pts) { addSamples(pts.data(), pts.size()); }
	void addSamples(const VipPoint* pts, int numPoints);

	template<class F>
	void updateSamples(F&& fun);

	/// @brief Returns the list of sub vectors (input VipPointVector splitted by NaN X value)
	const QList<VipPointVector>& vectors() const;
	/// @brief For each sub-vector, tells if the vector is sorted in X ascending order
	const QList<bool> continuousVectors() const;
	/// @brief Tells if the input VipPointVector (as set with setData() or setRawData()) is sorted in X ascending order
	bool isFullContinuous() const;
	/// @brief Tells if each sub-vector is sorted in X ascending order
	bool isSubContinuous() const;

	/// @brief Render a function instead of a VipPointVector
	/// @param fun Function to be rendered
	/// @param scale_interval starting X scale interval, also used for automatic scaling
	/// @param draw_interval X interval on which the curve is drawn. A default VipInterval means an infinit interval.
	void setFunction(const std::function<vip_double(vip_double)>& fun, const VipInterval& scale_interval, const VipInterval& draw_interval = VipInterval());
	template<class Fun>
	void setFunction(Fun fun, const VipInterval& scale_interval, const VipInterval& draw_interval = VipInterval())
	{
		std::function<vip_double(vip_double)> f = [fun](vip_double x) { return static_cast<vip_double>(fun(x)); };
		setFunction(f, scale_interval, draw_interval);
	}
	/// @brief Remove previously set function
	void resetFunction();

	/// @brief Reimplemented from VipPlotItem
	virtual QPainterPath shapeFromCoordinateSystem(const VipCoordinateSystemPtr& m) const;

	/// @brief Set/get curve attributes
	void setCurveAttribute(CurveAttribute, bool on = true);
	bool testCurveAttribute(CurveAttribute) const;
	void setCurveAttributes(CurveAttributes);
	CurveAttributes curveAttributes() const;

	/// @brief Set/get the curve box style used to render the curve.
	///
	/// The curve line will be rendered using the box style border pen.
	/// The curve background (filling up to the baseline) will be rendered using the box style background brush.
	/// Using rounded radius is supported and will generate spline interpolations.
	///
	void setBoxStyle(const VipBoxStyle&);
	const VipBoxStyle& boxStyle() const;
	VipBoxStyle& boxStyle();

	/// @brief Reimplemented from VipPlotItem, return the curve pen
	virtual QColor majorColor() const { return boxStyle().borderPen().color(); }

	/// @brief Reimplemented from VipPlotItem, set the curve pen
	virtual void setPen(const QPen&);
	void setPenColor(const QColor&);
	virtual QPen pen() const;

	/// @brief Reimplemented from VipPlotItem, set the filling brush
	virtual void setBrush(const QBrush&);
	void setBrushColor(const QColor&);
	virtual QBrush brush() const;

	/// @brief For sub-vectors (input VipPointVector containing NaN X value(s)), set the curve pen for the sub-curve at given index.
	/// This will override the default pen from the box style.
	void setSubPen(int index, const QPen&);
	QPen subPen(int index) const;
	bool hasSubPen(int index, QPen* p = nullptr) const;

	/// @brief For sub-vectors (input VipPointVector containing NaN X value(s)), set the brush used to fill the space between 2 consecutive curves.
	/// Using index 0 will fill the space between first and second curve, index 1 between second and third curve, etc.
	/// Filling the space between sub-curve only works with attribute FillMultiCurves set.
	void setSubBrush(int index, const QBrush&);
	QBrush subBrush(int index) const;
	bool hasSubBrush(int index, QBrush* b = nullptr) const;

	/// @brief Set the Y baseline value used to draw the curve background.
	/// The baseline is also used when using VipPlotCurve::Sticks style.
	void setBaseline(vip_double);
	vip_double baseline() const;

	/// @brief Set the curve style
	void setStyle(CurveStyle style);
	CurveStyle style() const;

	/// @brief Set the curve symbol. Symbols are only rendered if symbolVisible() is true.
	/// The VipPlotCurve takes ownership of the symbol object.
	void setSymbol(VipSymbol*);
	VipSymbol* symbol() const;

	/// @brief Show/hide symbols
	void setSymbolVisible(bool);
	bool symbolVisible() const;

	/// @brief Define the condition on which symbols are drawn.
	///
	/// The condition is a simple string that contains one or two clauses, like:
	/// 'x > 0', 'y <= 10', or 'x != 10 and y > 0.2'.
	/// Supported operators are '==', '!=', '<', '<=', '>' and '>='.
	/// The condition supports 2 clauses separated by a 'and'/'or' without parenthesis.
	///
	/// @param condition string condition
	/// @return syntax error on error, empty string on success
	QString setSymbolCondition(const QString& condition);
	QString symbolCondition() const;

	/// If the border pen has a width > 1 and is not partially transparent (and background is transparent),
	/// use a rendering optimization that could be up to 20 times faster than the standard polyline
	/// rendering using QPainter. Indeed, drawing complex curves with a pen width > 1 is painfully slow.
	///
	/// Note that the final result is not as good as standard one, but the speed boost can make a HUGE
	/// difference in streaming context.
	void setOptimizeLargePenDrawing(bool);
	bool optimizeLargePenDrawing() const;

	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void drawSelected(QPainter* painter, const VipCoordinateSystemPtr& m) const;
	virtual QList<VipText> legendNames() const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;
	virtual QPointF drawSelectionOrderPosition(const QFont& font, Qt::Alignment alignn, const QRectF& area_bounding_rect) const;

	/// Extract the sample bounding rect.
	/// \a samples the input samples
	/// \a vectors output list of samples that are inside given shape. The input vector is splitted based on Nan x/y values.
	/// \a continuous tells, for each output vector, the ones that are continuous (increasing y)
	/// \a full_continuous tells if the input signal is full continuous (ignoring Nan values)
	/// \a sub_continuous tells if all sub output vectors are continuous
	/// \a shape only consider input points inside the shape, or all points if a null shape is given
	/// \a shape_coord consider input points inside the shape x and/or y boundaries.
	/// A value of 1 means consider X only, 2 for Y only, 3 for X and Y. 0 means that the shape is used, not the shape bounding rect.
	static QList<VipInterval> dataBoundingRect(const VipPointVector& samples,
						   QList<VipPointVector>& vectors,
						   QList<bool>& continuous,
						   bool& full_continuous,
						   bool& sub_continuous,
						   const QPainterPath& shape = QPainterPath(),
						   int shape_coord = 0);

protected:
	void init();

	virtual void drawSymbols(QPainter* p, const VipSymbol& symbol, const VipCoordinateSystemPtr& m, const VipPointVector& simplified, bool continuous, int index) const;

	virtual QPolygonF drawCurve(QPainter* p, int style, const VipCoordinateSystemPtr& m, const QPolygonF& polygon, bool draw_selected, bool continuous, int index) const;

	virtual QPolygonF drawLines(QPainter* p, const VipCoordinateSystemPtr& m, const QPolygonF& polygon, bool draw_selected, bool continuous, int index) const;

	virtual QPolygonF drawSticks(QPainter* p, const VipCoordinateSystemPtr& m, const QPolygonF& polygon, bool draw_selected, bool continuous, int index) const;

	virtual QPolygonF drawDots(QPainter* p, const VipCoordinateSystemPtr& m, const QPolygonF& polygon, bool draw_selected, bool continuous, int index) const;

	virtual QPolygonF drawSteps(QPainter* p, const VipCoordinateSystemPtr& m, const QPolygonF& polygon, bool draw_selected, bool continuous, int index) const;

	/// @brief Defines additional item properties for VipPlotItem style sheet mechanism:
	/// Note that this function should not be called directly. Instead, use VipPlotItem::setStyleSheet()
	///
	/// @param name attribute name
	/// @param value attribute value
	/// @param index unused for curves
	/// @return true on success, false otherwise
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	virtual bool hasState(const QByteArray& state, bool enable) const;

	int closePolyline(QPainter*, const VipCoordinateSystemPtr& m, QPolygonF&) const;

	QPolygonF computeSimplified(QPainter* painter, const VipCoordinateSystemPtr& m, const VipPointVector& points, bool continuous) const;
	QPolygonF extractEnveloppe(const QPolygonF& points, int factor, double* length) const;

private:
	void dataBoundingRect(const VipPointVector&);
	int findClosestPos(const VipPointVector& data, const VipPoint& pos, int axis, double maxDistance, bool continuous) const;

	class PrivateData;
	PrivateData* d_data;
};

template<class F>
void VipPlotCurve::updateSamples(F&& fun)
{
	// First, lock data
	this->dataLock()->lock();

	const QList<VipPointVector>& vecs = this->vectors();
	if (vecs.size() == 1) {
		// We only have one vector (most situations):
		// take the internal data to remove a ref count
		takeData();
		// Call the functor on unref vector
		try {
			std::forward<F>(fun)(const_cast<VipPointVector&>(vecs.first()));
		}
		catch (...) {
			VipPointVector tmp(vecs.first());
			this->dataLock()->unlock();
			setRawData(tmp);
			throw;
		}
		// Unlock and set data
		VipPointVector tmp(vecs.first());
		this->dataLock()->unlock();
		setRawData(tmp);
		return;
	}
	this->dataLock()->unlock();
	// Use standard updateData()
	this->updateData(std::forward<F>(fun));
}

Q_DECLARE_OPERATORS_FOR_FLAGS(VipPlotCurve::LegendAttributes)
Q_DECLARE_OPERATORS_FOR_FLAGS(VipPlotCurve::CurveAttributes)

VIP_REGISTER_QOBJECT_METATYPE(VipPlotCurve*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotCurve* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotCurve* value);

/// @}
// end Plotting

#endif
