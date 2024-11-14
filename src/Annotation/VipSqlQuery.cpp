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

#include "VipSqlQuery.h"
#include "VipStandardWidgets.h"
#include "VipNetwork.h"

#include <QStandardPaths>
#include <qboxlayout.h>
#include <qgridlayout.h>
#include <qstandarditemmodel.h>

static int register_db_pulse_type()
{
	qRegisterMetaType<Vip_experiment_id>("Vip_experiment_id");
	return 0;
}
static int _register_db_pulse_type = register_db_pulse_type();

VipRequestCondition vipRequestCondition(const QString& varname, const QVariant& min, const QVariant& max, VipRequestCondition::Separator sep)
{
	VipRequestCondition c;
	c.varname = varname;
	c.min = min;
	c.max = max;
	c.sep = sep;
	return c;
}
VipRequestCondition vipRequestCondition(const QString& varname, const QString& equal)
{
	VipRequestCondition c;
	c.varname = varname;
	c.equal = equal;
	return c;
}
VipRequestCondition vipRequestCondition(const QString& varname, const QStringList& one_of_enum)
{
	VipRequestCondition c;
	c.varname = varname;
	c.enums = one_of_enum;
	c.sep = VipRequestCondition::OR;
	return c;
}

static QString sepToStr(VipRequestCondition::Separator sep)
{
	if (sep == VipRequestCondition::OR)
		return "OR";
	else
		return "AND";
}
static QString addQuotes(const QString& str)
{
	if (!str.startsWith("'"))
		return "'" + str + "'";
	return str;
}

QString vipFormatRequestCondition(const VipRequestCondition& c)
{
	if (c.min.userType() != 0 || c.max.userType() != 0) {
		QString cond = "(";
		if (c.min.userType() != 0)
			cond += c.varname + " > " + c.min.toString();
		if (c.max.userType() != 0) {
			if (c.min.userType() != 0)
				cond += " " + sepToStr(c.sep) + " ";
			cond += c.varname + " < " + c.max.toString();
		}
		cond += ")";
		return cond;
	}
	else if (!c.equal.isEmpty()) {
		QString eq = addQuotes(c.equal);
		return "(" + c.varname + " = " + eq + ")";
	}
	else if (c.enums.size()) {
		QStringList lst = c.enums;
		for (int i = 0; i < lst.size(); ++i)
			lst[i] = " " + c.varname + " = " + addQuotes(lst[i]) + " ";
		return "(" + lst.join(sepToStr(c.sep)) + ")";
	}
	return QString();
}

/*#include <qregion.h>
static int regionArea(const QRegion& reg)
{
	int pixel_area = 0;
	for (const QRect& r : reg)
		pixel_area += r.width() * r.height();
	return pixel_area;
}*/

#include "VipLogging.h"
#include <qsettings.h>
#include <qsqldatabase.h>
#include <qsqlerror.h>
#include <qsqlquery.h>

struct DB
{
	QString hostname;
	QString name;
	QString user;
	QString password;
	int port;
	QString sqlite_file;
	QString local_movie_folder;
	QString local_movie_suffix;
};

static QString removeQuote(const QString& line)
{
	QByteArray ar = line.toLatin1();
	// Remove '' or "" from string if it exists
	int ids = ar.indexOf("'");
	int idd = ar.indexOf('"');
	if (ids == -1 && idd == -1)
		return line; // no double quote, find

	int start = 0;
	char q = '"';

	if (ids != -1) {
		if (idd == -1 || idd > ids) {
			start = ids;
			q = '\'';
		}
		else if (idd != -1) {
			start = idd;
			q = '"';
		}
	}
	else {
		start = idd;
		q = '"';
	}

	ids = ar.lastIndexOf("'");
	idd = ar.lastIndexOf('"');

	if (q == '"' && ids > idd)
		return QString(); // starts with ", finish with '
	if (q == '\'' && idd > ids)
		return QString(); // starts with ', finish with "

	int end = q == '"' ? idd : ids;
	ar = ar.mid(start + 1, end - start - 1);
	return ar;
}

const DB& readDB()
{
	static DB db;
	if (db.hostname.isEmpty()) {
		if (QFileInfo("./.env").exists()) {
			QSettings settings("./.env", QSettings::IniFormat);
			if (settings.status() != QSettings::FormatError) {
				db.hostname = removeQuote(settings.value("MYSQL_HOST").toString());
				if (db.hostname.contains(":")) {
					QStringList lst = db.hostname.split(":");
					db.hostname = lst[0];
					db.port = lst[1].toInt();
				}
				else
					db.port = 3306;

				db.name = removeQuote(settings.value("MYSQL_DATABASE").toString());
				db.user = removeQuote(settings.value("MYSQL_USER").toString());
				db.password = removeQuote(settings.value("MYSQL_PASSWORD").toString());
				db.sqlite_file = removeQuote(settings.value("SQLITE_DATABASE_FILE").toString());
				db.local_movie_folder = removeQuote(settings.value("LOCAL_MOVIE_FOLDER").toString());
				db.local_movie_suffix = removeQuote(settings.value("LOCAL_MOVIE_SUFFIX").toString());
				db.local_movie_folder.replace("\\", "/");
				if (db.local_movie_folder.endsWith("/"))
					db.local_movie_folder = db.local_movie_folder.mid(0, db.local_movie_folder.size() - 1);
				if (settings.value("SQLITE").toString() != "True")
					db.sqlite_file = QString();
			}
		}
	}
	else {
		db.hostname = "localhost";
		db.port = 3306;
	}

	return db;
}

static QSqlDatabase createConnection()
{
	static int try_count = 3;
	static QSqlDatabase db;
	if (!db.isOpen() && --try_count > 0) {
		QSqlDatabase::database("in_mem_db", false).close();
		QSqlDatabase::removeDatabase("in_mem_db");

		DB param = readDB();
		db = QSqlDatabase::addDatabase("QMYSQL");
		db.setConnectOptions("MYSQL_OPT_CONNECT_TIMEOUT=4");
		db.setHostName(param.hostname);
		db.setDatabaseName(param.name); //
		db.setUserName(param.user);	// root
		db.setPort(param.port);		// or 3307
		db.setPassword(param.password); //

		if (!db.isValid()) {
			VIP_LOG_ERROR("DataBase not valid!!!");
			return db;
		}

		// First, ping host. Indeed Qt mysql plugin might crash if host is unreachable
		QString host = param.hostname;
		if (host.contains(":"))
			host = host.split(":")[0];
		if (!vipPing(host.toLatin1())) {
			VIP_LOG_ERROR("Unable to reach host ", host, ", DataBase not valid!!!");
			return db;
		}

		if (!db.open()) {
			VIP_LOG_ERROR("DataBase not created!!!! " + db.lastError().text());
			return db;
		}
		// else
		//	VIP_LOG_INFO( "DataBase %s created!!!!", db.databaseName().toLatin1().data());
	}
	return db;
}

QStringList vipCamerasDB()
{
	static QStringList cameras;
	if (!cameras.isEmpty())
		return cameras;

	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM lines_of_sight;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}
	while (q.next()) {
		cameras.append(q.value(0).toString());
	}
	return cameras;
}
QStringList vipUsersDB()
{
	static QStringList users;
	if (!users.isEmpty())
		return users;
	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM users;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}
	while (q.next()) {
		users.append(q.value(0).toString());
	}
	return users;
}

QStringList vipAnalysisStatusDB()
{
	static QStringList status;
	if (!status.isEmpty())
		return status;
	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM analysis_status;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}
	while (q.next()) {
		status.append(q.value(0).toString());
	}
	return status;
}

QStringList vipDevicesDB()
{
	static QStringList devices;
	if (!devices.isEmpty())
		return devices;
	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM devices;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}
	while (q.next()) {
		devices.append(q.value(0).toString());
	}
	return devices;
}

QMap<int, VipDataset> vipDatasetsDB()
{
	static QMap<int, VipDataset> res;
	if (!res.isEmpty())
		return res;

	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM datasets;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QMap<int, VipDataset>();
	}
	while (q.next()) {
		VipDataset d;
		d.id = q.value("id").toInt();
		d.annotation_type = q.value("annotation_type").toString();
		d.creation_date = q.value("creation_date").toString();
		d.description = q.value("description").toString();
		res[d.id] = d;
	}
	return res;
}

QStringList vipMethodsDB()
{
	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM methods;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}
	QStringList methods;
	while (q.next()) {
		methods.append(q.value(0).toString());
	}

	return methods;
}

QStringList vipEventTypesDB()
{
	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM thermal_event_categories;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}
	QStringList events, cams;
	while (q.next()) {
		events.append(q.value(0).toString());
		cams.append(q.value(1).toString());
	}

	return events;
}

QStringList vipEventTypesDB(const QString& line_of_sight)
{
	QSqlDatabase db = createConnection();
	QSqlQuery q = db.exec("SELECT * FROM thermal_event_category_lines_of_sight;");
	if (!q.lastError().nativeErrorCode().isEmpty()) {
		VIP_LOG_ERROR(q.lastError().nativeErrorCode());
		return QStringList();
	}

	QStringList events;
	while (q.next()) {
		QString event = q.value(0).toString();
		QString cam = q.value(1).toString();
		if (cam.compare(line_of_sight, Qt::CaseInsensitive) == 0)
			events.push_back(event);
	}

	return events;
}

QString vipLocalMovieFolderDB()
{
	return readDB().local_movie_folder;
}

QString vipLocalMovieSuffix()
{
	return readDB().local_movie_suffix;
}

static QStringList _users;
bool vipHasReadRightsDB()
{
	return true; // vipHasWriteRightsDB();
}

bool vipHasWriteRightsDB()
{
	if (_users.isEmpty())
		_users = vipUsersDB();
	return _users.contains(vipUserName(), Qt::CaseInsensitive);
}

Vip_event_list vipCopyEvents(const Vip_event_list& events)
{
	Vip_event_list res;
	for (Vip_event_list::const_iterator it = events.begin(); it != events.end(); ++it) {
		const QList<VipShape> shs = it.value();
		QList<VipShape> lst;
		for (int i = 0; i < shs.size(); ++i) {
			lst.append(shs[i].copy());
		}
		res[it.key()] = lst;
	}
	return res;
}

template<class Point>
QString polygonToString(const QVector<Point>& poly)
{
	// check for rectangle, returns an empty string if polygon is a rectangle
	if (vipIsRect(poly))
		return QString();

	QString res;
	{
		QTextStream stream(&res, QIODevice::WriteOnly);
		for (int i = 0; i < poly.size(); ++i) {
			stream << qRound((double)poly[i].x()) << " " << qRound((double)poly[i].y()) << " ";
		}
	}
	return res;
}

/*static QPolygon floorPolygonF(const QPolygonF& p)
{
	QPolygon res(p.size());
	for (int i = 0; i < p.size(); ++i)
		// res[i] = QPoint(std::floor(p[i].x()), std::floor(p[i].y()));
		res[i] = p[i].toPoint();
	return res;
}*/

static void convertShape(const VipShape& sh, QPolygon& p, QRect& r)
{
	p.clear();
	r = QRect();

	if (sh.polygon().isEmpty()) {
		return;
	}

	/*const QRegion reg = sh.region();
	QPainterPath path;
	path.addRegion(reg);
	QPolygonF poly = path.simplified().toFillPolygon();
	p = poly.toPolygon();
	r = p.boundingRect();*/

	if (vipIsRect(sh.polygon())) {
		// special case for rectangle: use the same coordinates as when displaying the ROI with "Draw exact pixels"
		const QRegion reg = sh.region();
		r = reg.boundingRect();
		p << r.topLeft() << r.topRight() + QPoint(1, 0) << r.bottomRight() + QPoint(1, 1) << r.bottomLeft() + QPoint(0, 1);
	}
	else {
		p = sh.polygon().toPolygon();
		r = p.boundingRect();
		r.setRight(r.right() - 1);
		r.setBottom(r.bottom() - 1);
	}
}

