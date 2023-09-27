#ifndef VIP_NDARRAY_VARIANT_H
#define VIP_NDARRAY_VARIANT_H

#include "VipNDArrayImage.h"

namespace detail
{
	///\internal Function used to return a default value
	template< class T> inline T returnFunction() { return T(); }
	template<> inline void returnFunction() {}

	///\internal Visitor adapter, convert a VipNDArray into a VipNDArrayTypeView
	template<class Visitor, class Ret>
	struct VipNDArrayUnaryVisitorAdapter
	{
		typedef Ret result_type;

		Visitor & visitor;
		VipNDArrayUnaryVisitorAdapter(Visitor & visitor) : visitor(visitor) {}

		template< class T>
		result_type applyWrapper(const VipNDArray & ar, const T* ) {
			return visitor(VipNDArrayTypeView<T>(ar));
		}
		result_type applyWrapper(const VipNDArray & ar, const QImage* ) {
			return visitor(vipToImage(ar));
		}
		result_type applyWrapper(const VipNDArray & , const void*) {
			return returnFunction<result_type>();
		}

		template< class T>
		result_type apply(const VipNDArray & ar){
			return applyWrapper(ar, (T*)(NULL));
		}
	};

	///\internal same as qMetaTypeId with a void specialization
	template< class T> inline int metaTypeId() { return qMetaTypeId<T>(); }
	template<> inline int metaTypeId<void>() {	return 0;}

	///\internal Manage a call to a unary visitor
	template< class T1, class T2 = void, class T3 = void, class T4 = void, class T5 = void, class T6 = void,
		class T7 = void, class T8 = void, class T9 = void, class T10 = void, class T11 = void, class T12 = void, class T13 = void>
	struct Handler
	{
		///Number of template types
		static const int count = 1 + (int)(!std::is_same<T2, void>::value) +
			(int)(!std::is_same<T3, void>::value) +
			(int)(!std::is_same<T4, void>::value) +
			(int)(!std::is_same<T5, void>::value) +
			(int)(!std::is_same<T6, void>::value) +
			(int)(!std::is_same<T7, void>::value) +
			(int)(!std::is_same<T8, void>::value) +
			(int)(!std::is_same<T9, void>::value) +
			(int)(!std::is_same<T10, void>::value) +
			(int)(!std::is_same<T11, void>::value) +
			(int)(!std::is_same<T12, void>::value) +
			(int)(!std::is_same<T13, void>::value);

		///Check if a runtime type is valis
		static bool isValidType(int data_type)
		{
			return data_type == metaTypeId<T1>() ||
				(!std::is_same<T2, void>::value && data_type == metaTypeId<T2>()) ||
				(!std::is_same<T3, void>::value && data_type == metaTypeId<T3>()) ||
				(!std::is_same<T4, void>::value && data_type == metaTypeId<T4>()) ||
				(!std::is_same<T5, void>::value && data_type == metaTypeId<T5>()) ||
				(!std::is_same<T6, void>::value && data_type == metaTypeId<T6>()) ||
				(!std::is_same<T7, void>::value && data_type == metaTypeId<T7>()) ||
				(!std::is_same<T8, void>::value && data_type == metaTypeId<T8>()) ||
				(!std::is_same<T9, void>::value && data_type == metaTypeId<T9>()) ||
				(!std::is_same<T10, void>::value && data_type == metaTypeId<T10>()) ||
				(!std::is_same<T11, void>::value && data_type == metaTypeId<T11>()) ||
				(!std::is_same<T12, void>::value && data_type == metaTypeId<T12>()) ||
				(!std::is_same<T13, void>::value && data_type == metaTypeId<T13>());
		}

