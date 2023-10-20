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

#include "VipHistogram.h"
#include "VipDataType.h"

template<class T>
struct sort_by_arg
{
	VIP_ALWAYS_INLINE bool operator()(const T& a, const T& b) const { return std::arg(a) < std::arg(b); }
};
template<class T>
struct sort_by_amplitude
{
	VIP_ALWAYS_INLINE bool operator()(const T& a, const T& b) const { return std::norm(a) < std::norm(b); }
};
template<class T>
struct sort_by_real
{
	VIP_ALWAYS_INLINE bool operator()(const T& a, const T& b) const { return std::real(a) < std::real(b); }
};
template<class T>
struct sort_by_imag
{
	VIP_ALWAYS_INLINE bool operator()(const T& a, const T& b) const { return std::imag(a) < std::imag(b); }
};

template<class T>
bool isNan(T)
{
	return false;
}
template<>
VIP_ALWAYS_INLINE bool isNan(float value)
{
	return value != value;
}
template<>
VIP_ALWAYS_INLINE bool isNan(double value)
{
	return value != value;
}

template<class T>
struct sort_pair
{
	VIP_ALWAYS_INLINE bool operator()(const std::pair<T, int>& a, const std::pair<T, int>& b) const { return a.first < b.first; }
};
template<>
struct sort_pair<float>
{
	VIP_ALWAYS_INLINE bool operator()(const std::pair<float, int>& a, const std::pair<float, int>& b) const
	{
		if (isNan(a.first))
			return true;
		else if (isNan(b.first))
			return false;
		else
			return a.first < b.first;
	}
};
template<>
struct sort_pair<double>
{
	VIP_ALWAYS_INLINE bool operator()(const std::pair<double, int>& a, const std::pair<double, int>& b) const
	{
		if (isNan(a.first))
			return true;
		else if (isNan(b.first))
			return false;
		else
			return a.first < b.first;
	}
};

template<class T>
struct sort_std
{
	VIP_ALWAYS_INLINE bool operator()(const T& a, const T& b) const { return a < b; }
};
template<>
struct sort_std<float>
{
	VIP_ALWAYS_INLINE bool operator()(const float a, const float b) const
	{
		if (isNan(a))
			return true;
		else if (isNan(b))
			return false;
		else
			return a < b;
	}
};
template<>
struct sort_std<double>
{
	VIP_ALWAYS_INLINE bool operator()(const double a, const double b) const
	{
		if (isNan(a))
			return true;
		else if (isNan(b))
			return false;
		else
			return a < b;
	}
};

static void expandSampleWidth(VipIntervalSampleVector& out, double max_width)
{
	if (out.size() == 0)
		return;
	if (out.size() == 1) {
		out[0].interval.setMinValue(out[0].interval.minValue() - max_width / 2);
		out[0].interval.setMaxValue(out[0].interval.maxValue() + max_width / 2);
	}
	else {
		// get the min dist between samples
		double min_width = out[1].interval.minValue() - out[0].interval.maxValue();
		for (int i = 2; i < out.size(); ++i) {
			min_width = qMin(min_width, (double)(out[i].interval.minValue() - out[i - 1].interval.maxValue()));
		}

		max_width = qMin(max_width, min_width);
		for (int i = 0; i < out.size(); ++i) {
			out[i].interval.setMinValue(out[i].interval.minValue() - max_width / 2);
			out[i].interval.setMaxValue(out[i].interval.maxValue() + max_width / 2);
		}
	}
}

