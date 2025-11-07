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

#include <vtkObject.h>
#include <vtkOpenGLError.h>

#include "p_VTKOffscreenExtractContour.h"
#include "VipVTKWidget.h"
#include "VipDisplayVTKObject.h"

#include "VipCore.h"
#include "VipPlotSpectrogram.h"
#include "VipPolygon.h"
#include "VipTimer.h"

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkContourFilter.h>
#include <vtkCoordinate.h>
#include <vtkCubeAxesActor.h>
#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkInteractorObserver.h>
#include <vtkInteractorStyle.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPointData.h>
#include <vtkPointPicker.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRendererCollection.h>
#include <vtkTextProperty.h>
#include <vtkTransform.h>
#include <vtkWindowToImageFilter.h>
#include <vtkPolyDataMapper.h>

#include <QApplication>
#include <qdatetime.h>
#include <qtimer.h>

#include <sstream>

// dot product (2D) which allows vector operations in arguments
#define dot2D(u, v) ((u[0]) * (v[0]) + (u[1]) * (v[1]))

// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
static void Barycentric2D(double a[2], double b[2], double c[2], double p[2], double& u, double& v, double& w)
{
	double v0[2] = { b[0] - a[0], b[1] - a[1] };
	double v1[2] = { c[0] - a[0], c[1] - a[1] };
	double v2[2] = { p[0] - a[0], p[1] - a[1] };
	double d00 = dot2D(v0, v0);
	double d01 = dot2D(v0, v1);
	double d11 = dot2D(v1, v1);
	double d20 = dot2D(v2, v0);
	double d21 = dot2D(v2, v1);
	double invDenom = 1.0 / (d00 * d11 - d01 * d01);
	v = (d11 * d20 - d01 * d21) * invDenom;
	w = (d00 * d21 - d01 * d20) * invDenom;
	u = 1.0f - v - w;
}

/**
Convert a value to VTK RGB normalized color
*/
static inline void toColor(unsigned int value, double* color)
{
	color[0] = ((value & 0xFF0000) >> 16) / 254.99;
	color[1] = ((value & 0xFF00) >> 8) / 254.99;
	color[2] = (value & 0xFF) / 254.99;
}

/**
Convert a value to VTK RGB color
*/
static inline void toColorUChar(unsigned int value, double* color)
{
	color[0] = ((value & 0xFF0000) >> 16);
	color[1] = ((value & 0xFF00) >> 8);
	color[2] = (value & 0xFF);
}

/**
Convert a color from a RGBA vtkImageData to a value.
Use with #toColor.
*/
static inline unsigned int toValue(double* color)
{
	return (int(color[0] * 255) << 16) | (int(color[1] * 255) << 8) | (int(color[2] * 255));
}

static inline unsigned int toValue(QRgb color)
{
	return (qBlue(color) << 16) | (qGreen(color) << 8) | (qRed(color));
}

static bool blockedByModalWidget(QWidget* widget)
{
	if (QWidget* w = QApplication::activeModalWidget()) {
		while (widget) {
			if (w == widget)
				return false;
			widget = widget->parentWidget();
		}
		return true;
	}
	else
		return false;
}

struct ExtractContour
{
	using ContourLevel = QList<QPolygonF>;
	using ContourLevels = QMap<int, ContourLevel>;

	VipNDArrayType<unsigned, 2> labels;

	ContourLevels extract(const VipNDArrayTypeView<unsigned>& img, const QRect& bounding, unsigned background)
	{
		if (labels.shape() != img.shape()) {
			labels = VipNDArrayType<unsigned, 2>(img.shape());
		}
		labels.fill(0);

		VipNDArrayTypeView<unsigned, 2> out(labels);
		vipLabelImage(img, out, background);

		ContourLevels res;

		int height = img.shape(0);
		unsigned* ptr = labels.ptr();
		unsigned last_visited = 0;
		for (int y = bounding.top(); y < bounding.bottom(); ++y) {
			for (int x = bounding.left(); x < bounding.right(); ++x) {
				unsigned pix = labels(y, x);
				if (pix > last_visited) {
					QPolygonF poly = vipExtractMaskPolygon(labels, pix, 0, QPoint(x, y));
					// mirror vertical
					for (int i = 0; i < poly.size(); ++i)
						poly[i].setY(height - poly[i].y() - 1);

					res[img(y, x)].append(poly);
					last_visited = pix;
				}
			}
		}
		return res;
	}
};

class OffscreenExtractContour::PrivateData
{
public:
	QRecursiveMutex mutex;
	std::map<const VipPlotVTKObject*, VipPlotVTKObject> data;
	std::map<const VipPlotVTKObject*, QPainterPath> shapes;
	std::map<const VipPlotVTKObject*, QRegion> regions;
	std::map<const VipPlotVTKObject*, QPolygonF> outlines;
	std::map<const VipPlotVTKObject*, QList<QPolygonF>> outlines_list;
	// vtkSmartPointer<vtkRenderer> render;
	QList<vtkRenderer*> renderers;
	vtkSmartPointer<vtkRenderWindow> renderWin;
	vtkSmartPointer<vtkWindowToImageFilter> filter;
	qint64 mTime;
	VipVTKImage image;
	States state;
	vtkRenderWindow* realRenderWin;

	const VipPlotVTKObject* highlightedData{ nullptr };
	VipPlotVTKObject highlightedDataRender;
	vtkSmartPointer<vtkUnsignedCharArray> highlightedCellData;
	vtkSmartPointer<vtkRenderer> highlightedRender;
	vtkSmartPointer<vtkRenderWindow> highlightedRenderWin;
	vtkSmartPointer<vtkWindowToImageFilter> highlightedFilter;
	VipVTKImage highlightedCells;

