#include "VipVTKRegion.h"
#include "VipHash.h"
#include "vtkCamera.h"
#include "vtkCellArray.h"
#include "vtkDataSet.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkMatrix4x4.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkPolyData.h"
#include "vtkCellData.h"
#include "vtkRenderWindow.h"
#include "vtkRenderer.h"
#include "vtkSelectionNode.h"
#include "vtkWindowToImageFilter.h"
#include "vtkUnstructuredGrid.h"
#include "vtkLine.h"

class SelectVisiblePoints
{
public:
	vtkRenderer* Renderer;
	vtkMatrix4x4* CompositePerspectiveTransform;

	vtkTypeBool SelectionWindow;
	int Selection[4];
	int InternalSelection[4];
	vtkTypeBool SelectInvisible;
	double DirectionOfProjection[3];
	double Tolerance;
	double ToleranceWorld;
	VipSpinlock lock;
	QPainterPath path;
	int* WindowSize = nullptr;
	/**
	 * Requires the renderer to be set. Populates the composite perspective transform
	 * and returns a pointer to the Z-buffer (that must be deleted) if getZbuff is set.
	 */
	float* Initialize(bool getZbuff);

	/**
	 * Tests if a point x is being occluded or not against the Z-Buffer array passed in by
	 * zPtr. Call Initialize before calling this method.
	 */
	bool IsPointOccluded(const double x[3], const float* zPtr);

	VipVTKRegionObject Execute(VipVTKGraphicsView* view, const VipVTKObject&, const VipPlotShape*);

	SelectVisiblePoints();
	~SelectVisiblePoints();

private:
	SelectVisiblePoints(const SelectVisiblePoints&) = delete;
	void operator=(const SelectVisiblePoints&) = delete;
};

// Instantiate object with no renderer; window selection turned off;
// tolerance set to 0.01; and select invisible off.
SelectVisiblePoints::SelectVisiblePoints()
{
	this->Renderer = nullptr;
	this->SelectionWindow = 0;
	this->Selection[0] = this->Selection[2] = 0;
	this->Selection[1] = this->Selection[3] = 1600;
	this->InternalSelection[0] = this->InternalSelection[2] = 0;
	this->InternalSelection[1] = this->InternalSelection[3] = 1600;
	this->CompositePerspectiveTransform = vtkMatrix4x4::New();
	this->DirectionOfProjection[0] = this->DirectionOfProjection[1] = this->DirectionOfProjection[2] = 0.0;
	this->Tolerance = 0.01;
	this->ToleranceWorld = 0.0;
	this->SelectInvisible = 0;
}

SelectVisiblePoints::~SelectVisiblePoints()
{
	this->Renderer = (nullptr);
	this->CompositePerspectiveTransform->Delete();
}

static vtkIdList* LocalIdList()
{
	thread_local vtkNew<vtkIdList> ids;
	return ids;
}

