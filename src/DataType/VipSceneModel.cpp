#include "VipSceneModel.h"
#include "VipNDArrayImage.h"
#include "VipMath.h"
#include "VipHistogram.h"
#include "p_fixExtractShapePixels.h"
#include <QWeakPointer>
#include <QPolygonF>
#include <QBitmap>
#include <QImage>
#include <QPainter>
#include <QBrush>
#include <QPen>
#include <QtMath>
#include <QSet>
#include <qthread.h>

/// See http://www.johndcook.com/blog/skewness_kurtosis/ for more details
class ComputeStats
{
public:
	ComputeStats()
		:n(0), M1(0), M2(0), M3(0), M4(0)
	{}

	void add(double x)
	{
		double delta, delta_n, delta_n2, term1;
		int n1 = n;
		n++;
		delta = x - M1;
		delta_n = delta / n;
		delta_n2 = delta_n * delta_n;
		term1 = delta * delta_n * n1;
		M1 += delta_n;
		M4 += term1 * delta_n2 * (n*n - 3 * n + 3) + 6 * delta_n2 * M2 - 4 * delta_n * M3;
		M3 += term1 * delta_n * (n - 2) - 3 * delta_n * M2;
		M2 += term1;
	}


	double mean() const
	{
		return M1;
	}

	double variance() const
	{
		return M2 / (n - 1.0);
	}

	double standardDeviation() const
	{
		return sqrt(variance());
	}

	double skewness() const
	{
		return sqrt(double(n)) * M3 / pow(M2, 1.5);
	}

	double kurtosis() const
	{
		return double(n)*M4 / (M2*M2) - 3.0;
	}


private:
	int n;
	double M1, M2, M3, M4;
};




/// Returns the pixel value of a QImage::Format_Mono image
//static bool monoValue(const uchar * scanline, int x)
// {
// return bool((*(scanline + (x >> 3)) >> (7- (x & 7))) & 1);
// }

#define monoValue(scanline,  x) ((*(scanline + (x >> 3)) >> (7- (x & 7))) & 1)


/// Returns a QImage::Format_Mono image with the given size, and set the palette to Qt::white and Qt::black
static QImage createEmptyMask(int width, int height)
{
	QImage bit(width, height, QImage::Format_Mono);
	bit.setColor(0, QColor(Qt::white).rgb());
	bit.setColor(1, QColor(Qt::black).rgb());
	bit.fill(0);
	return bit;
}

/// Returns black (foreground) and white (background) QImage::Format_Mono image
void extractMask(const QPainterPath & p, QImage & mask)
{
	QPainterPath temp(p);
	//TEST: remove Qt::WindingFill
	//temp.setFillRule(Qt::WindingFill);
	QRectF rect = (temp.boundingRect());
	temp.translate(rect.topLeft()*-1.0);
	rect = temp.boundingRect();
	int w = qRound(rect.width()) + 1;
	int h = qRound(rect.height()) + 1;
	if (mask.width() != w || mask.height() != h)
		mask = createEmptyMask(w, h);
	else
		mask.fill(0);
	QPainter painter(&mask);
	painter.setPen(Qt::white);
	painter.setBrush(Qt::black);
	painter.drawPath(temp);
}

QRegion extractRegion(const QPainterPath & p)
{
	if (p.isEmpty())
	{
		return QRegion();
	}
	QPainterPath temp(p);
	//temp.setFillRule(Qt::WindingFill);
	QRectF rect = (temp.boundingRect());
	QPoint top_left = rect.topLeft().toPoint();
	temp.translate(rect.topLeft()*-1.0);
	rect = temp.boundingRect();
	QBitmap bit(qRound(rect.width()) + 1, qRound(rect.height()) + 1);
	bit.fill(Qt::color0);
	{
		QPainter painter(&bit);
		painter.setPen(Qt::color1);
		painter.setBrush(Qt::color1);
		painter.fillPath(temp, Qt::color1);
	}
	return QRegion(bit).translated(top_left);
}

// QVector<QRect> extractRects(const QRegion & region, const QPoint & offset)
// {
// QVector<QRect> rects = region.rects();
// for (int i = 0; i < rects.size(); ++i)
// rects[i].translate(offset);
// return rects;
// }
QVector<QPoint> extractPixels(const QVector<QRect> & rects, const QPoint & offset)
{
	QVector<QPoint> res;

	//first, compute the total point number
	int size = 0;
	for (int i = 0; i < rects.size(); ++i)
		size += rects[i].width() * rects[i].height();
	res.resize(size);

	int count = 0;
	for (int i = 0; i < rects.size(); ++i)
	{
		const QRect r = rects[i].translated(offset);
		const int top = r.top(), bottom = r.bottom() + 1, left = r.left(), right = r.right() + 1;
		for (int y = top; y < bottom; ++y)
			for (int x = left; x < right; ++x)
				res[count++] = QPoint(x, y);
	}

	return res;
}

QVector<QPoint> extractPixels(const QImage & mask, const QPoint & offset)
{
	QVector<QPoint> res;
	res.reserve(1000);
	for (int y = 0; y < mask.height(); ++y)
	{
		const uchar* s = mask.scanLine(y);
		for (int x = 0; x < mask.width(); ++x)
		{
			if (monoValue(s, x))
				res.push_back(QPoint(x, y) + offset);
		}
	}
	return res;
}

/// Extracts all pixels coordinates along a line, without the last Point
static QPolygon extractPixels(const QLineF & l, bool all_pixels)
{
	QLine line = l.toLine();

	QPolygon res;

	//special cases: vertical and horizontal lines
	if (all_pixels)
	{
		if (line.dx() == 0)
		{
			int stepy = (line.dy() > 0 ? 1 : -1);
			for (int y = line.y1(); y != line.y2(); y += stepy)
				res.push_back(QPoint(line.x1(), y));
			return res;
		}
		else if (line.dy() == 0)
		{
			int stepx = (line.dx() > 0 ? 1 : -1);
			for (int x = line.x1(); x != line.x2(); x += stepx)
				res.push_back(QPoint(x, line.y1()));
			return res;
		}
	}
	else if (line.dx() == 0 || line.dy() == 0)
	{
		res << line.p1() << line.p2();
		return res;
	}

	//use affine function to extract pixels
	double a = (double(line.dy()) / double(line.dx()));
	double b = double(line.y1()) - a*line.x1();

	//loop on dx or dy
	if (qAbs(line.dx()) > qAbs(line.dy()))
	{
		//loop on dx
		int stepx = (line.dx() > 0 ? 1 : -1);
		for (int x = line.x1(); x != line.x2(); x += stepx)
			res.push_back(QPoint(x, qRound(x*a + b)));
	}
	else
	{
		//loop on dy
		int stepy = (line.dy() > 0 ? 1 : -1);
		for (int y = line.y1(); y != line.y2(); y += stepy)
			res.push_back(QPoint(qRound((y - b) / a), y));
	}

	return res;
}


/// Extracts all pixels coordinates along a polyline, without the last Point
static QPolygon extractPixels(const QPolygonF & polygon)
{
	QPolygon res;

	if (polygon.size() == 1)
		return res;

	for (int i = 1; i < polygon.size(); ++i)
		res += extractPixels(QLineF(polygon[i - 1], polygon[i]), true);

	return res;
}

/// Extracts all fill rects (pixels) along a polyline, without the last Point
//static QVector<QRect> extractRects(const QPolygonF & polygon)
// {
// QVector<QRect> res;
//
// if (polygon.size() == 1)
// {
// return res;
// }
//
// for (int i = 1; i < polygon.size(); ++i)
// {
// const QPolygon poly = extractPixels(QLineF(polygon[i - 1], polygon[i]), true);
// for (int p = 0; p < poly.size(); ++p)
// res.append(QRect(poly[p], QSize(1, 1)));
// }
//
// return res;
// }

