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

#ifndef VIP_VTK_DEVICES_H
#define VIP_VTK_DEVICES_H

#include "VipIODevice.h"
#include "VipFieldOfView.h"




// Register VTK archiving operators


VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipVTKObject& obj);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipVTKObject& obj);

VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipFieldOfView& obj);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipFieldOfView& obj);

VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipFieldOfViewList& fov);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipFieldOfViewList& fov);



/**
Reads a VTK file and return a VipVTKObject
*/
class VIP_CORE_EXPORT VipVTKFileReader : public VipTimeRangeBasedGenerator//VipIODevice
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	Q_CLASSINFO("category", "reader")
	Q_CLASSINFO("description", "Read a 3D model file using the VTK library")

public:

	VipVTKFileReader(QObject * parent = nullptr)
	  : VipTimeRangeBasedGenerator (parent)// VipIODevice(parent)
	{
		this->outputAt(0)->setData(QVariant::fromValue(VipVTKObject()));
	}
	~VipVTKFileReader();

	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes);
	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	//virtual VipIODevice::DeviceType deviceType() const { return Resource; }
	virtual QString fileFilters() const { return "3D model file (*.stl *.vtk *.vtp *.vtr *.vts *.vtu)"; }

protected:
	virtual bool readData(qint64 time) {
		if (!d_data.isEmpty())
		{
			d_data.setTime(time);
			outputAt(0)->setData(d_data);
			return true;
		}
		return false;
	}

private:
	VipAnyData d_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipVTKFileReader*)


/**
Reads a X Y Z Values text file and return a VipVTKObject
*/
class VIP_CORE_EXPORT VipXYZValueFileReader : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	Q_CLASSINFO("category", "reader")
	Q_CLASSINFO("description", "Read a X Y Z Values text/csv file")

public:
	VipXYZValueFileReader()
		:VipIODevice() {
		outputAt(0)->setData(VipVTKObject());
	}

	virtual bool probe(const QString &filename, const QByteArray &) const {
		return supportFilename(filename) || VipIODevice::probe(filename);
	}
	virtual bool open(VipIODevice::OpenModes);
	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Resource; }
	virtual QString fileFilters() const { return "Point file (*.csv *.txt)"; }

protected:
	virtual bool readData(qint64 time);

private:
	VipAnyData d_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipXYZValueFileReader*)





/**
Temporal FOV reader used to represent a moving camera.
*/
class VIP_CORE_EXPORT VipFOVSequence : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	Q_CLASSINFO("category", "reader")
	Q_CLASSINFO("description", "Temporal FOV reader used to represent a moving camera")

public:
	VipFOVSequence(QObject * parent = nullptr);

	void add(const VipFieldOfView & l);
	void add(const VipFieldOfViewList&);
	void removeAt( int i);
	VipFieldOfView& at(int i);
	const VipFieldOfView &at(int i) const;
	VipFieldOfView fovAtTime(qint64 time) const;
	int count() const;
	void clear();

	const VipFieldOfViewList & fieldOfViews() const;
	void setFieldOfViews(const VipFieldOfViewList&);
	
	void setFovName(const QString& name);
	QString fovName() const;

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool open(VipIODevice::OpenModes);
	virtual QString fileFilters() const { return "Field Of View file (*.fov)"; }

protected:
	virtual bool readData(qint64 time);

private:
	VipFieldOfViewList m_fovs;
	QString m_fov_name;
};

VIP_REGISTER_QOBJECT_METATYPE(VipFOVSequence*)

class VipArchive;
VIP_CORE_EXPORT VipArchive& operator<<(VipArchive& arch, const VipFOVSequence* fov);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive& arch, VipFOVSequence* fov);





/**
Writes VipVTKObject into a VTK file
*/
class VIP_CORE_EXPORT VipVTKFileWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)
	Q_CLASSINFO("category", "writer")
	Q_CLASSINFO("description", "Write a 3D model file using the VTK library")

public:
	VipVTKFileWriter(QObject* parent = nullptr)
	  : VipIODevice(parent)
	{
	}
	~VipVTKFileWriter() { close(); }

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipVTKObject>(); }
	virtual bool open(VipIODevice::OpenModes);
	virtual void close();
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Resource; }
	virtual QString fileFilters() const { return "3D model file (*.stl *.vtk *.vtp *.vtr *.vts *.vtu)"; }


protected:
	virtual void apply();

private:
	VipAnyData d_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipVTKFileWriter*)

/**
Writes VipFieldOfView objects into a XML file
*/
class VIP_CORE_EXPORT VipFOVFileWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)
	Q_CLASSINFO("category", "writer")
	Q_CLASSINFO("description", "Write a FOV file")

public:
	VipFOVFileWriter(QObject* parent = nullptr)
	  : VipIODevice(parent)
	{
	}
	~VipFOVFileWriter() { close(); }

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipFieldOfView>(); }
	virtual bool open(VipIODevice::OpenModes);
	virtual void close();
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Temporal; }
	virtual QString fileFilters() const { return "Field Of View file (*.fov)"; }

protected:
	virtual void apply();

private:
	VipFieldOfViewList m_fovs;
};

VIP_REGISTER_QOBJECT_METATYPE(VipFOVFileWriter*)



VIP_CORE_EXPORT VipFieldOfViewList vipLoadFieldOfViewsFromString(const QString& str);
VIP_CORE_EXPORT VipFieldOfViewList vipLoadFieldOfViews(const QString& filename);
VIP_CORE_EXPORT bool vipSaveFieldOfViews(const VipFieldOfViewList& fovs, const QString& filename);
VIP_CORE_EXPORT QString vipSaveFieldOfViewsAsString(const VipFieldOfViewList& fovs);


class VIP_CORE_EXPORT VipXYZAttributesWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipMultiInput input)
	Q_CLASSINFO("category", "writer")
	Q_CLASSINFO("description", "Write a CSV file containing X, Y, Z and attributes values for each point")

public:
	struct Attribute
	{
		VipVTKObject::AttributeType type{ VipVTKObject::Point };
		QString name;
	};
	enum Format
	{
		TXT,
		CSV
	};

	VipXYZAttributesWriter(QObject* parent = nullptr);

	void setAttributeList(const QList<Attribute>&);
	const QList<Attribute>& attributeList();

	void setFormat(Format);
	Format format() const;

	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipVTKObject>(); }
	virtual bool open(VipIODevice::OpenModes);
	virtual void close();
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	virtual VipIODevice::DeviceType deviceType() const { return Resource; }
	virtual QString fileFilters() const { return "Point file (*.csv *.txt)"; }

protected:
	virtual void apply();

private:
	QList<Attribute> m_attributes;
	Format m_format;
};

VIP_REGISTER_QOBJECT_METATYPE(VipXYZAttributesWriter*)


#endif
