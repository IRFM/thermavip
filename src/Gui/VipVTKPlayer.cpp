#include "VipVTKPlayer.h"
#include "VipVTKObjectProcessing.h"
#include "VipDisplayVTKObject.h"
#include "VipFieldOfViewEditor.h"
//#include "OffscreenFOVOverlapping.h"
//#include "OffscreenMapping.h"
#include "p_QVTKBridge.h"
#include "VipVTKDevices.h"
#include "VipVTKActorParametersEditor.h"
#include "VipVTKGraphicsView.h"
#include "VipStandardWidgets.h"

#include "VipDisplayArea.h"
#include "VipDragWidget.h"
#include "VipGui.h"
#include "VipLegendItem.h"
#include "VipLogging.h"
#include "VipMimeData.h"
#include "VipPlayer.h"
#include "VipPlotGrid.h"
#include "VipPlotMimeData.h"
#include "VipPlotSpectrogram.h"
#include "VipProcessingObjectEditor.h"
#include "VipProcessingObjectInfo.h"
#include "VipProgress.h"
#include "VipRecordToolWidget.h"
#include "VipSet.h"
#include "VipStandardWidgets.h"


//#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkLookupTable.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyLine.h>
#include <vtkProperty.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRendererCollection.h>
#include <vtkStringArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkWindowToImageFilter.h>

#include <QApplication>
#include <QBoxLayout>
#include <QCoreApplication>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QStyleOption>
#include <QTextStream>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

class ApplyMappingDialog::PrivateData
{
public:
	QRadioButton applyOnInput;
	QRadioButton createNewObject;

	QComboBox box;
	QLabel no_player;
	QList<VipVideoPlayer*> players;

	QPushButton ok;
	QPushButton cancel;
};

ApplyMappingDialog::ApplyMappingDialog(QWidget* parent)
  : QDialog(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QHBoxLayout* boxlay = new QHBoxLayout();
	boxlay->addWidget(&d_data->box);
	boxlay->addWidget(&d_data->no_player);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->addLayout(boxlay);
	vlay->addWidget(&d_data->applyOnInput);
	vlay->addWidget(&d_data->createNewObject);

	QHBoxLayout* hlay = new QHBoxLayout();
	hlay->addStretch(1);
	hlay->addWidget(&d_data->ok);
	hlay->addWidget(&d_data->cancel);

	vlay->addWidget(VipLineWidget::createHLine());
	vlay->addLayout(hlay);

	setLayout(vlay);

	d_data->applyOnInput.setText("Apply mapping on input CAD objects");
	d_data->applyOnInput.setChecked(true);
	d_data->createNewObject.setText("Create a new CAD object");
	d_data->box.setToolTip("Select the video player to map on the camera");

	d_data->no_player.setText("No video player available for the mapping...");
	d_data->no_player.hide();

	d_data->ok.setText("Ok");
	d_data->cancel.setText("Cancel");

	// add the video players within the current workspace
	VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea();
	QList<VipVideoPlayer*> players = VipUniqueId::objects<VipVideoPlayer>();
	QStringList title;
	for (int i = 0; i < players.size(); ++i) {
		if (!qobject_cast<VipVTKPlayer*>(players[i]) && players[i]->parentDisplayArea() == area) {
			title << VipDragWidget::fromChild(players[i])->windowTitle();
			d_data->players.append(players[i]);
			d_data->box.addItem(title.last());
		}
	}

	d_data->box.setVisible(d_data->players.size());
	d_data->no_player.setVisible(!d_data->players.size());
	// d_data->ok.setVisible(d_data->players.size());

	setMinimumWidth(300);
	setWindowTitle("Video mapping options");

	connect(&d_data->ok, SIGNAL(clicked(bool)), this, SLOT(accept()));
	connect(&d_data->cancel, SIGNAL(clicked(bool)), this, SLOT(reject()));
}

ApplyMappingDialog::~ApplyMappingDialog()
{
}

void ApplyMappingDialog::SetVideoPlayer(VipVideoPlayer* pl)
{
	int index = d_data->players.indexOf(pl);
	if (index >= 0) {
		d_data->box.setCurrentIndex(index);
	}
}
VipVideoPlayer* ApplyMappingDialog::VideoPlayer() const
{
	if (d_data->box.count() > 0)
		return d_data->players[d_data->box.currentIndex()];
	else
		return nullptr;
}

void ApplyMappingDialog::SetConstructNewObject(bool enable)
{
	d_data->createNewObject.setChecked(enable);
	d_data->createNewObject.setChecked(enable);
}
bool ApplyMappingDialog::ConstructNewObject() const
{
	return d_data->createNewObject.isChecked();
}

class VipFOVItem::PrivateData
{
public:
	QPointer<VipVTKGraphicsView> view;
	QPointer<VipPlotFieldOfView> plotFOV;
	QPointer<VipPlotVTKObject> cam_path; // the path of the camera (in case of dynamic camera like a drone)
	VipPlotVTKObject fov_pyramid; // a simple pyramid to display the FOV
	VipPlotVTKObject optical_axis;
	; // a simple pyramid to display the FOV

	QToolBar* toolBar;
	QToolButton* tool_show_visible_points_in_fov;
	QAction* show_visible_points_in_fov;
	QAction* map_image;
	QAction* show_fov_pyramid;
	QAction* import_camera;

	//QPointer<OffscreenMappingToInputData> mapping;
	//QPointer<ExtractSpatialCalibration> displaySC;
	VipFieldOfView last;

	bool once;
};

VipFOVItem::VipFOVItem(VipVTKGraphicsView* v, QTreeWidgetItem* parent)
  : QTreeWidgetItem(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->view = v;
	d_data->once = false;
	

	d_data->toolBar = new QToolBar();
	d_data->toolBar->setIconSize(QSize(18, 18));
	d_data->toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	d_data->toolBar->setMaximumHeight(22);
	d_data->toolBar->setMaximumWidth(150);
	//d_data->toolBar->setStyleSheet("QToolBar {border-style: flat;}");

	d_data->show_fov_pyramid = d_data->toolBar->addAction(vipIcon("fov_displayed.png"), QString("Show/hide camera field of view"));
	d_data->show_fov_pyramid->setCheckable(true);

	d_data->tool_show_visible_points_in_fov = new QToolButton();
	d_data->tool_show_visible_points_in_fov->setToolTip("Extract/remove camera visible pixels");
	d_data->tool_show_visible_points_in_fov->setIcon(vipIcon("mapping.png"));
	d_data->tool_show_visible_points_in_fov->setCheckable(true);
	d_data->tool_show_visible_points_in_fov->setPopupMode(QToolButton::MenuButtonPopup);
	QMenu* menu = new QMenu();
	menu->setToolTipsVisible(true);

	d_data->show_visible_points_in_fov = menu->addAction(vipIcon("inside_points.png"), "Extract/remove camera visible pixels");
	d_data->show_visible_points_in_fov->setCheckable(true);
	d_data->show_visible_points_in_fov->setVisible(false); // disable this options for now

	d_data->map_image = menu->addAction("Map/Unmap video on camera...");
	d_data->map_image->setCheckable(true);
	d_data->tool_show_visible_points_in_fov->setMenu(menu);
	d_data->toolBar->addWidget(d_data->tool_show_visible_points_in_fov);

	d_data->import_camera = d_data->toolBar->addAction(vipIcon("import.png"), QString("Import current camera (position, rotation and optical axis) into this field of view"));

	/*d_data->mappingOptions = new OffscreenMappingEditor();
	QWidgetAction * act = new QWidgetAction(menu);
	act->setDefaultWidget(d_data->mappingOptions);
	d_data->mappingOptionsAction = act;
	menu->addAction(act);
	d_data->mappingOptionsAction->setVisible(false);*/

	menu->addSeparator();

	QAction* save_calibration = menu->addAction("Save spatial calibration...");
	save_calibration->setToolTip("Save, for each camera pixel:<br>"
				     "- <b> The associated 3D coordinates, </b><br>"
				     "- <b> The distance to the camera pupil, </b><br>"
				     "- <b> The associated CAD object identifier and name, </b><br>"
				     "- <b> The pixel surface, </b><br>"
				     "- Any additional CAD models attributes.");
	connect(save_calibration, SIGNAL(triggered(bool)), this, SLOT(saveSpatialCalibrationFile()));

	QAction* display_calibration = menu->addAction("Display spatial calibration");
	display_calibration->setToolTip("Display a multi-channel image representing the camera mapping results:<br>"
					"- <b> The associated 3D coordinates, </b><br>"
					"- <b> The distance to the camera pupil, </b><br>"
					"- <b> The associated CAD object identifier and name, </b><br>"
					"- <b> The pixel surface, </b><br>"
					"- Any additional CAD models attributes.");
	connect(display_calibration, SIGNAL(triggered(bool)), this, SLOT(displaySpatialCalibration()));

	if (this->treeWidget())
		this->treeWidget()->setItemWidget(this, 0, d_data->toolBar);

	//connect(d_data->view, SIGNAL(dataChanged()), this, SLOT(newData()));
	connect(d_data->show_fov_pyramid, SIGNAL(triggered(bool)), this, SLOT(showFOVPyramid(bool)));
	connect(d_data->show_visible_points_in_fov, SIGNAL(triggered(bool)), this, SLOT(showVisiblePointsInFOVPyramid(bool)));
	connect(d_data->tool_show_visible_points_in_fov, SIGNAL(clicked(bool)), this, SLOT(applyImageMapping(bool)));
	connect(d_data->map_image, SIGNAL(triggered(bool)), this, SLOT(applyImageMapping(bool)));
	connect(d_data->import_camera, SIGNAL(triggered(bool)), this, SLOT(importCamera()));

	this->setToolTip(1, QString());
	this->setText(1,QString());
	this->setFlags(Qt::ItemIsDropEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled);
}

VipFOVItem::~VipFOVItem()
{
	// if (ImageMapper * m = qobject_cast<ImageMapper*>(d_data->mapper))
	//	delete m;

	clear();

	if (d_data->plotFOV) {
		d_data->plotFOV->deleteLater();
	}

	//if (d_data->mapping)
	//	d_data->mapping->deleteLater();

	//if (d_data->displaySC)
	//	d_data->displaySC->deleteLater();

}

/**
Build the mapping result for given item and possible input image
*/
static OffscreenMappingToInputData* buildMapping(VipFOVItem* item, VipOutput* image)
{
	return nullptr;
	/* OffscreenMappingToInputData* mapping = new OffscreenMappingToInputData();
	mapping->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	mapping->setToInputData(false);

	// connect the FOV to the OffscreenMappingProcessing and MapImage
	VipAnyData fov(QVariant::fromValue(item->fieldOfViewItem()), VipInvalidTime);
	mapping->inputAt(0)->setData(fov);

	// set the input VipVTKObject
	VipMultiInput* multi = mapping->topLevelInputAt(1)->toMultiInput();
	multi->clear();
	VipVTKObjectList allData = item->view()->objects();
	VipVTKObjectList fileData;
	for (int i = 0; i < allData.size(); ++i)
		if (allData[i]->isFileData())
			fileData << allData[i];
	for (int i = 0; i < fileData.size(); ++i) {
		multi->add();
		if (VipPlotItem* item = fileData[i]->plotObject())
			if (VipDisplayObject* disp = item->property("VipDisplayObject").value<VipDisplayObject*>())
				if (VipOutput* output = disp->inputAt(0)->connection()->source())
					multi->at(multi->count() - 1)->setData(output->data());
	}

	if (image)
		mapping->topLevelInputAt(2)->toInput()->setData(image->data());

	// apply
	mapping->update(true);

	return mapping;*/
}

OffscreenMappingToInputData* VipFOVItem::buildMapping(bool createNewObject, VipOutput* image)
{

	/* if (!d_data->mapping) {
		// TEST parent and remove name
		d_data->mapping = new OffscreenMappingToInputData(VipVTKPlayer::fromChild(this->view())->processingPool()); //(this);
															   // d_data->mapping->setObjectName("OffscreenMapping");
	}

	// disable the processings to avoid triggering an update
	d_data->mapping->setEnabled(false);
	d_data->mapping->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
	d_data->mapping->setToInputData(!createNewObject);

	//
	// build all connections
	//

	// connect the FOV to the OffscreenMappingProcessing and MapImage
	if (d_data->display && d_data->display->inputAt(0)->connection()->source()) {
		VipOutput* output = d_data->display->inputAt(0)->connection()->source();
		output->setConnection(d_data->mapping->inputAt(0));
	}

	// connect the input VipVTKObject
	VipMultiInput* multi = d_data->mapping->topLevelInputAt(1)->toMultiInput();
	multi->clear();
	VipVTKObjectList allData = d_data->view->objects();
	VipVTKObjectList fileData;
	for (int i = 0; i < allData.size(); ++i)
		if (allData[i]->isFileData())
			fileData << allData[i];

	for (int i = 0; i < fileData.size(); ++i) {
		multi->add();
		if (VipPlotItem* item = fileData[i]->plotObject())
			if (VipDisplayObject* disp = item->property("VipDisplayObject").value<VipDisplayObject*>())
				if (VipOutput* output = disp->inputAt(0)->connection()->source())
					output->setConnection(multi->at(multi->count() - 1)); // set the connection (if any)
	}

	// enable the processings
	d_data->mapping->setEnabled(true);

	//
	// set input data in addition to the connections (to trigger a processing)
	//

	// d_data->mapping->setScheduleStrategy(VipProcessingObject::Asynchronous,false);
	d_data->mapping->setScheduleStrategy(VipProcessingObject::AcceptEmptyInput, true);

	// d_data->mapping->setScheduleStrategy(VipProcessingObject::AllInputs, image != nullptr);
	if (image) {
		d_data->mapping->topLevelInputAt(2)->toInput()->setConnection(image);
		d_data->mapping->topLevelInputAt(2)->toInput()->setData(image->data());

		// enable all sources of image
		QList<VipProcessingObject*> sources = image->parentProcessing()->allSources() << image->parentProcessing();
		for (int i = 0; i < sources.size(); ++i) {
			if (!sources[i]->isEnabled()) {
				sources[i]->setEnabled(true);
				if (VipIODevice* dev = qobject_cast<VipIODevice*>(sources[i]))
					if (VipProcessingPool* pool = sources.first()->parentObjectPool())
						dev->read(pool->time(), true);
			}
		}
		image->parentProcessing()->wait(true);
	}
	else
		d_data->mapping->topLevelInputAt(2)->toInput()->clearConnection();
	// d_data->mapImage->setScheduleStrategy(VipProcessingObject::Asynchronous, false);

	// set the input VipVTKObject
	for (int i = 0; i < fileData.size(); ++i)
		d_data->mapping->topLevelInputAt(1)->toMultiInput()->at(i)->setData(fileData[i]);

	// set the input FOV
	d_data->mapping->inputAt(0)->setData(d_data->fov);

	// reload mapping (which will apply MapImage)
	d_data->mapping->update(true);

	d_data->mapping->setScheduleStrategy(VipProcessingObject::Asynchronous, true);*/

	/*d_data->mappingOptionsAction->setVisible(true);
	d_data->mappingOptions->setOffscreenMapping(d_data->mapping);
	d_data->mappingOptions->setVisible(true);*/

	return nullptr; // d_data->mapping;
}

void VipFOVItem::clear()
{
	if (!d_data->view)
		return;

	if (d_data->fov_pyramid.rawData()) {
		d_data->view->renderer()->RemoveActor(d_data->fov_pyramid.actor());
		d_data->fov_pyramid.actor()->SetVisibility(0);
	}
	if (d_data->optical_axis.rawData()) {
		d_data->view->renderer()->RemoveActor(d_data->optical_axis.actor());
		d_data->optical_axis.actor()->SetVisibility(0);
	}
	if (d_data->cam_path) {
		d_data->cam_path->deleteLater();
		d_data->cam_path = nullptr;
	}

	VipFieldOfView fov;
	if (VipPlotFieldOfView* plot = d_data->plotFOV)
		fov = plot->rawData();

	auto plots = d_data->view->find("FOV section", 0, fov.name);
	for (auto* pl : plots)
		pl->deleteLater();
	plots = d_data->view->find("FOV points", 0,fov.name);
	for (auto* pl : plots)
		pl->deleteLater();

	d_data->fov_pyramid.setRawData(VipVTKObject());
	d_data->optical_axis.setRawData(VipVTKObject());
}

void VipFOVItem::newData()
{
	/* if (!d_data->view)
		return;

	bool pyramid_visible = (d_data->fov_pyramid.rawData() && d_data->fov_pyramid.actor()->GetVisibility());

	if (d_data->fov_pyramid.rawData())
		d_data->view->renderer()->RemoveActor(d_data->fov_pyramid.actor());
	if (d_data->optical_axis.rawData())
		d_data->view->renderer()->RemoveActor(d_data->optical_axis.actor());

	d_data->fov_pyramid.setRawData(VipVTKObject());
	d_data->optical_axis.setRawData(VipVTKObject());
	*/
	if (d_data->plotFOV)
		setFieldOfView(d_data->plotFOV->rawData());

	/* if (pyramid_visible) {
		FOVPyramid()->actor()->SetVisibility(1);
		opticalAxis()->actor()->SetVisibility(1);
	}*/
}

VipVTKGraphicsView* VipFOVItem::view() const
{
	return d_data->view;
}

void VipFOVItem::setFieldOfView(const VipFieldOfView& f)
{
	if (d_data->plotFOV)
		if (VipFOVSequence* seq = source()) {
			if (seq->size() > 1 && !d_data->cam_path)
				// reset plotFOV to build camera path
				setPlotFov(d_data->plotFOV);
		}

	bool need_update_visibility = FOVPyramid()->isVisible() != (bool)FOVPyramid()->actor()->GetVisibility();
	if (!need_update_visibility)
		if (!d_data->view || f == d_data->last) {
			return;
		}

	d_data->last = f;

	this->setToolTip(1, f.print());
	this->setText(1, f.name);

	bool pyramid_visible = (d_data->fov_pyramid.isVisible());

	if (d_data->cam_path) {
		auto path = d_data->cam_path->rawData();
		QString name = f.name + " path";
		if (name != path.dataName()) {
			path.setDataName(name);
			d_data->cam_path->setRawData(path);
		}
	}

	// update fov pyramid and optical axis
	buildPyramid();
	
	if (pyramid_visible) {
		FOVPyramid()->setVisible(true);
		opticalAxis()->setVisible(true);
		FOVPyramid()->rawData().modified();
		opticalAxis()->rawData().modified();

		d_data->view->widget()->renderWindow()->Modified();
		d_data->view->renderer()->GetActiveCamera()->Modified();
		d_data->view->refresh();
	}

	// update the view if required
	if (this->treeWidget())
		if (VipFOVTreeWidget* tree = qobject_cast<VipFOVTreeWidget*>(this->treeWidget()->parentWidget())) {
			if (tree->currentFieldOfViewItem() == this && !d_data->view->widget()->cameraUserMoved())
				this->moveCamera();
		}

	//Q_EMIT changed();
}

void VipFOVItem::updateColor()
{
	QColor c = d_data->plotFOV ? d_data->plotFOV->selectedColor() : QColor(Qt::red);
	if (d_data->fov_pyramid.rawData()) {
		d_data->fov_pyramid.setColor(c);
		d_data->fov_pyramid.setSelectedColor(c);
	}
	if (d_data->optical_axis.rawData()) {
		d_data->optical_axis.setColor(c);
		d_data->optical_axis.setSelectedColor(c);
	}
	if (d_data->cam_path) {
		d_data->cam_path->setColor(c);
		d_data->cam_path->setSelectedColor(c);
	}
}

