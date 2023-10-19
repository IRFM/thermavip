#ifndef VIP_PLOT_QUIVER_H
#define VIP_PLOT_QUIVER_H


#include <QVector>

#include "VipQuiver.h"
#include "VipPlotItem.h"

/// \addtogroup Plotting
/// @{

struct VipQuiverPoint
{
	VipPoint position;
	VipPoint destination;
	double value;
};
using VipQuiverPointVector = QVector<VipQuiverPoint>;

Q_DECLARE_METATYPE(VipQuiverPoint);
Q_DECLARE_METATYPE(VipQuiverPointVector);

VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream&, const VipQuiverPoint&);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream&,  VipQuiverPoint&);


/// @brief VipPlotItem that draws a field of quivers
///
/// VipPlotQuiver is a plotting item that displays a field of quivers passed as a VipQuiverPointVector.
/// VipQuiverPointVector is a vector of VipQuiverPoint, that contains an anchor point, a destination point and a value.
/// The value has 2 purposes: it can be drawn around/inside the arrow, and it can influence the arrow color if
/// a color map is attached to the VipPlotQuiver.
/// 
/// The arrow style is controlled by the VipQuiverPath returned by VipPlotQuiver::quiverPath().
/// 
/// Like other VipPlotItemDataType, VipPlotQuiver::setData() can be called from a non GUI thread.
/// 
/// VipPlotQuiver support stylesheets and defines the following attributes:
/// -	'text-alignment' : see VipPlotQuiver::setTextAlignment(), combination of 'left|right|top|bottom|center|vcenter|hcenter'
/// -	'text-position': see VipPlotQuiver::setTextPosition(), combination of 'outside|xinside|yinside|xautomatic|yautomatic|automatic'
/// -	'text-distance': see VipPlotQuiver::setTextDistance()
/// -	'arrow-size': floating point value defining the arrow size in item's coordinates
/// -	'arrow-style': style of the arrow (sse VipQuiverPath), combination of 'line|startArrow|startSquare|startCircle|endArrow|endSquare|endCircle'
/// 
class VIP_PLOTTING_EXPORT VipPlotQuiver : public VipPlotItemDataType<VipQuiverPointVector, VipQuiverPoint>
{
	Q_OBJECT

public:


	VipPlotQuiver(const VipText & title = VipText());
	virtual ~VipPlotQuiver();

	/// @brief Reimplemented from VipPlotItem, returns the quiver path color
	virtual QColor majorColor() const { return quiverPath().pen().color(); }
	/// @brief Reimplemented from VipPlotItem, set the color of the quiver path and the arrow(s)
	virtual void setMajorColor(const QColor & c) {
		QPen p = quiverPath().pen();
		p.setColor(c);
		setPen(p);
		setBrush(QBrush(c));
	}
	/// @brief Reimplemented from VipPlotItem, set the pen for the quiver path and the arrow(s)
	virtual void setPen(const QPen & p) { 
		quiverPath().setPen(p); 
		quiverPath().setExtremityPen(VipQuiverPath::Start, p);
		quiverPath().setExtremityPen(VipQuiverPath::End, p);
	}
	virtual QPen pen() const { return quiverPath().pen(); }

	/// @brief Reimplemented from VipPlotItem, set the brush for the arrow(s)
	virtual void setBrush(const QBrush& b) {
		quiverPath().setExtremityBrush(VipQuiverPath::Start, b);
		quiverPath().setExtremityBrush(VipQuiverPath::End, b);
	}
	virtual QBrush brush() const { return QBrush(); }

	/// @brief Reimplemented from VipPlotItem in order to be stylesheet aware
	virtual void setTextStyle(const VipTextStyle& st);
	virtual VipTextStyle textStyle() const { return text().textStyle(); }

	/// @brief Reimplemented from VipPlotItemData, set the data as a QVariant containing a VipQuiverPointVector
	virtual void setData(const QVariant&);
	/// @brief Reimplemented from VipPlotItem
	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	/// @brief Set the bar text alignment within its bar based on the text position
	void setTextAlignment(Qt::Alignment align);
	Qt::Alignment textAlignment() const;

	/// @brief Set the bar text position: inside or outside the bar
	void setTextPosition(Vip::RegionPositions pos);
	Vip::RegionPositions textPosition() const;

	/// @brief Additional custom text transform.
	/// By default, the transform is applied from the top left corner of the text rectangle.
	/// You can specify a different origin using the ref parameter, which is a relative x and y distance from the rectangle dimensions.
	/// For Instance, to apply a rotation around the text center, use QPointF(0.5,0.5).
	void setTextTransform(const QTransform& tr, const QPointF& ref = QPointF(0, 0));
	const QTransform& textTransform() const;
	const QPointF& textTransformReference() const;

	/// @brief Set the distance (in item's coordinate) between a bar border and its text
	/// @param distance
	void setTextDistance(double distance);
	double textDistance() const;

	/// @brief Set the text to be drawn within each bar of the histogram.
	/// Each occurrence of the content '#value' will be replaced by the bar value.
	/// Each occurrence of the content '#min' will be replaced by the bar minimum X value, and '#max' by the bar maximum X value.
	void setText(const VipText& text);
	const VipText& text() const;
	VipText& text();

	/// @brief Set/get the VipQuiverPath defining the item styling
	void setQuiverPath(const VipQuiverPath &);
	const VipQuiverPath & quiverPath() const;
	VipQuiverPath & quiverPath();

	/// @brief Reimplemented from VipPlotItem. In addition to the features provided by
	/// VipPlotItem::formatText(), this function replace the substring '#value' by the 
	/// VipQuiverPoint value at given position.
	virtual QString formatText(const QString& text, const QPointF& pos) const;
	virtual bool areaOfInterest(const QPointF& pos, int axis, double maxDistance, VipPointVector& out_pos, VipBoxStyle& style, int& legend) const;
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;
	virtual QList<VipInterval> plotBoundingIntervals() const;

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private:

	QList<VipInterval> dataBoundingIntervals(const VipQuiverPointVector&) const;
	VipInterval computeInterval(const VipQuiverPointVector&, const VipInterval&) const;
	int findQuiverIndex(const VipQuiverPointVector& vec, const QPointF& pos, double max_dist) const;
	class PrivateData;
	PrivateData *d_data;
};

/// @}
//end Plotting

#endif
