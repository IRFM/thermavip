#ifndef VIP_ARCHIVE_H
#define VIP_ARCHIVE_H

#include <QByteArray>
#include <QDataStream>
#include <QIODevice>
#include <QStringList>

#include "VipFunctional.h"

/// \addtogroup Core
/// @{

/// Register a serializable class to the archiving system.
/// The stream operators
/// \code
/// VipArchive & operator<<(VipArchive &, const T &);
/// VipArchive& operator>>(VipArchive&, T&);
/// \endcode
/// must be defined.
template<class T>
int vipRegisterArchiveStreamOperators();

/// \a VipArchive is the base class for all serialization purpose classes.
///
/// An \a VipArchive is used to serialize and deserialize any kind of data declared to the Qt meta type system through the Q_DECLARE_METATYPE macro.
/// To make a QObject inheriting class serializable, it must be registered with the #VIP_REGISTER_QOBJECT_METATYPE macro.
/// Within Thermavip, there are 2 types or archives: Textual one and binary ones. In order to serialize/deserialize a QVariant object, it must fulfill one of these conditions (in this order):
///
/// - the data type defines the stream operators VipArchive & operator<<(VipArchive &, const T &) and VipArchive& operator>>(VipArchive&, T&), registered with #vipRegisterArchiveStreamOperators()
/// function
/// - the data type is convertible to QString
/// - the data type is convertible to QByteArray
/// - the data type is serializable through QDataStream and registered with qRegisterMetaTypeStreamOperators() function.
///
/// Basically, all standard Qt types that a QVariant can store are serializable by default.
///
/// Currently, Thermavip defines the following archive types:
/// -	VipBinaryArchive: archive type for binary serialization
/// -	VipXArchive inheriting classes (VipXOArchive, VipXIArchive, VipXOStringArchive, VipXOfArchive, VipXIStringArchive, VipXIfArchive):
///		archive types fro XML based serialization
///
class VIP_CORE_EXPORT VipArchive : public QObject
{
	Q_OBJECT
	template<class T>
	friend int vipRegisterArchiveStreamOperators();

public:
	/// Open mode of the archive. Default is NotOpen.
	enum OpenMode
	{
		NotOpen,
		Read,
		Write
	};

	/// VipArchive type
	enum Flag
	{
		Text,
		Binary
	};

	/// Read mode. Default one is Forward
	enum ReadMode
	{
		Forward,
		Backward
	};

	/// Supported operations
	enum Operation
	{
		Comment = 0x01,		    // insert a comment in the archive
		MetaDataOnContent = 0x02,   // attach metadata (QVariantMap object) to a serialized object
		MetaDataOnNodeStart = 0x04, // attach metadata to a node start
		ReadBackward = 0x08	    // read the archive in backward
	};
	typedef QFlags<Operation> SupportedOperations;

	/// Default ctor
	VipArchive(Flag, SupportedOperations);

	/// Destructor
	virtual ~VipArchive();

	/// Retruns the last error that occurred
	QString errorString() const;
	/// Returns the last error that occurred
	int errorCode() const;
	/// Retruns true if last operation provoked an error, false otherwise
	bool hasError() const;
	/// Reset the error flag
	void resetError();
	/// Set the current error flag
	void setError(const QString& error, int code = -1);

	/// Set the read mode
	void setReadMode(ReadMode);
	/// Returns the read mode
	ReadMode readMode() const;

	/// Returns the supported operations
	SupportedOperations supportedOperations() const;
	/// Returns the archive type (textual or binary)
	Flag flag() const;

	/// Returns the open mode
	OpenMode mode() const;

	/// Returns true if opened, false otherwise. Equivalent to:( mode() != NotOpen)
	bool isOpen() const;

	/// Conversion to boolean. Returns ( isOpen() && errorCode() >= 0 )
	operator const void*() const;

	// Write and read functions. Never throw. If an error occurred, leave the stream in a readable state.
	// All these functions will reset the error flag.

	/// Starts the node \a name and returns a reference to this archive.
	/// Reset the error code and call the virtual member #VipArchive::doStart.
	/// \param name the node name
	/// \param metadata the metadata to attach to the start node (if supported)
	VipArchive& start(const QString& name, const QVariantMap& metadata);
	/// Starts the node \a name and returns a reference to this archive.
	/// Reset the error code and call the virtual member #VipArchive::doStart.
	VipArchive& start(const QString& name);

	/// Ends the current node
	/// Reset the error code and call the virtual member #VipArchive::doEnd.
	/// Returns a reference to this archive.
	VipArchive& end();

	/// Write/Read a comment to/from the archive.
	///
	/// Reset the error code and call the virtual member #VipArchive::doComment.
	/// Its default implementation does nothing
	/// Returns a reference to this archive.
	VipArchive& comment(const QString& cdata);

