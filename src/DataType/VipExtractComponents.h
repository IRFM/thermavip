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

#ifndef VIP_EXTRACT_COMPONENTS_H
#define VIP_EXTRACT_COMPONENTS_H

#include "VipNDArray.h"
#include <QObject>
#include <QStringList>

/// \addtogroup DataType
/// @{

/// Helper function.
/// Inplace clamp \a ar based on \a min and \a max.
/// Clamp to \a min if \a min is not nullptr.
/// Clamp to \a max if \a max is not nullptr.
VIP_DATA_TYPE_EXPORT bool vipClamp(VipNDArray& ar, const double* min, const double* max);

/// VipExtractComponents manages how to  extract components from an array of a specific pixel type,
/// and how to build an array of this pixel type from separate components.
class VIP_DATA_TYPE_EXPORT VipExtractComponents : public QObject
{
	Q_OBJECT

	QList<VipNDArray> m_components;
	QMap<int, double*> m_clampMin;
	QMap<int, double*> m_clampMax;

protected:
	bool HasComponentsSameShapes() const;

public:
	enum Type
	{
		None,
		Color,
		Complex,
		UserType = 100
	};

	VipExtractComponents() {}
	virtual ~VipExtractComponents();

	virtual Type GetType() const = 0;
	virtual QString GetMethod() const = 0;

	/// Extract from 'array' the component of name 'component'.
	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const = 0;

	/// Separate 'array' in its components, store them internally using VipExtractComponents::SetComponents().
	/// Default implementation launch VipExtractComponents::ExtractOneComponent() on all component found  with VipExtractComponents::PixelComponentNames().
	/// All component extraction are executed concurrently using QtConcurrent.
	virtual void SeparateComponents(const VipNDArray& array);

	/// Merge the components stored internally into a new array.
	virtual VipNDArray MergeComponents() const = 0;

	/// Returns the component of name 'component'. Default implementation uses VipExtractComponents::PixelComponentNames() to find the component index.
	virtual VipNDArray GetComponent(const QString& component) const;

	/// Affects to the given component the array 'array'
	virtual bool SetComponent(const QString& component, const VipNDArray& array);

	/// Returns the input data types that class can handle (for instance, a class extracting ARGB components should return a list containing 'QImage').
	/// An empty list means that all types are supported.
	virtual QList<QByteArray> InputDataTypes() const = 0;

	/// Returns a string representation of all components this class can extract (for instance, a class extracting ARGB components should return
	/// a list containing 'Alpha', 'Red', 'Green', 'Blue').
	virtual QStringList PixelComponentNames() const = 0;

	/// Returns the type of each component.
	virtual QList<QByteArray> PixelComponentTypes() const = 0;

	/// Return true if the original image can be build from the extracted components.
	virtual bool CanBuildFromComponents() const { return true; }

	/// Returns the list of extracted components.
	QList<VipNDArray> GetComponents() const { return m_components; }

	/// Set the list of components
	virtual void SetComponents(const QList<VipNDArray>& components) { m_components = components; }

	/// Returns true if the given data_type is supported, false otherwise
	bool IsSupported(const QByteArray& type_name) { return this->InputDataTypes().size() == 0 || this->InputDataTypes().indexOf(type_name) >= 0; }

	bool HasComponent(const QString& component) { return PixelComponentNames().indexOf(component) >= 0; }

	void SetClampMin(double min, int component);
	void SetClampMax(double min, int component);
	bool HasClampMin(int component) const;
	bool HasClampMax(int component) const;
	double ClampMin(int component) const;
	double ClamMax(int component) const;
	const double* ClampMinPtr(int component) const;
	const double* ClamMaxPtr(int component) const;
};

// Invariant component extraction. Basically returns the inpute image.
class VIP_DATA_TYPE_EXPORT VipExtractInvariant : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractInvariant() {}

	virtual Type GetType() const { return None; }
	virtual QString GetMethod() const { return "Invariant"; }
	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString&) const { return array; }

	virtual void SeparateComponents(const VipNDArray& array) { SetComponents(QList<VipNDArray>() << array); }

	virtual bool SetComponent(const QString&, const VipNDArray& array)
	{
		SetComponents(QList<VipNDArray>() << array);
		return true;
	}

	virtual VipNDArray GetComponent(const QString&) const { return MergeComponents(); }

	virtual VipNDArray MergeComponents() const
	{
		if (GetComponents().size() == 0)
			return VipNDArray();
		else
			return GetComponents()[0];
	}

	virtual QList<QByteArray> InputDataTypes() const { return QList<QByteArray>(); }
	virtual QList<QByteArray> PixelComponentTypes() const { return QList<QByteArray>() << QByteArray(); }
	virtual QStringList PixelComponentNames() const { return QStringList() << "Invariant"; }
};

