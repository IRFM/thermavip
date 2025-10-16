#include "VipFieldOfViewEditor.h"
#include "VipVTKGraphicsView.h"
#include "VipVTKDevices.h"
#include "VipVTKPlayer.h"

#include "VipDisplayArea.h"

#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QBoxLayout>
#include <qlistwidget.h>
#include <qtoolbar.h>
#include <qsplitter.h>

static void formatGroupBox(QGroupBox * box, const QString & str, bool checkable = false)
{
	QFont f = box->font();
	f.setBold(true);
	box->setFont(f);
	box->setFlat(true);
	box->setTitle(str);
	box->setCheckable(checkable);
}

static QGroupBox * groupBox(const QString & str, bool checkable = false)
{
	QGroupBox * res = new QGroupBox(str);
	formatGroupBox(res,str, checkable);
	return res;
}




VipPoint3DEditor::VipPoint3DEditor(QWidget * parent)
:QWidget(parent)
{
	QHBoxLayout * lay = new QHBoxLayout;
	lay->addWidget(&x);
	lay->addWidget(&y);
	lay->addWidget(&z);

	x.setToolTip("X value");
	y.setToolTip("Y value");
	z.setToolTip("Z value");

	x.setValue(0);
	y.setValue(0);
	z.setValue(0);

	/*x.setValidator(new QDoubleValidator);
	y.setValidator(new QDoubleValidator);
	z.setValidator(new QDoubleValidator);*/

	lay->setContentsMargins(0,0,0,0);
	setLayout(lay);

	connect(&x, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&y, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&z, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
}

void VipPoint3DEditor::SetValue(const double * v)
{
	x.setValue((v[0]));
	y.setValue((v[1]));
	z.setValue((v[2]));
}

bool VipPoint3DEditor::GetValue(double * v) const
{
	bool ok1 = true, ok2=true, ok3=true;
	v[0] = x.value();
	v[1] = y.value();
	v[2] = z.value();
	return ok1 && ok2 && ok3;
}

double * VipPoint3DEditor::GetValue() const
{
	GetValue(const_cast<double*>(value));
	return const_cast<double*>(value);
}





VipFOVTimeEditor::VipFOVTimeEditor(QWidget * parent)
	:QWidget(parent)
{
	QHBoxLayout * lay = new QHBoxLayout();
	lay->addWidget(&nanoTimeEdit);
	lay->addWidget(&dateEdit);

	nanoTimeEdit.setToolTip("Time in nano seconds since Epoch");
	nanoTimeEdit.setText("0");
	dateEdit.setToolTip("Convert a date in nano seconds since Epoch");
	dateEdit.setDateTime(QDateTime());
	dateEdit.setDisplayFormat("dd MMM yyyy , hh:mm:ss");
	dateEdit.setCalendarPopup(true);

	connect(&dateEdit, SIGNAL(dateTimeChanged(const QDateTime &)), this, SLOT(dateEdited()));
	connect(&nanoTimeEdit, SIGNAL(textChanged(const QString&)), this, SLOT(nanoTimeEdited()));

	setLayout(lay);
}

void VipFOVTimeEditor::setTime(qint64 nano_time)
{
	nanoTimeEdit.blockSignals(true);
	dateEdit.blockSignals(true);

	nanoTimeEdit.setText(QString::number(nano_time));
	if (nano_time != VipInvalidTime)
		dateEdit.setDateTime(QDateTime::fromMSecsSinceEpoch(nano_time/1000000));
	else
		dateEdit.setDateTime(QDateTime());

	nanoTimeEdit.blockSignals(false);
	dateEdit.blockSignals(false);
}

qint64 VipFOVTimeEditor::time() const
{
	bool ok;
	qint64 t = nanoTimeEdit.text().toLongLong(&ok);
	if (ok)
		return t;
	else
		return VipInvalidTime;
}

void VipFOVTimeEditor::dateEdited()
{
	QDateTime d = dateEdit.dateTime();
	if (d != date)
	{
		date = d;
		qint64 t = d.toMSecsSinceEpoch()*1000000;
		nanoTimeEdit.setText(QString::number(t ));
	}
}

void VipFOVTimeEditor::nanoTimeEdited()
{
	qint64 t = time();
	if (t != VipInvalidTime)
	{
		dateEdit.blockSignals(true);
		dateEdit.setDateTime(QDateTime::fromMSecsSinceEpoch(t/1000000));
		dateEdit.blockSignals(false);
		Q_EMIT changed();
	}
}



VipFOVOffsetEditor::VipFOVOffsetEditor(QWidget * parent)
	:QWidget(parent)
{
	m_image.setPixmap(vipPixmap("camera_angles.png").scaled(150,100,Qt::IgnoreAspectRatio,Qt::SmoothTransformation));

	QWidget * yaw = new QWidget();
	QHBoxLayout * yawlay = new QHBoxLayout();
	yawlay->setContentsMargins(0, 0, 0, 0);
	yawlay->addWidget(&m_r_add_yaw);
	yawlay->addWidget(&m_add_yaw);
	yawlay->addWidget(&m_r_fixed_yaw);
	yawlay->addWidget(&m_fixed_yaw);
	yaw->setLayout(yawlay);

	QWidget * pitch = new QWidget();
	QHBoxLayout * pitchlay = new QHBoxLayout();
	pitchlay->setContentsMargins(0, 0, 0, 0);
	pitchlay->addWidget(&m_r_add_pitch);
	pitchlay->addWidget(&m_add_pitch);
	pitchlay->addWidget(&m_r_fixed_pitch);
	pitchlay->addWidget(&m_fixed_pitch);
	pitch->setLayout(pitchlay);

	QWidget * roll = new QWidget();
	QHBoxLayout * rolllay = new QHBoxLayout();
	rolllay->setContentsMargins(0, 0, 0, 0);
	rolllay->addWidget(&m_r_add_roll);
	rolllay->addWidget(&m_add_roll);
	rolllay->addWidget(&m_r_fixed_roll);
	rolllay->addWidget(&m_fixed_roll);
	roll->setLayout(rolllay);

	QWidget * alt = new QWidget();
	QHBoxLayout * altlay = new QHBoxLayout();
	altlay->setContentsMargins(0, 0, 0, 0);
	altlay->addWidget(&m_r_add_alt);
	altlay->addWidget(&m_add_alt);
	altlay->addWidget(&m_r_fixed_alt);
	altlay->addWidget(&m_fixed_alt);
	alt->setLayout(altlay);

	QVBoxLayout * vlay = new QVBoxLayout();
	vlay->addWidget(yaw);
	vlay->addWidget(pitch);
	vlay->addWidget(roll);
	vlay->addWidget(alt);


	QHBoxLayout * lay = new QHBoxLayout();
	lay->addWidget(&m_image);
	lay->addLayout(vlay);
	setLayout(lay);

	m_add_yaw.setValue(0);
	m_add_pitch.setValue(0);
	m_add_roll.setValue(0);
	m_add_alt.setValue(0);
	m_fixed_yaw.setValue(0);
	m_fixed_pitch.setValue(0);
	m_fixed_roll.setValue(0);
	m_fixed_alt.setValue(0);
	m_r_add_yaw.setChecked(true);
	m_r_add_pitch.setChecked(true);
	m_r_add_roll.setChecked(true);
	m_r_add_alt.setChecked(true);

	m_r_add_yaw.setText("Additional Yaw");
	m_r_add_pitch.setText("Additional Pitch");
	m_r_add_roll.setText("Additional Roll");
	m_r_add_alt.setText("Additional altitude");
	m_r_fixed_yaw.setText("Fixed Yaw");
	m_r_fixed_pitch.setText("Fixed Pitch");
	m_r_fixed_roll.setText("Fixed Roll");
	m_r_fixed_alt.setText("Fixed altitude");

	m_add_yaw.setToolTip("Angle (degree)");
	m_add_pitch.setToolTip("Angle (degree)");
	m_add_roll.setToolTip("Angle (degree)");
	m_add_alt.setToolTip("Altitude (meter)");
	m_fixed_yaw.setToolTip("Angle (degree)");
	m_fixed_pitch.setToolTip("Angle (degree)");
	m_fixed_roll.setToolTip("Angle (degree)");
	m_fixed_alt.setToolTip("Altitude (meter)");

	connect(&m_add_yaw, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_add_pitch, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_add_roll, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_add_alt, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_fixed_yaw, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_fixed_pitch, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_fixed_roll, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_fixed_alt, SIGNAL(valueChanged(double)), this, SLOT(emitChanged()));
	connect(&m_r_add_yaw, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_add_pitch, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_add_roll, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_add_alt, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_fixed_yaw, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_fixed_pitch, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_fixed_roll, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	connect(&m_r_fixed_alt, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));

}

double VipFOVOffsetEditor::additionalYaw() const
{
	return m_add_yaw.value();
}
double VipFOVOffsetEditor::additionalPitch() const
{
	return m_add_pitch.value();
}
double VipFOVOffsetEditor::additionalRoll() const
{
	return m_add_roll.value();
}
double VipFOVOffsetEditor::additionalAltitude() const
{
	return m_add_alt.value();
}
double VipFOVOffsetEditor::fixedYaw() const
{
	return m_fixed_yaw.value();
}
double VipFOVOffsetEditor::fixedPitch() const
{
	return m_fixed_pitch.value();
}
double VipFOVOffsetEditor::fixedRoll() const
{
	return m_fixed_roll.value();
}
double VipFOVOffsetEditor::fixedAltitude() const
{
	return m_fixed_alt.value();
}
void VipFOVOffsetEditor::setAdditionalYaw(double deg)
{
	m_add_yaw.setValue(deg);
}
void VipFOVOffsetEditor::setAdditionalPitch(double deg)
{
	m_add_pitch.setValue(deg);
}
void VipFOVOffsetEditor::setAdditionalRoll(double deg)
{
	m_add_roll.setValue(deg);
}
void VipFOVOffsetEditor::setAdditionalAltitude(double alt)
{
	m_add_alt.setValue(alt);
}
void VipFOVOffsetEditor::setFixedYaw(double deg)
{
	m_fixed_yaw.setValue(deg);
}
void VipFOVOffsetEditor::setFixedPitch(double deg)
{
	m_fixed_pitch.setValue(deg);
}
void VipFOVOffsetEditor::setFixedRoll(double deg)
{
	m_fixed_roll.setValue(deg);
}
void VipFOVOffsetEditor::setFixedAltitude(double alt)
{
	m_fixed_alt.setValue(alt);
}
bool VipFOVOffsetEditor::hasFixedYaw() const
{
	return m_r_fixed_yaw.isChecked();
}
bool VipFOVOffsetEditor::hasFixedPitch() const
{
	return m_r_fixed_pitch.isChecked();
}
bool VipFOVOffsetEditor::hasFixedRoll() const
{
	return m_r_fixed_roll.isChecked();
}
bool VipFOVOffsetEditor::hasFixedAltitude() const
{
	return m_r_fixed_alt.isChecked();
}
void VipFOVOffsetEditor::setUseFixedYaw(bool enable)
{
	m_r_fixed_yaw.setChecked(enable);
}
void VipFOVOffsetEditor::setUseFixedPitch(bool enable)
{
	m_r_fixed_pitch.setChecked(enable);
}
void VipFOVOffsetEditor::setUseFixedRoll(bool enable)
{
	m_r_fixed_roll.setChecked(enable);
}
void VipFOVOffsetEditor::setUseFixedAltitude(bool enable)
{
	m_r_fixed_alt.setChecked(enable);
}




VipFOVEditor::VipFOVEditor(QWidget * parent )
:QWidget(parent), lastSender(nullptr)
{
	{
		QGridLayout * lay = new QGridLayout;
		int row = 0;

		lay->addWidget(new QLabel("Name: "), row, 0);
		lay->addWidget(&name, row++, 1);

		lay->addWidget(new QLabel("Pupil position: "), row, 0);
		lay->addWidget(&pupilPos, row++, 1);

		lay->addWidget(new QLabel("Target position: "), row, 0);
		lay->addWidget(&targetPoint, row++, 1);

		lay->addWidget(new QLabel("Horizontal field of view: "), row, 0);
		lay->addWidget(&horizontalFov, row++, 1);

		lay->addWidget(new QLabel("Vertical field of view: "), row, 0);
		lay->addWidget(&verticalFov, row++, 1);

		lay->addWidget(new QLabel("View up: "), row, 0);
		lay->addWidget(&viewUp, row++, 1);

		lay->addWidget(new QLabel("Rotation: "), row, 0);
		lay->addWidget(&rotation, row++, 1);

		lay->addWidget(new QLabel("Focal: "), row, 0);
		lay->addWidget(&focal, row++, 1);

		lay->addWidget(new QLabel("Additional zoom: "), row, 0);
		lay->addWidget(&zoom, row++, 1);

		lay->addWidget(new QLabel("Matrix width: "), row, 0);
		lay->addWidget(&pixWidth, row++, 1);

		lay->addWidget(new QLabel("Matrix height: "), row, 0);
		lay->addWidget(&pixHeight, row++, 1);

		lay->addWidget(new QLabel("Crop X: "), row, 0);
		lay->addWidget(&cropX, row++, 1);

		lay->addWidget(new QLabel("Crop Y: "), row, 0);
		lay->addWidget(&cropY, row++, 1);

		

		lay->addWidget(VipLineWidget::createSunkenHLine(), row++, 0, 1, 2);
		lay->addWidget(new QLabel("Camera time: "), row, 0);
		lay->addWidget(&time, row++, 1);

		stdOptions.setLayout(lay);
	}

	{
		//Currently, disable optical distortion
		QGridLayout * lay = new QGridLayout;
		int row = 0;

		lay->addWidget(new QLabel("K2: "), row, 0);
		lay->addWidget(&K2, row++, 1);
		lay->addWidget(new QLabel("K4: "), row, 0);
		lay->addWidget(&K4, row++, 1);
		lay->addWidget(new QLabel("K6: "), row, 0);
		lay->addWidget(&K6, row++, 1);
		lay->addWidget(new QLabel("P1: "), row, 0);
		lay->addWidget(&P1, row++, 1);
		lay->addWidget(new QLabel("P2: "), row, 0);
		lay->addWidget(&P2, row++, 1);
		lay->addWidget(new QLabel("AlphaC"), row, 0);
		lay->addWidget(&AlphaC, row++, 1);

		opticalDistortions.setLayout(lay);
		opticalDistortions.hide();
	}


	formatGroupBox(&showStdOptions,"Standard parameters",true);
	formatGroupBox(&showOpticalDistortions, "Optical distortion parameters", true);
	opticalDistortions.hide();
	
	connect(&showStdOptions, SIGNAL(clicked(bool)), &stdOptions, SLOT(setVisible(bool)));
	connect(&showOpticalDistortions, SIGNAL(clicked(bool)), &opticalDistortions, SLOT(setVisible(bool)));
	connect(&showStdOptions, SIGNAL(clicked(bool)), this, SLOT(EmitSizeChanged()),Qt::QueuedConnection);
	connect(&showOpticalDistortions, SIGNAL(clicked(bool)), this, SLOT(EmitSizeChanged()), Qt::QueuedConnection);

	QVBoxLayout *lay = new QVBoxLayout();
	lay->setSpacing(5);
	lay->addWidget(&showStdOptions);
	lay->addWidget(&stdOptions);
	lay->addWidget(&showOpticalDistortions);
	lay->addWidget(&opticalDistortions);
	lay->addStretch(1);
	setLayout(lay);

	showStdOptions.setChecked(true);
	showOpticalDistortions.setChecked(false);

	name.setPlaceholderText("camera name");
	pupilPos.setToolTip("Coordinates of the position of the pupil");
	targetPoint.setToolTip("Coordinates of the target point");
	verticalFov.setToolTip("Vertical field of view (degree)");
	horizontalFov.setToolTip("Horizontal field of view (degree)");
	rotation.setToolTip("Rotation of the camera (degree)");
	viewUp.setToolTip("Camera axis view up (X,Y or Z)");
	focal.setToolTip("Focal length of the camera");
	zoom.setToolTip("Zoom parameter to create the exact texture");
	pixWidth.setToolTip("Matrix width (pixels)");
	pixHeight.setToolTip("Matrix height (pixels)");
	cropX.setToolTip("Horizontal coordinate of the top left corner of the cropped picture in the entire picture");
	cropY.setToolTip("Vertical coordinate of the top left corner of the cropped picture in the entire picture");

	viewUp.addItems(QStringList()<<"X"<<"Y"<<"Z");
	pixWidth.setRange(0,10000);
	pixHeight.setRange(0,10000);
	cropX.setRange(-10000,10000);
	cropY.setRange(-10000,10000);

	verticalFov.setText("0");
	horizontalFov.setText("0");
	rotation.setText("0");
	viewUp.setCurrentIndex(2);
	focal.setText("0");
	zoom.setText("1");
	K2.setText("0");
	K4.setText("0");
	K6.setText("0");
	P1.setText("0");
	P2.setText("0");
	AlphaC.setText("0");
		
	connect(&name, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&pupilPos, SIGNAL(changed()), this, SLOT(EmitChanged()));
	connect(&targetPoint, SIGNAL(changed()), this, SLOT(EmitChanged()));
	connect(&verticalFov, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&horizontalFov, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&rotation, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&viewUp, SIGNAL(currentIndexChanged(int)), this, SLOT(EmitChanged()));
	connect(&focal, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&zoom, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&pixWidth, SIGNAL(valueChanged(int)), this, SLOT(EmitChanged()));
	connect(&pixHeight, SIGNAL(valueChanged(int)), this, SLOT(EmitChanged()));
	connect(&cropX, SIGNAL(valueChanged(int)), this, SLOT(EmitChanged()));
	connect(&cropY, SIGNAL(valueChanged(int)), this, SLOT(EmitChanged()));
	connect(&time, SIGNAL(changed()), this, SLOT(EmitChanged()));
	connect(&K2, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&K4, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&K6, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&P1, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&P2, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
	connect(&AlphaC, SIGNAL(textChanged(const QString&)), this, SLOT(EmitChanged()));
}

QGroupBox* VipFOVEditor::AddSection(const QString & section_name, QWidget * section)
{
	QGroupBox * box = new QGroupBox();
	formatGroupBox(box, section_name, true);

	QVBoxLayout * lay = static_cast<QVBoxLayout*>(layout());
	lay->insertWidget(lay->count()-1, box);
	lay->insertWidget(lay->count() - 1, section);

	connect(box, SIGNAL(clicked(bool)), section, SLOT(setVisible(bool)));
	connect(box, SIGNAL(clicked(bool)), this, SLOT(EmitSizeChanged()), Qt::QueuedConnection);
	box->setChecked(true);
	return box;
}

void VipFOVEditor::setFieldOfView(const VipFieldOfView & fov)
{
	name.setText(fov.name);
	pupilPos.SetValue(fov.pupil);
	targetPoint.SetValue(fov.target);
	verticalFov.setText(QString::number(fov.vertical_angle));
	horizontalFov.setText(QString::number(fov.horizontal_angle));
	rotation.setText(QString::number(fov.rotation));
	viewUp.setCurrentIndex(fov.view_up);
	focal.setText(QString::number(fov.focal));
	zoom.setText(QString::number(fov.zoom));
	pixWidth.setValue(fov.width);
	pixHeight.setValue(fov.height);
	cropX.setValue(fov.crop_x);
	cropY.setValue(fov.crop_y);
	time.setTime(fov.time );
	K2.setText(QString::number(fov.K2));
	K4.setText(QString::number(fov.K4));
	K6.setText(QString::number(fov.K6));
	P1.setText(QString::number(fov.P1));
	P2.setText(QString::number(fov.P2));
	AlphaC.setText(QString::number(fov.AlphaC));
	FOV = fov;
}

VipFieldOfView VipFOVEditor::fieldOfView() const
{
	VipFieldOfView fov;
	fov.name = name.text();
	memcpy(fov.pupil, pupilPos.GetValue(), sizeof(fov.pupil));
	memcpy(fov.target, targetPoint.GetValue(), sizeof(fov.target));
	fov.horizontal_angle = horizontalFov.text().toDouble();
	fov.vertical_angle = verticalFov.text().toDouble();
	fov.rotation = rotation.text().toDouble();
	fov.width = pixWidth.value();
	fov.height = pixHeight.value();
	fov.crop_x = cropX.value();
	fov.crop_y = cropY.value();
	fov.zoom = zoom.text().toDouble();
	fov.view_up = viewUp.currentIndex();
	fov.K2 = K2.text().toDouble();
	fov.K4 = K4.text().toDouble();
	fov.K6 = K6.text().toDouble();
	fov.P1 = P1.text().toDouble();
	fov.P2 = P2.text().toDouble();
	fov.AlphaC = AlphaC.text().toDouble();
	fov.time = time.time() == VipInvalidTime ? VipInvalidTime : time.time();
	fov.attributes = FOV.attributes;
	return fov;

}








struct FOVListWidget : public QListWidget
{
	VipFOVSequenceEditor * editor;

	virtual void dropEvent(QDropEvent * evt)
	{
		QListWidget::dropEvent(evt);
		editor->CheckValidity();
	}

	virtual void keyPressEvent(QKeyEvent * evt)
	{
		if (evt->key() == Qt::Key_Delete)
		{
			editor->RemoveSelectedFOVs();
		}
		else if (evt->key() == Qt::Key_A && (evt->modifiers() & Qt::CTRL))
		{
			//select all
			for (int i = 0; i < count(); ++i)
				item(i)->setSelected(true);
		}
	}
};

static qint64 year2000 = QDateTime::fromString("2000", "yyyy").toMSecsSinceEpoch();

struct FOVListItem : public QListWidgetItem
{
	VipFieldOfView fov;
	void SetFov(const VipFieldOfView & f)
	{
		fov = f;
		//display a comprehensive text
		if( f.time > year2000)
		{
			setText(QDateTime::fromMSecsSinceEpoch(f.time/1000000).toString("hh:mm:ss.zzz"));
		}
		else
		{
			setText(QString::number(f.time / 1000000000.0) + " s");
		}
		
		//only set the attributes as tool tip
		QVariantMap attrs = f.attributes;
		if (attrs.size())
		{
			QString tool_tip;
			int count = 0;
			for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it, ++count)
			{
				if (count >= 30)
				{
					tool_tip += "...<br>";
					break;
				}
				tool_tip += "<b>" + it.key() + "</b> : " + it.value().toString() + "<br>";
			}
			setToolTip(tool_tip);
		}
		
	}
};

class VipFOVSequenceEditor::PrivateData
{
public:
	FOVListWidget * times;
	QWidget *sequenceOptions;
	VipFOVEditor *editor;
	
	QToolBar *controls;
	QSpinBox *samples;
	QCheckBox *interpolateFOV;
	QPushButton *apply;
	QPushButton *ok;
	QPushButton *cancel;

	VipFieldOfViewList fovs;
	VipFieldOfView template_fov;

	QPointer<VipVTKGraphicsView> view;
	QPointer<VipFOVSequence> sequence;
	QPointer<VipFOVItem> item;
};

VipFOVSequenceEditor::VipFOVSequenceEditor(VipVTKGraphicsView * view, QWidget * parent)
	:QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA();
	d_data->view = view;
	d_data->controls = new QToolBar();
	d_data->times = new FOVListWidget();
	d_data->editor = new VipFOVEditor();
	d_data->samples = new QSpinBox();
	d_data->apply = new QPushButton();
	d_data->ok = new QPushButton();
	d_data->cancel = new QPushButton();
	d_data->interpolateFOV = new QCheckBox();
	d_data->sequenceOptions = new QWidget();

	d_data->times->editor = this;
	d_data->times->setMaximumWidth(150);
	
	
	connect(d_data->controls->addAction(vipIcon("add_page.png"), "Add new field of view"), SIGNAL(triggered(bool)), this, SLOT(AddCurrentFOV()));
	connect(d_data->controls->addAction(vipIcon("reset.png"), "remove selected fields of view"), SIGNAL(triggered(bool)), this, SLOT(RemoveSelectedFOVs()));
	d_data->controls->addSeparator();
	connect(d_data->controls->addAction(vipIcon("open_fov.png"), "Apply selected field of view"), SIGNAL(triggered(bool)), this, SLOT(ApplyCurrentFOV()));
	connect(d_data->controls->addAction(vipIcon("apply.png"), "Apply current camera to selected field of view"), SIGNAL(triggered(bool)), this, SLOT(ChangeCurrentFOV()));

	d_data->times->setDragDropMode(QAbstractItemView::InternalMove);
	d_data->times->setSelectionMode(QAbstractItemView::ExtendedSelection);
	d_data->times->setDragDropOverwriteMode(false);
	d_data->times->setDefaultDropAction(Qt::TargetMoveAction);
	d_data->times->setToolTip("Field Of View list");

	d_data->samples->setRange(0, 1000000);
	d_data->samples->setValue(0);
	d_data->samples->setToolTip("Total number of Fields Of View");

	//d_data->apply.setAutoRaise(true);
	d_data->apply->setText("Apply");
	d_data->apply->setToolTip("Apply the changes");
	d_data->ok->setText("Ok");
	d_data->cancel->setText("Cancel");
	

	d_data->interpolateFOV->setText("Enable FOV interpolation");
	d_data->interpolateFOV->setToolTip("If checked, the sample count will be used as the total number of FOV\n"
		"and the FOV for an intermediate time will be computed by interpoling the 2 closest FOV.\n"
		"Otherwise, the sample count will be set to the exact number of FOV defined.");

	connect(d_data->times, SIGNAL(itemDoubleClicked(QListWidgetItem * )), this, SLOT(ApplyCurrentFOV()));
	connect(d_data->times, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
	connect(d_data->interpolateFOV, SIGNAL(clicked(bool)), this, SLOT(EnabledInterpolation(bool)));
	connect(d_data->editor, SIGNAL(changed()), this, SLOT(EditorChanged()));
	connect(d_data->editor, SIGNAL(SizeChanged()), this, SLOT(EmitSizeChanged()));
	
	connect(d_data->apply, SIGNAL(clicked(bool)), this, SLOT(Apply()));
	connect(d_data->ok,SIGNAL(clicked(bool)), this, SLOT(Apply()));
	connect(d_data->ok, SIGNAL(clicked(bool)), this, SLOT(EmitAccepted()));
	connect(d_data->cancel, SIGNAL(clicked(bool)), this, SLOT(EmitRejected()));

	QGridLayout * glay = new QGridLayout();
	glay->addWidget(d_data->controls,0,0,1,2);
	glay->addWidget(d_data->times, 1, 0, 1, 2);
	glay->addWidget(d_data->interpolateFOV, 2, 0, 1, 2);
	glay->addWidget(new QLabel("Sample count"), 3, 0);
	glay->addWidget(d_data->samples, 3, 1);
	d_data->sequenceOptions->setLayout(glay);

	QSplitter * splitter = new QSplitter(Qt::Horizontal);
	splitter->addWidget(d_data->sequenceOptions);
	d_data->sequenceOptions->hide();

	QWidget * editor = new QWidget();
	QVBoxLayout * vlay2 = new QVBoxLayout();
	vlay2->setContentsMargins(0, 0, 0, 0);
	vlay2->addWidget(d_data->editor);
	editor->setLayout(vlay2);

	splitter->addWidget(editor);
	
	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(splitter);

	QHBoxLayout * buttons_lay = new QHBoxLayout();
	buttons_lay->addStretch(1);
	buttons_lay->addWidget(d_data->apply);
	buttons_lay->addSpacing(50);
	buttons_lay->addWidget(d_data->ok);
	buttons_lay->addWidget(d_data->cancel);
	lay->addLayout(buttons_lay);
	
	setLayout(lay);
}

VipFOVSequenceEditor::~VipFOVSequenceEditor() = default;

VipFOVEditor * VipFOVSequenceEditor::Editor() const
{
	return d_data->editor;
}

void VipFOVSequenceEditor::SetGraphicsView(VipVTKGraphicsView * view)
{
	d_data->view = view;
}

VipVTKGraphicsView * VipFOVSequenceEditor::GetGraphicsView() const
{
	return d_data->view;
}

void VipFOVSequenceEditor::SetFOVItem(VipFOVItem * item)
{
	d_data->item = item;
	if (!d_data->sequence && item)
		d_data->editor->setFieldOfView(item->plotFov()->rawData());
}

VipFOVItem * VipFOVSequenceEditor::GetFOVItem() const
{
	return d_data->item;
}

void VipFOVSequenceEditor::SetSequence(VipFOVSequence * seq)
{
	d_data->sequence = seq;
	if (seq && seq->isOpen())
	{
		if (seq->count())
		{
			d_data->times->blockSignals(true);

			qint64 time = seq->time();
			qint64 dist = VipInvalidTime;
			FOVListItem * select = nullptr;

			d_data->times->clear();
			for (int i = 0; i < seq->count(); ++i)
			{
				FOVListItem * item = new FOVListItem();
				item->SetFov(seq->at(i));
				d_data->times->addItem(item);

				//find the closest FOV and select it
				if (i == 0 || qAbs(seq->at(i).time - time) < dist)
				{
					dist = qAbs(seq->at(i).time - time);
					select = item;
				}
			}

			d_data->times->blockSignals(false);

			//select the right FOV
			//d_data->times->setItemSelected(select, true);
			select->setSelected(true);
			d_data->editor->blockSignals(true);
			d_data->editor->setFieldOfView(select->fov);
			d_data->editor->blockSignals(false);

			//set the sampling count
			d_data->samples->setValue(seq->size());

			//set the template fov
			d_data->template_fov = seq->at(0);

			//set interpolation
			EnabledInterpolation(seq->count() != seq->size());
		}

		CheckValidity();
	}

	d_data->sequenceOptions->setVisible(seq);// && seq->count() > 1);
	EmitSizeChanged();
}

VipFOVSequence* VipFOVSequenceEditor::GetSequence() const
{
	return d_data->sequence;
}

void VipFOVSequenceEditor::Apply()
{
	//apply the changes to the current VipFOVSequence
	VipFieldOfViewList fovs = GetFOVs();

	if (d_data->sequence && fovs.size())
	{
		//update the FOVs in VipFOVSequence
		d_data->sequence->clear();

		// find the name
		QString name = fovs.first().name;
		if (name.isEmpty())
			name = d_data->sequence->fovName();

		if (d_data->interpolateFOV->isChecked()) {
			// Interpolate
			VipFOVSequence seq;
			seq.setFieldOfViews(fovs);
			seq.open(VipIODevice::ReadOnly);
			seq.setTimeWindows(VipTimeRange(fovs.first().time, fovs.last().time), d_data->samples->value());
			qint64 size = seq.size();
			VipFieldOfViewList fovs2;
			for (qint64 i = 0; i < size; ++i) {
				auto fov = seq.fovAtTime(seq.posToTime(i));
				fovs2.push_back(fov);
			}
			fovs = fovs2;
		}

		d_data->sequence->setFieldOfViews(fovs);
		d_data->sequence->setFovName(name);

		/* if (d_data->interpolateFOV->isChecked()) {
			if (d_data->samples->value() > fovs.size()) {
				d_data->sequence->setTimeWindows(VipTimeRange(fovs.first().time, fovs.last().time), d_data->samples->value());
			}
		}*/

		if (d_data->item)
			// Recompute the camera path
			d_data->item->setPlotFov(d_data->item->plotFov());

		//reload the processing pool
		if (d_data->view)
			if (VipVTKPlayer* tokida = VipVTKPlayer::fromChild(d_data->view))
				if (VipProcessingPool * pool = tokida->processingPool())
					pool->reload();

	}
	
}

VipFieldOfView VipFOVSequenceEditor::SelectedFOV() const
{
	QList<QListWidgetItem*> selection = d_data->times->selectedItems();
	if (selection.size())
	{
		return static_cast<FOVListItem*>(selection.last())->fov;
	}
	else
		return d_data->editor->fieldOfView();
}

void VipFOVSequenceEditor::ChangeCurrentFOV()
{
	QList<QListWidgetItem*> selection = d_data->times->selectedItems();
	if (selection.size() && d_data->view)
	{
		for (int i = 0; i < selection.size(); ++i)
		{
			FOVListItem * item = static_cast<FOVListItem*>(selection.last());
			VipFieldOfView fov = static_cast<FOVListItem*>(selection.last())->fov;
			item->fov.importCamera(d_data->view->renderer()->GetActiveCamera());
		}

		d_data->editor->setFieldOfView(static_cast<FOVListItem*>(selection.last())->fov);
	}
}

void VipFOVSequenceEditor::ApplyCurrentFOV()
{
	QList<QListWidgetItem*> selection = d_data->times->selectedItems();
	if (selection.size() && d_data->view)
	{
		VipFieldOfView fov = static_cast<FOVListItem*>(selection.last())->fov;
		fov.changePointOfView(d_data->view->widget()->renderWindow());
	}
}

void VipFOVSequenceEditor::addFieldOfView(const VipFieldOfView & fov)
{
	FOVListItem * item = new FOVListItem();
	item->SetFov(fov);

	d_data->times->addItem(item);
	d_data->times->setCurrentItem(item);

	d_data->samples->setValue(qMax(d_data->samples->value(), d_data->times->count()));
}

VipFieldOfViewList VipFOVSequenceEditor::GetFOVs() const
{
	//return the full list of VipFieldOfView.
	//if the list is not valid (if times are nor ordered), return an empty list.

	qint64 time = VipInvalidTime;

	VipFieldOfViewList res;
	for (int i = 0; i < d_data->times->count(); ++i)
	{
		FOVListItem * item = static_cast<FOVListItem*>(d_data->times->item(i));
		if (i == 0)
			time = item->fov.time;
		else if (item->fov.time <= time)
			return VipFieldOfViewList();

		res.append(item->fov);
	}
	return res;
}

void VipFOVSequenceEditor::AddCurrentFOV()
{
	VipFieldOfView fov = d_data->template_fov;
	if (d_data->view)
	{
		fov.importCamera(d_data->view->renderer()->GetActiveCamera());
	}

	//set the current time
	if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		fov.time = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->time();
	//set the time to the last fov time +1
	if (fov.time == VipInvalidTime)
		fov.time = static_cast<FOVListItem*>(d_data->times->item(d_data->times->count() - 1))->fov.time + 1000;

	addFieldOfView(fov);
}

void VipFOVSequenceEditor::RemoveSelectedFOVs()
{
	QList<QListWidgetItem*> selection = d_data->times->selectedItems();
	if (selection.size() < d_data->times->count())
	{
		d_data->times->blockSignals(true);
		for (int i = 0; i < selection.size(); ++i)
			delete selection[i];
		d_data->times->blockSignals(false);

		d_data->times->item(0)->setSelected(true);
		//d_data->times->setItemSelected(d_data->times->item(0), true);
	}

	d_data->samples->setValue(qMax(d_data->samples->value(), d_data->times->count()));

	CheckValidity();
}

void VipFOVSequenceEditor::selectionChanged()
{
	QList<QListWidgetItem*> selection = d_data->times->selectedItems();
	if (selection.size())
	{
		d_data->editor->blockSignals(true);
		d_data->editor->setFieldOfView(static_cast<FOVListItem*>(selection.last())->fov);
		d_data->editor->blockSignals(false);

	}
}

void VipFOVSequenceEditor::EditorChanged()
{
	QList<QListWidgetItem*> selection = d_data->times->selectedItems();
	if (selection.size())
	{
		FOVListItem * item = static_cast<FOVListItem*>(selection.last());
		QString old_name = item->fov.name;
		item->SetFov(d_data->editor->fieldOfView());
		QString new_name = item->fov.name;

		//apply the changes to all selected FOV
		for (int i = 0; i < selection.size() - 1; ++i)
		{
			FOVListItem * it = static_cast<FOVListItem*>(selection[i]);

			if (d_data->editor->lastSender == &d_data->editor->pupilPos)
				d_data->editor->pupilPos.GetValue(it->fov.pupil);
			else if (d_data->editor->lastSender == &d_data->editor->targetPoint)
				d_data->editor->targetPoint.GetValue(it->fov.target);
			else if (d_data->editor->lastSender == &d_data->editor->verticalFov)
				it->fov.vertical_angle = d_data->editor->verticalFov.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->horizontalFov)
				it->fov.horizontal_angle = d_data->editor->horizontalFov.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->rotation)
				it->fov.rotation = d_data->editor->rotation.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->viewUp)
				it->fov.view_up = d_data->editor->viewUp.currentIndex();
			else if (d_data->editor->lastSender == &d_data->editor->focal)
				it->fov.focal = d_data->editor->focal.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->zoom)
				it->fov.zoom = d_data->editor->zoom.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->pixWidth)
				it->fov.width = d_data->editor->pixWidth.value();
			else if (d_data->editor->lastSender == &d_data->editor->pixHeight)
				it->fov.height = d_data->editor->pixHeight.value();
			else if (d_data->editor->lastSender == &d_data->editor->cropX)
				it->fov.crop_x = d_data->editor->cropX.value();
			else if (d_data->editor->lastSender == &d_data->editor->cropY)
				it->fov.crop_y = d_data->editor->cropY.value();
			else if (d_data->editor->lastSender == &d_data->editor->K2)
				it->fov.K2 = d_data->editor->K2.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->K4)
				it->fov.K4 = d_data->editor->K4.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->K6)
				it->fov.K6 = d_data->editor->K6.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->P1)
				it->fov.P1 = d_data->editor->P1.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->K2)
				it->fov.P2 = d_data->editor->P2.text().toDouble();
			else if (d_data->editor->lastSender == &d_data->editor->AlphaC)
				it->fov.AlphaC = d_data->editor->AlphaC.text().toDouble();
		}


		//if the name changed, apply the change to all other FOV
		if (new_name != old_name)
		{
			for (int i = 0; i < d_data->times->count(); ++i)
			{
				static_cast<FOVListItem*>(d_data->times->item(i))->fov.name = new_name;
			}
			d_data->template_fov.name = new_name;
		}

		CheckValidity();
	}
}

