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

#include "VipExtractStatistics.h"
#include "VipSceneModel.h"

QStringList VipExtractComponent::supportedComponents() const
{
	// if (m_supportedComponents.isEmpty()) {
	//  VipAnyData in = inputAt(0)->probe();
	//  if (in.data().userType() == qMetaTypeId<VipNDArray>())
	//  const_cast<QStringList&>(m_supportedComponents) = m_extract.SupportedComponents(in.value<VipNDArray>());
	//  else if (in.data().userType() == qMetaTypeId<VipComplexPointVector>())
	//  const_cast<QStringList&>(m_supportedComponents) = m_extract.SupportedComponents(VipNDArrayType<complex_d>());
	//  }
	return m_supportedComponents;
}

void VipExtractComponent::apply()
{
	VipAnyData in = inputAt(0)->data();

	if (in.data().userType() == qMetaTypeId<VipNDArray>()) {
		VipNDArray input_image = in.value<VipNDArray>();
		m_supportedComponents = m_extract.SupportedComponents(input_image);
		QString component = propertyAt(0)->data().value<QString>();
		if (component.isEmpty()) {
			component = defaultComponent();
			if (!component.isEmpty())
				propertyAt(0)->setData(component);
		}
		m_extract.SetComponent(component);

		VipNDArray out = m_extract.Extract(input_image);
		VipAnyData any = create(QVariant::fromValue(out));
		any.mergeAttributes(in.attributes());
		any.setTime(in.time());
		outputAt(0)->setData(any);
		return;
	}
	else if (in.data().userType() == qMetaTypeId<VipComplexPointVector>()) {
		VipComplexPointVector samples = in.value<VipComplexPointVector>();
		if (!in.isEmpty()) {
			m_supportedComponents = m_extract.SupportedComponents(VipNDArrayType<complex_d>());
			QString component = propertyAt(0)->data().value<QString>();
			if (component.isEmpty()) {
				component = defaultComponent();
				if (!component.isEmpty())
					propertyAt(0)->setData(component);
			}
			m_extract.SetComponent(component);

			VipNDArrayType<complex_d> ar(vipVector(samples.size()));
			for (int i = 0; i < samples.size(); ++i)
				ar[i] = samples[i].y();

			VipNDArrayType<double> out = m_extract.Extract(ar);
			VipPointVector out_data(out.size());

			// if (!samples.hasNanosecondPrecision())
			{
				for (int i = 0; i < out.size(); ++i)
					out_data[i] = VipPoint(samples[i].x(), out[i]);
			}
			// else
			//  {
			//  for (int i = 0; i < out.size(); ++i)
			//  out_data.setValues(i, samples.time(i), out[i]);
			//  }

			outputAt(0)->setData(create(QVariant::fromValue(out_data)));
			return;
		}
	}

	outputAt(0)->setData(in);
}

static QList<QVariant> extract_components(const QVariant& in, VipExtractComponents* extract)
{
	QList<QVariant> res;
	if (extract->GetType() == VipExtractComponents::None || in.userType() == qMetaTypeId<VipPointVector>() || in.canConvert<double>()) {
		res << in;
		return res;
	}

	if (in.userType() == qMetaTypeId<complex_d>() || in.userType() == qMetaTypeId<complex_f>()) {
		if (extract->GetType() == VipExtractComponents::Complex) {
			VipNDArray ar(qMetaTypeId<complex_d>(), vipVector(1));
			*(complex_d*)ar.data() = in.value<complex_d>();
			extract->SeparateComponents(ar);
			QList<VipNDArray> arrays = extract->GetComponents();
			for (int i = 0; i < arrays.size(); ++i)
				res.append(arrays[i].value(vipVector(0)));
		}
	}
	else if (in.userType() == qMetaTypeId<VipComplexPointVector>()) {
		const VipComplexPointVector vec = in.value<VipComplexPointVector>();
		const VipNDArray x = vipExtractXValues(vec);
		const VipNDArray y = vipExtractYValues(vec);
		extract->SeparateComponents(y);
		QList<VipNDArray> arrays = extract->GetComponents();
		for (int i = 0; i < arrays.size(); ++i) {
			VipPointVector tmp = vipCreatePointVector(x, arrays[i]);
			res.append(QVariant::fromValue(tmp));
		}
	}
	else if (in.userType() == qMetaTypeId<VipNDArray>()) {
		const VipNDArray ar = in.value<VipNDArray>();
		extract->SeparateComponents(ar);
		QList<VipNDArray> arrays = extract->GetComponents();
		for (int i = 0; i < arrays.size(); ++i)
			res.append(QVariant::fromValue(arrays[i]));
	}
	return res;
}