// Extract the component 'Alpha', 'Red', 'Green' and 'Blue' of a color image.
// The input VipNDArray object must have been constructed from a QImage.
class VIP_DATA_TYPE_EXPORT VipExtractARGBComponents : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractARGBComponents() {}

	virtual Type GetType() const { return Color; }
	virtual QString GetMethod() const { return "Color ARGB"; }

	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual QList<QByteArray> InputDataTypes() const { return QList<QByteArray>() << "QImage"; }
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<quint8>()).name();
		return QList<QByteArray>() << name << name << name << name;
	}
	virtual QStringList PixelComponentNames() const
	{
		return QStringList() << "Red"
				     << "Green"
				     << "Blue"
				     << "Alpha";
	}

	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i)
			tmp.append(components[i].toUInt8());
		VipExtractComponents::SetComponents(tmp);
	}
};

// Extract the component 'Hsl Hue', 'Hsl Saturation', 'Hsl Lightness' and 'Hsl Alpha' of a color image.
// The input VipNDArray object must have been constructed from a QImage.
class VIP_DATA_TYPE_EXPORT VipExtractHSLComponents : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractHSLComponents() {}

	virtual Type GetType() const { return Color; }
	virtual QString GetMethod() const { return "Color AHSL"; }
	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual QList<QByteArray> InputDataTypes() const { return QList<QByteArray>() << "QImage"; }
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<int>()).name();
		QByteArray name_uchar = QMetaType(qMetaTypeId<quint8>()).name();
		return QList<QByteArray>() << name << name << name << name_uchar;
	}
	virtual QStringList PixelComponentNames() const
	{
		return QStringList() << "Hsl Hue"
				     << "Hsl Saturation"
				     << "Hsl Lightness"
				     << "Hsl Alpha";
	}

	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i) {
			if (i == 0)
				tmp.append(components[i].toInt32());
			else
				tmp.append(components[i].toUInt8());
		}
		VipExtractComponents::SetComponents(tmp);
	}
};

// Extract the component 'Hsv Hue', 'Hsv Saturation', 'Hsv Value' and 'Hsv Alpha' of a color image.
// The input VipNDArray object must have been constructed from a QImage.
class VIP_DATA_TYPE_EXPORT VipExtractHSVComponents : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractHSVComponents() {}

	virtual Type GetType() const { return Color; }
	virtual QString GetMethod() const { return "Color AHSV"; }
	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual QList<QByteArray> InputDataTypes() const { return QList<QByteArray>() << "QImage"; }
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<int>()).name();
		QByteArray name_uchar = QMetaType(qMetaTypeId<quint8>()).name();
		return QList<QByteArray>() << name << name << name << name_uchar;
	}
	virtual QStringList PixelComponentNames() const
	{
		return QStringList() << "Hsv Hue"
				     << "Hsv Saturation"
				     << "Hsv Value"
				     << "Hsv Alpha";
	}
	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i) {
			if (i == 0)
				tmp.append(components[i].toInt32());
			else
				tmp.append(components[i].toUInt8());
		}
		VipExtractComponents::SetComponents(tmp);
	}
};

// Extract the component 'CMYK Cyan', 'CMYK Magenta', 'CMYK Yellow', 'CMYK Black' and 'CMYK Alpha' of a color image.
// The input VipNDArray object must have been constructed from a QImage.
class VIP_DATA_TYPE_EXPORT VipExtractCMYKComponents : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractCMYKComponents() {}

	virtual Type GetType() const { return Color; }
	virtual QString GetMethod() const { return "Color ACMYK"; }
	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual QList<QByteArray> InputDataTypes() const { return QList<QByteArray>() << "QImage"; }
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<quint8>()).name();
		return QList<QByteArray>() << name << name << name << name << name;
	}
	QStringList PixelComponentNames() const
	{
		return QStringList() << "CMYK Cyan"
				     << "CMYK Magenta"
				     << "CMYK Yellow"
				     << "CMYK Black"
				     << "CMYK Alpha";
	}
	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i) {
			tmp.append(components[i].toUInt8());
		}
		VipExtractComponents::SetComponents(tmp);
	}
};

