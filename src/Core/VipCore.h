/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
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

#ifndef VIP_CORE_H
#define VIP_CORE_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QPolygonF>
#include <QVariant>
#include <qicon.h>
#include <qpixmap.h>

#include "VipFunctional.h"
#include "VipSceneModel.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)

namespace Qt
{
	namespace detail
	{
		struct Endl
		{
		};
	}
	static const detail::Endl endl;
}
inline QTextStream& operator<<(QTextStream& str, const Qt::detail::Endl&)
{
	str << '\n';
	str.flush();
	return str;
}

#endif

///\defgroup Core Core
///
/// Core is the base library of Thermavip's SDK and defines the main concepts and classes that should be used to extend Thermavip through plugins.
///
/// The first main class is the VipProcessingObject one which provides an interface for all processing classes. It provides a nice Qt-based way to link
/// a processing output to another one's input, and build complex block diagrams of algorithms. If you need a processing object that reads or writes data (for instance to handle a new video file
/// format), you must inherit the VipIODevice class, which is a VipProcessingObject dedicated to reading or writing any kind of data. If you need to perform image processing algorithms based on a
/// scene model (see VipSceneModel class), you should use the VipSceneModelBasedProcessing class. All VipProcessingObject exchange their inputs/outputs using the VipAnyData class. A few processing
/// classes are definied in Core: <ul> <li>VipExtractComponent: extract a component from an image <li>VipExtractHistogram: extract an histogram from an image (VipNDArray) and a shape (VipShape)
/// <li>VipExtractPolyline: extract the pixels along a polyline (VipShape) inside an image
/// <li>VipExtractStatistics: extract the min,max, mean, standard deviation of the pixels inside a sub-part of an image
/// <li>VipProcessingList: chains a list of processings having one input and one output
/// <li>VipSceneModelBasedProcessing: base class to apply an algorithm based on a scene model (VipSceneModel)
/// <li>VipAnyResource: a VipIODevice load any kind of resource
/// <li>VipTimeRangeBasedGenerator: base helper class for temporal devices
/// <li>VipTextFileReader: read text file as series of images or curves
/// <li>VipTextFileWriter: write images or curves into text files
/// <li>VipImageReader: read standard image formats
/// <li>VipImageWriter: write standard image formats
/// <li>VipDirectoryReader: read full directory
/// <li>VipArchiveRecorder: record any number/type of signals to a binary archive
/// <li>VipArchiveReader: read and number/type of signals from a binary archive
/// </ul>
///
/// The second main functionality provided by the Core library is the serialization/deserialization of any kind of data. It relies on the VipArchive base class,
/// providing an interface to read/write data from/to potentially any kind of container. This feature is used for data persistency (for instance saving a block diagram of VipProcessingObject)
/// and to save/load Thermavip sessions.
/// Currently, the Core library supports XML archives (VipXOStringArchive, VipXOfArchive, VipXIStringArchive and VipXIfArchive) and binary archives (VipBinaryArchive).
///
/// The third main concept provided by the Core library is the function dispatching. It relies on the VipFunctionDispatcher class and is one of the main
/// way to extend Thermavip. For instance, the archiving mechanism relies on function dispatching to serialize/deserialize custom types.
/// For more information, see the VipFunctionDispatcher class and the \ref Gui module which defines most of the functions distacher objects.
/// The function dispatching concept relies heavily on the Qt meta type system. Objects are passed to a function dispatcher through QVariant objects, therefore you
/// must declare custom types with the Q_DECLARE_METATYPE macro. When writing plugins, you should use the VIP_REGISTER_QOBJECT_METATYPE macro to declare a QObject
/// inheriting type.
///
/// In addition, the Core library provides these functionalities:
/// <ul>
/// <li>VipCommandOptions: parse command arguments
/// <li>VipPluginInterface: the base interface for Thermavip plugins
/// <li>Timestamp manipulation in VipTimestamping.h
/// <li>VipUniqueId: set unique identifiers to QObject inheriting instances
/// <li>VipProgress: progress status for time consuming operations
/// </ul>
///
/// The Core library depends on Logging and DataType ones.
///

/// \addtogroup Core
/// @{