/*
void VipFOVItem::SetMapper(ImageMapper * image_mapper)
{
	if (!d_data->view)
		return;

	//remove previous one
	if (ImageMapper * m = qobject_cast<ImageMapper*>(this->d_data->mapper))
		delete m;

	if (image_mapper)
	{
		d_data->mapper = image_mapper;
		image_mapper->setParent(this);
		image_mapper->setGraphicsView(d_data->view);
		image_mapper->setFOV(this->fieldOfViewItem());
	}
}

ImageMapper * VipFOVItem::mapper() const
{
	return d_data->mapper;
}
*/
void VipFOVItem::setPlotFov(VipPlotFieldOfView* p)
{
	if (p != d_data->plotFOV) {
		if (d_data->plotFOV) {
			disconnect(d_data->plotFOV, SIGNAL(dataChanged()), this, SLOT(newData()));
			disconnect(d_data->plotFOV, SIGNAL(colorChanged()), this, SLOT(updateColor()));
			disconnect(d_data->plotFOV, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
		}
		d_data->plotFOV = p;
		if (p) {
			connect(d_data->plotFOV, SIGNAL(dataChanged()), this, SLOT(newData()));
			connect(d_data->plotFOV, SIGNAL(colorChanged()), this, SLOT(updateColor()));
			connect(d_data->plotFOV, SIGNAL(destroyed(QObject*)), this, SLOT(deleteLater()));
			setFieldOfView(p->rawData());
		}
		else
			return;
	}

	if (VipDisplayObject* display = p->property("VipDisplayObject").value<VipDisplayObject*>())
		if (VipProcessingPool* pool = display->parentObjectPool()) {
			VipOutput* last_proc = display->inputAt(0)->connection()->source();
			if (!last_proc)
				return;

			auto fov = p->rawData();

			// if the path already exists , remove it from the view only if the camera name changed
			if (d_data->cam_path) {
				if (d_data->cam_path->rawData().dataName() != fov.name + " path") {
					//TODO
					//d_data->view->remove(d_data->cam_path->name());
					//d_data->cam_path.clear();
					d_data->cam_path->deleteLater();
					d_data->cam_path = nullptr;
				}
			}
			else {
				// make sure (for session reloading) to suppress all CAD objects with the same name
				VipPlotVTKObject* found = d_data->view->objectByName(fov.name + " path");
				if (found)
					found->deleteLater();
			}

			// create the path

			vtkPoints* pts = nullptr;
			vtkSmartPointer<vtkPolyLine> polyLine = vtkSmartPointer<vtkPolyLine>::New();
			vtkCellArray* cells = nullptr;
			vtkPolyData* polyData = nullptr;

			if (d_data->cam_path && d_data->cam_path->rawData()) {
				polyData = d_data->cam_path->rawData().polyData();
				pts = polyData->GetPoints();
				pts->Initialize();

				cells = polyData->GetLines();
				cells->Initialize();
			}
			else {
				pts = vtkPoints::New();
				cells = vtkCellArray::New();
				polyData = vtkPolyData::New();

				polyData->SetPoints(pts);
				polyData->SetLines(cells);

				pts->Delete();
				cells->Delete();

				VipVTKObject obj(polyData);
				obj.setDataName(fov.name + " path");

				d_data->cam_path = new VipPlotVTKObject();
				d_data->cam_path->setRawData(obj);

				d_data->cam_path->setVisible(true);
				//d_data->cam_path->setColor(vipToQColor(d_data->color));
				//d_data->cam_path->setSelectedColor(vipToQColor(d_data->color));
				d_data->cam_path->actor()->GetProperty()->SetLineWidth(2);
				polyData->Delete();
			}

			// modify this
			// get the full pipeline for this FOV, and disable all VipDisplayObject and all VipIODevice open in write mode.
			// also disable the processing pool.

			VipProcessingObjectList pipeline = display->allSources();
			VipOutput* src = display->inputAt(0)->connection()->source();
			if (src)
				src->setEnabled(false);
			pool->save();
			pipeline.save();

			pool->blockSignals(true);
			pool->disableExcept(pipeline);
			pool->setTimeLimitsEnable(false);

			for (int i = 0; i < pipeline.size(); ++i) {
				if (qobject_cast<VipDisplayObject*>(pipeline[i]))
					pipeline[i]->setEnabled(false);
				else if (qobject_cast<VipIODevice*>(pipeline[i]) && (static_cast<VipIODevice*>(pipeline[i].data())->openMode() & VipIODevice::WriteOnly))
					pipeline[i]->setEnabled(false);
				else
					pipeline[i]->setScheduleStrategy(VipProcessingObject::Asynchronous, false);
			}

			qint64 time = /*seq ? seq->firstTime() :*/ pool->firstTime();
			qint64 end_time = /*seq ? seq->lastTime() :*/ pool->lastTime();
			VipFieldOfView last_fov;
			while (time != VipInvalidTime && time <= end_time) {
				pool->read(time, true);
				last_proc->parentProcessing()->update();
				const VipFieldOfView fov = last_proc->data().value<VipFieldOfView>();
				if (fov != last_fov) {
					pts->InsertNextPoint(fov.pupil);
				}
				last_fov = fov;

				qint64 current = time;
				time = pool->nextTime(time);
				if (time == current)
					break;
			}

			if (src)
				src->setEnabled(true);
			pipeline.restore();
			pool->restore();
			pool->blockSignals(false);
			pts->Modified();

			polyLine->GetPointIds()->SetNumberOfIds(pts->GetNumberOfPoints());
			for (int i = 0; i < pts->GetNumberOfPoints(); i++)
				polyLine->GetPointIds()->SetId(i, i);

			cells->InsertNextCell(polyLine);
			if (pts->GetNumberOfPoints() > 1) {
				d_data->view->setResetCameraEnabled(d_data->view->objects().size() == 0);
				d_data->cam_path->setAxes(d_data->view->area()->bottomAxis(), d_data->view->area()->leftAxis(), VipCoordinateSystem::Cartesian);
				d_data->view->setResetCameraEnabled(true);
			}
		}
	updateColor();
}

VipPlotFieldOfView* VipFOVItem::plotFov() const 
{
	return d_data->plotFOV;
}
VipDisplayFieldOfView* VipFOVItem::display() const
{
	if (VipPlotFieldOfView* plot = plotFov())
		if (VipDisplayFieldOfView* d = plot->property("VipDisplayObject").value<VipDisplayFieldOfView*>())
			return d;
	return nullptr;
}


VipFOVSequence* VipFOVItem::source() const
{
	if (!d_data->plotFOV)
		return nullptr;

	VipOutput* out = nullptr;
	if (auto* display = d_data->plotFOV->property("VipDisplayObject").value<VipDisplayObject*>()) {

		auto fov = d_data->plotFOV->rawData();
		QList<VipProcessingObject*> sources = display->allSources();
		QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
		if (devices.size() == 1) {
			// Find the output
			VipIODevice* d = devices.first();
			for (int i = 0; i < d->outputCount(); ++i) {
				if (d->outputAt(i)->data().value<VipFieldOfView>().name == fov.name) {
					out = d->outputAt(i);
					break;
				}
			}
		}
	}
	if (!out)
		return nullptr;

	auto fov = d_data->plotFOV->rawData();
	VipProcessingObject* proc = out->parentProcessing();
	if (VipFOVSequence* seq = qobject_cast<VipFOVSequence*>(proc))
		return seq;
	if (VipDirectoryReader* dir = qobject_cast<VipDirectoryReader*>(proc)) {
		if (dir->type() != VipDirectoryReader::IndependentData)
			return nullptr;
		VipMultiOutput* multi = dir->topLevelOutputAt(0)->toMultiOutput();
		for (int i = 0; i < multi->count(); ++i) {
			if (multi->at(i)->data().value<VipFieldOfView>().name == fov.name) {
				if (VipFOVSequence* seq = qobject_cast<VipFOVSequence*>(dir->deviceFromOutput(i)))
					return seq;
			}
		}
	}
	return nullptr;
}

OffscreenMappingToInputData* VipFOVItem::mapping() const
{
	return nullptr;
	//d_data->mapping;
}

void VipFOVItem::buildPyramid() 
{
	if (!d_data->view || ! d_data->plotFOV)
		return;
	
	auto fov = d_data->plotFOV->rawData();

	if (d_data->fov_pyramid.actor())
		d_data->view->renderer()->RemoveActor(d_data->fov_pyramid.actor());
	if (d_data->optical_axis.actor())
		d_data->view->renderer()->RemoveActor(d_data->optical_axis.actor());

	VipVTKObject obj;
	double depth = fov.preferredDepth(d_data->view->computeVisualBounds());
	fov.pyramid(obj, depth); // = d_data->fov.pyramid(depth);
	obj.setDataName(fov.name + "/FOV");
	d_data->fov_pyramid.setRawData(obj);
	//d_data->fov_pyramid.setColor(vipToQColor(d_data->color));
	//d_data->fov_pyramid.setSelectedColor(vipToQColor(d_data->color));
	d_data->fov_pyramid.setOpacity(0.4);
	d_data->fov_pyramid.setVisible(false);
	d_data->fov_pyramid.actor()->SetVisibility(false);


	VipVTKObject axis;
	fov.opticalAxis(axis, depth);
	axis.setDataName(fov.name + "/Optical axis");
	d_data->optical_axis.setRawData(axis);
	//d_data->optical_axis.setColor(vipToQColor(d_data->color));
	//d_data->optical_axis.setSelectedColor(vipToQColor(d_data->color));
	d_data->optical_axis.setVisible(false);
	d_data->optical_axis.actor()->SetVisibility(false);

	updateColor();

	d_data->view->renderer()->AddActor(d_data->optical_axis.actor());
	d_data->view->renderer()->AddActor(d_data->fov_pyramid.actor());
}

VipPlotVTKObject* VipFOVItem::FOVPyramid()
{
	if (!d_data->view || ! d_data->plotFOV)
		return nullptr;
	
	if (!d_data->fov_pyramid.rawData()) {
		buildPyramid();
		d_data->view->refresh();
	}

	return &d_data->fov_pyramid;
}

VipPlotVTKObject* VipFOVItem::opticalAxis()
{
	if (!d_data->view || !d_data->plotFOV)
		return nullptr;

	if (!d_data->optical_axis.rawData()) {
		buildPyramid();
		d_data->view->refresh();
	}
	
	return &d_data->optical_axis;
}

void VipFOVItem::saveSpatialCalibrationFile()
{
	/*
	QString filters = "CSV File (*.csv)";

	// check if the H5StillImage plugin is loaded
	VipIODevice* dev = vipCreateVariant("H5StillImageWriter*").value<VipIODevice*>();
	if (dev)
		filters += ";;H5 file (*.h5)";

	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save spatial calibration", filters);
	if (!filename.isEmpty()) {
		// get a mapping processing that creates a new VipVTKObject
		OffscreenMappingToInputData* mapping = nullptr;
		bool delete_mapping = false;
		// TEST: no not use d_data->mapping
		if (true)//!d_data->mapping) {
			mapping = buildMapping(this, nullptr);
			delete_mapping = true;
		}
		else if (d_data->mapping->toInputData()) {
			mapping = buildMapping(this, d_data->mapping->topLevelInputAt(2)->toInput()->connection()->source());
			delete_mapping = true;
		}
		else
			mapping = d_data->mapping;

		if (!mapping->outputAt(0)->value<VipVTKObject>()) {
			QMessageBox::warning(nullptr, "Error", "Unable to save scene model for this camera!");
			if (delete_mapping)
				delete mapping;
			return;
		}

		if (QFileInfo(filename).suffix().compare("csv", Qt::CaseInsensitive) == 0) {
			// save in csv format
			QFile file(filename);
			file.open(QFile::WriteOnly | QFile::Text);
			QTextStream out(&file);
			mapping->mapping()->SaveImageProperties(out);
		}
		else {
			// save in H5 format
			VipProgress progress;
			QMap<QString, VipNDArray> ars = OffscreenMapping::CreateImageProperties(
			  mapping->mapping()->polyData(), mapping->mapping()->fov().name, mapping->mapping()->fov().width, mapping->mapping()->fov().height, &progress);

			progress.setText("Save to " + QFileInfo(filename).fileName());
			dev->setPath(filename);
			dev->open(VipIODevice::WriteOnly);

			VipMultiNDArray array;
			array.setNamedArrays(ars);

			VipAnyData any(QVariant::fromValue(VipNDArray(array)), 0);
			dev->inputAt(0)->setData(any);

			dev->update();
			dev->close();
		}
		if (delete_mapping)
			delete mapping;
	}

	if (dev)
		delete dev;
	*/
}

void VipFOVItem::displaySpatialCalibration()
{
	/*
		if (VipDisplayPlayerArea* area = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()) {
		
		// get the mapping result as a VipVTKObject
		VipVTKObject mapping;
		VipProcessingObject* source = nullptr;

		// TEST: comment d_data->mapping
		if (true) { //! d_data->mapping || d_data->mapping->toInputData()) {
			OffscreenMappingToInputData* map = buildMapping(this, nullptr);
			mapping = map->outputAt(0)->value<VipVTKObject>();
			if (!mapping) {
				QMessageBox::warning(nullptr, "Error", "Unable to display scene model for this camera!");
				return;
			}
			VipAnyResource* dev = new VipAnyResource(VipVTKPlayer::fromChild(this->view())->processingPool());
			dev->setData(QVariant::fromValue(mapping));
			source = dev;
			delete map;
		}
		else {
			mapping = d_data->mapping->outputAt(0)->value<VipVTKObject>();
			source = d_data->mapping;
		}

		// create the ExtractSpatialCalibration and connect it to the OffscreenMappingProcessing
		if (!d_data->displaySC) {
			d_data->displaySC = new ExtractSpatialCalibration();
			d_data->displaySC->setScheduleStrategy(ExtractSpatialCalibration::Asynchronous, true);
			d_data->displaySC->setDeleteOnOutputConnectionsClosed(true);
		}
		if (!d_data->displaySC->inputAt(0)->connection()->source() || d_data->displaySC->inputAt(0)->connection()->source()->parentProcessing() != source)
			d_data->displaySC->inputAt(0)->setConnection(source->outputAt(0));

		// connect the FOV property
		VipOutput* fov_out = d_data->display ? d_data->display->inputAt(0)->connection()->source() : nullptr;
		if (fov_out) {
			if (fov_out != d_data->displaySC->propertyAt(0)->connection()->source())
				d_data->displaySC->propertyAt(0)->setConnection(fov_out);
			if (fov_out->data().data().userType() == qMetaTypeId<VipFieldOfView>())
				d_data->displaySC->propertyAt(0)->setData(fov_out->data());
			else
				d_data->displaySC->propertyAt(0)->setData(d_data->fov);
		}
		else
			d_data->displaySC->propertyAt(0)->setData(d_data->fov);

		// compute once
		d_data->displaySC->inputAt(0)->setData(source->outputAt(0)->data());
		d_data->displaySC->wait();

		// insert a VipProcessingList before the display
		VipProcessingList* lst = new VipProcessingList();
		lst->setScheduleStrategy(VipProcessingList::Asynchronous, true);
		lst->setDeleteOnOutputConnectionsClosed(true);
		lst->outputAt(0)->setData(d_data->displaySC->outputAt(0)->data());
		lst->inputAt(0)->setConnection(d_data->displaySC->outputAt(0));

		vipProcessEvents();

		if (d_data->displaySC) {
			// display the result
			QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessing(lst, nullptr);
			if (VipMultiDragWidget* w = vipCreateFromBaseDragWidget(vipCreateFromWidgets(vipListCast<QWidget*>(players)))) {
				area->addWidget(w);
				d_data->displaySC->reload();
			}
		}
	}*/
}

/* void VipFOVItem::refreshView()
{
	return;

	// deprecate
	if (!d_data->view)
		return ;

	vipProcessEvents(nullptr, 10);

	//Since the renderer does not update itself when selecting a camera until a mouse interaction has been processed,
	//simulate a mouse interaction the first time a camera change is requested.
	//In fact, ALWAYS do it since further rendering issues might appear.
	d_data->view->widget()->forceInteractorRefresh();
	d_data->view->widget()->update();
}*/

void VipFOVItem::resetPyramid()
{
	auto fov = d_data->last;
	d_data->last = VipFieldOfView();
	setFieldOfView(fov);
}

void VipFOVItem::moveCamera()
{
	if (!d_data->plotFOV)
		return;
	auto fov = d_data->plotFOV->rawData();
	double depth = -1;							      // d_data->fov.preferredDepth(d_data->view->ComputeBounds())/10.0;

	auto renderers = d_data->view->renderers();
	for (vtkRenderer* ren : renderers) {
		fov.changePointOfView(ren->GetActiveCamera());
		ren->ResetCameraClippingRange();
	}
	//fov.changePointOfView(d_data->view->widget()->renderWindow(), depth); // renderer()->GetActiveCamera());
	// if (d_data->once)
	{
		d_data->view->widget()->renderWindow()->Modified();
		d_data->view->refresh();
	}
	// else
	//{
	//	d_data->once = true;
	//	refreshView();
	//}
}

bool VipFOVItem::FOVPyramidVisible() const
{
	return d_data->show_fov_pyramid->isChecked();
}

void VipFOVItem::showFOVPyramid(bool visible)
{
	if (!d_data->view)
		return;

	d_data->show_fov_pyramid->blockSignals(true);
	d_data->show_fov_pyramid->setChecked(visible);

	/* if (visible) {
		d_data->show_fov_pyramid->setIcon(vipIcon("fov_displayed.png"));
	}
	else {
		d_data->show_fov_pyramid->setIcon(vipIcon("fov.png"));
	}*/

	FOVPyramid()->setVisible(visible);
	opticalAxis()->setVisible(visible);

	d_data->show_fov_pyramid->blockSignals(false);

	d_data->view->widget()->renderWindow()->Modified();
	d_data->view->renderer()->Modified();
	d_data->view->refresh();
}

void VipFOVItem::applyMapping(bool enable, bool createNewObject, VipPlotSpectrogram* spec, bool remove_mapped_data)
{
	/* if (!d_data->view)
		return;

	if (remove_mapped_data) {
		QString map_object_name = this->fieldOfViewItem().name + " mapping";
		d_data->view->remove(map_object_name);
	}

	// disable first if necessary
	if (d_data->mapping && enable) {
		applyMapping(false);
	}

	QPointer<VipPlotSpectrogram> p_spec = spec;

	// first remove fov pyramid and optical axis
	if (d_data->fov_pyramid) {
		bool visible = d_data->fov_pyramid->actor()->GetVisibility();
		d_data->fov_pyramid->actor()->VisibilityOff();
		d_data->optical_axis->actor()->VisibilityOff();
		d_data->view->renderer()->RemoveActor(d_data->fov_pyramid->actor());
		d_data->view->renderer()->RemoveActor(d_data->optical_axis->actor());
		d_data->fov_pyramid.clear();
		d_data->optical_axis.clear();
		if (visible) {
			FOVPyramid()->actor()->VisibilityOn();
			opticalAxis()->actor()->VisibilityOn();
		}
	}

	d_data->map_image->blockSignals(true);
	d_data->tool_show_visible_points_in_fov->blockSignals(true);
	d_data->show_visible_points_in_fov->blockSignals(true);
	d_data->map_image->setChecked(false);
	d_data->tool_show_visible_points_in_fov->setChecked(false);
	d_data->show_visible_points_in_fov->setChecked(false);
	d_data->map_image->blockSignals(false);
	d_data->show_visible_points_in_fov->blockSignals(false);
	d_data->tool_show_visible_points_in_fov->blockSignals(false);

	if (enable) {
		// if the spectrogram is the output of a VipDisplayObject, connect its source to the mapper
		VipOutput* src = nullptr;
		if (p_spec)
			if (VipDisplayObject* display = spec->property("VipDisplayObject").value<VipDisplayObject*>()) {
				src = display->inputAt(0)->connection()->source();
				if (src->data().isEmpty()) // make sure we have a valid image
					src->setData(spec->rawData().extract(spec->rawData().boundingRect()));
			}

		// only use the FILE object data (and not the computed ones)
		VipVTKObjectList tmp = DataSets(d_data->view->objects());

		buildMapping(createNewObject, src);
		d_data->mapping->wait();

		// add the mapping result (poly data) into the view with a layer of 1 (under all other CAD objects)
		if (VipVTKObject data = d_data->mapping->outputAt(0)->data().value<VipVTKObject>()) {
			if (data->data() && tmp.indexOf(data) < 0) {
				vipCreatePlayersFromProcessing(d_data->mapping, VipVTKPlayer::fromChild(d_data->view));

				// d_data->view->addObject(data);
				data->setVisible(true);
				// data->setLayer(1);
				data->objectSignals()->emitAttributeChanged();
			}
		}

		d_data->tool_show_visible_points_in_fov->setChecked(true);
		d_data->show_visible_points_in_fov->setChecked(true);

		VipVTKPlayer* main = VipVTKPlayer::fromChild(view());
		if (src) {
			// display mapped image

			QString array_name = d_data->mapping->mapping()->MapImageName(); //"Mapped image";
			if (vipIsImageArray(spec->rawData().extract(spec->rawData().boundingRect())))
				main->attributes()->setDisplayedAttribute(VipVTKObject::Point, array_name, -1);
			else
				main->attributes()->setDisplayedAttribute(VipVTKObject::Point, array_name, 0);

			d_data->map_image->setChecked(true);
		}
		else {

			// make the attribute 'camera_name coordinates' visible
			if (main->attributes()->currentAttribute() == "None")
				main->attributes()->setDisplayedAttribute(VipVTKObject::Point, d_data->fov.name + " coordinates", 2);
		}
	}
	else {
		if (d_data->mapping) {
			if (remove_mapped_data) {
				VipVTKObject data = d_data->mapping->outputAt(0)->data().value<VipVTKObject>();
				if (data)
					d_data->view->remove(data);
			}
			d_data->mapping->reset();
			d_data->mapping->deleteLater();

			if (d_data->displaySC)
				d_data->displaySC->deleteLater();

			//d_data->mappingOptionsAction->setVisible(false);
			//d_data->mappingOptions->setOffscreenMapping(nullptr);
			//d_data->mappingOptions->setVisible(false);
		}
	}*/
}

void VipFOVItem::showVisiblePointsInFOVPyramid(bool)
{
	applyMapping(true);
}
/*
void VipFOVItem::Map(VipPlotSpectrogram * spec)
{
	QPointer<VipPlotSpectrogram> p_spec = spec;

	if (p_spec)
	{
		//if the spectrogram is the output of a VipDisplayObject, connect its source to the mapper
		VipOutput * src = nullptr;
		if (VipDisplayObject * display = spec->property("VipDisplayObject").value<VipDisplayObject*>())
		{
			src = display->inputAt(0)->connection()->source();
		}

		//only use the FILE object data (and not the computed ones)
		VipVTKObjectList tmp = DataSets(d_data->view->objects());
		buildMapping(src);
		d_data->mapping->wait();


		//TEST
		//add the mapping result (poly data) into the view with a layer of 1 (under all other CAD objects)
		if (VipVTKObject data = d_data->mapping->outputAt(0)->data().value<VipVTKObject>())
		{
			if (data->data() && tmp.indexOf(data) < 0)
			{
				vipCreatePlayersFromProcessing(d_data->mapping, d_data->view->TokidaWidget());

				//d_data->view->addObject(data);
				data->setVisible(true);
				//data->setLayer(1);
				data->emitAttributeChanged();
			}
		}

		//display mapped image

		QString array_name = d_data->mapping->mapping()->MapImageName();//"Mapped image";
		if (vipIsImageArray(spec->rawData().extract(spec->rawData().boundingRect())))
			view()->TokidaWidget()->attributes()->setDisplayedAttribute(VipVTKObject::Point, array_name, -1);
		else
			view()->TokidaWidget()->attributes()->setDisplayedAttribute(VipVTKObject::Point, array_name, 0);

		d_data->map_image->blockSignals(true);
		d_data->map_image->setChecked(true);
		d_data->map_image->blockSignals(false);

		//TEST
		d_data->tool_show_visible_points_in_fov->blockSignals(true);
		d_data->show_visible_points_in_fov->blockSignals(true);
		d_data->tool_show_visible_points_in_fov->blockSignals(true);
		d_data->tool_show_visible_points_in_fov->setChecked(true);
		d_data->show_visible_points_in_fov->setChecked(true);
		d_data->tool_show_visible_points_in_fov->setChecked(true);
		d_data->tool_show_visible_points_in_fov->blockSignals(false);
		d_data->show_visible_points_in_fov->blockSignals(false);
		d_data->tool_show_visible_points_in_fov->blockSignals(false);

	}
}
*/

void VipFOVItem::importCamera()
{
	if (!d_data->plotFOV)
		return;

	auto fov = d_data->plotFOV->rawData();
	// TODO: it seems that we need to call this several times to actually import the camera. To investigate.
	for (int i = 0; i < 10; ++i)
		fov.importCamera(d_data->view->renderer()->GetActiveCamera());

	
	if (VipFOVSequence* seq = source()) {
		
		VipFieldOfView f = seq->fovAtTime(seq->time());
		if (!f.isNull()) {
			qint64 pos = seq->timeToPos(seq->time());
			fov.time = f.time;
			seq->at(pos) = fov;

			// reset camera path
			if (d_data->cam_path) {
				d_data->cam_path->deleteLater();
				d_data->cam_path = nullptr;
			}
		}
		seq->reload();
	}
}

void VipFOVItem::applyImageMapping(bool enable)
{
	if (enable) {
		ApplyMappingDialog dialog;
		if (dialog.exec() == QDialog::Accepted) {
			VipProgress progress;
			progress.setText("Compute FOV mapping...");

			// if (dialog.VideoPlayer())
			{
				if (dialog.ConstructNewObject()) {

					applyMapping(true, true, dialog.VideoPlayer() ? dialog.VideoPlayer()->spectrogram() : nullptr);

					// cannot reconstruct the temporal mapping without a video player
					d_data->tool_show_visible_points_in_fov->blockSignals(true);
					d_data->tool_show_visible_points_in_fov->setChecked(false);
					d_data->tool_show_visible_points_in_fov->blockSignals(false);
				}
				else {
					applyMapping(true, false, dialog.VideoPlayer() ? dialog.VideoPlayer()->spectrogram() : nullptr);
				}
			}
		}
		else {
			d_data->tool_show_visible_points_in_fov->blockSignals(true);
			d_data->tool_show_visible_points_in_fov->setChecked(false);
			d_data->tool_show_visible_points_in_fov->blockSignals(false);
		}
	}
	else {
		applyMapping(false);
	}
}

class TreeWidget : public QTreeWidget
{
public:
	VipFOVTreeWidget* parentTree;
	TreeWidget(QWidget* parent = nullptr)
	  : QTreeWidget(parent)
	{
	}

protected:
	virtual void dragMoveEvent(QDragMoveEvent* event) { parentTree->dragMoveEvent(event); }
	virtual void dragEnterEvent(QDragEnterEvent* event) { parentTree->dragEnterEvent(event); }
	virtual void dropEvent(QDropEvent* event) { parentTree->dropEvent(event); }
	virtual void mouseMoveEvent(QMouseEvent* event) { parentTree->mouseMoveEvent(event); }
};
class VipFOVTreeWidget::PrivateData
{
public:
	QToolBar tools;
	QAction* overlapping;
	bool destroy{ false };
	bool dirtyPyramid{ false };
	TreeWidget tree;
	QPointer<VipVTKGraphicsView> view;
	QPointer<VipFOVItem> currentFOV;
	//QPointer<OffscreenFovOverlappingProcessing> overlappings;
	VipColorPalette palette;
	
};

VipFOVTreeWidget::VipFOVTreeWidget(VipVTKGraphicsView* view, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->view = view;
	d_data->tree.parentTree = this;
	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(&d_data->tools);
	lay->addWidget(&d_data->tree);
	setLayout(lay);

	d_data->tools.setIconSize(QSize(20, 20));
	//d_data->tools.setStyleSheet("");
	QAction* save = d_data->tools.addAction(vipIcon("save_as.png"), "Save selected cameras in file...");
	d_data->tools.addSeparator();
	QAction* add = d_data->tools.addAction(vipIcon("new_fov.png"), "Create new camera...");
	QAction* del = d_data->tools.addAction(vipIcon("del.png"), "remove selected camera");
	d_data->tools.addSeparator();
	d_data->overlapping = d_data->tools.addAction(vipIcon("overlapping.png"), "Compute/Recompute cameras overlappings");
	d_data->overlapping->setCheckable(true);
	// TEST: for now, just hide this fucntionality
	d_data->overlapping->setVisible(false);

	connect(add, SIGNAL(triggered(bool)), this, SLOT(create()));
	connect(del, SIGNAL(triggered(bool)), this, SLOT(deleteSelection()));
	connect(save, SIGNAL(triggered(bool)), this, SLOT(saveSelection()));
	connect(d_data->overlapping, SIGNAL(triggered(bool)), this, SLOT(computeOverlapping(bool)));

	d_data->tree.setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->tree.headerItem()->setHidden(true);
	d_data->tree.setColumnCount(2);
	d_data->tree.setColumnWidth(0, 150);
	//d_data->tree.setStyleSheet("QTreeWidget {border-style: flat;}");
	d_data->tree.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	d_data->tree.setAcceptDrops(true);
	d_data->tree.setDragDropMode(QAbstractItemView::DragDrop);

	QTreeWidgetItem* top = new QTreeWidgetItem();
	top->setText(0, "Cameras");
	QFont font(top->font(0));
	font.setBold(true);
	top->setFont(0, font);
	d_data->tree.addTopLevelItem(top);

	connect(&d_data->tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(itemDoubleClicked()));
	connect(&d_data->tree, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(itemPressed(QTreeWidgetItem*, int)));
	connect(&d_data->tree, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));

	connect(view, SIGNAL(dataChanged()), this, SLOT(dataChanged()));

	connect(view->area(), SIGNAL(itemAdded(VipPlotItem*)), this, SLOT(plotItemAdded(VipPlotItem*)));
	connect(view->area(), SIGNAL(itemRemoved(VipPlotItem*)), this, SLOT(plotItemRemoved(VipPlotItem*)));


	setMaximumHeight(sizeHint().height() + 20);

	d_data->tree.viewport()->installEventFilter(this);
}

VipFOVTreeWidget::~VipFOVTreeWidget()
{
}

VipVTKGraphicsView* VipFOVTreeWidget::view() const
{
	return d_data->view;
}

QTreeWidget* VipFOVTreeWidget::tree() const
{
	return &d_data->tree;
}

const VipColorPalette& VipFOVTreeWidget::itemColorPalette() const
{
	if (!d_data->palette.count()) {
		// if (VipVTKPlayer * w = TokidaWidget())
		//	const_cast<VipColorPalette&>(d_data->palette) = w->itemColorPalette();
		// else
		const_cast<VipColorPalette&>(d_data->palette) = VipColorPalette(VipLinearColorMap::ColorPaletteRandom);
	}
	return d_data->palette;
}

void VipFOVTreeWidget::setItemColorPalette(const VipColorPalette& p)
{
	d_data->palette = p;
}

VipFOVItem* VipFOVTreeWidget::addFieldOfView(VipPlotFieldOfView * plot)
{
	int index = -1;
	// look for fov name in items
	for (int i = 0; i < d_data->tree.topLevelItem(0)->childCount(); ++i) {
		VipFOVItem* item = static_cast<VipFOVItem*>(d_data->tree.topLevelItem(0)->child(i));
		if (item->plotFov() == plot) {
			index = i;
			break;
		}
	}

	VipFOVItem* item = nullptr;
	if (index < 0) {
		index = d_data->tree.topLevelItem(0)->childCount();
		item = new VipFOVItem(d_data->view, d_data->tree.topLevelItem(0));
	}
	else {
		item = static_cast<VipFOVItem*>(d_data->tree.topLevelItem(0)->child(index));
	}

	item->setPlotFov(plot);

	//TODO
	/* double color[3];
	const QColor c = itemColorPalette().color(index);
	color[0] = c.redF();
	color[1] = c.greenF();
	color[2] = c.blueF();
	item->setColor(color);*/
	d_data->tree.expandAll();
	// setVisible(this->topLevelItem(0)->childCount());

	setMaximumHeight(sizeHint().height() + 20);

	return item;
}

VipFieldOfViewList VipFOVTreeWidget::fieldOfViews() const
{
	VipFieldOfViewList res;
	QTreeWidgetItem* top = d_data->tree.topLevelItem(0);
	for (int i = 0; i < top->childCount(); ++i) {
		if (auto* plot = static_cast<VipFOVItem*>(top->child(i))->plotFov())
			res << plot->rawData();
	}
	return res;
}

VipFOVItem* VipFOVTreeWidget::currentFieldOfViewItem() const
{
	return d_data->currentFOV;
}

VipFOVItem* VipFOVTreeWidget::fieldOfViewItem(const VipPlotFieldOfView* plot) const
{
	QTreeWidgetItem* top = d_data->tree.topLevelItem(0);
	for (int i = 0; i < top->childCount(); ++i) {
		if (static_cast<VipFOVItem*>(top->child(i))->plotFov() == plot)
			return static_cast<VipFOVItem*>(top->child(i));
	}
	return nullptr;
}

VipFOVItem* VipFOVTreeWidget::fieldOfViewItem(const QString& name) const 
{
	QTreeWidgetItem* top = d_data->tree.topLevelItem(0);
	for (int i = 0; i < top->childCount(); ++i) {
		if (auto* plot = static_cast<VipFOVItem*>(top->child(i))->plotFov())
			if (plot->rawData().name == name)
				return static_cast<VipFOVItem*>(top->child(i));
	}
	return nullptr;
}

QList<VipFOVItem*> VipFOVTreeWidget::fieldOfViewItems() const
{
	QList<VipFOVItem*> res;
	QTreeWidgetItem* top = d_data->tree.topLevelItem(0);
	for (int i = 0; i < top->childCount(); ++i) {
		res << static_cast<VipFOVItem*>(top->child(i));
	}

	return res;
}

QStringList VipFOVTreeWidget::visibleFOVPyramidNames() const
{
	QStringList res;
	QTreeWidgetItem* top = d_data->tree.topLevelItem(0);
	for (int i = 0; i < top->childCount(); ++i) {
		auto* it = static_cast<VipFOVItem*>(top->child(i));
		if (auto* plot = it->plotFov())
			if (it->FOVPyramidVisible())
				res.append(plot->rawData().name);
	}
	return res;
}

QList<VipFOVItem*> VipFOVTreeWidget::selectedFieldOfViewItems() const
{
	if (d_data->tree.selectedItems().indexOf(d_data->tree.topLevelItem(0)) >= 0)
		return fieldOfViewItems();

	QList<QTreeWidgetItem*> items = d_data->tree.selectedItems();
	QList<VipFOVItem*> res;
	for (int i = 0; i < items.size(); ++i)
		res << static_cast<VipFOVItem*>(items[i]);

	return res;
}

VipFieldOfViewList VipFOVTreeWidget::selectedFieldOfViews() const
{
	QList<VipFOVItem*> items = selectedFieldOfViewItems();
	VipFieldOfViewList res;
	for (int i = 0; i < items.size(); ++i) {
		if (auto* plot = items[i]->plotFov())
			res << plot->rawData();
	}

	return res;
}

QList<VipPlotFieldOfView*> VipFOVTreeWidget::plotFieldOfViews() const
{
	QList<VipFOVItem*> items = this->fieldOfViewItems();
	QList<VipPlotFieldOfView*> res;

	for (int i = 0; i < items.size(); ++i)
		if (VipPlotFieldOfView* d = items[i]->plotFov())
			res.append(d);

	return res;
}

QList<VipPlotFieldOfView*> VipFOVTreeWidget::selectedPlotFieldOfViews() const
{
	QList<VipFOVItem*> items = selectedFieldOfViewItems();
	QList<VipPlotFieldOfView*> res;
	for (int i = 0; i < items.size(); ++i) {
		if (items[i]->isSelected())
			if (auto* plot = items[i]->plotFov())
				res << plot;
	}

	return res;
}

QList<VipDisplayFieldOfView*> VipFOVTreeWidget::displayObjects()
{
	QList<VipFOVItem*> items = this->fieldOfViewItems();
	QList<VipDisplayFieldOfView*> res;

	for (int i = 0; i < items.size(); ++i)
		if (VipDisplayFieldOfView* d = items[i]->display())
			res.append(d);

	return res;
}

void VipFOVTreeWidget::loadFOVFile()
{
	QString filename = VipFileDialog::getOpenFileName(0, "Open field of view file", "*.xml");
	if (!filename.isEmpty())
		loadFOVFile(filename);
}

void VipFOVTreeWidget::clear()
{
	QTreeWidgetItem* top = d_data->tree.topLevelItem(0);
	while (top->childCount())
		delete top->child(0);

	// setVisible(this->topLevelItem(0)->childCount());

	setMaximumHeight(sizeHint().height() + 20);
}