#include <unordered_map>
template<class T>
VipIntervalSampleVector extractHistogram(const T* begin, const T* end, int bins, Vip::BinsStrategy strategy, const VipInterval& inter)
{
	// sort
	// sorting with unordered_map and map is faster than std::sort
	std::unordered_map<T, int> unordered;
	std::map<T, int> ordered;
	int tot_count = 0;
	while (begin < end) {
		if (!isNan(*begin) && (!inter.isValid() || inter.contains(*begin))) {
			auto it = unordered.find(*begin);
			if (it != unordered.end())
				it->second++;
			else
				unordered.insert(std::pair<T, int>(*begin, 1));
		}
		++begin;
	}

	for (typename std::unordered_map<T, int>::const_iterator it = unordered.begin(); it != unordered.end(); ++it)
		ordered.insert(*it);
	VipIntervalSampleVector hist((int)ordered.size());
	int i = 0;
	for (typename std::map<T, int>::const_iterator it = ordered.begin(); it != ordered.end(); ++it, ++i) {
		hist[i] = VipIntervalSample(it->second, VipInterval(it->first, it->first));
		tot_count += hist[i].value;
	}

	// compute bins
	if (bins <= 0 || hist.size() <= bins) {
		// for (int i = 0; i < hist.size(); ++i)
		//  vip_debug("%f %f, %f\n", hist[i].interval.minValue(), hist[i].interval.maxValue(), hist[i].value);
		//  vip_debug(" \n");
		if (strategy == Vip::SameBinWidth)
			expandSampleWidth(hist, 1);
		return hist;
	}

	if (strategy == Vip::SameBinWidth) {
		T _min = hist.first().interval.minValue();
		T _max = hist.last().interval.minValue();

		// initialize histogram
		double width = (_max - _min) / (double)bins;
		double start = _min;
		VipIntervalSampleVector res(bins);
		for (int j = 0; j < res.size(); ++j, start += width)
			res[j] = VipIntervalSample(0, VipInterval(start, start + width, VipInterval::ExcludeMaximum));

		for (VipIntervalSampleVector::const_iterator it = hist.cbegin(); it != hist.cend(); ++it) {
			int index = std::floor((it->interval.minValue() - _min) / width);
			if (index == res.size())
				--index;
			res[index].value += it->value;
		}

		return res;
	}
	else {
		double bin_height = tot_count / (double)bins;
		double next_step = bin_height;
		double tot_values = 0;
		VipIntervalSample start = hist.first();
		tot_values = start.value;
		VipIntervalSampleVector res;
		VipIntervalSampleVector::const_iterator it = hist.cbegin();
		++it;
		int j = 1;
		for (; it != hist.cend(); ++it, ++j) {
			if (tot_values >= next_step) {
				res.append(start);
				start = *it;
				next_step += bin_height;
				tot_values += it->value;
			}
			else {
				start.interval.setMaxValue(it->interval.maxValue());
				start.value += it->value;
				tot_values += it->value;
			}
		}
		res.append(start);

		return res;
	}
}

#ifdef VIP_HAS_CPP_14

#include "ska_sort.hpp"

template<class T>
struct ExtractKey
{
	VIP_ALWAYS_INLINE typename std::make_unsigned<T>::type operator()(const std::pair<T, int>& v) const
	{
		// integral types
		using integral_type = typename std::make_unsigned<T>::type;
		integral_type val = static_cast<integral_type>(v.first);
		if (std::is_signed<T>::value)
			// wrap around to keep an unsigned value in the same order as the signed one
			val += (static_cast<integral_type>(1) << (static_cast<integral_type>(sizeof(T) * 8 - 1)));
		return (val);
	}
};

template<>
struct ExtractKey<bool>
{
	VIP_ALWAYS_INLINE std::uint8_t operator()(const std::pair<bool, int>& v) const { return v.first; }
};

template<>
struct ExtractKey<float>
{
	VIP_ALWAYS_INLINE std::uint32_t operator()(const std::pair<float, int>& v) const
	{
		union U
		{
			float d;
			std::uint32_t u;
		};
		U u = { v.first };
		// Interpret float as 32-bit unsigned.
		// Flip all except top if top bit is set.
		u.u ^= ((static_cast<unsigned>((static_cast<int>(u.u)) >> 31)) >> 1);
		// Flip top bit.
		u.u ^= (1u << 31u);
		return (u.u);
	}
};
template<>
struct ExtractKey<double>
{
	VIP_ALWAYS_INLINE std::uint64_t operator()(const std::pair<double, int>& v) const
	{
		union U
		{
			double d;
			std::uint64_t u;
		};
		U u = { v.first };
		// Interpret double as 64-bit unsigned.
		// Flip all except top if top bit is set.
		u.u ^= ((static_cast<std::uint64_t>((static_cast<std::int64_t>(u.u)) >> 63ll)) >> 1ll);
		// Flip top bit.
		u.u ^= (1ull << 63ull);
		return (u.u);
	}
};
template<>
struct ExtractKey<long double>
{
	VIP_ALWAYS_INLINE std::uint64_t operator()(const std::pair<long double, int>& v) const { return ExtractKey<double>{}(std::pair<double, int>{ v.first, 0 }); }
};

