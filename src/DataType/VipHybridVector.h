/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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
#include <algorithm>

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
	enum
	{
		static_size = N
	};
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
	VIP_ALWAYS_INLINE reference operator[](size_type i) noexcept { return elems[i]; }
	VIP_ALWAYS_INLINE const_reference operator[](size_type i) const noexcept { return elems[i]; }

	VIP_ALWAYS_INLINE reference at(size_type i) noexcept { return elems[i]; }
	VIP_ALWAYS_INLINE const_reference at(size_type i) const noexcept { return elems[i]; }

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
	static size_type size() noexcept { return N; }
	static bool isEmpty() noexcept { return false; }

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
	template<typename T2, qsizetype N2>
	VipHybridVector& operator=(const VipHybridVector<T2, N2>& rhs) noexcept
	{
		qsizetype s = qMin(size(), rhs.size());
		for (qsizetype i = 0; i < s; ++i)
			elems[i] = rhs[i];
		return *this;
	}

	template<typename T2>
	VipHybridVector<T, N>& operator=(const QVector<T2>& rhs) noexcept
	{
		fromVector(rhs);
		return *this;
	}

	// assign one value to all elements
	void assign(const T& value) noexcept { fill(value); } // A synonym for fill
	void fill(const T& value) noexcept { std::fill_n(elems, N, value); }

	void resize(size_type) noexcept {}

	void fromVector(const QVector<T>& vec) noexcept
	{
		qsizetype s = qMin(vec.size(), (qsizetype)size());
		for (qsizetype i = 0; i < s; ++i)
			elems[i] = vec[i];
	}

	QVector<T> toVector() const
	{
		QVector<T> res(N);
		std::copy(begin(), end(), res.begin());
		return res;
	}

	operator QVector<T>() const { return toVector(); }

	explicit operator bool() const noexcept { return true; }

	template<class T2, qsizetype Dim2>
	bool operator==(const VipHybridVector<T2, Dim2>& other) const noexcept
	{
		if (other.size() != size())
			return false;
		return std::equal(begin(), end(), other.begin());
	}

	bool operator==(const QVector<T>& other) const noexcept
	{
		if (other.size() != size())
			return false;
		return std::equal(begin(), end(), other.begin());
	}

	template<class T2, qsizetype Dim2>
	bool operator!=(const VipHybridVector<T2, Dim2>& other) const noexcept
	{
		if (other.size() != size())
			return true;
		return !std::equal(begin(), end(), other.begin());
	}
};

template<class T>
class VipHybridVector<T, Vip::None>
{

	T m_elems[VIP_MAX_DIMS];
	qsizetype m_size;

public:
	enum
	{
		static_size = Vip::None
	};

	// type definitions
	typedef T value_type;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef T& reference;
	typedef const T& const_reference;
	typedef qsizetype size_type;
	typedef qsizetype difference_type;

	VipHybridVector() noexcept
	  : m_size(0)
	{
	}
	VipHybridVector(qsizetype size) noexcept
	  : m_size(size)
	{
	}
	VipHybridVector(qsizetype size, const T& elem) noexcept
	  : m_size(size)
	{
		fill(elem);
	}

	template<class T2>
	VipHybridVector(const QVector<T2>& v) noexcept
	{
		resize(v.size());
		for (qsizetype i = 0; i < m_size; ++i)
			m_elems[i] = static_cast<T>(v[i]);
	}

	VipHybridVector(const VipHybridVector& other) noexcept
	  : m_size(other.size())
	{
		memcpy(m_elems, other.m_elems, VIP_MAX_DIMS * sizeof(T));
		// std::copy(other.m_elems,other.m_elems + VIP_MAX_DIMS,m_elems);
	}

	template<class T2, qsizetype N2>
	VipHybridVector(const VipHybridVector<T2, N2>& v) noexcept
	{
		resize(v.size());
		for (qsizetype i = 0; i < m_size; ++i)
			m_elems[i] = static_cast<T>(v[i]);
	}

