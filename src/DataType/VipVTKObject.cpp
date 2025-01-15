/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipVTKObject.h"
#include "VipVTKImage.h"

#include <vtkAbstractArray.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCompositeDataSet.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDoubleArray.h>
#include <vtkHierarchicalBoxDataSet.h>
#include <vtkMath.h>
#include <vtkPointData.h>
#include <vtkRectilinearGrid.h>
#include <vtkStructuredGrid.h>
#include <vtkStructuredPoints.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPolyData.h>
#include <vtkAppendFilter.h>
#include <vtkCharArray.h>
#include <vtkCompositeDataIterator.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkProperty.h>
#include <vtkShortArray.h>
#include <vtkStringArray.h>
#include <vtkTable.h>
#include <vtkTree.h>
#include <vtkTypeInt64Array.h>
#include <vtkUnsignedIntArray.h>
#include <vtkUnsignedLongLongArray.h>
#include <vtkUnsignedShortArray.h>
#include <vtkGenericDataObjectReader.h>
#include <vtkXMLGenericDataObjectReader.h>
#include <vtkSTLWriter.h>
#include <vtkGenericDataObjectWriter.h>
#include <vtkXMLRectilinearGridWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLStructuredGridWriter.h>
#include <vtkXMLRectilinearGridReader.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLStructuredGridReader.h>
#include <vtkSTLReader.h>
#include <vtkDecimatePro.h>
#include <vtkCallbackCommand.h>

#include <QDir>
#include <QDateTime>
#include <QTemporaryFile>

#include <sstream>


namespace detail
{

	static QRecursiveMutex _vtk_objects_mutex;

	struct VTKObjectList
	{
		QList<vtkObject*> objects;
		QList<int> lines;
		QStringList files;

		void add(vtkObject* o, const QString& file, int line)
		{
			QMutexLocker lock(&_vtk_objects_mutex);
			objects.append(o);
			files.append(file);
			lines.append(line);
		}

		void remove(vtkObject* o)
		{
			QMutexLocker lock(&_vtk_objects_mutex);
			int index = objects.indexOf(o);
			if (index >= 0) {
				objects.removeAt(index);
				files.removeAt(index);
				lines.removeAt(index);
			}
		}

		~VTKObjectList()
		{
			for (int i = 0; i < objects.size(); ++i) {
				printf("leak object %s at address %lld, file %s, line %d\n", objects[i]->GetClassName(), (qint64)objects[i], files[i].toLatin1().data(), lines[i]);
				/*if (objects[i]->IsA("vtkDataSet"))
				{
					vtkPointData * pd = static_cast<vtkDataSet*>(objects[i])->GetPointData();
					for (int a = 0; a < pd->GetNumberOfArrays(); ++a)
						printf("\tHas array: %s\n", pd->GetAbstractArray(a)->GetName());
				}
				fflush(stdout);*/
			}
			// bool stop = true;
		}
	};
	static VTKObjectList list;

	static void func(vtkObject* object, unsigned long, void*, void*)
	{
		QMutexLocker lock(&_vtk_objects_mutex);
		int index = list.objects.indexOf(object);
		if (index >= 0) {
			int line = list.lines[index];
			QString file = list.files[index];
			list.remove(object);
			printf("delete object %s at address %lld, file %s, line %d, remaining = %i\n", object->GetClassName(), (qint64)object, file.toLatin1().data(), line, (int)list.objects.size());
		}
	}

	static vtkSmartPointer<vtkCallbackCommand> _callback;
	static void initialize()
	{
		static bool init = false;
		if (!init) {
			_callback = vtkSmartPointer<vtkCallbackCommand>::New();
			_callback->SetCallback(func);
			init = true;
		}
	}

	void AddObjectObserver(vtkObject* object, const char* file, int line)
	{
		if (object) {
			QMutexLocker lock(&_vtk_objects_mutex);
			initialize();
			if (list.objects.indexOf(object) < 0) {
				// print create object
				printf("create object %s at address %lld, objects = %i\n", object->GetClassName(), (qint64)object, (int)list.objects.size());
				object->AddObserver(vtkCommand::DeleteEvent, _callback);
				list.add(object, file, line);
			}
		}
	}

}


/* static vtkMapper* MergeComposite(vtkCompositeDataSet* composite)
{
	vtkDataSetMapper* mapper = vtkDataSetMapper::New();
	vtkSmartPointer<vtkAppendFilter> filter = vtkSmartPointer<vtkAppendFilter>::New();

	vtkCompositeDataIterator* iter = composite->NewIterator();
	for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem()) {
		vtkDataSet* inputDS = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
		filter->AddInputData(inputDS);
	}
	iter->Delete();

	filter->Update();
	mapper->SetInputData(filter->GetInput());
	return mapper;
}

static vtkUnstructuredGrid* mergeCompositeInput(vtkCompositeDataSet* composite)
{
	vtkSmartPointer<vtkAppendFilter> filter = vtkSmartPointer<vtkAppendFilter>::New();

	vtkCompositeDataIterator* iter = composite->NewIterator();
	for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem()) {
		vtkDataSet* inputDS = vtkDataSet::SafeDownCast(iter->GetCurrentDataObject());
		filter->AddInputData(inputDS);
	}
	iter->Delete();

	filter->Update();
	vtkUnstructuredGrid* res = filter->GetOutput();
	res->Register(res);
	return res;
}*/




class VipVTKObject::PrivateData
{
public:

	vtkSmartPointer<vtkDataObject> data;
	
	qint64 cadMTime;

	// vtkSmartPointer<vtkDecimatePro > simplifiedData;
	bool simplified;
	QVector<QPair<double, double>> ranges;

	//QList<DynamicProperty> dynProperties;

	QRecursiveMutex mutex;

	PrivateData();
	~PrivateData();
	bool isValid() const;
};



VipVTKObject::PrivateData::PrivateData()
  : cadMTime(0)
  , simplified(false)
  , mutex()
{
}

VipVTKObject::PrivateData::~PrivateData()
{
	{
		QMutexLocker lock(&mutex);
		data = vtkSmartPointer<vtkDataObject>();
	}
}

bool VipVTKObject::PrivateData::isValid() const
{
	return data.GetPointer() != nullptr;
}



VipVTKObject::VipVTKObject(const SharedPointer& ptr) noexcept
  : d_data(ptr)
{
}

VipVTKObject::VipVTKObject()
{
	d_data.reset(new PrivateData());

	setObject(nullptr);
	
	// d_data->simplifiedData = vtkSmartPointer<vtkDecimatePro >::New();

	/*d_data->simplifiedData->SetTargetReduction(0.99);
	d_data->simplifiedData->SetPreserveTopology(0);
	//d_data->simplifiedData->SetSplitting (1);
	d_data->simplifiedData->SetBoundaryVertexDeletion(1);
	d_data->simplifiedData->SetMaximumError (VTK_DOUBLE_MAX);*/
}

VipVTKObject::VipVTKObject(vtkDataObject* object, const QString& name)
{
	d_data.reset(new PrivateData());
	
	// d_data->simplifiedData = vtkSmartPointer<vtkDecimatePro >::New();

	setObject(object);
	if (object && !name.isEmpty())
		object->SetObjectName(name.toLatin1().data());
	/*d_data->simplifiedData->SetTargetReduction(0.99);
	d_data->simplifiedData->SetPreserveTopology(0);
	//d_data->simplifiedData->SetSplitting (1);
	d_data->simplifiedData->SetBoundaryVertexDeletion(1);
	d_data->simplifiedData->SetMaximumError (VTK_DOUBLE_MAX);*/
}

VipVTKObject::~VipVTKObject()
{
}

QStringList VipVTKObject::supportedFileSuffix() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (!d_data->data)
		return QStringList();

	if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkPolyData"))
		return QStringList() << "stl"
				     << "vtk"
				     << "vtp";
	else if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkRectilinearGrid"))
		return QStringList() << "vtk"
				     << "vtr";
	else if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkStructuredGrid"))
		return QStringList() << "vtk"
				     << "vts";
	else if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkUnstructuredGrid"))
		return QStringList() << "vtk"
				     << "vtu";

	return QStringList() << "vtk";
}


QString VipVTKObject::preferredSuffix() const
{
	if (!d_data->data)
		return QString();

	if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkPolyData"))
		return "vtp";
	else if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkRectilinearGrid"))
		return "vtr";
	else if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkStructuredGrid"))
		return "vts";
	else if (const_cast<vtkDataObject*>(d_data->data.GetPointer())->IsA("vtkUnstructuredGrid"))
		return "vtu";

	return "vtk";
}

vtkDataObject* VipVTKObject::setObject(vtkDataObject* obj)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (obj == d_data->data)
		return obj;

	
	
	if (obj) {
		VIP_VTK_OBSERVER(obj);

		/* if (obj.IsA("vtkCompositeDataSet") || obj.IsA("vtkHierarchicalBoxDataSet") || obj.IsA("vtkMultiBlockDataSet")) {
			obj = mergeCompositeInput(static_cast<vtkCompositeDataSet*>(obj));
			d_data->data = vtkSmartPointer<vtkDataObject>(obj);
			VIP_VTK_OBSERVER(d_data->data);
			obj.Delete();
		}
		else*/
			d_data->data = vtkSmartPointer<vtkDataObject>(obj);
	}

	return obj;
}

