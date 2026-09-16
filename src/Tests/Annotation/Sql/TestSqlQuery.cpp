/// @file TestSqlQuery.cpp
///
/// What the operator types must reach the database as a value, never as SQL.
/// The statements are exercised against an in-memory SQLite database: it is
/// enough to tell a bound value from a pasted one, and it needs no server.

#include <QTest>

#include "vip_test_main.h"

#include "VipSqlQuery.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

class TestSqlQuery : public QObject
{
	Q_OBJECT

	QSqlDatabase db;
	// A file rather than ":memory:". A statement the database refuses makes the
	// module reconnect, which closes this connection; on a memory database that
	// takes the schema with it and every later case fails for the wrong reason.
	QTemporaryDir dir;

	/// The columns vipQueryDB() reads back, so that a query returns a complete row.
	static QString createTable()
	{
		return QStringLiteral("CREATE TABLE `thermal_events` ("
				      "`id` INTEGER PRIMARY KEY, `experiment_id` INTEGER, `line_of_sight` TEXT, `device` TEXT,"
				      "`initial_timestamp_ns` INTEGER, `final_timestamp_ns` INTEGER, `duration_ns` INTEGER,"
				      "`is_automatic_detection` INTEGER, `max_temperature_C` REAL, `method` TEXT,"
				      "`confidence` REAL, `analysis_status` TEXT, `user` TEXT, `comments` TEXT,"
				      "`dataset` TEXT, `name` TEXT, `category` TEXT)");
	}

