#include <QBoxLayout>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QStyleOption>
#include <QTimer>
#include <QToolTip>
#include <QtGlobal>
#include <qapplication.h>
#include <qthreadpool.h>
#include <qgroupbox.h>
#include <qgridlayout.h>
#include <qboxlayout.h>
#include <limits>

#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkContourFilter.h>
#include <vtkCubeAxesActor.h>
#include <vtkDataSet.h>
#include <vtkDoubleArray.h>
#include <vtkInteractorObserver.h>
#include <vtkInteractorStyle.h>
#include <vtkPointData.h>
#include <vtkPointPicker.h>
#include <vtkPropPicker.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTextProperty.h>
#include <vtkTransform.h>
#include <vtkWindowToImageFilter.h>
#include <vtkNamedColors.h>
#include <vtkAxisActor.h>

#include "VipDisplayVTKObject.h"
//#include "OffscreenExtractShapeStatistics.h"
#include "VipVTKGraphicsView.h"
#include "VipVTKImage.h"
#include "p_QVTKBridge.h"


#include "VipLegendItem.h"
#include "VipPlotGrid.h"
#include "VipLegendItem.h"
#include "VipSliderGrip.h"


VipInfoWidget::VipInfoWidget(VipVTKGraphicsView* view)
  : QLabel(view)
  , view(view)
{
	//setStyleSheet("background:rgba(255,255,255,130);color:black;");
	setStyleSheet("background: transparent;");
	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	this->setMargin(5);
	connect(view, SIGNAL(mouseMove(const QPoint&)), this, SLOT(updateDisplayInfo(const QPoint&)), Qt::QueuedConnection);

	view->installEventFilter(this);
	this->setMaximumWidth(view->width());
	this->setMinimumWidth(view->width());
	this->setWordWrap(true);
	this->move(0, 20);
}

/* void VipInfoWidget::AddStartingText(const QString& str)
{
	beginAddText.append(str);
}
void VipInfoWidget::AddEndingText(const QString & str)
{
	endAddText.append(str);
}

void VipInfoWidget::ClearAdditionalText()
{
	beginAddText.clear();
	endAddText.clear();
}*/

bool VipInfoWidget::eventFilter(QObject* w, QEvent* evt)
{
	if (evt->type() == QEvent::Resize) {
		this->setMaximumWidth(view->width());
		this->setMinimumWidth(view->width());
	}
	return false;
}

void VipInfoWidget::updateDisplayInfo(const QPoint& pt)
{
	QPoint pos = pt;
	if (pos == QPoint(-1, -1))
		pos = last;

	QString str;
	if (view->trackingEnabled()) {
		if (pos != QPoint(-1, -1)) {
			if (pos != last) {
				str = view->contours()->Description(pos);
				lastDescription = str;
			}
			else
				str = lastDescription;
		}
	}

	// for (int i = 0; i < beginAddText.size(); ++i)
	//	str = beginAddText[i] + str;
	// for (int i = 0; i < endAddText.size(); ++i)
	//	str += endAddText[i];

	setText(str);
	setVisible(!str.isEmpty());
	last = pos;
	
	//resize(VipText(text()).textSize().toSize() + QSize(10, 10));
}

void VipInfoWidget::wheelEvent(QWheelEvent* event)
{
	view->wheelEvent(event);
}

void VipInfoWidget::mousePressEvent(QMouseEvent* event)
{
	view->mousePressEvent(event);
}
void VipInfoWidget::mouseMoveEvent(QMouseEvent* event)
{
	view->mouseMoveEvent(event);
}
void VipInfoWidget::mouseReleaseEvent(QMouseEvent* event)
{
	view->mouseReleaseEvent(event);
}




#include <vtkAxesActor.h>
#include <vtkCaptionActor2D.h>

static vtkSmartPointer<vtkAxesActor> MakeAxesActor(std::array<double, 3>& scale, std::array<std::string, 3>& xyzLabels)
{
	vtkNew<vtkAxesActor> axes;
	axes->SetScale(scale[0], scale[1], scale[2]);
	axes->SetShaftTypeToCylinder();
	axes->SetXAxisLabelText(xyzLabels[0].c_str());
	axes->SetYAxisLabelText(xyzLabels[1].c_str());
	axes->SetZAxisLabelText(xyzLabels[2].c_str());
	axes->SetCylinderRadius(0.5 * axes->GetCylinderRadius());
	axes->SetConeRadius(1.025 * axes->GetConeRadius());
	axes->SetSphereRadius(1.5 * axes->GetSphereRadius());
	vtkTextProperty* tprop = axes->GetXAxisCaptionActor2D()->GetCaptionTextProperty();
	tprop->ItalicOn();
	tprop->ShadowOn();
	tprop->SetFontFamilyToTimes();
	// Use the same text properties on the other two axes.
	axes->GetYAxisCaptionActor2D()->GetCaptionTextProperty()->ShallowCopy(tprop);
	axes->GetZAxisCaptionActor2D()->GetCaptionTextProperty()->ShallowCopy(tprop);
	return axes;
}




static QBrush WidgetTextBrush(QWidget* w)
{
	if (w)
		return w->palette().text();
	else
		return qApp->palette().text();
}


#include <vtkTriangleFilter.h>
#include <vtkDecimatePro.h>
#include <vtkUnstructuredGrid.h>
#include <vtkStructuredGrid.h>
#include <vtkGeometryFilter.h>

static vtkDataObject* toDecimatedPolyData(vtkDataObject* obj)
{
	vtkSmartPointer < vtkPolyData> poly ;
	if (obj->IsA("vtkPolyData"))
		poly = vtkSmartPointer<vtkPolyData> (static_cast<vtkPolyData*>(obj));
	else if (obj->IsA("vtkDataSet")) {
		vtkGeometryFilter* f = vtkGeometryFilter::New();
		f->SetInputData(obj);
		f->Update();
		poly = vtkSmartPointer<vtkPolyData> (f->GetOutput());
		f->Delete();
		if (!poly)
			return nullptr;

		// Remove attributes as the vtkGeometryFilter produces errors
		while (poly->GetPointData()->GetNumberOfArrays())
			poly->GetPointData()->RemoveArray(0);
		while (poly->GetCellData()->GetNumberOfArrays())
			poly->GetCellData()->RemoveArray(0);
		while (poly->GetFieldData()->GetNumberOfArrays())
			poly->GetFieldData()->RemoveArray(0);
	}
	if (!poly)
		return nullptr;

	vtkTriangleFilter* tr = vtkTriangleFilter::New();
	tr->SetInputData(poly);
	tr->Update();
	vtkSmartPointer<vtkPolyData> tmp = vtkSmartPointer<vtkPolyData> (tr->GetOutput());
	if (!tmp) 
		return nullptr;
	tr->Delete();

	vtkDecimatePro* dec = vtkDecimatePro::New();
	dec->SetInputData(tmp);
	dec->SetTargetReduction(0.95);
	dec->PreserveTopologyOff();
	dec->Update();
	vtkDataObject* res = dec->GetOutput();
	res->Register(res);
	dec->Delete();
	return res;
}
/*
static vtkDataObject* toDecimatedPolyData(vtkDataObject* obj)
{
	if (obj->IsA("vtkStructuredGrid")) {
		vtkExplicitStructuredGridToUnstructuredGrid* conv = vtkExplicitStructuredGridToUnstructuredGrid::New();
		conv->SetInputData(obj);
		conv->Update();
		vtkUnstructuredGrid * u = conv->GetOutput();
		u->Register(u);
		conv->Delete();

		vtkUnstructuredGridQuadricDecimation* dec = vtkUnstructuredGridQuadricDecimation::New();
		dec->SetTargetReduction(0.9);
		dec->SetInputData(u);
		dec->Update();
		obj = dec->GetOutput();
		obj->Register(obj);
		dec->Delete();
		return obj;
	}

	if (obj->IsA("vtkUnstructuredGrid")) {
		vtkUnstructuredGridQuadricDecimation* dec = vtkUnstructuredGridQuadricDecimation::New();
		dec->SetTargetReduction(0.9);
		dec->SetInputData(obj);
		dec->Update();
		obj = dec->GetOutput();
		obj->Register(obj);
		dec->Delete();
		return obj;
	}
	if (obj->IsA("vtkStructuredGrid")) {
		vtkUnstructuredGridQuadricDecimation* dec = vtkUnstructuredGridQuadricDecimation::New();
		dec->SetTargetReduction(0.9);
		dec->SetInputData(obj);
		dec->Update();
		obj = dec->GetOutput();
		obj->Register(obj);
		dec->Delete();
		return obj;
	}
	if (obj->IsA("vtkPolyData")) {
		vtkDecimatePro* dec = vtkDecimatePro::New();
		dec->SetInputData(obj);
		dec->SetTargetReduction(0.9);
		dec->PreserveTopologyOn();
		dec->Update();
		obj = dec->GetOutput();
		obj->Register(obj);
		dec->Delete();
		return obj;
	}
	
}
*/
struct Decimated
{
	VipPlotVTKObject* plot = nullptr;
	vtkDataObject* src = nullptr;
};

