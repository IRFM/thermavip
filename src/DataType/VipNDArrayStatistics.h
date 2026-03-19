/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#ifndef VIP_ND_ARRAY_STATISTICS_H
#define VIP_ND_ARRAY_STATISTICS_H

#include <unordered_map>
#include <memory>

#include "VipEval.h"
#include "VipOverRoi.h"
#include "VipNDArray.h"

namespace Vip
{
	/// Array statistic values extracted with #VipArrayStatistics
	enum ArrayStatistic
	{
		DynamicStats = -1,
		NoStats = 0,
		Min = 0x001,	  //! minimum element
		Max = 0x002,	  //! maximum element
		MinPos = 0x004,	  //! minimum element position
		MaxPos = 0x008,	  //! maximum element position
		Mean = 0x010,	  //! aaverage value
		Sum = 0x020,	  //! cumulative sum
		Multiply = 0x040, //! cumulative multiplication
		Std = 0x080,	  //! standard deviation
		Skewness = 0x100,
		Kurtosis = 0x200,
		Entropy = 0x400,
		AllStats = Min | Max | MinPos | MaxPos | Mean | Sum | Multiply | Std | Skewness | Kurtosis | Entropy
	};
	Q_DECLARE_FLAGS(ArrayStatistics, ArrayStatistic);
	Q_DECLARE_OPERATORS_FOR_FLAGS(ArrayStatistics)
}

template<class T>
struct VipArrayStatistics
{
	using sum_type = std::conditional_t<std::is_integral_v<T>, std::conditional_t<std::is_signed_v<T>, qint64, quint64>, double>;

	qsizetype count = 0;
	T min = 0, max = 0;
	sum_type sum = 0, multiply = 0;
	double std = 0, var = 0, mean = 0;
	double skewness = 0, kurtosis = 0, entropy = 0;
	VipNDArrayShape minPos, maxPos;
};

namespace detail
{
	/// See http://www.johndcook.com/blog/skewness_kurtosis/ for more details
	template<class T, int Stats>
	class ComputeStats
	{
	public:
		ComputeStats(qsizetype st = 0)
		  : stats(st)
		{
		}

		void _add(T x) noexcept(!(Stats & Vip::Entropy))
		{
			double delta, delta_n, delta_n2, term1;
			qsizetype n1 = n++;
			delta = (double)x - M1;
			delta_n = delta / n;
			delta_n2 = delta_n * delta_n;
			term1 = delta * delta_n * n1;
			M1 += delta_n;
			M4 += term1 * delta_n2 * (n * n - 3 * n + 3) + 6 * delta_n2 * M2 - 4 * delta_n * M3;
			M3 += term1 * delta_n * (n - 2) - 3 * delta_n * M2;
			M2 += term1;

			if (stats & Vip::Entropy) {
				if (!map)
					map.reset(new std::unordered_map<T, qsizetype>());
				auto p = map->insert({ x, 1 });
				if (!p.second)
					p.first->second++;
			}
		}

		double _skewness() const noexcept { return std::sqrt(double(n)) * M3 / std::pow(M2, 1.5); }
		double _kurtosis() const noexcept { return double(n) * M4 / (M2 * M2) - 3.0; }
		double _entropy() const noexcept
		{
			double inv_log_2 = 1 / std::log(2);
			double entropy = 0;
			for (auto it = map->begin(); it != map->end(); ++it) {
				double p = it->second / double(n);
				entropy += p * std::log(p) * inv_log_2;
			}
			entropy = -entropy;
			return entropy;
		}

	private:
		std::unique_ptr<std::unordered_map<T, qsizetype>> map;
		qsizetype stats = 0;
		qsizetype n = 0;
		double M1 = 0, M2 = 0, M3 = 0, M4 = 0;
	};

	template<class T>
	struct ComputeStats<T, 0>
	{
		void _add(T) noexcept {}
		double _skewness() const noexcept { return 0; }
		double _kurtosis() const noexcept { return 0; }
		double _entropy() const noexcept { return 0; }
	};