static QVariant merge_components(const QVariantList& lst, VipExtractComponents* extract)
{
	if (extract->GetType() == VipExtractComponents::None || lst.isEmpty()) {
		if (lst.size() == 1)
			return lst.first();
		else
			return QVariant();
	}

	QList<VipNDArray> arrays;
	for (int i = 0; i < lst.size(); ++i) {
		const QVariant v = lst[i];
		if (v.canConvert<double>()) {
			VipNDArray ar(QMetaType::Double, vipVector(1));
			*(double*)ar.data() = v.value<double>();
			arrays.append(ar);
		}
		else if (v.userType() == qMetaTypeId<VipPointVector>()) {
			arrays.append(vipExtractYValues(v.value<VipPointVector>()));
		}
		else if (v.userType() == qMetaTypeId<VipNDArray>()) {
			arrays.append(v.value<VipNDArray>());
		}
	}

	if (arrays.size() != lst.size())
		return QVariant();

	extract->SetComponents(arrays);
	VipNDArray ar = extract->MergeComponents();

	// case complex
	if (ar.size() == 1 && ar.isComplex())
		return ar.value(vipVector(0));
	else if (lst.first().userType() == qMetaTypeId<VipPointVector>() && ar.isComplex()) {
		return QVariant::fromValue(vipCreateComplexPointVector(vipExtractXValues(lst.first().value<VipPointVector>()), ar));
	}
	else {
		return QVariant::fromValue(ar);
	}
}

VipSplitAndMerge::VipSplitAndMerge(QObject* parent)
  : VipProcessingObject(parent)
  , m_extract(nullptr)
  , m_is_applying(false)
{
}

VipSplitAndMerge::~VipSplitAndMerge()
{
	QMutexLocker lock(&m_mutex);
	for (int i = 0; i < m_procList.size(); ++i)
		delete m_procList[i];
	if (m_extract)
		delete m_extract;
}

bool VipSplitAndMerge::setMethod(const QString& method)
{
	QMutexLocker lock(&m_mutex);
	VipAnyData in = inputAt(0)->probe();
	QStringList possible_methods = possibleMethods(in.data());

	if (!possible_methods.isEmpty() && possible_methods.indexOf(method) < 0)
		return false;

	int component_count = componentCount(method);
	if (component_count != m_procList.size()) {
		// different number of components: reset the lists
		for (int i = 0; i < m_procList.size(); ++i)
			delete m_procList[i];
		m_procList.resize(component_count);
		for (int i = 0; i < m_procList.size(); ++i) {
			m_procList[i] = new VipProcessingList();
			// detect whenever a processing list is applyied
			connect(m_procList[i], SIGNAL(processingDone(VipProcessingObject*, qint64)), this, SLOT(receivedProcessingDone(VipProcessingObject*, qint64)), Qt::DirectConnection);
		}
	}

	if (m_extract) {
		delete m_extract;
		m_extract = nullptr;
	}

	if (method == "Complex Real/Imag")
		m_extract = new VipExtractComplexRealImag();
	else if (method == "Complex Amplitude/Argument")
		m_extract = new VipExtractComplexAmplitudeArgument();
	else if (method == "Color ARGB")
		m_extract = new VipExtractARGBComponents();
	else if (method == "Color AHSL")
		m_extract = new VipExtractHSVComponents();
	else if (method == "Color AHSV")
		m_extract = new VipExtractHSVComponents();
	else if (method == "Color ACMYK")
		m_extract = new VipExtractCMYKComponents();

	if (!m_extract)
		return false;

	// set the input data for each proc list
	QList<QVariant> ins = extract_components(in.data(), m_extract);
	if (!ins.isEmpty()) {
		if (m_extract->GetComponents().size() == m_procList.size()) {
			m_mutex.unlock();
			for (int i = 0; i < m_procList.size(); ++i) {
				// set data and update (will also set the data to the output)
				m_procList[i]->inputAt(0)->setData(ins[i]);
				m_procList[i]->update();
			}
			m_mutex.lock();
		}
	}

	m_method = method;
	return true;
}