/// Extracts the contour of given polygon
//static QPolygon extractContour(const QPolygonF & polygon)
// {
// QPolygon res;
//
// if(polygon.size() ==1)
// return res;
//
// for(int i=1; i < polygon.size(); ++i)
// res +=  extractPixels(QLineF(polygon[i-1],polygon[i]),false);
//
// return res;
// }

//static QList<QPolygonF> toPolygons(const QPainterPath & path)
// {
// QPainterPath pa = path.simplified();
// QList<QPolygonF> p = pa.toFillPolygons();
// return p;
// }
// static QList<QPolygon> extractContour(const QPainterPath & p, QRect * bounding = nullptr)
// {
// QList<QPolygon> res;
//
// QList<QPolygonF> tmp = toPolygons(p);
// if(bounding)
// bounding = QRect();
//
// for(int i=0; i < tmp.size(); ++i)
// {
// res << extractContour(tmp[i]);
// if(bounding)
// bounding = bounding->united(res.back().boundingRect());
// }
//
// return res;
// }
#include <limits>

//Same as int, but initialized to zero by default
struct Int {
	int value;
	Int(int value = 0) : value(value) {}
};

template<class T>
struct PixelPoint
{
	T value;
	QPoint pos;
	PixelPoint(const T& v = T(), const QPoint& p = QPoint())
		:value(v), pos(p) {}
	bool operator<(const PixelPoint& other) const {
		return value < other.value;
	}
};

template<int InnerStride, class T>
static VipShapeStatistics extractStats(const T * input, int OuterStride, const QVector<QRect> & rects, const QPoint & img_offset, VipShapeStatistics::Statistics stats, const QVector<double> & bbox_quantiles)
{
	VipShapeStatistics res;
	res.min = std::numeric_limits<double>::max();
	res.max = -std::numeric_limits<double>::max();
	bool compute_min_max = (stats & VipShapeStatistics::Minimum) || (stats & VipShapeStatistics::Maximum);

	//for quantiles extraction
	std::vector< PixelPoint<T> > pixel_points;

	//extract min, max, pixel count
	for (int i = 0; i < rects.size(); ++i)
	{
		const QRect r = rects[i];
		const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();

		for (int y = top; y <= bottom; ++y) {

			for (int x = left; x <= right; ++x) {
				const QPoint pt(x - img_offset.x(), y - img_offset.y());
				const T value = input[pt.y() * OuterStride + pt.x() * InnerStride];
				if (!vipIsNan(value)) {
					if (bbox_quantiles.size()) {
						pixel_points.push_back(PixelPoint<T>(value, QPoint(x, y)));
					}
					if (compute_min_max) {
						if (value > res.max) {
							res.max = value;
							res.maxPoint = QPoint(x, y);
						}
						if (value < res.min) {
							res.min = value;
							res.minPoint = QPoint(x, y);
						}
					}
					res.pixelCount++;
					res.average += value;
				}
			}
		}
	}

	if (res.pixelCount)
		res.average /= res.pixelCount;


	//compute quantiles bbox
	if (bbox_quantiles.size()) {
		std::sort(pixel_points.begin(), pixel_points.end());
		for (int i = 0; i < bbox_quantiles.size(); ++i) {
			double quantile = bbox_quantiles[i]; //value between 0 and 1
			size_t pixels = (size_t)std::ceil(res.pixelCount * quantile);
			if (!pixels) {
				res.quantiles.push_back(QRect());
			}
			else {
				QRect r;
				for (std::ptrdiff_t j = pixel_points.size() - 1; j >= (std::ptrdiff_t)(pixel_points.size() - pixels); --j) {
					const QPoint& pt = pixel_points[j].pos;
					if (r.isEmpty()) {
						r = QRect(pt, QSize(1, 1));
					}
					else {
						if (pt.x() < r.left()) r.setLeft(pt.x());
						if (pt.x() > r.right()) r.setRight(pt.x());
						if (pt.y() < r.top()) r.setTop(pt.y());
						if (pt.y() > r.bottom()) r.setBottom(pt.y());
					}
				}
				res.quantiles.push_back(r);
			}
		}
	}

	//extract standard deviation
	if (res.pixelCount && (stats & VipShapeStatistics::Std))
	{
		for (int i = 0; i < rects.size(); ++i)
		{
			const QRect r = rects[i];
			const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();
			for (int y = top; y <= bottom; ++y)
				for (int x = left; x <= right; ++x)
				{
					const QPoint pt(x - img_offset.x(), y - img_offset.y());
					const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
					if (!vipIsNan(value))
					{
						double std = value - res.average;
						res.std += std*std;
					}
				}
		}
		res.std /= res.pixelCount;
		res.std = qSqrt(res.std);
	}

	//extract entropy
	if (stats & VipShapeStatistics::Entropy && res.pixelCount)
	{
		QMap<T, Int> values;
		for (int i = 0; i < rects.size(); ++i)
		{
			const QRect r = rects[i];
			const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();
			for (int y = top; y <= bottom; ++y)
				for (int x = left; x <= right; ++x)
				{
					const QPoint pt(x - img_offset.x(), y - img_offset.y());
					const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
					if (!vipIsNan(value))
					{
						++values[value].value;
					}
				}
		}

		double inv_log_2 = 1 / std::log(2);
		for (typename QMap<T, Int>::const_iterator it = values.begin(); it != values.end(); ++it)
		{
			double p = it.value().value / double(res.pixelCount);
			res.entropy += p * std::log(p)* inv_log_2;
		}
		res.entropy = -res.entropy;
	}

	//extract Kurtosis and Skewness
	if (((stats & VipShapeStatistics::Kurtosis) || (stats & VipShapeStatistics::Skewness)) && res.pixelCount)
	{
		ComputeStats c;
		for (int i = 0; i < rects.size(); ++i)
		{
			const QRect r = rects[i];
			const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();
			for (int y = top; y <= bottom; ++y)
				for (int x = left; x <= right; ++x)
				{
					const QPoint pt(x - img_offset.x(), y - img_offset.y());
					const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
					if (!vipIsNan(value))
					{
						c.add(value);
					}
				}
		}
		res.kurtosis = c.kurtosis();
		res.skewness = c.skewness();
	}

	return res;
}



template<int InnerStride, class T>
static QVector<VipIntervalSample> extractHist(const T * input, int OuterStride, const QVector<QRect> & rects, int bins, const QPoint & img_offset)
{
	std::vector<double> values;
	for (int i = 0; i < rects.size(); ++i)
	{
		const QRect r = rects[i];
		const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();
		for (int y = top; y <= bottom; ++y)
			for (int x = left; x <= right; ++x)
			{
				const QPoint pt(x - img_offset.x(), y - img_offset.y());
				const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
				if (!vipIsNan(value))
				{
					values.push_back(value);
				}
			}
	}

	QVector<VipIntervalSample> res;
	VipNDArrayTypeView<double> tmp(values.data(), vipVector((int)values.size()));
	vipExtractHistogram(tmp, res, bins);
	return res;
}

