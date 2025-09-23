#ifndef TH_CONFIG_H
#define TH_CONFIG_H

#include <QtGlobal>

#ifdef VIP_BUILD_LOGGING_LIB
#define VIP_LOGGING_EXPORT Q_DECL_EXPORT
#else
#define VIP_LOGGING_EXPORT Q_DECL_IMPORT
#endif

#ifdef VIP_BUILD_DATA_TYPE_LIB
#define VIP_DATA_TYPE_EXPORT Q_DECL_EXPORT
#else
#define VIP_DATA_TYPE_EXPORT Q_DECL_IMPORT
#endif

#ifdef VIP_BUILD_PLOTTING_LIB
#define VIP_PLOTTING_EXPORT Q_DECL_EXPORT
#else
#define VIP_PLOTTING_EXPORT Q_DECL_IMPORT
#endif

#ifdef VIP_BUILD_CORE_LIB
#define VIP_CORE_EXPORT Q_DECL_EXPORT
#else
#define VIP_CORE_EXPORT Q_DECL_IMPORT
#endif

#ifdef VIP_BUILD_GUI_LIB
#define VIP_GUI_EXPORT Q_DECL_EXPORT
#else
#define VIP_GUI_EXPORT Q_DECL_IMPORT
#endif

#ifdef VIP_BUILD_ANNOTATION_LIB
#define VIP_ANNOTATION_EXPORT Q_DECL_EXPORT
#else
#define VIP_ANNOTATION_EXPORT Q_DECL_IMPORT
#endif


// Try to avoid LOTS of warnings on msvc due to math constants being redefined
#if defined(_MSC_VER)
#define _USE_MATH_DEFINES 1
#include <qmath.h>
#endif

// For unique_ptr
#include <memory>

#include "VipBuildConfig.h"


/// @Enable/disable printf function in debug/release
#define VIP_ENABLE_PRINTF_DEBUG 1
#define VIP_ENABLE_PRINTF_RELEASE 0




namespace vip_log_detail {
	VIP_LOGGING_EXPORT bool _vip_enable_debug();
	VIP_LOGGING_EXPORT void _vip_set_enable_debug(bool);
}

// Print debug information
#define vip_debug(...) { if(vip_log_detail::_vip_enable_debug()) printf(__VA_ARGS__);}



#ifdef _MSC_VER
#pragma warning ( disable : 4127 ) //suppress useless "conditional expression is constant" warning
#pragma warning ( disable : 4505 ) // suppress "unreferenced function with internal linkage has been removed"
#endif


// Suppress deprecated warning with QString::SkipEmptyParts
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#define VIP_SKIP_BEHAVIOR QString
#else
#define VIP_SKIP_BEHAVIOR Qt
#endif


// __MINGW32__ doesn't seem to be properly defined, so define it.
#ifndef __MINGW32__
#if	(defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && defined(__GNUC__) && !defined(__CYGWIN__)
#define __MINGW32__
#endif
#endif

// Unreachable code
#ifdef _MSC_VER
#define VIP_UNREACHABLE() __assume(0)
#else
#define VIP_UNREACHABLE() __builtin_unreachable()
#endif

//pragma directive might be different between compilers, so define a generic VIP_PRAGMA macro.
//Use VIP_PRAGMA with no quotes around argument (ex: VIP_PRAGMA(omp parallel) and not VIP_PRAGMA("omp parallel") ).
#ifdef _MSC_VER
#define _VIP_PRAGMA(text) __pragma(text)
#else
#define _VIP_PRAGMA(text) _Pragma(#text)
#endif

#define VIP_PRAGMA(text) _VIP_PRAGMA(text)