VipVTKRegionObject SelectVisiblePoints::Execute(VipVTKGraphicsView* view, const VipVTKObject& data, const VipPlotShape* shape)
{
	VipVTKRegionObject res;
	auto data_lock = vipLockVTKObjects(data);
	vtkDataSet* input = data.dataSet();

	if (!input)
		return res;

	vtkIdType ptId;
	int visible;
	vtkPointData* inPD = input->GetPointData();
	vtkIdType numPts = input->GetNumberOfPoints();

	// Nothing to extract if there are no points in the data set.
	if (numPts < 1) {
		return res;
	}

	if (this->Renderer == nullptr) {
		return res;
	}

	if (!this->Renderer->GetRenderWindow()) {
		return res;
	}

	// This will trigger if you do something like ResetCamera before the Renderer or
	// RenderWindow have allocated their appropriate system resources (like creating
	// an OpenGL context)." Resource allocation must occur before we can use the Z
	// buffer.
	if (this->Renderer->GetRenderWindow()->GetNeverRendered()) {
		return res;
	}

	vtkCamera* cam = this->Renderer->GetActiveCamera();
	if (!cam) {
		return res;
	}

	VipShape sh = shape->rawData();

	// Check that we have a closed shape
	if (sh.type() != VipShape::Polygon && sh.type() != VipShape::Path)
		return res;

	path = sh.shape();
	path = shape->sceneMap()->transform(path);
	path = shape->mapToScene(path);
	path = view->mapFromScene(path);
	QRect area = path.boundingRect().toRect();

	WindowSize = view->renderWindow()->GetSize();
	int window[4] = { area.left(), area.right(), WindowSize[1] - area.bottom() - 1, WindowSize[1] - area.top() - 1 };
	memcpy(Selection, window, sizeof(window));

	// Mandatory to extract ROIs
	Renderer->SetPreserveDepthBuffer(true);
	Renderer->Render();

	/* {
		Renderer = vtkRenderer::New();
		vtkNew<vtkRenderWindow> win;
		win->SetOffScreenRendering(1);
		win->SetSize(view->renderWindow()->GetSize());
		win->AddRenderer(Renderer);

		auto lst = view->objects();
		for (const auto* plot : lst) {
			if (plot->layer() == 0)
				Renderer->AddActor(plot->actor());
		}
		vtkNew<vtkCamera> cam;
		cam->DeepCopy(view->renderer()->GetActiveCamera());
		Renderer->SetActiveCamera(cam);
	}*/
	

	float* zPtr = this->Initialize(true);

	vtkIdType progressInterval = numPts / 20 + 1;

	vtkNew<vtkIdList> cellIds;
	QSet<vtkIdType> cells;
	QSet<vtkIdType> points;
	input->GetPointCells(0, cellIds);

//#pragma omp parallel for
	for (ptId = 0; ptId < numPts; ptId++) {
		// perform conversion
		double x[4];
		x[3] = 1.0;
		input->GetPoint(ptId, x);

		visible = IsPointOccluded(x, zPtr);

		if ((visible && !this->SelectInvisible) || (!visible && this->SelectInvisible)) {

			vtkIdList* ids = LocalIdList();
			input->GetPointCells(ptId, ids);

			std::lock_guard<VipSpinlock> ll(lock);
			for (vtkIdType j = 0; j < ids->GetNumberOfIds(); ++j)
				cells.insert(ids->GetId(j));
			res.pointIds.push_back(ptId);
			points.insert(ptId);
		}
	} // for all points

	// Mandatory to extract ROIs
	Renderer->SetPreserveDepthBuffer(false);
	Renderer->Render();

	// Compute all connected cells
	res.cellIds.resize(cells.size());
	ptId = 0;
	for (auto it = cells.begin(); it != cells.end(); ++it, ++ptId)
		res.cellIds[ptId] = *it;

	// Compute fully enclosed cells
	for (auto it = cells.begin(); it != cells.end();) {
		vtkIdList* ids = LocalIdList();
		input->GetCellPoints(*it, ids);

		bool missing_points = false;
		for (vtkIdType i = 0; i < ids->GetNumberOfIds(); ++i) {
			auto ptid = ids->GetId(i);
			if (!points.contains(ptid)) {
				missing_points = true;
				break;
			}
		}
		if (missing_points) {
			it = cells.erase(it);
		}
		else
			++it;
	}

	res.completeCellIds.resize(cells.size());
	ptId = 0;
	for (auto it = cells.begin(); it != cells.end(); ++it, ++ptId)
		res.completeCellIds[ptId] = *it;

	delete[] zPtr;

	return res;
}