template<int InnerStride, class T>
static QVector<VipIntervalSample> extractHist2(const T * input, int OuterStride, const QVector<QRect> & rects, int bins, const QPoint & img_offset)
{
	double min = std::numeric_limits<double>::max();
	double max = -std::numeric_limits<double>::max();

	//find min and max
	for (int i = 0; i < rects.size(); ++i)
	{
		const QRect r = rects[i];
		const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();
		for (int y = top; y <= bottom; ++y)
			for (int x = left; x <= right; ++x)
			{
				const QPoint pt(x - img_offset.x(), y - img_offset.y());
				const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
				if (!vipIsNan(value))
				{
					if (value > max)
						max = value;
					if (value < min)
						min = value;
				}
			}
	}

	//add the pixels in a QMap
	QMap<T, Int> values;
	for (int i = 0; i < rects.size(); ++i)
	{
		const QRect r = rects[i];
		const int top = r.top(), bottom = r.bottom(), left = r.left(), right = r.right();
		for (int y = top; y <= bottom; ++y)
			for (int x = left; x <= right; ++x)
			{
				const QPoint pt(x - img_offset.x(), y - img_offset.y());
				const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
				if (!vipIsNan(value))
				{
					values[value].value++;
				}

			}
	}
	if (values.size() == 0)
		return QVector<VipIntervalSample>();

	//compute the different steps between each values

	if (values.size() <= bins)
	{
		//there are less values than bins: keep the values as is, just compute the bin width (min vipDistance between 2 values)
		double width = 1;
		typename QMap<T, Int>::iterator it = values.begin();
		double left_bound = it.key();
		it++;
		for (; it != values.end(); ++it)
		{
			double tmp_width = it.key() - left_bound;
			left_bound = it.key();
			width = qMin(width, tmp_width);
		}

		//now fill in the result vector
		QVector<VipIntervalSample> res;
		for (typename QMap<T, Int>::iterator it = values.begin(); it != values.end(); ++it)
		{
			int count = it.value().value;
			VipIntervalSample sample(count, VipInterval(it.key() - width / 2, it.key() + width / 2));
			res.append(sample);
		}
		return res;
	}
	else
	{
		//initialize histogram
		double width = (max - min) / bins;
		double start = min;
		QVector<VipIntervalSample> res(bins);
		for (int i = 0; i < res.size(); ++i, start += width)
			res[i] = VipIntervalSample(0, VipInterval(start, start + width, VipInterval::ExcludeMaximum));


		for (typename QMap<T, Int>::iterator it = values.begin(); it != values.end(); ++it)
		{
			int index = std::floor((it.key() - min) / width);
			if (index == res.size())
				--index;
			res[index].value++;
		}

		return res;
	}
}

