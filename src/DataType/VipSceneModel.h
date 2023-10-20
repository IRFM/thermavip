#ifndef VIP_SCENE_MODEL_H
#define VIP_SCENE_MODEL_H

#include <QString>
#include <QPainterPath>
#include <QSharedPointer>
#include <QMap>
#include <QVariant>
#include <QObject>
#include <qflags.h>

#include "VipNDArray.h"
#include "VipInterval.h"
#include "VipReduction.h"

/// \addtogroup DataType
/// @{

class VipShape;
class VipSceneModel;
class VipShapeSignals;

///Statistic informations extracted from on a 2D shape (#Shape type) and an image (#VipNDArray type)
class VipShapeStatistics
{
public:
	enum Statistic
	{
		Minimum = 0x0001,
		Maximum = 0x0002,
		Mean = 0X0004,
		Std = 0x0008,
		PixelCount = 0x0010,
		Entropy = 0x0020,
		Kurtosis = 0x0040,
		Skewness = 0x0080,
		All = Minimum | Maximum | Mean | Std | PixelCount | Entropy | Kurtosis | Skewness
	};
	Q_DECLARE_FLAGS(Statistics, Statistic);

	VipShapeStatistics()
		:pixelCount(0), average(0), std(0), min(0), max(0), entropy(0), kurtosis(0), skewness(0), minPoint(-1, -1), maxPoint(-1,-1)
	{}

	int pixelCount; //! Number of pixels
	double average; //! Average value
	double std; //! Standard deviation
	double min;//! Minimum value
	double max;//! Maximum value
	double entropy;
	double kurtosis;
	double skewness;
	QPoint minPoint;
	QPoint maxPoint;
	QList<QRect> quantiles;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipShapeStatistics::Statistics);

/// \a VipShape represents a 2D shape.
/// A 2D shape can be of several types:
/// - Any kind of closed path represented by the QPainterPath class
/// - A closed polygon
/// - A non closed polyline
/// - A point
///
/// Copies of the VipShape class share their data using explicit sharing. This means that modifying one node will change all copies.
/// You can make an independent (deep) copy of the scene model with #VipShape::copy.
///
/// A \a VipShape provides an interface to store and query any kind of attributes. See functions #VipShape::setAttributes, #VipShape::setAttribute, #VipShape::attribute, #VipShape::attributes.
class VIP_DATA_TYPE_EXPORT VipShape
{
	friend class VipSceneModel;

public:
	///Shape type
	enum Type
	{
		Unknown,
		Path,
		Polygon,
		Polyline,
		Point
	};


	VipShape();
	///Create the shape from a QPainterPath
	VipShape(const QPainterPath & path, Type type = Path, bool is_polygon_based = false);
	///Create a Polygon or Polyline shape
	VipShape(const QPolygonF & polygon, Type type = Polygon);
	///Create a polygon shape from a QRectF
	VipShape(const QRectF & rect);
	///Create a Point shape
	VipShape(const QPointF & point);
	///Copy constructor
	VipShape(const VipShape & other);

	///Copy operator
	VipShape & operator=(const VipShape & other);

	///Comparison operator.
	/// Only compares the internal data pointer, not the actual shape.
	bool operator!=(const VipShape & other) const;
	///Comparison operator.
	/// Only compares the internal data pointer, not the actual shape.
	bool operator==(const VipShape & other) const;

	///Returns true if the shape is valid
	bool isValid() const {return type() != Unknown;}
	///Returns true if the shape is null
	bool isNull() const {return type() == Unknown;}
	///Returns a copy of this shape
	VipShape copy() const;


	///Set the shape attributes
	void setAttributes(const QVariantMap & attrs) ;
	///Set the shape attribute \a name
	void setAttribute(const QString & name, const QVariant & value);
	///Returns the shape attributes
	QVariantMap attributes() const;
	///Set the shape attribute \a name
	QVariant attribute(const QString & name) const;
	///Returns true if the shape defines the attribute \a name
	bool hasAttribute(const QString & name) const;
	///Merge the shape attributes with \a attrs.
	/// Returns the attribute names that has been modified/created.
	QStringList mergeAttributes(const QVariantMap & attrs);

	///Set the 'Name' attribute
	void setName(const QString & name){setAttribute("Name",name);}
	///Returns the 'Name' attribute
	QString name() const;

