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

#include <complex>

#include "VipLogging.h"
#include "VipMath.h"
#include "VipStandardProcessing.h"
#include "VipUniqueId.h"
#include "VipNDArrayImage.h"

class VipOtherPlayerData::PrivateData
{
public:
	bool isDynamic;
	VipLazyPointer processing;
	VipLazyPointer parent;
	int outputIndex;
	int otherPlayerId;
	int otherDisplayIndex;
	VipAnyData staticData;
	bool shouldResizeArray;
	PrivateData()
	  : isDynamic(false)
	  , outputIndex(0)
	  , otherPlayerId(0)
	  , otherDisplayIndex(0)
	  , shouldResizeArray(false)
	{
	}
};

VipOtherPlayerData::VipOtherPlayerData()
  : d_data(new PrivateData())
{
}

VipOtherPlayerData::VipOtherPlayerData(const VipAnyData& static_data)
  : d_data(new PrivateData())
{
	d_data->staticData = static_data;
}

VipOtherPlayerData::VipOtherPlayerData(bool is_dynamic, VipProcessingObject* object, VipProcessingObject* parent, int outputIndex, int otherPlayerId, int otherDisplayIndex)
  : d_data(new PrivateData())
{
	d_data->isDynamic = is_dynamic;
	d_data->processing = object;
	d_data->parent = parent;
	d_data->outputIndex = outputIndex;
	d_data->otherPlayerId = otherPlayerId;
	d_data->otherDisplayIndex = otherDisplayIndex;
	d_data->staticData = dynamicData();
}

void VipOtherPlayerData::setShouldResizeArray(bool enable)
{
	d_data->shouldResizeArray = enable;
}

bool VipOtherPlayerData::shouldResizeArray() const
{
	return d_data->shouldResizeArray;
}

void VipOtherPlayerData::setParentProcessing(VipProcessingObject* parent)
{
	d_data->parent = parent;
	// reset the static data
	d_data->staticData = dynamicData();
}
VipProcessingObject* VipOtherPlayerData::parentProcessingObject() const
{
	return d_data->parent.data<VipProcessingObject>();
}

bool VipOtherPlayerData::isDynamic() const
{
	return d_data->isDynamic;
}
int VipOtherPlayerData::otherPlayerId() const
{
	return d_data->otherPlayerId;
}
int VipOtherPlayerData::otherDisplayIndex() const
{
	return d_data->otherDisplayIndex;
}
int VipOtherPlayerData::outputIndex() const
{
	return d_data->outputIndex;
}
VipProcessingObject* VipOtherPlayerData::processing() const
{
	return d_data->processing.data<VipProcessingObject>();
}
VipAnyData VipOtherPlayerData::staticData() const
{
	return d_data->staticData;
}
VipAnyData VipOtherPlayerData::dynamicData()
{
	VipAnyData tmp = d_data->staticData;
	if (tmp.isEmpty() || d_data->isDynamic) {
		if (VipProcessingObject* obj = d_data->processing.data<VipProcessingObject>())
			if (d_data->outputIndex < obj->outputCount())
				if (VipOutput* output = obj->outputAt(d_data->outputIndex)) {
					if (VipProcessingObject* parent = parentProcessingObject()) {
						VipProcessingList* lst = qobject_cast<VipProcessingList*>(parent->property("VipProcessingList").value<VipProcessingList*>());
						if (lst == obj) {
							// case: the parent is inside a processing list: take the data just before parent in the prc list
							int index = lst->indexOf(parent);
							if (index == 0) {
								if (VipOutput* out = lst->inputAt(0)->connection()->source())
									tmp = out->data();
								else
									tmp = lst->inputAt(0)->data();
							}
							else if (index > 0) {
								tmp = lst->at(index - 1)->outputAt(0)->data();
							}
						}
						else if (parent == obj) {
							VIP_LOG_WARNING("Potential recursion detected while trying to grab the data");
						}
						else {
							if (!output->parentProcessing()->isUpdating())
								output->parentProcessing()->wait(true, 1000);
							tmp = output->data();
						}
					}
					else {
						if (!output->parentProcessing()->isUpdating())
							output->parentProcessing()->wait(true, 1000);
						tmp = output->data();
					}
				}
	}

	return tmp;
}

VipAnyData VipOtherPlayerData::data()
{
	return d_data->isDynamic ? dynamicData() : staticData();
}

QDataStream& operator<<(QDataStream& arch, const VipOtherPlayerData& o)
{
	return arch << o.d_data->processing << o.d_data->parent << o.d_data->outputIndex << o.d_data->isDynamic << o.d_data->otherPlayerId << o.d_data->otherDisplayIndex << o.d_data->staticData
		    << o.d_data->shouldResizeArray;
}

QDataStream& operator>>(QDataStream& arch, VipOtherPlayerData& o)
{
	return arch >> o.d_data->processing >> o.d_data->parent >> o.d_data->outputIndex >> o.d_data->isDynamic >> o.d_data->otherPlayerId >> o.d_data->otherDisplayIndex >> o.d_data->staticData >>
	       o.d_data->shouldResizeArray;
}

int registerVipOtherPlayerData()
{
	qRegisterMetaType<VipOtherPlayerData>();
	qRegisterMetaTypeStreamOperators<VipOtherPlayerData>();
	return 0;
}
int _registerVipOtherPlayerData = registerVipOtherPlayerData();

VipNormalize::VipNormalize(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(0.);
	propertyAt(1)->setData(1.);
}

bool VipNormalize::acceptInput(int // index
			       ,
			       const QVariant& v) const
{
	int type = v.userType();
	return type == qMetaTypeId<VipNDArray>() || type == qMetaTypeId<VipPointVector>() || type == qMetaTypeId<VipIntervalSampleVector>();
}

void VipNormalize::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	if (!acceptInput(0, any.data())) {
		setError("wrong input type (" + QString(any.data().typeName()) + ")", VipProcessingObject::WrongInput);
		return;
	}

	if (any.data().userType() == qMetaTypeId<VipNDArray>()) {
		VipNDArray ar = any.value<VipNDArray>().toDouble();
		ar.detach();

		if (ar.isEmpty()) {
			setError("empty array or wrong array type", VipProcessingObject::WrongInput);
			return;
		}

		int size = ar.size();
		double* ptr = (double*)ar.data();

		// find min and max values
		double min = ptr[0];
		double max = ptr[0];
		for (int i = 1; i < size; ++i) {
			min = qMin(min, ptr[i]);
			max = qMax(max, ptr[i]);
		}

		// normalize
		double norm_min = propertyAt(0)->data().value<double>();
		double norm_max = propertyAt(1)->data().value<double>();

		if (min != max) {
			double factor = (norm_max - norm_min) / (max - min);
			for (int i = 0; i < size; ++i) {
				ptr[i] = (ptr[i] - min) * factor + norm_min;
			}
		}
		else {
			for (int i = 0; i < size; ++i) {
				ptr[i] = norm_max;
			}
		}

		VipAnyData data = create(QVariant::fromValue(ar));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
	else if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
		VipPointVector ar = any.value<VipPointVector>();
		if (ar.isEmpty()) {
			setError("empty point vector", VipProcessingObject::WrongInput);
			return;
		}

		// find min and max values
		vip_double min = ar[0].y();
		vip_double max = ar[0].y();
		for (int i = 1; i < ar.size(); ++i) {
			min = qMin(min, ar[i].y());
			max = qMax(max, ar[i].y());
		}

		// normalize
		double norm_min = propertyAt(0)->data().value<double>();
		double norm_max = propertyAt(1)->data().value<double>();

		if (min != max) {
			vip_double factor = (norm_max - norm_min) / (max - min);
			for (int i = 0; i < ar.size(); ++i) {
				ar[i].setY((ar[i].y() - min) * factor + norm_min);
			}
		}
		else {
			for (int i = 0; i < ar.size(); ++i) {
				ar[i].setY(norm_max);
			}
		}

		VipAnyData data = create(QVariant::fromValue(ar));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
	else if (any.data().userType() == qMetaTypeId<VipIntervalSampleVector>()) {
		VipIntervalSampleVector ar = any.value<VipIntervalSampleVector>();
		if (ar.isEmpty()) {
			setError("empty interval sample vector", VipProcessingObject::WrongInput);
			return;
		}

		// find min and max values
		vip_double min = ar[0].value;
		vip_double max = ar[0].value;
		for (int i = 1; i < ar.size(); ++i) {
			min = qMin(min, ar[i].value);
			max = qMax(max, ar[i].value);
		}

		// normalize
		double norm_min = propertyAt(0)->data().value<double>();
		double norm_max = propertyAt(1)->data().value<double>();

		if (min != max) {
			vip_double factor = (norm_max - norm_min) / (max - min);
			for (int i = 0; i < ar.size(); ++i) {
				ar[i].value = ((ar[i].value - min) * factor + norm_min);
			}
		}
		else {
			for (int i = 0; i < ar.size(); ++i) {
				ar[i].value = norm_max;
			}
		}

		VipAnyData data = create(QVariant::fromValue(ar));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
}

VipClamp::VipClamp(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(0.);
	propertyAt(1)->setData(1.);
}

bool VipClamp::acceptInput(int // index
			   ,
			   const QVariant& v) const
{
	int type = v.userType();
	return type == qMetaTypeId<VipNDArray>() || type == qMetaTypeId<VipPointVector>() || type == qMetaTypeId<VipIntervalSampleVector>() || v.canConvert<double>();
}

void VipClamp::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	if (!acceptInput(0, any.data())) {
		setError("wrong input type (" + QString(any.data().typeName()) + ")", VipProcessingObject::WrongInput);
		return;
	}

	if (any.data().userType() == qMetaTypeId<VipNDArray>()) {
		VipNDArray ar = any.value<VipNDArray>().toDouble();
		ar.detach();

		if (ar.isEmpty()) {
			setError("empty array or wrong array type", VipProcessingObject::WrongInput);
			return;
		}

		int size = ar.size();
		double* ptr = (double*)ar.data();

		// clamp
		double min = propertyAt(0)->data().value<double>();
		double max = propertyAt(1)->data().value<double>();
		for (int i = 0; i < size; ++i) {
			if (min == min && ptr[i] < min)
				ptr[i] = min;
			else if (max == max && ptr[i] > max)
				ptr[i] = max;
		}

		VipAnyData data = create(QVariant::fromValue(ar));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
	else if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
		VipPointVector ar = any.value<VipPointVector>();
		if (ar.isEmpty()) {
			setError("empty point vector", VipProcessingObject::WrongInput);
			return;
		}

		double min = propertyAt(0)->data().value<double>();
		double max = propertyAt(1)->data().value<double>();
		for (int i = 0; i < ar.size(); ++i) {
			if (min == min && ar[i].y() < min)
				ar[i].setY(min);
			else if (max == max && ar[i].y() > max)
				ar[i].setY(max);
		}

		VipAnyData data = create(QVariant::fromValue(ar));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
	else if (any.data().userType() == qMetaTypeId<VipIntervalSampleVector>()) {
		VipIntervalSampleVector ar = any.value<VipIntervalSampleVector>();
		if (ar.isEmpty()) {
			setError("empty interval sample vector", VipProcessingObject::WrongInput);
			return;
		}

		double min = propertyAt(0)->data().value<double>();
		double max = propertyAt(1)->data().value<double>();
		for (int i = 0; i < ar.size(); ++i) {
			if (min == min && ar[i].value < min)
				ar[i].value = (min);
			else if (max == max && ar[i].value > max)
				ar[i].value = (max);
		}

		VipAnyData data = create(QVariant::fromValue(ar));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
	else {
		double min = propertyAt(0)->data().value<double>();
		double max = propertyAt(1)->data().value<double>();
		double val = any.value<double>();
		if (min == min && val < min)
			val = (min);
		else if (max == max && val > max)
			val = (max);
		VipAnyData data = create(QVariant::fromValue(val));
		data.setTime(any.time());
		outputAt(0)->setData(data);
	}
}

VipAbs::VipAbs(QObject* parent)
  : VipProcessingObject(parent)
{
}

bool VipAbs::acceptInput(int // index
			 ,
			 const QVariant& v) const
{
	int type = v.userType();
	return type == qMetaTypeId<VipNDArray>() || type == qMetaTypeId<VipPointVector>() || type == qMetaTypeId<VipComplexPointVector>() || type == qMetaTypeId<VipIntervalSampleVector>() ||
	       v.canConvert<double>();
}

void VipAbs::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	QVariant out_data;
	if (any.data().userType() == qMetaTypeId<VipNDArray>()) {
		VipNDArray in = any.value<VipNDArray>();
		if (in.canConvert<double>()) {
			VipNDArray ar = in.toDouble();
			ar.detach();
			if (ar.isEmpty()) {
				setError("empty array or wrong array type", VipProcessingObject::WrongInput);
				return;
			}
			int size = ar.size();
			double* ptr = (double*)ar.data();
			for (int i = 0; i < size; ++i)
				ptr[i] = std::abs(ptr[i]);
			out_data = QVariant::fromValue(ar);
		}
		else if (in.isComplex()) {
			VipNDArray ar = in.convert<complex_d>();
			ar.detach();
			if (ar.isEmpty()) {
				setError("empty array or wrong array type", VipProcessingObject::WrongInput);
				return;
			}
			int size = ar.size();
			complex_d* ptr = (complex_d*)ar.data();
			for (int i = 0; i < size; ++i)
				ptr[i] = std::abs(ptr[i]);
			out_data = QVariant::fromValue(ar);
		}
		else {
			setError("unknown array type", VipProcessingObject::WrongInput);
			return;
		}
	}
	else if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
		VipPointVector ar = any.value<VipPointVector>();
		if (ar.isEmpty()) {
			setError("empty point vector", VipProcessingObject::WrongInput);
			return;
		}
		for (int i = 0; i < ar.size(); ++i)
			ar[i].setY(vipAbs(ar[i].y()));
		out_data = QVariant::fromValue(ar);
	}
	else if (any.data().userType() == qMetaTypeId<VipIntervalSampleVector>()) {
		VipIntervalSampleVector ar = any.value<VipIntervalSampleVector>();
		if (ar.isEmpty()) {
			setError("empty interval sample vector", VipProcessingObject::WrongInput);
			return;
		}
		for (int i = 0; i < ar.size(); ++i)
			ar[i].value = vipAbs(ar[i].value);
		out_data = QVariant::fromValue(ar);
	}
	else if (any.data().userType() == qMetaTypeId<VipComplexPointVector>()) {
		VipComplexPointVector ar = any.value<VipComplexPointVector>();
		if (ar.isEmpty()) {
			setError("empty point vector", VipProcessingObject::WrongInput);
			return;
		}
		for (int i = 0; i < ar.size(); ++i)
			ar[i].setY(std::abs(ar[i].y()));
		out_data = QVariant::fromValue(ar);
	}
	else {
		double val = any.value<double>();
		out_data = std::abs(val);
	}

	VipAnyData data = create(out_data);
	data.setTime(any.time());
	outputAt(0)->setData(data);
}