template<int InnerStride, class T>
static QVector<QPointF> extractPolyline(const T * input, int OuterStride, const QVector<QPoint> & pixels, const QPoint & img_offset)
{
	QVector<QPointF> res(pixels.size());

	for (int i = 0; i < pixels.size(); ++i)
	{
		const QPoint pt = pixels[i] - img_offset;
		const T value = input[pt.y()*OuterStride + pt.x()*InnerStride];
		if (!vipIsNan(value))
		{
			res[i] = QPointF(i, value);
		}
		else
		{
			res[i] = QPointF(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
		}
	}

	return res;
}

static int findId(const QList<VipShape> & shapes, int id, int & insert_index)
{
	if (id > 0)
	{
		insert_index = shapes.size();

		for (int i = 0; i < shapes.size(); ++i)
			if (shapes[i].id() > id)
			{
				insert_index = i;
				return id;
			}
			else if (shapes[i].id() == id)
			{
				insert_index = -1;
				break;
			}

		if (insert_index >= 0)
			return id;
	}

	int start_id = shapes.size() + 1;
	insert_index = shapes.size();

	for (int i = 0; i < shapes.size(); ++i)
		if (shapes[i].id() != i + 1)
		{
			start_id = i + 1;
			insert_index = i;
			break;
		}

	return start_id;
}


#include <qreadwritelock.h>

class VipShape::PrivateData
{
public:
	PrivateData() : type(Unknown), polygonBased(false), id(0), mutex(QReadWriteLock::Recursive) {

	}
	~PrivateData()
	{

	}

	QVariantMap attributes;
	QPainterPath path;
	Type type;
	bool polygonBased;
	int id;
	QString group;
	QRegion region;
	QVector<QRect> rects;
	QWeakPointer<VipSceneModel::PrivateData> parent;

	QReadWriteLock mutex;
};


VipShape::VipShape()
	:d_data(new PrivateData())
{}

VipShape::VipShape(const QPainterPath & path, Type type, bool is_polygon_based)
	: d_data(new PrivateData())
{
	d_data->path = path;
	d_data->type = type;
	d_data->polygonBased = is_polygon_based;
}

VipShape::VipShape(const QPolygonF & polygon, Type type)
	:d_data(new PrivateData())
{
	if (type == Polygon)
		setPolygon(polygon);
	else
		setPolyline(polygon);
}

VipShape::VipShape(const QRectF & rect)
	:d_data(new PrivateData())
{
	d_data->path.addRect(rect);
	d_data->type = Polygon;
	d_data->polygonBased = true;
}

VipShape::VipShape(const QPointF & point)
	:d_data(new PrivateData())
{
	d_data->path.moveTo(point);
	d_data->path.lineTo(point);
	d_data->type = Point;
}

VipShape::VipShape(const VipShape & other)
	:d_data(other.d_data)
{
}

VipShape & VipShape::operator=(const VipShape & other)
{
	if (other.d_data != d_data)
	{
		d_data->mutex.lockForWrite();
		other.d_data->mutex.lockForWrite();

		QSharedPointer<PrivateData> tmp = d_data;
		d_data = other.d_data;

		d_data->mutex.unlock();
		tmp->mutex.unlock();
	}

	return *this;
}

bool VipShape::operator!=(const VipShape & other) const
{
	return d_data != other.d_data;
}

bool VipShape::operator==(const VipShape & other) const
{
	return d_data == other.d_data;
}

VipShape VipShape::copy() const
{
	VipShape shape;
	d_data->mutex.lockForRead();
	shape.d_data->path = d_data->path;
	shape.d_data->group = d_data->group;
	shape.d_data->id = d_data->id;
	shape.d_data->type = d_data->type;
	shape.d_data->attributes = d_data->attributes;
	//TEST: detach
	shape.d_data->attributes.detach();
	shape.d_data->region = d_data->region;
	shape.d_data->rects = d_data->rects;
	shape.d_data->polygonBased = d_data->polygonBased;
	d_data->mutex.unlock();
	return shape;
}

void VipShape::setAttributes(const QVariantMap & attrs)
{
	{
		d_data->mutex.lockForWrite();
		d_data->attributes = attrs;
		//TEST: detach
		d_data->attributes.detach();
		d_data->mutex.unlock();
	}
	emitShapeChanged();
}

void VipShape::setAttribute(const QString & name, const QVariant & value)
{
	{
		d_data->mutex.lockForWrite();
		if (value.isNull())
			d_data->attributes.remove(name);
		else
			d_data->attributes[name] = value;
		d_data->mutex.unlock();
	}

	emitShapeChanged();
}

QVariantMap VipShape::attributes() const
{
	d_data->mutex.lockForRead();
	QVariantMap res = d_data->attributes;
	//TEST: detach
	res.detach();
	d_data->mutex.unlock();
	return res;
}

QVariant VipShape::attribute(const QString & attr) const
{
	QVariant res;
	d_data->mutex.lockForRead();
	auto it = d_data->attributes.constFind(attr);
	if (it != d_data->attributes.cend())
		res = it.value();
	//TEST: detach
	res.detach();
	d_data->mutex.unlock();
	return res;
}

bool VipShape::hasAttribute(const QString & attr) const
{
	d_data->mutex.lockForRead();
	bool res = d_data->attributes.constFind(attr) != d_data->attributes.cend();
	d_data->mutex.unlock();
	return res;
}

QStringList VipShape::mergeAttributes(const QVariantMap & attrs)
{
	QStringList res;
	{
		d_data->mutex.lockForWrite();

		for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
		{
			QVariantMap::const_iterator found = d_data->attributes.find(it.key());
			if (found == d_data->attributes.end() || it.value() != found.value())
			{
				d_data->attributes[it.key()] = it.value();
				res << it.key();
			}
		}
		d_data->mutex.unlock();
	}
	if (res.size())
		emitShapeChanged();
	return res;
}


QString VipShape::name() const
{
	QString res;
	d_data->mutex.lockForRead();
	QVariantMap::const_iterator it = d_data->attributes.constFind("Name");
	if (it != d_data->attributes.cend())
		res = it.value().toString();
	//TEST: detach
	res.detach();
	d_data->mutex.unlock();
	return res;
}


VipShape& VipShape::setShape(const QPainterPath & path, Type type, bool is_polygon_based)
{
	{
		d_data->mutex.lockForWrite();
		d_data->polygonBased = is_polygon_based;
		d_data->path = path;
		d_data->type = type;
		d_data->region = QRegion();
		d_data->rects.clear();
		d_data->mutex.unlock();
	}
	emitShapeChanged();
	return *this;
}

VipShape& VipShape::setPolygon(const QPolygonF & polygon)
{
	{
		d_data->mutex.lockForWrite();
		d_data->path = QPainterPath();
		d_data->region = QRegion();
		d_data->rects.clear();
		if (polygon.size() == 1 || (polygon.size() && polygon.first() != polygon.last()))
		{
			QPolygonF p = polygon;
			p.append(p.first());
			d_data->path.addPolygon(p);
		}
		else
			d_data->path.addPolygon(polygon);
		d_data->type = Polygon;
		d_data->polygonBased = true;
		d_data->mutex.unlock();
	}
	emitShapeChanged();
	return *this;
}

VipShape& VipShape::setPolyline(const QPolygonF & polygon)
{
	{
		d_data->mutex.lockForWrite();
		d_data->path = QPainterPath();
		d_data->region = QRegion();
		d_data->rects.clear();
		if (polygon.size() && polygon.first() == polygon.last())
		{
			QPolygonF p = polygon;
			p.remove(p.size() - 1);
			d_data->path.addPolygon(p);
		}
		else
			d_data->path.addPolygon(polygon);
		d_data->type = Polyline;
		d_data->polygonBased = true;
		d_data->mutex.unlock();
	}
	emitShapeChanged();
	return *this;
}

VipShape& VipShape::setRect(const QRectF & rect)
{
	{
		d_data->mutex.lockForWrite();
		d_data->path = QPainterPath();
		d_data->region = QRegion();
		d_data->rects.clear();
		d_data->path.addRect(rect);
		d_data->type = Polygon;
		d_data->polygonBased = true;
		d_data->mutex.unlock();
	}
	emitShapeChanged();
	return *this;
}

VipShape& VipShape::setPoint(const QPointF & point)
{
	{
		d_data->mutex.lockForWrite();
		d_data->path = QPainterPath();
		d_data->region = QRegion();
		d_data->rects.clear();
		d_data->path.moveTo(point);
		d_data->path.lineTo(point);
		d_data->type = Point;
		d_data->polygonBased = false;
		d_data->mutex.unlock();
	}
	emitShapeChanged();
	return *this;
}

VipShape& VipShape::transform(const QTransform & tr)
{
	{
		d_data->mutex.lockForWrite();
		d_data->path = tr.map(d_data->path);
		d_data->region = QRegion();
		d_data->rects.clear();
		d_data->mutex.unlock();
	}
	emitShapeChanged();
	return *this;
}

VipShape & VipShape::unite(const VipShape & other)
{
	QPainterPath p;
	{
		d_data->mutex.lockForWrite();
		d_data->region = QRegion();
		d_data->rects.clear();
		p = d_data->path;
		p |= other.shape();
		d_data->polygonBased &= other.isPolygonBased();
		d_data->mutex.unlock();
	}
	this->setShape(p, Path, d_data->polygonBased);
	this->mergeAttributes(other.attributes());
	return *this;
}

VipShape & VipShape::intersect(const VipShape & other)
{
	QPainterPath p;
	{
		d_data->mutex.lockForWrite();
		d_data->region = QRegion();
		d_data->rects.clear();
		p = d_data->path;
		p &= (other.shape());
		d_data->polygonBased &= other.isPolygonBased();
		d_data->mutex.unlock();
	}
	this->setShape(p, Path, d_data->polygonBased);
	this->mergeAttributes(other.attributes());
	return *this;
}

VipShape & VipShape::subtract(const VipShape & other)
{
	QPainterPath p;
	{
		d_data->mutex.lockForWrite();
		d_data->region = QRegion();
		d_data->rects.clear();
		p = d_data->path;
		p = p.subtracted(other.shape());
		d_data->polygonBased &= other.isPolygonBased();
		d_data->mutex.unlock();
	}
	this->setShape(p, Path, d_data->polygonBased);
	this->mergeAttributes(other.attributes());
	return *this;
}

bool VipShape::isPolygonBased() const
{
	return d_data->polygonBased;
}

QRectF VipShape::boundingRect() const
{
	d_data->mutex.lockForRead();
	const QRectF res = d_data->path.boundingRect();
	d_data->mutex.unlock();
	return res;
}

QPainterPath VipShape::shape() const
{
	d_data->mutex.lockForRead();
	const QPainterPath res = d_data->path;
	d_data->mutex.unlock();
	return res;
}

QPolygonF VipShape::polygon() const
{
	QPolygonF res;
	d_data->mutex.lockForRead();
	const QList<QPolygonF> p = d_data->path.toSubpathPolygons();
	if (p.size())
	{
		QPolygonF poly = p.first();
		if (poly.first() != poly.last())
			poly.append(poly.first());
		res = poly;
	}
	d_data->mutex.unlock();
	return res;
}

QPolygonF VipShape::polyline() const
{
	QPolygonF res;
	d_data->mutex.lockForRead();
	const QList<QPolygonF> p = d_data->path.toSubpathPolygons();
	if (p.size())
	{
		QPolygonF poly = p.first();
		if (poly.first() == poly.last())
			poly.remove(poly.size() - 1);
		res = poly;
	}
	d_data->mutex.unlock();
	return res;
}

QPointF VipShape::point() const
{
	d_data->mutex.lockForRead();
	const QPointF res = d_data->path.currentPosition();
	d_data->mutex.unlock();
	return res;
}

VipShape::Type VipShape::type() const
{
	if (!d_data)
		return Unknown;
	d_data->mutex.lockForRead();
	VipShape::Type res = d_data->type;
	d_data->mutex.unlock();
	return res;
}


//pixel based functions
QVector<QPoint> VipShape::fillPixels() const
{
	QVector<QPoint> res;

	d_data->mutex.lockForRead();
	Type t = d_data->type;
	d_data->mutex.unlock();

	if (t == Point)
		res = QVector<QPoint>() << QPoint(std::floor(point().x()), std::floor(point().y()));
	else if (t == Polyline)
		res = extractPixels(polyline());
	else if (t != Unknown)
		res = extractPixels(fillRects(), QPoint(0, 0));

	return res;
}

QVector<QRect> VipShape::fillRects() const
{
	//QVector<QRect> res;
//
	// d_data->mutex.lockForRead();
	// Type t = d_data->type;
	// d_data->mutex.unlock();
//
	// if (t == Point)
	// res  << QRect(point().toPoint(), QSize(1,1));
	// else if (t == Polyline)
	// res = extractRects(polyline());
	// else if (t != Unknown)
	// {
	// region();
	// d_data->mutex.lockForRead();
	// res = d_data->rects;
	// d_data->mutex.unlock();
	// }
	// res = extractRects(region(), QPoint(0, 0));
//
	// return res;

	QVector<QRect> res;
	(void)region(&res);
	return res;
}

QVector<QPoint> VipShape::fillPixels(const QList<VipShape> & shapes)
{
	QRegion full_region;
	for (int i = 0; i < shapes.size(); ++i)
	{
		full_region |= shapes[i].region();
	}
	QVector<QRect> rects(full_region.rectCount());
	std::copy(full_region.begin(), full_region.end(),rects.begin());
	return extractPixels(rects, QPoint(0, 0));
}

QVector<QRect> VipShape::fillRects(const QList<VipShape> & shapes)
{
	QRegion full_region;
	for (int i = 0; i < shapes.size(); ++i)
	{
		full_region |= shapes[i].region();
	}
	QVector<QRect> rects(full_region.rectCount());
	std::copy(full_region.begin(), full_region.end(), rects.begin());
	return rects;
}

QRegion VipShape::region(QVector<QRect> *out_rects) const
{
	d_data->mutex.lockForRead();

	bool empty_region = d_data->region.isEmpty();
	Type t = d_data->type;
	QRegion region = d_data->region;
	QVector<QRect> rects = d_data->rects;

	d_data->mutex.unlock();

	if (empty_region)
	{
		if (t == Path || t == Polygon)
		{
			d_data->mutex.lockForRead();
			region = vipExtractRegion(d_data->path);//extractRegion(d_data->path);
			d_data->mutex.unlock();
			rects.resize(region.rectCount());
			std::copy(region.begin(), region.end(), rects.begin());
			//rects = region.rects(); // QVector<QRect>(region.begin(), region.end());//region.rects();
		}
		else if (t == Point)
		{
			QPoint p (std::floor(point().x()), std::floor(point().y()));
			region = QRegion(p.x(), p.y(), 1, 1);
			rects.append(QRect(p, QSize(1, 1)));
		}
		else if (t == Polyline)
		{
			QVector<QPoint> points = extractPixels(polyline());
			rects = QVector<QRect>(points.size());
			for (int i = 0; i < points.size(); ++i)
				rects[i] = QRect(points[i].x(), points[i].y(), 1, 1);
			region.setRects(rects.data(), rects.size());
			rects.resize(region.rectCount());
			std::copy(region.begin(), region.end(), rects.begin());
		}

		d_data->mutex.lockForWrite();
		const_cast<QRegion&>(d_data->region) = region;
		const_cast<QVector<QRect> &>(d_data->rects) = rects;
		d_data->mutex.unlock();
		if (out_rects)
			*out_rects = rects;
	}
	else if (out_rects)
		*out_rects = rects;

	return region;
}

QList<QPolygon> VipShape::outlines() const
{
	QList<QPolygon> res;

	Type t = type();
	if (!(t == Point || t == Polyline || t == Unknown))
	{
		QPainterPath p;
		p.addRegion(region());
		p = p.simplified();
		QList<QPolygonF> polygons = p.toFillPolygons();
		for (int i = 0; i < polygons.size(); ++i)
			res << polygons[i].toPolygon();
	}

	return res;
}

int VipShape::id() const
{
	d_data->mutex.lockForRead();
	int res = d_data->id;
	d_data->mutex.unlock();
	return res;
}

bool VipShape::setId(int id)
{
	d_data->mutex.lockForRead();
	int current_id = d_data->id;
	const QString group = d_data->group;
	d_data->mutex.unlock();

	if (id < 1)
		return false;
	else if (id == current_id)
		return true;

	if (VipSceneModel  model = parent())
	{
		VipShape sh = model.find(d_data->group, id);
		if (sh.isNull())
		{
			d_data->mutex.lockForWrite();
			d_data->id = id;
			d_data->mutex.unlock();

			model.add(group, *this);

			return true;
		}
		else
			return false;
	}

	d_data->mutex.lockForWrite();
	d_data->id = id;
	d_data->mutex.unlock();
	return true;
}

QString VipShape::group() const
{
	d_data->mutex.lockForRead();
	const QString res = d_data->group;
	d_data->mutex.unlock();
	return res;
}

QString VipShape::identifier() const
{
	d_data->mutex.lockForRead();
	const QString res = d_data->group + ":" + QString::number(d_data->id);
	d_data->mutex.unlock();
	return res;
}

void VipShape::setGroup(const QString & group)
{
	d_data->mutex.lockForWrite();
	d_data->group = group;
	d_data->mutex.unlock();
	if (VipSceneModel  model = parent())
	{
		model.add(group, *this);
	}
}

VipShapeSignals * VipShape::shapeSignals() const
{
	VipShapeSignals * res = nullptr;

	d_data->mutex.lockForRead();
	if (VipSceneModel s = this->parent())
		res = s.shapeSignals();
	d_data->mutex.unlock();
	return res;
}

void VipShape::emitShapeChanged()
{
	if (VipSceneModel s = this->parent())
	{
		Q_EMIT s.shapeSignals()->sceneModelChanged(s);
	}
}




VipShapeStatistics VipShape::statistics(const QVector<QRect> & rects, const VipNDArray & img, const QPoint & img_offset, const QRect & bounding_rect, VipNDArray * tmp, VipShapeStatistics::Statistics stats, const QVector<double>& bbox_quantiles)
{
	if (!img.canConvert<double>())
		return VipShapeStatistics();

	QRect bounding = bounding_rect;
	if (bounding.isEmpty())
	{
		for (int i = 0; i < rects.size(); ++i)
			bounding |= rects[i];
	}
	if (bounding.isEmpty())
		return VipShapeStatistics();

	QRect image_rect(img_offset, QSize(img.shape(1), img.shape(0)));
	bounding = bounding.united(image_rect);

	VipNDArray buffer;
	if (!tmp)
		tmp = &buffer;

	if (tmp->isNull() || tmp->dataType() != QMetaType::Double || tmp->shapeCount() < 2 || tmp->shape(0) != bounding.height() || tmp->shape(1) != bounding.width())
		*tmp = VipNDArray(QMetaType::Double, vipVector(bounding.height(), bounding.width()));

	//TEST
	//if (img.mid(vipVector(bounding.top() - img_offset.y(), bounding.left() - img_offset.x()), vipVector(bounding.height(), bounding.width())).convert(*tmp))
	// {
	// QPointF topLeft = bounding.topLeft();
	// VipArrayStats<double, Vip::AllStats> st = vipArrayStats<double, Vip::AllStats>(*tmp, vipOverRects(rects), vipVector(topLeft.y(), topLeft.x()));
	// VipShapeStatistics res;
	// res.average = st.mean;
	// res.min = st.min;
	// res.max = st.max;
	// res.minPoint.rx() = st.minPos[1];
	// res.minPoint.ry() = st.minPos[0];
	// res.maxPoint.rx() = st.maxPos[1];
	// res.maxPoint.ry() = st.maxPos[0];
	// res.std = st.std;
	// res.pixelCount = st.count;
	// return res;
	// }
	// return VipShapeStatistics();

	if (img.mid(vipVector(bounding.top() - img_offset.y(), bounding.left() - img_offset.x()), vipVector(bounding.height(), bounding.width())).convert(*tmp))
	{
		return extractStats<1>((const double*)tmp->data(), tmp->shape(1), rects, bounding.topLeft(), stats, bbox_quantiles);
	}

	return VipShapeStatistics();
}


VipShapeStatistics VipShape::statistics(const VipNDArray & img, const QPoint & img_offset, VipNDArray * buffer, VipShapeStatistics::Statistics stats, const QVector<double>& bbox_quantiles ) const
{
	QRect bounding;
	const QVector<QRect> tmp = fillRects();
	const QVector<QRect> rects = clip(tmp, QRect(img_offset, QSize(img.shape(1), img.shape(0))), &bounding);
	return statistics(rects, img, img_offset, bounding, buffer, stats, bbox_quantiles);
}


QVector<VipIntervalSample> VipShape::histogram(int bins, const QVector<QRect> & rects, const VipNDArray & img, const QPoint & img_offset, const QRect & bounding_rect, VipNDArray * tmp)
{
	if (!img.canConvert<double>())
		return QVector<VipIntervalSample>();

	QRect bounding = bounding_rect;
	if (bounding.isEmpty())
	{
		for (int i = 0; i < rects.size(); ++i)
			bounding |= rects[i];
	}
	if (bounding.isEmpty())
		return QVector<VipIntervalSample>();

	QRect image_rect(img_offset, QSize(img.shape(1), img.shape(0)));
	bounding = bounding.united(image_rect);

	VipNDArray buffer;
	if (!tmp)
		tmp = &buffer;

	if (tmp->isNull() || tmp->dataType() != QMetaType::Double || tmp->shapeCount() < 2 || tmp->shape(0) != bounding.height() || tmp->shape(1) != bounding.width())
		*tmp = VipNDArray(QMetaType::Double, vipVector(bounding.height(), bounding.width()));

	if (img.mid(vipVector(bounding.top() - img_offset.y(), bounding.left() - img_offset.x()), vipVector(bounding.height(), bounding.width())).convert(*tmp))
	{
		//QVector<VipIntervalSample> res;
		//vipExtractHistogram(*tmp, res, bins);
		return  extractHist<1>((const double*)tmp->data(), tmp->shape(1), rects, bins, bounding.topLeft());
	}

	return QVector<VipIntervalSample>();
}

QVector<VipIntervalSample> VipShape::histogram(int bins, const VipNDArray & img, const QPoint & img_offset, VipNDArray * buffer) const
{
	QRect bounding;
	const QVector<QRect> tmp = fillRects();
	const QVector<QRect> rects = clip(tmp, QRect(img_offset, QSize(img.shape(1), img.shape(0))), &bounding);
	return histogram(bins, rects, img, img_offset, bounding, buffer);
}


QVector<QPointF> VipShape::polyline(const QVector<QPoint> & points, const VipNDArray & img, const QPoint & img_offset, const QRect & bounding_rect, VipNDArray * tmp)
{
	if (!img.canConvert<double>())
		return QVector<QPointF>();


	QRect bounding = bounding_rect;
	if (bounding.isEmpty())
		bounding = QPolygon(points).boundingRect();
	if (bounding.isEmpty())
		return QVector<QPointF>();

	QRect image_rect(img_offset, QSize(img.shape(1), img.shape(0)));
	bounding = bounding.united(image_rect);

	VipNDArray buffer;
	if (!tmp)
		tmp = &buffer;

	if (tmp->isNull() || tmp->dataType() != QMetaType::Double || tmp->shapeCount() < 2 || tmp->shape(0) != bounding.height() || tmp->shape(1) != bounding.width())
		*tmp = VipNDArray(QMetaType::Double, vipVector(bounding.height(), bounding.width()));

	if (img.mid(vipVector(bounding.top() - img_offset.y(), bounding.left() - img_offset.x()), vipVector(bounding.height(), bounding.width())).convert(*tmp))
	{
		return extractPolyline<1>((const double*)tmp->data(), tmp->shape(1), points, bounding.topLeft());
	}
	//else
	// {
	// //test
	// img.mid(vipVector(bounding.top() - img_offset.y(), bounding.left() - img_offset.x()), vipVector(bounding.height(), bounding.width())).convert(*tmp);
	// }

	return QVector<QPointF>();
}


QVector<QPointF> VipShape::polyline(const VipNDArray & img, const QPoint & img_offset, VipNDArray * buffer) const
{
	if (type() != Polyline)
		return QVector<QPointF>();

	QRect bounding;
	const QVector<QPoint> tmp = fillPixels();
	const QVector<QPoint> pixels = clip(tmp, QRect(img_offset, QSize(img.shape(1), img.shape(0))), &bounding);
	return polyline(pixels, img, img_offset, bounding, buffer);
}

bool VipShape::writeAttribute(const QVariant & value, const QVector<QPoint> & points, VipNDArray & img, const QPoint & img_offset, const QRect & bounding_rect)
{
	QRect bounding = bounding_rect;
	if (bounding.isEmpty())
		bounding = QPolygon(points).boundingRect();
	if (bounding.isEmpty())
		return false;

	QVariant v = value;
	if (!v.convert(img.dataType()))
		return false;
	for (int i = 0; i < points.size(); ++i)
	{
		const QPoint pt = points[i] - img_offset;
		img.setValue(vipVector(pt.y(), pt.x()), value);
	}
	return true;
}

bool VipShape::writeAttribute(const QString & attribute, VipNDArray & img, const QPoint & img_offset)
{
	QRect bounding;
	const QVector<QPoint> tmp = fillPixels();
	const QVector<QPoint> pixels = clip(tmp, QRect(img_offset, QSize(img.shape(1), img.shape(0))), &bounding);

	QVariant value;
	if (attribute == "id")
		value = id();
	else if (attribute == "group")
		value = group();
	else if (hasAttribute(attribute))
		value = this->attribute(attribute);
	else
		return false;

	if (!value.canConvert(img.dataType()))
		return false;

	return writeAttribute(value, pixels, img, img_offset, bounding);
}


QVector<QPoint> VipShape::clip(const QVector<QPoint> & points, const QRect & rect, QRect * bounding)
{
	QVector<QPoint> pixels;
	pixels.reserve(points.size());

	QPoint img_offset = rect.topLeft();

	for (int i = 0; i <points.size(); ++i)
	{
		const QPoint pt = points[i] - img_offset;
		if (pt.x() >= 0 && pt.y() >= 0 && pt.x() < rect.width() && pt.y() < rect.height())
		{
			pixels.append(points[i]);

			if (bounding)
			{
				const QPoint p = points[i];

				if (bounding->isEmpty())
					*bounding = QRect(p, QSize(0, 0));

				if (bounding->left() > p.x())
					bounding->setLeft(p.x());
				else if (bounding->right() < p.x())
					bounding->setRight(p.x());

				if (bounding->top() > p.y())
					bounding->setTop(p.y());
				else if (bounding->bottom() < p.y())
					bounding->setBottom(p.y());
			}
		}
	}

	return pixels;
}


QVector<QRect> VipShape::clip(const QVector<QRect> & rects, const QRect & rect, QRect * bounding)
{
	QVector<QRect> res;
	res.reserve(rects.size());

	if (bounding)
		*bounding = QRect();

	for (int i = 0; i < rects.size(); ++i)
	{
		const QRect r = rects[i] & rect;

		if (!r.isEmpty()) {
			res.append(r);
			if (bounding)
				*bounding |= r;
		}
	}

	return res;
}



class VipSceneModel::PrivateData
{
public:
	QMap<QString, QList<VipShape> > shapes;
	VipSceneModel * sceneModel;
	VipShapeSignals * shapeSignals;
	QVariantMap attributes;

	PrivateData() : sceneModel(nullptr), shapeSignals(nullptr)
	{
		shapeSignals = new VipShapeSignals();

	}

	~PrivateData()
	{
		for (QMap<QString, QList<VipShape> >::iterator it = shapes.begin(); it != shapes.end(); ++it)
		{
			QList<VipShape> & in = it.value();
			for (int i = 0; i < in.size(); ++i)
				in[i].d_data->parent = QWeakPointer<PrivateData>();
		}
		shapeSignals->d_data = QSharedPointer<VipSceneModel::PrivateData>();

		// Do NOT use deleteLater() as the object will never be deleted if it belongs to a thread without event loop!!!
		//shapeSignals->deleteLater();
		//shapeSignals->moveToThread(QThread::currentThread());
		delete shapeSignals;
	}
};

VipSceneModel VipSceneModel::null()
{
	return VipSceneModel(QSharedPointer<VipSceneModel::PrivateData>());
}

VipSceneModel::VipSceneModel()
	:d_data(new VipSceneModel::PrivateData())
{
	d_data->sceneModel = this;
	d_data->shapeSignals->d_data = d_data;
}

VipSceneModel::VipSceneModel(const VipSceneModel & other)
	:d_data(other.d_data)
{}

VipSceneModel::VipSceneModel(QSharedPointer<VipSceneModel::PrivateData> data)
	: d_data(data)
{}

VipSceneModel:: ~VipSceneModel()
{
}

VipSceneModel & VipSceneModel::operator=(const VipSceneModel & other)
{
	d_data = other.d_data;
	return *this;
}

bool VipSceneModel::operator!=(const VipSceneModel & other) const
{
	return d_data != other.d_data;
}

bool VipSceneModel::operator==(const VipSceneModel & other) const
{
	return d_data == other.d_data;
}

void VipSceneModel::clear()
{
	for (QMap<QString, QList<VipShape> >::const_iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
		Q_EMIT shapeSignals()->groupRemoved(it.key());

	d_data->shapes.clear();
	Q_EMIT shapeSignals()->sceneModelChanged(*this);
}

VipSceneModel VipSceneModel::copy() const
{
	VipSceneModel model;
	for (QMap<QString, QList<VipShape> >::const_iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		QList<VipShape> & out = model.d_data->shapes[it.key()];
		const QList<VipShape> & in = it.value();

		for (int i = 0; i < in.size(); ++i)
		{
			out.append(in[i].copy());
			out.back().d_data->parent = model.d_data;
		}
	}
	model.setAttributes(attributes());
	return model;
}

void VipSceneModel::transform(const QTransform & tr)
{
	shapeSignals()->blockSignals(true);
	for (QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		QList<VipShape> & in = it.value();
		for (int i = 0; i < in.size(); ++i) {
			in[i].transform(tr);
		}
	}
	shapeSignals()->blockSignals(false);
	Q_EMIT shapeSignals()->sceneModelChanged(*this);
}

VipSceneModel & VipSceneModel::add(const QString & group, const VipShape & shape)
{
	VipShape sh(shape);

	//remove from a previous VipSceneModel
	if (sh.parent())
		sh.parent().remove(sh);

	QList<VipShape> & shapes = d_data->shapes[group];
	if (shapes.isEmpty())
		Q_EMIT shapeSignals()->groupAdded(group);

	if (shapes.indexOf(sh) < 0)
	{
		int index;
		sh.d_data->id = findId(shapes, sh.id(), index);
		sh.d_data->parent = d_data;
		sh.d_data->group = group;
		shapes.insert(index, sh);

		Q_EMIT shapeSignals()->sceneModelChanged(*this);
	}
	return *this;
}

VipSceneModel & VipSceneModel::add(const QString & group, const QList<VipShape> & shapes)
{
	bool empty_group = d_data->shapes.find(group) == d_data->shapes.end();
	shapeSignals()->blockSignals(true);
	for (int i = 0; i < shapes.size(); ++i)
		this->add(group, shapes[i]);
	shapeSignals()->blockSignals(false);

	if (empty_group && shapes.size())
		Q_EMIT shapeSignals()->groupAdded(group);
	Q_EMIT shapeSignals()->sceneModelChanged(*this);
	return *this;
}

VipSceneModel & VipSceneModel::add(const QList<VipShape> & shapes)
{
	QSet<QString> new_groups;

	shapeSignals()->blockSignals(true);
	for (int i = 0; i < shapes.size(); ++i)
	{
		if (d_data->shapes.find(shapes[i].group()) == d_data->shapes.end())
			new_groups.insert(shapes[i].group());
		this->add(shapes[i]);
	}
	shapeSignals()->blockSignals(false);

	for (QSet<QString>::const_iterator it = new_groups.begin(); it != new_groups.end(); ++it)
		Q_EMIT shapeSignals()->groupAdded(*it);

	Q_EMIT shapeSignals()->sceneModelChanged(*this);
	return *this;
}

bool VipSceneModel::add(const VipShape & shape, int id)
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(shape.group());
	if (it != d_data->shapes.end() ) {
		if(it.value().indexOf(shape) >= 0)
			//already there
			return VipShape(shape).setId(id);
	}

	VipShape sh = find(shape.group(), id);
	if (sh.isNull()) {
		d_data->shapes[shape.group()].append(shape);
		VipShape(shape).setId(id);
		if(d_data->shapes[shape.group()].size() == 1)
			Q_EMIT shapeSignals()->groupAdded(shape.group()); //new group
		Q_EMIT shapeSignals()->sceneModelChanged(*this);
		return true;
	}
	return false;

}

VipSceneModel & VipSceneModel::add(const VipShape & shape)
{
	return add(shape.group(), shape);
}

VipSceneModel & VipSceneModel::reset(const VipSceneModel & other)
{
	if (other == *this)
		return *this;

	const QList<QString> keys = d_data->shapes.keys();
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	QSet<QString> prev_groups = keys.toSet();
#else
	QSet<QString> prev_groups(keys.begin(),keys.end());//(keys.begin(),keys.end());
#endif
	d_data->shapes = other.d_data->shapes;
	//clear other
	VipSceneModel model(other);
	model.d_data->shapes.clear();

	//set the shape parent
	for (QMap<QString, QList<VipShape> >::const_iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		const QList<VipShape> shs = it.value();
		for (int i = 0; i < shs.size(); ++i) {
			VipShape tmp(shs[i]);
			tmp.d_data->parent = d_data;
		}
	}

	const QList<QString> keys2 = d_data->shapes.keys();
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	QSet<QString> cur_groups = keys2.toSet();
#else
	QSet<QString> cur_groups(keys2.begin(),keys2.end());
#endif

	QSet<QString> added = QSet<QString>(cur_groups).subtract(prev_groups);
	QSet<QString> removed = QSet<QString>(prev_groups).subtract(cur_groups);

	//emit groupRemoved() for other VipSceneModel which is cleared
	for (QSet<QString>::const_iterator it = prev_groups.begin(); it != prev_groups.end(); ++it) {
		Q_EMIT other.shapeSignals()->groupRemoved(*it);
	}
	Q_EMIT other.shapeSignals()->sceneModelChanged(other);

	//emit groupRemoved() for this scene model
	for (QSet<QString>::const_iterator it = removed.begin(); it != removed.end(); ++it) {
		Q_EMIT shapeSignals()->groupRemoved(*it);
	}
	//emit groupAdded() for this scene model
	for (QSet<QString>::const_iterator it = added.begin(); it != added.end(); ++it) {
		Q_EMIT shapeSignals()->groupAdded(*it);
	}

	Q_EMIT shapeSignals()->sceneModelChanged(*this);
	return *this;
}

VipSceneModel & VipSceneModel::add(const VipSceneModel & other)
{
	if (other == *this)
		return *this;

	QSet<QString> new_groups;
	QStringList groups = other.groups();
	for (int i = 0; i < groups.size(); ++i)
	{
		const QString group = groups[i];

		if (d_data->shapes.find(group) == d_data->shapes.end())
			new_groups.insert(group);

		QList<VipShape> & this_shapes = d_data->shapes[group];
		const QList<VipShape> other_shapes = other.shapes(group);

		for (int s = 0; s<other_shapes.size(); ++s)
		{
			VipShape sh = other_shapes[s];
			int index;
			sh.d_data->id = findId(this_shapes, sh.id(), index);
			sh.d_data->parent = d_data;
			sh.d_data->group = group;
			this_shapes.insert(index, sh);
		}
	}

	for (QSet<QString>::const_iterator it = new_groups.begin(); it != new_groups.end(); ++it)
		Q_EMIT shapeSignals()->groupAdded(*it);

	Q_EMIT shapeSignals()->sceneModelChanged(*this);

	VipSceneModel model(other);
	model.d_data->shapes.clear();
	for (int i = 0; i < groups.size(); ++i)
		Q_EMIT model.shapeSignals()->groupRemoved(groups[i]);

	Q_EMIT model.shapeSignals()->sceneModelChanged(model);

	return *this;
}

VipSceneModel & VipSceneModel::remove(const VipShape & shape)
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(shape.group());
	if (it != d_data->shapes.end()) {
		if (it.value().removeOne(shape))
		{
			shape.d_data->parent = QWeakPointer<PrivateData>();
			Q_EMIT shapeSignals()->sceneModelChanged(*this);
		}

		if (it.value().isEmpty()) {
			d_data->shapes.erase(it);
			Q_EMIT shapeSignals()->groupRemoved(shape.group());
		}
	}
	return *this;
}

