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

#ifndef VIP_POLAR_WIDGETS_H
#define VIP_POLAR_WIDGETS_H

#include "VipPlotWidget2D.h"

class VipPlotMarker;
class VipPieItem;

/**
 * Example widget representing a polar gauge and a central value.
 * The inner polar area is composed of a polar scale and a radial scale.
 * The radial axis range values scale from 0 (area center) to 100 (at the polar scale level) and is always hidden.
 *
 * This widget displays:
 *	- A polar scale on the external boundary
 *	- A polar gradient with colors going from 0xD02128 to 0x11B34C
 *	- A polar light on top of the gradient
 *	- A polar shadow below the gradient
 *	- A central text
 *	- A bottom text.
 *
 * Use #setValue function to change the current displayed value.
 */
class VIP_PLOTTING_EXPORT VipPolarValueGauge : public VipPlotPolarWidget2D
{
	Q_OBJECT

public:
	VipPolarValueGauge(QWidget* parent = nullptr);
	~VipPolarValueGauge();

	/**
	 * Set get the start and end values of the polar scale.
	 */
	void setRange(double min, double max, double tick_step = 0);
	VipInterval range() const;

	/**
	 * Set get the start and end angles of the polar scale.
	 * By default, the axis starts at -15° and ends at 195°.
	 */
	void setAngles(double start, double end);
	VipInterval angles() const;

	/**
	 * Set/get the width of the polar scale from 0 (invisible) to 100 (expand to the center)
	 */
	void setRadialWidth(double width);
	double radialWidth();

	/**
	Set/get the polar shadow size from 0 to 100. Default to 2.5 .
	The shadow is displayed below the polar gradient.
	*/
	void setShadowSize(double);
	double shadowSize() const;

	/**
	Set/get the polar light size from 0 to 100. Default to 10.
	The light is displayed on top of the polar gradient, on the external boundary.
	*/
	void setLightSize(double);
	double lightSize() const;

	/**
	 * Set/get the shadow color
	 */
	void setShadowColor(const QColor&);
	QColor shadowColor() const;

	/**
	 * Set/get the light color
	 */
	void setLightColor(const QColor&);
	QColor lightColor() const;

	/**
	 * Set/get the central text format that will exand the (double) value.
	 * Example: "<span>%3.0f&#176;</span>" expands to "120°" for a value of 120.2
	 */
	void setTextFormat(const QString& format);
	QString textFormat() const;

	/**
	 * Set/get the #VipValueToText object used to convert polar scale values to string.
	 */
	VipValueToText* scaleValueToText() const;
	void setScaleValueToText(VipValueToText*);

	/**
	 * Set/get the vertical position of the central text in radial coordinate.
	 * 0 is the widget center, 100 is the on the polar scale.
	 */
	void setTextVerticalPosition(double);
	double textVerticalPosition() const;

	/**
	 * Set/get the vertical position of the bottom text in radial coordinate.
	 * 0 is the widget center, 100 is the on the polar scale.
	 */
	void setBottomTextVerticalPosition(double);
	double bottomTextVerticalPosition() const;

	/**Returns the VipPlotMarker used to draw the bottom text */
	VipPlotMarker* bottomText() const;
	/**Returns the VipPlotMarker used to draw the central text */
	VipPlotMarker* centralText() const;
	/**Returns the VipPieItem used to draw the polar gradient */
	VipPieItem* gradientPie() const;
	/**Returns the VipPieItem used to clip the gradient pie based on the current value */
	VipPieItem* valuePie() const;
	/**Returns the VipPieItem used to draw the background of the gradient. By default, only draw a white outline. */
	VipPieItem* backgroundPie() const;
	/** Returns the VipPieItem used to draw the shadow */
	VipPieItem* shadowPie() const;
	/** Returns the VipPieItem used to draw the light */
	VipPieItem* lightPie() const;

	/**
	 * Returns the current displayed value
	 */
	double value() const;

public Q_SLOTS:
	/**
	 * Set the current displayed value
	 */
	void setValue(double);

private:
	void recomputeFullGeometry();
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
