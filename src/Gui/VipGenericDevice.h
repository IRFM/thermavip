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

#ifndef VIP_GENERIC_DEVICE_H
#define VIP_GENERIC_DEVICE_H

#include "VipIODevice.h"
#include <QWidget>

/// \addtogroup Gui
/// @{

/// VipGenericRecorder is a helper class that encapsulates any kind of writing VipIODevice classes registerd with VIP_REGISTER_QOBJECT_METATYPE.
/// Use this class if you need a writing VipIODevice that supports several data types / file formats.
///
/// VipGenericRecorder uses a VipMultiInput as input since the internal VipIODevice can possibly define any number of inputs
///
/// Calling VipGenericRecorder::setPath will internally create the right VipIODevice instance according to given path and setup the inputs.
/// Then you just need to call VipGenericRecorder::open to start writing data.
class VIP_GUI_EXPORT VipGenericRecorder : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipMultiInput inputs)
	Q_CLASSINFO("description", "Record any type of input data into a single file of any supported format");

public:
	VipGenericRecorder(QObject* parent = nullptr);
	~VipGenericRecorder();

	/// Returns true if any already registered VipIODevice supports given filename or first_bytes.
	virtual bool probe(const QString& filename, const QByteArray& first_bytes) const;

	/// Open the device. You must have set the path first with VipGenericRecorder::setPath.
	virtual bool open(VipIODevice::OpenModes mode);
	virtual DeviceType deviceType() const { return Temporal; }
	virtual VipIODevice::OpenModes supportedModes() const { return WriteOnly; }
	/// Returns all supported file filters for all writing VipIODevice classes registerd with VIP_REGISTER_QOBJECT_METATYPE
	virtual QString fileFilters() const;
	/// Set the writing path. This will create the internal VipIODevice and setup the inputs.
	virtual bool setPath(const QString& path);
	/// Close the internal VipIODevice
	virtual void close();
	virtual qint64 estimateFileSize() const;

	/// Directly set the internal recorder.
	///  This will delete the preivous recorder, if any.
	void setRecorder(VipIODevice* device);
	VipIODevice* recorder() const;

	/// @brief If enabled, stop the streaming when closing the device, and re-enabled it afterward.
	/// This might be mandatory if some processing take longer than the streaming sampling time.
	/// Indeed, closing the device will wait on its sources. And slower sources might keep accumulating data
	/// if the streaming is still enabled, leading to an infinit loop.
	/// Disabled by default.
	void setStopStreamingOnClose(bool);
	bool stopStreamingOnClose() const;

	/// Returns the date prefix format, which must be compatible with QDateTime::toString function
	QString datePrefix() const;
	/// Returns true adding a date prefix to the output file is requested
	bool hasDatePrefix() const;

	/// Generate the output filename from the current path and the date prefix (if any)
	QString generateFilename() const;

	void setProbeInputs(const QVariantList& lst);

	qint64 recordedSize() const;

public Q_SLOTS:
	/// Set the date prefix
	void setDatePrefix(const QString& date_prefix);
	void setHasDatePrefix(bool enable);
	void setRecorderAvailableDataOnOpen(bool enable);

	void setOpened(bool);

	void openDeviceIfNotOpened();

	void openDevice() { setOpened(true); }
	void closeDevice() { setOpened(false); }
	void setRecordedSize(qint64 bytes);

	/// Display again the recorder parameters (if any).
	/// This works only if the device is closed.
	void resetRecorderParameters();

protected:
	virtual void apply();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class QToolButton;
class VipFileName;

/// VipRecordWidget provides a graphical interface to manage a VipGenericRecorder instance:
/// <ul>
/// <li>Set the device path
/// <li>Optionally add a prefix to the output path that contains the current date when opening the device
/// <li>Start/stop recording
/// </ul>
class VIP_GUI_EXPORT VipRecordWidget : public QWidget
{
	Q_OBJECT

public:
	enum RecordInfos
	{
		FramesAndInputSize,
		DurationAndOutputSize
	};
	enum InfosLocation
	{
		Bottom,
		Right
	};

	VipRecordWidget(InfosLocation loc = Bottom, QWidget* widget = nullptr);
	~VipRecordWidget();

	/// Set the VipGenericRecorder instance managed by this widget
	void setGenericRecorder(VipGenericRecorder* recorder);
	VipGenericRecorder* genericRecorder() const;

	void setRecordInfos(RecordInfos infos);
	RecordInfos recordInfos() const;

	void setDateOptionsVisible(bool);
	bool dateOptionsVisible() const;

	void setDatePrefixEnabled(bool);
	bool datePrefixEnabled() const;
	void setDatePrefix(const QString&);
	QString datePrefix() const;

	/// Returns the record button
	QToolButton* record() const;
	QToolButton* suspend() const;
	VipFileName* filenameWidget() const;

	QString path() const;
	QString filename() const;
	static QString updateFileFilters(const QVariantList& data, VipFileName* filename);

public Q_SLOTS:
	QString updateFileFilters(const QVariantList& data);
	void updateFileFilters(const QString& filters);
	void updateFileFilters();
	void setFilename(const QString& filename);
	void enableRecording(bool record);
	void suspend(bool enable);

private Q_SLOTS:
	/// Start/stop recording
	void setRecording(bool);
	/// Start recording
	void startRecording();
	/// Stop recording
	void stopRecording();
	void updateWidgetFromDevice();
	void updateDeviceFromWidget();
	/// Update the record info. VipRecordWidget provides a QLabel displaying the number of frames currently save in the VipGenericRecorder.
	///  This function update the QLabel's content.
	void updateRecordInfo();

	void resetParameters();

Q_SIGNALS:
	void recordingChanged(bool);

private:
	bool canDisplayRecorderParametersEditor() const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @}
// end Gui

#endif
