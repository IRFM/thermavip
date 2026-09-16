/// @file TestLogFormat.cpp
///
/// Characterisation tests for the log line codec: the module exposes a formatter
/// and a splitter meant to be inverses of each other, and nothing exercised
/// them. Two of the cases below pin a known asymmetry rather than a behaviour to
/// rely on; they are marked as such.

#include <QTest>

#include "vip_test_main.h"

#include "VipLogging.h"

#include <QDateTime>

class TestLogFormat : public QObject
{
	Q_OBJECT

	/// Width of the two fixed fields the formatter writes before the text.
	static const int typeWidth = 10;
	static const int dateWidth = 25;

	static QDateTime aDate() { return QDateTime(QDate(2026, 9, 11), QTime(7, 29, 35, 123)); }

private Q_SLOTS:

	/// One entry, three fields, and the text comes back as it went in.
	void aFormattedEntrySplitsBackIntoItsThreeFields()
	{
		const QString entry = QString::fromLatin1(VipLogging::formatLogEntry(QStringLiteral("nothing to report"), VipLogging::Info, aDate()));

		QString type, date, text;
		QVERIFY(VipLogging::splitLogEntry(entry, type, date, text));
		QCOMPARE(type.trimmed(), QStringLiteral("Info"));
		QCOMPARE(date.trimmed(), aDate().toString(QStringLiteral("yy:MM:dd-hh:mm:ss.zzz")));
		QCOMPARE(text, QStringLiteral("nothing to report\n"));
	}

	/// The two fixed fields keep their width whatever they carry, since the
	/// splitter reads them by offset and not by separator.
	void theFixedFieldsKeepTheirWidth()
	{
		const QList<VipLogging::Level> levels = { VipLogging::Info, VipLogging::Warning, VipLogging::Error, VipLogging::Debug };
		for (VipLogging::Level level : levels) {
			const QByteArray entry = VipLogging::formatLogEntry(QStringLiteral("x"), level, aDate());
			QCOMPARE(entry.size(), typeWidth + dateWidth + 2); // "x" and the newline
		}
	}

	/// A level outside the four named ones leaves the field blank instead of
	/// writing a number: the entry stays readable and stays aligned.
	void anUnnamedLevelLeavesTheTypeBlank()
	{
		const QString entry = QString::fromLatin1(VipLogging::formatLogEntry(QStringLiteral("x"), VipLogging::Level(0), aDate()));

		QString type, date, text;
		QVERIFY(VipLogging::splitLogEntry(entry, type, date, text));
		QVERIFY(type.trimmed().isEmpty());
		QCOMPARE(date.trimmed(), aDate().toString(QStringLiteral("yy:MM:dd-hh:mm:ss.zzz")));
		QCOMPARE(text, QStringLiteral("x\n"));
	}

	/// An invalid date produces no text at all, and the field is still padded to
	/// its width: the columns of a log file written across a clock change stay
	/// aligned, and the splitter keeps finding the text.
	void anInvalidDateStillFillsItsField()
	{
		const QString entry = QString::fromLatin1(VipLogging::formatLogEntry(QStringLiteral("x"), VipLogging::Info, QDateTime()));

		QString type, date, text;
		QVERIFY(VipLogging::splitLogEntry(entry, type, date, text));
		QCOMPARE(date.size(), dateWidth);
		QVERIFY(date.trimmed().isEmpty());
		QCOMPARE(text, QStringLiteral("x\n"));
	}

	/// Anything shorter than the two fixed fields is refused rather than read
	/// past its end: a truncated last line of a log file is such an entry.
	void anEntryShorterThanItsFieldsIsRefused()
	{
		QString type, date, text;
		QVERIFY(!VipLogging::splitLogEntry(QString(typeWidth + dateWidth - 1, QLatin1Char('x')), type, date, text));
		QVERIFY(VipLogging::splitLogEntry(QString(typeWidth + dateWidth, QLatin1Char('x')), type, date, text));
		QVERIFY(text.isEmpty());
	}

	/// Known limitation. The formatter indents the continuation lines of a
	/// multiline entry so the file reads in columns, and the splitter, which
	/// reads by offset, hands that indentation back inside the text. The pair is
	/// therefore not invertible on multiline entries.
	void aMultilineEntryDoesNotComeBackWhole()
	{
		const QString entry = QString::fromLatin1(VipLogging::formatLogEntry(QStringLiteral("first\nsecond"), VipLogging::Info, aDate()));

		QString type, date, text;
		QVERIFY(VipLogging::splitLogEntry(entry, type, date, text));
		QEXPECT_FAIL("", "the splitter does not undo the indentation the formatter adds", Continue);
		QCOMPARE(text, QStringLiteral("first\nsecond\n"));
		QVERIFY(text.startsWith(QStringLiteral("first\n")));
		QVERIFY(text.contains(QString(typeWidth + dateWidth, QLatin1Char(' ')) + QStringLiteral("second")));
	}

	/// Known limitation. The text is narrowed to Latin-1, so a character outside
	/// it is written as a question mark and the entry no longer says what was
	/// logged. Widening the conversion changes the format of every existing log
	/// file, which is not this test's call to make.
	void charactersOutsideLatin1DoNotSurvive()
	{
		const QString logged = QStringLiteral("température 120 °C — Ω");
		const QString entry = QString::fromLatin1(VipLogging::formatLogEntry(logged, VipLogging::Warning, aDate()));

		QString type, date, text;
		QVERIFY(VipLogging::splitLogEntry(entry, type, date, text));
		QEXPECT_FAIL("", "the text is narrowed to Latin-1 before it is written", Continue);
		QCOMPARE(text, logged + QStringLiteral("\n"));
		QVERIFY(text.contains(QLatin1Char('?')));
	}
};

VIP_TEST_MAIN(TestLogFormat)
#include "TestLogFormat.moc"