/// Declare QPointer<QObject> so that it can be passed to a QVariant object
typedef QPointer<QObject> QObjectPointer;
Q_DECLARE_METATYPE(QObjectPointer)

/// @brief Cast a QList of QObject pointer to another list of QObject pointer.
/// All nullptr pointers or pointers that cannot be cast to the output type are removed.
template<class T, class U>
QList<T> vipListCast(const QList<U>& lst)
{
	QList<T> res;
	for (U u : lst)
		if (T tmp = qobject_cast<T>(u))
			res.append(tmp);
	return res;
}

/// @brief Cast a QList of QObject pointer to another list of QObject pointer.
/// All nullptr pointers or pointers that cannot be cast to the output type are removed.
/// If obj_name is not empty, only QObject with given objectName() are kept.
template<class T, class U>
QList<T> vipListCast(const QList<U>& lst, const QString& obj_name)
{
	QList<T> res;
	for (U u : lst)
		if (!obj_name.isEmpty() && u->objectName() == obj_name)
			if (T tmp = qobject_cast<T>(u))
				res.append(tmp);
	return res;
}

/// @brief Cast a QSet of QObject pointer to another set of QObject pointer.
/// All nullptr pointers or pointers that cannot be cast to the output type are removed.
template<class T, class U>
QSet<T> vipListCast(const QSet<U>& lst)
{
	QSet<T> res;
	for (U u : lst)
		if (T tmp = qobject_cast<T>(u))
			res.insert(tmp);
	return res;
}

/// @brief Helper function, returns the first item of \a lst that can be converted to type T using qobject_cast.
template<class T, class U>
T vipFirstItem(const QList<U>& lst)
{
	for (U u : lst)
		if (T tmp = qobject_cast<T>(u))
			return tmp;
	return nullptr;
}

/// Helper function. Returns the last item of \a lst that can be converted to type T using qobject_cast.
template<class T, class U>
T vipLastItem(const QList<U>& lst)
{
	for (int i = lst.size() - 1; i >= 0; --i)
		if (T tmp = qobject_cast<T>(lst[i]))
			return tmp;
	return nullptr;
}

/// @brief Convert a QVariantList to a typed list.
/// Only the successful conversions are returned.
/// This function is meant for QVariant storing a pointer to QObject, but works for any types.
/// All items in input list that were not convertible are deleted if inheriting QObject.
template<class T>
QList<T> vipListCast(const QVariantList& lst)
{
	QList<T> res;
	for (int i = 0; i < lst.size(); ++i) {
		if (vipIsConvertible(VipType(lst[i].userType()), VipType(qMetaTypeId<T>())))
			res.append(lst[i].value<T>());
		else
			vipReleaseVariant(lst[i]);
	}
	return res;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
/// @brief Cast a QVector of QObject pointer to a QList of QObject pointer.
/// All nullptr pointers or pointers that cannot be cast to the output type are removed.
template<class T, class U>
QList<T> vipListCast(const QVector<U>& lst)
{
	QList<T> res;
	for (int i = 0; i < lst.size(); ++i)
		if (T tmp = qobject_cast<T>(lst[i]))
			res.append(tmp);
	return res;
}
#endif

/// @brief Make a QList unique (remove all duplicates)
template<class T>
QList<T> vipListUnique(const QList<T>& lst)
{
	QList<T> res;
	for (int i = 0; i < lst.size(); ++i) {
		if (res.indexOf(lst[i]) < 0)
			res.append(lst[i]);
	}
	return res;
}

/// @brief Copy a list of QVariant to the clipboard.
/// Internally uses a XML archive to serialize the values into a textual representation.
VIP_CORE_EXPORT void vipCopyObjectsToClipboard(const QVariantList& lst);

/// @brief Retrieve a list of QVariant from the clipboard.
VIP_CORE_EXPORT QVariantList vipCreateFromClipboard();

/// @brief Copy a list of object to the clipboard.
/// Internally uses a XML archive to serialize the values into a textual representation.
template<class T>
void vipCopyObjectsToClipboard(const QList<T>& lst)
{
	QVariantList tmp;
	for (int i = 0; i < lst.size(); ++i)
		tmp.append(QVariant::fromValue(lst[i]));
	vipCopyObjectsToClipboard(tmp);
}

/// @brief Retrieve a list of objects from the clipboard.
template<class T>
QList<T> vipCreateFromClipboard()
{
	QVariantList tmp = vipCreateFromClipboard();
	QList<T> res;
	for (int i = 0; i < tmp.size(); ++i) {
		if (vipIsConvertible(VipType(tmp[i].userType()), VipType(qMetaTypeId<T>())))
			res.append(tmp[i].value<T>());
		else
			vipReleaseVariant(tmp[i]);
	}
	return res;
}

/// @brief Process all events from the main event loop.
///
/// This function can be called from any thread in order to keep the GUI responsive, and can even be called recursively without deadlock.
///
/// If \a keep_going is not nullptr, the function will return when *keep_going == false.
/// If \a milli is greater than 0, the function will return 0 if all events where processed before  given milli seconds.
///
/// The return values are the following:
/// <ul>
/// <li>0: all events were processed</li>
/// <li>-1: stopped due to keep_going set to false</li>
/// <li>-2: stopped due to a timeout</li>
/// <li>-3: stopped for another reason (like recursive call detected)</li>
/// </ul>
VIP_CORE_EXPORT int vipProcessEvents(bool* keep_going = nullptr, int milli = -1);

class VipArchive;
class QDataStream;
class QTextStream;

/// @brief Stream a VipShape to/from a VipArchive
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipShape& value);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipShape& value);
/// @brief Stream a VipSceneModel to/from a VipArchive
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipSceneModel& value);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipSceneModel& value);
/// @brief Stream a VipSceneModelList to/from a VipArchive
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipSceneModelList& value);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipSceneModelList& value);

