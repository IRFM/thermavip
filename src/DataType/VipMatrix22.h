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

#ifndef VIP_MATRIX_22_H
#define VIP_MATRIX_22_H

#include <cmath>

/// \addtogroup DataType
/// @{

/// Very simple 2*2 matrix class only providing matrix inversion
struct VipMatrix22
{
	double m11, m12, m21, m22;
	VipMatrix22(double _m11 = 1, double _m12 = 0, double _m21 = 0, double _m22 = 1)
	  : m11(_m11)
	  , m12(_m12)
	  , m21(_m21)
	  , m22(_m22)
	{
	}
	bool isInvertible() const { return !(std::abs(m11 * m22 - m12 * m21) <= 0.000000000001); }
	double determinant() const { return m11 * m22 - m12 * m21; }
	VipMatrix22 inverted(bool* invertible) const
	{
		double dtr = determinant();
		if (dtr == 0.0) {
			if (invertible)
				*invertible = false; // singular matrix
			return VipMatrix22();
		}
		else { // invertible matrix
			if (invertible)
				*invertible = true;
			double dinv = 1.0 / dtr;
			return VipMatrix22((m22 * dinv), (-m12 * dinv), (-m21 * dinv), (m11 * dinv));
		}
	}
};

/// @}
// end DataType

#endif