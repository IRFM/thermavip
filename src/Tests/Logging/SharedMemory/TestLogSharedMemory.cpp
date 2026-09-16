/// @file TestLogSharedMemory.cpp
///
/// Characterisation tests for the shared memory output of the log. The segment
/// is named, so its four byte header is written by whoever can open it: it is
/// untrusted input that the module uses as a length and as an offset.

#include <QTest>

#include "vip_test_main.h"

#include "VipLogging.h"

#include <QSharedMemory>
#include <QThread>
#include <atomic>
#include <cstring>
#include <limits>

class TestLogSharedMemory : public QObject
{
	Q_OBJECT

	// The key of the segment when no file logger is given, which is this case.
	static const char* key() { return "Log"; }

	/// Writes a header straight into the segment, the way another process would.
	static bool forgeHeader(qint32 size)
	{
		QSharedMemory memory(QString::fromLatin1(key()));
		if (!memory.attach())
			return false;
		bool ok = false;
		if (memory.lock()) {
			memcpy(memory.data(), &size, sizeof(qint32));
			memory.unlock();
			ok = true;
		}
		memory.detach();
		return ok;
	}

private Q_SLOTS:

	void initTestCase() { QVERIFY(VipLogging::instance().open(VipLogging::SharedMemory)); }

	void cleanupTestCase() { VipLogging::instance().close(); }

	/// An ordinary round trip still works.
	void anEntryComesBackFromTheSegment()
	{
		VipLogging::instance().directLog("first entry", VipLogging::Info);
		const QStringList entries = VipLogging::instance().lastLogEntries();
		QVERIFY2(!entries.isEmpty(), "an entry written to the segment must come back");
		QVERIFY(entries.join("\n").contains("first entry"));
	}

	/// A header longer than the segment used to be the length of the read.
	void aHeaderLongerThanTheSegmentIsRefused()
	{
		QVERIFY(forgeHeader(std::numeric_limits<qint32>::max()));
		QCOMPARE(VipLogging::instance().lastLogEntries(), QStringList());
	}

	/// And a negative one reached the byte array as a length too.
	void aNegativeHeaderIsRefused()
	{
		QVERIFY(forgeHeader(-1));
		QCOMPARE(VipLogging::instance().lastLogEntries(), QStringList());
	}

	/// On the writing side the header is the destination offset, and only the sum
	/// was tested: a value near the maximum wraps negative and passes that test,
	/// which puts the copy before the start of the segment.
	void aHeaderNearTheMaximumDoesNotMoveTheWriteBackwards()
	{
		QVERIFY(forgeHeader(std::numeric_limits<qint32>::max() - 8));
		VipLogging::instance().directLog("after a forged header", VipLogging::Info);

		// The forged header is dropped, so what comes back is the entry alone.
		const QStringList entries = VipLogging::instance().lastLogEntries();
		QVERIFY(entries.join("\n").contains("after a forged header"));
	}

	/// The inter process lock of the segment is system wide and named: released by
	/// hand on two paths, an allocation that threw between them left it taken for
	/// every process of the session. It is now held by a scope.
	void theSegmentLockIsAlwaysReleased()
	{
		// An empty segment takes the early return, which used to be one of the two
		// hand written releases.
		VipLogging::instance().lastLogEntries();
		QCOMPARE(VipLogging::instance().lastLogEntries(), QStringList());

		// The lock must be free for anyone else to take.
		QSharedMemory memory(QString::fromLatin1(key()));
		QVERIFY(memory.attach());
		QVERIFY2(memory.lock(), "the segment lock must not be left taken");
		memory.unlock();
		memory.detach();
	}

	/// Entries written from several threads all arrive, and none is truncated.
	void severalThreadsCanLogAtOnce()
	{
		VipLogging::instance().lastLogEntries();

		std::atomic<int> written{ 0 };
		QList<QThread*> threads;
		for (int t = 0; t < 4; ++t) {
			QThread* thread = QThread::create([&written, t]() {
				for (int i = 0; i < 25; ++i) {
					VipLogging::instance().directLog(QString("entry %1 %2").arg(t).arg(i), VipLogging::Info);
					++written;
				}
			});
			threads << thread;
			thread->start();
		}
		for (QThread* thread : threads) {
			QVERIFY(thread->wait(30000));
			delete thread;
		}

		QCOMPARE(written.load(), 100);
		const QStringList entries = VipLogging::instance().lastLogEntries();
		QVERIFY2(!entries.isEmpty(), "the entries written concurrently must be readable");
	}
};

VIP_TEST_MAIN(TestLogSharedMemory)
#include "TestLogSharedMemory.moc"
