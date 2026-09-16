/// @file vip_test_main.h
///
/// Common entry point for the SDK tests. It replaces QTEST_MAIN and fixes two
/// problems QTEST_MAIN leaves open on Windows.
///
/// 1. abort(), a CRT assertion or an access violation open a MODAL DIALOG. A
///    test failing that way never returns: it blocks the developer's machine
///    and makes the CI job time out with no diagnostic. Everything is
///    redirected to stderr instead.
///
/// 2. The QTest report does not reach a redirected stream. Measured with a
///    minimal QTest executable linking none of the SDK: run in a console the
///    report shows, redirected to a file or a pipe it is EMPTY, and "-o -,txt"
///    changes nothing. That is exactly how CTest runs tests, so
///    "ctest --output-on-failure" reported a failure without saying which.
///    QTest therefore writes to a temporary file which is echoed to stdout with
///    fwrite, which does cross the redirection.
///
/// This is also where the Windows leak check belongs, since LSan is not ported
/// there.

#ifndef VIP_TEST_MAIN_H
#define VIP_TEST_MAIN_H

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QList>
#include <QTest>

#include <cstdio>

#ifdef _MSC_VER
#include <crtdbg.h>
#include <stdlib.h>
#include <windows.h>

/// Sends to stderr everything that would otherwise open a window.
inline void vipTestSilenceCrashDialogs()
{
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	const int reports[] = { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT };
	for (int r : reports) {
		_CrtSetReportMode(r, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(r, _CRTDBG_FILE_STDERR);
	}
}
#else
inline void vipTestSilenceCrashDialogs() {}
#endif

/// Arms the CRT debug heap, which reports leaks when the process exits. This is
/// the answer to LSan not being ported to Windows, and it costs almost nothing,
/// so it can stay enabled on every build.
///
/// Disabled under AddressSanitizer, and that is a measured fact: a deliberate
/// 4096-byte leak is reported without it and goes unnoticed with it, because
/// AddressSanitizer replaces the allocator. Arming both would give the illusion
/// of leak detection that does not happen.
inline void vipTestEnableCrtLeakCheck()
{
#if defined(_MSC_VER) && defined(_DEBUG) && !defined(THERMAVIP_WITH_ASAN)
	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
}

/// Message handler writing to stderr and flushing immediately: without the
/// flush the buffer of a process that aborts is lost, which is exactly when the
/// message matters most.
inline void vipTestMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
	const char* label = "";
	switch (type) {
		case QtDebugMsg: label = "DEBUG"; break;
		case QtInfoMsg: label = "INFO"; break;
		case QtWarningMsg: label = "WARNING"; break;
		case QtCriticalMsg: label = "CRITICAL"; break;
		case QtFatalMsg: label = "FATAL"; break;
	}
	std::fprintf(stderr, "[Qt %s] %s", label, qPrintable(msg));
	if (ctx.file)
		std::fprintf(stderr, "  (%s:%d)", ctx.file, ctx.line);
	std::fprintf(stderr, "\n");
	std::fflush(stderr);
}

/// Runs the test object, making sure the QTest report reaches stdout even when
/// redirected. Returns the QTest exit code.
inline int vipTestExec(QObject* testObject, int argc, char** argv)
{
	// If the caller already asked for an output, leave it alone.
	for (int i = 1; i < argc; ++i)
		if (qstrcmp(argv[i], "-o") == 0)
			return QTest::qExec(testObject, argc, argv);

	const QString report = QDir::tempPath() + QStringLiteral("/vip_test_%1_%2.txt").arg(testObject->metaObject()->className()).arg(QCoreApplication::applicationPid());

	QList<char*> args;
	for (int i = 0; i < argc; ++i)
		args.append(argv[i]);
	QByteArray oflag("-o");
	QByteArray otarget = report.toLocal8Bit() + ",txt";
	args.append(oflag.data());
	args.append(otarget.data());

	const int code = QTest::qExec(testObject, args.size(), args.data());

	QFile f(report);
	if (f.open(QIODevice::ReadOnly)) {
		const QByteArray content = f.readAll();
		f.close();
		std::fwrite(content.constData(), 1, static_cast<size_t>(content.size()), stdout);
		std::fflush(stdout);
		QFile::remove(report);
	}
	else {
		std::fprintf(stderr, "[harness] QTest report not found: %s\n", qPrintable(report));
		std::fflush(stderr);
	}
	return code;
}

/// Equivalent of QTEST_MAIN, with the guards above set before any Qt object.
#define VIP_TEST_MAIN(TestObject)                                                        	int main(int argc, char* argv[])                                                   	{                                                                                  		vipTestSilenceCrashDialogs();                                              		vipTestEnableCrtLeakCheck();                                               		qInstallMessageHandler(vipTestMessageHandler);                             		QApplication app(argc, argv);                                              		app.setAttribute(Qt::AA_Use96Dpi, true);                                   		TestObject tc;                                                             		QTEST_SET_MAIN_SOURCE_PATH                                                 		return vipTestExec(&tc, argc, argv);                                       	}

#endif // VIP_TEST_MAIN_H
