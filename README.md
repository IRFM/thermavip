

<img src="docs/images/logo.png" width="615">

# Thermavip

*Thermavip* is an open source framework for multi-sensor data acquisition, processing and visualization.

This software aims to gather within the same framework libraries for:
-	Firm real-time sensor acquisition and multi-screen display. Thermavip is optimized to display in parallel several video streams or curves/histograms/..., as well as applying complex firm real-time processing pipelines.
-	Offline visualization and post processing of multi-sensor data with a strong emphasis on video ones.
-	Video annotation and thermal event database management to train deep-learning models, like Region Based Convolutional Neural Network, for automatic detection and classification of thermal events based on IR movies. 

This software platform is widely used on the  [WEST](https://irfm.cea.fr/en/west/) tokamak for offline study of all WEST diagnostics data using the Librir open-source library, but also for online display of IR videos and temperature time traces for critical Plasma Facing Components (PFC). 
Thermavip platform is also used on the EAST tokamak (ASIPP/China) for IR video post-processing, and on the W7-X Stellarator (IPP/Germany) for the IR diagnostic acquisition (during OP1.0), PFC monitoring, online display and offline video analysis.

The Thermavip framework is composed of a C++ **S**oftware **D**evelopment **K**it (SDK) based on the [Qt](https://www.qt.io/) library and providing high-level classes for offline/real-time analysis and visualization of multi-sensor data. Its strength comes from its unique component block architecture, allowing to build multicore and distributed processing pipelines for both offline and real-time applications.

The framework can be used as a foreign library for external application, or as an independent software relying on third party plugins.

Below screenshot shows an example of software built based on Thermavip SDK for the post analysis of [WEST](https://irfm.cea.fr/en/west/) sensor data:

![Thermavip](docs/images/thermavip.png)

Thermavip is based on a [versatile software architecture](docs/architecture.md) composed of a C++  SDK and a plugin mechanism based on [Qt](https://www.qt.io/) only. Currently, the SDK is composed of 6 libraries:
-	Generic libraries that can be used outside *Thermavip* application, like any third party library:
	-	[Logging](docs/logging.md): logging to file/console/GUI tools
	-	[DataType](docs/datatype.md): base data types manipulated by Thermavip (N-D arrays, vector of points, scene models...)
	-	[Core](docs/core.md): asynchronous agents library based on dataflow, archiving, plugin mechanism
	-	[Plotting](docs/plotting.md): high performance plotting library for offline/firm real-time display of multi-sensor data
-	Libraries strongly connected to the *Thermavip* application:
	-	[Gui](docs/gui.md): base graphical components (main window, players...)
	-	[Annotation](docs/annotation.md): graphical components used to annotate IR videos, upload annotations to JSON files or to a MySQL/SQLite DB, query and display annotations from a DB.


The basic *Thermavip* application, without plugins, provides at least the necessary features dedicated to video annotation.
Note that this requires to build *Thermavip* with the [librir](https://github.com/IRFM/librir) library (see [build](docs/compilation.md) notes).

Thermavip framework is meant to build desktop applications at it relies on [Qt Widgets](https://doc.qt.io/qt-6/qtwidgets-index.html) and do not use Qt Quick.

## Prerequisites

To compile and run *Thermavip*, you need a valid Qt installation (starting version 5.9).
Qt needs to be compiled with the desktop opengl option, and with mysql support if you wish to use the [Annotation](docs/annotation.md) library with a MySQL database.

You can download Qt source code from this [website](https://download.qt.io/archive/qt/).

Note that *Thermavip* will compile and run on almost any platform supporting Qt, including Windows and all Linux distributions.

Default plugins shipped within the git reprository rely on the [HDF5](https://www.hdfgroup.org/solutions/hdf5/), [CPython](https://github.com/python/cpython) and [Ffmpeg](https://ffmpeg.org/) libraries, and are NOT compiled by default.

## Compilation

*Thermavip* compilation relies on cmake. See this [page](docs/compilation.md) for more details.

## Get started

Check one of the numerous [examples](src/Examples) to get started.
You can also check the [gallery](docs/gallery.md) to see the plotting capabilities.

If you just wish to use the plotting capabilities of Thermavip, the following example shows how to stream 100 curves of 10000 points each in the same window:

```cpp
#include <cmath>
#include <iostream>

#include <qapplication.h>
#include <qthread.h>
#include <qdir.h>
#include <qsurfaceformat.h>

#include "VipPlotWidget2D.h"
#include "VipColorMap.h"
#include "VipPlotShape.h"
#include "VipPlotHistogram.h"
#include "VipPlotSpectrogram.h"
#include "VipToolTip.h"
#include "VipPlotCurve.h"
#include "VipColorMap.h"
#include "VipSliderGrip.h"
#include "VipAxisColorMap.h"

struct Curve
{
	VipPlotCurve* curve;
	double factor;
};

/// @brief Generate a cosinus curve of at most 500 points with X values being in seconds
class CurveStreaming : public QThread
{
	QList<Curve> curves;
	bool stop;

public:
	CurveStreaming(const QList<Curve>& cs)
	  : curves(cs)
	  , stop(false)
	{
	}
	~CurveStreaming() { stopThread(); }
	void stopThread()
	{
		stop = true;
		this->wait();
	}
	virtual void run()
	{
		while (!stop) {
			// Update all curves with a noisy signal in a dedicated thread
			for (int i = 0; i < curves.size(); ++i) {
				VipPointVector vec(10000);
				double f = curves[i].factor;
				for (int j = 0; j < vec.size(); ++j)
					vec[j] = VipPoint(j, f + (rand() % 16) - 7);
				curves[i].curve->setRawData(vec);
			}

			QThread::msleep(1);
		}
	}
};

void setup_plot_area(VipPlotArea2D* area)
{
	// show title
	area->titleAxis()->setVisible(true);
	area->titleAxis()->setTitle("<b>Stream 100 curves of 10 000 points each");
	// allow wheel zoom
	area->setMouseWheelZoom(true);
	// allow mouse panning
	area->setMousePanning(Qt::RightButton);

	// hide right and top axes
	area->rightAxis()->setVisible(false);
	area->topAxis()->setVisible(false);

	// bottom axis intersect left one at 0
	area->bottomAxis()->setAxisIntersection(area->leftAxis(), 0);

	// add margins to the plotting area
	area->setMargins(10);
	// Use high update frame rate if possible
	area->setMaximumFrameRate(100);
}

int main(int argc, char** argv)
{
	// Setup opengl settings
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
	QSurfaceFormat format;
	format.setSamples(10);
	format.setSwapInterval(0);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication app(argc, argv);
	VipPlotWidget2D w;
	// Enable threaded OpenGL rendering if necessary
	//w.setRenderingMode(VipPlotWidget2D::OpenGLThread);

	// setup plotting area
	setup_plot_area(w.area());

	VipColorPalette p(VipLinearColorMap::ColorPaletteRandom);
	QList<Curve> curves;

	// Create 100 curves of 10000 points each)
	for (int i = 0; i < 100; ++i) {
		VipPlotCurve* c = new VipPlotCurve();
		c->setPen(QPen(p.color(i)));
		c->setAxes(w.area()->bottomAxis(), w.area()->leftAxis(), VipCoordinateSystem::Cartesian);
		curves.push_back(Curve{ c, (double)i * 16 * (i % 2 ? 1 : -1) });
	}

	w.resize(1000, 500);
	w.show();

	// Start streaming curves (100 curves of
	CurveStreaming thread(curves);
	thread.start();

	int ret = app.exec();
	thread.stopThread();
	return ret;
}
```

And the corresponding cmake file:

```cmake
cmake_minimum_required(VERSION 3.16)
project(CurveStreaming2 VERSION 1.0 LANGUAGES C CXX)

find_package(thermavip)

add_executable(CurveStreaming2 main.cpp)
target_link_libraries(CurveStreaming2 PRIVATE VipLogging VipDataType VipCore VipPlotting ${THERMAVIP_QT_LIBRARIES_NO_GUI} )
```

## Authors

* [Victor MONCADA](mailto:victor.moncada@cea.fr) (victor.moncada@cea.fr)
* [Léo DUBUS](mailto:leo.dubus@cea.fr) (leo.dubus@cea.fr)
* [Erwan GRELIER](mailto:erwan.grelier@cea.fr) (erwan.grelier@cea.fr)

The software has been built with the collaboration and intensive testing of:

* The [CEA/IRFM](http://irfm.cea.fr/en/index.php) which develops and maintains Thermavip,
* The Greifswald branch of [IPP](https://www.ipp.mpg.de/w7x) for the Wendelstein 7-X Stellarator,
* The [ITER-CODAC](https://www.iter.org/mach/Codac) team.

## Related publications

-	V. Moncada et Al., *«Software platform for imaging diagnostic exploitation applied to edge plasma physics and real-time PFC monitoring»*, Fusion Engineering and Design, Volume 190, 113528, ISSN 0920-3796, 2023
-	E. Grelier et Al., *«Deep Learning-Based Process for the Automatic Detection, Tracking, and Classification of Thermal Events on the In-Vessel Components of Fusion Reactors,»* Proceedings of the 32nd Symposium on Fusion Technology, 2022.
-	E. Grelier et Al., *«Deep Learning and Image Processing for the Automated Analysis of Thermal Events on the First Wall and Divertor of Fusion Reactors,»* Plasma Physics and Controlled Fusion (2022). 
-	H. Roche et Al., *«Commissioning of the HADES - High heAt loaD tESting - facility at CEA-IRFM,»* Proceedings of the 32nd Symposium on Fusion Technology, 2022.
-	Y. Corre et Al., *«Thermographic reconstruction of heat load on the first wall of Wendelstein 7-X due to ECRH shine-through power,»* Nuclear Fusion, vol. 61, n° %1066002, 2021. 
-	A. Puig Sitjes et Al., *«Wendelstein 7-X Near Real-Time Image Diagnostic System for Plasma-Facing Components Protection,»* Fusion Science and Technology, vol. 74(10), pp. 1-9, 2017. 
-	M. Jakubowski et Al., *«Infrared imaging systems for wall protection in the W7-X stellarator (invited),»* Review of Scientific Instruments, vol. 89, n° %110E116, 2018.


## Acknowledgments

Thermavip SDK and plugins are shipped with raw/modified versions of a few libraries:

* [ska_sort](https://github.com/skarupke/ska_sort) is used for fast sorting of numerical values
* The *Plotting* library of Thermavip SDK is a heavily modified version of [qwt](https://qwt.sourceforge.io/) library.
* The *Image Warping* processing uses a modified version of this [Delaunay](https://github.com/paulzfm/MSTSolver/tree/master/delaunay) library written by Ken Clarkson.


Thermavip framework and this page Copyright (c) 2025, CEA/IRFM