VIP_CORE_EXPORT void vipShapeToJSON(QTextStream& str, const VipShape& value, const QByteArray& indent = QByteArray());
VIP_CORE_EXPORT void vipSceneModelToJSON(QTextStream& str, const VipSceneModel& value, const QByteArray& indent = QByteArray());
VIP_CORE_EXPORT void vipSceneModelListToJSON(QTextStream& str, const VipSceneModelList& value, const QByteArray& indent = QByteArray());
VIP_CORE_EXPORT VipSceneModelList vipSceneModelListFromJSON(const QByteArray& content, QString* error = nullptr);

/// Save a QVariant to a QDataStream.
/// This function is equivalent to QDataStream & operator<<(QDataStream &, const QVariant&),
/// except that it returns false on error instead of crashing on an assertion.
/// On failure, the input stream position is left unchanged.
VIP_CORE_EXPORT bool vipSafeVariantSave(QDataStream& s, const QVariant& v);
/// Save a QVariantMap to a QDataStream.
/// This function is equivalent to QDataStream & operator<<(QDataStream &, const QVariantMap&),
/// except that it only save the valid entries (and silently drop the unvalid ones with #vipSafeVariantSave).
/// Returns the number of entry saved.
/// The QVariantMap can be read back with the standard version of QDataStream & operator>>(QDataStream &, QVariantMap&);
VIP_CORE_EXPORT qsizetype vipSafeVariantMapSave(QDataStream& s, const QVariantMap& c);

/// \internal add a function that will be called in QCoreApplication constructor. Should only be used by Thermavip SDK libraries.
VIP_CORE_EXPORT bool vipAddInitializationFunction(const VipFunction<0>& fun);
VIP_CORE_EXPORT bool vipAddInitializationFunction(void (*fun)());
VIP_CORE_EXPORT bool vipAddInitializationFunction(int (*fun)());

/// \internal add a function that will be called in QCoreApplication constructor. Should only be used by Thermavip SDK libraries.
VIP_CORE_EXPORT bool vipPrependInitializationFunction(const VipFunction<0>& fun);
VIP_CORE_EXPORT bool vipPrependInitializationFunction(void (*fun)());
VIP_CORE_EXPORT bool vipPrependInitializationFunction(int (*fun)());

/// \internal add a function that will be called in QCoreApplication event loop. Should only be used by Thermavip SDK libraries.
VIP_CORE_EXPORT bool vipAddGuiInitializationFunction(int (*fun)());
VIP_CORE_EXPORT bool vipAddGuiInitializationFunction(void (*fun)());
VIP_CORE_EXPORT bool vipAddGuiInitializationFunction(const VipFunction<0>& fun);