static void computeDecimatedObjects(QHash<VipPlotVTKObject*, Decimated>& map, const PlotVipVTKObjectList& plots)
{
	QHash<VipPlotVTKObject*, Decimated> new_map;
	QVector<VipPlotVTKObject*> vector_to_add;

	for ( auto* plot : plots) {
		auto it = map.find((VipPlotVTKObject*)plot);
		bool to_add = false;
		VipVTKObject obj = plot->rawData();
		if (vtkPoints* pts = obj.points())
			if (pts->GetNumberOfPoints() > 10000) {
				if (it != map.end()) {
					auto mtime_pts = pts->GetMTime();
					if (mtime_pts > it.value().plot->rawData().data()->GetMTime() || it.value().src != obj.data())
						to_add = true;
					else {
						new_map.insert(it.key(), it.value());
					}
				}
				else
					to_add = true;
			}
		if (to_add) {
			vector_to_add.push_back(plot);
			/* vtkPolyData* dec = toDecimatedPolyData(obj.data());
			if (dec) {
				VipPlotVTKObject* p = it == map.end() ? new VipPlotVTKObject() : it.value().plot;
				p->setProperty("_vip_hidden",true);
				p->setRawData(VipVTKObject(dec));
				dec->Delete();
				p->setAxes(plot->axes(), VipCoordinateSystem::Cartesian);
				new_map.insert(plot, { p, obj.data() });
			}*/
		}
	}

	QVector<vtkDataObject*> decimated(vector_to_add.size());
#pragma omp parallel for
	for (int i = 0; i < (int)vector_to_add.size(); ++i) {
		VipPlotVTKObject* plot = vector_to_add[i];
		decimated[i] = toDecimatedPolyData(plot->rawData().data());
	}

	for (int i = 0; i < (int)vector_to_add.size(); ++i) {
		vtkDataObject* dec = decimated[i];
		VipPlotVTKObject* plot = vector_to_add[i];
		if (dec) {
			auto it = map.find((VipPlotVTKObject*)plot);
			VipPlotVTKObject* p = it == map.end() ? new VipPlotVTKObject() : it.value().plot;
			p->setProperty("_vip_no_serialize", true);
			p->setProperty("_vip_hidden", true);
			p->setRawData(VipVTKObject(dec));
			dec->Delete();
			p->setAxes(plot->axes(), VipCoordinateSystem::Cartesian);
			new_map.insert(plot, { p, plot->rawData().data() });
		}
	}

	// remove old objects
	for (auto it = map.begin(); it != map.end(); ++it) {
		if (new_map.find(it.key()) == new_map.end()) {
			//delete value
			it.value().plot->deleteLater();
		}
	}

	map = std::move(new_map);

	//set all attributes
	for (auto it = map.begin(); it != map.end(); ++it) {
		auto* plot = it.key();
		auto* dec = it.value().plot;

		dec->actor()->SetVisibility(plot->isVisible());
		plot->actor()->SetVisibility(false);

		dec->setSelectedColor(plot->selectedColor());
		dec->setColor(plot->color());
		dec->setEdgeColor(plot->edgeColor());
		if (plot->hasHighlightColor())
			dec->setHighlightColor(plot->highlightColor());
		else
			dec->removeHighlightColor();
		dec->setEdgeVisible(plot->edgeVisible());
		dec->setOpacity(plot->opacity());
		dec->setLayer(plot->layer());
		dec->actor()->GetProperty()->SetLighting(plot->actor()->GetProperty()->GetLighting());
		dec->setSelected(plot->isSelected());
	}

	
}


class VipVTKGraphicsView::PrivateData
{
public:
	VipVTKWidget* widget = nullptr;
	VipInfoWidget* infos = nullptr;
	vtkRenderer* renderer = nullptr;
	QList<vtkRenderer*> renderers;
	QGraphicsItem* itemUnderMouse = nullptr;
	QPointer<QGraphicsObject> objectUnderMouse;
	VipColorPalette palette;
	VipBorderLegend* annotationLegend;
	OffscreenExtractShapeStatistics* stats = nullptr;

	vtkSmartPointer<vtkLookupTable> lut;
	vtkSmartPointer<vtkScalarBarActor> scalarBar;
	vtkSmartPointer<vtkCoordinate> coordinates;

	vtkSmartPointer<vtkCubeAxesActor> cubeAxesActor;
	// vtkSmartPointer<vtkGridAxes3DActor> mGridAxesActor;
	vtkSmartPointer<vtkOrientationMarkerWidget> orientationAxes;

	QHash<VipPlotVTKObject*, Decimated> decimated;

	bool trackingEnabled = false;
	bool dirtyColorMapDiv;
	bool initialized{ false };
	bool inRefresh{ false };
	bool hasLight{ true };
	bool resetCameraEnabled{ true };
	bool decimateOnMove{ true };
	OffscreenExtractContour contours;
	qint64 lastFailContour = 0;
};


