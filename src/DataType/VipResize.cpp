#include "VipResize.h"
#include "VipNDArray.h"



namespace detail
{

	//for complex types, we are missing a few operators, so define them
	inline complex_f operator*(double v, const complex_f & c) { return complex_f(c.real()*v, c.imag()*v); }
	inline complex_d operator*(float v, const complex_d & c) { return complex_d(c.real()*v, c.imag()*v); }
	inline complex_f operator*(const complex_f & c, double v) { return complex_f(c.real()*v, c.imag()*v); }
	inline complex_d operator*(const complex_d & c, float v) { return complex_d(c.real()*v, c.imag()*v); }

	template<class Src, class Dst >
	void vipResizeArrayView(const Src& src, Dst & dst, Vip::InterpolationType type)
	{
		if (type == Vip::NoInterpolation)
			vipResizeNoInterpolation(src, dst);
		else if (type == Vip::LinearInterpolation)
			vipResizeLinear(src, dst);
		else if (type == Vip::CubicInterpolation)
			vipResizeSpline(src, dst);
	}

	template< class T>
	bool vipResizeArray(const void * dataPointer, const VipNDArrayShape & shape, const VipNDArrayShape & src_strides, VipNDArrayHandle * dst, Vip::InterpolationType type)
	{
		VipNDArrayTypeView< T> input(const_cast<T*>(static_cast<const T*>(dataPointer)), shape, src_strides);
		int other_type = dst->dataType();

		if (other_type == QMetaType::Bool) {
			VipNDArrayTypeView<bool> output(static_cast<bool*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::Char) {
			VipNDArrayTypeView<char> output(static_cast<char*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::SChar) {
			VipNDArrayTypeView<qint8> output(static_cast<qint8*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::UChar) {
			VipNDArrayTypeView<quint8> output(static_cast<quint8*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::Short) {
			VipNDArrayTypeView<qint16> output(static_cast<qint16*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::UShort) {
			VipNDArrayTypeView<quint16> output(static_cast<quint16*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::Int) {
			VipNDArrayTypeView<qint32> output(static_cast<qint32*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::UInt) {
			VipNDArrayTypeView<quint32> output(static_cast<quint32*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::Long) {
			VipNDArrayTypeView<long> output(static_cast<long*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::ULong) {
			VipNDArrayTypeView<unsigned long> output(static_cast<unsigned long*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::LongLong) {
			VipNDArrayTypeView<qint64> output(static_cast<qint64*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::ULongLong) {
			VipNDArrayTypeView<quint64> output(static_cast<quint64*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::Float) {
			VipNDArrayTypeView<float> output(static_cast<float*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == QMetaType::Double) {
			VipNDArrayTypeView<double> output(static_cast<double*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == qMetaTypeId<long double>()) {
			VipNDArrayTypeView<long double> output(static_cast<long double*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == qMetaTypeId<complex_f>()) {
			VipNDArrayTypeView<complex_f> output(static_cast<complex_f*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == qMetaTypeId<complex_d>()) {
			VipNDArrayTypeView<complex_d> output(static_cast<complex_d*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		return false;
	}



	template<>
	bool vipResizeArray<complex_f>(const void * dataPointer, const VipNDArrayShape & shape, const VipNDArrayShape & src_strides, VipNDArrayHandle * dst, Vip::InterpolationType type)
	{
		VipNDArrayTypeView<complex_f> input(const_cast<complex_f*>(static_cast<const complex_f*>(dataPointer)), shape, src_strides);
		int other_type = dst->dataType();

		if (other_type == qMetaTypeId<complex_f>()) {
			VipNDArrayTypeView<complex_f> output(static_cast<complex_f*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == qMetaTypeId<complex_d>()) {
			VipNDArrayTypeView<complex_d> output(static_cast<complex_d*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		return false;
	}

	template<>
	bool vipResizeArray<complex_d>(const void * dataPointer, const VipNDArrayShape & shape, const VipNDArrayShape & src_strides, VipNDArrayHandle * dst, Vip::InterpolationType type)
	{
		VipNDArrayTypeView<complex_d> input(const_cast<complex_d*>(static_cast<const complex_d*>(dataPointer)), shape, src_strides);
		int other_type = dst->dataType();

		if (other_type == qMetaTypeId<complex_f>()) {
			VipNDArrayTypeView<complex_f> output(static_cast<complex_f*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		else if (other_type == qMetaTypeId<complex_d>()) {
			VipNDArrayTypeView<complex_d> output(static_cast<complex_d*>(dst->opaque), dst->shape, dst->strides);
			vipResizeArrayView(input, output, type); return true;
		}
		return false;
	}

	bool vipResizeArray(const VipNDArrayHandle * src, VipNDArrayHandle * dst, Vip::InterpolationType type)
	{
		int this_type = src->dataType();
		int other_type = dst->dataType();
		if ((vipIsArithmetic(this_type) || vipIsComplex(this_type)) && (vipIsArithmetic(other_type) || vipIsComplex(other_type)))
		{
			VipNDArray srcar, dstar;
			VipNDArrayHandle *_src = const_cast<VipNDArrayHandle*>(src), *_dst = dst;
			//bool src_unstrided = true;
			// bool dst_unstrided = true;
			// vipShapeToSize(src->shape, src->strides, &src_unstrided);
			// vipShapeToSize(dst->shape, dst->strides, &dst_unstrided);

			//if (type != Vip::NoInterpolation || src->shape.size() != 2)
			// {
			// //The algorithm works on unstrided arrays, so copy into intermediate arrays if needed.
			// //If NoInterpolation is selected, the algorithm works on strided 2D data (minor optimization for used display mainly).
			// if (!src_unstrided)
			// {
			// srcar = VipNDArray(src->dataType(), src->shape);
			// src->exportData(VipNDArrayShape(src->shape.size(), 0), src->shape, const_cast<VipNDArrayHandle*>(srcar.sharedHandle().data()), VipNDArrayShape(src->shape.size(), 0), srcar.shape());
			// _src = const_cast<VipNDArrayHandle*>(srcar.sharedHandle().data());
			// }
			// if (!dst_unstrided)
			// {
			// dstar = VipNDArray(dst->dataType(), dst->shape);
			// dst->exportData(VipNDArrayShape(dst->shape.size(), 0), dst->shape, const_cast<VipNDArrayHandle*>(dstar.sharedHandle().data()), VipNDArrayShape(dst->shape.size(), 0), dstar.shape());
			// _dst = const_cast<VipNDArrayHandle*>(dstar.sharedHandle().data());
			// }
			// }


			bool res;
			if (this_type == QMetaType::Bool) res = vipResizeArray<bool>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::Char)  res = vipResizeArray<char>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::SChar)  res = vipResizeArray<qint8>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::UChar)  res = vipResizeArray<quint8>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::Short)  res = vipResizeArray<qint16>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::UShort)  res = vipResizeArray<quint16>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::Int)  res = vipResizeArray<qint32>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::UInt)  res = vipResizeArray<quint32>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::Long)  res = vipResizeArray<long>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::ULong)  res = vipResizeArray<unsigned long>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::LongLong)  res = vipResizeArray<qint64>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::ULongLong)  res = vipResizeArray<quint64>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::Float)  res = vipResizeArray<float>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == QMetaType::Double)  res = vipResizeArray<double>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == qMetaTypeId<long double>())  res = vipResizeArray<long double>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == qMetaTypeId<complex_f>())  res = vipResizeArray<complex_f>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else if (this_type == qMetaTypeId<complex_d>())  res = vipResizeArray<complex_d>(_src->opaque, _src->shape, _src->strides, _dst, type);
			else return false;

			if (!res)
				return false;

			//if (type != Vip::NoInterpolation || src->shape.size() != 2)
			// {
			// if (!dst_unstrided)
			// {
			// //copy back from dstar to dst
			// dstar.sharedHandle()->exportData(VipNDArrayShape(dstar.shapeCount(), 0), dstar.shape(), dst, VipNDArrayShape(dst->shape.size(), 0), dst->shape);
			// }
			// }
			return true;
		}
		return false;
	}



}