VipSceneModel & VipSceneModel::remove(const VipShapeList & shapes)
{
	if (shapes.isEmpty())
		return *this;
	QSet<QString> groups;
	for (int i = 0; i < shapes.size(); ++i) {
		QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(shapes[i].group());
		if (it.value().removeOne(shapes[i]))
		{
			shapes[i].d_data->parent = QWeakPointer<PrivateData>();
			if (it.value().isEmpty()) {
				groups.insert(it.key());
				d_data->shapes.erase(it);
			}
		}
	}

	Q_EMIT shapeSignals()->sceneModelChanged(*this);
	for(QSet<QString>::iterator it = groups.begin(); it != groups.end(); ++it)
		Q_EMIT shapeSignals()->groupRemoved(*it);
	return *this;
}

VipSceneModel & VipSceneModel::removeGroup(const QString & group)
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	if (it != d_data->shapes.end())
	{
		d_data->shapes.erase(it);

		Q_EMIT shapeSignals()->groupRemoved(group);
		Q_EMIT shapeSignals()->sceneModelChanged(*this);
	}
	return *this;
}

bool VipSceneModel::hasGroup(const QString & group)
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	return (it != d_data->shapes.end());
}

int VipSceneModel::shapeCount(const QString & group) const
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	if (it != d_data->shapes.end())
		return it.value().size();
	return 0;
}