float* SelectVisiblePoints::Initialize(bool getZbuff)
{
	vtkCamera* cam = this->Renderer->GetActiveCamera();
	if (!cam) {
		return nullptr;
	}
	cam->GetDirectionOfProjection(this->DirectionOfProjection);

	const int* size = this->Renderer->GetRenderWindow()->GetSize();

	// specify a selection window to avoid querying
	if (this->SelectionWindow) {
		for (int i = 0; i < 4; i++) {
			this->InternalSelection[i] = this->Selection[i];
		}
	}
	else {
		this->InternalSelection[0] = this->InternalSelection[2] = 0;
		this->InternalSelection[1] = size[0] - 1;
		this->InternalSelection[3] = size[1] - 1;
	}

	// Grab the composite perspective transform.  This matrix is used to convert
	// each point to view coordinates.  vtkRenderer provides a WorldToView()
	// method but it computes the composite perspective transform each time
	// WorldToView() is called.  This is expensive, so we get the matrix once
	// and handle the transformation ourselves.
	this->CompositePerspectiveTransform->DeepCopy(this->Renderer->GetActiveCamera()->GetCompositeProjectionTransformMatrix(this->Renderer->GetTiledAspectRatio(), 0, 1));

	// If we have more than a few query points, we grab the z-buffer for the
	// selection region all at once and probe the resulting array.  When we
	// have just a few points, we perform individual z-buffer queries.
	if (getZbuff) {
		return this->Renderer->GetRenderWindow()->GetZbufferData(this->InternalSelection[0], this->InternalSelection[2], this->InternalSelection[1], this->InternalSelection[3]);
	}
	return nullptr;
}

bool SelectVisiblePoints::IsPointOccluded(const double x[3], const float* zPtr)
{
	double view[4];
	double dx[3], z;
	double xx[4] = { x[0], x[1], x[2], 1.0 };
	if (this->ToleranceWorld > 0.0) {
		xx[0] -= this->DirectionOfProjection[0] * this->ToleranceWorld;
		xx[1] -= this->DirectionOfProjection[1] * this->ToleranceWorld;
		xx[2] -= this->DirectionOfProjection[2] * this->ToleranceWorld;
	}

	this->CompositePerspectiveTransform->MultiplyPoint(xx, view);
	if (view[3] == 0.0) {
		return false;
	}

	{
		/* {
			std::lock_guard<VipSpinlock> ll(lock);
			this->Renderer->SetViewPoint(view[0] / view[3], view[1] / view[3], view[2] / view[3]);
			this->Renderer->ViewToDisplay();
			this->Renderer->GetDisplayPoint(dx);
		}*/
		double _dx[3];
		dx[0] = view[0] / view[3];
		dx[1] = view[1] / view[3];
		dx[2] = view[2] / view[3];
		this->Renderer->ViewToDisplay(dx[0], dx[1], dx[2]);
	}
	// check whether visible and in selection window
	if (dx[0] >= this->InternalSelection[0] && dx[0] <= this->InternalSelection[1] && dx[1] >= this->InternalSelection[2] && dx[1] <= this->InternalSelection[3]) {
		if (zPtr != nullptr) {
			// Access the value from the captured zbuffer.  Note, we only
			// captured a portion of the zbuffer, so we need to offset dx by
			// the selection window.
			z = zPtr[static_cast<int>(dx[0]) - this->InternalSelection[0] +
				 (static_cast<int>(dx[1]) - this->InternalSelection[2]) * (this->InternalSelection[1] - this->InternalSelection[0] + 1)];
		}
		else {
			z = this->Renderer->GetZ(static_cast<int>(dx[0]), static_cast<int>(dx[1]));
		}

		if (dx[2] < (z + this->Tolerance)) {

			// Check if point is inside path
			dx[1] = WindowSize[1] - dx[1] - 1;
			return path.contains(QPointF(dx[0], dx[1]));
		}
	}

	return false;
}

