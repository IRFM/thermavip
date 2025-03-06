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

#include "VipVTKDevices.h"
#include "VipVTKObject.h"
#include "VipXmlArchive.h"

#include "VipMath.h"
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkDelaunay3D.h>

#include <QTemporaryFile>



VipArchive& operator<<(VipArchive& arch, const VipVTKObject& obj)
{
	VipVTKObjectLocker lock = vipLockVTKObjects(obj);

	QString name = obj.dataName();
	arch.content("name", name);

	// save the dynamic properties
	/* const QList<DynamicProperty>& dyns = obj.dynamicProperties();
	arch.start("dynamicProperties");
	arch.content("count", dyns.size());
	for (int i = 0; i < dyns.size(); ++i)
		arch.content(dyns[i]);
	arch.end();*/

	if (obj && !QFileInfo(name).exists()) {

		QTemporaryFile file;
		if (!file.open()) {
			arch.setError("Cannot create temporary file");
			return arch;
		}

		QString format = obj.preferredSuffix();
		if (format.isEmpty()) {
			arch.setError("Cannot find 3D object format");
			return arch;
		}
		QString path = file.fileName() + "." + format;
		if (!obj.save(path)) {
			arch.setError("Cannot save 3D object");
			return arch;
		}
		QFile fin(path);
		if (!fin.open(QFile::ReadOnly)) {
			arch.setError("Cannot open temporary file");
			return arch;
		}
		QByteArray ar = fin.readAll();
		ar = qCompress(ar, 9);

		arch.content("format", format);
		arch.content("data", ar);
	}

	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipVTKObject& obj)
{
	QString name = arch.read("name").toString();

	/* if (arch.start("dynamicProperties")) {
		int count = arch.read("count").toInt();
		for (int i = 0; i < count; ++i) {
			DynamicProperty dyn = arch.read().value<DynamicProperty>();
			if (!dyn.timeRanges().isEmpty())
				obj.addDynamicProperty(dyn);
		}
		arch.end();
	}*/

	arch.resetError();

	// Try to read the CAD object
	arch.save();
	QString format = arch.read("format").toString();
	QByteArray ar = arch.read("data").toByteArray();
	if (format.isEmpty() || ar.isEmpty()) {
		arch.restore();
		return arch;
	}
	QByteArray dec = qUncompress(ar);
	if (!dec.isEmpty())
		ar = dec;
	VipVTKObject res = VipVTKObject::loadFromBuffer(ar, format);

	obj = res;
	if (obj)
		obj.data()->SetObjectName(name.toLatin1().data());

	arch.resetError();
	return arch;
}

static int memoryFootprint(int, const QVariant& v)
{
	if (VipVTKObject ptr = v.value<VipVTKObject>()) {
		int size = 0;
		if (ptr.data())
			size += ptr.data()->GetActualMemorySize() * 1024;

		return size;
	}
	return 0;
}

