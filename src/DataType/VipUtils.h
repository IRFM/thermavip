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

#ifndef VIP_NDARRAY_UTILS_H
#define VIP_NDARRAY_UTILS_H

#include <QColor>
#include <QMetaType>
#include <QString>
#include <qdatastream.h>
#include <qtextstream.h>

#include "VipHybridVector.h"
#include "VipInterval.h"
#include "VipRgb.h"
#include "VipVectors.h"

/// \addtogroup DataType
/// @{

#if defined(__GNUC__) && (__GNUC__ < 5)

namespace std
{
	// Fix for gcc under 5.0.0
	// see https://stackoverflow.com/questions/35753920/why-does-the-void-t-detection-idiom-not-work-with-gcc-4-9
	namespace void_details
	{
		template<class...>
		struct make_void
		{
			using type = void;
		};
	}

	template<class... T>
	using void_t = typename void_details::make_void<T...>::type;
}

#endif

typedef VipHybridVector<double, -1> VipNDDoubleCoordinate;
Q_DECLARE_METATYPE(VipNDArrayShape)
Q_DECLARE_METATYPE(VipNDDoubleCoordinate)

// List of rectangles, mainly used for quantiles bounding box (see VipSceneModel.h)
typedef QList<QRect> VipRectList;
typedef QList<QRectF> VipRectFList;
typedef QPair<qint64, VipRectList> VipTimestampedRectList;
typedef QPair<qint64, VipRectFList> VipTimestampedRectFList;
typedef QVector<VipTimestampedRectList> VipTimestampedRectListVector;
typedef QVector<VipTimestampedRectFList> VipTimestampedRectFListVector;
Q_DECLARE_METATYPE(VipRectList)
Q_DECLARE_METATYPE(VipRectFList)
Q_DECLARE_METATYPE(VipTimestampedRectList)
Q_DECLARE_METATYPE(VipTimestampedRectFList)
Q_DECLARE_METATYPE(VipTimestampedRectListVector)
Q_DECLARE_METATYPE(VipTimestampedRectFListVector)
class VipNDArray;

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const complex_f& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, complex_f& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const complex_d& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, complex_d& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipComplexPoint& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipComplexPoint& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipIntervalSample& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipIntervalSample& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipInterval& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipInterval& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipRGB& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipRGB& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipLongPoint& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipLongPoint& c);

#if VIP_USE_LONG_DOUBLE == 0
VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipPoint& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipPoint& c);
#endif

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipNDArray& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipNDArray& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipPointVector& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipPointVector& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipComplexPointVector& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipComplexPointVector& c);

VIP_DATA_TYPE_EXPORT QDataStream& operator<<(QDataStream& s, const VipIntervalSampleVector& c);
VIP_DATA_TYPE_EXPORT QDataStream& operator>>(QDataStream& s, VipIntervalSampleVector& c);

class QTextStream;
VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const complex_f& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, complex_f& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const complex_d& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, complex_d& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const QColor& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, QColor& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipIntervalSample& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipIntervalSample& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const QPointF& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, QPointF& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipLongPoint& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipLongPoint& c);

#if VIP_USE_LONG_DOUBLE == 0
VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipPoint& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipPoint& c);
#endif

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipComplexPoint& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipComplexPoint& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipInterval& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipInterval& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& str, const VipRGB& v);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& str, VipRGB& v);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipIntervalSampleVector& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipIntervalSampleVector& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipPointVector& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipPointVector& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipComplexPointVector& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipComplexPointVector& c);

VIP_DATA_TYPE_EXPORT QTextStream& operator<<(QTextStream& s, const VipNDArray& c);
VIP_DATA_TYPE_EXPORT QTextStream& operator>>(QTextStream& s, VipNDArray& c);

/// @brief Extract X values of a VipPointVector as a 1d VipNDArray
/// @param samples input VipPointVector
/// @return 1d VipNDArray of type vip_double
VIP_DATA_TYPE_EXPORT VipNDArray vipExtractXValues(const VipPointVector& samples);

/// @brief Extract Y values of a VipPointVector as a 1d VipNDArray
/// @param samples input VipPointVector
/// @return 1d VipNDArray of type vip_double
VIP_DATA_TYPE_EXPORT VipNDArray vipExtractYValues(const VipPointVector& samples);

/// @brief Extract X values of a VipComplexPointVector as a 1d VipNDArray
/// @param samples input VipComplexPointVector
/// @return 1d VipNDArray of type vip_double
VIP_DATA_TYPE_EXPORT VipNDArray vipExtractXValues(const VipComplexPointVector& samples);

/// @brief Extract Y values of a VipComplexPointVector as a 1d VipNDArray
/// @param samples input VipComplexPointVector
/// @return 1d VipNDArray of type complex_d
VIP_DATA_TYPE_EXPORT VipNDArray vipExtractYValues(const VipComplexPointVector& samples);

/// @brief Create a VipPointVector from X and Y arrays
/// @param x input X coordinates
/// @param y input Y coordinates
/// @return VipPointVector object
VIP_DATA_TYPE_EXPORT VipPointVector vipCreatePointVector(const VipNDArray& x, const VipNDArray& y);

/// @brief Create a VipComplexPointVector from X and Y arrays
/// @param x input X coordinates casted to vip_double
/// @param y input Y coordinates casted to complex_d
/// @return VipPointVector object
VIP_DATA_TYPE_EXPORT VipComplexPointVector vipCreateComplexPointVector(const VipNDArray& x, const VipNDArray& y);