	ExtractContour extractor;

	QTimer timer;
	qint64 lastReset = 0;
	bool mightNeedRest = false;
};

OffscreenExtractContour::OffscreenExtractContour()
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->realRenderWin = nullptr;
	d_data->state = ExtractAll;
	d_data->mTime = 0;

	d_data->highlightedRender = vtkSmartPointer<vtkRenderer>::New();
	d_data->highlightedRender->SetBackground(0, 0, 0);
	d_data->highlightedRender->UseShadowsOff();
	VIP_VTK_OBSERVER(d_data->highlightedRender);

	d_data->renderWin = vtkSmartPointer<vtkRenderWindow>::New();
	d_data->renderWin->SetOffScreenRendering(1);
	// d_data->renderWin->AddRenderer(d_data->render);
	d_data->renderWin->Render();
	VIP_VTK_OBSERVER(d_data->renderWin);

	d_data->highlightedRenderWin = vtkSmartPointer<vtkRenderWindow>::New();
	d_data->highlightedRenderWin->SetOffScreenRendering(1);
	d_data->highlightedRenderWin->SetMultiSamples(1); // disable antialiasing
	// d_data->highlightedRender->UseFXAAOff();
	d_data->highlightedRenderWin->AddRenderer(d_data->highlightedRender);
	d_data->highlightedRenderWin->Render();
	VIP_VTK_OBSERVER(d_data->highlightedRenderWin);

	d_data->filter = vtkSmartPointer<vtkWindowToImageFilter>::New();
	d_data->filter->SetInput(d_data->renderWin);
	d_data->filter->SetInputBufferType(VTK_RGBA);
	d_data->filter->ReadFrontBufferOff();
	VIP_VTK_OBSERVER(d_data->filter);

	d_data->highlightedFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
	d_data->highlightedFilter->SetInput(d_data->highlightedRenderWin);
	d_data->highlightedFilter->SetInputBufferType(VTK_RGBA);
	d_data->highlightedFilter->ReadFrontBufferOff();
	VIP_VTK_OBSERVER(d_data->highlightedFilter);

	d_data->highlightedCellData = vtkSmartPointer<vtkUnsignedCharArray>::New();
	d_data->highlightedCellData->SetName("extract");
	d_data->highlightedCellData->SetNumberOfComponents(3);
	VIP_VTK_OBSERVER(d_data->highlightedCellData);

	// d_data->timer.setSingleShot(true);
	d_data->timer.setInterval(20);
	d_data->timer.setSingleShot(false);
	connect(&d_data->timer, SIGNAL(timeout()), this, SLOT(Update()));
	d_data->timer.start();
}

OffscreenExtractContour::~OffscreenExtractContour()
{
	{
		QMutexLocker lock(&d_data->mutex);
		d_data->timer.stop();
		disconnect(&d_data->timer, SIGNAL(timeout()), this, SLOT(Update()));

		/*vtkRendererCollection * this_col = d_data->renderWin->GetRenderers();

		this_col->InitTraversal();
		while (vtkRenderer * tmp = this_col->GetNextItem())
			tmp->Delete();
			*/

		d_data->filter = vtkSmartPointer<vtkWindowToImageFilter>();
		d_data->renderWin = vtkSmartPointer<vtkRenderWindow>();
	}
}

void OffscreenExtractContour::MightNeedReset()
{
	d_data->mightNeedRest = true;
}