void VipFOVTreeWidget::loadFOVFile(const QString& filename)
{
	if (VipVTKPlayer* pl = VipVTKPlayer::fromChild(d_data->view))
	{
		VipPathList lst;
		lst << filename;
		vipGetMainWindow()->openPaths(lst, pl);
	}

	/* VipFieldOfViewList lst = LoadFieldOfViews(filename);

	for (int i = 0; i < lst.size(); ++i) {
		VipFOVItem* item = new VipFOVItem(d_data->view, d_data->tree.topLevelItem(0));
		item->setFieldOfView(lst[i]);
	}
	*/
	d_data->tree.expandAll();
	
	setMaximumHeight(sizeHint().height() + 20);
}

void VipFOVTreeWidget::itemDoubleClicked()
{
	if (!d_data->tree.selectedItems().size() || d_data->tree.selectedItems()[0] == d_data->tree.topLevelItem(0))
		return;

	VipFOVItem* item = static_cast<VipFOVItem*>(d_data->tree.selectedItems()[0]);
	item->moveCamera();
	d_data->currentFOV = item;
	// item->refreshView();
	d_data->view->widget()->resetCameraUserMoved();
}

void VipFOVTreeWidget::itemPressed(QTreeWidgetItem* item, int)
{
	/*{
		QList<QTreeWidgetItem*> selected = d_data->tree.selectedItems();

		//TEST: tells if fov overlaps
		if (selected.size() == 2)
		{
			VipFieldOfView fov1 = static_cast<VipFOVItem*>(selected[0])->fieldOfViewItem();
			VipFieldOfView fov2 = static_cast<VipFOVItem*>(selected[1])->fieldOfViewItem();
			qint64 start = QDateTime::currentMSecsSinceEpoch();
			bool overlap = false;
			for (int i = 0; i < 100; ++i)
				overlap = fov1.intersectWith(fov2, 1000000);
			qint64 el = QDateTime::currentMSecsSinceEpoch() - start;
			printf("overlap: %i\n", (int)el);
			if (overlap)
			{
				d_data->tree.topLevelItem(0)->setBackgroundColor(0,Qt::red);
			}
			else
			{
				d_data->tree.topLevelItem(0)->setBackgroundColor(0,Qt::transparent);
			}
		}
		else
			d_data->tree.topLevelItem(0)->setBackgroundColor(0, Qt::transparent);
	}*/

	// redraw the view to display a mark for the FOV pupil position
	view()->refresh();

	// set the processing info
	if (item && item != d_data->tree.topLevelItem(0) ) {
		if (auto* plot = static_cast<VipFOVItem*>(item)->plotFov())
			if (auto* display = plot->property("VipDisplayObject").value<VipDisplayObject*>()) {

				vipGetProcessingObjectInfo()->setProcessingObject(display);
				vipGetProcessingEditorToolWidget()->setProcessingObject(display);
			}
	}

	if (!(QApplication::mouseButtons() & Qt::RightButton))
		return;

	item->setSelected(true);
	QList<QTreeWidgetItem*> selected = d_data->tree.selectedItems();

	QMenu menu;
	QAction* del = menu.addAction(vipIcon("del.png"), "Remove selection");
	connect(del, SIGNAL(triggered(bool)), this, SLOT(deleteSelection()));
	menu.addSeparator();

	bool top_level = selected.size() == 1 && selected.first() == d_data->tree.topLevelItem(0);

	QAction* save = menu.addAction(vipIcon("save_as.png"), top_level ? "Save all cameras..." : "Save selected cameras...");
	connect(save, SIGNAL(triggered(bool)), this, SLOT(saveSelection()));

	QAction* duplicate = menu.addAction("Duplicate selected cameras");
	connect(duplicate, SIGNAL(triggered(bool)), this, SLOT(duplicateSelection()));

	// one single camera
	if (selected.size() == 1 && selected.first() != d_data->tree.topLevelItem(0)) {
		QAction* fov = menu.addAction(vipIcon("open_fov.png"), "Apply camera");
		connect(fov, SIGNAL(triggered(bool)), this, SLOT(itemDoubleClicked()));

		QAction* edit = menu.addAction("edit camera...");
		connect(edit, SIGNAL(triggered(bool)), this, SLOT(edit()));

		QAction* save_image = menu.addAction("Save camera image...");
		connect(save_image, SIGNAL(triggered(bool)), this, SLOT(saveAttributeFieldOfView()));
	}
	// top level item only
	else if (top_level) {
		QAction* create = menu.addAction("New camera...");
		connect(create, SIGNAL(triggered(bool)), this, SLOT(create()));

		QAction* reset = menu.addAction("Reset camera");
		connect(reset, SIGNAL(triggered(bool)), this, SLOT(resetCamera()));
	}

	menu.exec(QCursor::pos());
}


void VipFOVTreeWidget::duplicateSelection()
{
	auto items = this->selectedFieldOfViewItems();
	for (VipFOVItem* item : items) {
		if (!item->plotFov())
			continue;
		VipFOVSequence* seq = item->source();
		if (!seq)
			continue;

		auto fovs = seq->fieldOfViews();

		VipFOVSequence* duplicate = new VipFOVSequence();
		duplicate->setFieldOfViews(fovs);

		QString name = seq->fovName() + "-copy";
		duplicate->setFovName(name);
		duplicate->open(VipIODevice::ReadOnly);

		vipGetMainWindow()->openDevices(VipIODeviceList() << duplicate, VipVTKPlayer::fromChild(view()));
	}

	setMaximumHeight(sizeHint().height() + 20);
}

void VipFOVTreeWidget::deleteSelection()
{
	QList<QTreeWidgetItem*> selected = d_data->tree.selectedItems();
	for (int i = 0; i < selected.size(); ++i) {
		if (selected[i] == d_data->tree.topLevelItem(0)) {
			// remove all
			clear();
			return;
		}
		else {
			delete selected[i];
		}
	}

	setMaximumHeight(sizeHint().height() + 20);
	// setVisible(this->topLevelItem(0)->childCount());
}


bool VipFOVTreeWidget::eventFilter(QObject* watched, QEvent* evt)
{
	if (evt->type() == QEvent::KeyPress) {
		QKeyEvent* e = static_cast<QKeyEvent*>(evt);
		if (e->key() == Qt::Key_Delete)
			this->deleteSelection();
	}
	else if (evt->type() == QEvent::DragEnter) {
		const VipMimeDataCoordinateSystem* mime = qobject_cast<const VipMimeDataCoordinateSystem*>(static_cast<QDragEnterEvent*>(evt)->mimeData());
		if (mime) {
			static_cast<QDragEnterEvent*>(evt)->acceptProposedAction();
			return true;
		}
	}
	else if (evt->type() == QEvent::DragMove) {
		const VipMimeDataCoordinateSystem* mime = qobject_cast<const VipMimeDataCoordinateSystem*>(static_cast<QDragMoveEvent*>(evt)->mimeData());
		if (mime) {
			static_cast<QDragMoveEvent*>(evt)->acceptProposedAction();
			return true;
		}
	}
	else if (evt->type() == QEvent::Drop) {
		const VipMimeDataCoordinateSystem* mime = qobject_cast<const VipMimeDataCoordinateSystem*>(static_cast<QDropEvent*>(evt)->mimeData());
		if (mime) {
			// internal move, from CAD files to CAD Computed (or converse)
			QDropEvent* event = static_cast<QDropEvent*>(evt);
			if (event->source() == &d_data->tree) {
				if (QTreeWidgetItem* it = d_data->tree.itemAt(event->VIP_EVT_POSITION())) {
					if (const VipMimeDataProcessingObjectList* mime = qobject_cast<const VipMimeDataProcessingObjectList*>(static_cast<QDropEvent*>(evt)->mimeData())) {
						/* bool is_file = false;
						while (it) {
							if (it == d_data->inFile) {
								is_file = true;
								break;
							}
							it = it->parent();
						}*/

						// wa can only move simple pipelines, like device -> processing list -> display
						/* QList<VipDisplayObject*> displays = mime->processings().findAll<VipDisplayObject*>();
						for (int i = 0; i < displays.size(); ++i) {
							// find a source VipProcessingList that contains a VipVTKObject
							VipProcessingList* lst = nullptr;
							QList<VipProcessingList*> all_lst = VipProcessingObjectList(displays[i]->allSources()).findAll<VipProcessingList*>();
							for (int l = all_lst.size() - 1; l >= 0; --l) {
								if (all_lst[l]->outputAt(0)->data().data().userType() == qMetaTypeId<VipVTKObject>()) {
									if (VipVTKObject ptr = all_lst[l]->outputAt(0)->data().value<VipVTKObject>())
										if (ptr->isFileData() != is_file) {
											lst = all_lst[l];
											break;
										}
								}
							}
							if (lst) {
								// just add a processing that change the file attribute
								lst->append(new ChangeDataFileAttribute(is_file));
								lst->reload();
							}
						}*/
					}
				}
			}
			else {
				QList<VipPlotItem*> items = mime->plotData(view()->area()->canvas(), view());
				for (int i = 0; i < items.size(); ++i) {
					items[i]->setAxes(view()->area()->canvas()->axes(), view()->area()->canvas()->coordinateSystemType());
				}
			}
			return true;
		}
	}
	else if (evt->type() == QEvent::MouseMove) {
		QMouseEvent* e = static_cast<QMouseEvent*>(evt);
		if (e->buttons() == Qt::LeftButton) {
			if (qobject_cast<QScrollBar*>(watched))
				return false;

			auto selection = selectedPlotFieldOfViews();
			if (selection.size()) {
				QList<VipProcessingObject*> objects;
				for (int i = 0; i < selection.size(); ++i) {
					if (auto* display = selection[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
						objects << display;
					}
				}

				if (objects.size()) {
					VipMimeDataProcessingObjectList* mime = new VipMimeDataProcessingObjectList();
					mime->setCoordinateSystem(VipCoordinateSystem::Cartesian);
					mime->setProcessing(objects);
					QDrag drag(this);
					drag.setMimeData(mime);
					drag.exec();
					return true;
				}
			}
		}
	}

	return false;
}


void VipFOVTreeWidget::removeFieldOfView(VipFOVItem* item)
{
	delete item;
	setMaximumHeight(sizeHint().height() + 20);
}

void VipFOVTreeWidget::saveSelection()
{
	QList<QTreeWidgetItem*> selected = d_data->tree.selectedItems();
	bool top_level = selected.size() == 1 && selected.first() == d_data->tree.topLevelItem(0);

	QMap<int,VipFieldOfViewList> fovs;
	{
		VipProgress progress;
		progress.setText("Compute cameras... ");
		progress.setModal(true);
		progress.setCancelable(true);

		QList<VipFOVItem*> items;

		if (!top_level)
			items = selectedFieldOfViewItems();
		else {
			// all items
			for (int i = 0; i < d_data->tree.topLevelItem(0)->childCount(); ++i)
				items << static_cast<VipFOVItem*>(d_data->tree.topLevelItem(0)->child(i));
		}

		for (int i = 0; i < items.size(); ++i) {
			VipFOVItem* it = items[i];
			bool saved = false;
			if (VipDisplayFieldOfView* display = it->display())
				if (VipOutput* output = display->inputAt(0)->connection()->source())
					if (VipProcessingPool* pool = display->parentObjectPool()) {
						VipFieldOfViewList cur_fovs;

						pool->stop();

						// disable all processing expect sources
						pool->save();
						QList<VipProcessingObject*> sources = display->allSources();
						pool->disableExcept(sources);

						qint64 time = pool->firstTime();
						qint64 end_time = pool->lastTime();
						progress.setRange(time, end_time);
						pool->blockSignals(true);

						// disable the VipOutput to avoid the sink processings being applied
						output->setEnabled(false);

						VipFieldOfView previous;
						while (time != VipInvalidTime && time <= end_time) {
							progress.setValue(time);
							pool->read(time, true);

							output->parentProcessing()->wait();
							const VipFieldOfView fov = output->data().value<VipFieldOfView>();
							if (fov != previous) {
								cur_fovs.append(fov);
								cur_fovs.last().time = time;
							}
							previous = fov;

							qint64 next = pool->nextTime(time);
							if (next == time || progress.canceled() || next == VipInvalidTime)
								break;
							time = next;
						}

						output->setEnabled(true);

						if (cur_fovs.isEmpty()) {
							if (VipFOVSequence* seq = it->source()) {
								cur_fovs.append(seq->fovAtTime(seq->time()));
							}
							else
								// the FOVSource contains only one FOV
								cur_fovs.append(output->data().value<VipFieldOfView>());
						}
						fovs[i] = cur_fovs;

						pool->blockSignals(false);
						pool->restore();
						saved = true;
					}

			if (!saved) {
				if (it->plotFov())
					fovs[i].append(it->plotFov()->rawData());
			}
		}
	}

	if (fovs.size() == 1) {

		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save field of view file", "Camera file (*.fov)");
		if (!filename.isEmpty()) {
			vipSaveFieldOfViews(fovs.begin().value(), filename);
		}
	}
	else if (fovs.size() > 1) {
		QString dirname = VipFileDialog::getExistingDirectory(nullptr, "Save fields of views in folder");
		if (dirname.isEmpty())
			return;

		dirname.replace("\\", "/");
		if (!dirname.endsWith("/"))
			dirname += "/";

		for (auto it = fovs.begin(); it != fovs.end(); ++it) {
			QString view_name = it.value().first().name;
			if (view_name.isEmpty())
				continue;
			QString filename = dirname + view_name;
			vipSaveFieldOfViews(it.value(), filename);
		}
	}
}

void VipFOVTreeWidget::dataChanged()
{
	// When CAD data changed, we need to recompute the FOV pyramids as the 3D bounds might have changed
	if (!d_data->dirtyPyramid) {
		d_data->dirtyPyramid = true;
		QMetaObject::invokeMethod(this, "dataChangedInternal", Qt::QueuedConnection);
	}
}
void VipFOVTreeWidget::dataChangedInternal()
{
	d_data->dirtyPyramid = false;
	auto* top = d_data->tree.topLevelItem(0);
	for (int i = 0; i < top->childCount(); ++i)
		static_cast<VipFOVItem*>(top->child(i))->resetPyramid();
}

void VipFOVTreeWidget::selectionChanged()
{
	// Select/unselect plot items
	QList<VipPlotFieldOfView*> plots = plotFieldOfViews();
	QList<VipFOVItem*> items = selectedFieldOfViewItems();
	QVector<VipPlotFieldOfView*> to_select;
	bool changed = false;

	// Select plot items based on selected VipFOVItem
	for (VipFOVItem* it : items) {
		if (auto* pl = it->plotFov()) {
			to_select.push_back(pl);
			if (!pl->isSelected())
				changed = true;
			pl->setSelected(true);
			pl->setProperty("_force_select", true);
		}
	}

	// Unselect all other plot items
	for (VipPlotFieldOfView* pl : plots) {
		if (to_select.indexOf(pl) < 0) {
			if (pl->isSelected())
				changed = true;
			pl->setSelected(false);
			pl->setProperty("_force_select", QVariant());
		}
	}

	// update viewport
	if (changed)
		d_data->view->refresh();
}

void VipFOVTreeWidget::plotItemAdded(VipPlotItem* item)
{
	if (VipPlotFieldOfView* plot = qobject_cast<VipPlotFieldOfView*>(item)) {
		addFieldOfView(plot);
	}
}
void VipFOVTreeWidget::plotItemRemoved(VipPlotItem* item)
{
	if (VipPlotFieldOfView* plot = qobject_cast<VipPlotFieldOfView*>(item)) {
		if (auto it = fieldOfViewItem(plot)) {
			it->setPlotFov(nullptr);
			removeFieldOfView(it);
		}
	}
}

void VipFOVTreeWidget::computeOverlapping(bool enable)
{
	/* d_data->overlapping->blockSignals(true);
	d_data->overlapping->setChecked(enable);
	d_data->overlapping->blockSignals(false);

	if (enable) {
		if (!d_data->overlappings)
			d_data->overlappings = new OffscreenFovOverlappingProcessing(this);

		// disable the processings to avoid triggering an update
		d_data->overlappings->setEnabled(false);

		//
		// build all connections
		//

		// connect the input VipVTKObject
		VipMultiInput* multi = d_data->overlappings->topLevelInputAt(0)->toMultiInput();
		multi->clear();
		VipVTKObjectList allData = d_data->view->objects();
		for (int i = 0; i < allData.size(); ++i) {
			multi->add();
			if (VipPlotItem* item = allData[i]->plotObject())
				if (VipDisplayObject* disp = item->property("VipDisplayObject").value<VipDisplayObject*>())
					if (VipOutput* output = disp->inputAt(0)->connection()->source())
						output->setConnection(multi->at(multi->count() - 1)); // set the connection (if any)
		}

		// enable the processings
		d_data->overlappings->setEnabled(true);

		//
		// set input data in addition to the connections (to trigger a processing)
		//

		d_data->overlappings->setScheduleStrategy(VipProcessingObject::Asynchronous, false);

		// set the input VipVTKObject
		for (int i = 0; i < allData.size(); ++i)
			d_data->overlappings->topLevelInputAt(0)->toMultiInput()->at(i)->setData(allData[i]);

		// reload mapping (which will apply MapImage)
		d_data->overlappings->update();

		d_data->overlappings->setScheduleStrategy(VipProcessingObject::Asynchronous, true);

		// make the attribute 'cameras' visible
		VipVTKPlayer::fromChild(d_data->view)->attributes()->setDisplayedAttribute(VipVTKObject::Point, "cameras", 0);
	}
	else if (d_data->overlappings) {
		d_data->overlappings->setEnabled(false);
		d_data->overlappings->deleteLater();
	}

	d_data->view->refresh();*/
}

static void saveImage(const QString& out_file, VipVTKGraphicsView* view, const VipFieldOfView& fov, int buffer_type)
{
	VipProgress display;
	display.setModal(true);
	display.setText("<b>Extract image...</b>");

	vtkSmartPointer<vtkCoordinate> coord = vtkSmartPointer<vtkCoordinate>::New();
	coord->SetCoordinateSystemToWorld();
	coord->SetViewport(view->renderer());

	// get the camera boundaries in display coordinates

	double topLeft[3];
	double topRight[3];
	double bottomRight[3];
	double bottomLeft[3];
	fov.fieldOfViewCorners(topLeft, topRight, bottomRight, bottomLeft);
	// TEST
	// fov.fieldOfViewCorners(topLeft, topRight, bottomRight, bottomLeft,10000);
	// fov.fieldOfViewCorners(topLeft, topRight, bottomRight, bottomLeft, 100000);
	// fov.fieldOfViewCorners(topLeft, topRight, bottomRight, bottomLeft, 1000000);

	coord->SetValue(topLeft);
	double left = coord->GetComputedDoubleViewportValue(view->renderer())[0];
	double top = coord->GetComputedDoubleViewportValue(view->renderer())[1];
	coord->SetValue(bottomRight);
	double right = coord->GetComputedDoubleViewportValue(view->renderer())[0];
	double bottom = coord->GetComputedDoubleViewportValue(view->renderer())[1];

	int* size = view->renderer()->GetRenderWindow()->GetActualSize();
	double bounds[4] = { left / size[0], top / size[1], right / size[0], bottom / size[1] };

	// clip to 0
	for (int i = 0; i < 4; ++i)
		bounds[i] = qMax(0.0, bounds[i]);

	VipVTKImage im = view->imageContent(1, bounds, buffer_type).scaled(fov.width, fov.height);

	if (buffer_type == VTK_ZBUFFER) {
		

		// for z buffer only: replace 1 (background) by NaN, and look for the other real z value (distance to camera position)

		// find the real min and max z value

		display.setText("<b>Convert z buffer to depth...</b>");

		double z_min, z_max;
		VipFieldOfView::extractZBounds(fromPlotVipVTKObject(view->objects()), fov, z_min, z_max);

		for (int y = 0; y < im.height(); ++y) {
			for (int x = 0; x < im.width(); ++x) {
				double _z = im.doublePixelAt(x, y);
				if (_z >= 1.) {
					im.setDoublePixelAt(x, y, vipNan());
				}
				else {
					double _x = (left + (x) / (double)im.width() * (right - left)) / size[0] * 2 - 1;
					double _y = -((top + (y) / (double)im.height() * (bottom - top)) / size[1] * 2 - 1);
					view->renderer()->ViewToWorld(_x, _y, _z);
					_x = _x - view->renderer()->GetActiveCamera()->GetPosition()[0];
					_y = _y - view->renderer()->GetActiveCamera()->GetPosition()[1];
					_z = _z - view->renderer()->GetActiveCamera()->GetPosition()[2];
					double dist = sqrt(_x * _x + _y * _y + _z * _z);
					if (dist < z_max)
						im.setDoublePixelAt(x, y, dist);
					else
						im.setDoublePixelAt(x, y, vipNan());
				}
			}
		}

		display.setText("<b>Save to file...</b>");
		im.save(out_file);
		return;
	}

	display.setText("<b>Save to file...</b>");
	// im.save(out_file);
	// GetImageTreeWidget()->AddItem(im);
	QImage tmp = view->widgetContent(bounds).scaled(fov.width, fov.height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	tmp.save(out_file);
}

#include <vtkDelaunay2D.h>
#include <vtkDoubleArray.h>
#include <vtkIdList.h>
static void saveImage(const QString& out_file, const VipVTKObjectList& lst, const VipFieldOfView& fov, VipVTKObject::AttributeType type, const QString& name, int comp)
{
	VipProgress display;
	display.setText("<b>Start saving image...</b>");
	display.setModal(true);

	VipVTKImage img(fov.width, fov.height, 0, VTK_DOUBLE);

	vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
	vtkSmartPointer<vtkDoubleArray> data = vtkSmartPointer<vtkDoubleArray>::New();

	// just to avoid having more than one point per pixel, which is useless and slow down (or crash) vtkDelaunay2D
	QVector<double> image_depth(fov.width * fov.height, 0);
	QVector<int> image_index(fov.width * fov.height, -1);

	for (int i = 0; i < lst.size(); ++i) {
		// only works for vtkDataSet
		if (!lst[i].data()->IsA("vtkDataSet"))
			continue;

		vtkAbstractArray* array = lst[i].pointsAttribute(name);
		vtkDataArray* coord = static_cast<vtkDataArray*>(lst[i].pointsAttribute(fov.name + " coordinates"));
		if (!coord || !array || array->GetNumberOfComponents() <= comp)
			continue;

		if (type == VipVTKObject::Field) {
			/*double value = VipVTKObject::makeAttribute(array,0).second[comp].ToDouble();
			int num_points = coord->GetNumberOfTuples();
			for(int i=0; i < num_points; ++i)
			{
				double x = coord->GetComponent(i,0);
				double y = coord->GetComponent(i,1);
				if(std::isnan(x) || std::isnan(y))
					continue;

				if(x >=0 && y>=0 && x < fov.width && y < fov.height && !image_points[y * fov.width + x])
				{
					pts->InsertNextPoint(x,y,0);
					data->InsertNextTuple1(value);
					image_points[y * fov.width + x] = true;
				}
			}*/
		}
		else if (type == VipVTKObject::Point) {
			vtkDataArray* values = static_cast<vtkDataArray*>(array);
			int num_points = coord->GetNumberOfTuples();
			for (int i = 0; i < num_points; ++i) {
				double x = coord->GetComponent(i, 0);
				double y = coord->GetComponent(i, 1);
				double z = coord->GetComponent(i, 2);
				if (vtkMath::IsNan(x) || vtkMath::IsNan(y))
					continue;

				if (x >= 0 && y >= 0 && x < fov.width && y < fov.height) {
					if (image_index[int(y) * fov.width + int(x)] < 0) {
						double value = values->GetComponent(i, comp);
						pts->InsertNextPoint(x, y, 0);
						data->InsertNextTuple1(value);
						image_depth[int(y) * fov.width + int(x)] = z;
						image_index[int(y) * fov.width + int(x)] = pts->GetNumberOfPoints() - 1;
					}
					else if (z < image_depth[int(y) * fov.width + int(x)]) {
						double value = values->GetComponent(i, comp);
						data->SetTuple1(image_index[int(y) * fov.width + int(x)], value);
						image_depth[int(y) * fov.width + int(x)] = z;
					}
				}
			}
		}
	} // end for loop

	// add the 4 corners
	pts->InsertNextPoint(0, 0, 0);
	pts->InsertNextPoint(fov.width, 0, 0);
	pts->InsertNextPoint(fov.width, fov.height, 0);
	pts->InsertNextPoint(0, fov.height, 0);
	data->InsertNextTuple1(0);
	data->InsertNextTuple1(0);
	data->InsertNextTuple1(0);
	data->InsertNextTuple1(0);

	display.setText("<b>Apply Delaunay triangulation...</b>");

	vtkSmartPointer<vtkPolyData> polydata = vtkSmartPointer<vtkPolyData>::New();
	polydata->SetPoints(pts);
	polydata->GetPointData()->AddArray(data);

	vtkSmartPointer<vtkDelaunay2D> delaunay = vtkSmartPointer<vtkDelaunay2D>::New();
	delaunay->SetInputData(polydata);
	delaunay->Update();

	polydata = delaunay->GetOutput();

	display.setRange(0, fov.height);
	display.setText("<b>Interpolate pixels...</b>");

	// find cell once
	{
		double pos[3] = { 0, 0, 0 };
		double pcoords[3];
		double weights[3];
		int subId;
		polydata->FindCell(pos, nullptr, 0, 0.0001, subId, pcoords, weights);
	}

	for (int y = 0; y < fov.height; ++y) {
		if (display.canceled())
			break;
		display.setValue(y);

		for (int x = 0; x < fov.width; ++x) {
			// find the cell at this position
			double pos[3] = { (double)x, (double)y, 0. };
			double pcoords[3];
			double weights[3];
			int subId;
			double value = 0;

			vtkIdType id = polydata->FindCell(pos, nullptr, 0, 0.0001, subId, pcoords, weights);
			if (id >= 0) {
				vtkIdList* ids = vtkIdList::New();
				polydata->GetCellPoints(id, ids);
				for (int i = 0; i < ids->GetNumberOfIds(); ++i)
					value += weights[i] * data->GetTuple1(ids->GetId(i));
				ids->Delete();
			}
			img.setDoublePixelAt(x, y, value);
		}
	}

	display.setText("<b>Save to file...</b>");
	img.save(out_file);
	// GetImageTreeWidget()->AddItem(img);
}

void VipFOVTreeWidget::saveSpatialCalibrationFile()
{
	QList<VipFOVItem*> items = selectedFieldOfViewItems();
	if (items.size() != 1) {
		VIP_LOG_ERROR("Only one field of view can be selected");
		return;
	}

	items.first()->saveSpatialCalibrationFile();
}

void VipFOVTreeWidget::displaySpatialCalibration()
{
	QList<VipFOVItem*> items = selectedFieldOfViewItems();
	if (items.size() != 1) {
		VIP_LOG_ERROR("Only one field of view can be selected");
		return;
	}

	items.first()->displaySpatialCalibration();
}

void VipFOVTreeWidget::saveAttributeFieldOfView()
{
	QList<VipFOVItem*> items = selectedFieldOfViewItems();
	if (items.size() != 1) {
		VIP_LOG_ERROR("Only one field of view can be selected");
		return;
	}

	VipPlotFieldOfView* plot = items.first()->plotFov();
	if (!plot)
		return;

	VipFieldOfView fov = plot->rawData();

	// check that every needed variables are properly defined
	VipVTKPlayer* main = VipVTKPlayer::fromChild(view());
	VipVTKObject::AttributeType type = main->attributes()->currentAttributeType();
	QString name = main->attributes()->currentAttribute();
	int comp = main->attributes()->currentComponent();

	// create and display the choice widget (between direct rendering, z buffer and current attribute
	QWidget* choice = new QWidget();
	QVBoxLayout* vlay = new QVBoxLayout();
	QRadioButton* direct = new QRadioButton("Direct rendering", choice);
	QRadioButton * zbuffer = new  QRadioButton("Depth image",choice);
	QRadioButton* attribute = new QRadioButton("Current attribute", choice);
	vlay->addWidget(direct);
	vlay->addWidget(zbuffer);
	vlay->addWidget(attribute);
	choice->setLayout(vlay);
	direct->setChecked(true);

	VipGenericDialog dialog(choice, "Select image type");
	if (dialog.exec() == QDialog::Rejected)
		return;

	if (direct->isChecked()) {
		items.first()->moveCamera();
		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save image file", "Image file (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)");
		if (!filename.isEmpty())
			saveImage(filename, d_data->view, fov, VTK_RGBA);
	}
	else if (attribute->isChecked()) {
		VipVTKObjectList lst = fromPlotVipVTKObject( d_data->view->objects());

		if (type == VipVTKObject::Unknown || type == VipVTKObject::Cell || name == "None") {
			std::cerr << "Wrong attribute selection" << std::endl;
			return;
		}

		// check that at least one object hase the coordinates attributes
		bool has_attribute = false;
		for (int i = 0; i < lst.size(); ++i) {
			if (lst[i].hasAttribute(VipVTKObject::Point, fov.name + " coordinates")) {
				has_attribute = true;
				break;
			}
		}
		if (!has_attribute) {
			std::cerr << "Pixel coordinates not found for this camera" << std::endl;
			return;
		}

		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save image file", "Image file (*.txt *.vti)");
		if (!filename.isEmpty())
			saveImage(filename, lst, fov, type, name, comp);
	}
	else {
		// save z buffer
		items.first()->moveCamera();
		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save image file", "Image file (*.txt *.vti)");
		if (!filename.isEmpty())
			saveImage(filename, d_data->view, fov, VTK_ZBUFFER);
	}
}

void VipFOVTreeWidget::edit()
{
	// because of a bug in Qt (see https://bugreports.qt.io/browse/QTBUG-56280),
	// calling resetSize() on the VipFOVSequenceEditorTool before show() produce an infinit loop
	static bool reset_size_once = false;

	QList<VipFOVItem*> fovs = selectedFieldOfViewItems();
	VipFOVItem* item = fovs.first();
	if (VipFOVSequence * seq = item->source())
	{
		VipFieldOfView fov = item->plotFov()->rawData();
		
		vipGetFOVSequenceEditorTool()->editor()->SetSequence(seq);
		vipGetFOVSequenceEditorTool()->editor()->SetGraphicsView(d_data->view);
		vipGetFOVSequenceEditorTool()->editor()->SetFOVItem(item);
		vipGetFOVSequenceEditorTool()->show();
		vipGetFOVSequenceEditorTool()->setWindowTitle("Field Of View editor - " + fov.name);
		
		if (!reset_size_once) {
			vipGetFOVSequenceEditorTool()->resetSize();
			reset_size_once = true;
		}
	}
	/*else
	{
		VipFOVEditor * editor = new VipFOVEditor();
		editor->setFieldOfView(item->fieldOfViewItem());
		VipGenericDialog dialog(editor, "edit field of view");

		if (dialog.exec() == QDialog::Accepted)
		{
			item->setFieldOfView(editor->fieldOfViews());
		}
	}*/
}

void VipFOVTreeWidget::create()
{
	vtkCamera* camera = d_data->view->renderer()->GetActiveCamera();

	double pupile[3], target[3], view_up[3];
	camera->GetPosition(pupile);
	camera->GetFocalPoint(target);
	camera->GetViewUp(view_up);
	double fov_angle = camera->GetViewAngle();

	double view_up_cam_wanted[3];
	double view_up_cam_current[3];
	double view_up_machine[3] = { 0, 0, 1 };
	vtkMatrix4x4* view_tr = camera->GetViewTransformMatrix();
	vtkMatrix3x3* mat = vtkMatrix3x3::New();
	mat->SetElement(0, 0, view_tr->GetElement(0, 0));
	mat->SetElement(1, 0, view_tr->GetElement(1, 0));
	mat->SetElement(2, 0, view_tr->GetElement(2, 0));
	mat->SetElement(0, 1, view_tr->GetElement(0, 1));
	mat->SetElement(1, 1, view_tr->GetElement(1, 1));
	mat->SetElement(2, 1, view_tr->GetElement(2, 1));
	mat->SetElement(0, 2, view_tr->GetElement(0, 2));
	mat->SetElement(1, 2, view_tr->GetElement(1, 2));
	mat->SetElement(2, 2, view_tr->GetElement(2, 2));
	mat->MultiplyPoint(view_up_machine, view_up_cam_wanted);
	mat->MultiplyPoint(view_up, view_up_cam_current);
	mat->Delete();

	// compute rotation to a view up of 0,0,1

	/*double rotation = std::acos( (view_up_cam_wanted[0]*view_up_cam_current[0]+view_up_cam_wanted[1]*view_up_cam_current[1]) /
			(sqrt(view_up_cam_wanted[0]*view_up_cam_wanted[0]+view_up_cam_wanted[1]*view_up_cam_wanted[1])*
					sqrt(view_up_cam_current[0]*view_up_cam_current[0]+view_up_cam_current[1]*view_up_cam_current[1]))) * 57.295779513;*/

	double dot = view_up_cam_wanted[0] * view_up_cam_current[0] + view_up_cam_wanted[1] * view_up_cam_current[1]; // dot product
	double det = view_up_cam_wanted[0] * view_up_cam_current[1] - view_up_cam_wanted[1] * view_up_cam_current[0]; // determinant
	double rotation = -std::atan2(det, dot) * 57.295779513;
	;

	VipFOVEditor* editor = new VipFOVEditor();
	editor->pupilPos.SetValue(pupile);
	editor->targetPoint.SetValue(target);
	editor->horizontalFov.setText(QString::number(fov_angle));
	editor->verticalFov.setText(QString::number(fov_angle));
	editor->pixWidth.setValue(320);
	editor->pixHeight.setValue(240);
	editor->rotation.setText(QString::number(rotation));
	editor->viewUp.setCurrentIndex(2);

	VipGenericDialog dialog(editor, "edit field of view");

	if (dialog.exec() == QDialog::Accepted) {
		// add a VipFOVSequence in this player
		VipFOVSequence* seq = new VipFOVSequence();
		VipFieldOfView fov = editor->fieldOfView();

		// use the current workspace time
		if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
			fov.time = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->time();
		if (fov.time == VipInvalidTime)
			fov.time = 0;
		seq->add(fov);
		seq->open(VipIODevice::ReadOnly);
		VipVTKPlayer* main = VipVTKPlayer::fromChild(d_data->view);
		seq->setParent(main->processingPool());
		vipCreatePlayersFromProcessing(seq, main);
	}
}

void VipFOVTreeWidget::resetCamera()
{
	d_data->view->renderer()->ResetCamera();
}

VipPlotSpectrogram* VipFOVTreeWidget::acceptDrop(const QMimeData* mime)
{
	if (const VipMimeDataProcessingObjectList* data = qobject_cast<const VipMimeDataProcessingObjectList*>(mime))
		return nullptr;

	if (const VipBaseDragWidgetMimeData* data = qobject_cast<const VipBaseDragWidgetMimeData*>(mime))
		if (const VipMultiDragWidget* multi = qobject_cast<const VipMultiDragWidget*>(data->dragWidget))
			if (multi->count() == 1)
				if (VipDragWidget* drag = qobject_cast<VipDragWidget*>(multi->widget(0, 0, 0)))
					if (VipVideoPlayer* pl = qobject_cast<VipVideoPlayer*>(drag->widget()))
						return pl->spectrogram();

	/*if (const VipPlotMimeData * data = qobject_cast<const VipPlotMimeData*>(mime))
	{
		QList<VipPlotItem*> items = data->plotData(nullptr,this);
		if (items.size() == 1)
			if (VipPlotSpectrogram * spec = qobject_cast<VipPlotSpectrogram*>(items.first()))
				return spec;
	}*/

	return nullptr;
}

void VipFOVTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
	const QMimeData* mime = event->mimeData();

	if (acceptDrop(mime))
		event->acceptProposedAction();
}

void VipFOVTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
	event->accept();
}