	template<qsizetype N2>
	VipHybridVector(const VipHybridVector<T, N2>& v) noexcept
	{
		m_size = N2;
		memcpy(m_elems, v.elems, sizeof(T) * N2);
	}

	void push_back(const T& elem) noexcept { m_elems[m_size++] = elem; }

	void clear() noexcept { m_size = 0; }

	// iterator support
	iterator begin() noexcept { return m_elems; }		  // elems.begin(); }
	const_iterator begin() const noexcept { return m_elems; } // elems.begin();
	iterator end() noexcept { return m_elems + m_size; }	  // elems.end(); }
	const_iterator end() const noexcept
	{
		return m_elems + m_size;
	} // return elems.end(); }

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
	  //  operator[]
	reference operator[](size_type i) noexcept
	{
		return m_elems[i];
	}
	const_reference operator[](size_type i) const noexcept
	{
		return m_elems[i];
	}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

	reference at(size_type i) noexcept
	{
		return m_elems[i];
	}
	const_reference at(size_type i) const noexcept
	{
		return m_elems[i];
	}

	// front() and back()
	reference front() noexcept
	{
		return m_elems[0];
	}
	const_reference front() const noexcept
	{
		return m_elems[0];
	}

	reference back() noexcept
	{
		return m_elems[m_size - 1];
	}
	const_reference back() const noexcept
	{
		return m_elems[m_size - 1];
	}

	reference first() noexcept
	{
		return front();
	}
	const_reference first() const noexcept
	{
		return front();
	}
	reference last() noexcept
	{
		return back();
	}
	const_reference last() const noexcept
	{
		return back();
	}

	// size is constant
	size_type size() const noexcept
	{
		return m_size;
	} // static_cast<size_type>(elems.size()); }
	bool isEmpty() const noexcept
	{
		return m_size == 0;
	}

	// swap (note: linear complexity)
	void swap(VipHybridVector<T, Vip::None>& y) noexcept
	{
		std::swap(m_size, y.m_size);
		for (size_type i = 0; i < VIP_MAX_DIMS; ++i)
			std::swap(m_elems[i], y.m_elems[i]);
	}

	// direct access to data (read-only)
	const T* data() const noexcept
	{
		return &m_elems[0];
	}
	T* data() noexcept
	{
		return &m_elems[0];
	}

	// assignment with type conversion
	template<typename T2, qsizetype N>
	VipHybridVector& operator=(const VipHybridVector<T2, N>& rhs) noexcept
	{
		m_size = rhs.size();
		for (qsizetype i = 0; i < m_size; ++i)
			m_elems[i] = static_cast<T>(rhs[i]);
		return *this;
	}

	VipHybridVector& operator=(const VipHybridVector& other) noexcept
	{
		m_size = other.m_size;
		// std::copy(other.m_elems, other.m_elems + VIP_MAX_DIMS, m_elems);
		memcpy(m_elems, other.m_elems, VIP_MAX_DIMS * sizeof(T));
		return *this;
	}

	template<typename T2>
	VipHybridVector& operator=(const QVector<T2>& rhs) noexcept
	{
		fromVector(rhs);
		return *this;
	}

	// assign one value to all elements
	void assign(const T& value) noexcept
	{
		fill(value);
	} // A synonym for fill
	void fill(const T& value) noexcept
	{
		std::fill_n(m_elems, VIP_MAX_DIMS, value);
	}

	void resize(size_type new_size) noexcept
	{
		m_size = new_size;
	}

	void fromVector(const QVector<T>& vec) noexcept
	{
		resize(vec.size());
		for (qsizetype i = 0; i < m_size; ++i)
			m_elems[i] = vec[i];
	}

	QVector<T> toVector() const
	{
		QVector<T> res(size());
		std::copy(begin(), end(), res.begin());
		return res;
	}

	operator QVector<T>() const
	{
		return toVector();
	}

