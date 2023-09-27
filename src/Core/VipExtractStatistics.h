#ifndef VIP_EXTRACT_STATISTICS_H
#define VIP_EXTRACT_STATISTICS_H

#include "VipExtractComponents.h"
#include "VipProcessingObject.h"
#include "VipSceneModel.h"

/// \addtogroup Core
/// @{

class VipArchive;

/// \a VipExtractComponent is a \a VipProcessingObject that extract a component from a \a VipNDArray.
/// For instance, it can extract the Red, Green, Blue or Alpha component from a color image, the Real, Imaginary, Amplitude or Argument from a complex image, etc.
///
/// The component is a QString property.
class VIP_CORE_EXPORT VipExtractComponent : public VipProcessingObject
{
	Q_OBJECT

	Q_CLASSINFO("description", "Extract a unique component from an image")
	Q_CLASSINFO("category", "Miscellaneous")

	VIP_IO(VipInput image)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty component)

	QStringList m_supportedComponents;
	VipGenericExtractComponent m_extract;

public:
	VipExtractComponent(QObject* parent = NULL)
	  : VipProcessingObject(parent)
	{
	}
	QStringList supportedComponents() const;
	void resetSupportedComponents() { m_supportedComponents.clear(); }

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
	}

	bool isInvariant() const { return m_extract.isInvariant(); }

	QString defaultComponent() const
	{
		// return the preferred component for display. Usually an empty one, unless for complex image (use the 'Real' component)
		if (m_supportedComponents.indexOf("Real") >= 0)
			return "Real";
		// TEST: return the first component
		if (m_supportedComponents.size())
			return m_supportedComponents.first();
		return QString();
	}

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractComponent*)

/// Split input data into multiple components (like ARGB images or complex ones), and
/// apply independent algorithms on each component before merging them back.
///
/// The split method is set with #VipSplitAndMerge::setMethod. It could be one of the following:
/// - "Color ARGB"
/// - "Color AHSL"
/// - "Color AHSV"
/// - "Color ACMYK"
/// - "Complex Real/Imag"
/// - "Complex Amplitude/Argument"
class VIP_CORE_EXPORT VipSplitAndMerge : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput Input)
	VIP_IO(VipOutput Output)
	Q_CLASSINFO("description", "Split input data into multiple components (like ARGB images or complex ones),\nand apply independent algorithms on each component before merging them back")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipSplitAndMerge(QObject* parent = NULL);
	~VipSplitAndMerge();

	/// Set the split/merge method.
	/// This processing must already have a valid input data. If this input data cannot be splitted with given method, false is returned and nothing is done.
	/// Otherwise, try to create the new processing lists (one per component) and, if possible, keep the processings that were added before (if any, and only possible
	/// if the current method has the same number of components as the previous one).
	bool setMethod(const QString& method);
	const QString& method() const { return m_method; }
	/// Returns the possible components for the current method
	QStringList components() const;

	static bool acceptData(const QVariant& data, const QString& method);
	static int componentCount(const QString& method);
	static QStringList possibleMethods(const QVariant& data);

	int componentCount() const { return m_procList.size(); }
	VipProcessingList* componentProcessings(int index) const { return m_procList[index]; }

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int, const QVariant& v);

protected:
	virtual void apply();

private Q_SLOTS:
	void receivedProcessingDone(VipProcessingObject* lst, qint64);
	void applyInternal(bool update);

private:
	QMutex m_mutex;
	QString m_method;
	VipExtractComponents* m_extract;
	QVector<VipProcessingList*> m_procList;
	bool m_is_applying;
};

VIP_REGISTER_QOBJECT_METATYPE(VipSplitAndMerge*)

/// Base class for processing that extract any kind of statistics from a #VipNDArray and a #VipShape.
class VIP_CORE_EXPORT VipExtractShapeData : public VipSceneModelBasedProcessing
{
	Q_OBJECT

public:
	VipExtractShapeData(QObject* parent = NULL)
	  : VipSceneModelBasedProcessing(parent)
	{
	}

	/// Returns all default components for the current VipNDArray.
	/// For color image, returns "Alpha","Red","Green",Blue". For complex image, returns "Real","Image".
	QStringList components() const { return m_components; }

protected:
	void setArray(const VipNDArray& ar);
	VipNDArray& buffer() { return m_buffer; }

private:
	QStringList m_components;
	VipNDArray m_buffer;
	VipShape m_shape;
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractShapeData*)

