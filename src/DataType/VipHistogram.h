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

#ifndef VIP_HISTOGRAM_H
#define VIP_HISTOGRAM_H

#include "VipConfig.h"
#include "VipVectors.h"

/// \addtogroup DataType
/// @{

namespace Vip
{
	/// Define the bins repartition for the #vipHistogram function
	enum BinsStrategy
	{
		SameBinWidth, //! each bin has the same width
		SameBinHeight //! each bin tries to have the same number of values
	};
	/// Sorting strategy for arrays of std::complex types
	enum ComplexSorting
	{
		SortByArgument,	 //! sort complex array by argument
		SortByAmplitude, //! sort complex array by amplitude (or magnitude)
		SortByReal,	 //! sort complex array by argument
		SortByImag	 //! sort complex array by argument
	};
}

class VipNDArray;

/// Inplace sort input \a array of arithmetic or complex type.
/// For complex types, uses \a sort strategy.
///
/// Returns false on error:
/// - empty input array
/// - input array is not a contiguous unstrided vector
/// - data type is not arithmetic or complex.
VIP_DATA_TYPE_EXPORT bool vipSort(VipNDArray& array, Vip::ComplexSorting sort = Vip::SortByArgument);

/// Compute the histogram of input array \a ar.
///
/// \param ar input ND array of arithmetic type
/// \param output histogram
/// \param strategy bin strategy (either #Vip::SameBinWidth or #Vip::SameBinWidth)
/// \param inter only values within interval (or all values if interval is invalid) are used to compute the histogram
/// \param indexes new sorted indexes. Must be nullptr or a preallocated array of size ar.size().
/// \param index_offset start index  in \a indexes
/// \param replace_inf replace infinit values by the value at given index
/// \param replace_sup replace values outside valid interval by the value at given index
/// \param replace_nan replace NaN values by the value at given index
///
/// Returns true on success, false otherwise.
VIP_DATA_TYPE_EXPORT bool vipExtractHistogram(VipNDArray& ar,
					      VipIntervalSampleVector& out,
					      int bins = -1,
					      Vip::BinsStrategy strategy = Vip::SameBinWidth,
					      const VipInterval& inter = VipInterval(),
					      int* indexes = nullptr,
					      int index_offset = 0,
					      int replace_inf = -1,
					      int replace_sup = -1,
					      int replace_nan = -1,
					      int slop_factor = 0);

/// \internal internal use only
inline int vipFindUpperEqual(const VipIntervalSampleVector& hist, double pos)
{
	int index = 0;
	int n = hist.size();

	while (n > 0) {
		const int half = n >> 1;
		const int middle = index + half;

		if (hist[middle].interval.contains(pos))
			return middle;
		else if (hist[middle].interval.minValue() <= pos) {
			index = middle + 1;
			n -= half + 1;
		}
		else
			n = half;
	}

	return index;
}

/// @}
// end DataType

#endif