	explicit operator bool() const noexcept
	{
		return size() > 0;
	}

	template<qsizetype Dim2>
	operator VipHybridVector<T, Dim2>() const noexcept
	{
		VipHybridVector<T, Dim2> res;
		std::copy(begin(), end(), res.begin());
		return res;
	}

	template<class T2, qsizetype N2>
	bool operator==(const VipHybridVector<T2, N2>& other) const noexcept
	{
		if (size() != other.size())
			return false;
		return std::equal(begin(), end(), other.begin());
	}

	template<class T2, qsizetype N2>
	bool operator!=(const VipHybridVector<T2, N2>& other) const noexcept
	{
		if (size() != other.size())
			return true;
		return !std::equal(begin(), end(), other.begin());
	}

	VipHybridVector& operator<<(const T& elem) noexcept
	{
		push_back(elem);
		return *this;
	}
};


/// @brief ND coordinate typedef
template<qsizetype NDims>
using VipCoordinate = VipHybridVector<qsizetype, NDims>;

/// @brief VipNDArrayShape is a dynamic VipHybridVector. It is used to represent the shape of a #ipNDArray.
using VipNDArrayShape = VipCoordinate<Vip::None>;


template<qsizetype N1, qsizetype N2>
VipCoordinate< Vip::None> operator+(const VipCoordinate< N1>& v1, const VipCoordinate< N2>& v2) noexcept
{
	VipCoordinate< Vip::None> tmp;
	if (v1.size() >= v2.size()) {
		tmp = v1;
		for (qsizetype i = 0; i < v2.size(); ++i)
			tmp[i] += v2[i];
	}
	else {
		tmp = v2;
		for (qsizetype i = 0; i < v1.size(); ++i)
			tmp[i] += v1[i];
	}
	return tmp;
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

/// @brief Reverse \a vec into \a reverse
template<class T, qsizetype N>
void vipReverse(const VipHybridVector<T, N>& vec, VipHybridVector<T, N>& reverse) noexcept
{
	if (N == 1) {
		reverse[0] = vec[0];
	}
	else if (N == 2) {
		reverse[0] = vec[1];
		reverse[1] = vec[0];
	}
	else if (N == 3) {
		reverse[0] = vec[2];
		reverse[1] = vec[1];
		reverse[2] = vec[0];
	}
	else {
		reverse.resize(vec.size());
		for (qsizetype i = 0; i < vec.size(); ++i)
			reverse[i] = vec[vec.size() - i - 1];
	}
}
/// @brief Return a reversed version of \a vec
template<class T, qsizetype N>
VipHybridVector<T, N> vipReverse(const VipHybridVector<T, N>& vec) noexcept
{
	VipHybridVector<T, N> res;
	vipReverse(vec, res);
	return res;
}

/// @brief Returns a copy of \a v and change its static size. Note that this won't work if N != N2 && N > 0 && N2 > 0.
template<qsizetype N, qsizetype N2>
VipCoordinate<N> vipVector(const VipCoordinate<N2>& v) noexcept
{
	return v;
}
/// @brief Create a dynamic VipHybridVector from a QVector
template<class T>
VipHybridVector<T, Vip::None> vipVector(const QVector<T>& v) noexcept
{
	VipHybridVector<T, Vip::None> res;
	res = v;
	return res;
}

/// @brief Create a VipCoordinate object
template<class... Args >
auto vipVector(Args... sizes) noexcept
{
	return VipCoordinate<sizeof...(Args)>{ { std::forward<qsizetype>(sizes)... } };
}


#include <tuple>
/// https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments
/// MACRO version of vipVector, much faster on msvc (?)
#define vip_vector(...)                                                                                                                                                                                \
	VipCoordinate<std::tuple_size<decltype(std::make_tuple(__VA_ARGS__))>::value>                                                                                                           \
	{                                                                                                                                                                                              \
		__VA_ARGS__                                                                                                                                                                            \
	}

/// @}
// end DataType

#endif
