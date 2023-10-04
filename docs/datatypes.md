


# DataType library

The *DataType* library defines the data structures that are commonly manipulated within Thermavip. These are:

-	`complex_f` and `complex_d`: typedefs for `std::complex<float>` and `std::complex<double>`.
-	`VipPoint`: class similar to `QPointF` but using either a `double` representation of coordinates or a `long double` representation if `VIP_USE_LONG_DOUBLE` is defined. 
-	`VipPointVector`: a `QVector` of `VipPoint` with additional members `VipPointVector::boundingRect()` and  `VipPointVector::toPointF()`. Used to represent 2D curves within Thermavip.
-	`VipInterval`: represents a 1D interval.
-	`VipIntervalSample`: represents a 1D interval and an associated value, used for histogram samples
-	`VipIntervalSampleVector`: a typedef of`QVector<VipIntervalSample>` used to represent histograms within Thermavip.
-	`VipNDArray` and its derived classes (`VipNDArrayView`, `VipNDArrayType` and `VipNDArrayTypeView`) representing a dynamic N-dimension array of any type. Images within Thermavip are manipulated using `VipNDArray`.
-	`VipHybridVector` 1D vector class using either static or dynamic storage and used to represent coordinates, shapes and strides for `VipNDArray` objects.
-	`VipShape` and `VipSceneModel` representing arbitray 2D shapes mainly used to extract statistical data inside sub-part of images, or to represent video annotations.

The classes `VipPointVector`, `VipIntervalSampleVector` and `VipNDArray` internally use **Copy On Write** (COW) to avoid unnecessary allocations and copies. The classes `VipShape` and `VipSceneModel` use **explicit sharing**. This means that modifying one object will change all copies.

All of these types can be stored into `QVariant` objects. Most of them are serializable through `QDataStream` and `QTextStream`, and convertible to `QString` or `QByteArray`.

## N-D array manipulation

The `VipNDArray` class represents a N-dimension array of any type. Its goal is to provide a unique interface to exchange array objects within Thermavip using `QVariant`. It uses internally implicit sharing (Copy On Write) based on the `VipNDArrayHandle` structure to reduce memory usage and to avoid the needless copying of data.

All relevant data (data pointer, array size, shape) are internally stored in a shared pointer of `VipNDArrayHandle`. You can use `vipCreateArrayHandle()` function to create such object for a specific data type, and `vipRegisterArrayType()` to register a handle for a specific type.
 
 The shape of a `VipNDArray` is represented through a `VipNDArrayShape` object, which is a typeded for `VipHybridVector<int,Vip::None>`. To create `VipNDArrayShape` objects, you can use the helper function `vipVector(...)`.
 
 `VipNDArray` stores the data in row major order in a linear memory storage. For a 2D array, the height is the dimension of index 0, the width is the dimension of index 1. `VipNDArray` can also store `QImage` objects. See `vipToArray`, `vipToImage` and `vipIsImageArray` functions for more details.

Example:
```cpp
// Create a 640*512 image of type int
VipNDArray ar(qmetaTypeId<int>(), vipVector(512,640));
// Fill with 0
ar.fill(0);
// Print some info
std::cout<< ar.shapeCount() << std::endl;		// Number of dimensions
std::cout<< ar.shape(1) << std::endl;			// Width
std::cout<< ar.shape(0) << std::endl;			// Height
std::cout<< ar.size() << std::endl;				// Total size
std::cout<< ar.dataType() << std::endl;			// Data type based on Qt metatype system
std::cout<< ar.dataName() << std::endl;			// Data type name
std::cout<< ar(vipVector(10,0)).toInt() << std::endl;	// Print value at y=10 and x=0

// Convert int array to double array
VipNDArray ar2 = ar.convert(qMetaTypeId<double>());
// Convert int array to double array using VipNDArrayType
VipNDArrayType<double> ar3 = ar;
// Print value at y=10 and x=0. Returned value is directly a double and not a QVariant as we use VipNDArrayType
std::cout << ar3(10,0) << std::endl;

VipNDArray ar4(qmetaTypeId<int>(), vipVector(1000,1000));
// Resize and convert ar3 into ar4
ar3.resize(ar4, Vip::LinearInterpolation);

// Change the shape
ar4.reshape(vipVector(10000,100));
```