/* bool VipVTKObject::convertToPolydata()
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));

	if (this->d_data->data->IsA("vtkPolyData"))
		return true;

	vtkSmartPointer<vtkDataSetSurfaceFilter> filter = vtkSmartPointer<vtkDataSetSurfaceFilter>::New();
	filter->SetInputData(this->d_data->data);
	filter->Update();

	if (filter->GetOutput()) {
		this->setObject(filter->GetOutput());
		return true;
	}
	return false;
}*/


void VipVTKObject::modified()
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));
	if (d_data->data) {
		d_data->data->Modified();
		d_data->cadMTime = d_data->data->GetMTime();

		if (vtkDataSet* set = dataSet()) {
			set->GetPointData()->Modified();
			set->GetCellData()->Modified();
			if (vtkPointSet* pts = pointSet()) {
				pts->GetPoints()->Modified();
			}
		}
	}
	else
		d_data->cadMTime = 0;
}

void VipVTKObject::clear()
{
	d_data->cadMTime = 0;
	d_data->data = vtkSmartPointer<vtkDataObject>();
	//d_data->dynProperties.clear();
	d_data->ranges.clear();
	d_data->simplified = false;
}

QVariantMap VipVTKObject::buildAllAttributes() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QVariantMap attrs;
	
	if (d_data->data) {
		attrs["Name"] = QFileInfo(d_data->data->GetObjectName().c_str()).fileName();
	}

	vtkDataObject* data = d_data->data.GetPointer();
	if (data) {
		attrs["Class"] = QString(data->GetClassName());
		if (data && data->IsA("vtkDataSet")) {
			vtkDataSet* set = static_cast<vtkDataSet*>(data);
			attrs["Point count"] = QString::number(set->GetNumberOfPoints());
			attrs["Cell count"] = QString::number(set->GetNumberOfCells());

			vtkPointData* pdata = set->GetPointData();
			for (int i = 0; i < pdata->GetNumberOfArrays(); ++i) {
				vtkDataArray* ar = pdata->GetArray(i);
				QString value = QString(ar->GetClassName()) + " (" + QString::number(ar->GetNumberOfTuples()) + ", " + QString::number(ar->GetNumberOfComponents()) + ")";
				attrs["PointData/" + QString(ar->GetName())] = value;
			}
			vtkCellData* cdata = set->GetCellData();
			for (int i = 0; i < cdata->GetNumberOfArrays(); ++i) {
				vtkDataArray* ar = cdata->GetArray(i);
				QString value = QString(ar->GetClassName()) + " (" + QString::number(ar->GetNumberOfTuples()) + ", " + QString::number(ar->GetNumberOfComponents()) + ")";
				attrs["CellData/" + QString(ar->GetName())] = value;
			}
		}
		QMap<QString, vtkVariantList> fattrs = fieldAttributes();
		for (auto it = fattrs.begin(); it != fattrs.end(); ++it) {
			const vtkVariantList& lst = it.value();
			QStringList values;
			for (const vtkVariant& v : lst)
				values.append(v.ToString().c_str());
			attrs["FieldData/" + it.key()] = values.join(" ");
		}
	}
	return attrs;
}
/*
void VipVTKObject::SetSimplified(bool s)
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));

	if (polyData() && d_data->simplified != s)
	{
		d_data->simplified = s;
		if (s)
		{
			if (d_data->mapper && d_data->simplifiedData->GetInput())
			{
				static_cast<vtkPolyDataMapper*>(d_data->mapper.GetPointer())->SetInputData(d_data->simplifiedData->GetOutput());
			}
		}
		else
		{
			if (d_data->mapper && d_data->simplifiedData->GetInput())
			{
				static_cast<vtkPolyDataMapper*>(d_data->mapper.GetPointer())->SetInputData(polyData());
			}
		}
	}
}

bool VipVTKObject::IsSimplified() const
{
	return d_data->simplified;
}

void VipVTKObject::UpdateSimplified()
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));

	if (d_data->simplifiedData && d_data->simplifiedData->GetInput())
	{
		d_data->simplifiedData->Modified();
		d_data->simplifiedData->Update();
	}
}

vtkPolyData * VipVTKObject::Simplified() const
{
	//if (d_data->simplifiedData)
	//	return const_cast<vtkDecimatePro*>(d_data->simplifiedData.GetPointer())->GetOutput();
	return nullptr;
}
*/



QRecursiveMutex* VipVTKObject::mutex() const
{
	return const_cast<QRecursiveMutex*>(&d_data->mutex);
}


QString VipVTKObject::description(int pointId, int cellId) const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QStringList text;

	if (d_data->data)
		text << QString("<b>Name: </b>") + QFileInfo(d_data->data->GetObjectName().c_str()).fileName();

	if (d_data->data && d_data->data->IsA("vtkDataSet")) {
		text << "<b>Point count: </b>" + QString::number(static_cast<vtkDataSet*>(d_data->data.GetPointer())->GetNumberOfPoints());
		text << "<b>Cell count: </b>" + QString::number(static_cast<vtkDataSet*>(d_data->data.GetPointer())->GetNumberOfCells());
	}

	// field attributes
	{
		QVector<vtkAbstractArray*> arrays = this->fieldAttributeArrays();
		for (int i = 0; i < arrays.size(); ++i) {
			vtkVariantList lst = makeAttribute(arrays[i], 0).second;
			QStringList values;
			for (int j = 0; j < lst.size(); ++j) {
				std::ostringstream oss;
				oss << lst[j];
				values << oss.str().c_str();
			}
			text << "<b>" + QString(arrays[i]->GetName()) + ": </b>" + values.join(", ");
		}
	}

	if (d_data->data && d_data->data->IsA("vtkDataSet")) {
		// point attributes
		if (pointId >= 0) {
			QVector<vtkAbstractArray*> arrays = this->pointsAttributes();
			for (int i = 0; i < arrays.size(); ++i) {
				vtkVariantList lst = makeAttribute(arrays[i], pointId).second;
				QStringList values;
				for (int j = 0; j < lst.size(); ++j) {
					std::ostringstream oss;
					oss << lst[j];
					values << oss.str().c_str();
				}
				text << "<b>" + QString(arrays[i]->GetName()) + ": </b>" + values.join(", ");
			}
		}

		// point attributes
		if (cellId >= 0) {
			QVector<vtkAbstractArray*> arrays = this->cellsAttributes();
			for (int i = 0; i < arrays.size(); ++i) {
				vtkVariantList lst = makeAttribute(arrays[i], pointId).second;
				QStringList values;
				for (int j = 0; j < lst.size(); ++j) {
					std::ostringstream oss;
					oss << lst[j];
					values << oss.str().c_str();
				}
				text << "<b>" + QString(arrays[i]->GetName()) + ": </b>" + values.join(", ");
			}
		}
	}

	QString res = QString("<div align='left'>") + text.join("<br>") + QString("</div>");
	return res;
}

QString VipVTKObject::dataName() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));
	return d_data->data ? QString(d_data->data->GetObjectName().c_str()) : QString();
}

void VipVTKObject::setDataName(const QString& name)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));
	if (d_data->data)
		d_data->data->SetObjectName(name.toLatin1().data());
}

bool VipVTKObject::isA(const char* class_name) const
{
	if (d_data->data)
		return d_data->data->IsA(class_name);
	return false;
}