void OffscreenExtractContour::Reset()
{
	QMutexLocker lock(&d_data->mutex);

	const VipPlotVTKObject * highlight = HighlightedData();
	SetHighlightedData(nullptr);

	d_data->highlightedRender = vtkSmartPointer<vtkRenderer>::New();
	d_data->highlightedRender->SetBackground(0, 0, 0);
	d_data->highlightedRender->UseShadowsOff();
	VIP_VTK_OBSERVER(d_data->highlightedRender);

	d_data->renderWin = vtkSmartPointer<vtkRenderWindow>::New();
	d_data->renderWin->SetOffScreenRendering(1);
	// d_data->renderWin->AddRenderer(d_data->render);
	d_data->renderWin->Render();
	VIP_VTK_OBSERVER(d_data->renderWin);

	d_data->highlightedRenderWin = vtkSmartPointer<vtkRenderWindow>::New();
	d_data->highlightedRenderWin->SetOffScreenRendering(1);
	d_data->highlightedRenderWin->SetMultiSamples(1); // disable antialiasing
	// d_data->highlightedRender->UseFXAAOff();
	d_data->highlightedRenderWin->AddRenderer(d_data->highlightedRender);
	d_data->highlightedRenderWin->Render();
	VIP_VTK_OBSERVER(d_data->highlightedRenderWin);

	d_data->filter = vtkSmartPointer<vtkWindowToImageFilter>::New();
	d_data->filter->SetInput(d_data->renderWin);
	d_data->filter->SetInputBufferType(VTK_RGBA);
	d_data->filter->ReadFrontBufferOff();
	VIP_VTK_OBSERVER(d_data->filter);

	d_data->highlightedFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
	d_data->highlightedFilter->SetInput(d_data->highlightedRenderWin);
	d_data->highlightedFilter->SetInputBufferType(VTK_RGBA);
	d_data->highlightedFilter->ReadFrontBufferOff();
	VIP_VTK_OBSERVER(d_data->highlightedFilter);

	d_data->highlightedCellData = vtkSmartPointer<vtkUnsignedCharArray>::New();
	d_data->highlightedCellData->SetName("extract");
	d_data->highlightedCellData->SetNumberOfComponents(3);
	VIP_VTK_OBSERVER(d_data->highlightedCellData);




	vtkRenderWindow  *w = d_data->realRenderWin;
	vtkRendererCollection* col = w->GetRenderers();
	vtkRendererCollection* this_col = d_data->renderWin->GetRenderers();
	this_col->RemoveAllItems();

	col->InitTraversal();
	while (vtkRenderer* tmp = col->GetNextItem()) {
		vtkRenderer* ren = vtkRenderer::New();
		VIP_VTK_OBSERVER(ren);
		ren->SetLayer(tmp->GetLayer());
		ren->SetBackground(0, 0, 0);
		ren->UseShadowsOff();
		d_data->renderWin->AddRenderer(ren);
		ren->Delete();
	}

	d_data->renderers.clear();
	this_col->InitTraversal();
	while (vtkRenderer* tmp = this_col->GetNextItem()) {
		// NEW for vtk9.1
		tmp->GetActiveCamera();
		tmp->Render();
		d_data->renderers.append(tmp);
	}

	d_data->renderWin->SetNumberOfLayers(d_data->renderers.size());


	for (auto it = d_data->data.begin(); it != d_data->data.end(); ++it) {
		const VipPlotVTKObject* data = it->first;
		VipPlotVTKObject* dst= &it->second;

		VipVTKObjectLocker locker = vipLockVTKObjects(data->rawData());
		VipVTKObject ptr(data->rawData().data());
		dst->setRawData(ptr);
		dst->setLayer(data->layer());
		if (vtkActor* act = dst->actor()) {
			act->GetProperty()->LightingOff();
			d_data->renderers[data->layer()]->AddActor(act);
		}
		if (vtkMapper* m = dst->mapper())
			m->SetScalarVisibility(0);
	}

	SetHighlightedData(highlight);
}

void OffscreenExtractContour::ForceUpdate()
{
	d_data->mTime = 0;

	d_data->filter->Modified();
	d_data->renderWin->Modified();
	d_data->highlightedRender->Modified();

	Execute();
}

void OffscreenExtractContour::Update()
{
	Execute();
}

void OffscreenExtractContour::SetEnabled(bool enabled)
{
	SetState(enabled ? ExtractAll : ExtractShape);
}

bool OffscreenExtractContour::IsEnabled() const
{
	return d_data->state == ExtractAll;
}

OffscreenExtractContour::States OffscreenExtractContour::GetState() const
{
	return d_data->state;
}

void OffscreenExtractContour::SetState(States state)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->state = state;
	if (d_data->realRenderWin)
		d_data->realRenderWin->Modified();
}

void OffscreenExtractContour::SetRenderWindow(vtkRenderWindow* w)
{
	QMutexLocker lock(&d_data->mutex);
	d_data->realRenderWin = w;

	vtkRendererCollection* col = w->GetRenderers();
	vtkRendererCollection* this_col = d_data->renderWin->GetRenderers();
	this_col->RemoveAllItems();

	col->InitTraversal();
	while (vtkRenderer* tmp = col->GetNextItem()) {
		vtkRenderer* ren = vtkRenderer::New();
		VIP_VTK_OBSERVER(ren);
		ren->SetLayer(tmp->GetLayer());
		ren->SetBackground(0, 0, 0);
		ren->UseShadowsOff();
		d_data->renderWin->AddRenderer(ren);
		ren->Delete();
	}

	d_data->renderers.clear();
	this_col->InitTraversal();
	while (vtkRenderer* tmp = this_col->GetNextItem()) {
		// NEW for vtk9.1
		tmp->GetActiveCamera();
		tmp->Render();
		d_data->renderers.append(tmp);
	}

	d_data->renderWin->SetNumberOfLayers(d_data->renderers.size());
}

const VipPlotVTKObject* OffscreenExtractContour::HighlightedData() const
{
	return d_data->highlightedData;
}

void OffscreenExtractContour::SetHighlightedData(const VipPlotVTKObject* data)
{
	if (data == d_data->highlightedData)
		return;

	QMutexLocker lock(&d_data->mutex);
	VipVTKObjectLocker locker = vipLockVTKObjects(data ? data->rawData() : VipVTKObject());

	if (d_data->highlightedDataRender.actor()) {
		d_data->highlightedRender->RemoveActor(d_data->highlightedDataRender.actor());
		d_data->highlightedDataRender.setRawData(VipVTKObject());
	}

	d_data->highlightedData = data;
	if (data) {
		d_data->highlightedDataRender.setRawData(VipVTKObject(data->rawData().data()));
		if (d_data->highlightedDataRender.actor()) {
			d_data->highlightedDataRender.actor()->GetProperty()->LightingOff();
			double color[4] = { 1, 0, 0, 1 };
			d_data->highlightedDataRender.actor()->GetProperty()->SetColor(color);
			d_data->highlightedRender->AddActor(d_data->highlightedDataRender.actor());
		}
	}

	d_data->mTime = 0;
}

const VipVTKImage& OffscreenExtractContour::HighlightedCells() const
{
	return d_data->highlightedCells;
}

