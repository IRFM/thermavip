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

#ifndef VIP_VTK_OBJECT_H
#define VIP_VTK_OBJECT_H

#include <QFileInfo>
#include <QMap>
#include <QSharedPointer>
#include <QVariant>
#include <QVector>
#include <QMutex>
#include <QMetaType>
#include <QStringList>

#include <vtkVariant.h>
#include <vtkDataObject.h>
#include <vtkSmartPointer.h>

#include <vector>
#include <mutex> //for std::adopt_lock_t

#include "VipSleep.h"

typedef QVector<vtkVariant> vtkVariantList;

class vtkPoints;
class vtkAbstractArray;
class vtkDataArray;
class vtkUnstructuredGrid;
class vtkSelectEnclosedPoints;
class vtkGraph;
class vtkRectilinearGrid;
class vtkStructuredGrid;
class vtkStructuredPoints;
class vtkTable;
class vtkTree;
class vtkHierarchicalBoxDataSet;
class vtkMultiBlockDataSet;
class vtkImageData;
class vtkCellArray;
class vtkDataObject;
class vtkPolyData;
class vtkPointSet;
class vtkDataSet;

class VipVTKObjectLocker;

namespace detail
{
	VIP_DATA_TYPE_EXPORT void AddObjectObserver(vtkObject* object, const char* file, int line);
}

#ifdef VIP_VTK_ADD_OBJECT_OBSERVER
#define VIP_VTK_OBSERVER(object) detail::AddObjectObserver(object, __FILE__, __LINE__)
#else
#define VIP_VTK_OBSERVER(object)
#endif

/// @brief Wrapper class around vtkDataObject.
///
/// VipVTKObject wrap a vtkDataObject object and
/// provides some convenient functions for its
/// manipulation. It is used as data type through
/// thermavip to represent 3D objects.
///
/// VipVTKObject uses shared ownership.
///
/// A VipVTKObject is displayed using a
/// PlotVipVTKObject (inheriting VipPlotItem)
/// and VTKGraphicsView (inheriting VipImageWidget2D)
/// as viewport.
///
/// You should always lock a VipVTKObject before
/// modifying it. Indeed, VipVTKObject uses shared
/// ownership instead of COW to avoid copies of
/// potentially huge 3D objects.
///
/// Therefore, a VipVTKObject might be modified by
/// a processing object while being displayed in a
/// VTKGraphicsView, which might end up in a race condition.
///
/// Since VTKGraphicsView locks all VipVTKObject that it is
/// about to render, locking a VipVTKObject before any
/// manipulation will prevent crashes.
///
/// Locking one or multiple objects should be done with vipLockVTKObjects()
/// set of functions to avoid potential deadlocks. The VipVTKObject
/// is itself recursive to prevent deadlocks when calling
/// VipVTKObject member functions while the lock is already acquired.
///
/// All member functions of VipVTKObject are thread safe.
///
class VIP_DATA_TYPE_EXPORT VipVTKObject
{
	friend class VipVTKObjectLocker;

	#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	friend size_t qHash(const VipVTKObject& key, size_t seed);
#else
	friend size_t qHash(const VipVTKObject& key, size_t seed);
#endif

	class PrivateData;
	using SharedPointer = QSharedPointer<PrivateData>;

	VipVTKObject(const SharedPointer&) noexcept;

public:
	/// @brief Attribute type used for attribute manipulation functions
	enum AttributeType
	{
		Unknown = -1,
		Field, // Field attributes
		Point, // Point attributes
		Cell   // Cell attributes
	};

	static void SetObjectName(vtkDataObject*, const char*);
	static const char * GetObjectName(const vtkDataObject*);

	/// @brief Default Ctor
	VipVTKObject();

	/// @brief Construct from a vtkDataObject and a data name.
	/// The VipVTKObject will internally store the vtkDataObject in a vtkSmartPointer.
	/// The data name is set using vtkDataObject::SetObjectName().
	VipVTKObject(vtkDataObject* object, const QString& name = QString());
	VipVTKObject(const VipVTKObject&) = default;
	VipVTKObject(VipVTKObject&&) = default;
	VipVTKObject& operator=(const VipVTKObject&) = default;
	VipVTKObject& operator=(VipVTKObject&&) = default;
	~VipVTKObject();

