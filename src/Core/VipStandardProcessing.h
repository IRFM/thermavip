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

#ifndef VIP_STANDARD_PROCESSING_H
#define VIP_STANDARD_PROCESSING_H

#include "VipProcessingHelper.h"
#include "VipProcessingObject.h"

/// Represents a data comming from another processing object.
class VIP_CORE_EXPORT VipOtherPlayerData
{
	friend QDataStream& operator<<(QDataStream& arch, const VipOtherPlayerData& o);
	friend QDataStream& operator>>(QDataStream& arch, VipOtherPlayerData& o);

public:
	VipOtherPlayerData();
	VipOtherPlayerData(const VipAnyData& static_data);
	VipOtherPlayerData(bool is_dynamic, VipProcessingObject* object, VipProcessingObject* parent, int outputIndex, int otherPlayerId = -1, int otherDisplayIndex = -1);
	bool isDynamic() const;
	int otherPlayerId() const;
	int otherDisplayIndex() const;
	int outputIndex() const;
	VipProcessingObject* processing() const;
	VipAnyData staticData() const;
	VipAnyData dynamicData();
	VipAnyData data();

	// additional flags not necessarly used

	/// Tells if the output data should be resized
	void setShouldResizeArray(bool enable);
	bool shouldResizeArray() const;

	/// Set/get the processing object that uses this VipOtherPlayerData as input.
	///  This is used in dynamic mode to avoid infinit recursion.
	void setParentProcessing(VipProcessingObject* parent);
	VipProcessingObject* parentProcessingObject() const;

private:
	class PrivateData;
	QSharedPointer<PrivateData> m_data;
};

Q_DECLARE_METATYPE(VipOtherPlayerData)

/// Normalize a VipPointVector, a VipIntervalSampleVector or a VipNDArray between 2 values given as parameters.
/// By default, normalize between 0 and 1.
class VIP_CORE_EXPORT VipNormalize : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty minimum)
	VIP_IO(VipProperty maximum)
	Q_CLASSINFO("description",
		    "Normalize an image, a curve or a histogram between 2 values given as properties.\n"
		    "Default behavior normalizes between 0 and 1.")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipNormalize(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipNormalize*)

/// Clamp a VipPointVector, a VipIntervalSampleVector, VipNDArray or numerical value between 2 values given as parameters.
/// By default, the minimum and maximum are set to NaN, which means that clampling is disabled.
class VIP_CORE_EXPORT VipClamp : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty minimum)
	VIP_IO(VipProperty maximum)
	Q_CLASSINFO("description", "Clamp an image, a curve or a histogram between 2 values given as properties")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipClamp(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipClamp*)

/// Clamp a VipPointVector, a VipIntervalSampleVector, VipNDArray or numerical value between 2 values given as parameters.
/// By default, the minimum and maximum are set to NaN, which means that clampling is disabled.
class VIP_CORE_EXPORT VipAbs : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("description", "Absolute value of an image or point vector (possibly complex, the norm is used)")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipAbs(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipAbs*)

/// Convert a numeric value, a QString, a QByteArray, a VipPointVector or a VipNDArray to given type.
/// The output data type is given as a property.
class VIP_CORE_EXPORT VipConvert : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty out_type)
	Q_CLASSINFO("description", "Convert a numeric value, a string, a point vector or a ND array to given type")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipConvert(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipConvert*)

/// Apply a constant offset to the X values of a VipPointVector in order to start at 0.
class VIP_CORE_EXPORT VipStartAtZero : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("description", "Apply an offset to a curve to make it start at zero")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipStartAtZero(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

/// Apply a constant offset to the X values of a VipPointVector in order to start at 0.
class VIP_CORE_EXPORT VipStartYAtZero : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("description", "Apply an offset to a curve to make it start at zero")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipStartYAtZero(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

/// Apply a constant offset to the X values of a VipPointVector in order to start at 0.
class VIP_CORE_EXPORT VipXOffset : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty offset)
	Q_CLASSINFO("description", "Apply a X offset to a curve\nFor temporal curves, don't forget that the unit is the nanosecond.")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipXOffset(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipStartAtZero*)

class VIP_CORE_EXPORT FastMedian3x3 : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("category", "Filters")
public:
	FastMedian3x3()
	  : VipProcessingObject()
	{
	}
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipNDArray>();
	}