VipVTKGraphicsView::VipVTKGraphicsView()
  : VipImageWidget2D()
{
	VIP_CREATE_PRIVATE_DATA();

	// Tell that we use a specific viewport
	this->setUseInternalViewport(true);
	this->setAttribute(Qt::WA_DeleteOnClose);
	// this->setFrameShape(QFrame::NoFrame);

	d_data->widget = new VipVTKWidget(); // mCtx);
	this->setViewport(d_data->widget);
	this->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	d_data->widget->renderWindow()->SetSwapBuffers(0); // don't let VTK swap buffers on us

	// Currently unused
	//d_data->stats = new OffscreenExtractShapeStatistics(this);
	d_data->stats = nullptr;

	// Overlayed information widget
	d_data->infos = new VipInfoWidget(this);
	d_data->infos->move(0, 0);

	// Lookup table
	d_data->lut = vtkSmartPointer<vtkLookupTable>::New();
	d_data->lut->SetRange(0, 100);
	d_data->lut->SetNanColor((double*)VipVTKObject::defaultObjectColor());
	VIP_VTK_OBSERVER(d_data->lut);

	// Thread pool used to extract point data range
	//int concurrency = std::thread::hardware_concurrency() / 2;
	//if (concurrency == 0)
	//	concurrency = 1;
	//mPool.setMaxThreadCount(concurrency);

	// set the colors
	double min = 0;
	double max = 100;
	VipInterval interval(min, max);
	int num_colors = table()->GetNumberOfColors();
	double step = 1;
	if (num_colors > 1)
		step = (max - min) / double(num_colors - 1);
	VipLinearColorMap* map = VipLinearColorMap::createColorMap(VipLinearColorMap::Jet);
	map->setExternalValue(VipColorMap::ColorBounds);

	// set the values for the grip interval
	for (int i = 0; i < num_colors; ++i) {
		double c[4];
		QColor color = map->rgb(interval, min + i * step); // stops[i].second;
		c[0] = color.redF();
		c[1] = color.greenF();
		c[2] = color.blueF();
		c[3] = 1;
		table()->SetTableValue(i, c);
	}
	delete map;

	

	// Color palette used to affect colors to data objects
	d_data->palette = VipColorPalette(VipLinearColorMap::ColorPaletteRandom);

	d_data->scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();
	d_data->scalarBar->SetLookupTable(d_data->lut);
	d_data->scalarBar->SetTitle("Title");
	d_data->scalarBar->SetNumberOfLabels(4);
	d_data->scalarBar->SetOrientationToVertical();
	d_data->scalarBar->SetMaximumWidthInPixels(80);
	d_data->scalarBar->GetLabelTextProperty()->SetColor(0, 0, 0);
	d_data->scalarBar->GetLabelTextProperty()->SetBold(1);
	d_data->scalarBar->GetLabelTextProperty()->SetItalic(0);
	d_data->scalarBar->GetLabelTextProperty()->SetFontSize(14);
	d_data->scalarBar->GetLabelTextProperty()->SetShadow(0);

	d_data->scalarBar->GetTitleTextProperty()->SetColor(0, 0, 0);
	d_data->scalarBar->GetTitleTextProperty()->SetBold(1);
	d_data->scalarBar->GetTitleTextProperty()->SetItalic(0);
	d_data->scalarBar->GetTitleTextProperty()->SetFontSize(18);
	d_data->scalarBar->GetTitleTextProperty()->SetShadow(0);
	d_data->scalarBar->VisibilityOff();
	VIP_VTK_OBSERVER(d_data->scalarBar);

	/* mLegend = vtkSmartPointer<vtkLegendBoxActor>::New();
	mLegend->GetEntryTextProperty ()->SetColor(0,0,0);
	mLegend->GetEntryTextProperty()->SetBold(1);
	mLegend->GetEntryTextProperty()->SetItalic(0);
	mLegend->GetEntryTextProperty()->SetFontSize(14);
	mLegend->GetEntryTextProperty()->SetShadow(0);
	mLegend->VisibilityOff();
	// place legend in lower right
	mLegend->GetPositionCoordinate()->SetCoordinateSystemToView();
	mLegend->GetPositionCoordinate()->SetValue(.5, -1.0);
	mLegend->GetPosition2Coordinate()->SetCoordinateSystemToView();
	mLegend->GetPosition2Coordinate()->SetValue(1.0, -0.5);
	VIP_VTK_OBSERVER(mLegend);*/

	// Main renderer
	d_data->renderer = vtkRenderer::New();
	widget()->renderWindow()->AddRenderer(d_data->renderer);
	d_data->renderer->SetInteractive(1);
	d_data->renderers.append(d_data->renderer);
	d_data->renderer->Delete();

	// NEW for vtk9.1
	d_data->renderer->GetActiveCamera();

	VIP_VTK_OBSERVER(d_data->renderer);
	VIP_VTK_OBSERVER(widget()->GetRenderWindow());

	// Add layers
	for (int i = 1; i < 10; ++i) {
		vtkRenderer* ren = vtkRenderer::New();
		ren->SetLayer(i);
		widget()->renderWindow()->AddRenderer(ren);
		ren->SetInteractive(0);
		ren->Delete();
		// NEW for vtk9.1
		ren->GetActiveCamera();
		d_data->renderers << ren;
		VIP_VTK_OBSERVER(ren);
	}
	widget()->renderWindow()->SetNumberOfLayers(10);

	// Add legend and bar to the last layer
	d_data->renderers.last()->AddActor(d_data->scalarBar);
	// d_data->renderers.last()->AddActor(mLegend);

	// Add axes
	/* d_data->cubeAxesActor = vtkSmartPointer<vtkCubeAxesActor>::New();
	double bounds[6] = { 0, 1, 0, 1, 0, 1 };
	d_data->cubeAxesActor->SetBounds(bounds); // superquadricSource->GetOutput()->GetBounds());
	d_data->cubeAxesActor->SetUse2DMode(1);
	d_data->cubeAxesActor->SetLabelScaling(false, 2, 2, 2);

	d_data->cubeAxesActor->SetCamera(d_data->renderers.last()->GetActiveCamera());
	d_data->cubeAxesActor->GetTitleTextProperty(0)->SetColor(1.0, 0.0, 0.0);
	d_data->cubeAxesActor->GetLabelTextProperty(0)->SetColor(1.0, 0.0, 0.0);
	d_data->cubeAxesActor->GetLabelTextProperty(0)->SetFontSize(12);
	d_data->cubeAxesActor->GetTitleTextProperty(0)->SetFontSize(16);
	d_data->cubeAxesActor->GetXAxesLinesProperty()->SetColor(1.0, 0.0, 0.0);

	d_data->cubeAxesActor->GetTitleTextProperty(1)->SetColor(0.0, 1.0, 0.0);
	d_data->cubeAxesActor->GetLabelTextProperty(1)->SetColor(0.0, 1.0, 0.0);
	d_data->cubeAxesActor->GetLabelTextProperty(1)->SetFontSize(12);
	d_data->cubeAxesActor->GetTitleTextProperty(1)->SetFontSize(16);
	d_data->cubeAxesActor->GetYAxesLinesProperty()->SetColor(0.0, 1.0, 0.0);

	d_data->cubeAxesActor->GetTitleTextProperty(2)->SetColor(0.0, 0.0, 1.0);
	d_data->cubeAxesActor->GetLabelTextProperty(2)->SetColor(0.0, 0.0, 1.0);
	d_data->cubeAxesActor->GetLabelTextProperty(2)->SetFontSize(12);
	d_data->cubeAxesActor->GetTitleTextProperty(2)->SetFontSize(16);
	d_data->cubeAxesActor->GetZAxesLinesProperty()->SetColor(0.0, 0.0, 1.0);
	VIP_VTK_OBSERVER(d_data->cubeAxesActor);

//d_data->cubeAxesActor->DrawXGridlinesOn();
//d_data->cubeAxesActor->DrawYGridlinesOn();
//d_data->cubeAxesActor->DrawZGridlinesOn();
#if VTK_MAJOR_VERSION > 5
// d_data->cubeAxesActor->SetGridLineLocation(VTK_GRID_LINES_FURTHEST);
#endif

	d_data->cubeAxesActor->XAxisMinorTickVisibilityOff();
	d_data->cubeAxesActor->YAxisMinorTickVisibilityOff();
	d_data->cubeAxesActor->ZAxisMinorTickVisibilityOff();
	d_data->cubeAxesActor->SetVisibility(0);

	d_data->renderers.last()->AddActor(d_data->cubeAxesActor);
	*/

	//TEST

	// Define colors for this example.
	vtkNew<vtkNamedColors> colors;

	vtkColor3d backgroundColor = colors->GetColor3d("DarkSlateGray");
	vtkColor3d actorColor = colors->GetColor3d("Tomato");
	vtkColor3d axis1Color = colors->GetColor3d("Salmon");
	vtkColor3d axis2Color = colors->GetColor3d("PaleGreen");
	vtkColor3d axis3Color = colors->GetColor3d("LightSkyBlue");

	//TODO
	double textColorf[3];
	QColor tcolor = WidgetTextBrush(qApp->topLevelWidgets().first()).color();
	vipFromQColor(tcolor, textColorf);

	d_data->cubeAxesActor = vtkSmartPointer<vtkCubeAxesActor>::New();
	d_data->cubeAxesActor->SetUseTextActor3D(1);
	double bounds[6] = { 0, 1, 0, 1, 0, 1 };
	d_data->cubeAxesActor->SetBounds(bounds);
	d_data->cubeAxesActor->SetCamera(d_data->renderers.last()->GetActiveCamera());
	d_data->cubeAxesActor->GetTitleTextProperty(0)->SetColor(axis1Color.GetData());
	d_data->cubeAxesActor->GetLabelTextProperty(0)->SetColor(axis1Color.GetData());

	d_data->cubeAxesActor->GetTitleTextProperty(1)->SetColor(axis2Color.GetData());
	d_data->cubeAxesActor->GetLabelTextProperty(1)->SetColor(axis2Color.GetData());

	d_data->cubeAxesActor->GetTitleTextProperty(2)->SetColor(axis3Color.GetData());
	d_data->cubeAxesActor->GetLabelTextProperty(2)->SetColor(axis3Color.GetData());

	d_data->cubeAxesActor->GetXAxesLinesProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetXAxesGridlinesProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetXAxesGridpolysProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetXAxesGridpolysProperty()->SetOpacity(0.2);

	d_data->cubeAxesActor->GetYAxesLinesProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetYAxesGridlinesProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetYAxesGridpolysProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetYAxesGridpolysProperty()->SetOpacity(0.2);

	d_data->cubeAxesActor->GetZAxesLinesProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetZAxesGridlinesProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetZAxesGridpolysProperty()->SetColor(textColorf);
	d_data->cubeAxesActor->GetZAxesGridpolysProperty()->SetOpacity(0.2);


	for (int i = 0; i < 3; ++i) {
		d_data->cubeAxesActor->GetLabelTextProperty(i)->SetFontSize(12);
		d_data->cubeAxesActor->GetTitleTextProperty(i)->SetFontSize(16);
	}

	d_data->cubeAxesActor->DrawXGridlinesOn();
	d_data->cubeAxesActor->DrawYGridlinesOn();
	d_data->cubeAxesActor->DrawZGridlinesOn();

	d_data->cubeAxesActor->SetCornerOffset(0);

	d_data->cubeAxesActor->SetUse2DMode(1);
	d_data->cubeAxesActor->SetLabelScaling(false, 2, 2, 2);


#if VTK_MAJOR_VERSION == 6
	d_data->cubeAxesActor->SetGridLineLocation(VTK_GRID_LINES_FURTHEST);
#endif
#if VTK_MAJOR_VERSION > 6
	d_data->cubeAxesActor->SetGridLineLocation(d_data->cubeAxesActor->VTK_GRID_LINES_FURTHEST);
#endif

	d_data->cubeAxesActor->XAxisMinorTickVisibilityOff();
	d_data->cubeAxesActor->YAxisMinorTickVisibilityOff();
	d_data->cubeAxesActor->ZAxisMinorTickVisibilityOff();

	//d_data->cubeAxesActor->SetFlyModeToStaticEdges();
	//d_data->cubeAxesActor->SetFlyModeToStaticTriad();
	//d_data->cubeAxesActor->SetFlyModeToFurthestTriad();
	//d_data->cubeAxesActor->SetFlyModeToClosestTriad();
	d_data->cubeAxesActor->SetFlyModeToOuterEdges();
	//d_data->cubeAxesActor->SetStickyAxes(true);

	d_data->cubeAxesActor->SetVisibility(0);

	d_data->renderers.first()->AddActor(d_data->cubeAxesActor);
	


	//TEST
	//mGridAxesActor = vtkSmartPointer<vtkGridAxes3DActor>::New();
	//mGridAxesActor->SetVisibility(1);
	//d_data->renderers.first()->AddActor(d_data->cubeAxesActor);




	std::array<std::string, 3> xyzLabels{ { "X", "Y", "Z" } };
	std::array<double, 3> scale{ { 1.0, 1.0, 1.0 } };
	auto axes = MakeAxesActor(scale, xyzLabels);
	d_data->orientationAxes = vtkSmartPointer<vtkOrientationMarkerWidget>::New();

	d_data->orientationAxes->SetOrientationMarker(axes);
	// widget->SetCurrentRenderer(ren);
	d_data->orientationAxes->SetInteractor(d_data->widget->renderWindow()->GetInteractor());
	d_data->orientationAxes->SetEnabled(1);
	d_data->orientationAxes->SetInteractive(0);
	d_data->orientationAxes->SetViewport(0., 0., 0.2, 0.2);
	
	

	widget()->renderWindow()->LineSmoothingOn();
	widget()->renderWindow()->PolygonSmoothingOn();
	widget()->renderWindow()->PointSmoothingOn();
	widget()->renderWindow()->AlphaBitPlanesOn();

	// Reset cameras
	for (int i = 0; i < d_data->renderers.size(); ++i) {
		d_data->renderers[i]->Render();
		d_data->renderers[i]->ResetCamera();
	}

	connect(this, SIGNAL(dataChanged()), this, SLOT(computeAxesBounds()));

	// Take into account the close/minimize/maximize icons on the top right
	area()->colorMapAxis()->setMinBorderDist(0, 30);

	// use the color map from thermavip
	connect(area()->colorMapAxis(), SIGNAL(scaleDivChanged(bool)), this, SLOT(colorMapDivModified()));
	connect(area()->colorMapAxis(), SIGNAL(scaleNeedUpdate()), this, SLOT(colorMapModified()));
	connect(area()->colorMapAxis()->grip1(), SIGNAL(valueChanged(double)), this, SLOT(colorMapDivModified()));
	connect(area()->colorMapAxis()->grip2(), SIGNAL(valueChanged(double)), this, SLOT(colorMapDivModified()));
	table()->SetNanColor(VipVTKObject::defaultObjectColor());
	d_data->dirtyColorMapDiv = false;

	this->area()->legend()->setVisible(true);

	// setup annotation legend
	d_data->annotationLegend = new VipBorderLegend(VipBorderLegend::Bottom);
	d_data->annotationLegend->setLegend(new VipLegend());
	this->area()->addScale(d_data->annotationLegend, false);
	d_data->annotationLegend->setVisible(false);

	// Setup contour extraction
	d_data->contours.SetRenderWindow(d_data->widget->renderWindow());
	setTrackingEnable(false);

	// Setup coordinate system
	d_data->coordinates = vtkSmartPointer<vtkCoordinate>::New();
	d_data->coordinates->SetCoordinateSystemToWorld();
	d_data->coordinates->SetViewport(d_data->renderer);
	VIP_VTK_OBSERVER(d_data->coordinates);

	
	// set the background
	this->style()->unpolish(this);
	this->style()->polish(this);
	setBackgroundColor(this->palette().color(QPalette::Window));

	// add a background color to the color map labels so they will always be visible
	QColor c = this->palette().color(QPalette::Window);
	c.setAlpha(50);
	area()->colorMapAxis()->scaleDraw()->textStyle().setBackgroundBrush(c);
	area()->colorMapAxis()->setVisible(false);

	// make sure source properties are propagated to VipDisplayObject
	connect(this, SIGNAL(dataChanged()), this, SLOT(propagateSourceProperties()));
}

