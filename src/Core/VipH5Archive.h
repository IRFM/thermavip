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


#include "VipArchive.h"

/// \a VipH5Archive is an \a VipArchive that stores its data in a HDF5 container.
/// It performs its I/O operations on a QIODevice.
/// 
/// VipH5Archive ease the manipulation of HDF5 files by providing a simpler
/// interface as HDF5 C library. In consequence, only a subset to HDF5 features
/// are supported. For instance links and compound types are not supported.
/// 
/// VipH5Archive can store basically all object types that Thermavip provides.
/// Some types are stored using HDF5 primitives, like VipNDArray or VipPointVector
/// which are stored in N dimension datasets.
/// 
/// Other types are stored in a binary format (1d bytes dataset) aimed to be written/read
/// with QDataStream. Finally, types defining custom archive stream functions (and registered
/// using vipRegisterArchiveStreamOperators()) are stored are groups instead of datasets.
/// 
/// Note that VipH5Archive supports similar object names as opposed to HDF5.
/// This is simulated by adding a '%obj_id' to the object name. This feature is
/// mandatory in order to use VipH5Archive for session saving.
/// 
/// VipH5Archive performs sequential reading in creation order. A VipH5Archive
/// cannot be opened in both rad and write modes.
/// 
class VIP_CORE_EXPORT VipH5Archive : public VipArchive
{
	Q_OBJECT
public:
	enum Type
	{
		None,
		Group,
		DataSet
	};
	using Object = QPair<QByteArray,Type>;
	using Content = QVector<Object>;

	VipH5Archive();
	VipH5Archive(QIODevice* d);
	VipH5Archive(QByteArray* a, QIODevice::OpenMode mode);
	VipH5Archive(const QByteArray& a);
	VipH5Archive(const QString& filename, QIODevice::OpenMode mode);
	~VipH5Archive();

	bool open(QByteArray* a, QIODevice::OpenMode mode);
	bool open(const QByteArray& a);
	bool open(const QString& filename, QIODevice::OpenMode mode);
	bool open(QIODevice* d);

	/// @brief Returns the last read/written object name (group or dataset).
	QByteArray lastRead() const noexcept;
	/// @brief Returns the current group position. 
	QByteArray currentGroup() const noexcept;
	/// @brief Returns the last closed group following a call to end()
	QByteArray lastEndGroup() const noexcept;
	/// @brief Returns the content of the current group.
	Content currentGroupContent() const;

	QIODevice* device() const;
	void close();

protected:
	virtual void doStart(QString& name, QVariantMap& metadata, bool read_metadata);
	virtual void doEnd();
	virtual void doContent(QString& name, QVariant& value, QVariantMap& metadata, bool read_metadata);
	virtual void doComment(QString& text);
	virtual void doSave();
	virtual void doRestore();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};