vtkSmartPointer<vtkAbstractArray> VipVTKObject::makeArray(const QString& name, const vtkVariantList& values, int size, vtkAbstractArray* ar)
{

	if (!values.size() || !size)
		return nullptr;

	vtkVariant value = values[0];
	vtkAbstractArray* array = ar;
	vtkSmartPointer<vtkAbstractArray> res;

	if (value.IsChar()) {
		if (!(array && array->GetDataType() == VTK_CHAR && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkCharArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkCharArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsUnsignedChar()) {
		if (!(array && array->GetDataType() == VTK_UNSIGNED_CHAR && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkUnsignedCharArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkUnsignedCharArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsShort()) {
		if (!(array && array->GetDataType() == VTK_SHORT && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkShortArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkShortArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsUnsignedShort()) {
		if (!(array && array->GetDataType() == VTK_UNSIGNED_SHORT && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkUnsignedShortArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkUnsignedShortArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsInt()) {
		if (!(array && array->GetDataType() == VTK_INT && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkIntArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkDataArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsUnsignedInt()) {
		if (!(array && array->GetDataType() == VTK_UNSIGNED_INT && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkUnsignedIntArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkDataArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsLongLong()) {
		if (!(array && array->GetDataType() == VTK_LONG_LONG && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkLongLongArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkDataArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsUnsignedLongLong()) {
		if (!(array && array->GetDataType() == VTK_UNSIGNED_LONG_LONG && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkUnsignedLongLongArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkDataArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsDouble() || value.IsFloat()) {
		if (!(array && array->GetDataType() == VTK_DOUBLE && array->GetNumberOfTuples() == size && array->GetNumberOfComponents() == values.size())) {
			array = vtkDoubleArray::New();
			VIP_VTK_OBSERVER(array);
			array->SetNumberOfComponents(values.size());
			array->SetNumberOfTuples(size);
			res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		}
		else
			res = vtkSmartPointer<vtkAbstractArray>(array);
		for (int i = 0; i < values.size(); ++i)
			static_cast<vtkDataArray*>(array)->FillComponent(i, values[i].ToDouble());
	}
	else if (value.IsString()) {
		array = vtkStringArray::New();
		VIP_VTK_OBSERVER(array);
		array->SetNumberOfComponents(1);
		array->SetNumberOfTuples(size);
		res = vtkSmartPointer<vtkAbstractArray>::Take(array);
		for (int i = 0; i < size; ++i)
			array->SetVariantValue(i, value);
		// static_cast<vtkStringArray*>(array)->SetValue(i, value.ToString().c_str());
	}

	if (array) {
		array->SetName(name.toLatin1().data());
		array->Modified();
	}

	return res;
}

QPair<QString, vtkVariantList> VipVTKObject::makeAttribute(vtkAbstractArray* array, int index)
{

	QString name = array->GetName();
	vtkVariantList values;
	vtkVariant v = array->GetVariantValue(index);

	if (v.IsString())
		values << v;
	else if (v.IsFloat()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << data->GetComponent(index, i);
	}
	else if (v.IsDouble()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << data->GetComponent(index, i);
	}
	else if (v.IsChar()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (int)data->GetComponent(index, i);
	}
	else if (v.IsUnsignedChar()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (int)data->GetComponent(index, i);
	}
	else if (v.IsSignedChar()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (int)data->GetComponent(index, i);
	}
	else if (v.IsShort()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (int)data->GetComponent(index, i);
	}
	else if (v.IsUnsignedShort()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (int)data->GetComponent(index, i);
	}
	else if (v.IsInt()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (int)data->GetComponent(index, i);
	}
	else if (v.IsUnsignedInt()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (quint32)data->GetComponent(index, i);
	}
	else if (v.IsLong()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (qint64)data->GetComponent(index, i);
	}
	else if (v.IsUnsignedLong()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (quint64)data->GetComponent(index, i);
	}
	else if (v.IsLongLong()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (qint64)data->GetComponent(index, i);
	}
	else if (v.IsUnsignedLongLong()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (quint64)data->GetComponent(index, i);
	}
	else if (v.IsLongLong()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (qint64)data->GetComponent(index, i);
	}
	else if (v.IsUnsignedLongLong()) {
		vtkDataArray* data = static_cast<vtkDataArray*>(array);
		for (int i = 0; i < data->GetNumberOfComponents(); ++i)
			values << (quint64)data->GetComponent(index, i);
	}

	return QPair<QString, vtkVariantList>(name, values);
}

void VipVTKObject::setFieldAttributes(const QMap<QString, vtkVariantList>& attr)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));
	if (d_data->data) {
		for (QMap<QString, vtkVariantList>::const_iterator it = attr.begin(); it != attr.end(); ++it) {
			setFieldAttribute(it.key(), it.value());
		}
	}
}

void VipVTKObject::setFieldAttribute(const QString& name, const vtkVariantList& values)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (d_data->data) {
		vtkFieldData* field = d_data->data->GetFieldData();
		vtkSmartPointer<vtkAbstractArray> array = makeArray(name, values, 1, field->GetAbstractArray(name.toLatin1().data()));

		if (array) {
			if (array.GetPointer() != field->GetAbstractArray(name.toLatin1().data())) {
				array->Register(0);
				field->RemoveArray(name.toLatin1().data());
				field->AddArray(array);
			}
		}
	}
}

QMap<QString, vtkVariantList> VipVTKObject::fieldAttributes() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QMap<QString, vtkVariantList> res;
	if (d_data->data) {
		vtkFieldData* field = d_data->data->GetFieldData();
		for (int i = 0; i < field->GetNumberOfArrays(); ++i) {
			vtkAbstractArray* array = field->GetAbstractArray(i);
			QPair<QString, vtkVariantList> pair = makeAttribute(array, 0);
			res.insert(pair.first, pair.second);
		}
	}
	return res;
}

vtkVariantList VipVTKObject::fieldAttribute(const QString& name) const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (d_data->data) {
		vtkFieldData* field = d_data->data->GetFieldData();
		vtkAbstractArray* array = field->GetAbstractArray(name.toLatin1().data());
		if (array)
			return makeAttribute(array, 0).second;
	}

	return vtkVariantList();
}

vtkAbstractArray* VipVTKObject::fieldAttributeArray(const QString& name) const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (d_data->data) {
		vtkFieldData* field = d_data->data->GetFieldData();
		return field->GetAbstractArray(name.toLatin1().data());
	}

	return nullptr;
}

QVector<vtkAbstractArray*> VipVTKObject::fieldAttributeArrays() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QVector<vtkAbstractArray*> res;
	if (d_data->data) {
		vtkFieldData* field = d_data->data->GetFieldData();
		for (int i = 0; i < field->GetNumberOfArrays(); ++i)
			res << field->GetAbstractArray(i);
	}
	return res;
}

QStringList VipVTKObject::fieldAttributesNames() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QStringList res;

	if (d_data->data) {
		vtkFieldData* field = d_data->data->GetFieldData();
		for (int i = 0; i < field->GetNumberOfArrays(); ++i)
			res << field->GetAbstractArray(i)->GetName();
	}

	return res;
}

void VipVTKObject::importData(const VipVTKObject& other) 
{
	auto locks = vipLockVTKObjects(VipVTKObjectList() << *this << other);
	if (other.d_data->data && other.d_data->data != d_data->data) {
		vtkDataObject* obj = other.d_data->data->NewInstance();
		obj->DeepCopy(other.d_data->data);

		if (d_data->data) {
			// Reset active scalars to avoid display flickering
			if (d_data->data->IsA("vtkDataSet")) {
				vtkDataSet* set = static_cast<vtkDataSet*>(d_data->data.GetPointer());
				vtkDataSet* obj_set = static_cast<vtkDataSet*>(obj);
				if (vtkDataArray* ar = set->GetPointData()->GetScalars())
					obj_set->GetPointData()->SetActiveScalars(ar->GetName());
				if (vtkDataArray* ar = set->GetCellData()->GetScalars())
					obj_set->GetCellData()->SetActiveScalars(ar->GetName());
			}
		}

		this->setObject(obj);
		obj->Delete();

		d_data->cadMTime = 0;
		d_data->ranges = other.d_data->ranges;
		//for (int i = 0; i < other.d_data->dynProperties.size(); ++i)
		//	d_data->dynProperties.push_back(other.d_data->dynProperties[i].copy());
	}
}

VipVTKObject VipVTKObject::copy() const
{
	VipVTKObject res;
	res.importData(*this);
	return res;
}

QVector<vtkAbstractArray*> VipVTKObject::pointsAttributes() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QVector<vtkAbstractArray*> res;

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkPointData* in_point = in->GetPointData();

		for (int i = 0; i < in_point->GetNumberOfArrays(); ++i) {
			vtkAbstractArray* array = in_point->GetAbstractArray(i);
			res << array;
		}
	}

	return res;
}

vtkAbstractArray* VipVTKObject::pointsAttribute(const QString& name) const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkPointData* in_point = in->GetPointData();
		return in_point->GetAbstractArray(name.toLatin1().data());
	}

	return nullptr;
}

QStringList VipVTKObject::pointsAttributesName() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QStringList res;
	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkPointData* in_point = in->GetPointData();

		for (int i = 0; i < in_point->GetNumberOfArrays(); ++i)
			if (in_point->GetArray(i))
				res << in_point->GetArray(i)->GetName();
	}
	return res;
}

vtkAbstractArray* VipVTKObject::setPointsAttribute(const QString& name, const vtkVariantList& default_components)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkPointData* in_point = in->GetPointData();
		vtkSmartPointer<vtkAbstractArray> array = makeArray(name, default_components, in->GetNumberOfPoints(), in_point->GetAbstractArray(name.toLatin1().data()));
		VIP_VTK_OBSERVER(array);
		if (array && array != in_point->GetAbstractArray(name.toLatin1().data())) {
			in_point->RemoveArray(name.toLatin1().data());
			in_point->AddArray(array);
		}
		return array;
	}

	return nullptr;
}

