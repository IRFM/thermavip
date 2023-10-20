#ifndef VIP_FUNCTIONAL_H
#define VIP_FUNCTIONAL_H

#include <functional>
#include <type_traits>

#include <qmetatype.h>
#include <qobject.h>
#include <qvariant.h>
#include <qvector.h>

#include "VipConfig.h"
#include "VipFunctionTraits.h"

/// \addtogroup Core
/// @{

/// \a VipType represents a data type registered to the Qt meta type system.
/// It stores the data type id (based on qMetaTypeId<T>()), its name and its QMetaObject if the type id represents a QObject inheriting class.
struct VipType
{
	int id;
	const char* name;
	const QMetaObject* metaObject;
	VipType()
	  : id(0)
	  , name(nullptr)
	  , metaObject(nullptr)
	{
	}
	VipType(int id, const char* name, const QMetaObject* meta)
	  : id(id)
	  , name(name)
	  , metaObject(meta)
	{
	}
	VipType(int id)
	  : id(id)
	  , name(QMetaType::typeName(id))
	  , metaObject(QMetaType(id).metaObject())
	{
	}

	VipType(const VipType&) = default;
	VipType& operator=(const VipType&) = default;

	inline bool operator==(const VipType& other) const noexcept { return id == other.id || (metaObject == other.metaObject && metaObject); }
	inline bool operator!=(const VipType& other) const noexcept { return !((*this) == other); }
};

/// The \a VipAny class is a kind of variant type used to store any kind of object type declared to the Qt meta type system.
/// The stored object is internally stored in a QVariant.
///
/// \a VipAny provides 2 additional features compared to the QVariant class:
///	- a template constructor, used to create static functions instead of template ones through Implicit Conversion Sequences
///	- the function #VipAny::type() to retrieve the type id, type name and QMetaObject at once.
class VipAny
{
public:
	QVariant variant;

	VipAny() {}
	template<class T>
	VipAny(const T& value)
	  : variant(QVariant::fromValue(value))
	{
	}
	VipAny(VipAny&& other)
	  : variant(std::move(other.variant))
	{
	}
	VipAny& operator=(VipAny&& other)
	{
		variant = std::move(other);
		return *this;
	}

	VipAny(const VipAny&) = default;
	VipAny& operator=(const VipAny&) = default;

	/// @brief Construct from literal string
	template<size_t Size>
	VipAny(char value[Size])
	  : variant(QString(value))
	{
	}
	/// @brief Construct from literal string
	VipAny(const char* value)
	  : variant(QString(value))
	{
	}

	/// @brief Returns the VipType associated to the QVariant's content
	VipType type() const
	{
		if (variant.userType() == 0)
			return VipType();
		const QMetaObject* meta = nullptr;
		const char* name = variant.typeName();
		if (QObject* obj = variant.value<QObject*>())
			meta = obj->metaObject();
		else
			meta = QMetaType(variant.userType()).metaObject();

		return VipType(variant.userType(), name, meta);
	}

	template<class T>
	VipAny& operator=(const T& value)
	{
		variant = QVariant::fromValue(value);
		return *this;
	}

	template<class T>
	T value() const
	{
		return variant.value<T>();
	}

	/// @brief Implicit conversion to QVariant
	operator QVariant() const { return variant; }
	/// @brief Implicit conversion to any type
	template<class T>
	operator T() const
	{
		return variant.value<T>();
	}
};

Q_DECLARE_METATYPE(VipAny)
typedef QVector<VipType> VipTypeList;


namespace details
{

	namespace has_operator_impl
	{
		typedef char no;
		typedef char yes[2];

		no operator<<(QDataStream const&, VipAny const&);
		no operator>>(QDataStream const&, VipAny const&);

		yes& test(QDataStream&);
		no test(no);

		template<typename T>
		struct has_insertion_operator
		{
			static QDataStream& s;
			static T const& t;
			static bool const value = sizeof(test(s << t)) == sizeof(yes);
		};

		template<typename T>
		struct has_extraction_operator
		{
			static QDataStream& s;
			static T& t;
			static bool const value = sizeof(test(s >> t)) == sizeof(yes);
		};
	}