VipConvert::VipConvert(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(0);
}

bool VipConvert::acceptInput(int // index
			     ,
			     const QVariant& v) const
{
	return v.canConvert<QString>();
}

void VipConvert::apply()
{
	VipAnyData in = inputAt(0)->data();
	int out_type = propertyAt(0)->value<int>();

	if (in.data().userType() == out_type || out_type == 0) {
		in.mergeAttributes(this->attributes());
		outputAt(0)->setData(in);
		return;
	}

	if (in.data().userType() == qMetaTypeId<VipPointVector>()) {
		QVariant v;
		if (out_type == qMetaTypeId<VipComplexPointVector>() || out_type == qMetaTypeId<VipComplexPoint>() || out_type == qMetaTypeId<complex_d>()) {
			v = QVariant::fromValue(vipToComplexPointVector(in.data().value<VipPointVector>()));
		}
		else {
			setError("cannot convert from VipPointVector to " + QString(QMetaType(out_type).name()));
			return;
		}
		VipAnyData out = create(v);
		out.setTime(in.time());
		outputAt(0)->setData(out);
	}
	else if (in.data().userType() == qMetaTypeId<VipNDArray>()) {
		VipNDArray ar = in.data().value<VipNDArray>();
		if (ar.canConvert(out_type)) {
			ar = ar.convert(out_type);
			VipAnyData out = create(QVariant::fromValue(ar));
			out.setTime(in.time());
			outputAt(0)->setData(out);
		}
		else {
			setError("cannot convert from VipNDArray to type " + QString(QMetaType(out_type).name()));
			return;
		}
	}
	else if (in.data().canConvert(VIP_META(out_type))) {
		QVariant v = in.data();
		v.convert(VIP_META(out_type));
		VipAnyData out = create(v);
		out.setTime(in.time());
		outputAt(0)->setData(out);
	}
	else {
		setError("cannot convert from " + QString(in.data().typeName()) + " to  " + QString(QMetaType(out_type).name()));
		return;
	}
}

VipStartAtZero::VipStartAtZero(QObject* parent)
  : VipProcessingObject(parent)
{
}

bool VipStartAtZero::acceptInput(int, const QVariant& v) const
{
	return v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
}

template<class Container>
Container startAtZero(const Container& t)
{
	Container v = t;
	if (v.size()) {
		// if (!v.hasNanosecondPrecision())
		{
			const vip_double offset = -v.first().x();
			for (int i = 0; i < v.size(); ++i) {
				v[i].setX(v[i].x() + offset);
			}
		}
		// else
		//  {
		//  QVector<qint64> times = v.times();
		//  const qint64 first = times.first();
		//  for (int i = 0; i < times.size(); ++i) times[i] -= first;
		//  v.setTimes(times);
		//  }
	}
	return v;
}

void VipStartAtZero::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	if (!acceptInput(0, any.data())) {
		setError("wrong input type (" + QString(any.data().typeName()) + ")", VipProcessingObject::WrongInput);
		return;
	}

	QVariant out;
	if (any.data().userType() == qMetaTypeId<VipPointVector>())
		out = QVariant::fromValue(VipPointVector(startAtZero(any.value<VipPointVector>())));
	else if (any.data().userType() == qMetaTypeId<VipComplexPointVector>())
		out = QVariant::fromValue(VipComplexPointVector(startAtZero(any.value<VipComplexPointVector>())));

	VipAnyData data = create(out);
	data.setTime(any.time());
	data.mergeAttributes(any.attributes());
	outputAt(0)->setData(data);
}

VipStartYAtZero::VipStartYAtZero(QObject* parent)
  : VipProcessingObject(parent)
{
}

bool VipStartYAtZero::acceptInput(int, const QVariant& v) const
{
	return v.userType() == qMetaTypeId<VipPointVector>();
}

void VipStartYAtZero::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	if (!acceptInput(0, any.data())) {
		setError("wrong input type (" + QString(any.data().typeName()) + ")", VipProcessingObject::WrongInput);
		return;
	}

	VipPointVector v = any.value<VipPointVector>();

	// find min Y value
	if (v.size()) {
		double min_y = v.first().y();
		for (int i = 1; i < v.size(); ++i)
			if (v[i].y() < min_y)
				min_y = v[i].y();

		// apply offset
		for (int i = 0; i < v.size(); ++i)
			v[i].ry() -= min_y;
	}

	VipAnyData data = create(QVariant::fromValue(v));
	data.setTime(any.time());
	data.mergeAttributes(any.attributes());
	// data.setAttribute("Min Y value", min_y);
	outputAt(0)->setData(data);
}

VipXOffset::VipXOffset(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(0.0);
}

bool VipXOffset::acceptInput(int, const QVariant& v) const
{
	return v.userType() == qMetaTypeId<VipPointVector>() || v.userType() == qMetaTypeId<VipComplexPointVector>();
}

void VipXOffset::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	if (!acceptInput(0, any.data())) {
		setError("wrong input type (" + QString(any.data().typeName()) + ")", VipProcessingObject::WrongInput);
		return;
	}

	QVariant out;
	if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
		VipPointVector v = any.value<VipPointVector>();
		double offset = propertyAt(0)->value<double>();
		if (v.size())
			for (int i = 0; i < v.size(); ++i)
				v[i].setX(v[i].x() + offset);
		out = QVariant::fromValue(v);
	}
	else if (any.data().userType() == qMetaTypeId<VipComplexPointVector>()) {
		VipComplexPointVector v = any.value<VipComplexPointVector>();
		double offset = propertyAt(0)->value<double>();
		if (v.size())
			for (int i = 0; i < v.size(); ++i)
				v[i].setX(v[i].x() + offset);
		out = QVariant::fromValue(v);
	}

	VipAnyData data = create(out);
	data.setTime(any.time());
	outputAt(0)->setData(data);
}

#include <algorithm>
#include <vector>

#define PIX_SORT(a, b)                                                                                                                                                                                 \
	{                                                                                                                                                                                              \
		if ((a) > (b))                                                                                                                                                                         \
			std::swap((a), (b));                                                                                                                                                           \
	}

//----------------------------------------------------------------------------
// Median for 3 values
// ---------------------------------------------------------------------------
template<class T>
T opt_med3(T* p)
{
	PIX_SORT(p[0], p[1]);
	PIX_SORT(p[1], p[2]);
	PIX_SORT(p[0], p[1]);
	return (p[1]);
}

//----------------------------------------------------------------------------
// Median for 9 values
// ---------------------------------------------------------------------------
template<class T>
T opt_med9(T* p)
{
	PIX_SORT(p[1], p[2]);
	PIX_SORT(p[4], p[5]);
	PIX_SORT(p[7], p[8]);
	PIX_SORT(p[0], p[1]);
	PIX_SORT(p[3], p[4]);
	PIX_SORT(p[6], p[7]);
	PIX_SORT(p[1], p[2]);
	PIX_SORT(p[4], p[5]);
	PIX_SORT(p[7], p[8]);
	PIX_SORT(p[0], p[3]);
	PIX_SORT(p[5], p[8]);
	PIX_SORT(p[4], p[7]);
	PIX_SORT(p[3], p[6]);
	PIX_SORT(p[1], p[4]);
	PIX_SORT(p[2], p[5]);
	PIX_SORT(p[4], p[7]);
	PIX_SORT(p[4], p[2]);
	PIX_SORT(p[6], p[4]);
	PIX_SORT(p[4], p[2]);
	return (p[4]);
}