QList<qint64> vipSendToDB(const QString& userName, const QString& camera, const QString& device, Vip_experiment_id pulse, const Vip_event_list& all_shapes, VipProgress* p)
{
	QSqlDatabase db = createConnection();
	if (!db.isOpen())
		return QList<qint64>();

	Vip_event_list shapes = all_shapes;
	/* for (int i = 0; i < lst.size(); ++i)
	{
		lst[i](shapes);
	}*/

	if (p) {
		p->setText("Send thermal events...");
		p->setRange(0, shapes.size());
	}

	// create a new entry in thermal_events
	int count = 0;
	QList<qint64> resids;
	for (Vip_event_list::iterator it = shapes.begin(); it != shapes.end(); ++it, ++count) {

		if (p) {
			p->setValue(count);
		}
		// get thermal event type
		QString thermal_event = it.value().first().group();

		// get confidence
		double confidence = it.value().first().attribute("confidence").toDouble();

		QString analysis_status = it.value().first().attribute("analysis_status").toString();

		// get method
		QString method = it.value().first().attribute("method").toString();

		// get is_automatic_detection
		int is_automatic_detection = it.value().first().attribute("is_automatic_detection").toInt();

		// get comment
		QString comment = it.value().first().attribute("comments").toString();

		// get dataset
		QString dataset = it.value().first().attribute("dataset").toString();
		if (dataset.isEmpty())
			dataset = "1";
		// else
		//	dataset = dataset.split(" ")[0];

		// get name
		QString name = it.value().first().attribute("name").toString();

		// retrieve bounding polygon
		QString polygon = it.value().first().attribute("polygon").toByteArray();

		// find min and max timestamps, and max temperature
		qint64 min = std::numeric_limits<qint64>::max();
		qint64 max = std::numeric_limits<qint64>::min();
		double max_t = -100000;
		qint64 max_T_timestamp_ns = std::numeric_limits<double>::min();
		const QList<VipShape>& sh = it.value();
		for (int i = 0; i < sh.size(); ++i) {
			qint64 t = sh[i].attribute("timestamp_ns").toLongLong();
			if (t > max)
				max = t;
			if (t < min)
				min = t;
			double temp = sh[i].attribute("max_temperature_C").toDouble();
			if (temp > max_t) {
				max_t = temp;
				max_T_timestamp_ns = t;
			}
		}

		it.value().first().setAttribute("max_temperature_C", max_t);

		// send to thermal_events
		QString query = QString("INSERT IGNORE INTO `thermal_events` (`experiment_id`,`line_of_sight`,`device`,`initial_timestamp_ns`,`final_timestamp_ns`,"
					"`duration_ns`,`category`,`is_automatic_detection`,`max_temperature_C`,`max_T_timestamp_ns`,`method`,`confidence`,"
					"`user`,`comments`,`dataset`,`name`,  `analysis_status`) \n"
					"VALUES\n"
					"('%1','%2','%3',%4,%5,%6,'%7',%8,%9,%10,'%11',%12,'%13','%14','%15','%16','%17');")
				  .arg(QString::number(pulse))
				  .arg(camera)
				  .arg(device)
				  .arg(min)
				  .arg(max)
				  .arg(max - min)
				  .arg(thermal_event)
				  .arg(is_automatic_detection)
				  .arg(max_t)
				  .arg(max_T_timestamp_ns)
				  .arg(method)
				  .arg(confidence)
				  .arg(userName)
				  .arg(comment)
				  .arg(dataset)
				  .arg(name)
				  .arg(analysis_status);

		QSqlQuery q(db);
		bool res = q.exec(query);

		// vip_debug("'%s'\n",q.lastError().text().toLatin1().data());
		// vip_debug("'%s'\n", db.lastError().text().toLatin1().data());

		if (!res) {
			VIP_LOG_ERROR(q.lastError().text());
			return QList<qint64>();
		}

		qint64 id = q.lastInsertId().toLongLong();

		if (id == 0) {
			VIP_LOG_ERROR("An error occurred while sending event to SQL database");
			return QList<qint64>();
		}

		resids << id;

		// set the new ID to all shapes
		for (int i = 0; i < sh.size(); ++i) {
			VipShape(sh[i]).setAttribute("id", id);
		}

		// send to thermal_events_instances
		query = QString("INSERT IGNORE INTO `thermal_events_instances` "
				"(`timestamp_ns`,`thermal_event_id`,`bbox_x`,`bbox_y`,`bbox_width`,`bbox_height`,"
				"`max_temperature_C`,`max_T_image_position_x`,`max_T_image_position_y`,`min_temperature_C`,`min_T_image_position_x`,`min_T_image_position_y`,`average_temperature_C`,"
				"`pixel_area`,`centroid_image_position_x`,`centroid_image_position_y`,`polygon`,`pfc_id`,`overheating_factor`,`max_T_world_position_x_m`,`max_T_world_position_y_m`,"
				"`max_T_world_position_z_m`,`min_T_world_position_x_m`,`min_T_world_position_y_m`,`min_T_world_position_z_m`,`max_overheating_world_position_x_m`,`max_overheating_"
				"world_position_y_m`,"
				"`max_overheating_world_position_z_m`,`max_overheating_image_position_x`,`max_overheating_image_position_y`,`centroid_world_position_x_m`,`centroid_world_position_y_m`"
				",`centroid_world_position_z_m`,`physical_area`) \n"
				"VALUES\n");

		QStringList values;
		for (int i = 0; i < sh.size(); ++i) {
			const QVariantMap a = sh[i].attributes();

			// fill spatial attributes

			QPolygon poly; // = floorPolygonF(sh[i].polygon());
			QRect r;       // = poly.boundingRect();
			convertShape(sh[i], poly, r);
			QPointF centroid(0, 0);
			QString poly_string;
			int pixel_area = r.width() * r.height();
			// recompute centroid
			for (const QPoint& pt : poly) {
				centroid.rx() += pt.x();
				centroid.ry() += pt.y();
			}
			if (poly.size()) {

				centroid.rx() /= poly.size();
				centroid.ry() /= poly.size();
			}
			if (!vipIsRect(poly)) {
				poly_string = polygonToString(poly);
				// recompute pixel_area
				pixel_area = sh[i].fillPixels().size();
			}

			QString value = QString("(%1, %2, %3, %4, %5, %6,"
						"%7, %8, %9, %10, %11, %12, %13,"
						"%14, %15, %16, '%17', %18, %19, %20, %21, %22,%23,%24,%25,%26,%27,%28,%29,%30,%31,%32,%33,%34)")
					  .arg(a["timestamp_ns"].toLongLong())
					  .arg(id)
					  .arg(r.left())
					  .arg(r.top())
					  .arg(r.width())
					  .arg(r.height())
					  .arg(a["max_temperature_C"].toDouble())
					  .arg(a["max_T_image_position_x"].toDouble())
					  .arg(a["max_T_image_position_y"].toDouble())
					  .arg(a["min_temperature_C"].toDouble())
					  .arg(a["min_T_image_position_x"].toDouble())
					  .arg(a["min_T_image_position_y"].toDouble())
					  .arg(a["average_temperature_C"].toDouble())
					  .arg(pixel_area)
					  .arg(centroid.x())
					  .arg(centroid.y())
					  .arg(poly_string)
					  .arg(a["pfc_id"].toLongLong())
					  .arg(a["overheating_factor"].toDouble())
					  .arg(a["max_T_world_position_x_m"].toDouble())
					  .arg(a["max_T_world_position_y_m"].toDouble())
					  .arg(a["max_T_world_position_z_m"].toDouble())
					  .arg(a["min_T_world_position_x_m"].toDouble())
					  .arg(a["min_T_world_position_y_m"].toDouble())
					  .arg(a["min_T_world_position_z_m"].toDouble())
					  .arg(a["max_overheating_world_position_x_m"].toDouble())
					  .arg(a["max_overheating_world_position_y_m"].toDouble())
					  .arg(a["max_overheating_world_position_z_m"].toDouble())
					  .arg(a["max_overheating_image_position_x"].toDouble())
					  .arg(a["max_overheating_image_position_y"].toDouble())
					  .arg(a["centroid_world_position_x_m"].toDouble())
					  .arg(a["centroid_world_position_y_m"].toDouble())
					  .arg(a["centroid_world_position_z_m"].toDouble())
					  .arg(a["physical_area"].toDouble());
			values.append(value);
		}

		query += values.join(",\n") + ";";

		// vip_debug("%s\n", query.toLatin1().data());

		res = q.exec(query);
		if (!res) {
			VIP_LOG_ERROR(q.lastError().text());
			return QList<qint64>();
		}
	}

	return resids;
}

#include "VipIODevice.h"

bool vipRemoveFromDB(const QList<qint64>& ids, VipProgress* p)
{
	if (p) {
		p->setText("Remove thermal events from DB...");
		p->setRange(0, ids.size());
	}
	QSqlDatabase db = createConnection();
	if (!db.isOpen())
		return false;

	for (int i = 0; i < ids.size(); ++i) {
		if (p)
			p->setValue(i);
		{
			QSqlQuery q(db);
			bool res = q.exec("DELETE FROM `thermal_events` WHERE `id` = " + QString::number(ids[i]));
			if (!res) {
				VIP_LOG_ERROR(q.lastError().text());
				return false;
			}
		}
		{
			QSqlQuery q(db);
			bool res = q.exec("DELETE FROM `thermal_events_instances` WHERE `thermal_event_id` = " + QString::number(ids[i]));
			if (!res) {
				VIP_LOG_ERROR(q.lastError().text());
				return false;
			}
		}
	}
	return true;
}

bool vipChangeColumnInfoDB(const QList<qint64>& ids, const QString& column, const QString& value, VipProgress* p)
{
	if (p) {
		p->setText("Change column in DB...");
		p->setRange(0, ids.size());
	}
	QSqlDatabase db = createConnection();
	if (!db.isOpen())
		return false;

	for (int i = 0; i < ids.size(); ++i) {
		if (p)
			p->setValue(i);
		QSqlQuery q(db);
		bool res = q.exec("UPDATE `thermal_events` SET `" + column + "` = " + value + "  WHERE `id` = " + QString::number(ids[i]));
		// vip_debug("%s\n", q.lastQuery().toLatin1().data());
		if (!res) {
			VIP_LOG_ERROR(q.lastError().text());
			return false;
		}
	}

	return true;
}

