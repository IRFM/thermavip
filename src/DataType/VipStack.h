#pragma once

#include "VipEval.h"

/// @brief Stack a1 and a2 arrays along axis, and store the result in dst
/// Returns true on success, false otherwise
template<class A1, class A2, class Dst>
bool vipStack(Dst& dst, const A1& a1, const A2& a2, int axis)
{
	const VipNDArrayShape sh1 = a1.shape();
	const VipNDArrayShape sh2 = a2.shape();

	// test same dim count
	if (sh1.size() != sh2.size())
		return false;
	// test same shape except for given axis
	for (int i = 0; i < sh1.size(); ++i) {
		if (i != axis && (sh1[i] != sh2[i] || sh1[i] != dst.shape(i)))
			return false;
	}

	// start position of each array
	VipNDArrayShape pos1, pos2;
	pos1.resize(sh1.size());
	pos1.fill(0);
	pos2.resize(sh1.size());
	pos2.fill(0);
	pos2[axis] = sh1[axis];

	typename Dst::view_type view1 = dst.mid(pos1, sh1);
	typename Dst::view_type view2 = dst.mid(pos2, sh2);
	if (!vipEval(view1, a1))
		return false;
	if (!vipEval(view2, a2))
		return false;
	return true;
}

/// @brief @brief Stack a1 and a2 arrays along axis, and return the result or an empty VipNDArray on error.
template<class A1, class A2>
VipNDArray vipStack(const A1& a1, const A2& a2, int axis)
{
	VipNDArrayShape sh = a1.shape();
	sh[axis] = a1.shape()[axis] + a2.shape()[axis];

	int t1 = a1.dataType();
	int t2 = a2.dataType();
	int type = t1 != t2 ? vipHigherArrayType(a1.dataType(), a2.dataType()) : t1;
	VipNDArray res = VipNDArray(type, sh);
	if (!vipStack(res, a1, a2, axis))
		res.clear();
	return res;
}

/// @brief Stack v1 and v2 arrays along axis, and store the result in dst
/// Returns true on success, false otherwise
VIP_DATA_TYPE_EXPORT bool vipStack(VipNDArray& dst, const VipNDArray& v1, const VipNDArray& v2, int axis);
/// @brief @brief Stack v1 and v2 arrays along axis, and return the result or an empty VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray vipStack(const VipNDArray& v1, const VipNDArray& v2, int axis);
