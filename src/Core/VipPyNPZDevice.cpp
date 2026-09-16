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

#include <QMutex>

#include "VipLogging.h"
#include "VipPyNPZDevice.h"

class VipPyNPZDevice::PrivateData
{
public:
	// Written by apply(), which runs in the thread of the task pool, and read by
	// close(), which any thread may call and which the destructor calls too. Both
	// are implicitly shared, so an assignment racing a copy loses a reference.
	QMutex mutex;
	VipNDArray previous;
	QString dataname;
};

VipPyNPZDevice::VipPyNPZDevice(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA();
}

// A failed write has no return path: close() gives nothing back and the
// destructor calls it too. It is logged either way, and only reported on the
// object itself while that object is still whole.
static void reportWriteFailure(VipIODevice* device, const QString& path, const QString& traceback, bool destroying)
{
	VIP_LOG_ERROR("Cannot write " + path + ": " + traceback);
	if (!destroying)
		device->setError(traceback);
}

VipPyNPZDevice::~VipPyNPZDevice()
{
	// Not close(): a virtual does not dispatch from here, and the wait is kept
	// short because the destruction usually runs in the thread serving the
	// interface. The recording is still written. The input is closed and the
	// scheduled work waited for first, so that apply() is not still writing the
	// members read below.
	setEnabled(false);
	wait(false, 2000);
	writeRecording(2000, true);
}

bool VipPyNPZDevice::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	// The path is checked before the previous recording is flushed and cleared: an
	// open that ends up refusing the extension used to write and purge it first.
	QString p = removePrefix(path());
	if (!p.endsWith(".npz"))
		return false;

	close();

	setOpenMode(mode);
	return true;
}

void VipPyNPZDevice::apply()
{
	while (inputAt(0)->hasNewData()) {
		VipAnyData any = inputAt(0)->data();
		VipNDArray ar = any.value<VipNDArray>();
		if (ar.isEmpty()) {
			setError("Empty input array");
			return;
		}
		bool mismatch = false;
		{
			QMutexLocker lock(&d_data->mutex);
			if (!d_data->previous.isEmpty() && ar.shape() != d_data->previous.shape())
				mismatch = true;
			else {
				d_data->dataname = any.name();
				d_data->previous = ar;
			}
		}
		if (mismatch) {
			setError("Shape mismatch");
			return;
		}

		QString varname = "arr" + QString::number((qint64)this);
		QString newname = "new" + QString::number((qint64)this);
		// A bare except caught everything and assigned the last image to the
		// accumulator, so one failed stack part way through a recording replaced the
		// whole sequence acquired so far with a single frame, without a word. The
		// two cases are told apart: the first frame starts the stack, a later one is
		// appended, and a real failure is reported instead of swallowed.
		QString code = "import numpy as np\n"
			       "if '" +
			       varname + "' not in globals():\n"
			       "  " +
			       varname + " = " + newname + ".reshape((1, *" + newname +
			       ".shape))\n"
			       "else:\n"
			       "  " +
			       varname + " = np.vstack((" + varname + ", " + newname + ".reshape((1, *" + newname +
			       ".shape))))\n";

		// vip_debug("%s\n", code.toLatin1().data());

		VipPyError lastError = VipPyInterpreter::instance()->sendObject(newname, QVariant::fromValue(ar)).value(10000).value<VipPyError>();
		// check sending errors
		if (!lastError.isNull()) {
			setError(lastError.traceback);
			return;
		}

		lastError = VipPyInterpreter::instance()->execCode(code).value(10000).value<VipPyError>();
		if (!lastError.isNull()) {
			setError(lastError.traceback);
			return;
		}
	}
}

void VipPyNPZDevice::close()
{
	// First: it disables the input and waits for the scheduled processings, so
	// that no apply() is still writing what is read below.
	VipIODevice::close();
	writeRecording(10000, false);
}