VipVTKGraphicsView::~VipVTKGraphicsView()
{
	//delete d_data->stats;
}

void VipVTKGraphicsView::resetActiveCameraToDirection(double look_x, double look_y, double look_z, double up_x, double up_y, double up_z)
{
	if (vtkCamera* cam = d_data->renderer->GetActiveCamera()) {
		cam->SetPosition(0, 0, 0);
		cam->SetFocalPoint(look_x, look_y, look_z);
		cam->SetViewUp(up_x, up_y, up_z);
		d_data->widget->applyCameraToAllLayers();
	}
}

void VipVTKGraphicsView::resetActiveCameraToPositiveX()
{
	this->resetActiveCameraToDirection(1, 0, 0, 0, 0, 1);
	this->resetCamera();
}
void VipVTKGraphicsView::resetActiveCameraToNegativeX()
{
	this->resetActiveCameraToDirection(-1, 0, 0, 0, 0, 1);
	this->resetCamera();
}
void VipVTKGraphicsView::resetActiveCameraToPositiveY()
{
	this->resetActiveCameraToDirection(0, 1, 0, 0, 0, 1);
	this->resetCamera();
}
void VipVTKGraphicsView::resetActiveCameraToNegativeY()
{
	this->resetActiveCameraToDirection(0, -1, 0, 0, 0, 1);
	this->resetCamera();
}
void VipVTKGraphicsView::resetActiveCameraToPositiveZ()
{
	this->resetActiveCameraToDirection(0, 0, 1, 0, 1, 0);
	this->resetCamera();
}
void VipVTKGraphicsView::resetActiveCameraToNegativeZ()
{
	this->resetActiveCameraToDirection(0, 0, -1, 0, 1, 0);
	this->resetCamera();
}

void VipVTKGraphicsView::rotateClockwise90()
{
	if (vtkCamera* cam = d_data->renderer->GetActiveCamera()) {
		cam->Roll(-90);
		d_data->widget->applyCameraToAllLayers();
		refresh();
	}
}
void VipVTKGraphicsView::rotateCounterClockwise90()
{
	if (vtkCamera* cam = d_data->renderer->GetActiveCamera()) {
		cam->Roll(-90);
		d_data->widget->applyCameraToAllLayers();
		refresh();
	}
}

static void RotateElevation(vtkCamera* camera, double angle)
{
	vtkNew<vtkTransform> transform;

	double scale = vtkMath::Norm(camera->GetPosition());
	if (scale <= 0.0) {
		scale = vtkMath::Norm(camera->GetFocalPoint());
		if (scale <= 0.0) {
			scale = 1.0;
		}
	}
	double* temp = camera->GetFocalPoint();
	camera->SetFocalPoint(temp[0] / scale, temp[1] / scale, temp[2] / scale);
	temp = camera->GetPosition();
	camera->SetPosition(temp[0] / scale, temp[1] / scale, temp[2] / scale);

	double v2[3];
	// translate to center
	// we rotate around 0,0,0 rather than the center of rotation
	transform->Identity();

	// elevation
	camera->OrthogonalizeViewUp();
	double* viewUp = camera->GetViewUp();
	vtkMath::Cross(camera->GetDirectionOfProjection(), viewUp, v2);
	transform->RotateWXYZ(-angle, v2[0], v2[1], v2[2]);

	// translate back
	// we are already at 0,0,0
	camera->ApplyTransform(transform.GetPointer());
	camera->OrthogonalizeViewUp();

	// For rescale back.
	temp = camera->GetFocalPoint();
	camera->SetFocalPoint(temp[0] * scale, temp[1] * scale, temp[2] * scale);
	temp = camera->GetPosition();
	camera->SetPosition(temp[0] * scale, temp[1] * scale, temp[2] * scale);
}

const double isometric_elev = vtkMath::DegreesFromRadians(std::asin(std::tan(vtkMath::Pi() / 6.0)));

void VipVTKGraphicsView::resetActiveCameraToIsometricView()
{
	vtkCamera* cam = d_data->renderer->GetActiveCamera();
	// Ref: Fig 2.4 - Brian Griffith: "Engineering Drawing for Manufacture", DOI
	// https://doi.org/10.1016/B978-185718033-6/50016-1
	resetActiveCameraToDirection(0, 0, -1, 0, 1, 0);
	cam->Azimuth(45.);
	RotateElevation(cam, isometric_elev);
	resetCamera();
}

static void ExpandBounds(vtkRenderer * ren, double bounds[6], vtkMatrix4x4* matrix)
{
  if (!matrix)
  {
    return;
  }

  // Expand the bounding box by model view transform matrix.
  double pt[8][4] = { { bounds[0], bounds[2], bounds[5], 1.0 },
    { bounds[1], bounds[2], bounds[5], 1.0 }, { bounds[1], bounds[2], bounds[4], 1.0 },
    { bounds[0], bounds[2], bounds[4], 1.0 }, { bounds[0], bounds[3], bounds[5], 1.0 },
    { bounds[1], bounds[3], bounds[5], 1.0 }, { bounds[1], bounds[3], bounds[4], 1.0 },
    { bounds[0], bounds[3], bounds[4], 1.0 } };

  // \note: Assuming that matrix doesn not have projective component. Hence not
  // dividing by the homogeneous coordinate after multiplication
  for (int i = 0; i < 8; ++i)
  {
    matrix->MultiplyPoint(pt[i], pt[i]);
  }

  // min = mpx = pt[0]
  double min[4], max[4];
  for (int i = 0; i < 4; ++i)
  {
    min[i] = pt[0][i];
    max[i] = pt[0][i];
  }

  for (int i = 1; i < 8; ++i)
  {
    for (int j = 0; j < 3; ++j)
    {
      if (min[j] > pt[i][j])
        min[j] = pt[i][j];
      if (max[j] < pt[i][j])
        max[j] = pt[i][j];
    }
  }

  // Copy values back to bounds.
  bounds[0] = min[0];
  bounds[2] = min[1];
  bounds[4] = min[2];

  bounds[1] = max[0];
  bounds[3] = max[1];
  bounds[5] = max[2];
}

static void ZoomToBoxUsingViewAngle(vtkRenderer * ren, const vtkRecti& box, double offsetRatio)
{
  const int* size = ren->GetSize();
  double zf1 = size[0] / static_cast<double>(box.GetWidth());
  double zf2 = size[1] / static_cast<double>(box.GetHeight());
  double zoomFactor = std::min(zf1, zf2);

  // OffsetRatio will let a free space between the zoomed data
  // And the edges of the window
  ren->GetActiveCamera()->Zoom(zoomFactor * offsetRatio);
}