	void insertRow(qint64 id, const QString& camera, const QString& user, const QString& category, const QString& comments)
	{
		QSqlQuery q(db);
		q.prepare(QStringLiteral("INSERT INTO `thermal_events` (`id`,`experiment_id`,`line_of_sight`,`device`,`initial_timestamp_ns`,"
					 "`final_timestamp_ns`,`duration_ns`,`is_automatic_detection`,`max_temperature_C`,`method`,`confidence`,"
					 "`analysis_status`,`user`,`comments`,`dataset`,`name`,`category`) "
					 "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
		q.addBindValue(id);
		q.addBindValue(100 + id);
		q.addBindValue(camera);
		q.addBindValue(QStringLiteral("WEST"));
		q.addBindValue((qint64)0);
		q.addBindValue((qint64)10);
		q.addBindValue((qint64)10);
		q.addBindValue(0);
		q.addBindValue(50.0);
		q.addBindValue(QStringLiteral("manual"));
		q.addBindValue(1.0);
		q.addBindValue(QStringLiteral("analyzed"));
		q.addBindValue(user);
		q.addBindValue(comments);
		q.addBindValue(QStringLiteral("first"));
		q.addBindValue(QStringLiteral("event") + QString::number(id));
		q.addBindValue(category);
		QVERIFY2(q.exec(), qPrintable(q.lastError().text()));
	}

	/// The identifiers a query returns, sorted, as a readable list.
	static QList<qint64> idsOf(const VipEventQueryResults& r) { return r.events.keys(); }

private Q_SLOTS:

	void initTestCase()
	{
		QVERIFY(dir.isValid());
		db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("test_thermal_events"));
		db.setDatabaseName(dir.filePath(QStringLiteral("events.db")));
		QVERIFY2(db.open(), qPrintable(db.lastError().text()));

		QSqlQuery q(db);
		QVERIFY2(q.exec(createTable()), qPrintable(q.lastError().text()));

		vipSetGlobalSQLConnection(db);
	}

	void cleanupTestCase()
	{
		// The module holds a copy of the connection: it has to let go of it before
		// the connection is removed, or Qt keeps it alive and warns.
		vipSetGlobalSQLConnection(QSqlDatabase());
		db.close();
		db = QSqlDatabase();
		QSqlDatabase::removeDatabase(QStringLiteral("test_thermal_events"));
	}

	/// Three rows, rebuilt before each case: one plain, one carrying an apostrophe
	/// in two of its free text fields, one to be found only by a query that leaks.
	void init()
	{
		if (!db.isOpen())
			QVERIFY2(db.open(), qPrintable(db.lastError().text()));

		QSqlQuery q(db);
		QVERIFY2(q.exec(QStringLiteral("DELETE FROM `thermal_events`")), qPrintable(q.lastError().text()));
		insertRow(1, QStringLiteral("cam1"), QStringLiteral("alice"), QStringLiteral("hot spot"), QStringLiteral("nothing to report"));
		insertRow(2, QStringLiteral("cam'2"), QStringLiteral("bob"), QStringLiteral("arc"), QStringLiteral("it's hot"));
		insertRow(3, QStringLiteral("cam3"), QStringLiteral("carol"), QStringLiteral("hot spot"), QStringLiteral("quiet"));
	}

	/// An apostrophe in a comment used to break the statement, so a search for a
	/// word the operator had written returned nothing.
	void aCommentWithAnApostropheFindsItsEvent()
	{
		VipEventQuery query;
		query.in_comment = QStringLiteral("it's");

		const VipEventQueryResults r = vipQueryDB(query);

		QVERIFY2(r.isValid(), qPrintable(r.error));
		QCOMPARE(idsOf(r), QList<qint64>() << 2);
	}

	/// The same field closing its own quote and appending a condition: it must
	/// match a comment that does not exist, not every row of the table.
	void aCommentCannotAddACondition()
	{
		VipEventQuery query;
		// The trailing comment closes the statement: without it the pasted text ends
		// on a comparison the database reads as false, and the leak goes unnoticed.
		query.in_comment = QStringLiteral("zz%' OR '1'='1' -- ");

		const VipEventQueryResults r = vipQueryDB(query);

		QVERIFY2(r.isValid(), qPrintable(r.error));
		QCOMPARE(idsOf(r), QList<qint64>());
	}

	/// Camera, device, user and event type are enumerations in the interface, but
	/// a session file or a script fills them with whatever it carries.
	void aCameraWithAnApostropheFindsItsEvent()
	{
		VipEventQuery plain;
		plain.cameras = QStringList() << QStringLiteral("cam1");
		QCOMPARE(idsOf(vipQueryDB(plain)), QList<qint64>() << 1);

		VipEventQuery quoted;
		quoted.cameras = QStringList() << QStringLiteral("cam'2");

		const VipEventQueryResults r = vipQueryDB(quoted);

		QVERIFY2(r.isValid(), qPrintable(r.error));
		QCOMPARE(idsOf(r), QList<qint64>() << 2);
	}

	void aCameraCannotAddACondition()
	{
		VipEventQuery query;
		query.cameras = QStringList() << QStringLiteral("zz' OR '1'='1");

		const VipEventQueryResults r = vipQueryDB(query);

		QVERIFY2(r.isValid(), qPrintable(r.error));
		QCOMPARE(idsOf(r), QList<qint64>());
	}

	void aUserNameCannotAddACondition()
	{
		VipEventQuery query;
		query.users = QStringList() << QStringLiteral("zz' OR '1'='1");

		const VipEventQueryResults r = vipQueryDB(query);

		QVERIFY2(r.isValid(), qPrintable(r.error));
		QCOMPARE(idsOf(r), QList<qint64>());
	}

	/// A column name cannot be bound, so it is checked against the columns the
	/// editor may change.
	void anUnexpectedColumnIsRefused()
	{
		QVERIFY(!vipChangeColumnInfoDB(QList<qint64>() << 1, QStringLiteral("id"), QStringLiteral("9")));
		QVERIFY(!vipChangeColumnInfoDB(QList<qint64>() << 1, QStringLiteral("comments` = 'x' --"), QStringLiteral("9")));

		VipEventQuery query;
		query.eventIds = QList<qint64>() << 1;
		QCOMPARE(vipQueryDB(query).events.value(1).comment, QStringLiteral("nothing to report"));
	}

	/// The value of an update is written as it was typed, and it stops at its own
	/// column: quoted text used to continue the statement and reach a second one.
	void anUpdatedValueIsWrittenLiterally()
	{
		const QString typed = QStringLiteral("x', `user` = 'root");
		QVERIFY(vipChangeColumnInfoDB(QList<qint64>() << 1, QStringLiteral("comments"), typed));

		VipEventQuery query;
		query.eventIds = QList<qint64>() << 1;
		const VipEventQueryResults r = vipQueryDB(query);

		QVERIFY2(r.isValid(), qPrintable(r.error));
		QCOMPARE(r.events.value(1).comment, typed);
		QCOMPARE(r.events.value(1).user, QStringLiteral("alice"));
	}
};

VIP_TEST_MAIN(TestSqlQuery)
#include "TestSqlQuery.moc"