void VipFOVTreeWidget::dropEvent(QDropEvent* event)
{
	/*if(VipPlotSpectrogram * spec = acceptDrop(event->mimeData()))
	{
		QTreeWidgetItem * item = d_data->tree.itemAt(event->pos());
		if (!item || item == d_data->tree.topLevelItem(0))
			return;

		VipFOVItem * fov = static_cast<VipFOVItem*>(item);

		//if the spectrogram is the output of a VipDisplayObject, connect its source to the mapper
		VipOutput * src = nullptr;
		if (VipDisplayObject * display = spec->property("VipDisplayObject").value<VipDisplayObject*>())
		{
			src = display->inputAt(0)->connection()->source();
		}

		fov->applyMapping(true,false, spec);
	}*/
}

void VipFOVTreeWidget::mouseMoveEvent(QMouseEvent*)
{
	/*if (e->buttons() == Qt::LeftButton)
	{
		QList<VipFOVItem*> selection = selectedFieldOfViewItems();
		if (selection.size())
		{
			QList<VipProcessingObject*> objects;
			for (int i = 0; i < selection.size(); ++i)
			{
				if (VipDisplayObject * display = selection[i]->display())
					objects << display;
			}

			if (objects.size())
			{
				VipMimeDataProcessingObjectList * mime = new VipMimeDataProcessingObjectList();
				mime->setCoordinateSystem(VipCoordinateSystem::Cartesian);
				mime->setProcessing(objects);
				QDrag drag(this);
				drag.setMimeData(mime);
				drag.exec();
				return;
			}

		}
	}*/
}

void VipFOVTreeWidget::keyPressEvent(QKeyEvent* evt) 
{
	if (evt->key() == Qt::Key_Delete) {
		deleteSelection();
		evt->accept();
	}
}

class CustomLabel : public QLabel
{
protected:
	virtual void mouseDoubleClickEvent(QMouseEvent* evt) { evt->ignore(); }
};

VipVTKObjectItem::VipVTKObjectItem(QTreeWidgetItem* parent)
  : QTreeWidgetItem(parent)
{
	visible = Visible;
	edge = Hidden;

	toolBar = new QToolBar();
	toolBar->setIconSize(QSize(12, 12));
	toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
	// toolBar->setMaximumHeight(18);
	// toolBar->setMaximumWidth(250);
	//toolBar->setStyleSheet("QToolBar {border-style: flat;}");

	/*QWidget* empty = new QWidget();
	empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
	toolBar->addWidget(empty);*/

	visibility = toolBar->addAction(vipIcon("visible.png"), "Switch Visible/Highlighted/Hidden");
	drawEdge = toolBar->addAction(vipIcon("hide_edge.png"), "Draw/Hide edge");
	opacity = toolBar->addWidget(new QSlider(Qt::Horizontal));

	label = new CustomLabel();
	toolBar->addWidget(label);

	opacitySlider()->setToolTip("Change opacity");
	opacitySlider()->setRange(0, 100);
	opacitySlider()->setValue(100);

	this->treeWidget()->setItemWidget(this, 0, toolBar);
	// this->setSizeHint(0,QSize(150,20));

	updateItem();

	connect(visibility, SIGNAL(triggered(bool)), this, SLOT(visibilityChanged()));
	connect(drawEdge, SIGNAL(triggered(bool)), this, SLOT(drawEdgeChanged()));
	connect(opacitySlider(), SIGNAL(valueChanged(int)), this, SLOT(opacityChanged()));
}

void VipVTKObjectItem::setText(const QString& str)
{
	label->setText(str);
	toolBar->setMaximumWidth(120 + label->sizeHint().width());
	toolBar->setMinimumWidth(120 + label->sizeHint().width());
	this->setSizeHint(0, QSize(120 + label->sizeHint().width(), 20));
}

QString VipVTKObjectItem::text() const
{
	return label->text();
}

QSlider* VipVTKObjectItem::opacitySlider()
{
	return static_cast<QSlider*>(static_cast<QWidgetAction*>(opacity)->defaultWidget());
}

PlotVipVTKObjectList VipVTKObjectItem::children() const
{
	PlotVipVTKObjectList res;
	if (this->plot)
		res << this->plot;

	for (int i = 0; i < this->childCount(); ++i) {
		VipVTKObjectItem* item = static_cast<VipVTKObjectItem*>(this->child(i));
		res << item->children();
	}

	return res;
	;
}

void VipVTKObjectItem::updateItem()
{
	if (plot && plot->hasActor()) {
		QString name = plot->dataName();
		QStringList lst = name.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
		if (lst.size())
			name = lst.last();
		if (text() != name)
			setText(name);

		// dataObject->actor()->GetProperty()->setEdgeColor(0,0,0);
		if (!plot->actor()->GetVisibility())
			visible = Hidden;

		if (!plot->edgeVisible()) // actor()->GetProperty()->GetEdgeVisibility())
			edge = Hidden;
		else
			edge = Visible;

		opacitySlider()->blockSignals(true);
		opacitySlider()->setValue((int)(plot->actor()->GetProperty()->GetOpacity() * 100));
		opacitySlider()->blockSignals(false);

		if (this->isSelected() != plot->isSelected())
			this->setSelected(plot->isSelected());
	}

	if (visible == Hidden)
		visibility->setIcon(vipIcon("hidden.png"));
	else
		visibility->setIcon(vipIcon("visible.png"));

	if (edge == Hidden)
		drawEdge->setIcon(vipIcon("hide_edge.png"));
	else
		drawEdge->setIcon(vipIcon("show_edge.png"));
}

void VipVTKObjectItem::setPlotObject(VipPlotVTKObject* pl)
{
	if (plot) {
		disconnect(plot, SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(updateItem()));
		disconnect(plot, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(updateItem()));
	}

	plot = pl;
	if (pl) {
		this->setToolTip(0, pl->rawData().description(-1, -1));
		connect(pl, SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(updateItem()));
		connect(pl, SIGNAL(selectionChanged(VipPlotItem*)), this, SLOT(updateItem()));

		name = pl->dataName();
	}

	updateItem();
}

VipPlotVTKObject* VipVTKObjectItem::plotObject() const
{
	return plot;
}

void VipVTKObjectItem::setVisibility(State state)
{
	visibilityChanged(this, state);
}

void VipVTKObjectItem::setDrawEdge(State state)
{
	drawEdgeChanged(this, state);
}

void VipVTKObjectItem::setOpacity(double opacity)
{
	opacityChanged(this, opacity);
}

void VipVTKObjectItem::visibilityChanged(VipVTKObjectItem* item, State state)
{
	item->visible = state;
	auto *d = item->plotObject();
	if (d && d->actor()) {
		if (state == Visible)
			d->setVisible(true);
		else
			d->setVisible(false);
	}
	item->updateItem();

	for (int i = 0; i < item->childCount(); ++i)
		visibilityChanged(static_cast<VipVTKObjectItem*>(item->child(i)), state);
}

void VipVTKObjectItem::drawEdgeChanged(VipVTKObjectItem* item, State state)
{
	item->edge = state;
	auto *d = item->plotObject();
	if (d && d->actor()) {
		/*if(state == Visible )
			item->data()->actor()->GetProperty()->EdgeVisibilityOn();
		else
			item->data()->actor()->GetProperty()->EdgeVisibilityOff();*/
		if (state == Visible)
			d->setEdgeVisible(true);
		else
			d->setEdgeVisible(false);
	}
	item->updateItem();

	for (int i = 0; i < item->childCount(); ++i)
		drawEdgeChanged(static_cast<VipVTKObjectItem*>(item->child(i)), state);
}

void VipVTKObjectItem::opacityChanged(VipVTKObjectItem* item, double opacity)
{
	if (item->plotObject() ) {
		item->plotObject()->setOpacity(opacity);
	}
	else {
		item->opacitySlider()->blockSignals(true);
		item->opacitySlider()->setValue((int)(opacity * 100));
		item->opacitySlider()->blockSignals(false);
	}

	item->updateItem();

	for (int i = 0; i < item->childCount(); ++i)
		opacityChanged(static_cast<VipVTKObjectItem*>(item->child(i)), opacity);
}

void VipVTKObjectItem::visibilityChanged()
{
	if (visible == Visible)
		visible = Hidden;
	else
		visible = Visible;

	visibilityChanged(this, visible);
	updateView();
}

void VipVTKObjectItem::drawEdgeChanged()
{
	if (edge == Visible)
		edge = Hidden;
	else
		edge = Visible;

	drawEdgeChanged(this, edge);
	updateView();
}

void VipVTKObjectItem::opacityChanged()
{
	opacityChanged(this, opacitySlider()->value() / 100.0);
	updateView();
}

void VipVTKObjectItem::updateView()
{
	static_cast<VipVTKObjectTreeWidget*>(this->treeWidget()->parent())->view()->refresh();
}

bool VipVTKObjectItem::isSync() const
{
	if (plot) {
		return plot->dataName() == name;
	}
	return plot;
}





VipXYZValueWidget::VipXYZValueWidget(QWidget* parent)
  : QWidget(parent)
  , fieldAttributes(this)
  , pointAttributes(this)
{
	fieldAttributes.setText("<b>Select field attributes</b>");
	pointAttributes.setText("<b>Select point attributes</b>");
	setLayout(new QVBoxLayout());
}

void VipXYZValueWidget::SetDataObjects(const VipVTKObjectList& lst)
{
	for (int i = 0; i < fieldBoxes.size(); ++i)
		delete fieldBoxes[i];
	for (int i = 0; i < pointBoxes.size(); ++i)
		delete pointBoxes[i];
	fieldBoxes.clear();
	pointBoxes.clear();
	fieldAttributes.hide();
	pointAttributes.hide();

	QStringList field = commonAttributes(lst, VipVTKObject::Field);
	QStringList point = commonAttributes(lst, VipVTKObject::Point);

	QVBoxLayout* vlay = new QVBoxLayout();

	fieldAttributes.setVisible(field.size() > 0);
	vlay->addWidget(&fieldAttributes);
	for (QStringList::iterator it = field.begin(); it != field.end(); ++it) {
		fieldBoxes << new QCheckBox(*it, this);
		fieldBoxes.back()->setProperty("value", *it);
		fieldBoxes.back()->setProperty("order", 0);
		vlay->addWidget(fieldBoxes.back());
		connect(fieldBoxes.back(), SIGNAL(clicked(bool)), this, SLOT(Checked(bool)));
	}

	pointAttributes.setVisible(point.size() > 0);
	vlay->addWidget(&pointAttributes);
	for (QStringList::iterator it = point.begin(); it != point.end(); ++it) {
		pointBoxes << new QCheckBox(*it, this);
		pointBoxes.back()->setProperty("value", *it);
		pointBoxes.back()->setProperty("order", 0);
		vlay->addWidget(pointBoxes.back());
		connect(pointBoxes.back(), SIGNAL(clicked(bool)), this, SLOT(Checked(bool)));
	}

	delete layout();
	setLayout(vlay);
}

QList<VipXYZValueWidget::Attribute> VipXYZValueWidget::SelectedAttributes() const
{
	QMap<int, Attribute> res;

	for (int i = 0; i < pointBoxes.size(); ++i)
		if (pointBoxes[i]->isChecked())
			res[pointBoxes[i]->property("order").toInt()] = Attribute{ VipVTKObject::Point, pointBoxes[i]->property("value").toString() };

	for (int i = 0; i < fieldBoxes.size(); ++i)
		if (fieldBoxes[i]->isChecked())
			res[fieldBoxes[i]->property("order").toInt()] = Attribute{ VipVTKObject::Field, fieldBoxes[i]->property("value").toString() };

	return res.values();
}

void VipXYZValueWidget::Checked(bool check)
{
	if (check) {
		int order = -1;
		// find maximum order
		for (int i = 0; i < pointBoxes.size(); ++i)
			order = qMax(pointBoxes[i]->property("order").toInt(), order);
		for (int i = 0; i < fieldBoxes.size(); ++i)
			order = qMax(fieldBoxes[i]->property("order").toInt(), order);
		// set the order of selected item (last one)
		sender()->setProperty("order", order + 1);
	}
	else {
		int order = sender()->property("order").toInt();
		sender()->setProperty("order", 0);

		// decrement all orders above this one
		for (int i = 0; i < pointBoxes.size(); ++i) {
			int o = pointBoxes[i]->property("order").toInt();
			if (o > order)
				pointBoxes[i]->setProperty("order", --o);
		}

		for (int i = 0; i < fieldBoxes.size(); ++i) {
			int o = fieldBoxes[i]->property("order").toInt();
			if (o > order)
				fieldBoxes[i]->setProperty("order", --o);
		}
	}

	// update the text of all check box

	for (int i = 0; i < pointBoxes.size(); ++i) {
		int order = pointBoxes[i]->property("order").toInt();
		if (order > 0)
			pointBoxes[i]->setText("(" + QString::number(order) + ") " + pointBoxes[i]->property("value").toString());
		else
			pointBoxes[i]->setText(pointBoxes[i]->property("value").toString());
	}

	for (int i = 0; i < fieldBoxes.size(); ++i) {
		int order = fieldBoxes[i]->property("order").toInt();
		if (order > 0)
			fieldBoxes[i]->setText("(" + QString::number(order) + ") " + fieldBoxes[i]->property("value").toString());
		else
			fieldBoxes[i]->setText(fieldBoxes[i]->property("value").toString());
	}
}






class VipVTKObjectTreeWidget::PrivateData
{
public:
	PlotVipVTKObjectList selected;
	QPointer<VipVTKGraphicsView> view;
	QTreeWidget* tree;

	VipVTKObjectItem* inFile;

	QTimer synchro;
	bool destroy{ false };

	QToolBar* bar;
	QSpinBox* maxDepth;
	QAction* unselectAll;
	QAction* reset;
	QAction* expandAll;
	QLabel* selectCount;
};

VipVTKObjectTreeWidget::VipVTKObjectTreeWidget(VipVTKGraphicsView* v, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->tree = new QTreeWidget();
	d_data->view = v;

	d_data->bar = new QToolBar();
	d_data->unselectAll = d_data->bar->addAction(vipIcon("list.png"), "Unselect all");
	d_data->reset = d_data->bar->addAction(vipIcon("reset.png"), "<b>Reset selected itsm</b><br>Reset colors, layer, edge color and visibility...");
	d_data->expandAll = d_data->bar->addAction(vipIcon("shortcuts.png"), "Expand all");
	d_data->bar->addWidget(d_data->maxDepth = new QSpinBox());
	d_data->bar->addWidget(d_data->selectCount = new QLabel());

	d_data->maxDepth->setRange(1, 10);
	d_data->maxDepth->setValue(3);
	d_data->maxDepth->setToolTip("Maximum tree depth");
	d_data->selectCount->setToolTip("Selected items count");

	connect(d_data->unselectAll, SIGNAL(triggered(bool)), this, SLOT(unselectAll()));
	connect(d_data->reset, SIGNAL(triggered(bool)), this, SLOT(resetSelection()));
	connect(d_data->expandAll, SIGNAL(triggered(bool)), this, SLOT(expandAll()));
	connect(d_data->maxDepth, SIGNAL(valueChanged(int)), this, SLOT(setMaxDepth(int)));
	connect(d_data->tree, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));

	d_data->tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->tree->headerItem()->setHidden(true);
	d_data->tree->header()->setMinimumSectionSize(500);
	// this->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	// this->setColumnCount(2);
	// this->setIndentation(10);
	//d_data->tree->setStyleSheet("QTreeWidget {border-style: flat;}");
	d_data->tree->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	d_data->tree->setAcceptDrops(true);

	d_data->tree->viewport()->installEventFilter(this);

	d_data->inFile = new VipVTKObjectItem(d_data->tree->invisibleRootItem());
	d_data->inFile->setText("<b>CAD files");
	d_data->inFile->setSizeHint(0, QSize(250, 30));

	d_data->tree->addTopLevelItem(d_data->inFile);

	connect(d_data->tree, SIGNAL(itemPressed(QTreeWidgetItem*, int)), this, SLOT(itemPressed(QTreeWidgetItem*, int)));
	
	// update the tree every second
	d_data->synchro.setSingleShot(false);
	d_data->synchro.setInterval(1000);
	connect(&d_data->synchro, SIGNAL(timeout()), this, SLOT(synchronize()), Qt::QueuedConnection);
	d_data->synchro.start();

	QVBoxLayout* lay = new QVBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(d_data->bar);
	lay->addWidget(d_data->tree);
	setLayout(lay);
}

VipVTKObjectTreeWidget::~VipVTKObjectTreeWidget()
{
	d_data->tree->viewport()->removeEventFilter(this);
}

VipVTKGraphicsView* VipVTKObjectTreeWidget::view()
{
	return d_data->view;
}

int VipVTKObjectTreeWidget::maxDepth() const
{
	return d_data->maxDepth->value();
}

void VipVTKObjectTreeWidget::findInItem(VipVTKObjectItem* item, PlotVipVTKObjectList& lst) const
{
	if (item->plotObject())
		lst << item->plotObject();

	for (int i = 0; i < item->childCount(); ++i)
		findInItem(static_cast<VipVTKObjectItem*>(item->child(i)), lst);
}

VipVTKObjectItem* VipVTKObjectTreeWidget::findInfo(VipVTKObjectItem*, const QFileInfo&) const
{
	/*if(item->data() && item->data()->Info() == path)
	{
		return item;
	}

	for(int i=0; i < item->childCount(); ++i)
	{
		VipVTKObjectItem * it = findInfo(static_cast<VipVTKObjectItem*>(item->child(i)),path);
		if(it)
			return it;
	}
	*/
	return nullptr;
}

void VipVTKObjectTreeWidget::unselectAll()
{
	d_data->tree->blockSignals(true);

	auto lst = selectedObjects();
	for (int i = 0; i < lst.size(); ++i) {
		lst[i]->setSelected(false);
	}

	this->resynchronize();

	for (int i = 0; i < d_data->tree->topLevelItemCount(); ++i)
		d_data->tree->topLevelItem(i)->setSelected(false);

	this->selectionChanged();

	d_data->tree->blockSignals(false);
}

static void expand(QTreeWidgetItem* item)
{
	item->setExpanded(true);
	for (int i = 0; i < item->childCount(); ++i)
		expand(item->child(i));
}

void VipVTKObjectTreeWidget::expandAll()
{
	d_data->tree->expandAll();
}

void VipVTKObjectTreeWidget::clear()
{
	d_data->tree->blockSignals(true);
	// clear the tree
	while (d_data->inFile->childCount())
		delete d_data->inFile->child(0);

	d_data->tree->blockSignals(false);
}

bool VipVTKObjectTreeWidget::isSync()
{
	QList<VipVTKObjectItem*> next;
	for (int i = 0; i < d_data->tree->topLevelItemCount(); ++i)
		next << static_cast<VipVTKObjectItem*>(d_data->tree->topLevelItem(i));

	while (next.size()) {
		QList<VipVTKObjectItem*> tmp = next;
		next.clear();

		for (int i = 0; i < tmp.size(); ++i) {
			VipVTKObjectItem* item = tmp[i];
			if (item->plotObject() && !item->isSync())
				return false;

			for (int c = 0; c < item->childCount(); ++c)
				next << static_cast<VipVTKObjectItem*>(item->child(c));
		}
	}

	return vipToSet(view()->objects()) == vipToSet(this->objects());
}

void VipVTKObjectTreeWidget::setMaxDepth(int d)
{
	d_data->maxDepth->blockSignals(true);
	d_data->maxDepth->setValue(d);
	d_data->maxDepth->blockSignals(false);
	clear();
	this->addObject(view()->objects());
}

void VipVTKObjectTreeWidget::resetSelection()
{
	QList<QTreeWidgetItem*> items = d_data->tree->selectedItems();
	for (QTreeWidgetItem* it : items) {
	
		VipVTKObjectItem* data = static_cast<VipVTKObjectItem*>(it);
		if (!data->plotObject())
			continue;

		QColor selected = data->plotObject()->selectedColor();
		vipGlobalActorParameters()->apply(data->plotObject());
		data->plotObject()->setSelectedColor(selected);
	}
	d_data->view->refresh();
}

void VipVTKObjectTreeWidget::resynchronize()
{
	synchronizeInternal(true);
}

void VipVTKObjectTreeWidget::synchronize()
{
	synchronizeInternal(false);
}

void VipVTKObjectTreeWidget::synchronizeInternal(bool force)
{
	if (!d_data->view)
		return;
	int pos = d_data->tree->verticalScrollBar()->sliderPosition();

	if (!isSync() || force) {
		// clear the tree
		clear();

		this->addObject(view()->objects());
	}

	if (view()->objects().size() == 0)
		hide();
	else
		show();

	// update number of points and cells
	auto lst = view()->objects();
	int point_count = 0, cell_count = 0;
	for (int i = 0; i < lst.size(); ++i) {
		vtkDataSet* set = lst[i]->rawData().dataSet();
		if (set) {
			point_count += set->GetNumberOfPoints();
			cell_count += set->GetNumberOfCells();
		}
	}

	d_data->inFile->setToolTip(0, "<b>Point count: </b>" + QString::number(point_count) + "<br>" + "<b>Cell count: </b>" + QString::number(cell_count));
	d_data->tree->verticalScrollBar()->setSliderPosition(pos);
}


