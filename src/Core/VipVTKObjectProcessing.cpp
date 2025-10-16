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

#include "VipVTKObjectProcessing.h"


#include <vtkTransform.h>
#include <vtkUnstructuredGrid.h>

#include <QMessageBox>


void VipVTKObjectProcessing::apply()
{
	VipVTKObjectList inputs;
	qint64 time = VipInvalidTime;
	QString name;
	VipAnyData out_any;
	out_any.setAttributes(this->attributes());

	for (int i = 0; i < inputCount(); ++i) {
		VipAnyData any = inputAt(0)->data();
		VipVTKObject data = any.value<VipVTKObject>();
		if (!data) {
			setError("empty input data", VipProcessingObject::WrongInput);
			return;
		}
		if (time == VipInvalidTime)
			time = any.time();
		else
			time = qMax(time, any.time());
		if (name.isEmpty())
			name = data.dataName();
		inputs.push_back(data);
		out_any.mergeAttributes(any.attributes());
	}

	VipVTKObject out;
	try {
		auto locks = vipLockVTKObjects(inputs);
		out = applyAlgorithm(inputs, time);
	}
	catch (const std::exception& e) {
		setError(QString(e.what()));
		return;
	}

	if (!out) {
		if (!hasError())
			setError("empty output data");
		return;
	}

	out.setDataName(name);
	out_any.setTime(time);
	out_any.setName(QFileInfo(name).fileName());
	out_any.setSource(this);
	out_any.setData(QVariant::fromValue(out));

	outputAt(0)->setData(out_any);
}


VipVTKObject CosXYZ::applyAlgorithm(const VipVTKObjectList& inputs, qint64 time)
{ 
	VipVTKObject data;
	{

		auto lock = vipLockVTKObjects(inputs.first());
		qint64 st = QDateTime::currentMSecsSinceEpoch();
		data = inputs.first().copy();
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		printf("import: %i ms\n", (int)el);
	}
	vtkPoints* pts = data.pointSet()->GetPoints();

	for (int i = 0; i < pts->GetNumberOfPoints(); ++i) {
	
		double pt[3];
		pts->GetPoint(i,pt);
		double cos = std::cos(time * 1e-6 * pt[0]);
		pt[0] = pt[0] + pt[0] * cos * 0.5;
		pt[1] = pt[1] + pt[1] * cos * 0.5;
		pt[2] = pt[2] + pt[2] * cos * 0.5;
		pts->SetPoint(i, pt);
	}
	pts->Modified();
	data.modified();
	return data;
}




VipLinearTransform::VipLinearTransform()
{

	this->propertyAt(0)->setData(0);
	this->propertyAt(1)->setData(0.0);
	this->propertyAt(2)->setData(0.0);
	this->propertyAt(3)->setData(0.0);
	this->propertyAt(4)->setData(1.0);
	this->propertyAt(5)->setData(1.0);
	this->propertyAt(6)->setData(1.0);
	this->propertyAt(7)->setData(0.);
	this->propertyAt(8)->setData(0);
}