int VipSceneModel::shapeCount() const
{
	int count = 0;
	for (QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		count += it.value().size();
	}
	return count;
}

int VipSceneModel::groupCount() const
{
	return d_data->shapes.size();
}

QStringList VipSceneModel::groups() const
{
	return d_data->shapes.keys();
}

int VipSceneModel::indexOf(const QString & group, const VipShape & sh) const
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	if (it != d_data->shapes.end())
		return it.value().indexOf(sh);
	return -1;
}

VipShape VipSceneModel::at(const QString & group, int index) const
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	if (it != d_data->shapes.end())
		return it.value()[index];
	return VipShape();
}

VipShape VipSceneModel::find(const QString & group, int id) const
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	if (it != d_data->shapes.end())
	{
		const QList<VipShape> shapes = it.value();
		for (int i = 0; i < shapes.size(); ++i)
			if (shapes[i].id() == id)
				return shapes[i];
	}
	return VipShape();
}

VipShape VipSceneModel::find(const QString & path) const
{
	QStringList lst = path.split(":");
	if (lst.size() != 2)
		return VipShape();

	return find(lst[0], lst[1].toInt());
}

QList<VipShape> VipSceneModel::shapes(const QString & group) const
{
	QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.find(group);
	if (it != d_data->shapes.end())
		return it.value();
	return QList<VipShape>();
}