protected:
	virtual void apply();
};
VIP_REGISTER_QOBJECT_METATYPE(FastMedian3x3*)

/// Read successive numeric values and convert them to a VipPointVector.
/// Read values are used for the y axis, read data times are used for the x axis.
class VIP_CORE_EXPORT VipNumericValueToPointVector : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Sliding_time_window)
	VIP_IO(VipProperty Restart_after)
	VIP_IO_DESCRIPTION(Sliding_time_window, "Temporal window of the curve (seconds).\nThis is only used when plotting a continuous curve (streaming)")
	VIP_IO_DESCRIPTION(Restart_after, "If 2 successives samples have a time difference greater than 'Restart_after', restart the curve (0 to disable)")
	Q_CLASSINFO("description",
		    "Read successive numeric values and convert them to a VipPointVector.\n"
		    "Read values are used for the y axis, read data times are used for the x axis.")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipNumericValueToPointVector(QObject* parent = nullptr);
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;

protected:
	virtual void apply();
	virtual void resetProcessing();

private:
	VipPointVector m_vector;
};

VIP_REGISTER_QOBJECT_METATYPE(VipNumericValueToPointVector*)

/// Base class for data fusion algorithms.
/// VipBaseDataFusion only defines a VipMultiInput and several methods to handle the inputs.
/// The algorithm inputs can be of any type, but some types are treated in a special way: VipNDArray, VipPointVector and VipComplexPointVector.
///
/// Subclasses must reimplement the #VipBaseDataFusion::mergeData member function.
class VIP_CORE_EXPORT VipBaseDataFusion : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiInput input)
	VIP_IO(VipProperty Time_range)
	VIP_IO_DESCRIPTION(Time_range, "Apply the processing on the union or intersection of input signals")
	VIP_PROPERTY_EDIT(Time_range, "VipEnumEdit{ qproperty-enumNames:'union,intersection';  qproperty-value:'intersection' ;}")

public:
	VipBaseDataFusion(QObject* parent = nullptr);
	~VipBaseDataFusion();

	void setAceptedInputs(const QList<int>& input_types);
	const QList<int>& acceptedInputs() const;

	/// If enabled, all algorithm inputs must be of the same object type: all VipNDArray, or all VipPointVector, or all Double,...
	/// If this condition is not met, the #VipBaseDataFusion::apply member will return with an error flag and without calling #VipBaseDataFusion::mergeData.
	void setWorkOnSameObjectType(bool enable);
	bool workOnSameObjectType() const;

	/// If enabled, all algorithm inputs must be convertible to the same data type.
	/// For instance, if the algorithm defines 2 inputs that contain a double numeric value and a VipNDArray containing integer values,
	/// the VipNDArray will be converted to a double array. Or, if the algorithm defines 2 inputs that contain a VipPointVector and a VipComplexPointVector,
	/// the VipPointVector will be converted to a VipComplexPointVector.
	///
	/// This function uses #vipHigherArrayType function to find the destination type.
	/// If \a possible_types is not empty, the destination type will be one defined in \a possible_types.
	///
	/// Not that some conversions are not possible: a VipComplexPointVector cannot be converted to VipPointVector, a complex value
	/// cannot be convertible to a numeric one,...
	///
	/// If at least one input is not convertible to the destination type,  the #VipBaseDataFusion::apply member will return with an error flag and without calling #VipBaseDataFusion::mergeData.
	void setSameDataType(bool enable, const QList<int>& possible_types = QList<int>());
	bool sameDataType() const;
	const QList<int>& possibleDataTypes() const;

	/// If enabled, inputs data will be resampled. This only applies to VipNDArray, VipPointVector and VipComplexPointVector.
	/// All VipNDArray will be resized to the widest possible shape. However, they still need to have the same number of dimensions.
	/// All VipPointVector and VipComplexPointVector are resampled with #vipResampleTemporalVectors.
	///
	/// If \a merge_point_vectors is true, VipComplexPointVector and VipPointVector will be resampled together (they will share the same bounds and sampling value).
	void setResampleEnabled(bool enable, bool merge_point_vectors = false);
	bool resampleEnabled() const;
	bool mergePointVectors() const;

	/// If resampling is enabled, set the interpolation type used when resizing VipNDArray.
	void setResizeArrayType(Vip::InterpolationType);
	Vip::InterpolationType resizeArrayType() const;

	/// Helper function.
	/// Usually , the output name of a data fusion processing will gather the names of its inputs.
	/// However, this could end up in a very long name. This function will find the common starting
	/// prefix for all inputs in order to remove it in the output name (and shorten it).
	static QString startPrefix(const QStringList& names);
	static QString startPrefix(const QVector<VipAnyData>& inputs);

