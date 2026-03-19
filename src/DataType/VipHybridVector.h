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

#ifndef VIP_HYBRID_VECTOR_H
#define VIP_HYBRID_VECTOR_H

#include "VipConfig.h"

#include <QDataStream>
#include <QPoint>

#include <algorithm>
#include <iterator>

/// \addtogroup DataType
/// @{

/// Maximum number of dimensions supported by the DataType library.
/// Can be extended to any value.
#define VIP_MAX_DIMS 4

namespace Vip
{
	/// Constant value for invalid indexes
	const qsizetype None = -1;
}

namespace detail
{
	// To allow ADL with custom begin/end
	using std::begin;
	using std::end;

	template<class T>
	auto is_iterable_impl(int) -> decltype(begin(std::declval<T&>()) != end(std::declval<T&>()),   // begin/end and operator !=
					       void(),						       // Handle evil operator ,
					       ++std::declval<decltype(begin(std::declval<T&>()))&>(), // operator ++
					       void(*begin(std::declval<T&>())),		       // operator*
					       std::true_type{});

	template<typename T>
	std::false_type is_iterable_impl(...);

}

template<class F, qsizetype... Dims>
VIP_ALWAYS_INLINE void vipForEachDims(F&& f, std::integer_sequence<qsizetype, Dims...>)
{
	(std::forward<F>(f)(Dims), ...);
}
template<class F, qsizetype... Dims>
VIP_ALWAYS_INLINE void vipForEachDimsBackward(F&& f, std::integer_sequence<qsizetype, Dims...>)
{
	(..., std::forward<F>(f)(Dims));
}

template<class F, qsizetype First, qsizetype... Dims>
VIP_ALWAYS_INLINE bool vipForEachDimsUntil(F&& f, std::integer_sequence<qsizetype, First, Dims...>)
{
	if constexpr (sizeof...(Dims) == 0)
		return std::forward<F>(f)(First);
	else {
		if (!std::forward<F>(f)(First))
			return false;
		else
			return vipForEachDimsUntil(std::forward<F>(f), std::integer_sequence<qsizetype, Dims...>{});
	}
}

/// @brief Check if given type is iterable
template<class T>
using VipIsIterable = decltype(detail::is_iterable_impl<T>(0));

template<class T>
constexpr bool VipIsIterable_v = VipIsIterable<T>::value;

/// @brief Check if givern type is an iterator
template<class T, class = void>
struct VipIsIterator : std::false_type
{
};

template<class T>
struct VipIsIterator<T, std::void_t<typename std::iterator_traits<T>::iterator_category>> : std::true_type
{
};
template<class T>
constexpr bool VipIsIterator_v = VipIsIterator<T>::value;

/// VipHybridVector is a container that follows an interface and a behavior similar to std::array.
/// It is mainly used to store shapes and positions for #VipNDArray objects.
/// Its behavior depends on the template integer \a N:
/// -If N is positive, VipHybridVector is a fixed size static vector
/// -If N is negative, VipHybridVector is a dynamic-size vector working on a static array of VIP_MAX_DIMS elements.
/// Use one of the #vipVector overloads to create static VipHybridVector objects.
template<class T, qsizetype N>
struct VipHybridVector
{
	static_assert(N <= VIP_MAX_DIMS, "");
	static constexpr qsizetype static_size = N;

	T elems[N];

	// type definitions
	typedef T value_type;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;

	// iterator support
	VIP_ALWAYS_INLINE iterator begin() noexcept { return elems; }
	VIP_ALWAYS_INLINE const_iterator begin() const noexcept { return elems; }

	VIP_ALWAYS_INLINE iterator end() noexcept { return elems + N; }
	VIP_ALWAYS_INLINE const_iterator end() const noexcept { return elems + N; }

	// operator[]
	VIP_ALWAYS_INLINE reference operator[](size_type i) noexcept
	{
		VIP_ASSERT_DEBUG(i < N && i >= 0);
		return elems[i];
	}
	VIP_ALWAYS_INLINE const_reference operator[](size_type i) const noexcept
	{
		VIP_ASSERT_DEBUG(i < N && i >= 0);
		return elems[i];
	}