	////////////////////////////////////////////////////////////////////////////////
	// Few helper type traits
	////////////////////////////////////////////////////////////////////////////////

	template<typename T>
	struct has_insertion_operator : has_operator_impl::has_insertion_operator<T>
	{
	};

	template<typename T>
	struct has_extraction_operator : has_operator_impl::has_extraction_operator<T>
	{
	};

	template<class T>
	struct remove_const_pointer
	{
		using type = typename std::remove_const<typename std::remove_pointer<T>::type>::type;
	};

	template<class T>
	struct remove_const_reference
	{
		using type = typename std::remove_const<typename std::remove_reference<T>::type>::type;
	};

	template<class T>
	class inherit_qobject
	{
		typedef typename remove_const_pointer<T>::type type;

		struct LargerThanChar
		{
			int x;
			int y;
		};
		struct Temp
		{
			static LargerThanChar isBaseClass(void*);
			static char isBaseClass(QObject*);
		};

	public:
		enum
		{
			value = sizeof(Temp::isBaseClass((type*)0)) == sizeof(char)
		};
	};


	// handle object create, taking care of abstract classes

	template<class T, bool IsAbstract>
	struct CreateObject
	{
		static T* create() { return new T(); }
	};

	template<class T>
	struct CreateObject<T, true>
	{
		static T* create() { return nullptr; }
	};

	template<class T>
	T* create()
	{
		return CreateObject<T, std::is_abstract<T>::value>::create();
	}

	template<class T>
	QVariant createVariant()
	{
		return QVariant::fromValue(create<T>());
	}

	////////////////////////////////////////////////////////////////////////////////
	// automatic registration of insertion/extraction operators
	////////////////////////////////////////////////////////////////////////////////

	template<bool value>
	struct RegisterMetaTypeStreamOperators
	{
		template<class T>
		static void apply(const char* typeName)
		{
			qRegisterMetaTypeStreamOperators<T>(typeName);
		}
	};

	template<>
	struct RegisterMetaTypeStreamOperators<false>
	{
		template<class T>
		static void apply(const char*)
		{
		}
	};

	template<class T>
	void registerMetaTypeStreamOperators(const char* typeName)
	{
		RegisterMetaTypeStreamOperators<has_insertion_operator<T>::value && has_extraction_operator<T>::value>::template apply<T>(typeName);
	}

	typedef QVariant (*create_fun)();
	VIP_CORE_EXPORT void registerCreateVariant(int metatype, create_fun);

	template<class T>
	void registerTypeCreate()
	{
		typedef typename remove_const_pointer<T>::type type;
		registerCreateVariant(qMetaTypeId<T>(), createVariant<type>);
	}

	template<class T, class = void>
	struct MetaTypeQObject
	{
		enum
		{
			Defined = QMetaTypeId<T>::Defined
		};
	};

	template<class T>
	struct MetaTypeQObject<T, typename std::enable_if<inherit_qobject<T>::value, void>::Type>
	{
		enum
		{
			Defined = 0
		};
	};

	template<class T>
	struct MetaType : public MetaTypeQObject<T>
	{
	};

	template<class Type>
	inline void registerQObjectType(const char* name)
	{
		Q_STATIC_ASSERT_X(QtPrivate::HasQ_OBJECT_Macro<typename remove_const_pointer<Type>::type>::Value, "No Q_OBJECT in class");
		static QBasicAtomicInt reg = Q_BASIC_ATOMIC_INITIALIZER(0);
		if (reg.loadAcquire())
			return;
		else {
			reg.storeRelease(1);
			registerTypeCreate<Type>();
			registerMetaTypeStreamOperators<Type>(QMetaType::typeName(qRegisterMetaType<Type>()));
			// register alias
			qRegisterMetaType<Type>(name);
		}
	}

} // end details

#define _VIP_CONCAT_IMPL(x, y) x##y
#define _VIP_MACRO_CONCAT(x, y) _VIP_CONCAT_IMPL(x, y)

