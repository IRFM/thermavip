#include "PyGenerator.h"
#include "PyEditor.h"
#include "PyProcessing.h"

#include "VipLogging.h"
#include "VipPlayer.h"

void PySignalGenerator::ReadThread::run()
{
	qint64 start = vipGetMilliSecondsSinceEpoch();

	while (PySignalGenerator * gen = generator)
	{
		qint64 time = vipGetMilliSecondsSinceEpoch() - start;

		qint64 st = QDateTime::currentMSecsSinceEpoch();
		if (!gen->readData(time * 1000000))
			break;
		qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
		int sleep = gen->propertyAt(0)->value<int>()/1000000 - el;
		if (sleep > 0)
			msleep(sleep); 
	}

	generator = nullptr;
}

void PySignalGenerator::close()
{
	VipIODevice::close();
	setStreamingEnabled(false);
	m_data = QVariant();
}

PySignalGenerator::DeviceType PySignalGenerator::deviceType() const
{
	if (m_data.userType() != 0)
		return Resource;

	qint64 start = propertyAt(1)->value<qint64>();
	qint64 end = propertyAt(2)->value<qint64>();
	if (start == VipInvalidTime || end == VipInvalidTime)
		return Sequential;
	else
		return VipTimeRangeBasedGenerator::deviceType();
}

QVariant PySignalGenerator::computeValue(qint64 time, bool & ok)
{
	
	GetPyOptions()->setObject("t", QVariant(time / 1000000000.0));

	QVariant value;
	if (!propertyAt(4)->value<bool>())
		value = GetPyOptions()->evalCode(m_code);
	else
		value = GetPyOptions()->wait(GetPyOptions()->execCode(m_code.pycode),4000);

	if (propertyAt(4)->value<bool>() && value.userType() != qMetaTypeId<PyError>())
	{
		//complex expression
		value = GetPyOptions()->getObject("value");
	}

	if (value.userType() == qMetaTypeId<PyError>())
	{
		setError(value.value<PyError>().traceback);
		ok = false;
		return QVariant();
	}

	ok = true;
	return value;
}

bool PySignalGenerator::open(VipIODevice::OpenModes mode)
{
	VipIODevice::close();

	if (!(mode & VipIODevice::ReadOnly))
		return false;

	qint64 sampling = propertyAt(0)->value<qint64>();
	qint64 start = propertyAt(1)->value<qint64>();
	qint64 end = propertyAt(2)->value<qint64>();
	QString code = propertyAt(3)->value<QString>();

	if (code.isEmpty())
		return false;
	if (deviceType() == Temporal && (end - start) <= 0)
		return false;
	if (sampling <= 0)
		return false;

	if(propertyAt(4)->value<bool>())
		m_code = CodeObject(code, CodeObject::File);
	else
		m_code = CodeObject(code, CodeObject::Eval);

	if (m_code.isNull())
	{
		PyError err;
		if (err.isNull())
			err.traceback = "Unable to exec code object";
		setError(err.traceback);
		return false;
	}

	PyProcessingLocker locker;

	//temporal device, generate the timestamps
	if (deviceType() == Temporal)
	{
		this->setTimeWindows(start, (end-start) / sampling + 1, sampling);

		//evaluate the first value. If it is a double, generate the full curve, and reset the time window with a size of 1
		GetPyOptions()->setObject("st", QVariant(start / 1000000000.0));

		bool ok = false;
		QVariant value = computeValue(start, ok);
		if (!ok)
			return false;

		ok = false;
		value.toDouble(&ok);
		if (ok)
		{
			//generate the curve
			VipPointVector vector;
			for (qint64 time = start; time <= end; time += sampling)
			{
				bool ok = false;
				QVariant value = computeValue(time, ok);
				if(!ok)
					return false;
				
				vector.append(QPointF(time, value.toDouble()));
			}
			m_data = QVariant::fromValue(vector);
			if (!readData(0))
				return false;
		}
		else {
			//generate a video device
			//this->setTimeWindows(start, end, sampling);
			if (!readData(start))
				return false;
		}
	}
	else
	{
		GetPyOptions()->setObject("st", QVariant(0.0));
		if (!readData(0))
			return false;
	}

	QStringList lst = code.split("\n", QString::SkipEmptyParts);
	if (lst.size() == 1)
		setAttribute("Name", lst[0]);
	else
		setAttribute("Name","Python expression");
	
	setOpenMode(mode);
	return true;
}