void VipVTKGraphicsView::resetCamera(bool closest , double offsetRatio) {
	if (closest) {
		double bounds[6];
		computeBounds(bounds);
#if VTK_VERSION_MAJOR >= 9
		d_data->renderer->ResetCameraScreenSpace(bounds, offsetRatio);
#else
		// Make sure all bounds are visible to project into screen space
		d_data->renderer->ResetCamera(bounds);

		double expandedBounds[6] = { bounds[0], bounds[1], bounds[2], bounds[3], bounds[4], bounds[5] };
		ExpandBounds(d_data->renderer,expandedBounds, d_data->renderer->GetActiveCamera()->GetModelTransformMatrix());

		// 1) Compute the screen space bounding box
		double xmin = VTK_DOUBLE_MAX;
		double ymin = VTK_DOUBLE_MAX;
		double xmax = VTK_DOUBLE_MIN;
		double ymax = VTK_DOUBLE_MIN;
		double currentPointDisplay[3];
		for (int i = 0; i < 2; ++i)
		{
		for (int j = 0; j < 2; ++j)
		{
			for (int k = 0; k < 2; ++k)
			{
			double currentPoint[4] = { expandedBounds[i], expandedBounds[j + 2], expandedBounds[k + 4],
				1.0 };

			d_data->renderer->SetWorldPoint(currentPoint);
			d_data->renderer->WorldToDisplay();
			d_data->renderer->GetDisplayPoint(currentPointDisplay);

			xmin = std::min(currentPointDisplay[0], xmin);
			xmax = std::max(currentPointDisplay[0], xmax);
			ymin = std::min(currentPointDisplay[1], ymin);
			ymax = std::max(currentPointDisplay[1], ymax);
			}
		}
		}

		// Project the focal point in screen space
		double fp[4];
		d_data->renderer->GetActiveCamera()->GetFocalPoint(fp);
		fp[3] = 1.0;
		double fpDisplay[3];
		d_data->renderer->SetWorldPoint(fp);
		d_data->renderer->WorldToDisplay();
		d_data->renderer->GetDisplayPoint(fpDisplay);

		// The focal point must be at the center of the box
		// So construct a box with fpDisplay at the center
		int xCenterFocalPoint = static_cast<int>(fpDisplay[0]);
		int yCenterFocalPoint = static_cast<int>(fpDisplay[1]);

		int xCenterBox = static_cast<int>((xmin + xmax) / 2);
		int yCenterBox = static_cast<int>((ymin + ymax) / 2);

		int xDiff = 2 * (xCenterFocalPoint - xCenterBox);
		int yDiff = 2 * (yCenterFocalPoint - yCenterBox);

		int xMaxOffset = std::max(xDiff, 0);
		int xMinOffset = std::min(xDiff, 0);
		int yMaxOffset = std::max(yDiff, 0);
		int yMinOffset = std::min(yDiff, 0);

		xmin += xMinOffset;
		xmax += xMaxOffset;
		ymin += yMinOffset;
		ymax += yMaxOffset;
		// Now the focal point is at the center of the box

		const vtkRecti box(xmin, ymin, xmax - xmin, ymax - ymin);
		ZoomToBoxUsingViewAngle(d_data->renderer, box, offsetRatio);
#endif
		d_data->widget->applyCameraToAllLayers();
	}
	else
		resetCamera();
}


void VipVTKGraphicsView::setBackgroundColor(const QColor& color)
{
	double dback[3];
	vipFromQColor(color, dback);
	d_data->renderer->SetBackground(dback); // stdBackgroundColor());
}

QColor VipVTKGraphicsView::backgroundColor() const
{
	return vipToQColor(d_data->renderer->GetBackground());
}

void VipVTKGraphicsView::startRender(VipRenderState& state)
{
	VipImageWidget2D::startRender(state);
	infos()->hide();
}

void VipVTKGraphicsView::endRender(VipRenderState& state)
{
	VipImageWidget2D::endRender(state);
	infos()->setVisible(!infos()->text().isEmpty());
}

bool VipVTKGraphicsView::renderObject(QPainter* p, const QPointF& pos, bool draw_background)
{
	if (isVisible()) {
		if (!draw_background) {
			if (scene()) {
				QRectF visible = mapToScene(viewport()->geometry()).boundingRect();
				QRectF target(QPointF(0, 0), p->worldTransform().map(QRectF(QPointF(0, 0), this->size())).boundingRect().size());

				QImage img(target.size().toSize(), QImage::Format_ARGB32);
				img.fill(QColor(255, 255, 255, 0));
				{
					QPainter p(&img);
					p.setTransform(QTransform().scale(double(target.width()) / this->width(), double(target.height()) / this->height()));
					p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
					this->QWidget::render(&p, QPoint(0, 0), QRegion(), QWidget::DrawChildren);
				}
				{
					QPainter p(&img);
					p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
					scene()->render(&p, target, visible);
				}

				/*uint back = vipToQColor(renderer()->GetBackground()).rgba();
				uint new_back = qRgba(255, 255, 255, 0);
				int size = img.width()*img.height();
				uint * pix = (uint*)img.bits();
				for (int i = 0; i < size; ++i)
					if (pix[i] == back)
						pix[i] = new_back;*/

				p->save();
				p->drawImage(QRectF(pos, this->size()), img, target);
				p->restore();
			}

			return false;
		}
		else {
			QWidget::render(p, pos.toPoint(), QRegion(), draw_background ? QWidget::DrawWindowBackground : QWidget::RenderFlags());
			return true;
		}
	}
	return false;
}

