#ifndef VIP_FUNCTIONAL_H
#define VIP_FUNCTIONAL_H

#include <functional>
#include <type_traits>

#include <qmetatype.h>
#include <qobject.h>
#include <qvariant.h>
#include <qvector.h>

#include "VipConfig.h"

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

	bool operator==(const VipType& other) const { return id == other.id || (metaObject == other.metaObject && metaObject); }
	bool operator!=(const VipType& other) const { return !((*this) == other); }
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
typedef std::function<VipAny(const VipAny&, const VipAny&, const VipAny&, const VipAny&, const VipAny&)> FunctionType;

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

	template<typename T>
	struct base_function_traits;

#define FUNCTION_TRAITS(arity, ...)                                                                                                                                                                    \
	struct base_function_traits<std::function<R(__VA_ARGS__)>>                                                                                                                                     \
	{                                                                                                                                                                                              \
		static const size_t nargs = arity;                                                                                                                                                     \
		typedef R result_type;                                                                                                                                                                 \
		template<size_t i>                                                                                                                                                                     \
		struct arg                                                                                                                                                                             \
		{                                                                                                                                                                                      \
			typedef typename std::tuple_element<i, std::tuple<__VA_ARGS__>>::type type;                                                                                                    \
		};                                                                                                                                                                                     \
	};

	template<typename R>
	FUNCTION_TRAITS(0)
	template<typename R, typename A0>
	FUNCTION_TRAITS(1, A0)
	template<typename R, typename A0, typename A1>
	FUNCTION_TRAITS(2, A0, A1)
	template<typename R, typename A0, typename A1, typename A2>
	FUNCTION_TRAITS(3, A0, A1, A2)
	template<typename R, typename A0, typename A1, typename A2, typename A3>
	FUNCTION_TRAITS(4, A0, A1, A2, A3)
	template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4>
	FUNCTION_TRAITS(5, A0, A1, A2, A3, A4)
	template<typename R, typename A0, typename A1, typename A2, typename A3, typename A4, typename A5>
	FUNCTION_TRAITS(5, A0, A1, A2, A3, A4, A5)

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

	template<class T>
	struct function_traits : details::base_function_traits<std::function<T>>
	{
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
/// A type registered with this macro changes the behavior of QVariant for this type: a QVariant created with the constructor QVariant(type_id,NULL) holds a VALID pointer instead of a NULL one.
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
inline bool vipIsConvertible(const VipType& type_from, const VipType& type_to)
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
inline int vipMetaTypeFromQObject(const T* obj)
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
inline QVariant vipVariantFromQObject(const T* obj)
{
	int id = vipMetaTypeFromQObject(obj);
	return QVariant(id, &obj);
}

namespace details
{
	template<class T, bool IsQObject = inherit_qobject<T>::value>
	struct ToVariant
	{
		static QVariant apply(const T& value) { return QVariant::fromValue(value); }
	};
	template<class T>
	struct ToVariant<T, true>
	{
		static QVariant apply(const T& value) { return ::vipVariantFromQObject(value); }
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
inline QVariant QVariant::fromValue<QObject*>(QObject* const& value)
{
	return vipToVariant(value);
}
class QWidget;
template<>
inline QVariant QVariant::fromValue<QWidget*>(QWidget* const& value)
{
	return vipToVariant(value);
}

/// @brief Returns a QVariant constructed with given type name.
/// \sa vipCreateVariant(int id)
inline QVariant vipCreateVariant(const char* name)
{
	return vipCreateVariant(QMetaType::type(name));
}

/// @brief Returns a QVariant constructed with given \a id.
/// If \a id represents a type declared with #VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE or #VIP_REGISTER_QOBJECT_METATYPE,
/// the returned QVariant will still hold a NULL pointer to QObject.
inline QVariant vipCreateNullVariant(int id)
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
/// the returned QVariant will still hold a NULL pointer to QObject.
inline QVariant vipCreateNullVariant(const char* name)
{
	return vipCreateNullVariant(QMetaType::type(name));
}

/// @brief Returns the QMetaObject associated to given class name (must end with '*')
inline const QMetaObject* vipMetaObjectFromName(const char* name)
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
QList<int> vipUserTypes()
{
	return vipUserTypes(qMetaTypeId<T>());
}

namespace details
{

	// build a VipTypeList from a function signature
	template<class Signature, int Arity>
	struct BuildTypeList
	{
		static void update(VipTypeList& lst)
		{
			typedef typename function_traits<Signature>::template arg<Arity>::type type;
			typedef typename remove_const_reference<type>::type new_type;
			int id = (qMetaTypeId<new_type>());
			if (std::is_same<new_type, QVariant>::value)
				lst[Arity] = VipType(0, nullptr, nullptr); // QVariant case
			else
				lst[Arity] = VipType(id, QMetaType::typeName(id), QMetaType(id).metaObject());
			BuildTypeList<Signature, Arity - 1>::update(lst);
		}
	};

	template<class Signature>
	struct BuildTypeList<Signature, -1>
	{
		static void update(VipTypeList&) {}
	};

	template<class Signature>
	VipTypeList buildTypeList()
	{
		VipTypeList lst(function_traits<Signature>::nargs);
		BuildTypeList<Signature, (int)function_traits<Signature>::nargs - 1>::update(lst);
		return lst;
	}

	template<class Signature>
	VipType buildReturnType()
	{
		int id = qMetaTypeId<typename function_traits<Signature>::result_type>();
		return VipType(id, QMetaType::typeName(id), QMetaType(id).metaObject());
	}

	// build a FunctionType from any callable object and its signature

	template<class Callable, class Ret>
	struct Wrap
	{
		Callable callable;
		Wrap(const Callable& c)
		  : callable(c)
		{
		}
		VipAny operator()(const VipAny& v1, const VipAny& v2, const VipAny& v3, const VipAny& v4, const VipAny& v5) { return callable(v1, v2, v3, v4, v5); }
	};

	template<class Callable>
	struct Wrap<Callable, void>
	{
		Callable callable;
		Wrap(const Callable& c)
		  : callable(c)
		{
		}
		VipAny operator()(const VipAny& v1, const VipAny& v2, const VipAny& v3, const VipAny& v4, const VipAny& v5)
		{
			callable(v1, v2, v3, v4, v5);
			return VipAny();
		}
	};

	template<class BindCallable>
	FunctionType createFunction(BindCallable c)
	{
		typedef typename BindCallable::result_type result_type;
		return Wrap<BindCallable, result_type>(c);
	}

	template<class Callable, int Arity>
	struct BuildBind
	{
		static FunctionType create(Callable c) { return createFunction(std::bind(c)); }
	};

	using namespace std::placeholders;
#define BUILD_BIND(arity, ...)                                                                                                                                                                         \
                                                                                                                                                                                                       \
	template<class Callable>                                                                                                                                                                       \
	struct BuildBind<Callable, arity>                                                                                                                                                              \
	{                                                                                                                                                                                              \
		static FunctionType create(Callable c) { return createFunction(std::bind(c, __VA_ARGS__)); }                                                                                           \
	};

	BUILD_BIND(1, _1)
	BUILD_BIND(2, _1, _2)
	BUILD_BIND(3, _1, _2, _3)
	BUILD_BIND(4, _1, _2, _3, _4)
	BUILD_BIND(5, _1, _2, _3, _4, _5)

	template<class Signature, class Callable>
	FunctionType create(Callable c)
	{
		return BuildBind<Callable, function_traits<Signature>::nargs>::create(c);
	}

} // end details

/// @brief VipFunction class is very similar to std::function, except it is limited to up to 5 arguments and is not a template class.
/// Instead, the argument are passed as VipAny objects, making possible runtime checking of the arguments.
/// The return type is also a VipAny object.
///
/// Usually, std::function SHOULD be preffered to VipFunction, except if you need runtime query of argument/result types.
/// VipFunction is mainly meant to be used with VipFunctionDispatcher.
///
class VipFunction
{
	FunctionType m_fun;
	VipTypeList m_typeList;
	VipType m_returnType;

public:
	using result_type = VipAny;

	/// @brief Construct from a FunctionType, a VipTypeList and result type
	VipFunction(const FunctionType& fun = FunctionType(), const VipTypeList& typelist = VipTypeList(), const VipType& ret = VipType())
	  : m_fun(fun)
	  , m_typeList(typelist)
	  , m_returnType(ret)
	{
	}
	/// @brief Construct from function pointer
	template<class Ret>
	VipFunction(Ret (*fun)())
	{
		*this = VipFunction::create<Ret()>(fun);
	}
	/// @brief Construct from function pointer
	template<class Ret, class A1>
	VipFunction(Ret (*fun)(A1))
	{
		*this = VipFunction::create<Ret(A1)>(fun);
	}
	/// @brief Construct from function pointer
	template<class Ret, class A1, class A2>
	VipFunction(Ret (*fun)(A1, A2))
	{
		*this = VipFunction::create<Ret(A1, A2)>(fun);
	}
	/// @brief Construct from function pointer
	template<class Ret, class A1, class A2, class A3>
	VipFunction(Ret (*fun)(A1, A2, A3))
	{
		*this = VipFunction::create<Ret(A1, A2, A3)>(fun);
	}
	/// @brief Construct from function pointer
	template<class Ret, class A1, class A2, class A3, class A4>
	VipFunction(Ret (*fun)(A1, A2, A3, A4))
	{
		*this = VipFunction::create<Ret(A1, A2, A3, A4)>(fun);
	}
	/// @brief Construct from function pointer
	template<class Ret, class A1, class A2, class A3, class A4, class A5>
	VipFunction(Ret (*fun)(A1, A2, A3, A4, A5))
	{
		*this = VipFunction::create<Ret(A1, A2, A3, A4, A5)>(fun);
	}

	VipFunction(const VipFunction&) = default;
	VipFunction(VipFunction&&) = default;
	VipFunction& operator=(const VipFunction&) = default;
	VipFunction& operator=(VipFunction&&) = default;

	/// @brief Returns true if the VipFunction is valid (contains a valid FunctionType)
	bool isValid() const
	{
		if (m_fun)
			return true;
		return false;
	}
	bool isNull() const { return !isValid(); }

	/// @brief Returns the underlying std::function object
	const FunctionType& function() const { return m_fun; }
	/// @brief Returns the arguments types as a \a VipTypeList
	const VipTypeList& typeList() const { return m_typeList; }
	/// @brief Returns the return type
	const VipType& returnType() const { return m_returnType; }

	/// @brief Call the function object with given arguments
	VipAny operator()(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const
	{
		return m_fun(v1, v2, v3, v4, v5);
	}

	/// @brief Comparison operator.
	/// Two VipFunction objects are considered equals if they have the same argument and return types.
	bool operator==(const VipFunction& other) { return m_typeList == other.m_typeList && m_returnType == other.m_returnType; }
	/// Comparison operator.
	/// Two VipFunction objects are considered equals if they have the same argument and return types.
	bool operator!=(const VipFunction& other) { return !((*this) == other); }

	/// @brief Create a \a VipFunction object from any callable object (in the sense of std::function)
	template<class Signature, class Callable>
	static VipFunction create(const Callable& c)
	{
		return VipFunction(details::create<Signature>(c), details::buildTypeList<Signature>(), details::buildReturnType<Signature>());
	}
};

/// @brief VipFunctionDispatcher is a list of VipFunction.
///
/// Its purpose is to pick the right function according to the argument types. Thus, it provides
/// a different and expendable approach for polymorphism based on Qt metatype system.
///
/// Usage:
///
/// \code
/// inline void print_int(int value) { std::cout<<"this is an integer: "<<value<<std::endl;}
/// inline void print_double(double value) { std::cout<<"this is a double: "<<value<<std::endl;}
///
/// VipFunctionDispatcher disp(1);
/// disp.append<void(int)>(print_int);
/// disp.append<void(double)>(print_double);
///
/// disp.callAllExactMatch(2); //call print_int(2)
/// disp.callAllExactMatch(2.3); //call print_double(2.3)
/// \end_code
///
/// When dealing with object pointers, VipFunctionDispatcher will only work on QObject inheriting classes.
/// Here is an example on how to extend a class behavior with VipFunctionDispatcher.
/// \code
/// struct Base :  QObject
/// {
///	Q_OBJECT
///
///	virtual QString identifier() {"I am a Base";}
/// };
///
/// struct Child : Base
/// {
///	Q_OBJECT
///
///	virtual QString identifier() {return "I am a Child";}
/// };
///
/// //I want to extend the functionalities of Base and Child because know I need a sub identifier
///
/// static QString Base_identifier(Base *) {return "And also a QObject";}
/// static QString Child_identifier(Child *) {return "And also a Base and a QObject";}
///
/// VipFunctionDispatcher sub_identifier(1);
/// sub_identifier.append<QString(Base*)>(Base_identifier);
/// sub_identifier.append<QString(Child*)>(Child_identifier);
///
/// //Test the sub_identifier on a Base pointers
///
/// Base * b1 = new Base();
/// Base * b2 = new Child();
///
/// std::cout<< sub_identifier(b1).value<QByteArray>().data() <<std::endl; // "And also a QObject"
/// std::cout<< sub_identifier(b2).value<QByteArray>().data() <<std::endl; // "And also a Base and a QObject"
///
/// \endcode
///
/// VipFunctionDispatcher provides ways to retrieve the functions that match exactly given arguments, but also the ones that can accept these arguments through implicit conversion.
///
class VIP_CORE_EXPORT VipFunctionDispatcher
{
	int m_argCount;
	QList<VipFunction> m_functions;

	// return false if equal, true otherwise
	bool nonConvertible(const VipTypeList& t1, const VipTypeList& t2) const;
	bool exactEqual(const VipTypeList& t1, const VipTypeList& t2) const;

public:
	using FunctionList = QList<VipFunction>;
	using result_type = VipAny;

	/// @brief Construct a VipFunctionDispatcher with given arity
	VipFunctionDispatcher(int arity = 0)
	  : m_argCount(arity)
	{
	}

	VipFunctionDispatcher(const VipFunctionDispatcher&) = default;
	VipFunctionDispatcher(VipFunctionDispatcher&&) = default;
	VipFunctionDispatcher& operator=(const VipFunctionDispatcher&) = default;
	VipFunctionDispatcher& operator=(VipFunctionDispatcher&&) = default;

	/// @brief Returns true if the dispatcher is valid (non 0 arity)
	bool isValid() const { return m_argCount != 0; }

	/// @brief Returns all the functions stored in this VipFunctionDispatcher
	const FunctionList& functions() const;
	/// @brief Returns the function dispatcher arity
	int arity() const;
	/// @brief Returns the number of functions stored in this VipFunctionDispatcher
	int count() const { return m_functions.size(); }

	/// @brief Returns all functions that match given argument types.
	/// This includes the functions that match exactly given argument types, but also the ones that can accept these arguments through implicit conversion.
	FunctionList match(const VipTypeList& lst) const;
	/// @brief Returns all functions that match given argument types.
	/// This includes the functions that match exactly given argument types, but also the ones that can accept these arguments through implicit conversion.
	FunctionList match(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;

	/// @brief Returns all functions that match exactly given argument types.
	FunctionList exactMatch(const VipTypeList& lst) const;
	/// @brief Returns all functions that match exactly given argument types.
	FunctionList exactMatch(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;

	/// @brief Call the first found function that match given arguments, and return the result
	VipAny callOneMatch(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;
	/// @brief Call the first found function that match exactly given arguments, and return the result
	VipAny callOneExactMatch(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;

	/// @brief Call all the functions that match given arguments, and return the results
	QList<VipAny> callAllMatch(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;
	/// @brief Call all the functions that match exactly given arguments, and return the results
	QList<VipAny> callAllExactMatch(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;

	/// @brief Equivalent to callOneExactMatch()
	VipAny operator()(const VipAny& v1 = VipAny(), const VipAny& v2 = VipAny(), const VipAny& v3 = VipAny(), const VipAny& v4 = VipAny(), const VipAny& v5 = VipAny()) const;

	/// @brief Add a new function
	void append(const VipFunction& fun);
	/// @brief Add new functions
	void append(const FunctionList& funs);
	/// Add a callable object
	template<class Signature, class Callable>
	void append(const Callable& c)
	{
		append(VipFunction::create<Signature>(c));
	}

	/// @brief Remove all functions having the same signature as \a fun
	void remove(const VipFunction& fun);
	/// @brief Remove all functions having the same signature as \a lst
	void remove(const FunctionList& lst);
	/// @brief Remove all functions
	void clear();
};

/// @}
// end Core

#endif