protected:
	virtual void apply();

	/// Reimplement in subclasses.
	/// Apply the algorithm.
	/// \a data_type is the common object type for all inputs (like all VipNDArray, or all VipPointVector,...). Set to 0 if workOnSameObjectType is not enabled.
	/// \a sub_data_type is the common data type for all inputs. Set to 0 if sameDataType is not enabled.
	virtual void mergeData(int data_type, int sub_data_type) = 0;

	/// Same as #VipProcessingObject::create, but also set the output time (max time of all inputs)
	/// and merge all inputs attributes.
	virtual VipAnyData create(const QVariant& data) const;

	const QVector<VipAnyData>& inputs() const;

private:
	class PrivateData;
	PrivateData* m_data;
};
/// Extract a feature (min, max, mean,...) from a set of samples (images, curves,...)
class VIP_CORE_EXPORT VipSamplesFeature : public VipBaseDataFusion
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Feature) //'min', 'max', 'mean', 'median'
	Q_CLASSINFO("description", "Extract the minimum, maximum, mean or median image/curve/value from N images/curves/values")
	Q_CLASSINFO("category", "Data Fusion/Numeric Operation")
	VIP_PROPERTY_EDIT(Feature, "VipEnumEdit{ qproperty-enumNames:'min,max,mean,median';  qproperty-value:'max' ;}")

public:
	VipSamplesFeature(QObject* parent = nullptr);
	~VipSamplesFeature();

	virtual DisplayHint displayHint() const { return DisplayOnSameSupport; }

protected:
	virtual void mergeData(int data_type, int sub_data_type);

private:
	void setOutput(const QVariant& v);
	class PrivateData;
	PrivateData* m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipSamplesFeature*)

/// @brief Running average working on images or curves
class VIP_CORE_EXPORT VipRunningAverage : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Window)
	Q_CLASSINFO("category", "Filters")

	VipSamplesFeature m_extract;
	QList<VipAnyData> m_lst;
	QMutex m_mutex;

public:
	VipRunningAverage();
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<VipPointVector>();
	}

protected:
	virtual void apply();
	virtual void resetProcessing();
};
VIP_REGISTER_QOBJECT_METATYPE(VipRunningAverage*)

/// @brief Running median working on images or curves
class VIP_CORE_EXPORT VipRunningMedian : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Window)
	Q_CLASSINFO("category", "Filters")

	VipSamplesFeature m_extract;
	QList<VipAnyData> m_lst;
	QMutex m_mutex;

public:
	VipRunningMedian();
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<VipPointVector>();
	}