VipEventQueryResults vipQueryDB(const VipEventQuery& query, VipProgress* p)
{
	if (p) {
		p->setText("Query DB...");
	}
	VipEventQueryResults result;
	QSqlDatabase db = createConnection();
	if (!db.isOpen()) {
		result.error = db.lastError().text();
		return result;
	}

	// first, select Ids in thermal_events table that matches cameras, pulses, comments, durations,...

	QStringList conditions;

	if (query.eventIds.size()) {
		// find by ids...

		QStringList lst;
		for (int i = 0; i < query.eventIds.size(); ++i)
			lst << ("id = " + QString::number(query.eventIds[i]));
		conditions << "(" + lst.join(" OR ") + ")";
	}
	else {
		//...or find with other conditions

		// camera condition
		if (!query.cameras.isEmpty()) {
			QStringList lst;
			for (int i = 0; i < query.cameras.size(); ++i)
				lst << ("line_of_sight = '" + query.cameras[i] + "'");
			conditions << "(" + lst.join(" OR ") + ")";
		}
		if (!query.devices.isEmpty()) {
			QStringList lst;
			for (int i = 0; i < query.devices.size(); ++i)
				lst << ("device = '" + query.devices[i] + "'");
			conditions << "(" + lst.join(" OR ") + ")";
		}
		// method condition
		if (!query.method.isEmpty()) {
			conditions << "(method LIKE '%" + query.method + "%')";
		}
		// PPO names
		if (!query.users.isEmpty()) {
			QStringList lst;
			for (int i = 0; i < query.users.size(); ++i)
				lst << ("user = '" + query.users[i] + "'");
			conditions << "(" + lst.join(" OR ") + ")";
		}
		// pulse condition
		// TODO
		if (query.min_pulse >= 0) {
			conditions << "(experiment_id >= '" + QString::number(query.min_pulse) + "')";
		}
		if (query.max_pulse >= 0) {
			conditions << "(experiment_id <= '" + QString::number(query.max_pulse) + "')";
		}

		// comment condition
		if (!query.in_comment.isEmpty()) {
			conditions << "(comments LIKE '%" + query.in_comment + "%')";
		}
		// dataset condition
		if (!query.dataset.isEmpty()) {
			QStringList lst = query.dataset.split(" ");
			QStringList queries;
			for (int i = 0; i < lst.size(); ++i) {
				queries.append("(dataset LIKE '%" + lst[i] + "%')");
			}
			conditions << "(" + queries.join(" OR ") + ")";
		}
		// name condition
		if (!query.in_name.isEmpty()) {
			conditions << "(name LIKE '%" + query.in_name + "%')";
		}
		// duration
		if (query.min_duration >= 0) {
			conditions << "(duration_ns >= " + QString::number(query.min_duration) + ")";
		}
		if (query.max_duration >= 0) {
			conditions << "(duration_ns <= " + QString::number(query.max_duration) + ")";
		}
		// max temperature
		if (query.min_temperature >= 0) {
			conditions << "(max_temperature_C >= " + QString::number(query.min_temperature) + ")";
		}
		if (query.max_temperature >= 0) {
			conditions << "(max_temperature_C <= " + QString::number(query.max_temperature) + ")";
		}

		// automatic
		if (query.automatic >= 0) {
			conditions << "(is_automatic_detection = " + QString::number(query.automatic) + ")";
		}
		// confidence
		if (query.min_confidence >= 0) {
			conditions << "(confidence >= " + QString::number(query.min_confidence) + ")";
		}
		if (query.max_confidence >= 0) {
			conditions << "(confidence <= " + QString::number(query.max_confidence) + ")";
		}
		// event type
		if (query.event_types.size()) {
			QStringList lst;
			for (int i = 0; i < query.event_types.size(); ++i)
				lst << ("category = '" + query.event_types[i] + "'");
			conditions << "(" + lst.join(" OR ") + ")";
		}
	}

	// find the list of ids satisfying the conditions
	QString sql = "SELECT * FROM thermal_events ";
	if (conditions.size()) { //&& query.id_thermaleventinfo <= 0) {
		sql += " WHERE " + conditions.join(" AND ") + ";";
	}
	// else if (query.id_thermaleventinfo > 0) {
	//	sql += " WHERE id = " + QString::number(query.id_thermaleventinfo);
	// }
	// vip_debug("%s\n", sql.toLatin1().data());

	QSqlQuery q(db);
	if (!q.exec(sql)) {
		VIP_LOG_ERROR(q.lastError().text());
		result.error = q.lastError().text();
		return result;
	}

	if (p) {
		p->setText("Retrieve thermal events from DB...");
		p->setRange(0, q.size());
	}

	int count = 0;
	while (q.next()) {

		if (p) {
			p->setValue(count++);
			if (p->canceled())
				return VipEventQueryResults();
		}

		VipEventQueryResult res;
		res.comment = q.value("comments").toString();
		res.dataset = q.value("dataset").toString();
		res.name = q.value("name").toString();
		res.eventName = q.value("category").toString();
		res.eventId = (q.value("id").toLongLong());
		res.device = (q.value("device").toString());
		res.experiment_id = (q.value("experiment_id").toString()).toLongLong();
		res.initialTimestamp = (q.value("initial_timestamp_ns").toLongLong());
		res.lastTimestamp = (q.value("final_timestamp_ns").toLongLong());
		res.duration = (q.value("duration_ns").toLongLong());
		res.automatic = (q.value("is_automatic_detection").toInt());
		res.maximum = (q.value("max_temperature_C").toDouble());
		res.method = (q.value("method").toString());
		res.confidence = (q.value("confidence").toDouble());
		res.analysis_status = (q.value("analysis_status").toString());
		res.user = (q.value("user").toString());
		res.camera = (q.value("line_of_sight").toString());

		result.events[res.eventId] = res;
	}
	return result;
}

VipFullQueryResult vipFullQueryDB(const VipEventQueryResults& evtres, VipProgress* p)
{
	if (p) {
		p->setText("Query DB...");
	}
	VipFullQueryResult result;

	QSqlDatabase db = createConnection();
	if (!db.isOpen()) {
		result.error = db.lastError().text();
		return result;
	}

	// now let's query thermal_events_instances
	QString query = "SELECT * FROM thermal_events_instances ";
	QStringList conditions;

	// additional conditions
	/* for (int i = 0; i < fullq.additional_conditions.size(); ++i)
	{
		const VipRequestCondition c = fullq.additional_conditions[i];
		QString cond = vipFormatRequestCondition(c);
		if (!cond.isEmpty()) {
			conditions << "( " + cond + ") ";
		}
	}*/

	if (conditions.size()) {
		query += " WHERE " + conditions.join(" AND ");
	}

	int total_count = 0;
	QVector<QSqlQuery> queries;
	if (evtres.events.size()) {
		// event type, launch one query per event ID
		QList<qint64> id = evtres.events.keys();
		for (int i = 0; i < id.size(); ++i) {
			QString _q = query;
			if (!_q.contains("WHERE"))
				_q += " WHERE ";
			else
				_q += " AND ";
			_q += " thermal_event_id = " + QString::number(id[i]) + ";";
			QSqlQuery q(db);
			// vip_debug("%s\n", _q.toLatin1().data());
			if (!q.exec(_q)) {
				VIP_LOG_ERROR(q.lastError().text());
				result.error = q.lastError().text();
				return result;
			}
			total_count += q.size();
			queries.push_back(q);
		}
	}
	/*else {
		QSqlQuery q(db);
		if (!q.exec(query)) {
			VIP_LOG_ERROR(q.lastError().text());
			result.error = q.lastError().text();
			return result;
		}
		total_count += q.size();
		queries.push_back(q);
	}*/

	// vip_debug("%s\n", query.toLatin1().data());

	if (p) {
		p->setText("Retrieve thermal events from DB...");
		p->setRange(0, total_count);
	}
	int count = 0;

	// retrieve all shapes
	Vip_event_list shapes;
	for (int i = 0; i < queries.size(); ++i) {
		QSqlQuery q = queries[i];
		while (q.next()) {

			if (p)
				p->setValue(count++);

			qint64 id = q.value("thermal_event_id").toLongLong();
			const VipEventQueryResult& evt = evtres.events[id];

			// build shape
			VipShape sh(QRectF(q.value("bbox_x").toDouble(), q.value("bbox_y").toDouble(), q.value("bbox_width").toDouble(), q.value("bbox_height").toDouble()));
			sh.setGroup(evt.eventName);

			// check polygon
			QString polygon = q.value("polygon").toString();
			if (polygon.size()) {
				QPolygonF poly;
				QTextStream stream(polygon.toLatin1());
				while (true) {
					int x, y;
					stream >> x >> y;
					if (stream.status() != QTextStream::Ok)
						break;
					poly.append(QPointF(x, y));
				}
				sh.setPolygon(poly);
			}

			// set attributes from global event info
			QVariantMap attrs;

			// attrs.insert("ID", id);
			sh.setId(id);
			attrs.insert("comments", evt.comment);
			attrs.insert("dataset", evt.dataset);
			attrs.insert("name", evt.name);
			attrs.insert("experiment_id", evt.experiment_id);
			attrs.insert("initial_timestamp_ns", evt.initialTimestamp);
			attrs.insert("final_timestamp_ns", evt.lastTimestamp);
			attrs.insert("duration_ns", evt.duration);
			attrs.insert("is_automatic_detection", evt.automatic);
			attrs.insert("max_temperature_C", evt.maximum);
			attrs.insert("method", evt.method);
			attrs.insert("confidence", evt.confidence);
			attrs.insert("analysis_status", evt.analysis_status);
			attrs.insert("user", evt.user);
			attrs.insert("line_of_sight", evt.camera);
			attrs.insert("device", evt.device);

			// set attributes from realtime table
			attrs.insert("timestamp_ns", q.value("timestamp_ns").toLongLong());
			attrs.insert("id", q.value("thermal_event_id").toLongLong());
			attrs.insert("id_hotspot", q.value("id").toLongLong());
			attrs.insert("bbox_x", q.value("bbox_x").toInt());
			attrs.insert("bbox_y", q.value("bbox_y").toInt());
			attrs.insert("bbox_width", q.value("bbox_width").toInt());
			attrs.insert("bbox_height", q.value("bbox_height").toInt());
			attrs.insert("max_temperature_C", q.value("max_temperature_C").toDouble());
			attrs.insert("max_T_image_position_x", q.value("max_T_image_position_x").toInt());
			attrs.insert("max_T_image_position_y", q.value("max_T_image_position_y").toInt());
			attrs.insert("min_temperature_C", q.value("min_temperature_C").toDouble());
			attrs.insert("min_T_image_position_x", q.value("min_T_image_position_x").toInt());
			attrs.insert("min_T_image_position_y", q.value("min_T_image_position_y").toInt());
			attrs.insert("average_temperature_C", q.value("average_temperature_C").toDouble());
			attrs.insert("pixel_area", q.value("pixel_area").toInt());
			attrs.insert("centroid_image_position_x", q.value("centroid_image_position_x").toDouble());
			attrs.insert("centroid_image_position_y", q.value("centroid_image_position_y").toDouble());

			attrs.insert("pfc_id", q.value("pfc_id").toLongLong());
			attrs.insert("overheating_factor", q.value("overheating_factor").toDouble());
			attrs.insert("max_T_world_position_x_m", q.value("max_T_world_position_x_m").toDouble());
			attrs.insert("max_T_world_position_y_m", q.value("max_T_world_position_y_m").toDouble());
			attrs.insert("max_T_world_position_z_m", q.value("max_T_world_position_z_m").toDouble());
			attrs.insert("min_T_world_position_x_m", q.value("min_T_world_position_x_m").toDouble());
			attrs.insert("min_T_world_position_y_m", q.value("min_T_world_position_y_m").toDouble());
			attrs.insert("min_T_world_position_z_m", q.value("min_T_world_position_z_m").toDouble());
			attrs.insert("max_overheating_world_position_x_m", q.value("max_overheating_world_position_x_m").toDouble());
			attrs.insert("max_overheating_world_position_y_m", q.value("max_overheating_world_position_y_m").toDouble());
			attrs.insert("max_overheating_world_position_z_m", q.value("max_overheating_world_position_z_m").toDouble());
			attrs.insert("max_overheating_image_position_x", q.value("max_overheating_image_position_x").toDouble());
			attrs.insert("max_overheating_image_position_y", q.value("max_overheating_image_position_y").toDouble());
			attrs.insert("centroid_world_position_x_m", q.value("centroid_world_position_x_m").toDouble());
			attrs.insert("centroid_world_position_y_m", q.value("centroid_world_position_y_m").toDouble());
			attrs.insert("centroid_world_position_z_m", q.value("centroid_world_position_z_m").toDouble());
			attrs.insert("physical_area", q.value("physical_area").toDouble());

			sh.setAttributes(attrs);
			shapes[id].push_back(sh);
		}
	}

	// now fill result with shapes
	for (Vip_event_list::const_iterator it = shapes.begin(); it != shapes.end(); ++it) {
		const QList<VipShape> sh = it.value();
		const VipEventQueryResult evt = evtres.events[it.key()];
		for (int i = 0; i < sh.size(); ++i) {
			VipPulseResult& experiment_id = result.result[evt.experiment_id];
			experiment_id.experiment_id = evt.experiment_id;
			VipCameraResult& cam = experiment_id.cameras[evt.camera];
			cam.cameraName = evt.camera;
			cam.device = evt.device;
			VipEventQueryResult& event = cam.events.events[evt.eventId];
			if (event.shapes.isEmpty()) {
				// copy struct
				event = evt;
			}
			event.shapes.push_back(sh[i]);
		}
	}
	result.totalEvents = total_count;
	return result;
}

Vip_event_list vipExtractEvents(const VipFullQueryResult& fres)
{
	Vip_event_list res;
	for (QMap<Vip_experiment_id, VipPulseResult>::const_iterator itp = fres.result.begin(); itp != fres.result.end(); ++itp) {
		const VipPulseResult p = itp.value();
		for (QMap<QString, VipCameraResult>::const_iterator itc = p.cameras.begin(); itc != p.cameras.end(); ++itc) {
			const VipEventQueryResults events = itc.value().events;
			for (QMap<qint64, VipEventQueryResult>::const_iterator ite = events.events.begin(); ite != events.events.end(); ++ite) {
				res.insert(ite.key(), ite.value().shapes);
			}
		}
	}
	return res;
}

