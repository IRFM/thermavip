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

#include <QColor>
#include <QVector>

#include "VipExtractComponents.h"
#include "VipMultiNDArray.h"



VipExtractComponents::~VipExtractComponents()
{
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
		int type = vipIdFromName(pixel_type.data());

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
		if (HasClampMax(index) && HasClampMin(index)) {
			double minval = HasClampMin(index) ? ClampMin(index) : -std::numeric_limits<double>::infinity();
			double maxval = HasClampMax(index) ? ClampMax(index) : std::numeric_limits<double>::infinity();
			m_components[index] = vipClamp(m_components[index], minval, maxval);
		}
		
		if (m_components[index].isNull())
			return false;

		return true;
	}
	VIP_UNREACHABLE();
	// return false;
}
void VipExtractComponents::SetClampMin(double min, int component)
{
	m_clampMin[component] = (min);
}
void VipExtractComponents::SetClampMax(double min, int component)
{
	m_clampMax[component] = (min);
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
	auto found = m_clampMin.find(component);
	return found == m_clampMin.end() ? 0. : found.value();
}
double VipExtractComponents::ClampMax(int component) const
{
	auto found = m_clampMax.find(component);
	return found == m_clampMax.end() ? 0. : found.value();
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
	return components[0];
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

	VipNDArray res(vipIdFromName("complex_d"), shape);
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
	if (component == "Norm")
		return ToNorm(array);
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

	VipNDArray res(vipIdFromName("complex_d"), shape);
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
	else if (ar.isComplex())
		return (QStringList() << "Invariant") + ComplexComponents();
	else if (ar.isRGB())
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
	if (ar.isComplex())
		return true;
	if (ar.isRGB())
		return true;
	if (vipIsMultiNDArray(ar))
		return true;
	return false;
}

QStringList VipGenericExtractComponent::StandardComponents(const VipNDArray& ar) const
{
	if (ar.isComplex())
		return (QStringList() << "Real"
				      << "Imag");
	else if (ar.isRGB())
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
	else if (ar.isComplex())
		return ToComplexComponent(ar, m_component);
	else if (ar.isRGB())
		return ToColorComponent(ar, m_component);
	else if (vipIsMultiNDArray(ar))
		return VipMultiNDArray(ar).array(m_component);
	else
		return ar;
}

VipNDArray ToReal(const VipNDArray& dat)
{
	return vipReal(dat);
}

VipNDArray ToImag(const VipNDArray& dat)
{
	return vipImag(dat);
}

VipNDArray ToNorm(const VipNDArray& dat)
{
	return vipNorm(dat);
}

VipNDArray ToArgument(const VipNDArray& dat)
{
	return vipArg(dat);
}

QStringList ComplexComponents()
{
	return QStringList() << "Real"
			     << "Imag"
			     << "Norm"
			     << "Argument";
}

VipNDArray ToComplexComponent(const VipNDArray& dat, Vip::ComplexComponent component)
{
	if (component == Vip::Real)
		return ToReal(dat);
	else if (component == Vip::Imag)
		return ToImag(dat);
	else if (component == Vip::Norm)
		return ToNorm(dat);
	else if (component == Vip::Argument)
		return ToArgument(dat);
	else
		return VipNDArray();
}

VipNDArray ToComplexComponent(const VipNDArray& dat, const QString& component)
{
	QStringList lst = ComplexComponents();
	int index = lst.indexOf(component);
	if (index >= 0)
		return ToComplexComponent(dat, Vip::ComplexComponent(index));
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
	return vipGrayscale(dat);
}

VipNDArray ToRed(const VipNDArray& dat)
{
	return vipRed(dat);
}

VipNDArray ToGreen(const VipNDArray& dat)
{
	return vipGreen(dat);
}

VipNDArray ToBlue(const VipNDArray& dat)
{
	return vipBlue(dat);
}

VipNDArray ToAlpha(const VipNDArray& dat)
{
	return vipAlpha(dat);
}

VipNDArray ToHslHue(const VipNDArray& dat)
{
	return vipHslHue(dat);
}

VipNDArray ToHslSaturation(const VipNDArray& dat)
{
	return vipHslSaturation(dat);
}

VipNDArray ToHslLightness(const VipNDArray& dat)
{
	return vipValue(dat);
}

VipNDArray ToHsvHue(const VipNDArray& dat)
{
	return vipHsvHue(dat);
}

VipNDArray ToHsvSaturation(const VipNDArray& dat)
{
	return vipHsvSaturation(dat);
}

VipNDArray ToHsvValue(const VipNDArray& dat)
{
	return vipValue(dat);
}

VipNDArray ToCMYKCyan(const VipNDArray& dat)
{
	return vipCyan(dat);
}

VipNDArray ToCMYKMagenta(const VipNDArray& dat)
{
	return vipMagenta(dat);
}

VipNDArray ToCMYKYellow(const VipNDArray& dat)
{
	return vipYellow(dat);
}

VipNDArray ToCMYKBlack(const VipNDArray& dat)
{
	return vipBlack(dat);
}

VipNDArray ToColorComponent(const VipNDArray& dat, Vip::ColorComponent component)
{
	if (component == Vip::GrayScale)
		return ToGrayScale(dat);
	else if (component == Vip::Red)
		return ToRed(dat);
	else if (component == Vip::Green)
		return ToGreen(dat);
	else if (component == Vip::Blue)
		return ToBlue(dat);
	else if (component == Vip::Alpha)
		return ToAlpha(dat);
	else if (component == Vip::HslHue)
		return ToHslHue(dat);
	else if (component == Vip::HslSaturation)
		return ToHslSaturation(dat);
	else if (component == Vip::HslLightness)
		return ToHslLightness(dat);
	else if (component == Vip::HsvHue)
		return ToHsvHue(dat);
	else if (component == Vip::HsvSaturation)
		return ToHsvSaturation(dat);
	else if (component == Vip::HsvValue)
		return ToHsvValue(dat);
	else if (component == Vip::CMYKCyan)
		return ToCMYKCyan(dat);
	else if (component == Vip::CMYKMagenta)
		return ToCMYKMagenta(dat);
	else if (component == Vip::CMYKYellow)
		return ToCMYKYellow(dat);
	else if (component == Vip::CMYKBlack)
		return ToCMYKBlack(dat);
	else
		return VipNDArray();
}

VipNDArray ToColorComponent(const VipNDArray& dat, const QString& component)
{
	QStringList lst = ColorComponents();
	int index = lst.indexOf(component);
	if (index >= 0)
		return ToColorComponent(dat, Vip::ColorComponent(index));
	else
		return VipNDArray();
}