	VIP_ALWAYS_INLINE reference at(size_type i) noexcept
	{
		VIP_ASSERT_DEBUG(i < N && i >= 0);
		return elems[i];
	}
	VIP_ALWAYS_INLINE const_reference at(size_type i) const noexcept
	{
		VIP_ASSERT_DEBUG(i < N && i >= 0);
		return elems[i];
	}

	// front() and back()
	VIP_ALWAYS_INLINE reference front() noexcept { return elems[0]; }
	VIP_ALWAYS_INLINE const_reference front() const noexcept { return elems[0]; }

	VIP_ALWAYS_INLINE reference back() noexcept { return elems[N - 1]; }
	VIP_ALWAYS_INLINE const_reference back() const noexcept { return elems[N - 1]; }

	VIP_ALWAYS_INLINE reference first() noexcept { return front(); }
	VIP_ALWAYS_INLINE const_reference first() const noexcept { return front(); }
	VIP_ALWAYS_INLINE reference last() noexcept { return back(); }
	VIP_ALWAYS_INLINE const_reference last() const noexcept { return back(); }

	// size is constant
	static VIP_ALWAYS_INLINE size_type size() noexcept { return N; }
	static VIP_ALWAYS_INLINE bool isEmpty() noexcept { return false; }

	// swap (note: linear complexity)
	void swap(VipHybridVector<T, N>& y) noexcept
	{
		for (size_type i = 0; i < N; ++i)
			std::swap(elems[i], y.elems[i]);
	}

	// direct access to data (read-only)
	VIP_ALWAYS_INLINE const T* data() const noexcept { return elems; }
	VIP_ALWAYS_INLINE T* data() noexcept { return elems; }

	// assignment with type conversion
	template<class Other>
	VipHybridVector& operator=(const Other& rhs) noexcept
	{
		qsizetype s = qMin(size(), rhs.size());
		for (qsizetype i = 0; i < s; ++i)
			elems[i] = static_cast<T>(rhs[i]);
		return *this;
	}

	// assign one value to all elements
	void assign(const T& value) noexcept { fill(value); } // A synonym for fill
	void fill(const T& value) noexcept { std::fill_n(elems, N, value); }

	void resize(size_type) noexcept {}

	QVector<T> toVector() const
	{
		QVector<T> res(N);
		std::copy(begin(), end(), res.begin());
		return res;
	}

	operator QVector<T>() const { return toVector(); }

	explicit operator bool() const noexcept { return true; }

	template<class Other>
	bool operator==(const Other& other) const noexcept
	{
		if (other.size() != size())
			return false;
		return std::equal(begin(), end(), other.begin());
	}
	template<class Other>
	bool operator!=(const Other& other) const noexcept
	{
		return !(*this == other);
	}
};

/// @brief Detect VipHybridVector type
template<class T>
struct VipIsHybridVector : std::false_type
{
};
template<class T, qsizetype N>
struct VipIsHybridVector<VipHybridVector<T, N>> : std::true_type
{
};

template<class T>
class VipHybridVector<T, Vip::None>
{

	T m_elems[VIP_MAX_DIMS];
	qsizetype m_size;

public:
	static constexpr qsizetype static_size = Vip::None;

	// type definitions
	typedef T value_type;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;

	VIP_ALWAYS_INLINE VipHybridVector() noexcept
	  : m_size(0)
	{
	}
	VIP_ALWAYS_INLINE VipHybridVector(qsizetype size) noexcept
	  : m_size(size)
	{
	}
	VIP_ALWAYS_INLINE VipHybridVector(qsizetype size, const T& elem) noexcept
	  : m_size(size)
	{
		fill(elem);
	}

	template<class Iter, std::enable_if_t<VipIsIterator_v<Iter>, int> = 0>
	VIP_ALWAYS_INLINE VipHybridVector(Iter first, Iter last)
	{
		resize(std::distance(first, last));
		std::copy(first, last, begin());
	}

	template<class Other, std::enable_if_t<VipIsIterable_v<Other>, int> = 0>
	VIP_ALWAYS_INLINE VipHybridVector(const Other& v) noexcept
	{
		VIP_ASSERT_DEBUG(v.size() <= VIP_MAX_DIMS);
		resize(v.size());
		for (qsizetype i = 0; i < m_size; ++i)
			m_elems[i] = static_cast<T>(v[i]);
	}