static QString CommonRootDirectory(const PlotVipVTKObjectList& lst)
{
	if (!lst.size())
		return QString();

	QString root;
	bool first = true;

	// find the maximum common root directory for all VipVTKObject based on a file
	for (int i = 0; i < lst.size(); ++i) {
		// if (lst[i]->isFileData())
		{
			QString dir = VipPath(lst[i]->dataName()).filePath();
			dir.replace("\\", "/");

			if (first) {
				root = dir;
				first = false;
			}
			else {
				/*int index1 = dir.indexOf(root);
				int index2 = root.indexOf(dir);

				if (index2 >= 0)
					root = dir;
				else if (index1 < 0)
					root.clear();*/
				// compute the common part
				int i = 0;
				for (i = 0; i < dir.size(); ++i) {
					if (i < root.size()) {
						if (root[i] == dir[i])
							continue;
						else
							break;
					}
					else
						break;
				}

				root = root.mid(0, i);
			}
		}
	}
	return root;
}


void VipVTKObjectTreeWidget::addObject(const PlotVipVTKObjectList& m)
{
	QString root_files = CommonRootDirectory(m);
	for (auto it = m.begin(); it != m.end(); ++it) {
		// remove the root directory from the name
		QString path = (*it)->dataName();
		if (!path.isEmpty()) {
			path.replace("\\", "/");
			path.remove(root_files );
			if (path.startsWith("/"))
				path = path.mid(1);

			// Take into account the mximum number of subdirs
			QStringList lst = path.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
			if (lst.size() > maxDepth())
				lst = lst.mid(lst.size() - maxDepth());
			path = lst.join('/');
		}
		addObject(*it, path);
	}
}

void VipVTKObjectTreeWidget::addObject(VipPlotVTKObject* plot, const QString& name)
{
	QString path = name;
	if (path.isEmpty())
		path = plot->dataName();

	// add the data in the inFile item
	// VipVTKObjectItem * item = findInfo(obj->Info());
	// if(!item)
	VipVTKObjectItem* item = find(d_data->inFile, path);
	item->setPlotObject(plot);
	
	d_data->tree->expandAll();
	for (int i = 0; i < d_data->tree->columnCount(); i++)
		d_data->tree->resizeColumnToContents(i);
}

VipPlotVTKObject* VipVTKObjectTreeWidget::objectByName(const QString& name) 
{
	// look inside inFile
	VipVTKObjectItem* item = find(d_data->inFile, name, false);
	if (item)
		return item->plotObject();

	return nullptr;
}

bool VipVTKObjectTreeWidget::remove(VipPlotVTKObject * plot)
{
	// return remove(data->name());
	// look inside inFile
	if (plot) {
		plot->deleteLater();
		return true;
	}
	return false;
}

/**
remove items that does not have at least one CAD children (usually empty directories).
Returns true if the item does not have at least one CAD children.
 */
bool VipVTKObjectTreeWidget::cleanItem(VipVTKObjectItem* item)
{
	bool is_empty = true;
	for (int i = 0; i < item->childCount(); ++i) {
		VipVTKObjectItem* child = static_cast<VipVTKObjectItem*>(item->child(i));
		is_empty &= cleanItem(child);
	}

	if (item == d_data->inFile )
		return false;

	is_empty &= (!item->plotObject());
	if (is_empty) {
		delete item;
	}

	return is_empty;
}

bool VipVTKObjectTreeWidget::remove(const QString& name)
{
	// look inside inFile
	VipVTKObjectItem* item = find(d_data->inFile, name, false);
	if (item) {
		if (auto* it = item->plotObject())
			it->deleteLater();
		delete item;
		return true;
	}

	return false;
}

PlotVipVTKObjectList VipVTKObjectTreeWidget::objects() const
{
	PlotVipVTKObjectList res;
	for (int i = 0; i < d_data->inFile->childCount(); ++i)
		findInItem(static_cast<VipVTKObjectItem*>(d_data->inFile->child(i)), res);
	return res;
}

PlotVipVTKObjectList VipVTKObjectTreeWidget::selectedObjects() const
{
	PlotVipVTKObjectList res;
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();

	for (int i = 0; i < selected.size(); ++i) {
		res << static_cast<VipVTKObjectItem*>(selected[i])->children();
	}

	return vipUnique(res);
}

VipVTKObjectItem* VipVTKObjectTreeWidget::find(QTreeWidgetItem* root, const QString& path, bool create_if_needed)
{
	QStringList lst = path.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.size() == 0)
		lst << path; // return nullptr;

	// QTreeWidgetItem * root = this->invisibleRootItem();

	for (int i = 0; i < lst.size(); ++i) {
		const QString name = lst[i];
		QTreeWidgetItem* item = nullptr;

		// find child with name
		for (int j = 0; j < root->childCount(); ++j) {
			if (static_cast<VipVTKObjectItem*>(root->child(j))->text() == name) {
				item = root->child(j);
				break;
			}
		}

		if (!item && create_if_needed) {
			item = new VipVTKObjectItem(root);
			// std::cout<<"add item "<<name.toLatin1().data()<<std::endl;
			static_cast<VipVTKObjectItem*>(item)->setText(name);
		}
		else if (!item)
			return nullptr;

		root = item;
	}

	return static_cast<VipVTKObjectItem*>(root);
}

/* VipVTKObjectItem* VipVTKObjectTreeWidget::findInfo(const QFileInfo& path)
{
	QTreeWidgetItem* root = d_data->inFile;

	for(int i=0; i < root->childCount(); ++i)
	{
		VipVTKObjectItem * it = findInfo( static_cast<VipVTKObjectItem*>(root->child(i)), path);
		if(it)
			return it;
	}

	return nullptr;
}*/

void VipVTKObjectTreeWidget::itemPressed(QTreeWidgetItem* item, int)
{
	if (!(QApplication::mouseButtons() & Qt::RightButton))
		return;

	item->setSelected(true);
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();

	QMenu menu;
	// one single file
	if (selected.size() == 1 && static_cast<VipVTKObjectItem*>(selected[0])->plotObject()) {
		QAction* save = menu.addAction(vipIcon("save_as.png"), "Save a copy in file...");
		connect(save, SIGNAL(triggered(bool)), this, SLOT(saveInFile()));
	}
	// a directory or multiple files
	else {
		QAction* save = menu.addAction(vipIcon("open_dir.png"), "Save selected objects in directory...");
		connect(save, SIGNAL(triggered(bool)), this, SLOT(saveInDir()));
	}

	QAction* del = menu.addAction(vipIcon("del.png"), "Delete selection");
	QAction* copy = menu.addAction("Create a copy of selected items");
	menu.addSeparator();
	QAction* hide_others = menu.addAction(vipIcon("visible.png"), "Hide all but selection");
	QAction* show_others = menu.addAction(vipIcon("hidden.png"), "Show all but selection");
	menu.addSeparator();
	QAction* show_all = menu.addAction(vipIcon("visible.png"), "Show all");
	menu.addSeparator();
	QAction* save_points = menu.addAction("Save points (TEXT file)");
	QAction* save_xyzv = menu.addAction("Save selection attributes (XYZValue file)");

	connect(del, SIGNAL(triggered(bool)), this, SLOT(deleteSelection()));
	connect(copy, SIGNAL(triggered(bool)), this, SLOT(copySelection()));
	connect(hide_others, SIGNAL(triggered(bool)), this, SLOT(hideAllButSelection()));
	connect(show_others, SIGNAL(triggered(bool)), this, SLOT(showAllButSelection()));
	connect(show_all, SIGNAL(triggered(bool)), this, SLOT(showAll()));
	connect(save_xyzv, SIGNAL(triggered(bool)), this, SLOT(saveAttributeXYZValue()));
	connect(save_points, SIGNAL(triggered(bool)), this, SLOT(saveXYZ()));

	PlotVipVTKObjectList objects;
	for (int i = 0; i < selected.size(); ++i) {
		VipVTKObjectItem* item = static_cast<VipVTKObjectItem*>(selected[i]);
		if (item->plotObject() && item->plotObject()->actor())
			objects << item->plotObject();
	}
	if (objects.size()) {
		VipVTKActorParametersEditor* editor = new VipVTKActorParametersEditor(&menu);
		editor->setObjects(objects);
		QWidgetAction* action = new QWidgetAction(this);
		action->setDefaultWidget(editor);

		menu.addSeparator();
		menu.addAction(action);
	}

	menu.exec(QCursor::pos());
}

static void select(VipVTKObjectItem* item, bool s)
{
	auto *d = item->plotObject();
	if(d) 
		d->setSelected(s);

	if (item->isSelected() != s)
		item->setSelected(s);

	for (int i = 0; i < item->childCount(); ++i)
		select(static_cast<VipVTKObjectItem*>(item->child(i)), s);
}

static void apply_selection(VipVTKObjectItem* item)
{
	auto *d = item->plotObject();
	if(d)
		d->setSelected(item->isSelected());

	if (item->isSelected())
		select(item, true);

	for (int i = 0; i < item->childCount(); ++i) {
		apply_selection(static_cast<VipVTKObjectItem*>(item->child(i)));
	}
}

void VipVTKObjectTreeWidget::selectionChanged()
{
	d_data->view->setUpdatesEnabled(false);
	d_data->tree->blockSignals(true);

	for (int i = 0; i < d_data->tree->topLevelItemCount(); ++i)
		apply_selection(static_cast<VipVTKObjectItem*>(d_data->tree->topLevelItem(i)));

	//TODO (?)
	//view()->selectionChanged();

	d_data->selected.clear();

	QList<QTreeWidgetItem*> items = d_data->tree->selectedItems();
	for (int i = 0; i < items.size(); ++i) {
		// extend selection (select child items)
		VipVTKObjectItem* it = static_cast<VipVTKObjectItem*>(items[i]);
		d_data->selected << it->children();
	}
	d_data->tree->blockSignals(false);

	d_data->view->setUpdatesEnabled(true);

	PlotVipVTKObjectList lst = selectedObjects();
	d_data->selectCount->setText(QString::number(lst.size()));
	//if (lst.size())
	//	if (auto* plot = lst.last()->plotObject()) {
	//		vipGetProcessingEditorToolWidget()->setPlotItem(plot);
	//	}
}

void VipVTKObjectTreeWidget::saveInFile()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	if (selected.size() == 1 && static_cast<VipVTKObjectItem*>(selected[0])->plotObject()) {
		VipVTKObject data = static_cast<VipVTKObjectItem*>(selected[0])->plotObject()->rawData();
		if (!data)
			return;

		VipAnyData any(QVariant::fromValue(data), 0);
		any.setSource(1); // do NOT set a nullptr source or the data might not be loaded back
		any.setName(data.dataName());

		QStringList filters = VipIODevice::possibleWriteFilters(QString(), QVariantList() << any.data());
		QString filename = VipFileDialog::getSaveFileName(nullptr, "Save data", filters.join(";;"));
		if (!filename.isEmpty()) {
			VipProgress progress;
			progress.setRange(0, 0);
			progress.setText("<b>Save</b> " + QFileInfo(filename).fileName() + "...");
			progress.setModal(true);
			QList<VipProcessingObject::Info> devices = VipIODevice::possibleWriteDevices(filename, QVariantList() << any.data());
			VipIODevice* device = VipCreateDevice::create(devices);
			if (device) {
				// if the device'w input is a VipMultiInput, add an input
				if (VipMultiInput* in = device->topLevelInputAt(0)->toMultiInput())
					in->add();
				device->setPath(filename);
				device->open(VipIODevice::WriteOnly);
				device->inputAt(0)->setData(any);
				device->update();
				delete device;

				VIP_LOG_INFO("Data saved to file " + filename);
			}
			else {
				VIP_LOG_ERROR("Unable to save file " + filename);
			}
		}
	}
}

void VipVTKObjectTreeWidget::saveInDir()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	PlotVipVTKObjectList lst;

	for (int i = 0; i < selected.size(); ++i)
		lst << static_cast<VipVTKObjectItem*>(selected[i])->children();

	auto data_lst = fromPlotVipVTKObject(lst);

	QComboBox* combo = new QComboBox();
	combo->addItems((QStringList() << "default") + supportedFileSuffix(data_lst));
	VipGenericDialog dialog(combo, "CAD files extension");
	if (dialog.exec() != QDialog::Accepted)
		return;

	QString dirname = VipFileDialog::getExistingDirectory(nullptr, "Save files in directory");
	if (!dirname.isEmpty()) {
		VipVTKObject::saveToDirectory(data_lst, dirname, combo->currentText());

		clear(); // clear the tree. It will be updated automatically
	}
}

void VipVTKObjectTreeWidget::deleteSelection()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	PlotVipVTKObjectList lst;

	for (int i = 0; i < selected.size(); ++i) {
		lst << static_cast<VipVTKObjectItem*>(selected[i])->children();
	}
	vipUnique(lst);

	for (int i = 0; i < lst.size(); ++i)
		this->remove(lst[i]);

	cleanItem(d_data->inFile);

}

void VipVTKObjectTreeWidget::copySelection()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	PlotVipVTKObjectList lst;

	for (int i = 0; i < selected.size(); ++i) {
		lst << static_cast<VipVTKObjectItem*>(selected[i])->children();
	}
	vipUnique(lst);

	for (int i = 0; i < lst.size(); ++i) {
		auto data = lst[i]->rawData().copy();

		QString name = QFileInfo(lst[i]->dataName()).fileName();
		QString fname = QFileInfo(name).baseName();
		QString suffix = QFileInfo(name).suffix();

		// Create a new name
		if (view()->objectByName(name)) {

			// find real fname, without '_num'
			int idx = fname.lastIndexOf('_');
			if (idx < 0)
				fname += "_";
			else {
				bool ok = false;
				int num = fname.mid(idx + 1).toInt(&ok);
				if (!ok)
					fname += "_";
				else
					fname = fname.mid(0, idx + 1);
			}

			// find unused idx
			idx = 1;
			for (;;) {
				QString _fname = fname + QString::number(idx);
				QString real_name = _fname;
				if (!suffix.isEmpty())
					real_name += "." + suffix;
				if (!view()->objectByName(real_name)) {
					fname = _fname;
					break;
				}
				++idx;
			}
		}
		name = fname;
		if (!suffix.isEmpty())
			name += "." + suffix;

		data.setDataName(name);

		VipPlotVTKObject* plot = new VipPlotVTKObject();
		plot->setRawData(data);
		plot->setAxes(view()->area()->bottomAxis(), view()->area()->leftAxis(), VipCoordinateSystem::Cartesian);
	}
}

void VipVTKObjectTreeWidget::hideAllButSelection()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	d_data->inFile->setVisibility(VipVTKObjectItem::Hidden);

	for (int i = 0; i < selected.size(); ++i) {
		VipVTKObjectItem* item = static_cast<VipVTKObjectItem*>(selected[i]);
		item->setVisibility(VipVTKObjectItem::Visible);
	}
}

void VipVTKObjectTreeWidget::showAllButSelection()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	d_data->inFile->setVisibility(VipVTKObjectItem::Visible);

	for (int i = 0; i < selected.size(); ++i) {
		VipVTKObjectItem* item = static_cast<VipVTKObjectItem*>(selected[i]);
		item->setVisibility(VipVTKObjectItem::Hidden);
	}
}

void VipVTKObjectTreeWidget::showAll()
{
	QList<QTreeWidgetItem*> selected = d_data->tree->selectedItems();
	d_data->inFile->setVisibility(VipVTKObjectItem::Visible);
}

void VipVTKObjectTreeWidget::saveAttributeXYZValue()
{
	VipXYZValueWidget* widget = new VipXYZValueWidget();
	widget->SetDataObjects(fromPlotVipVTKObject( d_data->view->selectedObjects()));

	VipGenericDialog dialog(widget, "Select attributes");
	if (dialog.exec() != QDialog::Accepted)
		return;
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save XYZValue file", "Text file (*.txt *.csv)");
	if (filename.isEmpty())
		return;

	bool is_csv = QFileInfo(filename).suffix().compare("csv", Qt::CaseInsensitive) == 0;

	VipXYZValueWidget::AttributeList attr = widget->SelectedAttributes();

	VipVTKObjectList lst = fromPlotVipVTKObject( this->selectedObjects());
	if (lst.isEmpty()) {
		VIP_LOG_ERROR("Empty selection");
		return;
	}

	VipXYZAttributesWriter writer;
	writer.setPath(filename);
	writer.setFormat(is_csv ? VipXYZAttributesWriter::CSV : VipXYZAttributesWriter::TXT);
	writer.setAttributeList(attr);
	writer.topLevelInputAt(0)->toMultiInput()->resize(lst.size());
	for (int i = 0; i < lst.size(); ++i)
		writer.inputAt(i)->setData(QVariant::fromValue(lst[i]));
	writer.setProperty("_vip_progress", true);
	if (!writer.open(VipIODevice::WriteOnly)) {
		VIP_LOG_ERROR("Cannot open output file ", filename);
		return;
	}
	writer.update();
}

void VipVTKObjectTreeWidget::saveXYZ()
{
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save points", "Text file (*.txt)");
	if (filename.isEmpty())
		return;

	PlotVipVTKObjectList lst = this->selectedObjects();
	std::ofstream out(filename.toLatin1().data());

	VipProgress display;
	display.setText("Create file " + filename + " ...");
	display.setCancelable(true);
	display.setModal(true);
	int range = 0, progress = 0;

	for (int i = 0; i < lst.size(); ++i)
		if (vtkDataSet* set = lst[i]->rawData().dataSet()) {
			range += set->GetNumberOfPoints();
		}

	display.setRange(0, range);

	for (int i = 0; i < lst.size(); ++i) {
		VipVTKObject ptr = lst[i]->rawData();
		vtkDataSet* set = ptr.dataSet();

		// only works for vtkDataSet
		if (!set)
			continue;

		int num_points = ptr.dataSet()->GetNumberOfPoints();
		for (int p = 0; p < num_points; ++p, ++progress) {
			if (progress % 5000 == 0) {
				if (display.canceled())
					return;
				display.setValue(progress);
			}

			double point[3];
			set->GetPoint(p, point);

			// save the XYZ  line
			std::ostringstream str;
			out << point[0] << "\t" << point[1] << "\t" << point[2] << std::endl;
		}
	}
}

bool VipVTKObjectTreeWidget::eventFilter(QObject* watched, QEvent* evt)
{
	if (evt->type() == QEvent::KeyPress) {
		QKeyEvent* e = static_cast<QKeyEvent*>(evt);
		if (e->key() == Qt::Key_Delete)
			this->deleteSelection();
	}
	else if (evt->type() == QEvent::DragEnter) {
		const VipMimeDataCoordinateSystem* mime = qobject_cast<const VipMimeDataCoordinateSystem*>(static_cast<QDragEnterEvent*>(evt)->mimeData());
		if (mime) {
			static_cast<QDragEnterEvent*>(evt)->acceptProposedAction();
			return true;
		}
	}
	else if (evt->type() == QEvent::DragMove) {
		const VipMimeDataCoordinateSystem* mime = qobject_cast<const VipMimeDataCoordinateSystem*>(static_cast<QDragMoveEvent*>(evt)->mimeData());
		if (mime) {
			static_cast<QDragMoveEvent*>(evt)->acceptProposedAction();
			return true;
		}
	}
	else if (evt->type() == QEvent::Drop) {
		const VipMimeDataCoordinateSystem* mime = qobject_cast<const VipMimeDataCoordinateSystem*>(static_cast<QDropEvent*>(evt)->mimeData());
		if (mime) {
			// internal move, from CAD files to CAD Computed (or converse)
			QDropEvent* event = static_cast<QDropEvent*>(evt);
			if (event->source() == d_data->tree) {
				if (QTreeWidgetItem* it = d_data->tree->itemAt(event->VIP_EVT_POSITION())) {
					if (const VipMimeDataProcessingObjectList* mime = qobject_cast<const VipMimeDataProcessingObjectList*>(static_cast<QDropEvent*>(evt)->mimeData())) {
						bool is_file = false;
						while (it) {
							if (it == d_data->inFile) {
								is_file = true;
								break;
							}
							it = it->parent();
						}

						// wa can only move simple pipelines, like device -> processing list -> display
						/* QList<VipDisplayObject*> displays = mime->processings().findAll<VipDisplayObject*>();
						for (int i = 0; i < displays.size(); ++i) {
							// find a source VipProcessingList that contains a VipVTKObject
							VipProcessingList* lst = nullptr;
							QList<VipProcessingList*> all_lst = VipProcessingObjectList(displays[i]->allSources()).findAll<VipProcessingList*>();
							for (int l = all_lst.size() - 1; l >= 0; --l) {
								if (all_lst[l]->outputAt(0)->data().data().userType() == qMetaTypeId<VipVTKObject>()) {
									if (VipVTKObject ptr = all_lst[l]->outputAt(0)->data().value<VipVTKObject>())
										if (ptr->isFileData() != is_file) {
											lst = all_lst[l];
											break;
										}
								}
							}
							if (lst) {
								// just add a processing that change the file attribute
								lst->append(new ChangeDataFileAttribute(is_file));
								lst->reload();
							}
						}*/
					}
				}
			}
			else {
				QList<VipPlotItem*> items = mime->plotData(view()->area()->canvas(), view());
				for (int i = 0; i < items.size(); ++i) {
					items[i]->setAxes(view()->area()->canvas()->axes(), view()->area()->canvas()->coordinateSystemType());
				}
			}
			return true;
		}
	}
	else if (evt->type() == QEvent::MouseMove) {
		QMouseEvent* e = static_cast<QMouseEvent*>(evt);
		if (e->buttons() == Qt::LeftButton) {
			if (qobject_cast<QScrollBar*>(watched))
				return false;

			PlotVipVTKObjectList selection = selectedObjects();
			if (selection.size()) {
				QList<VipProcessingObject*> objects;
				for (int i = 0; i < selection.size(); ++i) {
					if (auto* display = selection[i]->property("VipDisplayObject").value<VipDisplayObject*>()) {
						objects << display;
					}
				}

				if (objects.size()) {
					VipMimeDataProcessingObjectList* mime = new VipMimeDataProcessingObjectList();
					mime->setCoordinateSystem(VipCoordinateSystem::Cartesian);
					mime->setProcessing(objects);
					QDrag drag(this);
					drag.setMimeData(mime);
					drag.exec();
					return true;
				}
			}
		}
	}

	return false;
}

void VipVTKObjectTreeWidget::keyPressEvent(QKeyEvent* evt)
{
	if (evt->key() == Qt::Key_Delete) {
		this->deleteSelection();
		evt->accept();
	}
}

void VipVTKObjectTreeWidget::mousePressEvent(QMouseEvent* e)
{
	d_data->view->setUpdatesEnabled(false);
	QWidget::mousePressEvent(e);
	d_data->view->setUpdatesEnabled(true);
}

/*


static void UpdateQVTKWidget2(VipVTKWidget * w)
{
	w->GetRenderWindow()->GetRenderers()->GetFirstRenderer()->Modified();
	w->GetRenderWindow()->Modified();
	w->GetRenderWindow()->GetInteractor( )->Render( );
	w->GetRenderWindow()->GetRenderers()->GetFirstRenderer()->Render();
	w->GetRenderWindow()->Render();
	w->update();
}*/

struct CreateProperty : public QWidget
{
	QLineEdit name;
	QLineEdit value;
	QComboBox type;

	CreateProperty()
	  : QWidget()
	{
		QHBoxLayout* lay = new QHBoxLayout();
		lay->addWidget(&name);
		lay->addWidget(&value);
		lay->addWidget(&type);
		setLayout(lay);

		name.setToolTip("Property name");
		name.setPlaceholderText("Property Name");
		value.setToolTip("Property value.\nIt could be a multi-component value with comma separators");
		value.setPlaceholderText("Property Value");
		type.setToolTip("Property type");
		type.addItems(QStringList() << "string"
					    << "char"
					    << "unsigned char"
					    << "short"
					    << "unsigned short"
					    << "int"
					    << "unsigned int"
					    << "long long"
					    << "unsigned long long"
					    << "double");
	}

	QPair<QString, vtkVariantList> Property() const
	{
		QString data_type = type.currentText();
		QString data_name = name.text();
		QString data_value = value.text();

		if (data_type == "string")
			return QPair<QString, vtkVariantList>(data_name, vtkVariantList() << vtkVariant(data_value.toLatin1().data()));
		else {
			QStringList values = data_value.split(",", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
			vtkVariantList variants;
			for (int i = 0; i < values.size(); ++i) {
				if (data_type == "char")
					variants << (char)values[i].toInt();
				else if (data_type == "unsigned char")
					variants << (unsigned char)values[i].toInt();
				if (data_type == "short")
					variants << (short)values[i].toInt();
				else if (data_type == "unsigned short")
					variants << (unsigned short)values[i].toInt();
				if (data_type == "int")
					variants << (int)values[i].toInt();
				else if (data_type == "unsigned int")
					variants << (unsigned int)values[i].toUInt();
				if (data_type == "long long")
					variants << (qint64)values[i].toLongLong();
				else if (data_type == "unsigned long long")
					variants << (quint64)values[i].toULongLong();
				else if (data_type == "double")
					variants << (double)values[i].toDouble();
			}

			return QPair<QString, vtkVariantList>(data_name, variants);
		}
	}
};



class VipSelectDisplayedAttributeWidget::PrivateData
{
public:
	VipVTKGraphicsView* view;
	VipComboBox* types;
	VipComboBox* attributes;
	VipComboBox* component;
	QAction* typesAction;
	QAction* attributesAction;
	QAction* componentAction;
	QAction* createAttributeAction;
	QAction* deleteAttributeAction;
	QToolButton* deleteAttribute;
	QToolButton* makeAttribute;
	QMap<QString, PlotVipVTKObjectList > annotations;