OffscreenExtractContour::Type OffscreenExtractContour::ObjectType()
{
	QMutexLocker lock(&d_data->mutex);
	VipVTKObject obj;
	if (d_data->highlightedData)
		obj = d_data->highlightedData->rawData();
	VipVTKObjectLocker data_locker = vipLockVTKObjects(obj);

	if (!d_data->highlightedData)
		return Unknown;
	else if (vtkDataSet* set = obj.dataSet()) {
		if (set->GetNumberOfCells())
			return Cell;
		else
			return Point;
	}
	return Unknown;
}

int OffscreenExtractContour::GlobalObjectId(const QPoint& pos)
{
	QMutexLocker lock(&d_data->mutex);
	if (pos.x() >= 0 && pos.y() >= 0 && pos.x() < d_data->image.width() && pos.y() < d_data->image.height()) {
		QPoint pt(pos.x(), d_data->image.height() - pos.y() - 1);
		return ((QRgb*)d_data->image.image()->GetScalarPointer())[(pt.x(), pt.y() * d_data->image.width())] - 1;
	}
	return -1;
}

int OffscreenExtractContour::ObjectId(const QPoint& pos)
{
	QMutexLocker lock(&d_data->mutex);
	int id1 = -1;
	if (pos.x() >= 0 && pos.y() >= 0 && pos.x() < d_data->highlightedCells.width() && pos.y() < d_data->highlightedCells.height())
		id1 = ((QRgb*)d_data->highlightedCells.image()->GetScalarPointer())[pos.y() * d_data->highlightedCells.width() + pos.x()] - 1;
	return id1;
}

int OffscreenExtractContour::ClosestPointId(int object_id, const QPoint& pos, QPointF* point_pos, QPolygonF* cell, double* cell_point, double* weights)
{
	OffscreenExtractContour::Type type = ObjectType();
	if (type == Unknown || object_id < 0)
		return -1;
	else if (type == Point)
		return object_id;

	QMutexLocker lock(&d_data->mutex);
	if (d_data->highlightedData)
		if (vtkDataSet* set = d_data->highlightedData->rawData().dataSet()) {
			// lock the display and the highlighted data
			VipVTKObjectLocker data_locker = vipLockVTKObjects(d_data->highlightedData->rawData());

			// get the cell points, find the closest one

			int num_cells = set->GetNumberOfCells();
			if (object_id >= num_cells)
				return -1;

			vtkIdList* lst = vtkIdList::New();
			VIP_VTK_OBSERVER(lst);
			set->GetCellPoints(object_id, lst);

			int height = d_data->realRenderWin->GetSize()[1];
			d_data->realRenderWin->GetRenderers()->InitTraversal();
			vtkRenderer* renderer = d_data->realRenderWin->GetRenderers()->GetNextItem();

			vtkCoordinate* coord = vtkCoordinate::New();
			VIP_VTK_OBSERVER(coord);
			coord->SetCoordinateSystemToWorld();
			coord->SetViewport(renderer);

			int dist = INT_MAX;
			int id = -1;

			// 2D triangle points
			double t[3][2];
			// 3D cell points
			double c[3][3];

			// Debug
			/* {
				QString out = QString::number(object_id) + " ";
				double tmp[3] = { 0, 0, 0 };
				for (int i = 0; i < lst->GetNumberOfIds(); ++i) {
					double pt[3];
					set->GetPoint(lst->GetId(i), pt);
					tmp[0] += pt[0];
					tmp[1] += pt[1];
					tmp[2] += pt[2];
				}
				tmp[0] /= lst->GetNumberOfIds();
				tmp[1] /= lst->GetNumberOfIds();
				tmp[2] /= lst->GetNumberOfIds();
				out += QString::number(tmp[0]) + " " + QString::number(tmp[1]) + " " + QString::number(tmp[2]);
				vip_debug(out.toLatin1().data());
			}*/

			for (int i = 0; i < lst->GetNumberOfIds(); ++i) {
				double pt[3];
				set->GetPoint(lst->GetId(i), pt);
				coord->SetValue(pt);
				int* world = coord->GetComputedDisplayValue(renderer);
				double* normalize = coord->GetComputedDoubleDisplayValue(renderer);
				QPoint point(world[0], height - world[1] - 1);
				int p_dist = (pos - point).manhattanLength();
				if (p_dist < dist) {
					dist = p_dist;
					id = lst->GetId(i);

					if (point_pos)
						*point_pos = QPointF(normalize[0], height - normalize[1] - 1);
				}

				if (i < 3) {
					t[i][0] = normalize[0];
					t[i][1] = height - normalize[1] - 1;
					memcpy(c[i], pt, sizeof(pt));
				}

				if (cell)
					cell->append(QPointF(normalize[0], height - normalize[1] - 1));
			}

			// compute weights and cell point for triangle only
			if ((cell_point || weights) && lst->GetNumberOfIds() == 3) {
				double ws[3];
				double pt[2] = { (double)pos.x(), (double)pos.y() };
				Barycentric2D(t[0], t[1], t[2], pt, ws[0], ws[1], ws[2]);
				if (weights)
					memcpy(weights, ws, sizeof(ws));
				if (cell_point) {
					cell_point[0] = c[0][0] * ws[0] + c[1][0] * ws[1] + c[2][0] * ws[2];
					cell_point[1] = c[0][1] * ws[0] + c[1][1] * ws[1] + c[2][1] * ws[2];
					cell_point[2] = c[0][2] * ws[0] + c[1][2] * ws[1] + c[2][2] * ws[2];
				}
			}

			lst->Delete();
			coord->Delete();

			return id;
		}

	return -1;
}

