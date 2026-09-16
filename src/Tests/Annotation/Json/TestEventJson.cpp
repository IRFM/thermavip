/// @file TestEventJson.cpp
///
/// Characterisation tests for the JSON export of thermal events: the document it
/// produces has to be a document, whatever the operator typed in the free text
/// fields it carries.

#include <QTest>

#include "vip_test_main.h"

#include "VipSqlQuery.h"

#include <QJsonDocument>
#include <QJsonParseError>

class TestEventJson : public QObject
{
	Q_OBJECT

	/// One event with a single shape carrying @a comment.
	static Vip_event_list eventWithComment(const QString& comment)
	{
		VipShape shape(QPolygonF() << QPointF(0, 0) << QPointF(1, 0) << QPointF(1, 1));
		shape.setGroup("hot spot");
		shape.setAttribute("comments", comment);
		shape.setAttribute("timestamp_ns", (qint64)0);
		shape.setAttribute("confidence", 1.0);

		Vip_event_list events;
		events[1] = VipShapeList() << shape;
		return events;
	}

	static QString parseError(const QByteArray& json)
	{
		QJsonParseError error;
		QJsonDocument::fromJson(json, &error);
		return error.error == QJsonParseError::NoError ? QString() : error.errorString();
	}

private Q_SLOTS:

	/// A plain comment produces a document that parses.
	void plainCommentProducesValidJson()
	{
		const QByteArray json = vipEventsToJson(eventWithComment(QStringLiteral("nothing special")));

		QVERIFY(!json.isEmpty());
		QVERIFY2(parseError(json).isEmpty(), qPrintable(parseError(json)));
	}

	/// A quote inside a value was replaced by a space, so the exported comment was
	/// not what the operator wrote. Escaping it keeps both the document and the
	/// text.
	void quoteInCommentIsEscapedNotReplaced()
	{
		const QString comment = QStringLiteral("a \"quoted\" word");
		const QByteArray json = vipEventsToJson(eventWithComment(comment));

		QVERIFY2(parseError(json).isEmpty(), qPrintable(parseError(json)));
		QVERIFY2(!json.contains("a  quoted  word"), "the quotes must not be replaced by spaces");
	}

	/// A backslash was not escaped at all, which produced a document no parser
	/// accepts. A Windows path in a comment is enough.
	void backslashInCommentKeepsTheDocumentValid()
	{
		const QByteArray json = vipEventsToJson(eventWithComment(QStringLiteral("see C:\\data\\run7")));

		QVERIFY2(parseError(json).isEmpty(), qPrintable(parseError(json)));
	}

	/// A value that happened to start and end with a quote was passed through
	/// untouched, which closed the string early.
	void quotedValueIsStillEscaped()
	{
		const QByteArray json = vipEventsToJson(eventWithComment(QStringLiteral("\"quoted whole\"")));

		QVERIFY2(parseError(json).isEmpty(), qPrintable(parseError(json)));
	}

	/// The document is UTF-8 by definition; the conversion used to be Latin-1,
	/// which turned everything outside it into a question mark.
	void nonLatinCommentSurvives()
	{
		const QString comment = QString::fromUtf8("mesure \xc3\xa9lev\xc3\xa9" "e \xe2\x84\x83");
		const QByteArray json = vipEventsToJson(eventWithComment(comment));

		QVERIFY2(parseError(json).isEmpty(), qPrintable(parseError(json)));
		QVERIFY2(!json.contains('?'), "characters outside Latin-1 must not become question marks");
	}
};

VIP_TEST_MAIN(TestEventJson)
#include "TestEventJson.moc"