#include "VipEnvironment.h"

struct ExtractOption
{
	bool max, min, mean;
	bool pixel_area, maxX, maxY, minX, minY;
	bool mergePulses;
	bool mergeCameras;
	bool hasError;
	ExtractOption()
	{
		memset(this, 0, sizeof(ExtractOption));
		max = true;
	}
};

/**
Query the parameters to extract based on default parameters
*/
ExtractOption extractOptions(const ExtractOption& default_opt);

/**
Extract the time trace of several parameters from database events.
The result is stored in a HUGE map of:
	camera ('All' if merged) -> pulse (-1 id merged) -> ROI ('All' if merged) -> parameter ('max',...) -> points
*/
QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector>>>> extractParameters(const ExtractOption& opts, const VipEventQueryResults& events, VipProgress* progress);

static QCheckBox* createBox(const QString& text, bool state)
{
	QCheckBox* res = new QCheckBox(text);
	res->setChecked(state);
	return res;
}
// Query the parameters to extract
ExtractOption extractOptions(const ExtractOption& default_opt)
{
	QCheckBox* max = createBox("Maximum intensity", default_opt.max);
	QCheckBox* min = createBox("Minimum intensity", default_opt.min);
	QCheckBox* mean = createBox("Average intensity", default_opt.mean);
	QCheckBox* pixel_area = createBox("Event pixel area", default_opt.pixel_area);
	QCheckBox* maxX = createBox("Maximum intensity X", default_opt.maxX);
	QCheckBox* maxY = createBox("Maximum intensity Y", default_opt.maxY);
	QCheckBox* minX = createBox("Minimum intensity X", default_opt.minX);
	QCheckBox* minY = createBox("Minimum intensity Y", default_opt.minY);
	QCheckBox* mergePulses = createBox("Merge successive pulses", default_opt.mergePulses);
	mergePulses->setToolTip("Merge the time traces for successive pulses in one curve");
	QCheckBox* mergeCameras = createBox("Merge cameras for each experiment id", default_opt.mergeCameras);
	mergeCameras->setToolTip("For a specific experiment id, merge the time traces of each cameras");

	QVBoxLayout* lay = new QVBoxLayout();
	lay->addWidget(max);
	lay->addWidget(min);
	lay->addWidget(mean);
	lay->addWidget(pixel_area);
	lay->addWidget(maxX);
	lay->addWidget(maxY);
	lay->addWidget(minX);
	lay->addWidget(minY);
	lay->addWidget(VipLineWidget::createHLine());
	lay->addWidget(mergePulses);
	lay->addWidget(mergeCameras);

	QWidget* w = new QWidget();
	w->setLayout(lay);

	// hide this option, too complicated
	mergeCameras->hide();

	VipGenericDialog dial(w, "Extraction parameters");
	if (dial.exec() == QDialog::Accepted) {
		ExtractOption res;
		res.max = max->isChecked();
		res.min = min->isChecked();
		res.mean = mean->isChecked();
		res.pixel_area = pixel_area->isChecked();
		res.maxX = maxX->isChecked();
		res.maxY = maxY->isChecked();
		res.minX = minX->isChecked();
		res.minY = minY->isChecked();
		res.mergePulses = mergePulses->isChecked();
		res.mergeCameras = mergeCameras->isChecked();

		return res;
	}
	ExtractOption res;
	res.hasError = true;
	return res;
}

QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector>>>> extractParameters(const ExtractOption& opts, const VipEventQueryResults& events, VipProgress* progress)
{
	// a time value thatd efault to 0
	struct Time0
	{
		qint64 time;
		Time0()
		  : time(0)
		{
		}
	};

	// extract events from DB
	VipFullQueryResult fr = vipFullQueryDB(events, progress);

	if (progress) {
		progress->setText("Extract parmeters...");
		progress->setRange(0, fr.totalEvents * 2);
	}

	// parameters to extract
	QStringList colmuns;

	// ROIs
	// map of pulse -> camera - ROI name -> shape
	QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipShape>>> ROIs;

	QVector<VipShape> shape_list; // raw list of shapes

	// compute time range for each pulse
	QMap<Vip_experiment_id, VipTimeRange> pulse_ranges;

	QSet<Vip_experiment_id> available_pulses;
	QSet<QString> available_cameras;

	// QMap<QString, QMap<int, QMap<QString, qint64> > > min_times; //minimum events time for given camera (possibly merged, pulse and
	// QMap<QString, QMap<int, QMap<QString, qint64> > > offset_times;

	int count = 0;
	for (QMap<Vip_experiment_id, VipPulseResult>::const_iterator itp = fr.result.begin(); itp != fr.result.end(); ++itp) {
		const VipPulseResult& p = itp.value();
		available_pulses.insert(p.experiment_id);
		for (QMap<QString, VipCameraResult>::const_iterator itc = p.cameras.begin(); itc != p.cameras.end(); ++itc) {
			const VipEventQueryResults& r_events = itc.value().events;
			available_cameras.insert(itc.value().cameraName);
			for (QMap<qint64, VipEventQueryResult>::const_iterator ite = r_events.events.begin(); ite != r_events.events.end(); ++ite) {
				const VipEventQueryResult& evt = ite.value();
				const VipShapeList& lst = evt.shapes;
				const Vip_experiment_id experiment_id = evt.experiment_id;
				const QString camera = evt.camera;

				// load ROIs if needed
				if (ROIs[experiment_id][camera].isEmpty()) {

					// add the big 'All' shape
					ROIs[experiment_id][camera]["All"] = VipShape(QRectF(0, 0, 1000, 1000));
				}

				// get ROIs for this pulse and camera
				const QMap<QString, VipShape>& rois = ROIs[experiment_id][camera];

				// sort shapes by timestamps (should already be the case) and ROI

				for (int i = 0; i < lst.size(); ++i) {
					const VipShape sh = lst[i];
					// find the ROI
					for (QMap<QString, VipShape>::const_iterator it = rois.begin(); it != rois.end(); ++it) {
						QPointF pos(sh.attribute("max_T_image_position_x").toInt(), sh.attribute("max_T_image_position_y").toInt());
						if (it.value().shape().contains(pos)) {
							// qint64 time = sh.attribute("timestamp").toLongLong();
							// all_shapes[opts.mergeCameras ? "All" : camera][opts.mergePulses ? -1 : pulse][it.key()][time].append( sh);
							shape_list.push_back(sh);
							// add ROI attribute
							VipShape(sh).setAttribute("ROI", it.key());

							// update pulse time range
							if (opts.mergePulses) {
								qint64 time = sh.attribute("timestamp_ns").toLongLong();
								if (pulse_ranges.find(experiment_id) == pulse_ranges.end())
									pulse_ranges[experiment_id] = VipTimeRange(time, time);
								else {
									VipTimeRange& r = pulse_ranges[experiment_id];
									if (time < r.first)
										r.first = time;
									else if (time > r.second)
										r.second = time;
								}
							}
							break;
						}
						if (progress && progress->canceled())
							return QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector>>>>();
					}
				}

				count += lst.size();
				if (progress)
					progress->setValue(count);
				if (progress && progress->canceled())
					return QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector>>>>();
			}
		}
	}

	// compute time offset for each pulse
	QMap<Vip_experiment_id, qint64> offsets;
	if (opts.mergePulses) {
		qint64 end = 0;
		qint64 between_pulse = 1000000; // add 1ms between pulses
		for (QMap<Vip_experiment_id, VipTimeRange>::const_iterator it = pulse_ranges.begin(); it != pulse_ranges.end(); ++it) {
			offsets[it.key()] = -it.value().first + end;
			end += (it.value().second - it.value().first) + between_pulse;
		}
	}

	// sort all shapes by: camera ('All' if merged) -> pulse (-1 id merged) -> ROI ('All' if merged)-> time -> shape lists
	QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<qint64, VipShapeList>>>> all_shapes;
	for (int i = 0; i < shape_list.size(); ++i) {
		VipShape& sh = shape_list[i];
		const Vip_experiment_id experiment_id = sh.attribute("experiment_id").value<Vip_experiment_id>();
		const QString& camera = sh.attribute("line_of_sight").toString();
		qint64 time = sh.attribute("timestamp_ns").toLongLong();
		// apply time offset
		if (opts.mergePulses) {
			time += offsets[experiment_id];
		}
		all_shapes[opts.mergeCameras ? "All" : camera][opts.mergePulses ? -1 : experiment_id][sh.attribute("ROI").toString()][time].append(sh);
	}

	QMap<QString, QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector>>>> result;

	// now we extract the time traces
	const QStringList cameras = all_shapes.keys();
	for (int c = 0; c < cameras.size(); ++c) {
		const QString camera = cameras[c];
		const QList<Vip_experiment_id> pulses = all_shapes[camera].keys();
		for (int p = 0; p < pulses.size(); ++p) {
			const Vip_experiment_id experiment_id = pulses[p];
			const QStringList rois = all_shapes[camera][experiment_id].keys();
			for (int r = 0; r < rois.size(); ++r) {
				const QString roi = rois[r];
				const QMap<qint64, VipShapeList>& shapes = all_shapes[camera][experiment_id][roi];

				QMap<QString, VipPointVector>& res = result[camera][experiment_id][roi];

				// keep track of pulses for mergePulses only
				QSet<Vip_experiment_id> ids;

				// extract parameters
				for (QMap<qint64, VipShapeList>::const_iterator it = shapes.begin(); it != shapes.end(); ++it) {
					const VipShapeList& lst = it.value();
					double max, min, mean = 0;
					int pixel_area = 0, maxX = 0, maxY = 0, minX = 0, minY = 0;
					max = lst.first().attribute("max_temperature_C").toDouble();
					min = lst.first().attribute("min_temperature_C").toDouble();
					if (opts.mean)
						mean = lst.first().attribute("average_temperature_C").toDouble();
					if (opts.pixel_area)
						pixel_area = lst.first().attribute("pixel_area").toInt();
					if (opts.maxX)
						maxX = lst.first().attribute("max_T_image_position_x").toInt();
					if (opts.maxY)
						maxY = lst.first().attribute("max_T_image_position_y").toInt();
					if (opts.minX)
						minX = lst.first().attribute("min_T_image_position_x").toInt();
					if (opts.minY)
						minY = lst.first().attribute("min_T_image_position_y").toInt();

					bool new_pulse = false;
					if (opts.mergePulses) {
						Vip_experiment_id p_number = (lst.first().attribute("experiment_id").value<Vip_experiment_id>());
						if (ids.find(p_number) == ids.end()) {
							new_pulse = true;
							ids.insert(p_number);
						}
					}

					for (int i = 1; i < lst.size(); ++i) {
						double _max = lst[i].attribute("max_temperature_C").toDouble();
						double _min = lst[i].attribute("min_temperature_C").toDouble();
						if (_max > max) {
							max = _max;
							if (opts.maxX)
								maxX = lst[i].attribute("max_T_image_position_x").toInt();
							if (opts.maxY)
								maxY = lst[i].attribute("max_T_image_position_y").toInt();
						}
						if (_min > min) {
							min = _min;
							if (opts.minX)
								minX = lst[i].attribute("min_T_image_position_x").toInt();
							if (opts.minY)
								minY = lst[i].attribute("min_T_image_position_y").toInt();
						}
						if (opts.mean)
							mean += lst[i].attribute("average_temperature_C").toDouble();
						if (opts.pixel_area)
							pixel_area += lst[i].attribute("pixel_area").toInt();
					}
					if (opts.mean)
						mean /= lst.size();

					// fill points

					if (new_pulse) {
						// add NaN separator
						if (opts.max)
							res["max_temperature_C"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.min)
							res["min_temperature_C"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.mean)
							res["average_temperature_C"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.pixel_area)
							res["pixel_area"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.maxX)
							res["max_T_image_position_x"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.maxY)
							res["max_T_image_position_y"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.minX)
							res["min_T_image_position_x"].push_back(VipPoint(vipNan(), vipNan()));
						if (opts.minY)
							res["min_T_image_position_y"].push_back(VipPoint(vipNan(), vipNan()));
					}

					if (opts.max)
						res["max_temperature_C"].push_back(VipPoint(it.key(), max));
					if (opts.min)
						res["min_temperature_C"].push_back(VipPoint(it.key(), min));
					if (opts.mean)
						res["average_temperature_C"].push_back(VipPoint(it.key(), mean));
					if (opts.pixel_area)
						res["pixel_area"].push_back(VipPoint(it.key(), pixel_area));
					if (opts.maxX)
						res["max_T_image_position_x"].push_back(VipPoint(it.key(), maxX));
					if (opts.maxY)
						res["max_T_image_position_y"].push_back(VipPoint(it.key(), maxY));
					if (opts.minX)
						res["min_T_image_position_x"].push_back(VipPoint(it.key(), minX));
					if (opts.minY)
						res["min_T_image_position_y"].push_back(VipPoint(it.key(), minY));

					count += lst.size();
					if (progress)
						progress->setValue(count);
				}
			}
		}
	}

	// replace camera "All" if possible
	if (opts.mergeCameras && available_cameras.size() == 1) {
		result[*available_cameras.begin()] = result["All"];
		result.remove("All");
	}

	// replace pulse -1 if possible
	const QStringList cams = result.keys();
	if (opts.mergePulses && available_pulses.size() == 1) {
		for (int c = 0; c < cams.size(); ++c) {
			QMap<Vip_experiment_id, QMap<QString, QMap<QString, VipPointVector>>>& tmp = result[cams[c]];
			tmp[*available_pulses.begin()] = tmp[-1];
			tmp.remove(-1);
		}
	}

	return result;
}