VipArchive& operator<<(VipArchive& arch, const VipFieldOfView& fov)
{
	arch.content("name", fov.name);
	arch.content("pupil", VipFieldOfView::pointToString(fov.pupil));
	arch.content("target", VipFieldOfView::pointToString(fov.target));
	arch.content("vertical_angle", fov.vertical_angle);
	arch.content("horizontal_angle", fov.horizontal_angle);
	arch.content("rotation", fov.rotation);
	arch.content("focal", fov.focal);
	arch.content("view_up", fov.view_up);
	arch.content("width", fov.width);
	arch.content("height", fov.height);
	arch.content("crop_x", fov.crop_x);
	arch.content("crop_y", fov.crop_y);
	arch.content("zoom", fov.zoom);
	arch.content("K2", fov.K2);
	arch.content("K4", fov.K4);
	arch.content("K6", fov.K6);
	arch.content("P1", fov.P1);
	arch.content("P2", fov.P2);
	arch.content("AlphaC", fov.AlphaC);
	arch.content("time", fov.time);
	arch.content("attributes", fov.attributes);
	return arch;
}
VipArchive& operator>>(VipArchive& arch, VipFieldOfView& fov)
{
	fov.name = arch.read("name").toString();
	VipFieldOfView::pointFromString(fov.pupil, arch.read("pupil").toString());
	VipFieldOfView::pointFromString(fov.target, arch.read("target").toString());
	fov.vertical_angle = arch.read("vertical_angle").toDouble();
	fov.horizontal_angle = arch.read("horizontal_angle").toDouble();
	fov.rotation = arch.read("rotation").toDouble();
	fov.focal = arch.read("focal").toDouble();
	fov.view_up = arch.read("view_up").toInt();
	fov.width = arch.read("width").toInt();
	fov.height = arch.read("height").toInt();
	fov.crop_x = arch.read("crop_x").toInt();
	fov.crop_y = arch.read("crop_y").toInt();
	fov.zoom = arch.read("zoom").toDouble();
	fov.K2 = arch.read("K2").toDouble();
	fov.K4 = arch.read("K4").toDouble();
	fov.K6 = arch.read("K6").toDouble();
	fov.P1 = arch.read("P1").toDouble();
	fov.P2 = arch.read("P2").toDouble();
	fov.AlphaC = arch.read("AlphaC").toDouble();
	fov.time = arch.read("time").toLongLong();
	fov.attributes = arch.read("attributes").value<QVariantMap>();
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipFieldOfViewList& fov)
{
	arch.content("count", fov.size());
	arch.start("fovs");

	for (int i = 0; i < fov.size(); ++i) {
		arch.start("fov_" + QString::number(i));
		arch << fov[i];
		arch.end();
	}
	arch.end();
	return arch;
}
VipArchive& operator>>(VipArchive& arch, VipFieldOfViewList& fov)
{
	int count = arch.read("count").toInt();
	if (!arch.start("fovs"))
		return arch;

	for (int i = 0; i < count; ++i) {
		if (arch.start("fov_" + QString::number(i))) {
			VipFieldOfView f;
			arch >> f;
			arch.end();
			fov.push_back(f);
		}
		else {
			arch.end();
			return arch;
		}
	}

	arch.end();
	return arch;
}



VipVTKFileReader::~VipVTKFileReader()
{
	close();
}

bool VipVTKFileReader::open(VipIODevice::OpenModes mode)
{
	this->setOpenMode(NotOpen);

	if (mode != ReadOnly)
		return false;

	VipVTKObject data;

	QFileInfo info(removePrefix(path()));
	if (info.exists()) {
		// short path
		data = VipVTKObject::load(removePrefix(path()));
		if (data) {
			data.setDataName(info.canonicalFilePath());
			d_data = create(QVariant::fromValue(data));
			d_data.setName(QFileInfo(info.canonicalFilePath()).fileName());
			d_data.mergeAttributes(data.buildAllAttributes());
			
			this->setOpenMode(mode);
			read(0);
			return true;
		}
	}

	if (this->mapFileSystem()) {
		if (QIODevice* dev = createDevice(removePrefix(path()), QIODevice::ReadOnly)) {
			QByteArray ar = dev->readAll();
			data = VipVTKObject::loadFromBuffer(ar, QFileInfo(removePrefix(path())).suffix());
			if (data ) {
				QFileInfo info(removePrefix(path()));
				if (info.exists())
					data.setDataName(info.canonicalFilePath());
				else {
					QString str = removePrefix(path());
					str.replace("\\", "/");
					while (true) {
						if (str.startsWith("./"))
							str.remove(0, 2);
						else if (str.startsWith("../"))
							str.remove(0, 3);
						else if (str.startsWith("/"))
							str.remove(0, 1);
						else
							break;
					}
					data.setDataName(str);
				}
				// printf("file: %s\n", data->name().toLatin1().data());
			}
			else
				return false;
		}
	}
	else
		data = VipVTKObject::load(removePrefix(path()));

	if (!data)
		return false;

	d_data = create(QVariant::fromValue(data));
	d_data.setName(QFileInfo(data.dataName()).fileName());
	d_data.mergeAttributes(data.buildAllAttributes());
	this->setOpenMode(mode);
	read(0);
	return true;
}

