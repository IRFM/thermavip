#pragma once

#include "VipIODevice.h"

/**
Simple text file format that defines a session.
The file contains a list of signals to display in CSV like format that will be translated to Python in order to be opened.
The file is of the form:

//"Signal name"	"workspace"		"player id"		"y pos"		"x pos"		"stylesheet"
"54629;SMAG_IP"			0				0			0			0			"color: red;"
"54629;SMAG_TCUB"		0				1			1			0			"color: blue;" //2 signals in the same multi player (top and bottom)
"54629;SMAG_UTOR"		0				1			0			0			"color: red;"

Each column must be separated by one or more tabulations.
Ech comment line must start with '//'.
Comments at the end of a column are not supported.
*/
class SimplifiedSession : public VipFileHandler
{
	Q_OBJECT
	Q_CLASSINFO("description", "Simplified session file")
public:
	virtual QString fileFilters() const { return "Simplified session files (*.ssf)"; }
	bool probe(const QString &filename, const QByteArray &) const { return QFileInfo(removePrefix(filename)).suffix().compare("ssf", Qt::CaseInsensitive) == 0; }
	virtual bool open(const QString & path, QString * error);
};
VIP_REGISTER_QOBJECT_METATYPE(SimplifiedSession*)