QPolygonF vipSimplifyPolygonDB(const QPolygonF& p, int max_points)
{
	QPolygonF poly = p;
	double epsilon = 0.1;
	while (poly.size() > max_points) {
		poly = vipRDPSimplifyPolygon(poly, epsilon);
		epsilon *= 2;
	}
	return poly;
}

static QString polygonToJSON(const QPolygon& poly)
{
	QString res;
	QTextStream str(&res, QIODevice::WriteOnly);

	str << "[";

	for (int i = 0; i < poly.size(); ++i) {

		str << "[" << poly[i].x() << ", " << poly[i].y() << "]";
		if (i < poly.size() - 1)
			str << ", ";
	}

	str << "]";
	str.flush();
	return res;
}

/*static QString polygonToJSON(const QPolygonF& poly)
{
	QString res;
	QTextStream str(&res, QIODevice::WriteOnly);

	str << "[";

	for (int i = 0; i < poly.size(); ++i) {
		QPoint pt = poly[i].toPoint();
		str << "[" << pt.x() << ", " << pt.y() << "]";
		// str << "[" << std::floor(poly[i].x()) << ", " << std::floor(poly[i].y()) << "]";
		if (i < poly.size() - 1)
			str << ", ";
	}

	str << "]";
	str.flush();
	return res;
}*/

/*static QString polygonToJSON(const QString& polygon)
{
	QPolygon poly;
	{
		QTextStream str(polygon.toLatin1());
		int x, y;
		while (str.status() == QTextStream::Ok) {
			str >> x >> y;
			if (str.status() != QTextStream::Ok)
				break;
			poly.push_back(QPoint(x, y));
		}
	}
	return polygonToJSON(poly);
}*/

static QString addDoubleQuotes(const QString& str)
{
	QString tmp = str;
	for (int i = 0; i < str.size(); ++i) {
		if (tmp[i] == '"' && i > 0 && i < str.size() - 1)
			tmp[i] = ' ';
	}

	if (tmp.startsWith("\"") && tmp.endsWith("\""))
		return tmp;

	if (tmp.startsWith("\""))
		tmp[0] = ' ';
	if (tmp.endsWith("\""))
		tmp[tmp.size() - 1] = ' ';

	return "\"" + tmp + "\"";
}