bool PySignalGenerator::enableStreaming(bool enable)
{
	if (deviceType() != Sequential)
	{
		m_thread->generator = nullptr;
		m_thread->wait();
		return false;
	}

	if (enable)
	{
		m_thread->generator = this;
		m_thread->start();
	}
	else
	{
		m_thread->generator = nullptr;
		m_thread->wait();
	}

	return true;
}

bool PySignalGenerator::readData(qint64 time)
{
	PyProcessingLocker locker;

	//Resource
	if (m_data.userType() != 0)
	{
		VipAnyData any = create(m_data);
		any.setAttribute("Name", propertyAt(3)->value<QString>());
		any.setXUnit("Time");
		any.setYUnit(propertyAt(5)->value<QString>());
		any.setZUnit(propertyAt(5)->value<QString>());
		outputAt(0)->setData(any);
	}
	//temporal or sequential
	else 
	{
		bool ok = false;
		QVariant value = computeValue(time,ok);
		if (value.userType() == qMetaTypeId<PyError>() || !ok)
			return false;
		

		VipAnyData any = create(value);
		any.setTime(time);
		any.setAttribute("Name", propertyAt(3)->value<QString>());
		any.setXUnit("Time");
		any.setYUnit(propertyAt(5)->value<QString>());
		any.setZUnit(propertyAt(5)->value<QString>());
		outputAt(0)->setData(any);
	}
	return true;
}





#include "VipStandardWidgets.h"
#include "VipProcessingObjectEditor.h"
#include "VipDisplayArea.h"
#include <qradiobutton.h>
#include <qboxlayout.h>

class PySignalGeneratorEditor::PrivateData 
{
public:
	QLineEdit code;
	PyEditor editor;
	QComboBox unit;
	VipDoubleEdit sampling;

	QRadioButton sequential, temporal;

	QCheckBox usePoolTimeRange;
	VipDoubleEdit start, end;

	//gather in widgets
	QWidget * samplingWidget;
	QWidget * rangeWidget;

	QPointer<PySignalGenerator> generator;
};