	/// @brief Standard default color for most 3D objects within thermavip
	static const double* defaultObjectColor();
	static void setDefaultObjectColor(const double * c);

	/// @brief Clear the object
	void clear();

	/// @brief Less than operator in order to be inserted in sorted containers
	bool operator<(const VipVTKObject& right) const noexcept { return d_data.get() < right.d_data.get(); }
	/// @brief Conversion to void* in order to support if(data)...
	operator const void*() const noexcept;

	/// @brief Build attributes for this object.
	/// This is used to display information on the internal data object.
	QVariantMap buildAllAttributes() const;

	/// @brief Build a description of the data object used for tooltips mainly.
	/// @param pointId add information on given point if not < 0
	/// @param cellId add information on given cell if not < 0
	QString description(int pointId = -1, int cellId = -1) const;

	/// @brief Returns the data name (vtkDataObject::GetObjectName())
	QString dataName() const;
	/// @brief Set the data name
	void setDataName(const QString&);

	/// @brief Returns the vtk object class name (vtkDataObject::GetClassName())
	QString className() const;

	/// @brief performs a deep copy of the object
	VipVTKObject copy() const;

	/// @brief Returns true is internal vtkDataObject inherits given class.
	/// Returns false if the internal vtkDataObject is null.
	bool isA(const char* class_name) const;

	/// @brief Helper function, build a vtkAbstractArray
	/// @param name array name
	/// @param values list of values. The number of components will be set to values.size().
	/// Each component will be filled with values[component]. The first value type will
	/// decide on the whole vtkAbstractArray type: vtkDoubleArray, vtkFloatArray,...
	/// @param size number of tuples
	/// @param array reuse array if it has the right type and size. In this case, just fill it with values.
	static vtkSmartPointer<vtkAbstractArray> makeArray(const QString& name, const vtkVariantList& values, int size, vtkAbstractArray* array = nullptr);

	/// @brief Helper function, build a vtkVariantList from an array and an index.
	/// @param array any kind of array
	/// @param index tuple index within the array
	/// @return a pair of array_name -> list of vtkVariant (one per component)
	static QPair<QString, vtkVariantList> makeAttribute(vtkAbstractArray* array, int index);

	/// @brief Add field attributes to the data object
	void setFieldAttributes(const QMap<QString, vtkVariantList>& attr);

	/// @brief Add a field attribute to the data object.
	/// This will create a vtkAbstractArray of given with 1 tuple of provided list size components.
	/// If the data object already has a field attribute with the same name, it is discarded first.
	void setFieldAttribute(const QString&, const vtkVariantList&);

	/// @brief Returns all field attributes for this data object.
	QMap<QString, vtkVariantList> fieldAttributes() const;
	/// @brief Returns the field attribute for given name
	vtkVariantList fieldAttribute(const QString&) const;
	/// @brief Returns the field attribute array for given name
	vtkAbstractArray* fieldAttributeArray(const QString& name) const;
	/// @brief Returns all field attributes arrays
	QVector<vtkAbstractArray*> fieldAttributeArrays() const;
	/// @brief Returns all field attributes names
	QStringList fieldAttributesNames() const;

	/// @brief Returns all point attributes arrays if the data object is a vtkDataSet
	QVector<vtkAbstractArray*> pointsAttributes() const;
	/// @brief Returns the field attribute for given name if the data object is a vtkDataSet
	vtkAbstractArray* pointsAttribute(const QString& name) const;
	/// @brief Returns all point attributes names if the data object is a vtkDataSet
	QStringList pointsAttributesName() const;
	/// @brief Add a point attribute
	/// @param name array name
	/// @param default_components list of default values for all components. Defines the number of components.
	/// @return created attribute array
	vtkAbstractArray* setPointsAttribute(const QString& name, const vtkVariantList& default_components);
	vtkDataArray* setPointsAttribute(const QString& name,
					 const vtkVariantList& components_1,
					 int point_id1,
					 const vtkVariantList& components_2,
					 int point_id2,
					 QVector<int> interpolation_axes = QVector<int>());
	vtkDataArray* setPointsAttribute(const QString& name,
					 const vtkVariantList& components_1,
					 double* pt1,
					 const vtkVariantList& components_2,
					 double* pt2,
					 QVector<int> interpolation_axes = QVector<int>());

