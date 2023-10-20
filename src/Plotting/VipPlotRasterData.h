#ifndef VIP_PLOT_RASTER_DATA_H
#define VIP_PLOT_RASTER_DATA_H

#include <QSharedPointer>

#include "VipPlotItem.h"
#include "VipInterval.h"
#include "VipNDArray.h"

/// \addtogroup Plotting
/// @{


class VipAxisColorMap;
class VipColorMap;

/// @brief Base abstract class representing a 2D raster data stored in VipRasterData
class VIP_PLOTTING_EXPORT VipRasterConverter
{
public:
	virtual ~VipRasterConverter() {}
	virtual QRectF boundingRect() const = 0;
	virtual void extract(const QRectF & rect, VipNDArray * out_array, QRectF * out_rect = NULL) const = 0;
	virtual QVariant pick(const QPointF & pos) const = 0;
	virtual VipInterval bounds(const VipInterval & valid_interval) const = 0;
	virtual int dataType() const = 0;
};

/// @brief Raster data passed to VipPlotRasterData
///
/// VipRasterData represents a 2D raster data with a potentially infinit bounding rect.
/// It can hold a VipNDArray, a QImage or a QPixmap, or any kind of VipRasterConverter object.
/// 
/// VipRasterData uses shared ownership^.
/// 
class VIP_PLOTTING_EXPORT VipRasterData
{
public:
	VipRasterData();
	/// @brief Construct from a VipNDArray and an origin position
	VipRasterData(const VipNDArray & ar , const QPointF &p = QPointF());
	/// @brief Construct from a QImage and an origin position
	VipRasterData(const QImage & image, const QPointF &p = QPointF());
	/// @brief Construct from a QPixmap and an origin position
	VipRasterData(const QPixmap & pixmap, const QPointF &p = QPointF());
	/// @brief Construct from a VipRasterConverter object
	VipRasterData(VipRasterConverter * converter);
	VipRasterData(const VipRasterData & raster);
	~VipRasterData();
	VipRasterData & operator=(const VipRasterData & raster);

	/// @brief Returns true if the VipRasterData is null (default constructed)
	bool isNull() const { return !d_data; }
	/// @brief Returns true if the VipRasterData is empty (null or empty bounding rect)
	bool isEmpty() const { return isNull() || boundingRect() == QRectF(); }
	/// @brief Returns true if the VipRasterData holds a VipNDArray
	bool isArray() const;
	/// @brief Returns the construction time in milliseconds since epoch
	qint64 modifiedTime() const;
	/// @brief Returns the VipRasterData bounding rectangle
	QRectF boundingRect() const;
	/// @brief Returns the VipRasterData bounds (minimum and maximum value) within given validity interval
	VipInterval bounds(const VipInterval & interval) const;
	/// @brief Returns the VipRasterData data type based on Qt metatype system
	int dataType() const;
	/// @brief Extract a VipNDArray from this VipRasterData based on requested rectangle.
	/// The actual retrieved array rectangle is stored in out_rect.
	VipNDArray extract(const QRectF & rect = QRectF(), QRectF * out_rect = NULL) const;
	/// @brief Returns a pixel value at given position
	QVariant pick(const QPointF & pos) const;

private:
	class PrivateData;
	QSharedPointer<PrivateData> d_data;
};

Q_DECLARE_METATYPE(VipRasterData)




class VipImageData;

/// @brief Plot item displaying a raster data passed as a VipRasterData
/// 
/// Most of the time, VipPlotRasterData is used to display an image passed as a VipNDArray,
/// but can potentially display any kind of 2D raster data using custom VipRasterConverter
/// wrapped in a VipRasterData.
/// VipPlotRasterData::setData() accepts both VipNDArray and VipRasterData as input
/// Like other VipPlotItemDataType, setData() member is thread safe.
/// 
/// In addition to the input image passed with VipPlotRasterData::setData(), VipPlotRasterData
/// can display a background image (as input image can be displayed with transparency support)
/// as well a foreground image superimposed on the input image.
/// Use setBackgroundImage() to provide a background QImage and setSuperimposeImage() to provide
/// a foreground QImage. Foreground image opacity is controlled with setSuperimposeOpacity().
/// 
/// VipPlotRasterData supports style sheets and adds the following attribute:
/// -	'superimpose-opacity': equivalent to VipPlotRasterData::setSuperimposeOpacity()
/// 
class VIP_PLOTTING_EXPORT VipPlotRasterData : public VipPlotItemDataType<VipRasterData>
{
	Q_OBJECT

public:

	VipPlotRasterData(const VipText & title = VipText());
	virtual ~VipPlotRasterData();

	/// @brief Set the input data passed as a VipRasterData or VipNDArray
	virtual void setData(const QVariant & );
	virtual QVariant data() const;

	/// @brief Reimplemented from VipPlotItem
	virtual QList<VipInterval> plotBoundingIntervals() const;
	/// @brief Reimplemented from VipPlotItem
	virtual QPainterPath shape() const;
	/// @brief Reimplemented from VipPlotItem
	virtual QRectF boundingRect() const;
	/// @brief Reimplemented from VipPlotItem
	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;
	/// @brief Reimplemented from VipPlotItem
	virtual void draw(QPainter *, const VipCoordinateSystemPtr &) const;
	/// @brief Reimplemented from VipPlotItem
	virtual void drawSelected( QPainter *painter, const VipCoordinateSystemPtr & m) const;
	/// @brief Reimplemented from VipPlotItem
	virtual QList<VipText> legendNames() const;
	/// @brief Reimplemented from VipPlotItem
	virtual QRectF drawLegend(QPainter *, const QRectF &, int index) const;
	/// @brief Reimplemented from VipPlotItem. In addition to VipPlotItem::formatText features,
	/// replace occurrences of substring '#value' by the pixel value at given position.
	virtual QString formatText(const QString & str, const QPointF & pos) const;
	/// @brief Reimplemented from VipPlotItem
	virtual QColor majorColor() const { return QColor(); }
	virtual void setMajorColor(const QColor&) {}
	/// @brief Reimplemented from VipPlotItem
	virtual void setPen(const QPen& p) { setBorderPen(p); }
	virtual QPen pen() const { return borderPen(); }
	/// @brief Reimplemented from VipPlotItem
	virtual void setBrush(const QBrush&) { ; }
	virtual QBrush brush() const { return QBrush(); }

	/// @brief Returns the QImage that is currently displayed (input image with potential color map applied)
	const QImage& image() const;

	/// @brief Returns the input image bounding rectangle
	QRectF imageBoundingRect() const;

	/// @brief Returns the pixel value as a QString for given position within the input image
	QString imageValue(const QPoint & im_pos) const;

	/// @brief Set or reset the background image
	void setBackgroundImage(const QImage& img);
	const QImage& backgroundimage() const;

	/// @brief Set or reset the foreground image
	/// The foreground image opacity is controlled with setSuperimposeOpacity().
	void setSuperimposeImage(const QImage & img);
	const QImage & superimposeImage() const;

	/// @brief Set the foreground image opacity.
	/// A value of 0 means fully transparent, and 1 means fully opaque.
	void setSuperimposeOpacity(double factor);
	double superimposeOpacity() const;

	/// @brief Set the border pen drawn around the image
	void setBorderPen(const QPen & pen);
	const QPen & borderPen() const;

Q_SIGNALS:
	///Emitted when setting a new data changes the image bounding rect
	void imageRectChanged(const QRectF &);

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());

private Q_SLOTS:
	void updateInternal(bool dirty_color_scale);

private:
	/// Compute all necessary information for the image drawing.
	bool computeImage(const VipRasterData& ar,
			  const VipInterval& interval,
			  const VipCoordinateSystemPtr& m,
			  VipNDArray& tmp_array,
			  QImage& out,
			  QPolygonF& dst_polygon,
			  QRectF& src_rect,
			  QRectF& src_image_rect) const;
	bool computeImage(const VipRasterData& ar, const VipInterval& interval, const VipCoordinateSystemPtr& m, VipNDArray& tmp_array, VipImageData& img) const;

	QRectF computeArrayRect(const VipRasterData & raster) const;
	void drawBackground(QPainter* painter, const VipCoordinateSystemPtr& m, const QRectF& rect, const QPolygonF& dst) const;

	class PrivateData;
	PrivateData *d_data;
};



VIP_REGISTER_QOBJECT_METATYPE(VipPlotRasterData*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotRasterData* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotRasterData* value);


/// @}
//end Plotting

#endif