VipVTKRegion vipBuildRegion(const VipPlotShape* shape, VipVTKGraphicsView* view, int renderer_level)
{
	VipVTKRegion res;
	if (renderer_level < 0 || renderer_level >= 10)
		return res;

	SelectVisiblePoints select;
	select.Renderer = view->renderers()[renderer_level];
	select.SelectionWindow = true;

	const auto plots = view->objects();
	for (const auto& plot : plots) {
		if (plot->layer() == renderer_level) {
			const VipVTKObject object = plot->rawData();
			if (object.property("VipVTKRegion").value<VipVTKRegion>().isEmpty()) {
				// Exclude ROI objects
				auto r = select.Execute(view, object, shape);
				r.source = plot;
				res.regions.push_back(r);
			}
		}
	}

	res.name = shape->shapeName();
	res.renderer = select.Renderer;
	return res;
}

struct Point3D
{
	double p[3];
	bool operator==(const Point3D& o) const noexcept { return p[0] == o.p[0] && p[1] == o.p[1] && p[2] == o.p[2]; }
};
size_t qHash(const Point3D& p)
{
	return vipHashBytes(&p, sizeof(p));
}

VipVTKObject vipBuildRegionObject(const VipVTKRegion& r)
{
	if (!r.renderer || !r.renderer->GetActiveCamera())
		return VipVTKObject();

	vtkSmartPointer<vtkUnstructuredGrid> grid = vtkSmartPointer<vtkUnstructuredGrid>::New();

	vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
	grid->SetPoints(pts);

	for (const VipVTKRegionObject& o : r.regions) {

		if (!o.source)
			continue;

		VipVTKObject data = o.source->rawData();
		auto lock = vipLockVTKObjects(data);

		vtkDataSet* set = data.dataSet();
		if (!set)
			continue;

		auto* set_points = set->GetPoints();
		QHash<Point3D, vtkIdType> pts_to_id;

		// Add the points and shift them toard the camera
		vtkCamera* cam = r.renderer->GetActiveCamera();
		double bnd[6];
		r.renderer->ComputeVisiblePropBounds(bnd);
		double dist = sqrt((bnd[1] - bnd[0]) * (bnd[1] - bnd[0]) + (bnd[3] - bnd[2]) * (bnd[3] - bnd[2]) + (bnd[5] - bnd[4]) * (bnd[5] - bnd[4]));
		double inflate = -dist / 1000.0;

		vtkNew<vtkLine> line;

		for (qsizetype i = 0; i < o.pointIds.size(); ++i) {
			Point3D p;
			set_points->GetPoint(o.pointIds[i], p.p);

			line->GetPoints()->SetPoint(0, cam->GetPosition());
			line->GetPoints()->SetPoint(1, p.p);
			line->Inflate(inflate);

			auto id = pts->InsertNextPoint(line->GetPoints()->GetPoint(1));
			pts_to_id.insert(p, id);
			grid->InsertNextCell(VTK_VERTEX, 1, &id);
		}

		// Add fully enclosed cells only
		vtkNew<vtkIdList> ids;
		std::vector<vtkIdType> pts_ids;
		for (qsizetype i = 0; i < o.completeCellIds.size(); ++i) {
			auto id = o.completeCellIds[i];
			set->GetCellPoints(id, ids);
			int type = set->GetCellType(id);
			pts_ids.clear();
			for (qsizetype j = 0; j < ids->GetNumberOfIds(); ++j) {
				Point3D p;
				set_points->GetPoint(ids->GetId(j), p.p);
				auto it = pts_to_id.find(p);
				if (it != pts_to_id.end()) {
					pts_ids.push_back(it.value());
				}
				else {
					pts_ids.clear();
					break;
				}
			}
			if (pts_ids.size())
				grid->InsertNextCell(type, pts_ids.size(), pts_ids.data());
		}
	}

	VipVTKObject res(grid);
	res.setProperty("VipVTKRegion", QVariant::fromValue(r));
	return res;
}

static VipVTKObject getSourceObject(const VipPlotVTKObject* plot)
{
	if (VipDisplayObject* d = plot->property("VipDisplayObject").value<VipDisplayObject*>()) {
		return d->inputAt(0)->source()->data().value<VipVTKObject>();
	}
	return plot->rawData();
}