	/// Reads/writes a content from/to the archive.
	///
	/// Reset the error code and call the virtual members #VipArchive::doContent.
	/// Returns a reference to this archive.
	///
	/// \param name the content name (may be null)
	/// \param value the value to save/load
	/// \param metadata the metadata to attach to the content (if supported)
	template<class T>
	VipArchive& content(const QString& name, const T& value, const QVariantMap& metadata)
	{
		resetError();
		QVariant temp = vipToVariant(value);
		doContent(const_cast<QString&>(name), temp, const_cast<QVariantMap&>(metadata), true);
		if (!hasError() && mode() == Read)
			const_cast<T&>(value) = temp.value<T>();
		return *this;
	}

	/// This an overloaded function.
	/// Reads/writes a content from/to the archive.
	/// Reset the error code and call the virtual members #VipArchive::doContent.
	/// Returns a reference to this archive.
	template<class T>
	VipArchive& content(const QString& name, const T& value)
	{
		resetError();
		QVariant temp = vipToVariant(value);
		QVariantMap map;
		doContent(const_cast<QString&>(name), temp, map, false);
		if (!hasError() && mode() == Read)
			const_cast<T&>(value) = temp.value<T>();
		return *this;
	}

	/// This an overloaded function.
	/// Reads/writes a content from/to the archive.
	/// Reset the error code and call the virtual members #VipArchive::doContent.
	/// Returns a reference to this archive.
	template<class T>
	VipArchive& content(const T& value)
	{
		resetError();
		QVariant temp = vipToVariant(value);
		QVariantMap map;
		QString name;
		doContent(name, temp, map, false);
		if (!hasError() && mode() == Read)
			const_cast<T&>(value) = temp.value<T>();
		return *this;
	}

	/// Reads the next data from the archive with given name, or a null QVariant if the operation fails. Also loads its metadata (if any).
	/// If \a name is empty, read the next data and reset \a name to the content's name.
	QVariant read(const QString& name, QVariantMap& metadata);
	/// Reads the next data from the archive with given name, or a null QVariant if the operation fails.
	/// If \a name is empty, read the next data and reset \a name to the content's name.
	QVariant read(const QString& name);
	/// Reads and return the next data, or a null QVariant if the operation fails.
	QVariant read();

	/// Returns the current read/write position in the archive. The returned QStringList
	/// object contains the names of the current nodes from the top most node (index 0) to the
	/// bottom-most one (last index).
	/// Example:
	/// \code
	///	VipXIfArchive iarchive("my_file.xml");
	///	iarchive.start("node1");
	///	iarchive.start("node2");
	///	iarchive.start("node3");
	///	iarchive.snd();
	///
	///	iarchive.currentPosition(); //returns ["node1","node2"]
	/// \endcode
	QStringList currentPosition() const;

	/// Set a boolean attribute to the archive.
	/// These attributes can be used to control the archiving flow for a specific data type.
	void setAttribute(const char* name, bool value);
	bool hasAttribue(const char* name) const;
	bool attribute(const char* name, bool _default = false) const;

	/// Set/get the version number.
	/// It is sometimes usefull to set a version number at the beginning of reading an archive,
	/// as further reading functions can compare it to the current Thermavip version and perform
	/// custom tasks based on this.
	/// Note that the version string is not overwritten by a restore() call.
	void setVersion(const QString& version);
	QString version() const;

public Q_SLOTS:

	/// \a setRange can be called by any serialization function in order to inform the user about a progression status
	void setRange(double min, double max) { emit rangeUpdated(min, max); }

	/// \a setValue can be called by any serialization function in order to inform the user about a progression status
	void setValue(double value) { emit valueUpdated(value); }

	/// \a setText can be called by any serialization function in order to inform the user about a progression status
	void setText(const QString& text) { emit textUpdated(text); }

	/// Save the current archive status (read mode and position)
	virtual void save();
	/// Reset the archive status. Each call to \a restore must match to a call to #save().
	virtual void restore();

	/// Register a type (as returned by qMetaTypeId<T>()) as a fast type.
	/// A fast type has its serialize/deserialize functions buffered in the archive, and they will be the first ones to be checked when saving/loading an object.
	/// This avoid looking into the whole serialize and deserialize function dispatchers.
	/// Use this function when a type is likely to be saved/loaded several times in the archive
	void registerFastType(int type);

Q_SIGNALS:

	void rangeUpdated(double min, double max);
	void valueUpdated(double value);
	void textUpdated(const QString& text);

protected:
	/// Set the current archive open mode. This function should be called by any class inheriting VipArchive when opening the archive.
	void setMode(OpenMode m);

