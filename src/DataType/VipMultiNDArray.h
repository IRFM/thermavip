#ifndef VIP_MULTI_NDARRAY_H
#define VIP_MULTI_NDARRAY_H


#include "VipNDArray.h"

namespace detail
{
	struct MultiNDArrayHandle : public VipNDArrayHandle
	{
		QMap<QString, VipNDArray>	arrays;
		QString						current;
		SharedHandle				currentHandle;

		MultiNDArrayHandle();
		MultiNDArrayHandle(const MultiNDArrayHandle & h);

		void setCurrentArray(const QString& name);
		void addArray(const QString & name, const VipNDArray & ar);
		void removeArray(const QString & name);

		virtual MultiNDArrayHandle * copy() const { return new MultiNDArrayHandle(*this); }
		virtual void * dataPointer(const VipNDArrayShape & pos) const { return currentHandle->dataPointer(pos); }
		virtual int handleType() const { return MultiArray; }
		virtual bool realloc(const VipNDArrayShape & sh) { return currentHandle->realloc(sh); }
		virtual bool reshape(const VipNDArrayShape & sh) { return currentHandle->reshape(sh); }
		virtual bool resize(const VipNDArrayShape & start, const VipNDArrayShape & shape, VipNDArrayHandle * h, Vip::InterpolationType type, const VipNDArrayShape & out_start, const VipNDArrayShape & out_shape) const {
			return currentHandle->resize(start, shape, h, type, out_start, out_shape);
		}
		virtual void * opaqueForPos(void * op, const VipNDArrayShape & pos) const { return currentHandle->opaqueForPos(op, pos); }
		virtual const char* dataName() const { return currentHandle ? currentHandle->dataName() : NULL; }
		virtual int dataSize() const { return currentHandle ? currentHandle->dataSize() : 0; }
		virtual int dataType() const { return currentHandle ? currentHandle->dataType() : 0; }
		virtual bool canExport(int type) const { return currentHandle->canExport(type); }
		virtual bool canImport(int type) const { return currentHandle->canImport(type); }
		virtual bool exportData(const VipNDArrayShape & this_shape, const VipNDArrayShape & this_start, VipNDArrayHandle * dst, const VipNDArrayShape & dst_shape, const VipNDArrayShape & dst_start) const {
			return currentHandle->exportData(this_shape, this_start, dst, dst_shape, dst_start);
		}
		virtual bool importData(const VipNDArrayShape & this_shape, const VipNDArrayShape & this_start, const VipNDArrayHandle * src, const VipNDArrayShape & src_shape, const VipNDArrayShape & src_start) {
			return currentHandle->importData(this_shape, this_start, src, src_shape, src_start);
		}
		virtual bool fill(const VipNDArrayShape & start, const VipNDArrayShape & shape, const QVariant & v) { return currentHandle->fill(start, shape, v); }
		virtual QVariant toVariant(const VipNDArrayShape & sh) const { return currentHandle->toVariant(sh); }
		virtual void fromVariant(const VipNDArrayShape & sh, const QVariant & v) { return currentHandle->fromVariant(sh, v); }
		virtual QDataStream & ostream(const VipNDArrayShape & start, const VipNDArrayShape & shape, QDataStream & o) const;
		virtual QDataStream & istream(const VipNDArrayShape & start, const VipNDArrayShape & shape, QDataStream & i);
		virtual QTextStream & oTextStream(const VipNDArrayShape & start, const VipNDArrayShape & shape, QTextStream & stream, const QString & separator) const { return currentHandle->oTextStream(start, shape, stream, separator); }
	};
}


/// @brief A map of string -> VipNDArray with one active array
class VIP_DATA_TYPE_EXPORT VipMultiNDArray : public VipNDArray
{
public:
	///Default constructor
	VipMultiNDArray();
	///Copy constructor
	VipMultiNDArray(const VipNDArray & ar);

	detail::MultiNDArrayHandle * handle();
	const detail::MultiNDArrayHandle * handle() const;

	///Copy operator
	VipMultiNDArray & operator=(const VipNDArray & other);

	void addArray(const QString & name, const VipNDArray & array);
	void removeArray(const QString & name);
	int arrayCount() const;
	QStringList arrayNames() const;
	QList<VipNDArray> arrays() const;
	VipNDArray array(const QString & name) const;
	const QMap<QString, VipNDArray> & namedArrays() const;
	void setNamedArrays(const QMap<QString, VipNDArray> & ars);
	void setCurrentArray(const QString & name);
	const QString & currentArray() const;


protected:
	virtual void setSharedHandle(const SharedHandle & other);
};


VIP_DATA_TYPE_EXPORT bool vipIsMultiNDArray(const VipNDArray & ar);

#endif