QVector<QPair<QString, QVariant>> vipBuildRegionAttributes(const VipVTKRegion& r, const QVector<VipVTKObject>& objects)
{
	struct Region
	{
		VipVTKRegionObject region;
		VipVTKObject object;
	};

	QVector<QPair<QString, QVariant>> map;

	if (r.isEmpty())
		return map;

	if (objects.size() && r.regions.size() != objects.size())
		return map;

	map.push_back({ "Name", r.name });
	map.push_back({ "Models count", QString::number(r.regions.size()) });

	double xmin = std::numeric_limits<double>::infinity();
	double xmax = -std::numeric_limits<double>::infinity();
	double ymin = std::numeric_limits<double>::infinity();
	double ymax = -std::numeric_limits<double>::infinity();
	double zmin = std::numeric_limits<double>::infinity();
	double zmax = -std::numeric_limits<double>::infinity();

	// Compute bounds
	for (qsizetype i = 0; i < r.regions.size(); ++i) {
		const auto& region = r.regions[i];
		if (region.source) {
			VipVTKObject obj = objects.size() ? objects[i] : region.source->rawData();
			auto lock = vipLockVTKObjects(obj);

			if (vtkDataSet* set = obj.dataSet()) {
				vtkPoints* pts = set->GetPoints();
				for (vtkIdType id : region.pointIds) {
					double p[3];
					pts->GetPoint(id, p);
					xmin = std::min(xmin, p[0]);
					xmax = std::max(xmax, p[0]);
					ymin = std::min(ymin, p[1]);
					ymax = std::max(ymax, p[1]);
					zmin = std::min(zmin, p[2]);
					zmax = std::max(zmax, p[2]);
				}
			}
		}
	}

	map.push_back({ "X min", xmin });
	map.push_back({ "X max", xmax });
	map.push_back({ "Y min", ymin });
	map.push_back({ "Y max", ymax });
	map.push_back({ "Z min", zmin });
	map.push_back({ "Z max", zmax });

	// Compute point/cell count
	qint64 points = 0, cells = 0, full_cells = 0;
	QMap<QString, QVector<Region>> points_attrs;
	QMap<QString, QVector<Region>> cells_attrs;
	for (qsizetype i = 0; i < r.regions.size(); ++i) {
		const auto& region = r.regions[i];
		if (region.source) {

			points += region.pointIds.size();
			cells += region.cellIds.size();
			full_cells += region.completeCellIds.size();

			VipVTKObject obj = objects.size() ? objects[i] : region.source->rawData();
			auto lock = vipLockVTKObjects(obj);

			const auto pnames = obj.pointsAttributesName();
			for (const auto& n : pnames)
				points_attrs[n].push_back(Region{ region, obj });

			const auto cnames = obj.cellsAttributesName();
			for (const auto& n : cnames)
				cells_attrs[n].push_back(Region{ region, obj });
		}
	}
	map.push_back({ "Points", (points) });
	map.push_back({ "Cells (connected)", (cells) });
	map.push_back({ "Cells (full)", (full_cells) });

	// Compute points attributes
	for (auto it = points_attrs.begin(); it != points_attrs.end(); ++it) {
		QString name = /*"Points attributes/" +*/ it.key();
		const auto& regions = it.value();
		double min = vipNan(), max = vipNan(), mean = vipNan();
		qint64 count = 0;

		for (const auto& region : regions) {
			VipVTKObject obj = region.object;
			auto lock = vipLockVTKObjects(obj);

			if (auto* array = obj.pointsAttribute(it.key())) {
				if (array->IsA("vtkDataArray") && array->GetNumberOfComponents() == 1) {
					vtkDataArray* ar = static_cast<vtkDataArray*>(array);
					for (auto id : region.region.pointIds) {
						if (vipIsNan(min))
							min = max = mean = ar->GetTuple1(id);
						else {
							double v = ar->GetTuple1(id);
							min = std::min(min, v);
							max = std::max(max, v);
							mean += v;
						}
						++count;
					}
				}
			}
		}

		mean /= count;
		map.push_back({ name + " min", (min) });
		map.push_back({ name + " max", (max) });
		map.push_back({ name + " mean", (mean) });
	}

	// Compute cells attributes
	for (auto it = cells_attrs.begin(); it != cells_attrs.end(); ++it) {
		QString name = /*"Cells attributes/" +*/ it.key();
		const auto& regions = it.value();
		double min = vipNan(), max = vipNan(), mean = vipNan();
		qint64 count = 0;

		for (const auto& region : regions) {
			VipVTKObject obj = region.object;
			auto lock = vipLockVTKObjects(obj);

			if (auto* array = obj.cellsAttribute(it.key())) {
				if (array->IsA("vtkDataArray") && array->GetNumberOfComponents() == 1) {
					vtkDataArray* ar = static_cast<vtkDataArray*>(array);
					for (auto id : region.region.completeCellIds) {
						if (vipIsNan(min))
							min = max = mean = ar->GetTuple1(id);
						else {
							double v = ar->GetTuple1(id);
							min = std::min(min, v);
							max = std::max(max, v);
							mean += v;
						}
						++count;
					}
				}
			}
		}

		mean /= count;
		map.push_back({ name + " min", (min) });
		map.push_back({ name + " max", (min) });
		map.push_back({ name + " mean", (mean) });
	}

	return map;
}