void VipPyNPZDevice::writeRecording(int timeout_ms, bool destroying)
{
	QString dataname;
	{
		QMutexLocker lock(&d_data->mutex);
		if (d_data->previous.isEmpty())
			return;
		dataname = d_data->dataname;
	}
	if (dataname.isEmpty())
		dataname = "arr_0";
	else {
		for (int i = 0; i < dataname.size(); ++i)
			if (!dataname[i].isLetterOrNumber())
				dataname[i] = '_';
	}
	QString tmp;
	for (int i = 0; i < dataname.size(); ++i)
		if (!(i > 0 && dataname[i] == '_' && dataname[i - 1] == '_'))
			tmp.push_back(dataname[i]);
	if (tmp.isEmpty())
		tmp = "arr_0";
	else
		tmp = "arr_" + tmp;
	dataname = tmp;

	QString varname = "arr" + QString::number((qint64)this);
	QString newname = "new" + QString::number((qint64)this);

	QString file = removePrefix(path());
	file.replace("\\", "/");

	// The path used to be pasted between quotes in the generated source. It is not
	// the user's alone: a device path is written to the session file and restored
	// from it unchecked, so a single quote in it closed the literal and the rest ran
	// as Python. Send it as an object, like the array itself, so that only generated
	// identifiers appear in the source.
	const QString pathvar = "pth" + QString::number((qint64)this);
	const QString namevar = "nam" + QString::number((qint64)this);

	VipPyError lastError = VipPyInterpreter::instance()->sendObject(pathvar, QVariant::fromValue(file)).value(timeout_ms).value<VipPyError>();
	if (!lastError.isNull()) {
		reportWriteFailure(this, path(), lastError.traceback, destroying);
		return;
	}
	lastError = VipPyInterpreter::instance()->sendObject(namevar, QVariant::fromValue(dataname)).value(timeout_ms).value<VipPyError>();
	if (!lastError.isNull()) {
		reportWriteFailure(this, path(), lastError.traceback, destroying);
		return;
	}

	const QString code = "import numpy as np\n"
			     "np.savez(" + pathvar + ", **{" + namevar + ": " + varname + "})\n"
			     "del " + varname + "\n"
			     "del " + newname + "\n"
			     "del " + pathvar + "\n"
			     "del " + namevar;

	{
		QMutexLocker lock(&d_data->mutex);
		d_data->dataname.clear();
		d_data->previous = VipNDArray();
	}

	lastError = VipPyInterpreter::instance()->execCode(code).value(timeout_ms).value<VipPyError>();
	if (!lastError.isNull()) {
		reportWriteFailure(this, path(), lastError.traceback, destroying);
		return;
	}
}

class VipPyMATDevice::PrivateData
{
public:
	// Written by apply(), which runs in the thread of the task pool, and read by
	// close(), which any thread may call and which the destructor calls too. Both
	// are implicitly shared, so an assignment racing a copy loses a reference.
	QMutex mutex;
	VipNDArray previous;
	QString dataname;
};

VipPyMATDevice::VipPyMATDevice(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA();
}

VipPyMATDevice::~VipPyMATDevice()
{
	// Not close(): a virtual does not dispatch from here, and the wait is kept
	// short because the destruction usually runs in the thread serving the
	// interface. The recording is still written. The input is closed and the
	// scheduled work waited for first, so that apply() is not still writing the
	// members read below.
	setEnabled(false);
	wait(false, 2000);
	writeRecording(2000, true);
}

bool VipPyMATDevice::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	// Same order as the NPZ device above.
	QString p = removePrefix(path());
	if (!p.endsWith(".mat"))
		return false;

	close();

	setOpenMode(mode);
	return true;
}