vtkDataArray* VipVTKObject::setPointsAttribute(const QString& name, const vtkVariantList& components_1, int pointId1, const vtkVariantList& components_2, int pointId2, QVector<int> interpolation_axes)
{
	if (pointId1 == pointId2)
		return nullptr;

	vtkDataSet* in = dataSet();
	if (!in)
		return nullptr;

	if (pointId1 < 0 || pointId1 >= in->GetNumberOfPoints())
		return nullptr;
	if (pointId2 < 0 || pointId2 >= in->GetNumberOfPoints())
		return nullptr;

	// interpolate components_1 and components_2
	double pt1[3], pt2[3];
	in->GetPoint(pointId1, pt1);
	in->GetPoint(pointId2, pt2);

	return setPointsAttribute(name, components_1, pt1, components_2, pt2, interpolation_axes);
}

vtkDataArray* VipVTKObject::setPointsAttribute(const QString& name, const vtkVariantList& components_1, double* pt1, const vtkVariantList& components_2, double* pt2, QVector<int> interpolation_axes)
{
	if (name.isEmpty())
		return nullptr;
	if (components_1.size() != components_2.size())
		return nullptr;
	if (components_1.size() == 0)
		return nullptr;
	if (memcmp(pt1, pt2, sizeof(double) * 3) == 0)
		return nullptr;
	if (interpolation_axes.size() == 0) {
		interpolation_axes.resize(components_1.size());
		for (int i = 0; i < interpolation_axes.size(); ++i)
			interpolation_axes[i] = i;
	}

	// convert components to double
	vtkVariantList c_1, c_2;
	for (int i = 0; i < components_1.size(); ++i) {
		c_1.append(components_1[i].ToDouble());
		c_2.append(components_2[i].ToDouble());
	}

	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkPointData* in_point = in->GetPointData();

		vtkSmartPointer<vtkAbstractArray> array = makeArray(name, c_1, in->GetNumberOfPoints(), in_point->GetArray(name.toLatin1().data()));
		VIP_VTK_OBSERVER(array);
		if (array && array.GetPointer() != in_point->GetArray(name.toLatin1().data())) {
			in_point->RemoveArray(name.toLatin1().data());
			in_point->AddArray(array);
			// array->Delete();
		}

		vtkDataArray* dar = static_cast<vtkDataArray*>(array.GetPointer());

		// compute the affine transform for each component
		QVector<double> factor(c_1.size(), 0);
		QVector<double> offset(c_1.size(), 0);
		for (int c = 0; c < c_1.size(); ++c) {
			const int axis = interpolation_axes[c];
			const double v_c1 = c_1[c].ToDouble();
			const double v_c2 = c_2[c].ToDouble();
			double x1 = pt1[axis];
			double x2 = pt2[axis];
			offset[c] = v_c1 - (x1 / (x2 - x1)) * (v_c2 - v_c1);
			factor[c] = (v_c2 - v_c1) / (x2 - x1);
		}

		// compute interpolation for each components
		for (int i = 0; i < in->GetNumberOfPoints(); ++i) {
			double pt[3];
			in->GetPoint(i, pt);

			// for each component
			for (int c = 0; c < components_1.size(); ++c) {
				const int axis = interpolation_axes[c];
				const double value = pt[axis];
				const double f = factor[c];
				const double o = offset[c];
				const double component = value * f + o;
				dar->SetComponent(i, c, component);
			}
		}
		return dar;
	}

	return nullptr;
}

QVector<vtkAbstractArray*> VipVTKObject::cellsAttributes() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QVector<vtkAbstractArray*> res;

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkCellData* in_point = in->GetCellData();

		for (int i = 0; i < in_point->GetNumberOfArrays(); ++i) {
			vtkAbstractArray* array = in_point->GetAbstractArray(i);
			res << array;
		}
	}

	return res;
}

vtkAbstractArray* VipVTKObject::cellsAttribute(const QString& name) const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkCellData* in_point = in->GetCellData();
		return in_point->GetAbstractArray(name.toLatin1().data());
	}

	return nullptr;
}

QStringList VipVTKObject::cellsAttributesName() const
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	QStringList res;
	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkCellData* in_point = in->GetCellData();

		for (int i = 0; i < in_point->GetNumberOfArrays(); ++i)
			if (in_point->GetArray(i))
				res << in_point->GetArray(i)->GetName();
	}
	return res;
}

vtkAbstractArray* VipVTKObject::setCellsAttribute(const QString& name, const vtkVariantList& default_components)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	if (this->d_data->data && this->d_data->data->IsA("vtkDataSet")) {
		vtkDataSet* in = const_cast<vtkDataSet*>(static_cast<const vtkDataSet*>(this->d_data->data.GetPointer()));
		vtkCellData* in_point = in->GetCellData();
		vtkSmartPointer<vtkAbstractArray> array = makeArray(name, default_components, in->GetNumberOfPoints(), in_point->GetAbstractArray(name.toLatin1().data()));
		VIP_VTK_OBSERVER(array);
		if (array && array.GetPointer() != in_point->GetAbstractArray(name.toLatin1().data())) {
			in_point->RemoveArray(name.toLatin1().data());
			in_point->AddArray(array);
			// array->Delete();
		}
		return array;
	}

	return nullptr;
}

QVector<vtkAbstractArray*> VipVTKObject::attributes(AttributeType t) const
{
	switch (t) {
		case Field:
			return fieldAttributeArrays();
		case Point:
			return pointsAttributes();
		case Cell:
			return cellsAttributes();
		default:
			return QVector<vtkAbstractArray*>();
	}
}

vtkAbstractArray* VipVTKObject::attribute(AttributeType t, const QString& name) const
{
	switch (t) {
		case Field:
			return fieldAttributeArray(name);
		case Point:
			return pointsAttribute(name);
		case Cell:
			return cellsAttribute(name);
		default:
			return nullptr;
	}
}

QStringList VipVTKObject::attributesName(AttributeType t) const
{
	switch (t) {
		case Field:
			return fieldAttributesNames();
		case Point:
			return pointsAttributesName();
		case Cell:
			return cellsAttributesName();
		default:
			return QStringList();
	}
}

bool VipVTKObject::hasAttribute(AttributeType t, const QString& name) const
{
	return attributesName(t).indexOf(name) >= 0;
}

bool VipVTKObject::removeAttribute(AttributeType t, const QString& name)
{
	QMutexLocker lock(const_cast<QRecursiveMutex*>(&d_data->mutex));

	vtkAbstractArray* array = attribute(t, name);
	if (!array || t == Unknown)
		return false;

	if (t == Field) {
		d_data->data->GetFieldData()->RemoveArray(name.toLatin1().data());
	}
	else if (t == Cell) {
		dataSet()->GetCellData()->RemoveArray(name.toLatin1().data());
	}
	else if (t == Point) {
		dataSet()->GetPointData()->RemoveArray(name.toLatin1().data());
	}

	return true;
}

bool VipVTKObject::isColorAttribute(VipVTKObject::AttributeType t, const QString& name) const
{
	vtkAbstractArray* array = attribute(t, name);
	if (!array)
		return false;

	if (array->IsA("vtkDataArray")) {
		vtkDataArray* darray = static_cast<vtkDataArray*>(array);
		int c = darray->GetNumberOfComponents();
		int data_type = darray->GetDataType();

		// 3 or 4 components unsigned char can be considered as d_data->color
		if ((c == 3 || c == 4) && data_type == VTK_UNSIGNED_CHAR)
			return true;
		// 3 or 4 components double or float can be considered as d_data->color if values are in between 0 and 1
		else if ((c == 3 || c == 4) && (data_type == VTK_DOUBLE || data_type == VTK_FLOAT)) {
			for (int i = 0; i < c; ++i) {
				double range[2];
				darray->GetRange(range, i);
				if (range[0] < 0 || range[1] > 1)
					return false;
			}
			return true;
		}
		else
			return false;
	}

	return false;
}

