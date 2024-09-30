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

#include "VipImageProcessing.h"
#include "VipIODevice.h"
#include "VipPolygon.h"
#include "VipTransform.h"

#include <qdatastream.h>

template<class IN_, class OUT_>
void rotate90Right(const IN_& in, OUT_ out)
{
	for (int y = 0; y < in.shape(0); ++y)
		for (int x = 0; x < in.shape(1); ++x) {
			//_out[x + out.shape(1) * (out.shape(1) - y - 1)] = _in[x + y * in.shape(1)];
			out(x, out.shape(1) - y - 1) = in(y, x);
		}
}

template<class IN_, class OUT_>
void rotate90Left(const IN_& in, OUT_ out)
{
	for (int y = 0; y < in.shape(0); ++y)
		for (int x = 0; x < in.shape(1); ++x) {
			out(out.shape(0) - x - 1, y) = in(y, x);
		}
}

template<class IN_, class OUT_>
void rotate180(const IN_& in, OUT_ out)
{
	for (int y = 0; y < in.shape(0); ++y)
		for (int x = 0; x < in.shape(1); ++x) {
			out(out.shape(0) - y - 1, out.shape(1) - x - 1) = in(y, x);
		}
}

template<class IN_, class OUT_>
void mirrorH(const IN_& in, OUT_ out)
{
	for (int y = 0; y < in.shape(0); ++y)
		for (int x = 0; x < in.shape(1); ++x) {
			out(y, out.shape(1) - x - 1) = in(y, x);
		}
}

template<class IN_, class OUT_>
void mirrorV(const IN_& in, OUT_ out)
{
	for (int y = 0; y < in.shape(0); ++y)
		for (int x = 0; x < in.shape(1); ++x) {
			out(out.shape(0) - y - 1, x) = in(y, x);
		}
}

template<class IN_, class OUT_>
void threshold(const VipNDArrayTypeView<IN_>& in, VipNDArrayTypeView<OUT_> out, IN_ value)
{
	const IN_* inp = in.ptr();
	OUT_* outp = out.ptr();
	for (int i = 0; i < in.size(); ++i)
		outp[i] = inp[i] >= value ? 1 : 0;
}

VipNDArray VipStdImageAndPlotProcessing::extractY(const VipPointVector& v)
{
	VipNDArrayType<double> res(vipVector(v.size()));
	double* ptr = res.ptr();
	for (int i = 0; i < v.size(); ++i)
		ptr[i] = v[i].y();
	return res;
}

VipPointVector VipStdImageAndPlotProcessing::resetY(const VipNDArray& y_values, const VipPointVector& initial_vector)
{
	if (y_values.size() != initial_vector.size())
		return VipPointVector();

	VipNDArrayType<double> tmp = y_values.toDouble();
	if (tmp.size() != initial_vector.size())
		return VipPointVector();

	double* ptr = tmp.ptr();
	VipPointVector res = initial_vector;
	for (int i = 0; i < res.size(); ++i)
		res[i].setY(ptr[i]);

	return res;
}

VipNDArray VipRotate90Right::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("empty image", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	VipNDArray out;

	if (ar.isComplex()) {
		out = VipNDArray(qMetaTypeId<complex_d>(), vipVector(ar.shape(1), ar.shape(0)));
		VipNDArray in = ar.toComplexDouble();
		rotate90Right(VipNDArrayTypeView<complex_d>(in), VipNDArrayTypeView<complex_d>(out));
	}
	else if (vipIsImageArray(ar)) {
		QImage imout = vipToImage(ar).transformed(QTransform().rotate(90));
		out = vipToArray(imout);
	}
	else {
		QTransform tr;
		tr.rotate(90);
		out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, 0, QPointF(0, -1));
	}

	return out;
}

VipNDArray VipRotate90Left::applyProcessing(const VipNDArray& ar)
{

	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("empty image", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	VipNDArray out;

	if (ar.isComplex()) {
		out = VipNDArray(qMetaTypeId<complex_d>(), vipVector(ar.shape(1), ar.shape(0)));
		VipNDArray in = ar.toComplexDouble();
		rotate90Left(VipNDArrayTypeView<complex_d>(in), VipNDArrayTypeView<complex_d>(out));
	}
	else if (vipIsImageArray(ar)) {
		QImage imout = vipToImage(ar).transformed(QTransform().rotate(-90));
		out = vipToArray(imout);
	}
	else {
		QTransform tr;
		tr.rotate(-90);
		out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, 0, QPointF(-1, 0));
	}

	return out;
}