	struct Attribute
	{
		VipVTKObject::AttributeType type;
		QString name;
		int comp;
	};
	std::unique_ptr<Attribute> pending;
};

VipSelectDisplayedAttributeWidget::VipSelectDisplayedAttributeWidget(VipVTKGraphicsView* view, QWidget* parent)
  : QToolBar(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	d_data->view = view;
	d_data->types = new VipComboBox();
	d_data->types->setToolTip("Available attributes types");
	d_data->types->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	d_data->types->addItems(QStringList() << "Field attributes"
				      << "Point attributes"
				      << "Cell attributes");

	d_data->attributes = new VipComboBox();
	d_data->attributes->setToolTip("Available attributes");
	d_data->attributes->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	d_data->attributes->addItems(QStringList() << "None");

	d_data->component = new VipComboBox();
	d_data->component->setToolTip("Available components for selected attribute");
	d_data->component->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	d_data->component->addItem("0");

	d_data->deleteAttribute = new QToolButton();
	d_data->deleteAttribute->setToolTip("remove selected attribute\nfrom selected items");
	d_data->deleteAttribute->setIcon(vipIcon("del.png"));

	d_data->makeAttribute = new QToolButton();
	d_data->makeAttribute->setToolButtonStyle(Qt::ToolButtonIconOnly);
	d_data->makeAttribute->setPopupMode(QToolButton::InstantPopup);
	d_data->makeAttribute->setToolTip("Create new attribute for selected items");
	d_data->makeAttribute->setIcon(vipIcon("add_attribute.png"));
	QMenu* menu = new QMenu();
	QAction* new_attribute = menu->addAction("New global attribute");
	QAction* new_points_attribute = menu->addAction("New points attribute");
	QAction* new_interp_points_attribute = menu->addAction("New interpolated points attribute");
	d_data->makeAttribute->setMenu(menu);

	d_data->typesAction = this->addWidget(d_data->types);
	d_data->attributesAction = this->addWidget(d_data->attributes);
	d_data->componentAction = this->addWidget(d_data->component);

	this->addSeparator();
	d_data->createAttributeAction = this->addWidget(d_data->makeAttribute);
	d_data->deleteAttributeAction = this->addWidget(d_data->deleteAttribute);

	d_data->componentAction->setVisible(false);
	d_data->attributesAction->setVisible(false);
	d_data->typesAction->setVisible(false);
	d_data->createAttributeAction->setVisible(false);
	d_data->deleteAttributeAction->setVisible(false);

	connect(view, SIGNAL(dataChanged()), this, SLOT(dataChanged()));
	connect(view->area(), SIGNAL(childSelectionChanged(VipPlotItem*)), this, SLOT(selectionChanged()));
	connect(d_data->types, SIGNAL(currentIndexChanged(int)), this, SLOT(AttributeSelectionChanged()));
	connect(d_data->attributes, SIGNAL(currentIndexChanged(int)), this, SLOT(AttributeSelectionChanged()));
	connect(d_data->component, SIGNAL(currentIndexChanged(int)), this, SLOT(AttributeSelectionChanged()));
	connect(d_data->deleteAttribute, SIGNAL(clicked(bool)), this, SLOT(DeleteSelectedAttribue()));

	connect(new_attribute, SIGNAL(triggered(bool)), this, SLOT(CreateAttribute()));
	connect(new_points_attribute, SIGNAL(triggered(bool)), this, SLOT(CreatePointsAttribute()));
	connect(new_interp_points_attribute, SIGNAL(triggered(bool)), this, SLOT(CreateInterpolatedPointsAttribute()));
}

VipSelectDisplayedAttributeWidget ::~VipSelectDisplayedAttributeWidget()
{
}

void VipSelectDisplayedAttributeWidget::setPendingDisplayedAttribute(VipVTKObject::AttributeType type, const QString& name, int comp)
{
	if (!setDisplayedAttribute(type, name, comp)) 
		d_data->pending.reset(new PrivateData::Attribute{ type, name, comp });
	else
		d_data->pending.reset();
}


void VipSelectDisplayedAttributeWidget::UpdateAttributeWidgets(VipVTKObject::AttributeType t, const QString& attr, int comp)
{
	// TEST
	d_data->types->blockSignals(true);
	d_data->attributes->blockSignals(true);
	d_data->component->blockSignals(true);

	QStringList fields = fieldAttributes();
	fields.sort();
	QStringList points = pointsAttributes();
	points.sort();
	QStringList cells = cellsAttributes();
	cells.sort();

	QStringList attr_types;
	// update types
	if (fields.size())
		attr_types << (TypeToString(VipVTKObject::Field));
	if (points.size())
		attr_types << (TypeToString(VipVTKObject::Point));
	if (cells.size())
		attr_types << (TypeToString(VipVTKObject::Cell));

	if (d_data->types->items() != attr_types) {
		d_data->types->clear();
		d_data->types->addItems(attr_types);
	}
	int index = d_data->types->findText(TypeToString(t));
	if (index >= 0)
		d_data->types->setCurrentIndex(index);
	else if (d_data->types->count())
		d_data->types->setCurrentIndex(0);
	t = StringToType(d_data->types->currentText());

	// update attributes according to types
	QStringList attrs;
	attrs << "None";
	if (t == VipVTKObject::Field)
		attrs.append(fields);
	else if (t == VipVTKObject::Point)
		attrs.append(points);
	else
		attrs.append(cells);
	if (d_data->attributes->items() != attrs) {
		d_data->attributes->clear();
		d_data->attributes->addItems(attrs);
	}

	index = d_data->attributes->findText(attr);
	if (index >= 0)
		d_data->attributes->setCurrentIndex(index);
	else if (d_data->attributes->count())
		d_data->attributes->setCurrentIndex(0);

	// update component according to attributes and types
	int c = 1;
	int data_type = 0;
	auto plots = d_data->view->objects();
	for (int i = 0; i < plots.size(); ++i) {
		if (VipVTKObject obj = plots[i]->rawData()) {
			VipVTKObjectLocker lock = vipLockVTKObjects(obj);
			vtkAbstractArray* array = obj.attribute(t, d_data->attributes->currentText());
			if (array) {
				c = qMax(c, array->GetNumberOfComponents());
				data_type = array->GetDataType();
			}
		}
	}

	QStringList comps;
	bool is_color = ((c == 3 || c == 4) && (data_type == VTK_UNSIGNED_CHAR || data_type == VTK_DOUBLE || data_type == VTK_FLOAT));
	if (is_color)
		comps << ("color");
	for (int i = 0; i < c; ++i)
		comps << (QString::number(i));
	if (d_data->component->items() != comps) {
		d_data->component->clear();
		d_data->component->addItems(comps);
	}

	if (comp < 0 && is_color)
		d_data->component->setCurrentIndex(0);
	else
		d_data->component->setCurrentIndex(comp + (is_color ? 1 : 0));

	d_data->typesAction->setVisible(d_data->types->count() > 0);
	d_data->attributesAction->setVisible(d_data->typesAction->isVisible());
	d_data->componentAction->setVisible(d_data->typesAction->isVisible());

	d_data->types->blockSignals(false);
	d_data->attributes->blockSignals(false);
	d_data->component->blockSignals(false);
}

void VipSelectDisplayedAttributeWidget::updateContent()
{
	dataChanged();
}

void VipSelectDisplayedAttributeWidget::dataChanged()
{
	bool is_displaying = this->isDisplayingAttribute();
	VipVTKObject::AttributeType type = this->currentAttributeType();
	QString attr = this->currentAttribute();
	int comp = this->currentComponent();

	// update attribute list and components
	UpdateAttributeWidgets(type, attr, comp);

	// reset data object color and mapper
	auto plots = d_data->view->objects();
	for (int i = 0; i < plots.size(); ++i) {
		VipVTKObject data = plots[i]->rawData();
		VipVTKObjectLocker lock = vipLockVTKObjects(data);
		plots[i]->removeHighlightColor();
		if (vtkMapper* m = plots[i]->mapper()) {

			m->ScalarVisibilityOff();
			m->SetScalarModeToDefault();
			m->SetColorModeToDefault();
			m->SetLookupTable(nullptr);
		}
	}

	if (is_displaying) {

		if (this->setDisplayedAttribute(type, attr, comp, false))
			d_data->pending.reset();
	}
	else if(d_data->pending){
		if (this->setDisplayedAttribute(d_data->pending->type, d_data->pending->name, d_data->pending->comp))
			d_data->pending.reset();
	}
}

void VipSelectDisplayedAttributeWidget::selectionChanged()
{
	bool has_selection = d_data->view->selectedObjects().size();

	d_data->createAttributeAction->setVisible(has_selection);
	d_data->deleteAttributeAction->setVisible(has_selection);
}

bool VipSelectDisplayedAttributeWidget::setDisplayedAttribute(VipVTKObject::AttributeType type, const QString& attribute, int c, bool update_widget)
{
	if (update_widget)
		this->UpdateAttributeWidgets(type, attribute, c);

	int t_index = d_data->types->findText(TypeToString(type));
	int a_index = d_data->attributes->findText(attribute);
	int c_index = d_data->component->findText(QString::number(c));

	if (c_index < 0 && c < 0 && d_data->component->findText("color") == 0)
		c_index = 0;

	if (t_index >= 0 && a_index >= 0 && c_index >= 0) {
		if (update_widget) {
			d_data->types->blockSignals(true);
			d_data->attributes->blockSignals(true);
			d_data->component->blockSignals(true);

			d_data->types->setCurrentIndex(t_index);
			d_data->attributes->setCurrentIndex(a_index);
			d_data->component->setCurrentIndex(c_index);

			d_data->types->blockSignals(false);
			d_data->attributes->blockSignals(false);
			d_data->component->blockSignals(false);
		}
		DisplaySelectedAttribue(true);
		return true;
	}
	else {
		DisplaySelectedAttribue(false);
		return false;
	}
}

bool VipSelectDisplayedAttributeWidget::isDisplayingAttribute() const
{
	return this->d_data->attributes->currentIndex() > 0;
}

VipVTKObject::AttributeType VipSelectDisplayedAttributeWidget::currentAttributeType() const
{
	if (d_data->types->count())
		return StringToType(d_data->types->currentText());
	else
		return VipVTKObject::Unknown;
}

QString VipSelectDisplayedAttributeWidget::currentAttribute() const
{
	return d_data->attributes->currentText();
}

int VipSelectDisplayedAttributeWidget::currentComponent() const
{
	if (d_data->component->currentText() == "color")
		return -1;
	else if (d_data->component->count())
		return d_data->component->currentText().toInt();
	else
		return -2;
}

QStringList VipSelectDisplayedAttributeWidget::pointsAttributes() const
{
	QStringList res;
	auto plots = d_data->view->objects();
	for (int i = 0; i < plots.size(); ++i) {
		res << plots[i]->rawData().pointsAttributesName();
	}
	return vipUnique(res);
}

QStringList VipSelectDisplayedAttributeWidget::cellsAttributes() const
{
	QStringList res;
	auto plots = d_data->view->objects();
	for (int i = 0; i < plots.size(); ++i) {
		res << plots[i]->rawData().cellsAttributesName();
	}
	return vipUnique(res);
}

QStringList VipSelectDisplayedAttributeWidget::fieldAttributes() const
{
	QStringList res;
	auto plots = d_data->view->objects();
	for (int i = 0; i < plots.size(); ++i) {
		res << plots[i]->rawData().fieldAttributesNames();
	}
	return vipUnique(res);
}

void VipSelectDisplayedAttributeWidget::DisplaySelectedAttribue(bool display)
{
	if (!display || d_data->attributes->currentIndex() == 0 || this->currentAttributeType() == VipVTKObject::Field) {

		// reset data object color and mapper
		auto plots = d_data->view->objects();
		for (int i = 0; i < plots.size(); ++i) {
			VipVTKObject data = plots[i]->rawData();
			VipVTKObjectLocker lock = vipLockVTKObjects(data);
			plots[i]->removeHighlightColor();
			if (vtkMapper* m = plots[i]->mapper()) {

				m->ScalarVisibilityOff();
				m->SetScalarModeToDefault();
				m->SetColorModeToDefault();
				m->SetLookupTable(nullptr);
			}
		}

		// view->scalarBar()->VisibilityOff();
		// view->legend()->VisibilityOff();
		d_data->view->area()->colorMapAxis()->setVisible(false);
		d_data->view->annotationLegend()->setVisible(false);

		ClearAnnotations();
	}

	if (display && d_data->attributes->currentIndex() != 0) {
		if (this->currentAttributeType() == VipVTKObject::Field)
			DisplaySelectedAnnotatedAttribue(display);
		else
			DisplaySelectedScalarAttribue(display);
	}

	d_data->view->refresh();

	if (d_data->component->count() <= 1 && d_data->componentAction->isVisible())
		d_data->componentAction->setVisible(false);
	else if (d_data->component->count() > 1 && !d_data->componentAction->isVisible())
		d_data->componentAction->setVisible(true);
}

void VipSelectDisplayedAttributeWidget::DisplaySelectedScalarAttribue(bool display)
{
	
	// consider only points attributes
	if (display && d_data->attributes->currentIndex() != 0) {
		d_data->view->annotationLegend()->setVisible(false);
		ClearAnnotations();

		// apply to all
		VipVTKObject::AttributeType type = this->currentAttributeType();
		QString attr = d_data->attributes->currentText();
		int comp = currentComponent();
		double* range = d_data->view->table()->GetRange();

		auto plots = d_data->view->objects();
		auto objects = fromPlotVipVTKObject(plots);
		auto lockers = vipLockVTKObjects(objects);

		// update table colors
		if (comp >= 0) {
			d_data->view->table()->SetVectorComponent(comp);
			d_data->view->table()->SetVectorSize(1);

			// apply auto scale
			if (d_data->view->area()->colorMapAxis()->isAutoScale()) {
				double min, max;
				if (d_data->view->findPointAttributeBounds(objects,type, attr, comp, &min, &max)) {
					//if (min == max)
					//	max++;
					d_data->view->table()->SetRange(min, max);
				}
			}
		}

		if (comp < 0) {
			d_data->view->area()->colorMapAxis()->setVisible(false);
		}

		for (int i = 0; i < plots.size(); ++i) {
			VipVTKObject data = objects[i];

			if (data && data.data()->IsA("vtkDataSet")) {
				// set the current active scalar to both data set and simplified data
				{
					vtkDataSet* set = static_cast<vtkDataSet*>(data.data());
					int index = -1;
					if (type == VipVTKObject::Point) {
						index = set->GetPointData()->SetActiveScalars(attr.toLatin1().data());
						/*if(data->Simplified())
							data->Simplified()->GetPointData()->SetActiveScalars(attr.toLatin1().data());*/
					}
					else if (type == VipVTKObject::Cell) {
						index = set->GetCellData()->SetActiveScalars(attr.toLatin1().data());
						/*if(data->Simplified())
							data->Simplified()->GetCellData()->SetActiveScalars(attr.toLatin1().data());*/
					}

					if (index >= 0) {
						vtkMapper* m = plots[i]->mapper();
						if (comp >= 0) {
							if (!m->GetScalarVisibility())
								m->ScalarVisibilityOn();
							if (type == VipVTKObject::Point) {
								if (m->GetScalarMode() != VTK_SCALAR_MODE_USE_POINT_DATA)
									m->SetScalarModeToUsePointData();
							}
							else {
								if (m->GetScalarMode() != VTK_SCALAR_MODE_USE_CELL_DATA)
									m->SetScalarModeToUseCellData();
							}
							if (!m->GetColorMode() != VTK_COLOR_MODE_MAP_SCALARS)
								m->SetColorModeToMapScalars();
							m->SelectColorArray(comp);
							m->SetLookupTable(d_data->view->table());
							m->SetScalarRange(range[0], range[1]);
							//m->Modified();//TEST
						}
						else {
							m->ScalarVisibilityOn();
							m->SetScalarModeToUsePointFieldData();
							m->SetColorModeToDefault();
							m->SelectColorArray(attr.toLatin1().data());
							m->SetLookupTable(nullptr);
						}
					}
				}
				
				// notify the color map that it needs to recompute its scale div
				plots[i]->markColorMapDirty();
			}
		}

		if (comp >= 0) {
			d_data->view->scalarBar()->SetTitle(attr.toLatin1().data());
			// view->scalarBar()->VisibilityOn();

			d_data->view->area()->colorMapAxis()->setVisible(true);
			d_data->view->area()->colorMapAxis()->setTitle(attr);
		}
	}
}

void VipSelectDisplayedAttributeWidget::ClearAnnotations()
{
	if (d_data->annotations.isEmpty())
		return;

	// reset the color of each annotated object
	for (auto it = d_data->annotations.begin(); it != d_data->annotations.end(); ++it) {
		auto lst = it.value();
		for (int i = 0; i < lst.size(); ++i)
			lst[i]->removeHighlightColor();
	}

	d_data->annotations.clear();

	//
	// remove all items from the legend
	QList<VipPlotItem*> items = d_data->view->annotationLegend()->legend()->items();
	d_data->view->annotationLegend()->legend()->clear();
	for (int i = 0; i < items.size(); ++i) {
		delete items[i];
	}
}

void VipSelectDisplayedAttributeWidget::DisplaySelectedAnnotatedAttribue(bool display)
{
	// consider only points attributes
	if (display && d_data->attributes->currentIndex() != 0) {
		// apply to all
		QString attr = d_data->attributes->currentText();
		int comp = d_data->component->currentText().toInt();

		// build the map of possible values
		ClearAnnotations();

		auto plots = d_data->view->objects();

		for (int i = 0; i < plots.size(); ++i) {
			vtkVariantList lst = plots[i]->rawData().fieldAttribute(attr);
			if (comp < lst.size()) {
				std::ostringstream oss;
				oss << lst[comp];
				d_data->annotations[oss.str().c_str()] << plots[i];
			}
		}

		// update table colors
		/*view->setLegendEntries(possibleValues.size());
		VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);
		int i=0;
		for(QMap<QString,VipVTKObjectList>::iterator it = possibleValues.begin(); it != possibleValues.end(); ++it, ++i)
		{
			double color[3];
			const QColor c = palette.color(i);
			color[0] = c.redF();
			color[1] = c.greenF();
			color[2] = c.blueF();
			view->setLegend(i,it.key().toLatin1().data(),color);

			VipVTKObjectList lst = it.value();
			for(int l=0; l < lst.size(); ++l)
				lst[l]->SetColor(color);
		}

		view->legend()->VisibilityOn();*/

		// instead of VTK legend, use VipPlotting legend

		VipColorPalette palette(VipLinearColorMap::ColorPaletteRandom);
		int i = 0;
		for (auto it = d_data->annotations.begin(); it != d_data->annotations.end(); ++it, ++i) {
			double color[3];
			const QColor c = palette.color(i);
			color[0] = c.redF();
			color[1] = c.greenF();
			color[2] = c.blueF();

			PlotVipVTKObjectList lst = it.value();
			for (int l = 0; l < lst.size(); ++l) {
				lst[l]->setHighlightColor(vipToQColor(color));

				// create a new fake VipPlotVTKObject for each VipVTKObject and add them into the VipLegend
				VipPlotVTKObject* plot = new VipPlotVTKObject();
				plot->setColor(vipToQColor(color));
				plot->setTitle(it.key());
				d_data->view->annotationLegend()->legend()->addItem(plot);
			}
		}

		d_data->view->annotationLegend()->setVisible(true);

		VipTextStyle st = d_data->view->annotationLegend()->legend()->legendItemTextStyle();
		st.setTextPen(QPen(vipWidgetTextBrush(this).color()));
		d_data->view->annotationLegend()->legend()->setLegendItemTextStyle(st);
	}
}

void VipSelectDisplayedAttributeWidget::DeleteSelectedAttribue()
{
	PlotVipVTKObjectList lst = d_data->view->selectedObjects();
	VipVTKObject::AttributeType type = this->currentAttributeType();
	QString name = this->currentAttribute();

	if (lst.size() > 0 && type != VipVTKObject::Unknown && name != "None") {
		QMessageBox::StandardButton b = QMessageBox::question(nullptr, "remove attribute", "Do you want to remove selected attribute?");
		if (b == QMessageBox::Yes) {
			setDisplayedAttribute(VipVTKObject::Unknown, "None", 0);

			for (int i = 0; i < lst.size(); ++i) {
				lst[i]->rawData().removeAttribute(type, name);
			}

			UpdateAttributeWidgets(VipVTKObject::Unknown);
		}
	}

	this->updateContent();
}

static CreateProperty* GetGlobalCreateProperty()
{
	static CreateProperty* prop = new CreateProperty();
	return prop;
}

void VipSelectDisplayedAttributeWidget::CreatePointsAttribute()
{
	CreateProperty* create = GetGlobalCreateProperty();
	VipGenericDialog dialog(create, "Create points attribute");

	if (dialog.exec() == QDialog::Accepted) {
		QPair<QString, vtkVariantList> attribute = create->Property();
		PlotVipVTKObjectList lst = d_data->view->selectedObjects();

		for (int i = 0; i < lst.size(); ++i) {
			lst[i]->rawData().setPointsAttribute(attribute.first, attribute.second);
			lst[i]->markColorMapDirty();
		}

		this->updateContent();
	}

	create->setParent(nullptr);
}

void VipSelectDisplayedAttributeWidget::CreateInterpolatedPointsAttribute()
{
	/* PointAttributeInterpolate* editor = new PointAttributeInterpolate();
	VipGenericDialog* dialog = new VipGenericDialog(editor, "Create points attribute", d_data->view);
	dialog->setWindowFlags(this->windowFlags() | Qt::Tool |  Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
	editor->setGraphicsView(d_data->view);
	connect(dialog, SIGNAL(accepted()), editor, SLOT(apply()));
	connect(editor, SIGNAL(failed()), dialog, SLOT(show()));
	connect(editor, SIGNAL(succeded()), dialog, SLOT(close()));
	dialog->show();*/
}

void VipSelectDisplayedAttributeWidget::CreateAttribute()
{
	CreateProperty* create = GetGlobalCreateProperty();
	VipGenericDialog dialog(create, "Create global attribute");

	if (dialog.exec() == QDialog::Accepted) {
		QPair<QString, vtkVariantList> attribute = create->Property();
		PlotVipVTKObjectList lst = d_data->view->selectedObjects();

		for (int i = 0; i < lst.size(); ++i) {
			lst[i]->rawData().setFieldAttribute(attribute.first, attribute.second);
			lst[i]->markColorMapDirty();
		}
	}
	this->updateContent();
	create->setParent(nullptr);
}

void VipSelectDisplayedAttributeWidget::AttributeSelectionChanged()
{
	VipVTKObject::AttributeType type = this->currentAttributeType();
	QString attr = this->currentAttribute();
	int comp = this->currentComponent();

	if (sender() == d_data->attributes && type != VipVTKObject::Field && attr != "None") {
		// if the attribute name changed and is not a field attribute,
		// try to automatically find if the attribute should considered as a color or a scalar

		d_data->component->blockSignals(true);

		if (isColorAttribute(fromPlotVipVTKObject(d_data->view->objects()),type, attr))
			comp = -1;
		else if (comp == -1)
			comp = 0;

		d_data->component->blockSignals(false);
	}

	setDisplayedAttribute(type, attr, comp);
}

QString VipSelectDisplayedAttributeWidget::TypeToString(VipVTKObject::AttributeType t) const
{
	if (t == VipVTKObject::Field)
		return "Field attributes";
	else if (t == VipVTKObject::Point)
		return "Point attributes";
	else if (t == VipVTKObject::Cell)
		return "Cell attributes";
	else
		return QString();
}

VipVTKObject::AttributeType VipSelectDisplayedAttributeWidget::StringToType(const QString& t) const
{
	if (t == "Field attributes")
		return VipVTKObject::Field;
	else if (t == "Point attributes")
		return VipVTKObject::Point;
	else if (t == "Cell attributes")
		return VipVTKObject::Cell;
	else
		return VipVTKObject::Unknown;
}


/**
/// @brief Add a horizontal or vertical resize grip to a widget inside a layout/
/// The grip behave in a similar way to a QSplitter handle.
class VIP_GUI_EXPORT ResizableWidget : public QWidget
{
public:
	ResizableWidget(Qt::Orientation orientation = Qt::Horizontal, QWidget* parent = nullptr);
	~ResizableWidget();

	/// @brief Set the grip extent in pixels
	void setExtent(int);
	int extent() const;

protected:
	virtual bool eventFilter(QObject* watched, QEvent* evt);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);
	virtual void mouseReleaseEvent(QMouseEvent* evt);

private:
	VIP_DECLARE_PRIVATE_DATA();
};

class ResizableWidget::PrivateData
{
public:
	Qt::Orientation orientation;
	QPoint pos;
	int size;
	int width;
};

ResizableWidget::ResizableWidget(Qt::Orientation orientation, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->orientation = orientation;
	setExtent(8);

	parent->installEventFilter(this);

	this->hide();
	this->setStyleSheet("background-color:gray;");

	if (orientation == Qt::Horizontal)
		setCursor(Qt::SplitHCursor);
	else
		setCursor(Qt::SplitVCursor);
}

ResizableWidget::~ResizableWidget()
{
	parentWidget()->removeEventFilter(this);
}

void ResizableWidget::setExtent(int w)
{
	d_data->width = w;
	if (d_data->orientation == Qt::Horizontal) {
		setMaximumWidth(w);
		setMinimumWidth(w);
	}
	else {
		setMaximumHeight(w);
		setMinimumHeight(w);
	}
}

int ResizableWidget::extent() const
{
	return d_data->width;
}

bool ResizableWidget::eventFilter(QObject*, QEvent* evt)
{
	if (evt->type() == QEvent::Enter) {
		this->show();
	}
	else if (evt->type() == QEvent::Leave) {
		bool should_hide = true;
		if (d_data->orientation == Qt::Horizontal && parentWidget()->width() == extent())
			should_hide = false;
		else if (d_data->orientation == Qt::Vertical && parentWidget()->height() == extent())
			should_hide = false;
		if (should_hide)
			this->hide();
	}
	else if (evt->type() == QEvent::Resize) {
		if (d_data->orientation == Qt::Horizontal) {
			this->move(QPoint(parentWidget()->width() - extent(), 0));
			this->resize(extent(), parentWidget()->height());
		}
		else {
			this->move(QPoint(0, parentWidget()->height() - extent()));
			this->resize(parentWidget()->width(), extent());
		}
	}

	return false;
}

void ResizableWidget::mousePressEvent(QMouseEvent*)
{
	d_data->pos = QCursor::pos();
	if (d_data->orientation == Qt::Horizontal)
		d_data->size = parentWidget()->width();
	else
		d_data->size = parentWidget()->height();
}

void ResizableWidget::mouseMoveEvent(QMouseEvent*)
{
	if (d_data->pos != QPoint()) {
		if (d_data->orientation == Qt::Horizontal) {
			int s = qMax(extent(), d_data->size + (QCursor::pos() - d_data->pos).x());
			parentWidget()->setMaximumWidth(s);
			parentWidget()->setMinimumWidth(s);
		}
		else {
			int s = qMax(extent(), d_data->size + (QCursor::pos() - d_data->pos).y());
			parentWidget()->setMaximumHeight(s);
			parentWidget()->setMinimumHeight(s);
		}
	}
}

void ResizableWidget::mouseReleaseEvent(QMouseEvent*)
{
	d_data->pos = QPoint();
}
*/

VipVTKPlayerToolWidget::VipVTKPlayerToolWidget(VipMainWindow* parent)
  : VipToolWidgetPlayer(parent)
{
	setWindowTitle("3D Object Editor");
	setObjectName("3D Object Editor");
}

bool VipVTKPlayerToolWidget::setPlayer(VipAbstractPlayer* player)
{
	VipVTKPlayer* pl = qobject_cast<VipVTKPlayer*>(player);
	if (m_player == player)
		return pl != nullptr;

	QWidget* w = this->widget();
	if (w) {
		w->hide();
		this->takeWidget();
	}

	if (pl) {
		this->setWidget(pl->leftWidget());
	}
	m_player = pl;

	return pl != nullptr;
}

VipVTKPlayerToolWidget* vipGetVTKPlayerToolWidget(VipMainWindow* parent )
{
	static VipVTKPlayerToolWidget* inst = new VipVTKPlayerToolWidget(parent);
	return inst;
}






static VipLineEdit* makeLineEdit(const QString& placeholder, const QString& tooltip)
{
	VipLineEdit* edit = new VipLineEdit();
	edit->setPlaceholderText(placeholder);
	edit->setToolTip(tooltip);
	return edit;
}
static QCheckBox* makeCheckBox(const QString& text, const QString& tooltip = QString())
{
	QCheckBox* edit = new QCheckBox();
	edit->setText(text);
	if (!tooltip.isEmpty())
		edit->setToolTip(tooltip);
	else
		edit->setToolTip(text);
	return edit;
}
static VipComboBox* makeComboBox(const QStringList& text, const QString& tooltip)
{
	VipComboBox* edit = new VipComboBox();
	edit->addItems(text);
	edit->setToolTip(tooltip);
	return edit;
}
static QSpinBox* makeSpinBox(int min, int max, int step, const QString& tooltip)
{
	QSpinBox* edit = new QSpinBox();
	edit->setRange(min, max);
	edit->setSingleStep(step);
	edit->setToolTip(tooltip);
	return edit;
}
static QDoubleSpinBox* makeDoubleSpinBox(double min, double max, double step, const QString& tooltip)
{
	QDoubleSpinBox* edit = new QDoubleSpinBox();
	edit->setRange(min, max);
	edit->setSingleStep(step);
	edit->setToolTip(tooltip);
	return edit;
}

static QGroupBox* makeGroupBox(const QString& text)
{
	QGroupBox* box = new QGroupBox();
	box->setFlat(true);
	box->setTitle(text);
	box->setCheckable(false);
	return box;
}

static QHBoxLayout* makeLayout(QWidget* w1, QWidget* w2, QWidget* w3 = nullptr)
{
	QHBoxLayout* lay = new QHBoxLayout();
	lay->setContentsMargins(0, 0, 0, 0);
	lay->addWidget(w1);
	if (w2)
		lay->addWidget(w2);
	if (w3)
		lay->addWidget(w3);
	return lay;
}

static void addRow(QGridLayout* lay, const QString& title, QWidget* w)
{
	int h = lay->property("h").toInt();
	lay->addWidget(new QLabel(title), h, 0);
	lay->addWidget(w, h, 1);
	lay->setProperty("h", h + 1);
}
static void addRow(QGridLayout* lay, QWidget* w)
{
	int h = lay->property("h").toInt();
	lay->addWidget(w, h, 0, 1, 2);
	lay->setProperty("h", h + 1);
}
static void addRow(QGridLayout* lay, QLayout* w)
{
	int h = lay->property("h").toInt();
	lay->addLayout(w, h, 0, 1, 2);
	lay->setProperty("h", h + 1);
}
static void addRow(QGridLayout* lay, const QString& title)
{
	int h = lay->property("h").toInt();
	lay->addWidget(makeGroupBox(title), h, 0, 1, 2);
	lay->setProperty("h", h + 1);
}

class VipCubeAxesActorWidget::PrivateData
{
public:
	vtkCubeAxesActor* actor{ nullptr };
	VipVTKGraphicsView* view{ nullptr };