void VipVTKGraphicsView::setSourceProperty(const char* name, const QVariant& value)
{
	this->setProperty(name, value);
	this->setProperty((QByteArray("__source_") + name).data(), value);
	this->propagateSourceProperties();
}
QList<QByteArray> VipVTKGraphicsView::sourceProperties() const
{
	QList<QByteArray> res;
	QList<QByteArray> names = this->dynamicPropertyNames();
	for (int i = 0; i < names.size(); ++i)
		if (names[i].startsWith("__source_"))
			res << names[i].mid(9);

	return res;
}
#include "VipSet.h"
void VipVTKGraphicsView::propagateSourceProperties()
{
	QList<QByteArray> names = sourceProperties();
	QVariantMap props;
	for (int i = 0; i < names.size(); ++i)
		props[names[i]] = this->property(names[i].data());

	// propagate the properties
	QList<VipAbstractScale*> scales = this->area()->allScales();
	QSet<VipPlotItem*> its;
	for (int i = 0; i < scales.size(); ++i)
		its.unite(vipToSet( scales[i]->plotItems()));
	QList<VipPlotItem*> items ( its.begin(), its.end()); // this->area()->findItems<VipPlotItem*>();
	for (int i = 0; i < items.size(); ++i) {
		if (VipDisplayObject* display = items[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
			for (QVariantMap::const_iterator it = props.begin(); it != props.end(); ++it)
				display->setSourceProperty(it.key().toLatin1().data(), it.value());
		}
	}
}

QPoint VipVTKGraphicsView::transformToView(double* pt)
{
	d_data->coordinates->SetCoordinateSystemToWorld();
	d_data->coordinates->SetValue(pt);
	int* world = d_data->coordinates->GetComputedDisplayValue(d_data->renderer);
	return QPoint(world[0], this->height() - world[1] - 1);
}

QPointF VipVTKGraphicsView::transformToDoubleView(double* pt)
{
	d_data->coordinates->SetCoordinateSystemToWorld();
	d_data->coordinates->SetValue(pt);
	double* world = d_data->coordinates->GetComputedDoubleDisplayValue(d_data->renderer);
	return QPointF(world[0], this->height() - world[1] - 1);
}

QPointF VipVTKGraphicsView::transformToWorldXY(const QPoint& pt, double z)
{

	double pt0[3] = { 0, 0, z };
	double pt1[3] = { 1, 0, z };
	double pt2[3] = { 1, 1, z };
	double pt3[3] = { 0, 1, z };

	QPolygonF poly;
	poly << transformToDoubleView(pt0);
	poly << transformToDoubleView(pt1);
	poly << transformToDoubleView(pt2);
	poly << transformToDoubleView(pt3);

	QTransform tr;
	if (QTransform::quadToSquare(poly, tr)) {
		// bool invertible = false;
		// tr = tr.inverted(&invertible);
		// if (invertible)
		{
			QPointF res = tr.map(QPointF(pt));
			return res;
		}
	}
	return QPointF();
}

QPointF VipVTKGraphicsView::transformToWorldYZ(const QPoint& pt, double x)
{
	double pt0[3] = { x, 0, 0 };
	double pt1[3] = { x, 1, 0 };
	double pt2[3] = { x, 1, 1 };
	double pt3[3] = { x, 0, 1 };

	QPolygonF poly;
	poly << transformToDoubleView(pt0);
	poly << transformToDoubleView(pt1);
	poly << transformToDoubleView(pt2);
	poly << transformToDoubleView(pt3);

	QTransform tr;
	if (QTransform::squareToQuad(poly, tr)) {
		bool invertible = false;
		tr = tr.inverted(&invertible);
		if (invertible) {
			return tr.map(QPointF(pt));
		}
	}
	return QPointF();
}

QPointF VipVTKGraphicsView::transformToWorldXZ(const QPoint& pt, double y)
{
	double pt0[3] = { 0, y, 0 };
	double pt1[3] = { 1, y, 0 };
	double pt2[3] = { 1, y, 1 };
	double pt3[3] = { 0, y, 1 };

	QPolygonF poly;
	poly << transformToDoubleView(pt0);
	poly << transformToDoubleView(pt1);
	poly << transformToDoubleView(pt2);
	poly << transformToDoubleView(pt3);

	QTransform tr;
	if (QTransform::squareToQuad(poly, tr)) {
		bool invertible = false;
		tr = tr.inverted(&invertible);
		if (invertible) {
			return tr.map(QPointF(pt));
		}
	}
	return QPointF();
}

VipVTKWidget* VipVTKGraphicsView::widget() const
{
	return d_data->widget;
}
vtkLookupTable* VipVTKGraphicsView::table() const
{
	return d_data->lut;
}
vtkCubeAxesActor* VipVTKGraphicsView::cubeAxesActor() const 
{
	return d_data->cubeAxesActor;
}

vtkScalarBarActor* VipVTKGraphicsView::scalarBar() const
{
	return d_data->scalarBar;
}
// vtkLegendBoxActor * VipVTKGraphicsView::legend() const {return mLegend;}
VipBorderLegend* VipVTKGraphicsView::annotationLegend() const
{
	return d_data->annotationLegend;
}
OffscreenExtractContour* VipVTKGraphicsView::contours() const
{
	return const_cast<OffscreenExtractContour*>(&d_data->contours);
}
OffscreenExtractShapeStatistics* VipVTKGraphicsView::statistics() const
{
	return const_cast<OffscreenExtractShapeStatistics*>(d_data->stats);
}
VipInfoWidget* VipVTKGraphicsView::infos() const
{
	return const_cast<VipInfoWidget*>(d_data->infos);
}

void VipVTKGraphicsView::setDecimateOnMove(bool enable)
{
	d_data->decimateOnMove = enable;
	if (!enable) {
		for (auto it = d_data->decimated.begin(); it != d_data->decimated.end(); ++it) {
			it.value().plot->deleteLater();
		}
		d_data->decimated.clear();
	}
}
bool VipVTKGraphicsView::decimateOnMove() const
{
	return d_data->decimateOnMove;
}

/* void VipVTKGraphicsView::setLegendEntries(int count)
{
	mLegend->SetNumberOfEntries(count);
}

void VipVTKGraphicsView::setLegend(int index, const char * name, double * color)
{
	vtkSmartPointer<vtkCubeSource> legendBox = vtkSmartPointer<vtkCubeSource>::New();
	VIP_VTK_OBSERVER(legendBox);
	mLegend->SetEntry(index, legendBox->GetOutput(), name, color);
}

int VipVTKGraphicsView::legendEntries() const
{
	return mLegend->GetNumberOfEntries();
}
*/
void VipVTKGraphicsView::setTrackingEnable(bool enable)
{
	if (contours()->IsEnabled() != enable) {
		d_data->trackingEnabled = enable;
		if (enable)
			contours()->Reset();
		contours()->SetEnabled(enable);
		if (enable) {
			contours()->ForceUpdate();
			// Mark everything as modified
			for (int i = 0; i < d_data->renderers.size(); ++i) {
				d_data->renderers[i]->Modified();
				if (vtkCamera* cam = d_data->renderers[i]->GetActiveCamera())
					cam->Modified();
			}
			d_data->widget->renderWindow()->Modified();
			d_data->widget->interactor()->Modified();

			auto plots = objects();
			for (VipPlotVTKObject* pl : plots)
				pl->rawData().modified();
		}
		d_data->infos->setVisible(enable);

		// Update decimated models modified time.
		// That's because the extract contour update the models points time,
		// but we don't need to recompute the decimated models

		for (auto it = d_data->decimated.begin(); it != d_data->decimated.end(); ++it) {
			it.value().plot->rawData().modified();
		}
	}
}

bool VipVTKGraphicsView::trackingEnabled() const
{
	return d_data->trackingEnabled;
}

void VipVTKGraphicsView::setOrientationMarkerWidgetVisible(bool vis) 
{
	d_data->orientationAxes->GetOrientationMarker()->SetVisibility(vis);
	refresh();
}
bool VipVTKGraphicsView::orientationMarkerWidgetVisible() const
{
	return d_data->orientationAxes->GetOrientationMarker()->GetVisibility();
}

void VipVTKGraphicsView::setLighting(bool enable)
{
	if (d_data->hasLight != enable) {
		d_data->hasLight = enable;
		applyLighting();
	}
}
bool VipVTKGraphicsView::lighting() const
{
	return d_data->hasLight;
}

bool VipVTKGraphicsView::resetCameraEnabled() const 
{
	return d_data->resetCameraEnabled;
}

void VipVTKGraphicsView::setResetCameraEnabled(bool enable)
{
	d_data->resetCameraEnabled = enable;
}

void VipVTKGraphicsView::applyLighting()
{
	PlotVipVTKObjectList all = objects();
	//auto locks = vipLockVTKObjects(mDataObjects);
	for (auto& obj : all) {
		if (obj->hasActor())
			obj->actor()->GetProperty()->SetLighting(d_data->hasLight);
	}
	refresh();
}

void VipVTKGraphicsView::refresh()
{
	if (!d_data->inRefresh) {
		d_data->inRefresh = true;
		QMetaObject::invokeMethod(this, "immediateRefresh", Qt::QueuedConnection);
	}
}

void VipVTKGraphicsView::immediateRefresh()
{
	this->update();
	viewport()->update();
	d_data->inRefresh = false;
}

bool VipVTKGraphicsView::findPointAttributeBounds(const VipVTKObjectList& objs, VipVTKObject::AttributeType type, const QString& attribute, int component, double* min, double* max) const
{
	if (type == VipVTKObject::Unknown || type == VipVTKObject::Field)
		return false;

	*min = std::numeric_limits<double>::max();
	*max = -std::numeric_limits<double>::max();
	bool res{ false };
	VipSpinlock lock;

	const auto* objects = &objs;
	for (int i = 0; i < objs.size(); ++i) {
		
		if ((*objects)[i].data() && (*objects)[i].data()->IsA("vtkDataSet")) {
			vtkAbstractArray* array = nullptr;
			
			if(type ==VipVTKObject::Point)
				array = (*objects)[i].pointsAttribute(attribute);
			else
				array = (*objects)[i].cellsAttribute(attribute);

			if (array && array->IsA("vtkDataArray")) {
				vtkDataArray* data_array = static_cast<vtkDataArray*>(array);

				//mPool.start([&]() { 
					double range[2];
					data_array->GetRange(range, component);
					lock.lock();
					*min = qMin(*min, range[0]);
					*max = qMax(*max, range[1]);
					res = true;
					lock.unlock();
				//	});
				
			}
		}
	}
	//mPool.waitForDone();

	return res;
}

PlotVipVTKObjectList VipVTKGraphicsView::find(const QString& attribute, int component, const QString& value) const
{
	PlotVipVTKObjectList res;
	PlotVipVTKObjectList objs = objects();
	for (int i = 0; i < objs.size(); ++i) {
		vtkVariantList lst = objs[i]->rawData().fieldAttribute(attribute);
		if (component < lst.size() && value == lst[component].ToString().c_str())
			res.push_back(objs[i]);
	}

	return res;
}

VipPlotVTKObject* VipVTKGraphicsView::objectByName(const QString& name) const
{
	PlotVipVTKObjectList objs = objects();
	for (int i = 0; i < objs.size(); ++i) {
		if (objs[i]->dataName() == name)
			return objs[i];
	}
	return nullptr;
}

PlotVipVTKObjectList VipVTKGraphicsView::find(const QString& name) const
{
	PlotVipVTKObjectList res;
	PlotVipVTKObjectList objs = objects();
	for (int i = 0; i < objs.size(); ++i) {
		if (objs[i]->dataName() == name)
			res.append(objs[i]);
	}
	return res;
}

void VipVTKGraphicsView::computeVisualBounds(double bounds[6])
{
	bounds[0] = bounds[2] = bounds[4] = std::numeric_limits<double>::max();
	bounds[1] = bounds[3] = bounds[5] = -std::numeric_limits<double>::max();

	PlotVipVTKObjectList plots = objects();
	VipVTKObjectList objs = fromPlotVipVTKObject(plots);
	auto lst = vipLockVTKObjects(objs);

	for (int i = 0; i < plots.size(); ++i) {
		// if(mDataObjects[i]->actor()->GetVisibility())
		{
			double tmp[6];
			plots[i]->bounds(tmp);
			bounds[0] = qMin(bounds[0], tmp[0]);
			bounds[2] = qMin(bounds[2], tmp[2]);
			bounds[4] = qMin(bounds[4], tmp[4]);
			bounds[1] = qMax(bounds[1], tmp[1]);
			bounds[3] = qMax(bounds[3], tmp[3]);
			bounds[5] = qMax(bounds[5], tmp[5]);
		}
	}
}

double* VipVTKGraphicsView::computeVisualBounds()
{
	thread_local double bounds[6];
	computeVisualBounds(bounds);
	return bounds;
}

PlotVipVTKObjectList VipVTKGraphicsView::objects() const
{
	PlotVipVTKObjectList res;
	auto plots = this->area()->plotItems();
	for (VipPlotItem* it : plots)
		if (VipPlotVTKObject* o = qobject_cast<VipPlotVTKObject*>(it))
			if (!o->property("_vip_hidden").toBool())
				res.push_back(o);
	return res;
}

PlotVipVTKObjectList VipVTKGraphicsView::selectedObjects() const
{
	PlotVipVTKObjectList plots = objects();
	PlotVipVTKObjectList res;
	for (int i = 0; i < plots.size(); ++i)
		if (plots[i]->isSelected())
			res << plots[i];
	return res;
}
/* VipPlotVTKObject* VipVTKGraphicsView::underMouse(const QPoint& pt, int* pointId) const
{
	VipPlotVTKObject* obj = nullptr;
	vtkPointPicker* picker = vtkPointPicker::New();
	VIP_VTK_OBSERVER(picker);
	picker->SetTolerance(0.005);
	QPoint pos = d_data->widget->mapFrom(this, pt);
	pos.setY(d_data->widget->height() - pos.y());

	int res = picker->Pick(pos.x(), pos.y(), 0, d_data->renderer);
	if (res) {
		vtkActor* actor = picker->GetActor();
		if (pointId)
			*pointId = picker->GetPointId();

		PlotVipVTKObjectList plots = objects();
		for (int i = 0; i < plots.size(); ++i)
			if (plots[i]->actor() == actor) {
				obj = plots[i];
				break;
			}
	}

	picker->Delete();
	return obj;
}*/

vtkRenderWindow* VipVTKGraphicsView::renderWindow()
{
	return d_data->widget->renderWindow();
}

vtkRenderer* VipVTKGraphicsView::renderer()
{
	return d_data->renderer;
}

const QList<vtkRenderer*>& VipVTKGraphicsView::renderers() const 
{
	return d_data->renderers;
}

void VipVTKGraphicsView::setCurrentCamera(const VipFieldOfView& fov)
{
	fov.changePointOfView(renderWindow());
}
VipFieldOfView VipVTKGraphicsView::currentCamera() const
{
	VipFieldOfView fov;
	fov.importCamera(d_data->renderer->GetActiveCamera());
	//TEST: do it twice?
	fov.importCamera(d_data->renderer->GetActiveCamera());
	
	fov.name = "current";
	return fov;
}

QImage VipVTKGraphicsView::widgetContent(double* bounds)
{
	QMap<QGraphicsObject*, bool> visible;
	QList<QGraphicsItem*> items = (this->area()->scene()->items());
	QList<QPointer<QGraphicsObject>> objs;
	for (int i = 0; i < items.size(); ++i)
		if (items[i]->toGraphicsObject())
			objs.append(items[i]->toGraphicsObject());

	for (int i = 0; i < objs.size(); ++i) {
		if (QGraphicsObject* it = objs[i]) {
			if (!qobject_cast<VipPlotVTKObject*>(it) && !qobject_cast<VipAbstractPlotArea*>(it)) {
				visible[it] = it->isVisible();
				it->setVisible(false);
			}
		}
	}

	QImage img(this->size(), QImage::Format_ARGB32);
	{
		img.fill(qRgba(0, 0, 0, 0));
		QPainter p(&img);
		this->renderObject(&p, QPoint(0, 0), false);
	}
	if (bounds) {
		int left = qRound(bounds[0] * img.width());
		int right = qRound(bounds[2] * img.width());
		int top = qRound(bounds[1] * img.height());
		int bottom = qRound(bounds[3] * img.height());
		img = img.copy(QRect(left, top, right - left, bottom - top));
	}

	for (QMap<QGraphicsObject*, bool>::iterator it = visible.begin(); it != visible.end(); ++it) {
		it.key()->setVisible(it.value());
	}
	/*this->area()->legend()->setVisible(legend_vis);
	d_data->annotationLegend->setVisible(mAnnotationLegend_vis);*/
	return img;
}

VipVTKImage VipVTKGraphicsView::imageContent(int magnifier, double* bounds, int input_buffer_type)
{
	vtkSmartPointer<vtkWindowToImageFilter> windowToImageFilter = vtkSmartPointer<vtkWindowToImageFilter>::New();
	windowToImageFilter->SetInput(widget()->renderWindow());
	// TEST
	// widget()->GetRenderWindow()->SetAlphaBitPlanes(1);
	if (bounds)
		windowToImageFilter->SetViewport(bounds);
	windowToImageFilter->SetInputBufferType(input_buffer_type);
	// windowToImageFilter->SetMagnification(1); //set the resolution of the output image (3 times the current resolution of vtk render window)
	// TEST
	// windowToImageFilter->ShouldRerenderOn();
	windowToImageFilter->ReadFrontBufferOff(); // read from the back buffer
	windowToImageFilter->Modified();
	windowToImageFilter->Update();
	VipVTKImage data(windowToImageFilter->GetOutput());

#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
	data = data.mirrored(false, true);
#else
	data = data.flipped(Qt::Vertical);
#endif
	return data;
}

bool VipVTKGraphicsView::axesVisible() const
{
	return d_data->cubeAxesActor->GetVisibility();
}

void VipVTKGraphicsView::setAxesVisible(bool visible)
{
	if (visible)
		computeAxesBounds();

	d_data->cubeAxesActor->SetVisibility(visible);
	refresh();
}

void VipVTKGraphicsView::computeBounds(double *bounds) 
{
	auto plots = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject( plots));

	vtkRendererCollection* col = widget()->renderWindow()->GetRenderers();
	col->InitTraversal();
	vtkRenderer* ren = col->GetNextItem();
	ren->ComputeVisiblePropBounds(bounds);
	while (vtkRenderer* tmp = col->GetNextItem()) {
		double _bounds[6];
		tmp->ComputeVisiblePropBounds(_bounds);
		if (_bounds[0] < _bounds[1]) {
			bounds[0] = qMin(bounds[0], _bounds[0]);
			bounds[1] = qMax(bounds[1], _bounds[1]);
			bounds[2] = qMin(bounds[2], _bounds[2]);
			bounds[3] = qMax(bounds[3], _bounds[3]);
			bounds[4] = qMin(bounds[4], _bounds[4]);
			bounds[5] = qMax(bounds[5], _bounds[5]);
		}
	}
}