QString OffscreenExtractContour::Description(const QPoint& pt)
{
	if (!d_data->highlightedData)
		return QString();

	QString res;
	OffscreenExtractContour::Type type = ObjectType();
	int object_id = ObjectId(pt);
	double cell_point[3], weights[3];
	QPolygonF cell_points;
	int point_id = ClosestPointId(object_id, pt, nullptr, &cell_points, cell_point, weights);

	QMutexLocker lock(&d_data->mutex);
	VipVTKObject obj;
	if (d_data->highlightedData)
		obj = d_data->highlightedData->rawData();
	VipVTKObjectLocker locker = vipLockVTKObjects(obj);
	vtkDataSet* set = obj.dataSet();
	if (type == Unknown || object_id < 0)
		return res;

	// first paragraph: general infos
	res += "<p><b>Name</b>: " + obj.dataName() + "<br>";
	res += "<p><b>Type</b>: " + obj.className() + "<br>";
	if (set) {
		res += "<b>Cell count</b>: " + QString::number(set->GetNumberOfCells()) + "<br>";
		res += "<b>Point count</b>: " + QString::number(set->GetNumberOfPoints());
	}
	res += "</p>";

	// cell id, point id and coordinates
	if (set) {
		res += "<p>";
		if (type == Cell) {
			res += "<b>Cell id</b> :" + QString::number(object_id) + "<br>";
			if (cell_points.size() == 3)
				res += "<b>Cell point: </b> :" + QString::number(cell_point[0]) + ", " + QString::number(cell_point[1]) + ", " + QString::number(cell_point[2]) + "<br>";
		}
		res += "<b>Closest point id: </b> :" + QString::number(point_id) + "<br>";
		double point[3];
		set->GetPoint(point_id, point);
		res += "<b>Closest point coordinates: </b> :" + QString::number(point[0]) + ", " + QString::number(point[1]) + ", " + QString::number(point[2]) + "<br>";
		res += "</p>";
	}

	// field attributes
	QVector<vtkAbstractArray*> arrays = obj.fieldAttributeArrays();
	if (arrays.size()) {
		res += "<p>";
		QStringList attributes;
		for (int i = 0; i < arrays.size(); ++i) {
			vtkVariantList lst = obj.makeAttribute(arrays[i], 0).second;
			QStringList values;
			for (int j = 0; j < lst.size(); ++j) {
				std::ostringstream oss;
				oss << lst[j];
				values << oss.str().c_str();
			}
			attributes << "<b>" + QString(arrays[i]->GetName()) + "</b>: " + values.join(", ");
		}
		res += attributes.join("<br>");
		res += "</p>";
	}

	// cell attributes
	if (type == Cell && set) {
		arrays = obj.cellsAttributes();
		if (arrays.size()) {
			res += "<p>";
			QStringList attributes;
			for (int i = 0; i < arrays.size(); ++i) {
				vtkVariantList lst = obj.makeAttribute(arrays[i], object_id).second;
				QStringList values;
				for (int j = 0; j < lst.size(); ++j) {
					std::ostringstream oss;
					oss << lst[j];
					values << oss.str().c_str();
				}
				attributes << "<b>" + QString(arrays[i]->GetName()) + "</b>: " + values.join(", ");
			}
			res += attributes.join("<br>");
			res += "</p>";
		}
	}

	// points attributes
	arrays = obj.pointsAttributes();
	if (arrays.size() && set) {
		res += "<p>";
		QStringList attributes;
		for (int i = 0; i < arrays.size(); ++i) {
			vtkVariantList lst = obj.makeAttribute(arrays[i], point_id).second;
			QStringList values;
			for (int j = 0; j < lst.size(); ++j) {
				std::ostringstream oss;
				oss << lst[j];
				values << oss.str().c_str();
			}
			attributes << "<b>" + QString(arrays[i]->GetName()) + "</b>: " + values.join(", ");
		}
		res += attributes.join("<br>");
		res += "</p>";
	}

	res = QString("<div align='left'>") + res + QString("</div>");
	return res;
}

void OffscreenExtractContour::resetLayers()
{

	// set the right layer for all data object and set the actors to the right renderers

	for (auto it = d_data->data.begin(); it != d_data->data.end(); ++it) {
		// change the internal data object if needed
		const VipPlotVTKObject* src = it->first;
		VipVTKObject src_data = src->rawData();
		VipPlotVTKObject& dst = it->second;
		VipVTKObject dst_data = dst.rawData();

		if (src_data.data() != dst_data.data() || src->layer() != dst.layer()) {
			d_data->renderers[dst.layer()]->RemoveActor(dst.actor());

			dst_data = VipVTKObject(src_data.data());
			dst.setRawData(dst_data);
			dst.setLayer(src->layer());
			d_data->renderers[dst.layer()]->AddActor(dst.actor());

			if (vtkMapper* m = dst.mapper())
				m->SetScalarVisibility(0);
		}
	}
}

void OffscreenExtractContour::add(const VipPlotVTKObject* data)
{
	QMutexLocker lock(&d_data->mutex);
	VipPlotVTKObject* dst;
	auto it = d_data->data.find(data);
	if (it == d_data->data.end())
		it = d_data->data.try_emplace(data).first;
	dst = &it->second;

	VipVTKObjectLocker locker = vipLockVTKObjects(data->rawData());
	VipVTKObject ptr(data->rawData().data());
	dst->setRawData(ptr);
	dst->setLayer(data->layer());
	if (vtkActor* act = dst->actor()) {
		act->GetProperty()->LightingOff();
		d_data->renderers[data->layer()]->AddActor(act);
	}
	if (vtkMapper* m = dst->mapper())
		m->SetScalarVisibility(0);

	d_data->mTime = 0;
}