VipNDArray VipRotate180::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("empty image", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	VipNDArray out;

	if (ar.isComplex()) {
		out = VipNDArray(qMetaTypeId<complex_d>(), ar.shape());
		VipNDArray in = ar.toComplexDouble();
		rotate180(VipNDArrayTypeView<complex_d>(in), VipNDArrayTypeView<complex_d>(out));
	}
	else if (vipIsImageArray(ar)) {
		QImage imout = vipToImage(ar).transformed(QTransform().rotate(180));
		out = vipToArray(imout);
	}
	else {
		QTransform tr;
		tr.rotate(180);
		out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, 0, QPointF(-1, -1));
	}

	return out;
}

VipNDArray VipMirrorH::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("empty image", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	VipNDArray out;

	if (ar.isComplex()) {
		out = VipNDArray(qMetaTypeId<complex_d>(), ar.shape());
		VipNDArray in = ar.toComplexDouble();
		mirrorH(VipNDArrayTypeView<complex_d>(in), VipNDArrayTypeView<complex_d>(out));
	}
	else if (vipIsImageArray(ar)) {
		QImage imout = vipToImage(ar).mirrored(true, false);
		out = vipToArray(imout);
	}
	else {
		QTransform tr;
		tr.scale(-1, 1);
		out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, 0, QPointF(-1, 0));
	}

	return out;
}

VipNDArray VipMirrorV::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("empty image", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	VipNDArray out;

	if (ar.isComplex()) {
		out = VipNDArray(qMetaTypeId<complex_d>(), ar.shape());
		VipNDArray in = ar.toComplexDouble();
		mirrorV(VipNDArrayTypeView<complex_d>(in), VipNDArrayTypeView<complex_d>(out));
	}
	else if (vipIsImageArray(ar)) {
		QImage imout = vipToImage(ar).mirrored(false, true);
		out = vipToArray(imout);
	}
	else {
		QTransform tr;
		tr.scale(1, -1);
		out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, 0, QPointF(0, -1));
	}

	return out;
}