While VipNDArray provides a generic way to manipulate N-dimension arrays, it is usually more convenient to use a statically typed object and a number of dimension known at compile time. The VipNDArrayType class is used for this:
```cpp
// Create a 2D integer array
VipNDArrayType<int> ar(vipVector(512,640));
// Copy using shared ownership
VipNDArrayType<int> ar2 = ar;
// Deep copy as we convert to another data type
VipNDArrayType<double> ar3 = ar;

// Direct access to internal data
double * ptr = ar3.ptr();
std::fill_n(ptr, ar3.size(),0.);

// Create a 2D integer with its dimension known at compile time in order to fasten pixel access
VipNDArrayType<int, 2> ar4 = ar;
ar4.detach();	// detach (copy is shared)
// Direct pixel manipulation
for(int y=0; y < ar4.shape(0); ++y)
	for(int x=0; x < ar4.shape(1); ++x
		ar4(y,x) = x*y;

// Store in a QVariant
// You should ONLY store objects of type VipNDArray inside a QVariant, hence the VipNDArray(ar4)
QVariant v = QVariant::fromValue(VipNDArray(ar4));
```
It is possible to create views using `VipNDArray::mid()` or `VipNDArray::makeView()`, or directly by using the `VipNDArrayTypeView` class. Example:
```cpp
{
	int values[10] = { 0,1,2,3,4,5,6,7,8,9 };
	// Create a view over a flat array
	VipNDArray ar = VipNDArray::makeView(values, vipVector(10));
	// modify value at index 1
	ar.setValue(vipVector(1), 100);
	std::cout << values[1] << std::endl;
}

{
	// Create 2D array and fill with 0
	VipNDArray ar(qMetaTypeId<int>(), vipVector(512, 640));
	ar.fill(0);
	// Create a view starting at position (10,10) with a shape of (20,20)
	VipNDArray view = ar.mid(vipVector(10, 10), vipVector(20, 20));
	// Modify first value of the view (coordinate (10,10) in the original array)
	view.setValue(vipVector(0, 0), 3);
	std::cout << ar(vipVector(10, 10)).toInt() << std::endl;;
}

{
	// Create 2D array and fill with 0
	VipNDArray ar(qMetaTypeId<int>(), vipVector(512, 640));
	ar.fill(0);
	// Create a view starting at position (10,10) with a shape of (20,20)
	VipNDArrayTypeView<int,2> view = ar.mid(vipVector(10, 10), vipVector(20, 20));
	// Modify first value of the view (coordinate (10,10) in the original array)
	view(0, 0) = 3;
	std::cout << ar(vipVector(10, 10)).toInt() << std::endl;;
}
```

## Operations between arrays

While Thermavip SDK is NOT a linear algebra or image processing library, it still provides some convenient tools to manipulate N-D arrays through `VipNDArray` objects.

### Arithmetic operators
All arithmetic operators are overloaded for `VipNDArray`and its derived classes. Operations between `VipNDArray` objects use lazy evaluation to avoid unnecessary allocations: an expression starts to be evaluated once it is assigned to a `VipNDArray`. Arithmetic operators on arrays such as `operator+` don't perform any computation by themselves, they just return an "expression object" describing the computation to be performed. The actual computation happens later, when the whole expression is evaluated, typically in `operator=`. For example, when you do:
```cpp
VipNDArrayType<int> a(vip_vector(10, 10)),b(vip_vector(10, 10)),c(vip_vector(10, 10));
VipNDArray res = 2 *a + b * c -4;
```
Thermavip compiles it to just one for loop, so that the arrays are traversed only once. Simplifying this loop looks like this:
```cpp
for (int i = 0; i < 10*10; ++i)
res[i] = 2 * a[i] + b[i] * c[i] - 4;
```
Expression templates work with all `VipNDArray` based classes, even the standard untyped `VipNDArray`. Note that, when using raw `VipNDArray`, allocations might occur during the evaluation process to convert the array type. Evaluating an expression template is usually faster when the dimension count is known at compile time. An expression can mix unstrided arrays with views and raw `VipNDArray`. In any case, the evaluation process will select the fastest way to walk through the arrays. Furthermore, if all involved arrays in an expression provides the `Vip::Cwise` flag (default for _VipNDArrayType_ and _VipNDArrayView)_, the expression will be evaluated in parallel using openmp. This will work if `VIP_ENABLE_MULTI_THREADING` is defined (default on release build) and `VIP_DISABLE_EVAL_MULTI_THREADING` not defined.