void OffscreenExtractContour::reset(const VipPlotVTKObject* data)
{
	add(data);
}

void OffscreenExtractContour::remove(const VipPlotVTKObject* data)
{
	QMutexLocker lock(&d_data->mutex);

	auto it = d_data->data.find(data);
	if (it != d_data->data.end()) {
		VipVTKObjectLocker lock = vipLockVTKObjects(it->first->rawData());
		d_data->renderers[it->second.layer()]->RemoveActor(it->second.actor());
		d_data->data.erase(data);
		d_data->shapes.erase(data);
		d_data->outlines.erase(data);
		d_data->outlines_list.erase(data);
	}

	if (data == d_data->highlightedData)
		SetHighlightedData(nullptr);

	d_data->mTime = 0;
}

QPainterPath OffscreenExtractContour::Shape(const VipPlotVTKObject* data) const
{
	QMutexLocker lock(&d_data->mutex);
	auto it = d_data->shapes.find(data);
	if (it != d_data->shapes.end())
		return it->second;
	return QPainterPath();
}

QRegion OffscreenExtractContour::Region(const VipPlotVTKObject* data) const
{
	QMutexLocker lock(&d_data->mutex);
	auto it = d_data->regions.find(data);
	if (it != d_data->regions.end())
		return it->second;
	return QRegion();
}

QPolygonF OffscreenExtractContour::Outline(const VipPlotVTKObject* data) const
{
	QMutexLocker lock(&d_data->mutex);
	auto it = d_data->outlines.find(data);
	if (it != d_data->outlines.end())
		return it->second;
	return QPolygonF();
}

QList<QPolygonF> OffscreenExtractContour::Outlines(const VipPlotVTKObject* data) const
{
	QMutexLocker lock(&d_data->mutex);
	auto it = d_data->outlines_list.find(data);
	if (it != d_data->outlines_list.end())
		return it->second;
	return QList<QPolygonF>();
}

qint64 OffscreenExtractContour::currentTime()
{
	d_data->filter->Modified();
	return d_data->filter->GetMTime();
}

static int vipVTKOpenGLCheckErrors()
{
	const int maxErrors = 16;
	unsigned int errCode[maxErrors] = { 0 };
	const char* errDesc[maxErrors] = { NULL };

	int numErrors = vtkGetOpenGLErrors(maxErrors, errCode, errDesc);

	return (numErrors);
}

