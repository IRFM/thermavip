/// @file TestComponents.cpp
///
/// Characterisation tests for the colour component extraction: what a class
/// publishes as the type of a component must be what it stores and what it
/// reads back when merging.

#include <QTest>

#include "vip_test_main.h"

#include "VipExtractComponents.h"
#include "VipNDArray.h"

static const int WIDTH = 16;
static const int HEIGHT = 8;

class TestComponents : public QObject
{
	Q_OBJECT

	template<class T>
	static VipNDArray plane(int first, int step)
	{
		VipNDArrayType<T> ar(vipVector(HEIGHT, WIDTH));
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
				ar(vipVector(y, x)) = static_cast<T>(first + ((y * WIDTH + x) * step));
		return VipNDArray(ar);
	}

	template<class T>
	static T at(const VipNDArray& ar, int y, int x)
	{
		return ar.value(vipVector(y, x)).value<T>();
	}

	static QImage merged(VipExtractComponents& extractor, const QList<VipNDArray>& components)
	{
		extractor.SetComponents(components);
		const VipNDArray out = extractor.MergeComponents();
		if (out.isEmpty())
			return QImage();
		return vipToImage(out);
	}

private Q_SLOTS:

	/// The control: this class always read its components at the width it stores
	/// them, and its round trip is exact.
	void argbSurvivesARoundTrip()
	{
		QImage original(WIDTH, HEIGHT, QImage::Format_ARGB32);
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
				original.setPixel(x, y, qRgba(x * 15, y * 30, 255 - x * 15, 255));

		VipExtractARGBComponents extractor;
		extractor.SeparateComponents(vipToArray(original));
		const QImage result = vipToImage(extractor.MergeComponents());

		QCOMPARE(result.size(), original.size());
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
				QCOMPARE(result.pixel(x, y), original.pixel(x, y));
	}

	/// Merging must read each component at the width the class publishes for it.
	/// Three of the four were read eight bytes at a time, from arrays of four and
	/// of one, so every pixel but the first few came from past the end.
	void hslMergesTheComponentsItWasGiven()
	{
		const QList<VipNDArray> components = QList<VipNDArray>() << plane<int>(0, 2) << plane<quint8>(40, 1) << plane<quint8>(60, 1) << plane<quint8>(255, 0);

		VipExtractHSLComponents extractor;
		const QImage result = merged(extractor, components);
		QCOMPARE(result.size(), QSize(WIDTH, HEIGHT));

		const QList<VipNDArray> stored = extractor.GetComponents();
		for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x) {
				const QColor expected = QColor::fromHsl(at<int>(stored[0], y, x), at<int>(stored[1], y, x), at<int>(stored[2], y, x), at<int>(stored[3], y, x));
				QCOMPARE(result.pixel(x, y), expected.rgba());
			}
		}
	}

	void hsvMergesTheComponentsItWasGiven()
	{
		const QList<VipNDArray> components = QList<VipNDArray>() << plane<int>(0, 2) << plane<quint8>(40, 1) << plane<quint8>(60, 1) << plane<quint8>(255, 0);

		VipExtractHSVComponents extractor;
		const QImage result = merged(extractor, components);
		QCOMPARE(result.size(), QSize(WIDTH, HEIGHT));

		const QList<VipNDArray> stored = extractor.GetComponents();
		for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x) {
				const QColor expected = QColor::fromHsv(at<int>(stored[0], y, x), at<int>(stored[1], y, x), at<int>(stored[2], y, x), at<int>(stored[3], y, x));
				QCOMPARE(result.pixel(x, y), expected.rgba());
			}
		}
	}

	/// This one read four bytes at a time from arrays of one.
	void cmykMergesTheComponentsItWasGiven()
	{
		const QList<VipNDArray> components = QList<VipNDArray>() << plane<quint8>(10, 1) << plane<quint8>(30, 1) << plane<quint8>(50, 1) << plane<quint8>(70, 1)
									<< plane<quint8>(255, 0);

		VipExtractCMYKComponents extractor;
		const QImage result = merged(extractor, components);
		QCOMPARE(result.size(), QSize(WIDTH, HEIGHT));

		const QList<VipNDArray> stored = extractor.GetComponents();
		for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x) {
				const QColor expected = QColor::fromCmyk(at<int>(stored[0], y, x),
									 at<int>(stored[1], y, x),
									 at<int>(stored[2], y, x),
									 at<int>(stored[3], y, x),
									 at<int>(stored[4], y, x));
				QCOMPARE(result.pixel(x, y), expected.rgba());
			}
		}
	}

	/// What a class publishes as the type of a component is what it stores.
	void theStoredComponentsHaveThePublishedTypes()
	{
		QImage original(WIDTH, HEIGHT, QImage::Format_ARGB32);
		original.fill(Qt::blue);

		VipExtractHSLComponents extractor;
		extractor.SeparateComponents(vipToArray(original));

		const QList<QByteArray> types = extractor.PixelComponentTypes();
		const QList<VipNDArray> components = extractor.GetComponents();
		QCOMPARE(components.size(), types.size());
		for (int i = 0; i < components.size(); ++i)
			QCOMPARE(QByteArray(QMetaType(components[i].dataType()).name()), types[i]);
	}

	/// The lightness of a colour is not its value: a saturated blue has a
	/// lightness of one half and a value of one. The extraction of the third HSL
	/// component calls the value function, so an HSL round trip turns every
	/// saturated colour white.
	void hslLightnessIsTheLightness()
	{
		QImage original(WIDTH, HEIGHT, QImage::Format_ARGB32);
		original.fill(Qt::blue);

		VipExtractHSLComponents extractor;
		extractor.SeparateComponents(vipToArray(original));
		const QImage result = vipToImage(extractor.MergeComponents());

		QEXPECT_FAIL("", "the third component is extracted as the value, not the lightness", Continue);
		QCOMPARE(result.pixel(0, 0), QColor(Qt::blue).rgba());
	}
};

VIP_TEST_MAIN(TestComponents)
#include "TestComponents.moc"