void VipVTKGraphicsView::resetCamera()
{
	if (!d_data->resetCameraEnabled)
		return;

	if (!d_data->widget->isValid()) {
		QMetaObject::invokeMethod(this, "resetCamera", Qt::QueuedConnection);
		return;
	}

	auto plots = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(plots));

	double bounds[6];
	computeBounds(bounds);

	vtkRendererCollection* col = widget()->renderWindow()->GetRenderers();
	col->InitTraversal();
	while (vtkRenderer* tmp = col->GetNextItem())
		tmp->ResetCamera(bounds);

	d_data->widget->applyCameraToAllLayers();
	refresh();
}

void VipVTKGraphicsView::colorMapModified()
{
	// set the title(optional)
	scalarBar()->SetTitle(area()->colorMapAxis()->title().text().toLatin1().data());

	// set the colors. We want 200 different colors.
	VipInterval interval = area()->colorMapAxis()->gripInterval().normalized();
	double min = interval.minValue();
	double max = interval.maxValue();
	int num_colors = table()->GetNumberOfColors();
	double step = 1;
	if (num_colors > 1)
		step = (max - min) / double(num_colors - 1);

	// set the values for the grip interval
	for (int i = 0; i < num_colors; ++i) {
		double c[4];
		QColor color = area()->colorMapAxis()->colorMap()->rgb(interval, min + i * step); // stops[i].second;
		c[0] = color.redF();
		c[1] = color.greenF();
		c[2] = color.blueF();
		c[3] = 1;
		table()->SetTableValue(i, c);
	}

	colorMapDivModified();
}

void VipVTKGraphicsView::colorMapDivModified()
{
	if (!d_data->dirtyColorMapDiv) {
		d_data->dirtyColorMapDiv = true;
		QMetaObject::invokeMethod(this, "computeColorMap", Qt::QueuedConnection);
	}
}

void VipVTKGraphicsView::computeColorMap()
{
	d_data->dirtyColorMapDiv = false;
	VipInterval interval = area()->colorMapAxis()->gripInterval().normalized();
	double min = interval.minValue();
	double max = interval.maxValue();
	// set the min/max to all data

	scalarBar()->GetLookupTable()->SetRange(min, max);
	auto lst = objects();
	for (int i = 0; i < lst.size(); ++i) {
		if (vtkMapper* mapper = lst[i]->mapper()) {
			if (mapper->GetLookupTable())
				mapper->SetScalarRange(min, max);
		}
	}
}


