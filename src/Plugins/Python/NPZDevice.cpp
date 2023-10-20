#include "NPZDevice.h"


class NPZDevice::PrivateData
{
public:
	VipNDArray previous;
	QString dataname;
};


NPZDevice::NPZDevice(QObject* parent)
	:VipIODevice(parent)
{
	d_data = new PrivateData();
}

NPZDevice::~NPZDevice()
{
	close();
	delete d_data;
}

bool NPZDevice::open(VipIODevice::OpenModes mode)
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

void NPZDevice::apply()
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
		QString code =
			"import numpy as np\n"
			"try: \n"
			"  if " + varname + ".shape == " + newname + ".shape: " + varname + ".shape=(1,*" + varname + ".shape)\n"
			"  " + newname + ".shape=(1,*" + newname + ".shape)\n"
			"  " + varname + " = np.vstack((" + varname + "," + newname + "))\n"
			"except:\n"
			"  " + varname + "=" + newname + "\n";

		//vip_debug("%s\n", code.toLatin1().data());

		PyIOOperation::command_type c = GetPyOptions()->sendObject(newname, QVariant::fromValue(ar));
		//check sending errors
		PyError lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
		if (!lastError.isNull())
		{
			setError(lastError.traceback);
			return;
		}

		c = GetPyOptions()->execCode(code);
		lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
		if (!lastError.isNull())
		{
			setError(lastError.traceback);
			return;
		}
	}
}


void NPZDevice::close()
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
		code =
			"import numpy as np\n"
			"np.savez('" + file + "', " + dataname + "=" + varname + ")\n"
			"del " + varname + "\n"
			"del " + newname;
	}
	
	d_data->dataname.clear();
	d_data->previous = VipNDArray();

	PyIOOperation::command_type c = GetPyOptions()->execCode(code);
	PyError lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
	if (!lastError.isNull())
	{
		setError(lastError.traceback);
		return;
	}
}



	




















class MATDevice::PrivateData
{
public:
	VipNDArray previous;
	QString dataname;
};


MATDevice::MATDevice(QObject* parent)
	:VipIODevice(parent)
{
	d_data = new PrivateData();
}

MATDevice::~MATDevice()
{
	close();
	delete d_data;
}

bool MATDevice::open(VipIODevice::OpenModes mode)
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

void MATDevice::apply()
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
		QString code =
			"import numpy as np\n"
			"try: \n"
			"  if " + varname + ".shape == " + newname + ".shape: " + varname + ".shape=(1,*" + varname + ".shape)\n"
			"  " + newname + ".shape=(1,*" + newname + ".shape)\n"
			"  " + varname + " = np.vstack((" + varname + "," + newname + "))\n"
			"except:\n"
			"  " + varname + "=" + newname + "\n";

		//vip_debug("%s\n", code.toLatin1().data());

		PyIOOperation::command_type c = GetPyOptions()->sendObject(newname, QVariant::fromValue(ar));
		//check sending errors
		PyError lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
		if (!lastError.isNull())
		{
			setError(lastError.traceback);
			return;
		}

		c = GetPyOptions()->execCode(code);
		lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
		if (!lastError.isNull())
		{
			setError(lastError.traceback);
			return;
		}
	}
}

void MATDevice::close()
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
	
		
	code =
		"from scipy.io import savemat\n"
		"d={'" + dataname + "':" + varname + "}\n"
		//"print(d)\n"
		"savemat('" + file + "', d)\n"
		"del " + varname + "\n"
		"del " + newname + "\n"
		"del d";
	

	//vip_debug("%s\n", code.toLatin1().data());

	d_data->dataname.clear();
	d_data->previous = VipNDArray();

	PyIOOperation::command_type c = GetPyOptions()->execCode(code);
	PyError lastError = GetPyOptions()->wait(c, 10000).value<PyError>();
	if (!lastError.isNull())
	{
		setError(lastError.traceback);
		return;
	}
}



