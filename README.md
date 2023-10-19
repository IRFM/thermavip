
![Logo](docs/images/logo.png)

# Thermavip

[CEA-IRFM](http://irfm.cea.fr/en/index.php) has gained in-depth expertise at using imaging diagnostics to understand quick-ageing and damaging of materials under high thermal stresses. This knowledge has been used to developp the ThermaVIP (*Viewing Imaging Platform*) software platform, initially designed for the exploitation of infrared thermography diagnostics in fusion tokamaks. This software platform, which required 8 years of development, is made up of a set of modules allowing the exploitation of imaging diagnoses in a complex measuring environments.

Thermavip is mainly dedicated to operational safety, quality control and comprehension of high-temperature processes in several fields. It is currently used for offline and real-time analaysis of multi-sensor data in tokamaks.

For offline diagnostic data analysis, Thermavip provides tools for:

* Data browsing/searching based on a database of signals,
* Synchronization and visualization of heterogeneous sensor data (images, videos, 1D + time signals,...),
* Extracting statistics within videos/signals,
* Applying predefined/custom processings to videos/signals,
* Performing signal fusion processings,
* Recording any kind of sensor/processed data to share with partners.

For real-time exploitation of sensor data, Thermavip provide tools for:

* Defining asynchronous processing pipelines on distributed architectures,
* Recording any kind of sensor data within a single or multiple archives,
* Online/multiscreen displaying of several videos and temporal signals,

Below screenshot shows an example of software built based on Thermavip SDK for the post analysis of [WEST](https://irfm.cea.fr/en/west/) sensor data:

![Thermavip](docs/images/thermavip.png)

Thermavip is based on a [versatile software architecture](docs/architecture.md) composed of a C++  **S**oftware **D**evelopment  **K**it (SDK) and a plugin mechanism based on [Qt](https://www.qt.io/) only. Currently, the SDK is composed of 6 libraries:

-	Generic libraries that can be used outside *Thermavip* application, like any third party library:

	-	[Logging](docs/logging.md): logging to file/console/GUI tools
	-	[DataType](docs/datatype.md): base data types manipulated by Thermavip (N-D arrays, vector of points, scene models...)
	-	[Core](docs/core.md): asynchronous agents library based on dataflow, archiving, plugin mechanism
	-	[Plotting](docs/plotting.md): high performance plotting library for offline/firm real-time display of multi-sensor data
-	Libraries strongly connected to *Thermavip* application:
	-	[Gui](docs/gui.md): base graphical components (main window, players...)
	-	[Annotation](docs/annotation.md): graphical components used to annotate IR videos, upload annotations to JSON files or to a MySQL/SQLite DB, query and display annotations from a DB.


The basic *Thermavip* application, without plugins, provides at least the necessary features dedicated to video annotation.
Note that this requires to build *Thermavip* with the [librir](https://github.com/IRFM/librir) library (see [build](docs/compilation.md) notes).


## Prerequisites

To compile and run *Thermavip*, you need a valid Qt installation (starting version 5.9).
Qt needs to be compiled with the desktop opengl option, and with mysql support if you wish to use the [Annotation](docs/annotation.md) library with a MySQL database.

You can download Qt source code from this [website](https://download.qt.io/archive/qt/).

Note that *Thermavip* will compile and run on almost any platform supporting Qt, including Windows and all Linux distributions.

Default plugins shipped within the git reprository rely on the [HDF5](https://www.hdfgroup.org/solutions/hdf5/), [CPython](https://github.com/python/cpython) and [Ffmpeg](https://ffmpeg.org/) libraries, and are NOT compiled by default.

## Compilation

*Thermavip* compilation relies on cmake. See this [page](docs/compilation.md) for more details.

## Authors

* [Victor MONCADA](mailto:victor.moncada@cea.fr) (victor.moncada@cea.fr)
* [Léo DUBUS](mailto:leo.dubus@cea.fr) (victor.moncada@cea.fr)
* [Erwan GRELIER](mailto:erwan.grelier@cea.fr) (victor.moncada@cea.fr)

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


Thermavip framework and this page Copyright (c) 2023, CEA/IRFM