void OffscreenExtractContour::Execute()
{
	qint64 start = QDateTime::currentMSecsSinceEpoch();
	qint64 el1 = 0, el2 = 0, el3 = 0, el4 = 0, el5 = 0, el6 = 0, el7 = 0;

	if (!d_data->realRenderWin || !d_data->data.size()) {
		return;
	}

	QMutexLocker lock(&d_data->mutex);

	resetLayers();

	if (d_data->state == Disable) {
		d_data->shapes.clear();
		d_data->regions.clear();
		d_data->outlines.clear();
		d_data->outlines_list.clear();
		return;
	}

	if (blockedByModalWidget(nullptr))
		return;

	// check if update is required

	bool render_win_up_to_date = d_data->mTime > (qint64)d_data->realRenderWin->GetMTime();
	bool interactor_up_to_date = true;
	// d_data->mTime > d_data->realRenderWin->GetInteractor()->GetMTime();
	if (render_win_up_to_date && interactor_up_to_date) {
		// check if the data changed
		bool need_update = false;
		for (auto it = d_data->data.begin(); it != d_data->data.end(); ++it) {
			VipVTKObject obj = it->first->rawData();
			auto l = vipLockVTKObjects(obj);
			if (obj.data())
				if ((qint64)obj.data()->GetMTime() > d_data->mTime || (qint64)it->first->actor()->GetMTime() > d_data->mTime) {
					need_update = true;
					break;
				}
		}

		// check if the renderers or cameras changed
		if (!need_update) {
			if (d_data->mTime < (qint64)d_data->realRenderWin->GetRenderers()->GetFirstRenderer()->GetActiveCamera()->GetMTime())
				need_update = true;

			/*for (int i = 0; i < d_data->renderers.size(); ++i)
			{
				//qint64 ren_time = d_data->renderers[i]->GetMTime();
				qint64 cam_time = d_data->renderers[i]->GetActiveCamera()->GetMTime();
				//currently only check camera time
				if ( cam_time > d_data->mTime)
				{
					need_update = true;
					break;
				}
			}*/
		}

		if (!need_update)
			return;
	}
	else
		bool stop = true; // TEST

	// qint64 cam_time = d_data->renderers[0]->GetActiveCamera()->GetMTime();
	// start algorithm

	// printf("start rendering offscreen\n"); fflush(stdout);
	d_data->mTime = currentTime();

	//
	// Draw the different actors
	//

	QVector<VipVTKObject> keys;
	keys.reserve((int)d_data->data.size());
	for (auto it = d_data->data.begin(); it != d_data->data.end(); ++it)
		keys.push_back(it->first->rawData());

	// set the render window size, set the renderers camera
	d_data->renderWin->SetSize(d_data->realRenderWin->GetSize());
	d_data->renderWin->Modified();
	d_data->realRenderWin->GetRenderers()->InitTraversal();
	vtkCamera* camera = d_data->realRenderWin->GetRenderers()->GetFirstRenderer()->GetActiveCamera();
	{
		auto lockers = vipLockVTKObjects(keys);
		for (int i = 0; i < d_data->renderers.size(); ++i) {
			d_data->renderers[i]->GetActiveCamera()->DeepCopy(camera);
			d_data->renderers[i]->GetActiveCamera()->Modified();
			double range[6];
			d_data->renderers[i]->ComputeVisiblePropBounds(range);
			d_data->renderers[i]->ResetCameraClippingRange(range);
			d_data->renderers[i]->Modified();
		}
	}

	// create the colors for each data, from red to green
	unsigned int value = 1;
	QMap<int, const VipPlotVTKObject*> data_levels;
	QList<vip_double> levels;

	{
		auto lockers = vipLockVTKObjects(keys);
		for (auto it = d_data->data.begin(); it != d_data->data.end(); ++it, ++value) {
			it->second.actor()->SetVisibility(it->first->actor()->GetVisibility());
			it->second.actor()->Modified();
			if (it->second.mapper()) {
				it->second.mapper()->SetScalarVisibility(0);
				it->second.mapper()->Modified();
			}
			if (it->first->actor()->GetVisibility() == 0)
				continue;
			double color[4] = { 0, 0, 0, 1 };
			toColor(value, color);

			it->second.setColor(vipToQColor(color));
			data_levels[value] = it->first;
			levels << value;
		}

		d_data->filter->Modified();
		d_data->filter->Update();
	}
	d_data->image = VipVTKImage(d_data->filter->GetOutput());
	// display_lock.reset();

	el1 = QDateTime::currentMSecsSinceEpoch() - start;

	//
	// Draw the highlighted data (if any)
	//

	if (d_data->highlightedData && d_data->highlightedDataRender.rawData() && (d_data->state & ExtractHighlitedData)) {
		VipVTKObjectLocker data_locker = vipLockVTKObjects(d_data->highlightedData->rawData());

		d_data->highlightedRenderWin->SetSize(d_data->realRenderWin->GetSize());
		d_data->highlightedRenderWin->Modified();
		d_data->highlightedRender->GetActiveCamera()->DeepCopy(camera);
		d_data->highlightedRender->GetActiveCamera()->Modified();
		d_data->highlightedRender->Modified();

		// create the cell array if necessary
		if (d_data->highlightedDataRender.mapper())
			if (vtkDataSet* set = d_data->highlightedDataRender.rawData().dataSet()) {
				// save the active cell and point scalars for all data
				vtkDataArray* point_scalar = nullptr;
				vtkDataArray* cell_scalar = nullptr;

				d_data->highlightedDataRender.mapper()->Modified();

				// by default, we extract the cells.
				// if the object has no cells but only point, just extract the points
				vtkDataSetAttributes* cdata = nullptr;
				int count = set->GetNumberOfCells();
				if (count) {
					cell_scalar = set->GetCellData()->GetScalars();
					point_scalar = set->GetPointData()->GetScalars();
					if (cell_scalar)
						cell_scalar->Register(cell_scalar);
					if (point_scalar)
						point_scalar->Register(point_scalar);

					cdata = set->GetCellData();
					if (d_data->highlightedCellData->GetNumberOfTuples() != set->GetNumberOfCells())
						d_data->highlightedCellData->SetNumberOfTuples(set->GetNumberOfCells());
				}
				else {
					point_scalar = set->GetPointData()->GetScalars();
					if (point_scalar)
						point_scalar->Register(point_scalar);

					count = set->GetNumberOfPoints();
					cdata = set->GetPointData();
					if (d_data->highlightedCellData->GetNumberOfTuples() != set->GetNumberOfPoints())
						d_data->highlightedCellData->SetNumberOfTuples(set->GetNumberOfPoints());
				}

				// assign a different color for each cell
				for (int i = 0; i < count; ++i) {
					double color[3];
					toColorUChar((unsigned int)i + 1, color);
					d_data->highlightedCellData->SetTuple(i, color);
				}

				// draw the actor, hide all others
				d_data->highlightedDataRender.actor()->SetVisibility(d_data->highlightedData->actor()->GetVisibility());
				cdata->AddArray(d_data->highlightedCellData);
				cdata->SetScalars(d_data->highlightedCellData);
				d_data->highlightedDataRender.mapper()->SetScalarModeToUseCellData();
				d_data->highlightedFilter->Modified();
				d_data->highlightedFilter->Update();
				cdata->RemoveArray("extract");
				cdata->SetScalars(nullptr);
				// display_lock.reset();

				// reset previous cell and point scalars
				if (point_scalar) {
					set->GetPointData()->SetScalars(point_scalar);
					point_scalar->Delete();
				}
				if (cell_scalar) {
					set->GetCellData()->SetScalars(cell_scalar);
					cell_scalar->Delete();
				}

				el2 = QDateTime::currentMSecsSinceEpoch() - start;

				// update the modification time
				d_data->mTime = currentTime();

				// TEST
				if (vipVTKOpenGLCheckErrors())
					bool stop = true;

				if (d_data->highlightedFilter->GetOutput()) {

					/* static qint64 last_date = 0;
					qint64 date = d_data->highlightedFilter->GetOutput()->GetMTime();
					if (date != last_date) {
						last_date = date;
					}
					else
						bool stop = true;*/

					d_data->highlightedCells = VipVTKImage(d_data->highlightedFilter->GetOutput());

					// TEST debug
					/* {
						int width = d_data->highlightedCells.width(), height = d_data->highlightedCells.height();
						QRgb* ptr = (QRgb*)(d_data->highlightedCells.image()->GetScalarPointer());
						std::ofstream out("C:/Users/VM213788/Desktop/img.txt");
						for (int y = 0; y < height; ++y)
						{
							for (int x = 0; x < width; ++x)
								out << ptr[y * width + x] << "\t";
							out << std::endl;
						}
						out.close();
					}*/

					// d_data->highlightedCells.toQImage().save("C:/Users/VM213788/Desktop/cells.png");
					int width = d_data->highlightedCells.width(), height = d_data->highlightedCells.height();
					if (width * height > 0) {
						
						// convert back colors to cell id+1 and flip vertically
						QRgb* ptr = (QRgb*)(d_data->highlightedCells.image()->GetScalarPointer());

						//Detect corrution of the cell extractor
						QRgb* imp = (QRgb*)(d_data->image.image()->GetScalarPointer());
						auto it = d_data->data.find(d_data->highlightedData);
						unsigned idx = std::distance(d_data->data.begin(), it);
						int diff_count = 0;
						int pix_count = 0;
						for (int i = 0; i < width * height; ++i) {
							unsigned value = toValue(imp[i]);
							if (value == idx + 1)
								pix_count++;
							diff_count += (bool)(value == idx + 1) != (bool)ptr[i];
						}
						if (pix_count && (double)diff_count / pix_count > 0.3)
						{
							// Corrupted!
							qint64 time = QDateTime::currentMSecsSinceEpoch();
							if (time - d_data->lastReset > 1000 ) {
								d_data->lastReset = time;
								Reset();
								vip_debug("Force update!\n");
								return ForceUpdate();
							}
						}


						for (int y = 0; y < height / 2; ++y) {
							for (int x = 0; x < width; ++x) {
								QRgb tmp = ptr[y * width + x];
								ptr[y * width + x] = toValue(ptr[(height - y - 1) * width + x]);
								ptr[(height - y - 1) * width + x] = toValue(tmp);
							}
						}

					}
				}

				el3 = QDateTime::currentMSecsSinceEpoch() - start;
			}
	}

	// convert RGBA image into actor's id and extract the bounding rect

	QRgb* ptr = (QRgb*)d_data->image.image()->GetScalarPointer();
	int width = d_data->image.width(), height = d_data->image.height();
	int left = INT_MAX, right = -1, top = INT_MAX, bottom = -1;
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) {
			const int index = y * width + x;
			if (ptr[index] != 0) {
				ptr[index] = toValue(ptr[index]);
				left = qMin(left, x);
				right = qMax(right, x);
				top = qMin(top, y);
				bottom = qMax(bottom, y);
			}
		}
	QRect bounding(left, top, right - left + 1, bottom - top + 1);

	el4 = QDateTime::currentMSecsSinceEpoch() - start;

	// ContourLines lines;
	QMap<int, QList<QPolygonF>> lines_list; // TEST
	if (d_data->state & ExtractOutlines) {

		// lines = VipPlotSpectrogram::contourLines(VipNDArray::makeView(ptr, vipVector(height, width)), bounding, levels, true);
		lines_list = d_data->extractor.extract(VipNDArrayTypeView<unsigned>(ptr, vipVector(height, width)), bounding, 0); // TEST
	}

	el5 = QDateTime::currentMSecsSinceEpoch() - start;

	QVector<QVector<QRect>> regions((int)d_data->data.size() + 1);

	if (d_data->state & ExtractShape) {
		QRect pending_rect;
		int pending_value = 0;
		for (int y = bounding.top(); y <= bounding.bottom(); ++y) {
			for (int x = bounding.left(); x <= bounding.right(); ++x) {
				const int val = ptr[y * width + x];

				if (val == 0) {
					if (pending_value > 0) {
						if (pending_value < regions.size())
							regions[pending_value].append(pending_rect);
						pending_rect = QRect();
						pending_value = 0;
					}
				}
				else {
					if (val != pending_value) {
						if (pending_value > 0 && pending_value < regions.size())
							regions[pending_value].append(pending_rect);

						pending_rect = QRect(x, height - y - 1, 1, 1);
						pending_value = val;
					}
					else {
						pending_rect.setRight(x);
					}
				}
			}

			// end of row: add pending rect
			if (pending_value > 0) {
				if (pending_value < regions.size())
					regions[pending_value].append(pending_rect);
				pending_rect = QRect();
				pending_value = 0;
			}
		}
	}

	el6 = QDateTime::currentMSecsSinceEpoch() - start;

	// setup shapes
	// set the final contours
	d_data->shapes.clear();
	d_data->regions.clear();
	d_data->outlines.clear();
	d_data->outlines_list.clear();
	for (auto it = data_levels.begin(); it != data_levels.end(); ++it) {
		QRegion reg;
		reg.setRects(regions[it.key()].data(), regions[it.key()].size());
		QPainterPath p;
		p.addRegion(reg);

		d_data->shapes[it.value()] = p;
		d_data->regions[it.value()] = reg;

		// mirror vertical for outlines
		// QPolygonF poly = ( lines[it.key()]);
		// for (int i = 0; i < poly.size(); ++i)
		//	poly[i].setY(height - poly[i].y() - 1);
		// d_data->outlines[it.value()] = poly;

		d_data->outlines_list[it.value()] = lines_list[it.key()];
	}

	el7 = QDateTime::currentMSecsSinceEpoch() - start;

	vip_debug("Extract controur: %i %i %i %i %i %i %i ms\n", (int)el1, (int)el2, (int)el3, (int)el4, (int)el5, (int)el6, (int)el7);
	// qint64 cam_time2 = d_data->renderers[0]->GetActiveCamera()->GetMTime();
	// qint64 cam_time1 = cam_time;
}