#include <qfile.h>
#include <vtkDelaunay2D.h>
#include <vtkUnstructuredGrid.h>

bool VipXYZValueFileReader::open(VipIODevice::OpenModes mode)
{
	setOpenMode(NotOpen);
	if (mode != VipIODevice::ReadOnly)
		return false;


	if (QIODevice* device = this->createDevice(removePrefix(path()), QIODevice::ReadOnly | QIODevice::Text)) {
		QString data_name = removePrefix(path());
		if (QFileInfo(data_name).exists())
			data_name = QFileInfo(data_name).canonicalFilePath();

		QByteArray content = device->readAll();

		QStringList attributes;
		int skip_line = 0;
		{
			// read attribute names for CSV format
			QTextStream str(content);
			QString line = str.readLine();
			if (line.contains("sep=")) {
				//read next line
				line = str.readLine();
				if (line.startsWith("X")) {
					line.replace("\t", " ");
					line.replace(",", " ");
					line.replace(";", " ");
					attributes = line.split(" ", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
					if (attributes.size() < 3)
						attributes.clear();
				}
			}

			if (attributes.size()) {
				skip_line = 2;
				while (true) {
					if (str.atEnd())
						break;
					line = str.readLine();
					line.replace(",", ".");
					QTextStream substr(&line);
					double val = 0;
					substr >> val;
					if (substr.status() == QTextStream::Ok)
						break;
					else {
						++skip_line;
					}
				}
			}
		}


		// read the device as a VipNDArray
		content.replace(',', '.');
		QTextStream stream(content);

		// skip header lines
		for (int i = 0; i < skip_line; ++i)
			stream.readLine();


		if (attributes.isEmpty()) {

			qint64 pos = stream.pos();
			QString line = stream.readLine();
			
			//fill attribute names, find column count

			QString val;
			int columns = 0;
			QTextStream str(&line);
			while (true) {
				str >> val;
				if (str.status() == QTextStream::Ok && !val.isEmpty())
					++columns;
				else
					break;
			}
			if (columns < 3) {
				setError("Wrong number of columns (shouble be >= 3)");
				return false;
			}
			attributes << "X"
					<< "Y"
					<< "Z";
			for (int i = 3; i < columns; ++i)
				attributes << "Value" + QString::number(i - 3);

			stream.seek(pos);
		}


		VipNDArrayType<double> ar;
		stream >> ar;

		// the file must contain at least 3 columns
		if (ar.shape(1) < 3 || ar.shape(1) != attributes.size()) {
			setError("Wrong number of columns (should be >= 3)");
			return false;
		}

		vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
		pts->SetNumberOfPoints(ar.shape(0));
		for (int i = 0; i < ar.shape(0); ++i) {
			double point[3] = { ar(vipVector(i, 0)), ar(vipVector(i, 1)), ar(vipVector(i, 2)) };
			pts->SetPoint(i, point);
		}


		QVector<vtkSmartPointer<vtkDoubleArray>> attrs;
		// Create attributes
		QString prefix;
		int count = 0;
		for (int i = 3; i < attributes.size(); ++i) {
		
			QString name = attributes[i];
			if (!name.contains("_")) {
				if (!prefix.isEmpty()) {
					attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
					attrs.back()->SetName(prefix.toLatin1().data());
					attrs.back()->SetNumberOfComponents(count);
					attrs.back()->SetNumberOfTuples(ar.shape(0));
					count = 0;
					prefix.clear();
				}
				attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
				attrs.back()->SetName(name.toLatin1().data());
				attrs.back()->SetNumberOfComponents(1);
				attrs.back()->SetNumberOfTuples(ar.shape(0));
			}
			else {
				int index = name.lastIndexOf("_");
				QString after = name.mid(index + 1);
				bool ok;
				int val = after.toInt(&ok);
				if (!ok) {
					if (!prefix.isEmpty()) {
						attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
						attrs.back()->SetName(prefix.toLatin1().data());
						attrs.back()->SetNumberOfComponents(count);
						attrs.back()->SetNumberOfTuples(ar.shape(0));
						count = 0;
						prefix.clear();
					}
					attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
					attrs.back()->SetName(name.toLatin1().data());
					attrs.back()->SetNumberOfComponents(1);
					attrs.back()->SetNumberOfTuples(ar.shape(0));
					
				}
				else {
					if (prefix.isEmpty()) {
						prefix = name.mid(0, index);
						count = 1;
						if (prefix.isEmpty()) {
							attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
							attrs.back()->SetName(name.toLatin1().data());
							attrs.back()->SetNumberOfComponents(1);
							attrs.back()->SetNumberOfTuples(ar.shape(0));
							count = 0;
						}

					}
					else {
						if (prefix == name.mid(0, index))
							++count;
						else {
							attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
							attrs.back()->SetName(prefix.toLatin1().data());
							attrs.back()->SetNumberOfComponents(count);
							attrs.back()->SetNumberOfTuples(ar.shape(0));
							prefix = name.mid(0, index);
							count = 1;
						}
					}
				}
			}
		}
		if (!prefix.isEmpty()) {
			attrs.push_back(vtkSmartPointer<vtkDoubleArray>::New());
			attrs.back()->SetName(prefix.toLatin1().data());
			attrs.back()->SetNumberOfComponents(count);
			attrs.back()->SetNumberOfTuples(ar.shape(0));
		}

		// fill attributes
		
		for (int pt = 0; pt < ar.shape(0); ++pt) {
			int col = 0;
			for (int i = 0; i < attrs.size(); ++i) {
				for (int j = 0; j < attrs[i]->GetNumberOfComponents(); ++j, ++col) {
					attrs[i]->SetComponent(pt,j,ar(vipVector(pt,3 + col)));
				}
			}
		}


		VipVTKObject out;
		if (true) {
			vtkSmartPointer<vtkPolyData> data = vtkSmartPointer<vtkPolyData>::New();
			data->SetPoints(pts);
			for (int i = 0; i < attrs.size(); ++i)
				data->GetPointData()->SetScalars(attrs[i]);
			
			// Create the topology of the point(a vertex)
			vtkSmartPointer<vtkCellArray> vertices = vtkSmartPointer<vtkCellArray>::New();
			for (int i = 0; i < pts->GetNumberOfPoints(); ++i) {
				vertices->InsertNextCell(1);
				vertices->InsertCellPoint(i);
			}
			data->SetVerts(vertices);

			out = VipVTKObject(data);
			out.setDataName(data_name);
		}
		/* else {
			vtkSmartPointer<vtkPolyData> data = vtkSmartPointer<vtkPolyData>::New();
			data->SetPoints(pts);

			vtkSmartPointer<vtkDelaunay2D> delaunay = vtkSmartPointer<vtkDelaunay2D>::New();
			delaunay->SetInputData(data);
			delaunay->Update();
			data = delaunay->GetOutput();
			if (data) {
				for (int i = 0; i < attrs.size(); ++i)
					data->GetPointData()->SetScalars(attrs[i]);
				out = VipVTKObject(data, data_name);
			}
		}*/

		if (out) {
			d_data.setData(QVariant::fromValue(out));
			d_data.mergeAttributes(out.buildAllAttributes());
			d_data.setName(data_name);
			outputAt(0)->setData(d_data);
			setOpenMode(mode);
			return true;
		}
	}

	return false;
}

bool VipXYZValueFileReader::readData(qint64)
{
	if (!d_data.isEmpty()) {
		outputAt(0)->setData(d_data);
		return true;
	}
	return false;
}


VipFOVSequence::VipFOVSequence(QObject* parent)
  : VipTimeRangeBasedGenerator(parent)
{
	this->outputAt(0)->setData(QVariant::fromValue(VipFieldOfView()));
}


void VipFOVSequence::add(const VipFieldOfView& l)
{
	VipFieldOfView fov = l;
	fov.setViewUpZ();
	qint64 time = fov.time;

	if (m_fov_name.isEmpty())
		m_fov_name = fov.name;
	else
		fov.name = m_fov_name;

	auto it = std::lower_bound(m_fovs.begin(), m_fovs.end(), l.time, [](const auto& l, qint64 time) { return l.time < time; });
	if (it != m_fovs.end() && it->time == l.time)
		*it = l;
	else
		m_fovs.insert(it, l);
}

void VipFOVSequence::add(const VipFieldOfViewList& lst)
{
	for (const VipFieldOfView& f : lst)
		add(f);
	setFieldOfViews(fieldOfViews());
}

void VipFOVSequence::removeAt( int i)
{
	m_fovs.removeAt(i);
}

VipFieldOfView& VipFOVSequence::at( int i)
{
	return m_fovs[i];
}

const VipFieldOfView &VipFOVSequence::at(int i) const
{
	return m_fovs[i];
}

int VipFOVSequence::count() const
{
	return m_fovs.size();
}

void VipFOVSequence::clear()
{
	m_fovs.clear();
	setFieldOfViews(VipFieldOfViewList());
}

const VipFieldOfViewList &VipFOVSequence::fieldOfViews() const
{
	return m_fovs;
}

void VipFOVSequence::setFieldOfViews(const VipFieldOfViewList& lst)
{
	m_fovs.clear();

	QSet<QString> names;

	for (const VipFieldOfView& f : lst) {
		if (m_fov_name.isEmpty())
			if(!f.name.isEmpty()) names.insert(f.name);
		m_fovs.push_back(f);
		if (!m_fov_name.isEmpty())
			m_fovs.back().name = m_fov_name;
	}

	if (m_fov_name.isEmpty()) {
		if (names.size()) {
			m_fov_name = *names.begin();
			for (VipFieldOfView& f : m_fovs)
				f.name = m_fov_name;
		}
	}

	std::sort(m_fovs.begin(), m_fovs.end(), [](const auto& l, const auto& r) { return l.time < r.time; });
	auto it = std::unique(m_fovs.begin(), m_fovs.end());
	m_fovs.resize(it - m_fovs.begin());

	VipTimestamps times(m_fovs.size());
	for (int i = 0; i < m_fovs.size(); ++i)
		times[i] = m_fovs[i].time;

	this->setTimestamps(times);
}

void VipFOVSequence::setFovName(const QString& name)
{
	if (name.isEmpty())
		return;

	m_fov_name = name;
	for (VipFieldOfView& f : m_fovs)
		f.name = name;
}
QString VipFOVSequence::fovName() const
{
	return m_fov_name;
}


bool VipFOVSequence::open(VipIODevice::OpenModes mode)
{
	if (mode == VipIODevice::ReadOnly) {

		QString filename = removePrefix(path());
		if (!filename.isEmpty() && m_fovs.isEmpty()) {

			VipFieldOfViewList lst;
			if (mapFileSystem()) {
				if (QIODevice* dev = createDevice(removePrefix(path()), QIODevice::ReadOnly | QIODevice::Text))
					lst = vipLoadFieldOfViewsFromString(dev->readAll());
			}
			else
				lst = vipLoadFieldOfViews(filename);

			setFieldOfViews(lst);
		}
		else {
			auto tmp = m_fovs;
			setFieldOfViews(tmp);
		}

		setOpenMode(mode);
		readData(posToTime(0));
		return true;
	}
	return false;
}
	
VipFieldOfView VipFOVSequence::fovAtTime( qint64 time) const
{
	if (m_fovs.size() == 0)
		return VipFieldOfView();

	const VipFieldOfViewList* lst = &m_fovs;

	VipFieldOfView found = lst->first();

	if (time <= lst->first().time)
		found = lst->first();
	else if (time >= lst->last().time)
		found = lst->last();
	else {
		for (int i = 1; i < lst->size(); ++i) {
			if (time <= (*lst)[i].time) {
				double range((*lst)[i].time - (*lst)[i - 1].time);
				double f1 = 1.0 - (time - (*lst)[i - 1].time) / range;
				double f2 = 1.0 - ((*lst)[i].time - time) / range;
				const VipFieldOfView& fov1 = (*lst)[i - 1];
				const VipFieldOfView& fov2 = (*lst)[i];
				found = fov1;

				found.pupil[0] = fov1.pupil[0] * f1 + fov2.pupil[0] * f2;
				found.pupil[1] = fov1.pupil[1] * f1 + fov2.pupil[1] * f2;
				found.pupil[2] = fov1.pupil[2] * f1 + fov2.pupil[2] * f2;
				found.target[0] = fov1.target[0] * f1 + fov2.target[0] * f2;
				found.target[1] = fov1.target[1] * f1 + fov2.target[1] * f2;
				found.target[2] = fov1.target[2] * f1 + fov2.target[2] * f2;
				found.vertical_angle = fov1.vertical_angle * f1 + fov2.vertical_angle * f2;
				found.horizontal_angle = fov1.horizontal_angle * f1 + fov2.horizontal_angle * f2;

				// Get positive rotation angles between 0 and 360
				double a = fov1.rotation;
				while (a < 0)
					a += 360;
				if (a >= 360)
					a -= 360;
				double b = fov2.rotation;
				while (b < 0)
					b += 360;
				if (b >= 360)
					b -= 360;
				if (std::abs(a - b) > 180) {
					if (a > b)
						a -= 360;
					else
						b -= 360;
				}
				found.rotation = a * f1 + b * f2;

				break;
			}
		}
	}

	found.time = time;

	

	/* 
	found.computeAngles();
	found.roll += m_roll;
	found.yaw += m_yaw;
	found.pitch += m_pitch;
	if (m_fixed_attributes & Yaw)
		found.yaw = m_fixed_yaw;
	if (m_fixed_attributes & Roll)
		found.roll = m_fixed_roll;
	if (m_fixed_attributes & Pitch)
		found.pitch = m_fixed_pitch;

	found.setAngles(found.pitch, found.roll, found.yaw);*/
	return found;
}

bool VipFOVSequence::readData(qint64 time)
{
	if (m_fovs.size() == 0)
		return false;

	
	VipFieldOfView found = fovAtTime( time);
	VipAnyData any = create(QVariant::fromValue(found));
	any.setAttributes(found.attributes);
	any.setName(m_fov_name);
	this->outputAt(0)->setData(any);
	
	return true;
}


#include "VipArchive.h"

VipArchive& operator<<(VipArchive& arch, const VipFOVSequence* fov)
{
	return arch.content("fovs", fov->fieldOfViews());
}
VipArchive& operator>>(VipArchive& arch, VipFOVSequence* fov)
{
	VipFieldOfViewList fovs;
	arch.content("fovs", fovs);
	fov->setFieldOfViews(fovs);
	return arch;
}


static int register_Operators()
{
	vipRegisterArchiveStreamOperators<VipFieldOfView>();
	vipRegisterArchiveStreamOperators<VipFieldOfViewList>();
	vipRegisterArchiveStreamOperators<VipVTKObject>();
	vipRegisterArchiveStreamOperators<VipFOVSequence*>();
	vipRegisterMemoryFootprintFunction(qMetaTypeId<VipVTKObject>(), memoryFootprint);
	return 0;
}

static bool _register_Operators = vipAddInitializationFunction(register_Operators);





VipFieldOfViewList vipLoadFieldOfViewsFromString(const QString& str)
{
	VipXIStringArchive arch(str);
	VipFieldOfViewList res;
	arch.start("VipFieldOfView");
	arch >> res;
	return res;
}

VipFieldOfViewList vipLoadFieldOfViews(const QString& filename)
{
	VipXIfArchive arch(filename);
	VipFieldOfViewList res;
	arch.start("VipFieldOfView");
	arch >> res;
	return res;
}
bool vipSaveFieldOfViews(const VipFieldOfViewList& fovs, const QString& filename)
{
	VipXOfArchive arch(filename);
	arch.start("VipFieldOfView");
	arch << fovs;
	arch.end();
	return !arch.hasError();
}
QString vipSaveFieldOfViewsAsString(const VipFieldOfViewList& fovs)
{
	VipXOStringArchive arch;
	arch.start("VipFieldOfView");
	arch << fovs;
	arch.end();
	return arch.toString();
}

bool VipVTKFileWriter::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	if (!probe(path(), QByteArray()))
		return false;

	QString filename = removePrefix(path());
	QFile file(filename);
	if (!file.open(QFile::WriteOnly))
		return false;

	setOpenMode(mode);
	return true;
}