	// Properties
	VipLineEdit* xTitle{ makeLineEdit("X title", "X title") };
	VipLineEdit* yTitle{ makeLineEdit("Y title", "Y title") };
	VipLineEdit* zTitle{ makeLineEdit("Z title", "Z title") };

	VipLineEdit* xUnit{ makeLineEdit("X unit", "X unit") };
	VipLineEdit* yUnit{ makeLineEdit("Y unit", "Y unit") };
	VipLineEdit* zUnit{ makeLineEdit("Z unit", "Z unit") };

	VipLineEdit* xFormat{ makeLineEdit("X format", "X format") };
	VipLineEdit* yFormat{ makeLineEdit("Y format", "Y format") };
	VipLineEdit* zFormat{ makeLineEdit("Z format", "Z format") };

	// General display
	VipComboBox* tickLocation{ makeComboBox(QStringList() << "Inside"
							      << "Outside"
							      << "Both",
						"Tick position") };
	QCheckBox* labelScaling{ makeCheckBox("Label scaling", "Enable label scaling") };

	QDoubleSpinBox* labelOffset{ makeDoubleSpinBox(0, 100, 0.1, "Label offset") };
	QDoubleSpinBox* titleOffset{ makeDoubleSpinBox(0, 100, 0.1, "Title offset") };

	QCheckBox *xVisible{ makeCheckBox("X axis visible") }, *yVisible{ makeCheckBox("Y axis visible") }, *zVisible{ makeCheckBox("Z axis visible") };
	QCheckBox *xLabelVisible{ makeCheckBox("X labels visible") }, *yLabelVisible{ makeCheckBox("Y labels visible") }, *zLabelVisible{ makeCheckBox("Z labels visible") };
	QCheckBox *xTickVisible{ makeCheckBox("X ticks visible") }, *yTickVisible{ makeCheckBox("Y ticks visible") }, *zTickVisible{ makeCheckBox("Z ticks visible") };
	QCheckBox *xMinorTickVisible{ makeCheckBox("X minor ticks visible") }, *yMinorTickVisible{ makeCheckBox("Y minor ticks visible") }, *zMinorTickVisible{ makeCheckBox("Z minor ticks visible") };
	QCheckBox *xGridlines{ makeCheckBox("X grid lines visible") }, *yGridlines{ makeCheckBox("Y grid lines visible") }, *zGridlines{ makeCheckBox("Z grid lines visible") };
	QCheckBox *xInnerGridlines{ makeCheckBox("X inner grid lines visible") }, *yInnerGridlines{ makeCheckBox("Y inner grid lines visible") },
	  *zInnerGridlines{ makeCheckBox("Z inner grid lines visible") };
	QCheckBox *xGridpolys{ makeCheckBox("X grid polys visible") }, *yGridpolys{ makeCheckBox("Y grid polys visible") }, *zGridpolys{ makeCheckBox("Z grid polys visible") };
	QCheckBox* useTextActor3D{ makeCheckBox("Use text actor 3D") };
	QCheckBox* use2DMode{ makeCheckBox("Use 2D mode") };
	QCheckBox* stickyAxes{ makeCheckBox("Sticky axes") };
	QCheckBox* centerStickyAxes{ makeCheckBox("Center sticky axes") };
	QDoubleSpinBox* cornerOffset{ makeDoubleSpinBox(0, 1, 0.01, "Corner offset") };

	// Dynamic behavior
	VipComboBox* flyMode{ makeComboBox(QStringList() << "FLY_OUTER_EDGES"
							 << "FLY_CLOSEST_TRIAD"
							 << "FLY_FURTHEST_TRIAD"
							 << "FLY_STATIC_TRIAD"
							 << "VTK_FLY_STATIC_EDGES",
					   "Corner offset") };
	VipComboBox* gridLineLocation{ makeComboBox(QStringList() << "GRID_LINES_ALL"
								  << "GRID_LINES_CLOSEST"
								  << "GRID_LINES_FURTHEST",
						    "Grid lines location") };
	QSpinBox* inertia{ makeSpinBox(1, 100, 1, "Inertial factor that controls how often (i.e, how many renders) the axes can switch position (jump from one axes to another)") };

	QCheckBox* distanceLOD{ makeCheckBox("Distance LOD", "Use of distance based LOD for titles and labels.") };
	QDoubleSpinBox* distanceLODThreshold{ makeDoubleSpinBox(0, 1, 0.1, "Set distance LOD threshold [0.0 - 1.0] for titles and labels.") };

	QCheckBox* viewAngleLOD{ makeCheckBox("View angle LOD", "Enable and disable the use of view angle based LOD for titles and labels.") };
	QDoubleSpinBox* viewAngleLODThreshold{ makeDoubleSpinBox(0, 1, 0.1, "Set view angle LOD threshold [0.0 - 1.0] for titles and labels.") };
};

VipCubeAxesActorWidget::VipCubeAxesActorWidget(QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	QGridLayout* grid = new QGridLayout();

	addRow(grid, "Titles & units");
	addRow(grid, makeLayout(d_data->xTitle, d_data->yTitle, d_data->zTitle));
	addRow(grid, makeLayout(d_data->xUnit, d_data->yUnit, d_data->zUnit));
	addRow(grid, makeLayout(d_data->xFormat, d_data->yFormat, d_data->zFormat));

	addRow(grid, "General display");
	addRow(grid, "Tick location", d_data->tickLocation);
	addRow(grid, d_data->labelScaling);
	addRow(grid, "Label offset", d_data->labelOffset);
	addRow(grid, "Title offset", d_data->titleOffset);
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xVisible, d_data->yVisible, d_data->zVisible));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xLabelVisible, d_data->yLabelVisible, d_data->zLabelVisible));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xTickVisible, d_data->yTickVisible, d_data->zTickVisible));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xMinorTickVisible, d_data->yMinorTickVisible, d_data->zMinorTickVisible));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xGridlines, d_data->yGridlines, d_data->zGridlines));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xInnerGridlines, d_data->yInnerGridlines, d_data->zInnerGridlines));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->xGridpolys, d_data->yGridpolys, d_data->zGridpolys));
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, d_data->useTextActor3D);
	addRow(grid, d_data->use2DMode);
	addRow(grid, d_data->stickyAxes);
	addRow(grid, d_data->centerStickyAxes);
	addRow(grid, "Corner offset", d_data->cornerOffset);

	addRow(grid, "Dynamic behavior");
	addRow(grid, "Fly mode", d_data->flyMode);
	addRow(grid, "Grid line location", d_data->gridLineLocation);
	addRow(grid, "Inertia", d_data->inertia);
	addRow(grid, VipLineWidget::createHLine());
	addRow(grid, makeLayout(d_data->distanceLOD, d_data->distanceLODThreshold));
	addRow(grid, makeLayout(d_data->viewAngleLOD, d_data->viewAngleLODThreshold));

	setLayout(grid);

	connectWidget(d_data->xTitle, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->yTitle, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->zTitle, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->xUnit, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->yUnit, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->zUnit, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->xFormat, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->yFormat, SIGNAL(textChanged(const QString&)));
	connectWidget(d_data->zFormat, SIGNAL(textChanged(const QString&)));

	connectWidget(d_data->tickLocation, SIGNAL(currentIndexChanged(int)));
	connectWidget(d_data->flyMode, SIGNAL(currentIndexChanged(int)));
	connectWidget(d_data->gridLineLocation, SIGNAL(currentIndexChanged(int)));

	connectWidget(d_data->labelOffset, SIGNAL(valueChanged(double)));
	connectWidget(d_data->titleOffset, SIGNAL(valueChanged(double)));
	connectWidget(d_data->cornerOffset, SIGNAL(valueChanged(double)));
	connectWidget(d_data->inertia, SIGNAL(valueChanged(int)));

	connectWidget(d_data->distanceLODThreshold, SIGNAL(valueChanged(double)));
	connectWidget(d_data->viewAngleLODThreshold, SIGNAL(valueChanged(double)));

	QList<QCheckBox*> boxes = vipListCast<QCheckBox*>(this->children());
	for (QCheckBox* b : boxes)
		connectWidget(b, SIGNAL(clicked(bool)));
}
VipCubeAxesActorWidget::~VipCubeAxesActorWidget() {}

void VipCubeAxesActorWidget::connectWidget(QWidget* src, const char* signal)
{
	connect(src, signal, this, SLOT(updateActor()));
}

void VipCubeAxesActorWidget::setView(VipVTKGraphicsView* view)
{
	d_data->view = view;
	d_data->actor = nullptr;
	if (view) {
		d_data->actor = view->cubeAxesActor();
		updateWidget();
	}
}
VipVTKGraphicsView* VipCubeAxesActorWidget::view()
{
	return d_data->view;
}

void VipCubeAxesActorWidget::updateWidget()
{
	if (!d_data->actor)
		return;
	QList<QWidget*> ws = vipListCast<QWidget*>(children());
	for (QWidget* w : ws)
		w->blockSignals(true);

	// Properties
	d_data->xTitle->setText(d_data->actor->GetXTitle());
	d_data->yTitle->setText(d_data->actor->GetYTitle());
	d_data->zTitle->setText(d_data->actor->GetZTitle());

	d_data->xUnit->setText(d_data->actor->GetXUnits());
	d_data->yUnit->setText(d_data->actor->GetYUnits());
	d_data->zUnit->setText(d_data->actor->GetZUnits());

	d_data->xFormat->setText(d_data->actor->GetXLabelFormat());
	d_data->yFormat->setText(d_data->actor->GetYLabelFormat());
	d_data->zFormat->setText(d_data->actor->GetZLabelFormat());

	d_data->tickLocation->setCurrentIndex(d_data->actor->GetTickLocation());
	d_data->labelOffset->setValue(d_data->actor->GetLabelOffset());

#if VTK_VERSION_NUMBER >= 90020210809ULL
	double x, y;
	d_data->actor->GetTitleOffset(x, y);
	d_data->titleOffset->setValue(x);
#else
	d_data->titleOffset->setValue(d_data->actor->GetTitleOffset());
#endif

	d_data->xVisible->setChecked(d_data->actor->GetXAxisVisibility());
	d_data->yVisible->setChecked(d_data->actor->GetYAxisVisibility());
	d_data->zVisible->setChecked(d_data->actor->GetZAxisVisibility());

	d_data->xLabelVisible->setChecked(d_data->actor->GetXAxisLabelVisibility());
	d_data->yLabelVisible->setChecked(d_data->actor->GetXAxisLabelVisibility());
	d_data->zLabelVisible->setChecked(d_data->actor->GetYAxisLabelVisibility());

	d_data->xTickVisible->setChecked(d_data->actor->GetXAxisTickVisibility());
	d_data->yTickVisible->setChecked(d_data->actor->GetYAxisTickVisibility());
	d_data->zTickVisible->setChecked(d_data->actor->GetZAxisTickVisibility());

	d_data->xMinorTickVisible->setChecked(d_data->actor->GetXAxisMinorTickVisibility());
	d_data->yMinorTickVisible->setChecked(d_data->actor->GetYAxisMinorTickVisibility());
	d_data->zMinorTickVisible->setChecked(d_data->actor->GetZAxisMinorTickVisibility());

	d_data->xGridlines->setChecked(d_data->actor->GetDrawXGridlines());
	d_data->yGridlines->setChecked(d_data->actor->GetDrawYGridlines());
	d_data->zGridlines->setChecked(d_data->actor->GetDrawZGridlines());

	d_data->xInnerGridlines->setChecked(d_data->actor->GetDrawXInnerGridlines());
	d_data->yInnerGridlines->setChecked(d_data->actor->GetDrawYInnerGridlines());
	d_data->zInnerGridlines->setChecked(d_data->actor->GetDrawZInnerGridlines());

	d_data->xGridpolys->setChecked(d_data->actor->GetDrawXGridpolys());
	d_data->yGridpolys->setChecked(d_data->actor->GetDrawYGridpolys());
	d_data->zGridpolys->setChecked(d_data->actor->GetDrawZGridpolys());

	d_data->useTextActor3D->setChecked(d_data->actor->GetUseTextActor3D());
	d_data->use2DMode->setChecked(d_data->actor->GetUse2DMode());
	d_data->stickyAxes->setChecked(d_data->actor->GetStickyAxes());
	d_data->centerStickyAxes->setChecked(d_data->actor->GetCenterStickyAxes());

	d_data->cornerOffset->setValue(d_data->actor->GetCornerOffset());

	d_data->flyMode->setCurrentIndex(d_data->actor->GetFlyMode());
	d_data->gridLineLocation->setCurrentIndex(d_data->actor->GetGridLineLocation());
	d_data->inertia->setValue(d_data->actor->GetInertia());

	d_data->distanceLOD->setChecked(d_data->actor->GetEnableDistanceLOD());
	d_data->distanceLODThreshold->setValue(d_data->actor->GetDistanceLODThreshold());

	d_data->viewAngleLOD->setChecked(d_data->actor->GetEnableViewAngleLOD());
	d_data->viewAngleLODThreshold->setValue(d_data->actor->GetViewAngleLODThreshold());

	for (QWidget* w : ws)
		w->blockSignals(false);
}
void VipCubeAxesActorWidget::updateActor()
{
	vtkCubeAxesActor* a = d_data->actor;
	PrivateData* d = d_data.get();
	if (!a)
		return;

	a->SetXTitle(d->xTitle->text().toLatin1().data());
	a->SetYTitle(d->yTitle->text().toLatin1().data());
	a->SetZTitle(d->zTitle->text().toLatin1().data());

	a->SetXUnits(d->xUnit->text().toLatin1().data());
	a->SetYUnits(d->yUnit->text().toLatin1().data());
	a->SetZUnits(d->zUnit->text().toLatin1().data());

	a->SetXLabelFormat(d->xFormat->text().toLatin1().data());
	a->SetYLabelFormat(d->yFormat->text().toLatin1().data());
	a->SetZLabelFormat(d->zFormat->text().toLatin1().data());

	a->SetTickLocation(d->tickLocation->currentIndex());
	a->SetLabelOffset(d->labelOffset->value());

#if VTK_VERSION_NUMBER >= 90020210809ULL
	double off[2] = { d->titleOffset->value(), d->titleOffset->value() };
	a->SetTitleOffset(off);
#else
	a->SetTitleOffset(d->titleOffset->value());
#endif

	a->SetXAxisVisibility(d->xVisible->isChecked());
	a->SetYAxisVisibility(d->yVisible->isChecked());
	a->SetZAxisVisibility(d->zVisible->isChecked());

	a->SetXAxisLabelVisibility(d->xLabelVisible->isChecked());
	a->SetYAxisLabelVisibility(d->yLabelVisible->isChecked());
	a->SetZAxisLabelVisibility(d->zLabelVisible->isChecked());

	a->SetXAxisTickVisibility(d->xTickVisible->isChecked());
	a->SetYAxisTickVisibility(d->yTickVisible->isChecked());
	a->SetZAxisTickVisibility(d->zTickVisible->isChecked());

	a->SetXAxisMinorTickVisibility(d->xMinorTickVisible->isChecked());
	a->SetYAxisMinorTickVisibility(d->yMinorTickVisible->isChecked());
	a->SetZAxisMinorTickVisibility(d->zMinorTickVisible->isChecked());

	a->SetDrawXGridlines(d->xGridlines->isChecked());
	a->SetDrawYGridlines(d->yGridlines->isChecked());
	a->SetDrawZGridlines(d->zGridlines->isChecked());

	a->SetDrawXInnerGridlines(d->xInnerGridlines->isChecked());
	a->SetDrawYInnerGridlines(d->yInnerGridlines->isChecked());
	a->SetDrawZInnerGridlines(d->zInnerGridlines->isChecked());

	a->SetDrawXGridpolys(d->xGridpolys->isChecked());
	a->SetDrawYGridpolys(d->yGridpolys->isChecked());
	a->SetDrawZGridpolys(d->zGridpolys->isChecked());

	a->SetUseTextActor3D(d->useTextActor3D->isChecked());
	a->SetUse2DMode(d->use2DMode->isChecked());
	a->SetStickyAxes(d->stickyAxes->isChecked());
	a->SetCenterStickyAxes(d->centerStickyAxes->isChecked());

	a->SetCornerOffset(d->cornerOffset->value());

	a->SetFlyMode(d->flyMode->currentIndex());
	a->SetGridLineLocation(d->gridLineLocation->currentIndex());
	a->SetInertia(d->inertia->value());

	a->SetEnableDistanceLOD(d->distanceLOD->isChecked());
	a->SetDistanceLODThreshold(d->distanceLODThreshold->value());
	a->SetEnableViewAngleLOD(d->viewAngleLOD->isChecked());
	a->SetViewAngleLODThreshold(d->viewAngleLODThreshold->value());

	d_data->view->refresh();
}





static VipVTKPlayerOptions _vtk_player_options;

const VipVTKPlayerOptions& VipVTKPlayerOptions::get()
{
	return _vtk_player_options;
}

void VipVTKPlayerOptions::set(const VipVTKPlayerOptions& opts)
{
	_vtk_player_options = opts;


}

void VipVTKPlayerOptions::save(VipArchive& arch) const
{
	arch.content("lighting", lighting);
	arch.content("orientationWidget", orientationWidget);
	arch.content("showHideFOVItems", showHideFOVItems);
	arch.content("defaultObjectColor", vipToQColor( VipVTKObject::defaultObjectColor()));
}
void VipVTKPlayerOptions::restore(VipArchive& arch)
{
	arch.content("lighting", lighting);
	arch.content("orientationWidget", orientationWidget);
	arch.content("showHideFOVItems", showHideFOVItems);

	QColor defaultObjectColor;
	arch.content("defaultObjectColor", defaultObjectColor);
	VipVTKObject::setDefaultObjectColor(vipFromQColor(defaultObjectColor));
}



class VipVTKPlayer::PrivateData
{
public:
	VipVTKGraphicsView* view;
	VipVTKObjectTreeWidget* tree;
	VipFOVTreeWidget* fov;
	VipSelectDisplayedAttributeWidget* properties;
	QSplitter* splitter;
	QPointer<QWidget> leftWidget;
	QPointer<VipProcessingPool> pool;

	VipCubeAxesActorWidget *axesEditor;

	QToolButton* camera;
	QAction* show_legend;
	QAction* reset_camera;
	QAction* sharedCamera;
	QAction* save_image;
	QAction* open_file;
	QAction* open_dir;
	QAction* tracking;
	QAction* axes;
	QAction* orientation_axes;
	QAction* light;

	std::unique_ptr<QStringList> pendingVisibleFOV;
	std::unique_ptr<VipFieldOfView> pendingCamera;
};

VipVTKPlayer::VipVTKPlayer(QWidget* parent)
  : VipVideoPlayer(new VipVTKGraphicsView(), parent)
{
	VIP_CREATE_PRIVATE_DATA();

	

	// disable the ImageProcessing plugin on this player
	this->setProperty("NoImageProcessing", true);
	// this->superimposeAction()->setVisible(false);

	// for AdvancedDisplay plugin
	setProperty("_vip_moveKeyModifiers", (int)Qt::AltModifier);
	// hide title
	this->plotWidget2D()->area()->titleAxis()->setVisible(false);

	d_data->view = static_cast<VipVTKGraphicsView*>(this->plotWidget2D());

	// For shared cameras
	connect(d_data->view, SIGNAL(cameraUpdated()), this, SLOT(cameraChanged()));

	d_data->view->area()->setMousePanning(Qt::NoButton);
	d_data->view->area()->setMouseWheelZoom(false);
	// this->toolBar()->selectionModeAction->setVisible(false);
	// this->recordAction()->setVisible(false);
	this->toolBar()->selectionModeAction->setVisible(false);
	this->showAxesAction()->setChecked(false);
	this->showAxesAction()->setVisible(false);
	this->showAxes(false);

	this->plotSceneModel()->setDrawComponent(QString(), VipPlotShape::FillPixels, false);
	this->plotSceneModel()->setShapesRenderHints(QString(), QPainter::Antialiasing | QPainter::TextAntialiasing);

	d_data->tree = new VipVTKObjectTreeWidget(d_data->view);
	d_data->fov = new VipFOVTreeWidget(d_data->view);

	d_data->properties = new VipSelectDisplayedAttributeWidget(d_data->view);
	d_data->properties->setStyleSheet("QToolBar {border-style: flat; spacing: 3px;}");

	//d_data->tree->setStyleSheet("QWidget { background-color: transparent; }");
	d_data->tree->resize(100, d_data->tree->height());

	//d_data->fov->setStyleSheet("QWidget { background-color: transparent; }");
	d_data->fov->resize(200, d_data->fov->height());

	QSplitter* left = new QSplitter(Qt::Vertical);
	//left->setStyleSheet("QSplitter { background-color: transparent; }");
	left->addWidget(d_data->tree);
	left->addWidget(d_data->fov);
	d_data->splitter = left;

	d_data->leftWidget = new QWidget();
	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(left);
	d_data->leftWidget->setLayout(lay);

	//new ResizableWidget(Qt::Horizontal, d_data->leftWidget);
	//this->gridLayout()->addWidget(d_data->leftWidget, 9, 9, 3, 1);
	//this->gridLayout()->setColumnStretch(10, 2);

	d_data->tree->hide();
	// fov->hide();


	QAction* first = toolBar()->actions().first();
	d_data->open_file = new QAction();
	d_data->open_file->setIcon(vipIcon("open_file.png"));
	d_data->open_file->setToolTip("Add data files to this player (CAD files, FOV)");
	d_data->open_dir = new QAction();
	d_data->open_dir->setIcon(vipIcon("open_dir.png"));
	d_data->open_dir->setToolTip("Add data directory to this player (CAD files, FOV)");
	toolBar()->insertAction(first, d_data->open_dir);
	toolBar()->insertAction(d_data->open_dir, d_data->open_file);
	
	d_data->show_legend = toolBar()->addAction(vipIcon("show_legend.png"), "Show/hide the legend");
	d_data->show_legend->setCheckable(true);
	
	QToolButton* camera = new QToolButton();
	camera->setAutoRaise(true);
	camera->setIcon(vipIcon("open_fov.png"));
	camera->setToolTip("Reset camera");
	camera->setPopupMode(QToolButton::MenuButtonPopup);
	camera->setCheckable(true);
	camera->setChecked(true);
	VipDragMenu* cmenu = new VipDragMenu();
	camera->setMenu(cmenu);
	connect(cmenu->addAction(vipIcon("plusX.png"), "Set view direction to +X"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToPositiveX()));
	connect(cmenu->addAction(vipIcon("minusX.png"), "Set view direction to -X"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToNegativeX()));
	connect(cmenu->addAction(vipIcon("plusY.png"), "Set view direction to +Y"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToPositiveY()));
	connect(cmenu->addAction(vipIcon("minusY.png"), "Set view direction to -Y"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToNegativeY()));
	connect(cmenu->addAction(vipIcon("plusZ.png"), "Set view direction to +Z"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToPositiveZ()));
	connect(cmenu->addAction(vipIcon("minusZ.png"), "Set view direction to -Z"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToNegativeZ()));
	connect(cmenu->addAction(vipIcon("isometric.png"), "Apply isometric view"), SIGNAL(triggered(bool)), d_data->view, SLOT(resetActiveCameraToIsometricView()));
	connect(cmenu->addAction(vipIcon("rotate_right.png"), "Rotate 90 degrees clockwise"), SIGNAL(triggered(bool)), d_data->view, SLOT(rotateClockwise90()));
	connect(cmenu->addAction(vipIcon("rotate_left.png"), "Rotate 90 degrees counterclockwise"), SIGNAL(triggered(bool)), d_data->view, SLOT(rotateCounterClockwise90()));
	d_data->reset_camera = toolBar()->addWidget(camera);
	d_data->camera = camera;
	connect(camera, SIGNAL(clicked(bool)), this, SLOT(setAutoCamera(bool)));


	d_data->sharedCamera = toolBar()->addAction(vipIcon("zoom.png"), "<b>Shared zoom</b><br>Zooming or panning within a video will apply the same zoom/panning to other videos in this workspace.");
	d_data->sharedCamera->setCheckable(true);
	connect(d_data->sharedCamera, SIGNAL(triggered(bool)), this, SLOT(setSharedCamera(bool)));

	d_data->tracking = toolBar()->addAction(vipIcon("cursor.png"), "Enable/disable CAD object information display");

	d_data->axesEditor = new VipCubeAxesActorWidget();
	d_data->axesEditor->setView(d_data->view);


	QToolButton* show_axes = new QToolButton();
	show_axes->setAutoRaise(true);
	show_axes->setIcon(vipIcon("axises.png"));
	show_axes->setToolTip("Show/Hide world coordinate axes");
	show_axes->setCheckable(true);
	show_axes->setPopupMode(QToolButton::MenuButtonPopup);
	VipDragMenu* menu = new VipDragMenu();
	menu->setWidget(d_data->axesEditor);
	show_axes->setMenu(menu);

	d_data->axes = toolBar()->addWidget(show_axes);

	d_data->orientation_axes = toolBar()->addAction(vipIcon("display_axes.png"), "Show/hide orientation widget");
	d_data->orientation_axes->setCheckable(true);
	d_data->orientation_axes->setChecked(true);

	d_data->light = toolBar()->addAction(vipIcon("light_orange.png"), "Enable/disable lighting");
	d_data->light->setCheckable(true);
	d_data->light->setChecked(true);

	d_data->tracking->setCheckable(true);
	d_data->axes->setCheckable(true);
	toolBar()->setIconSize(QSize(20, 20));
	toolBar()->addSeparator();

	toolBar()->addWidget(d_data->properties);

	//d_data->save_image->setVisible(false);

	this->plotWidget2D()->area()->canvas()->boxStyle().setBorderPen(QPen(Qt::NoPen));
	// we want to catch the mouse events on the canvas
	this->plotWidget2D()->area()->canvas()->setVisible(true);
	this->plotWidget2D()->area()->canvas()->setItemAttribute(VipPlotItem::IgnoreMouseEvents, false);
	this->spectrogram()->colorMap()->setVisible(false);
	this->setZoomFeaturesVisible(false);

	connect(d_data->show_legend, SIGNAL(triggered(bool)), this, SLOT(setLegendVisible(bool)));
	connect(d_data->orientation_axes, SIGNAL(triggered(bool)), view(), SLOT(setOrientationMarkerWidgetVisible(bool)));
	connect(d_data->light, SIGNAL(triggered(bool)), view(), SLOT(setLighting(bool)));
	//connect(d_data->save_image, SIGNAL(triggered(bool)), this, SLOT(saveImage()));
	connect(d_data->open_file, SIGNAL(triggered(bool)), this, SLOT(loadCadFiles()));
	connect(d_data->open_dir, SIGNAL(triggered(bool)), this, SLOT(loadCadDirectory()));
	connect(d_data->tracking, SIGNAL(triggered(bool)), this, SLOT(setTrackingEnable(bool)));
	connect(show_axes, SIGNAL(clicked(bool)), d_data->view, SLOT(setAxesVisible(bool)));

	this->style()->unpolish(this);
	this->style()->polish(this);

	d_data->view->area()->setDrawSelectionOrder(nullptr);
	frozenAction()->setVisible(false);
	zoomWidget()->setVisible(false);
	setLegendVisible(false);

	// Apply pending actions on session loading
	connect(vipGetMainWindow(), SIGNAL(sessionLoaded()), this, SLOT(applyPendingActions()));

	// Affect unique id for type VipVTKPlayer
	VipUniqueId::id(this);

	// Hide tool tip
	this->setToolTipFlags(VipToolTip::Hidden);

	// Apply global options;
	fov()->setVisible(VipVTKPlayerOptions::get().showHideFOVItems);
	setLighting(VipVTKPlayerOptions::get().lighting);
	setOrientationMarkerWidgetVisible(VipVTKPlayerOptions::get().orientationWidget);
}

