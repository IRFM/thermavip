/// @file TestStandardWidgets.cpp
///
/// Characterisation tests for the entry widgets of the SDK.

#include <QTest>

#include "vip_test_main.h"

#include "VipGenericDevice.h"
#include <QFileInfo>

#include "VipStandardWidgets.h"

#include <QLineEdit>

class TestStandardWidgets : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// The parsing loop dropped every component it read, so the coordinate stayed
	/// empty: the widget declared itself invalid whatever it held, stayed in the
	/// error style, and never returned an entered value.
	void aMultiComponentEditReadsBackWhatItShows()
	{
		VipMultiComponentDoubleEdit edit;
		edit.setFixedNumberOfComponents(2);

		QLineEdit* line = edit.findChild<QLineEdit*>();
		QVERIFY(line);
		line->setText("1.5 , -2.25");

		QVERIFY2(edit.isValid(), "the widget must read back what is written in it");

	}

	/// A count of components that does not match what is shown is still refused.
	void aMultiComponentEditRefusesTheWrongCount()
	{
		VipMultiComponentDoubleEdit edit;
		edit.setFixedNumberOfComponents(3);

		QLineEdit* line = edit.findChild<QLineEdit*>();
		QVERIFY(line);
		line->setText("1.5 , -2.25");

		QVERIFY(!edit.isValid());
	}

	/// The slider is built from the span of the spin box, which accepts the whole
	/// range of a double until a bound is set: the count of steps was converted to
	/// an int from an infinite span, which gave a maximum below the minimum.
	void aSliderEditSurvivesAnUnboundedRange()
	{
		VipDoubleSliderEdit edit;
		edit.setSingleStep(0.001);

		VipDoubleSliderEdit bounded;
		bounded.setMinimum(0);
		bounded.setMaximum(10);
		bounded.setSingleStep(0.5);
		bounded.setValue(2.5);
		QCOMPARE(bounded.value(), 2.5);
	}

	/// The date prefixed name of a recording is built from the directory of the
	/// path. That directory used to be rebuilt by removing a substring from the
	/// raw path: on a path with backslashes nothing was removed, so the output
	/// directory was the file itself and the recording was lost without a word;
	/// and on a path whose directory carries the name of the file, both
	/// occurrences went and the file landed one level up.
	void theGeneratedRecordingNameStaysInItsDirectory()
	{
		VipGenericRecorder recorder;
		recorder.setDatePrefix("yyyy.MM.dd");
		recorder.setHasDatePrefix(true);

		recorder.setPath("C:\\data\\run.h5");
		QString generated = recorder.generateFilename();
		QVERIFY2(generated.endsWith("run.h5"), qPrintable(generated));
		QVERIFY2(!generated.contains("run.h5/"), qPrintable(generated));
		QCOMPARE(QFileInfo(generated).absolutePath(), QFileInfo("C:/data/run.h5").absolutePath());

		recorder.setPath("/data/run.h5/run.h5");
		generated = recorder.generateFilename();
		QCOMPARE(QFileInfo(generated).absolutePath(), QFileInfo("/data/run.h5/run.h5").absolutePath());
	}
};

VIP_TEST_MAIN(TestStandardWidgets)
#include "TestStandardWidgets.moc"