protected:
	virtual void apply();
	virtual void resetProcessing();
};
VIP_REGISTER_QOBJECT_METATYPE(VipRunningMedian*)

/// Extract the bounding boxes of the regions inside an image.
/// Usually, the image must have been segmented and a CCL algorithm have been applied.
///
/// For each possible pixel value in the image, this algorithm will compute the bounding box of all pixels with the same value.
/// The output is a VipSceneModel. All bboxes are added with a label given as parameter (default is "All").
/// The pixel value 0 is considered as the image background and is ignored.
class VIP_CORE_EXPORT VipExtractBoundingBox : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput image)
	VIP_IO(VipOutput scene_model)
	VIP_IO(VipProperty Bounding_Box_label)
	Q_CLASSINFO("description",
		    "Extract the bounding boxes of the regions inside an image.\n"
		    "Usually, the image must have been segmented and a CCL algorithm have been applied.\n"
		    "\n"
		    "For each possible pixel value in the image, this algorithm will compute the bounding box of all pixels with the same value.\n"
		    "The output is a VipSceneModel.All bboxes are added with a label given as parameter(default is 'All').\n"
		    "The pixel value 0 is considered as the image background and is ignored.)")

	Q_CLASSINFO("category", "Computer Vision")

public:
	VipExtractBoundingBox(QObject* parent = nullptr);
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const;
	virtual VipProcessingObject::DisplayHint displayHint() const { return DisplayOnSameSupport; }

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipExtractBoundingBox*)

/// Returns the minimum and maximum X values for given list of VipPointVector, as well as the minimum non 0 sampling value (if provided).
/// This function assumes that each vector as increasing x values representing nanoseconds.
/// Returns an invalid time interval (first > second) in case of error.
VIP_CORE_EXPORT VipTimeRange vipFindTemporalVectorsBoundaries(const QList<VipPointVector>& vectors, qint64* min_sampling);
VIP_CORE_EXPORT VipTimeRange vipFindTemporalVectorsBoundaries(const QList<VipComplexPointVector>& vectors, qint64* min_sampling);
VIP_CORE_EXPORT VipTimeRange vipFindTemporalVectorsBoundaries(const QList<VipPointVector>& vectors, const QList<VipComplexPointVector>& cvectors, qint64* min_sampling);

/// Resample a VipPointVector based on given start and end X values and sampling value.
/// This function assumes that the vector as increasing x values representing nanoseconds.
/// This function always returns a vector of size floor((end - start) / sampling + 1), except if its size is too high or if sampling is <= 0.
VIP_CORE_EXPORT VipPointVector vipResampleTemporalVector(const VipPointVector& vector, const VipTimeRange& range, qint64 sampling);
VIP_CORE_EXPORT VipComplexPointVector vipResampleTemporalVector(const VipComplexPointVector& vector, const VipTimeRange& range, qint64 sampling);
VIP_CORE_EXPORT void vipResampleTemporalVector(VipPointVector& vector, VipComplexPointVector& cvector, const VipTimeRange& range, qint64 sampling);

/// Resample multiple VipPointVector objects based on given start and end X values and sampling value.
VIP_CORE_EXPORT QList<VipPointVector> vipResampleTemporalVectors(const QList<VipPointVector>& vectors, const VipTimeRange& range, qint64 sampling);
VIP_CORE_EXPORT QList<VipComplexPointVector> vipResampleTemporalVectors(const QList<VipComplexPointVector>& vectors, const VipTimeRange& range, qint64 sampling);
VIP_CORE_EXPORT void vipResampleTemporalVectors(QList<VipPointVector>& vectors, QList<VipComplexPointVector>& cvectors, const VipTimeRange& range, qint64 sampling);
/// Same as #vipResampleTemporalVectors but returns the result as a VipNDArray image.
/// The VipNDArray as a width of vectors.size()+1 where the first column is the X values for all vectors and the other columns are the Y values of each vector.
VIP_CORE_EXPORT VipNDArray vipResampleTemporalVectorsAsNDArray(const QList<VipPointVector>& vectors, const VipTimeRange& range, qint64 sampling);