QStringList VipSplitAndMerge::components() const
{
	if (m_extract)
		return m_extract->PixelComponentNames();
	else
		return QStringList();
}

bool VipSplitAndMerge::acceptData(const QVariant& data, const QString& method)
{
	if (method == "Complex Real/Imag" || method == "Complex Amplitude/Argument")
		return data.userType() == qMetaTypeId<complex_f>() || data.userType() == qMetaTypeId<complex_d>() || data.userType() == qMetaTypeId<VipComplexPoint>() ||
		       data.userType() == qMetaTypeId<VipComplexPointVector>() || (data.userType() == qMetaTypeId<VipNDArray>() && data.value<VipNDArray>().isComplex());
	else if (method == "Color ARGB" || method == "Color AHSL" || method == "Color AHSV" || method == "Color ACMYK")
		return (data.userType() == qMetaTypeId<VipNDArray>() && vipIsImageArray(data.value<VipNDArray>()));
	else
		return false;
}

int VipSplitAndMerge::componentCount(const QString& method)
{
	if (method == "Complex Real/Imag" || method == "Complex Amplitude/Argument")
		return 2;
	else if (method == "Color ARGB" || method == "Color AHSL" || method == "Color AHSV")
		return 4;
	else if (method == "Color ACMYK")
		return 5;
	else
		return 0;
}

QStringList VipSplitAndMerge::possibleMethods(const QVariant& data)
{
	if (data.userType() == qMetaTypeId<VipNDArray>()) {
		const VipNDArray& ar = data.value<VipNDArray>();
		if (ar.dataType() == qMetaTypeId<complex_f>() || ar.dataType() == qMetaTypeId<complex_d>())
			return QStringList() << "Complex Real/Imag"
					     << "Complex Amplitude/Argument";
		else if (vipIsImageArray(ar))
			return QStringList() << "Color ARGB"
					     << "Color AHSL"
					     << "Color AHSV"
					     << "Color ACMYK";
	}
	else if (data.userType() == qMetaTypeId<complex_f>() || data.userType() == qMetaTypeId<complex_d>() || data.userType() == qMetaTypeId<VipComplexPoint>() ||
		 data.userType() == qMetaTypeId<VipComplexPointVector>()) {
		return QStringList() << "Complex Real/Imag"
				     << "Complex Amplitude/Argument";
	}

	return QStringList();
}

bool VipSplitAndMerge::acceptInput(int, const QVariant& v) const
{
	return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<complex_f>() || v.userType() == qMetaTypeId<complex_d>() || v.userType() == qMetaTypeId<VipComplexPoint>() ||
	       v.userType() == qMetaTypeId<VipComplexPointVector>();
}

void VipSplitAndMerge::receivedProcessingDone(VipProcessingObject* lst, qint64)
{
	if (lst && !m_is_applying) {
		// one of the processing list was just applied outside of a apply() call.
		// this is probably due to a user change in the processing editor.
		// reapply the split and merge but exclude lst from the computation.
		applyInternal(false);
	}
}

void VipSplitAndMerge::apply()
{
	applyInternal(true);
}