	/// Reduction algorithm extracting the min, max, sum and mean values of an array or a functor expression.
	/// This is also the return type of function #VipArrayStatistics.
	///
	/// Depending on the value of \a Stats, not all statistics are extracted to optimize the computation.
	template<class T, int Stats = Vip::AllStats>
	class ExtractArrayStatistics
	  : public detail::Reductor<decltype(T() * T())>
	  , private detail::ComputeStats<T, (Stats < 0 ? Vip::AllStats : Stats)>
	{
		using base = detail::ComputeStats<T, (Stats < 0 ? Vip::AllStats : Stats)>;
		using val_type = typename detail::Reductor<decltype(T() * T())>::value_type;
		using sum_type = std::conditional_t<std::is_integral_v<T>, std::conditional_t<std::is_signed_v<T>, qint64, quint64>, double>;

		bool first = true;
		sum_type sum2 = 0;
		qsizetype stats = 0;

	public:
		using value_type = val_type;
		static constexpr qsizetype access_type = (Stats < 0) ? Vip::Position : ((Vip::Position | (((Stats & Vip::MinPos) | (Stats & Vip::MaxPos)) ? 0 : Vip::Flat)));

		VipArrayStatistics<T> ret;

		ExtractArrayStatistics(qsizetype st = Stats < 0 ? Vip::AllStats : Stats) noexcept
		  : base(Stats < 0 ? Vip::AllStats : Stats)
		  , stats(st)
		{
		}

		bool setAt(qsizetype, const T& value) { return setPos(vipVector(0), value); }

		template<class Coord>
		bool setPos(const Coord& pos, const T& value)
		{
			if (vipIsNan(value))
				return false;

			++ret.count;
			if (first) {
				ret.min = ret.max = value;
				ret.sum = (sum_type)value;
				ret.multiply = 1;
				first = false;
				if (!std::is_integral_v<Coord> && (stats & Vip::MinPos))
					ret.minPos = pos;
				if (!std::is_integral_v<Coord> && (stats & Vip::MaxPos))
					ret.maxPos = pos;
				if (stats & Vip::Std)
					sum2 += (sum_type)value * (sum_type)value;
				if ((stats & Vip::Skewness) || (stats & Vip::Kurtosis) || (stats & Vip::Entropy))
					this->_add(value);
				return true;
			}

			if constexpr (Stats < 0) {

				if (value < ret.min) {
					ret.min = value;
					if (!std::is_integral_v<Coord> && (stats & Vip::MinPos))
						ret.minPos = pos;
				}

				if (value > ret.max) {
					ret.max = value;
					if (!std::is_integral_v<Coord> && (stats & Vip::MaxPos))
						ret.maxPos = pos;
				}

				if (stats & Vip::Multiply)
					ret.multiply *= (sum_type)value;

				ret.sum += (sum_type)value;
				if (stats & Vip::Std)
					sum2 += (sum_type)value * (sum_type)value;

				if ((stats & Vip::Skewness) || (stats & Vip::Kurtosis) || (stats & Vip::Entropy)) {
					this->_add(value);
				}
			}
			else {
				if constexpr (Stats & Vip::Min) {
					if (value < ret.min) {
						ret.min = value;
						if constexpr (!std::is_integral_v<Coord> && (Stats & Vip::MinPos))
							ret.minPos = pos;
					}
				}
				if constexpr (Stats & Vip::Max) {
					if (value > ret.max) {
						ret.max = value;
						if constexpr (!std::is_integral_v<Coord> && (Stats & Vip::MaxPos))
							ret.maxPos = pos;
					}
				}
				if constexpr (Stats & Vip::Multiply)
					ret.multiply *= (sum_type)value;

				if constexpr ((Stats & Vip::Mean) || (Stats & Vip::Sum) || (Stats & Vip::Std)) {
					ret.sum += (sum_type)value;
					if constexpr (Stats & Vip::Std)
						sum2 += (sum_type)value * (sum_type)value;
				}
				if constexpr ((Stats & Vip::Skewness) || (Stats & Vip::Kurtosis) || (Stats & Vip::Entropy))
					this->_add(value);
			}

			return true;
		}
		bool finish()
		{
			if ((stats & Vip::Mean) || stats & Vip::Std) {
				if (ret.count) {
					ret.mean = (ret.sum / (double)ret.count);
					if (stats & Vip::Std) {
						ret.var = (sum2 - ret.count * ret.mean * ret.mean) / (ret.count);
						ret.std = std::sqrt(ret.var);
					}
				}
			}
			if ((stats & Vip::Skewness) && ret.count)
				ret.skewness = this->_skewness();
			if ((stats & Vip::Kurtosis) && ret.count)
				ret.kurtosis = this->_kurtosis();
			if ((stats & Vip::Entropy) && ret.count)
				ret.entropy = this->_entropy();
			return true;
		}
	};

}