		///Call a unary visitor
		template< class Ret, class Visitor>
		static Ret visitUnary(const Visitor & visitor, const VipNDArray & ar, bool * ok)
		{
			const int data_type = ar.dataType();

			VipNDArrayUnaryVisitorAdapter<Visitor,Ret> adapter(const_cast<Visitor&>(visitor));

			if (data_type == metaTypeId<T1>())
			{
				if(ok) *ok = true;
				return adapter.template apply<T1>(ar);
			}
			else if (!std::is_same<T2, void>::value && data_type == metaTypeId<T2>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T2>(ar);
			}
			else if (!std::is_same<T3, void>::value && data_type == metaTypeId<T3>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T3>(ar);
			}
			else if (!std::is_same<T4, void>::value && data_type == metaTypeId<T4>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T4>(ar);
			}
			else if (!std::is_same<T5, void>::value && data_type == metaTypeId<T5>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T5>(ar);
			}
			else if (!std::is_same<T6, void>::value && data_type == metaTypeId<T6>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T6>(ar);
			}
			else if (!std::is_same<T7, void>::value && data_type == metaTypeId<T7>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T7>(ar);
			}
			else if (!std::is_same<T8, void>::value && data_type == metaTypeId<T8>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T8>(ar);
			}
			else if (!std::is_same<T9, void>::value && data_type == metaTypeId<T9>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T9>(ar);
			}
			else if (!std::is_same<T10, void>::value && data_type == metaTypeId<T10>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T10>(ar);
			}
			else if (!std::is_same<T11, void>::value && data_type == metaTypeId<T11>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T11>(ar);
			}
			else if (!std::is_same<T12, void>::value && data_type == metaTypeId<T12>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T12>(ar);
			}
			else if (!std::is_same<T13, void>::value && data_type == metaTypeId<T13>())
			{
				if (ok) *ok = true;
				return adapter.template apply<T13>(ar);
			}
			else
			{
				if (ok) *ok = false;
				return returnFunction<Ret>();
			}
		}

		static int findBestType(int type)
		{
			if (isValidType(type))
				return type;

			//sort the types by size
			QMultiMap<int, int> sizeTypes;
			QMetaType t(metaTypeId<T1>());
			sizeTypes.insert(QMetaType(metaTypeId<T1>()).sizeOf(), metaTypeId<T1>());
			if(!std::is_same<T2, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T2>()).sizeOf(), metaTypeId<T2>());
			if (!std::is_same<T3, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T3>()).sizeOf(), metaTypeId<T3>());
			if (!std::is_same<T4, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T4>()).sizeOf(), metaTypeId<T4>());
			if (!std::is_same<T5, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T5>()).sizeOf(), metaTypeId<T5>());
			if (!std::is_same<T6, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T6>()).sizeOf(), metaTypeId<T6>());
			if (!std::is_same<T7, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T7>()).sizeOf(), metaTypeId<T7>());
			if (!std::is_same<T8, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T8>()).sizeOf(), metaTypeId<T8>());
			if (!std::is_same<T9, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T9>()).sizeOf(), metaTypeId<T9>());
			if (!std::is_same<T10, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T10>()).sizeOf(), metaTypeId<T10>());
			if (!std::is_same<T11, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T11>()).sizeOf(), metaTypeId<T11>());
			if (!std::is_same<T12, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T12>()).sizeOf(), metaTypeId<T12>());
			if (!std::is_same<T13, void>::value) sizeTypes.insert(QMetaType(metaTypeId<T13>()).sizeOf(), metaTypeId<T13>());

			//find the first convertible type that has a bigger size
			int type_size = QMetaType(type).sizeOf();
			for (QMultiMap<int, int>::const_iterator it = sizeTypes.begin(); it != sizeTypes.end(); ++it)
			{
				if (it.key() > type_size && QVariant(type, NULL).canConvert(it.value()))
				{
					return it.value();
				}
			}
			//no match, then find the biggest convertible type
			QMapIterator<int, int> i(sizeTypes);
			i.toBack();
			while (i.hasPrevious()) {
				i.previous();
				if (QVariant(type, NULL).canConvert(i.value()))
					return i.value();
			}
			return 0;
		}
	};




} //end detail



/// Base class for visitor objects used with VipNDArrayVariant::visit.
/// Its only goal is to provide the result_type typedef, which must be the result of a call to the parenthesis operator.
template< class Res = void>
struct VipNDArrayVisitor
{
	typedef Res result_type;

	template< class T>
	result_type operator()(const VipNDArrayTypeView<T> &);
};


