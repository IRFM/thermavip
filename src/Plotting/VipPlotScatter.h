#ifndef VIP_PLOT_SCATTER
#define VIP_PLOT_SCATTER


#include "VipPlotItem.h"
#include "VipSymbol.h"
#include "VipText.h"

struct VipScatterPoint
{
	VipPoint position;
	double value;
};

using VipScatterPointVector = QVector<VipScatterPoint>;

Q_DECLARE_METATYPE(VipScatterPoint)
Q_DECLARE_METATYPE(VipScatterPointVector)

/// @brief Plot item used to create scatter plots based on VipScatterPointVector
///
/// VipPlotScatter displays scatter plots (cloud of points) based on a VipScatterPointVector object.
/// A VipScatterPointVector is a vector of VipScatterPoint that basically stores a 2D position and a value.
/// The value could be used to draw a specific text around/inside each point, can customize the item tool tip
/// or can define the color of each point if a color map is affected to the item.
/// 
/// Each point is drawn based on the item symbol passed with VipPlotScatter::setSymbol().
/// The symbol controls the point style (rectangle, ellipse, start... see VipSymbol class for more details)
/// and the point size. By default, each point has the same size given in item's unit.
/// Use VipPlotScatter::setSizeUnit(VipPlotScatter::AxisUnit) to interpret the point size in scale unit instead.
/// 
/// The point size can also be controlled by each VipScatterPoint value. 
/// Use setUseValueAsSize() to interpret the VipScatterPoint value as the point size in either scale or item unit.
/// 
/// The symbol outline and filling style are controlled with VipPlotScatter::setPen() and VipPlotScatter::setBrush().
/// Note that the filling color of each point might be based on the VipScatterPoint value if a color map is attached 
/// to the item.
/// 
/// Optionally, VipPlotScatter can display a custom text inside/around each point. Use VipPlotScatter::setText() to
/// define the displayed text. Note that occurrences of the sub-string '#value' will be replaced by the 
/// corresponding VipScatterPoint value.
/// 
/// As other VipPlotItemDataType inheriting classes, VipPlotScatter::setData() is thread safe.
/// 
/// VipPlotScatter support stylesheets and defines the following attributes:
/// -	'text-alignment' : see VipPlotScatter::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
/// -	'text-position': see VipPlotScatter::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
/// -	'text-distance': see VipPlotScatter::setTextDistance()
/// -	'size-unit': one of 'itemUnit' and 'axisUnit'
/// -	'use-value-as-size': equivalent to VipPlotScatter::setUseValueAsSize()
/// -   'symbol': symbol, one of 'none', 'ellipse', 'rect', 'diamond', ....
/// -   'symbol-size': symbol size with width==height
/// 
/// In addition, VipPlotScatter defines the following selector: 'itemUnit' and 'axisUnit'.
/// 
class VIP_PLOTTING_EXPORT VipPlotScatter : public VipPlotItemDataType< VipScatterPointVector>
{
	Q_OBJECT

public:
	/// @brief Size unit of each point
	enum SizeUnit
	{
		/// @brief Provided size is in item's unit
		ItemUnit,
		/// @brief Provided size is in scale's unit
		AxisUnit
	};

	/// @brief Construct from title
	explicit VipPlotScatter(const VipText& title = VipText());
	virtual ~VipPlotScatter();

	/// @brief Set the point size unit type.
	/// Depending on the unit type, the symbol size will be interpreted as item's unit or scale's unit.
	void setSizeUnit(SizeUnit unit);
	SizeUnit sizeUnit() const;

	/// @brief Interpret the VipScalePoint value field as the symbol size, in either scale or item's unit
	void setUseValueAsSize(bool);
	bool useValueAsSize() const;

	/// @brief Get/set the symbol used to draw each point
	VipSymbol& symbol();
	const VipSymbol& symbol() const;
	void setSymbol(const VipSymbol&);

	/// @brief Reimplemented from VipPlotItem, returns the symbol pen
	virtual QColor majorColor() const { return symbol().pen().color(); }

	/// @brief Reimplemented from VipPlotItem, set the symbol pen
	virtual void setPen(const QPen& p) { 
		symbol().setPen(p); 
		emitItemChanged();
	}
	virtual QPen pen() const { return symbol().pen(); }

	/// @brief Reimplemented from VipPlotItem, set the symbol filling brush
	virtual void setBrush(const QBrush& b){
		symbol().setBrush(b);
		emitItemChanged();
	}
	virtual QBrush brush() const { return symbol().brush(); }

	
	/// @brief Reimplemented from VipPlotItem in order to be stylesheet aware
	virtual void setTextStyle(const VipTextStyle& st);
	virtual VipTextStyle textStyle() const { return text().textStyle(); }

	/// @brief Reimplemented from VipPlotItemData, set the data as a QVariant containing a VipScatterPointVector
	virtual void setData(const QVariant&);
	/// @brief Reimplemented from VipPlotItem
	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	/// @brief Set the text alignment within its symbol based on the text position
	void setTextAlignment(Qt::Alignment align);
	Qt::Alignment textAlignment() const;

	/// @brief Set the text position: inside or outside the symbol
	void setTextPosition(Vip::RegionPositions pos);
	Vip::RegionPositions textPosition() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform() const;
	const QPointF& textTransformReference() const;

	/// @brief Set the distance (in item's coordinate) between a symbol border and its text
	/// @param distance
	void setTextDistance(double distance);
	double textDistance() const;

	/// @brief Set the text to be drawn within each symbol.
	/// Each occurrence of the content '#value' will be replaced by the VipScatterPoint value.
	void setText(const VipText& text);
	const VipText& text() const;
	VipText& text();

	/// @brief Reimplemented from VipPlotItem, format a text for given position in item's unit.
	/// In addition to the features provided by VipPlotItem::formatText(), this implementation
	/// replaces all occurrences of the sub-string '#value' by the corresponding VipScatterPoint value.
	virtual QString formatText(const QString& text, const QPointF& pos) const;
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private:
	virtual bool hasState(const QByteArray& state, bool enable) const;
	
	int findClosestPos(const VipScatterPointVector& vec, const QPointF& pos, double maxDistance, QRectF* out) const;
	VipInterval computeInterval(const VipScatterPointVector& vec, const VipInterval& interval = Vip::InfinitInterval) const;
	QList<VipInterval> dataBoundingIntervals(const VipScatterPointVector& data) const;

	class PrivateData;
	PrivateData* d_data;
};


#endif