/// Resample multiple VipPointVector objects.
/// This function uses #vipFindTemporalVectorsBoundaries to compute the minimum and maximum X values and the sampling value.
VIP_CORE_EXPORT QList<VipPointVector> vipResampleTemporalVectors(const QList<VipPointVector>& vectors);
VIP_CORE_EXPORT QList<VipComplexPointVector> vipResampleTemporalVectors(const QList<VipComplexPointVector>& vectors);
VIP_CORE_EXPORT bool vipResampleTemporalVectors(QList<VipPointVector>& vectors, QList<VipComplexPointVector>& cvectors);

/// Same as #vipResampleTemporalVectors but returns the result as a VipNDArray image.
/// The VipNDArray as a width of vectors.size()+1 where the first column is the X values for all vectors and the other columns are the Y values of each vector.
VIP_CORE_EXPORT VipNDArray vipResampleTemporalVectorsAsNDArray(const QList<VipPointVector>& vectors);

/// @brief Multiply, add, subtract or divide a combination of numerical values, complex values and or VipNDArray.
/// The operator is given in the 'Operator' property (could be: '*','+','-', '/', '&', '|', '^').
class VIP_CORE_EXPORT VipNumericOperation : public VipBaseDataFusion
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Operator)

	Q_CLASSINFO("description",
		    "Multiply, add, subtract, divide, or apply a binary operator on a combination of numerical values, complex values and or VipNDArray."
		    "The operator is given in the 'Operator' property(could be : '*', '+', '-', '/', '&', '|', '^').")
	Q_CLASSINFO("category", "Data Fusion/Numeric Operation")
	VIP_IO_DESCRIPTION(Operator, "Operation operator. Could be: : '*', '+', '-', '/', '&', '|', '^'.")
	VIP_PROPERTY_EDIT(Operator, "VipEnumEdit{ qproperty-enumNames:'+,-,*,/,&,|,^';  qproperty-value:'+' ;}")

public:
	VipNumericOperation(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return DisplayOnSameSupport; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<complex_d>() || v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>() ||
		       v.userType() == qMetaTypeId<VipNDArray>() || v.canConvert<double>();
	}

protected:
	virtual void mergeData(int data_type, int sub_data_type);

private:
	VipNDArray m_buffer;
};

VIP_REGISTER_QOBJECT_METATYPE(VipNumericOperation*)

/// Apply an affine transformation to an input array
class VIP_CORE_EXPORT VipAffineTransform : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Multiplication_factor)
	VIP_IO(VipProperty Offset)
	Q_CLASSINFO("description", "Apply an affine transformation to an input array/point vector")
	Q_CLASSINFO("category", "Numeric Operation")

public:
	VipAffineTransform(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.canConvert<VipNDArray>() || v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
	}

protected:
	virtual void apply();

private:
	VipNumericOperation m_op;
};

VIP_REGISTER_QOBJECT_METATYPE(VipAffineTransform*)

/// Apply an affine transformation to the time component of an input VipPointVector or VipComplexPointVector
class VIP_CORE_EXPORT VipAffineTimeTransform : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Multiplication_factor)
	VIP_IO(VipProperty Offset)
	Q_CLASSINFO("description", "Apply an affine transformation to the time component of an input point vector")
	Q_CLASSINFO("category", "Numeric Operation")

public:
	VipAffineTimeTransform(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
	}

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipAffineTimeTransform*)

/// Subtract the background to an input array.
/// The background is the first input array after a call to #VipProcessingObject::reset().
/// Therefore, the first output of this processing should be an array filled with only 0 values.
class VIP_CORE_EXPORT VipSubtractBackground : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty background)
	Q_CLASSINFO("description",
		    "Subtract the background to an input array.\n"
		    "The background is the first input array after reseting the processing.\n"
		    "Therefore, the first output of this processing should be an array filled with only 0 values.")
	Q_CLASSINFO("category", "Numeric Operation")

