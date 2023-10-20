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

#ifndef VIP_RIR_DEVICE_H
#define VIP_RIR_DEVICE_H

#include "VipIODevice.h"
#include "VipPlayer.h"

/// @brief IO device able to read video file format supported by the librir.
/// This includes:
/// -	HCC infrared video files
/// -	MP4 infrared video files compressed with h264 or hevc codecs
/// -	PCR raw video files
///
class VIP_ANNOTATION_EXPORT VipRIRDevice : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipOutput image)
	VIP_IO(VipProperty Calibration) // selected calibration as an integer
	VIP_IO(VipProperty BadPixels)	// selected calibration as an integer

public:
	VipRIRDevice(QObject* parent = nullptr);
	~VipRIRDevice();

	virtual bool open(VipIODevice::OpenModes mode);
	virtual QString fileFilters() const { return "Librir infrared video files (*.pcr *.bin *.h264 *.h265 *.hcc)"; }
	virtual void close();
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }
	virtual int camera() const { return m_file; }
	virtual QStringList calibrations() const { return m_calibrations; }
	virtual QSize imageSize() const { return m_size; }
	virtual int getRawValue(int x, int y);

	VipNDArray lastRawImage() const { return m_raw; }

	virtual QString fileName() const;

protected:
	virtual bool readData(qint64 time);

private:
	int m_file;
	int m_count;
	QSize m_size;
	VipNDArray m_raw;
	QStringList m_calibrations;
};

VIP_REGISTER_QOBJECT_METATYPE(VipRIRDevice*)

/**
Widget to edit a VipRIRDevice instance (either WEST_IR_Device or WEST_BIN_PCR_Device)
*/
class VipRIRDeviceEditor : public QObject
{
	Q_OBJECT

public:
	VipRIRDeviceEditor(VipVideoPlayer* player);
	~VipRIRDeviceEditor();

	void setDevice(VipRIRDevice* dev);
	VipRIRDevice* device() const;

public Q_SLOTS:
	void updateDevice();
	void setBadPixels(bool);

Q_SIGNALS:
	void deviceUpdated();

private:
	class PrivateData;
	PrivateData* m_data;
};

/**
Customize a VipVideoPlayer for VipRIRDevice instance
*/
class CustomizeRIRVideoPlayer : public QObject
{
	Q_OBJECT
	QPointer<VipRIRDevice> m_device;
	QPointer<VipVideoPlayer> m_player;
	VipRIRDeviceEditor* m_options;

public:
	CustomizeRIRVideoPlayer(VipVideoPlayer* player, VipRIRDevice* device);
};

#endif