	VIP_ALWAYS_INLINE VipHybridVector(const VipHybridVector& other) noexcept
	  : m_size(other.size())
	{
		memcpy(m_elems, other.m_elems, VIP_MAX_DIMS * sizeof(T));
	}

	VIP_ALWAYS_INLINE void push_back(const T& elem) noexcept
	{
		VIP_ASSERT_DEBUG(m_size < VIP_MAX_DIMS);
		m_elems[m_size++] = elem;
	}

	VIP_ALWAYS_INLINE void clear() noexcept { m_size = 0; }

	// iterator support
	VIP_ALWAYS_INLINE iterator begin() noexcept { return m_elems; }		    // elems.begin(); }
	VIP_ALWAYS_INLINE const_iterator begin() const noexcept { return m_elems; } // elems.begin();
	VIP_ALWAYS_INLINE iterator end() noexcept { return m_elems + m_size; }	    // elems.end(); }
	VIP_ALWAYS_INLINE const_iterator end() const noexcept { return m_elems + m_size; }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
	//  operator[]
	VIP_ALWAYS_INLINE reference operator[](size_type i) noexcept
	{
		VIP_ASSERT_DEBUG(i >= 0 && i < m_size);
		return m_elems[i];
	}
	VIP_ALWAYS_INLINE const_reference operator[](size_type i) const noexcept
	{
		VIP_ASSERT_DEBUG(i >= 0 && i < m_size);
		return m_elems[i];
	}

	VIP_ALWAYS_INLINE reference at(size_type i) noexcept
	{
		VIP_ASSERT_DEBUG(i >= 0 && i < m_size);
		return m_elems[i];
	}
	VIP_ALWAYS_INLINE const_reference at(size_type i) const noexcept
	{
		VIP_ASSERT_DEBUG(i >= 0 && i < m_size);
		return m_elems[i];
	}

	VIP_ALWAYS_INLINE reference front() noexcept
	{
		VIP_ASSERT_DEBUG(!isEmpty());
		return m_elems[0];
	}
	VIP_ALWAYS_INLINE const_reference front() const noexcept
	{
		VIP_ASSERT_DEBUG(!isEmpty());
		return m_elems[0];
	}

	VIP_ALWAYS_INLINE reference back() noexcept
	{
		VIP_ASSERT_DEBUG(!isEmpty());
		return m_elems[m_size - 1];
	}
	VIP_ALWAYS_INLINE const_reference back() const noexcept
	{
		VIP_ASSERT_DEBUG(!isEmpty());
		return m_elems[m_size - 1];
	}

	VIP_ALWAYS_INLINE reference first() noexcept { return front(); }
	VIP_ALWAYS_INLINE const_reference first() const noexcept { return front(); }
	VIP_ALWAYS_INLINE reference last() noexcept { return back(); }
	VIP_ALWAYS_INLINE const_reference last() const noexcept { return back(); }

	// size is constant
	VIP_ALWAYS_INLINE size_type size() const noexcept { return m_size; }
	VIP_ALWAYS_INLINE bool isEmpty() const noexcept { return m_size == 0; }

	VIP_ALWAYS_INLINE void swap(VipHybridVector& y) noexcept
	{
		std::swap(m_size, y.m_size);
		for (size_type i = 0; i < VIP_MAX_DIMS; ++i)
			std::swap(m_elems[i], y.m_elems[i]);
	}

	VIP_ALWAYS_INLINE const T* data() const noexcept { return m_elems; }
	VIP_ALWAYS_INLINE T* data() noexcept { return m_elems; }

	// assignment with type conversion
	template<class Other>
	VIP_ALWAYS_INLINE VipHybridVector& operator=(const Other& rhs) noexcept
	{
		VIP_ASSERT_DEBUG(rhs.size() <= VIP_MAX_DIMS);
		m_size = rhs.size();
		for (qsizetype i = 0; i < m_size; ++i)
			m_elems[i] = static_cast<T>(rhs[i]);
		return *this;
	}

	VIP_ALWAYS_INLINE VipHybridVector& operator=(const VipHybridVector& other) noexcept
	{
		m_size = other.m_size;
		// std::copy(other.m_elems, other.m_elems + VIP_MAX_DIMS, m_elems);
		memcpy(m_elems, other.m_elems, VIP_MAX_DIMS * sizeof(T));
		return *this;
	}

