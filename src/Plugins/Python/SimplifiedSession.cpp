#include "SimplifiedSession.h"

#include "VipDisplayArea.h"
#include "VipProgress.h"

#include "PyOperation.h"

#include <qfile.h>

static bool cleanLine(const QByteArray & ar, int & start, int & end)
{
	if (ar.isEmpty()) return false;
	start = 0;
	while (start < ar.size() && (ar[start] == ' ' || ar[start] == '\t' || ar[start] == '\r' || ar[start] == '\n'))
		++start;
	if (start == ar.size()) return false;
	end = ar.size() - 1;
	while (end >= start && (ar[end] == ' ' || ar[end] == '\t' || ar[end] == '\r' || ar[end] == '\n'))
		--end;
	if (end < 0) return false;
	++end;
	return true;
}
static QByteArray cleanLine(const QByteArray & ar)
{
	int s, e;
	if (!cleanLine(ar, s, e))return QByteArray();
	return ar.mid(s, e - s);
}
static QByteArray removeQuotes(const QByteArray & ar, bool *ok)
{
	if (ar.startsWith("\"")) {
		if (!ar.endsWith("\"")) {
			if (ok) *ok = false;
			return QByteArray();
		}
		if (ok) *ok = true;
		return ar.mid(1, ar.size() - 2);
	}
	if (ar.startsWith("'")) {
		if (!ar.endsWith("'")) {
			if (ok) *ok = false;
			return QByteArray();
		}
		if (ok) *ok = true;
		return ar.mid(1, ar.size() - 2);
	}
	if (ok) *ok = true;
	return ar;
}
static QString createVarName(int & var_count) {
	return "var" + QString::number(var_count++);
}

struct Signal
{
	QByteArray name;
	QByteArray stylesheet;
	Signal(const QByteArray & n = QByteArray(), const QByteArray & s = QByteArray())
		:name(n), stylesheet(s) {}
};
struct PlayerRow
{
	QMap<int, QList<Signal> > columns;
};
struct Player
{
	QMap<int, PlayerRow> rows;
};
struct Workspace
{
	QMap<int, Player > players;
};



/**
Simple CSV file format that defines a session.
The file contains a list of signals to display in CSV format that will be translated to Python in order to be opened.
The file is of the form:

"Signal name"	"workspace"		"player id"		"y pos"		"x pos"		"stylesheet"
"54629;SMAG_IP"			0				0				0			0			"color: red;"
"54629;SMAG_TCUB"		0				1				1			0			"color: blue;" //2 signals in the same multi player (top and bottom)
"54629;SMAG_UTOR"		0				1				0			0			"color: red;"
*/

bool SimplifiedSession::open(const QString & file, QString * error)
{
	QFile fin(file);
	if (!fin.open(QFile::ReadOnly | QFile::Text)) {
		if (error) *error = "Unknown file '" + file + "'";
		return false;
	}

	QStringList lines = QString(fin.readAll()).split("\n", QString::SkipEmptyParts);
	if (lines.isEmpty()) {
		if (error) *error = "Empty file '" + file + "'";
		return false;
	}


	QMap<int, Workspace> workspaces;

	for (int i = 0; i < lines.size(); ++i)
	{
		QByteArray ar = lines[i].toLatin1();
		ar = cleanLine(ar);
		if (ar.startsWith("//"))
			continue;

		//extract values from line
		QList<QByteArray> values = ar.split('\t');
		//remove empty values;
		for(int i=0; i < values.size(); ++i)
			if (values[i].isEmpty()) {
				values.removeAt(i);
				--i;
			}

		if (values.size() < 5 || values.size() > 6) {
			if (error) *error = "synatx error at line " + QString::number(i + 1);
			return false;
		}

		bool ok;

		//read signal name
		QByteArray name = removeQuotes(cleanLine(values[0]), &ok);
		if (!ok || name.isEmpty()) {
			if (error) *error = "synatx error at line " + QString::number(i + 1) + ", column " + QString::number(1);
			return false;
		}

		//read workspace
		int wks = values[1].toInt(&ok);
		if (!ok) {
			if (error) *error = "synatx error at line " + QString::number(i + 1) + ", column " + QString::number(2);
			return false;
		}

		//read player
		int player = values[2].toInt(&ok);
		if (!ok) {
			if (error) *error = "synatx error at line " + QString::number(i + 1) + ", column " + QString::number(3);
			return false;
		}

		//read y pos
		int ypos = values[3].toInt(&ok);
		if (!ok) {
			if (error) *error = "synatx error at line " + QString::number(i + 1) + ", column " + QString::number(4);
			return false;
		}

		//read x pos
		int xpos = values[4].toInt(&ok);
		if (!ok) {
			if (error) *error = "synatx error at line " + QString::number(i + 1) + ", column " + QString::number(5);
			return false;
		}

		QByteArray stylesheet;
		if (values.size() == 6)
			stylesheet = removeQuotes(cleanLine(values[5]),&ok);

		workspaces[wks].players[player].rows[ypos].columns[xpos].append(Signal(name, stylesheet));
	}

	//now, translate to Python

	QStringList code;
	QStringList comment;

	code << "import Thermavip as th";
	comment << QString();

	for (QMap<int, Workspace>::const_iterator it = workspaces.begin(); it != workspaces.end(); ++it)
	{
		//create workspace except for id of 0
		if (it.key() != 0) {
			code << "th.workspace()";
			comment << "create workspace";
		}
		 
		Workspace wks = it.value();
		for (QMap<int, Player >::const_iterator pls = wks.players.begin(); pls != wks.players.end(); ++pls)
		{
			bool first_player = true;
			Player pl = pls.value();
			for (QMap<int, PlayerRow>::const_iterator row = pl.rows.begin(); row != pl.rows.end(); ++row)
			{
				bool new_row = true;
				PlayerRow r = row.value();
				for (QMap<int, QList<Signal> >::const_iterator col = r.columns.begin(); col != r.columns.end(); ++col)
				{
					bool new_col = true;
					QList<Signal> sigs = col.value();
					for (int i = 0; i < sigs.size(); ++i) {

						//open signal
						if (first_player) {
							code << "id = th.open('" + sigs[i].name + "')";
							first_player = false;
							new_row = false;
							new_col = false;
						}
						else if (new_row) {
							code << "id = th.open('" + sigs[i].name + "', id, 'bottom')";
							new_row = false;
							new_col = false;
						}
						else if(new_col){
							code << "id = th.open('" + sigs[i].name + "', id, 'right')";
							new_col = false;
						}
						else
							code << "id = th.open('" + sigs[i].name + "', id)";

						comment << "open " + sigs[i].name;

						//set stylesheet
						if (sigs[i].stylesheet.size()) {
							code << "th.set_stylesheet(id, '" + sigs[i].stylesheet + "')";
							comment << "set style to " + sigs[i].name;
						}
					}
				}
			}
		}

	}

	//we have the code, execute it

	VipProgress p;
	p.setRange(0, code.size());
	p.setModal(true);
	p.setValue(0);

	for (int i = 0; i < code.size(); ++i) {

		//reorganize before creating a new workspace
		//if(code[i].contains("workspace"))
		//	vipGetMainWindow()->rearrangeTiles();

		p.setValue(i);
		p.setText(comment[i].isEmpty() ? " " : comment[i]);
		QVariant res = VipPyInterpreter::instance()->execCode(code[i]).value();
		if (!res.value<VipPyError>().isNull()) {
			if (error) *error = res.value<VipPyError>().traceback;
			return false; 
		}
	}

	//vipGetMainWindow()->rearrangeTiles();

	return true;
}