QList<VipShape> VipSceneModel::shapes() const
{
	QList<VipShape> res;
	for (QMap<QString, QList<VipShape> >::iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		res << it.value();
	}
	return res;
}

QMap<QString, QList<VipShape> > VipSceneModel::groupShapes() const
{
	return d_data->shapes;
}

//QList<VipShape> VipSceneModel::selectedShapes() const
// {
// QList<VipShape> res;
// for(QMap<QString, QList<VipShape> >::const_iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
// {
// const QList<VipShape> & in = it.value();
// for(int i=0; i < in.size(); ++i)
// if(in[i].isSelected())
// res.append(in[i]);
// }
// return res;
// }

QPainterPath VipSceneModel::shape() const
{
	QPainterPath res;
	for (QMap<QString, QList<VipShape> >::const_iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		const QList<VipShape> & in = it.value();
		for (int i = 0; i < in.size(); ++i)
		{
			res |= in[i].shape();
		}
	}
	return res;
}

QRectF VipSceneModel::boundingRect() const
{
	QRectF res;
	for (QMap<QString, QList<VipShape> >::const_iterator it = d_data->shapes.begin(); it != d_data->shapes.end(); ++it)
	{
		const QList<VipShape> & in = it.value();
		for (int i = 0; i < in.size(); ++i)
		{
			res |= in[i].boundingRect();
		}
	}
	return res;
}