	// assign one value to all elements
	VIP_ALWAYS_INLINE void assign(const T& value) noexcept { fill(value); } // A synonym for fill
	VIP_ALWAYS_INLINE void fill(const T& value) noexcept { std::fill_n(m_elems, VIP_MAX_DIMS, value); }

	VIP_ALWAYS_INLINE void resize(size_type new_size) noexcept
	{
		VIP_ASSERT_DEBUG(new_size <= VIP_MAX_DIMS && new_size >= 0);
		m_size = new_size;
	}
	VIP_ALWAYS_INLINE void resize(size_type new_size, const T & val) noexcept
	{
		VIP_ASSERT_DEBUG(new_size <= VIP_MAX_DIMS && new_size >= 0);
		for (qsizetype i = m_size; i < new_size; ++i)
			m_elems[i] = val;
		m_size = new_size;
	}

	QVector<T> toVector() const
	{
		QVector<T> res(size());
		std::copy(begin(), end(), res.begin());
		return res;
	}

	template<class Other, std::enable_if_t<VipIsIterable_v<Other>, int> = 0>
	VIP_ALWAYS_INLINE operator Other() const
	{
		if constexpr (VipIsHybridVector<Other>::value) {
			Other ret;
			ret.resize(size());
			std::copy(begin(), end(), ret.begin());
			return ret;
		}
		else
			return Other(begin(), end());
	}

	VIP_ALWAYS_INLINE explicit operator bool() const noexcept { return size() > 0; }

	template<class Other>
	VIP_ALWAYS_INLINE bool operator==(const Other& other) const noexcept
	{
		if (size() != other.size())
			return false;
		return std::equal(begin(), end(), other.begin());
	}
	template<class Other>
	VIP_ALWAYS_INLINE bool operator!=(const Other& other) const noexcept
	{
		return !(*this == other);
	}

	VIP_ALWAYS_INLINE VipHybridVector& operator<<(const T& elem) noexcept
	{
		push_back(elem);
		return *this;
	}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
};

/// @brief ND coordinate typedef
template<qsizetype NDims>
using VipCoordinate = VipHybridVector<qsizetype, NDims>;

/// @brief VipNDArrayShape is a dynamic VipHybridVector. It is used to represent the shape of a VipNDArray.
using VipNDArrayShape = VipCoordinate<Vip::None>;
VIP_IS_RELOCATABLE(VipNDArrayShape);

template<qsizetype N1, qsizetype N2>
VIP_ALWAYS_INLINE auto operator+(const VipCoordinate<N1>& v1, const VipCoordinate<N2>& v2) noexcept
{
	if constexpr (N1 > 0 && N2 > 0) {

		static constexpr qsizetype min_size = N1 < N2 ? N1 : N2;
		static constexpr qsizetype max_size = N1 < N2 ? N2 : N1;
		VipCoordinate<max_size> ret;

		vipForEachDims([&](qsizetype i) { ret[i] = v1[i] + v2[i]; }, std::make_integer_sequence<qsizetype, min_size>{});
		vipForEachDims(
		  [&](qsizetype i) {
			  if constexpr (N1 > N2)
				  ret[min_size + i] = v1[min_size + i];
			  else
				  ret[min_size + i] = v2[min_size + i];
		  },
		  std::make_integer_sequence<qsizetype, max_size - min_size>{});
		return ret;
	}
	else {
		VipCoordinate<Vip::None> ret;
		qsizetype min_size = std::min(v1.size(), v2.size());
		ret.resize(std::max(v1.size(), v2.size()));
		for (qsizetype i = 0; i < min_size; ++i)
			ret[i] = v1[i] + v2[i];
		for (qsizetype i = min_size; i < ret.size(); ++i)
			ret[i] = v1.size() > v2.size() ? v1[i] : v2[i];
		return ret;
	}
}

template<class T, qsizetype N>
QDataStream& operator<<(QDataStream& os, const VipHybridVector<T, N>& v)
{
	os << v.size();
	for (qsizetype i = 0; i < v.size(); ++i)
		os << v[i];
	return os;
}