template<class T, bool Numeric = std::is_arithmetic<T>::value>
struct SortVector
{
	static void apply(std::vector<std::pair<T, int>>& values) { std::sort(values.begin(), values.end(), sort_pair<T>()); }
};
template<class T>
struct SortVector<T, true>
{
	static void apply(std::vector<std::pair<T, int>>& values) { ska_sort(values.begin(), values.end(), ExtractKey<T>()); }
};

#else

template<class T, bool Numeric = std::is_arithmetic<T>::value>
struct SortVector
{
	static void apply(std::vector<std::pair<T, int>>& values) { std::sort(values.begin(), values.end(), sort_pair<T>()); }
};

#endif

template<class T>
VipIntervalSampleVector extractHistogram(const T* begin,
					 const T* end,
					 int bins,
					 Vip::BinsStrategy strategy,
					 VipInterval inter,
					 int* indexes,
					 int index_offset = 0,
					 int replace_inf = -1,
					 int replace_sup = -1,
					 int replace_nan = -1,
					 int slop_factor = 0)
{
	int size = end - begin;

	static thread_local std::vector<std::pair<T, int>> values;
	if (values.size() != size)
		values.resize(size);

	// extract index: we must sort a pair of value -> index
	bool has_nan = false;

	for (int i = 0; i < size; ++i) {
		if (!isNan(begin[i])) {
			values[i].first = (begin[i]);
			values[i].second = i;
		}
		else {
			has_nan = true;
			// ignore nan values
		}
	}

	int tot_count = 0;
	VipIntervalSampleVector hist;

	// sort
	if (!has_nan)
		SortVector<T>::apply(values);
	else
		std::sort(values.begin(), values.end(), sort_pair<T>());

	const std::pair<T, int>* beg = values.data();
	const std::pair<T, int>* en = values.data() + values.size();

	// ignore nan values
	if (has_nan) {
		while (beg < en && isNan(*beg)) {
			indexes[beg->second] = replace_nan;
			++beg;
		}
		if (beg == en)
			return VipIntervalSampleVector();
	}

	if (inter.isValid()) {
		// ignore values outside given interval
		while (beg < en && !inter.contains(beg->first)) {
			indexes[beg->second] = replace_inf;
			++beg;
		}
		if (beg != en) {
			while (en > beg && !inter.contains((en - 1)->first)) {
				indexes[(en - 1)->second] = replace_sup;
				--en;
			}
		}
	}
	if (beg == en)
		return VipIntervalSampleVector();

	// build first histogram
	hist.reserve(size / 100);

	VipIntervalSample first;
	first.value = 1;
	first.interval.setMinValue(beg->first);
	first.interval.setMaxValue(beg->first);
	hist.append(first);

	indexes[beg->second] = index_offset;
	++beg;

	VipIntervalSample* current = &hist.last();

	// compute histogram
	tot_count = 1;
	for (;;) {
		while (beg < en && beg->first == (T)current->interval.minValue()) {
			indexes[beg->second] = hist.size() - 1 + index_offset;
			++current->value;
			++beg;
		}
		if (beg >= en)
			break;

		tot_count += current->value;

		hist.append(VipIntervalSample());
		current = &hist.last();
		current->value = 1;
		current->interval.setMinValue(beg->first);
		current->interval.setMaxValue(beg->first);
		indexes[beg->second] = hist.size() - 1 + index_offset;
		++beg;
	}
	tot_count += current->value;

	// compute bins
	if (bins <= 0 || hist.size() <= bins) {
		if (strategy == Vip::SameBinWidth)
			expandSampleWidth(hist, 1);
		return hist;
	}

	// now, indexes contains, for each pixel, its index into the histogram
	// since we are reducing the histogram size, we need to compute the map old_index -> new_index
	std::vector<int> old_to_new(hist.size());
	old_to_new[0] = 0;

	VipIntervalSampleVector res;

	if (strategy == Vip::SameBinWidth) {
		T _min = hist.first().interval.minValue();
		T _max = hist.last().interval.minValue();

		// initialize histogram
		double width = (_max - _min) / (double)bins;
		double start = _min;
		res.resize(bins);
		for (int i = 0; i < res.size(); ++i, start += width)
			res[i] = VipIntervalSample(0, VipInterval(start, start + width, VipInterval::ExcludeMaximum));

		int i = 0;
		for (VipIntervalSampleVector::const_iterator it = hist.cbegin(); it != hist.cend(); ++it, ++i) {
			int index = std::floor((it->interval.minValue() - _min) / width);
			if (index == res.size())
				--index;
			res[index].value += it->value;
			old_to_new[i] = index;
		}
	}
	else {
		double slop_max = (tot_count / (double)bins) / (bins / 2);
		double slop = (slop_max / 4) * slop_factor;
		// double b = 2 * tot_count / std::sqrt(-2 * tot_count / slop);
		double b = (tot_count / (double)bins) - slop * (bins / 2);
		// if (slop == 0) b = b2;
		std::vector<double> heights(bins);
		for (std::size_t x = 0; x < heights.size(); ++x) {
			heights[x] = (slop)*x + b;
		}

		res.reserve(hist.size());

		// double bin_height = tot_count / (double)bins;
		double next_step = heights[0]; // bin_height;
		double tot_values = 0;
		int h = 0;
		VipIntervalSample start = hist.first();
		tot_values = start.value;
		old_to_new[0] = 0;
		VipIntervalSampleVector::const_iterator it = hist.cbegin();
		++it;
		int i = 1;
		for (; it != hist.cend(); ++it, ++i) {
			if (tot_values >= next_step) {
				res.append(start);
				start = *it;
				// next_step  += bin_height;
				next_step += heights[++h];
				tot_values += it->value;
				if (h == bins - 1 || heights[h] < 0) {
					// finsh
					old_to_new[i] = res.size();
					it++;
					++i;
					for (; it != hist.cend(); ++it, ++i) {
						start.interval.setMaxValue(it->interval.maxValue());
						start.value += it->value;
						old_to_new[i] = res.size();
					}
					break;
				}
			}
			else {
				start.interval.setMaxValue(it->interval.maxValue());
				start.value += it->value;
				tot_values += it->value;
			}
			old_to_new[i] = res.size();
		}
		res.append(start);
	}

	// we need to change the indexes from old to new ones
	bool valid_inter = inter.isValid();
	if (has_nan) {
#pragma omp parallel for num_threads(2)
		for (int i = 0; i < size; ++i)
			if (!isNan(begin[i]) && (!valid_inter || inter.contains(begin[i])))
				indexes[i] = old_to_new[indexes[i] - index_offset] + index_offset;
	}
	else {
#pragma omp parallel for num_threads(2)
		for (int i = 0; i < size; ++i)
			if ((!valid_inter || inter.contains(begin[i])))
				indexes[i] = old_to_new[indexes[i] - index_offset] + index_offset;
	}

	return res;
}

