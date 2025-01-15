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

#ifndef VIP_COLOR_MAP_H
#define VIP_COLOR_MAP_H

#include "VipAdaptativeGradient.h"
#include "VipArchive.h"
#include "VipGlobals.h"
#include "VipInterval.h"
#include <QColor>
#include <QGradient>
#include <QObject>
#include <QVector>

/// \addtogroup Plotting
/// @{

class QPainter;
class VipScaleMap;
class VipNDArray;

/// Number of threads used by default to render an object based on a color map
#ifndef VIP_COLOR_MAP_THREADS
#define VIP_COLOR_MAP_THREADS 1
#endif

/// \brief VipColorMap is used to map values into colors.
///
/// For displaying 3D data on a 2D plane the 3rd dimension is often
/// displayed using colors, like f.e in a spectrogram.
///
/// Each color map is optimized to return colors for only one of the
/// following image formats:
///
/// - QImage::Format_Indexed8\n
/// - QImage::Format_ARGB32\n
///
/// \sa VipPlotSpectrogram, VipScaleWidget

class VIP_PLOTTING_EXPORT VipColorMap : public QObject
{
	Q_OBJECT
public:
	/// Format for color mapping
	/// \sa rgb(), colorIndex(), colorTable()
	enum Format
	{
		//! The map is intended to map into RGB values.
		RGB,

		/// The map is intended to map into 8 bit values, that
		/// are indices into the color table.
		Indexed
	};

	/// How to handle values outside given interval
	enum ExternalValue
	{
		//! use the closest color
		ColorBounds,
		//! use a fixed color
		ColorFixed
	};

	enum Type
	{
		Linear,
		Alpha,
		UserType = 100
	};

	VipColorMap(Format = VipColorMap::RGB);
	virtual ~VipColorMap();

	virtual Type mapType() const = 0;

	Format format() const;
	void setFormat(Format);

	void setExternalValue(ExternalValue format, QRgb color = 0)
	{
		d_externalValue = format;
		d_externalColor = color;
		dirtyColorMap();
	}

	ExternalValue externalValue() const { return d_externalValue; }
	QRgb externalColor() const { return d_externalColor; }

	/// Map a value of a given interval into a RGB value.
	///
	/// \param interval Range for the values
	/// \param value Value
	/// \return RGB value, corresponding to value
	virtual QRgb rgb(const VipInterval& interval, double value) const = 0;

	virtual void applyColorMap(const VipInterval& interval, const VipNDArray& values, QRgb* out, int num_threads = VIP_COLOR_MAP_THREADS) const;

	/// Map a value of a given interval into a color index
	///
	/// \param interval Range for the values
	/// \param value Value
	/// \return color index, corresponding to value
	virtual unsigned char colorIndex(const VipInterval& interval, double value) const = 0;

	QColor color(const VipInterval&, double value) const;
	virtual QVector<QRgb> colorTable(const VipInterval&) const;

	virtual void startDraw() {}
	virtual void endDraw() {}

protected:
	virtual void dirtyColorMap() {}

private:
	Format d_format;
	ExternalValue d_externalValue;
	QRgb d_externalColor;
};

/// \brief VipLinearColorMap builds a color map from color stops.
///
/// A color stop is a color at a specific position. The valid
/// range for the positions is [0.0, 1.0]. When mapping a value
/// into a color it is translated into this interval according to mode().
class VIP_PLOTTING_EXPORT VipLinearColorMap : public VipColorMap
{
	Q_OBJECT

	template<class T>
	friend void applyColorMapLinear(const VipLinearColorMap* map, const VipInterval& interval, const T* values, QRgb* out, const int w, const int h, const int);

public:
	/// Mode of color map
	/// \sa setMode(), mode()
	enum Mode
	{
		//! Return the color from the next lower color stop
		FixedColors,

		//! Interpolating the colors of the adjacent stops.
		ScaledColors
	};

	/// Predefined (standard) color maps. These colormaps are pre-computed in memory, and can be copied very quickly.
	enum StandardColorMap
	{
		Unknown = -1,
		Autumn,
		Bone,
		BuRd,
		Cool,
		Copper,
		Gray,
		Hot,
		Hsv,
		Jet,
		Fusion,
		Pink,
		Rainbow,
		Spring,
		Summer,
		Sunset,
		Viridis,
		White,
		Winter,

		// color palette
		ColorPaletteStandard,
		ColorPaletteRandom,
		ColorPalettePastel,

		// Matplotlib color palettes
		ColorPalettePastel1,
		ColorPalettePastel2,
		ColorPalettePaired,
		ColorPaletteAccent,
		ColorPaletteDark2,
		ColorPaletteSet1,
		ColorPaletteSet2,
		ColorPaletteSet3,
		ColorPaletteTab10,
	};
	Q_ENUM(StandardColorMap);

	/// @brief Returns the StandardColorMap enum corresponding to given name ('autumn', 'bone', ..., 'standard', 'random', 'pastel', pastel1'...)
	static StandardColorMap colorMapFromName(const char* name);
	static const char* colorMapToName(StandardColorMap);
	/// @brief Returns the QGradientStops for given color map name ('autumn', 'bone', ..., 'standard', 'random', 'pastel', pastel1'...)
	/// The color map name can have an additional light factor that will be used to lighten/darken the QGradientStops using QColor::lighter. (example: 'jet-150')
	static QGradientStops createGradientStops(const char* name);
	static QGradientStops createGradientStops(StandardColorMap color_map);
	static VipLinearColorMap* createColorMap(StandardColorMap color_map);
	static VipLinearColorMap* createColorMap(const QGradientStops& stops);