template<class T, class U>
void median_filter(const T* src, U* out, int w, int h)
{
	// first row
	const T* _src = src;
	T* _out = out;

	_out[0] = std::min(_src[0], _src[1]);
	_out[w - 1] = std::min(_src[w - 2], _src[w - 1]);
	for (int i = 1; i < w - 1; ++i) {
		T tmp[3];
		std::copy(_src + i - 1, _src + i + 2, tmp);
		_out[i] = opt_med3(tmp);
	}
	// last row
	_src = src + (h - 1) * w;
	_out = out + (h - 1) * w;

	_out[0] = std::min(_src[0], _src[1]);
	_out[w - 1] = std::min(_src[w - 2], _src[w - 1]);
	for (int i = 1; i < w - 1; ++i) {
		T tmp[3];
		std::copy(_src + i - 1, _src + i + 2, tmp);
		_out[i] = opt_med3(tmp);
	}

	// first column
	_src = src;
	_out = out;
	for (int y = 1; y < h - 1; ++y) {
		T tmp[3] = { _src[(y - 1) * w], _src[y * w], _src[(y + 1) * w] };
		_out[y * w] = opt_med3(tmp);
	}

	// last column
	_src = src + w - 1;
	_out = out + w - 1;
	for (int y = 1; y < h - 1; ++y) {
		T tmp[3] = { _src[(y - 1) * w], _src[y * w], _src[(y + 1) * w] };
		_out[y * w] = opt_med3(tmp);
	}

	// remaining
#pragma omp parallel for
	for (int y = 1; y < h - 1; ++y)
		for (int x = 1; x < w - 1; ++x) {
			const T* s1 = src + (y - 1) * w + x - 1;
			const T* s2 = src + (y)*w + x - 1;
			const T* s3 = src + (y + 1) * w + x - 1;
			T tmp[9] = { s1[0], s1[1], s1[2], s2[0], s2[1], s2[2], s3[0], s3[1], s3[2] };
			std::nth_element(tmp, tmp + 4, tmp + 9);
			out[x + y * w] = tmp[4];
		}
}

