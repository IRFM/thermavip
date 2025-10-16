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

#ifndef VIP_CONCATENATE_VIDEO_H
#define VIP_CONCATENATE_VIDEO_H

#include "VipIODevice.h"


/// @brief IO device that concatenate multiple videos into a single one
class VIP_CORE_EXPORT VipConcatenateVideos : public VipTimeRangeBasedGenerator
{
	Q_OBJECT
	VIP_IO(VipProperty StartTimeS) // Start time for each sub video (relative to video start). This can lead to ignored files.
	VIP_IO(VipProperty EndTimeS) // End time for each sub video (relative to video start). This can lead to ignored files.
	VIP_IO(VipProperty FrameOutOf) // Take one frame out of N for each sub-video (always keep one frame)
	VIP_IO(VipProperty Bufferize) // Bufferize output data on opening (might take a lot of memory), true by default
	VIP_IO(VipOutput Image)
public:
	// Tells how to sort files with listFiles().
	enum SortOption
	{
		Sorted ,			// Alphabetical sorted
		Reversed ,			// Alphabetical reversed
		UseTrailingNumber	// Use trailing number before the suffix.
							// The trailing number is after a '.', ';', '-' or '_'.
							// Only works if all found files ends with a number.
	};

	using VipIODeviceSPtr = QSharedPointer<VipIODevice>;

	struct Frame
	{
		VipIODeviceSPtr device;	// frame device (might be null if bufferized)
		QString path; // frame device path
		qint64 pos; // frame position within device
		VipAnyData any; // output data (if bufferized)
		qint64 time; // absolute frame time
	};
	using FrameVector = QVector<Frame>;

	VipConcatenateVideos(QObject* parent = nullptr);
	~VipConcatenateVideos();


	void setSuffixTemplate(const QString& suffix, VipIODevice* device);
	virtual void setSourceProperty(const char* name, const QVariant& value);
	virtual bool probe(const QString& filename, const QByteArray&) const;
	virtual bool open(VipIODevice::OpenModes mode);


	FrameVector frames() const;
	void setFrames(const FrameVector& frs);
	int deviceCount() const;

	static QStringList listFiles(VipMapFileSystem* map, const QString& dirname, const QStringList& suffixes, SortOption sort, bool recursive = false);



protected:
	virtual bool readData(qint64 time);

private:
	VIP_DECLARE_PRIVATE_DATA();
};


/// @brief Small class managing a VipConcatenateVideos in order to filter input devices
class VIP_CORE_EXPORT VipConcatenateVideosManager : public QObject
{
	Q_OBJECT

public:
	VipConcatenateVideosManager(VipConcatenateVideos* device = nullptr);
	~VipConcatenateVideosManager();

	void setDevice(VipConcatenateVideos* device);
	VipConcatenateVideos* device() const;

	int undoCount() const;
	int redoCount() const;

public Q_SLOTS:
	bool removeDeviceAtTime(qint64 time);
	bool removeDeviceAtPos(qint64 pos);
	void undo();
	void redo();
	void resetState();

Q_SIGNALS:
	void undoDone();
	void redoDone();

private:
	VipConcatenateVideos::FrameVector saveState();
	VIP_DECLARE_PRIVATE_DATA();
};


#endif