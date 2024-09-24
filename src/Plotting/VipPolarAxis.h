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

#ifndef VIS_POLAR_AXIS_H
#define VIS_POLAR_AXIS_H

#include "VipAbstractScale.h"
#include "VipScaleDraw.h"

/// \addtogroup Plotting
/// @{

class VIP_PLOTTING_EXPORT VipAbstractPolarScale : public VipAbstractScale
{
	Q_OBJECT

public:
	VipAbstractPolarScale(QGraphicsItem* parent = nullptr);

	void setOuterRect(const QRectF&);
	QRectF outerRect() const;

	VipBoxStyle& axisBoxStyle();
	const VipBoxStyle& axisBoxStyle() const;
	void setAxisBoxStyle(const VipBoxStyle&);

	virtual QRectF axisRect() const = 0;

	virtual void setCenter(const QPointF& center) = 0;
	virtual QPointF center() const = 0;

private:
	QRectF d_outerRect;
	VipBoxStyle d_style;
};

class VIP_PLOTTING_EXPORT VipPolarAxis : public VipAbstractPolarScale
{
	Q_OBJECT

protected:
	virtual void setScaleDraw(VipPolarScaleDraw*);
	virtual bool hasState(const QByteArray& state, bool enable) const;

public:
	VipPolarAxis(QGraphicsItem* parent = nullptr);
	~VipPolarAxis();

	virtual QPainterPath shape() const;

	virtual void draw(QPainter* painter, QWidget* widget = 0);

	virtual void computeGeometry(bool compute_intersection_geometry = true);

	virtual const VipPolarScaleDraw* constScaleDraw() const;
	virtual VipPolarScaleDraw* scaleDraw();

	virtual QRectF axisRect() const;

	void setAdditionalRadius(double radius);
	double additionalRadius() const;

	void setCenterProximity(int);
	int centerProximity() const;

	virtual void getBorderDistHint(double& start, double& end) const;
	double minRadius() const;
	double maxRadius() const;
	double radiusExtent() const { return maxRadius() - minRadius(); }

	void setCenter(const QPointF& center);
	QPointF center() const;

	void setRadius(double radius);
	void setMaxRadius(double max_radius);
	void setMinRadius(double min_radius);
	double radius() const;

	void setStartAngle(double start);
	double startAngle() const;

	void setEndAngle(double end);
	double endAngle() const;

	double sweepLength() const;

	virtual void layoutScale();

private:
	void getBorderRadius(double& min_radius, double& max_radius) const;
	void computeScaleDrawRadiusAndCenter();

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VIP_PLOTTING_EXPORT VipRadialAxis : public VipAbstractPolarScale
{
	Q_OBJECT

protected:
	virtual void setScaleDraw(VipRadialScaleDraw*);
	bool hasState(const QByteArray& state, bool enable) const;

public:
	VipRadialAxis(QGraphicsItem* parent = nullptr);
	~VipRadialAxis();

	virtual QPainterPath shape() const;

	virtual void draw(QPainter* painter, QWidget* widget = 0);

	virtual void computeGeometry(bool compute_intersection_geometry = true);

	virtual const VipRadialScaleDraw* constScaleDraw() const;
	virtual VipRadialScaleDraw* scaleDraw();

	virtual void getBorderDistHint(double& start, double& end) const;

	void setCenter(const QPointF& center);
	QPointF center() const;

	void setRadiusRange(double start_radius, double end_radius);
	void setStartRadius(double start_radius, VipPolarAxis* axis = nullptr);
	void setEndRadius(double end_radius, VipPolarAxis* axis = nullptr);
	double startRadius() const;
	double endRadius() const;

	void setAngle(double start, VipPolarAxis* axis = nullptr, Vip::ValueType type = Vip::Relative);
	double angle() const;

	virtual void layoutScale();
	virtual QRectF axisRect() const;

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
// end Plotting

#endif
