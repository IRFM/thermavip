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

#include <QColor>
#include <QVector>

#include "VipExtractComponents.h"
#include "VipMultiNDArray.h"
#include "VipNDArrayImage.h"
#include "VipNDArrayVariant.h"

struct Clamp
{
	const double* min;
	const double* max;
	template<class T>
	T operator()(const T& val)
	{
		return min ? (val < *min ? *min : (max ? (val > *max ? *max : val) : val)) : (max ? (val > *max ? *max : val) : val);
	}
};
struct ClampArray
{
	Clamp clamp;
	template<class T>
	void operator()(VipNDArrayTypeView<T> val)
	{
		vipArrayTransform(val.ptr(), val.shape(), val.strides(), val.ptr(), val.shape(), val.strides(), clamp);
	}
};

bool vipClamp(VipNDArray& ar, const double* min, const double* max)
{
	if (!min && !max)
		return true;

	ar.detach();
	VipNDNumericArray tmp;
	if (tmp.isValidType(ar.dataType())) {
		tmp = ar;
		ClampArray c;
		c.clamp.min = min;
		c.clamp.max = max;
		tmp.apply<void, ClampArray>(c);
		return true;
	}
	return false;
}

VipExtractComponents::~VipExtractComponents()
{
	for (QMap<int, double*>::iterator it = m_clampMax.begin(); it != m_clampMax.end(); ++it)
		delete it.value();
	for (QMap<int, double*>::iterator it = m_clampMin.begin(); it != m_clampMin.end(); ++it)
		delete it.value();
}

bool VipExtractComponents::HasComponentsSameShapes() const
{
	QList<VipNDArray> components = GetComponents();
	if (components.size() == 0)
		return false;

	VipNDArrayShape sh = components[0].shape();
	for (qsizetype i = 1; i < components.size(); ++i) {
		VipNDArrayShape temp = components[i].shape();
		if (temp.size() != sh.size())
			return false;
		if (!std::equal(sh.begin(), sh.end(), temp.begin()))
			return false;
	}

	return true;
}

void VipExtractComponents::SeparateComponents(const VipNDArray& array)
{
	QStringList components = this->PixelComponentNames();
	QList<VipNDArray> temp;
	for (qsizetype i = 0; i < components.size(); ++i) {
		temp << ExtractOneComponent(array, components[i]);
	}

	this->SetComponents(temp);
}

VipNDArray VipExtractComponents::GetComponent(const QString& component) const
{
	qsizetype index = this->PixelComponentNames().indexOf(component);
	if (index >= 0)
		return GetComponents()[index];
	else
		return VipNDArray();
}

bool VipExtractComponents::SetComponent(const QString& component, const VipNDArray& array)
{
	if (m_components.size() != this->PixelComponentNames().size())
		return false;

	int index = this->PixelComponentNames().indexOf(component);
	if (index < 0)
		return false;
	else {
		VipNDArray temp = array;
		QByteArray pixel_type = this->PixelComponentTypes()[index];
		int type = vipFromName(pixel_type.data()).id();

		if (pixel_type.isEmpty())
			m_components[index] = array;
		else {
			if (m_components[index].dataType() != type || m_components[index].isNull())
				m_components[index] = VipNDArray(type, array.shape());

			if (array.dataType() != type)
				array.convert(m_components[index]);
			else
				m_components[index] = array;
		}

		// clamp
		vipClamp(m_components[index], ClampMinPtr(index), ClamMaxPtr(index));

		if (m_components[index].isNull())
			return false;

		return true;
	}
	VIP_UNREACHABLE();
	// return false;
}
void VipExtractComponents::SetClampMin(double min, int component)
{
	QMap<int, double*>::iterator found = m_clampMin.find(component);
	if (found == m_clampMin.end())
		m_clampMin[component] = new double(min);
	else
		*found.value() = min;
}
void VipExtractComponents::SetClampMax(double min, int component)
{
	QMap<int, double*>::iterator found = m_clampMax.find(component);
	if (found == m_clampMax.end())
		m_clampMax[component] = new double(min);
	else
		*found.value() = min;
}
bool VipExtractComponents::HasClampMin(int component) const
{
	return m_clampMin.find(component) != m_clampMin.end();
}
bool VipExtractComponents::HasClampMax(int component) const
{
	return m_clampMax.find(component) != m_clampMin.end();
}
double VipExtractComponents::ClampMin(int component) const
{
	QMap<int, double*>::const_iterator found = m_clampMin.find(component);
	return found == m_clampMin.end() ? 0. : *found.value();
}
double VipExtractComponents::ClamMax(int component) const
{
	QMap<int, double*>::const_iterator found = m_clampMax.find(component);
	return found == m_clampMax.end() ? 0. : *found.value();
}