	///Reset the VipShape shape.
	/// Returns a reference to this shape.
	VipShape& setShape(const QPainterPath & path, Type type = Path, bool is_polygon_based = false);
	///Reset the VipShape shape.
	/// Returns a reference to this shape.
	VipShape& setPolygon(const QPolygonF & polygon);
	///Reset the VipShape shape.
	/// Returns a reference to this shape.
	VipShape& setPolyline(const QPolygonF & polygon);
	///Reset the VipShape shape.
	/// Returns a reference to this shape.
	VipShape& setRect(const QRectF & rect);
	///Reset the VipShape shape.
	/// Returns a reference to this shape.
	VipShape& setPoint(const QPointF & point);
	///Apply a QTransform to the shape.
	/// Returns a reference to this shape.
	VipShape& transform(const QTransform & tr);
	///Reset the shape with the union of this shape and \a other one.
	/// The new shape is always of type #VipShape::Path.
	/// Returns a reference to this shape.
	VipShape & unite(const VipShape & other);
	///Reset the shape with the intersection of this shape and \a other one.
	/// The new shape is always of type #VipShape::Path.
	/// Returns a reference to this shape.
	VipShape & intersect(const VipShape & other);
	///Reset the shape by subtracting \a other.
	/// The new shape is always of type #VipShape::Path.
	/// Returns a reference to this shape.
	VipShape & subtract(const VipShape & other);

	bool isPolygonBased() const;

	///Returns the shape bounding rectangle
	QRectF boundingRect() const;
	///Returns the shape as a QPainterPath
	QPainterPath shape() const;
	///Returns the shape polygon
	QPolygonF polygon() const;
	///Returns the shape polyline
	QPolygonF polyline() const;
	///Returns the shape point
	QPointF point() const;
	///Returns the shape type
	Type type() const;


	//pixel management

	///Returns all the pixels filled by this shape
	QVector<QPoint> fillPixels() const;

	///Returns all the rects filled by this shape
	QVector<QRect> fillRects() const;

	///Returns the outlines of the shape.
	/// It is used to draw the extact pixels covered by the shape.
	/// This function is only valid for #VipShape::Path and #VipShape::Polygon.
	QList<QPolygon> outlines() const;

	///Returns the region exactly covered by this shape.
	/// If \a out_rects is not null, fill it with the region rects.
	QRegion region(QVector<QRect> *out_rects = nullptr) const;

	///Returns the shape identifier.
	/// Within a #VipSceneModel group, each shape has a unique non null identifier.
	/// If the shape does not belong to a #VipSceneModel, its identifier is 0 by default.
	int id() const;

	///Set the shape identifier.
	/// You should not need to call this function yourself.
	bool setId(int id);

	///Returns the shape group. The group is used to sort shapes of different nature.
	QString group() const;
	///Set the shape group
	void setGroup(const QString & group);

	///Returns the shape identifier (concatenation of the group and id with a colon separator)*
	/// This can be used to retrieve a shape from a VipSceneModel.
	/// \sa VipSceneModel::find
	QString identifier() const;

	///Returns the shape's parent #VipSceneModel, or a null #VipSceneModel if it has no parent
	VipSceneModel parent() const;
	///Returns the #VipShapeSignals associated with the parent's #VipSceneModel, or a nullptr VipShapeSignals if it has no parent.
	/// The #VipShapeSignals class is used to emit signals whenever a VipShape or a VipSceneModel changes.
	VipShapeSignals * shapeSignals() const;

	///Returns all pixels filled by a list of shapes
	static QVector<QPoint> fillPixels(const QList<VipShape> & shapes);

	///Returns all rectangle filled by a list of shapes
	static QVector<QRect> fillRects(const QList<VipShape> & shapes);

	///Given the list of pixels \a points, remove the pixels outside the bounding rectangle \a rect.
	/// If \a bounding is non nullptr, set it to the new pixels bounding rect.
	static QVector<QPoint> clip(const QVector<QPoint> & points, const QRect & rect, QRect * bounding = nullptr);

	///Given the list of rectangles \a rects, clip all rectangle with given bounding rectangle \a rect.
	/// If \a bounding is non nullptr, set it to the new bounding rect.
	static QVector<QRect> clip(const QVector<QRect> & rects, const QRect & rect, QRect * bounding = nullptr);