void VipFOVSequenceEditor::EnabledInterpolation(bool enable)
{
	d_data->interpolateFOV->blockSignals(true);
	d_data->interpolateFOV->setChecked(enable);
	d_data->interpolateFOV->blockSignals(false);
	d_data->samples->setEnabled(enable);
}

bool VipFOVSequenceEditor::CheckValidity()
{
	qint64 time = VipInvalidTime;
	for (int i = 0; i < d_data->times->count(); ++i)
	{
		FOVListItem * item = static_cast<FOVListItem*>(d_data->times->item(i));
		if (i == 0)
			time = item->fov.time;
		else if (item->fov.time <= time)
		{
			d_data->times->setStyleSheet("border: 1px solid red;");
			return false;
		}
	}

	d_data->times->setStyleSheet("");
	return true;
}





VipFOVSequenceEditorTool::VipFOVSequenceEditorTool(VipMainWindow * window)
	:VipToolWidget(window)
{
	m_editor = new VipFOVSequenceEditor();
	m_editor->setMaximumWidth(800);
	this->setWidget(m_editor);
	this->setWindowTitle("Field Of View editor");
	this->setObjectName("Field Of View editor");

	connect(m_editor, SIGNAL(Accepted()), this, SLOT(hide()));
	connect(m_editor, SIGNAL(Rejected()), this, SLOT(hide()));
	connect(m_editor, SIGNAL(SizeChanged()), this, SLOT(resetSize()));
}

VipFOVSequenceEditor * VipFOVSequenceEditorTool::editor() const
{
	return m_editor;
}


VipFOVSequenceEditorTool * vipGetFOVSequenceEditorTool(VipMainWindow * win)
{
	static VipFOVSequenceEditorTool* tool = new VipFOVSequenceEditorTool(win);
	return tool;
}