VipVTKObject VipLinearTransform::applyAlgorithm(const VipVTKObjectList& inputs, qint64 time)
{
	if (!inputs.first().data()->IsA("vtkPointSet"))
	{
		setError("Input must be of type vtkPointSet", VipProcessingObject::WrongInput);
		return VipVTKObject();
	}

	//create the output data object
	VipVTKObject res = inputs.first().copy();
	
	int tr_origin = propertyAt(0)->value<int>();

	double x_offset = propertyAt(1)->value<double>();
	double y_offset = propertyAt(2)->value<double>();
	double z_offset = propertyAt(3)->value<double>();

	double x_factor = propertyAt(4)->value<double>();
	double y_factor = propertyAt(5)->value<double>();
	double z_factor = propertyAt(6)->value<double>();

	double rotation = propertyAt(7)->value<double>();
	int axis = propertyAt(8)->value<int>();

	double origin[3] = { 0,0,0 };
	if (tr_origin == 0) //axis origin
	{
	}
	else if (tr_origin == 1) // bounding box top left
	{
		double bounds[6];
		res.dataSet()->GetBounds(bounds);
		origin[0] = bounds[0];
		origin[1] = bounds[2];
		origin[2] = bounds[4];
	}
	else if (tr_origin == 2) //bounding box center
	{
		double bounds[6];
		res.dataSet()->GetBounds(bounds);
		origin[0] = (bounds[0]+ bounds[1])/2;
		origin[1] = (bounds[2] + bounds[3]) / 2;
		origin[2] = (bounds[4] + bounds[5]) / 2;
	}
	else if (tr_origin == 3) //barycentre
	{
		vtkPointSet * set = static_cast<vtkPointSet*>(res.dataSet());
		vtkPoints * pts = set->GetPoints();
		int count = pts->GetNumberOfPoints();
		for (int i = 0; i < count; ++i)
		{
			double pt[3];
			pts->GetPoint(i, pt);
			origin[0] += pt[0]; origin[1] += pt[1]; origin[2] += pt[2];
		}
		origin[0] /= count; origin[1] /= count; origin[0] /= count;
	}
	else
	{
		setError("wrong origin parameter (" + QString::number(tr_origin) + ")");
		return VipVTKObject();
	}

	//build the vtkTransform for the rotation only
	vtkSmartPointer<vtkTransform> tr = vtkSmartPointer<vtkTransform>::New();
	tr->Identity();
	tr->PostMultiply();
	if (axis == 0)
		tr->RotateX(rotation);
	else if (axis == 1)
		tr->RotateY(rotation);
	else if (axis == 2)
		tr->RotateZ(rotation);
	else
	{
		setError("wrong rotation axis");
		return VipVTKObject();
	}
	tr->Update();

	vtkPointSet * set = static_cast<vtkPointSet*>( res.dataSet());
	vtkPoints * pts = set->GetPoints();
	int count = pts->GetNumberOfPoints();
	for (int i = 0; i < count; ++i)
	{
		double pt[3];
		pts->GetPoint(i,pt);

		pt[0] -= origin[0];
		pt[1] -= origin[1];
		pt[2] -= origin[2];

		pt[0] = pt[0] * x_factor + x_offset;
		pt[1] = pt[1] * y_factor + y_offset;
		pt[2] = pt[2] * z_factor + z_offset;

		tr->TransformPoint(pt, pt);

		pt[0] += origin[0];
		pt[1] += origin[1];
		pt[2] += origin[2];

		pts->SetPoint(i,pt);
	}
	
	return res;
}








#include <qlistwidget.h>

class VipPlotItem;
class VipLineEdit;
class VipVTKGraphicsView;
 
/**
Editor to create a vtkDataSet point attribute of any number of components.
You have to specify 2 values and point id for each component, and then use #VipVTKObject::setPointsAttribute to create the interpolated point attribute.
*/
/* class PointAttributeInterpolate : public QWidget
{
	TODO: add q_object macro

public:
	PointAttributeInterpolate();

	QString name() const;
	vtkVariantList components1() const;
	vtkVariantList components2() const;
	bool point1(double* coords) const;
	bool point2(double* coords) const;

	QVector<int> interpolationAxes() const;
	QListWidget* components();

	void setGraphicsView(VipVTKGraphicsView*);
	VipVTKGraphicsView* graphicsView() const;

public Q_SLOTS:
	void setPoint1SelectionEnabled(bool);
	void setPoint2SelectionEnabled(bool);
	bool apply();

private Q_SLOTS:
	void addComponent();
	void removeComponent();
	void itemClicked(VipPlotItem*, int);
	void checkPoints();

Q_SIGNALS:
	void succeded();
	void failed();

private:
	bool point(VipLineEdit* edit, double* coords) const;

	VIP_DECLARE_PRIVATE_DATA();
};*/

/**
Editor to create a VipLinearTransform.
*/
/*class LinearTransformEditor : public QWidget
{
	//TODO: add q_object macro

public:
	LinearTransformEditor();
	~LinearTransformEditor();

	vtkTransform * transform() const;

	void setLinearTransform(VipLinearTransform *);
	VipLinearTransform * linearTransform() const;

public Q_SLOTS:
	void updateLinearTransform();

private:

	VIP_DECLARE_PRIVATE_DATA();
};
*/