template<class T, qsizetype N>
QDataStream& operator>>(QDataStream& is, VipHybridVector<T, N>& v)
{
	qsizetype size;
	is >> size;
	v.resize(size);
	for (qsizetype i = 0; i < size; ++i)
		is >> v[i];
	return is;
}

/// @brief Return a reversed version of \a vec
template<class T, qsizetype N>
VIP_ALWAYS_INLINE VipHybridVector<T, N> vipReverse(const VipHybridVector<T, N>& vec) noexcept
{
	VipHybridVector<T, N> reverse;
	if constexpr (N == 1) {
		reverse[0] = vec[0];
	}
	else if constexpr (N == 2) {
		reverse[0] = vec[1];
		reverse[1] = vec[0];
	}
	else if constexpr (N == 3) {
		reverse[0] = vec[2];
		reverse[1] = vec[1];
		reverse[2] = vec[0];
	}
	else {
		reverse.resize(vec.size());
		for (qsizetype i = 0; i < vec.size(); ++i)
			reverse[i] = vec[vec.size() - i - 1];
	}
	return reverse;
}

template<qsizetype N, class F>
VIP_ALWAYS_INLINE void vipForEachCoord(const VipCoordinate<N>& c, F&& f)
{
	if constexpr (N > 0) {
		using seq = std::make_integer_sequence<qsizetype, N>;
		vipForEachDims(std::forward<F>(f), seq{});
	}
	else {
		for (qsizetype i = 0; i < c.size(); ++i)
			f(i);
	}
}
template<qsizetype N, class F>
VIP_ALWAYS_INLINE void vipForEachCoordBackward(const VipCoordinate<N>& c, F&& f)
{
	if constexpr (N > 0) {
		vipForEachDimsBackward(std::forward<F>(f), std::make_integer_sequence<qsizetype, N>{});
	}
	else {
		for (qsizetype i = c.size() - 1; i >= 0; --i)
			std::forward<F>(f)(i);
	}
}

template<qsizetype N, class F>
VIP_ALWAYS_INLINE bool vipForEachCoordUntil(const VipCoordinate<N>& c, F&& f)
{
	if constexpr (N > 0) {
		return vipForEachDimsUntil(std::forward<F>(f), std::make_integer_sequence<qsizetype, N>{});
	}
	else {
		for (qsizetype i = 0; i < c.size(); ++i) {
			if (!std::forward<F>(f)(i))
				return false;
		}
		return true;
	}
}

/// @brief Returns a copy of \a v and change its static size. Note that this won't work if N != N2 && N > 0 && N2 > 0.
template<qsizetype N, qsizetype N2>
VIP_ALWAYS_INLINE VipCoordinate<N> vipVector(const VipCoordinate<N2>& v) noexcept
{
	return v;
}
/// @brief Create a dynamic VipHybridVector from a QVector
template<class T>
VIP_ALWAYS_INLINE VipHybridVector<T, Vip::None> vipVector(const QVector<T>& v) noexcept
{
	VipHybridVector<T, Vip::None> res;
	res = v;
	return res;
}

/// @brief Create a VipCoordinate object
template<class... Args>
VIP_ALWAYS_INLINE auto vipVector(Args... sizes) noexcept
{
	return VipCoordinate<sizeof...(Args)>{ { std::forward<qsizetype>(sizes)... } };
}

VIP_ALWAYS_INLINE auto vipVector(const QPoint& pt)
{
	return vipVector(pt.y(), pt.x());
}

template<qsizetype N>
VIP_ALWAYS_INLINE auto vipPoint(const VipCoordinate<N>& c)
{
	if (c.size() != 2)
		return QPoint();
	return QPoint(c[1], c[0]);
}

template<qsizetype NDims>
VIP_ALWAYS_INLINE auto vipRepeat(qsizetype count, qsizetype value) noexcept
{
	VipCoordinate<NDims> ret;
	if constexpr (NDims == Vip::None)
		ret.resize(count);
	vipForEachCoord(ret, [&](qsizetype i) { ret[i] = value; });
	return ret;
}

#include <tuple>
/// https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
/// MACRO version of vipVector, much faster on msvc (?)
#define vip_vector(...)                                                                                                                                                                                \
	VipCoordinate<std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value>                                                                                                                  \
	{                                                                                                                                                                                              \
		__VA_ARGS__                                                                                                                                                                            \
	}

/// @}
// end DataType

#endif