void FastMedian3x3::apply()
{
	VipAnyData in = inputAt(0)->data();
	VipNDArray ar = in.value<VipNDArray>();

	if (in.isEmpty() || ar.isEmpty() || ar.shapeCount() != 2) {
		setError("wrong input");
		return;
	}

	VipNDArray out(ar.dataType(), ar.shape());
	int this_type = ar.dataType();
	if (this_type == QMetaType::Bool)
		median_filter((bool*)ar.constData(), (bool*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::Char)
		median_filter((char*)ar.constData(), (char*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::SChar)
		median_filter((signed char*)ar.constData(), (signed char*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::UChar)
		median_filter((quint8*)ar.constData(), (quint8*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::Short)
		median_filter((qint16*)ar.constData(), (qint16*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::UShort)
		median_filter((quint16*)ar.constData(), (quint16*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::Int)
		median_filter((qint32*)ar.constData(), (qint32*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::UInt)
		median_filter((quint32*)ar.constData(), (quint32*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::Long)
		median_filter((long*)ar.constData(), (long*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::ULong)
		median_filter((unsigned long*)ar.constData(), (unsigned long*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::LongLong)
		median_filter((qint64*)ar.constData(), (qint64*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::ULongLong)
		median_filter((quint64*)ar.constData(), (quint64*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::Float)
		median_filter((float*)ar.constData(), (float*)out.data(), ar.shape(1), ar.shape(0));
	else if (this_type == QMetaType::Double)
		median_filter((double*)ar.constData(), (double*)out.data(), ar.shape(1), ar.shape(0));
	else {
		setError("wrong input data type");
		return;
	}

	VipAnyData any = create(QVariant::fromValue(out));
	any.mergeAttributes(in.attributes());
	any.setTime(in.time());
	outputAt(0)->setData(any);
}


static double _defaultSlidingTimeWindow = -1.;

double VipNumericValueToPointVector::defaultSlidingTimeWindow() {
	return _defaultSlidingTimeWindow;
}

void VipNumericValueToPointVector::setDefaultSlidingTimeWindow(double seconds)
{
	_defaultSlidingTimeWindow = seconds;
}

VipNumericValueToPointVector::VipNumericValueToPointVector(QObject* parent)
  : VipProcessingObject(parent)
{
	this->propertyName("Sliding_time_window")->setData(_defaultSlidingTimeWindow);
	this->propertyName("Restart_after")->setData(-1);
	this->outputAt(0)->setData(VipPointVector());
}

bool VipNumericValueToPointVector::acceptInput(int, const QVariant& v) const
{
	return v.canConvert(VIP_META(QMetaType::Double)) || v.userType() == qMetaTypeId<VipPoint>() || v.userType() == qMetaTypeId<VipPointVector>();
}

void VipNumericValueToPointVector::resetProcessing()
{
	m_vector.clear();
}

void VipNumericValueToPointVector::apply()
{

	VipAnyData any;
	while (inputAt(0)->hasNewData()) {
		any = inputAt(0)->data();

		if (any.data().userType() == qMetaTypeId<VipPointVector>())
			m_vector = any.value<VipPointVector>();
		else if (any.data().userType() == qMetaTypeId<VipPoint>())
			m_vector.append(any.value<VipPoint>());
		else if (any.time() != VipInvalidTime) {
			bool ok = false;
			double value = any.data().toDouble(&ok);

			if (!ok) {
				setError("input type is not convertible to a numerical value", VipProcessingObject::WrongInput);
				return;
			}
			m_vector.append(VipPoint(any.time(), value));
		}
	}

	double window = propertyAt(0)->value<double>();
	double restart_after = propertyAt(1)->value<double>();
	if (window > 0 && m_vector.size()) {
		// convert to nanoseconds
		window *= 1000000000;
		for (int i = 0; i < m_vector.size(); ++i) {
			double range = m_vector.last().x() - m_vector[i].x();
			if (range < window) {
				if (i != 0)
					m_vector.erase(m_vector.begin(), m_vector.begin() + i);
				break;
			}
		}
	}
	if (restart_after > 0 && m_vector.size() > 1) {
		vip_double diff = m_vector.last().x() - m_vector[m_vector.size() - 2].x();
		if (diff > restart_after * 1000000000ULL) {
			m_vector = m_vector.mid(m_vector.size() - 1);
		}
	}

	VipAnyData out = create(QVariant::fromValue(m_vector));
	if (m_vector.size())
		out.setTime(m_vector.last().x());
	out.mergeAttributes(any.attributes());
	outputAt(0)->setData(out);
}


static void serialize_VipNumericValueToPointVector(VipArchive& ar)
{
	if (ar.mode() == VipArchive::Write) {

		if (ar.start("VipNumericValueToPointVector")) {
			
			// save the default sliding time window as well
			ar.content("slidingTimeWindow", VipNumericValueToPointVector::defaultSlidingTimeWindow());

			ar.end();
		}
	}
	else {
		ar.save();
		if (ar.start("VipNumericValueToPointVector")) {
			double slidingTimeWindow = -1;
			ar.content("slidingTimeWindow", slidingTimeWindow);
			VipNumericValueToPointVector::setDefaultSlidingTimeWindow(slidingTimeWindow);
			
			ar.end();
		}
		else
			ar.restore();
	}
}



static int registerVipNumericValueToPointVector()
{
	vipRegisterSettingsArchiveFunctions(serialize_VipNumericValueToPointVector, serialize_VipNumericValueToPointVector);
	return 0;
}
static int _registerVipNumericValueToPointVector = vipAddInitializationFunction(registerVipNumericValueToPointVector);






static int __data_type(const QVariant& v)
{
	if (v.userType() == qMetaTypeId<VipNDArray>())
		return v.value<VipNDArray>().dataType();
	else if (v.userType() == qMetaTypeId<VipPointVector>())
		return QMetaType::Double;
	else if (v.userType() == qMetaTypeId<VipComplexPointVector>())
		return qMetaTypeId<complex_d>();
	else if (v.userType() == qMetaTypeId<complex_d>())
		return qMetaTypeId<complex_d>();
	else if (v.userType() == qMetaTypeId<complex_f>())
		return qMetaTypeId<complex_f>();
	else
		return v.userType();
}

class VipBaseDataFusion::PrivateData
{
public:
	PrivateData()
	  : resample(false)
	  , same_object_type(false)
	  , same_data_type(false)
	  , merge_point_vector(false)
	  , resize_array_type(Vip::NoInterpolation)
	{
	}

	QVector<VipAnyData> inputs;
	QVector<VipNDArray> arrays;

	bool resample;
	bool same_object_type;
	bool same_data_type;
	bool merge_point_vector;
	QList<int> possible_types;
	QList<int> accepted_inputs;
	Vip::InterpolationType resize_array_type;
};

VipBaseDataFusion::VipBaseDataFusion(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	propertyAt(0)->setData(QString("intersection"));
}

VipBaseDataFusion::~VipBaseDataFusion()
{
}

void VipBaseDataFusion::setAceptedInputs(const QList<int>& input_types)
{
	d_data->accepted_inputs = input_types;
}
const QList<int>& VipBaseDataFusion::acceptedInputs() const
{
	return d_data->accepted_inputs;
}

void VipBaseDataFusion::setResampleEnabled(bool resample, bool merge_point_vector)
{
	d_data->resample = resample;
	d_data->merge_point_vector = merge_point_vector;
}
bool VipBaseDataFusion::mergePointVectors() const
{
	return d_data->merge_point_vector;
}
bool VipBaseDataFusion::resampleEnabled() const
{
	return d_data->resample;
}

void VipBaseDataFusion::setWorkOnSameObjectType(bool enable)
{
	d_data->same_object_type = enable;
}
bool VipBaseDataFusion::workOnSameObjectType() const
{
	return d_data->same_object_type;
}

VipAnyData VipBaseDataFusion::create(const QVariant& data, const QVariantMap& attr) const
{
	VipAnyData res = VipProcessingObject::create(data, attr);
	if (inputs().size()) {
		qint64 time = inputs().first().time();
		for (int i = 0; i < inputs().size(); ++i) {
			res.mergeAttributes(inputs()[i].attributes());
			qint64 t = inputs()[i].time();
			if (t != VipInvalidTime) {
				if (time == VipInvalidTime)
					time = t;
				else
					time = qMax(time, t);
			}
		}
		res.setTime(time);
	}

	return res;
}

void VipBaseDataFusion::setSameDataType(bool enable, const QList<int>& possible_types)
{
	d_data->same_data_type = enable;
	d_data->possible_types = possible_types;
}
bool VipBaseDataFusion::sameDataType() const
{
	return d_data->same_data_type;
}
const QList<int>& VipBaseDataFusion::possibleDataTypes() const
{
	return d_data->possible_types;
}

const QVector<VipAnyData>& VipBaseDataFusion::inputs() const
{
	return d_data->inputs;
}

void VipBaseDataFusion::setResizeArrayType(Vip::InterpolationType type)
{
	d_data->resize_array_type = type;
}
Vip::InterpolationType VipBaseDataFusion::resizeArrayType() const
{
	return d_data->resize_array_type;
}

void VipBaseDataFusion::apply()
{
	if (inputCount() == 0) {
		setError("input count is 0");
		return;
	}
	// else if (inputCount() == 1) {
	//  d_data->inputs.resize(1);
	//  d_data->inputs[0] = inputAt(0)->data();
	//  mergeData(0,0);
	//  return;
	//  }

	// copy inputs
	int count = inputCount();
	QVector<VipAnyData> inputs(count);
	for (int i = 0; i < count; ++i) {
		inputs[i] = inputAt(i)->data();
		// check input type accepted
		if (d_data->accepted_inputs.size() && d_data->accepted_inputs.indexOf(inputs[i].data().userType()) < 0) {
			setError("input type " + QString(inputs[i].data().typeName()) + " not accepted");
			return;
		}
	}

	int otype = 0;
	if (d_data->same_object_type) {
		// check that all inputs have the same object type (all VipNDArray, or all VipPointVector,...)
		otype = inputs[0].data().userType();
		for (int i = 1; i < count; ++i) {
			if (inputs[i].data().userType() != otype) {
				setError("input types are different");
				return;
			}
		}
	}

	int dtype = 0;
	if (d_data->same_data_type) {
		// check that all inputs can be converted to the same data type
		dtype = __data_type(inputs[0].data());
		for (int i = 1; i < count; ++i) {
			dtype = vipHigherArrayType(dtype, __data_type(inputs[i].data()));
			if (dtype == 0) {
				setError("input types are not compatibles");
				return;
			}
		}
	}

	if (dtype) {
		// check that the data type is compatible with given types
		if (!d_data->possible_types.isEmpty())
			if (!(dtype = vipHigherArrayType(dtype, d_data->possible_types))) {
				setError("input types are not convertible to requested types");
				return;
			}
	}

	// resample inputs
	if (d_data->resample) {
		QList<VipPointVector> pvectors;
		QList<VipComplexPointVector> cvectors;
		QVector<VipNDArray> arrays;
		// compute max shape
		VipNDArrayShape sh;

		for (int i = 0; i < count; ++i) {
			const QVariant v = inputs[i].data();
			if (v.userType() == qMetaTypeId<VipNDArray>()) {
				if (sh.isEmpty())
					sh = v.value<VipNDArray>().shape();
				else {
					VipNDArrayShape tmp = v.value<VipNDArray>().shape();
					if (tmp.size() != sh.size()) {
						setError("different input arrays dimensions");
						return;
					}
					else {
						for (int j = 0; j < tmp.size(); ++j)
							sh[j] = qMax(sh[j], tmp[j]);
					}
				}
				arrays.append(v.value<VipNDArray>());
			}
			else if (v.userType() == qMetaTypeId<VipPointVector>()) {
				pvectors.append(v.value<VipPointVector>());
			}
			else if (v.userType() == qMetaTypeId<VipComplexPointVector>()) {
				cvectors.append(v.value<VipComplexPointVector>());
			}
		}

		QString time_range = propertyAt(0)->value<QString>();
		// resample vectors
		ResampleStrategies s = ResampleIntersection | ResampleInterpolation;
		if (time_range == "union")
			s = ResampleUnion | ResampleInterpolation;
		if (!d_data->merge_point_vector) {
			vipResampleVectors(pvectors, s);
			vipResampleVectors(cvectors, s);
		}
		else if (pvectors.size() || cvectors.size()) {
			if (!vipResampleVectors(pvectors, cvectors, s)) {
				// vip_debug("points: %i, %i\n",pvectors[0].size(),pvectors[1].size());
				setError("unable to resample point vectors");
				return;
			}
		}

		// resample arrays using the internal vector of array, and cast to dtype if necessary
		if (d_data->arrays.size() != arrays.size())
			d_data->arrays.resize(arrays.size());
		for (int i = 0; i < arrays.size(); ++i) {
			bool valid_type = ((dtype == 0 && d_data->arrays[i].dataType() == arrays[i].dataType()) || (d_data->arrays[i].dataType() == dtype));
			if (!(valid_type && d_data->arrays[i].shape() == sh)) {
				if (dtype)
					d_data->arrays[i] = VipNDArray(dtype, sh);
				else
					d_data->arrays[i] = VipNDArray(arrays[i].dataType(), sh);
			}
			arrays[i].resize(d_data->arrays[i], resizeArrayType());
		}

		// copy back to input data
		int i_a = 0, i_p = 0, i_c = 0;
		for (int i = 0; i < count; ++i) {
			const QVariant v = inputs[i].data();
			if (v.userType() == qMetaTypeId<VipNDArray>())
				inputs[i].setData(QVariant::fromValue(d_data->arrays[i_a++]));
			else if (v.userType() == qMetaTypeId<VipPointVector>())
				inputs[i].setData(QVariant::fromValue(pvectors[i_p++]));
			else if (v.userType() == qMetaTypeId<VipComplexPointVector>())
				inputs[i].setData(QVariant::fromValue(cvectors[i_c++]));
		}
	}

	// we have resampled data, now cast them
	if (dtype) {
		VipNDArray tmp(dtype, VipNDArrayShape());
		bool is_numeric = tmp.isNumeric();
		bool is_complex = tmp.isComplex();

		QVector<VipNDArray> arrays;

		// first, cast everything but VipNDArray
		for (int i = 0; i < count; ++i) {
			const QVariant v = inputs[i].data();
			if (v.userType() == qMetaTypeId<VipNDArray>()) {
				if (!d_data->resample)
					arrays.append(v.value<VipNDArray>());
			}
			else if (v.userType() == qMetaTypeId<VipPointVector>()) {
				if (is_numeric) {
					// right type, nothing to do
				}
				else if (is_complex) {
					// convert to complex
					VipPointVector vector = v.value<VipPointVector>();
					VipComplexPointVector cvector(vector.size());
					for (const VipPoint& p : vector)
						cvector[i] = VipComplexPoint(p.x(), p.y());
					inputs[i].setData(QVariant::fromValue(cvector));
				}
				else {
					setError("cannot convert from type VipPointVector to type " + QString(vipTypeName(dtype)));
					return;
				}
			}
			else if (v.userType() == qMetaTypeId<VipComplexPointVector>()) {
				if (!is_complex) {
					setError("cannot convert from type VipComplexPointVector to type " + QString(vipTypeName(dtype)));
					return;
				}
			}
			else if (v.canConvert(VIP_META(QMetaType::Double)) && is_complex) {
				if (dtype == qMetaTypeId<complex_f>())
					inputs[i].setData(QVariant::fromValue(complex_f(v.toFloat())));
				else
					inputs[i].setData(QVariant::fromValue(complex_d(v.toDouble())));
			}
			else {
				QVariant _tmp = v;
				if (!_tmp.convert(VIP_META(dtype))) {
					setError("cannot convert from type " + QString(v.typeName()) + " to type " + QString(vipTypeName(dtype)));
					return;
				}
				inputs[i].setData(_tmp);
			}
		}

		// cast VipNDArray
		if (!d_data->resample) // id resample is enabled, the arrays are already casted to the right type
		{
			if (d_data->arrays.size() != arrays.size())
				d_data->arrays.resize(arrays.size());
			int i_a = 0;
			for (int i = 0; i < count; ++i) {
				const QVariant v = inputs[i].data();
				if (v.userType() == qMetaTypeId<VipNDArray>()) {
					const VipNDArray ar = arrays[i_a++];
					if (d_data->arrays[i].dataType() == dtype && d_data->arrays[i].shape() == ar.shape())
						ar.convert(d_data->arrays[i]);
					else
						d_data->arrays[i] = ar.convert(dtype);
					inputs[i].setData(QVariant::fromValue(d_data->arrays[i]));
				}
			}
		}
	}

	d_data->inputs = inputs;
	mergeData(otype, dtype);
}

QString VipBaseDataFusion::startPrefix(const QStringList& names)
{
	if (names.isEmpty())
		return QString();
	QString prefix = names.first();
	bool all_same = true;
	for (int i = 1; i < names.size(); ++i) {
		const QString n = names[i];
		int s = qMin(prefix.size(), n.size());
		int j = 0;
		for (; j < s; ++j) {
			if (n[j] != prefix[j])
				break;
		}
		prefix = prefix.mid(0, j);
		all_same = all_same && (prefix.size() == n.size());
	}

	// we have the prefix. from it, remove the last found word (use / separator)
	if (!prefix.isEmpty() && !prefix.endsWith("/")) {
		int index = prefix.lastIndexOf("/");
		if (index > 0)
			prefix = prefix.mid(0, index);
		return prefix;
	}

	if (all_same)
		return QString();
	return prefix;
}
QString VipBaseDataFusion::startPrefix(const QVector<VipAnyData>& inputs)
{
	QStringList lst;
	for (int i = 0; i < inputs.size(); ++i)
		lst.append(inputs[i].name());
	return startPrefix(lst);
}

template<class T>
bool is_min(const T& t1, const T& t2)
{
	return t1 < t2;
}
template<>
bool is_min(const complex_d& t1, const complex_d& t2)
{
	return std::abs(t1) < std::abs(t2);
}
template<class T>
T _div(const T& v, int div)
{
	return v / div;
}
template<>
complex_d _div(const complex_d& v, int div)
{
	return complex_d(v.real() / div, v.imag() / div);
}
template<class T>
T min_val(const T& t1, const T& t2)
{
	return t1 < t2 ? t1 : t2;
}
template<class T>
T max_val(const T& t1, const T& t2)
{
	return t1 > t2 ? t1 : t2;
}
template<>
complex_d min_val(const complex_d& t1, const complex_d& t2)
{
	return std::abs(t1) < std::abs(t2) ? t1 : t2;
}
template<>
complex_d max_val(const complex_d& t1, const complex_d& t2)
{
	return std::abs(t1) > std::abs(t2) ? t1 : t2;
}

template<class T>
T median(std::vector<T>& v)
{
	size_t n = v.size() / 2;
	std::nth_element(v.begin(), v.begin() + n, v.end(), is_min<T>);
	return v[n];
}

template<class T>
VipNDArray sampleFeatures(const QVector<VipNDArray>& arrays, const QString& algo)
{
	VipNDArrayType<T> res(arrays.first().shape());
	T* out = res.ptr();

	int size = res.size();
	std::vector<const T*> ins(arrays.size());
	for (int i = 0; i < arrays.size(); ++i)
		ins[i] = (const T*)arrays[i].constData();

	if (algo == "min") {
		// min
		for (int i = 0; i < size; ++i) {
			T min = ins[0][i];
			for (size_t j = 1; j < ins.size(); ++j)
				min = min_val(min, ins[j][i]);
			out[i] = min;
		}
	}
	else if (algo == "max") {
		// max
		for (int i = 0; i < size; ++i) {
			T max = ins[0][i];
			for (size_t j = 1; j < ins.size(); ++j)
				max = max_val(max, ins[j][i]);
			out[i] = max;
		}
	}
	else if (algo == "mean") {
		// mean
		for (int i = 0; i < size; ++i) {
			T v = ins[0][i];
			for (size_t j = 1; j < ins.size(); ++j)
				v += ins[j][i];
			out[i] = v / (double)ins.size(); //_div(v , (int)ins.size());
		}
	}
	else if (algo == "median") {
		// median
		std::vector<T> values(ins.size());
		for (int i = 0; i < size; ++i) {
			for (size_t j = 0; j < ins.size(); ++j)
				values[j] = ins[j][i];
			out[i] = median(values);
		}
	}

	return res;
}

static QVector<VipNDArray> __extractArrays(const QVector<VipAnyData>& data)
{
	QVector<VipNDArray> res;
	for (int i = 0; i < data.size(); ++i) {
		if (data[i].data().userType() == qMetaTypeId<VipNDArray>())
			res.append(data[i].data().value<VipNDArray>());
		else if (data[i].data().userType() == qMetaTypeId<VipPointVector>())
			res.append(vipExtractYValues(data[i].data().value<VipPointVector>()));
		else if (data[i].data().userType() == qMetaTypeId<VipComplexPointVector>())
			res.append(vipExtractYValues(data[i].data().value<VipComplexPointVector>()));
	}
	return res;
}

class VipSamplesFeature::PrivateData
{
public:
	// QVector<VipNDArray> arrays;
};

VipSamplesFeature::VipSamplesFeature(QObject* parent)
  : VipBaseDataFusion(parent)
{
	// VIP_CREATE_PRIVATE_DATA();
	setWorkOnSameObjectType(true);
	setSameDataType(true, QList<int>() << QMetaType::Int << QMetaType::Double << qMetaTypeId<complex_d>());
	setResampleEnabled(true);

	propertyAt(1)->setData(QString("max"));
	topLevelInputAt(0)->toMultiInput()->resize(2);
	topLevelInputAt(0)->toMultiInput()->setMaxSize(20);
	topLevelInputAt(0)->toMultiInput()->setMinSize(2);
}
VipSamplesFeature::~VipSamplesFeature()
{
	// delete d_data;
}

void VipSamplesFeature::setOutput(const QVariant& v)
{
	VipAnyData any = create(v);
	// QString prefix = startPrefix(inputs());
	if (inputs().size() == 1)
		any.setName(propertyAt(1)->value<QString>() + "(" + inputs()[0].name() + ")");
	else if (inputs().size() == 2) {
		QString left = inputs()[0].name();  // left.remove(0,prefix.size());
		QString right = inputs()[1].name(); // right.remove(0,prefix.size());
		any.setName(propertyAt(1)->value<QString>() + "(" + left + " , " + right + ")");
	}
	else {
		any.setName(propertyAt(1)->value<QString>());
	}
	outputAt(0)->setData(any);
}

void VipSamplesFeature::mergeData(int data_type, int sub_data_type)
{
	if (data_type == 0) {
		setError("wrong input type");
		return;
	}

	// 0=min, 1=max, 2=mean, 3=median
	QString algo = propertyAt(1)->value<QString>();

	if (data_type == QMetaType::Double) {
		// double type
		if (algo == "min") {
			double v = inputs()[0].value<double>();
			for (int i = 1; i < inputCount(); ++i)
				v = qMin(v, inputs()[i].value<double>());
			setOutput(v);
		}
		else if (algo == "max") {
			double v = inputs()[0].value<double>();
			for (int i = 1; i < inputCount(); ++i)
				v = qMax(v, inputs()[i].value<double>());
			setOutput((v));
		}
		else if (algo == "mean") {
			double v = 0;
			for (int i = 0; i < inputCount(); ++i)
				v += inputs()[i].value<double>();
			setOutput((v / inputCount()));
		}
		else {
			std::vector<double> values(inputCount());
			for (int i = 0; i < inputCount(); ++i)
				values[i] = inputs()[i].value<double>();
			double res = median(values);
			setOutput(res);
		}
	}
	else if (data_type == qMetaTypeId<complex_d>()) {
		// complex type
		if (algo == "min") {
			complex_d v = inputs()[0].value<complex_d>();
			for (int i = 1; i < inputCount(); ++i)
				v = min_val(v, inputs()[i].value<complex_d>());
			setOutput(QVariant::fromValue(v));
		}
		else if (algo == "max") {
			complex_d v = inputs()[0].value<complex_d>();
			for (int i = 1; i < inputCount(); ++i)
				v = max_val(v, inputs()[i].value<complex_d>());
			setOutput(QVariant::fromValue(v));
		}
		else if (algo == "mean") {
			complex_d v = 0;
			for (int i = 0; i < inputCount(); ++i)
				v += inputs()[i].value<complex_d>();
			setOutput(QVariant::fromValue(v / (double)inputCount()));
		}
		else {
			std::vector<complex_d> values(inputCount());
			for (int i = 0; i < inputCount(); ++i)
				values[i] = inputs()[i].value<complex_d>();
			// complex_d res =
			median(values);
			setOutput(QVariant::fromValue(values[values.size() / 2]));
		}
	}
	else if (data_type == qMetaTypeId<VipNDArray>()) {
		if (sub_data_type == 0) {
			setError("unable to convert input arrays: incompatible types");
			return;
		}
		// apply algo
		VipNDArray res;
		QVector<VipNDArray> arrays = __extractArrays(inputs());
		if (sub_data_type == QMetaType::Int)
			res = sampleFeatures<int>(arrays, algo);
		else if (sub_data_type == QMetaType::Double)
			res = sampleFeatures<double>(arrays, algo);
		else if (sub_data_type == qMetaTypeId<complex_d>())
			res = sampleFeatures<complex_d>(arrays, algo);
		setOutput(QVariant::fromValue(res));
	}
	else if (data_type == qMetaTypeId<VipPointVector>()) {
		VipNDArray res = sampleFeatures<vip_double>(__extractArrays(inputs()), algo);
		VipPointVector vec = inputs()[0].value<VipPointVector>();
		vipSetYValues(vec, res);
		setOutput(QVariant::fromValue(vec));
	}
	else if (data_type == qMetaTypeId<VipComplexPointVector>()) {
		VipNDArray res = sampleFeatures<complex_d>(__extractArrays(inputs()), algo);
		VipComplexPointVector vec = inputs()[0].value<VipComplexPointVector>();
		vipSetYValues(vec, res);
		setOutput(QVariant::fromValue(vec));
	}
}

VipRunningAverage::VipRunningAverage()
  : VipProcessingObject()
{
	propertyName("Window")->setData(3);
	// m_extract.topLevelInputAt(0)->toMultiInput()->resize(3);
	m_extract.propertyName("Feature")->setData(QString("mean"));
	m_extract.topLevelInputAt(0)->toMultiInput()->setMinSize(1);
	m_extract.topLevelInputAt(0)->toMultiInput()->setMaxSize(INT_MAX);
}

void VipRunningAverage::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		any = inputAt(0)->probe();
	int window = propertyName("Window")->value<int>();
	if (window <= 0)
		window = 1;

	{
		QMutexLocker lock(&m_mutex);
		m_lst.push_back(any);
		while (m_lst.size() > window)
			m_lst.pop_front();

		m_extract.topLevelInputAt(0)->toMultiInput()->resize(m_lst.size());
		for (int i = 0; i < m_lst.size(); ++i)
			m_extract.inputAt(i)->setData(m_lst[i]);
	}

	m_extract.update();

	VipAnyData out = m_extract.outputAt(0)->data();
	out.mergeAttributes(this->attributes());
	out.mergeAttributes(any.attributes());
	out.setSource((qint64)this);
	out.setTime(any.time());

	outputAt(0)->setData(out);
}

void VipRunningAverage::resetProcessing()
{
	QMutexLocker lock(&m_mutex);
	m_lst.clear();
}

VipRunningMedian::VipRunningMedian()
  : VipProcessingObject()
{
	propertyName("Window")->setData(3);
	// m_extract.topLevelInputAt(0)->toMultiInput()->resize(3);
	m_extract.propertyName("Feature")->setData(QString("median"));
	m_extract.topLevelInputAt(0)->toMultiInput()->setMinSize(1);
	m_extract.topLevelInputAt(0)->toMultiInput()->setMaxSize(INT_MAX);
}

void VipRunningMedian::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		any = inputAt(0)->probe();
	int window = propertyName("Window")->value<int>();
	if (window <= 0)
		window = 1;

	{
		QMutexLocker lock(&m_mutex);
		m_lst.push_back(any);
		while (m_lst.size() > window)
			m_lst.pop_front();

		m_extract.topLevelInputAt(0)->toMultiInput()->resize(m_lst.size());
		for (int i = 0; i < m_lst.size(); ++i)
			m_extract.inputAt(i)->setData(m_lst[i]);
	}

	m_extract.update();

	VipAnyData out = m_extract.outputAt(0)->data();
	out.mergeAttributes(this->attributes());
	out.mergeAttributes(any.attributes());
	out.setSource((qint64)this);
	out.setTime(any.time());

	outputAt(0)->setData(out);
}

void VipRunningMedian::resetProcessing()
{
	QMutexLocker lock(&m_mutex);
	m_lst.clear();
}

VipExtractBoundingBox::VipExtractBoundingBox(QObject* parent)
  : VipProcessingObject(parent)
{
	this->propertyAt(0)->setData(QString("BBox"));
	this->outputAt(0)->setData(VipSceneModel());
}

bool VipExtractBoundingBox::acceptInput(int // index
					,
					const QVariant& v) const
{
	return v.userType() == qMetaTypeId<VipNDArray>();
}

void VipExtractBoundingBox::apply()
{
	VipAnyData any = inputAt(0)->data();
	if (any.isEmpty())
		return;

	if (!acceptInput(0, any.data())) {
		setError("wrong input type (" + QString(any.data().typeName()) + ")", VipProcessingObject::WrongInput);
		return;
	}

	VipNDArray ar = any.value<VipNDArray>().toInt32();
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("wrong input array shape", VipProcessingObject::WrongInput);
		return;
	}

	ar.detach();

	VipNDArrayTypeView<int> img = ar;

	// first pass to get the maximum value
	int* ptr = img.ptr();
	int size = img.size();
	int max = ptr[0];
	for (int i = 1; i < size; ++i)
		max = qMax(max, ptr[i]);

	// build the bboxes
	QVector<QRect> rects(max + 1);
	for (int y = 0; y < img.shape(0); ++y)
		for (int x = 0; x < img.shape(1); ++x) {
			const int pixel = img(vipVector(y, x));
			if (pixel != 0) {
				QRect& r = rects[pixel];
				if (r.isEmpty()) {
					r = QRect(x, y, 1, 1);
				}
				else {
					r.setTop(qMin(r.top(), y));
					r.setBottom(qMax(r.bottom(), y));
					r.setLeft(qMin(r.left(), x));
					r.setRight(qMax(r.right(), x));
				}
			}
		}

	// build the scene model
	VipSceneModel scene;
	QString label = propertyAt(0)->data().value<QString>();

	for (int i = 0; i < rects.size(); ++i) {
		const QRect r = rects[i];
		if (!r.isEmpty()) {
			scene.add(label, VipShape(QRectF(r)));
		}
	}

	VipAnyData data = create(QVariant::fromValue(scene));
	data.setTime(any.time());
	outputAt(0)->setData(data);
}

#include <vector>
static vip_double median(std::vector<vip_double>& vec)
{
	typedef std::vector<vip_double>::size_type vec_sz;
	vec_sz size = vec.size();
	if (size == 0)
		return 0;
	std::sort(vec.begin(), vec.end());
	vec_sz mid = size / 2;
	return size % 2 == 0 ? (vec[mid] + vec[mid - 1]) / 2 : vec[mid];
}

template<class Vector>
VipInterval __FindTemporalVectorsBoundaries(const QList<Vector>& vectors, vip_double* min_sampling)
{
	if (vectors.size() == 0)
		return VipInterval();

	vip_double start = std::numeric_limits<vip_double>::max();
	vip_double end = -std::numeric_limits<vip_double>::max();
	vip_double sampling = std::numeric_limits<vip_double>::max();
	std::vector<vip_double> samplings;
	samplings.reserve(20);

	// find the min time, max time and minimum sampling time
	for (int i = 0; i < vectors.size(); ++i) {
		const Vector& vec = vectors[i];
		if (vec.size()) {
			samplings.clear();
			start = qMin(vec[0].x(), start);
			end = qMax(vec[vec.size() - 1].x(), end);
			if (vec.size() > 1 && min_sampling) {
				for (int j = 1; j < vec.size(); ++j) {
					// for the sampling time, we use the median value of the 10 first sampling times to avoid extremums
					vip_double samp = vec[j].x() - vec[j - 1].x();
					if (samp > 0) {
						samplings.push_back(samp);
						if (samplings.size() > 10)
							break;
					}
				}

				if (samplings.size()) {
					vip_double med = median(samplings);
					sampling = qMin(med, sampling);
				}
			}
		}
	}

	if (min_sampling)
		*min_sampling = sampling;

	return VipInterval(start, end);
}

VipInterval vipFindTemporalVectorsBoundaries(const QList<VipPointVector>& vectors, vip_double* min_sampling)
{
	return __FindTemporalVectorsBoundaries(vectors, min_sampling);
}

VipInterval vipFindTemporalVectorsBoundaries(const QList<VipComplexPointVector>& vectors, vip_double* min_sampling)
{
	return __FindTemporalVectorsBoundaries(vectors, min_sampling);
}

VipInterval vipFindTemporalVectorsBoundaries(const QList<VipPointVector>& vectors, const QList<VipComplexPointVector>& cvectors, vip_double* min_sampling)
{
	vip_double sampling = 0;
	VipInterval bounds = vipFindTemporalVectorsBoundaries(vectors, min_sampling ? &sampling : nullptr);

	vip_double csampling = 0;
	VipInterval cbounds = vipFindTemporalVectorsBoundaries(cvectors, min_sampling ? &csampling : nullptr);

	if (min_sampling) {
		if (vectors.isEmpty())
			*min_sampling = csampling;
		else if (cvectors.isEmpty())
			*min_sampling = sampling;
		else
			*min_sampling = qMin(sampling, csampling);
	}

	VipInterval b;
	if (vectors.isEmpty())
		b = cbounds;
	else if (cvectors.isEmpty())
		b = bounds;
	else
		b = VipInterval(qMin(bounds.minValue(), cbounds.minValue()), qMax(bounds.maxValue(), cbounds.maxValue()));
	return b;
}

template<class Vector, class Sample>
Vector __ResampleTemporalVector(const Vector& vector, const VipInterval& range, vip_double sampling)
{
	if (sampling <= 0)
		return Vector();

	int size = floor(range.width() / sampling + 1);
	if (size > 200000000 || size < 0) {
		VIP_LOG_WARNING("Unable to resample array: size too big");
		return Vector();
	}

	if (vector.size() == 0) {
		Vector res(size);
		for (int i = 0; i < size; ++i) {
			res[i] = Sample(range.minValue() + i * sampling, 0);
			// res.setValues(i, range.first + i*sampling, 0);
		}
		return res;
	}

	if (vector[0].x() >= range.maxValue() || vector.size() == 1) {
		Vector res(size);
		for (int i = 0; i < size; ++i)
			res[i] = Sample(range.minValue() + i * sampling, vector.first().y());
		return res;
	}
	else if (vector.last().x() <= range.minValue()) {
		Vector res(size);
		for (int i = 0; i < size; ++i)
			res[i] = Sample(range.minValue() + i * sampling, vector.last().y());
		return res;
	}
	else {
		int pos = 0;
		Vector res(size);
		for (int i = 0; i < size; ++i) {
			vip_double x = range.minValue() + i * sampling;

			if (x <= vector[0].x())
				res[i] = Sample(x, vector.first().y());
			else if (x >= vector[vector.size() - 1].x())
				res[i] = Sample(x, vector.last().y());
			else {
				vip_double x_val = vector[pos].x();
				vip_double next = vector[pos + 1].x();
				while (next < x) {
					++pos;
					x_val = next;
					next = vector[pos + 1].x();
				}

				double factor = (x - x_val) / double(next - x_val);
				res[i] = Sample(x, (1 - factor) * vector[pos].y() + (factor)*vector[pos + 1].y());
			}
		}
		return res;
	}
}

VipPointVector vipResampleTemporalVector(const VipPointVector& vector, const VipInterval& range, vip_double sampling)
{
	return __ResampleTemporalVector<VipPointVector, VipPoint>(vector, range, sampling);
}

VipComplexPointVector vipResampleTemporalVector(const VipComplexPointVector& vector, const VipInterval& range, vip_double sampling)
{
	return __ResampleTemporalVector<VipComplexPointVector, VipComplexPoint>(vector, range, sampling);
}

void vipResampleTemporalVector(VipPointVector& vector, VipComplexPointVector& cvector, const VipInterval& range, vip_double sampling)
{
	vector = vipResampleTemporalVector(vector, range, sampling);
	cvector = vipResampleTemporalVector(cvector, range, sampling);
}

QList<VipPointVector> vipResampleTemporalVectors(const QList<VipPointVector>& vectors, const VipInterval& range, vip_double sampling)
{
	if (vectors.isEmpty())
		return vectors;

	if (range.isNull() || sampling <= 0)
		return QList<VipPointVector>();

	QList<VipPointVector> res;
	for (int i = 0; i < vectors.size(); ++i) {
		res.append(vipResampleTemporalVector(vectors[i], range, sampling));
	}
	return res;
}

QList<VipComplexPointVector> vipResampleTemporalVectors(const QList<VipComplexPointVector>& vectors, const VipInterval& range, vip_double sampling)
{
	if (vectors.isEmpty())
		return vectors;

	if (range.isNull() || sampling <= 0)
		return QList<VipComplexPointVector>();

	QList<VipComplexPointVector> res;
	for (int i = 0; i < vectors.size(); ++i) {
		res.append(vipResampleTemporalVector(vectors[i], range, sampling));
	}
	return res;
}

void vipResampleTemporalVectors(QList<VipPointVector>& vectors, QList<VipComplexPointVector>& cvectors, const VipInterval& range, vip_double sampling)
{
	vectors = vipResampleTemporalVectors(vectors, range, sampling);
	cvectors = vipResampleTemporalVectors(cvectors, range, sampling);
}

VipNDArray vipResampleTemporalVectorsAsNDArray(const QList<VipPointVector>& vectors, const VipInterval& range, vip_double sampling)
{
	QList<VipPointVector> tmp = vipResampleTemporalVectors(vectors, range, sampling);
	if (tmp.isEmpty())
		return VipNDArray();

	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(tmp.first().size(), tmp.size() + 1));
	vip_double* values = (vip_double*)res.data();
	int width = tmp.size() + 1;

	// copy X values;
	const VipPointVector& first = tmp.first();
	for (int i = 0; i < first.size(); ++i)
		values[i * width] = first[i].x();

	// copy all y values
	for (int j = 0; j < tmp.size(); ++j) {
		const VipPointVector& vec = tmp[j];
		int start = j + 1;
		for (int i = 0; i < vec.size(); ++i)
			values[start + i * width] = vec[i].y();
	}

	return res;
}

QList<VipPointVector> vipResampleTemporalVectors(const QList<VipPointVector>& vectors)
{
	if (vectors.size() < 2)
		return vectors;

	vip_double sampling = -1;
	VipInterval bounds = vipFindTemporalVectorsBoundaries(vectors, &sampling);
	if (sampling < 0)
		return QList<VipPointVector>();

	return vipResampleTemporalVectors(vectors, bounds, sampling);
}

QList<VipComplexPointVector> vipResampleTemporalVectors(const QList<VipComplexPointVector>& vectors)
{
	if (vectors.size() < 2)
		return vectors;

	vip_double sampling = -1;
	VipInterval bounds = vipFindTemporalVectorsBoundaries(vectors, &sampling);
	if (sampling < 0)
		return QList<VipComplexPointVector>();

	return vipResampleTemporalVectors(vectors, bounds, sampling);
}

bool vipResampleTemporalVectors(QList<VipPointVector>& vectors, QList<VipComplexPointVector>& cvectors)
{
	if (vectors.size() + cvectors.size() == 1)
		return true;
	vip_double sampling = -1;
	VipInterval bounds = vipFindTemporalVectorsBoundaries(vectors, cvectors, &sampling);
	if (sampling < 0)
		return false;
	vipResampleTemporalVectors(vectors, cvectors, bounds, sampling);
	return vectors.size() > 0 || cvectors.size() > 0;
}

VipNDArray vipResampleTemporalVectorsAsNDArray(const QList<VipPointVector>& vectors)
{
	QList<VipPointVector> tmp = vipResampleTemporalVectors(vectors);
	if (tmp.isEmpty())
		return VipNDArray();

	VipNDArray res(qMetaTypeId<vip_double>(), vipVector(tmp.first().size(), tmp.size() + 1));
	vip_double* values = (vip_double*)res.data();
	int width = tmp.size() + 1;

	// copy X values;
	const VipPointVector& first = tmp.first();
	for (int i = 0; i < first.size(); ++i)
		values[i * width] = first[i].x();

	// copy all y values
	for (int j = 0; j < tmp.size(); ++j) {
		const VipPointVector& vec = tmp[j];
		int start = j + 1;
		for (int i = 0; i < vec.size(); ++i)
			values[start + i * width] = vec[i].y();
	}

	return res;
}

VipNumericOperation::VipNumericOperation(QObject* parent)
  : VipBaseDataFusion(parent)
{
	propertyAt(1)->setData(QString("+"));
	this->setSameDataType(true, QList<int>() << QMetaType::Int << QMetaType::Double << qMetaTypeId<complex_d>());
	this->setResampleEnabled(true, true);
	this->topLevelInputAt(0)->toMultiInput()->resize(2);
	this->topLevelInputAt(0)->toMultiInput()->setMaxSize(10);
	this->topLevelInputAt(0)->toMultiInput()->setMinSize(2);
}

template<class T>
T divide(const T& a, const T& b)
{
	return b ? a / b : vipNan();
}
complex_f divide(const complex_f& a, const complex_f& b)
{
	return (b.real() != 0 || b.imag() != 0) ? a / b : complex_f(vipNan(), vipNan());
}
complex_d divide(const complex_d& a, const complex_d& b)
{
	return (b.real() != 0 || b.imag() != 0) ? a / b : complex_d(vipNan(), vipNan());
}

struct AND
{
	template<class T>
	T operator()(const T& a, const T& b) const
	{
		return a & b;
	}
};
struct OR
{
	template<class T>
	T operator()(const T& a, const T& b) const
	{
		return a | b;
	}
};
struct XOR
{
	template<class T>
	T operator()(const T& a, const T& b) const
	{
		return a ^ b;
	}
};
template<class T,
	 bool is_float =
	   std::is_same<T, vip_double>::value || std::is_same<T, float>::value || std::is_same<T, double>::value || std::is_same<T, complex_f>::value || std::is_same<T, complex_d>::value>
struct BIN_OP
{
	template<class OP>
	T operator()(const T& a, const T& b, OP op) const
	{
		return op(a, b);
	}
};
template<class T>
struct BIN_OP<T, true>
{
	template<class OP>
	T operator()(const T& a, const T& b, OP op) const
	{
		T out;
		char* _a = (char*)&a;
		char* _b = (char*)&b;
		char* _o = (char*)&out;
		for (int i = 0; i < (int)sizeof(T); ++i)
			_o[i] = op(_a[i], _b[i]);
		return out;
	}
};

template<class T>
static void __apply_operator_raw(const T* v1, int s1, const T* v2, int s2, T* out, char op)
{
	if (s1 == 1 && s2 == 1) {
		switch (op) {
			case '+':
				*out = *v1 + *v2;
				break;
			case '-':
				*out = *v1 - *v2;
				break;
			case '*':
				*out = *v1 * *v2;
				break;
			case '/':
				*out = divide(*v1, *v2);
				break;
			case '&':
				*out = BIN_OP<T>()(*v1, *v2, AND());
				break;
			case '|':
				*out = BIN_OP<T>()(*v1, *v2, OR());
				break;
			case '^':
				*out = BIN_OP<T>()(*v1, *v2, XOR());
				break;
		}
	}
	else if (s1 == 1) {
		switch (op) {
			case '+':
				for (int i = 0; i < s2; ++i)
					out[i] = (*v1) + v2[i];
				break;
			case '-':
				for (int i = 0; i < s2; ++i)
					out[i] = (*v1) - v2[i];
				break;
			case '*':
				for (int i = 0; i < s2; ++i)
					out[i] = (*v1) * v2[i];
				break;
			case '/':
				for (int i = 0; i < s2; ++i)
					out[i] = divide((*v1), v2[i]);
				break;
			case '&':
				for (int i = 0; i < s2; ++i)
					out[i] = BIN_OP<T>()((*v1), v2[i], AND());
				break;
			case '|':
				for (int i = 0; i < s2; ++i)
					out[i] = BIN_OP<T>()((*v1), v2[i], OR());
				break;
			case '^':
				for (int i = 0; i < s2; ++i)
					out[i] = BIN_OP<T>()((*v1), v2[i], XOR());
				break;
		}
	}
	else if (s2 == 1) {
		switch (op) {
			case '+':
				for (int i = 0; i < s1; ++i)
					out[i] = v1[i] + (*v2);
				break;
			case '-':
				for (int i = 0; i < s1; ++i)
					out[i] = v1[i] - (*v2);
				break;
			case '*':
				for (int i = 0; i < s1; ++i)
					out[i] = v1[i] * (*v2);
				break;
			case '/':
				for (int i = 0; i < s1; ++i)
					out[i] = divide(v1[i], (*v2));
				break;
			case '&':
				for (int i = 0; i < s1; ++i)
					out[i] = BIN_OP<T>()(v1[i], (*v2), AND());
				break;
			case '|':
				for (int i = 0; i < s1; ++i)
					out[i] = BIN_OP<T>()(v1[i], (*v2), OR());
				break;
			case '^':
				for (int i = 0; i < s1; ++i)
					out[i] = BIN_OP<T>()(v1[i], (*v2), XOR());
				break;
		}
	}
	else {
		switch (op) {
			case '+':
				for (int i = 0; i < s2; ++i)
					out[i] = v1[i] + v2[i];
				break;
			case '-':
				for (int i = 0; i < s2; ++i)
					out[i] = v1[i] - v2[i];
				break;
			case '*':
				for (int i = 0; i < s2; ++i)
					out[i] = v1[i] * v2[i];
				break;
			case '/':
				for (int i = 0; i < s2; ++i)
					out[i] = divide(v1[i], v2[i]);
				break;
			case '&':
				for (int i = 0; i < s2; ++i)
					out[i] = BIN_OP<T>()(v1[i], v2[i], AND());
				break;
			case '|':
				for (int i = 0; i < s2; ++i)
					out[i] = BIN_OP<T>()(v1[i], v2[i], OR());
				break;
			case '^':
				for (int i = 0; i < s2; ++i)
					out[i] = BIN_OP<T>()(v1[i], v2[i], XOR());
				break;
		}
	}
}

template<class T>
static QVariant __apply_operator(const QVariant& v1, const QVariant& v2, char op, VipNDArray& m_buffer)
{
	if (v1.userType() == qMetaTypeId<VipNDArray>() && v2.userType() == qMetaTypeId<VipNDArray>()) {
		// VipNDArray op VipNDArray
		m_buffer.reset(v1.value<VipNDArray>().shape(), qMetaTypeId<T>());
		__apply_operator_raw(
		  (T*)v1.value<VipNDArray>().constData(), v1.value<VipNDArray>().size(), (T*)v2.value<VipNDArray>().constData(), v2.value<VipNDArray>().size(), (T*)m_buffer.data(), op);
		return QVariant::fromValue(m_buffer);
	}
	else if (v1.userType() == qMetaTypeId<VipNDArray>() && v2.userType() == qMetaTypeId<T>()) {
		// VipNDArray op value
		m_buffer.reset(v1.value<VipNDArray>().shape(), qMetaTypeId<T>());
		T val = v2.value<T>();
		__apply_operator_raw((T*)v1.value<VipNDArray>().constData(), v1.value<VipNDArray>().size(), &val, 1, (T*)m_buffer.data(), op);
		return QVariant::fromValue(m_buffer);
	}
	else if (v1.userType() == qMetaTypeId<T>() && v2.userType() == qMetaTypeId<VipNDArray>()) {
		// value op VipNDArray
		m_buffer.reset(v2.value<VipNDArray>().shape(), qMetaTypeId<T>());
		T val = v1.value<T>();
		__apply_operator_raw(&val, 1, (T*)v2.value<VipNDArray>().constData(), v2.value<VipNDArray>().size(), (T*)m_buffer.data(), op);
		return QVariant::fromValue(m_buffer);
	}
	else if (v1.userType() == qMetaTypeId<VipPointVector>() && v2.userType() == qMetaTypeId<VipPointVector>()) {
		// VipPointVector op VipPointVector
		VipPointVector samples = v1.value<VipPointVector>();
		m_buffer.reset(vipVector(v1.value<VipPointVector>().size()), qMetaTypeId<vip_double>());
		__apply_operator_raw((const vip_double*)vipExtractYValues(v1.value<VipPointVector>()).constData(),
				     v1.value<VipPointVector>().size(),
				     (const vip_double*)vipExtractYValues(v2.value<VipPointVector>()).constData(),
				     v2.value<VipPointVector>().size(),
				     (vip_double*)m_buffer.data(),
				     op);
		vipSetYValues(samples, m_buffer);
		return QVariant::fromValue(samples);
	}
	else if (v1.userType() == qMetaTypeId<VipPointVector>() && v2.userType() == qMetaTypeId<T>()) {
		// VipPointVector op value
		VipPointVector samples = v1.value<VipPointVector>();
		m_buffer.reset(vipVector(v1.value<VipPointVector>().size()), qMetaTypeId<vip_double>());
		vip_double val = v2.value<vip_double>();
		__apply_operator_raw((const vip_double*)vipExtractYValues(v1.value<VipPointVector>()).constData(), v1.value<VipPointVector>().size(), &val, 1, (vip_double*)m_buffer.data(), op);
		vipSetYValues(samples, m_buffer);
		return QVariant::fromValue(samples);
	}
	else if (v1.userType() == qMetaTypeId<T>() && v2.userType() == qMetaTypeId<VipPointVector>()) {
		// value op VipPointVector
		VipPointVector samples = v2.value<VipPointVector>();
		m_buffer.reset(vipVector(v2.value<VipPointVector>().size()), qMetaTypeId<vip_double>());
		vip_double val = v1.value<vip_double>();
		__apply_operator_raw(&val, 1, (const vip_double*)vipExtractYValues(v2.value<VipPointVector>()).constData(), v2.value<VipPointVector>().size(), (vip_double*)m_buffer.data(), op);
		vipSetYValues(samples, m_buffer);
		return QVariant::fromValue(samples);
	}
	else if (v1.userType() == qMetaTypeId<VipComplexPointVector>() && v2.userType() == qMetaTypeId<VipComplexPointVector>()) {
		// VipComplexPointVector op VipComplexPointVector
		VipComplexPointVector samples = v1.value<VipComplexPointVector>();
		m_buffer.reset(vipVector(v1.value<VipComplexPointVector>().size()), qMetaTypeId<complex_d>());
		__apply_operator_raw((const complex_d*)vipExtractYValues(v1.value<VipComplexPointVector>()).constData(),
				     v1.value<VipComplexPointVector>().size(),
				     (const complex_d*)vipExtractYValues(v2.value<VipComplexPointVector>()).constData(),
				     v2.value<VipComplexPointVector>().size(),
				     (complex_d*)m_buffer.data(),
				     op);
		vipSetYValues(samples, m_buffer);
		return QVariant::fromValue(samples);
	}
	else if (v1.userType() == qMetaTypeId<VipComplexPointVector>() && v2.userType() == qMetaTypeId<complex_d>()) {
		// VipComplexPointVector op value
		VipComplexPointVector samples = v1.value<VipComplexPointVector>();
		m_buffer.reset(vipVector(v1.value<VipComplexPointVector>().size()), qMetaTypeId<complex_d>());
		complex_d val = v2.value<complex_d>();
		__apply_operator_raw(
		  (const complex_d*)vipExtractYValues(v1.value<VipComplexPointVector>()).constData(), v1.value<VipComplexPointVector>().size(), &val, 1, (complex_d*)m_buffer.data(), op);
		vipSetYValues(samples, m_buffer);
		return QVariant::fromValue(samples);
	}
	else if (v1.userType() == qMetaTypeId<complex_d>() && v2.userType() == qMetaTypeId<VipComplexPointVector>()) {
		// value op VipComplexPointVector
		VipComplexPointVector samples = v2.value<VipComplexPointVector>();
		m_buffer.reset(vipVector(v2.value<VipComplexPointVector>().size()), qMetaTypeId<complex_d>());
		complex_d val = v1.value<complex_d>();
		__apply_operator_raw(
		  &val, 1, (const complex_d*)vipExtractYValues(v2.value<VipComplexPointVector>()).constData(), v2.value<VipComplexPointVector>().size(), (complex_d*)m_buffer.data(), op);
		vipSetYValues(samples, m_buffer);
		return QVariant::fromValue(samples);
	}
	else if (v1.userType() == qMetaTypeId<T>() && v2.userType() == qMetaTypeId<T>()) {
		switch (op) {
			case '+':
				return QVariant::fromValue(v1.value<T>() + v2.value<T>());
			case '-':
				return QVariant::fromValue(v1.value<T>() - v2.value<T>());
			case '*':
				return QVariant::fromValue(v1.value<T>() * v2.value<T>());
			case '/':
				return QVariant::fromValue(v1.value<T>() / v2.value<T>());
			case '&':
				return QVariant::fromValue(BIN_OP<T>()(v1.value<T>(), v2.value<T>(), AND()));
			case '|':
				return QVariant::fromValue(BIN_OP<T>()(v1.value<T>(), v2.value<T>(), OR()));
			case '^':
				return QVariant::fromValue(BIN_OP<T>()(v1.value<T>(), v2.value<T>(), XOR()));
		}
	}
	return QVariant();
}

void VipNumericOperation::mergeData(int, int sub_data_type)
{
	QString _operator = propertyAt(1)->value<QString>();
	// vip_debug("op: '%s'\n", _operator.toLatin1().data());
	if (_operator != "+" && _operator != "-" && _operator != "*" && _operator != "/" && _operator != "&" && _operator != "|" && _operator != "^") {
		setError("wrong operator");
		return;
	}
	char op = _operator.toLatin1().data()[0];

	QVariant res;
	if (sub_data_type == QMetaType::Int) {
		res = __apply_operator<int>(inputs()[0].data(), inputs()[1].data(), op, m_buffer);
	}
	else if (sub_data_type == QMetaType::Double) {
		res = __apply_operator<double>(inputs()[0].data(), inputs()[1].data(), op, m_buffer);
	}
	else if (sub_data_type == qMetaTypeId<complex_d>()) {
		res = __apply_operator<complex_d>(inputs()[0].data(), inputs()[1].data(), op, m_buffer);
	}

	if (res.userType()) {
		VipAnyData any = create(res);
		// QString prefix = startPrefix(inputs());
		QString left = inputs()[0].name();  // left.remove(0,prefix.size());
		QString right = inputs()[1].name(); // right.remove(0,prefix.size());
		any.setName(left + " " + _operator + " " + right);
		outputAt(0)->setData(any);
	}
	else {
		setError("wrong input type");
	}
}

VipAffineTransform::VipAffineTransform(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(1.);
	propertyAt(1)->setData(0.);
	outputAt(0)->setData(VipNDArray());
}

void VipAffineTransform::apply()
{
	VipAnyData in = inputAt(0)->data();
	// VipNDArray ar = in.value<VipNDArray>();

	if (in.isEmpty()) {
		setError("empty input data", VipProcessingObject::WrongInput);
		return;
	}

	double factor = propertyAt(0)->value<double>();
	double offset = propertyAt(1)->value<double>();

	QVariant ar_out = in.data();

	if (ar_out.userType() == qMetaTypeId<VipNDArray>()) {
		const VipNDArray ar = ar_out.value<VipNDArray>();
		if (vipIsImageArray(ar) && ar.shapeCount() == 2 && !ar.isEmpty()) {

			// Work on RGB image
			QImage qimg = vipToImage(ar);
			VipNDArrayTypeView<VipRGB> img = vipQImageView(qimg);
			VipRGB* ptr = img.ptr();
			int size = img.size();
			for (int i = 0; i < size; ++i) {
				auto a = ptr[i].a;
				auto rgb = ptr[i] * factor + offset;
				ptr[i] = rgb.clamp(0, 255);
				ptr[i].a = a;
			}
			ar_out = QVariant::fromValue(vipToArray(qimg));
			VipAnyData out = create(ar_out);
			out.setTime(in.time());
			out.mergeAttributes(in.attributes());
			outputAt(0)->setData(out);
			return;
		}
	}

	if (factor != 1) {
		m_op.inputAt(0)->setData(ar_out);
		m_op.inputAt(1)->setData(factor == (int)factor ? QVariant((int)factor) : QVariant(factor));
		m_op.propertyAt(1)->setData(QString("*"));
		m_op.update();
		ar_out = m_op.outputAt(0)->data().data();
	}
	if (offset != 0) {
		m_op.inputAt(0)->setData(ar_out);
		m_op.inputAt(1)->setData(offset == (int)offset ? QVariant((int)offset) : QVariant(offset));
		m_op.propertyAt(1)->setData(QString("+"));
		m_op.update();
		ar_out = m_op.outputAt(0)->data().data();
	}

	VipAnyData out = create(ar_out);
	out.setTime(in.time());
	out.mergeAttributes(in.attributes());
	outputAt(0)->setData(out);
}

VipAffineTimeTransform::VipAffineTimeTransform(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(1.);
	propertyAt(1)->setData(0.);
	outputAt(0)->setData(VipNDArray());
}

void VipAffineTimeTransform::apply()
{
	VipAnyData in = inputAt(0)->data();
	// VipNDArray ar = in.value<VipNDArray>();

	if (in.isEmpty()) {
		setError("empty input data", VipProcessingObject::WrongInput);
		return;
	}
	if (in.data().userType() != qMetaTypeId<VipPointVector>() && in.data().userType() != qMetaTypeId<VipComplexPointVector>()) {
		setError("wrong input data", VipProcessingObject::WrongInput);
		return;
	}

	double factor = propertyAt(0)->value<double>();
	double offset = propertyAt(1)->value<double>();
	QVariant ar_out;

	if (in.data().userType() == qMetaTypeId<VipPointVector>()) {
		VipPointVector v = in.value<VipPointVector>();
		for (int i = 0; i < v.size(); ++i)
			v[i].setX(v[i].x() * factor + offset);
		ar_out = QVariant::fromValue(v);
	}
	else {
		VipComplexPointVector v = in.value<VipComplexPointVector>();
		for (int i = 0; i < v.size(); ++i)
			v[i].setX(v[i].x() * factor + offset);
		ar_out = QVariant::fromValue(v);
	}

	VipAnyData out = create(ar_out);
	out.setTime(in.time());
	out.mergeAttributes(in.attributes());
	outputAt(0)->setData(out);
}

VipSubtractBackground::VipSubtractBackground(QObject* parent)
  : VipProcessingObject(parent)
{
	outputAt(0)->setData(QVariant::fromValue(VipNDArray()));
	m_op.propertyName("Operator")->setData(QString("-"));
}

void VipSubtractBackground::resetProcessing()
{
	propertyAt(0)->setData(inputAt(0)->probe());
}

void VipSubtractBackground::apply()
{
	VipAnyData in = inputAt(0)->data();

	if (in.isEmpty()) {
		setError("empty input data", VipProcessingObject::WrongInput);
		return;
	}

	QVariant background = propertyAt(0)->data().data();
	if (background.userType() == 0) {
		background = in.data();
		propertyAt(0)->setData(background);
	}

	m_op.inputAt(0)->setData(in);
	m_op.inputAt(1)->setData(background);
	m_op.update();

	QVariant ar_out = m_op.outputAt(0)->data().data();
	VipAnyData out = create(ar_out);
	out.setTime(in.time());
	out.mergeAttributes(in.attributes());
	outputAt(0)->setData(out);
}

QList<VipProcessingObject*> VipOperationBetweenPlayers::directSources() const
{
	VipOtherPlayerData data = propertyAt(1)->value<VipOtherPlayerData>();
	QList<VipProcessingObject*> res = VipProcessingObject::directSources();
	if (data.isDynamic() && data.processing() && res.indexOf(data.processing()) < 0)
		res.append(data.processing());
	return res;
}

void VipOperationBetweenPlayers::apply()
{
	VipAnyData in = inputAt(0)->data();
	if (in.isEmpty()) {
		setError("empty input data", VipProcessingObject::WrongInput);
		return;
	}

	VipAnyData input2 = propertyAt(1)->value<VipOtherPlayerData>().data();

	QVariant out_data;
	if (!input2.isEmpty()) {
		// if we have 2 VipPointVector, resample them
		if (in.data().userType() == qMetaTypeId<VipPointVector>() && input2.data().userType() == qMetaTypeId<VipPointVector>()) {
			QList<VipPointVector> vectors;
			vectors.append(in.data().value<VipPointVector>());
			vectors.append(input2.data().value<VipPointVector>());
			vipResampleVectors(vectors);
			if (vectors.size() == 2) {
				in.setData(QVariant::fromValue(vectors[0]));
				input2.setData(QVariant::fromValue(vectors[1]));
			}
		}

		m_op.propertyName("Operator")->setData(propertyName("Operator")->data());
		m_op.inputAt(0)->setData(in);
		m_op.inputAt(1)->setData(input2);
		m_op.update();
		out_data = m_op.outputAt(0)->data().data();
	}
	else
		out_data = in.data();

	VipAnyData out = create(out_data);
	out.setTime(in.time());
	outputAt(0)->setData(out);
}

void VipOperationBetweenPlayers::resetProcessing()
{
	VipOtherPlayerData other = propertyAt(1)->value<VipOtherPlayerData>();
	if (!other.isDynamic()) {
		// reset the VipOtherPlayerData to compute the new static data
		VipOtherPlayerData new_data(other.isDynamic(), other.processing(), other.parentProcessingObject(), other.outputIndex(), other.otherPlayerId(), other.otherDisplayIndex());
		propertyAt(1)->setData(new_data);
	}
}

VipTimeDifference::VipTimeDifference(QObject* parent)
  : VipProcessingObject(parent)
{
	propertyAt(0)->setData(QString("ns"));
}

template<class Container>
VipPointVector TimeDifference(const Container& v, double factor)
{
	VipPointVector res(v.size());
	for (int i = 1; i < v.size(); ++i)
		res[i] = VipPoint(v[i].x(), (v[i].x() - v[i - 1].x()) * factor);
	if (v.size() > 1)
		res[0] = VipPoint(v[0].x(), res[1].y());
	else if (v.size() == 1)
		res[0] = VipPoint(v[0].x(), 0);
	return res;
}

void VipTimeDifference::apply()
{
	VipAnyData any = inputAt(0)->data();
	QString unit = propertyAt(0)->value<QString>();
	double factor = 1;
	if (unit == "us")
		factor = 1.0 / 1000.0;
	else if (unit == "ms")
		factor = 1.0 / 1000000.0;
	else if (unit == "s")
		factor = 1.0 / 1000000000.0;

	VipPointVector res;
	if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
		res = TimeDifference(any.value<VipPointVector>(), factor);
	}
	else if (any.data().userType() == qMetaTypeId<VipComplexPointVector>()) {
		res = TimeDifference(any.value<VipComplexPointVector>(), factor);
	}
	else {
		setError("Wrong input type");
		return;
	}

	VipAnyData out = create(QVariant::fromValue(res));
	out.mergeAttributes(any.attributes());
	out.setTime(any.time());
	out.setYUnit(unit);
	outputAt(0)->setData(out);
}

VipSignalDerivative::VipSignalDerivative(QObject* parent)
  : VipProcessingObject(parent)
{
}

template<class Vector, class Sample>
Vector SignalDerivative(const Vector& v)
{
	Vector res(v.size() - 1);
	// if (v.hasNanosecondPrecision())
	//  {
	//  for (int i = 1; i < v.size(); ++i) {
	//  const T v1 = v[i - 1];
	//  const T v2 = v[i];
	//  const qint64 t1 = v.time(i - 1);
	//  const qint64 t2 = v.time(i);
	//  res.setValues(i - 1, (t1 + t2) / 2, (v2.y() - v1.y()) / double(t2 - t1));
	//  }
	//  }
	//  else
	{
		for (int i = 1; i < v.size(); ++i) {
			const Sample v1 = v[i - 1];
			const Sample v2 = v[i];
			res[i - 1] = Sample((v1.x() + v2.x()) / 2, (v2.y() - v1.y()) / double(v2.x() - v1.x()));
		}
	}
	return res;
}

void VipSignalDerivative::apply()
{
	VipAnyData any = inputAt(0)->data();
	VipAnyData out;

	if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
		const VipPointVector v = any.value<VipPointVector>();
		if (v.size() < 2) {
			setError("Signal too small");
			return;
		}
		VipPointVector res = SignalDerivative<VipPointVector, VipPoint>(v);
		out = create(QVariant::fromValue(res));
	}
	else if (any.data().userType() == qMetaTypeId<VipComplexPointVector>()) {
		const VipComplexPointVector v = any.value<VipComplexPointVector>();
		if (v.size() < 2) {
			setError("Signal too small");
			return;
		}
		VipComplexPointVector res = SignalDerivative<VipComplexPointVector, VipComplexPoint>(v);
		out = create(QVariant::fromValue(res));
	}
	else {
		setError("Wrong input type");
		return;
	}

	QString x = any.xUnit();
	QString y = any.yUnit();
	QString unit;

	QStringList lst = y.split(".");
	if (lst.size() > 1 && lst.last() == x) {
		lst.removeLast();
		unit = lst.join('.');
	}
	else
		unit = y + "/" + x;

	out.mergeAttributes(any.attributes());
	out.setTime(any.time());
	out.setYUnit(unit);
	outputAt(0)->setData(out);
}

// VipSignalIntegral::VipSignalIntegral(QObject * parent)
//  :VipProcessingObject(parent)
//  {}
//
//  void VipSignalIntegral::apply()
//  {
//  VipAnyData any = inputAt(0)->data();
//  VipAnyData out;
//
//  if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
//  const VipPointVector v = any.value<VipPointVector>();
//  if (v.size() < 2) {
//	setError("Signal too small");
//	return;
//  }
//  VipPointVector res(v.size() - 1);
//  for (int i = 1; i < v.size(); ++i) {
//	QPointF v1 = v[i - 1];
//	QPointF v2 = v[i];
//	res[i - 1] = QPointF((v2.x() + v1.x()) / 2, ((v2.y() + v1.y()) /2 )*  (v2.x() - v1.x()));
//  }
//  out = create(QVariant::fromValue(res));
//  }
//  else if (any.data().userType() == qMetaTypeId<VipComplexPointVector>()) {
//  const VipComplexPointVector v = any.value<VipComplexPointVector>();
//  if (v.size() < 2) {
//	setError("Signal too small");
//	return;
//  }
//  VipComplexPointVector res(v.size() - 1);
//  for (int i = 1; i < v.size(); ++i) {
//	VipComplexPoint v1 = v[i - 1];
//	VipComplexPoint v2 = v[i];
//	res[i - 1] = VipComplexPoint((v2.first + v1.first) / 2, ((v2.second + v1.second) / 2.)*  (v2.first - v1.first) );
//  }
//  out = create(QVariant::fromValue(res));
//  }
//  else {
//  setError("Wrong input type");
//  return;
//  }
//
//  QString x = any.xUnit();
//  QString y = any.yUnit();
//  QString unit;
//
//  QStringList lst = y.split("/");
//  if (lst.size() > 1 && lst.last() == x) {
//  lst.removeLast();
//  unit = lst.join('/');
//  }
//  else
//  unit = y + "." + x;
//
//  out.mergeAttributes(any.attributes());
//  out.setTime(any.time());
//  out.setYUnit(unit);
//  outputAt(0)->setData(out);
//  }