void VipSplitAndMerge::applyInternal(bool update)
{
	qint64 st = vipGetNanoSecondsSinceEpoch();

	VipAnyData in = inputAt(0)->data();

	QMutexLocker lock(&m_mutex);
	m_is_applying = true;
	if (m_procList.size() == 0) {
		// nullptr method: just return the input
		outputAt(0)->setData(in);
		m_is_applying = false;
		return;
	}

	if (!acceptData(in.data(), m_method)) {
		// wrong input type
		setError("Wrong input data type", VipProcessingObject::WrongInput);
		outputAt(0)->setData(in);
		m_is_applying = false;
		return;
	}

	// build the input array
	VipAnyDataList inputs;
	QVariantList lst = extract_components(in.data(), m_extract);
	for (int i = 0; i < lst.size(); ++i) {
		VipAnyData any(lst[i], in.time());
		any.setAttributes(in.attributes());
		inputs.append(any);
	}

	// apply
	if (update) {
#pragma omp parallel for
		for (int i = 0; i < m_procList.size(); ++i) {
			m_procList[i]->inputAt(0)->setData(inputs[i]);
			m_procList[i]->update();
			// lst[i] = m_procList[i]->outputAt(0)->data().data();
		}
	}

	// merge
	for (int i = 0; i < lst.size(); ++i) {
		lst[i] = m_procList[i]->outputAt(0)->data().data();
	}
	QVariant out = merge_components(lst, m_extract);

	m_is_applying = false;
	if (out.userType() == 0) {
		setError("Unable to merge components");
		outputAt(0)->setData(in);
		return;
	}

	outputAt(0)->setData(create(out));

	if (!update) {
		// this function wasn't called from apply(), the signal processingDone() is not emitted, so emit it
		qint64 elapsed = vipGetNanoSecondsSinceEpoch() - st;
		Q_EMIT processingDone(this, elapsed);
	}
}

void VipExtractShapeData::setArray(const VipNDArray& ar)
{
	m_components = VipGenericExtractComponent().StandardComponents(ar);
	if (!m_components.size())
		m_components.append(QString());
}

VipExtractHistogram::~VipExtractHistogram()
{
	if (m_extract)
		delete m_extract;
}

VipExtractComponents* VipExtractHistogram::extract() const
{
	return m_extract;
}

void VipExtractHistogram::apply()
{
	VipNDArray ar;
	VipShape shape;
	int bins = 0;

	VipAnyData data = inputAt(0)->data();
	ar = data.data().value<VipNDArray>();
	bins = propertyName("bins")->data().value<int>();
	shape = this->shape();

	QString title = shape.group() + " " + QString::number(shape.id());
	if (shape.isNull() || (ar.shapeCount() == 2 && shape.shape().boundingRect() == QRectF(0, 0, ar.shape(1), ar.shape(0))))
		title = "Histogram " + data.name();

	VipMultiOutput* out = topLevelOutputAt(0)->toMultiOutput();

	if (!ar.isEmpty() && !shape.isNull() && bins) {
		QString name = propertyName("output_name")->value<QString>();
		if (name.isEmpty()) {
			name = shape.attribute("Name").toString();
			if (name.isEmpty())
				name = "Histogram (" + shape.group() + " " + QString::number(shape.id()) + ")";
		}

		if (ar.canConvert<double>()) {
			VipIntervalSampleVector histogram = shape.histogram(bins, ar, QPoint(0, 0), &buffer());
			VipAnyData any = create(QVariant::fromValue(histogram));
			any.setTime(data.time());
			any.setXUnit(data.zUnit());
			any.setName(name);
			out->resize(1);
			out->at(0)->setData(any);
		}
		else {
			QString method = propertyName("method")->value<QString>();
			if (!m_extract || m_extract->GetMethod() != method) {
				if (m_extract) {
					delete m_extract;
					m_extract = nullptr;
				}
				if (!(m_extract = vipCreateExtractComponents(method))) {
					setError("Invalid component splitting method: " + method);
					return;
				}
			}
			out->resize(vipComponentsCount(method));
			m_extract->SeparateComponents(ar);
			QList<VipNDArray> ars = m_extract->GetComponents();
			if (ars.size() == 0) {
				setError("Invalid component splitting method: " + method);
				return;
			}

			for (int i = 0; i < ars.size(); ++i) {
				VipIntervalSampleVector histogram = shape.histogram(bins, ars[i], QPoint(0, 0), &buffer());
				VipAnyData any = create(QVariant::fromValue(histogram));
				any.setTime(data.time());
				any.setXUnit(data.zUnit());
				any.setName(name + " " + m_extract->PixelComponentNames()[i]);
				out->at(i)->setData(any);
			}
		}
	}
	else {
		VipIntervalSampleVector histogram;
		for (int i = 0; i < outputCount(); ++i) {
			VipAnyData any = create(QVariant::fromValue(histogram));
			any.setTime(data.time());
			any.setXUnit(data.zUnit());
			outputAt(i)->setData(create(QVariant::fromValue(histogram)));
		}
		emitError(this, VipErrorData("VipExtractHistogram: wrong input values"));
	}
}

