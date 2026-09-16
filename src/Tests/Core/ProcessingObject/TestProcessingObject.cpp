/// @file TestProcessingObject.cpp
///
/// Characterisation tests for the scheduling pivot of the SDK. They capture
/// CURRENT behaviour, flaws included: the point is to lock down boundaries so
/// that any change to these surfaces shows up, not to validate a specification.

#include <QTest>

#include "vip_test_main.h"

#include "VipProcessingObject.h"
#include "VipProcessingSnapshot.h"
#include "VipImageProcessing.h"
#include "VipStandardProcessing.h"
#include "VipStreamingFromDevice.h"
#include "VipXmlArchive.h"
#include "VipCore.h"
#include "VipIterator.h"

#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <QElapsedTimer>
#include <QThread>

#ifdef _WIN32
#include <windows.h>
#else
#include <ctime>
#endif

/// Processor time charged to the calling thread, in milliseconds. A wait that
/// sleeps leaves it flat; a wait that spins makes it follow the clock.
static qint64 processorMilliseconds()
{
#ifdef _WIN32
	FILETIME creation, exited, kernel, user;
	if (!GetThreadTimes(GetCurrentThread(), &creation, &exited, &kernel, &user))
		return 0;
	const auto toMs = [](const FILETIME& f) { return ((static_cast<qint64>(f.dwHighDateTime) << 32) | f.dwLowDateTime) / 10000; };
	return toMs(kernel) + toMs(user);
#else
	timespec ts;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
		return 0;
	return static_cast<qint64>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
#endif
}

// ---------------------------------------------------------------------------
// Test processings, kept to the strict minimum.
// ---------------------------------------------------------------------------

/// Multiplies its input by a property. Counts its invocations, which makes the
/// scheduling observable.
/// A processing whose output list can be resized, to reach the I/O change path.
class MultiOutputProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipMultiOutput outputs)

protected:
	void apply() override {}
};

class MultiplyByProperty : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty factor)

public:
	std::atomic<int> applyCount{ 0 };

	MultiplyByProperty(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		propertyAt(0)->setData(2.0);
	}

protected:
	void apply() override
	{
		const double in = inputAt(0)->data().value<double>();
		const double f = propertyAt(0)->value<double>();
		outputAt(0)->setData(create(QVariant(in * f)));
		++applyCount;
	}
};

/// Adds a constant. Used to compose a VipProcessingList.
class AddOne : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	AddOne(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override { outputAt(0)->setData(create(QVariant(inputAt(0)->data().value<double>() + 1.0))); }
};

/// Throws from apply(). The task pool thread has to survive it.
class ThrowingProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)

public:
	ThrowingProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override { throw std::runtime_error("boom"); }
};

/// Takes a long time and no processor while doing so.
class SlowProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	SlowProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override
	{
		QThread::msleep(400);
		outputAt(0)->setData(create(inputAt(0)->data().data()));
	}
};

/// Reports when the list propagates its source properties to it. The hook is
/// the shortest reimplemented virtual the list calls while it inserts.
class WatchingProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	std::function<void()> onSourceProperty;

	WatchingProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

	void setSourceProperty(const char* name, const QVariant& value) override
	{
		if (onSourceProperty)
			onSourceProperty();
		VipProcessingObject::setSourceProperty(name, value);
	}

protected:
	void apply() override { outputAt(0)->setData(create(inputAt(0)->data().data())); }
};

/// Reports an image transform of its own and passes the image through, so the
/// composition done by the base class can be measured.
class TransformingProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)

public:
	QTransform transform;

	TransformingProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

	using VipProcessingObject::imageTransform;
	QTransform imageTransform(bool* from_center) const override
	{
		*from_center = true;
		return transform;
	}

protected:
	void apply() override { outputAt(0)->setData(create(inputAt(0)->data().data())); }
};

/// Carries a multi input, so the container operations can be exercised.
class MultiInputProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiInput inputs)
	VIP_IO(VipOutput output)

public:
	MultiInputProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
	}

protected:
	void apply() override {}
};

// ---------------------------------------------------------------------------

class TestProcessingObject : public QObject
{
	Q_OBJECT

	static VipAnyData makeData(double v)
	{
		return VipAnyData(QVariant(v), 0);
	}

private Q_SLOTS:

	// -- Input buffer boundaries --------------------------------------------

	/// A fresh buffer is empty and status() is -1 until something is pushed.
	void dataListEmptyBeforeFirstPush()
	{
		const VipDataList::Type types[] = { VipDataList::FIFO, VipDataList::LIFO, VipDataList::LastAvailable };
		for (VipDataList::Type type : types) {
			MultiplyByProperty proc;
			proc.inputAt(0)->setListType(type, VipDataList::None);
			QVERIFY2(proc.inputAt(0)->empty(), "a fresh buffer must be empty");
			QCOMPARE(proc.inputAt(0)->status(), -1);
			QVERIFY(!proc.inputAt(0)->hasNewData());
		}
	}

	/// After a single push the buffer is no longer empty and data is available,
	/// whatever the list type.
	void dataListSingleElement()
	{
		const VipDataList::Type types[] = { VipDataList::FIFO, VipDataList::LIFO, VipDataList::LastAvailable };
		for (VipDataList::Type type : types) {
			MultiplyByProperty proc;
			VipInput* in = proc.inputAt(0);
			in->setListType(type, VipDataList::None);
			in->setData(makeData(7.0));

			QVERIFY(!in->empty());
			QVERIFY(in->hasNewData());
			QCOMPARE(in->data().value<double>(), 7.0);
		}
	}

	/// Ordering, measured on the three VipDataList implementations DIRECTLY
	/// rather than through VipInput, which replaces instead of queueing while
	/// the processing is not asynchronous (see the dedicated test below).
	void dataListOrderingPerType()
	{
		{
			VipFIFOList list;
			list.setListLimitType(VipDataList::None);
			list.push(makeData(1.0));
			list.push(makeData(2.0));
			list.push(makeData(3.0));
			QCOMPARE(list.next().value<double>(), 1.0);
		}
		{
			VipLIFOList list;
			list.setListLimitType(VipDataList::None);
			list.push(makeData(1.0));
			list.push(makeData(2.0));
			list.push(makeData(3.0));
			QCOMPARE(list.next().value<double>(), 3.0);
		}
		{
			VipLastAvailableList list;
			list.push(makeData(1.0));
			list.push(makeData(2.0));
			list.push(makeData(3.0));
			QCOMPARE(list.next().value<double>(), 3.0);
		}
	}

