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

#ifndef OFFSCREEN_EXTRACT_CONTOUR_H
#define OFFSCREEN_EXTRACT_CONTOUR_H


#include <qthread.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>

#include "VipDisplayVTKObject.h"
#include "VipVTKImage.h"

class VipPlotVTKObject;

class VIP_PLOTTING_EXPORT OffscreenExtractContour : public QObject
{
	Q_OBJECT

public:
	enum Type
	{
		Unknown,
		Point,
		Cell
	};

	enum State
	{
		Disable = 0x00,
		ExtractShape = 0x01,
		ExtractHighlitedData = 0x02,
		ExtractOutlines = 0x04,
		ExtractAll = ExtractShape | ExtractHighlitedData | ExtractOutlines
	};
	typedef QFlags<State> States;

	OffscreenExtractContour();
	~OffscreenExtractContour();

	void SetRenderWindow(vtkRenderWindow * w);

	void add(const VipPlotVTKObject*);
	void reset(const VipPlotVTKObject*);
	void remove(const VipPlotVTKObject*);
	void SetHighlightedData(const VipPlotVTKObject*);
	const VipPlotVTKObject* HighlightedData() const;

	QPainterPath Shape(const VipPlotVTKObject*) const;
	QRegion Region(const VipPlotVTKObject*) const;
	QPolygonF Outline(const VipPlotVTKObject*) const;
	QList<QPolygonF> Outlines(const VipPlotVTKObject*) const;

	const VipVTKImage & HighlightedCells() const;
	Type ObjectType();
	int ObjectId(const QPoint & pt);
	int ClosestPointId(int object_id, const QPoint & pt, QPointF * closest_point = nullptr, QPolygonF * cell = nullptr, double * cell_point = nullptr, double *weights = nullptr);
	QString Description(const QPoint & pt);

	bool IsEnabled() const;
	States GetState() const;


public Q_SLOTS:
	void SetEnabled(bool);
	void SetState(States state);

	/*
	Apply only if an input data object changed
	*/
	void Update();
	void ForceUpdate();

private:
	void Execute();
	void resetLayers();
	qint64 currentTime();

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(OffscreenExtractContour::States)

#endif