/// Register a QObject inheriting class to the Thermavip type system. The class must be registered first with Q_DECLARE_METATYPE.
/// Use #VIP_REGISTER_QOBJECT_METATYPE if it is not the case.
/// This macro does 3 things:
/// - Automatically register QDataStream stream operators for given QObject inheriting class if they exist
/// - Register to the meta type system the QObject inheriting class with its true class name (in case
/// Q_DECLARE_METATYPE() has been called with a typedef)
/// - Make sure that a QVariant created with this class id contains a valid non null pointer.
///
/// A type registered with this macro changes the behavior of QVariant for this type: a QVariant created with the constructor QVariant(type_id,nullptr) holds a VALID pointer instead of a nullptr one.
/// This way, QVariant can be used as a factory for types declared with VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE. It also means that the underlying QObject must be destroyed after the QVariant is
/// destroyed itself using vipReleaseVariant().
#define VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE(Type)                                                                                                                                                 \
	namespace details                                                                                                                                                                              \
	{                                                                                                                                                                                              \
		template<>                                                                                                                                                                             \
		struct MetaType<Type>                                                                                                                                                                  \
		{                                                                                                                                                                                      \
			MetaType()                                                                                                                                                                     \
			{                                                                                                                                                                              \
				Q_STATIC_ASSERT_X(QtPrivate::HasQ_OBJECT_Macro<typename remove_const_pointer<Type>::type>::Value, "No Q_OBJECT in the class " #Type);                                  \
				registerQObjectType<Type>(#Type);                                                                                                                                      \
			}                                                                                                                                                                              \
                                                                                                                                                                                                       \
			enum                                                                                                                                                                           \
			{                                                                                                                                                                              \
				Defined = 1                                                                                                                                                            \
			};                                                                                                                                                                             \
		};                                                                                                                                                                                     \
		static const MetaType<Type> _VIP_MACRO_CONCAT(meta_type_, __COUNTER__);                                                                                                                \
	}                                                                                                                                                                                              \
	template<>                                                                                                                                                                                     \
	inline QVariant QVariant::fromValue<Type>(Type const& value)                                                                                                                                   \
	{                                                                                                                                                                                              \
		/* make sure to take the highest metatype*/                                                                                                                                            \
		int id = 0;                                                                                                                                                                            \
		if (value)                                                                                                                                                                             \
			id = QMetaType::type(QByteArray(value->metaObject()->className()) + "*");                                                                                                      \
		if (id)                                                                                                                                                                                \
			return QVariant(id, &value);                                                                                                                                                   \
		return QVariant(qMetaTypeId<Type>(), &value);                                                                                                                                          \
	}                                                                                                                                                                                              \
	template<>                                                                                                                                                                                     \
	inline QVariant qVariantFromValue(Type const& value)                                                                                                                                           \
	{                                                                                                                                                                                              \
		return QVariant::fromValue(value);                                                                                                                                                     \
	}

/// @brief Same as VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE, but also add the Q_DECLARE_METATYPE declaration
#define VIP_REGISTER_QOBJECT_METATYPE(Type)                                                                                                                                                            \
	Q_DECLARE_METATYPE(Type)                                                                                                                                                                       \
	VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE(Type)

template<>
inline QVariant qVariantFromValue(const VipAny& any)
{
	return any.variant;
}

template<>
inline void qVariantSetValue(QVariant& v, const VipAny& any)
{
	v = any.variant;
}

template<>
inline VipAny qvariant_cast(const QVariant& v)
{
	return VipAny(v);
}

/// If a QVariant holds a valid QObject, delete it
VIP_CORE_EXPORT void vipReleaseVariant(const QVariant& v);

/// Check if type \a type_from is convertible to type \a type_to.
/// This is done by checking if the types are convertible with QVariant::canConvert() function,
/// but also by using (in case of QObject inheriting classes) an equivalent of \a qobject_cast function.
VIP_ALWAYS_INLINE bool vipIsConvertible(const VipType& type_from, const VipType& type_to)
{
	if (type_from == type_to || type_to.id == 0)
		return true;

	// bug with QVariantMap: any map seems to be convertible to a QVariantMap. So disable this behavior.
	if (type_to == QMetaType::QVariantMap)
		return false;

	if (const QMetaObject* meta_from = type_from.metaObject) {
		if (const QMetaObject* meta_to = type_to.metaObject) {
			// check id conversion id possible
			const QMetaObject* meta = meta_from;
			while (meta) {
				if (meta == meta_to)
					return true;
				meta = meta->superClass();
			}
			return false;
		}

		return false;
	}

	QVariant variant(type_from.id, nullptr);
	bool res = variant.canConvert(type_to.id);
	vipReleaseVariant(variant);
	return res;
}

/// @brief Returns a QVariant constructed with given \a id.
/// If \a id represents a type declared with #VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE or #VIP_REGISTER_QOBJECT_METATYPE,
/// the returned QVariant will hold a VALID pointer to QObject. If you don't use the QObject, call #vipReleaseVariant to delete it
VIP_CORE_EXPORT QVariant vipCreateVariant(int id);

/// @brief Returns the type id for given QObject pointer.
/// Note that this function returns the highest possible type id based on object's QMetaObject.
template<class T>
VIP_ALWAYS_INLINE int vipMetaTypeFromQObject(const T* obj)
{
	int id = 0;
	if (obj)
		id = QMetaType::type(QByteArray(obj->metaObject()->className()) + "*");
	if (id)
		return id;
	return qMetaTypeId<T*>();
}

/// @brief Converts a pointer to QObject to QVariant.
/// Note that this function returns the highest possible type id based on object's QMetaObject.
template<class T>
VIP_ALWAYS_INLINE QVariant vipVariantFromQObject(const T* obj)
{
	int id = vipMetaTypeFromQObject(obj);
	return QVariant(id, &obj);
}

namespace details
{
	template<class T, bool IsQObject = inherit_qobject<T>::value>
	struct ToVariant
	{
		static VIP_ALWAYS_INLINE QVariant apply(const T& value) { return QVariant::fromValue(value); }
	};
	template<class T>
	struct ToVariant<T, true>
	{
		static VIP_ALWAYS_INLINE QVariant apply(const T& value) { return ::vipVariantFromQObject(value); }
	};
}

/// @brief Convert given object to QVariant.
/// This is equivalent to QVariant::fromValue(), except for QObject inherting objects
/// where this function tries to create a variant with highest possible metatype ID.
template<class T>
QVariant vipToVariant(const T& value)
{
	return details::ToVariant<T>::apply(value);
}
template<>
VIP_ALWAYS_INLINE QVariant QVariant::fromValue<QObject*>(QObject* const& value)
{
	return vipToVariant(value);
}
class QWidget;
template<>
VIP_ALWAYS_INLINE QVariant QVariant::fromValue<QWidget*>(QWidget* const& value)
{
	return vipToVariant(value);
}

/// @brief Returns a QVariant constructed with given type name.
/// \sa vipCreateVariant(int id)
VIP_ALWAYS_INLINE QVariant vipCreateVariant(const char* name)
{
	return vipCreateVariant(QMetaType::type(name));
}

/// @brief Returns a QVariant constructed with given \a id.
/// If \a id represents a type declared with #VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE or #VIP_REGISTER_QOBJECT_METATYPE,
/// the returned QVariant will still hold a nullptr pointer to QObject.
VIP_ALWAYS_INLINE QVariant vipCreateNullVariant(int id)
{
	if (QMetaType(id).flags() & QMetaType::PointerToQObject) {
		QObject* ptr1 = nullptr;
		QObject** ptr2 = &ptr1;
		return QVariant(id, ptr2);
	}
	else
		return QVariant(id, nullptr);
}

/// @brief Returns a QVariant constructed with given type name.
/// If the type name represents a type declared with #VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE or #VIP_REGISTER_QOBJECT_METATYPE,
/// the returned QVariant will still hold a nullptr pointer to QObject.
VIP_ALWAYS_INLINE QVariant vipCreateNullVariant(const char* name)
{
	return vipCreateNullVariant(QMetaType::type(name));
}

/// @brief Returns the QMetaObject associated to given class name (must end with '*')
VIP_ALWAYS_INLINE const QMetaObject* vipMetaObjectFromName(const char* name)
{
	int id = QMetaType::type(name);
	if (id)
		return QMetaType(id).metaObject();
	return nullptr;
}

/// @brief Returns all user types (id >= QMetaType::User)
inline QList<int> vipUserTypes()
{
	QList<int> types;
	int id = QMetaType::User;

	while (id) {
		if (QMetaType::isRegistered(id)) {
			types << id;
			++id;
		}
		else {
			id = 0;
		}
	}

	return types;
}

/// @brief Returns all user types (id >= QMetaType::User) that are convertible to given type.
VIP_CORE_EXPORT QList<int> vipUserTypes(int type);

/// @brief Returns all user types (id >= QMetaType::User) that are convertible to given template type.
template<class T>
VIP_ALWAYS_INLINE QList<int> vipUserTypes()
{
	return vipUserTypes(qMetaTypeId<T>());
}




namespace details
{

	/// @brief Build std::function type by repeating N times VipAny
	/// @tparam ...Args
	template<size_t N, class... Args>
	struct FType
	{
		using type = typename FType<N - 1, VipAny, Args...>::type;
	};
	template<class... Args>
	struct FType<0, Args...>
	{
		using type = std::function<VipAny(Args...)>;
	};

	// build a VipTypeList from a function signature
	template<class Signature, size_t NArgs = VipFunctionTraits<Signature>::nargs>
	struct BuildTypeListFromSignature
	{
		using traits = VipFunctionTraits<Signature>;
		using arg_type = typename traits::template element<NArgs>::type;
		using valid_arg_type = typename remove_const_reference<arg_type>::type;//typename std::remove_reference<typename std::remove_const<arg_type>::type>::type;

		static void update(VipTypeList& lst)
		{
			const int id = (qMetaTypeId<valid_arg_type>());
			if (std::is_same<valid_arg_type, QVariant>::value || std::is_same<valid_arg_type, VipAny>::value)
				lst[NArgs] = VipType(0, nullptr, nullptr); // QVariant case
			else
				lst[NArgs] = VipType(id, QMetaType::typeName(id), QMetaType(id).metaObject());
			BuildTypeListFromSignature<Signature, NArgs - 1>::update(lst);
		}
	};

	template<class Signature>
	struct BuildTypeListFromSignature<Signature, (size_t)-1>
	{
		static void update(VipTypeList&) {}
	};

	template<class Signature>
	VipTypeList buildTypeListFromSignature()
	{
		VipTypeList lst(VipFunctionTraits<Signature>::nargs);
		BuildTypeListFromSignature<Signature, VipFunctionTraits<Signature>::nargs - 1>::update(lst);
		return lst;
	}

	// build a VipTypeList from a function signature
	struct BuildTypeListFromObjects
	{
		template<class A, class... Args>
		static void update(VipTypeList& lst, int index, const A& a, Args&&... args)
		{
			lst[index] = VipAny(a).type();
			update(lst, index + 1, std::forward<Args>(args)...);
		}
		static void update(VipTypeList&, int) {}
	};

	template<class... Args>
	VipTypeList buildTypeListFromObjects(Args&&... args)
	{
		VipTypeList lst(sizeof...(Args));
		BuildTypeListFromObjects::update(lst, 0, std::forward<Args>(args)...);
		return lst;
	}

	/// @brief Callable wrapper that returns a VipAny
	template<class Callable, class Signature = Callable, class Ret = typename VipFunctionTraits<Signature>::return_type>
	struct CallableWrapper
	{
		using call_type = typename std::decay<Callable>::type;
		call_type c;

		static VIP_ALWAYS_INLINE VipType returnType() { return VipType(qMetaTypeId<Ret>()); }

		template<class... Args>
		VIP_ALWAYS_INLINE VipAny operator()(Args&&... args) const
		{
			return const_cast<call_type&>(c)(std::forward<Args>(args)...);
		}
	};
	template<class Callable, class Signature>
	struct CallableWrapper<Callable, Signature, void>
	{
		using call_type = typename std::decay<Callable>::type;
		call_type c;

		static VIP_ALWAYS_INLINE VipType returnType() { return VipType(qMetaTypeId<void>()); }

		template<class... Args>
		VIP_ALWAYS_INLINE VipAny operator()(Args&&... args) const
		{
			const_cast<call_type&>(c)(std::forward<Args>(args)...);
			return VipAny();
		}
	};

	VIP_ALWAYS_INLINE bool nonConvertible(const VipTypeList& t1, const VipTypeList& t2)
	{
		int count = std::min(t1.size(), t2.size());
		for (int i = 0; i < count; ++i) {
			if (!vipIsConvertible(t1[i], t2[i]))
				return true;
		}
		return false;
	}

	VIP_ALWAYS_INLINE bool exactEqual(const VipTypeList& t1, const VipTypeList& t2)
	{
		int count = std::min(t1.size(), t2.size());
		for (int i = 0; i < count; ++i) {
			const VipType& _t1 = t1[i];
			const VipType& _t2 = t2[i];
			if (_t1.metaObject || _t2.metaObject) {
				// compare meta objects instead of ids
				if (_t1.metaObject == _t2.metaObject)
					continue;
				else
					return false;
			}
			else if (_t1.id != _t2.id)
				return false;
		}
		return true;
	}
}

/// @brief VipFunction class is very similar to std::function, except it is limited to up to 5 arguments and is not a template class.
/// Instead, the argument are passed as VipAny objects, making possible runtime checking of the arguments.
/// The return type is also a VipAny object.
///
/// Usually, std::function SHOULD be preffered to VipFunction, except if you need runtime query of argument/result types.
/// VipFunction is mainly meant to be used with VipFunctionDispatcher.
///
template<size_t NArgs>
class VipFunction
{
	typename details::FType<NArgs>::type m_function;
	VipTypeList m_typeList;
	VipType m_returnType;

public:
	using function_type = typename details::FType<NArgs>::type;

	VipFunction(){};

	template<class Callable>
	VipFunction(Callable&& c, typename std::enable_if<!std::is_same<typename std::decay<Callable>::type, VipFunction>::value, void>::type* = 0)
	  : m_function(details::CallableWrapper<Callable>{ c })
	  , m_typeList(details::buildTypeListFromSignature<Callable>())
	  , m_returnType(details::CallableWrapper<Callable>::returnType())
	{
		static_assert(VipFunctionTraits<Callable>::nargs == NArgs, "invalid number of arguments!");
	}

	template<class Callable, class Signature>
	VipFunction(Callable&& c, Signature* )
	  : m_function(details::CallableWrapper<Callable, Signature>{ c })
	  , m_typeList(details::buildTypeListFromSignature<Signature>())
	  , m_returnType(details::CallableWrapper<Signature>::returnType())
	{
		static_assert(VipFunctionTraits<Signature>::nargs == NArgs, "invalid number of arguments!");
	}

	VipFunction(const VipFunction&) = default;
	VipFunction(VipFunction&&) = default;
	VipFunction& operator=(const VipFunction&) = default;
	VipFunction& operator=(VipFunction&&) = default;

	template<class... Args>
	VipAny operator()(Args&&... args) const
	{
		static_assert((sizeof...(Args)) == NArgs, "invalid number of arguments!");
		return m_function(std::forward<Args>(args)...);
	}

	/// @brief Returns true if the VipFunction is valid (contains a valid FunctionType)
	bool isValid() const noexcept { return m_function; }
	bool isNull() const noexcept { return !isValid(); }

	/// @brief Returns the underlying std::function object
	const function_type& function() const noexcept { return m_function; }
	/// @brief Returns the arguments types as a \a VipTypeList
	const VipTypeList& typeList() const noexcept { return m_typeList; }
	/// @brief Returns the return type
	const VipType& returnType() const noexcept { return m_returnType; }

	/// @brief Comparison operator.
	/// Two VipFunction objects are considered equals if they have the same argument and return types.
	bool operator==(const VipFunction& other) const noexcept { return m_typeList == other.m_typeList && m_returnType == other.m_returnType; }
	/// Comparison operator.
	/// Two VipFunction objects are considered equals if they have the same argument and return types.
	bool operator!=(const VipFunction& other) const noexcept { return !((*this) == other); }
};

/// @brief VipFunctionDispatcher is a list of VipFunction.
///
/// Its purpose is to pick the right function according to the argument types. Thus, it provides
/// a different and extendable approach for polymorphism based on Qt metatype system.
///
/// Usage:
///
/// \code
/// inline void print_int(int value) { std::cout<<"this is an integer: "<<value<<std::endl;}
/// inline void print_double(double value) { std::cout<<"this is a double: "<<value<<std::endl;}
///
/// VipFunctionDispatcher<1> disp;
/// disp.append<void(int)>(print_int);
/// disp.append<void(double)>(print_double);
///
/// disp.callAllExactMatch(2); //call print_int(2)
/// disp.callAllExactMatch(2.3); //call print_double(2.3)
/// disp.callAllMatch(2.3); //call both print_int(2) and print_double(2.3)
/// \end_code
///
/// When dealing with object pointers, VipFunctionDispatcher will only work on QObject inheriting classes.
/// Here is an example on how to extend a class behavior with VipFunctionDispatcher.
/// \code
/// struct Base :  QObject
/// {
///	Q_OBJECT
///
///	virtual const char* identifier() {return "I am a Base";}
/// };
///
/// struct Child : Base
/// {
///	Q_OBJECT
///
///	virtual const char* identifier() {return "I am a Child";}
/// };
///
/// //I want to extend the functionalities of Base and Child because know I need a sub identifier
///
/// static QString Base_identifier(Base *) {return "And also a QObject";}
/// static QString Child_identifier(Child *) {return "And also a Base and a QObject";}
///
/// VipFunctionDispatcher<1> sub_identifier;
/// sub_identifier.append<QString(Base*)>(Base_identifier);
/// sub_identifier.append<QString(Child*)>(Child_identifier);
///
/// //Test the sub_identifier on a Base pointers
///
/// Base * b1 = new Base();
/// Base * b2 = new Child();
///
/// std::cout<<b1->identifier() <<" "<< sub_identifier(b1).value<QByteArray>().data() <<std::endl; // "I am a Base And also a QObject"
/// std::cout<<b2->identifier() <<" "<<  sub_identifier(b2).value<QByteArray>().data() <<std::endl; // "I am a Child And also a Base and a QObject"
///
/// \endcode
///
/// VipFunctionDispatcher provides ways to retrieve the functions that match exactly given arguments, but also the ones that can accept these arguments through implicit conversion.
///
template<size_t NArgs>
class VipFunctionDispatcher
{
	QVector<VipFunction<NArgs>> m_functions;

public:
	using function_type = VipFunction<NArgs>;
	using function_list_type = QVector<function_type>;
	using result_type = VipAny;

	/// @brief Construct a VipFunctionDispatcher with given arity
	VipFunctionDispatcher() {}
	VipFunctionDispatcher(const VipFunctionDispatcher&) = default;
	VipFunctionDispatcher(VipFunctionDispatcher&&) = default;
	VipFunctionDispatcher& operator=(const VipFunctionDispatcher&) = default;
	VipFunctionDispatcher& operator=(VipFunctionDispatcher&&) = default;

	/// @brief Returns true if the dispatcher is valid (non 0 arity)
	constexpr bool isValid() const noexcept { return NArgs != 0; }

	/// @brief Returns all the functions stored in this VipFunctionDispatcher
	const function_list_type& functions() const noexcept { return m_functions; }
	/// @brief Returns the function dispatcher arity
	constexpr int nargs() const noexcept { return NArgs; }
	/// @brief Returns the number of functions stored in this VipFunctionDispatcher
	int count() const noexcept { return m_functions.size(); }

	/// @brief Returns all functions that match given argument types.
	/// This includes the functions that match exactly given argument types, but also the ones that can accept these arguments through implicit conversion.
	function_list_type match(const VipTypeList& lst) const
	{
		if (lst.size() > NArgs)
			return function_list_type();
		function_list_type res;
		for (const function_type& f : m_functions) {
			if (!details::nonConvertible(lst, f.typeList()))
				res.push_back(f);
		}
		return res;
	}
	function_list_type match(VipTypeList& lst) const { return match(static_cast<const VipTypeList&>(lst)); }
	/// @brief Returns all functions that match given argument types.
	/// This includes the functions that match exactly given argument types, but also the ones that can accept these arguments through implicit conversion.
	template<class... Args>
	function_list_type match(Args&&... args) const
	{
		static_assert((sizeof...(Args)) <= NArgs, "invalid number of arguments!");
		return match(static_cast<const VipTypeList&>(details::buildTypeListFromObjects(std::forward<Args>(args)...)));
	}

	/// @brief Returns all functions that match exactly given argument types.
	function_list_type exactMatch(const VipTypeList& lst) const
	{
		if (lst.size() > NArgs)
			return function_list_type();
		function_list_type res;
		for (const function_type& f : m_functions) {
			if (details::exactEqual(lst, f.typeList()))
				res.push_back(f);
		}
		return res;
	}
	function_list_type exactMatch(VipTypeList& lst) const { return exactMatch(static_cast<const VipTypeList&>(lst)); }
	/// @brief Returns all functions that match exactly given argument types.
	template<class... Args>
	function_list_type exactMatch(Args&&... args) const
	{
		static_assert((sizeof...(Args)) <= NArgs, "invalid number of arguments!");
		return exactMatch(static_cast<const VipTypeList&>(details::buildTypeListFromObjects(std::forward<Args>(args)...)));
	}

	/// @brief Call the first found function that match given arguments, and return the result
	template<class... Args>
	VipAny callOneMatch(Args&&... args) const
	{
		function_list_type lst = match(std::forward<Args>(args)...);
		if (lst.size())
			return lst.back()(std::forward<Args>(args)...);
		return VipAny();
	}
	/// @brief Call the first found function that match exactly given arguments, and return the result
	template<class... Args>
	VipAny callOneExactMatch(Args&&... args) const
	{
		function_list_type lst = exactMatch(std::forward<Args>(args)...);
		if (lst.size())
			return lst.back()(std::forward<Args>(args)...);
		return VipAny();
	}

	/// @brief Call all the functions that match given arguments, and return the results
	template<class... Args>
	QVector<VipAny> callAllMatch(Args&&... args) const
	{
		const function_list_type lst = match(std::forward<Args>(args)...);
		QVector<VipAny> res;
		res.reserve(lst.size());
		for (int i = 0; i < lst.size(); ++i)
			res.push_back(lst[i](std::forward<Args>(args)...));
		return res;
	}
	/// @brief Call all the functions that match exactly given arguments, and return the results
	template<class... Args>
	QVector<VipAny> callAllExactMatch(Args&&... args) const
	{
		const function_list_type lst = exactMatch(std::forward<Args>(args)...);
		QVector<VipAny> res;
		res.reserve(lst.size());
		for (int i = 0; i < lst.size(); ++i)
			res.push_back(lst[i](std::forward<Args>(args)...));
		return res;
	}

	/// @brief Equivalent to callOneExactMatch()
	template<class... Args>
	VipAny operator()(Args&&... args) const
	{
		return callOneExactMatch(std::forward<Args>(args)...);
	}

	/// @brief Add a new function
	void append(const function_type& fun) { m_functions.append(fun); }
	/// @brief Add new functions
	void append(const function_list_type& funs) { m_functions.append(funs); }
	/// Add a callable object
	template<class Signature, class Callable>
	void append(const Callable& c)
	{
		append(function_type(c, (Signature*)nullptr));
	}

	/// @brief Remove all functions having the same signature as \a fun
	void remove(const function_type& fun) { m_functions.removeAll(fun); }
	/// @brief Remove all functions having the same signature as \a lst
	void remove(const function_list_type& lst)
	{
		for (int i = 0; i < lst.size(); ++i)
			remove(lst[i]);
	}
	/// @brief Remove all functions
	void clear() { m_functions.clear(); }
};



/// @}
// end Core

#endif