void VipVTKFileWriter::close()
{
	if (!d_data.isEmpty()) {
		if (VipVTKObject obj = d_data.value<VipVTKObject>()) {
			obj.save( removePrefix(path()));
		}
	}
	VipIODevice::close();
}

void VipVTKFileWriter::apply()
{
	while (inputAt(0)->hasNewData())
		d_data = inputAt(0)->data();
}

bool VipFOVFileWriter::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	if (!probe(path(), QByteArray()))
		return false;

	QString filename = removePrefix(path());
	QFile file(filename);
	if (!file.open(QFile::WriteOnly))
		return false;

	setOpenMode(mode);
	return true;
}

void VipFOVFileWriter::close()
{
	if (!m_fovs.isEmpty()) {
		vipSaveFieldOfViews(m_fovs, removePrefix(path()));
	}
	VipIODevice::close();
}

void VipFOVFileWriter::apply()
{
	while (inputAt(0)->hasNewData()) {
		const VipFieldOfView fov = inputAt(0)->data().value<VipFieldOfView>();
		if (!fov.isNull()) {
			m_fovs.append(fov);
		}
	}
}

#include "VipProgress.h"
#include <vtkDataSet.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkFieldData.h>
#include <vtkStringArray.h>

VipXYZAttributesWriter::VipXYZAttributesWriter(QObject* parent)
  : VipIODevice(parent)
{
}