template<class T>
VipIntervalSampleVector genericExtractHistogram(const T* begin,
						const T* end,
						int bins,
						Vip::BinsStrategy strategy,
						const VipInterval& inter,
						int* indexes,
						int index_offset = 0,
						int replace_inf = -1,
						int replace_sup = -1,
						int replace_nan = -1,
						int slop_factor = 0)
{
	if (indexes)
		return extractHistogram(begin, end, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else
		return extractHistogram(begin, end, bins, strategy, inter);
}

bool vipSort(VipNDArray& array, Vip::ComplexSorting sort)
{
	array.detach();
	if (!array.isUnstrided())
		return false;
	if (array.isEmpty())
		return false;
	if (!array.constHandle()->dataPointer(VipNDArrayShape()))
		return false;

	int size = array.size();

	if (array.dataType() == QMetaType::Char)
		std::sort((char*)array.data(), (char*)array.data() + size);
	else if (array.dataType() == QMetaType::SChar)
		std::sort((signed char*)array.data(), (signed char*)array.data() + size);
	else if (array.dataType() == QMetaType::UChar)
		std::sort((unsigned char*)array.data(), (unsigned char*)array.data() + size);
	else if (array.dataType() == QMetaType::Short)
		std::sort((qint16*)array.data(), (qint16*)array.data() + size);
	else if (array.dataType() == QMetaType::UShort)
		std::sort((quint16*)array.data(), (quint16*)array.data() + size);
	else if (array.dataType() == QMetaType::Int)
		std::sort((qint32*)array.data(), (qint32*)array.data() + size);
	else if (array.dataType() == QMetaType::UInt)
		std::sort((quint32*)array.data(), (quint32*)array.data() + size);
	else if (array.dataType() == QMetaType::Long)
		std::sort((long*)array.data(), (long*)array.data() + size);
	else if (array.dataType() == QMetaType::ULong)
		std::sort((unsigned long*)array.data(), (unsigned long*)array.data() + size);
	else if (array.dataType() == QMetaType::LongLong)
		std::sort((qint64*)array.data(), (qint64*)array.data() + size);
	else if (array.dataType() == QMetaType::ULongLong)
		std::sort((quint64*)array.data(), (quint64*)array.data() + size);
	else if (array.dataType() == QMetaType::Double)
		std::sort((double*)array.data(), (double*)array.data() + size);
	else if (array.dataType() == QMetaType::Float)
		std::sort((float*)array.data(), (float*)array.data() + size);
	else if (array.dataType() == qMetaTypeId<complex_d>()) {
		switch (sort) {
			case Vip::SortByArgument:
				std::sort((complex_d*)array.data(), (complex_d*)array.data() + size, sort_by_arg<complex_d>());
				break;
			case Vip::SortByAmplitude:
				std::sort((complex_d*)array.data(), (complex_d*)array.data() + size, sort_by_amplitude<complex_d>());
				break;
			case Vip::SortByReal:
				std::sort((complex_d*)array.data(), (complex_d*)array.data() + size, sort_by_real<complex_d>());
				break;
			case Vip::SortByImag:
				std::sort((complex_d*)array.data(), (complex_d*)array.data() + size, sort_by_imag<complex_d>());
				break;
			default:
				return false;
		}
	}
	else if (array.dataType() == qMetaTypeId<complex_f>()) {
		switch (sort) {
			case Vip::SortByArgument:
				std::sort((complex_f*)array.data(), (complex_f*)array.data() + size, sort_by_arg<complex_f>());
				break;
			case Vip::SortByAmplitude:
				std::sort((complex_f*)array.data(), (complex_f*)array.data() + size, sort_by_amplitude<complex_f>());
				break;
			case Vip::SortByReal:
				std::sort((complex_f*)array.data(), (complex_f*)array.data() + size, sort_by_real<complex_f>());
				break;
			case Vip::SortByImag:
				std::sort((complex_f*)array.data(), (complex_f*)array.data() + size, sort_by_imag<complex_f>());
				break;
			default:
				return false;
		}
	}
	else
		return false;

	return true;
}

bool vipExtractHistogram(VipNDArray& array,
			 VipIntervalSampleVector& out,
			 int bins,
			 Vip::BinsStrategy strategy,
			 const VipInterval& inter,
			 int* indexes,
			 int index_offset,
			 int replace_inf,
			 int replace_sup,
			 int replace_nan,
			 int slop_factor)
{
	array.detach();
	if (!array.isUnstrided())
		return false;
	if (array.isEmpty())
		return false;
	if (!array.constHandle()->dataPointer(VipNDArrayShape()))
		return false;

	int size = array.size();
	if (array.dataType() == QMetaType::Bool)
		out = genericExtractHistogram((bool*)array.data(), (bool*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::Char)
		out = genericExtractHistogram((char*)array.data(), (char*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::SChar)
		out = genericExtractHistogram(
		  (signed char*)array.data(), (signed char*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::UChar)
		out = genericExtractHistogram(
		  (unsigned char*)array.data(), (unsigned char*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::Short)
		out = genericExtractHistogram((qint16*)array.data(), (qint16*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::UShort)
		out = genericExtractHistogram((quint16*)array.data(), (quint16*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::Int)
		out = genericExtractHistogram((qint32*)array.data(), (qint32*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::UInt)
		out = genericExtractHistogram((quint32*)array.data(), (quint32*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::Long)
		out = genericExtractHistogram((long*)array.data(), (long*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::ULong)
		out = genericExtractHistogram(
		  (unsigned long*)array.data(), (unsigned long*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::LongLong)
		out = genericExtractHistogram((qint64*)array.data(), (qint64*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::ULongLong)
		out = genericExtractHistogram((quint64*)array.data(), (quint64*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::Double)
		out = genericExtractHistogram((double*)array.data(), (double*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == QMetaType::Float)
		out = genericExtractHistogram((float*)array.data(), (float*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else if (array.dataType() == qMetaTypeId<long double>())
		out = genericExtractHistogram(
		  (long double*)array.data(), (long double*)array.data() + size, bins, strategy, inter, indexes, index_offset, replace_inf, replace_sup, replace_nan, slop_factor);
	else
		return false;
	return true;
}