VipVTKPlayer::~VipVTKPlayer()
{
	if (d_data->leftWidget)
		d_data->leftWidget->deleteLater();
}

VipCubeAxesActorWidget* VipVTKPlayer::cubeAxesActorEditor() const
{
	return d_data->axesEditor;
}

void VipVTKPlayer::setAutoCamera(bool enable)
{
	d_data->camera->blockSignals(true);
	d_data->camera->setChecked(enable);
	d_data->camera->blockSignals(false);
	d_data->view->setResetCameraEnabled(enable);
	if (enable)
		d_data->view->resetCamera();
}
bool VipVTKPlayer::isAutoCamera() const
{
	return d_data->camera->isChecked();
}

bool VipVTKPlayer::isSharedCamera() const
{
	bool res = false;
	if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChild(const_cast<VipVTKPlayer*>(this))) {
		res = area->property("_vip_sharedCamera").toBool();
	}
	else
		res = d_data->sharedCamera->isChecked();

	if (d_data->sharedCamera->isChecked() != res) {
		d_data->sharedCamera->blockSignals(true);
		d_data->sharedCamera->setChecked(res);
		d_data->sharedCamera->blockSignals(false);
	}
	return res;
}
void VipVTKPlayer::setSharedCamera(bool enable)
{
	d_data->sharedCamera->blockSignals(true);
	d_data->sharedCamera->setChecked(enable);
	d_data->sharedCamera->blockSignals(false);
	if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChild((this))) {
		area->setProperty("_vip_sharedCamera", enable);
		if (enable)
			applyThisCameraToAll();
	}
}

void VipVTKPlayer::applyThisCameraToAll()
{
	if (!isSharedCamera())
		return;

	if (this->property("_vip_watched").toBool())
		return;

	this->setProperty("_vip_watched", true);
	vtkCamera* cam = view()->renderer()->GetActiveCamera();
	if (VipDisplayPlayerArea* area = VipDisplayPlayerArea::fromChild(this)) {
		// Get all workspace VipVTKPlayer
		QList<VipVTKPlayer*> pls = area->findChildren<VipVTKPlayer*>();

		for (VipVTKPlayer* pl : pls) {
			if (pl->property("_vip_watched").toBool() || cam == pl->view()->renderer()->GetActiveCamera())
				continue;

			pl->view()->renderer()->GetActiveCamera()->DeepCopy(cam);
			pl->setProperty("_vip_watched", true);
			pl->view()->widget()->applyCameraToAllLayers();
			pl->view()->immediateRefresh(); 
			// This seems to be the only way to properly update another renderer window 
			// without glitches
			pl->view()->widget()->simulateMouseClick(QPoint(), QPoint());
		}

		for (VipVTKPlayer* pl : pls) 
			pl->setProperty("_vip_watched", false);
	}
}

void VipVTKPlayer::setPendingVisibleFOV(const QStringList& names)
{
	d_data->pendingVisibleFOV.reset(new QStringList(names));
}
void VipVTKPlayer::setPendingCurrentCamera(const VipFieldOfView& fov)
{
	d_data->pendingCamera.reset(new VipFieldOfView(fov));
}
void VipVTKPlayer::setPendingVisibleAttribute(VipVTKObject::AttributeType type, const QString& name, int comp)
{
	attributes()->setPendingDisplayedAttribute( type, name, comp );
}

void VipVTKPlayer::applyDelayedPendingActions(const VipFieldOfView& fov) 
{
	QSize min_size = view()->minimumSize();
	QSize new_min_size = view()->size() + QSize(1, 0);
	view()->setMinimumSize(new_min_size);
	view()->setMinimumSize(min_size);
	view()->refresh();

	QMetaObject::invokeMethod(view(), "resetCamera", Qt::QueuedConnection);
	if (!fov.name.isEmpty())
		QMetaObject::invokeMethod(view(), "setCurrentCamera", Qt::QueuedConnection,Q_ARG(VipFieldOfView,fov));
}

void VipVTKPlayer::applyPendingActions()
{	

	// Apply visible FOV pyramids
	QStringList names = d_data->pendingVisibleFOV ? *d_data->pendingVisibleFOV : QStringList();
	d_data->pendingVisibleFOV.reset();
	for (const QString& name : names) {
		if (VipFOVItem* item = fov()->fieldOfViewItem(name)) {
			item->showFOVPyramid(true);
		}
	}

	// Apply current camera
	if (d_data->pendingCamera) {
		QMetaObject::invokeMethod(this, "applyDelayedPendingActions", Qt::QueuedConnection, Q_ARG(VipFieldOfView, *d_data->pendingCamera));
		d_data->pendingCamera.reset();
	}
}

VipVTKGraphicsView* VipVTKPlayer::view() const
{
	return d_data->view;
}
VipVTKObjectTreeWidget* VipVTKPlayer::tree() const
{
	return d_data->tree;
}
VipFOVTreeWidget* VipVTKPlayer::fov() const
{
	return d_data->fov;
}
QSplitter* VipVTKPlayer::verticalSplitter() const
{
	return d_data->splitter;
}
QWidget* VipVTKPlayer::leftWidget() const
{
	return d_data->leftWidget;
}
VipSelectDisplayedAttributeWidget* VipVTKPlayer::attributes() const
{
	return d_data->properties;
}

void VipVTKPlayer::startRender(VipRenderState& state)
{
	d_data->leftWidget->hide();
	VipVideoPlayer::startRender(state);
}

void VipVTKPlayer::endRender(VipRenderState& state)
{
	d_data->leftWidget->show();
	VipVideoPlayer::endRender(state);
}

QSize VipVTKPlayer::sizeHint() const
{
	return QSize(800, 600);
}

QList<VipDisplayObject*> VipVTKPlayer::displayObjects() const
{
	QList<VipDisplayObject*> res = VipVideoPlayer::displayObjects();
	// res += vipListCast<VipDisplayObject*>(fov()->GetMappers());
	res += vipListCast<VipDisplayObject*>(fov()->displayObjects());
	return res;
}

void VipVTKPlayer::setLegendVisible(bool visible)
{
	view()->area()->legend()->setVisible(visible);
	view()->area()->recomputeGeometry();
	view()->refresh();
}

void VipVTKPlayer::setOrientationMarkerWidgetVisible(bool vis)
{
	d_data->orientation_axes->blockSignals(true);
	d_data->orientation_axes->setChecked(vis);
	d_data->orientation_axes->blockSignals(false);
	view()->setOrientationMarkerWidgetVisible(vis);
}
void VipVTKPlayer::setAxesVisible(bool vis)
{
	d_data->axes->blockSignals(true);
	d_data->axes->setChecked(vis);
	d_data->axes->blockSignals(false);
	view()->setAxesVisible(vis);
}
void VipVTKPlayer::setLighting(bool enable)
{
	d_data->light->blockSignals(true);
	d_data->light->setChecked(enable);
	d_data->light->blockSignals(false);
	view()->setLighting(enable);
}

void VipVTKPlayer::loadCadDirectory()
{
	QString dir = VipFileDialog::getExistingDirectory(nullptr, "Open data directory");
	if (!dir.isEmpty()) {
		/*QVariantList lst = LoadAnyDirectory(dir,(QStringList()<<"*.fov" )+ vtkFileFilters()+ imageFilters());
		VipVTKObjectList data;

		for(int i=0; i < lst.size(); ++i)
		{
			QVariant v = lst[i];

			if(v.canConvert<VipVTKObject>())
				data << v.value<VipVTKObject>();
			else if(v.canConvert<VipFieldOfView>())
				fov->addFieldOfView(v.value<VipFieldOfView>());
		}

		view->addObject(data);*/

		vipGetMainWindow()->openPaths(VipPathList() << VipPath(dir, true), this);
	}
}

void VipVTKPlayer::loadCadFiles()
{
	QStringList filters = VipIODevice::possibleReadFilters(QString(), QByteArray(), QVariant::fromValue(VipVTKObject()));
	filters += VipIODevice::possibleReadFilters(QString(), QByteArray(), QVariant::fromValue(VipFieldOfView()));
	vipUnique(filters);
	QString filter =
	  filters.join(";;"); //"Any files(" + (vtkFileFilters()+ imageFilters() <<"*.fov").join(" ") +  ");; CAD files(" + vtkFileFilters().join(" ") +  ");; FOV files(*.fov))";
	QStringList lst = VipFileDialog::getOpenFileNames(nullptr, "Open data files", filter);

	VipPathList paths;
	for (int i = 0; i < lst.size(); ++i)
		paths.append(lst[i]);
	vipGetMainWindow()->openPaths(paths, this);

	/*VipProgress display;
	display.setRange(0,lst.size()-1);
	display.setCancelable(true);
	display.setModal(true);

	for(int i=0; i < lst.size(); ++i)
	{
		display.setText("Load " + lst[i]);
		QVariantList variants = LoadAnyFile(lst[i]);
		VipVTKObjectList data;

		for(int j=0; j < variants.size(); ++j)
		{
			QVariant v = variants[j];

			if(v.canConvert<VipVTKObject>())
				data << v.value<VipVTKObject>();
			else if(v.canConvert<VipFieldOfView>())
				fov->addFieldOfView(v.value<VipFieldOfView>());
		}

		view->addObject(data);
	}*/
}

void VipVTKPlayer::setTrackingEnable(bool enable)
{
	if (d_data->view->trackingEnabled() != enable) {
		d_data->view->setTrackingEnable(enable);
	}

	d_data->tracking->blockSignals(true);
	d_data->tracking->setChecked(enable);
	d_data->tracking->blockSignals(false);
}

void VipVTKPlayer::saveImage()
{
	QString filename = VipFileDialog::getSaveFileName(nullptr, "Save screen in image", "Image file (*.bmp *.png *.jpg *.jpeg *.tif *.tiff)");
	if (filename.isEmpty())
		return;

	QImage res = d_data->view->imageContent(1).toQImage(nullptr);
	res.save(filename);
}

void VipVTKPlayer::setProcessingPool(VipProcessingPool* p)
{
	if (d_data->pool)
		disconnect(d_data->pool, SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged(qint64)));

	d_data->pool = p;
	if (p) {
		connect(d_data->pool, SIGNAL(timeChanged(qint64)), this, SLOT(timeChanged(qint64)), Qt::DirectConnection);
		timeChanged(p->time());
	}
	VipVideoPlayer::setProcessingPool(p);

	isSharedCamera();
}

void VipVTKPlayer::showAxes(bool)
{
	// disable axes permanently
	VipVideoPlayer::showAxes(false);
}

void VipVTKPlayer::timeChanged(qint64)
{
	/*VipVTKObjectList lst = view->objects();
	for (int i = 0; i < lst.size(); ++i)
		lst[i]->applyDynamicProperty(time);*/
}

void VipVTKPlayer::cameraChanged()
{
	if (isSharedCamera())
		applyThisCameraToAll();
}



#include <QTextDocument>
#include <qclipboard.h>
static void copyToClipboard(VipVTKPlayer* player)
{
	// remove html tags
	QTextDocument doc;
	doc.setHtml(player->view()->infos()->text());

	QApplication::clipboard()->setText(doc.toPlainText());
}

static QList<QAction*> onRightClick(VipPlotItem*, VipVTKPlayer* player)
{
	if (player) {
		QAction* act = new QAction(vipIcon("copy.png"), "Copy displayed parameters to clipboard", player);
		QObject::connect(act, &QAction::triggered, std::bind(copyToClipboard, player));
		return QList<QAction*>() << act;
	}
	return QList<QAction*>();
}

static void onMainWidgetCreated(VipVTKPlayer* player)
{

	if (VipBaseDragWidget* d = VipBaseDragWidget::fromChild(player)) {
		// Add widget used to record a Tokida window
		if (!player->property("addRecord").toBool()) {
			player->setProperty("addRecord", true);

			VipRecordWidgetButton* button = new VipRecordWidgetButton(d);

			QAction* act = player->toolBar()->addWidget(button);
			act->setToolTip(button->toolTip());

			// Whenever recording is on, the player tool bar is hidden, and we cannot stop recording!
			// Add another button on the top left corner of the player

			QToolButton* stop = new QToolButton(d);
			stop->setIcon(vipIcon("record_icon.png"));
			stop->setToolTip("Stop recording");
			stop->move(0, 0);
			stop->hide();
			stop->setMaximumWidth(20);

			QObject::connect(button, SIGNAL(started()), stop, SLOT(show()));
			QObject::connect(button, SIGNAL(stopped()), stop, SLOT(hide()));
			QObject::connect(stop, SIGNAL(clicked(bool)), button, SLOT(stop()));
		}
	}
}

static void setMainWidget(VipDragWidget* drag, VipVTKPlayer* player)
{
	/*if (VipDragWidgetHeader* h = qobject_cast<VipDragWidgetHeader*>(drag->Header()))
	{
		QAction * tracking = new QAction(vipIcon("cursor.png"), "Enable/disable CAD object information display", player);
		tracking->setCheckable(true);
		QObject::connect(tracking, SIGNAL(triggered(bool)), player, SLOT(setTrackingEnable(bool)));
		h->afterTitleToolbar()->insertAction(h->afterTitleToolbar()->actions().first(), tracking);
		*/
	/*QAction * show_scale = new QAction(vipIcon("scalevisible.png"), "Show/hide color scale", player);
	show_scale->setCheckable(true);
	show_scale->setChecked(player->spectrogram()->colorMap()->isVisible());
	QAction * auto_scale = new QAction(vipIcon("scaleauto.png"), "Toogle auto scaling", player);
	auto_scale->setCheckable(true);
	auto_scale->setChecked(player->spectrogram()->colorMap()->isAutoScale());
	QAction * fit_to_grip = new QAction(vipIcon("fit_to_scale.png"), "Fit color scale to grips", player);
	QAction * histo_scale = new QAction(vipIcon("scalehisto.png"), "Adjust color scale to have the best dynamic", player);
	histo_scale->setCheckable(true);
	histo_scale->setChecked(player->spectrogram()->colorMap()->useFlatHistogram());
	QAction * scale_params = new QAction(vipIcon("scaletools.png"), "Display color scale parameters", player);

	VipColorScaleButton * scale = new VipColorScaleButton();
	scale->setColorPalette(player->colorMap());

	h->afterTitleToolbar()->insertSeparator(h->afterTitleToolbar()->actions().first());
	QAction * sc = h->afterTitleToolbar()->insertWidget(h->afterTitleToolbar()->actions().first(), scale);
	h->afterTitleToolbar()->insertAction(sc, scale_params);
	h->afterTitleToolbar()->insertAction(scale_params, histo_scale);
	h->afterTitleToolbar()->insertAction(histo_scale, fit_to_grip);
	h->afterTitleToolbar()->insertAction(fit_to_grip, auto_scale);
	h->afterTitleToolbar()->insertAction(auto_scale, show_scale);

	QObject::connect(show_scale, SIGNAL(triggered(bool)), player, SLOT(setColorScaleVisible(bool)));
	QObject::connect(auto_scale, SIGNAL(triggered(bool)), player, SLOT(setAutomaticColorScale(bool)));
	QObject::connect(fit_to_grip, SIGNAL(triggered(bool)), player, SLOT(fitColorScaleToGrips()));
	QObject::connect(histo_scale, SIGNAL(triggered(bool)), player, SLOT(setFlatHistogramColorScale(bool)));
	QObject::connect(scale_params, SIGNAL(triggered(bool)), player, SLOT(showColorScaleParameters()));

	QObject::connect(player->spectrogram()->colorMap(), SIGNAL(visibilityChanged(bool)), show_scale, SLOT(setChecked(bool)));
	QObject::connect(player->spectrogram()->colorMap(), SIGNAL(autoScaleChanged(bool)), auto_scale, SLOT(setChecked(bool)));
	QObject::connect(player->spectrogram()->colorMap(), SIGNAL(useFlatHistogramChanged(bool)), histo_scale, SLOT(setChecked(bool)));

	QObject::connect(scale, SIGNAL(colorPaletteChanged(int)), player, SLOT(setColorMap(int)));
	QObject::connect(player, SIGNAL(colorMapChanged(int)), scale, SLOT(setColorPalette(int)));*/
	//}
}

static VipArchive& operator<<(VipArchive& arch, const VipVTKPlayer* w)
{
	// left widget max width and splitter size
	int max_width = w->leftWidget()->maximumWidth();
	arch.content("max_width", w->leftWidget()->maximumWidth());
	arch.content("splitter_state", w->verticalSplitter()->saveState());

	// max tree depth
	arch.content("tree_depth", w->tree()->maxDepth());

	// auto camera
	arch.content("auto_camera", w->isAutoCamera());

	// active camera
	arch.content("camera", w->view()->currentCamera());
	// visible FOV pyramid
	arch.content("FOVPyramids", w->fov()->visibleFOVPyramidNames());
	// displayed attribute
	arch.content("attribute_type", (int)w->attributes()->currentAttributeType());
	arch.content("attribute_name", w->attributes()->currentAttribute());
	arch.content("attribute_comp", w->attributes()->currentComponent());

	arch.content("orientation_widget_visible", w->view()->orientationMarkerWidgetVisible());
	arch.content("axes_visible", w->view()->axesVisible());
	arch.content("lighting", w->view()->lighting());


	arch.content("VipVTKGraphicsView", w->view());
	arch.content("VipVTKObjectTreeWidget", w->tree());
	arch.content("VipFOVTreeWidget", w->fov());
	arch.content("VipSelectDisplayedAttributeWidget", w->attributes());
	return arch;
}

static VipArchive& operator>>(VipArchive& arch, VipVTKPlayer* w)
{
	// left widget max width and splitter size
	int max_width = INT_MAX;
	QByteArray splitter_state;
	VipFieldOfView fov;
	arch.content("max_width", max_width);
	arch.content("splitter_state", splitter_state);

	// max tree depth
	w->tree()->setMaxDepth(arch.read("tree_depth").toInt());

	arch.save();
	bool auto_camera = true;
	// auto camera
	if (arch.content("auto_camera", auto_camera)) 
		w->setAutoCamera(auto_camera);
	else
		arch.restore();

	w->leftWidget()->setMaximumWidth(max_width);
	w->leftWidget()->setMinimumWidth(max_width);
	w->verticalSplitter()->restoreState(splitter_state);

	// active camera
	arch.content("camera", fov);
	w->setPendingCurrentCamera(fov);
	w->setPendingVisibleFOV(arch.read("FOVPyramids").value<QStringList>());
	int type = arch.read("attribute_type").toInt();
	QString name = arch.read("attribute_name").toString();
	int comp = arch.read("attribute_comp").toInt();
	w->setPendingVisibleAttribute((VipVTKObject::AttributeType)type, name, comp);

	w->setOrientationMarkerWidgetVisible( arch.read("orientation_widget_visible").toBool());
	w->setAxesVisible(arch.read("axes_visible").toBool());
	w->setLighting(arch.read("lighting").toBool());
	
	arch.content("VipVTKGraphicsView", w->view());
	arch.content("VipVTKObjectTreeWidget", w->tree());
	arch.content("VipFOVTreeWidget", w->fov());
	arch.content("VipSelectDisplayedAttributeWidget", w->attributes());
	arch.resetError();
	w->view()->resetCamera();
	return arch;
}




class VipVTKPlayerOptionPage::PrivateData
{
public:
	QToolButton lighting;
	QToolButton orientationWidget;
	QToolButton showHideFOVItems;
	VipColorWidget defaultObjectColor;
};


VipVTKPlayerOptionPage::VipVTKPlayerOptionPage(QWidget* parent)
  : VipPageOption(parent)
{
	VIP_CREATE_PRIVATE_DATA();

	this->setWindowIcon(vipIcon("CAD.png"));

	d_data->lighting.setIcon(vipIcon("light_orange.png"));
	d_data->lighting.setCheckable(true);

	d_data->orientationWidget.setIcon(vipIcon("display_axes.png"));
	d_data->orientationWidget.setCheckable(true);

	d_data->showHideFOVItems.setIcon(vipIcon("fov_displayed.png"));
	d_data->showHideFOVItems.setCheckable(true);

	d_data->defaultObjectColor.setColor(vipToQColor(VipVTKObject::defaultObjectColor()));

	QGridLayout* lay = new QGridLayout();
	int row = 0;

	lay->addWidget(new QLabel("Enable lighting"), row, 0, Qt::AlignLeft);
	lay->addWidget(&d_data->lighting, row++, 1, Qt::AlignLeft);

	lay->addWidget(new QLabel("Display orientation widget"), row, 0, Qt::AlignLeft);
	lay->addWidget(&d_data->orientationWidget, row++, 1, Qt::AlignLeft);

	lay->addWidget(new QLabel("Display Field Of View list"), row, 0, Qt::AlignLeft);
	lay->addWidget(&d_data->showHideFOVItems, row++, 1, Qt::AlignLeft);

	lay->addWidget(new QLabel("Default 3D object color"), row, 0, Qt::AlignLeft);
	lay->addWidget(&d_data->defaultObjectColor, row++, 1, Qt::AlignLeft);

	QVBoxLayout* vlay = new QVBoxLayout();
	vlay->setContentsMargins(0,0,0,0);
	vlay->addLayout(lay);
	vlay->addStretch(1);

	setLayout(vlay);
}
VipVTKPlayerOptionPage::~VipVTKPlayerOptionPage() {}

void VipVTKPlayerOptionPage::applyPage() 
{
	VipVTKPlayerOptions opts;
	opts.lighting = d_data->lighting.isChecked();
	opts.orientationWidget = d_data->orientationWidget.isChecked();
	opts.showHideFOVItems = d_data->showHideFOVItems.isChecked();
	QColor defaultObjectColor = d_data->defaultObjectColor.color();
	VipVTKObject::setDefaultObjectColor(vipFromQColor( defaultObjectColor));

	VipVTKPlayerOptions::set(opts);

	QList<VipVTKPlayer*> players = VipUniqueId::objects<VipVTKPlayer>();
	for (VipVTKPlayer* p : players) {
		p->setLighting(opts.lighting);
		p->setOrientationMarkerWidgetVisible(opts.orientationWidget);
		p->fov()->setVisible(opts.showHideFOVItems);
	}
}
void VipVTKPlayerOptionPage::updatePage() 
{
	VipVTKPlayerOptions opts = VipVTKPlayerOptions::get();

	d_data->lighting.setChecked(opts.lighting);
	d_data->orientationWidget.setChecked(opts.orientationWidget);
	d_data->showHideFOVItems.setChecked(opts.showHideFOVItems);
	d_data->defaultObjectColor.setColor(vipToQColor(VipVTKObject::defaultObjectColor()));
}






/**\internal Create a VipDisplayVTKObject object from a VipVTKObject data*/
static VipDisplayVTKObject* createDisplayDataObject(const VipVTKObject& data, VipAbstractPlayer* pl, const VipAnyData& any)
{
	VipDisplayVTKObject* disp = new VipDisplayVTKObject();
	disp->inputAt(0)->setData(any);
	disp->item()->setData(QVariant::fromValue(data));
	// disp->item()->setData(QVariant::fromValue(data));
	if (VipVTKPlayer* player = qobject_cast<VipVTKPlayer*>(pl)) {
		disp->item()->setAxes(player->view()->area()->canvas()->axes(), VipCoordinateSystem::Cartesian);
	}
	return disp;
}

static VipDisplayVTKObject* createDisplayDataObjectTokida(const VipVTKObject& data, VipVTKPlayer* pl, const VipAnyData& any)
{
	return createDisplayDataObject(data, pl, any);
}

/**\internal Create a VipVTKPlayer object from a VipDisplayVTKObject */
static QList<VipAbstractPlayer*> createTokidaPlayerFromDisplay(VipDisplayVTKObject* disp, VipAbstractPlayer* pl, VipOutput*, QObject*)
{
	if (!disp->item())
		return QList<VipAbstractPlayer*>();

	// remove the data object from the previous VipVTKGraphicsView
	// if (VipVTKPlayer * old = qobject_cast<VipVTKPlayer*>(disp->widget()))
	//	old->view()->remove(disp->item()->rawData());

	if (VipVTKPlayer* player = qobject_cast<VipVTKPlayer*>(pl)) {
		disp->item()->setAxes(player->view()->area()->canvas()->axes(), VipCoordinateSystem::Cartesian);
		return QList<VipAbstractPlayer*>() << pl;
	}
	else if (pl)
		return QList<VipAbstractPlayer*>();

	VipVTKPlayer* player = new VipVTKPlayer();
	disp->item()->setAxes(player->view()->area()->canvas()->axes(), VipCoordinateSystem::Cartesian);
	return QList<VipAbstractPlayer*>() << player;
}

/**\internal Create a VipDisplayVTKObject object from a VipVTKObject data*/
static VipDisplayFieldOfView* createDisplayFOV(const VipFieldOfView& data, VipAbstractPlayer* pl, const VipAnyData& any)
{
	VipDisplayFieldOfView* disp = new VipDisplayFieldOfView();
	disp->inputAt(0)->setData(any);
	disp->item()->setData(QVariant::fromValue(data));
	// disp->item()->setData(QVariant::fromValue(data));
	if (VipVTKPlayer* player = qobject_cast<VipVTKPlayer*>(pl)) {
		disp->item()->setAxes(player->view()->area()->canvas()->axes(), VipCoordinateSystem::Cartesian);
	}
	return disp;
}

static VipDisplayFieldOfView* createDisplayFOVTokida(const VipFieldOfView& data, VipVTKPlayer* pl, const VipAnyData& any)
{
	return createDisplayFOV(data, pl, any);
}

/**\internal Create a VipVTKPlayer object from a VipDisplayVTKObject */
static QList<VipAbstractPlayer*> createPlayerFromDisplayFOV(VipDisplayFieldOfView* disp, VipAbstractPlayer* pl, VipOutput*, QObject*)
{
	if (!disp->item())
		return QList<VipAbstractPlayer*>();

	// remove the data object from the previous VipVTKGraphicsView
	// if (VipVTKPlayer * old = qobject_cast<VipVTKPlayer*>(disp->widget()))
	//	old->view()->remove(disp->item()->rawData());

	if (VipVTKPlayer* player = qobject_cast<VipVTKPlayer*>(pl)) {
		disp->item()->setAxes(player->view()->area()->canvas()->axes(), VipCoordinateSystem::Cartesian);
		return QList<VipAbstractPlayer*>() << pl;
	}
	else if (pl)
		return QList<VipAbstractPlayer*>();

	VipVTKPlayer* player = new VipVTKPlayer();
	disp->item()->setAxes(player->view()->area()->canvas()->axes(), VipCoordinateSystem::Cartesian);
	return QList<VipAbstractPlayer*>() << player;
}


static int registerOperators()
{
	vipFDCreateDisplayFromData().append<VipDisplayVTKObject*(const VipVTKObject&, VipAbstractPlayer*, const VipAnyData&)>(createDisplayDataObject);
	vipFDCreateDisplayFromData().append<VipDisplayVTKObject*(const VipVTKObject&, VipVTKPlayer*, const VipAnyData&)>(createDisplayDataObjectTokida);
	vipFDCreateDisplayFromData().append<VipDisplayFieldOfView*(const VipFieldOfView&, VipAbstractPlayer*, const VipAnyData&)>(createDisplayFOV);
	vipFDCreateDisplayFromData().append<VipDisplayFieldOfView*(const VipFieldOfView&, VipVTKPlayer*, const VipAnyData&)>(createDisplayFOVTokida);

	vipFDCreatePlayersFromProcessing().append<QList<VipAbstractPlayer*>(VipDisplayVTKObject*, VipAbstractPlayer*, VipOutput*, QObject*)>(createTokidaPlayerFromDisplay);
	vipFDCreatePlayersFromProcessing().append<QList<VipAbstractPlayer*>(VipDisplayFieldOfView*, VipAbstractPlayer*, VipOutput*, QObject*)>(createPlayerFromDisplayFOV);


	vipRegisterArchiveStreamOperators<VipVTKPlayer*>();
	VipFDItemRightClick().append<QList<QAction*>(VipPlotItem*, VipVTKPlayer*)>(onRightClick);
	vipSetDragWidget().append<void(VipDragWidget*, VipVTKPlayer*)>(setMainWidget);

	vipFDPlayerCreated().append<void(VipVTKPlayer*)>(onMainWidgetCreated);

	return 0;
}

static int _registerOperators = registerOperators();