void VipVTKGraphicsView::mousePressEvent(QMouseEvent* event)
{
	// TEST
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));

	if (d_data->trackingEnabled) {
		d_data->contours.SetState(OffscreenExtractContour::Disable);
		//d_data->contours.SetEnabled(false);
	}
	
	QList<QGraphicsItem*> items = this->items(event->pos());
	d_data->itemUnderMouse = nullptr;
	for (int i = 0; i < items.size(); ++i) {
		QGraphicsItem* item = items[i];
		if (item != this->area()->rubberBand() && item != this->area()->canvas() && item != this->area()) {
			d_data->itemUnderMouse = item;
			d_data->objectUnderMouse = item->toGraphicsObject();
			break;
		}
	}

	// send the event to the VipImageWidget2D and the viewport
	VipImageWidget2D::mousePressEvent(event);
	static_cast<VipVTKWidget*>(viewport())->event(event);

	// display the simplified version of each CAD object IF no attribute is displayed
	// if(GetPropertyWidget()->currentAttribute() == "None" || GetPropertyWidget()->currentAttribute().isEmpty())
	/*if(!has_item)
	{
		for(int i=0; i < mDataObjects.size(); ++i)
			mDataObjects[i]->SetSimplified(true);
	}*/

	setProperty("_vip_decimate", d_data->decimateOnMove);
}
// overloaded mouse move handler
void VipVTKGraphicsView::mouseMoveEvent(QMouseEvent* event)
{
	// lock the data objects
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));

	


	// send the event to the VipImageWidget2D.
	// If the event is not accepted, forward it tot the viewport.
	VipImageWidget2D::mouseMoveEvent(event);
	QObject* plot_object = d_data->objectUnderMouse ? qobject_cast<VipPlotVTKObject*>(d_data->objectUnderMouse) : nullptr;
	if (!plot_object)
		plot_object = d_data->objectUnderMouse ? qobject_cast<VipPlotFieldOfView*>(d_data->objectUnderMouse) : nullptr;
	if (!this->area()->rubberBand()->filter() &&
	    (!VipPlotItem::eventAccepted() || !d_data->itemUnderMouse || plot_object)) // If no QGraphicsItem accepted this mouse move event, forward it to the viewport
	{
		if (event->buttons() & Qt::LeftButton) {
			// compute decimated
			if (d_data->decimateOnMove && property("_vip_decimate").toBool()) {
				setProperty("_vip_decimate", false);
				computeDecimatedObjects(d_data->decimated, lst);
			}
			static_cast<VipVTKWidget*>(viewport())->event(event);
		}
	}

	// reset the event accepted flag to true
	VipPlotItem::setEventAccepted(true);

	if (event->buttons() & Qt::LeftButton) {
		/* if (d_data->trackingEnabled) {
			d_data->contours.MightNeedReset();
			d_data->contours.SetEnabled(false);
		}*/
	}
	else {

		// If we are just moving the mouse without clicking (hovering),
		// try to detect if the contour extractor is corrupted.
		// For that check if a plot object is hovered while the
		// contour extractor returns and empty object id.
		if (d_data->trackingEnabled) {
			
			/* if (d_data->contours.ObjectId(event->pos()) < 0) {
				QPointF scene = mapToScene(event->pos());
				bool has_hover = false;
				for (const auto* plot : lst) {
					if (plot->isHovering())
						has_hover = true;
				}
				if (has_hover) {
					qint64 time = QDateTime::currentMSecsSinceEpoch();
					if (d_data->lastFailContour && time - d_data->lastFailContour > 1000) {

						// likely corrupted
						setTrackingEnable(false);
						setTrackingEnable(true);
						d_data->lastFailContour = 0;
						vip_debug("Recompute corrupted contours!");
					}
					else
						d_data->lastFailContour = time;
				}
			}*/
		}
		
	}

	refresh();
	Q_EMIT mouseMove(event->pos());
}
// overloaded mouse release handler
void VipVTKGraphicsView::mouseReleaseEvent(QMouseEvent* event)
{
	// TEST
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));

	if (d_data->decimateOnMove)
		for (auto it = d_data->decimated.begin(); it != d_data->decimated.end(); ++it) {
			it.key()->actor()->SetVisibility(it.key()->isVisible());
			it.value().plot->actor()->SetVisibility(false);
		}

	// Update contours when releasing mouse
	/* if (d_data->trackingEnabled) {
		d_data->contours.SetEnabled(true);
		d_data->contours.ForceUpdate();
	}*/
	d_data->contours.SetState(d_data->trackingEnabled ? OffscreenExtractContour::ExtractAll : OffscreenExtractContour::ExtractShape);

	// send the event to the VipImageWidget2D and the viewport
	VipImageWidget2D::mouseReleaseEvent(event);
	static_cast<VipVTKWidget*>(viewport())->event(event);


	refresh();
}
// overloaded key press handler
void VipVTKGraphicsView::keyPressEvent(QKeyEvent* event)
{
	// TEST
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));


	if (d_data->itemUnderMouse) {
		VipImageWidget2D::keyPressEvent(event);
		if (!VipPlotItem::eventAccepted())
			static_cast<VipVTKWidget*>(viewport())->event(event);
	}
	else
		static_cast<VipVTKWidget*>(viewport())->event(event);
	//viewport()->update();
	refresh();
}
// overloaded key release handler
void VipVTKGraphicsView::keyReleaseEvent(QKeyEvent* event)
{
	// TEST
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));


	if (d_data->itemUnderMouse) {
		VipImageWidget2D::keyReleaseEvent(event);
		if (!VipPlotItem::eventAccepted())
			static_cast<VipVTKWidget*>(viewport())->event(event);
	}
	else
		static_cast<VipVTKWidget*>(viewport())->event(event);
	//viewport()->update();
	refresh();
}
// overload wheel mouse event
void VipVTKGraphicsView::wheelEvent(QWheelEvent* event)
{
	// TEST
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));

	if (d_data->trackingEnabled) {
		d_data->contours.MightNeedReset();
	}

	if (d_data->itemUnderMouse) {
		VipImageWidget2D::wheelEvent(event);
		if (!event->isAccepted()) //! VipPlotItem::eventAccepted())
			static_cast<VipVTKWidget*>(viewport())->event(event);
	}
	else
		static_cast<VipVTKWidget*>(viewport())->event(event);
	//viewport()->update();
	refresh();

	QMetaObject::invokeMethod(this, "touchCamera", Qt::QueuedConnection);
}

void VipVTKGraphicsView::paintEvent(QPaintEvent* evt)
{
	//glEnable(GL_MULTISAMPLE);
	//glEnable(GL_LINE_SMOOTH);
	//glEnable(GL_POLYGON_SMOOTH);

	// qint64 st = QDateTime::currentMSecsSinceEpoch();
	VipImageWidget2D::paintEvent(evt);
	// qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	// printf("VipVTKGraphicsView::paintEvent: %i ms\n", (int)el);
}

void VipVTKGraphicsView::drawBackground(QPainter* p, const QRectF&)
{
	// TEST
#if QT_VERSION >= 0x040600
	p->beginNativePainting();
#endif
	d_data->widget->paintGL(); 
#if QT_VERSION >= 0x040600
	p->endNativePainting();
#endif

	/*
	auto lst = objects();
	auto lockers = vipLockVTKObjects(fromPlotVipVTKObject(lst));


	// Force rendering on background drawing.
	// Mandatory to avoid flickering when rendering 3D objects with 2D plotting items.
#if QT_VERSION >= 0x040600
	p->beginNativePainting();
#endif
	d_data->widget->paintGL();//TEST
	//d_data->widget->renderWindow()->GetInteractor()->Render();
#if QT_VERSION >= 0x040600
	p->endNativePainting();
#endif
*/
}

void VipVTKGraphicsView::touchCamera()
{
	this->d_data->renderer->GetActiveCamera()->Modified();
	QList<VipPlotVTKObject*> items = vipCastItemList<VipPlotVTKObject*>(this->scene()->items());
	for (int i = 0; i < items.size(); ++i) {
		items[i]->geometryChanged();
	}
}


/* void VipVTKGraphicsView::drawForeground(QPainter*, const QRectF&) {

}*/

void VipVTKGraphicsView::initializeViewRendering()
{
	// Force a resize event to intialize the opengl widget (or black screen)
	if (!d_data->initialized)
		QMetaObject::invokeMethod(this, "sendFakeResizeEvent", Qt::QueuedConnection);
}
void VipVTKGraphicsView::sendFakeResizeEvent()
{
	if (!d_data->initialized) {
		d_data->initialized = true;
		QSize min_size = this->minimumSize();
		QSize new_min_size = this->size() + QSize(1, 0);
		this->setMinimumSize(new_min_size);
		this->setMinimumSize(min_size);
	}
}

void VipVTKGraphicsView::resizeEvent(QResizeEvent* event)
{
	// give the same size to the scene that his widget has
	// TEST: comment
	// if (scene())
	//{
	//	scene()->setSceneRect(QRect(QPoint(0, 0), event->size()));
	//}
	VipImageWidget2D::resizeEvent(event);
	d_data->widget->renderWindow()->SetSize(event->size().width(), event->size().height());
}

void VipVTKGraphicsView::computeAxesBounds()
{
	double extend_factor = 0;//0.2;
	double bounds[6];
	double center[3];
	//double end_point[3];
	computeVisualBounds(bounds);

	if (extend_factor != 0) {

		for (int i = 0; i < 3; ++i) {
			center[i] = (bounds[i * 2] + bounds[i * 2 + 1]) / 2;

			double extent = bounds[i * 2 + 1] - bounds[i * 2];
			bounds[i * 2] = bounds[i * 2] - extent * extend_factor;
			bounds[i * 2 + 1] = bounds[i * 2 + 1] + extent * extend_factor;
		}
	}

	VipLinearScaleEngine engine;
	double step_size = 0;
	engine.autoScale(10, bounds[0], bounds[1], step_size);
	step_size = 0;
	engine.autoScale(10, bounds[2], bounds[3], step_size);
	step_size = 0;
	engine.autoScale(10, bounds[4], bounds[5], step_size);
	
	d_data->cubeAxesActor->SetBounds(bounds);
	d_data->cubeAxesActor->Modified();

	//TEST
	//mGridAxesActor->SetGridBounds(bounds);

}

static VipArchive& operator<<(VipArchive& arch, const VipVTKGraphicsView*)
{
	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipVTKGraphicsView* view)
{
	view->area()->colorMapAxis()->setVisible(false);
	return arch;
}

/* static QString shapeStatistics(VTK3DPlayer* pl, const VipShapeList& lst)
{
	pl->view()->statistics()->SetShapes(lst);
	pl->view()->statistics()->Update();
	return pl->view()->statistics()->GetHtmlDescription();
}

#include "VipDrawShape.h"
*/
static int registerOperators()
{
	// vipFDShapeStatistics().append<QString(VTK3DPlayer*, const VipShapeList &)>(shapeStatistics);
	vipRegisterArchiveStreamOperators<VipVTKGraphicsView*>();
	return 0;
}

static int _registerOperators = registerOperators();