	/// @brief Returns all cell attributes arrays if the data object is a vtkDataSet
	QVector<vtkAbstractArray*> cellsAttributes() const;
	/// @brief Returns the cell attribute array for given name if the data object is a vtkDataSet
	vtkAbstractArray* cellsAttribute(const QString& name) const;
	/// @brief Returns all cell attributes names if the data object is a vtkDataSet
	QStringList cellsAttributesName() const;
	/// @brief Add a cell attribute
	/// @param name array name
	/// @param default_components list of default values for all components. Defines the number of components.
	/// @return created attribute array
	vtkAbstractArray* setCellsAttribute(const QString& name, const vtkVariantList& default_components);

	/// @brief Returns all attributes arrays for given attribute type
	QVector<vtkAbstractArray*> attributes(AttributeType) const;
	/// @brief Returns the attribute array for given name and attribute type
	vtkAbstractArray* attribute(AttributeType, const QString& name) const;
	/// @brief Returns all attributes names for given attribute type
	QStringList attributesName(AttributeType) const;
	/// @brief Returns true if the data object has given attribute
	bool hasAttribute(AttributeType, const QString& name) const;
	/// @brief Remove attribute
	bool removeAttribute(AttributeType, const QString& name);
	/// @brief Check if the given attribute is a color one.
	/// Returns true if:
	/// -	The attribute exists
	/// -	The attribute array is of type unsigned char and has 3 or 4 components
	/// -	The attribute array is of type float or double, and has 3 or 4 components, with only values in the range [0,1]
	bool isColorAttribute(VipVTKObject::AttributeType t, const QString& name) const;

	/// @brief Import all attributes from another vtk data object
	/// Returns true on success, false otherwise.
	/// This function only works for vtkDataSet with the same number of points and cells.
	bool importAttributes(const VipVTKObject& other);

	/// @brief Returns the supported file suffixes to save this object
	QStringList supportedFileSuffix() const;
	/// @brief Returns the preffered file suffix to save this object
	QString preferredSuffix() const;

	//
	// Conversion functions
	//
	// Cast the data object to specified type if not null and the conversion is valid.
	//

	vtkPoints* points() const;
	vtkDataObject* data() const;
	vtkDataSet* dataSet() const;
	vtkPolyData* polyData() const;
	vtkPointSet* pointSet() const;
	vtkGraph* graph() const;
	vtkRectilinearGrid* rectilinearGrid() const;
	vtkStructuredGrid* structuredGrid() const;
	vtkUnstructuredGrid* unstructuredGrid() const;
	vtkStructuredPoints* structuredPoints() const;
	vtkTable* table() const;
	vtkTree* tree() const;
	vtkImageData* image() const;

	/// @brief Mark as modified the data object, the cell data and point data, and the points themselves.
	void modified();

	/// @brief Lock the VipVTKObject and returns a VipVTKObjectLocker
	VipVTKObjectLocker lock();
	/// @brief Try to lock the VipVTKObject and returns a maybe null VipVTKObjectLocker
	VipVTKObjectLocker tryLock();

	/// @brief Returns all file suffixes supported by VTK: "stl", "vtk", "vti", "vtp", "vtr", "vts", "vtu"
	static QStringList vtkFileSuffixes();
	/// @brief Returns supported file suffixes as file filters suitable for VipFileDialog
	static QStringList vtkFileFilters();

	/// @brief Save object to file. The provided filename suffix is used as format.
	bool save(const QString& filename) const;
	bool saveToBuffer(QByteArray& str, const QString& format = QString()) const;

	/// @brief Load a file. Supported formats are those defined by vtkFileSuffixes().
	static VipVTKObject load(const QString& filename, QString * error = nullptr);
	/// @brief Load a buffered object with given format.
	static VipVTKObject loadFromBuffer(const QByteArray& str, const QString& format = QString(), QString* error = nullptr);
	/// @brief Save several VipVTKObject in a folder.
	/// This function use the VipVTKObject objects dataName() to extract (and drop) a common prefix, and will then create sub folders
	/// to save each object in the right location.
	static bool saveToDirectory(const QVector<VipVTKObject>& lst, const QString& directory, const QString& format = "default");

private:
	QRecursiveMutex* mutex() const;
	vtkDataObject* setObject(vtkDataObject* obj);
	/// @brief Import data (through deep copies) from other.
	void importData(const VipVTKObject& other);