Note that evaluating an expression will never throw an exception, unless VIP_EVAL_THROW is defined. If the evaluation fails (because, for instance, the type of operation is unsupported by the underlying data type), the destination array will be cleared. Example:
```cpp
VipNDArray ar1(qMetaTypeId<complex_d>(), vipVector(100, 100));
VipNDArray ar2(qMetaTypeId<double>(), vipVector(100, 100));
// evaluation of vipMin will fail as comparing complex value with arithmetic value is unsupported. The code compile, but res will be empty.
VipNDArray res= vipMin(ar1 , ar2);
```
Operations on raw VipNDArray will only work if underlying data types are arithmetic, complex (complex_f or complex_d) or RGB (`VipRGB` type). Most of the time, you should favor operations on typed array as they will never trigger internal allocations and generate much less code bloat.

Thermavip SDK provides several functions that return a functor when applied on a VipNDArray. For instance, one can do:
```cpp
VipNDArrayType<int> a(vip_vector(10, 10)),b(vip_vector(10, 10)),c(vip_vector(10, 10));
VipNDArray res = vipWhere( (2 * vipCos (a)) > 0, b , c);
```
This will expand to:
```cpp
for (int i = 0; i < 10*10; ++i)
res[i] = (2 * vipCos(a[i])) > 0 ? b : c;
```
Using expression template in video acquisition tools produces a smaller and more comprehensive code, as well as a usually more optimize binary.

Currently, Thermavip provides the following functions working on arrays and/or values:

-	`vipCast`: cast array type to another one
-	`vipMin`: minimum between 2 arrays/values
-	`vipMax`: maximum between 2 arrays/values
-	`vipReal`: real part of a complex array/value
-	`vipImag`: imaginary part of a complex array/value
-	`vipArg`: argument of a complex array/value
-	`vipNorm`: norm of a complex array/value
-	`vipConjugate`: complex conjugate of a complex array/value
-	`vipAbs`: absolute value of an array/value
-	`vipCeil`: wrapper for std::ceil
-	`vipFloor`: wrapper for std::floor
-	`vipRound`: wrapper for std::round
-	`vipClamp`: clamp a	array/value on min and max value
-	`vipIsNan`: returns true if array/value is NaN
-	`vipIsInf`: returns true if array/value is Inf
-	`vipReplaceNan`: replaces Nan values
-	`vipReplaceInf`: replaces Inf values
-	`vipReplaceNanInf`: replaces both NaN and Inf values
-	`vipWhere`: evalutes first argument, returns second argument if true, otherwise returns third argument
-	`vipFuzzyCompare`: wrapper for qFuzzyCompare
-	`vipFuzzyIsNull`: wrapper for qFuzzyIsNul
-	`vipSetReal`: set the real part of a complex value/array
-	`vipSetImag`: set the imaginary part of a complex value/array
-	`vipSetArg`: set the argument of complex array/value
-	`vipSetMagnitude`: set the magnitude of complex array/value
-	`vipMakeComplex`: build complex array/value from real and imaginary arrays/values
-	`vipMakeComplexd`: build complex array/value from real and imaginary arrays/values
-	`vipMakeComplexPolar`: build complex array/value from magnitude and phase arrays/values
-	`vipMakeComplexPolard`: build complex array/value from magnitude and phase arrays/values
-	`vipRed`, `vipGreen`, `vipBlue`, `vipAlpha`: extract red, green, blue or alpha component
-	`vipSetRed`, `vipSetGreen`, `vipSetBlue`, `vipSetAlpha`: set red, green, blue or alpha component
-	`vipMakeRGB`: build a VipRGB array from red, green and blue values/arrays
-	`vipSign`: sign of array/value (-1, 0 or 1)
-	Wrapper functions for all functions within `<cmath>`: `vipCos`, `vipSin`, `vipTan`, `vipACos`, `vipASin`, `vipATan`, `vipATan2`, `vipCosh`, `vipSinh`, `vipTanh`, `vipACosh`, `vipASinh`, `vipATanh`, `vipExp`, `vipSignificand`, `vipExponent`, `vipLdexp`, `vipLog`, `vipLog10`, `vipFractionalPart`, `vipIntegralPart`, `vipExp2`, `vipExpm1`, `vipIlogb`, `vipLog1p`, `vipLog2`, `vipLogb`, `vipPow`, `vipSqrt`, `vipCbrt`, `vipHypot`, `vipErf`, `vipErfc`, `vipTGamma`, `vipLGamma`.