VipShapeSignals * VipSceneModel::shapeSignals() const
{
	return d_data->shapeSignals;
}

void VipSceneModel::setAttributes(const QVariantMap & attrs)
{
	d_data->attributes = attrs;
}
void VipSceneModel::setAttribute(const QString & name, const QVariant & value)
{
	d_data->attributes[name] = value;
}
QVariantMap VipSceneModel::attributes() const
{
	return d_data->attributes;
}
QVariant VipSceneModel::attribute(const QString & name) const
{
	return d_data->attributes[name];
}
bool VipSceneModel::hasAttribute(const QString & name) const
{
	return d_data->attributes.find(name) != d_data->attributes.end();
}
QStringList VipSceneModel::mergeAttributes(const QVariantMap & attrs)
{
	QStringList res;
	{
		for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
		{
			QVariantMap::const_iterator found = d_data->attributes.find(it.key());
			if (found == d_data->attributes.end() || it.value() != found.value())
			{
				d_data->attributes[it.key()] = it.value();
				res << it.key();
			}
		}
	}
	return res;
}



VipSceneModel VipShape::parent() const
{
	if (QSharedPointer<VipSceneModel::PrivateData> data = d_data->parent.lock())
		return VipSceneModel(data);
	return VipSceneModel(QSharedPointer<VipSceneModel::PrivateData>());
}


VipShapeSignals::VipShapeSignals()
{

}
VipShapeSignals::~VipShapeSignals()
{

}

VipSceneModel VipShapeSignals::sceneModel() const
{
	if (QSharedPointer<VipSceneModel::PrivateData> data = d_data.lock())
		return VipSceneModel(data);
	return VipSceneModel(QSharedPointer<VipSceneModel::PrivateData>());
}


int vipShapeCount()
{
	return -1; //shape_count;
}

int vipSceneModelCount()
{
	return -1;//sm_count;
}




static int reg_QPainterPath()
{
	qRegisterMetaTypeStreamOperators<QPainterPath>("QPainterPath");
	return 0;
}

static int _reg_QPainterPath = reg_QPainterPath();
