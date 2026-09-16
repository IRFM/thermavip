/// @file TestResize.cpp
///
/// Characterisation tests for the resampling kernels. The sizes exercised here
/// are the degenerate ones: a single row, a single column, a single sample.

#include <QTest>

#include "vip_test_main.h"

#include "VipNDArray.h"
#include "VipResize.h"

class TestResize : public QObject
{
	Q_OBJECT

private Q_SLOTS:

	/// A destination of one element made the step +inf. The linear kernel started
	/// its counter one past its end bound and never met it again, writing past a
	/// buffer of one element; the nearest kernel converted +inf to an index.
	void aSingleDestinationElementIsNotAnEndlessLoop()
	{
		VipNDArrayType<double> src(vipVector(8));
		for (int i = 0; i < 8; ++i)
			src(vipVector(i)) = i;

		for (int inter : { (int)Vip::NoInterpolation, (int)Vip::LinearInterpolation, (int)Vip::CubicInterpolation }) {
			VipNDArrayType<double> dst(vipVector(1));
			dst(vipVector(0)) = -1;
			QVERIFY(vipResize(VipNDArray(src), dst, (Vip::InterpolationType)inter));
			QVERIFY2(dst(vipVector(0)) >= 0 && dst(vipVector(0)) <= 7, "the single sample must come from the source");
		}
	}

	/// And a source of one element: the cubic kernel computed its loop start from
	/// 1/0, so the second loop began at the lowest representable index and wrote
	/// far below the destination.
	void aSingleSourceElementIsNotAnEndlessLoop()
	{
		VipNDArrayType<double> src(vipVector(1));
		src(vipVector(0)) = 42;

		for (int inter : { (int)Vip::NoInterpolation, (int)Vip::LinearInterpolation, (int)Vip::CubicInterpolation }) {
			VipNDArrayType<double> dst(vipVector(6));
			QVERIFY(vipResize(VipNDArray(src), dst, (Vip::InterpolationType)inter));
			for (int i = 0; i < 6; ++i)
				QCOMPARE(dst(vipVector(i)), 42.0);
		}
	}

	/// The two dimensional fast paths built their row index as index times step,
	/// and the first one is 0 times +inf, that is NaN.
	void aSingleDestinationRowIsNotIndexedFromNaN()
	{
		VipNDArrayType<double> src(vipVector(4, 4));
		for (int y = 0; y < 4; ++y)
			for (int x = 0; x < 4; ++x)
				src(vipVector(y, x)) = y * 4 + x;

		for (int inter : { (int)Vip::NoInterpolation, (int)Vip::LinearInterpolation, (int)Vip::CubicInterpolation }) {
			VipNDArrayType<double> dst(vipVector(1, 4));
			QVERIFY(vipResize(VipNDArray(src), dst, (Vip::InterpolationType)inter));
			for (int x = 0; x < 4; ++x) {
				const double value = dst(vipVector(0, x));
				QVERIFY2(value >= 0 && value <= 15, "every sample must come from the source");
			}
		}
	}

	/// The cubic kernel overshoots the range of its samples on a sharp edge. The
	/// conversion to the destination type used to wrap, so the bright side of an
	/// edge came out dark.
	void theCubicOvershootIsClampedNotWrapped()
	{
		VipNDArrayType<unsigned char> src(vipVector(8));
		for (int i = 0; i < 8; ++i)
			src(vipVector(i)) = i < 4 ? 0 : 255;

		VipNDArrayType<unsigned char> dst(vipVector(64));
		QVERIFY(vipResize(VipNDArray(src), dst, Vip::CubicInterpolation));

		// A monotonic edge stays monotonic in appearance: the dark side must hold
		// no bright sample and the bright side no dark one.
		for (int i = 0; i < 24; ++i)
			QVERIFY2(dst(vipVector(i)) < 128, "a dark sample must not wrap to a bright one");
		for (int i = 40; i < 64; ++i)
			QVERIFY2(dst(vipVector(i)) > 128, "a bright sample must not wrap to a dark one");
	}
};

VIP_TEST_MAIN(TestResize)
#include "TestResize.moc"