QByteArray vipEventsToJson(const Vip_event_list& all_shapes, VipProgress* p)
{
	Vip_event_list evts = all_shapes;

	if (p) {
		p->setText("Pre-process thermal events...");
		p->setRange(0, evts.size());
	}

	int count = 0;
	QList<qint64> resids;
	for (Vip_event_list::iterator it = evts.begin(); it != evts.end(); ++it, ++count) {

		if (p) {
			p->setValue(count);
		}

		// find min and max timestamps, and max temperature
		qint64 min = std::numeric_limits<qint64>::max();
		qint64 max = -std::numeric_limits<qint64>::max();
		double max_t = -std::numeric_limits<double>::max();
		qint64 max_T_timestamp_ns = -std::numeric_limits<qint64>::max();
		const QList<VipShape>& sh = it.value();
		for (int i = 0; i < sh.size(); ++i) {
			qint64 t = sh[i].attribute("timestamp_ns").toLongLong();
			if (t > max)
				max = t;
			if (t < min)
				min = t;
			double temp = sh[i].attribute("max_temperature_C").toDouble();
			if (temp > max_t) {
				max_t = temp;
				max_T_timestamp_ns = t;
			}
		}

		it.value().first().setAttribute("max_temperature_C", max_t);
		it.value().first().setAttribute("max_T_timestamp_ns", max_T_timestamp_ns);

		// if (!device.isEmpty())
		//	it.value().first().setAttribute("device", device);

		QStringList values;
		for (int i = 0; i < sh.size(); ++i) {
			const QVariantMap a = sh[i].attributes();

			// fill spatial attributes
			QPolygon poly = sh[i].polygon().toPolygon();
			QPointF centroid(0, 0);
			QRect r = poly.boundingRect();
			int pixel_area = r.width() * r.height();
			// recompute centroid
			for (const QPoint& pt : poly) {
				centroid.rx() += pt.x();
				centroid.ry() += pt.y();
			}
			centroid.rx() /= poly.size();
			centroid.ry() /= poly.size();
			if (!vipIsRect(poly)) {
				// recompute pixel_area
				pixel_area = sh[i].fillPixels().size();
			}

			VipShape tmp(sh[i]);
			tmp.setAttribute("bbox_x", r.left());
			tmp.setAttribute("bbox_y", r.top());
			tmp.setAttribute("bbox_width", r.width());
			tmp.setAttribute("bbox_height", r.height());
			tmp.setAttribute("pixel_area", pixel_area);
			tmp.setAttribute("centroid_image_position_x", centroid.x());
			tmp.setAttribute("centroid_image_position_y", centroid.y());
		}
	}

	if (p) {
		p->setText("Convert to JSON...");
		p->setRange(0, evts.size());
		p->setValue(0);
	}

	QString res;
	QTextStream str(&res, QIODevice::WriteOnly);

	// START
	str << "{\n";

	int i = 0;
	for (Vip_event_list::const_iterator it = evts.begin(); it != evts.end(); ++it, ++i) {

		if (p) {
			p->setValue(i);
		}

		qint64 id = it.key();
		const QList<VipShape> shs = it.value();

		str << "\t"
		    << "\"" << id << "\":\n";
		// Start event
		str << "\t"
		    << "{\n";

		QString device = shs.first().attribute("device").toString();

		str << "\t\t"
		    << "\"experiment_id\": " << QString::number(shs.first().attribute("experiment_id").toLongLong()) << ",\n";
		str << "\t\t"
		    << "\"line_of_sight\": " << addDoubleQuotes(shs.first().attribute("line_of_sight").toString()) << ",\n";
		str << "\t\t"
		    << "\"device\": " << addDoubleQuotes(device) << ",\n";
		str << "\t\t"
		    << "\"initial_timestamp_ns\": " << addDoubleQuotes(shs.first().attribute("initial_timestamp_ns").toString()) << ",\n";
		str << "\t\t"
		    << "\"final_timestamp_ns\": " << addDoubleQuotes(shs.first().attribute("final_timestamp_ns").toString()) << ",\n";
		str << "\t\t"
		    << "\"duration_ns\": " << addDoubleQuotes(shs.first().attribute("duration_ns").toString()) << ",\n";
		str << "\t\t"
		    << "\"category\": " << addDoubleQuotes(shs.first().group()) << ",\n";
		str << "\t\t"
		    << "\"is_automatic_detection\": " << shs.first().attribute("is_automatic_detection").toInt() << ",\n";
		str << "\t\t"
		    << "\"max_temperature_C\": " << shs.first().attribute("max_temperature_C").toInt() << ",\n";
		str << "\t\t"
		    << "\"method\": " << addDoubleQuotes(shs.first().attribute("method").toString()) << ",\n";
		str << "\t\t"
		    << "\"dataset\": " << addDoubleQuotes(shs.first().attribute("dataset").toString()) << ",\n";
		str << "\t\t"
		    << "\"confidence\": " << shs.first().attribute("confidence").toString() << ",\n";
		str << "\t\t"
		    << "\"user\": " << addDoubleQuotes(shs.first().attribute("user").toString()) << ",\n";
		str << "\t\t"
		    << "\"comments\": " << addDoubleQuotes(shs.first().attribute("comments").toString()) << ",\n";
		str << "\t\t"
		    << "\"name\": " << addDoubleQuotes(shs.first().attribute("name").toString()) << ",\n";
		str << "\t\t"
		    << "\"analysis_status\": " << addDoubleQuotes(shs.first().attribute("analysis_status").toString()) << ",\n";

		// Start images
		str << "\t\t"
		    << "\"thermal_events_instances\": [\n";

		for (int j = 0; j < shs.size(); ++j) {
			str << "\t\t{\n";

			QPolygon poly;
			QRect r;
			convertShape(shs[j], poly, r);

			str << "\t\t\t"
			    << "\"polygon\": " << polygonToJSON(poly) << ",\n";
			str << "\t\t\t"
			    << "\"timestamp_ns\": " << addDoubleQuotes(shs[j].attribute("timestamp_ns").toString()) << ",\n";
			str << "\t\t\t"
			    << "\"bbox_x\": " << (r.left()) << ",\n";
			str << "\t\t\t"
			    << "\"bbox_y\": " << (r.top()) << ",\n";
			str << "\t\t\t"
			    << "\"bbox_width\": " << (r.width()) << ",\n";
			str << "\t\t\t"
			    << "\"bbox_height\": " << (r.height()) << ",\n";
			str << "\t\t\t"
			    << "\"max_temperature_C\": " << (shs[j].attribute("max_temperature_C").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"min_temperature_C\": " << (shs[j].attribute("min_temperature_C").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_T_image_position_x\": " << (shs[j].attribute("max_T_image_position_x").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"max_T_image_position_y\": " << (shs[j].attribute("max_T_image_position_y").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"min_T_image_position_x\": " << (shs[j].attribute("min_T_image_position_x").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"min_T_image_position_y\": " << (shs[j].attribute("min_T_image_position_y").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"average_temperature_C\": " << (shs[j].attribute("average_temperature_C").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"pixel_area\": " << (shs[j].attribute("pixel_area").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"centroid_image_position_x\": " << (shs[j].attribute("centroid_image_position_x").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"centroid_image_position_y\": " << (shs[j].attribute("centroid_image_position_y").toDouble()) << ",\n";

			str << "\t\t\t"
			    << "\"pfc_id\": " << (shs[j].attribute("pfc_id").toLongLong()) << ",\n";
			str << "\t\t\t"
			    << "\"overheating_factor\": " << (shs[j].attribute("overheating_factor").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_T_world_position_x_m\": " << (shs[j].attribute("max_T_world_position_x_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_T_world_position_y_m\": " << (shs[j].attribute("max_T_world_position_y_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_T_world_position_z_m\": " << (shs[j].attribute("max_T_world_position_z_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"min_T_world_position_x_m\": " << (shs[j].attribute("min_T_world_position_x_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"min_T_world_position_y_m\": " << (shs[j].attribute("min_T_world_position_y_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"min_T_world_position_z_m\": " << (shs[j].attribute("min_T_world_position_z_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_overheating_world_position_x_m\": " << (shs[j].attribute("max_overheating_world_position_x_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_overheating_world_position_y_m\": " << (shs[j].attribute("max_overheating_world_position_y_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_overheating_world_position_z_m\": " << (shs[j].attribute("max_overheating_world_position_z_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"max_overheating_image_position_x\": " << (shs[j].attribute("max_overheating_image_position_x").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"max_overheating_image_position_y\": " << (shs[j].attribute("max_overheating_image_position_y").toInt()) << ",\n";
			str << "\t\t\t"
			    << "\"centroid_world_position_x_m\": " << (shs[j].attribute("centroid_world_position_x_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"centroid_world_position_y_m\": " << (shs[j].attribute("centroid_world_position_y_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"centroid_world_position_z_m\": " << (shs[j].attribute("centroid_world_position_z_m").toDouble()) << ",\n";
			str << "\t\t\t"
			    << "\"physical_area\": " << (shs[j].attribute("physical_area").toDouble()) << "\n";

			str << "\t\t}";
			if (j < shs.size() - 1)
				str << ",";
			str << "\n";
		}

		// Stop images
		str << "\t\t]\n";

		// end event
		str << "\t"
		    << "}";
		if (i < evts.size() - 1)
			str << ", ";
		str << "\n";
	}

	// END
	str << "}\n";

	str.flush();
	return res.toLatin1();
}

#include <qjsonarray.h>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>

static qint64 toTimestamp(const QJsonObject& obj, const QString& name)
{
	QJsonValue val = obj.value(name);
	if (val.isDouble())
		return (qint64)val.toDouble();
	else
		return val.toVariant().toLongLong();
}

Vip_event_list vipEventsFromJson(const QByteArray& content)
{
	QJsonParseError error;
	QJsonDocument loadDoc(QJsonDocument::fromJson(content, &error));
	if (error.error != QJsonParseError::NoError) {
		VIP_LOG_ERROR(error.errorString());
		return Vip_event_list();
	}

	if (loadDoc.isNull()) {
		VIP_LOG_ERROR("Null JSON file");
		return Vip_event_list();
	}

	QJsonObject root = loadDoc.object();
	Vip_event_list result;

	for (QJsonObject::const_iterator evt = root.begin(); evt != root.end(); ++evt) {
		bool ok = false;
		qint64 id = evt.key().toLongLong(&ok);
		if (ok == false) {
			VIP_LOG_ERROR("JSON error: nullptr id");
			return Vip_event_list();
		}

		vip_debug("%d\n", (int)id);

		QJsonObject event = evt.value().toObject();
		QList<VipShape> shapes;

		// first , read all shapes
		QJsonArray thermal_events_instances = event.value("thermal_events_instances").toArray();
		for (QJsonArray::const_iterator it = thermal_events_instances.begin(); it != thermal_events_instances.end(); ++it) {
			QJsonObject obj = it->toObject();
			VipShape sh;
			QJsonArray p = obj.value("polygon").toArray();
			if (p.count()) {
				QPolygon poly;
				for (int i = 0; i < p.count(); ++i) {
					QJsonArray xy = p.at(i).toArray();
					if (xy.size() != 2) {
						VIP_LOG_ERROR("Json file: wrong polygon format");
						return Vip_event_list();
					}
					poly.push_back(QPoint(xy[0].toInt(), xy[1].toInt()));
				}

				sh.setPolygon(poly);
			}
			else {
				QString p_string = obj.value("polygon").toString();
				if (!p_string.isEmpty()) {
					QTextStream str(&p_string);
					QPolygon poly;
					while (true) {
						double x, y;
						str >> x >> y;
						if (str.status() != QTextStream::Ok)
							break;
						poly.push_back(QPoint(x, y));
					}
					sh.setPolygon(poly);
				}
				else {
					QRect r(obj.value("bbox_x").toVariant().toInt(),
						obj.value("bbox_y").toVariant().toInt(),
						obj.value("bbox_width").toVariant().toInt(),
						obj.value("bbox_height").toVariant().toInt());
					sh.setRect(r);
				}
			}

			// TEST
			sh.setAttribute("timestamp_ns", toTimestamp(obj, "timestamp_ns"));

			sh.setAttribute("bbox_x", obj.value("bbox_x").toVariant().toInt());
			sh.setAttribute("bbox_y", obj.value("bbox_y").toVariant().toInt());
			sh.setAttribute("bbox_width", obj.value("bbox_width").toVariant().toInt());
			sh.setAttribute("bbox_height", obj.value("bbox_height").toVariant().toInt());
			sh.setAttribute("max_temperature_C", obj.value("max_temperature_C").toVariant().toInt());
			sh.setAttribute("min_temperature_C", obj.value("min_temperature_C").toVariant().toInt());
			sh.setAttribute("max_T_image_position_x", obj.value("max_T_image_position_x").toVariant().toInt());
			sh.setAttribute("max_T_image_position_y", obj.value("max_T_image_position_y").toVariant().toInt());
			sh.setAttribute("min_T_image_position_x", obj.value("min_T_image_position_x").toVariant().toInt());
			sh.setAttribute("min_T_image_position_y", obj.value("min_T_image_position_y").toVariant().toInt());
			sh.setAttribute("average_temperature_C", obj.value("average_temperature_C").toVariant().toDouble());
			sh.setAttribute("pixel_area", obj.value("pixel_area").toVariant().toInt());
			sh.setAttribute("centroid_image_position_x", obj.value("centroid_image_position_x").toVariant().toDouble());
			sh.setAttribute("centroid_image_position_y", obj.value("centroid_image_position_y").toVariant().toDouble());

			sh.setAttribute("pfc_id", obj.value("pfc_id").toVariant().toLongLong());
			sh.setAttribute("overheating_factor", obj.value("overheating_factor").toVariant().toDouble());
			sh.setAttribute("max_T_world_position_x_m", obj.value("max_T_world_position_x_m").toVariant().toDouble());
			sh.setAttribute("max_T_world_position_y_m", obj.value("max_T_world_position_y_m").toVariant().toDouble());
			sh.setAttribute("max_T_world_position_z_m", obj.value("max_T_world_position_z_m").toVariant().toDouble());
			sh.setAttribute("min_T_world_position_x_m", obj.value("min_T_world_position_x_m").toVariant().toDouble());
			sh.setAttribute("min_T_world_position_y_m", obj.value("min_T_world_position_y_m").toVariant().toDouble());
			sh.setAttribute("min_T_world_position_z_m", obj.value("min_T_world_position_z_m").toVariant().toDouble());
			sh.setAttribute("max_overheating_world_position_x_m", obj.value("max_overheating_world_position_x_m").toVariant().toDouble());
			sh.setAttribute("max_overheating_world_position_y_m", obj.value("max_overheating_world_position_y_m").toVariant().toDouble());
			sh.setAttribute("max_overheating_world_position_z_m", obj.value("max_overheating_world_position_z_m").toVariant().toDouble());
			sh.setAttribute("max_overheating_image_position_x", obj.value("max_overheating_image_position_x").toVariant().toInt());
			sh.setAttribute("max_overheating_image_position_y", obj.value("max_overheating_image_position_y").toVariant().toInt());
			sh.setAttribute("centroid_world_position_x_m", obj.value("centroid_world_position_x_m").toVariant().toDouble());
			sh.setAttribute("centroid_world_position_y_m", obj.value("centroid_world_position_y_m").toVariant().toDouble());
			sh.setAttribute("centroid_world_position_z_m", obj.value("centroid_world_position_z_m").toVariant().toDouble());
			sh.setAttribute("physical_area", obj.value("physical_area").toVariant().toDouble());

			sh.setId(id);
			shapes.append(sh);
		}

		if (shapes.size()) {
			// add event infos

			shapes.first().setAttribute("experiment_id", event.value("experiment_id").toVariant());
			shapes.first().setAttribute("line_of_sight", event.value("line_of_sight").toString());
			shapes.first().setAttribute("device", event.value("device").toString());
			shapes.first().setAttribute("initial_timestamp_ns", toTimestamp(event, "initial_timestamp_ns"));
			shapes.first().setAttribute("final_timestamp_ns", toTimestamp(event, "final_timestamp_ns"));
			shapes.first().setAttribute("duration_ns", toTimestamp(event, "duration_ns"));
			QString group = event.value("category").toString();
			shapes.first().setAttribute("is_automatic_detection", event.value("is_automatic_detection").toVariant().toBool());
			shapes.first().setAttribute("max_temperature_C", event.value("max_temperature_C").toVariant().toInt());
			shapes.first().setAttribute("method", event.value("method").toString());
			shapes.first().setAttribute("confidence", event.value("confidence").toVariant().toDouble());
			shapes.first().setAttribute("analysis_status", event.value("analysis_status").toString());
			shapes.first().setAttribute("user", event.value("user").toString());
			shapes.first().setAttribute("comments", event.value("comments").toString());
			shapes.first().setAttribute("dataset", event.value("dataset").toString());

			QJsonArray p = event.value("polygon").toArray();
			if (p.count()) {
				QPolygon poly;
				for (int i = 0; i < p.count(); ++i) {
					QJsonArray xy = p.at(i).toArray();
					if (xy.size() != 2) {
						VIP_LOG_ERROR("Json file: wrong polygon format");
						return Vip_event_list();
					}
					poly.push_back(QPoint(xy[0].toInt(), xy[1].toInt()));
				}
				shapes.first().setAttribute("polygon", polygonToString(poly));
			}
			else {
				QString p_string = event.value("polygon").toString();
				if (!p_string.isEmpty()) {
					QTextStream str(&p_string);
					QPolygon poly;
					while (true) {
						double x, y;
						str >> x >> y;
						if (str.status() != QTextStream::Ok)
							break;
						poly.push_back(QPoint(x, y));
					}
					shapes.first().setAttribute("polygon", polygonToString(poly));
				}
				else
					shapes.first().setAttribute("polygon", QString());
			}

			shapes.first().setAttribute("name", event.value("name").toString());

			// set group to all shapes and static attributes
			for (int i = 0; i < shapes.size(); ++i) {
				shapes[i].setGroup(group);
				shapes[i].setAttribute("experiment_id", event.value("experiment_id").toVariant());
				shapes[i].setAttribute("line_of_sight", event.value("line_of_sight").toString());
				shapes[i].setAttribute("device", event.value("device").toString());
				shapes[i].setAttribute("initial_timestamp_ns", toTimestamp(event, "initial_timestamp_ns"));
				shapes[i].setAttribute("final_timestamp_ns", toTimestamp(event, "final_timestamp_ns"));
				shapes[i].setAttribute("duration_ns", toTimestamp(event, "duration_ns"));
				shapes[i].setAttribute("is_automatic_detection", event.value("is_automatic_detection").toVariant().toBool());
				shapes[i].setAttribute("max_temperature_C", event.value("max_temperature_C").toVariant().toInt());
				shapes[i].setAttribute("method", event.value("method").toString());
				shapes[i].setAttribute("confidence", event.value("confidence").toVariant().toDouble());
				shapes[i].setAttribute("analysis_status", event.value("analysis_status").toString());
				shapes[i].setAttribute("user", event.value("user").toString());
				shapes[i].setAttribute("comments", event.value("comments").toString());
				shapes[i].setAttribute("dataset", event.value("dataset").toString());
				shapes[i].setAttribute("name", event.value("name").toString());
			}

			result.insert(id, shapes);
		}
	}

	return result;
}

bool vipEventsToJsonFile(const QString& out_file, const Vip_event_list& evts, VipProgress* p)
{
	QFile out(out_file);
	if (!out.open(QFile::WriteOnly))
		return false;

	QByteArray json = vipEventsToJson(evts, p);

	if (p) {
		p->setText("Write to file...");
	}

	out.write(json);
	out.close();
	return true;
}

/// @brief W7X way of handling pulse parameters
struct W7XDeviceParameters : public VipBaseDeviceParameters
{
	virtual QSize defaultVideoSize() const { return QSize(1024, 768); }
	virtual QString createDevicePath(Vip_experiment_id pulse, const QString& camera) const
	{
		QString pulse_str = QString::number(pulse);
		pulse_str = pulse_str.mid(0, 8) + (pulse_str.size() > 8 ? ("." + pulse_str.mid(8)) : QString());
		// before 14/09/2022: OP1.2
		const qint64 pulse_date = pulse_str.mid(0, 8).toLongLong();
		const qint64 first_OP2 = 20220914;
		if (pulse_date < first_OP2) {
			return "qir::ArchiveQrtRawOP1:ArchiveDB;/raw/W7X/QRT_IRCAM/" + camera + "_raw_DATASTREAM/V1/0/raw" + ";" + pulse_str + ";0;0";
		}
		else {
			QString path;
			if (camera == "AEF10")
				path = "/raw/W7X/ControlStation.2206/AEF10_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF11")
				path = "/raw/W7X/ControlStation.2207/AEF11_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF20")
				path = "/raw/W7X/ControlStation.2208/AEF20_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF21")
				path = "/raw/W7X/ControlStation.2209/AEF21_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEA30")
				path = "/raw/W7X/ControlStation.2204/AEA30_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEK30")
				path = "/raw/W7X/ControlStation.2202/AEK30_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEA31")
				path = "/raw/W7X/ControlStation.2205/AEA31_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEK31")
				path = "/raw/W7X/ControlStation.2203/AEK31_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF40")
				path = "/raw/W7X/ControlStation.2210/AEF40_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF41")
				path = "/raw/W7X/ControlStation.2211/AEF41_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF50")
				path = "/raw/W7X/ControlStation.2212/AEF50_IR_Thermal_DATASTREAM/V1/0/full";
			else if (camera == "AEF51")
				path = "/raw/W7X/ControlStation.2213/AEF51_IR_Thermal_DATASTREAM/V1/0/full";
			return "qir::ArchiveQrtThermal:Test;" + path + ";" + pulse_str + ";0;0";
		}
	}
	virtual QWidget* pulseEditor() const { return new VipPulseSpinBox(); }
};

/// @brief WEST way to handle experiment_id numbers
struct WESTDeviceParameters : VipBaseDeviceParameters
{
	virtual QString createDevicePath(Vip_experiment_id experiment_id, const QString& camera) const { return "WEST_IR_Device:" + QString::number(experiment_id) + ";" + camera; }
	virtual QSize defaultVideoSize() const { return QSize(640, 512); }
	virtual QWidget* pulseEditor() const { return new VipPulseSpinBox(); }
};

/// @brief Default way to handle experiment_id numbers using local movie folder
struct DefaultDeviceParameters : VipBaseDeviceParameters
{
	virtual QString createDevicePath(Vip_experiment_id experiment_id, const QString& camera) const
	{
		return vipLocalMovieFolderDB() + "/" + QString::number(experiment_id) + "_" + camera + "." + vipLocalMovieSuffix();
	}
	virtual QSize defaultVideoSize() const { return QSize(); }
	virtual QWidget* pulseEditor() const { return new VipPulseSpinBox(); }
};

static QMap<QString, QSharedPointer<VipBaseDeviceParameters>> _devices_parameters;

static bool registerDefaultDevices()
{
	QSharedPointer<VipBaseDeviceParameters> _default(new DefaultDeviceParameters());
	_devices_parameters[QString()] = _default;
	_devices_parameters["WEST"] = QSharedPointer<VipBaseDeviceParameters>(new WESTDeviceParameters());
	_devices_parameters["W7X"] = QSharedPointer<VipBaseDeviceParameters>(new W7XDeviceParameters());
	return true;
}
static bool _registerDefaultDevices = registerDefaultDevices();

bool vipRegisterDeviceParameters(const QString& name, VipBaseDeviceParameters* param)
{
	_devices_parameters[name] = QSharedPointer<VipBaseDeviceParameters>(param);
	return true;
}

const VipBaseDeviceParameters* vipFindDeviceParameters(const QString& name)
{
	auto it = _devices_parameters.find(name);
	if (it == _devices_parameters.end())
		return _devices_parameters[QString()].data();
	return it.value().data();
}

class VipDatasetButton::PrivateData
{
public:
	VipDragMenu* menu;
	QWidget* widget;
	QList<QCheckBox*> boxes;
	QCheckBox* all;
};

VipDatasetButton::VipDatasetButton(QWidget* parent)
  : QToolButton(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);
	d_data->menu = new VipDragMenu();

	d_data->widget = new QWidget();
	QVBoxLayout* lay = new QVBoxLayout();

	QMap<int, VipDataset> dsets = vipDatasetsDB();
	for (auto it = dsets.begin(); it != dsets.end(); ++it) {
		QCheckBox* box = new QCheckBox();
		box->setProperty("id", it.key());
		box->setText(it.value().creation_date + " " + it.value().annotation_type);
		lay->addWidget(box);
		d_data->boxes.append(box);
		connect(box, SIGNAL(clicked(bool)), this, SLOT(emitChanged()));
	}

	lay->addWidget(VipLineWidget::createHLine());
	d_data->all = new QCheckBox();
	d_data->all->setText("Check/uncheck all");
	lay->addWidget(d_data->all);
	connect(d_data->all, SIGNAL(clicked(bool)), this, SLOT(checkAll(bool)));

	d_data->widget->setLayout(lay);
	d_data->widget->resize(100, 100);

	d_data->menu->setWidget(d_data->widget);
	this->setPopupMode(QToolButton::InstantPopup);
	this->setMenu(d_data->menu);
	this->setText("Datasets...");

	connect(d_data->menu, SIGNAL(aboutToShow()), this, SLOT(aboutToShow()));
}

VipDatasetButton::~VipDatasetButton()
{
}

int VipDatasetButton::datasetCount() const
{
	return d_data->boxes.size();
}
QString VipDatasetButton::datasetName(int index) const
{
	return d_data->boxes[index]->text();
}
bool VipDatasetButton::datasetChecked(int index) const
{
	return d_data->boxes[index]->isChecked();
}
void VipDatasetButton::setChecked(int index, bool checked)
{
	d_data->boxes[index]->setChecked(checked);
}

void VipDatasetButton::aboutToShow()
{
	// bool stop = true;
}

QString VipDatasetButton::dataset() const
{
	// return list of checked ids
	QStringList lst;
	for (int i = 0; i < d_data->boxes.size(); ++i) {
		if (d_data->boxes[i]->isChecked())
			lst.push_back(d_data->boxes[i]->property("id").toString());
	}
	return lst.join(" ");
}

void VipDatasetButton::setDataset(const QString& dataset)
{
	this->blockSignals(true);
	// unckeck all
	for (int i = 0; i < d_data->boxes.size(); ++i) {
		d_data->boxes[i]->setChecked(false);
	}

	if (dataset.isEmpty()) {
		this->blockSignals(false);
		return;
	}

	QStringList lst = dataset.split(" ");
	for (int i = 0; i < lst.size(); ++i) {
		int id = lst[i].toInt();
		if (id == 0)
			continue;

		for (int j = 0; j < d_data->boxes.size(); ++j) {
			if (d_data->boxes[j]->property("id").toInt() == id)
				d_data->boxes[j]->setChecked(true);
		}
	}
	this->blockSignals(false);
	this->emitChanged();
}

void VipDatasetButton::checkAll(bool enable)
{
	this->blockSignals(true);

	for (int i = 0; i < d_data->boxes.size(); ++i) {
		d_data->boxes[i]->setChecked(enable);
	}

	this->blockSignals(false);
}

class VipQueryDBWidget::PrivateData
{
public:
	QWidget* minPulse;
	QWidget* maxPulse;
	QSpinBox idThermalEventInfo;
	QToolButton linked;
	QComboBox userName;
	QComboBox camera;
	QComboBox device;
	int pulseRow;
	VipDatasetButton dataset;
	QLineEdit inComment;
	QLineEdit inName;
	QComboBox method;

	QDoubleSpinBox minDuration;
	QDoubleSpinBox maxDuration;
	QDoubleSpinBox minTemperature;
	QDoubleSpinBox maxTemperature;

	/*QDoubleSpinBox minSMAG_IP;
	QDoubleSpinBox minGINTLIDRT;
	QDoubleSpinBox minSHYBPTOT;
	QDoubleSpinBox minSICHPTOT;*/

	QComboBox automatic;
	QDoubleSpinBox minConfidence;
	QDoubleSpinBox maxConfidence;
	QComboBox thermalEvent;
	QCheckBox removePrevious;
};

VipQueryDBWidget::VipQueryDBWidget(const QString& device, QWidget* parent)
  : QWidget(parent)
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	QGridLayout* lay = new QGridLayout();

	d_data->minPulse = vipFindDeviceParameters(device)->pulseEditor();
	d_data->maxPulse = vipFindDeviceParameters(device)->pulseEditor();

	int row = 0;
	lay->addWidget(new QLabel("Min experiment id"), row, 0);
	lay->addWidget(d_data->minPulse, row, 1);
	++row;
	{
		QHBoxLayout* hlay = new QHBoxLayout();
		hlay->setSpacing(0);
		hlay->setContentsMargins(0, 0, 0, 0);
		hlay->addWidget(d_data->maxPulse);
		hlay->addWidget(&d_data->linked);
		lay->addWidget(new QLabel("Max experiment id"), row, 0);
		lay->addLayout(hlay, row, 1);
		++row;
	}

	lay->addWidget(new QLabel("ID ThermalEventInfo"), row, 0);
	lay->addWidget(&d_data->idThermalEventInfo, row, 1);
	++row;

	lay->addWidget(VipLineWidget::createHLine(), row, 0, 1, 2);
	++row;

	lay->addWidget(new QLabel("User name"), row, 0);
	lay->addWidget(&d_data->userName, row, 1);
	++row;

	lay->addWidget(new QLabel("Camera name"), row, 0);
	lay->addWidget(&d_data->camera, row, 1);
	++row;

	lay->addWidget(new QLabel("Device name"), row, 0);
	lay->addWidget(&d_data->device, row, 1);
	++row;

	lay->addWidget(new QLabel("Dataset name"), row, 0);
	lay->addWidget(&d_data->dataset, row, 1);
	++row;

	lay->addWidget(new QLabel("Thermal event"), row, 0);
	lay->addWidget(&d_data->thermalEvent, row, 1);
	++row;

	lay->addWidget(VipLineWidget::createHLine(), row, 0, 1, 2);
	++row;

	lay->addWidget(new QLabel("Min duration (s)"), row, 0);
	lay->addWidget(&d_data->minDuration, row, 1);
	++row;

	lay->addWidget(new QLabel("Max duration (s)"), row, 0);
	lay->addWidget(&d_data->maxDuration, row, 1);
	++row;

	lay->addWidget(new QLabel("Min temperature"), row, 0);
	lay->addWidget(&d_data->minTemperature, row, 1);
	++row;

	lay->addWidget(new QLabel("Max temperature"), row, 0);
	lay->addWidget(&d_data->maxTemperature, row, 1);
	++row;

	lay->addWidget(VipLineWidget::createHLine(), row, 0, 1, 2);
	++row;

	/*lay->addWidget(new QLabel("Min IP (kA)"), row, 0);
	lay->addWidget(&d_data->minSMAG_IP, row, 1);
	++row;
	lay->addWidget(new QLabel("Min density (1E19 p/m^3)"), row, 0);
	lay->addWidget(&d_data->minGINTLIDRT, row, 1);
	++row;
	lay->addWidget(new QLabel("Min LH power (MW)"), row, 0);
	lay->addWidget(&d_data->minSHYBPTOT, row, 1);
	++row;
	lay->addWidget(new QLabel("Min ICRH power (kW)"), row, 0);
	lay->addWidget(&d_data->minSICHPTOT, row, 1);
	++row;


	lay->addWidget(VipLineWidget::createHLine(), row, 0, 1, 2);
	++row;*/

	lay->addWidget(new QLabel("Text in comments"), row, 0);
	lay->addWidget(&d_data->inComment, row, 1);
	++row;

	lay->addWidget(new QLabel("Text in name"), row, 0);
	lay->addWidget(&d_data->inName, row, 1);
	++row;

	lay->addWidget(new QLabel("Detection method"), row, 0);
	lay->addWidget(&d_data->method, row, 1);
	++row;

	lay->addWidget(VipLineWidget::createHLine(), row, 0, 1, 2);
	++row;

	lay->addWidget(new QLabel("Automatic detection"), row, 0);
	lay->addWidget(&d_data->automatic, row, 1);
	++row;

	lay->addWidget(new QLabel("Min confidence"), row, 0);
	lay->addWidget(&d_data->minConfidence, row, 1);
	++row;

	lay->addWidget(new QLabel("Max confidence"), row, 0);
	lay->addWidget(&d_data->maxConfidence, row, 1);
	++row;

	lay->addWidget(VipLineWidget::createHLine(), row, 0, 1, 2);
	++row;

	lay->addWidget(&d_data->removePrevious, row, 0, 1, 2);

	setLayout(lay);

	d_data->minPulse->setToolTip("Minimum experiment id");
	d_data->maxPulse->setToolTip("Maximum experiment id");
	d_data->idThermalEventInfo.setRange(0, INT_MAX);

	/*d_data->minSMAG_IP.setRange(-1, 10000);
	d_data->minSMAG_IP.setSingleStep(1);
	d_data->minSMAG_IP.setValue(-1);
	d_data->minGINTLIDRT.setRange(-1, 100);
	d_data->minGINTLIDRT.setSingleStep(0.1);
	d_data->minGINTLIDRT.setValue(-1);
	d_data->minSHYBPTOT.setRange(-1, 20);
	d_data->minSHYBPTOT.setSingleStep(0.1);
	d_data->minSHYBPTOT.setValue(-1);
	d_data->minSICHPTOT.setRange(-1, 20000);
	d_data->minSICHPTOT.setSingleStep(100);
	d_data->minSICHPTOT.setValue(-1);*/

	d_data->linked.setAutoRaise(true);
	d_data->linked.setIcon(vipIcon("next_day.png"));
	d_data->linked.setCheckable(true);
	d_data->linked.setChecked(true);
	d_data->linked.setToolTip("Start experiment and End experiment id are the same");
	d_data->userName.addItems(QStringList() << "All" << vipUsersDB());
	d_data->camera.addItems(QStringList() << "All" << vipCamerasDB());
	d_data->device.addItems(QStringList() << "All" << vipDevicesDB());

	d_data->inComment.setToolTip("Find given text in thermal event comments");
	d_data->inComment.setPlaceholderText("Search in comments");
	d_data->inName.setToolTip("Find given text in thermal event name");
	d_data->inName.setPlaceholderText("Search in name");
	d_data->method.setToolTip("Find detection method");
	d_data->method.addItems(QStringList() << "All" << vipMethodsDB());
	d_data->thermalEvent.addItems(QStringList() << "All" << vipEventTypesDB());

	d_data->minDuration.setRange(0, 1000);
	d_data->minDuration.setValue(0);
	d_data->minDuration.setToolTip("Event minimum duration in seconds");
	d_data->maxDuration.setRange(0, 1000);
	d_data->maxDuration.setValue(1000);
	d_data->maxDuration.setToolTip("Event maximum duration in seconds");

	d_data->maxTemperature.setRange(0, 50000);
	d_data->maxTemperature.setValue(5000);
	d_data->maxTemperature.setToolTip("High limit of event maximum temperature (Celsius)");

	d_data->minTemperature.setRange(0, 50000);
	d_data->minTemperature.setValue(0);
	d_data->minTemperature.setToolTip("Low limit of event maximum temperature (Celsius)");

	d_data->automatic.addItems(QStringList() << "All"
						 << "Automatic"
						 << "Manual");
	d_data->minConfidence.setRange(0, 1);
	d_data->minConfidence.setSingleStep(0.25);
	d_data->minConfidence.setValue(0);
	d_data->minConfidence.setToolTip("Minimum confidence value (0->1)");

	d_data->maxConfidence.setRange(0, 1);
	d_data->maxConfidence.setSingleStep(0.25);
	d_data->maxConfidence.setValue(1);
	d_data->maxConfidence.setToolTip("Maximum confidence value (0->1)");

	d_data->removePrevious.setText("Remove previous events");
	d_data->removePrevious.setToolTip("Clear the playr's content before displaying retrieved events from DB");
	d_data->removePrevious.setVisible(false);

	connect(d_data->minPulse, SIGNAL(valueChanged(Vip_experiment_id)), this, SLOT(pulseChanged(Vip_experiment_id)));
	connect(d_data->maxPulse, SIGNAL(valueChanged(Vip_experiment_id)), this, SLOT(pulseChanged(Vip_experiment_id)));

	connect(&d_data->device, SIGNAL(currentIndexChanged(int)), this, SLOT(deviceChanged()));
}
VipQueryDBWidget::~VipQueryDBWidget()
{
}

void VipQueryDBWidget::deviceChanged()
{
	auto range = pulseRange();
	QWidget* minp = vipFindDeviceParameters(device())->pulseEditor();
	QWidget* maxp = vipFindDeviceParameters(device())->pulseEditor();

	auto* l1 = layout()->replaceWidget(d_data->minPulse, minp);
	auto* l2 = layout()->replaceWidget(d_data->maxPulse, maxp);
	delete l1;
	delete d_data->minPulse;
	d_data->minPulse = minp;
	delete l2;
	delete d_data->maxPulse;
	d_data->maxPulse = maxp;

	connect(d_data->minPulse, SIGNAL(valueChanged(Vip_experiment_id)), this, SLOT(pulseChanged(Vip_experiment_id)));
	connect(d_data->maxPulse, SIGNAL(valueChanged(Vip_experiment_id)), this, SLOT(pulseChanged(Vip_experiment_id)));

	setPulseRange(range);
}

void VipQueryDBWidget::setRemovePreviousVisible(bool vis)
{
	d_data->removePrevious.setVisible(vis);
}
bool VipQueryDBWidget::isRemovepreviousVisible() const
{
	return d_data->removePrevious.isVisible();
}

void VipQueryDBWidget::setRemovePrevious(bool enable)
{
	d_data->removePrevious.setChecked(enable);
}
bool VipQueryDBWidget::removePrevious() const
{
	return d_data->removePrevious.isChecked();
}

void VipQueryDBWidget::enablePulseRange(bool enable)
{
	if (enable) {
		d_data->linked.setEnabled(true);
	}
	else {
		d_data->linked.setChecked(true);
		d_data->linked.setEnabled(false);
	}
}
bool VipQueryDBWidget::pulseRangeEnabled() const
{
	return d_data->linked.isEnabled();
}

void VipQueryDBWidget::enableAllDevices(bool enable)
{
	if (d_data->device.count() == 0) {
		if (enable)
			d_data->device.addItem("All");
		return;
	}
	if (d_data->device.itemText(0) == "All" && !enable)
		d_data->device.removeItem(0);
	else if (d_data->device.itemText(0) != "All" && enable)
		d_data->device.insertItem(0, "All");
}
bool VipQueryDBWidget::isAllDevicesEnabled() const
{
	return d_data->device.count() > 0 && d_data->device.itemText(0) == "All";
}

void VipQueryDBWidget::enableAllCameras(bool enable)
{
	if (d_data->camera.count() == 0) {
		if (enable)
			d_data->camera.addItem("All");
		return;
	}

	if (d_data->camera.itemText(0) == "All" && !enable)
		d_data->camera.removeItem(0);
	else if (d_data->camera.itemText(0) != "All" && enable)
		d_data->camera.insertItem(0, "All");
}
bool VipQueryDBWidget::isAllCamerasEnabled() const
{
	return d_data->camera.count() > 0 && d_data->camera.itemText(0) == "All";
}

void VipQueryDBWidget::setPulseRange(const QPair<Vip_experiment_id, Vip_experiment_id>& range)
{
	d_data->minPulse->setProperty("value", range.first);
	d_data->maxPulse->setProperty("value", range.second);
}
QPair<Vip_experiment_id, Vip_experiment_id> VipQueryDBWidget::pulseRange() const
{
	return QPair<Vip_experiment_id, Vip_experiment_id>(d_data->minPulse->property("value").value<Vip_experiment_id>(), d_data->maxPulse->property("value").value<Vip_experiment_id>());
}

void VipQueryDBWidget::setIDThermalEventInfo(int v)
{
	d_data->idThermalEventInfo.setValue(v);
}
int VipQueryDBWidget::idThermalEventInfo() const
{
	return d_data->idThermalEventInfo.value();
}

void VipQueryDBWidget::setUserName(const QString& name)
{
	d_data->userName.setCurrentText(name);
}
QString VipQueryDBWidget::userName() const
{
	QString res = d_data->userName.currentText();
	if (res == "All")
		return QString();
	return res;
}

void VipQueryDBWidget::setCamera(const QString& camera)
{
	d_data->camera.setCurrentText(camera);
}
QString VipQueryDBWidget::camera() const
{
	QString res = d_data->camera.currentText();
	if (res == "All")
		return QString();
	return res;
}

void VipQueryDBWidget::setDevice(const QString& device)
{
	d_data->device.setCurrentText(device);
}
QString VipQueryDBWidget::device() const
{
	QString res = d_data->device.currentText();
	if (res == "All")
		return QString();
	return res;
}

void VipQueryDBWidget::setDataset(const QString& dataset)
{
	d_data->dataset.setDataset(dataset);
}
QString VipQueryDBWidget::dataset() const
{
	return d_data->dataset.dataset();
}

void VipQueryDBWidget::setInComment(const QString& comment)
{
	d_data->inComment.setText(comment);
}
QString VipQueryDBWidget::inComment() const
{
	return d_data->inComment.text();
}

void VipQueryDBWidget::setInName(const QString& name)
{
	d_data->inName.setText(name);
}
QString VipQueryDBWidget::inName() const
{
	return d_data->inName.text();
}

void VipQueryDBWidget::setMethod(const QString& method)
{
	d_data->method.setCurrentText(method);
}
QString VipQueryDBWidget::method() const
{
	QString res = d_data->method.currentText();
	if (res == "All")
		return QString();
	return res;
}

void VipQueryDBWidget::setDurationRange(const QPair<qint64, qint64>& range)
{
	d_data->minDuration.setValue(range.first / 1000000000.0);
	d_data->maxDuration.setValue(range.second / 1000000000.0);
}
QPair<qint64, qint64> VipQueryDBWidget::durationRange() const
{
	return QPair<qint64, qint64>(d_data->minDuration.value() * 1000000000, d_data->maxDuration.value() * 1000000000);
}

void VipQueryDBWidget::setMaxTemperatureRange(const QPair<double, double>& range)
{
	d_data->minTemperature.setValue(range.first);
	d_data->maxTemperature.setValue(range.second);
}
QPair<double, double> VipQueryDBWidget::maxTemperatureRange() const
{
	return QPair<double, double>(d_data->minTemperature.value(), d_data->maxTemperature.value());
}

void VipQueryDBWidget::setAutomatic(int automatic)
{
	if (automatic < 0)
		d_data->automatic.setCurrentIndex(0);
	else if (automatic == 0)
		d_data->automatic.setCurrentIndex(2);
	else
		d_data->automatic.setCurrentIndex(1);
}
int VipQueryDBWidget::automatic() const
{
	QString r = d_data->automatic.currentText();
	if (r == "All")
		return -1;
	else if (r == "Automatic")
		return 1;
	return 0;
}

void VipQueryDBWidget::setMinConfidence(double value)
{
	d_data->minConfidence.setValue(value);
}
double VipQueryDBWidget::minConfidence() const
{
	return d_data->minConfidence.value();
}

void VipQueryDBWidget::setMaxConfidence(double value)
{
	d_data->maxConfidence.setValue(value);
}
double VipQueryDBWidget::maxConfidence() const
{
	return d_data->maxConfidence.value();
}

void VipQueryDBWidget::setThermalEvent(const QString& evt)
{
	d_data->thermalEvent.setCurrentText(evt);
}
QString VipQueryDBWidget::thermalEvent() const
{
	QString res = d_data->thermalEvent.currentText();
	if (res == "All")
		return QString();
	return res;
}

void VipQueryDBWidget::pulseChanged(Vip_experiment_id v)
{
	if (d_data->linked.isChecked()) {
		d_data->minPulse->blockSignals(true);
		d_data->maxPulse->blockSignals(true);
		d_data->minPulse->setProperty("value", v);
		d_data->maxPulse->setProperty("value", v);
		d_data->minPulse->blockSignals(false);
		d_data->maxPulse->blockSignals(false);
	}
}
