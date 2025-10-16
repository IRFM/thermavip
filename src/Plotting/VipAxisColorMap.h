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

#ifndef VIP_AXIS_COLOR_MAP_H
#define VIP_AXIS_COLOR_MAP_H

#include "VipAxisBase.h"
#include "VipColorMap.h"
#include "VipSliderGrip.h"

/// \addtogroup Plotting
/// @{


/// A vertical or horizontal axis displaying an additional color map and 2 slider grips (that can be hidden).
/// It is mostly used to display a color map for spectrograms (see VipPlotSpectrogram)
///
/// VipAxisColorMap supports stylesheets and adds the following attributes:
/// -	'color-bar-enabled': boolean value equivalent to VipAxisColorMap::setColorBarEnabled()
/// -	'color-bar-width': floating point value equivalent to VipAxisColorMap::setColorBarWidth()
/// -	'use-flat-histogram':  boolean value equivalent to VipAxisColorMap::setUseFlatHistogram()
/// -	'flat-histogram-strength': integer point value equivalent to VipAxisColorMap::setFlatHistogramStrength()
///
class VIP_PLOTTING_EXPORT VipAxisColorMap : public VipAxisBase
{
	Q_OBJECT

	friend class VipPlotItem;

public:
	/// @brief Construct from the axis alignment
	VipAxisColorMap(Alignment pos = Right, QGraphicsItem* parent = 0);
	virtual ~VipAxisColorMap();

	/// @brief Reset alignment
	virtual void reset(Alignment);

	/// @brief Show/hide the color bar
	void setColorBarEnabled(bool on);
	bool isColorBarEnabled() const;

	/// @brief Set the color bar width
	void setColorBarWidth(double width);
	double colorBarWidth() const;

	/// @brief Set the color map interval on which the color bar is drawn
	void setColorMapInterval(const VipInterval& interval);
	VipInterval colorMapInterval() const;

	/// @brief Enable/disable histogram flattening for the color map
	void setUseFlatHistogram(bool);
	bool useFlatHistogram() const;

	void setFlatHistogramStrength(int strength);
	int flatHistogramStrength() const;

	/// @brief Set the color map and value interval, that are used for displaying the color bar.
	void setColorMap(const VipInterval& interval, VipColorMap* colorMap);
	void setColorMap(const VipInterval& interval, VipLinearColorMap::StandardColorMap map);
	void setColorMap(VipLinearColorMap::StandardColorMap map);
	void setColorMap(VipColorMap* map);
	/// @brief Returns the underlying VipColorMap object used to draw the color bar
	VipColorMap* colorMap() const;
	/// @brief Returns the color bar rectangle in item's coordinate
	QRectF colorBarRect() const;

	/// @brief Set the grip interval
	void setGripInterval(const VipInterval& interval);
	VipInterval gripInterval() const;

	/// @brief Returns the first slider grip
	VipSliderGrip* grip1();
	const VipSliderGrip* grip1() const;
	/// @brief Returns the second slider grip
	VipSliderGrip* grip2();
	const VipSliderGrip* grip2() const;

	/// @brief Add a new slider grip
	VipSliderGrip* addGrip(VipSliderGrip*);
	VipSliderGrip* addGrip(const QImage& img = QImage());
	/// @brief Remove grip but do NOT delete it
	void removeGrip(VipSliderGrip*);
	QList<VipSliderGrip*> grips() const;

	/// @brief Set a maximum value above which values are discarded when computing the scale div in case of autoscaling.
	/// Set a NaN value to disable this feature.
	void setAutoScaleMax(vip_double value);
	vip_double autoScaleMax() const;
	bool hasAutoScaleMax() const;
	void setHasAutoScaleMax(bool);

	/// @brief Set a minimum value under which values are discarded when computing the scale div in case of autoscaling.
	/// Set a NaN value to disable this feature.
	void setAutoScaleMin(vip_double value);
	vip_double autoScaleMin() const;
	bool hasAutoScaleMin() const;
	void setHasAutoScaleMin(bool);

	/// @brief Based on #autoScaleMin() and #autoScaleMax(), build a valid interval that can be passed to #VipPlotItem::plotInterval function.
	VipInterval validInterval() const;

	void divideAxisScale(vip_double min, vip_double max, vip_double stepSize = 0.);

	virtual void draw(QPainter* painter, QWidget* widget = 0);

	virtual void startRender(VipRenderState& state);
	virtual void endRender(VipRenderState& state);

	virtual void computeScaleDiv();

	/// Returns the list of VipPlotItem related to this color map.
	const QList<VipPlotItem*> itemList() const;

	virtual double extentForLength(double length) const;

Q_SIGNALS:

	void valueChanged(double);
	void colorMapChanged(int);
	void useFlatHistogramChanged(bool);

protected:
	virtual bool setItemProperty(const char* name, const QVariant& value, const QByteArray& index);

	void drawColorBar(QPainter* painter, const QRectF& rect) const;

	virtual void itemGeometryChanged(const QRectF&);
	virtual double additionalSpace() const;

private Q_SLOTS:

	void scaleDivHasChanged();
	void gripValueChanged(double);

private:
	QRectF colorBarRect(const QRectF& rect) const;

	void addItem(VipPlotItem*);
	void removeItem(VipPlotItem*);

	
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_REGISTER_QOBJECT_METATYPE(VipAxisColorMap*)

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipAxisColorMap* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipAxisColorMap* value);

/// @}
// end Plotting

#endif
