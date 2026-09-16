/// @file TestTimer.cpp
///
/// VipTimer says it can be started and stopped from any thread. These tests
/// pin what it answers about itself and that it fires.

#include <QTest>

#include "vip_test_main.h"

#include "VipTimer.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QThread>
#include <type_traits>

#include <atomic>

class TestTimer : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// The class used to derive from QThread, and declared start() and
	/// isRunning() next to the members of the same name of that base. Neither is
	/// virtual there, so the answer depended on the static type of the pointer:
	/// through a QThread* the timer never started, and a stopped timer reported
	/// itself as running because the thread was. The thread is held now.
	void theTimerIsNotAThread()
	{
		static_assert(!std::is_base_of<QThread, VipTimer>::value, "the timer must not be a thread");
		QVERIFY(true);
	}

	/// A timer that has not been started is not running, and says so.
	void aTimerThatWasNotStartedIsNotRunning()
	{
		VipTimer timer;
		QVERIFY(!timer.isRunning());
		QCOMPARE(timer.elapsed(), (qint64)0);
	}

	/// The nominal cycle: start, one timeout, and it stops by itself.
	void aSingleShotTimerFiresOnceAndStops()
	{
		VipTimer timer;
		timer.setInterval(50);
		timer.setSingleShot(true);

		QSignalSpy spy(&timer, SIGNAL(timeout()));
		QVERIFY(timer.start());
		QVERIFY(timer.isRunning());

		QVERIFY(spy.wait(30000));
		QCOMPARE(spy.count(), 1);

		QElapsedTimer waited;
		waited.start();
		while (timer.isRunning() && waited.elapsed() < 30000)
			QThread::msleep(1);
		QVERIFY(!timer.isRunning());
	}

	/// Stopping keeps the timeout from arriving.
	void stoppingBeforeTheIntervalCancelsTheTimeout()
	{
		VipTimer timer;
		timer.setInterval(30000);
		timer.setSingleShot(true);

		QSignalSpy spy(&timer, SIGNAL(timeout()));
		QVERIFY(timer.start());
		timer.stop();

		QVERIFY(!timer.isRunning());
		QThread::msleep(50);
		QCOMPARE(spy.count(), 0);
	}

	/// Refusing a restart is the documented answer, and it must be that one.
	void aRunningTimerRefusesToRestartWhenAskedTo()
	{
		VipTimer timer;
		timer.setInterval(30000);
		timer.setRestartWhenRunningEnabled(false);

		QVERIFY(timer.start());
		QVERIFY(!timer.start());

		timer.stop();
		QVERIFY(timer.start());
	}

	/// The class advertises start and stop from any thread.
	void startingAndStoppingFromAnotherThreadIsAccepted()
	{
		VipTimer timer;
		timer.setInterval(1);
		timer.setSingleShot(false);

		std::atomic<int> cycles{ 0 };
		QThread* worker = QThread::create([&]() {
			for (int i = 0; i < 500; ++i) {
				timer.start();
				timer.stop();
				++cycles;
			}
		});
		worker->start();
		QVERIFY(worker->wait(30000));
		delete worker;

		QCOMPARE(cycles.load(), 500);
		QVERIFY(!timer.isRunning());
	}
};

VIP_TEST_MAIN(TestTimer)

#include "TestTimer.moc"