void VipExtractPolyline::apply()
{

	VipNDArray ar;
	VipShape shape;

	VipAnyData data = inputAt(0)->data();

	ar = data.data().value<VipNDArray>();

	shape = this->shape();

	VipMultiOutput* out = topLevelOutputAt(0)->toMultiOutput();

	if (!ar.isEmpty() && !shape.isNull()) {
		QString name = propertyName("output_name")->value<QString>();
		if (name.isEmpty()) {
			name = shape.attribute("Name").toString();
			if (name.isEmpty())
				name = shape.group() + " " + QString::number(shape.id());
		}

		if (ar.canConvert<double>()) {
			VipPointVector polyline = shape.polyline(ar, QPoint(0, 0), &buffer());

			out->resize(1);

			VipAnyData any = create(QVariant::fromValue(polyline));
			any.setTime(data.time());
			any.setYUnit(data.zUnit());
			any.setName(name);

			out->at(0)->setData(any);
		}
		else {
			QString method = propertyName("method")->value<QString>();
			if (!m_extract || m_extract->GetMethod() != method) {
				if (m_extract) {
					delete m_extract;
					m_extract = nullptr;
				}
				if (!(m_extract = vipCreateExtractComponents(method))) {
					setError("Invalid component splitting method: " + method);
					return;
				}
			}
			out->resize(vipComponentsCount(method));
			m_extract->SeparateComponents(ar);
			QList<VipNDArray> ars = m_extract->GetComponents();
			if (ars.size() == 0) {
				setError("Invalid component splitting method: " + method);
				return;
			}

			for (int i = 0; i < ars.size(); ++i) {
				VipPointVector polyline = shape.polyline(ars[i], QPoint(0, 0), &buffer());

				VipAnyData any = create(QVariant::fromValue(polyline));
				any.setTime(data.time());
				any.setXUnit(data.zUnit());
				any.setName(name + " " + m_extract->PixelComponentNames()[i]);
				out->at(i)->setData(any);
			}
		}
	}
	else
		emitError(this, VipErrorData("Wrong input values"));
}

void VipExtractStatistics::setStatistics(VipShapeStatistics::Statistics s)
{
	if (s != m_stats) {
		m_stats = s;
		updateStatistics();
	}
}

void VipExtractStatistics::setStatistic(VipShapeStatistics::Statistic s, bool on)
{
	if (m_stats.testFlag(s) != on) {
		if (on)
			m_stats |= s;
		else
			m_stats &= ~s;

		updateStatistics();
	}
}
bool VipExtractStatistics::testStatistic(VipShapeStatistics::Statistic s) const
{
	return m_stats.testFlag(s);
}

VipShapeStatistics::Statistics VipExtractStatistics::statistics() const
{
	return m_stats;
}

void VipExtractStatistics::setShapeQuantiles(const QVector<double>& quantiles)
{
	m_quantiles = quantiles;
	updateStatistics();
}
const QVector<double>& VipExtractStatistics::shapeQuantiles() const
{
	return m_quantiles;
}