	///Extract the statistics inside an image for a list of pixels.
	/// \param points The input pixels to consider
	/// \param img the input image
	/// \param img_offset the input image offset
	/// \param the bounding rect of input points. If a null rect is given it will be computed.
	/// \param buffer a buffer image that will speed up the computing. This only necessary if you intent to compute the statistics with the same shape for several images.
	/// \return a list of #VipShapeStatistics (one per image component). If input image is a color image, output will contain the statistics for components Alpha, Red, Green, Blue.
	/// If input image is complex, output will contain the statistics for components Real and Imag. Otherwise, only one component statistics is returned.
	static VipShapeStatistics statistics(const QVector<QRect> & rects, const VipNDArray & img, const QPoint & img_offset = QPoint(0,0), const QRect & bounding_rect = QRect(), VipNDArray * buffer = nullptr, VipShapeStatistics::Statistics stats = VipShapeStatistics::All, const QVector<double> & bbox_quantiles = QVector<double>());

	///Extract the statistics inside an image for this shape.
	/// \param img the input image
	/// \param img_offset the input image offset
	/// \param buffer a buffer image that will speed up the computing. This only necessary if you intent to compute the statistics with the same shape for several images.
	/// \return a list of #VipShapeStatistics (one per image component). If input image is a color image, output will contain the statistics for components Alpha, Red, Green, Blue.
	/// If input image is complex, output will contain the statistics for components Real and Imag. Otherwise, only one component statistics is returned.
	VipShapeStatistics statistics(const VipNDArray & img, const QPoint & img_offset = QPoint(0,0), VipNDArray * buffer = nullptr, VipShapeStatistics::Statistics stats = VipShapeStatistics::All, const QVector<double>& bbox_quantiles = QVector<double>()) const;

	template<class T, Vip::ArrayStats Stats>
	VipArrayStats<T, Stats> imageStats(const VipNDArray & img, const QPoint & img_offset = QPoint(0, 0)) const
	{
		return vipArrayStats<T, Stats>(img, vipOverRects(region()),vipVector(img_offset.y(), img_offset.x()));
	}

	///Extract the histogram inside an image for a list of pixels.
	/// \param bins number of bins for the output histogram(s)
	/// \param points The input pixels to consider
	/// \param img the input image
	/// \param img_offset the input image offset
	/// \param the bounding rect of input points. If a null rect is given it will be computed.
	/// \param buffer a buffer image that will speed up the computing. This only necessary if you intent to compute the statistics with the same shape for several images.
	/// \return a list of #VipIntervalSample (one per image component). If input image is a color image, output will contain the histogram for components Alpha, Red, Green, Blue.
	/// If input image is complex, output will contain the histogram for components Real and Imag. Otherwise, only one component histogam is returned.
	static  QVector<VipIntervalSample> histogram(int bins, const QVector<QRect> & rects, const VipNDArray & img, const QPoint & img_offset = QPoint(0,0), const QRect & bounding_rect = QRect(), VipNDArray * buffer = nullptr );
	///Extract the histogram inside an image for this shape.
	/// \param bins number of bins for the output histogram(s)
	/// \param img the input image
	/// \param img_offset the input image offset
	/// \param buffer a buffer image that will speed up the computing. This only necessary if you intent to compute the statistics with the same shape for several images.
	/// \return a list of #VipIntervalSample (one per image component). If input image is a color image, output will contain the histogram for components Alpha, Red, Green, Blue.
	/// If input image is complex, output will contain the histogram for components Real and Imag. Otherwise, only one component histogam is returned.
	QVector<VipIntervalSample> histogram(int bins , const VipNDArray & img, const QPoint & img_offset = QPoint(0,0), VipNDArray * buffer = nullptr ) const;

	///Extract the pixel values inside an image for a list of pixels.
	/// \param points The input pixels to consider
	/// \param img the input image
	/// \param img_offset the input image offset
	/// \param the bounding rect of input points. If a null rect is given it will be computed.
	/// \param buffer a buffer image that will speed up the computing. This only necessary if you intent to compute the statistics with the same shape for several images.
	/// \return a list of point vector (one per image component), where each point contains (pixel index, pixel value). If input image is a color image, output will contain the pixel values for components Alpha, Red, Green, Blue.
	/// If input image is complex, output will contain the pixel values for components Real and Imag. Otherwise, only one component pixel values is returned.
	static QVector<QPointF> polyline(const QVector<QPoint> & points, const VipNDArray & img, const QPoint & img_offset = QPoint(0,0), const QRect & bounding_rect = QRect(), VipNDArray * buffer = nullptr );
	///Extract the pixel values inside an image for this shape (type() == Polyline).
	/// \param img the input image
	/// \param img_offset the input image offset
	/// \param buffer a buffer image that will speed up the computing. This only necessary if you intent to compute the statistics with the same shape for several images.
	/// \return a list of point vector (one per image component), where each point contains (pixel index, pixel value). If input image is a color image, output will contain the pixel values for components Alpha, Red, Green, Blue.
	/// If input image is complex, output will contain the pixel values for components Real and Imag. Otherwise, only one component pixel values is returned.
	QVector<QPointF> polyline(const VipNDArray & img, const QPoint & img_offset = QPoint(0,0), VipNDArray * buffer = nullptr ) const;