VIP_CORE_EXPORT void vipEnableGuiInitializationFunction(bool);

/// \internal add a function that will be called in QCoreApplication destructor. Should only be used by Thermavip SDK libraries.
VIP_CORE_EXPORT bool vipAddUninitializationFunction(const VipFunction<0>& fun);
VIP_CORE_EXPORT bool vipAddUninitializationFunction(void (*fun)());
VIP_CORE_EXPORT bool vipAddUninitializationFunction(int (*fun)());

/// An error data, represented by an error code (<0) and an error string
class VipErrorData
{
	QString m_error;
	int m_code;
	qint64 m_date;

public:
	VipErrorData(const QString& error = QString(), int code = -1, qint64 date = QDateTime::currentMSecsSinceEpoch())
	  : m_error(error)
	  , m_code(code)
	  , m_date(date)
	{
	}

	VipErrorData(const VipErrorData&) = default;
	VipErrorData(VipErrorData&&) = default;
	VipErrorData& operator=(const VipErrorData&) = default;
	VipErrorData& operator=(VipErrorData&&) = default;

	const QString& errorString() const { return m_error; }
	int errorCode() const { return m_code; }
	qint64 msecsSinceEpoch() const { return m_date; }
};

Q_DECLARE_METATYPE(VipErrorData)
/// Serialize a VipErrorData into a QDataStream AND a VipArchive
VIP_CORE_EXPORT QDataStream& operator<<(QDataStream& stream, const VipErrorData& data);
VIP_CORE_EXPORT QDataStream& operator>>(QDataStream& stream, VipErrorData& data);

/// Function object that can be registered with #vipRegisterFunction.
struct VIP_CORE_EXPORT VipFunctionObject
{
	typedef std::function<QVariant(const QVariantList&)> function_type;

	QString name;
	QString description;
	function_type function;
	bool mainThread; // function should be executed in main thread

	VipFunctionObject() {}
	VipFunctionObject(const function_type& _fun, const QString& _name, const QString& _descr = QString())
	  : name(_name)
	  , description(_descr)
	  , function(_fun)
	  , mainThread(true)
	{
	}

	bool isValid() const;
};
Q_DECLARE_METATYPE(VipFunctionObject)

/// Register a function object to make it accessible from other plugins.
///
/// This function is usefull when you want to make available a function of your Thermavip plugin to other plugins. Indeed,
/// each plugin can use the Thermavip SDK, but cannot use other plugins unless linking with them.
///
/// Therefore, you can use vipRegisterFunction to share functions between plugins.
/// Likewise, you can use the VIP_REGISTER_QOBJECT_METATYPE macro to register QObject inheriting classes.
///
/// Note that functions registered this way can be called within a Python interpreter using call_internal_func function of thermavip module.
/// A function can return a VipErrorData object in case of error.
VIP_CORE_EXPORT bool vipRegisterFunction(const VipFunctionObject& fun);
VIP_CORE_EXPORT bool vipRegisterFunction(const VipFunctionObject::function_type& fun, const QString& name, const QString& description = QString(), bool mainThread = true);

/// Find a function registered with #vipRegisterFunction using its name.
VIP_CORE_EXPORT VipFunctionObject vipFindFunction(const QString& name);
/// Returns all function objects registered with #vipRegisterFunction.
VIP_CORE_EXPORT QList<VipFunctionObject> vipAllFunctions();

/// \internal Add a serialize and deserialize function that will be used to save/load specific settings from the session file.
/// This is only used in Thermavip SDK, plugins have their own way to manage serialization.
VIP_CORE_EXPORT bool vipRegisterSettingsArchiveFunctions(void (*save)(VipArchive&), void (*restore)(VipArchive&));
VIP_CORE_EXPORT void vipSaveSettings(VipArchive& arch);
VIP_CORE_EXPORT void vipRestoreSettings(VipArchive& arch);