	/// Start a new node with the given name
	virtual void doStart(QString& name, QVariantMap& metadata, bool read_metadata) = 0;

	/// End the current node
	virtual void doEnd() = 0;

	/// Create a content with the given name and value (write mode), or read the content with the given name.
	virtual void doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata) = 0;

	/// Create a comment section with the given text
	virtual void doComment(QString& text) { Q_UNUSED(text); }

	VipFunctionDispatcher::FunctionList serializeFunctions(const QVariant& value);
	VipFunctionDispatcher::FunctionList deserializeFunctions(const QVariant& value);
	void copyFastTypes(VipArchive& other);

private:
	static VipFunctionDispatcher& serializeDispatcher();
	static VipFunctionDispatcher& deserializeDispatcher();

	class PrivateData;
	PrivateData* m_data;
};

Q_DECLARE_METATYPE(VipArchive*);
Q_DECLARE_OPERATORS_FOR_FLAGS(VipArchive::SupportedOperations);

/// @brief Serialize a QVariant object to a VipArchive
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const QVariant& value);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, QVariant& value);


/// @brief Serialize a QMap into a VipArchive
template<class T, class U>
VipArchive& operator<<(VipArchive& arch, const QMap<T, U>& any)
{
	arch.content("count", any.size());
	for (typename QMap<T, U>::const_iterator it = any.begin(); it != any.end(); ++it) {
		arch.content(it.key());
		arch.content(it.value());
	}
	return arch;
}

/// @brief Serialize a QMap into a VipArchive
template<class T, class U>
VipArchive& operator>>(VipArchive& arch, QMap<T, U>& any)
{
	int count = arch.read("count").toInt();
	for (int i = 0; i < count; ++i) {
		T key;
		U value;
		arch.content(key);
		arch.content(value);
		if (!arch.hasError())
			any.insert(key, value);
		else
			break;
	}

	arch.resetError();
	return arch;
}


namespace detail
{
	template<class T>
	void serializeAdapter(const T& value, VipArchive* arch)
	{
		(*arch) << value;
	}
	template<class T>
	QVariant deserializeAdapter(const T& value, VipArchive* arch)
	{
		(*arch) >> const_cast<T&>(value);
		return QVariant::fromValue(value);
	} 
}

template<class T>
int vipRegisterArchiveStreamOperators()
{
	if (VipArchive::serializeDispatcher().exactMatch(VipTypeList() << VipType(qMetaTypeId<T>())).size() == 0) {
		VipArchive::serializeDispatcher().append<void(const T&, VipArchive*)>(detail::serializeAdapter<T>);
		VipArchive::deserializeDispatcher().append<void(const T&, VipArchive*)>(detail::deserializeAdapter<T>);
		return qMetaTypeId<T>();
	}
	return 0;
}

/// Mark a VipArchive's content as editable. Currently only supported with XML based archives.
/// You can mark an archive's content as editable by using this function like this:
/// \code
///
/// struct Test {
/// QString value;
/// };
///
/// VipArchive & operator<<(VipArchive & arch, const Test & test)
/// {
/// return arch.content("value",test.value, vipEditableSymbol("A test value", "VipLineEdit {qproperty-value: 'test value';}") );
/// }
///
/// \endcode
///
/// This will tell to the archiving system that the content \a value of class \a Test can be edited using a VipLineEdit widget.
/// This feature is used for modifiable sessions.
VIP_CORE_EXPORT QVariantMap vipEditableSymbol(const QString& comment, const QString& style_sheet);

/// \a VipBinaryArchive is an \a VipArchive that stores its data in a binary way.
/// It performs its I/O operations on a QIODevice.
class VIP_CORE_EXPORT VipBinaryArchive : public VipArchive
{
	Q_OBJECT
public:
	VipBinaryArchive();
	VipBinaryArchive(QIODevice* d);
	VipBinaryArchive(QByteArray* a, QIODevice::OpenMode mode);
	VipBinaryArchive(const QByteArray& a);
	VipBinaryArchive(const QString& filename, QIODevice::OpenMode mode);

	void setDevice(QIODevice* d);
	QIODevice* device() const;
	void close();

	virtual void save();
	virtual void restore();

	/// Reads the next data and returns it unserialized.
	/// Returns an empty QByteArray on error.
	QByteArray readBinary(const QString& name = QString());
	QVariant deserialize(const QByteArray& ar);

protected:
	virtual void doStart(QString& name, QVariantMap& metadata, bool read_metadata);
	virtual void doEnd();
	virtual void doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata);

private:
	QIODevice* m_device;
	QList<qint64> m_saved_pos;
};

/// @}
// end Core

#endif