	///Write a value for all given pixels in an output image
	/// \param value pixel value to write
	/// \param point input pixels
	/// \param img output image
	/// \param img_offset output image offset
	/// \param bounding_rect input pixels bounding rect. If a null rect is given it will be computed.
	/// \return true on success, false otherwise
	static bool writeAttribute( const QVariant & value, const QVector<QPoint> & points, VipNDArray & img, const QPoint & img_offset = QPoint(0,0), const QRect & bounding_rect = QRect());
	///Write an attribute for all given pixels in an output image
	/// \param attribute name
	/// \param img output image
	/// \param img_offset output image offset
	/// \return true on success, false otherwise
	bool writeAttribute(const QString & attribute, VipNDArray & img, const QPoint & img_offset = QPoint(0,0));

	/// Internal shape id, used to uniquely identify shapes.
	qint64 internalId() const { return (qint64)d_data.data(); }
private:
	void emitShapeChanged();

	class PrivateData;
	QSharedPointer<VipShape::PrivateData> d_data;
};

Q_DECLARE_METATYPE(VipShape)
Q_DECLARE_METATYPE(QPainterPath)

typedef QList<VipShape> VipShapeList;
Q_DECLARE_METATYPE(VipShapeList);

VIP_DATA_TYPE_EXPORT int vipShapeCount();


/// \a VipSceneModel is a collection of #VipShape sorted by groups.
/// A group is a string identifier that categorize a list of shapes. For instance, in Thermavip, all closed shapes (path or polygon) drawn with drawing tool widget are in the group 'ROI' (for Regions Of Interest).
///
/// \a VipSceneModel provides functions to add or remove shapes (see  #VipSceneModel::add, #VipSceneModel::remove or #VipSceneModel::removeGroup),
/// and to query for specific shapes (see #VipSceneModel::at, #VipSceneModel::find and #VipSceneModel::shapes).
/// Notes that adding a shape to a scene model will remove it from its previous scene model (if any).
///
/// Copies of the VipSceneModel class share their data using explicit sharing. This means that modifying one node will change all copies.
/// You can make an independent (deep) copy of the scene model with #VipSceneModel::copy.
///
/// \a VipSceneModel class internally holds a #VipShapeSignals used to emit signals whenever the scene model or a single shape changes.
class VIP_DATA_TYPE_EXPORT VipSceneModel
{
	friend class VipShape;
	friend class VipShapeSignals;

public:

	static VipSceneModel null();

	///Constructor
	VipSceneModel();
	///Destructor
	~VipSceneModel();
	///Copy constructor
	VipSceneModel(const VipSceneModel & other);

	///Copy operator
	VipSceneModel & operator=(const VipSceneModel & other);
	///Comparison operator
	bool operator!=(const VipSceneModel & other) const;
	///Comparison operator
	bool operator==(const VipSceneModel & other) const;

	/// Remove all shapes and groups from the scene model
	void clear();

	///Returns a deep copy of this scene model
	VipSceneModel copy() const;
	///Applies a QTransform to all shapes of this scene model
	void transform(const QTransform & tr);

	///Add a shape to given \a group
	VipSceneModel & add(const QString & group, const VipShape & shape);
	///Add a list of shapes to given \a group
	VipSceneModel & add(const QString & group, const QList<VipShape> & shapes);
	///Add a list of shapes.
	/// Each shape will be inserted in the group returned by #vipShape::group with, if possible, the id returned by #VipShape::id.
	VipSceneModel & add(const QList<VipShape> & shapes);
	///Add a shape. The shape will be inserted in the group returned by #vipShape::group with, if possible, the id returned by #VipShape::id.
	VipSceneModel & add(const VipShape & shape);
	///Add the content of a scene model in this scene model. This will clear the content of \a other.
	VipSceneModel & add(const VipSceneModel & other);
	///Removes a shape from the scene model
	VipSceneModel & remove(const VipShape & shape);
	VipSceneModel & remove(const VipShapeList & shapes);
	///Removes a full group from the scene model
	VipSceneModel & removeGroup(const QString & group);
	///Add a new shape and try to force its id.
	/// if the id cannot be set, the shape is not added and this function returns false.
	bool add(const VipShape & shape, int id);