const double* VipExtractComponents::ClampMinPtr(int component) const
{
	QMap<int, double*>::const_iterator found = m_clampMin.find(component);
	return found == m_clampMin.end() ? nullptr : found.value();
}
const double* VipExtractComponents::ClamMaxPtr(int component) const
{
	QMap<int, double*>::const_iterator found = m_clampMax.find(component);
	return found == m_clampMax.end() ? nullptr : found.value();
}

VipNDArray VipExtractARGBComponents::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "Red")
		return ToRed(array);
	else if (component == "Green")
		return ToGreen(array);
	else if (component == "Blue")
		return ToBlue(array);
	else if (component == "Alpha")
		return ToAlpha(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractARGBComponents::MergeComponents() const
{
	QList<VipNDArray> components = GetComponents();
	if (components.size() != 4)
		return VipNDArray();

	if (!HasComponentsSameShapes()) {
		qWarning("Unable to vipMerge components: components have different sizes");
		return VipNDArray();
	}

	VipNDArrayShape shape = components[0].shape();
	qsizetype size = components[0].size();

	const quint8* r = (const quint8*)components[0].constData();
	const quint8* g = (const quint8*)components[1].constData();
	const quint8* b = (const quint8*)components[2].constData();
	const quint8* a = (const quint8*)components[3].constData();

	QImage res(shape[1], shape[0], QImage::Format_ARGB32);
	uint* data = (uint*)res.bits();

#pragma omp parallel for
	for (qsizetype i = 0; i < size; ++i)
		data[i] = qRgba(r[i], g[i], b[i], a[i]);

	return vipToArray(res);
}

VipNDArray VipExtractHSLComponents::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "Hsl Hue")
		return ToHslHue(array);
	else if (component == "Hsl Saturation")
		return ToHslSaturation(array);
	else if (component == "Hsl Lightness")
		return ToHslLightness(array);
	else if (component == "Hsl Alpha")
		return ToAlpha(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractHSLComponents::MergeComponents() const
{
	const QList<VipNDArray> components = GetComponents();
	if (components.size() != 4)
		return VipNDArray();

	if (!HasComponentsSameShapes()) {
		qWarning("Unable to vipMerge components: components have different sizes");
		return VipNDArray();
	}

	VipNDArrayShape shape = components[0].shape();
	qsizetype size = components[0].size();

	qsizetype* h = (qsizetype*)components[0].data();
	qsizetype* s = (qsizetype*)components[1].data();
	qsizetype* l = (qsizetype*)components[2].data();
	quint8* a = (quint8*)components[3].data();

	QImage res(shape[1], shape[0], QImage::Format_ARGB32);
	uint* data = (uint*)res.bits();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		data[i] = QColor::fromHsl(h[i], s[i], l[i], a[i]).rgba();

	return vipToArray(res);
}

VipNDArray VipExtractHSVComponents::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "Hsv Hue")
		return ToHsvHue(array);
	else if (component == "Hsv Saturation")
		return ToHsvSaturation(array);
	else if (component == "Hsv Value")
		return ToHsvValue(array);
	else if (component == "Hsv Alpha")
		return ToAlpha(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractHSVComponents::MergeComponents() const
{
	const QList<VipNDArray> components = GetComponents();
	if (components.size() != 4)
		return VipNDArray();

	if (!HasComponentsSameShapes()) {
		qWarning("Unable to vipMerge components: components have different sizes");
		return VipNDArray();
	}

	VipNDArrayShape shape = components[0].shape();
	qsizetype size = components[0].size();

	const VipNDArray a0 = components[0];
	const VipNDArray a1 = components[1];
	const VipNDArray a2 = components[2];
	const VipNDArray a3 = components[3];

	const qsizetype* h = (const qsizetype*)components[0].data();
	const qsizetype* s = (const qsizetype*)components[1].data();
	const qsizetype* v = (const qsizetype*)components[2].data();
	const quint8* a = (const quint8*)components[3].data();

	QImage res(shape[1], shape[0], QImage::Format_ARGB32);
	uint* data = (uint*)res.bits();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i) {
		data[i] = QColor::fromHsv(h[i], s[i], v[i], a[i]).rgba();
	}

	return vipToArray(res);
}

