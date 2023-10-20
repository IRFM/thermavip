#include "VipColorMap.h"
#include "VipScaleMap.h"
#include "VipHistogram.h"
#include "VipNDArray.h"
#include <QPainter>
#include <QSet>
#include <QDateTime>
#include <qnumeric.h>
#include <qreadwritelock.h>
#include <vector>

template< class T>
static void applyColorMapStd(const VipColorMap * map, const VipInterval &interval, const T * values, QRgb * out, int size, int num )
{
#if defined(_OPENMP)
#pragma omp parallel for num_threads(num)
	for (int i = 0; i < size; ++i)
		out[i] = map->rgb(interval, values[i]);
#else
	Q_UNUSED(num);
	for (int i = 0; i < size; ++i)
		out[i] = map->rgb(interval, values[i]);
#endif
}

void VipColorMap::applyColorMap(const VipInterval &interval, const VipNDArray & ar, QRgb * out, int num_threads ) const
{
	if (!ar.isUnstrided())
		return;

	switch (ar.dataType())
	{
	case QMetaType::Char: return applyColorMapStd(this, interval, (char*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::SChar: return applyColorMapStd(this, interval, (signed char*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::UChar: return applyColorMapStd(this, interval, (unsigned char*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::Short: return applyColorMapStd(this, interval, (qint16*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::UShort: return applyColorMapStd(this, interval, (quint16*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::Int: return applyColorMapStd(this, interval, (qint32*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::UInt: return applyColorMapStd(this, interval, (quint32*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::Long: return applyColorMapStd(this, interval, (long*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::ULong: return applyColorMapStd(this, interval, (unsigned long*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::LongLong: return applyColorMapStd(this, interval, (qint64*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::ULongLong: return applyColorMapStd(this, interval, (quint64*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::Float: return applyColorMapStd(this, interval, (float*)ar.constData(), out, ar.size(), num_threads);
	case QMetaType::Double: return applyColorMapStd(this, interval, (double*)ar.constData(), out, ar.size(), num_threads);
	default: break;
	}
}


class VipLinearColorMap::ColorStops
{
public:
    ColorStops()
    {
    }

    void insert( double pos, const QColor &color );
    QRgb rgb( VipLinearColorMap::Mode, double pos ) const;
    QRgb rgbNoBoundaryCheck( VipLinearColorMap::Mode, double pos ) const;
    QRgb rgbNoBoundaryCheckNoFixed( double pos, int index ) const;

    QVector<double> stops() const;

//private:

    class ColorStop
    {
    public:
        ColorStop(): pos( 0.0 ), rgb( 0 ) {};
        ColorStop( double p, const QColor &c ):pos( p ), rgb( c.rgb() )
        {
            r = qRed( rgb );
            g = qGreen( rgb );
            b = qBlue( rgb );
        }

        double pos;
        QRgb rgb;
        int r, g, b;

        //step to next ColorStop
        int r_step;
        int g_step;
        int b_step;
        double one_on_pos_step;
    };

    inline int findUpper(double pos) const;
    QVector<ColorStop> _stops;

	static int findUpper(double pos, const ColorStop * stops, int size);
	static QRgb rgbNoBoundaryCheck(VipLinearColorMap::Mode, double pos, const ColorStop * stops, int size);
	static QRgb rgbNoBoundaryCheckNoFixed(double pos, int index, const ColorStop * stops, int size);
};

void VipLinearColorMap::ColorStops::insert( double pos, const QColor &color )
{
    // Lookups need to be very fast, insertions are not so important.
    // Anyway, a balanced tree is what we need here. TODO ...

    if ( pos < 0.0 || pos > 1.0 )
        return;

    int index;
    if ( _stops.size() == 0 )
    {
        index = 0;
        _stops.resize( 1 );
    }
    else
    {
        index = findUpper( pos );
        if ( index == _stops.size() ||
                qAbs( _stops[index].pos - pos ) >= 0.001 )
        {
            _stops.resize( _stops.size() + 1 );
            for ( int i = _stops.size() - 1; i > index; i-- )
                _stops[i] = _stops[i-1];
        }
    }

    _stops[index] = ColorStop( pos, color );

    for(int i=0; i < _stops.size()-1; ++i)
    {
    	_stops[i].r_step = _stops[i+1].r - _stops[i].r ;
    	_stops[i].g_step = _stops[i+1].g - _stops[i].g;
    	_stops[i].b_step = _stops[i+1].b - _stops[i].b ;
    	_stops[i].one_on_pos_step = 1.0/(_stops[i+1].pos - _stops[i].pos );
    }
}

inline QVector<double> VipLinearColorMap::ColorStops::stops() const
{
    QVector<double> positions( _stops.size() );
    for ( int i = 0; i < _stops.size(); i++ )
        positions[i] = _stops[i].pos;
    return positions;
}

int VipLinearColorMap::ColorStops::findUpper(double pos, const VipLinearColorMap::ColorStops::ColorStop * stops, int size)
{
	int index = 0;
	int n = size;

	while (n > 0)
	{
		const int half = n >> 1;
		const int middle = index + half;

		if (stops[middle].pos <= pos)
		{
			index = middle + 1;
			n -= half + 1;
		}
		else
			n = half;
	}

	return index;
}
QRgb VipLinearColorMap::ColorStops::rgbNoBoundaryCheck(VipLinearColorMap::Mode mode, double pos, const VipLinearColorMap::ColorStops::ColorStop * stops, int size)
{
	const int index = findUpper(pos,stops,size);
	if (mode == FixedColors)
	{
		return stops[index - 1].rgb;
	}
	else
	{
		return rgbNoBoundaryCheckNoFixed(pos, index, stops,size);
	}
}
QRgb VipLinearColorMap::ColorStops::rgbNoBoundaryCheckNoFixed(double pos, int index, const VipLinearColorMap::ColorStops::ColorStop * stops, int )
{
	const ColorStop &s1 = stops[index - 1];
	//const ColorStop &s2 = _stops[index];

	const double ratio = (pos - s1.pos) * (s1.one_on_pos_step);

	const int r = s1.r + (ratio * (s1.r_step));
	const int g = s1.g + (ratio * (s1.g_step));
	const int b = s1.b + (ratio * (s1.b_step));

	return qRgb(r, g, b);
}

inline int VipLinearColorMap::ColorStops::findUpper( double pos ) const
{
    int index = 0;
    int n = _stops.size();

    const ColorStop *stops = _stops.data();
    while ( n > 0 )
    {
        const int half = n >> 1;
        const int middle = index + half;

        if ( stops[middle].pos <= pos )
        {
            index = middle + 1;
            n -= half + 1;
        }
        else
            n = half;
    }

    return index;
}

inline QRgb VipLinearColorMap::ColorStops::rgbNoBoundaryCheck( VipLinearColorMap::Mode mode, double pos ) const
{
	 const int index = findUpper( pos );
	 if ( mode == FixedColors )
	{
		return _stops[index-1].rgb;
	}
	else
	{
		return rgbNoBoundaryCheckNoFixed(pos,index);
	}
}

inline QRgb VipLinearColorMap::ColorStops::rgbNoBoundaryCheckNoFixed( double pos, const int index ) const
{
	 const ColorStop &s1 = _stops[index-1];
	 //const ColorStop &s2 = _stops[index];

	 const double ratio = ( pos - s1.pos ) * ( s1.one_on_pos_step );

	 const int r = s1.r + ( ratio * ( s1.r_step ) );
	 const int g = s1.g + ( ratio * ( s1.g_step ) );
	 const int b = s1.b + ( ratio * ( s1.b_step ) );

	 return qRgb( r, g, b );
}

inline QRgb VipLinearColorMap::ColorStops::rgb(
    VipLinearColorMap::Mode mode, double pos ) const
{
    if ( pos <= 0.0 )
        return _stops[0].rgb;
    if ( pos >= 1.0 )
        return _stops[ _stops.size() - 1 ].rgb;

    return rgbNoBoundaryCheck(mode,pos);
}

//! Constructor
VipColorMap::VipColorMap( Format format ):
    d_format( format ), d_externalValue(ColorBounds), d_externalColor(0)
{
}

//! Destructor
VipColorMap::~VipColorMap()
{
}

/// Build and return a color map of 256 colors
///
/// The color table is needed for rendering indexed images in combination
/// with using colorIndex().
///
/// \param interval Range for the values
/// \return A color table, that can be used for a QImage
QVector<QRgb> VipColorMap::colorTable(const VipInterval &interval) const
{
	QVector<QRgb> table(256);

	if (interval.isValid())
	{
		const double step = interval.width() / (table.size() - 1);
		for (int i = 0; i < table.size(); i++)
			table[i] = rgb(interval, interval.minValue() + step * i);
	}

	return table;
}





class VipLinearColorMap::PrivateData
{
public:
	ColorStops colorStops;
	VipLinearColorMap::Mode mode;
	StandardColorMap type;
	QRgb * renderColors;
	int renderColorsCount;
	bool useFlatHistogram;
	int flatHistogramStrength;
	VipIntervalSampleVector histogram;
	std::vector<int> indexes;
	VipNDArrayType<float> tmpArray;
	QReadWriteLock histLock;
};



template< class T>
inline bool isNan(T  ) { return false; }
inline bool isNan(float  value) { return value != value ; }
inline bool isNan(double  value) { return value != value ; }
template <typename T>
inline T clamp(T val, T lo, T hi) {
	return (val > hi) ? hi : (val < lo) ? lo : val;
}
inline int fastrand(unsigned g_seed) {
	g_seed = (214013 * g_seed + 2531011);
	return (g_seed >> 16);// &0x7FFF; //no need to have the same range as rand() function
}

struct CastToFloat
{
	template<class T>
	float operator()(const T& v)const { return static_cast<float>(v); }
};

template<class T>
void histogram(const VipNDArrayTypeView<T>& img, VipNDArrayType<float>& tmp, int strength, const VipInterval& interval, VipIntervalSampleVector& out, int* indexes, int max_index, int num_threads)
{
	(void)num_threads;
	const T* _src = img.ptr();
	float* _out = tmp.ptr();
	int w = img.shape(1);
	int h = img.shape(0);
	//std::copy(_src, _src + w * h, _out);

	if (strength == 1) {
#pragma omp parallel for num_threads(num_threads)
		for (int y = 0; y < h; ++y) {
			for (int x = 1; x < w - 1; ++x) {
				int index = x + y * w;
				_out[index] = _src[index - 1] * 0.1f + _src[index] * 0.9f;
			}
			_out[y * w] = _src[y * w];
			_out[y * w + w - 1] = _src[y * w + w - 1];
		}
	}
	else if (strength == 2) {
#pragma omp parallel for num_threads(num_threads)
		for (int y = 0; y < h; ++y) {
			for (int x = 1; x < w - 1; ++x) {
				int index = x + y * w;
				_out[index] = _src[index - 1] * 0.1f + _src[index] * 0.8f + _src[index + 1] * 0.1f;
			}
			_out[y * w] = _src[y * w];
			_out[y * w + w - 1] = _src[y * w + w - 1];
		}
	}
	else if (strength == 3) {
		//copy first and last lines
		std::transform(_src, _src + w, _out, CastToFloat());
		std::transform(_src + (h-1)*w, _src + h*w, _out+ (h - 1) * w, CastToFloat());
#pragma omp parallel for num_threads(num_threads)
		for (int y = 1; y < h; ++y) {
			for (int x = 1; x < w - 1; ++x) {
				int index = x + y * w;
				_out[index] = _src[index - 1] * 0.1f + _src[x+(y-1)*w]*0.05f + _src[index] * 0.8f + _src[index + 1] * 0.05f;
			}
			_out[y * w] = _src[y * w];
			_out[y * w + w - 1] = _src[y * w + w - 1];
		}
	}
	else {
		//copy first and last lines
		std::transform(_src, _src + w, _out, CastToFloat());
		std::transform(_src + (h - 1) * w, _src + h * w, _out + (h - 1) * w, CastToFloat());
#pragma omp parallel for num_threads(num_threads)
		for (int y = 1; y < h-1; ++y) {
			for (int x = 1; x < w - 1; ++x) {
				int index = x + y * w;
				_out[index] = _src[index - 1] * 0.05f + _src[x + (y - 1) * w] * 0.05f + _src[x + (y + 1) * w] * 0.05f  + _src[index] * 0.8f + _src[index + 1] * 0.05f;
			}
			_out[y * w] = _src[y * w];
			_out[y * w + w - 1] = _src[y * w + w - 1];
		}
	}

	
	
	int numcolors = 1024;
	vipExtractHistogram(tmp, out, numcolors,
		Vip::SameBinHeight, interval,indexes, 2, 1, max_index, 0,-(4-(strength-1)));
}

template< class T>
void applyColorMapLinear(const VipLinearColorMap * map, const VipInterval &interval, const T * values, QRgb * out, const int w, const int h, const int num_threads )
{
	const_cast<VipLinearColorMap*>(map)->computeRenderColors();

	const int num_colors = map->colorRenderCount();
	const int multiply = num_colors - 1;
	const int max_index = num_colors + 2;
	const int size = w * h;
	//Make the for loop only use (almost) POD data
	const double one_on_width = interval.width() > 0.0 ? 1.0 / interval.width() : 0;
	const QRgb * palette = map->colorRender();
	const double min_value = interval.minValue();
	const double factor = one_on_width * multiply;
	if (!map->useFlatHistogram())
	{
#pragma omp parallel for num_threads(num_threads)
		for (int i = 0; i < size; ++i)
		{
			const T value = values[i];
			const int index = isNan(value) ? 0 :
				clamp((value - min_value) *factor + 2, 1., (double)max_index);
			out[i] = palette[index];
		}
	}
	else
	{
		//protect histogram, that can be used to draw the color map
		QWriteLocker lock(&map->d_data->histLock);

		//input array
		VipNDArrayTypeView<const T> tmp(values, vipVector(h,w));
		//prepare array of indexes in the histogram
		if ((int)map->d_data->indexes.size() != size)
			map->d_data->indexes.resize(size);
		//prepare histogram
		map->d_data->histogram.clear();
		//compute array histogram
		if (std::is_integral<T>::value) {
			if(map->d_data->tmpArray.shape() != tmp.shape())
				map->d_data->tmpArray.reset(tmp.shape());
			histogram(tmp, map->d_data->tmpArray, map->d_data->flatHistogramStrength, interval, map->d_data->histogram, map->d_data->indexes.data(), max_index, num_threads);
		}
		else {
			vipExtractHistogram(tmp, map->d_data->histogram, num_colors,
				Vip::SameBinHeight, interval, map->d_data->indexes.data(), 2, 1, max_index, 0);
		}
		//std::vector<VipIntervalSample> hist(map->d_data->histogram.size());
		//qCopy(map->d_data->histogram.begin(), map->d_data->histogram.end(), hist.begin());

		//qint64 el1 = QDateTime::currentMSecsSinceEpoch() - start;
		//apply color map
		if (map->d_data->histogram.size() == 0)
		{
			//null histogram set all pixels to 0 (nan color)
			QRgb val = palette[0];
			for (int i = 0; i < size; ++i)
				out[i] = val;
		}
		else if (map->d_data->histogram.size() < num_colors)
		{
			//small histogram, expand to num_colors
			double f = num_colors / (double)(map->d_data->histogram.size());

#pragma omp parallel for num_threads(num_threads)
			for (int i = 0; i < size; ++i){
				int index = map->d_data->indexes[i];
				if(index < max_index && index > 1)
					index = (int)(((index - 2) * f) + 2.5);
				out[i] = palette[index];
			}
		}
		else
		{
			//histogram of size num_colors
#pragma omp parallel for num_threads(num_threads)
			for (int i = 0; i < size; ++i){
				out[i] = palette[map->d_data->indexes[i]];
			}
		}

	}
}



/// Build a color map with two stops at 0.0 and 1.0. The color
/// at 0.0 is Qt::blue, at 1.0 it is Qt::yellow.
///
/// \param format Preferred format of the color map
VipLinearColorMap::VipLinearColorMap( VipColorMap::Format format ):
    VipColorMap( format )
{
    d_data = new PrivateData;
    d_data->mode = ScaledColors;
    d_data->type = Unknown;
	d_data->renderColors = NULL;
	d_data->renderColorsCount = 1024;
	d_data->useFlatHistogram = false;
	d_data->flatHistogramStrength = 1;

    setColorInterval( Qt::blue, Qt::yellow );
}

/// Build a color map with two stops at 0.0 and 1.0.
///
/// \param color1 Color used for the minimum value of the value interval
/// \param color2 Color used for the maximum value of the value interval
/// \param format Preferred format for the color map
VipLinearColorMap::VipLinearColorMap( const QColor &color1,
        const QColor &color2, VipColorMap::Format format ):
    VipColorMap( format )
{
    d_data = new PrivateData;
    d_data->mode = ScaledColors;
    setColorInterval( color1, color2 );
}

//! Destructor
VipLinearColorMap::~VipLinearColorMap()
{
	{
		QWriteLocker lock(&d_data->histLock);
		dirtyColorMap();
	}

    delete d_data;
}

const VipLinearColorMap::ColorStops & VipLinearColorMap::internalColorStops() const
{
	return d_data->colorStops;
}

/// \brief Set the mode of the color map
///
/// FixedColors means the color is calculated from the next lower
/// color stop. ScaledColors means the color is calculated
/// by interpolating the colors of the adjacent stops.
///
/// \sa mode()
void VipLinearColorMap::setMode( Mode mode )
{
    d_data->mode = mode;
	dirtyColorMap();
}

/// \return Mode of the color map
/// \sa setMode()
VipLinearColorMap::Mode VipLinearColorMap::mode() const
{
    return d_data->mode;
}

VipLinearColorMap::StandardColorMap VipLinearColorMap::type() const
{
	return d_data->type;
}

void VipLinearColorMap::setType(VipLinearColorMap::StandardColorMap t)
{
	d_data->type = t;
	dirtyColorMap();
}

void VipLinearColorMap::setUseFlatHistogram(bool enable)
{
	d_data->useFlatHistogram = enable;
	dirtyColorMap();
}
bool VipLinearColorMap::useFlatHistogram() const
{
	return d_data->useFlatHistogram;
}

void VipLinearColorMap::setFlatHistogramStrength(int strength)
{
	d_data->flatHistogramStrength = strength;
	dirtyColorMap();
}
int VipLinearColorMap::flatHistogramStrength() const
{
	return d_data->flatHistogramStrength;
}

/// Set the color range
///
/// Add stops at 0.0 and 1.0.
///
/// \param color1 Color used for the minimum value of the value interval
/// \param color2 Color used for the maximum value of the value interval
///
/// \sa color1(), color2()
void VipLinearColorMap::setColorInterval(
    const QColor &color1, const QColor &color2 )
{
    d_data->colorStops = ColorStops();
    d_data->colorStops.insert( 0.0, color1 );
    d_data->colorStops.insert( 1.0, color2 );
	dirtyColorMap();
}

QGradientStops VipLinearColorMap::gradientStops() const
{
	QGradientStops res;
	for(int i=0; i < d_data->colorStops._stops.size(); ++i)
	{
		res.append( QGradientStop(d_data->colorStops._stops[i].pos, d_data->colorStops._stops[i].rgb ));
	}
	return res;
}

void VipLinearColorMap::setGradientStops(const QGradientStops & stops)
{
	d_data->colorStops = ColorStops();
	for(int i=0; i < stops.size(); ++i)
	{
		d_data->colorStops.insert(stops[i].first,stops[i].second);
	}
	dirtyColorMap();
}


/// Add a color stop
///
/// The value has to be in the range [0.0, 1.0].
/// F.e. a stop at position 17.0 for a range [10.0,20.0] must be
/// passed as: (17.0 - 10.0) / (20.0 - 10.0)
///
/// \param value Value between [0.0, 1.0]
/// \param color Color stop
void VipLinearColorMap::addColorStop( double value, const QColor& color )
{
    if ( value >= 0.0 && value <= 1.0 )
        d_data->colorStops.insert( value, color );
	dirtyColorMap();
}

/// \return Positions of color stops in increasing order
QVector<double> VipLinearColorMap::colorStops() const
{
    return d_data->colorStops.stops();
}

/// \return the first color of the color range
/// \sa setColorInterval()
QColor VipLinearColorMap::color1() const
{
    return QColor( d_data->colorStops.rgb( d_data->mode, 0.0 ) );
}

/// \return the second color of the color range
/// \sa setColorInterval()
QColor VipLinearColorMap::color2() const
{
    return QColor( d_data->colorStops.rgb( d_data->mode, 1.0 ) );
}

void VipLinearColorMap::dirtyColorMap()
{
	if (d_data->renderColors)
		delete[] d_data->renderColors;
	d_data->renderColors = NULL;
}

QRgb VipLinearColorMap::rgbFlatHistogram(const VipInterval & interval, double value) const
{
	if (!vipIsNan(value))
	{
		//QReadLocker lock(&d_data->histLock);
		//value not in interval
		if (!interval.contains(value))
		{
			if (externalValue() == ColorFixed)
				return externalColor();
			else if (value <= interval.minValue())
				return d_data->colorStops._stops[0].rgb;
			else
				return d_data->colorStops._stops[d_data->colorStops._stops.size() - 1].rgb;
		}
		else if(value >= d_data->histogram.last().interval.maxValue())
			return d_data->colorStops._stops[d_data->colorStops._stops.size() - 1].rgb;
		else if(value <= d_data->histogram.first().interval.minValue())
			return d_data->colorStops._stops[0].rgb;

		int index = vipFindUpperEqual(d_data->histogram, value);
		if (index < 0 || index >= d_data->renderColorsCount)
			return qRgba(0, 0, 0, 0);

		if (d_data->histogram.size() < d_data->renderColorsCount) {
			index = (int)(index * (d_data->renderColorsCount / (double)d_data->histogram.size()) + 2.5);
		}

		return d_data->renderColors[index];
	}
	return qRgba(0, 0, 0, 0);
}

void VipLinearColorMap::setColorRenderCount(int num_colors)
{
	if (num_colors  != d_data->renderColorsCount)
	{
		d_data->renderColorsCount = num_colors;
		dirtyColorMap();
	}
}


int VipLinearColorMap::colorRenderCount() const
{
	return d_data->renderColorsCount;
}

QRgb * VipLinearColorMap::colorRender() const
{
	return d_data->renderColors;
}


void  VipLinearColorMap::computeRenderColors()
{
	if (!d_data->renderColors)
	{
		d_data->renderColors = new QRgb[d_data->renderColorsCount + 3];

		const int num_colors = d_data->renderColorsCount;
		const int multiply = num_colors - 1;
		const int max_index = num_colors + 2;

		//Make the for loop only use (almost) POD data
		const VipLinearColorMap::ColorStops::ColorStop * stops = internalColorStops()._stops.constData();
		const int color_count = internalColorStops()._stops.size();
		const QRgb ext_color = externalColor();
		const VipLinearColorMap::ExternalValue ext_value = externalValue();

		d_data->renderColors[0] = 0; //nan value
		if (ext_value == VipLinearColorMap::ColorFixed)
			d_data->renderColors[1] = ext_color;
		else
			d_data->renderColors[1] = stops[0].rgb;
		if (ext_value == VipLinearColorMap::ColorFixed)
			d_data->renderColors[max_index] = ext_color;
		else
			d_data->renderColors[max_index] = stops[color_count - 1].rgb;
		for (int i = 2; i < max_index; ++i)
		{
			d_data->renderColors[i] = VipLinearColorMap::ColorStops::rgbNoBoundaryCheck(mode(), (i - 2) / double(multiply), stops, color_count);
		}
	}
}

int VipLinearColorMap::colorCount() const
{
	return d_data->colorStops._stops.size();
}

QRgb VipLinearColorMap::colorAt(int index)
{
	return d_data->colorStops._stops[index].rgb;
}

double VipLinearColorMap::stopAt(int index)
{
	return d_data->colorStops._stops[index].pos;
}

void VipLinearColorMap::startDraw()
{
	d_data->histLock.lockForRead();
}
void VipLinearColorMap::endDraw()
{
	d_data->histLock.unlock();
}

/// Map a value of a given interval into a RGB value
///
/// \param interval Range for all values
/// \param value Value to map into a RGB value
///
/// \return RGB value for value
QRgb VipLinearColorMap::rgb(
    const VipInterval &interval, double value ) const
{
	if (d_data->useFlatHistogram && d_data->histogram.size() && d_data->renderColors)
		return rgbFlatHistogram(interval, value);

	//nan value
    if ( vipIsNan(value) )
        return qRgba(0, 0, 0, 0);

    const double width = interval.width();
	double ratio = 0.0;
	if ( width > 0.0 )
		ratio = ( value - interval.minValue() ) / width;

    if(!interval.contains(value))
    {
    	if(externalValue() == ColorFixed)
    		return externalColor();
    	else if ( ratio <= 0.0 )
    	    return d_data->colorStops._stops[0].rgb;
    	else if ( ratio >= 1.0 )
    	    return d_data->colorStops._stops[ d_data->colorStops._stops.size() - 1 ].rgb;
    }

    return d_data->colorStops.rgbNoBoundaryCheck( d_data->mode, ratio );
}


void VipLinearColorMap::applyColorMap(const VipInterval &interval, const VipNDArray & ar, QRgb * out, int num_threads) const
{
	if (!ar.isUnstrided())
		return;

	switch (ar.dataType())
	{
	case QMetaType::Char: return applyColorMapLinear(this, interval, (char*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::SChar: return applyColorMapLinear(this, interval, (signed char*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::UChar: return applyColorMapLinear(this, interval, (unsigned char*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::Short: return applyColorMapLinear(this, interval, (qint16*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::UShort: return applyColorMapLinear(this, interval, (quint16*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::Int: return applyColorMapLinear(this, interval, (qint32*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::UInt: return applyColorMapLinear(this, interval, (quint32*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::Long: return applyColorMapLinear(this, interval, (long*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::ULong: return applyColorMapLinear(this, interval, (unsigned long*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::LongLong: return applyColorMapLinear(this, interval, (qint64*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::ULongLong: return applyColorMapLinear(this, interval, (quint64*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::Float: return applyColorMapLinear(this, interval, (float*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	case QMetaType::Double: return applyColorMapLinear(this, interval, (double*)ar.constData(), out, ar.shape(1), ar.shape(0), num_threads);
	default: break;
	}

}

/// \brief Map a value of a given interval into a color index
///
/// \param interval Range for all values
/// \param value Value to map into a color index
///
/// \return Index, between 0 and 255
unsigned char VipLinearColorMap::colorIndex(
    const VipInterval &interval, double value ) const
{
    const double width = interval.width();

    if ( qIsNaN(value) || width <= 0.0 || value <= interval.minValue() )
        return 0;

    if ( value >= interval.maxValue() )
        return 255;

    const double ratio = ( value - interval.minValue() ) / width;

    unsigned char index;
    if ( d_data->mode == FixedColors )
        index = static_cast<unsigned char>( ratio * 255 ); // always floor
    else
        index = static_cast<unsigned char>( qRound( ratio * 255 ) );

    return index;
}

VipLinearColorMap * VipLinearColorMap::createColorMap(const QGradientStops & stops)
{
	VipLinearColorMap * map = new VipLinearColorMap();
	map->setColorInterval(stops.front().second,stops.back().second);
	for(int i=1; i < stops.size()-1; ++i)
	{
		map->addColorStop(stops[i].first,stops[i].second);
	}

	return map;
}

const char* VipLinearColorMap::colorMapToName(StandardColorMap map)
{
	switch (map) {
		case Autumn:
			return "autumn";
		case Bone:
			return "bone";
		case BuRd:
			return "burd";
		case Cool:
			return "cool";
		case Copper:
			return "copper";
		case Gray:
			return "gray";
		case Hot:
			return "hot";
		case Hsv:
			return "hsv";
		case Jet:
			return "jet";
		case Fusion:
			return "fusion";
		case Pink:
			return "pink";
		case Rainbow:
			return "rainbow";
		case Spring:
			return "spring";
		case Summer:
			return "summer";
		case Sunset:
			return "sunset";
		case Viridis:
			return "viridis";
		case White:
			return "white";
		case Winter:
			return "winter";
		case ColorPaletteStandard:
			return "standard";
		case ColorPaletteRandom:
			return "random";
		case ColorPalettePastel:
			return "pastel";
		case ColorPalettePastel1:
			return "pastel1";
		case ColorPalettePastel2:
			return "pastel2";
		case ColorPalettePaired:
			return "paired";
		case ColorPaletteAccent:
			return "accent";
		case ColorPaletteDark2:
			return "dark2";
		case ColorPaletteSet1:
			return "set1";
		case ColorPaletteSet2:
			return "set2";
		case ColorPaletteSet3:
			return "set3";
		case ColorPaletteTab10:
			return "tab10";
		default:
			return "";
	}
}

VipLinearColorMap::StandardColorMap VipLinearColorMap::colorMapFromName(const char* name)
{
	if (strcmp(name, "autumn") == 0)
		return Autumn;
	if (strcmp(name, "bone") == 0)
		return Bone;
	if (strcmp(name, "burd") == 0)
		return BuRd;
	if (strcmp(name, "cool") == 0)
		return Cool;
	if (strcmp(name, "copper") == 0)
		return Copper;
	if (strcmp(name, "gray") == 0)
		return Gray;
	if (strcmp(name, "hot") == 0)
		return Hot;
	if (strcmp(name, "hsv") == 0)
		return Hsv;
	if (strcmp(name, "jet") == 0)
		return Jet;
	if (strcmp(name, "fusion") == 0)
		return Fusion;
	if (strcmp(name, "pink") == 0)
		return Pink;
	if (strcmp(name, "rainbow") == 0)
		return Rainbow;
	if (strcmp(name, "spring") == 0)
		return Spring;
	if (strcmp(name, "summer") == 0)
		return Summer;
	if (strcmp(name, "sunset") == 0)
		return Sunset;
	if (strcmp(name, "viridis") == 0)
		return Viridis;
	if (strcmp(name, "white") == 0)
		return White;
	if (strcmp(name, "winter") == 0)
		return Winter;
	if (strcmp(name, "standard") == 0)
		return ColorPaletteStandard;
	if (strcmp(name, "random") == 0)
		return ColorPaletteRandom;
	if (strcmp(name, "pastel") == 0)
		return ColorPalettePastel;
	if (strcmp(name, "pastel1") == 0)
		return ColorPalettePastel1;
	if (strcmp(name, "pastel2") == 0)
		return ColorPalettePastel2;
	if (strcmp(name, "paired") == 0)
		return ColorPalettePaired;
	if (strcmp(name, "accent") == 0)
		return ColorPaletteAccent;
	if (strcmp(name, "dark2") == 0)
		return ColorPaletteDark2;
	if (strcmp(name, "set1") == 0)
		return ColorPaletteSet1;
	if (strcmp(name, "set2") == 0)
		return ColorPaletteSet2;
	if (strcmp(name, "set3") == 0)
		return ColorPaletteSet3;
	if (strcmp(name, "tab10") == 0)
		return ColorPaletteTab10;
	return Unknown;
}

QGradientStops VipLinearColorMap::createGradientStops(const char* name) 
{
	QByteArray ar(name);
	int index = ar.indexOf(':');
	QByteArray cname = ar;
	int light = 0;
	if (index > 0) {
		cname = ar.mid(0,index);
		light = ar.mid(index + 1).toInt();
	}
	VipLinearColorMap::StandardColorMap map = colorMapFromName(cname.data());
	if (map == Unknown)
		return QGradientStops();

	QGradientStops res = createGradientStops(map);
	if (light) {
		for (QGradientStop & stop : res)
			stop.second = stop.second.lighter(light);
	}
	return res;
}

QGradientStops VipLinearColorMap::createGradientStops(StandardColorMap color_map)
{
	QGradientStops colorStops_;

	switch(color_map){

	case VipLinearColorMap::Autumn:
		colorStops_ << QGradientStop(0, QColor(255, 0, 0))
					<< QGradientStop(1, QColor(255, 255, 0));
		break;
	case VipLinearColorMap::Bone:
		colorStops_ << QGradientStop(0, QColor(0, 0, 0))
					<< QGradientStop(0.372549, QColor(83, 83, 115))
					<< QGradientStop(0.749020, QColor(167, 199, 199))
					<< QGradientStop(1, QColor(255, 255, 255));
		break;
	case VipLinearColorMap::BuRd:
		colorStops_ << QGradientStop(0, QColor(0x2166AC))
			<< QGradientStop(0.125, QColor(0x4393C3))
			<< QGradientStop(0.25, QColor(0x92C5DE))
			<< QGradientStop(0.375, QColor(0xD1E5F0))
			<< QGradientStop(0.5, QColor(0xF7F7F7))
			<< QGradientStop(0.625, QColor(0xFDDBC7))
			<< QGradientStop(0.750, QColor(0xF4A582))
			<< QGradientStop(0.875, QColor(0xD6604D))
			<< QGradientStop(1, QColor(0xB2182B));
		break;

		break;
	case VipLinearColorMap::Cool:
		colorStops_ << QGradientStop(0, QColor(0, 255, 255))
					<< QGradientStop(1, QColor(255, 0, 255));
		break;
	case VipLinearColorMap::Copper:
		colorStops_ << QGradientStop(0, QColor(0, 0, 0))
					<< QGradientStop(1, QColor(255, 199, 127));
		break;
	case VipLinearColorMap::Gray:
		colorStops_ << QGradientStop(0, QColor(0, 0, 0))
					<< QGradientStop(1, QColor(255, 255, 255));
		break;
	case VipLinearColorMap::Hot:
		colorStops_ << QGradientStop(0, QColor(3, 0, 0))
					<< QGradientStop(0.372549, QColor(255, 0, 0))
					<< QGradientStop(0.749020, QColor(255, 255, 0))
					<< QGradientStop(1, QColor(255, 255, 255));
		break;
	case VipLinearColorMap::Hsv:
		colorStops_ << QGradientStop(0, QColor(255, 0, 0))
					<< QGradientStop(0.4, QColor(0, 255, 99))
					<< QGradientStop(0.8, QColor(199, 0, 255))
					<< QGradientStop(1, QColor(255, 0, 6));
		break;

	case VipLinearColorMap::Pink:// This is a linear interpolation of a non-linear calculation.
		colorStops_ << QGradientStop(0, QColor(15, 0, 0))
					<< QGradientStop(0.372549, QColor(195, 128, 128))
					<< QGradientStop(0.749020, QColor(234, 234, 181))
					<< QGradientStop(1, QColor(255, 255, 255));
		break;

	case VipLinearColorMap::Rainbow:// This is a linear interpolation of a non-linear calculation.
		colorStops_ << QGradientStop(0, QColor(0xE8ECFB))
			<< QGradientStop(0.045454, QColor(0xD9CCE3))
			<< QGradientStop(0.0909, QColor(0xCAACCB))
			<< QGradientStop(0.136, QColor(0xBA8DB4))
			<< QGradientStop(0.181, QColor(0xAA6F9E))
			<< QGradientStop(0.227, QColor(0x994F88))
			<< QGradientStop(0.272, QColor(0x882E72))
			<< QGradientStop(0.318, QColor(0x1965B0))
			<< QGradientStop(0.363, QColor(0x437DBF))
			<< QGradientStop(0.409, QColor(0x6195CF))
			<< QGradientStop(0.454, QColor(0x7BAFDE))
			<< QGradientStop(0.5, QColor(0x4EB265))
			<< QGradientStop(0.545, QColor(0x90C987))
			<< QGradientStop(0.591, QColor(0xCAE0AB))
			<< QGradientStop(0.636, QColor(0xF7F056))
			<< QGradientStop(0.682, QColor(0xF7CB45))
			<< QGradientStop(0.727, QColor(0xF4A736))
			<< QGradientStop(0.773, QColor(0xEE8026))
			<< QGradientStop(0.819, QColor(0xE65518))
			<< QGradientStop(0.863, QColor(0xDC050C))
			<< QGradientStop(0.909, QColor(0xA5170E))
			<< QGradientStop(0.954, QColor(0x72190E))
			<< QGradientStop(1, QColor(0x42150A));
		break;

	case VipLinearColorMap::Spring:
		colorStops_ << QGradientStop(0, QColor(255, 0, 255))
					<< QGradientStop(1, QColor(255, 255, 0));
		break;

	case VipLinearColorMap::Summer:
		colorStops_ << QGradientStop(0, QColor(0, 128, 102))
					<< QGradientStop(1, QColor(255, 255, 102));
		break;

	case VipLinearColorMap::Viridis:
		colorStops_ << QGradientStop(0, QColor(0x440154)) << QGradientStop(0.25, QColor(0x3B528B)) << QGradientStop(0.5, QColor(0x21918C)) << QGradientStop(0.75, QColor(0x5EC962))
			    << QGradientStop(1, QColor(0xFDE725));
		break;

	case VipLinearColorMap::White:
		colorStops_ << QGradientStop(0, QColor(255, 255, 255))
					<< QGradientStop(1, QColor(255, 255, 255));
		break;

	case VipLinearColorMap::Winter:
		colorStops_ << QGradientStop(0, QColor(0, 0, 255))
					<< QGradientStop(1, QColor(0, 255, 128));
		break;

	case VipLinearColorMap::Jet:

		colorStops_ << QGradientStop(0.0, QColor(0x0080))
			<< QGradientStop(0.03125381516298376, QColor(0x009f))
			<< QGradientStop(0.06250763032596753, QColor(0x00bf))
			<< QGradientStop(0.0937614454889513, QColor(0x00df))
			<< QGradientStop(0.12501526065193505, QColor(0x00ff))
			<< QGradientStop(0.1562690758149188, QColor(0x20ff))
			<< QGradientStop(0.1875228909779026, QColor(0x40ff))
			<< QGradientStop(0.21877670614088635, QColor(0x60ff))
			<< QGradientStop(0.2500305213038701, QColor(0x80ff))
			<< QGradientStop(0.28128433646685386, QColor(0x9fff))
			<< QGradientStop(0.3125381516298376, QColor(0xbfff))
			<< QGradientStop(0.34379196679282137, QColor(0xdfff))
			<< QGradientStop(0.3750457819558052, QColor(0xffff))
			<< QGradientStop(0.40629959711878894, QColor(0x20ffdf))
			<< QGradientStop(0.4375534122817727, QColor(0x40ffbf))
			<< QGradientStop(0.46880722744475645, QColor(0x60ff9f))
			<< QGradientStop(0.5000610426077402, QColor(0x80ff7f))
			<< QGradientStop(0.531314857770724, QColor(0x9fff60))
			<< QGradientStop(0.5625686729337077, QColor(0xbfff40))
			<< QGradientStop(0.5938224880966915, QColor(0xdfff20))
			<< QGradientStop(0.6250763032596752, QColor(0xffff00))
			<< QGradientStop(0.656330118422659, QColor(0xffdf00))
			<< QGradientStop(0.6875839335856427, QColor(0xffbf00))
			<< QGradientStop(0.7188377487486265, QColor(0xff9f00))
			<< QGradientStop(0.7500915639116104, QColor(0xff7f00))
			<< QGradientStop(0.7813453790745941, QColor(0xff6000))
			<< QGradientStop(0.8125991942375779, QColor(0xff4000))
			<< QGradientStop(0.8438530094005616, QColor(0xff2000))
			<< QGradientStop(0.8751068245635454, QColor(0xff0000))
			<< QGradientStop(0.9063606397265291, QColor(0xdf0000))
			<< QGradientStop(0.9376144548895129, QColor(0xbf0000))
			<< QGradientStop(0.9688682700524966, QColor(0x9f0000))
			<< QGradientStop(1.0, QColor(0x800000));

		//olorStops_ << QGradientStop(0, QColor(0x0000C1))
		// << QGradientStop(1 / 19., QColor(0x0000E1))
		// << QGradientStop(2. / 19., QColor(0x0003FF))
		// << QGradientStop(3. / 19., QColor(0x0043FF))
		// << QGradientStop(4. / 19., QColor(0x0083FF))
		// << QGradientStop(5. / 19., QColor(0x00C2FF))
		// << QGradientStop(6. / 19., QColor(0x02FFFD))
		// << QGradientStop(7. / 19., QColor(0x22FFDD))
		// << QGradientStop(8. / 19., QColor(0x42FFBD))
		// << QGradientStop(9. / 19., QColor(0x62FF9D))
		// << QGradientStop(10. / 19., QColor(0x82FF7D))
		// << QGradientStop(11. / 19., QColor(0xA2FF5D))
		// << QGradientStop(12. / 19., QColor(0xC2FF3D))
		// << QGradientStop(13. / 19., QColor(0xE4FF1B))
		// << QGradientStop(14. / 19., QColor(0xFFF900))
		// << QGradientStop(15. / 19., QColor(0xFFB900))
		// << QGradientStop(16. / 19., QColor(0xFF7900))
		// << QGradientStop(17. / 19., QColor(0xFF3900))
		// << QGradientStop(18. / 19., QColor(0xFC0000))
		// << QGradientStop(1, QColor(0xDC0000));

		break;

	case VipLinearColorMap::Fusion:
		colorStops_ << QGradientStop(0, QColor(0,0,0))
			<< QGradientStop(0.143, QColor(52, 0, 141))
			<< QGradientStop(0.285, QColor(145, 0, 157))
			<< QGradientStop(0.428, QColor(202, 20, 131))
			<< QGradientStop(0.571, QColor(235, 83, 8))
			<< QGradientStop(0.714, QColor(250, 145, 1))
			<< QGradientStop(0.857, QColor(255, 219, 17))
			<< QGradientStop(1, QColor(255, 255, 255));
		break;

	case VipLinearColorMap::Sunset:
		colorStops_ << QGradientStop(0, QColor(0x364B9A))
			<< QGradientStop(0.1, QColor(0x4A7BB7))
			<< QGradientStop(0.2, QColor(0x6EA6CD))
			<< QGradientStop(0.3, QColor(0x98CAE1))
			<< QGradientStop(0.4, QColor(0xC2E4EF))
			<< QGradientStop(0.5, QColor(0xEAECCC))
			<< QGradientStop(0.6, QColor(0xFEDA8B))
			<< QGradientStop(0.7, QColor(0xFDB366))
			<< QGradientStop(0.8, QColor(0xF67E4B))
			<< QGradientStop(0.9, QColor(0xDD3D2D))
			<< QGradientStop(0.1, QColor(0xA50026));
		break;

	case VipLinearColorMap::ColorPaletteStandard:
		colorStops_ << QGradientStop(0, QColor(0xED1C24)) << QGradientStop(0.091, QColor(0xF17524)) << QGradientStop(0.18, QColor(0xF79700)) << QGradientStop(0.27, QColor(0xE3008E) )
						<< QGradientStop(0.36, QColor(0xFCEC00)) << QGradientStop(0.45, QColor(0xAEE000)) <<QGradientStop(0.54, QColor(0x00CC1D)) << QGradientStop(0.63, QColor(0x0BB4C3) )
						<< QGradientStop(0.73, QColor(0x0051D4) )<< QGradientStop(0.82, QColor(0x460091)) << QGradientStop(0.91, QColor(0x8414A6)) << QGradientStop(1, QColor(0xE3008E));
		break;

		//from https://personal.sron.nl/~pault/#fig:scheme_bright
	case VipLinearColorMap::ColorPaletteRandom:
		colorStops_ << QGradientStop(0, QColor(0x0077BB)) << QGradientStop(0.091, QColor(0xCC3311)) << QGradientStop(0.18, QColor(0x009988)) << QGradientStop(0.27, QColor(0x22BBEE))
			<< QGradientStop(0.36, QColor(0xEE7733)) << QGradientStop(0.45, QColor(0xEE3377)) << QGradientStop(0.54, QColor(0xBBBBBB));
		break;

	case VipLinearColorMap::ColorPalettePastel:
		colorStops_ << QGradientStop(0,QColor(0xF4EFEC)) << QGradientStop(0.14,QColor(0xF4E0E9)) << QGradientStop(0.28,QColor(0xF4D9D0)) << QGradientStop(0.43,QColor(0xF4E3C9) )
				<< QGradientStop(0.57,QColor(0xB5DCE1)) << QGradientStop(0.71,QColor(0xD7E0B1)) <<QGradientStop(0.86,QColor(0xD6CDC8)) << QGradientStop(1,QColor(0xCFDAF0)) ;
		break;

	case VipLinearColorMap::ColorPalettePastel1: 
	{
		const int count = 9;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0xFBB4AE)) << QGradientStop(start += step, QColor(0xB3CDE3)) << QGradientStop(start += step, QColor(0xCCEBC5))
			    << QGradientStop(start += step, QColor(0xDECBE4)) << QGradientStop(start += step, QColor(0xFED9A6)) << QGradientStop(start += step, QColor(0xFFFFCC))
			    << QGradientStop(start += step, QColor(0xE5D8BD)) << QGradientStop(start += step, QColor(0xFDDAEC)) << QGradientStop(start += step, QColor(0xF2F2F2));
	} break;
	case VipLinearColorMap::ColorPalettePastel2: {
		const int count = 8;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0xB3E2CD)) << QGradientStop(start += step, QColor(0xFDCDAC)) << QGradientStop(start += step, QColor(0xCBD5E8))
			    << QGradientStop(start += step, QColor(0xF4CAE4)) << QGradientStop(start += step, QColor(0xE6F5C9)) << QGradientStop(start += step, QColor(0xFFF2AE))
			    << QGradientStop(start += step, QColor(0xF1E2CC)) << QGradientStop(start += step, QColor(0xCCCCCC));
	} break;
	case VipLinearColorMap::ColorPalettePaired: {
		const int count = 12;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0xA6CEE3)) << QGradientStop(start += step, QColor(0x1F78B4)) << QGradientStop(start += step, QColor(0xB2DF8A))
			    << QGradientStop(start += step, QColor(0x33A02C)) << QGradientStop(start += step, QColor(0xFB9A99)) << QGradientStop(start += step, QColor(0xE31A1C))
			    << QGradientStop(start += step, QColor(0xFDBF6F)) << QGradientStop(start += step, QColor(0xFF7F00)) << QGradientStop(start += step, QColor(0xCAB2D6))
			    << QGradientStop(start += step, QColor(0x6A3D9A)) << QGradientStop(start += step, QColor(0xFFFF99)) << QGradientStop(start += step, QColor(0xB15928));
	} break;
	case VipLinearColorMap::ColorPaletteAccent: {
		const int count = 8;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0x7FC97F)) << QGradientStop(start += step, QColor(0xBEAED4)) << QGradientStop(start += step, QColor(0xFDC086))
			    << QGradientStop(start += step, QColor(0xFFFF99)) << QGradientStop(start += step, QColor(0x386CB0)) << QGradientStop(start += step, QColor(0xF0027F))
			    << QGradientStop(start += step, QColor(0xFDBF6F)) << QGradientStop(start += step, QColor(0x666666)) ;
	} break;
	case VipLinearColorMap::ColorPaletteDark2: {
		const int count = 8;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0x1B9E77)) << QGradientStop(start += step, QColor(0xD95F02)) << QGradientStop(start += step, QColor(0x7570B3))
			    << QGradientStop(start += step, QColor(0xE7298A)) << QGradientStop(start += step, QColor(0x66A61E)) << QGradientStop(start += step, QColor(0xE6AB02))
			    << QGradientStop(start += step, QColor(0xA6761D)) << QGradientStop(start += step, QColor(0x666666));
	} break;
	case VipLinearColorMap::ColorPaletteSet1: {
		const int count = 9;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0xE41A1C)) << QGradientStop(start += step, QColor(0x377EB8)) << QGradientStop(start += step, QColor(0x4DAF4A))
			    << QGradientStop(start += step, QColor(0x984EA3)) << QGradientStop(start += step, QColor(0xFF7F00)) << QGradientStop(start += step, QColor(0xFFFF33))
			    << QGradientStop(start += step, QColor(0xA65628)) << QGradientStop(start += step, QColor(0xF781BF)) << QGradientStop(start += step, QColor(0x999999));
	} break;
	case VipLinearColorMap::ColorPaletteSet2: {
		const int count = 8;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0x66C2A5)) << QGradientStop(start += step, QColor(0xFC8D62)) << QGradientStop(start += step, QColor(0x8D9DCB))
			    << QGradientStop(start += step, QColor(0xE78AC3)) << QGradientStop(start += step, QColor(0xA6D854)) << QGradientStop(start += step, QColor(0xFFD92F))
			    << QGradientStop(start += step, QColor(0xE5C494)) << QGradientStop(start += step, QColor(0xB3B3B3)) ;
	} break;
	case VipLinearColorMap::ColorPaletteSet3: {
		const int count = 12;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0x8DD3C7)) << QGradientStop(start += step, QColor(0xFFFFB3)) << QGradientStop(start += step, QColor(0xBEBADA))
			    << QGradientStop(start += step, QColor(0xFB8072)) << QGradientStop(start += step, QColor(0x80B1D3)) << QGradientStop(start += step, QColor(0xFDB462))
			    << QGradientStop(start += step, QColor(0xB3DE69)) << QGradientStop(start += step, QColor(0xFCCDE5)) << QGradientStop(start += step, QColor(0xD9D9D9))
			    << QGradientStop(start += step, QColor(0xBC80BD)) << QGradientStop(start += step, QColor(0xCCEBC5)) << QGradientStop(start += step, QColor(0xFFED6F));
	} break;
	case VipLinearColorMap::ColorPaletteTab10: {
		const int count = 10;
		double step = 1. / (count - 1), start = 0;
		colorStops_ << QGradientStop(start, QColor(0x1F77B4)) << QGradientStop(start += step, QColor(0xFF7F0E)) << QGradientStop(start += step, QColor(0x2CA02C))
			    << QGradientStop(start += step, QColor(0xD62728)) << QGradientStop(start += step, QColor(0x9467BD)) << QGradientStop(start += step, QColor(0x8C564B))
			    << QGradientStop(start += step, QColor(0xE377C2)) << QGradientStop(start += step, QColor(0x7F7F7F)) << QGradientStop(start += step, QColor(0xBCBD22))
			    << QGradientStop(start += step, QColor(0x17BECF));
	} break;
	default:
		break;
	}
	return colorStops_;
}

VipLinearColorMap * VipLinearColorMap::createColorMap(VipLinearColorMap::StandardColorMap color_map)
{
	VipLinearColorMap * map =  createColorMap(createGradientStops(color_map));
	map->d_data->type = color_map;
	return map;
}






class VipAlphaColorMap::PrivateData
{
public:
    QColor color;
    QRgb rgb;
};


/// Constructor
/// \param color Color of the map
VipAlphaColorMap::VipAlphaColorMap( const QColor &color ):
    VipColorMap( VipColorMap::RGB )
{
    d_data = new PrivateData;
    d_data->color = color;
    d_data->rgb = color.rgb() & qRgba( 255, 255, 255, 0 );
}

//! Destructor
VipAlphaColorMap::~VipAlphaColorMap()
{
    delete d_data;
}

/// Set the color
///
/// \param color Color
/// \sa color()
void VipAlphaColorMap::setColor( const QColor &color )
{
    d_data->color = color;
    d_data->rgb = color.rgb();
}

/// \return the color
/// \sa setColor()
QColor VipAlphaColorMap::color() const
{
    return d_data->color;
}

/// \brief Map a value of a given interval into a alpha value
///
/// alpha := (value - interval.minValue()) / interval.width();
///
/// \param interval Range for all values
/// \param value Value to map into a RGB value
/// \return RGB value, with an alpha value
QRgb VipAlphaColorMap::rgb( const VipInterval &interval, double value ) const
{
    const double width = interval.width();
    if ( !qIsNaN(value) && width >= 0.0 )
    {
        const double ratio = ( value - interval.minValue() ) / width;
        int alpha = qRound( 255 * ratio );
        if ( alpha < 0 )
            alpha = 0;
        if ( alpha > 255 )
            alpha = 255;

        return d_data->rgb | ( alpha << 24 );
    }
    return d_data->rgb;
}

/// Dummy function, needed to be implemented as it is pure virtual
/// in VipColorMap. Color indices make no sense in combination with
/// an alpha channel.
///
/// \return Always 0
unsigned char VipAlphaColorMap::colorIndex(
    const VipInterval &, double ) const
{
    return 0;
}








VipColorPalette::VipColorPalette()
{
}

VipColorPalette::VipColorPalette(const QVector<QColor> & colors)
:d_colors(colors)
{
}

VipColorPalette::VipColorPalette(const QGradientStops& colors) 
: d_colors(colors.size())
{
	for (int i = 0; i < colors.size(); ++i)
		d_colors[i] = colors[i].second;
}

VipColorPalette::VipColorPalette(VipLinearColorMap::StandardColorMap map)
{
	QGradientStops stops = VipLinearColorMap::createGradientStops(map);
	d_colors.resize(stops.size());
	for(int i=0; i < stops.size(); ++i)
		d_colors[i] = stops[i].second;
}

VipColorPalette::VipColorPalette(const QColor& c)
	: d_colors(1)
{
	d_colors[0] = c;
}

int VipColorPalette::count() const
{
	return d_colors.size();
}

const QColor & VipColorPalette::color(int i) const
{
	return d_colors[i%count()];
}

QColor VipColorPalette::color(int i, uchar alpha) const
{
	QColor c = d_colors[i%count()];
	c.setAlpha(alpha);
	return c;
}

void VipColorPalette::setColors(const QVector<QColor> & colors)
{
	d_colors = colors;
}


VipColorPalette VipColorPalette::lighter(int ligthFactor) const
{
	QVector<QColor> colors = d_colors;
	for(int i=0; i < colors.size(); ++i)
		colors[i] = colors[i].lighter(ligthFactor);

	return VipColorPalette(colors);
}

VipColorPalette VipColorPalette::darker(int darkFactor) const
{
	QVector<QColor> colors = d_colors;
	for(int i=0; i < colors.size(); ++i)
		colors[i] = colors[i].darker(darkFactor);

	return VipColorPalette(colors);
}

VipColorPalette VipColorPalette::reorder(int increment, int start) const
{
	QVector<QColor> colors(count());

	QSet<int> starts;
	starts.insert(start);

	int index = start;
	for(int i=0; i < count(); i++ , index += increment)
	{
		if(index >= count())
		{
			start ++;
			index = start;

			//find new start
			int j=0;
			for(QSet<int>::iterator it = starts.begin(); it != starts.end(); ++it, ++j)
			{
				if(j != *it)
				{
					starts.insert(j);
					index = j;
					break;
				}
			}
		}


		colors[i] = color(index);
	}

	return VipColorPalette(colors);
}

bool VipColorPalette::operator==(const VipColorPalette & other)
{
	return d_colors == other.colors();
}
bool VipColorPalette::operator!=(const VipColorPalette & other)
{
	return d_colors != other.colors();
}



#include <QDataStream>

QDataStream & operator<<(QDataStream & stream, const VipColorPalette & p)
{
	return stream <<p.colors();
}

QDataStream & operator>>(QDataStream & stream, VipColorPalette & p)
{
	QVector<QColor> colors;
	stream >> colors;
	p.setColors(colors);
	return stream;
}


VipArchive& operator<<(VipArchive& arch, const VipColorMap* value)
{
	arch.content("format", (int)value->format());
	arch.content("externalValue", (int)value->externalValue());
	arch.content("externalColor", value->externalColor());
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipColorMap* value)
{
	value->setFormat((VipColorMap::Format)arch.read("format").value<int>());
	VipColorMap::ExternalValue ext_value = (VipColorMap::ExternalValue)arch.read("externalValue").value<int>();
	QRgb ext_color = arch.read("externalColor").value<int>();
	value->setExternalValue(ext_value, ext_color);
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipLinearColorMap* value)
{
	arch.content("type", (int)value->type());
	return arch.content("gradientStops", value->gradientStops());
}

VipArchive& operator>>(VipArchive& arch, VipLinearColorMap* value)
{
	value->setType((VipLinearColorMap::StandardColorMap)arch.read("type").value<int>());
	value->setGradientStops(arch.read("gradientStops").value<QGradientStops>());
	return arch;
}

VipArchive& operator<<(VipArchive& arch, const VipAlphaColorMap* value)
{
	return arch.content("color", value->color());
}

VipArchive& operator>>(VipArchive& arch, VipAlphaColorMap* value)
{
	value->setColor(arch.read("color").value<QColor>());
	return arch;
}


static int registerStreamOperators()
{
	qRegisterMetaType<VipColorPalette>();
	qRegisterMetaTypeStreamOperators<VipColorPalette>("VipColorPalette");

	qRegisterMetaType<VipColorMap*>();
	qRegisterMetaType<VipLinearColorMap*>();
	qRegisterMetaType<VipAlphaColorMap*>();

	vipRegisterArchiveStreamOperators<VipColorMap*>();
	vipRegisterArchiveStreamOperators<VipLinearColorMap*>();
	vipRegisterArchiveStreamOperators<VipAlphaColorMap*>();

	return 0;
}

static int _registerStreamOperators = registerStreamOperators();