/// A VipNDArray class that can only contains a sub set of types (up to 13).
/// VipNDArrayVariant behaves exactly like VipNDArray, except that it can contains only a few data type defined at compile time.
/// Trying to assign a data type that is not part of the supported types will result in an empty array.
///
/// The main advantage of VipNDArrayVariant is that it provides a visitor like pattern through its member function VipNDArrayVariant::apply.
/// A visitor class allows statically typed algorithms, as displayed in the following example.
///
/// \code
/// struct visitor : public VipNDArrayVisitor<void>
/// {
/// template< class T>
/// void operator()(VipNDArrayTypeView<T> view)
/// {
/// for (int i = 0; i < view.size(); ++i)
/// {
/// printf("%s\t", QString::number(view[i]).toLatin1().data());
/// }
/// printf("\n");
/// }
/// };
///
///
/// VipNDArrayType<float> ar(vipVector(2,3));
/// ar = 0, 1, 2, 3, 4, 5;
///
/// VipNDArrayVariant<int, double, float> array = ar;
/// array.apply(visitor());
///
/// \endcode
///
/// A visitor class must inherit VipNDArrayVisitor and define a parenthesis operator with any kind of return type.
template< class T1, class T2 = void, class T3 = void, class T4 = void, class T5 = void, class T6 = void,
	class T7 = void, class T8 = void, class T9 = void, class T10 = void, class T11 = void, class T12 = void, class T13 = void>
class VipNDArrayVariant : public VipNDArray
{
	template< class Visitor,class Array1, class Res = void>
	struct BinaryAdapterFinal
	{
		typedef Res result_type;
		Array1 * ar1;
		Visitor * visitor;
		template< class Array2>
		result_type operator()(const Array2 & ar2) const {
			return (*visitor)(*ar1, ar2);
		}
	};
	template< class Visitor,class VariantType, class Res = void>
	struct BinaryAdapter
	{
		typedef Res result_type;
		VariantType * ar2;
		Visitor * visitor;
		bool *ok;
		template< class Array1>
		result_type operator()(const Array1 & ar1) const {
			BinaryAdapterFinal<Visitor,Array1,Res> adapter;
			adapter.ar1 = const_cast<Array1*>(&ar1);
			adapter.visitor = visitor;
			return ar2->template apply<Res>(adapter,ok);
		}
	};

public:
	typedef detail::Handler<T1, T2, T3, T4, T5, T6, T7, T8, T9, T10, T11, T12,T13> handler_type;

	///Returns true if given type can be managed by this VipNDArrayVariant
	bool isValidType(int data_type) const
	{
		return handler_type::isValidType(data_type);
	}

	///Returns true if given type can be managed by this VipNDArrayVariant
	template<class T>
	bool isValidType() const
	{
		return handler_type::isValidType(qMetaTypeId<T>());
	}

	///Returns true if given type can be imported by this VipNDArrayVariant
	bool canImportType(int data_type) const
	{
		return handler_type::isValidType(data_type) || handler_type::findBestType(data_type) != 0;
	}

	///Returns true if given type can be imported by this VipNDArrayVariant
	template<class T>
	bool canImportType() const
	{
		return canImportType(qMetaTypeId<T>());
	}

	///Default constructor
	VipNDArrayVariant()
		:VipNDArray()
	{}

	///Construct from a VipNDArray
	VipNDArrayVariant(const VipNDArray & ar)
		:VipNDArray()
	{
		*this = ar;
	}

	///Create and allocate a VipNDArrayVariant of data type \a data_type and shape \a shape
	VipNDArrayVariant(int data_type, const VipNDArrayShape & shape)
		:VipNDArray()
	{
		if (handler_type::isValidType(data_type))
		{
			*this = VipNDArray(data_type, shape);
		}
	}