	/// A FIFO drains in insertion order until exhausted.
	void fifoDrainsInInsertionOrder()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::None);
		for (int i = 0; i < 5; ++i)
			list.push(makeData(i));
		QCOMPARE(list.remaining(), 5);

		for (int i = 0; i < 5; ++i)
			QCOMPARE(list.next().value<double>(), static_cast<double>(i));
		QCOMPARE(list.remaining(), 0);
	}

	/// LastAvailable always returns the last data, even when read repeatedly:
	/// this is what tells it apart from a FIFO of size one.
	void lastAvailableKeepsReturningLastData()
	{
		VipLastAvailableList list;
		list.push(makeData(5.0));

		QCOMPARE(list.next().value<double>(), 5.0);
		QCOMPARE(list.next().value<double>(), 5.0);
		QCOMPARE(list.next().value<double>(), 5.0);
	}

	/// readAll is documented as reading and removing. LastAvailable read the datum
	/// and left the flag set, so a caller of readAll or allNext saw the same datum
	/// as new for ever.
	void readAllConsumesTheDatum()
	{
		VipLastAvailableList list;
		list.push(makeData(5.0));
		QVERIFY(list.hasNewData());

		VipAnyDataList read;
		QVERIFY(list.readAll(read));
		QCOMPARE(read.size(), 1);
		QCOMPARE(read[0].value<double>(), 5.0);

		QVERIFY2(!list.hasNewData(), "the datum was read, so it is no longer new");
		QCOMPARE(list.status(), 0);

		VipAnyDataList again;
		QVERIFY2(!list.readAll(again), "a second read has nothing to give");

		// The value itself stays available, which is what tells this list apart from
		// a FIFO of size one.
		QCOMPARE(list.probe().value<double>(), 5.0);
	}

	/// A fresh list is empty and status() is -1 until something is pushed.
	void dataListEmptyBeforeFirstPushDirect()
	{
		{
			VipFIFOList list;
			QVERIFY(list.empty());
			QCOMPARE(list.status(), -1);
			QVERIFY(!list.hasNewData());
			QCOMPARE(list.remaining(), 0);
		}
		{
			VipLIFOList list;
			QVERIFY(list.empty());
			QCOMPARE(list.status(), -1);
			QVERIFY(!list.hasNewData());
		}
		{
			VipLastAvailableList list;
			QVERIFY(list.empty());
			QCOMPARE(list.status(), -1);
			QVERIFY(!list.hasNewData());
		}
	}

	/// Count limit on a FIFO: push past the maximum and check the invariant,
	/// namely that the oldest data is what gets dropped.
	void fifoNumberLimitCaps()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::Number);
		list.setMaxListSize(4);
		QCOMPARE(list.maxListSize(), 4);

		for (int i = 0; i < 7; ++i)
			list.push(makeData(i));

		const int remaining = list.remaining();
		QVERIFY2(remaining <= 4, qPrintable(QStringLiteral("count limit not honoured: %1 left for a maximum of 4").arg(remaining)));
		QVERIFY(remaining > 0);
		QVERIFY2(list.next().value<double>() > 0.0, "the limit must drop the oldest data");
	}

	/// Same limit on a LIFO, where it is implemented the other way round. Only
	/// the shared invariant is pinned here.
	void lifoNumberLimitCaps()
	{
		VipLIFOList list;
		list.setListLimitType(VipDataList::Number);
		list.setMaxListSize(4);

		for (int i = 0; i < 7; ++i)
			list.push(makeData(i));

		const int remaining = list.remaining();
		QVERIFY2(remaining <= 4, qPrintable(QStringLiteral("count limit not honoured on LIFO: %1 left for a maximum of 4").arg(remaining)));
		QVERIFY(remaining > 0);
	}

	/// The memory cap is a floor, not a ceiling: the eviction walks from the
	/// newest datum backwards and stops on the one that reaches the cap, which it
	/// keeps. Measured on a datum whose footprint is read rather than assumed.
	void theMemoryCapKeepsTheDatumThatReachesIt()
	{
		const qint64 one = makeData(0).memoryFootprint();
		QVERIFY(one > 0);

		VipFIFOList list;
		list.setListLimitType(VipDataList::MemorySize);
		list.setMaxListMemory(3 * one);

		for (int i = 0; i < 8; ++i)
			list.push(makeData(i));
		QCOMPARE(list.remaining(), 3);

		// One byte below three data worth of footprint: the third is still kept,
		// so the buffer holds more than the cap allows. That is the contract.
		VipFIFOList tight;
		tight.setListLimitType(VipDataList::MemorySize);
		tight.setMaxListMemory(3 * one - 1);
		for (int i = 0; i < 8; ++i)
			tight.push(makeData(i));
		QCOMPARE(tight.remaining(), 3);
	}

	/// The two cap kinds are a combination, and the tighter of the two decides.
	void theTighterOfTheTwoCapsDecides()
	{
		const qint64 one = makeData(0).memoryFootprint();

		VipFIFOList memoryTighter;
		memoryTighter.setListLimitType(VipDataList::Number | VipDataList::MemorySize);
		memoryTighter.setMaxListSize(6);
		memoryTighter.setMaxListMemory(2 * one);
		for (int i = 0; i < 9; ++i)
			memoryTighter.push(makeData(i));
		QCOMPARE(memoryTighter.remaining(), 2);

		VipFIFOList numberTighter;
		numberTighter.setListLimitType(VipDataList::Number | VipDataList::MemorySize);
		numberTighter.setMaxListSize(2);
		numberTighter.setMaxListMemory(100 * one);
		for (int i = 0; i < 9; ++i)
			numberTighter.push(makeData(i));
		QCOMPARE(numberTighter.remaining(), 2);

		// None means none, whatever the two values say.
		VipFIFOList uncapped;
		uncapped.setListLimitType(VipDataList::None);
		uncapped.setMaxListSize(2);
		uncapped.setMaxListMemory(one);
		for (int i = 0; i < 9; ++i)
			uncapped.push(makeData(i));
		QCOMPARE(uncapped.remaining(), 9);
	}

	/// A buffer holding exactly its number cap keeps everything: the eviction
	/// tests for strictly more, and the boundary is where an off-by-one shows.
	void aBufferAtItsExactNumberCapKeepsEverything()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::Number);
		list.setMaxListSize(3);

		for (int i = 0; i < 3; ++i)
			list.push(makeData(i));
		QCOMPARE(list.remaining(), 3);

		list.push(makeData(3));
		QCOMPARE(list.remaining(), 3);

		// And the oldest is the one that went, since this is a first in first out.
		QCOMPARE(list.next().data().value<double>(), 1.0);
	}

	/// A cap of one keeps the newest datum and nothing else.
	void aCapOfOneKeepsTheNewest()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::Number);
		list.setMaxListSize(1);

		for (int i = 0; i < 5; ++i)
			list.push(makeData(i));

		QCOMPARE(list.remaining(), 1);
		QCOMPARE(list.next().data().value<double>(), 4.0);
	}

	/// clear() drops pending data.
	void clearRemovesPendingData()
	{
		VipFIFOList list;
		list.setListLimitType(VipDataList::None);
		list.push(makeData(1.0));
		list.push(makeData(2.0));
		QCOMPARE(list.remaining(), 2);

		list.clear();
		QCOMPARE(list.remaining(), 0);
		QVERIFY(!list.hasNewData());
	}

	/// In synchronous mode, the default, VipInput::setData REPLACES the buffer
	/// content instead of queueing, whatever the list type. This is intended and
	/// commented in the code, but it was written nowhere else, and it makes any
	/// list type setting inert until the processing becomes asynchronous.
	void inputInSynchronousModeReplacesInsteadOfQueueing()
	{
		const VipDataList::Type types[] = { VipDataList::FIFO, VipDataList::LIFO, VipDataList::LastAvailable };
		for (VipDataList::Type type : types) {
			MultiplyByProperty proc;
			QVERIFY2(!(proc.scheduleStrategies() & VipProcessingObject::Asynchronous), "the default mode must be synchronous");

			VipInput* in = proc.inputAt(0);
			in->setListType(type, VipDataList::None);
			in->setData(makeData(1.0));
			in->setData(makeData(2.0));
			in->setData(makeData(3.0));

			QCOMPARE(in->buffer()->remaining(), 1);
			QCOMPARE(in->data().value<double>(), 3.0);
		}
	}

	// -- Synchronous pipeline -----------------------------------------------

	/// One data in, apply() runs once, the output carries the expected result.
	void synchronousUpdateAppliesOnce()
	{
		MultiplyByProperty proc;
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		proc.propertyAt(0)->setData(3.0);
		proc.inputAt(0)->setData(makeData(4.0));

		proc.update();

		QCOMPARE(proc.applyCount.load(), 1);
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 12.0);
	}

	/// The property is read on every application, not captured once.
	void propertyChangeIsHonoured()
	{
		MultiplyByProperty proc;
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);

		proc.propertyAt(0)->setData(2.0);
		proc.inputAt(0)->setData(makeData(10.0));
		proc.update();
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 20.0);

		proc.propertyAt(0)->setData(5.0);
		proc.inputAt(0)->setData(makeData(10.0));
		proc.update();
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 50.0);
	}

	/// A disabled processing must not apply.
	void disabledProcessingDoesNotApply()
	{
		MultiplyByProperty proc;
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		proc.setEnabled(false);
		proc.inputAt(0)->setData(makeData(4.0));
		proc.update();

		QCOMPARE(proc.applyCount.load(), 0);
	}

	// -- Asynchronous scheduling and thread pool ----------------------------

	/// Data is queued, wait() returns once the queue is drained, and the apply
	/// count is exact. This is the only test exercising run() and the pool.
	void asynchronousSchedulingConsumesEveryInput()
	{
		MultiplyByProperty proc;
		proc.setComputeTimeStatistics(false);
		proc.inputAt(0)->setListType(VipDataList::FIFO, VipDataList::None);
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, true);

		const int count = 200;
		VipInput* in = proc.inputAt(0);
		for (int i = 0; i < count; ++i)
			in->setData(makeData(1.0));

		// wait(bool wait_for_sources, int max_milli_time): the first parameter is
		// a boolean, not a timeout. Passing 30000 directly truncates to true and
		// leaves the timeout at -1, that is an unbounded wait.
		QVERIFY2(proc.wait(true, 30000), "wait() must return once the queue is drained");
		QCOMPARE(proc.applyCount.load(), count);
	}

	// -- Error paths ---------------------------------------------------------

	/// The selector is a property, so it comes from a session file or from an
	/// editor: out of range it must be refused, not used as an index.
	void aSelectorOutsideTheInputsIsRefused()
	{
		VipSwitch proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs);
		QVERIFY(inputs->resize(2));

		for (int selector : { -1, 2, 7 }) {
			proc.resetError();
			proc.propertyAt(0)->setData(selector);
			proc.inputAt(0)->setData(makeData(1.0));
			proc.update();

			QVERIFY2(proc.hasError(), qPrintable(QStringLiteral("selector %1 must be refused").arg(selector)));
			QCOMPARE(proc.errorCode(), (int)VipProcessingObject::WrongInputNumber);
		}
	}

	/// And in range it sends the input it names, and only that one.
	void theSelectorSendsTheInputItNames()
	{
		VipSwitch proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs);
		QVERIFY(inputs->resize(2));

		proc.propertyAt(0)->setData(1);
		proc.inputAt(0)->setData(makeData(10.0));
		proc.inputAt(1)->setData(makeData(20.0));
		proc.update();

		QVERIFY2(!proc.hasError(), qPrintable(proc.errorString()));
		QCOMPARE(proc.outputAt(0)->data().data().value<double>(), 20.0);
	}

	/// An attribute that is not on the datum is an error, not an empty output:
	/// a downstream processing reading an empty value would take it for a reading.
	void anAbsentAttributeIsAnError()
	{
		VipExtractAttribute proc;
		proc.propertyAt(0)->setData(QString("temperature"));

		proc.inputAt(0)->setData(makeData(1.0));
		proc.update();

		QVERIFY(proc.hasError());
		QVERIFY2(proc.errorString().contains(QStringLiteral("attribute")), qPrintable(proc.errorString()));
	}

	/// Asked for a number, a value that is not one is refused rather than sent as
	/// zero. The conversion itself is covered elsewhere; this is the error path.
	void anAttributeThatIsNotANumberIsRefusedWhenANumberIsAsked()
	{
		VipAnyData any(QVariant(1.0), 0);
		any.setAttribute("label", QString("not a number"));

		VipExtractAttribute proc;
		proc.propertyAt(0)->setData(QString("label"));
		proc.propertyAt(1)->setData(true);
		proc.inputAt(0)->setData(any);
		proc.update();

		QVERIFY(proc.hasError());
		QVERIFY2(proc.errorString().contains(QStringLiteral("double")), qPrintable(proc.errorString()));

		// The same attribute without the conversion goes through, and keeps the
		// time of the datum it came from.
		VipExtractAttribute plain;
		plain.propertyAt(0)->setData(QString("label"));
		plain.inputAt(0)->setData(any);
		plain.update();

		QVERIFY2(!plain.hasError(), qPrintable(plain.errorString()));
		QCOMPARE(plain.outputAt(0)->data().data().toString(), QStringLiteral("not a number"));
		QCOMPARE(plain.outputAt(0)->data().time(), (qint64)0);
	}

	/// An exception thrown by apply() reaches a thread of the task pool, where
	/// nothing above it can catch it: it has to become an error on the object
	/// rather than end the process.
	void anExceptionInApplyBecomesAnError()
	{
		ThrowingProcessing proc;
		proc.setComputeTimeStatistics(false);
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, true);

		proc.inputAt(0)->setData(makeData(1.0));

		QVERIFY2(proc.wait(true, 30000), "wait() must return even when apply() threw");
		QVERIFY(proc.hasError());
		QVERIFY2(proc.errorString().contains(QStringLiteral("Unhandled exception")), qPrintable(proc.errorString()));
		QVERIFY2(proc.errorString().contains(QStringLiteral("boom")), qPrintable(proc.errorString()));
	}

	// -- Session format round trip ------------------------------------------

	/// copy() serialises then reads the object back through the session format.
	/// VipClamp is used because it is actually registered with the type system,
	/// so this exercises the real path.
	void copyPreservesTypeAndProperties()
	{
		VipClamp proc;
		proc.propertyAt(0)->setData(-1.5);
		proc.propertyAt(1)->setData(7.25);

		VipProcessingObject* clone = proc.copy();
		QVERIFY2(clone != nullptr, "copy() must not return nullptr for a registered type");
		const std::unique_ptr<VipProcessingObject> guard(clone);

		QVERIFY2(qobject_cast<VipClamp*>(clone) != nullptr, "the copy must carry the concrete type");
		QCOMPARE(clone->propertyCount(), proc.propertyCount());
		QCOMPARE(clone->inputCount(), proc.inputCount());
		QCOMPARE(clone->outputCount(), proc.outputCount());
		QCOMPARE(clone->propertyAt(0)->value<double>(), -1.5);
		QCOMPARE(clone->propertyAt(1)->value<double>(), 7.25);
	}

	/// The copy is independent from the source.
	void copyIsIndependentFromSource()
	{
		VipClamp proc;
		proc.propertyAt(0)->setData(1.0);

		VipProcessingObject* clone = proc.copy();
		QVERIFY(clone != nullptr);
		const std::unique_ptr<VipProcessingObject> guard(clone);

		clone->propertyAt(0)->setData(99.0);
		QCOMPARE(proc.propertyAt(0)->value<double>(), 1.0);
	}

	/// Counterpart: a processing NOT registered with the type system cannot be
	/// read back and copy() returns nullptr, silently, with no error and no log.
	/// Pinned as is, because a plugin forgetting the registration macro gets
	/// exactly that today.
	void copyOfUnregisteredTypeReturnsNull()
	{
		MultiplyByProperty proc;
		proc.propertyAt(0)->setData(6.5);

		VipProcessingObject* clone = proc.copy();
		const std::unique_ptr<VipProcessingObject> guard(clone);
		QVERIFY2(clone == nullptr, "an unregistered type cannot be rebuilt by the factory");
	}

	/// The three offset processings are rebuildable by name. Regression guard:
	/// two of them were missing from the type registry, so copy() returned
	/// nullptr on them.
	void offsetProcessingsAreRegistered()
	{
		{
			VipStartAtZero p;
			const std::unique_ptr<VipProcessingObject> c(p.copy());
			QVERIFY2(c != nullptr, "VipStartAtZero must be rebuildable by name");
		}
		{
			VipStartYAtZero p;
			const std::unique_ptr<VipProcessingObject> c(p.copy());
			QVERIFY2(c != nullptr, "VipStartYAtZero must be rebuildable by name");
		}
		{
			VipXOffset p;
			const std::unique_ptr<VipProcessingObject> c(p.copy());
			QVERIFY2(c != nullptr, "VipXOffset must be rebuildable by name");
		}
	}

	/// The list writer records an element count. Kept separate from the read
	/// below so that a failure points at one side or the other.
	/// Each io type has its own pair of archive operators, and they were only ever
	/// exercised through a whole processing. Read back one type at a time: the name,
	/// the enabled flag and the connection address are what the pair carries.
	void theInputOperatorRoundTrips()
	{
		VipInput source;
		source.setName("an input");
		source.setEnabled(false);

		VipXOStringArchive out;
		QVERIFY(out.content("io", source));

		VipInput target;
		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		in.content("io", target);

		QVERIFY2(!in.hasError(), qPrintable(in.errorString()));
		QCOMPARE(target.name(), QStringLiteral("an input"));
		QCOMPARE(target.isEnabled(), false);
	}

	void theOutputOperatorRoundTrips()
	{
		VipOutput source;
		source.setName("an output");
		source.setEnabled(true);

		VipXOStringArchive out;
		QVERIFY(out.content("io", source));

		VipOutput target;
		target.setEnabled(false);
		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		in.content("io", target);

		QVERIFY2(!in.hasError(), qPrintable(in.errorString()));
		QCOMPARE(target.name(), QStringLiteral("an output"));
		QCOMPARE(target.isEnabled(), true);
	}

	/// A property carries a value on top of what the other io types write.
	void thePropertyOperatorCarriesItsValue()
	{
		VipProperty source;
		source.setName("a property");
		source.setData(VipAnyData(QVariant(42.5), VipInvalidTime));

		VipXOStringArchive out;
		QVERIFY(out.content("io", source));

		VipProperty target;
		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		in.content("io", target);

		QVERIFY2(!in.hasError(), qPrintable(in.errorString()));
		QCOMPARE(target.name(), QStringLiteral("a property"));
		QCOMPARE(target.data().data().value<double>(), 42.5);
	}

	/// The multi io types write a count and then their elements. The count is the
	/// field a session file could lie about, and it is read back here on the
	/// nominal path so the refusal of a bad one stays distinguishable.
	void theMultiInputOperatorRoundTripsItsElements()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs);
		QVERIFY(inputs->resize(3));
		inputs->at(1)->setName("the middle one");

		VipXOStringArchive out;
		QVERIFY(out.content("io", *inputs));

		MultiInputProcessing rebuilt;
		VipMultiInput* target = rebuilt.topLevelInputAt(0)->toMultiInput();
		QVERIFY(target);
		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		in.content("io", *target);

		QVERIFY2(!in.hasError(), qPrintable(in.errorString()));
		QCOMPARE(target->count(), 3);
		QCOMPARE(target->at(1)->name(), QStringLiteral("the middle one"));
	}

	void theMultiOutputOperatorRoundTripsItsElements()
	{
		MultiOutputProcessing proc;
		VipMultiOutput* outputs = proc.topLevelOutputAt(0)->toMultiOutput();
		QVERIFY(outputs);
		QVERIFY(outputs->resize(2));
		outputs->at(0)->setName("the first one");

		VipXOStringArchive out;
		QVERIFY(out.content("io", *outputs));

		MultiOutputProcessing rebuilt;
		VipMultiOutput* target = rebuilt.topLevelOutputAt(0)->toMultiOutput();
		QVERIFY(target);
		VipXIStringArchive in(out.toString());
		QVERIFY(in.isOpen());
		in.content("io", *target);

		QVERIFY2(!in.hasError(), qPrintable(in.errorString()));
		QCOMPARE(target->count(), 2);
		QCOMPARE(target->at(0)->name(), QStringLiteral("the first one"));
	}

	/// The error is a flag next to a string, and both are read by callers that
	/// never touched the object that set them. resetError() has to clear the two.
	void resetErrorClearsTheFlagAndTheMessage()
	{
		AddOne proc;
		QVERIFY(!proc.hasError());

		proc.setError("something went wrong", VipProcessingObject::WrongInput);
		QVERIFY(proc.hasError());
		QCOMPARE(proc.errorString(), QStringLiteral("something went wrong"));
		QCOMPARE(proc.errorCode(), (int)VipProcessingObject::WrongInput);

		proc.resetError();
		QVERIFY2(!proc.hasError(), "the flag must go down, or every later read reports a stale error");
		QVERIFY(proc.errorString().isEmpty());
		// And the code stays -1, which is RuntimeError: reading the code of an object
		// that never failed answers an error identifier, so callers must read the flag.
		QCOMPARE(proc.errorCode(), -1);
	}

	void sessionListWritesItsCount()
	{
		VipProcessingList source;
		// A registered processing: the writer only serialises those the factory
		// can rebuild.
		QVERIFY(source.append(new VipClamp()));

		VipXOStringArchive out;
		QVERIFY(out.content("list", &source));
		QVERIFY2(out.toString().contains(">1</count>"), qPrintable(out.toString()));
	}

	/// The reader used that count directly as a loop bound, so a session file
	/// declaring a huge one made it allocate until memory ran out, silently.
	/// The archive here is a fixed string rather than one just written, which is
	/// what a crafted session file actually is.
	void hugeCountInSessionIsRejected()
	{
		const QString xml = QStringLiteral(
			"<list type_name=\"VipProcessingList*\">"
			"<processing_name type_name=\"QString\"></processing_name>"
			"<count type_name=\"qlonglong\">100000000</count>"
			"</list>");

		VipProcessingList target;
		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		in.content("list", &target);

		QCOMPARE(target.size(), 0);
		QVERIFY2(in.hasError(), "an out of range count must be reported, not consumed");
	}

	/// A negative count is the other end of the same field. It used to be a loop
	/// bound taken as read, so it silently produced an empty list and no error.
	void aNegativeCountInSessionIsRejected()
	{
		const QString xml = QStringLiteral(
			"<list type_name=\"VipProcessingList*\">"
			"<processing_name type_name=\"QString\"></processing_name>"
			"<count type_name=\"qlonglong\">-5</count>"
			"</list>");

		VipProcessingList target;
		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		in.content("list", &target);

		QCOMPARE(target.size(), 0);
		QVERIFY2(in.hasError(), "a negative count must be reported, not consumed");
	}

	/// A session naming a class the factory does not know must leave the list
	/// empty and say so, not append a null or stop reading silently.
	void aSessionNamingAnUnknownProcessingIsRefused()
	{
		const QString xml = QStringLiteral(
			"<list type_name=\"VipProcessingList*\">"
			"<processing_name type_name=\"QString\"></processing_name>"
			"<count type_name=\"qlonglong\">1</count>"
			"<processing type_name=\"NoSuchProcessingClass*\">"
			"<processing_name type_name=\"QString\">ghost</processing_name>"
			"</processing>"
			"</list>");

		VipProcessingList target;
		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		in.content("list", &target);

		QCOMPARE(target.size(), 0);
		for (int i = 0; i < target.size(); ++i)
			QVERIFY2(target.at(i), "a null processing must never be appended");
	}

	/// Every change of an I/O ran a full descent of the upstream graph, output ones
	/// included, and an output cannot change the set of sources. Resizing a list of
	/// outputs runs one such change per element added.
	void anIoChangeOnAnOutputDoesNotWalkTheSources()
	{
		MultiplyByProperty upstream;
		MultiOutputProcessing downstream;
		downstream.inputAt(0)->setConnection(upstream.outputAt(0));

		// Marks both nodes, which is what makes a later I/O change descend at all.
		downstream.setSourceProperty("Probe", QVariant(1));
		QCOMPARE(upstream.property("Probe").toInt(), 1);

		upstream.setProperty("Probe", QVariant(2));

		VipMultiOutput* outputs = downstream.topLevelOutputAt(0)->toMultiOutput();
		QVERIFY(outputs);
		outputs->resize(outputs->count() + 1);

		// The source keeps what was written on it directly.
		QCOMPARE(upstream.property("Probe").toInt(), 2);
	}

	/// Extracting an attribute as a number must accept a number and nothing
	/// else. The conversion used to accept any string starting with digits and
	/// silently drop the rest, so "12abc" became 12 and "1,5" became 1, both
	/// reported as valid measurements. Non finite values must be refused for the
	/// same reason: they travel downstream as if they had been measured.
	void attributeToDoubleRejectsPartialNumbers()
	{
		struct Case
		{
			const char* text;
			bool accepted;
			double value;
		};
		const Case cases[] = { { "12", true, 12.0 },	 { " 3.5 ", true, 3.5 },  { "-2.25", true, -2.25 }, { "1e3", true, 1000.0 },
				       { "12abc", false, 0.0 },	 { "1,5", false, 0.0 },	  { "abc", false, 0.0 },    { "", false, 0.0 },
				       { "nan", false, 0.0 },	 { "inf", false, 0.0 },	  { "1e999", false, 0.0 },  { "0x10", false, 0.0 } };

		for (const Case& c : cases) {
			VipExtractAttribute proc;
			proc.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
			proc.propertyAt(0)->setData(QString("measure"));
			proc.propertyAt(1)->setData(true);

			VipAnyData in(QVariant(0), 0);
			in.setAttribute("measure", QString::fromLatin1(c.text));
			proc.inputAt(0)->setData(in);
			proc.update();

			const bool accepted = !proc.hasError();
			QVERIFY2(accepted == c.accepted, qPrintable(QStringLiteral("'%1': expected %2, got %3").arg(c.text).arg(c.accepted).arg(accepted)));
			if (c.accepted)
				QCOMPARE(proc.outputAt(0)->data().value<double>(), c.value);
		}
	}

	// -- Image transform list ------------------------------------------------

	/// A transform list must survive a session round trip. It does not.
	///
	/// EXPECTED FAILURE. Saving two transforms writes 48 bytes — the size as a
	/// qsizetype plus twenty bytes per transform — and loading them back returns
	/// success with an empty list. The stream operators the file declares are
	/// static, so the metatype system does not use them: what runs is the generic
	/// container streaming, and the two halves do not agree. An image transform
	/// list stored in a session is therefore lost on reload, without a message.
	/// This test states the EXPECTED behaviour and must stay red until the format
	/// is made symmetric.
	void transformListRoundTrip()
	{
		TransformList source;
		source.push_back(Transform(Transform::Rotate, 90, 0));
		source.push_back(Transform(Transform::Scale, 2, 3));

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			QVERIFY(QMetaType(qMetaTypeId<TransformList>()).save(out, &source));
		}
		QCOMPARE(buffer.size(), 48);

		TransformList read;
		QDataStream in(buffer);
		QVERIFY(QMetaType(qMetaTypeId<TransformList>()).load(in, &read));

		QEXPECT_FAIL("", "the declared stream operators are static, the metatype system uses the generic ones", Continue);
		QCOMPARE(read.size(), source.size());
	}

	/// The memory a queue holds is counted in bytes, and the cap it feeds is too.
	/// The whole chain was a signed 32 bit int, which saturates at two gigabytes: a
	/// queue of large images passes that without difficulty, the count wraps, and a
	/// negative count compares favourably against any cap, so the limit stopped
	/// working exactly where it was needed.
	void queueMemoryAccountingIsSixtyFourBit()
	{
		VipFIFOList list;
		list.setMaxListMemory(Q_INT64_C(8) * 1024 * 1024 * 1024);
		QCOMPARE(list.maxListMemory(), Q_INT64_C(8) * 1024 * 1024 * 1024);

		VipProcessingManager::setMaxListMemory(Q_INT64_C(6) * 1024 * 1024 * 1024);
		QCOMPARE(VipProcessingManager::maxListMemory(), Q_INT64_C(6) * 1024 * 1024 * 1024);
		VipProcessingManager::setMaxListMemory(50000000);
	}

	/// The sum itself was an int, so a footprint above two gigabytes came back
	/// negative. The copies below share one buffer, so this measures the sum
	/// without allocating three gigabytes.
	void aFootprintAboveTwoGigabytesStaysPositive()
	{
		VipNDArrayType<double> big(vipVector(2048, 8192)); // 128 MB
		QVariantList many;
		for (int i = 0; i < 24; ++i)
			many.append(QVariant::fromValue(VipNDArray(big)));

		const qint64 footprint = vipGetMemoryFootprint(QVariant(many));
		QVERIFY2(footprint > Q_INT64_C(2) * 1024 * 1024 * 1024, "the sum must not wrap at two gigabytes");
	}

	/// And the eviction loop accumulated in an int too, so a negative sum never
	/// reached the cap and the buffer was never trimmed.
	void theMemoryCapTrimsTheBufferAboveTwoGigabytes()
	{
		VipNDArrayType<double> big(vipVector(2048, 8192)); // 128 MB, one shared buffer

		VipFIFOList list;
		list.setListLimitType(VipDataList::MemorySize);
		list.setMaxListMemory(Q_INT64_C(2560) * 1024 * 1024);

		int count = 0;
		for (int i = 0; i < 24; ++i)
			count = list.push(VipAnyData(QVariant::fromValue(VipNDArray(big)), i));

		QVERIFY2(count < 24, "the buffer must be trimmed once its footprint passes the cap");
	}

	/// A footprint larger than an int can hold is reported as it is.
	void largeDataFootprintIsNotTruncated()
	{
		VipNDArrayType<double> big(vipVector(4096, 8192)); // 256 MB
		VipAnyData any(QVariant::fromValue(VipNDArray(big)), 0);

		const qint64 footprint = any.memoryFootprint();
		QVERIFY2(footprint > 0, "a large array must not report a negative footprint");
		QVERIFY(footprint >= Q_INT64_C(256) * 1024 * 1024);
	}

	// -- Multiple inputs -----------------------------------------------------

	/// Inserting anywhere but at the end configured the element that happened to be
	/// last instead of the one just inserted, so the new entry was left unset. The
	/// setter next to it writes the right one.
	void insertingAnInputConfiguresTheInsertedOne()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs);

		QVERIFY(inputs->resize(3));
		QCOMPARE(inputs->count(), 3);

		QVERIFY(inputs->insert(0));
		QCOMPARE(inputs->count(), 4);
		for (int i = 0; i < inputs->count(); ++i)
			QVERIFY2(inputs->at(i)->parentProcessing() == &proc, qPrintable(QStringLiteral("input %1 was left unconfigured").arg(i)));
	}

	/// An index outside the vector is refused rather than applied.
	void insertingOutsideTheRangeIsRefused()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs->resize(2));

		QVERIFY(!inputs->insert(-1));
		QVERIFY(!inputs->insert(99));
		QCOMPARE(inputs->count(), 2);
	}

	/// The container is documented as empty by default, and back() on it is out of
	/// bounds: both accessors used it without a test.
	void anEmptyMultiInputHasNoData()
	{
		MultiInputProcessing proc;
		VipMultiInput* inputs = proc.topLevelInputAt(0)->toMultiInput();
		QVERIFY(inputs->resize(0));
		QCOMPARE(inputs->count(), 0);

		QVERIFY(!inputs->data().isValid());
		inputs->setData(makeData(1.0)); // must not crash
		QVERIFY(true);
	}

	// -- VipProcessingList ---------------------------------------------------

	/// An empty list short circuits and forwards its input to its output.
	void emptyProcessingListForwardsInput()
	{
		VipProcessingList list;
		list.setScheduleStrategy(VipProcessingObject::Asynchronous, false);
		QCOMPARE(list.size(), 0);

		list.inputAt(0)->setData(makeData(42.0));
		list.update();

		QCOMPARE(list.outputAt(0)->data().value<double>(), 42.0);
	}

	/// Two stacked processings apply in insertion order: (3 * 2) + 1 = 7, not
	/// (3 + 1) * 2 = 8.
	void processingListAppliesInInsertionOrder()
	{
		VipProcessingList list;
		list.setScheduleStrategy(VipProcessingObject::Asynchronous, false);

		MultiplyByProperty* mult = new MultiplyByProperty();
		mult->propertyAt(0)->setData(2.0);
		QVERIFY(list.append(mult));
		QVERIFY(list.append(new AddOne()));
		QCOMPARE(list.size(), 2);

		list.inputAt(0)->setData(makeData(3.0));
		list.update();

		QCOMPARE(list.outputAt(0)->data().value<double>(), 7.0);
	}

	/// VipProcessingList owns its processings: destroying the list destroys
	/// them, checked through QPointer.
	void processingListOwnsItsProcessings()
	{
		QPointer<VipProcessingObject> observed;
		{
			VipProcessingList list;
			AddOne* child = new AddOne();
			observed = child;
			QVERIFY(list.append(child));
			QVERIFY(!observed.isNull());
		}
		QVERIFY2(observed.isNull(), "destroying the list must destroy the processings it owns");
	}

	/// The indexed accessors cast their argument to size_t and indexed a vector
	/// with it, so a negative index addressed far past the end. They now hold
	/// the contract of the accessors by name: out of range gives nullptr.
	void indexedAccessorsRejectAnIndexOutOfRange()
	{
		MultiplyByProperty processing;

		QVERIFY(processing.inputAt(0));
		QVERIFY2(!processing.inputAt(-1), "a negative index must not be cast into a huge one");
		QVERIFY(!processing.inputAt(processing.inputCount()));
		QVERIFY(!processing.outputAt(-1));
		QVERIFY(!processing.outputAt(processing.outputCount()));
		QVERIFY(!processing.propertyAt(-1));
		QVERIFY(!processing.propertyAt(processing.propertyCount()));
		QVERIFY(!processing.topLevelInputAt(-1));
		QVERIFY(!processing.topLevelInputAt(processing.topLevelInputCount()));
		QVERIFY(!processing.topLevelOutputAt(processing.topLevelOutputCount()));
		QVERIFY(!processing.topLevelPropertyAt(processing.topLevelPropertyCount()));
	}

	/// Same for the list, whose position argument is public input: inserting
	/// past the end of a QList is undefined, and at()/take() indexed on trust.
	void processingListBoundsItsPositions()
	{
		VipProcessingList list;
		AddOne* first = new AddOne();
		QVERIFY(list.append(first));

		AddOne* late = new AddOne();
		QVERIFY2(list.insert(50, late), "a position past the end is clamped, not rejected");
		QCOMPARE(list.size(), 2);
		QCOMPARE(list.at(1), late);

		QVERIFY(!list.at(-1));
		QVERIFY(!list.at(list.size()));
		QVERIFY(!list.take(-1));
		QVERIFY(!list.take(list.size()));
		QCOMPARE(list.size(), 2);
	}

	/// Assigning an output left its data behind, so the replaced output kept
	/// serving the value of the one it was supposed to become. The sibling
	/// property class always assigned it.
	void assigningAnOutputCarriesItsData()
	{
		VipOutput first("first");
		first.setData(VipAnyData(QVariant(1.0), 10));
		VipOutput second("second");
		second.setData(VipAnyData(QVariant(2.0), 20));

		first = second;

		QCOMPARE(first.data().value<double>(), 2.0);
		QCOMPARE(first.data().time(), (qint64)20);
	}

	/// The set of error codes actually logged was copied from an empty member in
	/// the initialiser list, before the constructor body filled it, so nothing
	/// was ever logged.
	void theDefaultLoggedErrorCodesAreNotEmpty()
	{
		QVERIFY(VipProcessingManager::isLogErrorEnabled(VipProcessingObject::RuntimeError));
		QVERIFY(VipProcessingManager::isLogErrorEnabled(VipProcessingObject::WrongInput));
		QVERIFY(VipProcessingManager::isLogErrorEnabled(VipProcessingObject::IOError));
	}

	/// A connection carries a parent processing object only once it has been
	/// attached to one. Opening a standalone connection walked straight through
	/// that null parent, and so did the branch meant to report the bad address.
	void anUnattachedConnectionReportsInsteadOfFaulting()
	{
		VipConnectionPtr connection(new VipConnection());
		connection->setupConnection("VipConnection:no_such_processing;output");

		QVERIFY2(!connection->openConnection(VipConnection::InputConnection), "an address that resolves to nothing must fail to open");
		QVERIFY(connection->hasError());

		connection->receiveData(VipAnyData(QVariant(1.0), 0));
	}

	/// Opening the connections of a processing dropped the result for every one
	/// of them, so a session whose address does not resolve reloaded silently
	/// incomplete.
	void openingConnectionsReportsAnAddressThatDoesNotResolve()
	{
		MultiplyByProperty processing;
		processing.setObjectName("consumer");
		processing.inputAt(0)->setConnection("VipConnection:no_such_processing;output");

		QVERIFY(!processing.openInputConnections());
	}

	/// The priority map used to be streamed through a reinterpret_cast onto a map
	/// of int, which is undefined and let any value from the stream reach
	/// QThread::setPriority. The wire format is unchanged.
	void aThreadPriorityOutsideTheEnumerationIsRefused()
	{
		QByteArray buffer;
		{
			QDataStream str(&buffer, QIODevice::WriteOnly);
			str << (quint32)2 << QString("known") << (qint32)QThread::HighPriority << QString("forged") << (qint32)987654;
		}

		PriorityMap map;
		{
			QDataStream str(&buffer, QIODevice::ReadOnly);
			str >> map;
		}

		QCOMPARE(map.size(), 2);
		QCOMPARE(map.value("known"), QThread::HighPriority);
		QCOMPARE(map.value("forged"), QThread::InheritPriority);
	}

	/// And a map written by the current code reads back identically.
	void thePriorityMapSurvivesARoundTrip()
	{
		PriorityMap written;
		written.insert("first", QThread::LowestPriority);
		written.insert("second", QThread::TimeCriticalPriority);

		QByteArray buffer;
		{
			QDataStream str(&buffer, QIODevice::WriteOnly);
			str << written;
		}
		PriorityMap read;
		{
			QDataStream str(&buffer, QIODevice::ReadOnly);
			str >> read;
		}
		QCOMPARE(read, written);
	}

	/// The descent along the sources had no visited set, so two processings that
	/// are sources of each other recursed until the stack ran out.
	void aCycleInTheSourcesDoesNotRecurseForever()
	{
		MultiplyByProperty first;
		MultiplyByProperty second;
		QVERIFY(first.outputAt(0)->setConnection(second.inputAt(0)));
		QVERIFY(second.outputAt(0)->setConnection(first.inputAt(0)));
		QVERIFY(first.directSources().contains(&second));
		QVERIFY(second.directSources().contains(&first));

		first.setSourceProperty("campaign", QVariant(7));

		QCOMPARE(first.property("campaign").toInt(), 7);
		QCOMPARE(second.property("campaign").toInt(), 7);
	}

	/// update() walks its sources while holding its own lock, and that lock does
	/// not nest. Two processings that are sources of each other brought the walk
	/// back to the first one on the same thread, onto the lock it was already
	/// holding, and nothing came back from there.
	void aCycleInTheSourcesDoesNotBlockUpdate()
	{
		MultiplyByProperty first;
		MultiplyByProperty second;
		first.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::NoThread);
		second.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::NoThread);

		QVERIFY(first.outputAt(0)->setConnection(second.inputAt(0)));
		QVERIFY(second.outputAt(0)->setConnection(first.inputAt(0)));

		first.inputAt(0)->setData(makeData(2.0));
		QVERIFY(first.update(true));

		QCOMPARE(first.applyCount.load(), 1);
		QCOMPARE(first.outputAt(0)->data().value<double>(), 4.0);
	}

	/// The reader dropped the connections and wrote the object as it went, so an
	/// archive that stops after the first field left a disconnected object
	/// carrying default values. It now applies nothing at all.
	void aTruncatedProcessingArchiveLeavesTheObjectAlone()
	{
		const QString xml = QStringLiteral("<processing type_name=\"VipClamp*\">"
						   "<processing_name type_name=\"QString\">from_file</processing_name>"
						   "</processing>");

		VipClamp target;
		target.setObjectName("original");
		target.setProcessingVisible(true);

		VipXIStringArchive in(xml);
		QVERIFY(in.isOpen());
		in.content("processing", &target);

		QCOMPARE(target.objectName(), QString("original"));
		QVERIFY2(target.isProcessingVisible(), "a field that was never read must not be applied");
	}

	/// The task pool outlives the derived parts of the object: it is destroyed by
	/// the base destructor, after every derived destructor has run. Nothing may
	/// be submitted or run from the moment destruction starts.
	void anObjectBeingDestroyedTakesNoMoreWork()
	{
		MultiplyByProperty* processing = new MultiplyByProperty();
		processing->inputAt(0)->setData(VipAnyData(QVariant(3.0), 0));
		QVERIFY(processing->update(true));
		const int applied = processing->applyCount.load();

		bool seen = false;
		bool accepted = true;
		bool flagged = false;
		QObject::connect(processing, &VipProcessingObject::destroyed, processing, [&](VipProcessingObject* obj) {
			seen = true;
			flagged = obj->isBeingDestroyed();
			obj->inputAt(0)->setData(VipAnyData(QVariant(5.0), 1));
			accepted = obj->update(true);
		});

		delete processing;

		QVERIFY2(seen, "the destruction signal must reach the slot");
		QVERIFY2(flagged, "the object must know it is being destroyed");
		QVERIFY2(!accepted, "no work may be submitted once destruction has started");
		Q_UNUSED(applied);
	}

	/// The setter says it takes ownership of the device. Refusing it once the
	/// object is open used to leave it neither stored nor destroyed, and the
	/// caller had already let go of it.
	void aRefusedDeviceIsStillDestroyed()
	{
		VipStreamingFromDevice streaming;
		VipAnyResource* inner = new VipAnyResource();
		inner->setData(QVariant(1.0));
		streaming.setIODevice(inner);
		QVERIFY(streaming.open(VipIODevice::ReadOnly));

		QPointer<VipIODevice> observed = new VipAnyResource();
		streaming.setIODevice(observed);

		QVERIFY2(observed.isNull(), "a device that is taken but not kept must be destroyed");
		QCOMPARE(streaming.IODevice(), (VipIODevice*)inner);
	}

	/// The signal that says a processing is done reaches a processing list in a
	/// direct connection, and that list then takes its own mutex; the list, while
	/// holding that mutex, runs its children, which take the lock serialising a
	/// run. Two threads closed the cycle, so the signal now goes out once that
	/// lock is released, through a flag the run leaves behind. This pins the
	/// contract of that flag: one signal per run, no more and no less.
	void processingDoneIsEmittedOncePerRun()
	{
		MultiplyByProperty processing;
		processing.inputAt(0)->setData(VipAnyData(QVariant(2.0), 0));

		processing.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::NoThread);

		// Running again from the slot needs the lock that serialises a run, and that
		// lock does not nest: emitted from under it, this call spins for ever.
		int depth = 0;
		QObject::connect(&processing,
				 &VipProcessingObject::processingDone,
				 &processing,
				 [&](VipProcessingObject* obj, qint64) {
					 ++depth;
				 },
				 Qt::DirectConnection);

		QVERIFY(processing.update(true));
		QCOMPARE(depth, 1);

		QVERIFY(processing.update(true));
		QCOMPARE(depth, 2);
	}

	/// The manager is read from the thread that processes and written from the
	/// thread that configures. The set of error codes and the map of priorities
	/// were read without the mutex that guards the writes, and a Qt container
	/// being rehashed is not readable. This exercises both sides at once.
	void theManagerSurvivesConcurrentConfiguration()
	{
		const QSet<int> initial = VipProcessingManager::logErrors();
		const qint64 initialMemory = VipProcessingManager::maxListMemory();

		std::atomic<bool> stop{ false };
		std::atomic<int> reads{ 0 };

		QThread* reader = QThread::create([&]() {
			while (!stop.load()) {
				VipProcessingManager::logErrors();
				VipProcessingManager::defaultPriorities();
				VipProcessingManager::maxListMemory();
				VipProcessingManager::listLimitType();
				++reads;
			}
		});
		reader->start();

		// The writer below is short: without this the reader could still be
		// starting when it ends, and the count asserted at the end would be zero.
		QElapsedTimer started;
		started.start();
		while (reads.load() == 0 && started.elapsed() < 30000)
			QThread::msleep(1);

		for (int i = 0; i < 200; ++i) {
			VipProcessingManager::setLogErrorEnabled(VipProcessingObject::RuntimeError, i % 2 == 0);
			VipProcessingManager::setMaxListMemory(40000000 + i);
		}

		stop.store(true);
		QVERIFY(reader->wait(30000));
		delete reader;

		QVERIFY2(reads.load() > 0, "the reader must have run");

		VipProcessingManager::setLogErrors(initial);
		VipProcessingManager::setMaxListMemory(initialMemory);
		QCOMPARE(VipProcessingManager::maxListMemory(), initialMemory);
	}

	/// The list ran its whole pipeline holding the mutex that guards its
	/// container, so anything else that only wanted to look at the list waited
	/// behind every processing. Looking at it from inside a processing of that
	/// same list is the shortest way to show the mutex is no longer held: the
	/// mutex nests, but only for the thread that owns it, and this is the thread
	/// that runs the pipeline.
	void theListDoesNotHoldItsMutexWhileRunning()
	{
		VipProcessingList list;
		AddOne* first = new AddOne();
		QVERIFY(list.append(first));

		bool sourcesRead = false;
		QObject::connect(first,
				 &VipProcessingObject::processingDone,
				 &list,
				 [&](VipProcessingObject*, qint64) {
					 // Reads the container under the mutex.
					 list.directSources();
					 sourcesRead = true;
				 },
				 Qt::DirectConnection);

		list.inputAt(0)->setData(VipAnyData(QVariant(1.0), 0));
		QVERIFY(list.update(true));
		list.wait();

		QVERIFY2(sourcesRead, "the container must be readable while the pipeline runs");
		QCOMPARE(list.outputAt(0)->data().value<double>(), 2.0);
	}

	/// The pool thread held the mutex of the pool for the whole processing, so
	/// every bounded wait spun in try_lock_for until the processing was over:
	/// the calling thread, most often the one of the interface, burnt a core for
	/// as long as the work lasted. The wait now sleeps on the condition, which
	/// shows as processor time far below the elapsed time.
	void waitingForAProcessingDoesNotBurnTheProcessor()
	{
		SlowProcessing proc;
		proc.setComputeTimeStatistics(false);
		proc.setScheduleStrategy(VipProcessingObject::Asynchronous, true);
		proc.inputAt(0)->setData(makeData(1.0));
		QVERIFY(proc.update());

		const qint64 processorBefore = processorMilliseconds();
		QElapsedTimer timer;
		timer.start();
		proc.wait(false, 200);
		const qint64 elapsed = timer.elapsed();
		const qint64 processor = processorMilliseconds() - processorBefore;

		QVERIFY2(elapsed >= 150, qPrintable(QString("the wait returned after %1 ms, it did not wait").arg(elapsed)));
		QVERIFY2(processor * 2 < elapsed, qPrintable(QString("%1 ms of processor time for %2 ms of wait").arg(processor).arg(elapsed)));

		QVERIFY(proc.wait(false, 30000));
	}

	/// update() took a spinlock and kept it across the wait for the result, so a
	/// second call from another thread span on it for the whole processing. The
	/// lock is now released before that wait, and what an update in flight is
	/// answered by a counter rather than by the state of the lock.
	void aSecondUpdateDoesNotSpinOnTheFirst()
	{
		SlowProcessing proc;
		proc.setComputeTimeStatistics(false);
		proc.inputAt(0)->setData(makeData(1.0));

		std::atomic<bool> updatingSeen{ false };
		QThread* first = QThread::create([&]() {
			proc.update(true);
		});
		first->start();

		// Let the first call reach its wait.
		QThread::msleep(100);
		updatingSeen = proc.isUpdating();

		const qint64 processorBefore = processorMilliseconds();
		QElapsedTimer timer;
		timer.start();
		proc.update(true);
		const qint64 elapsed = timer.elapsed();
		const qint64 processor = processorMilliseconds() - processorBefore;

		QVERIFY(first->wait(30000));
		delete first;

		QVERIFY2(updatingSeen.load(), "an update waiting for its result is still an update in flight");
		QVERIFY2(elapsed >= 200, qPrintable(QString("the second update returned after %1 ms").arg(elapsed)));
		QVERIFY2(processor * 4 < elapsed, qPrintable(QString("%1 ms of processor time for %2 ms of update").arg(processor).arg(elapsed)));
	}

	/// A run that finds no new input asks the pool to drop what is scheduled.
	/// Asked for from a thread that is not the one of the pool, the request
	/// simply stayed armed, and the pool then threw away the first batch it woke
	/// up for: real work, pushed after the skip, silently lost.
	void skippingForLackOfInputDoesNotLoseTheNextTask()
	{
		MultiplyByProperty proc;
		proc.setComputeTimeStatistics(false);

		// Asynchronous first, so the pool exists and runs one real input.
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::SkipIfNoInput | VipProcessingObject::Asynchronous);
		proc.inputAt(0)->setData(makeData(1.0));
		QVERIFY(proc.wait(true, 30000));
		QCOMPARE(proc.applyCount.load(), 1);

		// A forced run in the calling thread, with nothing new on the input: this
		// is the skip, and the pool is idle at that moment.
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::SkipIfNoInput | VipProcessingObject::NoThread);
		QVERIFY(proc.update(true));
		QCOMPARE(proc.applyCount.load(), 1);

		// Back to the pool, with real input.
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::SkipIfNoInput | VipProcessingObject::Asynchronous);
		proc.inputAt(0)->setData(makeData(3.0));
		QVERIFY(proc.wait(true, 30000));

		QCOMPARE(proc.applyCount.load(), 2);
		QCOMPARE(proc.outputAt(0)->data().value<double>(), 6.0);
	}

	/// Inserting a processing propagated the source properties of the list while
	/// holding the mutex of the list. That propagation is a virtual reimplemented
	/// outside the library, plugins included, and every other thread that only
	/// wanted the size of the list waited behind it.
	void insertingDoesNotHoldTheMutexWhileItCallsTheProcessing()
	{
		VipProcessingList list;
		list.setSourceProperty("test_property", QVariant(1));

		WatchingProcessing* proc = new WatchingProcessing();
		bool readInTime = false;
		QThread* reader = nullptr;
		proc->onSourceProperty = [&]() {
			if (reader)
				return;
			reader = QThread::create([&]() { list.size(); });
			reader->start();
			// Answered here, while the insert is still on the stack: joined after
			// it returns, the read always gets through in the end.
			readInTime = reader->wait(1000);
		};

		QVERIFY(list.insert(0, proc));

		QVERIFY(reader);
		QVERIFY(reader->wait(30000));
		delete reader;

		QVERIFY2(readInTime, "the list must be readable while it configures a processing");
	}

	/// The error was handed out by reference and deleted by the next setError()
	/// or resetError(), on a class whose documentation invites any thread to read
	/// it and which resets before every run. The reader now holds a share of what
	/// it reads. Meant to be run under a sanitiser, where the old code reports a
	/// read of freed memory.
	void readingTheErrorWhileItIsReplacedIsSafe()
	{
		MultiplyByProperty proc;
		proc.setLogErrors(QSet<int>());

		std::atomic<bool> stop{ false };
		std::atomic<int> seen{ 0 };

		QThread* reader = QThread::create([&]() {
			while (!stop.load()) {
				const VipErrorData err = proc.errorData();
				if (!err.errorString().isEmpty())
					++seen;
				proc.errorString();
				proc.errorCode();
				proc.hasError();
			}
		});
		reader->start();

		QElapsedTimer started;
		started.start();
		while (seen.load() == 0 && started.elapsed() < 30000) {
			proc.setError("replaced", -2);
			proc.resetError();
		}
		for (int i = 0; i < 5000; ++i) {
			proc.setError("replaced", -2);
			proc.resetError();
		}

		stop.store(true);
		QVERIFY(reader->wait(30000));
		delete reader;

		QVERIFY2(seen.load() > 0, "the reader must have seen an error");
		QVERIFY(!proc.hasError());
	}

	/// Asking which processings accept a list of inputs resized the multi input
	/// of every candidate prototype to the size of that list. The prototypes are
	/// shared for the life of the process, so the answer depended on the previous
	/// question, and two questions at once wrote the same container.
	void queryingTheProcessingsLeavesThePrototypesAlone()
	{
		const QList<const VipProcessingObject*> all = VipProcessingObject::allObjects();

		QMap<const VipProcessingObject*, int> before;
		for (const VipProcessingObject* obj : all)
			if (obj->topLevelInputCount() > 0)
				if (VipMultiInput* multi = obj->topLevelInputAt(0)->toMultiInput())
					if (multi->minSize() <= 3 && multi->maxSize() >= 3)
						before[obj] = multi->count();

		QVERIFY2(!before.isEmpty(), "the library must register at least one multi input processing");

		QVariantList three;
		three << QVariant(1.0) << QVariant(2.0) << QVariant(3.0);
		VipProcessingObject::validProcessingObjects<VipProcessingObject*>(three);

		for (QMap<const VipProcessingObject*, int>::const_iterator it = before.begin(); it != before.end(); ++it)
			QCOMPARE(it.key()->topLevelInputAt(0)->toMultiInput()->count(), it.value());
	}

	/// The snapshot is documented as something one side fills in real time while
	/// the other displays it, and its accessors are part of a family the base
	/// class declares thread safe. They read maps and a structure the loader
	/// writes, and neither side took a lock. Meant to be run under a sanitiser.
	void readingASnapshotWhileItIsLoadedIsSafe()
	{
		VipProcessingPool source;
		MultiplyByProperty* origin = new MultiplyByProperty(&source);
		origin->setObjectName("multiply");

		const QByteArray snapshot = vipSaveBinarySnapshot(&source);
		QVERIFY(!snapshot.isEmpty());

		VipProcessingPool target;
		QVERIFY(vipLoadBinarySnapshot(&target, snapshot));

		const QList<VipProcessingObject*> loaded = target.findChildren<VipProcessingObject*>();
		QVERIFY2(!loaded.isEmpty(), "the snapshot must have created a processing");

		std::atomic<bool> stop{ false };
		std::atomic<int> reads{ 0 };
		QThread* reader = QThread::create([&]() {
			while (!stop.load()) {
				for (VipProcessingObject* obj : loaded) {
					obj->inputDescription("input");
					obj->outputDescription("output");
					obj->info();
					obj->processingTime();
					++reads;
				}
			}
		});
		reader->start();

		QElapsedTimer started;
		started.start();
		while (reads.load() == 0 && started.elapsed() < 30000)
			QThread::msleep(1);

		for (int i = 0; i < 300; ++i)
			QVERIFY(vipLoadBinarySnapshot(&target, snapshot));

		stop.store(true);
		QVERIFY(reader->wait(30000));
		delete reader;

		QVERIFY2(reads.load() > 0, "the reader must have run");
	}

	/// A connection walks its peers by index every time a datum goes out, while
	/// connecting and disconnecting empties and reallocates that same vector.
	/// Nothing stood between the two. Meant to be run under a sanitiser, where
	/// the old code reads past the end or into freed memory.
	void sendingWhileTheGraphIsRewiredIsSafe()
	{
		MultiplyByProperty source;
		MultiplyByProperty first;
		MultiplyByProperty second;
		source.setComputeTimeStatistics(false);

		QVERIFY(source.outputAt(0)->setConnection(first.inputAt(0)));

		std::atomic<bool> stop{ false };
		std::atomic<int> sends{ 0 };
		QThread* sender = QThread::create([&]() {
			while (!stop.load()) {
				source.outputAt(0)->setData(makeData(1.0));
				++sends;
			}
		});
		sender->start();

		QElapsedTimer started;
		started.start();
		while (sends.load() == 0 && started.elapsed() < 30000)
			QThread::msleep(1);

		for (int i = 0; i < 2000; ++i)
			source.outputAt(0)->setConnection((i % 2) ? first.inputAt(0) : second.inputAt(0));

		stop.store(true);
		QVERIFY(sender->wait(30000));
		delete sender;

		QVERIFY2(sends.load() > 0, "the sender must have run");
	}

	/// The transform reported for an image is composed from the one the
	/// processing declares, recentred on the middle of the image before and
	/// after. Nothing measured that composition, and the inversion it needs was
	/// called without ever consulting the flag that says whether it succeeded: a
	/// matrix that cannot be inverted gives the identity back, and the result was
	/// then a transform that describes nothing.
	void theComposedImageTransformMatchesItsReference()
	{
		TransformingProcessing proc;
		proc.setComputeTimeStatistics(false);
		proc.setScheduleStrategies(VipProcessingObject::OneInput | VipProcessingObject::NoThread);

		VipNDArrayType<double> image(vipVector(4, 6));
		image.fill(1.);

		proc.transform = QTransform().rotate(90) * QTransform().scale(2, 2);
		proc.inputAt(0)->setData(VipAnyData(QVariant::fromValue(VipNDArray(image)), 0));
		QVERIFY(proc.update(true));

		// Composed by hand from the same definition: centre on the input, apply,
		// then translate back by the centre of the output taken through the
		// inverse.
		QTransform expected;
		const QTransform inv = proc.transform.inverted();
		const QPointF back = inv.map(QPointF(6 / 2., 4 / 2.));
		expected.translate(-6 / 2., -4 / 2.);
		expected *= proc.transform;
		expected.translate(back.x(), back.y());

		const QTransform got = proc.imageTransform();
		const double tolerance = 1e-9;
		QVERIFY2(std::abs(got.m11() - expected.m11()) < tolerance, "m11");
		QVERIFY2(std::abs(got.m12() - expected.m12()) < tolerance, "m12");
		QVERIFY2(std::abs(got.m21() - expected.m21()) < tolerance, "m21");
		QVERIFY2(std::abs(got.m22() - expected.m22()) < tolerance, "m22");
		QVERIFY2(std::abs(got.dx() - expected.dx()) < tolerance, "dx");
		QVERIFY2(std::abs(got.dy() - expected.dy()) < tolerance, "dy");

		// A matrix that cannot be inverted reports no transform rather than one
		// built from an inversion that did not happen.
		proc.transform = QTransform(0, 0, 0, 0, 0, 0);
		QVERIFY(proc.update(true));
		QVERIFY2(proc.imageTransform().isIdentity(), "a transform that cannot be inverted must report nothing");
	}

	/// The block of a session file sets the queue limits of every input of the
	/// process and the priority of every processing thread. The five values were
	/// applied as they stood: a limit type outside the enumeration switched
	/// eviction off, so each queue grew without bound; a size of zero emptied
	/// every queue on each push; and a priority outside the enumeration reached
	/// QThread::setPriority.
	void aSessionCannotImposeLimitsTheProgramDoesNotDefine()
	{
		const int limitType = VipProcessingManager::listLimitType();
		const int maxSize = VipProcessingManager::maxListSize();
		const qint64 maxMemory = VipProcessingManager::maxListMemory();
		const int threads = vipIterateThreadCount();

		PriorityMap bogus;
		bogus["MultiplyByProperty"] = (QThread::Priority)12345;

		VipXOStringArchive out;
		QVERIFY(out.start("VipProcessingManager"));
		out.content("arrayThreads", threads);
		out.content("listLimitType", 0x40);
		out.content("maxListSize", -5);
		out.content("maxListMemory", (qint64)-1);
		out.content("logErrors", VipProcessingManager::logErrors());
		out.content("priorities", QVariant::fromValue(bogus));
		out.end();

		VipXIStringArchive in(out.toString());
		vipRestoreSettings(in);

		QCOMPARE(VipProcessingManager::listLimitType(), limitType);
		QCOMPARE(VipProcessingManager::maxListSize(), maxSize);
		QCOMPARE(VipProcessingManager::maxListMemory(), maxMemory);
		QCOMPARE((int)VipProcessingManager::defaultPriority(&MultiplyByProperty::staticMetaObject), (int)QThread::InheritPriority);

		vipSetIterateThreadCount(threads);
	}
};

VIP_TEST_MAIN(TestProcessingObject)
#include "TestProcessingObject.moc"