	SharedPointer d_data;
};

Q_DECLARE_METATYPE(VipVTKObject);

/// @brief VipVTKObject unique locker class, similar to std::unique_ptr.
///
/// The main difference compared to unique_ptr is that VipVTKObjectLocker
/// will increment the VipVTKObject internal reference count. This
/// prevents any destruction of the object while the lock is held.
///
class VIP_DATA_TYPE_EXPORT VipVTKObjectLocker
{
	friend class VipVTKObject;
	using SharedPointer = VipVTKObject::SharedPointer;
	SharedPointer d_ptr;

	VipVTKObjectLocker(const SharedPointer& ptr);
	VipVTKObjectLocker(const SharedPointer& ptr, std::adopt_lock_t)
	  : d_ptr(ptr)
	{
	}

public:
	VipVTKObjectLocker() {}
	VipVTKObjectLocker(const VipVTKObject& obj)
	  : d_ptr(obj.d_data)
	{
		obj.mutex()->lock();
	}
	VipVTKObjectLocker(const VipVTKObject& obj, std::adopt_lock_t)
	  : d_ptr(obj.d_data)
	{
	}
	VipVTKObjectLocker(VipVTKObjectLocker&& other) noexcept
	  : d_ptr(std::move(other.d_ptr))
	{
	}
	VipVTKObjectLocker& operator=(VipVTKObjectLocker&& other) noexcept
	{
		d_ptr = std::move(other.d_ptr);
		return *this;
	}
	~VipVTKObjectLocker() noexcept;

	operator const void*() const noexcept { return d_ptr.data(); }
};

/// @brief List of VipVTKObjectLocker implemented as std::vector (faster than QVector, and we need speed here)
using VipVTKObjectLockerList = std::vector<VipVTKObjectLocker>;
using VipVTKObjectList = QVector<VipVTKObject>;
Q_DECLARE_METATYPE(VipVTKObjectList);

VIP_DATA_TYPE_EXPORT bool isColorAttribute(const VipVTKObjectList& lst, VipVTKObject::AttributeType t, const QString& name);
VIP_DATA_TYPE_EXPORT QStringList supportedFileSuffix(const VipVTKObjectList& lst);
VIP_DATA_TYPE_EXPORT QStringList commonAttributes(const VipVTKObjectList& lst, VipVTKObject::AttributeType type);

/// @brief Define qHash for VipVTKObject
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
inline uint qHash(const VipVTKObject& key, uint seed)
{
	return qHash(static_cast<void*>(key.d_data.data()), seed);
}
#else
inline size_t qHash(const VipVTKObject& key, size_t seed)
{
	return qHash(static_cast<void*>(key.d_data.data()), seed);
}
#endif

inline VipVTKObjectLocker vipLockVTKObjects(const VipVTKObject& obj)
{
	return VipVTKObjectLocker(obj);
}

inline VipVTKObjectLocker vipLockVTKObjects(const VipVTKObject& obj, std::adopt_lock_t adopt)
{
	return VipVTKObjectLocker(obj, adopt);
}

template<class Iter>
VipVTKObjectLockerList vipLockVTKObjects(Iter begin, Iter end)
{
	auto size = end - begin;
	VipVTKObjectLockerList res;
	res.reserve(size);

	while (true) {
		for (auto it = begin; it != end; ++it) {
			if (VipVTKObjectLocker lock = const_cast<VipVTKObject&>(*it).tryLock())
				res.push_back(std::move(lock));
			else {
				res.clear();
				break;
			}
		}

		if (res.size() != size)
			vipSleep(1);
		else
			break;
	}
	return res;
}

template<class T>
VipVTKObjectLockerList vipLockVTKObjects(const T& lst)
{
	return vipLockVTKObjects(lst.begin(), lst.end());
}
inline VipVTKObjectLockerList vipLockVTKObjects(std::initializer_list<VipVTKObject> lst)
{
	return vipLockVTKObjects(lst.begin(), lst.end());
}


#endif