PySignalGeneratorEditor::PySignalGeneratorEditor(QWidget * parent)
	:QWidget(parent)
{
	m_data = new PrivateData();

	QHBoxLayout * slay = new QHBoxLayout();
	slay->setContentsMargins(0, 0, 0, 0);
	slay->addWidget(new QLabel("Sampling"));
	slay->addWidget(&m_data->sampling);
	m_data->samplingWidget = new QWidget();
	m_data->samplingWidget->setLayout(slay);

	QHBoxLayout * rlay = new QHBoxLayout();
	rlay->setContentsMargins(0, 0, 0, 0);
	rlay->addWidget(new QLabel("Start"));
	rlay->addWidget(&m_data->start);
	rlay->addWidget(new QLabel("End"));
	rlay->addWidget(&m_data->end);
	m_data->rangeWidget = new QWidget();
	m_data->rangeWidget->setLayout(rlay);

	QVBoxLayout * lay = new QVBoxLayout();
	lay->addWidget(&m_data->code);
	lay->addWidget(&m_data->editor,10);
	lay->addWidget(&m_data->unit);
	lay->addWidget(m_data->samplingWidget);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(&m_data->sequential);
	lay->addWidget(&m_data->temporal);
	lay->addWidget(&m_data->usePoolTimeRange);
	lay->addWidget(m_data->rangeWidget);
	lay->addStretch(1);
	lay->addWidget(VipLineWidget::createHLine());
	lay->setContentsMargins(0, 0, 0, 0);
	setLayout(lay);

	m_data->code.setPlaceholderText("Python expression");
	m_data->editor.SetDefaultCode("value = (t - st) * 10");
	if (!m_data->editor.CurrentEditor())
		m_data->editor.NewFile();

	m_data->code.setToolTip(tr(
			"Python expression that can be evaluated to numerical value or a numpy array.<br><br>"
			"Example:<br>"
			"    <b>2*cos((t-st)/10)</b><br>"
			"<i>t</i> represents the device time in seconds.<br>"
			"<i>st</i> represents the device starting time in seconds."
		));
	m_data->editor.setToolTip(tr(
			"Python script with a <i>value</i> variable that can be evaluated to numerical value or a numpy array.<br><br>"
			"Example:<br>"
			"    <b>value = 2*cos((t-st)/10)</b><br>"
			"<i>t</i> represents the device time in seconds.<br>"
			"<i>st</i> represents the device starting time in seconds.<br>"
			"<i>value</i> represents the output value (numerical value or numpy array).<br>"
		));
	
	m_data->sampling.setToolTip(tr(
		"Device sampling time in seconds"));
		/*"For sequential device, the sampling time (in seconds) is used to determine the elapsed time between 2 generated values.<br>"
		"For temporal device, this is the inverse of the device frequency."
	));*/

	m_data->sequential.setText("Sequential device");
	m_data->temporal.setText("Temporal device");

	m_data->sequential.setToolTip(tr("Create a sequential (streaming) video or plot device"));
	m_data->temporal.setToolTip(tr("Create a temporal video or plot device"));

	m_data->usePoolTimeRange.setText("Find best time limits");
	m_data->usePoolTimeRange.setToolTip(tr("Use the current workspace to find the best time range"));
	m_data->start.setToolTip(tr("Device start time in seconds"));
	m_data->end.setToolTip(tr("Device end time in seconds"));

	m_data->unit.setEditable(true);
	m_data->unit.lineEdit()->setPlaceholderText("Signal unit (optional)");
	m_data->unit.setToolTip("<b>Signal unit (optional)</b><br>Enter the signal y unit or the image z unit (if the output signal is an image)");

	m_data->sequential.setChecked(true);
	m_data->usePoolTimeRange.setVisible(false);
	m_data->rangeWidget->setVisible(false);
	m_data->editor.setVisible(false);

	connect(&m_data->sequential, SIGNAL(clicked(bool)), this, SLOT(updateVisibility()));
	connect(&m_data->temporal, SIGNAL(clicked(bool)), this, SLOT(updateVisibility()));
	connect(&m_data->usePoolTimeRange, SIGNAL(clicked(bool)), this, SLOT(updateVisibility()));

	connect(&m_data->code, SIGNAL(returnPressed()), this, SLOT(updateGenerator()));
	connect(&m_data->editor, SIGNAL(Applied()), this, SLOT(updateGenerator()));
	connect(&m_data->sampling, SIGNAL(valueChanged(double)), this, SLOT(updateGenerator()));
	connect(&m_data->start, SIGNAL(valueChanged(double)), this, SLOT(updateGenerator()));
	connect(&m_data->end, SIGNAL(valueChanged(double)), this, SLOT(updateGenerator()));
	connect(&m_data->sequential, SIGNAL(clicked(bool)), this, SLOT(updateGenerator()));
	connect(&m_data->temporal, SIGNAL(clicked(bool)), this, SLOT(updateGenerator()));
	connect(&m_data->usePoolTimeRange, SIGNAL(clicked(bool)), this, SLOT(updateGenerator()));

}
PySignalGeneratorEditor::~PySignalGeneratorEditor()
{
	delete m_data;
}

void PySignalGeneratorEditor::setGenerator(PySignalGenerator * gen)
{
	if (gen != m_data->generator)
	{
		m_data->generator = gen;
		updateWidget();
	}
}

PySignalGenerator * PySignalGeneratorEditor::generator() const
{
	return m_data->generator;
}

void PySignalGeneratorEditor::updateGenerator()
{
	if (PySignalGenerator * gen = m_data->generator)
	{
		gen->propertyAt(0)->setData((qint64)(m_data->sampling.value() * 1000000000));

		if (!gen->propertyAt(4)->value<bool>())
			gen->propertyAt(3)->setData(m_data->code.text());
		else if (CodeEditor * ed = m_data->editor.CurrentEditor())
			gen->propertyAt(3)->setData(ed->toPlainText());

		VipTimeRange range = VipInvalidTimeRange;

		if (m_data->temporal.isChecked())
		{
			if (m_data->usePoolTimeRange.isChecked())
			{
				if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
				{
					range = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool()->timeLimits();
					if (range.first == VipInvalidTime)
						range = VipTimeRange(0, 10000000000);
				}
				else
				{
					range = VipTimeRange(0, 10000000000);
				}
			}
			else
			{
				range.first = ((qint64)(m_data->start.value() * 1000000000));
				range.second = ((qint64)(m_data->end.value() * 1000000000));
			}
		}

		gen->propertyAt(1)->setData(range.first);
		gen->propertyAt(2)->setData(range.second);
		gen->propertyAt(5)->setData(m_data->unit.currentText());

		updateWidget();

		if ( (gen->isOpen() || gen->property("shouldOpen").toBool()) && gen->deviceType() != VipIODevice::Sequential)
		{
			//new sampling time or time range for temporal device: recompute it
			gen->close();
			if (gen->open(VipIODevice::ReadOnly))
				gen->setProperty("shouldOpen", false);
			else
				gen->setProperty("shouldOpen", true);
			gen->reload();
		}
	}
}