// Forces data to be n-byte aligned (this might be used to satisfy SIMD requirements).
#if (defined __GNUC__) || (defined __PGI) || (defined __IBMCPP__) || (defined __ARMCC_VERSION)
#define VIP_ALIGN_TO_BOUNDARY(n) __attribute__((aligned(n)))
#elif (defined _MSC_VER)
#define VIP_ALIGN_TO_BOUNDARY(n) __declspec(align(n))
#elif (defined __SUNPRO_CC)
// FIXME not sure about this one:
#define VIP_ALIGN_TO_BOUNDARY(n) __attribute__((aligned(n)))
#else
#define VIP_ALIGN_TO_BOUNDARY(n) VIP_USER_ALIGN_TO_BOUNDARY(n)
#endif


// Simple function inlining
#define VIP_INLINE inline

// Strongest available function inlining
#if (defined(__GNUC__) && (__GNUC__>=4)) || defined(__MINGW32__)
#define VIP_ALWAYS_INLINE __attribute__((always_inline)) inline
#elif defined(__GNUC__)
#define VIP_ALWAYS_INLINE  inline
#elif (defined _MSC_VER) || (defined __INTEL_COMPILER)
#define VIP_ALWAYS_INLINE __forceinline
#else
#define VIP_ALWAYS_INLINE inline
#endif



// no inline
#if defined( _MSC_VER) && !defined(__clang__)
#define VIP_NOINLINE(...) __declspec(noinline) __VA_ARGS__
#else
#define VIP_NOINLINE(...) __VA_ARGS__ __attribute__((noinline))
#endif

// fallthrough
#ifndef __has_cpp_attribute 
#    define __has_cpp_attribute(x) 0
#endif
#if __has_cpp_attribute(clang::fallthrough)
#    define VIP_FALLTHROUGH() [[clang::fallthrough]]
#elif __has_cpp_attribute(gnu::fallthrough)
#    define VIP_FALLTHROUGH() [[gnu::fallthrough]]
#else
#    define VIP_FALLTHROUGH()
#endif

// likely/unlikely definition
#if !defined( _MSC_VER) || defined(__clang__)
#define VIP_LIKELY(x)    __builtin_expect (!!(x), 1)
#define VIP_UNLIKELY(x)  __builtin_expect (!!(x), 0)
#define VIP_HAS_EXPECT
#else
#define VIP_LIKELY(x) x
#define VIP_UNLIKELY(x) x
#endif



// assume data are aligned
#if defined(__GNUC__) && (__GNUC__>=4 && __GNUC_MINOR__>=7)
#define VIP_RESTRICT __restrict
#define VIP_ASSUME_ALIGNED(type,ptr,out,alignment) type * VIP_RESTRICT out = (type *)__builtin_assume_aligned((ptr),alignment);
#elif defined(__GNUC__)
#define VIP_RESTRICT __restrict
#define VIP_ASSUME_ALIGNED(type,ptr,out,alignment) type * VIP_RESTRICT out = (ptr);
//on intel compiler, another way is to use #pragma vector aligned before the loop.
#elif defined(__INTEL_COMPILER) || defined(__ICL) || defined(__ICC) || defined(__ECC)
#define VIP_RESTRICT restrict
#define VIP_ASSUME_ALIGNED(type,ptr,out,alignment) type * VIP_RESTRICT out = ptr;__assume_aligned(out,alignment);
#elif defined(__IBMCPP__)
#define VIP_RESTRICT restrict
#define VIP_ASSUME_ALIGNED(type,ptr,out,alignment) type __attribute__((aligned(alignment))) * VIP_RESTRICT out = (type __attribute__((aligned(alignment))) *)(ptr);
#elif defined(_MSC_VER)
#define VIP_RESTRICT __restrict
#define VIP_ASSUME_ALIGNED(type,ptr,out,alignment) type * VIP_RESTRICT out = ptr;
#endif


