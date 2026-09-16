/// @file TestNDArray.cpp
///
/// Characterisation tests for the array handles: who owns the buffer, what a
/// failed allocation leaves behind, and what a shape read from a file may ask
/// for.

#include <QTest>

#include "vip_test_main.h"

#include "VipNDArray.h"
#include "VipMultiNDArray.h"
#include "VipNDRect.h"
#include "VipIterator.h"
#include "VipNDArrayOperations.h"
#include "VipResize.h"
#include "VipNDArrayStatistics.h"
#include "VipStack.h"
#include "VipConvolve.h"
#include <QTemporaryFile>
#include <QDir>
#include "VipEval.h"

#include <limits>
#include <vector>

class TestNDArray : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// A shape comes from the file, and it sizes the allocation on its own. The
	/// elements have not been read yet, so a shape asking for more bytes than the
	/// stream holds cannot be honoured.
	void aShapeLargerThanTheStreamIsRefused()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << (int)VipNDArrayHandle::Standard;
			out << (int)QMetaType::Double;
			out << vipVector(100000, 100000); // 80 GB, and eight bytes of stream
		}

		VipNDArray ar;
		QDataStream in(&buffer, QIODevice::ReadOnly);
		in >> ar;

		QVERIFY2(ar.isEmpty(), "an unreadable shape must not size an allocation");
		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
	}

	/// A shape whose product wraps would allocate less than the caller writes.
	void aShapeWhoseProductWrapsIsRefused()
	{
		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << (int)VipNDArrayHandle::Standard;
			out << (int)QMetaType::Double;
			out << vipVector((qsizetype)1 << 40, (qsizetype)1 << 40);
		}

		VipNDArray ar;
		QDataStream in(&buffer, QIODevice::ReadOnly);
		in >> ar;

		QVERIFY(ar.isEmpty());
		QCOMPARE(in.status(), QDataStream::ReadCorruptData);
	}

	/// A round trip of an ordinary array still works.
	void anOrdinaryArrayStillRoundTrips()
	{
		VipNDArrayType<double> source(vipVector(2, 3));
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 3; ++x)
				source(vipVector(y, x)) = y * 3 + x;

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << VipNDArray(source);
		}
		VipNDArray read;
		{
			QDataStream in(&buffer, QIODevice::ReadOnly);
			in >> read;
		}

		QCOMPARE(read.shape(), source.shape());
		VipNDArrayType<double> typed = read.toDouble();
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 3; ++x)
				QCOMPARE(typed(vipVector(y, x)), (double)(y * 3 + x));
	}

	/// An allocation that cannot be served leaves the handle as it was. It used
	/// to publish the new shape and size first, so the handle then described an
	/// array that does not exist.
	void aFailedReallocLeavesTheHandleIntact()
	{
		VipNDArrayType<double> ar(vipVector(4));
		for (int i = 0; i < 4; ++i)
			ar(vipVector(i)) = i;

		VipNDArrayHandle* handle = const_cast<VipNDArrayHandle*>(ar.handle());
		QVERIFY(!handle->realloc(vipVector((qsizetype)1 << 45))); // 256 TB

		QCOMPARE(handle->size, (qsizetype)4);
		QVERIFY(handle->opaque != nullptr);
		for (int i = 0; i < 4; ++i)
			QCOMPARE(ar(vipVector(i)), (double)i);
	}

	/// Two views over the same external buffer: destroying one used to clear the
	/// buffer of the handle they share, pulling it from under the other.
	void oneViewDoesNotEmptyAnother()
	{
		std::vector<double> owned(8, 3.5);

		VipNDArray first = VipNDArray::makeView(owned.data(), vipVector(8));
		{
			VipNDArray second = first;
			QCOMPARE(second.shape(), vipVector(8));
		}

		QVERIFY2(!first.isEmpty(), "the surviving view must still see the buffer");
		QCOMPARE(first.value(vipVector(0)).toDouble(), 3.5);

		// And the buffer belongs to the caller: it is still readable here.
		QCOMPARE(owned[0], 3.5);
	}

	/// The convolution kept its two coordinate buffers inside the functor, and
	/// the evaluation calls that functor from several threads at once: each one
	/// resized and wrote the same two objects and read what its neighbours had
	/// written. Nothing crashed, the numbers were simply wrong. Only the generic
	/// path uses those buffers, which is the one a shape of four dimensions takes,
	/// and the parallel loop only starts above a few thousand elements.
	void convolvingOnSeveralThreadsGivesWhatOneThreadGives()
	{
		VipNDArrayType<double> source(vipVector(8, 8, 8, 16));
		for (qsizetype i = 0; i < source.size(); ++i)
			source[i] = static_cast<double>(i % 17);

		VipNDArrayType<double> kernel(vipVector(3, 3, 3, 3));
		for (qsizetype i = 0; i < kernel.size(); ++i)
			kernel[i] = static_cast<double>(i % 5) + 1.;

		const int previous = vipIterateThreadCount();

		vipSetIterateThreadCount(1);
		VipNDArrayType<double> alone(source.shape());
		QVERIFY(vipEval(alone, vipConvolve<Vip::Nearest>(source, kernel)));

		vipSetIterateThreadCount(8);
		VipNDArrayType<double> together(source.shape());
		QVERIFY(vipEval(together, vipConvolve<Vip::Nearest>(source, kernel)));

		vipSetIterateThreadCount(previous);

		qsizetype different = 0;
		for (qsizetype i = 0; i < alone.size(); ++i)
			if (together[i] != alone[i])
				++different;

		QCOMPARE(different, (qsizetype)0);
	}

	/// The automatic detection of the file format read two qsizetype, sixteen
	/// bytes, for a header of two int, eight bytes: the pair it tested was never
	/// the pair that had been written, no handle matched, and every binary file
	/// was taken for text.
	void aBinaryArrayFileIsRecognisedAsBinary()
	{
		VipNDArrayType<double> source(vipVector(3, 4));
		for (qsizetype i = 0; i < source.size(); ++i)
			source[i] = static_cast<double>(i) + 0.5;

		QTemporaryFile file(QDir::tempPath() + "/vip_array_XXXXXX.dat");
		QVERIFY(file.open());
		const QString path = file.fileName();
		file.close();

		QVERIFY(VipNDArray(source).save(path.toLatin1().data(), VipNDArray::Binary));

		VipNDArray read;
		QVERIFY2(read.load(path.toLatin1().data()), "a binary array file must load without being told its format");
		QCOMPARE(read.shape(), vipVector(3, 4));

		VipNDArrayType<double> back = read.toDouble();
		QCOMPARE(back.size(), source.size());
		qsizetype different = 0;
		for (qsizetype i = 0; i < back.size(); ++i)
			if (back[i] != source[i])
				++different;
		QCOMPARE(different, (qsizetype)0);
	}

	/// Turning a double into text went through the number of digits a float
	/// guarantees, six, so nine significant digits were dropped on the path every
	/// value of the library takes to become text, and reading the text back did
	/// not give the value.
	void aDoubleTurnedIntoTextKeepsItsDigits()
	{
		VipNDArrayType<double> source(vipVector(1));
		source[0] = 3.14159265358979;

		VipNDArrayType<QString> text(vipVector(1));
		QVERIFY(VipNDArray(source).convert(text));

		bool ok = false;
		const double back = text[0].toDouble(&ok);
		QVERIFY2(ok, qPrintable(text[0]));
		QVERIFY2(back == source[0], qPrintable(text[0]));
	}

	/// long and unsigned long are declared arithmetic and convertible, and the
	/// histogram has branches for them, but no array handle was registered for
	/// either: an array of long could not be created at all, and it said nothing.
	void anArrayOfLongCanBeCreated()
	{
		VipNDArray longs(qMetaTypeId<long>(), vipVector(2, 3));
		QVERIFY2(!longs.isNull(), "an array of long must exist");
		QCOMPARE(longs.shape(), vipVector(2, 3));

		VipNDArray ulongs(qMetaTypeId<unsigned long>(), vipVector(4));
		QVERIFY2(!ulongs.isNull(), "an array of unsigned long must exist");
		QCOMPARE(ulongs.size(), (qsizetype)4);
	}

	/// The eleven converters from long double to an integer cast without a bound.
	/// A cast of a value outside the range of the destination is undefined, and
	/// these are reached from any QVariant conversion, so a number read from a
	/// file decided what happened.
	void aLongDoubleOutOfRangeIsClampedNotUndefined()
	{
		QVariant big = QVariant::fromValue((vip_long_double)1e30L);
		QVERIFY(big.canConvert<qint32>());
		QCOMPARE(big.value<qint32>(), std::numeric_limits<qint32>::max());

		QVariant lower = QVariant::fromValue((vip_long_double)-1e30L);
		QCOMPARE(lower.value<qint32>(), std::numeric_limits<qint32>::lowest());

		QVariant ordinary = QVariant::fromValue((vip_long_double)42.5L);
		QCOMPARE(ordinary.value<qint32>(), 42);
	}

	/// The axis of a stack comes from the caller and is used as an index into a
	/// shape held on the stack, twice to write. The only bound was a debug
	/// assertion, which is nothing in a release build.
	void stackingRefusesAnAxisOutOfRange()
	{
		VipNDArrayType<double> a(vipVector(2, 3));
		VipNDArrayType<double> b(vipVector(2, 3));

		QVERIFY(vipStack(VipNDArray(a), VipNDArray(b), 7).isEmpty());
		QVERIFY(vipStack(VipNDArray(a), VipNDArray(b), -1).isEmpty());

		VipNDArray dst(qMetaTypeId<double>(), vipVector(4, 3));
		QVERIFY(!vipStack(dst, VipNDArray(a), VipNDArray(b), 7));
		QVERIFY(!vipStack(dst, VipNDArray(a), VipNDArray(b), -1));

		// And the ordinary case still works.
		QVERIFY(vipStack(dst, VipNDArray(a), VipNDArray(b), 0));
		QCOMPARE(vipStack(VipNDArray(a), VipNDArray(b), 0).shape(), vipVector(4, 3));
	}

	/// The guard on the shapes compared the same two things twice, so the shape of
	/// the destination was never checked at all.
	void stackingRefusesADestinationOfTheWrongShape()
	{
		VipNDArrayType<double> a(vipVector(2, 3));
		VipNDArrayType<double> b(vipVector(2, 3));

		VipNDArray narrow(qMetaTypeId<double>(), vipVector(4, 2));
		QVERIFY2(!vipStack(narrow, VipNDArray(a), VipNDArray(b), 0), "a destination that is too narrow must be refused");
	}

	/// The cumulative product left its first factor out: the branch that handles
	/// the first element set the accumulator to the neutral element and returned,
	/// so that element never multiplied in.
	void theCumulativeProductKeepsItsFirstFactor()
	{
		VipNDArrayType<double> ar(vipVector(4));
		ar(vipVector(0)) = 2;
		ar(vipVector(1)) = 3;
		ar(vipVector(2)) = 5;
		ar(vipVector(3)) = 7;

		QCOMPARE(vipArrayCumMultiply<double>(ar), 210.0);

		VipNDArrayType<double> single(vipVector(1));
		single(vipVector(0)) = 4;
		QCOMPARE(vipArrayCumMultiply<double>(single), 4.0);
	}

	/// Replacing the whole table of a multi array left the pointer to the current
	/// one on a destroyed element, and the name kept beside it stopped the
	/// insertion loop from ever pointing it somewhere valid again.
	void replacingTheNamedArraysLeavesNoDanglingCurrent()
	{
		VipMultiNDArray multi;
		multi.addArray("first", VipNDArray(VipNDArrayType<double>(vipVector(2))));
		multi.setCurrentArray("first");
		QCOMPARE(multi.currentArrayName(), QString("first"));

		QMap<QString, VipNDArray> replacement;
		replacement.insert("second", VipNDArray(VipNDArrayType<double>(vipVector(3))));
		multi.setNamedArrays(replacement);

		QCOMPARE(multi.namedArrays().size(), 1);
		QVERIFY(multi.namedArrays().contains("second"));
		QCOMPARE(multi.currentArrayName(), QString("second"));
		QCOMPARE(multi.shape(), vipVector(3));
	}

	/// The comparator used to sort the candidate types said a type was under
	/// itself, which is not the ordering std::sort requires; a duplicate in the
	/// list is enough to reach it.
	void theTypeOrderingIsIrreflexive()
	{
		QList<int> types;
		for (int i = 0; i < 40; ++i) {
			types << qMetaTypeId<double>() << qMetaTypeId<int>() << qMetaTypeId<float>();
			types << qMetaTypeId<quint8>() << qMetaTypeId<qint64>();
		}

		const int higher = vipHigherArrayType(qMetaTypeId<int>(), types);
		QVERIFY(higher != 0);
	}

	/// The engine clips every region of interest by the bounds of the image with
	/// this. It used neither the start of one rectangle nor the end of the other,
	/// so a region overlapping an edge came back extended rather than clipped,
	/// and the engine then walked past the array.
	void intersectingTwoRectanglesClips()
	{
		struct Case
		{
			qsizetype a0, a1, b0, b1, r0, r1;
		};
		const Case cases[] = {
			{ 0, 10, 2, 5, 2, 5 },	 // contained
			{ 3, 8, 0, 12, 3, 8 },	 // containing
			{ 2, 5, 0, 10, 2, 5 },	 // overlapping on the left
			{ 0, 5, 2, 10, 2, 5 },	 // overlapping on the right
		};

		for (const Case& c : cases) {
			VipNDRect<Vip::None> first(vipVector(c.a0), vipVector(c.a1));
			VipNDRect<Vip::None> second(vipVector(c.b0), vipVector(c.b1));

			const VipNDRect<Vip::None> result = first & second;
			const QString where = QString("[%1,%2) & [%3,%4)").arg(c.a0).arg(c.a1).arg(c.b0).arg(c.b1);
			QVERIFY2(result.start(0) == c.r0 && result.end(0) == c.r1, qPrintable(where));

			// And it is symmetric.
			const VipNDRect<Vip::None> other = second & first;
			QVERIFY2(other.start(0) == c.r0 && other.end(0) == c.r1, qPrintable(where + " reversed"));
		}

		// Disjoint rectangles give nothing.
		QVERIFY((VipNDRect<Vip::None>(vipVector(0), vipVector(2)) & VipNDRect<Vip::None>(vipVector(5), vipVector(7))).isEmpty());
	}

	/// The two dimensional specialisation reads its bounds through the accessors
	/// of the rectangle it holds, not through a pointer to int over it.
	void theTwoDimensionalRectangleReadsItsOwnBounds()
	{
		VipNDRect<2> rect(vipVector(3, 5), vipVector(9, 11));

		QCOMPARE(rect.start(0), (qsizetype)3);
		QCOMPARE(rect.start(1), (qsizetype)5);
		QCOMPARE(rect.end(0), (qsizetype)9);
		QCOMPARE(rect.end(1), (qsizetype)11);
		QCOMPARE(rect.shape(0), (qsizetype)6);
		QCOMPARE(rect.shape(1), (qsizetype)6);
	}

	/// A shape built from a container of the wrong size used to be left entirely
	/// as it came off the stack, and the constructor is implicit.
	void aShapeBuiltFromTheWrongSizeIsStillInitialised()
	{
		QVector<qsizetype> two;
		two << 4 << 7;

		const VipCoordinate<4> coord(two);
		QCOMPARE(coord[0], (qsizetype)4);
		QCOMPARE(coord[1], (qsizetype)7);
		QCOMPARE(coord[2], (qsizetype)0);
		QCOMPARE(coord[3], (qsizetype)0);
	}

	/// Writing into an array goes through a const accessor that does not detach,
	/// and the evaluation is public: called directly, it used to write into a
	/// buffer another array still shares.
	void evaluatingIntoASharedArrayDetachesIt()
	{
		VipNDArrayType<double> source(vipVector(4));
		for (int i = 0; i < 4; ++i)
			source(vipVector(i)) = i;

		const VipNDArray shared(source); // shares the handle, and never detaches

		VipNDArrayType<double> other(vipVector(4));
		for (int i = 0; i < 4; ++i)
			other(vipVector(i)) = 100 + i;

		QVERIFY(vipEval(source, other));

		for (int i = 0; i < 4; ++i) {
			QCOMPARE(source(vipVector(i)), (double)(100 + i));
			QVERIFY2(shared.value(vipVector(i)).toDouble() == (double)i, "the array sharing the buffer must not have changed");
		}
	}

	/// The central conversion is a bare cast: a value outside the range of an
	/// integer destination is undefined there, and wraps in practice, so a bright
	/// pixel came out dark.
	void convertingOutOfRangeSaturates()
	{
		VipNDArrayType<double> source(vipVector(4));
		source(vipVector(0)) = -1000;
		source(vipVector(1)) = 0;
		source(vipVector(2)) = 300;
		source(vipVector(3)) = 1e30;

		VipNDArrayType<unsigned char> dst(vipVector(4));
		QVERIFY(vipEval(dst, vipCast<unsigned char>(source)));

		QCOMPARE((int)dst(vipVector(0)), 0);
		QCOMPARE((int)dst(vipVector(1)), 0);
		QCOMPARE((int)dst(vipVector(2)), 255);
		QCOMPARE((int)dst(vipVector(3)), 255);
	}

	/// A shape longer than the storage of the fixed size vector used to be copied
	/// in whole, past the member array.
	void aShapeLongerThanTheStorageIsClamped()
	{
		QVector<qsizetype> many;
		for (int i = 0; i < 32; ++i)
			many << i;

		VipNDArrayShape shape(many);
		QVERIFY(shape.size() <= VIP_MAX_DIMS);

		VipNDArrayShape resized;
		resized.resize(64);
		QVERIFY(resized.size() <= VIP_MAX_DIMS);
	}

	/// Serialising an array walks its elements through the same transform the
	/// arithmetic goes through, and that transform runs in parallel as soon as the
	/// iteration thread count is raised. Several threads writing into one stream
	/// race on it and, even without corrupting it, emit the elements in an
	/// arbitrary order that the reader cannot detect.
	void serialisingAnArrayKeepsItsOrderWithSeveralThreads()
	{
		const int previous = vipIterateThreadCount();
		vipSetIterateThreadCount(8);

		VipNDArrayType<double> source(vipVector(64, 64)); // over the parallel threshold
		for (int y = 0; y < 64; ++y)
			for (int x = 0; x < 64; ++x)
				source(vipVector(y, x)) = y * 64 + x;

		QByteArray buffer;
		{
			QDataStream out(&buffer, QIODevice::WriteOnly);
			out << VipNDArray(source);
		}
		VipNDArray read;
		{
			QDataStream in(&buffer, QIODevice::ReadOnly);
			in >> read;
		}

		vipSetIterateThreadCount(previous);

		QCOMPARE(read.shape(), source.shape());
		const VipNDArrayType<double> typed = read.toDouble();
		for (int y = 0; y < 64; ++y)
			for (int x = 0; x < 64; ++x)
				QCOMPARE(typed(vipVector(y, x)), (double)(y * 64 + x));
	}
};

VIP_TEST_MAIN(TestNDArray)
#include "TestNDArray.moc"