/*

#include "VipStandardWidgets.h"
#include <qboxlayout.h>
#include <qtimer.h>

class PointAttributeInterpolateWidget : public QWidget
{
public:
	PointAttributeInterpolate * widget;
	QListWidgetItem * item;
	VipDoubleEdit value1;
	VipDoubleEdit value2;
	VipComboBox axis;
	QToolButton add;
	QToolButton del;
	QLabel component;
	QTimer timer;

	PointAttributeInterpolateWidget(PointAttributeInterpolate * widget, QListWidgetItem * item)
		:widget(widget),item(item)
	{
		add.setText("+");
		add.setToolTip("Add a new component");
		del.setText(QString(QChar(0x00D7)));
		del.setToolTip("remove this component");
		value1.setValue(0);
		value1.setToolTip("Component value for point 1");
		value2.setValue(0);
		value2.setToolTip("Component value for point 2");
		axis.setChoices("X,Y,Z");
		axis.setCurrentIndex(0);

		QVBoxLayout * vlay = new QVBoxLayout();
		vlay->setContentsMargins(0, 0, 0, 0);
		vlay->setSpacing(0);
		vlay->addWidget(&add);
		vlay->addWidget(&del);

		QHBoxLayout * hlay = new QHBoxLayout();
		hlay->addLayout(vlay);
		hlay->addWidget(&component);
		hlay->addWidget(&value1);
		hlay->addWidget(new QLabel(" , "));
		hlay->addWidget(&value2);
		hlay->addWidget(new QLabel(" , interp. axis: "));
		hlay->addWidget(&axis);
		hlay->addStretch();
		hlay->setSizeConstraint(QLayout::SetFixedSize);
		setLayout(hlay);

		connect(&add, SIGNAL(clicked(bool)), widget, SLOT(addComponent()));
		connect(&del, SIGNAL(clicked(bool)), widget, SLOT(removeComponent()));
		connect(&timer, &QTimer::timeout, this, &PointAttributeInterpolateWidget::updateComponent);

		timer.setSingleShot(false);
		timer.setInterval(20);
		timer.start();
	}

	void updateComponent()
	{
		component.setText("Component "+QString::number(widget->components()->row(item)));
	}
};

class PointAttributeInterpolate::PrivateData 
{
public:
	VipLineEdit name;
	VipLineEdit coord1;
	VipLineEdit coord2;
	QToolButton selectCoord1;
	QToolButton selectCoord2;
	QListWidget components;
	QPointer<VipVTKGraphicsView> view;
};

PointAttributeInterpolate::PointAttributeInterpolate()
{
	VIP_CREATE_PRIVATE_DATA();

	QHBoxLayout * hlay1 = new QHBoxLayout();
	hlay1->addWidget(new QLabel("Attribute name: "));
	hlay1->addWidget(&d_data->name);
	d_data->name.setPlaceholderText("Enter name");

	QHBoxLayout * hlay2 = new QHBoxLayout();
	hlay2->addWidget(new QLabel("Point 1"));
	hlay2->addWidget(&d_data->coord1);
	hlay2->addWidget(&d_data->selectCoord1);
	hlay2->addWidget(VipLineWidget::createSunkenVLine());
	hlay2->addWidget(new QLabel("Point 2"));
	hlay2->addWidget(&d_data->coord2);
	hlay2->addWidget(&d_data->selectCoord2);

	d_data->coord1.setPlaceholderText("Coordinates");
	d_data->coord2.setPlaceholderText("Coordinates");
	d_data->coord1.setToolTip("Point 1 coordinates (format: 'X,Y,Z')");
	d_data->coord2.setToolTip("Point 1 coordinates (format: 'X,Y,Z')");
	d_data->selectCoord1.setToolTip("Manually select the point 1");
	d_data->selectCoord1.setIcon(vipIcon("cursor.png"));
	d_data->selectCoord1.setCheckable(true);
	d_data->selectCoord2.setToolTip("Manually select the point 2");
	d_data->selectCoord2.setIcon(vipIcon("cursor.png"));
	d_data->selectCoord2.setCheckable(true);

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addLayout(hlay1);
	vlay->addWidget(VipLineWidget::createSunkenHLine());
	vlay->addLayout(hlay2);
	vlay->addWidget(&d_data->components);

	setLayout(vlay);

	setMinimumWidth(500);

	connect(&d_data->coord1, SIGNAL(textChanged(const QString &)), this, SLOT(checkPoints()));
	connect(&d_data->coord2, SIGNAL(textChanged(const QString &)), this, SLOT(checkPoints()));
	connect(&d_data->selectCoord1, SIGNAL(clicked(bool)), this, SLOT(setPoint1SelectionEnabled(bool)));
	connect(&d_data->selectCoord2, SIGNAL(clicked(bool)), this, SLOT(setPoint2SelectionEnabled(bool)));
	connect(VipPlotItemManager::instance(), SIGNAL(itemClicked(VipPlotItem*, int)), this, SLOT(itemClicked(VipPlotItem*, int)));

	addComponent();
}

QString PointAttributeInterpolate::name() const
{
	return d_data->name.text();
}

vtkVariantList PointAttributeInterpolate::components1() const
{
	vtkVariantList res;
	for (int i = 0; i < d_data->components.count(); ++i)
	{
		res << static_cast<PointAttributeInterpolateWidget*>(d_data->components.itemWidget(d_data->components.item(i)))->value1.value();
	}
	return res;
}

vtkVariantList PointAttributeInterpolate::components2() const
{
	vtkVariantList res;
	for (int i = 0; i < d_data->components.count(); ++i)
	{
		res << static_cast<PointAttributeInterpolateWidget*>(d_data->components.itemWidget(d_data->components.item(i)))->value2.value();
	}
	return res;
}

void PointAttributeInterpolate::checkPoints()
{
	double coords[3];
	point1(coords);
	point2(coords);
}

bool PointAttributeInterpolate::point(VipLineEdit * edit, double * coords) const
{
	static QString rightStyle = "QLineEdit { border: 1px solid lightGray; }";
	static QString wrongStyle = "QLineEdit { border: 1px solid red; }";

	QString text = edit->text();
	text.replace(" ", "");
	QStringList lst = text.split(",", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	if (lst.size() != 3)
	{
		edit->setStyleSheet(wrongStyle);
		return false;
	}

	bool ok1 = false, ok2 = false, ok3 = false;
	coords[0] = lst[0].toDouble(&ok1);
	coords[1] = lst[1].toDouble(&ok2);
	coords[2] = lst[2].toDouble(&ok3);

	if(!ok1 || !ok2 || !ok3)
	{
		edit->setStyleSheet(wrongStyle);
		return false;
	}

	edit->setStyleSheet(rightStyle);
	return true;
}

bool PointAttributeInterpolate::point1(double * coords) const
{
	return point(const_cast<VipLineEdit*>(&d_data->coord1), coords);
}

bool PointAttributeInterpolate::point2(double * coords) const
{
	return point(const_cast<VipLineEdit*>(&d_data->coord2), coords);
}

QVector<int> PointAttributeInterpolate::interpolationAxes() const
{
	QVector<int> res;
	for (int i = 0; i < d_data->components.count(); ++i)
	{
		res << static_cast<PointAttributeInterpolateWidget*>(d_data->components.itemWidget(d_data->components.item(i)))->axis.currentIndex();
	}
	return res;
}

QListWidget * PointAttributeInterpolate::components()
{
	return &d_data->components;
}

void PointAttributeInterpolate::setGraphicsView(VipVTKGraphicsView * v)
{
	d_data->view = v;
	if (v)
	{
		if (VTK3DPlayer* widget = VTK3DPlayer::fromChild( v))
		{
			widget->setTrackingEnable(true);
		}
	}
}

VipVTKGraphicsView *  PointAttributeInterpolate::graphicsView() const
{
	return d_data->view;
}

void PointAttributeInterpolate::addComponent()
{
	int row = d_data->components.count();
	QObject * s = sender();
	if (s)
		row = d_data->components.row( static_cast<PointAttributeInterpolateWidget*>(s->parent())->item)+1;

	QListWidgetItem * new_item = new QListWidgetItem();
	PointAttributeInterpolateWidget * w = new PointAttributeInterpolateWidget(this, new_item);
	new_item->setSizeHint(w->sizeHint());
	d_data->components.insertItem(row, new_item);
	d_data->components.setItemWidget(new_item, w);
}

void PointAttributeInterpolate::removeComponent()
{
	if (d_data->components.count() < 2)
		return;

	int row = d_data->components.count()-1;
	QObject * s = sender();
	if(s)
		row = d_data->components.row(static_cast<PointAttributeInterpolateWidget*>(s->parent())->item) ;

	if (row >= 0)
	{
		delete d_data->components.itemWidget(d_data->components.item(row));
		delete d_data->components.takeItem(row);
	}
}

void PointAttributeInterpolate::setPoint1SelectionEnabled(bool enable)
{
	d_data->selectCoord1.blockSignals(true);
	d_data->selectCoord2.blockSignals(true);

	d_data->selectCoord1.setChecked(enable);
	d_data->selectCoord2.setChecked(false);

	d_data->selectCoord1.blockSignals(false);
	d_data->selectCoord2.blockSignals(false);
}

void PointAttributeInterpolate::setPoint2SelectionEnabled(bool enable)
{
	d_data->selectCoord1.blockSignals(true);
	d_data->selectCoord2.blockSignals(true);

	d_data->selectCoord1.setChecked(false);
	d_data->selectCoord2.setChecked(enable);

	d_data->selectCoord1.blockSignals(false);
	d_data->selectCoord2.blockSignals(false);
}


bool PointAttributeInterpolate::apply()
{
	if (!d_data->view)
	{
		Q_EMIT failed();
		return false;
	}

	PlotVipVTKObjectList lst = d_data->view->selectedObjects();
	if (lst.size())
	{
		double pt1[3], pt2[3];
		this->point1(pt1);
		this->point2(pt2);
		VipPlotVTKObject* ptr = lst.first();
		vtkDataArray * ar = ptr->rawData().setPointsAttribute(name(), components1(), pt1, components2(), pt2, interpolationAxes());
		if (!ar)
		{
			vipWarning("Error", "Unable to create point attribute.\nPlease check that you correcly fill all parameters.");
			Q_EMIT failed();
			return false;
		}
		else
		{
			for (int i = 1; i < lst.size(); ++i)
			{
				lst[i]->rawData().setPointsAttribute(name(), components1(), pt1, components2(), pt2, interpolationAxes());
			}
			Q_EMIT succeded();
			return true;
		}

	}
	Q_EMIT failed();
	return false;
}

void PointAttributeInterpolate::itemClicked(VipPlotItem* item, int button)
{
	if (!qobject_cast<VipPlotVTKObject*>(item))
		return;
	if (button != VipPlotItem::LeftButton)
		return;
	if (!qobject_cast<VipVTKGraphicsView*>(item->view()))
		return;

	VipVTKGraphicsView * view = static_cast<VipVTKGraphicsView*>(item->view());

	if (d_data->selectCoord1.isChecked())
	{
		view->setTrackingEnable(true);
		QPoint pt = view->mapFromGlobal(QCursor::pos());
		int object_id = view->contours()->ObjectId(pt);
		int point_id = view->contours()->ClosestPointId(object_id, pt);
		if (point_id >= 0)
		{
			const VipPlotVTKObject *ptr = view->contours()->HighlightedData();
			if(ptr)
				if (vtkDataSet * set = ptr->rawData().dataSet())
				{
					if (point_id < set->GetNumberOfPoints())
					{
						double coord[3];
						set->GetPoint(point_id, coord);
						d_data->coord1.setText(QString::number(coord[0]) + ", " + QString::number(coord[1]) + ", " + QString::number(coord[2]));

						d_data->selectCoord1.blockSignals(true);
						d_data->selectCoord1.setChecked(false);
						d_data->selectCoord1.blockSignals(false);
					}
				}
		}
	}
	else if (d_data->selectCoord2.isChecked())
	{
		view->setTrackingEnable(true);
		QPoint pt = view->mapFromGlobal(QCursor::pos());
		int object_id = view->contours()->ObjectId(pt);
		int point_id = view->contours()->ClosestPointId(object_id, pt);
		if (point_id >= 0)
		{
			const VipPlotVTKObject* ptr = view->contours()->HighlightedData();
			if (ptr)
				if (vtkDataSet * set = ptr->rawData().dataSet())
				{
					if (point_id < set->GetNumberOfPoints())
					{
						double coord[3];
						set->GetPoint(point_id, coord);
						d_data->coord2.setText(QString::number(coord[0]) + ", " + QString::number(coord[1]) + ", " + QString::number(coord[2]));

						d_data->selectCoord2.blockSignals(true);
						d_data->selectCoord2.setChecked(false);
						d_data->selectCoord2.blockSignals(false);
					}
				}
		}
	}
}

*/