VipNDArray VipThresholding::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty()) {
		setError("empty array", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	VipNDArray out(QMetaType::Int, ar.shape());
	double value = propertyAt(0)->data().value<double>();

	if (ar.canConvert<double>()) {
		VipNDArray in = ar.toDouble();
		threshold(VipNDArrayTypeView<double>(in), VipNDArrayTypeView<int>(out), value);
	}
	else {
		setError("wrong input array type (" + QString(ar.dataName()) + ")", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	return out;
}

void VipImageCrop::setStartPosition(const VipNDArrayShape& sh)
{
	QStringList lst;
	for (int i = 0; i < sh.size(); ++i)
		lst.append(QString::number(sh[i]));
	propertyName("Top_left")->setData(lst.join(","));
}

void VipImageCrop::setEndPosition(const VipNDArrayShape& sh)
{
	QStringList lst;
	for (int i = 0; i < sh.size(); ++i)
		lst.append(QString::number(sh[i]));
	propertyName("Bottom_right")->setData(lst.join(","));
}

#include <qtextstream.h>

QTransform VipImageCrop::imageTransform(bool* from_center) const
{
	*from_center = false;

	VipNDArrayShape start, end;
	const VipNDArray ar = inputAt(0)->probe().value<VipNDArray>();
	cropping(ar, start, end);

	QTransform tr;
	if (start.size() == 2) {
		tr.translate(-start[1], -start[0]);
	}
	return tr;
}

void VipImageCrop::cropping(const VipNDArray& src, VipNDArrayShape& start, VipNDArrayShape& end) const
{
	QString top_left = propertyName("Top_left")->value<QString>();
	QString bottom_right = propertyName("Bottom_right")->value<QString>();

	start.clear();
	end.clear();

	// use the top left and bottom right
	top_left.replace(",", " ");
	bottom_right.replace(",", " ");

	QTextStream str_tl(&top_left);
	int value = 0;
	while ((str_tl >> value).status() == QTextStream::Ok)
		start.push_back(value);

	QTextStream str_br(&bottom_right);
	while ((str_br >> value).status() == QTextStream::Ok)
		end.push_back(value);

	if (!src.isNull()) {
		while (start.size() < src.shapeCount())
			start.push_back(0);

		while (end.size() < src.shapeCount())
			end.push_back(src.shape(end.size()));
	}

	// invert coordinates if required and make sure we stay in image boundaries
	for (int i = 0; i < start.size(); ++i) {
		if (end[i] < start[i]) {
			int tmp = start[i];
			start[i] = end[i] + 1;
			end[i] = tmp + 1;
		}

		if (start[i] < 0)
			start[i] = 0;
		if (src.shapeCount() > i)
			if (end[i] >= src.shape(i))
				end[i] = src.shape(i);
	}
}
void VipImageCrop::apply()
{
	VipAnyData any = inputAt(0)->data();
	VipNDArray ar = any.value<VipNDArray>();
	if (ar.isEmpty()) {
		setError("empty input array", VipProcessingObject::WrongInput);
		return;
	}

	VipNDArrayShape start, end;
	cropping(ar, start, end);

	VipNDArray out;
	if (vipIsImageArray(ar)) {
		out = vipToArray(vipToImage(ar).copy(QRect(QPoint(start[1], start[0]), QPoint(end[1] - 1, end[0] - 1))));
	}
	else {
		VipNDArrayShape shape;
		for (int i = 0; i < start.size(); ++i)
			shape.push_back(end[i] - start[i]);
		out = ar.mid(start, shape).convert(ar.dataType());
	}

	VipAnyData anyout = create(QVariant::fromValue(out));
	anyout.setTime(any.time());
	anyout.mergeAttributes(any.attributes());
	outputAt(0)->setData(anyout);
}

VipNDArray VipResize::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty()) {
		setError("empty input array", VipProcessingObject::WrongInput);
		return VipNDArray();
	}

	QString s_shape = propertyAt(0)->value<QString>();

	int interpol = propertyAt(1)->value<int>();
	if (interpol < 0)
		interpol = 0;
	if (interpol > 2)
		interpol = 2;

	VipNDArrayShape shape;

	// use the string shape
	s_shape.replace(",", " ");
	QTextStream str_tl(&s_shape);
	int value = 0;
	while ((str_tl >> value).status() == QTextStream::Ok)
		shape.push_back(value);

	while (shape.size() < ar.shapeCount())
		shape.push_back(ar.shape(shape.size()));

	Vip::InterpolationType inter = Vip::NoInterpolation;
	if (interpol == 1)
		inter = Vip::LinearInterpolation;
	else if (interpol == 2)
		inter = Vip::CubicInterpolation;

	VipNDArray out = ar.resize(shape, inter);

	return out;
}

static QDataStream& operator<<(QDataStream& str, const Transform& tr)
{
	return str << (int)tr.type << tr.x << tr.y;
}
static QDataStream& operator>>(QDataStream& str, Transform& tr)
{
	return str >> (int&)tr.type >> tr.x >> tr.y;
}
static QDataStream& operator<<(QDataStream& str, const TransformList& trs)
{
	str << trs.size();
	for (int i = 0; i < trs.size(); ++i)
		str << trs[i];
	return str;
}
static QDataStream& operator>>(QDataStream& str, TransformList& trs)
{
	int size;
	str >> size;
	for (int i = 0; i < size; ++i) {
		Transform t;
		str >> t;
		trs.push_back(t);
	}
	return str;
}

static QTransform toQTransform(const TransformList& trs)
{
	QTransform res;
	for (int i = 0; i < trs.size(); ++i) {
		if (trs[i].type == Transform::Translate)
			res.translate(trs[i].x, trs[i].y);
		else if (trs[i].type == Transform::Rotate)
			res.rotate(trs[i].x);
		else if (trs[i].type == Transform::Scale)
			res.scale(trs[i].x, trs[i].y);
		else if (trs[i].type == Transform::Shear)
			res.shear(trs[i].x, trs[i].y);
	}
	return res;
}

static int registerTypes()
{
	qRegisterMetaType<Transform>();
	qRegisterMetaType<TransformList>();
	qRegisterMetaType<QTransform>();

	qRegisterMetaTypeStreamOperators<Transform>();
	qRegisterMetaTypeStreamOperators<TransformList>();

	QMetaType::registerConverter<TransformList, QTransform>(toQTransform);
	return 0;
}
static int _registerTypes = registerTypes();


VipNDArray VipGenericImageTransform::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("invalid image", VipProcessingObject::WrongInput);
		return ar;
	}

	QTransform tr = propertyAt(0)->value<QTransform>();
	int interp = propertyAt(1)->value<int>();
	int size = propertyAt(2)->value<int>();

	VipNDArray out;

	if (vipCanConvert(ar.dataType(), QMetaType::Double)) {
		if (size == Vip::TransformBoundingRect) {
			if (interp == Vip::NoInterpolation)
				out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, propertyAt(3)->value<double>());
			else
				out = vipTransform<Vip::TransformBoundingRect, Vip::LinearInterpolation>(ar, tr, propertyAt(3)->value<double>());
		}
		else {
			if (interp == Vip::NoInterpolation)
				out = vipTransform<Vip::SrcSize, Vip::NoInterpolation>(ar, tr, propertyAt(3)->value<double>());
			else
				out = vipTransform<Vip::SrcSize, Vip::LinearInterpolation>(ar, tr, propertyAt(3)->value<double>());
		}
	}
	else if (vipCanConvert(ar.dataType(), qMetaTypeId<complex_d>())) {
		if (size == Vip::TransformBoundingRect) {
			if (interp == Vip::NoInterpolation)
				out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, propertyAt(3)->value<complex_d>());
			else
				out = vipTransform<Vip::TransformBoundingRect, Vip::LinearInterpolation>(ar, tr, propertyAt(3)->value<complex_d>());
		}
		else {
			if (interp == Vip::NoInterpolation)
				out = vipTransform<Vip::SrcSize, Vip::NoInterpolation>(ar, tr, propertyAt(3)->value<complex_d>());
			else
				out = vipTransform<Vip::SrcSize, Vip::LinearInterpolation>(ar, tr, propertyAt(3)->value<complex_d>());
		}
	}
	else if (vipIsImageArray(ar)) {
		if (size == Vip::TransformBoundingRect) {
			if (interp == Vip::NoInterpolation)
				out = vipTransform<Vip::TransformBoundingRect, Vip::NoInterpolation>(ar, tr, propertyAt(3)->value<VipRGB>());
			else
				out = vipTransform<Vip::TransformBoundingRect, Vip::LinearInterpolation>(ar, tr, propertyAt(3)->value<VipRGB>());
		}
		else {
			if (interp == Vip::NoInterpolation)
				out = vipTransform<Vip::SrcSize, Vip::NoInterpolation>(ar, tr, propertyAt(3)->value<VipRGB>());
			else
				out = vipTransform<Vip::SrcSize, Vip::LinearInterpolation>(ar, tr, propertyAt(3)->value<VipRGB>());
		}
	}

	if (out.isEmpty()) {
		setError("Unable to apply transform");
		return ar;
	}
	return out;
}

VipNDArray VipComponentLabelling::applyProcessing(const VipNDArray& ar)
{
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("invalid image", VipProcessingObject::WrongInput);
		return ar;
	}

	bool m_connectivity_8 = propertyAt(0)->value<bool>();

	VipNDArrayType<int> out(ar.shape());
	if (m_buffer.size() != out.size())
		m_buffer.resize(out.size());

	if (ar.canConvert<double>()) {
		VipNDArrayType<double> in = ar.toDouble();
		vipLabelImage(in, out, 0., m_connectivity_8, m_buffer.data());
	}
	else if (ar.canConvert<complex_d>()) {
		VipNDArrayType<complex_d> in = ar.toComplexDouble();
		vipLabelImage(in, out, complex_d(0., 0.), m_connectivity_8, m_buffer.data());
	}
	else {
		setError("invalid image type (" + QString(ar.dataName()) + ")");
	}
	return out;
}

void VipComponentLabelling::setConnectivity8(bool enable)
{
	propertyAt(0)->setData(enable);
}

bool VipComponentLabelling::connectivity8() const
{
	return propertyAt(0)->value<bool>();
}