### Additional functions working on arrays

Thermavip defines a few additional functions working on arrays. These are:
#### vipTransform
Transform a 2D array based on a QTransform object.  vipTransform returns a functor suitable for lazy evaluation. Usage:
```cpp
#include "VipTransform.h"

//...

VipNDArrayType<double> ar1( vipVector(100, 100));
//...
// Fill ar1 in the way you want
//...

// Transform. res will have the same shape as ar1 since we use Vip::SrcSize.
VipNDArray res= vipTransform< Vip::SrcSize>(ar1,QTransform().rotate(45),0.);
```
#### vipTranspose
Transpose a N-D array. vipTranspose returns a functor suitable for lazy evaluation. Usage:
```cpp
#include "VipTranspose.h"

//...

VipNDArrayType<double> ar1( vipVector(100, 100));
//...
// Fill ar1 in the way you want
//...

// Transpose.
VipNDArray res= vipTranspose(ar1);
```
#### vipReverse
Returns a functor expression to reverse input N-D array.
If Vip::ReverseFlat is used, this will swap the elements (0,N), (1, N-1), ... considering that the array is flat.
If Vip::ReverseAxis is used, this will swap full rows/columns for given axis.
To swap rows on a 2D array, use Vip::ReverseAxis with axis=0. Usage:
```cpp
#include "VipTranspose.h"

//...

VipNDArrayType<double> ar1( vipVector(100, 100));
//...
// Fill ar1 in the way you want
//...

// Reverse rows.
VipNDArray res= vipReverse<Vip::ReverseAxis>(ar1,0);
```
#### vipConvolve
Convolve a N-D array. vipConvolve returns a functor suitable for lazy evaluation. Usage:
```cpp
#include "VipConvolve.h"

//...

// Define kernel. Use list initialization
VipNDArrayType<double> kernel = { { 1.0278445, 4.10018648, 6.49510362, 4.10018648, 1.0278445 },
	{4.10018648, 16.35610171, 25.90969361, 16.35610171, 4.10018648 },
	{6.49510362, 25.90969361, 41.0435344, 25.90969361, 6.49510362},
	{4.10018648, 16.35610171, 25.90969361, 16.35610171, 4.10018648},
	{1.0278445, 4.10018648, 6.49510362, 4.10018648, 1.0278445} };

// Input array, fill it the way you want
VipNDArray ar(qMetaTypeId<double>(), vip_vector(512, 640));

// Apply convolution and store the result in res.
// Possible border rules are: Vip::Avoid, Vip::Nearest, Vip::Wrap
VipNDArray res = vipConvolve<Vip::Nearest>(ar, kernel, vip_vector(2, 2));
```

