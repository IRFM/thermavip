



# Plotting library

The *Plotting* library defines items and widgets to create several types of 2D plots (see the [gallery](gallery.md)):

-	Curves,
-	Histograms,
-	Spectrograms or color images,
-	Pie charts,
-	Bar charts,
-	Polar gauges,
-	Shapes and scene models,
-	Text, arrows,... 

Its goal is to provide fast, dynamic and interactive plots meant for firm real-time display of 2D data. The library is highly customizable in order to define custom plotting items. *Plotting* library relies on the Qt **Graphics View Framework**, and is based on a heavily modified version of [Qwt](https://qwt.sourceforge.io/). The *Plotting* library depends on the *Logging*, *DataType* and *Core* libraries.

## Widgets and plotting areas

The library defines several graphics items for plotting purposes which  all inherit `VipAbstractPlotArea` base class (which itself is a `QGraphicsWidget`):

-	`VipPlotArea2D`: display plot items in a 2D cartesian system
-	`VipPlotPolarArea2D`: display items in a 2D polar system
-	`VipImageArea2D`: display color images and spectrograms
-	`VipVMultiPlotArea2D`: vertically stacked plot items in a 2D cartesian system.

These classes can be used in any `QGraphicsView` object, but the library provides several widgets to wrap one or more `VipAbstractPlotArea`:

-	`VipPlotWidget2D`: a `QGraphicsView` containing a unique `VipPlotArea2D`
-	`VipPlotPolarWidget2D`: a `QGraphicsView` containing a unique `VipPlotPolarArea2D`
-	`VipImageWidget2D`: a `QGraphicsView` containing a unique `VipImageArea2D`
-	`VipMultiGraphicsView`: a `QGraphicsView` containing a `QGraphicsWidget` used to layout several `VipAbstractPlotArea`.

These widgets (except `VipMultiGraphicsView`) contain a unique top-level `QGraphicsItem` inheriting `VipAbstractPlotArea` class, accessible using `VipAbstractPlotWidget2D::area()` method. A `VipAbstractPlotArea` defines several *base* axes that can be removed/hidden/extended, and plot items (curves, histograms...) are simply added by setting their axes.

All plot items are `QGraphicsItem` that must inherit `VipPlotItem` base class. Currently, the Plotting library defines the following plot items:

-	`VipPlotGrid`: cartesian or polar grid
-	`VipPlotCanvas`: inner part of the plotting area (space between axes) for cartesian or polar coordinate systems
-	`VipPlotCurve`: curve displayed as connected segments (default), sticks, steps or dots
-	`VipPlotHistogram`: histogram displayed as columns or lines
-	`VipPieItem`: displays one pie
-	`VipPieChart`: displays a collection of pies around the same center
-	`VipPlotBarChart`: displays a collection of bars
-	`VipPlotMarker`: displays a symbol with optional vertical/horizontal line and text
-	`VipPlotQuiver`: display several quivers
-	`VipPlotSpectrogram`: displays an image with optional contour lines
-	`VipPlotScatter`: displays scatter points
-	`VipPlotShape`: displays any kind of 2D shape
-	`VipPlotSceneModel`: displays a `VipSceneModel` (collection of shapes)

See the [gallery](gallery.md) and related examples for more details on the library capabilities.

## Firm real-time display
The Plotting library was developped for firm real-time applications. It provides a handfull of tricks/features to achieve high display frame rates even on heavy loads (see [StreamingMandelbrot](../src/Tests/Plotting/StreamingMandelbrot/main.cpp) example for instance). Its first goal is to develop online control panels displaying data from dozens of sensors: cameras (visible/infrared), thermocouples, voltage measurements...

### Data streaming
All plotting classes (inheriting `VipPlotItem`) have been optimized to render static as well as dynamic contents for streaming purposes.

For instance, the `VipPlotCurve` class is used to display 2D curves with optional symbols and curve filling. Currently, the class is able to draw around 30M points/second using software rendering only (based on Qt raster engine), even for thick lines. Likewise, the `VipPlotSpectrogram` class can display several videos at 60Hz if required, with one or more colormaps.

A `VipAbstractPlotArea` object (used to represent `VipPlotItem` instances) is designed to **absorb (and display) any amount of incoming data without blocking the GUI thread**. If the amount of data to display is too high for the rendering engine, some samples will silently be dropped in order to keep the GUI responsive.

`VipAbstractPlotArea` controls the maximum display rate using `VipAbstractPlotArea::setMaximumFrameRate()`, which is set by default to 60Hz. You can lower this value if high frame rate is not required in order to save some CPU time.

### Multithreading

Most plotting classes support setting their data in another thread than the GUI one using `VipPlotItemData::setData()`. This concerns `VipPlotCurve`, `VipPlotHistogram`, `VipPlotShape`, `VipPlotSceneModel`, `VipPlotMarker`, `VipPlotQuiver`, `VipPlotScatter` and `VipPlotSpectrogram`. This provides several advantages:

- Some computing tasks can be performed in the calling thread instead of the GUI one to increase global GUI responsiveness
- The streaming thread (which generates the input data) does not need to go through the main event loop to set the data.

All streaming examples use this feature: [StreamingMandelbrot](../src/Tests/Plotting/StreamingMandelbrot/main.cpp), [CurveStreaming](../src/Tests/Plotting/CurveStreaming/main.cpp), [CurveStreaming2](../src/Tests/Plotting/CurveStreaming2/main.cpp), [HeavyCurveStreaming](../src/Tests/Plotting/HeavyCurveStreaming/main.cpp), [SceneModel](../src/Tests/Plotting/SceneModel/main.cpp), [Quivers](../src/Tests/Plotting/Quivers/main.cpp).

### Rendering modes

The Plotting library supports OpenGL rendering based on [Qt OpenGL library]( https://doc.qt.io/qt-6/qtopengl-index.html). All classes inheriting `VipBaseGraphicsView` (like `VipPlotWidget2D`, `VipPlotPolarWidget2D`, `VipImageWidget2D` or`VipMultiGraphicsView`) provide the member `setRenderingMode()` supporting the following values:

-	`VipBaseGraphicsView::Raster`: set the viewport to a standard QWidget. The `QGraphicsView` will draw its items using the default raster engine.
-	`VipBaseGraphicsView::OpenGL`: set the viewport to a `QOpenGLWidget`. The `QGraphicsView` will draw its items using Qt OpenGL engine.
-	`VipBaseGraphicsView::OpenGLThread`: set the viewport to a `VipOpenGLWidget`. The `QGraphicsView` will draw its items using Qt OpenGL engine in a dedicated thread.

Note that using `QOpenGLWidget` is usually not the best way to leverage opengl rendering as it does not decerase the application CPU load or the paint duration (except for some situations like thick line drawing), and the raster engine provides almost always better performances.

However , the `VipOpenGLWidget` class uses a thread dedicated to rendering tasks, and is almost always faster than the raster engine. See the class documentation for more details on its internals.

Using opengl rendering requires some setup at runtime before the main `QApplication` is created, like this:

```cpp
// Setup default surface format
QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
QSurfaceFormat format;
format.setSamples(4);
format.setSwapInterval(0);
QSurfaceFormat::setDefaultFormat(format);

// Optionally, disable text caching (which is always done by default on opengl engine)
VipText::setCacheTextWhenPossible(false);
```

### Using the Agent Library

It is possible to use the *Plotting* library with the Asynchronous Agent mechanism defined in the [Core](core.md) library. The following asynchronous processing objects are provided (they all define one unique input):

- `VipDisplayCurve`: display a curve based on `VipPlotCurve`
- `VipDisplayImage`: display an image based on `VipPlotSpectrogram`
- `VipDisplaySceneModel`: display a scene model based on `VipPlotSceneModel`
- `VipDisplayHistogram`: display a histogram based on `VipPlotHistogram`
- `VipDisplayScatterPoints`: display scatter points based on `VipPlotScatter`
- `VipDisplayQuiver`: display arrows based on `VipPlotQuiver`
- `VipDisplayBars`: display bars based on `VipPlotBarChart`
- `VipDisplayMarker`: display a marker based on `VipPlotMarker`

See for instance the [StreamingCurvePipeline](../src/Tests/Gui/StreamingCurvePipeline/main.cpp), [StreamingMandelbrotPipeline](../src/Tests/Gui/StreamingMandelbrotPipeline/main.cpp) and [TemporalMandelbrotPipeline](../src/Tests/Gui/TemporalMandelbrotPipeline/main.cpp) examples.

## Style sheets

The Plotting library defines all required functions to modify the look & feel of each plot item programatically.  However it can be very cumbersome to call member functions in order to modify colors, fonts, etc. , which is why the library provides an additional style sheet mechanism modelled on the Qt one.

Each class inheriting `VipPaintItem` (all plot items and areas, axes...) provides the `setStyleSheet()` member taking either a string object or a `VipStyleSheet` object. The style sheet applies to the item itself and all its children. It is also possible to define an application-wide style sheet using `VipGlobalStyleSheet::setStyleSheet()`.

### Syntax

The style sheets syntax supports a subset of Qt style sheets syntax, and can modify any kind of property like a color, pen, brush, font, width, string, qt property, etc.

The style sheet system is extendible, which means that any plot item can define new properties. Below is the global style sheet used in the [StreamingCurvePipeline](../src/Tests/Gui/StreamingCurvePipeline/main.cpp) example:

```cpp
VipGlobalStyleSheet::setStyleSheet(

	  "VipAbstractPlotArea { title-color: white; background: #383838; mouse-wheel-zoom: true; mouse-panning:leftButton; colorpalette: set1; tool-tip-selection-border: yellow; "
	  "tool-tip-selection-background: rgba(255,255,255,30); legend-position: innerTopLeft; legend-border-distance:20; }"
	  "VipPlotItem { title-color: white; color: white; render-hint: antialiasing; }"
	  "VipPlotCurve {border-width: 2; attribute[clipToScaleRect]: true; }"
	  "VipAxisBase {title-color: white; label-color: white; pen: white;}"
	  "VipAxisBase:title {margin: 10;}"
	  "VipPlotGrid { major-pen: 1px dot white; }"
	  "VipPlotCanvas {background: #333333; border : 1px solid green;} "
	  "VipLegend { font: bold 10pt 'Arial'; display-mode: allItems; max-columns: 1; color: white; alignment:hcenter|vcenter; expanding-directions:vertical; border:white; border-radius:5px; background: "
	  "rgba(255,255,255,50); maximum-width: 16;}");
```
Most of the properties are similar to Qt ones, like `font: bold 10pt 'Arial'` or `color: white`. However some are specific to a type of plot item. For instance all `VipPlotItem` inheriting classes support the `render-hint` property which is an enum value among `antialiasing`, `highQualityAntialiasing` or `noAntialiasing`.  Likewise, `VipLegend` class  supports the property `alignment` which a combination of enum values `left|right|top|bottom|hcenter|vcenter`. The documentation of each class specifies the additionally supported properties and their possible values.

Style sheets support pseudo-states like Qt ones. For instance, `VipAbstractScale:top {}` only applies to the top axes of a `VipAbstractPlotArea` (including title axis), `VipAbstractScale:title{}` only applies to the title axis and `VipAbstractScale:top:!title{}` applies to all top axes excluding the title one.

Style sheets support child selector using the '>' operator. For instance, `VipAxisColorMap > VipSliderGrip {handle-distance:0;}` applies to all `VipSliderGrip` children of a `VipAxisColorMap`.

Currently, style sheets do NOT support several Qt features:

- Universal Selector (`*` )
- Property Selector (things like `QPushButton[flat="false"]`)
- Class Selector
- ID Selector (like `QPushButton#okButton`)
- Descendant Selector
- Sub-Controls (like `QComboBox::drop-down`)

See [StyleSheet](../src/Tests/Plotting/StyleSheet/main.cpp) for another example of complete style sheet.