// Check for C++14-17-20
#if defined(_MSC_VER) && !defined(__clang__)
#if _MSVC_LANG >= 201300L
#define VIP_HAS_CPP_14
#endif
#if _MSVC_LANG >= 201703L
#define VIP_HAS_CPP_17
#endif
#if _MSVC_LANG >= 202002L
#define VIP_HAS_CPP_20
#endif
#else
#if __cplusplus >= 201300L
#define VIP_HAS_CPP_14
#endif
#if __cplusplus >= 201703L
#define VIP_HAS_CPP_17
#endif
#if __cplusplus >= 202002L
#define VIP_HAS_CPP_20
#endif
#endif


// If constexpr
#ifdef VIP_HAS_CPP_17
#define VIP_CONSTEXPR constexpr
#else
#define VIP_CONSTEXPR
#endif


#ifndef VIP_DEBUG
#ifndef NDEBUG
#define VIP_DEBUG
#endif
#endif

// Debug assertion
#ifndef VIP_DEBUG
#define VIP_ASSERT_DEBUG(condition, msg) 
#else
#define VIP_ASSERT_DEBUG(condition, ... )  assert((condition) && (__VA_ARGS__))
#endif


// Support for __has_builtin
#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

// Support for __has_attribute
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif


// Default move copy/construct
#define VIP_DEFAULT_MOVE(Type) \
	Type(const Type& other) = default;\
	Type(Type&& other) noexcept = default;\
	Type& operator=(const Type&) = default;\
	Type& operator=(Type&&) noexcept = default



#define _VIP_VA_NUM_ARGS_HELPER(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

/// @brief Finde number of arguments passed to a macro
#define VIP_VA_NUM_ARGS(...) _VIP_VA_NUM_ARGS_HELPER(__VA_ARGS__, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

// Make a VIP_FOR_EACH macro
#define _VIP_0(WHAT, SEP)
#define _VIP_1(WHAT, SEP, X) WHAT(X)
#define _VIP_2(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_1(WHAT, SEP, __VA_ARGS__)
#define _VIP_3(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_2(WHAT, SEP, __VA_ARGS__)
#define _VIP_4(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_3(WHAT, SEP, __VA_ARGS__)
#define _VIP_5(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_4(WHAT, SEP, __VA_ARGS__)
#define _VIP_6(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_5(WHAT, SEP, __VA_ARGS__)
#define _VIP_7(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_6(WHAT, SEP, __VA_ARGS__)
#define _VIP_8(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_7(WHAT, SEP, __VA_ARGS__)
#define _VIP_9(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_8(WHAT, SEP, __VA_ARGS__)
#define _VIP_10(WHAT, SEP, X, ...) WHAT(X) SEP() _VIP_9(WHAT, SEP, __VA_ARGS__)

#define _VIP_GET_MACRO(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, NAME, ...) NAME


/// @brief Null separator for VIP_FOR_EACH_GENERIC
#define VIP_NULL()

/// @brief Comma separator for VIP_FOR_EACH_GENERIC
#define VIP_COMMA() ,

/// @brief Invoke macro for each argument with SEP separator
#define VIP_FOR_EACH_GENERIC(action, SEP, ...) _VIP_GET_MACRO(_0, __VA_ARGS__, _VIP_10, _VIP_9, _VIP_8, _VIP_7, _VIP_6, _VIP_5, _VIP_4, _VIP_3, _VIP_2, _VIP_1, _VIP_0)(action, SEP, __VA_ARGS__)

/// @brief Invoke macro for each argument with space separator
#define VIP_FOR_EACH(action, ...) VIP_FOR_EACH_GENERIC(action,VIP_NULL, __VA_ARGS_)

/// @brief Invoke macro for each argument with comma separator
#define VIP_FOR_EACH_COMMA(action, ...) VIP_FOR_EACH_GENERIC(action,VIP_COMMA, __VA_ARGS_)

/// @brief String representation of input
#define VIP_STRINGIZE( val ) #val


#include <QMetaType>
#include <QVariant>
#include <QMutex>

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
/// @brief Recursive mutex introduced in Qt5.14.0
class QRecursiveMutex : public QMutex
{
	QRecursiveMutex()
	  : QMutex(QMutex::Recursive)
	{
	}
};