#### vipStack
Stack 2 arrays along a given axis. Usage:
```cpp
#include "VipStack.h"

//...

// Define kernel. Use list initialization
VipNDArrayType<double> kernel = { { 1.0278445, 4.10018648, 6.49510362, 4.10018648, 1.0278445 },
	{4.10018648, 16.35610171, 25.90969361, 16.35610171, 4.10018648 },
	{6.49510362, 25.90969361, 41.0435344, 25.90969361, 6.49510362},
	{4.10018648, 16.35610171, 25.90969361, 16.35610171, 4.10018648},
	{1.0278445, 4.10018648, 6.49510362, 4.10018648, 1.0278445} };

// Stake vertically kernel array and kernel multiplied by 2
kernel = vipStack(kernel,kernel*2,0);
```

## Working with QImage

A VipNDArray can wrap a QImage in order to manipulate color images. In this case, the QImage is automatically converted to ```QImage::Format_ARGB32``` format. Use `VipNDArrayTypeView<VipRGB>` to manipulate a view on the underlying QImage. Usage:

```cpp
#include "VipNDArrayImage.h"

//...

QImage img("test.png");

// Build a VipNDArray from a QImage.
// The VipNDArray can then be manipulated using functions seen above.
VipNDArray ar = vipToArray(img);
VipNDArray out = vipTransform< Vip::SrcSize>(ar, QTransform().rotate(45), VipRGB(0, 0, 0));
out.save("test2.png");

// Direct access to pixels:
VipNDArrayTypeView<VipRGB, 2> view = out;
// Print red component of first pixel
std::cout << view(0, 0).r << std::endl;

// Extract back a QImage
QImage im = vipToImage(out);
```

## Times series
The classes `VipPointVector` and `VipComplexPointVector` are used to represent 2D series of floating point and complex values respectively. Their main goal is to manipulate times series within Thermavip, although they could be used to manipulate any kind of 2D series.

Both classes are serializable using either `QTextStream` or `QDataStream`.

The `DataType` module provides several functions to interact with these types:

-	`vipExtractXValues`: convert X values of `VipPointVector` or`VipComplexPointVector` to 1d `VipNDArray`
-	`vipExtractYValues`: convert Y values of `VipPointVector` or`VipComplexPointVector` to 1d `VipNDArray`
-	`vipCreatePointVector`: build a `VipPointVector` object from 2 `VipNDArray`
-	`vipCreateComplexPointVector`: build a `VipComplexPointVector` object from 2 `VipNDArray`
-	`vipSetYValues`: set Y values of a `VipPointVector` or`VipComplexPointVector` object using a 1d `VipNDArray`
-	`vipToComplexPointVector`: convert a `VipPointVector` object or`VipComplexPointVector`
-	`vipResampleVectors` overloads: resample multiple time series  in order to share the same X coordinates
-	`vipResampleVectorsAsNDArray`: resample multiple time series  in order to share the same X coordinates and return the result as a 2D `VipNDArray`

## Histograms
Within Thermavip, a histogram is represented through the `VipIntervalSampleVector` class, which is a typedef for `QVector<VipIntervalSample>` . The ` VipIntervalSample`  class is a combination of an interval (` VipInterval`  type) and `vip_double`  (usually double) value.

Use `vipExtractHistogram` function to extract a histogram from a `VipNDArray` object.

## Scene models
Within Thermavip, a scene model is collection of 2D shapes groupped by labels. A scene model is manipulated through `VipSceneModel` class, and a unique shape through `VipShape`. Unlike most data structures within DataType module, both `VipSceneModel` and `VipShape` use shared ownership instead of COW. The reason behind this is that a `VipSceneModel` object stores internally a pointer to a `QObject` inheriting class (`VipShapeSignals`) used to emit signals when the scene model changes. Within Thermavip, scene models are used to:

-	Draw Regions Of Intereset (ROI) over images/videos
-	Extract temporal statistics over ROI on videos
-	Extract dynamic histograms
-	Display 2D shapes over videos as the result of pattern recognition tools or video annotations