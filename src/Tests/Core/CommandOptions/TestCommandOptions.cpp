/// @file TestCommandOptions.cpp
///
/// Characterisation tests for the command line parser, which is the first
/// untrusted input the application handles.

#include <QTest>

#include "vip_test_main.h"

#include "VipCommandOptions.h"

class TestCommandOptions : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// An empty argument reaches the parser whenever a script interpolates an
	/// unset variable between quotes. The first character of every argument was
	/// read before anything checked the length, which is out of bounds on an
	/// empty string: an assertion in debug builds, an indeterminate read
	/// otherwise. Parsing must simply return.
	void emptyArgumentIsNotIndexed()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.add("help");

		options.parse(QStringList() << "app" << QString() << "--help");

		QVERIFY2(options.count("help") > 0, "the flag after the empty argument must still be seen");
		QVERIFY2(options.positional().contains(QString()), "the empty argument is positional");
	}

	/// Same argument in the position where an optional value is looked for.
	void emptyArgumentAfterOptionIsNotIndexed()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.add("output", QString(), VipCommandOptions::ValueOptional);

		options.parse(QStringList() << "app" << "--output" << QString());

		QVERIFY(options.count("output") > 0);
	}

	/// A lone prefix is neither a flag nor a value, and must stay positional.
	void lonePrefixStaysPositional()
	{
		VipCommandOptions& options = VipCommandOptions::instance();

		options.parse(QStringList() << "app" << "-");

		QVERIFY(options.positional().contains(QStringLiteral("-")));
	}

	/// A short option consuming a value must take it out of the argument stream,
	/// which is what the long form does. It used to read the value and leave it in
	/// place, so the next turn of the loop saw it again as an argument of its own.
	void shortOptionConsumesItsValue()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.setFlagStyle(VipCommandOptions::DoubleDash);
		options.add("output", QString(), VipCommandOptions::ValueRequired);
		options.alias("output", "o");

		options.parse(QStringList() << "app" << "-o" << "result.txt" << "keep");

		QCOMPARE(options.value("output").toString(), QString("result.txt"));
		QVERIFY2(!options.positional().contains(QStringLiteral("result.txt")), "the value must not show up as a positional argument");
		QVERIFY(options.positional().contains(QStringLiteral("keep")));
	}

	/// The long form, for comparison: it already behaved this way.
	void longOptionConsumesItsValue()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.setFlagStyle(VipCommandOptions::DoubleDash);
		options.add("target", QString(), VipCommandOptions::ValueRequired);

		options.parse(QStringList() << "app" << "--target" << "result.txt" << "keep");

		QCOMPARE(options.value("target").toString(), QString("result.txt"));
		QVERIFY(!options.positional().contains(QStringLiteral("result.txt")));
	}

	/// The name to option map cached raw pointers into a QList held by value, so
	/// every append moved the elements and left those pointers on freed memory.
	/// parse() writes through them while count() rescans the live list, so an
	/// option registered before the table grew never records anything.
	void anOptionSurvivesTheTableGrowing()
	{
		VipCommandOptions& options = VipCommandOptions::instance();
		options.setFlagStyle(VipCommandOptions::DoubleDash);
		options.add("early");
		for (int i = 0; i < 64; ++i)
			options.add(QStringLiteral("filler%1").arg(i));

		options.parse(QStringList() << "app" << "--early");

		QCOMPARE(options.count("early"), 1);
	}

	/// The singleton reset the style on every access, so setFlagStyle() had no
	/// lasting effect and no caller could pick another convention.
	void theFlagStyleSurvivesTheNextAccess()
	{
		VipCommandOptions::instance().setFlagStyle(VipCommandOptions::Slash);
		QCOMPARE(VipCommandOptions::instance().flagStyle(), VipCommandOptions::Slash);

		VipCommandOptions& options = VipCommandOptions::instance();
		options.add("slashed");
		options.parse(QStringList() << "app" << "/slashed");
		QCOMPARE(options.count("slashed"), 1);

		VipCommandOptions::instance().setFlagStyle(VipCommandOptions::DoubleDash);
	}
};

VIP_TEST_MAIN(TestCommandOptions)
#include "TestCommandOptions.moc"