bool VipXYZAttributesWriter::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	if (!probe(path(), QByteArray()))
		return false;

	QString filename = removePrefix(path());
	QFile file(filename);
	if (!file.open(QFile::WriteOnly))
		return false;

	setOpenMode(mode);
	return true;
}

void VipXYZAttributesWriter::close()
{
	VipIODevice::close();
}

void VipXYZAttributesWriter::setAttributeList(const QList<Attribute>& attrs)
{
	m_attributes = attrs;
}
const QList<VipXYZAttributesWriter::Attribute>& VipXYZAttributesWriter::attributeList()
{
	return m_attributes;
}

void VipXYZAttributesWriter::setFormat(Format f)
{
	m_format = f;
}
VipXYZAttributesWriter::Format VipXYZAttributesWriter::format() const
{
	return m_format;
}

void VipXYZAttributesWriter::apply()
{

	VipVTKObjectList lst;
	for (int i = 0; i < inputCount(); ++i) {
		VipVTKObject obj = inputAt(i)->data().value<VipVTKObject>();
		if (obj.dataSet())
			lst.push_back(obj);
	}

	QString filename = removePrefix(path());
	QFile out(filename);
	if (!out.open(QFile::WriteOnly | QFile::Text)) {
		setError("Cannot open file " + filename);
		return;
	}

	std::unique_ptr<VipProgress> display;
	if (property("_vip_progress").toBool()) {

		display.reset(new VipProgress());
		display->setText("Create file " + filename + " ...");
		display->setCancelable(true);
		display->setModal(true);
		display->setRange(0, lst.size());
	}

	bool is_csv = m_format == CSV;

	int progress = 0;
	QList<Attribute> attributes;
	// write csv header
	if (true) {
		QTextStream stream(&out);
		stream.setLocale(QLocale(QLocale::German));
		// write EXCEL separator
		if (is_csv)
			stream << "\"sep=\t\"\n";
		// write the header
		QStringList names;
		names << "X"
		      << "Y"
		      << "Z";
		for (const Attribute& a : m_attributes) {

			int components = 0;
			if (a.type == VipVTKObject::Point) {
				// check if valid for all objects
				for (const VipVTKObject& o : lst) {
					vtkAbstractArray* ar = o.pointsAttribute(a.name);
					if (!ar) {
						components = 0;
						break;
					}
					if (components == 0)
						components = ar->GetNumberOfComponents();
					else if (components != ar->GetNumberOfComponents()) {
						components = 0;
						break;
					}
				}
				if (components == 0)
					continue; // Pass this attribute

				attributes.push_back(a);
				if (components == 1)
					names << a.name;
				else {
					for (int i = 0; i < components; ++i)
						names << a.name + "_" + QString::number(i);
				}
			}
			else if (a.type == VipVTKObject::Field) {

				// check if valid for all objects
				for (const VipVTKObject& o : lst) {
					vtkVariantList l = o.fieldAttribute(a.name);
					if (l.isEmpty()) {
						components = 0;
						break;
					}
					if (components == 0)
						components = l.size();
					else if (components != l.size()) {
						components = 0;
						break;
					}
				}

				if (components == 0)
					continue; // Pass this attribute

				attributes.push_back(a);
				if (components == 1)
					names << a.name;
				else {
					for (int i = 0; i < components; ++i)
						names << a.name + "_" + QString::number(i);
				}
			}
		}

		if (is_csv) {
			stream << names.join("\t") << "\n";
			stream.flush();
		}
	}

	if (display) {
		std::int64_t points = 0;
		for (int i = 0; i < lst.size(); ++i) {
			vtkDataSet* set = lst[i].dataSet();
			points += set->GetNumberOfPoints();
		}
		display->setRange(0., static_cast<double>(points));
	}

	for (int i = 0; i < lst.size(); ++i) {
		vtkDataSet* set = lst[i].dataSet();

		// only works for vtkDataSet
		if (!set)
			continue;

		// store arrays
		QVector<vtkAbstractArray*> arrays;
		for (int a = 0; a < attributes.size(); ++a) {
			if (attributes[a].type == VipVTKObject::Point)
				arrays << set->GetPointData()->GetAbstractArray(attributes[a].name.toLatin1().data());
			else
				arrays << set->GetFieldData()->GetAbstractArray(attributes[a].name.toLatin1().data());
		}

		int num_points = set->GetNumberOfPoints();
		for (int p = 0; p < num_points; ++p, ++progress) {

			if (display) {
				if (progress % 5000 == 0) {
					if (display->canceled())
						return;
					display->setValue(progress);
				}
			}

			double point[3];
			set->GetPoint(p, point);

			if (vtkMath::IsNan(point[0]) || vtkMath::IsNan(point[1]) || vtkMath::IsNan(point[2]))
				continue;

			// create the XYZ values line
			QString line;
			QTextStream stream(&line, QIODevice::WriteOnly);

			stream << point[0] << "\t" << point[1] << "\t" << point[2];

			// save attributes

			bool has_nan = false;
			for (int a = 0; a < arrays.size(); ++a) {
				vtkAbstractArray* ar = arrays[a];
				int index = p;
				if (p >= ar->GetNumberOfTuples())
					index = 0;

				const char* name = ar->GetClassNameA();
				int cc = ar->GetNumberOfComponents();
				int tt = ar->GetNumberOfTuples();

				if (ar->IsNumeric()) {
					vtkDataArray* d = static_cast<vtkDataArray*>(ar);
					for (int c = 0; c < d->GetNumberOfComponents(); ++c) {
						double val = d->GetComponent(index, c);
						if (vtkMath::IsNan(val)) {
							has_nan = true;
							break;
						}

						stream << "\t" << d->GetComponent(index, c);
					}
				}
				else {
					// be aware that this code might crash in debug mode since the std::string implementation is different from the one
					// of vtk (compiled in release).
					vtkStringArray* d = static_cast<vtkStringArray*>(ar);
					stream << "\t" << d->GetPointer(index)->c_str();
				}
			}

			// write the line
			if (!has_nan) {

				if (is_csv)
					line.replace(".", ",");

				stream << "\n";
				stream.flush();
				out.write(line.toLatin1());
			}
		}
	}
}