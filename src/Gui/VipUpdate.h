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

#ifndef VIP_UPDATE_H
#define VIP_UPDATE_H

#include "VipConfig.h"
#include <qobject.h>

class QProcess;

/// VipUpdate is used to update a Thermavip copy based on the vipupdate process.
/// For VipUpdate to work properly, you need the vipupdate process in the same directory as your Thermavip installation.
class VIP_CORE_EXPORT VipUpdate : public QObject
{
	Q_OBJECT

public:
	VipUpdate(QObject* parent = nullptr);
	~VipUpdate();

	static QString getUpdateProgram();

public Q_SLOTS:

	/// Stops the current process (download or update)
	bool stop();

	/// Checks if updates are available based on given output directory containing the Thermavip installation.
	int hasUpdate(const QString& out_dir, bool* already_downloaded = nullptr, void** stop = nullptr);

	/// Tells if all updates has been downloaded
	bool isDownloadFinished();

	/// Download the updates based on given output directory, but do not copy or remove files into the output directory
	bool startDownload(const QString& out_dir);

	/// Updates output directory. This will download all needed files (if required) and copy them to the output directory.
	bool startUpdate(const QString& out_dir);

	/// When this object is destroyed, the current process (update or download) will keep going if detachedOnQuit is true
	void setDetachedOnQuit(bool enable);
	bool detachedOnQuit() const;

	/// Returns the underlying QProcess object
	QProcess* process();

	/// When updating Thermavip, some new files might not be copied into the installation directory, mainly because Thermavip is still running.
	///  In this case, the files are still copied to the installation directory, but with a name ending with '.vipnewfile'.
	///  This function recursively walks through the installation directory and tries to rename all new files by removing the trailing '.vipnewfile'.
	bool renameNewFiles(const QString& dir_name);

private Q_SLOTS:
	void emitFinished();
	void newOutput();

Q_SIGNALS:
	void updateProgressed(int);
	void finished();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