void VipPyMATDevice::apply()
{
	while (inputAt(0)->hasNewData()) {
		VipAnyData any = inputAt(0)->data();
		VipNDArray ar = any.value<VipNDArray>();
		if (ar.isEmpty()) {
			setError("Empty input array");
			return;
		}
		bool mismatch = false;
		{
			QMutexLocker lock(&d_data->mutex);
			if (!d_data->previous.isEmpty() && ar.shape() != d_data->previous.shape())
				mismatch = true;
			else {
				d_data->dataname = any.name();
				d_data->previous = ar;
			}
		}
		if (mismatch) {
			setError("Shape mismatch");
			return;
		}

		QString varname = "arr" + QString::number((qint64)this);
		QString newname = "new" + QString::number((qint64)this);
		// A bare except caught everything and assigned the last image to the
		// accumulator, so one failed stack part way through a recording replaced
		// the whole sequence acquired so far with a single frame, without a word.
		// The two cases are told apart: the first frame starts the stack, a later
		// one is appended, and a real failure is reported instead of swallowed.
		QString code = "import numpy as np\n"
			       "if '" +
			       varname + "' not in globals():\n"
			       "  " +
			       varname + " = " + newname + ".reshape((1, *" + newname +
			       ".shape))\n"
			       "else:\n"
			       "  " +
			       varname + " = np.vstack((" + varname + ", " + newname + ".reshape((1, *" + newname +
			       ".shape))))\n";

		// vip_debug("%s\n", code.toLatin1().data());

		VipPyError lastError = VipPyInterpreter::instance()->sendObject(newname, QVariant::fromValue(ar)).value(10000).value<VipPyError>();
		// check sending errors
		if (!lastError.isNull()) {
			setError(lastError.traceback);
			return;
		}

		lastError = VipPyInterpreter::instance()->execCode(code).value(10000).value<VipPyError>();
		if (!lastError.isNull()) {
			setError(lastError.traceback);
			return;
		}
	}
}

void VipPyMATDevice::close()
{
	// First: it disables the input and waits for the scheduled processings, so
	// that no apply() is still writing what is read below.
	VipIODevice::close();
	writeRecording(10000, false);
}

void VipPyMATDevice::writeRecording(int timeout_ms, bool destroying)
{
	QString dataname;
	{
		QMutexLocker lock(&d_data->mutex);
		if (d_data->previous.isEmpty())
			return;
		dataname = d_data->dataname;
	}
	if (dataname.isEmpty())
		dataname = "arr_0";
	else {
		for (int i = 0; i < dataname.size(); ++i)
			if (!dataname[i].isLetterOrNumber())
				dataname[i] = '_';
	}
	QString tmp;
	for (int i = 0; i < dataname.size(); ++i)
		if (!(i > 0 && dataname[i] == '_' && dataname[i - 1] == '_'))
			tmp.push_back(dataname[i]);
	if (tmp.isEmpty())
		tmp = "arr_0";
	else
		tmp = "arr_" + tmp;
	dataname = tmp;

	QString varname = "arr" + QString::number((qint64)this);
	QString newname = "new" + QString::number((qint64)this);

	QString file = removePrefix(path());
	file.replace("\\", "/");

	// Same as the NPZ device above: the path comes back from a session file, so it
	// travels as an object rather than as source text.
	const QString pathvar = "pth" + QString::number((qint64)this);
	const QString namevar = "nam" + QString::number((qint64)this);

	VipPyError lastError = VipPyInterpreter::instance()->sendObject(pathvar, QVariant::fromValue(file)).value(timeout_ms).value<VipPyError>();
	if (!lastError.isNull()) {
		reportWriteFailure(this, path(), lastError.traceback, destroying);
		return;
	}
	lastError = VipPyInterpreter::instance()->sendObject(namevar, QVariant::fromValue(dataname)).value(timeout_ms).value<VipPyError>();
	if (!lastError.isNull()) {
		reportWriteFailure(this, path(), lastError.traceback, destroying);
		return;
	}

	const QString code = "from scipy.io import savemat\n"
			     "d={" + namevar + ": " + varname + "}\n"
			     "savemat(" + pathvar + ", d)\n"
			     "del " + varname + "\n"
			     "del " + newname + "\n"
			     "del " + pathvar + "\n"
			     "del " + namevar + "\n"
			     "del d";

	{
		QMutexLocker lock(&d_data->mutex);
		d_data->dataname.clear();
		d_data->previous = VipNDArray();
	}

	lastError = VipPyInterpreter::instance()->execCode(code).value(timeout_ms).value<VipPyError>();
	if (!lastError.isNull()) {
		reportWriteFailure(this, path(), lastError.traceback, destroying);
		return;
	}
}