bool VipVTKObject::importAttributes( const VipVTKObject& other)
{
	if (other == *this)
		return true;

	auto locks = vipLockVTKObjects({ *this, other });

	if (!dataSet() || !other.dataSet())
		return false;

	if (dataSet()->GetNumberOfPoints() != other.dataSet()->GetNumberOfPoints())
		return false;

	if (dataSet()->GetNumberOfCells() != other.dataSet()->GetNumberOfCells())
		return false;

	// copy field attributes
	this->setFieldAttributes(other.fieldAttributes());

	// copy point attributes
	QVector<vtkAbstractArray*> pts = other.pointsAttributes();
	for (int i = 0; i < pts.size(); ++i) {
		vtkAbstractArray* ar = pts[i];
		vtkAbstractArray* tmp = ar->NewInstance();
		tmp->SetName(ar->GetName());
		tmp->DeepCopy(ar);
		dataSet()->GetPointData()->AddArray(tmp);
		tmp->Delete();
	}

	// copy cells attributes
	QVector<vtkAbstractArray*> cells = other.pointsAttributes();
	for (int i = 0; i < cells.size(); ++i) {
		vtkAbstractArray* ar = cells[i];
		vtkAbstractArray* tmp = ar->NewInstance();
		tmp->SetName(ar->GetName());
		tmp->DeepCopy(ar);
		dataSet()->GetCellData()->AddArray(tmp);
		tmp->Delete();
	}

	return true;
}
/*
void VipVTKObject::addDynamicProperty(const DynamicProperty& prop)
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));

	for (int i = 0; i < d_data->dynProperties.size(); ++i) {
		if (d_data->dynProperties[i].propertyName() == prop.propertyName()) {
			d_data->dynProperties[i] = prop;
			return;
		}
	}

	d_data->dynProperties.append(prop);
}

DynamicProperty VipVTKObject::findDynamicProperty(const QString& name, bool* found)
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));

	if (found)
		*found = true;

	for (int i = 0; i < d_data->dynProperties.size(); ++i) {
		if (d_data->dynProperties[i].propertyName() == name) {
			return d_data->dynProperties[i];
		}
	}

	if (found)
		*found = false;
	return DynamicProperty();
}

const QList<DynamicProperty>& VipVTKObject::dynamicProperties() const
{
	return d_data->dynProperties;
}

void VipVTKObject::removeDynamicProperty(const DynamicProperty& prop)
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));
	d_data->dynProperties.removeOne(prop);
}

void VipVTKObject::clearDynamicProperties()
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));
	d_data->dynProperties.clear();
}

void VipVTKObject::applyDynamicProperty(qint64 time)
{
	QMutexLocker lock(const_cast<QMutex*>(&d_data->mutex));
	for (int i = 0; i < d_data->dynProperties.size(); ++i) {
		d_data->dynProperties[i].applyProperty(time, this);
	}
}
*/
VipVTKObjectLocker VipVTKObject::lock()
{
	return VipVTKObjectLocker(d_data);
}
VipVTKObjectLocker VipVTKObject::tryLock()
{
	if (d_data->mutex.tryLock())
		return VipVTKObjectLocker(d_data, std::adopt_lock_t{});
	return VipVTKObjectLocker();
}


VipVTKObject::operator const void*() const noexcept
{
	return d_data->data.GetPointer();
}

vtkPoints* VipVTKObject::points() const
{
	if (d_data->data->IsA("vtkPointSet"))
		return static_cast<vtkPointSet*>(d_data->data.GetPointer())->GetPoints();
	return nullptr;
}

vtkDataSet* VipVTKObject::dataSet() const
{
	if (d_data->data && d_data->data->IsA("vtkDataSet"))
		return static_cast<vtkDataSet*>(d_data->data.GetPointer());
	return nullptr;
}

vtkDataObject* VipVTKObject::data() const
{
	return d_data->data;
}

vtkPolyData* VipVTKObject::polyData() const
{
	if (d_data->data && d_data->data->IsA("vtkPolyData"))
		return static_cast<vtkPolyData*>(d_data->data.GetPointer());
	return nullptr;
}

vtkPointSet* VipVTKObject::pointSet() const
{
	if (d_data->data && d_data->data->IsA("vtkPointSet"))
		return static_cast<vtkPointSet*>(d_data->data.GetPointer());
	return nullptr;
}

vtkGraph* VipVTKObject::graph() const
{
	if (d_data->data && d_data->data->IsA("vtkGraph"))
		return static_cast<vtkGraph*>(d_data->data.GetPointer());
	return nullptr;
}

vtkRectilinearGrid* VipVTKObject::rectilinearGrid() const
{
	if (d_data->data && d_data->data->IsA("vtkRectilinearGrid"))
		return static_cast<vtkRectilinearGrid*>(d_data->data.GetPointer());
	return nullptr;
}

vtkStructuredGrid* VipVTKObject::structuredGrid() const
{
	if (d_data->data && d_data->data->IsA("vtkStructuredGrid"))
		return static_cast<vtkStructuredGrid*>(d_data->data.GetPointer());
	return nullptr;
}

vtkUnstructuredGrid* VipVTKObject::unstructuredGrid() const
{
	if (d_data->data && d_data->data->IsA("vtkUnstructuredGrid"))
		return static_cast<vtkUnstructuredGrid*>(d_data->data.GetPointer());
	return nullptr;
}

vtkStructuredPoints* VipVTKObject::structuredPoints() const
{
	if (d_data->data && d_data->data->IsA("vtkStructuredPoints"))
		return static_cast<vtkStructuredPoints*>(d_data->data.GetPointer());
	return nullptr;
}

vtkTable* VipVTKObject::table() const
{
	if (d_data->data && d_data->data->IsA("vtkTable"))
		return static_cast<vtkTable*>(d_data->data.GetPointer());
	return nullptr;
}

vtkTree* VipVTKObject::tree() const
{
	if (d_data->data && d_data->data->IsA("vtkTree"))
		return static_cast<vtkTree*>(d_data->data.GetPointer());
	return nullptr;
}

vtkImageData* VipVTKObject::image() const
{
	if (d_data->data && d_data->data->IsA("vtkImageData"))
		return static_cast<vtkImageData*>(d_data->data.GetPointer());
	return nullptr;
}




QStringList VipVTKObject::vtkFileSuffixes()
{
	return (QStringList() << "stl"
			      << "vtk"
			      << "vti"
			      << "vtp"
			      << "vtr"
			      << "vts"
			      << "vtu");
}

QStringList VipVTKObject::vtkFileFilters()
{
	return (QStringList() << "*.stl"
			      << "*.vtk"
			      << "*.vti"
			      << "*.vtp"
			      << "*.vtr"
			      << "*.vts"
			      << "*.vtu");
}


/* bool VipVTKObject::ConvertFile(const QString& infile, const QString& outfile)
{
	// Convert file using VTKConverter tool
	QString path = vipAppCanonicalPath();
	path = QFileInfo(path).canonicalPath();
	QString converter = path + "/win32/VTKConverter.exe";
	printf("converter: '%s'\n", converter.toLatin1().data());
	QProcess p;
	p.start(converter, QStringList() << infile << outfile);
	p.waitForStarted();
	p.waitForFinished();
	int code = p.exitCode();
	printf("%s\n", p.readAllStandardOutput().data());
	if (code != 0) {
		VIP_LOG_ERROR(p.readAllStandardOutput());
		return false;
	}
	return true;
}*/

/* bool IsVTK51File(const QString& filename)
{
	// Returns true if given VTK file has a version higher or equal to 5
	QFile fin(filename);
	if (!fin.open(QIODevice::ReadOnly))
		return false;
	QByteArray line = fin.readLine();
	QList<QByteArray> words = line.split(' ');
	for (int i = 0; i < words.size(); ++i) {
		bool ok = false;
		double version = words[i].toDouble(&ok);
		if (ok && version > 5)
			return true;
	}
	return false;
}*/

