#include <VipStack.h>


bool vipStack(VipNDArray & dst, const VipNDArray & v1, const VipNDArray & v2, int axis)
{
	//test same dim count
	if (v1.shapeCount() != v2.shapeCount())
		return false;
	//test same shape except for given axis
	for (int i = 0; i < v1.shapeCount(); ++i) {
		if (i != axis && (v1.shape(i) != v2.shape(i) || v1.shape(i) != v2.shape(i)))
			return false;
	}

	//start position of each array
	VipNDArrayShape pos1, pos2;
	pos1.resize(v1.shapeCount());
	pos1.fill(0);
	pos2.resize(v1.shapeCount());
	pos2.fill(0);
	pos2[axis] = v1.shape(axis);

	VipNDArray view1 = dst.mid(pos1, v1.shape());
	VipNDArray view2 = dst.mid(pos2, v2.shape());
	if (!v1.convert(view1))
		return false;
	if (!v2.convert(view2))
		return false;
	return true;
}

#include <qimage.h>
VipNDArray vipStack(const VipNDArray & v1, const VipNDArray & v2, int axis)
{
	VipNDArrayShape sh = v1.shape();
	sh[axis] = v1.shape()[axis] + v2.shape()[axis];

	int t1 = v1.dataType();
	int t2 = v2.dataType();
	if (t1 == qMetaTypeId<QImage>() && t2 == qMetaTypeId<QImage>()) {
		VipNDArray res = VipNDArray(vipCreateArrayHandle(VipNDArrayHandle::Image, qMetaTypeId<QImage>(), sh));
		if (!vipStack(res, v1, v2, axis))
			res.clear();
		return res;
	}
	int type = t1 != t2 ? vipHigherArrayType(v1.dataType(), v2.dataType()) : t1;
	VipNDArray res = VipNDArray(type, sh);
	if (!vipStack(res, v1, v2, axis))
		res.clear();
	return res;
}
