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

#include "VipScaleMap.h"
#include "VipScaleEngine.h"

/// \brief Constructor
///
/// The scale and paint device intervals are both set to [0,1].
VipScaleMap::VipScaleMap()
  : d_s1(0.0)
  , d_s2(1.0)
  , d_p1(0.0)
  , d_p2(1.0)
  , d_cnv(1.0)
  , d_abs_cnv(1.0)
  , d_ts1(0.0)
  , d_transform(nullptr)
{
}

//! Copy constructor
VipScaleMap::VipScaleMap(const VipScaleMap& other)
  : d_s1(other.d_s1)
  , d_s2(other.d_s2)
  , d_p1(other.d_p1)
  , d_p2(other.d_p2)
  , d_cnv(other.d_cnv)
  , d_abs_cnv(other.d_abs_cnv)
  , d_ts1(other.d_ts1)
  , d_transform(nullptr)
{
	if (other.d_transform)
		d_transform = other.d_transform->copy();
}

/// Destructor
VipScaleMap::~VipScaleMap()
{
	if (d_transform)
		delete d_transform;
}

//! Assignment operator
VipScaleMap& VipScaleMap::operator=(const VipScaleMap& other)
{
	d_s1 = other.d_s1;
	d_s2 = other.d_s2;
	d_p1 = other.d_p1;
	d_p2 = other.d_p2;
	d_cnv = other.d_cnv;
	d_abs_cnv = other.d_abs_cnv;
	d_ts1 = other.d_ts1;

	if (d_transform)
		delete d_transform;
	d_transform = nullptr;

	if (other.d_transform)
		d_transform = other.d_transform->copy();

	return *this;
}
