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

#ifndef VIP_PLOT_SPECTROGRAM_H
#define VIP_PLOT_SPECTROGRAM_H

#include "VipNDArray.h"
#include "VipPlotRasterData.h"

/// \addtogroup Plotting
/// @{

typedef QMap<double, QPolygonF> ContourLines;

/// @brief A VipPlotRasterData that additionally manages iso contour lines
///
/// VipPlotSpectrogram is a VipPlotRasterData that adds the management
/// of iso contour lines.
///
/// Like VipPlotRasterData, VipPlotSpectrogram::setData() is thread safe.
///
/// VipPlotSpectrogram supports stylesheets and defines the following attributes:
/// -	'default-contour-pen': see VipPlotSpectrogram::setDefaultContourPen()
///
class VIP_PLOTTING_EXPORT VipPlotSpectrogram : public VipPlotRasterData
{
	Q_OBJECT
	Q_PROPERTY(QPen defaultContourPen READ defaultContourPen WRITE setDefaultContourPen)

public:
	/// @brief Calculate contour lines
	///
	/// @param array_2D input 2D array convertible to double
	/// @param rect Bounding rectangle for the contour lines
	/// @param levels List of limits, where to insert contour lines
	/// @param IgnoreAllVerticesOnLevel flag to customize the contouring algorithm
	///
	/// @return Calculated contour lines
	///
	/// An adaption of CONREC, a simple contouring algorithm.
	/// http://local.wasp.uwa.edu.au/~pbourke/papers/conrec/
	static ContourLines contourLines(const VipNDArray& array_2D, const QRectF& rect, const QList<vip_double>& levels, bool IgnoreAllVerticesOnLevel);

	VipPlotSpectrogram(const VipText& title = VipText());
	virtual ~VipPlotSpectrogram();

	/// @brief Reimplemented from VipPlotRasterData
	virtual void setData(const QVariant&);
	/// @brief Reimplemented from VipPlotRasterData
	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;

	/// @brief Returns the default contour pen
	QPen defaultContourPen() const;

	/// @brief Calculate the pen for a contour line
	///
	/// The color of the pen is the color for level calculated by the color map
	/// contourPen is only used if defaultContourPen().style() == Qt::NoPen
	///
	QPen contourPen(double level) const;

	/// @brief Set the levels of the contour lines
	///
	/// @param levels Values of the contour levels
	/// @param add_grip is true, add slider grip(s) to the color map in order to modify the coutour levels values
	/// @param grip_pixmap if add_grip is true, set the grip(s) pixmap
	///
	void setContourLevels(const QList<vip_double>& levels, bool add_grip = false, const QImage& grip_pixmap = QImage());

	/// @brief Returns the current contour levels
	QList<vip_double> contourLevels() const;
	/// @brief Returns the current contour lines
	ContourLines contourLines() const;

	/// @brief Returns the contour grips (if any)
	QList<VipSliderGrip*> contourGrips() const;

	/// @brief Set/get flag for the contour line extraction algorithm
	void setIgnoreAllVerticesOnLevel(bool ignore);
	bool ignoreAllVerticesOnLevel() const;

public Q_SLOTS:

	/// @brief Set the default pen for the contour lines
	///
	/// If the spectrogram has a valid default contour pen
	/// a contour line is painted using the default contour pen.
	/// Otherwise (pen.style() == Qt::NoPen) the pen is calculated
	/// for each contour level using contourPen().
	///
	void setDefaultContourPen(const QPen&);

private Q_SLOTS:

	void levelGripChanged(double);

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index = QByteArray());
	virtual void scaleDivChanged();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotSpectrogram*)
VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotSpectrogram* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotSpectrogram* value);

/// @}
// end Plotting

#endif