// Convert a color image to a grayscale one.
// The input VipNDArray object must have been constructed from a QImage.
class VIP_DATA_TYPE_EXPORT VipExtractGrayScale : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractGrayScale() {}

	virtual Type GetType() const { return Color; }
	virtual QString GetMethod() const { return "Greyscale"; }
	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual bool CanBuildFromComponents() const { return false; }

	QList<QByteArray> InputDataTypes() const { return QList<QByteArray>() << "QImage"; }
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<quint8>()).name();
		return QList<QByteArray>() << name;
	}
	QStringList PixelComponentNames() const { return QStringList() << "Grayscale"; }
	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i) {
			tmp.append(components[i].toUInt8());
		}
		VipExtractComponents::SetComponents(tmp);
	}
};

// Extract the real and imaginary parts of a complex image.
class VIP_DATA_TYPE_EXPORT VipExtractComplexRealImag : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractComplexRealImag() {}

	virtual Type GetType() const { return Complex; }
	virtual QString GetMethod() const { return "Complex Real/Imag"; }

	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual QList<QByteArray> InputDataTypes() const
	{
		return QList<QByteArray>() << "complex_f"
					   << "complex_d";
	}
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<double>()).name();
		return QList<QByteArray>() << name << name;
	}
	virtual QStringList PixelComponentNames() const
	{
		return QStringList() << "Real"
				     << "Imag";
	}
	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i) {
			tmp.append(components[i].toDouble());
		}
		VipExtractComponents::SetComponents(tmp);
	}
};

// Extract the amplitude and the argument of a complex image.
class VIP_DATA_TYPE_EXPORT VipExtractComplexAmplitudeArgument : public VipExtractComponents
{
	Q_OBJECT
public:
	VipExtractComplexAmplitudeArgument() {}

	virtual Type GetType() const { return Complex; }
	virtual QString GetMethod() const { return "Complex Amplitude/Argument"; }

	virtual VipNDArray ExtractOneComponent(const VipNDArray& array, const QString& component) const;
	virtual VipNDArray MergeComponents() const;

	virtual QList<QByteArray> InputDataTypes() const
	{
		return QList<QByteArray>() << "complex_f"
					   << "complex_d";
	}
	virtual QList<QByteArray> PixelComponentTypes() const
	{
		QByteArray name = QMetaType(qMetaTypeId<double>()).name();
		return QList<QByteArray>() << name << name;
	}
	virtual QStringList PixelComponentNames() const
	{
		return QStringList() << "Amplitude"
				     << "Argument";
	}
	virtual void SetComponents(const QList<VipNDArray>& components)
	{
		QList<VipNDArray> tmp;
		for (qsizetype i = 0; i < components.size(); ++i) {
			tmp.append(components[i].toDouble());
		}
		VipExtractComponents::SetComponents(tmp);
	}
};

class VIP_DATA_TYPE_EXPORT VipGenericExtractComponent
{
public:
	QStringList SupportedComponents(const VipNDArray& ar) const;
	QStringList StandardComponents(const VipNDArray& ar) const;
	void SetComponent(const QString& component);
	VipNDArray Extract(const VipNDArray& ar);

	bool isInvariant() const;

	static bool HasComponents(const VipNDArray& ar);

private:
	QString m_component;
};

/// \enum ComplexComponent
/// \brief Components which might be extracted from  a complex image
enum ComplexComponent
{
	Real,
	Imag,
	Amplitude,
	Argument
};

/// Returns the real part of a complex image, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToReal(const VipNDArray& dat);

/// Returns the imaginary part of a complex image, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToImag(const VipNDArray& dat);

/// Returns the amplitude of a complex image, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToAmplitude(const VipNDArray& dat);

/// Returns the argument of a complex image, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToArgument(const VipNDArray& dat);

/// Returns the names of the components that might be extracted from a complex image.
/// Basically returns QStringList()<<"Real"<<"Imag"<<"Amplitude"<<"Argument";
VIP_DATA_TYPE_EXPORT QStringList ComplexComponents();

/// Returns the component \a component from the complex image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToComplexComponent(const VipNDArray& dat, ComplexComponent component);
/// Returns the component \a component from the complex image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToComplexComponent(const VipNDArray& dat, const QString& component);

/// \enum ColorComponent
/// \brief Components which might be extracted from  a color image
enum ColorComponent
{
	GrayScale,
	Red,
	Green,
	Blue,
	Alpha,
	HslHue,
	HslSaturation,
	HslLightness,
	HsvHue,
	HsvSaturation,
	HsvValue,
	CMYKCyan,
	CMYKMagenta,
	CMYKYellow,
	CMYKBlack
};