class VipVTKRegionProcessing::PrivateData
{
public:
	VipVTKRegion region;
	VipVTKObject output;
	size_t hashs = 0; // hash value for all objects points and cells
};

static size_t computeHash(const VipVTKRegion& region, const QVector<VipVTKObject>& objects = QVector<VipVTKObject>())
{
	size_t h = 0;

	if (objects.size() && objects.size() != region.regions.size())
		return h;

	for (qsizetype i = 0; i < region.regions.size(); ++i) {
		const auto& r = region.regions[i];
		if (r.source) {
			VipVTKObject obj = objects.size() ? objects[i] : r.source->rawData();
			auto lock = vipLockVTKObjects(obj);

			if (vtkDataSet* set = obj.dataSet()) {
				// Hash points
				auto* pts = set->GetPoints();
				if (pts->GetNumberOfPoints()) {
					size_t tmp = vipHashBytes(pts->GetData()->GetVoidPointer(0), pts->GetNumberOfPoints() * sizeof(double));
					vipHashCombine(h, tmp);
				}

				// Hash cells count
				vtkIdType cell_count = set->GetNumberOfCells();
				vipHashCombine(h, vipHashBytes(&cell_count, sizeof(cell_count)));
			}
		}
	}
	return h;
}

static QVariantMap computeAttributes(const QVector<QPair<QString, QVariant>>& attrs)
{
	QVariantMap map;
	for (const auto& pair : attrs)
		map.insert(pair.first, pair.second);
	return map;
}

VipVTKRegionProcessing::VipVTKRegionProcessing(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	outputAt(0)->setData(VipVTKObject());
}
VipVTKRegionProcessing::~VipVTKRegionProcessing()
{
	d_data->hashs = 0; // just for debug
}

