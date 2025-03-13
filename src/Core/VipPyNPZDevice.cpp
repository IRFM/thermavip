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

#include "VipPyNPZDevice.h"

class VipPyNPZDevice::PrivateData
{
public:
	VipNDArray previous;
	QString dataname;
};

VipPyNPZDevice::VipPyNPZDevice(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipPyNPZDevice::~VipPyNPZDevice()
{
	close();
}

bool VipPyNPZDevice::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	close();

	QString p = removePrefix(path());
	if (!p.endsWith(".npz"))
		return false;

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
		d_data->dataname = any.name();

		if (!d_data->previous.isEmpty() && ar.shape() != d_data->previous.shape()) {
			setError("Shape mismatch");
			return;
		}

		d_data->previous = ar;

		QString varname = "arr" + QString::number((qint64)this);
		QString newname = "new" + QString::number((qint64)this);
		QString code = "import numpy as np\n"
			       "try: \n"
			       "  if " +
			       varname + ".shape == " + newname + ".shape: " + varname + ".shape=(1,*" + varname +
			       ".shape)\n"
			       "  " +
			       newname + ".shape=(1,*" + newname +
			       ".shape)\n"
			       "  " +
			       varname + " = np.vstack((" + varname + "," + newname +
			       "))\n"
			       "except:\n"
			       "  " +
			       varname + "=" + newname + "\n";

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
	if (d_data->previous.isEmpty())
		return;
	QString dataname = d_data->dataname;
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
	QString code;
	{
		code = "import numpy as np\n"
		       "np.savez('" +
		       file + "', " + dataname + "=" + varname +
		       ")\n"
		       "del " +
		       varname +
		       "\n"
		       "del " +
		       newname;
	}

	d_data->dataname.clear();
	d_data->previous = VipNDArray();

	VipPyError lastError = VipPyInterpreter::instance()->execCode(code).value(10000).value<VipPyError>();
	if (!lastError.isNull()) {
		setError(lastError.traceback);
		return;
	}
}

class VipPyMATDevice::PrivateData
{
public:
	VipNDArray previous;
	QString dataname;
};

VipPyMATDevice::VipPyMATDevice(QObject* parent)
  : VipIODevice(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
}

VipPyMATDevice::~VipPyMATDevice()
{
	close();
}

bool VipPyMATDevice::open(VipIODevice::OpenModes mode)
{
	if (mode != WriteOnly)
		return false;

	close();

	QString p = removePrefix(path());
	if (!p.endsWith(".mat"))
		return false;

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
		d_data->dataname = any.name();

		if (!d_data->previous.isEmpty() && ar.shape() != d_data->previous.shape()) {
			setError("Shape mismatch");
			return;
		}

		d_data->previous = ar;

		QString varname = "arr" + QString::number((qint64)this);
		QString newname = "new" + QString::number((qint64)this);
		QString code = "import numpy as np\n"
			       "try: \n"
			       "  if " +
			       varname + ".shape == " + newname + ".shape: " + varname + ".shape=(1,*" + varname +
			       ".shape)\n"
			       "  " +
			       newname + ".shape=(1,*" + newname +
			       ".shape)\n"
			       "  " +
			       varname + " = np.vstack((" + varname + "," + newname +
			       "))\n"
			       "except:\n"
			       "  " +
			       varname + "=" + newname + "\n";

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
	if (d_data->previous.isEmpty())
		return;
	QString dataname = d_data->dataname;
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
	QString code;

	code = "from scipy.io import savemat\n"
	       "d={'" +
	       dataname + "':" + varname +
	       "}\n"
	       //"print(d)\n"
	       "savemat('" +
	       file +
	       "', d)\n"
	       "del " +
	       varname +
	       "\n"
	       "del " +
	       newname +
	       "\n"
	       "del d";

	// vip_debug("%s\n", code.toLatin1().data());

	d_data->dataname.clear();
	d_data->previous = VipNDArray();

	VipPyError lastError = VipPyInterpreter::instance()->execCode(code).value(10000).value<VipPyError>();
	if (!lastError.isNull()) {
		setError(lastError.traceback);
		return;
	}
}
