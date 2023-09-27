#pragma once

#include <cmath>

/// Very simple 2*2 matrix class only providing matrix inversion
struct VipMatrix22
{
	double m11, m12, m21, m22;
	VipMatrix22(double _m11 = 1, double _m12 = 0, double _m21 = 0, double _m22 = 1)
		:m11(_m11), m12(_m12), m21(_m21), m22(_m22)
	{}
	bool isInvertible() const { return !(std::abs(m11 * m22 - m12 * m21) <= 0.000000000001); }
	double determinant() const { return m11 * m22 - m12 * m21; }
	VipMatrix22 inverted(bool* invertible) const
	{
		double dtr = determinant();
		if (dtr == 0.0) {
			if (invertible)
				*invertible = false;                // singular matrix
			return VipMatrix22();
		}
		else {                                        // invertible matrix
			if (invertible)
				*invertible = true;
			double dinv = 1.0 / dtr;
			return VipMatrix22((m22 * dinv), (-m12 * dinv),
				(-m21 * dinv), (m11 * dinv));
		}
	}
};