void VipVTKRegionProcessing::setRegion(const VipVTKRegion& region)
{
	// Disconnect previous inputs
	for (int i = 0; i < inputCount(); ++i)
		inputAt(i)->clearConnection();

	// Disconnect previous region
	for (const auto& r : d_data->region.regions) {
		if (r.source)
			disconnect(r.source.get(), SIGNAL(destroy()), this, SLOT(recompute()));
	}

	d_data->region = region;

	// Remove null sources
	for (qsizetype i = 0; i < d_data->region.regions.size(); ++i) {
		auto& r = d_data->region.regions[i];
		if (!r.source) {
			d_data->region.regions.removeAt(i);
			--i;
		}
	}

	d_data->hashs = computeHash(d_data->region);

	// Connect region, make sure the destruction of a source destroys this region.
	int input_count = 0;
	for (const auto& r : d_data->region.regions) {
		if (r.source) {
			connect(r.source.get(), SIGNAL(destroy()), this, SLOT(recompute()), Qt::QueuedConnection);
			if (r.source->property("VipDisplayObject").value<VipDisplayObject*>())
				input_count++;
		}
	}

	topLevelInputAt(0)->toMultiInput()->resize(input_count);

	// Connect inputs
	input_count = 0;
	for (const auto& r : d_data->region.regions) {
		if (r.source)
			if (VipDisplayObject* display = r.source->property("VipDisplayObject").value<VipDisplayObject*>()) {
				inputAt(input_count++)->setConnection(display->inputAt(0)->source());
			}
	}

	// Prepare output
	d_data->output = vipBuildRegionObject(d_data->region);
	d_data->output.setDataName(d_data->region.name);

	VipAnyData any;
	any.setData(QVariant::fromValue(d_data->output));
	any.setSource(this);
	any.setAttributes(computeAttributes(vipBuildRegionAttributes(d_data->region)));
	outputAt(0)->setData(any);
}

void VipVTKRegionProcessing::recompute()
{
	for (qsizetype i = 0; i < d_data->region.regions.size(); ++i) {
		auto& r = d_data->region.regions[i];
		if (!r.source) {
			d_data->region.regions.removeAt(i);
			--i;
		}
	}

	if (d_data->region.regions.isEmpty()) {
		if (outputAt(0)->connection()) {
			auto sinks = outputAt(0)->connection()->sinks();
			if (sinks.size() == 1) {
				VipDisplayPlotItem* item = qobject_cast<VipDisplayPlotItem*>(sinks.first()->parentProcessing());
				if (item) {
					item->item()->deleteLater();
					item->deleteLater();
				}
				return;
			}
		}
		deleteLater();
		return;
	}
	setRegion(d_data->region);
}

VipVTKRegion VipVTKRegionProcessing::region()
{
	return d_data->region;
}

void VipVTKRegionProcessing::apply()
{
	VipVTKObjectList objects;

	// Consume inputs
	for (int i = 0; i < inputCount(); ++i) {
		objects.push_back(inputAt(i)->data().value<VipVTKObject>());
	}

	// Check hash value
	size_t hash = computeHash(d_data->region, objects);
	if (hash != d_data->hashs) {
		VipAnyData any;
		any.setSource(this);
		outputAt(0)->setData(any);
		return;
	}

	VipAnyData any;
	d_data->output.setProperty("VipVTKObjectList", QVariant::fromValue(objects));

	any.setData(QVariant::fromValue(d_data->output));
	any.setSource(this);
	any.setAttributes(computeAttributes(vipBuildRegionAttributes(d_data->region, objects)));
	outputAt(0)->setData(any);
}

class VipVTKExtractHistogram::PrivateData
{
public:
	VipExtractHistogram hist;
	QString arrayName;
	int component = 0;
	VipVTKObject::AttributeType type = VipVTKObject::Unknown;
};

VipVTKExtractHistogram::VipVTKExtractHistogram(QObject* parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	outputAt(0)->setData(VipIntervalSampleVector());
	propertyAt(0)->setData(100);
	d_data->hist.topLevelOutputAt(0)->toMultiOutput()->resize(1);
}
VipVTKExtractHistogram::~VipVTKExtractHistogram() {}

void VipVTKExtractHistogram::setArrayName(const QString& name)
{
	d_data->arrayName = name;
}
QString VipVTKExtractHistogram::arrayName() const
{
	return d_data->arrayName;
}

void VipVTKExtractHistogram::setComponent(int comp)
{
	d_data->component = comp;
}
int VipVTKExtractHistogram::component() const
{
	return d_data->component;
}

void VipVTKExtractHistogram::setAttributeType(VipVTKObject::AttributeType type)
{
	d_data->type = type;
}
VipVTKObject::AttributeType VipVTKExtractHistogram::attributeType() const
{
	return d_data->type;
}