	VipLinearColorMap(VipColorMap::Format = VipColorMap::RGB);
	VipLinearColorMap(const QColor& from, const QColor& to, VipColorMap::Format = VipColorMap::RGB);

	virtual ~VipLinearColorMap();

	virtual Type mapType() const { return Linear; }

	void setMode(Mode);
	Mode mode() const;

	StandardColorMap type() const;
	void setType(VipLinearColorMap::StandardColorMap t);

	void setUseFlatHistogram(bool);
	bool useFlatHistogram() const;

	void setFlatHistogramStrength(int);
	int flatHistogramStrength() const;

	void setColorInterval(const QColor& color1, const QColor& color2);
	void addColorStop(double value, const QColor&);

	QGradientStops gradientStops() const;
	void setGradientStops(const QGradientStops& stops);

	QVector<double> colorStops() const;
	int colorCount() const;
	QRgb colorAt(int index);
	double stopAt(int index);

	QColor color1() const;
	QColor color2() const;

	void setColorRenderCount(int num_colors);
	int colorRenderCount() const;

	virtual QRgb rgb(const VipInterval&, double value) const;
	virtual void applyColorMap(const VipInterval& interval, const VipNDArray& values, QRgb* out, int num_threads = 1) const;

	virtual unsigned char colorIndex(const VipInterval&, double value) const;

	virtual void startDraw();
	virtual void endDraw();

	// for private use only
	QRgb* colorRender() const;
	void computeRenderColors();

	class ColorStops;
	const ColorStops& internalColorStops() const;

protected:
	virtual void dirtyColorMap();
	QRgb rgbFlatHistogram(const VipInterval&, double value) const;

private:
	// Disabled copy constructor and operator=
	VipLinearColorMap(const VipLinearColorMap&);
	VipLinearColorMap& operator=(const VipLinearColorMap&);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// \brief VipAlphaColorMap varies the alpha value of a color
class VIP_PLOTTING_EXPORT VipAlphaColorMap : public VipColorMap
{
	Q_OBJECT

public:
	VipAlphaColorMap(const QColor& = QColor(Qt::gray));
	virtual ~VipAlphaColorMap();

	virtual Type mapType() const { return Alpha; }

	void setColor(const QColor&);
	QColor color() const;

	virtual QRgb rgb(const VipInterval&, double value) const;

private:
	VipAlphaColorMap(const VipAlphaColorMap&);
	VipAlphaColorMap& operator=(const VipAlphaColorMap&);

	virtual unsigned char colorIndex(const VipInterval&, double value) const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// Map a value into a color
///
/// \param interval Valid interval for values
/// \param value Value
///
/// \return Color corresponding to value
///
/// \warning This method is slow for Indexed color maps. If it is
///         necessary to map many values, its better to get the
///         color table once and find the color using colorIndex().
inline QColor VipColorMap::color(const VipInterval& interval, double value) const
{
	if (d_format == RGB) {
		return QColor(rgb(interval, value));
	}
	else {
		const unsigned int index = colorIndex(interval, value);
		return colorTable(interval)[index]; // slow
	}
}

/// \return Intended format of the color map
/// \sa Format
inline VipColorMap::Format VipColorMap::format() const
{
	return d_format;
}

inline void VipColorMap::setFormat(Format format)
{
	d_format = format;
	dirtyColorMap();
}

/// Class representing a color palette (basically a vector of colors).
class VIP_PLOTTING_EXPORT VipColorPalette
{
	QVector<QColor> d_colors;

public:
	VipColorPalette();
	VipColorPalette(const QVector<QColor>& colors);
	VipColorPalette(const QGradientStops& colors);
	VipColorPalette(VipLinearColorMap::StandardColorMap);
	VipColorPalette(const QColor& c);

	int count() const;
	const QColor& color(int) const;
	QColor color(int i, uchar alpha) const;
	void setColors(const QVector<QColor>&);
	const QVector<QColor>& colors() const { return d_colors; }
	VipColorPalette lighter(int ligthFactor = 150) const;
	VipColorPalette darker(int darkFactor = 200) const;
	VipColorPalette reorder(int increment = 3, int start = 1) const;

	bool operator==(const VipColorPalette& other);
	bool operator!=(const VipColorPalette& other);
};

Q_DECLARE_METATYPE(VipColorPalette)

class QDataStream;
VIP_PLOTTING_EXPORT QDataStream& operator<<(QDataStream& stream, const VipColorPalette& p);
VIP_PLOTTING_EXPORT QDataStream& operator>>(QDataStream& stream, VipColorPalette& p);

VIP_REGISTER_QOBJECT_METATYPE(VipColorMap*)
VIP_REGISTER_QOBJECT_METATYPE(VipLinearColorMap*)
VIP_REGISTER_QOBJECT_METATYPE(VipAlphaColorMap*)

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipColorMap* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipColorMap* value);

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipLinearColorMap* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipLinearColorMap* value);

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipAlphaColorMap* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipAlphaColorMap* value);

/// @}
// end Plotting

#endif