#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

#include <QRegExp>
#include <QRegularExpression>
//using QRegularExpression = QRegExp;

inline QRegExp vipFromWildcard(const QString& pattern, Qt::CaseSensitivity s)
{
/*                                                                                                                                                                                                  \
  #if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
	QString wildcardExp = QRegularExpression::wildcardToRegularExpression(pattern);
	QRegularExpression re(QRegularExpression::anchoredPattern(wildcardExp), s == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
	return re;
#else*/
	return QRegExp(pattern, Qt::CaseInsensitive, QRegExp::Wildcard);
	//#endif
}



// For QVariant::canConvert()
#define VIP_META(meta) meta

// For QMouseEvent::position(), QDropEvent::position()...
#define VIP_EVT_POSITION() pos()

// Declare a type as relocatable
#define VIP_IS_RELOCATABLE(type) Q_DECLARE_TYPEINFO(type, Q_MOVABLE_TYPE)

inline QVariant vipFromVoid(int meta, const void* p)
{
	return QVariant(meta, p);
}

inline int vipIdFromName(const char* name)
{
	return (QMetaType::type(name));
}

inline const char* vipTypeName(int id)
{
	return QMetaType::typeName(id);
}

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
using qsizetype = int;
#endif

#else

#include <QTypeInfo>
#include <QRegExp>
#include <QRecursiveMutex>

inline QRegExp vipFromWildcard(const QString& pattern, Qt::CaseSensitivity s)
{
	/* QString wildcardExp = QRegularExpression::wildcardToRegularExpression(pattern);
	QRegularExpression re(QRegularExpression::anchoredPattern(wildcardExp), s == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
	return re;*/
	return QRegExp(pattern, Qt::CaseInsensitive, QRegExp::Wildcard);
}

// For QVariant::canConvert()
#define VIP_META(meta) QMetaType(meta)

// For QMouseEvent::position(), QDropEvent::position()...
#define VIP_EVT_POSITION() position().toPoint()

// Declare a type as relocatable
#define VIP_IS_RELOCATABLE(type) Q_DECLARE_TYPEINFO(type, Q_RELOCATABLE_TYPE)


inline QVariant vipFromVoid(int meta, const void* p) {
	return QVariant(QMetaType(meta), p);
}

template<class T>
void qRegisterMetaTypeStreamOperators(const char* name = nullptr)
{
	if (name)
		qRegisterMetaType<T>(name);
	else
		qRegisterMetaType<T>();
}

inline int vipIdFromName(const char* name)
{
	return QMetaType::fromName(name).id();
}

inline const char* vipTypeName(int id)
{
	return QMetaType(id).name();
}

#endif




//define this macro to disable multithreading
//#define VIP_DISABLE_MULTI_THREADING

/**
* If OpenMP is enabled and VIP_DISABLE_MULTI_THREADING is not defined, define VIP_ENABLE_MULTI_THREADING
*/
#if defined(_OPENMP) && !defined(VIP_DISABLE_MULTI_THREADING)
#define VIP_ENABLE_MULTI_THREADING
#include <omp.h>

namespace detail
{
	//omp_get_num_threads broken on gcc, use a custom function
	inline size_t _omp_thread_count()
	{
		static size_t n = 0;
		if (!n)
		{
#pragma omp parallel reduction(+:n)
			n += 1;
		}
		return n;
	}
}

inline size_t vipOmpThreadCount()
{
	static size_t count = detail::_omp_thread_count();
	return count;
}

inline size_t vipOmpThreadId()
{
	return omp_get_thread_num();
}
#else
inline size_t vipOmpThreadCount()
{
	return 1;
}
inline size_t vipOmpThreadId()
{
	return 0;
}
#endif


#if defined(__clang__)
// With clang, remove warning inconsistent-missing-override
// until we add override specifier everywhere
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif


#endif