static void addValues(std::vector<double>& values, const VipVTKObject& obj, VipVTKObject::AttributeType type, const QString& name, int comp, const vtkIdType* ids = nullptr, qsizetype count = 0)
{
	if (auto* array = obj.attribute(type, name)) {
		if (!array->IsA("vtkDataArray"))
			return;
		vtkDataArray* ar = static_cast<vtkDataArray*>(array);
		if (!ar->IsNumeric())
			return;
		if (comp >= ar->GetNumberOfComponents())
			return;

		std::vector<double> tuple(ar->GetNumberOfComponents());
		if (ids) {
			auto tuples = ar->GetNumberOfTuples();
			values.reserve(values.capacity() + (size_t)count);
			for (qsizetype i = 0; i < count; ++i) {
				auto id = ids[i];
				if (id < tuples) {
					ar->GetTuple(id, tuple.data());
					values.push_back(tuple[comp]);
				}
			}
		}
		else {
			auto tuples = ar->GetNumberOfTuples();
			values.reserve(values.capacity() + (size_t)tuples);
			for (qsizetype i = 0; i < tuples; ++i) {
				ar->GetTuple(i, tuple.data());
				values.push_back(tuple[comp]);
			}
		}
	}
}

void VipVTKExtractHistogram::apply()
{
	if (d_data->component < 0 || d_data->arrayName.isEmpty() || d_data->type == VipVTKObject::Field || d_data->type == VipVTKObject::Unknown)
		return;

	VipVTKObjectList lst;

	for (int i = 0; i < inputCount(); ++i) {
		VipInput* input = inputAt(i);
		VipVTKObject obj = input->data().value<VipVTKObject>();
		if (!obj)
			continue;

		lst.push_back(obj);
	}

	std::vector<double> data;

	QString name = "Aggregate";
	QStringList names;

	for (const VipVTKObject& obj : lst) {

		auto lock = vipLockVTKObjects(obj);

		const VipVTKRegion region = obj.property("VipVTKRegion").value<VipVTKRegion>();
		if (!region.isEmpty()) {
			const VipVTKObjectList objects = obj.property("VipVTKObjectList").value<VipVTKObjectList>();
			names.push_back(region.name);
			
			for (qsizetype i = 0; i < region.regions.size(); ++i) 
			{
				const VipVTKRegionObject& r = region.regions[i];
				if (!r.source)
					continue;

				VipVTKObject raw;
				if (objects.size() == region.regions.size())
					raw = objects[i];
				else
					raw = r.source->rawData();

				auto ll = vipLockVTKObjects(raw);

				const QVector<vtkIdType>* ids = nullptr;
				if (d_data->type == VipVTKObject::Cell)
					ids = &r.completeCellIds;
				else
					ids = &r.pointIds;
				addValues(data,raw, d_data->type, d_data->arrayName, d_data->component, ids->data(), ids->size());
			}
		}
		else {
			names.push_back(QFileInfo(obj.dataName()).fileName());
			addValues(data, obj, d_data->type, d_data->arrayName, d_data->component);
		}
	}

	if (names.size() == 1)
		name = QFileInfo(names.first()).fileName();

	name += " " + d_data->arrayName + "[" + QString::number(d_data->component) + "] histogram";

	VipNDArrayTypeView<double> view(data.data(), vipVector(1, (qsizetype)data.size()));
	VipShape shape(QRectF(0, 0, data.size(), 1));

	d_data->hist.setShape(shape);
	d_data->hist.propertyName("bins")->setData(propertyAt(0)->data());
	d_data->hist.inputAt(0)->setData(QVariant::fromValue(VipNDArray(view)));
	d_data->hist.update();

	VipIntervalSampleVector vector = d_data->hist.outputAt(0)->data().value<VipIntervalSampleVector>();

	VipAnyData any = create(QVariant::fromValue(vector));
	any.setName(name);
	any.setXUnit(d_data->arrayName);
	outputAt(0)->setData(any);
}