/// @brief Set the Y values of a VipPointVector using a 1d VipNDArray
/// @param samples in/out VipPointVector
/// @param y input y array
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipSetYValues(VipPointVector& samples, const VipNDArray& y);

/// @brief Set the Y values of a VipComplexPointVector using a 1d VipNDArray
/// @param samples in/out VipComplexPointVector
/// @param y input y array casted to complex_d
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipSetYValues(VipComplexPointVector& samples, const VipNDArray& y);

/// @brief Convert a VipPointVector object to VipComplexPointVector
/// @param samples input VipPointVector
/// @return VipComplexPointVector object
VIP_DATA_TYPE_EXPORT VipComplexPointVector vipToComplexPointVector(const VipPointVector& samples);

enum ResampleStrategy
{
	ResampleUnion = 0,
	ResampleIntersection = 0x01,
	ResamplePadd0 = 0x02,
	ResampleInterpolation = 0x04
};
Q_DECLARE_FLAGS(ResampleStrategies, ResampleStrategy);
Q_DECLARE_OPERATORS_FOR_FLAGS(ResampleStrategies)

/// @brief Resample both a and b VipPointVector based on their timestamps (x coordinate).
/// For both vectors, this will create missing x values in order for both vectors to have the same x coordinates.
/// Both input vectors must have sorted x coordinates.
/// @param a first vector
/// @param b second vector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd_a padding Y value when using ResampleUnion for vector a
/// @param padd_b padding Y value when using ResampleUnion for vector b
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(VipPointVector& a, VipPointVector& b, ResampleStrategies s = ResampleIntersection | ResampleInterpolation, vip_double padd_a = 0, vip_double padd_b = 0);

/// @brief Resample both a and b VipComplexPointVector based on their timestamps (x coordinate).
/// For both vectors, this will create missing x values in order for both vectors to have the same x coordinates.
/// Both input vectors must have sorted x coordinates.
/// @param a first vector
/// @param b second vector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd_a padding Y value when using ResampleUnion for vector a
/// @param padd_b padding Y value when using ResampleUnion for vector b
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(VipComplexPointVector& a,
					     VipComplexPointVector& b,
					     ResampleStrategies s = ResampleIntersection | ResampleInterpolation,
					     complex_d padd_a = 0,
					     complex_d padd_b = 0);

/// @brief Resample both a and b vectors based on their timestamps (x coordinate).
/// For both vectors, this will create missing x values in order for both vectors to have the same x coordinates.
/// Both input vectors must have sorted x coordinates.
/// @param a first vector
/// @param b second vector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd_a padding Y value when using ResampleUnion for vector a
/// @param padd_b padding Y value when using ResampleUnion for vector b
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(VipPointVector& a,
					     VipComplexPointVector& b,
					     ResampleStrategies s = ResampleIntersection | ResampleInterpolation,
					     vip_double padd_a = 0,
					     complex_d padd_b = 0);

/// @brief Resample each VipPointVector in lst based on their timestamps (x coordinate).
/// @param lst input list of VipPointVector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd padding Y value when using ResampleUnion for all vectors
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(QList<VipPointVector>& lst, ResampleStrategies s = ResampleIntersection | ResampleInterpolation, vip_double padd = 0);

/// @brief Resample each VipComplexPointVector in lst based on their timestamps (x coordinate).
/// @param lst input list of VipComplexPointVector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd padding Y value when using ResampleUnion for all vectors
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(QList<VipComplexPointVector>& lst, ResampleStrategies s = ResampleIntersection | ResampleInterpolation, complex_d padd = 0);

/// @brief Resample each VipPointVector and VipComplexPointVector objects in lst_a and lst_b based on their timestamps (x coordinate).
/// @param lst_a input list of VipPointVector
/// @param lst_b input list of VipComplexPointVector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd_a padding Y value when using ResampleUnion for all VipPointVector vectors
/// @param padd_b padding Y value when using ResampleUnion for all VipComplexPointVector vectors
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(QList<VipPointVector>& lst_a,
					     QList<VipComplexPointVector>& lst_b,
					     ResampleStrategies s = ResampleIntersection | ResampleInterpolation,
					     vip_double padd_a = 0,
					     complex_d padd_b = 0);

/// @brief Resample each VipPointVector in lst based on their timestamps (x coordinate) in order for them to share the same X values with a constant step of x_step.
/// @param lst input list of VipPointVector
/// @param x_step x step
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd padding Y value when using ResampleUnion for all VipPointVector vectors
/// @return true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipResampleVectors(QList<VipPointVector>& lst, vip_double x_step, ResampleStrategies s = ResampleIntersection | ResampleInterpolation, vip_double padd = 0);

/// @brief Resample all VipPointVector in vectors based on their timestamps (x coordinate) and store the result in a VipNDArray of type double.
/// The output array will contain the x coordinates as its first column. The other columns will contrain the Y values of each input VipPointVector object.
/// @param vectors input list of VipPointVector
/// @param s Resample strategy, combination of ResampleUnion, ResampleIntersection, ResamplePadd0 (used with ResampleUnion), ResampleInterpolation
/// @param padd padding Y value when using ResampleUnion for all VipPointVector vectors
/// @return VipNDArray object
VIP_DATA_TYPE_EXPORT VipNDArray vipResampleVectorsAsNDArray(const QList<VipPointVector>& vectors, ResampleStrategies s = ResampleIntersection | ResampleInterpolation, vip_double padd = 0);

/// @}
// end DataType
#endif