/// Extracts the array or functor expression statistics and returns them as a VipArrayStatistics object.
///
/// Note that the statistics value type can be specified as template parameter and might be different from the source value type.
/// Indeed, it is better to compute the sum of a 8 bits integer array as a 32 bits integer instead of a 8 bits integer
/// to avoid overflow.
///
/// It is also advised to use floating point type when computing the mean value or the standard deviation.
template<class T, int Stats = Vip::AllStats, class Src, class OverRoi = VipInfinitRoi>
auto vipArrayStatistics(const Src& src, const OverRoi& roi = {})
{
	detail::ExtractArrayStatistics<T, Stats> stats;
	stats.ret.minPos.resize(src.shape().size(), 0);
	stats.ret.maxPos.resize(src.shape().size(), 0);
	if (!vipEval(stats, src, roi))
		return VipArrayStatistics<T>{};
	return stats.ret;
}
template<class T, class Src, class OverRoi = VipInfinitRoi>
auto vipArrayStatistics(const Src& src, int statistics, const OverRoi& roi = {})
{
	detail::ExtractArrayStatistics<T, Vip::DynamicStats> stats(statistics);
	stats.ret.minPos.resize(src.shape().size(), 0);
	stats.ret.maxPos.resize(src.shape().size(), 0);
	if (!vipEval(stats, src, roi))
		return VipArrayStatistics<T>{};
	return stats.ret;
}

/// Returns the minimum value of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi>
T vipArrayMin(const Src& src, const OverRoi& roi = {})
{
	return vipArrayStatistics<T, Vip::Min>(src, roi).ret.min;
}
/// Returns the maximum value of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi>
T vipArrayMax(const Src& src, const OverRoi& roi = {})
{
	return vipArrayStatistics<T, Vip::Max>(src, roi).ret.max;
}
/// Returns the cumulative sum of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi>
T vipArrayCumSum(const Src& src, const OverRoi& roi = {})
{
	return vipArrayStatistics<T, Vip::Sum>(src, roi).ret.sum;
}
/// Returns the cumulative multiplication of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi>
T vipArrayCumMultiply(const Src& src, const OverRoi& roi = {})
{
	return vipArrayStatistics<T, Vip::Multiply>(src, roi).ret.multiply;
}
/// Returns the mean value of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi>
T vipArrayMean(const Src& src, const OverRoi& roi = {})
{
	return vipArrayStatistics<T, Vip::Mean>(src, roi).ret.mean;
}
/// Returns the standard deviation of input array or functor expression
template<class T, class Src, class OverRoi = VipInfinitRoi>
T vipArrayStd(const Src& src, const OverRoi& roi = {})
{
	return vipArrayStatistics<T, Vip::Std>(src, roi).ret.std;
}

namespace detail
{
	template<class T, class Functor>
	struct Accumulate : public detail::Reductor<T>
	{
		static constexpr qsizetype access_type = Vip::Position | Vip::Flat;

		T init;
		Functor fun;

		template<class _T, class _F>
		Accumulate(_T&& val, _F&& f)
		  : init(std::forward<_T>(val))
		  , fun(std::forward<_F>(f))
		{
		}

		bool setAt(qsizetype, const T& value) { return setPos(0, value); }

		template<class Coord>
		bool setPos(const Coord&, const T& value)
		{
			init = fun(init, value);
			return true;
		}
	};
}

template<class Src, class T, class Fun, class OverRoi = VipInfinitRoi>
T vipAccumulate(const Src& src, T init, Fun f, const OverRoi& roi = {})
{
	detail::Accumulate<T, Fun> acc(std::move(init), std::move(f));
	if (!vipEval(acc, src, roi))
		return {};
	return std::move(acc.init);
}

#endif
