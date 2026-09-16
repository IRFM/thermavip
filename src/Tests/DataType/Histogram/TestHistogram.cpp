/// @file TestHistogram.cpp
///
/// Characterisation tests for the histogram extraction, exercised on the data
/// a thermal image actually carries: invalid pixels, many of them.

#include <QTest>

#include "vip_test_main.h"

#include "VipHistogram.h"
#include "VipNDArray.h"

#include <cmath>
#include <limits>
#include <vector>

class TestHistogram : public QObject
{
	Q_OBJECT

	static VipNDArray image(int size, int nan_count, double base)
	{
		VipNDArrayType<double> ar(vipVector(size));
		for (int i = 0; i < size; ++i)
			ar(vipVector(i)) = i < nan_count ? std::numeric_limits<double>::quiet_NaN() : base + i;
		return VipNDArray(ar);
	}

private Q_SLOTS:

	/// The bin index of an invalid pixel is the one the caller asked for. The
	/// block writing it tested the pair rather than its value, so it resolved to
	/// a catch-all returning false and never ran: the parameter had no effect.
	void anInvalidPixelGetsTheRequestedIndex()
	{
		VipNDArray ar = image(16, 4, 100.0);
		std::vector<int> indexes(16, -12345);
		VipIntervalSampleVector out;

		QVERIFY(vipExtractHistogram(ar, out, -1, Vip::SameBinWidth, VipInterval(), indexes.data(), 0, -1, -1, 77));

		for (int i = 0; i < 4; ++i)
			QCOMPARE(indexes[i], 77);
		for (int i = 4; i < 16; ++i)
			QVERIFY2(indexes[i] >= 0, "a valid pixel must land in a bin");
	}

	/// The working buffer is thread_local and survives from one call to the next.
	/// The positions of the invalid pixels were left untouched, so they still
	/// held the value and the index of the previous image: values from one frame
	/// were counted in the histogram of the next, and its indexes were written
	/// over the wrong pixels.
	void aPreviousImageDoesNotLeakIntoTheNextOne()
	{
		// First call, no invalid pixel, values far from the second image.
		{
			VipNDArray first = image(16, 0, 1000.0);
			std::vector<int> indexes(16, -1);
			VipIntervalSampleVector out;
			QVERIFY(vipExtractHistogram(first, out, -1, Vip::SameBinWidth, VipInterval(), indexes.data(), 0, -1, -1, -1));
		}

		VipNDArray second = image(16, 8, 0.0);
		std::vector<int> indexes(16, -1);
		VipIntervalSampleVector out;
		QVERIFY(vipExtractHistogram(second, out, -1, Vip::SameBinWidth, VipInterval(), indexes.data(), 0, -1, -1, 5));

		for (const VipIntervalSample& sample : out) {
			QVERIFY2(sample.interval.minValue() < 100, qPrintable(QString("a value of the previous image is counted: %1").arg(sample.interval.minValue())));
		}

		for (int i = 0; i < 8; ++i)
			QCOMPARE(indexes[i], 5);
	}

	/// Two invalid pixels compared less than each other in both directions, which
	/// is not the ordering std::sort requires: the partition may then run off the
	/// array. This exercises the sort on an image that is mostly invalid.
	void manyInvalidPixelsDoNotBreakTheSort()
	{
		VipNDArray ar = image(4096, 3000, 0.0);
		std::vector<int> indexes(4096, -1);
		VipIntervalSampleVector out;

		QVERIFY(vipExtractHistogram(ar, out, -1, Vip::SameBinWidth, VipInterval(), indexes.data(), 0, -1, -1, 3));

		for (int i = 0; i < 3000; ++i)
			QCOMPARE(indexes[i], 3);
		for (int i = 3000; i < 4096; ++i)
			QVERIFY(indexes[i] >= 0 && indexes[i] < 4096);
	}

	/// An image that carries nothing but invalid pixels produces no histogram,
	/// and every pixel takes the requested index.
	void anEntirelyInvalidImageProducesNoBin()
	{
		VipNDArray ar = image(32, 32, 0.0);
		std::vector<int> indexes(32, -1);
		VipIntervalSampleVector out;

		vipExtractHistogram(ar, out, -1, Vip::SameBinWidth, VipInterval(), indexes.data(), 0, -1, -1, 9);

		QCOMPARE(out.size(), 0);
		for (int i = 0; i < 32; ++i)
			QCOMPARE(indexes[i], 9);
	}
};

VIP_TEST_MAIN(TestHistogram)
#include "TestHistogram.moc"