void VipExtractStatistics::updateStatistics()
{
	topLevelOutputAt(0)->setEnabled(testStatistic(VipShapeStatistics::Minimum));
	topLevelOutputAt(1)->setEnabled(testStatistic(VipShapeStatistics::Maximum));
	topLevelOutputAt(2)->setEnabled(testStatistic(VipShapeStatistics::Mean));
	topLevelOutputAt(3)->setEnabled(testStatistic(VipShapeStatistics::Std));
	topLevelOutputAt(4)->setEnabled(testStatistic(VipShapeStatistics::PixelCount));
	topLevelOutputAt(5)->setEnabled(testStatistic(VipShapeStatistics::Entropy));
	topLevelOutputAt(6)->setEnabled(testStatistic(VipShapeStatistics::Kurtosis));
	topLevelOutputAt(7)->setEnabled(testStatistic(VipShapeStatistics::Skewness));
	topLevelOutputAt(8)->setEnabled(m_quantiles.size() > 0);

	outputAt(0)->setData(testStatistic(VipShapeStatistics::Minimum) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(1)->setData(testStatistic(VipShapeStatistics::Maximum) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(2)->setData(testStatistic(VipShapeStatistics::Mean) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(3)->setData(testStatistic(VipShapeStatistics::Std) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(4)->setData(testStatistic(VipShapeStatistics::PixelCount) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(5)->setData(testStatistic(VipShapeStatistics::Entropy) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(6)->setData(testStatistic(VipShapeStatistics::Kurtosis) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(7)->setData(testStatistic(VipShapeStatistics::Skewness) ? QVariant::fromValue(VipPointVector()) : QVariant());
	outputAt(8)->setData(m_quantiles.size() > 0 ? QVariant::fromValue(VipRectList()) : QVariant());
}

void VipExtractStatistics::apply()
{
	VipNDArray ar;
	VipShape shape;

	VipAnyData data = inputAt(0)->data();
	ar = data.data().value<VipNDArray>();

	shape = this->shape();

	if (!ar.isEmpty() && ar.shapeCount() == 2 && !shape.isNull() && ar.canConvert<double>()) {

		VipShapeStatistics statistics = shape.statistics(ar, QPoint(0, 0), &buffer(), m_stats, m_quantiles);

		VipOutput* min = topLevelOutputAt(0)->toOutput();
		VipOutput* max = topLevelOutputAt(1)->toOutput();
		VipOutput* mean = topLevelOutputAt(2)->toOutput();
		VipOutput* std = topLevelOutputAt(3)->toOutput();
		VipOutput* pixel_count = topLevelOutputAt(4)->toOutput();
		VipOutput* entropy = topLevelOutputAt(5)->toOutput();
		VipOutput* kurtosis = topLevelOutputAt(6)->toOutput();
		VipOutput* skewness = topLevelOutputAt(7)->toOutput();
		VipOutput* quantiles = topLevelOutputAt(8)->toOutput();

		QString name = shape.name();
		if (name.isEmpty())
			name = shape.group() + " " + QString::number(shape.id());

		if (testStatistic(VipShapeStatistics::Minimum)) {
			VipAnyData any = create(QVariant::fromValue(statistics.min));
			any.setName(name + " minimum");
			any.setTime(data.time());
			any.setYUnit(data.zUnit());
			any.setXUnit("time");
			any.setAttribute("Pos", QVariant::fromValue(statistics.minPoint));
			min->setData(any);
		}
		if (testStatistic(VipShapeStatistics::Maximum)) {
			VipAnyData any = create(QVariant::fromValue(statistics.max));
			any.setName(name + " maximum");
			any.setTime(data.time());
			any.setYUnit(data.zUnit());
			any.setXUnit("time");
			any.setAttribute("Pos", QVariant::fromValue(statistics.maxPoint));
			max->setData(any);
		}
		if (testStatistic(VipShapeStatistics::Mean)) {
			VipAnyData any = create(QVariant::fromValue(statistics.average));
			any.setName(name + " mean");
			any.setTime(data.time());
			any.setYUnit(data.zUnit());
			any.setXUnit("time");
			mean->setData(any);
		}
		if (testStatistic(VipShapeStatistics::Std)) {
			VipAnyData any = create(QVariant::fromValue(statistics.std));
			any.setName(name + " std");
			any.setTime(data.time());
			any.setXUnit("time");
			std->setData(any);
		}
		if (testStatistic(VipShapeStatistics::PixelCount)) {
			VipAnyData any = create(QVariant::fromValue(statistics.pixelCount));
			any.setName(name + " pixels");
			any.setTime(data.time());
			any.setXUnit("time");
			pixel_count->setData(any);
		}
		if (testStatistic(VipShapeStatistics::Entropy)) {
			VipAnyData any = create(QVariant::fromValue(statistics.entropy));
			any.setName(name + " entropy");
			any.setTime(data.time());
			any.setXUnit("time");
			entropy->setData(any);
		}
		if (testStatistic(VipShapeStatistics::Kurtosis)) {
			VipAnyData any = create(QVariant::fromValue(statistics.kurtosis));
			any.setName(name + " kurtosis");
			any.setTime(data.time());
			any.setXUnit("time");
			kurtosis->setData(any);
		}
		if (testStatistic(VipShapeStatistics::Skewness)) {
			VipAnyData any = create(QVariant::fromValue(statistics.skewness));
			any.setName(name + " skewness");
			any.setTime(data.time());
			any.setXUnit("time");
			skewness->setData(any);
		}
		if (m_quantiles.size() > 0) {
			VipAnyData any = create(QVariant::fromValue(statistics.quantiles));
			any.setName(name + " quantiles");
			any.setTime(data.time());
			any.setXUnit("time");
			quantiles->setData(any);
		}
	}
	else {
		setError("wrong input values", VipProcessingObject::WrongInput);
		// shape.isNull();
	}
}

VipExtractShapeAttribute::VipExtractShapeAttribute()
{
	outputAt(0)->setData(0);
}

void VipExtractShapeAttribute::apply()
{
	VipAnyData any = inputAt(0)->data();
	VipSceneModel scene = any.value<VipSceneModel>();
	if (any.data().userType() != qMetaTypeId<VipSceneModel>()) {
		setError("wrong input type", VipProcessingObject::WrongInput);
		return;
	}
	VipShape sh = scene.find(propertyAt(0)->value<QString>(), propertyAt(1)->value<int>());
	if (sh.isNull()) {
		return;
	}

	QVariant attr = sh.attribute(propertyAt(2)->value<QString>());
	if (attr.userType() == 0) {
		return;
	}

	bool ok = false;
	double value = attr.toDouble(&ok);
	if (!ok) {
		QString tmp = attr.toString();
		QTextStream stream(&tmp);
		if ((stream >> value).status() == QTextStream::Ok)
			ok = true;
	}

	QVariant out;
	if (ok)
		out = QVariant(value);
	else
		out = QVariant(attr);
	VipAnyData any_out = create(out);
	any_out.setTime(any.time());
	outputAt(0)->setData(any_out);
}

#include "VipArchive.h"

static VipArchive& operator<<(VipArchive& stream, const VipSplitAndMerge* r)
{
	stream.content("method", r->method());
	stream.content("componentCount", r->componentCount());
	stream.start("components");
	for (int i = 0; i < r->componentCount(); ++i) {
		stream.content("list", r->componentProcessings(i));
	}
	stream.end();
	return stream;
}

static VipArchive& operator>>(VipArchive& stream, VipSplitAndMerge* r)
{
	QString method = stream.read("method").toString();
	int count = stream.read("componentCount").toInt();
	stream.start("components");

	r->setMethod(method);
	if (count == r->componentCount()) {
		for (int i = 0; i < r->componentCount(); ++i) {
			stream.content("list", r->componentProcessings(i));
		}
	}

	stream.end();
	return stream;
}

VipArchive& operator<<(VipArchive& stream, const VipExtractStatistics* r)
{
	return stream.content("statistics", (int)r->statistics());
}

VipArchive& operator>>(VipArchive& stream, VipExtractStatistics* r)
{
	r->setStatistics(VipShapeStatistics::Statistics(stream.read("statistics").value<int>()));
	return stream;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipSplitAndMerge*>();
	vipRegisterArchiveStreamOperators<VipExtractStatistics*>();
	return 0;
}

static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);