/// Save all custom properties of \a obj within the archive.
/// A custom property is a QObject property which name starts with '_vip_custom'.
/// This function is called by several objects when serialized (like VipAbstractPlayer,
/// VipPlotItem,...) and is used to allow plugins to save extra data in a session file.
/// This function should only be called by the SDK.
VIP_CORE_EXPORT void vipSaveCustomProperties(VipArchive& arch, const QObject* obj);
/// Retrieve the serialized custom properties for given object and store them in \a obj.
VIP_CORE_EXPORT QList<QByteArray> vipLoadCustomProperties(VipArchive& arch, QObject* obj);

class VipArchive;
class VIP_CORE_EXPORT VipCoreSettings
{
	VipCoreSettings();
	bool m_log_overwrite;
	bool m_log_date;
	QString m_skin;

public:
	static VipCoreSettings* instance();

	void setLogFileOverwrite(bool);
	bool logFileOverwrite() const;

	void setLogFileDate(bool);
	bool logFileDate() const;

	/// Set the GUI skin.
	/// This function is provided in the Core module as the skin must be loaded before starting Thermavip GUI.
	void setSkin(const QString& skin);
	QString skin() const;

	// Save/restore settings.
	// These settings are stored in a separate file because they
	// must be loaded before the general settings file (the log file
	// is created before).
	bool save(VipArchive&);
	bool save(const QString&);

	bool restore(VipArchive&);
	bool restore(const QString&);
};

/// Returns an approximation of the variant memory footprint in bytes.
/// This function is used to compute the total memory size taken by a FIFO or LIFO input list of a VipInput.
/// This function works for standard data types: numerical values, QString and QByteArray, VipNDArray, VipIntervalSampleVector, VipPointVector, etc.
/// It is possible to define additional memory computation functions for custom types using vipRegisterMemoryFootprintFunction().
/// If the data type is not handled, 0 is returned.
VIP_CORE_EXPORT int vipGetMemoryFootprint(const QVariant& v);

/// Register, for a given Qt meta type id, a function that computes the memory footprint of a QVariant.
/// This function will be used in vipGetMemoryFootprint().
VIP_CORE_EXPORT int vipRegisterMemoryFootprintFunction(int metatype_id, int (*fun)(int, const QVariant&));

/// Set the time function used in Thermavip to get the current time in nanoseconds since epoch.
/// This function is used by vipGetNanoSecondsSinceEpoch() and vipGetMilliSecondsSinceEpoch().
/// A default time function is provided and use the QDateTime::currentMSecsSinceEpoch() static member function.
VIP_CORE_EXPORT void vipSetTimeFunction(qint64 (*fun)());

/// Returns elapsed nanoseconds since Epoch.
/// Uses the time function registered with vipSetTimeFunction().
VIP_CORE_EXPORT qint64 vipGetNanoSecondsSinceEpoch();

/// Returns elapsed milliseconds since Epoch.
/// Uses the time function registered with vipSetTimeFunction().
VIP_CORE_EXPORT qint64 vipGetMilliSecondsSinceEpoch();

/// Returns a QIcon loaded from given image file name.
/// The image file is searched in the paths registered with vipAddIconPath().
VIP_CORE_EXPORT QIcon vipIcon(const QString& filename);
/// Returns a QPixmap loaded from given image file name.
/// The image file is searched in the paths registered with vipAddIconPath().
VIP_CORE_EXPORT QPixmap vipPixmap(const QString& filename);
/// Register a new path for the icons to be found.
/// By default, icons are searched in the 'Icons' directory of the application path.
VIP_CORE_EXPORT void vipAddIconPath(const QString& path);
/// Register a new path for the icons to be found.
/// This path will be inspected first for icons.
VIP_CORE_EXPORT void vipAddFrontIconPath(const QString& path);
/// Set all possible paths for the icons to be found.
VIP_CORE_EXPORT void vipSetIconPaths(const QStringList& paths);
/// Returns the last build time in ms since epoch
VIP_CORE_EXPORT qint64 vipBuildTime();

/// Set Thermavip version name (if any) used to customize the title bar.
VIP_CORE_EXPORT void vipSetEditionVersion(const QString&);
VIP_CORE_EXPORT QString vipEditionVersion();

VIP_CORE_EXPORT void vipSetAppCanonicalPath(const QString&);
VIP_CORE_EXPORT QString vipAppCanonicalPath();

/// @brief Returns the current user name
VIP_CORE_EXPORT QString vipUserName();




/// @}
// end Core

#endif