/// Convert input color image to a grayscale image.
VIP_DATA_TYPE_EXPORT VipNDArray ToGrayScale(const VipNDArray& dat);

/// Returns the red component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToRed(const VipNDArray& dat);

/// Returns the green component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToGreen(const VipNDArray& dat);

/// Returns the blue component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToBlue(const VipNDArray& dat);

/// Returns the alpha component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToAlpha(const VipNDArray& dat);

/// Returns the HSL hue component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToHslHue(const VipNDArray& dat);

/// Returns the HSL saturation component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToHslSaturation(const VipNDArray& dat);

/// Returns the HSL lightness component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToHslLightness(const VipNDArray& dat);

/// Returns the HSV hue component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToHsvHue(const VipNDArray& dat);

/// Returns the HSV saturation component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToHsvSaturation(const VipNDArray& dat);

/// Returns the HSV value component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToHsvValue(const VipNDArray& dat);

/// Returns the CMYK cyan component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToCMYKCyan(const VipNDArray& dat);

/// Returns the CMYK Magenta component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToCMYKMagenta(const VipNDArray& dat);

/// Returns the CMYK Yellow component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToCMYKYellow(const VipNDArray& dat);

/// Returns the CMYK Black component of color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToCMYKBlack(const VipNDArray& dat);

/// Returns the names of the components that might be extracted from a color image.
VIP_DATA_TYPE_EXPORT QStringList ColorComponents();

/// Returns the component \a component from the color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToColorComponent(const VipNDArray& dat, ColorComponent component);

/// Returns the component \a component from the color image \a dat, or a null VipNDArray on error.
VIP_DATA_TYPE_EXPORT VipNDArray ToColorComponent(const VipNDArray& dat, const QString& component);

// helper functions

inline QStringList vipPossibleComponents(const VipNDArray& ar)
{
	if (vipIsNullArray(ar))
		return QStringList();
	if (strcmp(ar.dataName(), "QImage") == 0) {
		return QStringList() << "Color ARGB"
				     << "Color AHSL"
				     << "Color AHSV"
				     << "Color ACMYK";
	}
	else if (strcmp(ar.dataName(), "complex_f") == 0 || strcmp(ar.dataName(), "complex_d") == 0) {
		return QStringList() << "Complex Real/Imag"
				     << "Complex Amplitude/Argument";
	}
	else
		return QStringList();
}

inline QString vipMethodType(const QString& method)
{
	if (method == "Complex Real/Imag")
		return "Complex";
	else if (method == "Complex Amplitude/Argument")
		return "Complex";
	else if (method == "Color ARGB")
		return "Color";
	else if (method == "Color AHSL")
		return "Color";
	else if (method == "Color AHSV")
		return "Color";
	else if (method == "Color ACMYK")
		return "Color";
	else
		return QString();
}

inline QString vipMethodDescription(const QString& method)
{
	if (method == "Complex Real/Imag")
		return "Complex real and imaginary components";
	else if (method == "Complex Amplitude/Argument")
		return "Complex amplitude and argument components";
	else if (method == "Color ARGB")
		return "Color ARGB: red, green, blue and alpha components";
	else if (method == "Color AHSL")
		return "Color AHSL: hue, saturation, luminance and alpha components";
	else if (method == "Color AHSV")
		return "Color AHSV: hue, saturation, value and alpha components";
	else if (method == "Color ACMYK")
		return "Color ACMYK: cyan, magenta, yellow and black components";
	else
		return QString();
}

inline qsizetype vipComponentsCount(const QString& method)
{
	if (method == "Complex Real/Imag")
		return 2;
	else if (method == "Complex Amplitude/Argument")
		return 2;
	else if (method == "Color ARGB")
		return 4;
	else if (method == "Color AHSL")
		return 4;
	else if (method == "Color AHSV")
		return 4;
	else if (method == "Color ACMYK")
		return 5;
	else
		return 0;
}

inline VipExtractComponents* vipCreateExtractComponents(const QString& method)
{
	if (method == "Complex Real/Imag")
		return new VipExtractComplexRealImag();
	else if (method == "Complex Amplitude/Argument")
		return new VipExtractComplexAmplitudeArgument();
	else if (method == "Color ARGB")
		return new VipExtractARGBComponents();
	else if (method == "Color AHSL")
		return new VipExtractHSVComponents();
	else if (method == "Color AHSV")
		return new VipExtractHSVComponents();
	else if (method == "Color ACMYK")
		return new VipExtractCMYKComponents();
	else
		return nullptr;
}

/// @}
// end DataType

#endif
