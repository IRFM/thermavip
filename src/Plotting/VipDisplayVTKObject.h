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

#ifndef VIP_DISPLAY_DATA_OBJECT_H
#define VIP_DISPLAY_DATA_OBJECT_H

#include "VipVTKObject.h"
#include "VipFieldOfView.h"
#include "VipIODevice.h"
#include "VipPlotItem.h"
#include "VipDisplayObject.h"

#include <vtkActor.h>
#include <vtkMapper.h>
#include <vtkSmartPointer.h>

/// @brief Convert floating point color to QColor
inline QColor toQColor(const double* color)
{
	return QColor(color[0] * 255, color[1] * 255, color[2] * 255);
}

/// @brief Convert QColor to floating point color
inline void fromQColor(const QColor& c, double* color)
{
	color[0] = c.redF();
	color[1] = c.greenF();
	color[2] = c.blueF();
}

/// @brief Convert QColor to floating point color
inline double* fromQColor(const QColor& c)
{
	thread_local double color[3];
	fromQColor(c, color);
	return color;
}

/// @brief A VipPlotItem that "displays" a VTK object.
/// 
/// Actually, this item does not display anything. 
/// It is just a wrapper for the VipPlotting library of VTK objects (VipVTKObject class).
/// 
/// Its main goal is to link the VTK object displayed scalar property to a color map from the VipPlotting library.
/// Therefore, the VipPlotItem::plotInterval() is overloaded.
/// 
/// VipPlotItem also take care of creating the vtkActor and vtkMapper used
/// to display the object in a VipVTKGraphicsView
//
class VIP_PLOTTING_EXPORT VipPlotVTKObject : public VipPlotItemDataType<VipVTKObject>
{
	Q_OBJECT
public:
	VipPlotVTKObject(const VipText& title = VipText());
	~VipPlotVTKObject();

	QString dataName() const;

	bool hasActor() const;
	vtkSmartPointer<vtkMapper> mapper() const;
	vtkSmartPointer<vtkActor> actor() const;
	void range(double* range, int component) const;
	void bounds(double bounds[6]) const;

	virtual QList<VipInterval> plotBoundingIntervals() const { return QList<VipInterval>() << VipInterval() << VipInterval(); }
	virtual QPainterPath shapeFromCoordinateSystem(const VipCoordinateSystemPtr&) const { return QPainterPath(); }

	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;
	virtual QPainterPath shape() const;
	virtual QRectF boundingRect() const;

	using VipPlotItemDataType<VipVTKObject>::setAxes;
	virtual void setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type);

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void drawSelected(QPainter* painter, const VipCoordinateSystemPtr& m) const;

	virtual QList<VipText> legendNames() const { return QList<VipText>() << title(); }
	virtual QRectF drawLegend(QPainter*, const QRectF&, int index) const;

	virtual void setData(const QVariant& data);

	void setSelectedColor(const QColor& c);
	const QColor& selectedColor() const;

	virtual void setPen(const QPen&);
	virtual QPen pen() const;

	virtual void setBrush(const QBrush&);
	virtual QBrush brush() const;

	/**Set the object color, which is used to display the object in its default state*/
	void setColor(const QColor&);
	const QColor& color() const;

	/**Set the highlight color, which is used to display the object in its default state.
	The highlight color is always used when defined (if the object is not selected).
	By default, an object does not have a highlight color. You should only define a highlight color temporary and remove it when unused.*/
	void setHighlightColor(const QColor&);
	QColor highlightColor() const;
	bool hasHighlightColor() const;
	void removeHighlightColor();

	void setEdgeColor(const QColor&);
	const QColor& edgeColor() const;

	void setOpacity(double);
	double opacity() const;

	bool edgeVisible() const;
	int layer() const;
	void setEdgeVisible(bool visible);
	void setLayer(int);

	void synchronizeSelectionWith(VipPlotItem*);
	VipPlotItem* selectionSynchronizedWith() const;

public Q_SLOTS:
	void geometryChanged();

private Q_SLOTS:
	void receiveVisibilityChanged(VipPlotItem*);
	void receiveSelectionChanged(VipPlotItem*);
	void syncSelectionChanged(VipPlotItem*);

protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent*);
	virtual void hoverMoveEvent(QGraphicsSceneHoverEvent*);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent*);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* event);

	void buildMapperAndActor(const VipVTKObject& obj, bool in_set_data = false);
	void applyPropertiesInternal();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotVTKObject*)

using PlotVipVTKObjectList = QVector<VipPlotVTKObject*>;
VIP_PLOTTING_EXPORT VipVTKObjectList fromPlotVipVTKObject(const PlotVipVTKObjectList&);

/**
A VipDisplayPlotItem working on a VipPlotVTKObject.
*/
class VIP_PLOTTING_EXPORT VipDisplayVTKObject : public VipDisplayPlotItem
{
	Q_OBJECT
	qint64 m_modified;
	vtkDataObject* m_object;

public:
	VipDisplayVTKObject(QObject* parent = nullptr);
	VipPlotVTKObject* item() const { return static_cast<VipPlotVTKObject*>(VipDisplayPlotItem::item()); }
	virtual void formatItem(VipPlotItem* item, const VipAnyData& any);

protected:
	virtual void displayData(const VipAnyDataList& data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayVTKObject*)

class VTK3DPlayer;

/**
A VipPlotItem that "displays" a VipFieldOfView.
It just display a marker at the camera position.
*/
class VIP_PLOTTING_EXPORT VipPlotFieldOfView : public VipPlotItemDataType<VipFieldOfView>
{
	Q_OBJECT
public:
	VipPlotFieldOfView(const VipText& title = VipText());
	~VipPlotFieldOfView();

	virtual VipInterval plotInterval(const VipInterval& interval = Vip::InfinitInterval) const;

	virtual void draw(QPainter*, const VipCoordinateSystemPtr&) const;
	virtual void drawSelected(QPainter* painter, const VipCoordinateSystemPtr& m) const;
	virtual void setData(const QVariant& data);

	using VipPlotItemDataType<VipFieldOfView>::setAxes;
	virtual void setAxes(const QList<VipAbstractScale*>& axes, VipCoordinateSystem::Type type);

	void setSelectedColor(const QColor& c);
	const QColor& selectedColor() const;

	virtual void setPen(const QPen& p) { setSelectedColor(p.color()); }
	virtual QPen pen() const { return QPen(selectedColor()); }

	virtual void setBrush(const QBrush& b) { setSelectedColor(b.color()); }
	virtual QBrush brush() const { return QBrush(selectedColor()); }

public Q_SLOTS:
	void geometryChanged();
	void emitColorChanged() { Q_EMIT colorChanged(); }

Q_SIGNALS:
	void colorChanged();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotFieldOfView*)

class FOVItem;

/**
A VipDisplayObject used to display VipFieldOfView objects.
*/
class VIP_PLOTTING_EXPORT VipDisplayFieldOfView : public VipDisplayPlotItem
{
	Q_OBJECT

public:
	VipDisplayFieldOfView(QObject* parent = nullptr);
	~VipDisplayFieldOfView();

	VipPlotFieldOfView* item() const { return static_cast<VipPlotFieldOfView*>(VipDisplayPlotItem::item()); }

	void setFOVItem(FOVItem* item);
	FOVItem* getFOVItem() const;

protected:
	virtual void displayData(const VipAnyDataList& data);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayFieldOfView*)

#endif