/// Extract the histogram of an image VipNDArray.
/// The property \a bins is the number of bins in the histogram (100 by default).
///
/// \a histograms is a VipMultiOutput that stores the histograms for all components
class VIP_CORE_EXPORT VipExtractHistogram : public VipExtractShapeData
{
	Q_OBJECT

	VIP_IO(VipInput image)
	VIP_IO(VipProperty bins)
	VIP_IO(VipProperty method)
	VIP_IO(VipMultiOutput histograms)
	VIP_IO(VipProperty output_name)
public:
	VipExtractHistogram(QObject* parent = NULL)
	  : VipExtractShapeData(parent)
	  , m_extract(NULL)
	{
		propertyName("bins")->setData(1000);
		propertyName("output_name")->setData(QString());
	}
	~VipExtractHistogram();

	VipExtractComponents* extract() const;

protected:
	virtual void apply();

private:
	VipExtractComponents* m_extract;
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractHistogram*)

/// Extract the pixel values of a VipNDArray image along a polyline.
///
/// \a polylines is a VipMultiOutput that stores the pixels for all components
class VIP_CORE_EXPORT VipExtractPolyline : public VipExtractShapeData
{
	Q_OBJECT

	VIP_IO(VipInput image)
	VIP_IO(VipMultiOutput polylines)
	VIP_IO(VipProperty method)
	VIP_IO(VipProperty output_name)
public:
	VipExtractPolyline(QObject* parent = NULL)
	  : VipExtractShapeData(parent)
	  , m_extract(NULL)
	{
		propertyName("output_name")->setData(QString());
	}
	~VipExtractPolyline()
	{
		if (m_extract)
			delete m_extract;
	}
	VipExtractComponents* extract() const { return const_cast<VipExtractComponents*>(m_extract); }

protected:
	virtual void apply();

private:
	VipExtractComponents* m_extract;
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractPolyline*)

/// Extract the minimum value, maximum, mean, standard deviation and the pixel count of a VipNDArray image inside a \a VipShape.
/// This only works for numerical images (no color or complex image).
class VIP_CORE_EXPORT VipExtractStatistics : public VipExtractShapeData
{
	Q_OBJECT

	VIP_IO(VipInput image)

	VIP_IO(VipOutput min)
	VIP_IO(VipOutput max)
	VIP_IO(VipOutput mean)
	VIP_IO(VipOutput std)
	VIP_IO(VipOutput pixel_count);
	VIP_IO(VipOutput entropy);
	VIP_IO(VipOutput kurtosis);
	VIP_IO(VipOutput skewness);
	VIP_IO(VipOutput quantiles); // define as QList<QRect> (ot VipRectList)
public:
	VipExtractStatistics(QObject* parent = NULL)
	  : VipExtractShapeData(parent)
	  , m_stats(VipShapeStatistics::All)
	{
	}

	/// Set the statistics we want to extract. The corresponding outputs will only update there values if their statistics are enabled.
	void setStatistics(VipShapeStatistics::Statistics);
	void setStatistic(VipShapeStatistics::Statistic, bool on = true);
	bool testStatistic(VipShapeStatistics::Statistic) const;
	VipShapeStatistics::Statistics statistics() const;

	/// Set the bounding box quantile values that must be extracted (if any).
	/// This bounding box includes all pixels belonging to the (quantile * 100)% highest pixels.
	/// The quantil bounding boxes are stored in the quantiles output as a VipRectList object.
	void setShapeQuantiles(const QVector<double>& quantiles);
	const QVector<double>& shapeQuantiles() const;

protected:
	virtual void apply();

private:
	void updateStatistics();

	VipShapeStatistics::Statistics m_stats;
	QVector<double> m_quantiles;
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractStatistics*)
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& stream, const VipExtractStatistics* r);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& stream, VipExtractStatistics* r);

/// Extract a VipShape's attribute based on an input VipSceneModel and the properties \a shape_group, \a shape_id (to find the shape in
/// the scene model) and \a shape_attribute (the attribute name in the VipShape).
/// The output value is stored as a processing output. The value is converted to double if possible.
class VIP_CORE_EXPORT VipExtractShapeAttribute : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput scene_model)
	VIP_IO(VipOutput value)
	VIP_IO(VipProperty shape_group)
	VIP_IO(VipProperty shape_id)
	VIP_IO(VipProperty shape_attribute)

public:
	VipExtractShapeAttribute();

protected:
	virtual void apply();
};

/// @}
// end Core

#endif
