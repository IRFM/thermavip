/// @file TestSerialization.cpp
///
/// Characterisation tests for the binary deserialisation surface: sizes and
/// counts read from a stream, which is what a session or data file supplies.

#include <QTest>

#include "vip_test_main.h"

#include <QDir>
#include <QElapsedTimer>
#include <complex>
#include <type_traits>

#include "VipCircularVector.h"
#include "VipIterator.h"
#include "VipNDArray.h"
#include "VipHash.h"
#include "VipMath.h"
#include "VipSleep.h"
#include "VipLock.h"
#include "VipLongDouble.h"
#include "VipVectors.h"

class TestSerialization : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// A shape round trips through a data stream unchanged.
	void shapeRoundTrip()
	{
		const VipNDArrayShape source = vipVector(3, 5);

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << source;
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QCOMPARE(in.status(), QDataStream::Ok);
		QCOMPARE(read.size(), source.size());
		QCOMPARE(read[0], source[0]);
		QCOMPARE(read[1], source[1]);
	}

	/// A size read from the stream used to be applied as is. The storage is a
	/// fixed array of VIP_MAX_DIMS elements and the resize only asserts in debug
	/// builds, so a crafted stream wrote past the end of the object in release
	/// builds. The read must now refuse the size instead.
	void shapeWithOutOfRangeSizeIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qsizetype>(1000000);
			for (int i = 0; i < 16; ++i)
				out << static_cast<qsizetype>(1);
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY2(in.status() != QDataStream::Ok, "an out of range size must mark the stream corrupt");
		QVERIFY2(read.size() <= VIP_MAX_DIMS, "the shape must never hold more than its storage");
	}

	/// Same guard on the negative side: a signed size read from a file can be
	/// negative, and it used to be assigned unchecked.
	void shapeWithNegativeSizeIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qsizetype>(-1);
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY(in.status() != QDataStream::Ok);
		QVERIFY(read.size() >= 0);
	}

	/// A truncated stream must not be read as if it were complete: the size is
	/// announced but the elements are missing.
	void truncatedShapeStopsReading()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qsizetype>(3);
			out << static_cast<qsizetype>(7); // one element only, two are missing
		}

		VipNDArrayShape read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY2(in.status() != QDataStream::Ok, "a truncated stream must be reported");
	}

	/// A sample vector round trips through a data stream unchanged.
	void sampleVectorRoundTrip()
	{
		VipPointVector source;
		source.push_back(VipPoint(1.0, 2.0));
		source.push_back(VipPoint(3.0, 4.0));

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << source;
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QCOMPARE(in.status(), QDataStream::Ok);
		QCOMPARE(read.size(), source.size());
		QCOMPARE(read[1].x(), 3.0);
	}

	/// The element count was reserved before a single element had been read, so a
	/// crafted stream asked for an arbitrary allocation up front. A count larger
	/// than the bytes left in the stream cannot be honoured and must be refused.
	void sampleVectorWithImplausibleCountIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qint64>(200000000); // ~3 GB of points announced
			out << 1.0 << 2.0;
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY2(in.status() != QDataStream::Ok, "a count the stream cannot back must be refused");
		QVERIFY(read.isEmpty());
	}

	/// Same guard on the negative side: the count is signed and comes from the file.
	void sampleVectorWithNegativeCountIsRejected()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qint64>(-1);
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY(in.status() != QDataStream::Ok);
		QVERIFY(read.isEmpty());
	}

	/// A truncated stream must stop the read rather than fill the container.
	void truncatedSampleVectorStopsReading()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << static_cast<qint64>(3);
			out << 1.0 << 2.0; // one point only, two are missing
		}

		VipPointVector read;
		QDataStream in(buffer);
		in >> read;

		QVERIFY(in.status() != QDataStream::Ok);
		QVERIFY(read.isEmpty());
	}

	/// The width of an extended precision value is carried by a dynamic property
	/// on the device, so any code holding that device sets it, and it used to be
	/// passed as is to a raw read over a sixteen byte stack buffer. Anything wider
	/// than the buffer must be refused.
	void extendedPrecisionWidthIsBounded()
	{
		QByteArray payload(4096, '\xcc');
		QDataStream in(payload);

		const unsigned savedAsLongDouble = 1u << 31;
		const vip_double value = vipReadLEDouble(savedAsLongDouble | 1000u, in);

		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
		QCOMPARE(static_cast<double>(value), 0.0);
	}

	/// Same guard on the second reader, and on a width of zero.
	void extendedPrecisionZeroWidthIsRejected()
	{
		QByteArray payload(4096, '\xcc');
		QDataStream in(payload);

		const unsigned savedAsLongDouble = 1u << 31;
		vipReadLELongDouble(savedAsLongDouble | 0u, in);

		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
	}

	/// A width the buffer can hold, but the stream cannot supply, must be reported
	/// rather than read as if complete.
	void extendedPrecisionShortReadIsReported()
	{
		QByteArray payload(4, '\x01');
		QDataStream in(payload);

		const unsigned savedAsLongDouble = 1u << 31;
		vipReadLEDouble(savedAsLongDouble | 10u, in);

		QVERIFY(in.status() != QDataStream::Ok);
	}

	/// The size of an array is the product of its dimensions, computed in a signed
	/// integer. Four dimensions of 65536 reach 2^64: the product used to wrap, which
	/// is undefined behaviour and yielded a small or negative size that was then
	/// used to walk the array and to size allocations. Each dimension here is
	/// plausible on its own, which is what tells this apart from a bad dimension.
	void shapeSizeOverflowIsReported()
	{
		VipNDArrayShape shape = vipVector(65536, 65536, 65536, 65536);

		QCOMPARE(vipShapeToSize(shape), (qsizetype)-1);
	}

	/// A negative dimension is refused rather than multiplied.
	void negativeDimensionIsReported()
	{
		QCOMPARE(vipShapeToSize(vipVector(4, -1)), (qsizetype)-1);
	}

	/// A shape that fits still gives its size.
	void shapeSizeIsComputed()
	{
		QCOMPARE(vipShapeToSize(vipVector(3, 5, 7)), (qsizetype)105);
	}

	/// The stride computation multiplies the same dimensions and must report the
	/// same overflow.
	void defaultStridesReportOverflow()
	{
		VipNDArrayShape shape = vipVector(65536, 65536, 65536, 65536);
		VipNDArrayShape strides;

		QCOMPARE((vipComputeDefaultStrides<Vip::FirstMajor>(shape, strides)), (qsizetype)-1);
	}

	/// Reading an array from a file is a conversion no longer: with a default second
	/// argument these constructors converted, so any function taking a const
	/// VipNDArray& accepted a string literal and read the file it named, without a
	/// conversion appearing at the call site.
	void arrayIsNotConstructibleFromAStringImplicitly()
	{
		QVERIFY(!(std::is_convertible<const char*, VipNDArray>::value));
		QVERIFY(!(std::is_convertible<QIODevice*, VipNDArray>::value));
		QVERIFY((std::is_constructible<VipNDArray, const char*>::value));
	}

	/// A file that cannot be read leaves a null array rather than a half built one.
	void arrayFromMissingFileIsNull()
	{
		const QByteArray path = (QDir::tempPath() + QStringLiteral("/vip_no_such_file.bin")).toLatin1();
		const VipNDArray array(path.constData());

		QVERIFY(array.isNull());
	}

	/// Reading a bounded number of values from a text stream must leave the stream
	/// where those values end. The reader took the whole stream and then tried to
	/// put the position back with a value that is -1 whenever the loop stopped on a
	/// failed extraction, which is the normal case: the stream stayed at the end and
	/// everything after the values was silently unreadable.
	void textReaderLeavesTheRestReadable()
	{
		QString text = QStringLiteral("header 1.5 2.5 3.5 tail");
		QTextStream stream(&text, QIODevice::ReadOnly);

		// Start somewhere other than zero: the restored position is an absolute one,
		// and at zero a wrong one and a right one agree.
		QString header;
		stream >> header;
		QCOMPARE(header, QStringLiteral("header"));

		QVector<vip_long_double> values(3);
		QCOMPARE(vipReadNLongDouble(stream, values), 3);
		QCOMPARE(static_cast<double>(values[2]), 3.5);

		const QString rest = stream.readAll().trimmed();
		QCOMPARE(rest, QStringLiteral("tail"));
	}

	/// Same for the appending reader, which stops when the values run out.
	void appendingTextReaderLeavesTheRestReadable()
	{
		QString text = QStringLiteral("10 20 end of line");
		QTextStream stream(&text, QIODevice::ReadOnly);

		QVector<vip_long_double> values;
		QCOMPARE(vipReadNLongDoubleAppend(stream, values, 8), 2);
		QCOMPARE(values.size(), 2);

		const QString rest = stream.readAll().trimmed();
		QCOMPARE(rest, QStringLiteral("end of line"));
	}

	/// The iterator that walks an array while skipping one dimension computed its
	/// strides from the current position instead of from the shape. The constructor
	/// zeroes that position, so every stride but the last was zero and the first
	/// division of the first call divided by zero. Two dimensions hid it, the single
	/// stride being 1; three did not. This runs on the parallel path of resizing,
	/// once per thread.
	void skippingIteratorPlacesItselfOnAThreeDimensionalShape()
	{
		const VipNDArrayShape shape = vipVector(2, 3, 4);
		const qsizetype skip = 1;

		for (qsizetype flat = 0; flat < 2 * 4; ++flat) {
			detail::CIteratorFMajorSkipDim<VipNDArrayShape> it(shape, skip);
			it.setFlatPosition(flat);

			QCOMPARE(it.pos[skip], (qsizetype)0);
			QCOMPARE(it.pos[0] * 4 + it.pos[2], flat);
			QVERIFY(it.pos[0] < 2);
			QVERIFY(it.pos[2] < 4);
		}
	}

	/// The two dimensional case, which already worked, still does.
	void skippingIteratorPlacesItselfOnATwoDimensionalShape()
	{
		const VipNDArrayShape shape = vipVector(3, 5);

		for (qsizetype flat = 0; flat < 5; ++flat) {
			detail::CIteratorFMajorSkipDim<VipNDArrayShape> it(shape, 0);
			it.setFlatPosition(flat);

			QCOMPARE(it.pos[0], (qsizetype)0);
			QCOMPARE(it.pos[1], flat);
		}
	}

	// -- VipCircularVector ---------------------------------------------------

	/// Inserting at the last position called the front insertion, a copy of the
	/// line above it, so the element landed at the front instead of where it was
	/// asked for.
	void emplaceAtTheEndPutsTheElementAtTheEnd()
	{
		VipCircularVector<int> v;
		for (int i = 0; i < 4; ++i)
			v.push_back(i); // 0 1 2 3

		v.emplace(v.size() - 1, 99);

		QCOMPARE(v.size(), (qsizetype)5);
		QCOMPARE(v[0], 0);
		QCOMPARE(v[3], 99);
		QCOMPARE(v[4], 3);
	}

	/// Inserting at the size appends, and an index outside the container is
	/// refused rather than silently redirected.
	void emplaceBoundsAreEnforced()
	{
		VipCircularVector<int> v;
		v.push_back(1);
		v.push_back(2);

		v.emplace(v.size(), 3);
		QCOMPARE(v.back(), 3);

		bool threw = false;
		try {
			v.emplace(-1, 0);
		}
		catch (const std::out_of_range&) {
			threw = true;
		}
		QVERIFY2(threw, "a negative position must be refused");
	}

	/// size_type is signed, so testing only the upper bound let a negative index
	/// through to the masked indexing, which returns a reference to a slot that was
	/// never constructed.
	void atRejectsANegativeIndex()
	{
		VipCircularVector<int> v;
		v.push_back(7);

		bool threw = false;
		try {
			(void)v.at(-1);
		}
		catch (const std::out_of_range&) {
			threw = true;
		}
		QVERIFY2(threw, "a negative index must throw");
	}

	/// Appending a container to itself reads a range the growth invalidates.
	void insertingAContainerIntoItselfKeepsTheValues()
	{
		VipCircularVector<int> v;
		for (int i = 0; i < 4; ++i)
			v.push_back(i);

		v.insert(v.size(), v.begin(), v.end());

		QCOMPARE(v.size(), (qsizetype)8);
		for (int i = 0; i < 4; ++i) {
			QCOMPARE(v[i], i);
			QCOMPARE(v[i + 4], i);
		}
	}

	/// A text file is read through these, and the result of a failed parse used
	/// to be whatever the stack held: the variable was declared without an
	/// initialiser, and none of the callers passes the flag that would tell them.
	void aFailedLongDoubleParseReturnsZero()
	{
		bool ok = true;
		QCOMPARE((double)vipLongDoubleFromString(QStringLiteral("not a number"), &ok), 0.0);
		QVERIFY(!ok);

		ok = true;
		QCOMPARE((double)vipLongDoubleFromByteArray(QByteArray("not a number"), &ok), 0.0);
		QVERIFY(!ok);

		QCOMPARE((double)vipLongDoubleFromString(QString(), nullptr), 0.0);
		QCOMPARE((double)vipLongDoubleFromByteArray(QByteArray(), nullptr), 0.0);

		// And a number still parses.
		ok = false;
		QCOMPARE((double)vipLongDoubleFromString(QStringLiteral("2.5"), &ok), 2.5);
		QVERIFY(ok);
	}

	/// The same through the metatype converters, which have no error channel: a
	/// caller cannot tell a failure from a success there.
	void aFailedConversionThroughAVariantReturnsZero()
	{
		QCOMPARE((double)QVariant(QStringLiteral("not a number")).value<vip_long_double>(), 0.0);
		QCOMPARE((double)QVariant(QByteArray("not a number")).value<vip_long_double>(), 0.0);
	}

	/// A guard owns the lock it took. Copying one released the same lock twice,
	/// which corrupts the exclusion the lock exists for; the two lock classes of
	/// the same header already delete their copy.
	void aLockGuardCannotBeCopied()
	{
		QVERIFY(!std::is_copy_constructible<VipUniqueLock<VipSpinlock>>::value);
		QVERIFY(!std::is_copy_assignable<VipUniqueLock<VipSpinlock>>::value);
		QVERIFY(!std::is_copy_constructible<VipSharedLock<VipSharedSpinlock>>::value);
		QVERIFY(!std::is_copy_assignable<VipSharedLock<VipSharedSpinlock>>::value);
	}

	/// Hashing an arithmetic value copies its representation into an accumulator
	/// of eight bytes. A type wider than that wrote past it; here the extended
	/// floating point type is eight bytes, so this pins the sizes that exist on
	/// this platform.
	void hashingAnArithmeticValueIsStable()
	{
		QCOMPARE(vipHashValue((vip_long_double)1.5), vipHashValue((vip_long_double)1.5));
		QVERIFY(vipHashValue((vip_long_double)1.5) != vipHashValue((vip_long_double)2.5));
		QCOMPARE(vipHashValue(1.5), vipHashValue(1.5));
		QCOMPARE(vipHashValue((quint8)7), vipHashValue((quint8)7));
		QVERIFY(vipHashValue(1.5f) != vipHashValue(2.5f));
	}

	/// Assigning a strongly owning vector to itself freed its data before reading
	/// it. The shared ownership specialisation of the same pointer guards against
	/// it; this one did not.
	void aVectorSurvivesBeingAssignedToItself()
	{
		VipCircularVector<QString, Vip::StrongOwnership> vec;
		vec.push_back(QStringLiteral("first"));
		vec.push_back(QStringLiteral("second"));

		VipCircularVector<QString, Vip::StrongOwnership>& alias = vec;
		vec = alias;

		QCOMPARE(vec.size(), (qsizetype)2);
		QCOMPARE(vec[0], QStringLiteral("first"));
		QCOMPARE(vec[1], QStringLiteral("second"));
	}

	/// The modulus of a complex number is a magnitude, not a flag. The overload
	/// declared bool as its return type, and the modulus converts to bool without
	/// a word from the compiler: every non zero amplitude came out as one.
	void theModulusOfAComplexIsAMagnitude()
	{
		QCOMPARE(vipAbs(std::complex<double>(3.0, 4.0)), 5.0);
		QCOMPARE(vipAbs(std::complex<float>(3.f, 4.f)), 5.f);
		QVERIFY((std::is_same<decltype(vipAbs(std::complex<double>())), double>::value));

		QCOMPARE(vipFloor(std::complex<double>(1.7, -1.2)), std::complex<double>(1.0, -2.0));
		QCOMPARE(vipCeil(std::complex<double>(1.2, -1.7)), std::complex<double>(2.0, -1.0));
		QCOMPARE(vipRound(std::complex<double>(1.6, -1.6)), std::complex<double>(2.0, -2.0));
	}

	/// A duration reaches a conversion to an unsigned integer, undefined outside
	/// its range: a negative one used to sleep for over an hour on one platform
	/// and return at once on the other.
	void sleepingForANegativeDurationReturnsAtOnce()
	{
		QElapsedTimer timer;
		timer.start();
		vipSleep(-5);
		vipSleep(-1e12);
		vipSleep(vipNan());
		QVERIFY2(timer.elapsed() < 500, "a duration that is not positive must return at once");

		timer.restart();
		vipSleep(30);
		QVERIFY2(timer.elapsed() >= 10, "an ordinary duration must still wait");
	}

	/// A point vector is a circular buffer: its data() is private and points at
	/// the control structure it shares between copies, not at the samples. The
	/// public interface is the way to read and write one element at a time, which
	/// is what a device serialising it must use.
	void aPointVectorIsReadThroughItsInterface()
	{
		VipPointVector vec(3);
		for (qsizetype i = 0; i < vec.size(); ++i)
			vec[i] = VipPoint(i, i * 2.);

		const VipPointVector& readable = vec;
		for (qsizetype i = 0; i < readable.size(); ++i) {
			const VipPoint pt = readable[i];
			QCOMPARE(pt.x(), (double)i);
			QCOMPARE(pt.y(), i * 2.);
		}
	}
};

VIP_TEST_MAIN(TestSerialization)
#include "TestSerialization.moc"
