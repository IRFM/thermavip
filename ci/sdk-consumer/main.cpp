/// @file main.cpp
///
/// Minimal exercise of each SDK layer reached through the installed package:
/// logging, data types, plotting. If one of the three stops linking, or one of
/// its headers stops being installed, the SDK integration job fails.
///
/// Deliberately not a QTest: this binary must depend on nothing but the
/// exported package, Qt included. Linking Qt::Test here would add a dependency
/// the package does not declare and hide the failure it exists to catch.

#include <QApplication>

#include "VipDataType.h"
#include "VipLogging.h"
#include "VipPlotCurve.h"

int main(int argc, char** argv)
{
	QApplication app(argc, argv);

	VIP_LOG_INFO("SDK consumer started");

	VipPointVector points;
	for (int i = 0; i < 100; ++i)
		points.append(VipPoint(i, i * i));

	VipPlotCurve curve;
	curve.setRawData(points);

	if (curve.rawData().size() != points.size()) {
		VIP_LOG_ERROR("the curve data does not match what was set");
		return 1;
	}

	VIP_LOG_INFO("SDK consumer finished");
	return 0;
}