	/// Reset the content of this scene model with the content of \a other.
	/// This will clear \a other.
	/// This function is similar to clear() + add().
	/// It triggers the groupAdded() and groupRemoved() only if necessary.
	VipSceneModel & reset(const VipSceneModel & other);

	///Returns true if the scene model is empty
	bool isEmpty() const {return groupCount() == 0;}
	///Returns true if the group \a groups exists
	bool hasGroup(const QString & group);
	///Returns the number of shapes inside \a group
	int shapeCount(const QString & group) const;
	///Returns the total number of shapes in this scene model
	int shapeCount() const;
	///Returns the total number of group in this scene model
	int groupCount() const;
	///Returns all group names of this scene model
	QStringList groups() const;
	int indexOf(const QString & group, const VipShape &) const;

	///Returns the shape within the group \a group at index \a index
	VipShape at(const QString & group, int index) const;
	///Returns the shape within the group \a group with id \a id
	VipShape find(const QString & group, int id) const;

	///Returns the shape at given path.
	/// The path is a concatenation of the group name and the shape id, separated by a colon character.
	/// \sa VipShape::identifier()
	VipShape find(const QString & path) const;
	///Returns all shapes that belong to \a group
	QList<VipShape> shapes(const QString & group) const;
	///Returns scene model shapes
	QList<VipShape> shapes() const;
	///Returns scene model shapes sorted by groups
	QMap<QString, QList<VipShape> > groupShapes() const;
	//QList<VipShape> selectedShapes() const;

	///Returns the full scene model path
	QPainterPath shape() const;
	///Returns the full scene model bounding rect
	QRectF boundingRect() const;

	///Returns the #VipShapeSignals associated to this scene model
	VipShapeSignals * shapeSignals() const;

	///Returns true if this scene model is nullptr.
	/// A \a VipSceneModel can never be null, except in return of #VipShape::parent.
	bool isNull() const {return !d_data;}


	///Set the scene model attributes
	void setAttributes(const QVariantMap & attrs);
	///Set the scene model attribute \a name
	void setAttribute(const QString & name, const QVariant & value);
	///Returns the scene model attributes
	QVariantMap attributes() const;
	///Returns the scene model attribute \a name
	QVariant attribute(const QString & name) const;
	///Returns true if the scene model defines the attribute \a name
	bool hasAttribute(const QString & name) const;
	///Merge the scene model attributes with \a attrs.
	/// Returns the attribute names that has been modified/created.
	QStringList mergeAttributes(const QVariantMap & attrs);

	///Convert to boolean
	operator void*() const {return d_data.data();}

private:
	class PrivateData;
	QSharedPointer<VipSceneModel::PrivateData> d_data;

	VipSceneModel(QSharedPointer<VipSceneModel::PrivateData> data);
};

Q_DECLARE_METATYPE(VipSceneModel)

typedef QList<VipSceneModel> VipSceneModelList;
Q_DECLARE_METATYPE(VipSceneModelList)

VIP_DATA_TYPE_EXPORT int vipSceneModelCount();


/// \a VipShapeSignals is used by #VipSceneModel to emit signals whenever the scene model or one of the shapes changes.
class VIP_DATA_TYPE_EXPORT VipShapeSignals : public QObject
{
	Q_OBJECT
	friend class VipShape;
	friend class VipSceneModel;

public:

	VipShapeSignals();
	~VipShapeSignals();

	///Returns the scene model object associated with this VipShapeSignals
	VipSceneModel sceneModel() const;

Q_SIGNALS:
	///Emitted whenever a scene model's shape changed
	void sceneModelChanged(const VipSceneModel &);
	///Emitted whenever a new group is added to the scene model
	void groupAdded(const QString &);
	///Emitted whenever a group is removed from the scene model
	void groupRemoved(const QString &);
private:
	QWeakPointer<VipSceneModel::PrivateData> d_data;
};


/// @}
//end DataType

#endif