VipVTKObject VipVTKObject::load(const QString& filename, QString* error )
{
	QFileInfo info(filename);
	QString suffix = info.suffix();
	VipVTKObject res;

	if (suffix.compare("stl", Qt::CaseInsensitive) == 0) {
		//vtkSmartPointer<vtkPolyDataMapper> ptr = vtkSmartPointer<vtkPolyDataMapper>::New();
		vtkSmartPointer<vtkSTLReader> reader = vtkSmartPointer<vtkSTLReader>::New();
		reader->SetFileName(filename.toLatin1().data());
		reader->Update();
		//ptr->SetInputConnection(reader->GetOutputPort());
		//ptr->Update();
		res = VipVTKObject(reader->GetOutput());
	}
	else if (suffix.compare("vtk", Qt::CaseInsensitive) == 0) {
		// check if conversion is required
		/* if (IsVTK51File(filename)) {
			QUuid uuid = QUuid::createUuid();
			QString tempFileFullPath = QDir::toNativeSeparators(QDir::tempPath() + "/" + qApp->applicationName().replace(" ", "") + "_" + uuid.toString(QUuid::WithoutBraces) + ".vtk");
			printf("fname: %s\n", tempFileFullPath.toLatin1().data());
			if (!ConvertFile(filename, tempFileFullPath)) {
				VIP_LOG_ERROR("Unable to convert vtk file %s to older format",filename.toLatin1().data());
				return VipVTKObject();
			}


			vtkSmartPointer<vtkGenericDataObjectReader> reader = vtkSmartPointer<vtkGenericDataObjectReader>::New();
			QByteArray ar = tempFileFullPath.toLatin1();
			reader->SetFileName(ar.data());
			reader->Update();

			QFile::remove(tempFileFullPath);

			vtkDataObject* obj = reader->GetOutput();
			if (!obj) {
				VIP_LOG_ERROR("Unable to read input file %s", filename);
				return VipVTKObject();
			}
			else
				res->setObject(obj);
		}
		else*/
		{
			vtkSmartPointer<vtkGenericDataObjectReader> reader = vtkSmartPointer<vtkGenericDataObjectReader>::New();
			QByteArray ar = filename.toLatin1();
			printf("fname: %s\n", ar.data());
			reader->SetFileName(ar.data());
			reader->Update();

			vtkDataObject* obj = reader->GetOutput();
			if (!obj) {
				if(error)
					*error = "Unable to read input file" + filename;
				return VipVTKObject();
			}
			else
				res = VipVTKObject(obj);
		}
	}
	else if (suffix.startsWith("vt", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLGenericDataObjectReader> reader = vtkSmartPointer<vtkXMLGenericDataObjectReader>::New();
		reader->SetFileName(filename.toLatin1().data());
		reader->Update();
		vtkDataObject* obj = reader->GetOutput();
		if (!obj) {
			if (error)
					*error = "Unable to read input file "+ filename;
			return VipVTKObject();
		}
		else
			res = VipVTKObject(obj);
	}
	else if (VipVTKImage::imageSuffixes().indexOf(suffix) >= 0) {
		res = VipVTKObject(VipVTKImage(filename).image());
	}

	res.setDataName(info.canonicalFilePath());
	return res;
}

VipVTKObject VipVTKObject::loadFromBuffer(const QByteArray& str, const QString& format, QString * error)
{
	QString path;
	{
		QTemporaryFile file;
		if (!file.open())
			return VipVTKObject();
		path = file.fileName() + "." + format;
	}
	QFile out(path);
	if (!out.open(QFile::WriteOnly))
		return VipVTKObject();
	out.write(str);
	out.close();

	VipVTKObject res = load(path,error);
	QFile::remove(path);
	if (!res)
		return res;
	res.setDataName(QString());
	return res;
}

bool VipVTKObject::save(const QString& filename) const
{
	const VipVTKObject& obj = *this;
	QFileInfo info(filename);
	QString suffix = info.suffix();

	// In release only, there is problem when saving point arrays.
	// Indeed, the current scalar is not saved. So we have to unset the current scalar before saving, and reset it after.
	vtkDataArray* point_scalars = nullptr;
	vtkDataArray* cell_scalars = nullptr;
	if (vtkDataSet* set = obj.dataSet()) {
		point_scalars = set->GetPointData()->GetScalars();
		if (point_scalars) {
			point_scalars->Register(0);
			set->GetPointData()->SetScalars(nullptr);
			set->GetPointData()->AddArray(point_scalars);
		}

		cell_scalars = set->GetCellData()->GetScalars();
		if (cell_scalars) {
			cell_scalars->Register(0);
			set->GetCellData()->SetScalars(nullptr);
			set->GetCellData()->AddArray(cell_scalars);
		}
	}

	if (suffix.compare("stl", Qt::CaseInsensitive) == 0) {

		vtkSmartPointer<vtkSTLWriter> writer = vtkSmartPointer<vtkSTLWriter>::New();
		writer->SetFileName(filename.toLatin1().data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		writer->SetInputData(obj.data());
		bool res = (writer->Write());

		if (vtkDataSet* set = obj.dataSet()) {
			set->GetPointData()->SetScalars(point_scalars);
			set->GetCellData()->SetScalars(cell_scalars);
		}

		// obj.setIsFileData(true);
		return res;
	}
	else if (suffix.compare("vtk", Qt::CaseInsensitive) == 0) {
		vtkSmartPointer<vtkGenericDataObjectWriter> writer = vtkSmartPointer<vtkGenericDataObjectWriter>::New();
		writer->SetFileName(filename.toLatin1().data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		writer->SetInputData(obj.data());
		bool res = (writer->Write());
		if (vtkDataSet* set = obj.dataSet()) {
			set->GetPointData()->SetScalars(point_scalars);
			set->GetCellData()->SetScalars(cell_scalars);
		}
		// obj.setIsFileData(true);
		return res;
	}
	else if (suffix.startsWith("vtr", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLRectilinearGridWriter> writer = vtkSmartPointer<vtkXMLRectilinearGridWriter>::New();
		writer->SetFileName(filename.toLatin1().data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		writer->SetInputData(obj.data());
		bool res = (writer->Write());
		if (vtkDataSet* set = obj.dataSet()) {
			set->GetPointData()->SetScalars(point_scalars);
			set->GetCellData()->SetScalars(cell_scalars);
		}
		// obj.setIsFileData(true);
		return res;
	}
	else if (suffix.startsWith("vtu", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
		writer->SetFileName(filename.toLatin1().data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		writer->SetInputData(obj.data());
		bool res = (writer->Write());
		if (vtkDataSet* set = obj.dataSet()) {
			set->GetPointData()->SetScalars(point_scalars);
			set->GetCellData()->SetScalars(cell_scalars);
		}
		// obj.setIsFileData(true);
		return res;
	}
	else if (suffix.startsWith("vtp", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLPolyDataWriter> writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
		writer->SetFileName(filename.toLatin1().data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		writer->SetInputData(obj.data());
		bool res = (writer->Write());
		if (vtkDataSet* set = obj.dataSet()) {
			set->GetPointData()->SetScalars(point_scalars);
			set->GetCellData()->SetScalars(cell_scalars);
		}
		// obj.setIsFileData(true);
		return res;
	}
	else if (suffix.startsWith("vts", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLStructuredGridWriter> writer = vtkSmartPointer<vtkXMLStructuredGridWriter>::New();
		writer->SetFileName(filename.toLatin1().data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		writer->SetInputData(obj.data());
		bool res = (writer->Write());
		if (vtkDataSet* set = obj.dataSet()) {
			set->GetPointData()->SetScalars(point_scalars);
			set->GetCellData()->SetScalars(cell_scalars);
		}
		// obj.setIsFileData(true);
		return res;
	}

	if (vtkDataSet* set = obj.dataSet()) {
		set->GetPointData()->SetScalars(point_scalars);
		set->GetCellData()->SetScalars(cell_scalars);
	}

	return 0;
}

//#ifdef NDEBUG

bool VipVTKObject::saveToBuffer( QByteArray& str, const QString& f) const
{
	const VipVTKObject& obj = *this;
	VipVTKObjectLocker lock = vipLockVTKObjects(obj);

	if (!obj)
		return false;

	qint64 start = QDateTime::currentMSecsSinceEpoch();

	QString format = f;
	if (f.isEmpty()) {
		if (obj.rectilinearGrid())
			format = "vtr";
		else if (obj.unstructuredGrid())
			format = "vtu";
		else if (obj.structuredGrid())
			format = "vts";
		else if (obj.polyData())
			format = "vtp";
		else if (obj.data())
			format = "vtk";
	}

	bool res = false;

	// In release only, there is problem when saving point arrays.
	// Indeed, the current scalar is not saved. So we have to unset the current scalar before saving, and reset it after.
	vtkDataArray* point_scalars = nullptr;
	vtkDataArray* cell_scalars = nullptr;
	if (vtkDataSet* set = obj.dataSet()) {
		point_scalars = set->GetPointData()->GetScalars();
		if (point_scalars) {
			point_scalars->Register(0);
			set->GetPointData()->SetScalars(nullptr);
			set->GetPointData()->AddArray(point_scalars);
		}

		cell_scalars = set->GetCellData()->GetScalars();
		if (cell_scalars) {
			cell_scalars->Register(0);
			set->GetCellData()->SetScalars(nullptr);
			set->GetCellData()->AddArray(cell_scalars);
		}
	}

	if (format.isEmpty())
		res = false;
	else if (format.compare("vtk", Qt::CaseInsensitive) == 0) {
		vtkSmartPointer<vtkGenericDataObjectWriter> writer = vtkSmartPointer<vtkGenericDataObjectWriter>::New();
		writer->SetWriteToOutputString(1);
		writer->SetFileTypeToBinary();
		writer->SetInputData(obj.data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		res = writer->Write();
		str = QByteArray(writer->GetOutputString());
	}
	else if (format.startsWith("vtr", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLRectilinearGridWriter> writer = vtkSmartPointer<vtkXMLRectilinearGridWriter>::New();
		writer->SetWriteToOutputString(1);
		writer->SetDataModeToBinary();
		writer->SetInputData(obj.data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		res = writer->Write();
		std::string s = writer->GetOutputString();
		str = QByteArray(s.c_str(), (int)s.size());
	}
	else if (format.startsWith("vtu", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
		writer->SetWriteToOutputString(1);
		writer->SetDataModeToBinary();
		writer->SetInputData(obj.data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		res = writer->Write();
		std::string s = writer->GetOutputString();
		str = QByteArray(s.c_str(),(int) s.size());
	}
	else if (format.startsWith("vtp", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLPolyDataWriter> writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
		writer->SetWriteToOutputString(1);
		writer->SetDataModeToBinary();
		writer->SetInputData(obj.data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		res = writer->Write();
		std::string s = writer->GetOutputString();
		str = QByteArray(s.c_str(), (int)s.size());
	}
	else if (format.startsWith("vts", Qt::CaseInsensitive)) {
		vtkSmartPointer<vtkXMLStructuredGridWriter> writer = vtkSmartPointer<vtkXMLStructuredGridWriter>::New();
		writer->SetWriteToOutputString(1);
		writer->SetDataModeToBinary();
		writer->SetInputData(obj.data());
		VipVTKObjectLocker lock = vipLockVTKObjects(obj);
		res = writer->Write();
		std::string s = writer->GetOutputString();
		str = QByteArray(s.c_str(), (int)s.size());
	}

	if (vtkDataSet* set = obj.dataSet()) {
		set->GetPointData()->SetScalars(point_scalars);
		set->GetCellData()->SetScalars(cell_scalars);
	}

	qint64 el = QDateTime::currentMSecsSinceEpoch() - start;
	return res;
}

/* #else


bool VipVTKObject::saveDataString(const VipVTKObject& obj, QByteArray& str, const QString& f)
{
	VipVTKObjectLocker lock = vipLockVTKObjects(obj);

	if (!obj)
		return false;

	qint64 start = QDateTime::currentMSecsSinceEpoch();

	QString format = f;
	if (f.isEmpty()) {
		if (obj.rectilinearGrid())
			format = "vtr";
		else if (obj.unstructuredGrid())
			format = "vtu";
		else if (obj.structuredGrid())
			format = "vts";
		else if (obj.polyData())
			format = "vtp";
		else if (obj.data())
			format = "vtk";
	}
	if (format.isEmpty())
		return false;

	QTemporaryFile file;
	if (!file.open())
		return false;

	QString path = file.fileName() + "." + format;
	if (!SaveDataFile(obj, path))
		return false;

	QFile in(path);
	if (!in.open(QFile::ReadOnly))
		return false;
	str = in.readAll();

	qint64 el = QDateTime::currentMSecsSinceEpoch() - start;
	return true;
}

#endif
*/
bool VipVTKObject::saveToDirectory(const QVector<VipVTKObject>& lst, const QString& dir, const QString& suffix)
{
	if (lst.isEmpty())
		return false;

	QString directory = dir;
	directory.replace("\\", "/");
	if (directory.endsWith("/"))
		directory.remove(directory.size() - 1, 1);

	if (!QDir(directory).exists())
		return false;

	/* VipProgress display;
	display.setModal(true);
	display.setRange(0, lst.size() - 1);
	display.setCancelable(true);*/

	QString fname = lst.first().dataName();
	fname.replace("\\", "/");
	for (int i = 1; i < lst.size(); ++i) {
		QString tmp = lst[i].dataName();
		tmp.replace("\\", "/");

		int j = 0;
		int size = std::min(tmp.size(), fname.size());
		for (; j < size; ++j) {
			if (tmp[j] != fname[j])
				break;
		}

		fname = fname.mid(0, j);
	}
	// we must stop at a slash separator
	while (fname.size() > 0 && fname.back() != '/')
		fname = fname.mid(0, fname.size() - 1);

	for (int i = 0; i < lst.size(); ++i) {
		//if (display.canceled())
		//	return false;

		VipVTKObject data = lst[i];
		QString name = data.dataName();
		name.replace("\\", "/");
		name = name.mid(fname.size());

		if (name.startsWith("/"))
			name.remove(0, 1);

		// printf("%s\n", name.toLatin1().data());

		name.remove(QFileInfo(name).suffix());
		if (suffix != "default")
			name += suffix;
		else
			name += data.preferredSuffix();


		QString file = directory + "/" + name;
		QFileInfo info(file);


		//display.setValue(i);
		//display.setText("Save " + info.fileName());

		QString subdir = file;
		subdir.remove(info.fileName());
		// create sub directories if needed
		QDir().mkpath(subdir);

		if (!data.save(file))
			return false;
	}

	return true;
}







bool isColorAttribute(const VipVTKObjectList& lst, VipVTKObject::AttributeType t, const QString& name)
{
	for (int i = 0; i < lst.size(); ++i)
		if (!lst[i].isColorAttribute(t, name))
			return false;

	return true;
}
QStringList supportedFileSuffix(const VipVTKObjectList& lst)
{
	QMap<QString, int> counts;

	for (int i = 0; i < lst.size(); ++i) {
		QStringList ext = lst[i].supportedFileSuffix();
		for (int e = 0; e < ext.size(); ++e) {
			if (counts.find(ext[e]) != counts.end())
				counts[ext[e]]++;
			else
				counts[ext[e]] = 1;
		}
	}

	QStringList res;
	for (QMap<QString, int>::iterator it = counts.begin(); it != counts.end(); ++it)
		if (it.value() == lst.size())
			res << it.key();

	return res;
}
QStringList commonAttributes(const VipVTKObjectList& lst, VipVTKObject::AttributeType type)
{
	QMap<QString, int> counts;

	for (int i = 0; i < lst.size(); ++i) {
		QStringList attr = lst[i].attributesName(type);
		for (int e = 0; e < attr.size(); ++e) {
			if (counts.find(attr[e]) != counts.end())
				counts[attr[e]]++;
			else
				counts[attr[e]] = 1;
		}
	}

	QStringList res;
	for (QMap<QString, int>::iterator it = counts.begin(); it != counts.end(); ++it)
		if (it.value() == lst.size())
			res << it.key();

	return res;
}



VipVTKObjectLocker::VipVTKObjectLocker(const SharedPointer& ptr)
  : d_ptr(ptr)
{
	if (ptr)
		ptr->mutex.lock();
}
VipVTKObjectLocker::~VipVTKObjectLocker() noexcept
{
	if (d_ptr)
		d_ptr->mutex.unlock();
}
/*
bool VipVTKObjectList::hasAttribute(VipVTKObject::AttributeType t, const QString& name) const
{
	for (int i = 0; i < size(); ++i) {
		if (!at(i).hasAttribute(t, name))
			return false;
	}

	return true;
}

bool VipVTKObjectList::isColorAttribute(VipVTKObject::AttributeType t, const QString& name) const
{
	for (int i = 0; i < size(); ++i)
		if (!at(i).isColorAttribute(t, name))
			return false;

	return true;
}

QStringList VipVTKObjectList::supportedFileSuffix() const
{
	QMap<QString, int> counts;

	for (int i = 0; i < size(); ++i) {
		QStringList ext = at(i).supportedFileSuffix();
		for (int e = 0; e < ext.size(); ++e) {
			if (counts.find(ext[e]) != counts.end())
				counts[ext[e]]++;
			else
				counts[ext[e]] = 1;
		}
	}

	QStringList res;
	for (QMap<QString, int>::iterator it = counts.begin(); it != counts.end(); ++it)
		if (it.value() == size())
			res << it.key();

	return res;
}

QStringList VipVTKObjectList::commonAttributes(VipVTKObject::AttributeType type) const
{
	QMap<QString, int> counts;

	for (int i = 0; i < size(); ++i) {
		QStringList attr = at(i).attributesName(type);
		for (int e = 0; e < attr.size(); ++e) {
			if (counts.find(attr[e]) != counts.end())
				counts[attr[e]]++;
			else
				counts[attr[e]] = 1;
		}
	}

	QStringList res;
	for (QMap<QString, int>::iterator it = counts.begin(); it != counts.end(); ++it)
		if (it.value() == size())
			res << it.key();

	return res;
}

int VipVTKObjectList::indexOf(vtkDataObject* obj) const
{
	for (int i = 0; i < size(); ++i) {
		if ((*this)[i].data() == obj)
			return i;
	}
	return -1;
}
int VipVTKObjectList::indexOf(const VipVTKObject& obj) const
{
	for (int i = 0; i < size(); ++i) {
		if ((*this)[i] == obj)
			return i;
	}
	return -1;
}


VipVTKObjectLockerList VipVTKObjectList::vipLockVTKObjects() const
{
	return ::vipLockVTKObjects(*this);
}




*/





/**
At some point, we need to use dynamic properties, i.e. Point or Cell Attributes that can change through time.
This class is used to provide this behavior, but for vtkDoubleArray attributes only.

Use DynamicProperty::addProperty to add a property for a given time range. This function takes a vtkDoubleArray as parameter, but also a vector of indices.
Indeed, storing an attribute for each point of the model can be very (very) memory consuming. Instead, we define the attributes for valid points (or cells) given through there indices in the
vtkDataSet.

Use DynamicProperty::setName to set the attribute name. Use DynamicProperty::applyProperty to set the corresponding attribute to a VipVTKObject. This will create the
attribute and add it to the VipVTKObject if necessary, reset it to NaN, and fill it (for the valid indices only) with the corresponding values.

Thsi class is used to store the result of dynamic 3D mapping (for moving cameras).
*/
/* class VIP_DATA_TYPE_EXPORT DynamicProperty
{
	class PrivateData;
	DynamicProperty(const PrivateData&);

public:
	typedef vtkSmartPointer<vtkDataArray> DArray;
	typedef QVector<vtkSmartPointer<vtkDataArray>> array_vector;
	typedef QVector<int> Indexes;
	typedef QVector<Indexes> indexes_vector;
	typedef QVector<VipTimeRange> time_range_vector;

	DynamicProperty(const QString& name = QString(), VipVTKObject::AttributeType type = VipVTKObject::Unknown, const vtkVariant& default_value = vtkVariant());

	void setPropertyName(const QString& name);
	const QString& propertyName() const;

	void setPropertyType(VipVTKObject::AttributeType type);
	VipVTKObject::AttributeType propertyType() const;

	void setDefaultValue(const vtkVariant& value);
	const vtkVariant& defaultValue() const;

	void addProperty(const VipTimeRange& range, const DArray& array, const Indexes& indexes);
	bool getProperty(qint64 time, DArray& array, Indexes& indexes) const;
	bool applyProperty(qint64 time, VipVTKObject* data) const;

	const time_range_vector& timeRanges() const;
	const array_vector& arrays() const;
	const indexes_vector& indexes() const;

	DynamicProperty copy() const;

	bool operator==(const DynamicProperty& other) const { return d_data == other.d_data; }
	bool operator!=(const DynamicProperty& other) const { return d_data != other.d_data; }

	template<class T>
	static DArray makeArray()
	{
		vtkSmartPointer<T> res = vtkSmartPointer<T>::New();
		return res;
	}

private:
	QSharedPointer<PrivateData> d_data;
};

Q_DECLARE_METATYPE(DynamicProperty)

class DynamicProperty::PrivateData
{
public:
	time_range_vector ranges;
	array_vector arrays;
	indexes_vector indexes;
	QString name;
	VipVTKObject::AttributeType type;
	vtkVariant defaultValue;

	PrivateData()
	  : type(VipVTKObject::Unknown)
	{
	}

	PrivateData(const PrivateData& other)
	  : ranges(other.ranges)
	  , type(other.type)
	  , indexes(other.indexes)
	  , name(other.name)
	  , defaultValue(other.defaultValue)
	{
		arrays.resize(other.arrays.size());
		for (int i = 0; i < arrays.size(); ++i) {
			vtkDataArray* ar = other.arrays[i]->NewInstance();
			ar->DeepCopy(other.arrays[i].GetPointer());
			DArray dar;
			dar.TakeReference(ar);
			arrays[i] = dar;
		}
	}
};

DynamicProperty::DynamicProperty(const QString& name, VipVTKObject::AttributeType type, const vtkVariant& default_value)
  : d_data(new PrivateData())
{
	d_data->name = name;
	d_data->type = type;
	d_data->defaultValue = default_value;
}

DynamicProperty::DynamicProperty(const PrivateData& other)
  : d_data(new PrivateData(other))
{
}

DynamicProperty DynamicProperty::copy() const 
{
	return DynamicProperty(*d_data);
}

void DynamicProperty::setPropertyName(const QString& name)
{
	d_data->name = name;
}

const QString& DynamicProperty::propertyName() const
{
	return d_data->name;
}

void DynamicProperty::setPropertyType(VipVTKObject::AttributeType type)
{
	d_data->type = type;
}

VipVTKObject::AttributeType DynamicProperty::propertyType() const
{
	return d_data->type;
}

void DynamicProperty::setDefaultValue(const vtkVariant& value)
{
	d_data->defaultValue = value;
}

const vtkVariant& DynamicProperty::defaultValue() const
{
	return d_data->defaultValue;
}

void DynamicProperty::addProperty(const VipTimeRange& range, const DArray& array, const Indexes& indexes)
{
	d_data->ranges.append(range);
	d_data->arrays.append(array);
	d_data->indexes.append(indexes);
}

bool DynamicProperty::getProperty(qint64 time, DArray& array, Indexes& indexes) const
{
	for (int i = 0; i < d_data->ranges.size(); ++i) {
		if (vipIsInside(d_data->ranges[i], time)) {
			array = d_data->arrays[i];
			indexes = d_data->indexes[i];
			return true;
		}
	}
	return false;
}

bool DynamicProperty::applyProperty(qint64 time, VipVTKObject* data) const
{
	if (d_data->name.isEmpty() || d_data->type == VipVTKObject::Unknown || d_data->type == VipVTKObject::Field)
		return false;

	DArray ar;
	Indexes ind;
	// retrieve the property at given time
	if (getProperty(time, ar, ind)) {
		vtkDataArray* array = nullptr;
		// build the default values (NaN) for each component
		vtkVariantList lst;

		if (!d_data->defaultValue.IsValid()) {
			for (int i = 0; i < ar->GetNumberOfComponents(); ++i)
				lst << vtkVariant(std::numeric_limits<double>::quiet_NaN());
		}
		else {
			for (int i = 0; i < ar->GetNumberOfComponents(); ++i)
				lst << d_data->defaultValue;
		}

		if (d_data->type == VipVTKObject::Point) {
			// build the array attribute
			array = static_cast<vtkDataArray*>(data->setPointsAttribute(d_data->name, lst));
		}
		else {
			array = static_cast<vtkDataArray*>(data->setCellsAttribute(d_data->name, lst));
		}

		if (!array)
			return false;

		// fill the array
		for (int i = 0; i < ind.size(); ++i) {
			const int index = ind[i];
			array->SetTuple(index, ar->GetTuple(i));
		}

		array->Modified();
		return true;
	}
	return false;
}

const DynamicProperty::time_range_vector& DynamicProperty::timeRanges() const
{
	return d_data->ranges;
}

const DynamicProperty::array_vector& DynamicProperty::arrays() const
{
	return d_data->arrays;
}

const DynamicProperty::indexes_vector& DynamicProperty::indexes() const
{
	return d_data->indexes;
}

static QString CommonRoot(const QString& s1, const QString& s2)
{
	if (s1.isEmpty() || s2.isEmpty())
		return QString();

	int size = qMin(s1.size(), s2.size());
	int i = 0;
	for (; i < size; ++i) {
#ifdef Q_OS_WIN
		if (s1[i].toLower() != s2[i].toLower())
			break;
#else
		if (s1[i] != s2[i])
			break;
#endif
	}

	return s1.mid(0, i);
}*/

/*
QStringList OrganizeObjects(const VipVTKObjectList & lst)
{
	QStringList full_paths;
	QStringList res;
	for (int i = 0; i < lst.size(); ++i)
	{
		full_paths.append(lst[i]->name());
		res.append(lst[i]->name());
	}


}*/



/* static QDataStream& operator<<(QDataStream& str, const QVector<vtkSmartPointer<vtkDataArray>>& vec)
{
	int count = 0;
	for (int i = 0; i < vec.size(); ++i)
		if (vec[i]->IsA("vtkDoubleArray"))
			++count;

	str >> count;

	for (int i = 0; i < vec.size(); ++i) {
		if (vec[i]->IsA("vtkDoubleArray"))
			str << VipNDArray::makeView((double*)vec[i]->GetVoidPointer(0), vipVector((int)vec[i]->GetNumberOfTuples(), vec[i]->GetNumberOfComponents()));
		// VipNDArrayTypeView<double>((double*)vec[i]->GetVoidPointer(0), vipVector((int)vec[i]->GetNumberOfTuples(), vec[i]->GetNumberOfComponents()));
	}
	return str;
}

static QDataStream& operator>>(QDataStream& str, QVector<vtkSmartPointer<vtkDataArray>>& vec)
{
	vec.clear();
	int count = 0;
	str >> count;

	for (int i = 0; i < count; ++i) {
		VipNDArray ar;
		str >> ar;
		if (ar.shapeCount() == 2) {
			vtkSmartPointer<vtkDoubleArray> array = vtkSmartPointer<vtkDoubleArray>::New();
			array->SetNumberOfComponents(ar.shape(1));
			array->SetNumberOfTuples(ar.shape(0));
			memcpy(array->GetVoidPointer(0), ar.constData(), ar.size() * sizeof(double));
			vec.append(array);
		}
	}
	return str;
}

VipArchive& operator<<(VipArchive& arch, const DynamicProperty& obj)
{
	QByteArray data;
	{
		QDataStream stream(&data, QIODevice::WriteOnly);

		// property name
		stream << obj.propertyName();

		// type and default value
		stream << (int)obj.propertyType();
		stream << obj.defaultValue().ToDouble();

		// all time ranges
		stream << obj.timeRanges();
		// indexes
		stream << obj.indexes();
		// arrays
		stream << obj.arrays();
	}
	arch.content("content", data);
	return arch;
}

VipArchive& operator>>(VipArchive& arch, DynamicProperty& obj)
{
	QByteArray data = arch.read("content").toByteArray();
	if (data.size()) {
		QString name;
		int type = 0;
		double default_value = 0;
		QVector<QVector<int>> indexes;
		QVector<VipTimeRange> time_ranges;
		QVector<vtkSmartPointer<vtkDataArray>> arrays;

		QDataStream stream(&data, QIODevice::ReadOnly);
		stream >> name >> type >> default_value >> indexes >> time_ranges >> arrays;

		if (stream.status() == QDataStream::Ok) {
			obj.setPropertyName(name);
			obj.setPropertyType(VipVTKObject::AttributeType(type));
			obj.setDefaultValue(default_value);
			for (int i = 0; i < indexes.size(); ++i)
				obj.addProperty(time_ranges[i], arrays[i], indexes[i]);
		}
	}
	return arch;
}*/