void PySignalGeneratorEditor::updateVisibility()
{
	m_data->start.setEnabled(!m_data->usePoolTimeRange.isChecked());
	m_data->end.setEnabled(!m_data->usePoolTimeRange.isChecked());

	m_data->usePoolTimeRange.setVisible(m_data->temporal.isChecked());
	m_data->rangeWidget->setVisible(m_data->temporal.isChecked());
}

void PySignalGeneratorEditor::updateWidget()
{
	if (PySignalGenerator * gen = m_data->generator)
	{
		if (!gen->propertyAt(4)->value<bool>())
		{
			//simple line
			m_data->editor.setVisible(false);
			m_data->code.setVisible(true);
			m_data->code.blockSignals(true);
			m_data->code.setText(gen->propertyAt(3)->value<QString>());
			m_data->code.blockSignals(false);
		}
		else
		{
			//complex Python code
			m_data->editor.setVisible(true);
			m_data->code.setVisible(false);
			if (!m_data->editor.CurrentEditor())
				m_data->editor.NewFile();
			
			m_data->editor.blockSignals(true);
			m_data->editor.CurrentEditor()->setPlainText(gen->propertyAt(3)->value<QString>());
			m_data->editor.blockSignals(false);
		}

		

		m_data->sequential.blockSignals(true);
		if (gen->isOpen())
		{
			m_data->sequential.setChecked(gen->deviceType() == VipIODevice::Sequential);
			m_data->temporal.setChecked(gen->deviceType() != VipIODevice::Sequential);
			updateVisibility();
		}
		m_data->sequential.blockSignals(false);

		m_data->sampling.blockSignals(true);
		m_data->sampling.setValue(gen->propertyAt(0)->value<qint64>()/1000000000.0);
		m_data->sampling.blockSignals(false);

		m_data->start.blockSignals(true);
		m_data->end.blockSignals(true);

		VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(VipPlayer2D::dropTarget());

		if (pl) {
			//set the list of possible units
			QList<VipAbstractScale*> scales = pl->leftScales();
			QStringList units;
			for (int i = 0; i < scales.size(); ++i)
				units.append(scales[i]->title().text());

			m_data->unit.clear();
			m_data->unit.addItems(units);
			m_data->unit.setCurrentText(gen->propertyAt(5)->value<QString>());
		}

		if (gen->propertyAt(1)->value<qint64>() == VipInvalidTime && gen->propertyAt(2)->value<qint64>() == VipInvalidTime)
		{
			m_data->start.setValue(0);
			m_data->end.setValue(10);

			//use the drop target (if this is a VipPlotPlayer) to find a better time range
			if (pl)
			{
				if (pl->haveTimeUnit()) {
					VipInterval inter = pl->xScale()->scaleDiv().bounds().normalized();
					m_data->start.setValue(inter.minValue()*1e-9);
					m_data->end.setValue(inter.maxValue()*1e-9);
				}
			}
		}
		else
		{
			m_data->start.setValue(gen->propertyAt(1)->value<qint64>() / 1000000000.0);
			m_data->end.setValue(gen->propertyAt(2)->value<qint64>() / 1000000000.0);
		}

		m_data->start.blockSignals(false);
		m_data->end.blockSignals(false);

		
	}
}

PySignalGenerator * PySignalGeneratorEditor::createGenerator(bool complex_script)
{
	PySignalGenerator * gen = new PySignalGenerator();
	gen->propertyAt(4)->setData(complex_script);
	PySignalGeneratorEditor * editor = new PySignalGeneratorEditor();
	editor->setGenerator(gen);

	VipGenericDialog dialog(editor, "Edit Python Device");
	dialog.setMinimumWidth(300);
	if (dialog.exec() == QDialog::Accepted)
	{
		editor->updateGenerator();
		if (gen->open(VipIODevice::ReadOnly))
			return gen;
	}

	delete gen;
	return nullptr;
}



static QWidget * editPySignalGenerator(PySignalGenerator * gen)
{
	PySignalGeneratorEditor * editor = new PySignalGeneratorEditor();
	editor->setGenerator(gen);
	return editor;
}
static int registerFunction()
{
	vipFDObjectEditor().append<QWidget*(PySignalGenerator*)>(editPySignalGenerator);
	return 0;
}
static int _registerFunction = registerFunction();