public:
	VipSubtractBackground(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.canConvert<VipNDArray>() || v.userType() == qMetaTypeId<VipPointVector>();
	}

protected:
	virtual void apply();
	virtual void resetProcessing();

private:
	VipNumericOperation m_op;
};

VIP_REGISTER_QOBJECT_METATYPE(VipSubtractBackground*)

/// Same as VipNumericOperation, but apply the operation between different players.
/// VipOperationBetweenPlayers is considered as an input transformation, and can be applied in a VipProcessingList.
/// However, its editor is defined in VipGui library.
class VIP_CORE_EXPORT VipOperationBetweenPlayers : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput Input)
	VIP_IO(VipOutput Output)
	VIP_IO(VipProperty Operator)
	VIP_IO(VipProperty OtherData)
	Q_CLASSINFO("description", "Apply an operation between 2 images from different players")
	Q_CLASSINFO("category", "Numeric Operation")

public:
	VipOperationBetweenPlayers(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		propertyName("Operator")->setData(QString("-"));
		VipOtherPlayerData data;
		data.setParentProcessing(this);
		propertyName("OtherData")->setData(data);
	}
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int, const QVariant& v) const { return v.canConvert<VipNDArray>(); }

	virtual QList<VipProcessingObject*> directSources() const;

protected:
	virtual void apply();
	virtual void resetProcessing();

private:
	VipNumericOperation m_op;
	VipAnyData m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipOperationBetweenPlayers*)

/// Returns the time difference between 2 consecutive samples of a VipPointVector.
/// This is used to display the evolution of the sampling time in a signal.
class VIP_CORE_EXPORT VipTimeDifference : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Time_unit) // time unit, either 'ns', 'us', 'ms' or 's'
	Q_CLASSINFO("description", "Returns the time difference between 2 consecutive samples of a signal")
	Q_CLASSINFO("category", "Numeric Operation")
	VIP_PROPERTY_EDIT(Time_unit, "VipEnumEdit{ qproperty-enumNames:'ns,us,ms,s';  qproperty-value:'ns' ;}")

public:
	VipTimeDifference(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
	}

protected:
	virtual void apply();

private:
};

VIP_REGISTER_QOBJECT_METATYPE(VipTimeDifference*)

/// Returns the derivative of a VipPointVector.
/// The derivative as less points than the input vector.
class VIP_CORE_EXPORT VipSignalDerivative : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("description", "Returns the derivative of a signal")
	Q_CLASSINFO("category", "Numeric Operation")

public:
	VipSignalDerivative(QObject* parent = nullptr);
	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int // index
				 ,
				 const QVariant& v) const
	{
		return v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
	}

protected:
	virtual void apply();

private:
};

VIP_REGISTER_QOBJECT_METATYPE(VipSignalDerivative*)

/// Returns the integral of a VipPointVector.
/// The integral as less points than the input vector.
// class VIP_CORE_EXPORT VipSignalIntegral : public VipProcessingObject
//  {
//  Q_OBJECT
//  VIP_IO(VipInput input)
//  VIP_IO(VipOutput output)
//  Q_CLASSINFO("description", "Returns the integral of a signal")
//  Q_CLASSINFO("category", "Numeric Operation")
//
//  public:
//  VipSignalIntegral(QObject * parent = nullptr);
//  virtual bool isInputTransformation() const { return true; }
//  virtual bool acceptInput(int , const QVariant & v) const {
//  return v.userType() == qMetaTypeId<VipPointVector>() ||
//	v.userType() == qMetaTypeId<VipComplexPointVector>();
//  }
//
//  protected:
//  virtual void apply();
//
//  private:
//  };
//
//  VIP_REGISTER_QOBJECT_METATYPE(VipSignalIntegral*)

#endif