	///Create and allocate a VipNDArrayVariant of data type \a data_type and shape \a shape.
	/// Performs a deep copy of the data pointer \a ptr into the array
	VipNDArrayVariant(const void * ptr, int data_type, const VipNDArrayShape & shape)
		:VipNDArray()
	{
		if (handler_type::isValidType(data_type))
		{
			*this = VipNDArray(ptr,data_type, shape);
		}
	}
	///Create and allocate a VipNDArray of template data type \a T and shape \a shape.
	/// Performs a deep copy of the data pointer \a ptr into the array
	template< class T>
	VipNDArrayVariant(const T * ptr, const VipNDArrayShape & shape)
		:VipNDArray()
	{
		if (handler_type::isValidType(detail::metaTypeId<T>()))
		{
			*this = VipNDArray(ptr, shape);
		}
	}


	///Copy operator
	VipNDArrayVariant & operator=(const VipNDArray & other)
	{
		if (isValidType(other.dataType()))
			VipNDArray::operator=(other);
		else if(int dst_type = handler_type::findBestType(other.dataType()))
			VipNDArray::operator=(other.convert(dst_type));
		else
			clear();
		return *this;
	}

	///Reimplemented from #VipNDArray::setSharedHandle
	virtual void setSharedHandle(const SharedHandle & other)
	{
		if (other && handler_type::isValidType(other->dataType()))
			VipNDArray::setSharedHandle(other);
		else
			VipNDArray::setSharedHandle(vipNullHandle());
	}

	///Apply a unary visitor.
	/// The visitor should inherit VipNDArrayVisitor or provide the result_type typedef.
	/// This will call the operator Visitor::operator().
	template< class Visitor>
	typename Visitor::result_type apply(const Visitor & visitor, bool * ok = NULL)
	{
		return handler_type::template visitUnary<typename Visitor::result_type>(visitor, *this, ok);
	}

	/// Apply a unary visitor.
	/// The visitor can be a #VipNDArrayVisitor object or a lambda:
	/// \code
	/// VipNDArrayVariant<int, double, float> ar(QMetaType::Int, vipVector(2, 2));
	/// ar.fill(0);
	/// ar.setValue(vipVector(1, 1), 3);
///
	/// auto visitor = [](auto const& view) { for(int i=0; i < view.size(); ++i) view[i] *= 2; };
	/// ar.apply<void>( [](auto const& view) { my_fun(view.ptr(), view.shape(1), view.shape(0)); } );
///
	/// \endcode
	template< class Ret, class Visitor>
	Ret apply(const Visitor & visitor, bool * ok = NULL)
	{
		return handler_type::template visitUnary<Ret>(visitor, *this, ok);
	}


	///Apply a binary visitor.
	/// The visitor should inherit VipNDArrayVisitor or provide the result_type typedef.
	/// This will call the operator Visitor::operator().
	template< class Visitor, class VariantType>
	typename Visitor::result_type applyBinary(const Visitor & visitor, const VariantType & other, bool * ok = NULL)
	{
		BinaryAdapter<Visitor, VariantType,typename Visitor::result_type> adapter;
		adapter.ar2 = const_cast<VariantType*>(&other);
		adapter.visitor = const_cast<Visitor*>(&visitor);
		adapter.ok = ok;
		return handler_type::template visitUnary<typename Visitor::result_type>(adapter, *this, ok);
	}

	/// Apply a binary visitor.
	/// The visitor can be a #VipNDArrayVisitor object or a lambda.
	template< class Ret, class Visitor, class VariantType>
	Ret applyBinary(const Visitor & visitor, const VariantType & other, bool * ok = NULL)
	{
		BinaryAdapter<Visitor, VariantType, Ret> adapter;
		adapter.ar2 = const_cast<VariantType*>(&other);
		adapter.visitor = const_cast<Visitor*>(&visitor);
		adapter.ok = ok;
		return handler_type::template visitUnary<Ret>(visitor, *this, ok);
	}
};



typedef VipNDArrayVariant<qint8, quint8, qint16, quint16, qint32, quint32, qint64, quint64, float, double, long double, complex_f, complex_d> VipNDNumericOrComplexArray;
typedef VipNDArrayVariant<qint8, quint8, qint16, quint16, qint32, quint32, qint64, quint64, float, double, long double> VipNDNumericArray;
typedef VipNDArrayVariant<complex_f,complex_d> VipNDComplexArray;

#endif
