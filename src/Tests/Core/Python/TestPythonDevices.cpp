/// @file TestPythonDevices.cpp
///
/// Characterisation tests for the devices that build Python source out of values
/// they did not choose. Built only when the interpreter is compiled in.
///
/// numpy and scipy are replaced by stubs before each test: what is under test is
/// what the generated source does, not what the real libraries write, and the
/// stubs let the tests run wherever a plain interpreter is available.

#include <QTest>

#include "vip_test_main.h"

#include "VipPyNPZDevice.h"
#include "VipPyOperation.h"
#include "VipPyGenerator.h"
#include "VipPyProcessing.h"
#include "VipXmlArchive.h"

#include <QDir>
#include <QFile>

class TestPythonDevices : public QObject
{
	Q_OBJECT

	static QString markerPath() { return QDir::tempPath() + QStringLiteral("/vip_python_injection_marker.txt"); }

	/// A payload that closes the string literal the path used to be pasted into,
	/// evaluates an expression with an observable effect, and reopens the literal
	/// so that the surrounding source stays valid Python.
	static QString injectingPath(const QString& suffix = QStringLiteral(".npz"))
	{
		return QDir::tempPath() + QStringLiteral("/a' + str(open(r'%1', 'w').write('x')) + '%2").arg(markerPath()).arg(suffix);
	}

	static bool run(const QString& code)
	{
		// Bounded: without an interpreter, waiting on the result never returns.
		return VipPyInterpreter::instance()->execCode(code).value(30000).value<VipPyError>().isNull();
	}

	static void feed(VipProcessingObject& device)
	{
		VipNDArrayType<double> array(vipVector(4));
		for (int i = 0; i < 4; ++i)
			array(vipVector(i)) = i;

		// The array is handed to the interpreter by apply(), which needs the real
		// numpy; that step is allowed to fail. What matters is that the device has
		// recorded the data, because close() then generates its source.
		device.inputAt(0)->setData(VipAnyData(QVariant::fromValue(VipNDArray(array)), 0));
		device.update();
	}

private Q_SLOTS:

	void initTestCase()
	{
		if (!run(QStringLiteral("_vip_probe = 1 + 1")))
			QSKIP("no usable Python interpreter in this environment");
	}

	void init()
	{
		QFile::remove(markerPath());
		QVERIFY(run(QStringLiteral("import sys, types\n"
					   "_m = types.ModuleType('numpy')\n"
					   "_m.savez = lambda *a, **k: None\n"
					   "sys.modules['numpy'] = _m\n"
					   "_s = types.ModuleType('scipy')\n"
					   "_io = types.ModuleType('scipy.io')\n"
					   "_io.savemat = lambda *a, **k: None\n"
					   "_s.io = _io\n"
					   "sys.modules['scipy'] = _s\n"
					   "sys.modules['scipy.io'] = _io\n")));
	}

	void cleanup() { QFile::remove(markerPath()); }

	/// The stub really does let an injected expression run: without this, a green
	/// result below would prove nothing about the payload.
	void theHarnessCanObserveAnInjection()
	{
		QVERIFY(run(QStringLiteral("import numpy as np\n"
					   "np.savez('x' + str(open(r'%1', 'w').write('x')) + '.npz')\n")
			      .arg(markerPath())));

		QVERIFY2(QFile::exists(markerPath()), "the harness must be able to observe an injection");
	}

	/// The destination path was pasted between quotes into the generated source. A
	/// device path is written to the session file and restored from it unchecked,
	/// so a quote in it closed the literal and the rest of the name ran as code.
	/// Nothing from the path may reach the interpreter as source.
	void npzPathCannotInjectCode()
	{
		VipPyNPZDevice device;
		device.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		device.setPath(injectingPath());
		QVERIFY2(device.open(VipIODevice::WriteOnly), "the payload ends with the suffix the device requires");

		feed(device);
		device.close();

		QVERIFY2(!QFile::exists(markerPath()), "the path must never be executed as Python");
	}

	/// Same on the MAT device, which builds its source the same way.
	void matPathCannotInjectCode()
	{
		VipPyMATDevice device;
		device.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		device.setPath(injectingPath(QStringLiteral(".mat")));
		QVERIFY2(device.open(VipIODevice::WriteOnly), "the payload ends with the suffix the device requires");

		feed(device);
		device.close();

		QVERIFY2(!QFile::exists(markerPath()), "the path must never be executed as Python");
	}

	/// A signal generator holds its expression in a property, and properties are
	/// written to the session file and read back from it. The application opens
	/// one session at every start without asking, so restoring the device used to
	/// be enough to run whatever code that file carried. Restored code must not run
	/// on its own.
	void restoredGeneratorCodeDoesNotRun()
	{
		const QString code = QStringLiteral("open(r'%1', 'w').write('x')\nvalue = 1.0\n").arg(markerPath());

		QString session;
		{
			VipPySignalGenerator source;
			source.propertyAt(0)->setData((qint64)1000000000);
			source.propertyAt(1)->setData((qint64)0);
			source.propertyAt(2)->setData((qint64)1000000000);
			source.propertyAt(3)->setData(code);

			VipXOStringArchive out;
			out.start("root");
			out.content(&source);
			out.end();
			session = out.toString();
		}
		QFile::remove(markerPath());

		VipXIStringArchive in(session);
		QVERIFY(in.isOpen());
		QVERIFY(in.start("root"));

		VipPySignalGenerator restored;
		in.content(&restored);
		QCOMPARE(restored.propertyAt(3)->value<QString>(), code);

		const bool opened = restored.open(VipIODevice::ReadOnly);
		QVERIFY2(!QFile::exists(markerPath()), "a restored expression must not be executed");
		QVERIFY2(!opened, "a restored expression must not be opened for reading");
	}

	/// The same expression typed by the user still runs: the gate is about where
	/// the code came from, not about the code.
	void generatorCodeTypedByTheUserStillRuns()
	{
		VipPySignalGenerator generator;
		generator.propertyAt(0)->setData((qint64)1000000000);
		generator.propertyAt(1)->setData((qint64)0);
		generator.propertyAt(2)->setData((qint64)1000000000);
		generator.propertyAt(3)->setData(QStringLiteral("value = 1.0"));

		QVERIFY(generator.open(VipIODevice::ReadOnly));
	}

	/// The parameters of a scriptable processing used to be turned into the source
	/// of the call that applies them: name, an equals sign, the value, all joined
	/// with commas inside setParameters(...). Both halves come out of a session
	/// file, so a closing parenthesis in a value ended the call and what followed
	/// ran as top level Python, at archive read time and with no user action.
	void processingParametersCannotInjectCode()
	{
		QVERIFY(run(QStringLiteral("class ThermavipMarkerProc:\n"
					   "    def setParameters(self, **kw): pass\n"
					   "procs = {}\n")));

		VipProcessingObject::Info info("MarkerProc", QString(), "Python", QIcon(), qMetaTypeId<VipPyProcessing*>());
		VipProcessingObject::registerAdditionalInfoObject(info);

		VipPyProcessing processing;
		QVERIFY2(processing.setStdPyProcessingFile("MarkerProc"), "the fixture class must be reachable");

		QVariantMap params;
		params["victim"] = QStringLiteral("1) or open(r'%1', 'w').write('x') or (1").arg(markerPath());
		processing.setStdProcessingParameters(params);

		QVERIFY2(!QFile::exists(markerPath()), "a parameter value must never be executed as Python");
	}
};

VIP_TEST_MAIN(TestPythonDevices)
#include "TestPythonDevices.moc"