VipNDArray VipExtractCMYKComponents::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "CMYK Cyan")
		return ToCMYKCyan(array);
	else if (component == "CMYK Magenta")
		return ToCMYKMagenta(array);
	else if (component == "CMYK Yellow")
		return ToCMYKYellow(array);
	else if (component == "CMYK Black")
		return ToCMYKBlack(array);
	else if (component == "CMYK Alpha")
		return ToAlpha(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractCMYKComponents::MergeComponents() const
{
	const QList<VipNDArray> components = GetComponents();
	if (components.size() != 5)
		return VipNDArray();

	if (!HasComponentsSameShapes()) {
		qWarning("Unable to vipMerge components: components have different sizes");
		return VipNDArray();
	}

	VipNDArrayShape shape = components[0].shape();
	qsizetype size = components[0].size();

	const int* c = (const int*)components[0].data();
	const int* y = (const int*)components[1].data();
	const int* m = (const int*)components[2].data();
	const int* k = (const int*)components[3].data();
	const quint8* a = (const quint8*)components[4].data();

	QImage res(shape[1], shape[0], QImage::Format_ARGB32);
	uint* data = (uint*)res.bits();

#pragma omp parallel for
	for (int i = 0; i < size; ++i) {
		data[i] = QColor::fromCmyk(c[i], y[i], m[i], k[i], a[i]).rgba();
	}

	return vipToArray((res));
}

VipNDArray VipExtractGrayScale::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "Grayscale")
		return ToGrayScale(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractGrayScale::MergeComponents() const
{
	const QList<VipNDArray> components = GetComponents();
	if (components.size() != 1)
		return VipNDArray();

	QImage res = vipToImage(components[0]).convertToFormat(QImage::Format_ARGB32);
	return vipToArray((res));
}

VipNDArray VipExtractComplexRealImag::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "Real")
		return ToReal(array);
	else if (component == "Imag")
		return ToImag(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractComplexRealImag::MergeComponents() const
{
	const QList<VipNDArray> components = GetComponents();
	if (components.size() != 2)
		return VipNDArray();

	if (!HasComponentsSameShapes()) {
		qWarning("Unable to vipMerge components: components have different sizes");
		return VipNDArray();
	}

	VipNDArrayShape shape = components[0].shape();
	qsizetype size = components[0].size();

	VipNDArray res(vipFromName("complex_d").id(), shape);
	std::complex<double>* data = (std::complex<double>*)res.data();

	double* r = (double*)components[0].data();
	double* i = (double*)components[1].data();

#pragma omp parallel for
	for (int p = 0; p < (int)size; ++p)
		data[p] = std::complex<double>(r[p], i[p]);

	return res;
}

VipNDArray VipExtractComplexAmplitudeArgument::ExtractOneComponent(const VipNDArray& array, const QString& component) const
{
	if (component == "Amplitude")
		return ToAmplitude(array);
	else if (component == "Argument")
		return ToArgument(array);
	else
		return VipNDArray();
}

VipNDArray VipExtractComplexAmplitudeArgument::MergeComponents() const
{
	const QList<VipNDArray> components = GetComponents();
	if (components.size() != 2)
		return VipNDArray();

	if (!HasComponentsSameShapes()) {
		qWarning("Unable to vipMerge components: components have different sizes");
		return VipNDArray();
	}

	VipNDArrayShape shape = components[0].shape();
	qsizetype size = components[0].size();

	VipNDArray res(vipFromName("complex_d").id(), shape);
	std::complex<double>* data = (std::complex<double>*)res.data();

	double* am = (double*)components[0].data();
	double* ar = (double*)components[1].data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		data[i] = std::polar<double>(am[i], ar[i]);

	return res;
}

QStringList VipGenericExtractComponent::SupportedComponents(const VipNDArray& ar) const
{
	// TEST: remove Invariant if possible
	if (ar.isNull())
		return QStringList();
	else if (strcmp(ar.dataName(), "complex_f") == 0 || strcmp(ar.dataName(), "complex_d") == 0)
		return (QStringList() << "Invariant") + ComplexComponents();
	else if (strcmp(ar.dataName(), "QImage") == 0)
		return (QStringList() << "Invariant") + ColorComponents();
	else if (vipIsMultiNDArray(ar))
		return				    //(QStringList()<<"Invariant")+
		  VipMultiNDArray(ar).arrayNames(); // invariant not supported for VipMultiNDArray
	else
		return QStringList();
}

bool VipGenericExtractComponent::HasComponents(const VipNDArray& ar)
{
	if (ar.isEmpty())
		return false;
	if (strcmp(ar.dataName(), "complex_f") == 0 || strcmp(ar.dataName(), "complex_d") == 0)
		return true;
	else if (strcmp(ar.dataName(), "QImage") == 0)
		return true;
	else if (vipIsMultiNDArray(ar))
		return true;
	else
		return false;
}

QStringList VipGenericExtractComponent::StandardComponents(const VipNDArray& ar) const
{
	if (strcmp(ar.dataName(), "complex_f") == 0 || strcmp(ar.dataName(), "complex_d") == 0)
		return (QStringList() << "Real"
				      << "Imag");
	else if (strcmp(ar.dataName(), "QImage") == 0)
		return (QStringList() << "Alpha"
				      << "Red"
				      << "Green"
				      << "Blue");
	else if (vipIsMultiNDArray(ar))
		return VipMultiNDArray(ar).arrayNames();
	else
		return QStringList();
}

void VipGenericExtractComponent::SetComponent(const QString& component)
{
	m_component = component;
}

bool VipGenericExtractComponent::isInvariant() const
{
	return (m_component == "Invariant" || m_component.isEmpty());
}

VipNDArray VipGenericExtractComponent::Extract(const VipNDArray& ar)
{
	if (isInvariant())
		return ar;
	else if (strcmp(ar.dataName(), "complex_f") == 0 || strcmp(ar.dataName(), "complex_d") == 0)
		return ToComplexComponent(ar, m_component);
	else if (strcmp(ar.dataName(), "QImage") == 0)
		return ToColorComponent(ar, m_component);
	else if (vipIsMultiNDArray(ar))
		return VipMultiNDArray(ar).array(m_component);
	else
		return ar;
}

VipNDArray ToReal(const VipNDArray& dat)
{
	if (dat.isNull())
		return dat;

	if (dat.dataName() != QByteArray("complex_d") && dat.dataName() != QByteArray("complex_f"))
		return VipNDArray();

	VipNDArrayShape shape = dat.shape();
	qsizetype size = dat.size();
	VipNDArray res(qMetaTypeId<double>(), shape);

	double* data = (double*)res.data();

	if (dat.dataName() == QByteArray("complex_f")) {
		std::complex<float>* c = (std::complex<float>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = c[i].real();
	}
	else {
		std::complex<double>* c = (std::complex<double>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = c[i].real();
	}

	return res;
}

VipNDArray ToImag(const VipNDArray& dat)
{
	if (dat.isNull())
		return dat;

	if (dat.dataName() != QByteArray("complex_d") && dat.dataName() != QByteArray("complex_f"))
		return VipNDArray();

	VipNDArrayShape shape = dat.shape();
	qsizetype size = dat.size();
	VipNDArray res(qMetaTypeId<double>(), shape);

	double* data = (double*)res.data();

	if (dat.dataName() == QByteArray("complex_f")) {
		std::complex<float>* c = (std::complex<float>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = c[i].imag();
	}
	else {
		std::complex<double>* c = (std::complex<double>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = c[i].imag();
	}

	return res;
}

VipNDArray ToAmplitude(const VipNDArray& dat)
{
	if (dat.isNull())
		return dat;

	if (dat.dataName() != QByteArray("complex_d") && dat.dataName() != QByteArray("complex_f"))
		return VipNDArray();

	VipNDArrayShape shape = dat.shape();
	qsizetype size = dat.size();
	VipNDArray res(qMetaTypeId<double>(), shape);

	double* data = (double*)res.data();

	if (dat.dataName() == QByteArray("complex_f")) {
		std::complex<float>* c = (std::complex<float>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = std::abs(c[i]);
	}
	else {
		std::complex<double>* c = (std::complex<double>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = std::abs(c[i]);
	}

	return res;
}

VipNDArray ToArgument(const VipNDArray& dat)
{
	if (dat.isNull())
		return dat;

	if (dat.dataName() != QByteArray("complex_d") && dat.dataName() != QByteArray("complex_f"))
		return VipNDArray();

	VipNDArrayShape shape = dat.shape();
	qsizetype size = dat.size();
	VipNDArray res(qMetaTypeId<double>(), shape);

	double* data = (double*)res.data();

	if (dat.dataName() == QByteArray("complex_f")) {
		std::complex<float>* c = (std::complex<float>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = std::arg(c[i]);
	}
	else {
		std::complex<double>* c = (std::complex<double>*)dat.data();
#pragma omp parallel for
		for (int i = 0; i < (int)size; ++i)
			data[i] = std::arg(c[i]);
	}

	return res;
}

QStringList ComplexComponents()
{
	return QStringList() << "Real"
			     << "Imag"
			     << "Amplitude"
			     << "Argument";
}

VipNDArray ToComplexComponent(const VipNDArray& dat, ComplexComponent component)
{
	if (component == Real)
		return ToReal(dat);
	else if (component == Imag)
		return ToImag(dat);
	else if (component == Amplitude)
		return ToAmplitude(dat);
	else if (component == Argument)
		return ToArgument(dat);
	else
		return VipNDArray();
}

VipNDArray ToComplexComponent(const VipNDArray& dat, const QString& component)
{
	QStringList lst = ComplexComponents();
	int index = lst.indexOf(component);
	if (index >= 0)
		return ToComplexComponent(dat, ComplexComponent(index));
	else
		return VipNDArray();
}

QStringList ColorComponents()
{
	return QStringList() << "Grayscale"
			     << "Red"
			     << "Green"
			     << "Blue"
			     << "Alpha"
			     << "Hsl Hue"
			     << "Hsl Saturation"
			     << "Hsl Lightness"
			     << "Hsv Hue"
			     << "Hsv Saturation"
			     << "Hsv Value"
			     << "CMYK Cyan"
			     << "CMYK Magenta"
			     << "CMYK Yellow"
			     << "CMYK Black";
}

VipNDArray ToGrayScale(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	qsizetype size = im.width() * im.height();
	VipNDArray ar(qMetaTypeId<quint8>(), vipVector(im.height(), im.width()));

	quint8* res = (quint8*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = qRound(0.299 * qRed(pix[i]) + 0.587 * qGreen(pix[i]) + 0.114 * qBlue(pix[i]));

	return (ar);
}

VipNDArray ToRed(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<quint8>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	quint8* res = (quint8*)ar.data();

	//#pragma omp parallel for
	for (qsizetype i = 0; i < size; ++i)
		res[i] = qRed(pix[i]);

	return VipNDArray(ar);
}

VipNDArray ToGreen(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<quint8>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	quint8* res = (quint8*)ar.data();

	//#pragma omp parallel for
	for (qsizetype i = 0; i < size; ++i)
		res[i] = qGreen(pix[i]);

	return VipNDArray(ar);
}

VipNDArray ToBlue(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<quint8>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	quint8* res = (quint8*)ar.data();

	//#pragma omp parallel for
	for (qsizetype i = 0; i < size; ++i)
		res[i] = qBlue(pix[i]);

	return VipNDArray(ar);
}

VipNDArray ToAlpha(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<quint8>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	quint8* res = (quint8*)ar.data();

	//#pragma omp parallel for
	for (qsizetype i = 0; i < size; ++i)
		res[i] = qAlpha(pix[i]);

	return VipNDArray(ar);
}

VipNDArray ToHslHue(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (qsizetype i = 0; i < size; ++i)
		res[i] = QColor(pix[i]).hslHue();

	return VipNDArray(ar);
}

VipNDArray ToHslSaturation(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).saturation();

	return VipNDArray(ar);
}

VipNDArray ToHslLightness(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).lightness();

	return VipNDArray(ar);
}

VipNDArray ToHsvHue(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).hsvHue();

	return VipNDArray(ar);
}

VipNDArray ToHsvSaturation(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).saturation();

	return VipNDArray(ar);
}

VipNDArray ToHsvValue(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i <(int) size; ++i)
		res[i] = QColor(pix[i]).value();

	return VipNDArray(ar);
}

VipNDArray ToCMYKCyan(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).cyan();

	return VipNDArray(ar);
}

VipNDArray ToCMYKMagenta(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).magenta();

	return VipNDArray(ar);
}

VipNDArray ToCMYKYellow(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).yellow();

	return VipNDArray(ar);
}

VipNDArray ToCMYKBlack(const VipNDArray& dat)
{
	const QImage im = vipToImage(dat).convertToFormat(QImage::Format_ARGB32);
	if (im.isNull())
		return VipNDArray();

	const uint* pix = (uint*)im.bits();
	VipNDArray ar(qMetaTypeId<int>(), vipVector(im.height(), im.width()));
	qsizetype size = ar.size();
	int* res = (int*)ar.data();

#pragma omp parallel for
	for (int i = 0; i < (int)size; ++i)
		res[i] = QColor(pix[i]).black();

	return VipNDArray(ar);
}

VipNDArray ToColorComponent(const VipNDArray& dat, ColorComponent component)
{
	if (component == GrayScale)
		return ToGrayScale(dat);
	else if (component == Red)
		return ToRed(dat);
	else if (component == Green)
		return ToGreen(dat);
	else if (component == Blue)
		return ToBlue(dat);
	else if (component == Alpha)
		return ToAlpha(dat);
	else if (component == HslHue)
		return ToHslHue(dat);
	else if (component == HslSaturation)
		return ToHslSaturation(dat);
	else if (component == HslLightness)
		return ToHslLightness(dat);
	else if (component == HsvHue)
		return ToHsvHue(dat);
	else if (component == HsvSaturation)
		return ToHsvSaturation(dat);
	else if (component == HsvValue)
		return ToHsvValue(dat);
	else if (component == CMYKCyan)
		return ToCMYKCyan(dat);
	else if (component == CMYKMagenta)
		return ToCMYKMagenta(dat);
	else if (component == CMYKYellow)
		return ToCMYKYellow(dat);
	else if (component == CMYKBlack)
		return ToCMYKBlack(dat);
	else
		return VipNDArray();
}

VipNDArray ToColorComponent(const VipNDArray& dat, const QString& component)
{
	QStringList lst = ColorComponents();
	int index = lst.indexOf(component);
	if (index >= 0)
		return ToColorComponent(dat, ColorComponent(index));
	else
		return VipNDArray();
}
