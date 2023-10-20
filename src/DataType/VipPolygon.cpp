#include "VipPolygon.h"
#include "VipMatrix22.h"


QPolygon vipSimplifyPolygon(const QPolygon& polygon)
{
	if (polygon.size() < 3)
		return polygon;

	QPolygon res;
	res.push_back(polygon.front());

	double angle = 0;
	for (int i = 1; i < polygon.size() - 1; ++i) {
		if (polygon[i] == polygon[i - 1])
			continue;
		angle = QLineF(polygon[i - 1], polygon[i]).angle();
		double new_angle = 0;
		//QPoint pt1 = polygon[i];
		//QPoint pt2 = polygon[i + 1];
		while (i < polygon.size() - 1 && qFuzzyCompare(new_angle = QLineF(polygon[i], polygon[i + 1]).angle(), angle))
			++i;
		if (i < polygon.size()) {
			res.push_back(polygon[i]);
			angle = new_angle;
		}
	}

	if (polygon.front() == polygon.back()) {
		if (res.back() != polygon.back() && !qFuzzyCompare(QLineF(polygon[polygon.size() - 2], polygon[polygon.size() - 1]).angle(), angle))
			res.push_back(polygon.back());
	}
	else {
		const QPoint& pt1 = polygon[polygon.size() - 1];
		const QPoint& pt2 = polygon.front();
		if (res.back() != polygon.back() && !qFuzzyCompare(QLineF(pt1, pt2).angle(), angle))
			res.push_back(polygon.back());
	}

	return res;
}
QPolygonF vipSimplifyPolygon(const QPolygonF& polygon)
{
	if (polygon.size() < 3)
		return polygon;

	QPolygonF res;
	res.push_back(polygon.front());

	double angle = 0;
	for (int i = 1; i < polygon.size()-1; ++i) {
		if (polygon[i] == polygon[i - 1])
			continue;
		angle = QLineF(polygon[i - 1], polygon[i]).angle();
		double new_angle=0;
		//QPointF pt1 = polygon[i];
		//QPointF pt2 = polygon[i + 1];
		while (i < polygon.size()-1 && qFuzzyCompare(new_angle = QLineF(polygon[i], polygon[i + 1]).angle(), angle))
			++i;
		if (i < polygon.size()) {
			res.push_back(polygon[i]);
			angle = new_angle;
		}
	}

	if (polygon.front() == polygon.back()) {
		if (res.back() != polygon.back() && !qFuzzyCompare(QLineF(polygon[polygon.size() - 2], polygon[polygon.size() - 1]).angle(), angle))
			res.push_back(polygon.back());
	}
	else {
		const QPointF & pt1 = polygon[polygon.size() - 1];
		const QPointF & pt2 = polygon.front();
		if (res.back() != polygon.back() && !qFuzzyCompare(QLineF(pt1,pt2).angle(), angle))
			res.push_back(polygon.back());
	}

	return res;
}

bool vipIsRect(const QPolygon& _p, QRect* rect )
{
	if (_p.size() < 4) {
		if (rect) *rect = QRect();
		return false;
	}

	QPolygon p = vipSimplifyPolygon(_p);

	int posx[2] = { p.front().x(),0 };
	int posy[2] = { p.front().y(),0 };
	int posxcount = 1;
	int posycount = 1;

	for (int i = 1; i < p.size(); ++i) {
		const QPoint& pt = p[i];

		if (posxcount == 1) {
			if (pt.x() != posx[0]) {
				posx[1] = pt.x();
				++posxcount;
			}
		}
		else { //posxcount == 2
			if (pt.x() != posx[0] && pt.x() != posx[1]) {
				if (rect) *rect = QRect();
				return false;
			}
		}

		if (posycount == 1) {
			if (pt.y() != posy[0]) {
				posy[1] = pt.y();
				++posycount;
			}
		}
		else { //posycount == 2
			if (pt.y() != posy[0] && pt.y() != posy[1]) {
				if (rect) *rect = QRect();
				return false;
			}
		}

	}

	if (posxcount == 1)
		posx[1] = posx[0];
	else {
		if (posx[0] > posx[1])
			std::swap(posx[0], posx[1]);
	}
	if (posycount == 1)
		posy[1] = posy[0];
	else {
		if (posy[0] > posy[1])
			std::swap(posy[0], posy[1]);
	}

	if (rect) *rect = QRect(posx[0], posy[0], posx[1] - posx[0], posy[1] - posy[0]);
	return true;
}
bool vipIsRect(const QPolygonF& _p, QRectF* rect)
{
	if (_p.size() < 4) {
		if (rect) *rect = QRectF();
		return false;
	}

	QPolygonF p = vipSimplifyPolygon(_p);

	double posx[2] = { p.front().x(),0 };
	double posy[2] = { p.front().y(),0 };
	int posxcount = 1;
	int posycount = 1;

	for (int i = 1; i < p.size(); ++i) {
		const QPointF& pt = p[i];

		if (posxcount == 1) {
			if (pt.x() != posx[0]) {
				posx[1] = pt.x();
				++posxcount;
			}
		}
		else { //posxcount == 2
			if (pt.x() != posx[0] && pt.x() != posx[1]) {
				if (rect) *rect = QRectF();
				return false;
			}
		}

		if (posycount == 1) {
			if (pt.y() != posy[0]) {
				posy[1] = pt.y();
				++posycount;
			}
		}
		else { //posycount == 2
			if (pt.y() != posy[0] && pt.y() != posy[1]) {
				if (rect) *rect = QRectF();
				return false;
			}
		}

	}

	if (posxcount == 1)
		posx[1] = posx[0];
	else {
		if (posx[0] > posx[1])
			std::swap(posx[0], posx[1]);
	}
	if (posycount == 1)
		posy[1] = posy[0];
	else {
		if (posy[0] > posy[1])
			std::swap(posy[0], posy[1]);
	}

	if (rect) *rect = QRectF(posx[0], posy[0], posx[1] - posx[0], posy[1] - posy[0]);
	return true;
}

static inline QVector<double> line_dists(const QPointF* points, int size, const QPointF& start, const QPointF& end)
{
	//Returns the signed distances of each point to start and end
	if (start == end) {
		QVector<double> res;
		for (int i = 0; i < size; ++i) {
			QPointF diff = points[i] - start;
			res.push_back(std::sqrt(diff.x() * diff.x() + diff.y() * diff.y()));
		}
		return res;
	}

	QPointF vec = start - end;
	double vec_norm = std::sqrt(vec.x() * vec.x() + vec.y() * vec.y());

	QVector<double> res;
	for (int i = 0; i < size; ++i) {
		QPointF diff = start - points[i];
		//cross product
		double cross = vec.x() * diff.y() - vec.y() * diff.x();
		res.push_back(cross / vec_norm);
	}
	return res;
}

static inline QLineF glue(const QLineF& seg1, const QLineF& seg2)
{
	// Glues two segments together
	// In other words, given segments Aand B which have endpoints
	// that are close together, computes a "glue point" that both segments
	// can be extended to in order to intersect.
	// Assumes seg1 and seg2 are arrays containing two points each,
	// and that if seg1 = [a, b], it can be extended in the direction of b,
	// and if seg2 = [c, d], it can be extended in the direction of c.

	QPointF x1 = seg1.p1();
	QPointF dir1 = seg1.p2() - x1;
	QPointF x2 = seg2.p1();
	QPointF dir2 = seg2.p2() - x2;

	//Solve for the vector [t, -s]
	VipMatrix22 mat(dir1.x(), dir2.x(), dir1.y(), dir2.y());
	double _t_s[2];
	double* t_s = _t_s;
	bool ok;
	mat = mat.inverted(&ok);
	if (ok) {
		QPointF diff = x2 - x1;
		t_s[0] = mat.m11 * diff.x() + mat.m12 * diff.y();
		t_s[1] = mat.m21 * diff.x() + mat.m22 * diff.y();
		// Recall that we can't make a segment go in a backwards direction
		// So we must have t >= 0 and s <= 1. However, since we solved for[t, -s]
		// we want that t_s[0] >= 0 and t_s[1] >= -1. If this fails, set t_s to None
		// Also, don't allow segments to more than double
		if (!((0 <= t_s[0] && t_s[0] <= 2) && (-1 <= t_s[1] && t_s[1] <= 1)))
			t_s = nullptr;
	}
	else // Singular matrix i.e.parallel
		t_s = nullptr;
	if (!t_s)
		// Just connect them with a line
		return QLineF(seg1.p2(), seg2.p1());
	else {
		QPointF res(x1 + dir1 * t_s[0]);
		return QLineF(res, res);
	}
}


static inline int max_index_abs(const QVector<double>& v)
{
	double max = std::abs(v.front());
	int index = 0;
	for (int i = 1; i < v.size(); ++i) {
		double tmp = std::abs(v[i]);
		if (tmp > max) {
			max = tmp;
			index = i;
		}
	}
	return index;
}
static inline int argmin(const QVector<double>& v)
{
	double min = (v.front());
	int index = 0;
	for (int i = 1; i < v.size(); ++i) {
		double tmp = (v[i]);
		if (tmp < min) {
			min = tmp;
			index = i;
		}
	}
	return index;
}

//static inline QPolygonF& operator<<(QPolygonF& res, const QPolygonF& v2)
// {
// size_t s = res.size();
// res.resize(res.size() + v2.size());
// std::copy(v2.begin(), v2.end(), res.begin() + s);
// return res;
// }
// static inline QPolygonF& operator<<(QPolygonF& res, const QPointF& v2)
// {
// res.push_back(v2);
// return res;
// }
static inline QPolygonF mid(const QPolygonF& v, size_t start, size_t n = 0)
{
	if (start >= (size_t)v.size())
		return QPolygonF();
	if (start + n > (size_t)v.size() || n == 0)
		n = v.size() - start;
	QPolygonF res((int)n);
	std::copy(v.begin() + start, v.begin() + (start + n), res.begin());
	return res;
	//return v.mid(start, n);
}


static inline QPolygonF __rdp(const QVector <QPointF>& points, double epsilon)
{
	// Computes expansion only rdp assuming a clockwise orientation
	QPointF start = points.front();
	QPointF end = points.back();
	QVector<double>	dists = line_dists(points.data(), (int)points.size(), start, end);

	// First compute the largest point away from the line just like the ordinary RDP
	int index = max_index_abs(dists);
	double dmax = std::abs(dists[index]);
	QPolygonF result;

	if (dmax > epsilon) {
		QPolygonF result1 = __rdp(mid(points, 0, index + 1), epsilon);
		QPolygonF result2 = __rdp(mid(points, index), epsilon);
		QLineF gl = glue(QLineF(result1[result1.size() - 2], result1.back()), QLineF(result2[0], result2[1]));
		result = mid(result1, 0, result1.size() - 1);
		if (gl.p1() == gl.p2())
			result << (gl.p1());
		else
			result << gl.p1() << gl.p2();
		result << mid(result2, 1);
	}
	else {
		// All points are within epsilon of the line
		// We take the point furthest* outside* the boundary(i.e.with negative distance)
		//and shift the line segment parallel to itself to intersect that point
		QPointF new_start = start, new_end = end;
		QPointF diff = (end - start);
		double vec_x = diff.x(), vec_y = diff.y();
		double norm = std::sqrt(vec_x * vec_x + vec_y * vec_y);
		if (norm != 0) {
			QPointF vec_rot90(-vec_y / norm, vec_x / norm);
			// TODO -- verify that this works: can optimize so that if dists[index] < 0, no need to search again, we have index_min = index and dmin = -dmax
			int index_min = argmin(dists);
			double dmin = -dists[index_min];
			if (dmin > 0) {
				vec_rot90 *= dmin;
				new_start += vec_rot90;
				new_end += vec_rot90;
			}
		}
		result << new_start << new_end;
	}
	return result;
}



inline QPolygonF rdp(const QPolygonF& points, double epsilon = 0)
{
	//see https://github.com/prakol16/rdp-expansion-only
	return __rdp(points, epsilon);
}

inline QPolygonF rdp_closed(const QPolygonF& points, double epsilon = 0)
{
	//see https://github.com/prakol16/rdp-expansion-only
	//qint64 st = QDateTime::currentMSecsSinceEpoch();
	QPolygonF res;

	//Ensure that expansion only rdp returns a closed loop at the end
	QPolygonF new_points = rdp(points, epsilon);
	QLineF	glue_pts = glue(QLineF(new_points[new_points.size() - 2], new_points.back()), QLineF(new_points[0], new_points[1]));
	if (glue_pts.p1() == glue_pts.p2()) {
		res << glue_pts.p1() << mid(new_points, 1, new_points.size() - 2) << glue_pts.p1();
	}
	else {
		res= new_points << new_points[0];//return np.vstack((new_points, [new_points[0]]))
	}

	//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//vip_debug("vipRDPSimplifyPolygon: %i ms\n", (int)el);
	return res;
}

QPolygonF vipRDPSimplifyPolygon(const QPolygonF& polygon, double epsilon)
{
	QPolygonF res= rdp_closed(polygon, epsilon);
	return res;
}












static inline QPoint rotateClockwise45(const QPoint& pt)
{
	int x = (int)std::round(0.70710678118654757 * pt.x() + -0.70710678118654757 * pt.y());
	int y = (int)std::round(0.70710678118654757 * pt.x() + 0.70710678118654757 * pt.y());
	return QPoint(x, y);
}

template< class T>
static bool checkPoint(int x, int y, const VipNDArrayType<T> & ar, T mask_value)
{
	//check bounds
	if (x < 0 || y < 0 || x >= ar.shape(1) || y >= ar.shape(0)) return false;
	if (ar(y,x) == mask_value) {
		//this is a foreground pixel, check that it has at least one background pixel neighbor or a border
		if (x == 0 || y == 0 || x == ar.shape(1) - 1 || y == ar.shape(0) - 1 ||
			ar( y , x - 1) != mask_value || ar( y , x + 1) != mask_value || ar((y - 1), x) != mask_value || ar((y + 1), x) != mask_value)
			return true;
	}
	return false;
}

template< class T>
static inline QPoint nextPoint(const QPoint& prev, const QPoint& center, const VipNDArrayType<T>& ar, T mask_value)
{
	QPoint diff = prev - center;

	//start from prev +1
	for (int i = 0; i < 8; ++i) {
		diff = rotateClockwise45(diff);
		const QPoint pt = diff + center;
		if (checkPoint(pt.x(), pt.y(), ar, mask_value))
			return pt;
	}
	//no valid neigbor found: single point
	return center;

}

//static inline QPolygon toPolygon(const QPolygonF& p)
// {
// QPolygon res(p.size());
// for (size_t i = 0; i < p.size(); ++i) {
// res[i] = QPoint((int)std::round(p[i].x()), (int)std::round(p[i].y()));
// }
// return res;
// }

template< class T>
static QPointF toPointF(const QPoint& pt, const VipNDArrayType<T>& ar, T mask_value)
{
	QPointF res(pt.x(), pt.y());
	if (pt.x() == ar.shape(1) - 1 || ar(pt.y(), pt.x() + 1) != mask_value)
		res.rx() += 0.5;
	if (pt.y() == ar.shape(0) - 1 || ar(pt.y()+1, pt.x() ) != mask_value)
		res.ry() += 0.5;
	return res;
}

template< class T>
static void startPoint(QPoint pt, QPolygonF& out, const VipNDArrayType<T>& ar, T mask_value, double epsilon = 0)
{
	//qint64 st = QDateTime::currentMSecsSinceEpoch();

	out.push_back(pt);

	//consider the previous to be the left point
	QPoint prev = pt - QPoint(1, 0);

	while (true) {
		//find next point
		QPoint tmp = nextPoint(prev, pt,ar, mask_value);
		prev = pt;
		pt = tmp;

		//the next point is the first encountered one: close the polygon and stop
		if (pt == out.front()) {
			out.push_back(pt);
			break;
		}

		out.push_back(pt);
	}

	if (out.size() == 2) {
		//case single point: close the polygon (3 times the same point)
		out.push_back(out.front());
		return;
	}

	//filter polygon first time by removing all pixels inside vertical/horizontal lines
	if (out.size() > 3) {
		QPolygonF res;
		res.push_back(out.front());
		for (int i = 1; i < out.size() - 1; ++i) {
			QPointF p = out[i];
			if ((p.x() == out[i - 1].x() && p.x() == out[i + 1].x()) || (p.y() == out[i - 1].y() && p.y() == out[i + 1].y()))
				;//skip point
			else
				res.push_back(out[i]);
		}
		res.push_back(out.back());
		out = res;
	}

	//qint64 el1 = QDateTime::currentMSecsSinceEpoch() - st;

	//RDP algorithm
	if (out.size() > 9 && epsilon > 0) //arbitrary value
		out = rdp_closed(out, epsilon);

	//qint64 el2 = QDateTime::currentMSecsSinceEpoch() - st;
	//vip_debug("p: %i, el1: %i, el2: %i\n", out.size(), (int)el1, (int)el2);
	//return;
}

template< class T>
static QPolygonF filterPolygon(const QPolygonF & poly, const VipNDArrayType<T>& ar, T foreground)
{
	QPolygonF res(poly.size());
	for (int i = 0; i < poly.size(); ++i)
		res[i] = toPointF(poly[i].toPoint(), ar, foreground);
	return res;
}


template< class T>
QPolygonF extractMaskPolygon(const VipNDArrayType<T> & ar, double foreground, double epsilon = 0, const QPoint& pt = QPoint(-1, -1))
{
	T _foreground = (T)foreground;
	if (pt == QPoint(-1, -1) || ar(pt.y(), pt.x()) != _foreground) {
		//search from top left
		for (int y = 0; y < ar.shape(0); ++y)
			for (int x = 0; x < ar.shape(1); ++x) {
				T pix = ar(y,x);
				if (pix == _foreground) {
					//found pixel value!
					QPolygonF poly;
					startPoint(QPoint(x, y), poly, ar, _foreground, epsilon);
					return filterPolygon(poly,ar, _foreground);// toPolygon(poly);
				}
			}
		return QPolygon();
	}
	else {
		//find top border
		for (int y = pt.y()-1; y >= 0; --y) {
			T pix = ar(y, pt.x());
			if (pix != _foreground) {
				//found pixel value!
				QPolygonF poly;
				startPoint(QPoint(pt.x(), y+1), poly, ar, _foreground, epsilon);
				return filterPolygon(poly, ar, _foreground);// toPolygon(poly);
			}
		}
		QPolygonF poly;
		startPoint(pt, poly, ar, _foreground, epsilon);
		return filterPolygon(poly, ar, _foreground);//toPolygon(poly);
	}
}


QPolygonF vipExtractMaskPolygon(const VipNDArray& ar, double foreground, double epsilon , const QPoint& pt)
{
	if (ar.isEmpty() || ar.shapeCount() != 2)
		return QPolygon();

	if (ar.dataType() == QMetaType::Bool) return extractMaskPolygon(VipNDArrayType<bool>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::Char) return extractMaskPolygon(VipNDArrayType<char>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::UChar) return extractMaskPolygon(VipNDArrayType<unsigned char>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::SChar) return extractMaskPolygon(VipNDArrayType<signed char>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::Short) return extractMaskPolygon(VipNDArrayType<short>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::UShort) return extractMaskPolygon(VipNDArrayType<unsigned short>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::Int) return extractMaskPolygon(VipNDArrayType<int>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::UInt) return extractMaskPolygon(VipNDArrayType<unsigned int>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::LongLong) return extractMaskPolygon(VipNDArrayType<qint64>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::ULongLong) return extractMaskPolygon(VipNDArrayType<quint64>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::Long) return extractMaskPolygon(VipNDArrayType<long>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::ULong) return extractMaskPolygon(VipNDArrayType<unsigned long>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::Float) return extractMaskPolygon(VipNDArrayType<float>(ar), foreground, epsilon, pt);
	else if (ar.dataType() == QMetaType::Double) return extractMaskPolygon(VipNDArrayType<double>(ar), foreground, epsilon, pt);
	else return QPolygon();
}


//template< class T>
// inline std::map<int, QPolygon > extractPolygons(const Img<T>& img, double epsilon = 0, T background = 0)
// {
// std::map<int, QPolygon > res;
//
// for (int y = 0; y < img.height; ++y)
// for (int x = 0; x < img.width; ++x) {
//	T pix = img[x + y * img.width];
//	if (pix != background && res.find(pix) == res.end()) {
//		//found new pixel value != background
//		QPolygonF poly;
//		startPoint(QPoint(x, y), poly, img.width, img.height, img.data, pix, epsilon);
//		res[pix] = toPolygon(poly);
//	}
// }
// return res;
// }





//static double prevTime(const double* iter)
// {
// return *(iter - 1);
// }

/// \internal
/// Extract the time vector for several QVector<double>.
/// Returns a growing time vector containing all different time values of given QVector<double> samples.
inline QVector<double> extractTimes(const QVector<double> & v1, const QVector<double> & v2)
{
	QVector<double> res;
	res.reserve(v1.size());

	const double* vectors[2] = { v1.data(),v2.data() };

	size_t vector_count = 2;
	size_t vector_sizes[2] = { (size_t)v1.size(),(size_t)v2.size() };

	//create our time iterators
	std::vector<const double*> iters, ends;
	for (size_t i = 0; i < vector_count; ++i) {
		//search for nan time value
		const double* v = vectors[i];
		const size_t size = vector_sizes[i];
		size_t p = 0;
		for (; p < size; ++p) {
			if (std::isnan(v[p]))
				break;
		}
		if (p != size) {
			//split the vector in 2 vectors
			iters.push_back(vectors[i]);
			ends.push_back(vectors[i]);
			ends.back() += p;
			iters.push_back(vectors[i]);
			iters.back() += p + 1;
			ends.push_back(vectors[i] + vector_sizes[i]);
		}
		else {
			iters.push_back(vectors[i]);
			ends.push_back(vectors[i] + vector_sizes[i]);
		}
	}

	while (iters.size()) {
		//find minimum time among all time vectors
		double min_time = *iters.front();
		for (size_t i = 1; i < iters.size(); ++i)
			min_time = std::min(min_time, *iters[i]);

		//increment each iterator equal to min_time
		for (size_t i = 0; i < iters.size(); ++i) {
			if (iters[i] != ends[i])
				if (*iters[i] == min_time)
					if (++iters[i] == ends[i]) {
						iters.erase(iters.begin() + i);
						ends.erase(ends.begin() + i);
						--i;
					}
		}
		res.push_back(min_time);
	}

	return res;
}

static QPolygonF resampleSignal(const double* sample_x, const QPointF* sample_y, int size, const double* times, int times_size)
{
	QPolygonF res(times_size);

	const double* iters_x = sample_x;
	const double* ends_x = sample_x + size;
	const QPointF* iters_y = sample_y;
	//const double* ends_y = sample_y + size;

	for (int t = 0; t < times_size; ++t)
	{
		const double time = times[t];

		//we already reached the last sample value
		if (iters_x == ends_x) {
			res[t] = sample_y[size - 1];
			continue;
		}

		const double samp_x = *iters_x;
		const QPointF samp_y = *iters_y;

		//same time: just add the sample
		if (time == samp_x) {
			res[t] = samp_y;
			++iters_x;
			++iters_y;
		}
		//we are before the sample
		else if (time < samp_x) {
			//sample starts after
			if (iters_x == sample_x) {
				res[t] = samp_y;
			}
			//in between 2 values
			else {
				const double* prev_x = iters_x - 1;
				const QPointF* prev_y = iters_y - 1;

				double factor = (double)(time - *prev_x) / (double)(samp_x - *prev_x);
				res[t] = samp_y * factor + (1 - factor) * (*prev_y);


			}
		}
		else {
			//we are after the sample, increment until this is not the case
			while (iters_x != ends_x && *iters_x < time) {
				++iters_x;
				++iters_y;
			}
			if (iters_x != ends_x) {
				if (*iters_x == time) {
					res[t] = *iters_y;
				}
				else {
					const double* prev_x = iters_x - 1;
					const QPointF* prev_y = iters_y - 1;

					//interpolation
					double factor = (double)(time - *prev_x) / (double)(*iters_x - *prev_x);
					res[t] = *iters_y * factor + (1 - factor) * *prev_y;
				}
			}
			else {
				//reach sample end
				res[t] = sample_y[size - 1];
			}
		}
	}
	return res;
}


QPolygonF vipReorderPolygon(const QPolygonF& p, int new_start)
{
	QPolygonF res(p.size());
	int pos = new_start;
	for (int i = 0; i < p.size(); ++i, ++pos) {
		if (pos >= p.size())
			pos = 0;
		res[i] = p[pos];
	}
	return res;
}

bool vipIsClockwise(const QPolygonF& poly)
{
	double signedArea = 0.0;
	for (int i = 0; i < poly.size(); ++i) {
		QPointF point = poly[i];
		QPointF next = i == poly.size() - 1 ? poly[0] : poly[i + 1];
		signedArea += (point.x() * next.y() - next.x() * point.y());
	}
	return signedArea < 0;
}

static QPolygonF InterpolatePolygons(const QPolygonF& p1, const QPolygonF& p2, double advance)
{
	if (advance >= 1)
		return p2;
	else if (advance <= 0)
		return p1;
	else if (p1.size() == 0 || p2.size() == 0) {
		if (advance < 0.5)
			return p1;
		else
			return p2;
	}

	if (p1.isEmpty())
		return QPolygonF();
	if (p2.isEmpty())
		return QPolygonF();

	//check for rectangles
	QRectF r1, r2;
	if (vipIsRect(p1, &r1) && vipIsRect(p2, &r2)) {
		//interpolate rectangles
		QPointF topLeft = r1.topLeft() * (1. - advance) + r2.topLeft() * advance;
		QPointF topRight = r1.topRight() * (1. - advance) + r2.topRight() * advance;
		QPointF bottomRight = r1.bottomRight() * (1. - advance) + r2.bottomRight() * advance;
		QPointF bottomLeft = r1.bottomLeft() * (1. - advance) + r2.bottomLeft() * advance;
		return QPolygonF() << topLeft << topRight << bottomRight << bottomLeft;
	}

	QPolygonF poly1; poly1.push_back(p1.first());
	QPolygonF poly2; poly2.push_back(p2.first());

	//remove duplicates
	for (int i = 1; i < p1.size(); ++i) {
		if (p1[i] != p1[i - 1])
			poly1.push_back(p1[i]);
	}
	for (int i = 1; i < p2.size(); ++i) {
		if (p2[i] != p2[i - 1])
			poly2.push_back(p2[i]);
	}


	if (poly1.size() == 1) {
		QPointF pt1 = p1.first();
		//interpolate
		for (int i = 0; i < poly2.size(); ++i) {
			poly2[i] = pt1 * (1 - advance) + poly2[i] * advance;
		}
		return poly2;
	}
	else if(poly2.size() == 1) {
		QPointF pt2 = p2.first();
		//interpolate
		for (int i = 0; i < poly1.size(); ++i) {
			poly1[i] = poly1[i] * (1 - advance) + pt2 * advance;
		}
		return poly1;
	}

	if (vipIsClockwise(poly1) != vipIsClockwise(poly2)) {
		//revsere one of the 2
		for (int i = 0; i < poly1.size() / 2; ++i) {
			std::swap(poly1[i], poly1[poly1.size() - 1 - i]);
		}
	}

	//move p2's center on the center of p1
	QPointF c1 = p1.boundingRect().center();
	QPointF c2 = p2.boundingRect().center();
	QPointF diff = c2 - c1;
	for (int i = 0; i < p2.size(); ++i)
		poly2[i] -= diff;


	//find 2 closest points, which will become the new start point of each polygon
	int id1 = 0, id2 = 0;
	double len = std::numeric_limits<double>::max();
	for (int i = 0; i < poly1.size(); ++i)
		for (int j = 0; j < poly2.size(); ++j) {
			double _d = QLineF(poly1[i], poly2[j]).length();
			if (_d < len) {
				len = _d;
				id1 = i;
				id2 = j;
			}
		}

	//reorder
	poly1 = vipReorderPolygon(poly1, id1);
	poly2 = vipReorderPolygon(poly2, id2);

	//compute total length
	double tot_len1 = 0;
	double tot_len2 = 0;
	for (int i = 1; i < poly1.size(); ++i)
		tot_len1 += QLineF(poly1[i], poly1[i - 1]).length();
	for (int i = 1; i < poly2.size(); ++i)
		tot_len2 += QLineF(poly2[i], poly2[i - 1]).length();


	QVector<double> len1, len2;
	len1.push_back(0);
	len2.push_back(0);

	//compute cumulated relative length
	double cum1 = 0;
	for (int i = 1; i < poly1.size(); ++i) {
		cum1 += QLineF(poly1[i], poly1[i - 1]).length();
		len1.push_back(cum1 / tot_len1);
	}
	double cum2 = 0;
	for (int i = 1; i < poly2.size(); ++i) {
		cum2 += QLineF(poly2[i], poly2[i - 1]).length();
		len2.push_back(cum2 / tot_len2);
	}

	QVector<double> new_length = extractTimes(len1,len2);
	poly1 = resampleSignal(len1.constData(), poly1.constData(), poly1.size(), new_length.constData(), new_length.size());
	poly2 = resampleSignal(len2.constData(), poly2.constData(), poly2.size(), new_length.constData(), new_length.size());

	//move back p2 to its original center
	for (int i = 0; i < poly2.size(); ++i)
		poly2[i] += diff;

	//interpolate
	for (int i = 0; i < poly1.size(); ++i){
		poly1[i] = poly1[i] * (1 - advance) + poly2[i] * advance;
	}
	return poly1;
}


QPolygonF vipInterpolatePolygons(const QPolygonF& p1, const QPolygonF& p2, double advance)
{
	//qint64 st = QDateTime::currentMSecsSinceEpoch();
	QPolygonF res = InterpolatePolygons(p1, p2, advance);
	//qint64 el = QDateTime::currentMSecsSinceEpoch() - st;
	//vip_debug("vipInterpolatePolygons: %i ms\n", (int)el);
	return res;
}



double vipDistanceToSegment(double x, double y, double x1, double y1, double x2, double y2)
{

	double A = x - x1;
	double B = y - y1;
	double C = x2 - x1;
	double D = y2 - y1;

	double dot = A * C + B * D;
	double len_sq = C * C + D * D;
	double param = -1;
	if (len_sq != 0) //in case of 0 length line
		param = dot / len_sq;

	double xx, yy;

	if (param < 0) {
		xx = x1;
		yy = y1;
	}
	else if (param > 1) {
		xx = x2;
		yy = y2;
	}
	else {
		xx = x1 + param * C;
		yy = y1 + param * D;
	}

	double dx = x - xx;
	double dy = y - yy;
	return std::sqrt(dx * dx + dy * dy);
}

double vipDistanceToSegment(const QPointF& pt, const QLineF& segment)
{
	return vipDistanceToSegment(pt.x(), pt.y(), segment.p1().x(), segment.p1().y(), segment.p2().x(), segment.p2().y());
}


double vipPolygonArea(const QPolygonF& poly)
{
	//see https://www.101computing.net/the-shoelace-algorithm/#:~:text=The%20shoelace%20formula%20or%20shoelace,polygon%20to%20find%20its%20area.
	const QPolygonF p = vipSetPolygonOrientation(poly, false);
	double sum1 = 0;
	double sum2 = 0;

	for (int i = 0; i < poly.size() - 1; ++i) {
		sum1 += poly[i].x() * poly[i + 1].y();
		sum2 += poly[i].y() * poly[i + 1].x();
	}

	//Add xn.y1
	sum1 += poly.last().x() * poly[0].y();
	//Add x1.yn
	sum2 += poly[0].x() * poly.last().y();

	double area = std::abs(sum1 - sum2) / 2;
	return area;
}


#include "VipSceneModel.h"

int vipPolygonAreaRasterize(const QPolygonF& poly)
{
	try {
		VipShape sh(poly);
		return sh.fillPixels().size();
	}
	catch(const std::exception & ) {
		//possible bad alloc
		return 0;
	}
}

QPointF vipPolygonCentroid(const QPolygonF& poly)
{
	//see file:///C:/Users/VM213788/Downloads/shape-descriptors.pdf
	double A = vipPolygonArea(poly);
	double gx = 0;
	double gy = 0;
	for (int i = 0; i < poly.size() - 1; ++i) {

		gx += (poly[i].x() + poly[i + 1].x()) * (poly[i].x() * poly[i + 1].y() - poly[i + 1].x() * poly[i].y());
		gy += (poly[i].y() + poly[i + 1].y()) * (poly[i].x() * poly[i + 1].y() - poly[i + 1].x() * poly[i].y());
	}
	gx /= 6 * A;
	gy /= 6 * A;
	return QPointF(gx, gy);
}

QPolygonF vipReversePolygon(const QPolygonF& poly)
{
	if (poly.isEmpty()) return poly;
	QPolygonF res(poly.size());
	std::copy(poly.begin(), poly.end(), res.rbegin());
	return res;
}

QPolygonF vipSetPolygonOrientation(const QPolygonF& poly, bool clockwise)
{
	if (poly.isEmpty()) return poly;
	if (vipIsClockwise(poly)) {
		if (clockwise)
			return poly;
		else
			return vipReversePolygon(poly);
	}
	else {
		if (clockwise)
			return vipReversePolygon(poly);
		else
			return poly;
	}
}

QPolygonF vipClosePolygon(const QPolygonF& poly)
{
	if (poly.isEmpty()) return poly;
	if (poly.last() == poly.first())
		return poly;
	QPolygonF res = poly;
	res.push_back(poly.first());
	return res;
}

QPolygonF vipOpenPolygon(const QPolygonF& poly)
{
	if (poly.isEmpty()) return poly;
	if (poly.last() != poly.first())
		return poly;
	QPolygonF res(poly.size() - 1);
	std::copy(poly.begin(), poly.begin() + poly.size() - 1, res.begin());
	return res;
}


QPolygonF vipRemoveConsecutiveDuplicates(const QPolygonF& poly)
{
	QPolygonF res;
	res.reserve(poly.size());
	res.push_back(poly.first());
	for (int i = 1; i < poly.size() ; ++i) {
		if (poly[i] != poly[i - 1]) {
			res.push_back(poly[i]);
		}
	}
	return res;
}


static bool toleranceCompare(double x, double y)
{
	double maxXYOne = std::max({ 1.0, std::fabs(x) , std::fabs(y) });
	return std::fabs(x - y) <= std::numeric_limits<double>::epsilon() * maxXYOne;
}

QPolygonF vipConvexHull(const QPolygonF& poly)
{
	if (poly.size() < 3)
		return poly;


	QPolygonF p;

	//find bottom most point, remove duplicated points and unclose polygon
	int bottom_i = 0;
	double bottom = poly.first().y();
	p.push_back(poly.first());
	for (int i = 1; i < poly.size() - 1; ++i) {
		if (poly[i] != poly[i - 1]) {
			p.push_back(poly[i]);
			if (p.last().y() > bottom) {
				bottom = p.last().y();
				bottom_i = p.size() - 1;
			}
		}
	}
	if (poly.last() != poly.first() && poly.last() != poly[poly.size() - 2]){
		p.push_back(poly.last());
		if (p.last().y() > bottom) {
			bottom = p.last().y();
			bottom_i = p.size() - 1;
		}
	}

	//build list of points to inspect
	QPolygonF to_inspect = p;
	//std::list<QPointF> to_inspect;
	//for (int i = 0; i < p.size(); ++i) to_inspect.push_back(p[i]);

	//add first point (bottom most)
	QPolygonF res;
	res.push_back(p[bottom_i]);

	QLineF line(p[bottom_i] - QPointF(1, 0), p[bottom_i]);
	//start from bottom most
	//int count = 0;
	while (to_inspect.size()) {

		double angle = 361;
		int index = 0;
		//look for the point with smallest angle
		for (int i = 0; i < to_inspect.size(); ++i) {
			if (to_inspect[i] != res.last()) {
				double a = line.angleTo(QLineF(line.p2(), to_inspect[i]));
				if (a < angle) {
					angle = a;
					index = i;
				}
			}
		}

		line = QLineF(line.p2(), to_inspect[index]);

		if (toleranceCompare(angle,0) && res.size() > 1)
			res.last() = to_inspect[index]; //avoid several points on the same line
		else
			res.push_back(to_inspect[index]);
		if (res.last() == res.first())
			break;
		to_inspect.removeAt(index);
	}

	//close polygon
	//if (res.first() != res.last())
	// res.push_back(res.first());

	//vip_debug("inspected: %i\n", count);
	return res;
}


bool vipIsNonConcave(const QPolygonF& poly)
{
	//https://stackoverflow.com/questions/471962/how-do-i-efficiently-determine-if-a-polygon-is-convex-non-convex-or-complex

// Check for too few points
	if (poly.size() < 4)
		return true;

	// Get starting information
	QPointF old = poly[poly.size() - 2];
	QPointF new_ = poly.last();
	double new_direction = std::atan2(new_.y() - old.y(), new_.x() - old.x());
	double angle_sum = 0.0;
	double orientation = 0;
	// Check each point (the side ending there, its angle) and accum. angles
	for (int i = 0; i < poly.size(); ++i) {
		const QPointF newpoint = poly[i];
		if (newpoint == new_)
			continue;
		// Update point coordinates and side directions, check side length
		double old_direction = new_direction;
		old = new_;
		new_ = newpoint;
		new_direction = std::atan2(new_.y() - old.y(), new_.x() - old.x());
		if (old.x() == new_.x() && old.y() == new_.y())
			return false;  // repeated consecutive points
		// Calculate & check the normalized direction-change angle
		double angle = new_direction - old_direction;
		if (angle <= -M_PI)
			angle += M_2PI;  // make it in half-open interval (-Pi, Pi]
		else if (angle > M_PI)
			angle -= M_2PI;
		if (orientation == 0) {  // if first time through loop, initialize orientation
			if (angle == 0.0)
				continue;//return false;
			orientation = angle > 0. ? 1.0 : -1.0;
		}
		else {  // if other time through loop, check orientation is stable
			if (orientation * angle <= 0.0)  // not both pos. or both neg.
				return false;
		}
		// Accumulate the direction-change angle
		angle_sum += angle;
	}
	// Check that the total number of full turns is plus-or-minus 1
	return std::abs(std::round(angle_sum / M_2PI)) == 1;
}




static double angleToXAxis(const QLineF& s)
{
	QPointF delta = s.p1() - s.p2();
	return -std::atan(delta.y() / delta.x());
}

static QPointF rotateToXAxis(const QPointF& p, double angle)
{
	double s = std::sin(angle);
	double c = std::cos(angle);
	double newX = p.x() * c - p.y() * s;
	double newY = p.x() * s + p.y() * c;
	return QPointF(newX, newY );
}

static double area(const QRectF& r) { return r.width() * r.height(); }


VipOrientedRect vipMinimumAreaBBox(const QPolygonF& poly, bool check_convex)
{
	//see https://github.com/schmidt9/MinimalBoundingBox/blob/main/MinimalBoundingBox/MinimalBoundingBox.cpp

	const QPolygonF hullPoints = check_convex ? (vipIsNonConcave(poly) ? poly : vipConvexHull(poly)) : (poly);
	if (hullPoints.size() <= 1) {
		return VipOrientedRect();
	}

	QRectF minBox;
	double minAngle = 0;

	for (int i = 0; i < hullPoints.size(); ++i) {
		auto nextIndex = i + 1;

		auto current = hullPoints[i];
		auto next = hullPoints[nextIndex % hullPoints.size()];

		auto segment = QLineF(current, next);

		// min / max points

		auto top = DBL_MAX ;
		auto bottom = DBL_MAX * -1;
		auto left = DBL_MAX;
		auto right = DBL_MAX * -1;

		// get angle of segment to x axis

		auto angle = angleToXAxis(segment);
		//vip_debug("POINT (%f;%f) DEG %f\n", current.x(), current.y(), angle * (180 / M_PI));

		// rotate every point and get min and max values for each direction

		for (auto& p : hullPoints) {
			auto rotatedPoint = rotateToXAxis(p, angle);

			top = std::min(top, rotatedPoint.y());
			bottom = std::max(bottom, rotatedPoint.y());

			left = std::min(left, rotatedPoint.x());
			right = std::max(right, rotatedPoint.x());
		}

		// create axis aligned bounding box

		auto box = QRectF(left, top, right - left, bottom - top);

		if (minBox.isEmpty() || area(minBox) > area(box)) {
			minBox = box;
			minAngle = angle;
		}
	}

	// get bounding box dimensions using axis aligned box,
	// larger side is considered as height

	QPolygonF minBoxPoints;// = minBox.getPoints();
	minBoxPoints << minBox.topLeft() << minBox.topRight() << minBox.bottomRight() << minBox.bottomLeft();

	auto v1 = minBoxPoints[0] - minBoxPoints[1];
	auto v2 = minBoxPoints[1] - minBoxPoints[2];
	auto absX = std::abs(v1.x());
	auto absY = std::abs(v2.y());
	auto width = std::min(absX, absY);
	auto height = std::max(absX, absY);

	// rotate axis aligned box back and get center

	auto sumX = 0.0;
	auto sumY = 0.0;

	for (auto& point : minBoxPoints) {
		point = rotateToXAxis(point, -minAngle);
		sumX += point.x();
		sumY += point.y();
	}

	auto center = QPointF(sumX / 4, sumY / 4);

	// get angles

	auto heightPoint1 = (absX > absY)
		? minBoxPoints[0]
		: minBoxPoints[1];
	auto heightPoint2 = (absX > absY)
		? minBoxPoints[1]
		: minBoxPoints[2];

	auto heightAngle = angleToXAxis(QLineF(heightPoint1, heightPoint2));
	auto widthAngle = (heightAngle > 0) ? heightAngle - M_PI_2 : heightAngle + M_PI_2;

	// get alignment

	//auto tolerance = alignmentTolerance * (M_PI / 180);
	//auto isAligned = (widthAngle <= tolerance && widthAngle >= -tolerance);

	return VipOrientedRect(minBoxPoints, hullPoints, center, width,height,widthAngle,heightAngle);
}









struct VipPolygonDescriptors
{
	double area;
	QPointF centroid;
	double Ix;	// second moment of area about the origin (xx)
	double Iy;	// second moment of area about the origin (yy)
	double Ixy;	// second moment of area about the origin (xy)
	double orientation; //in degree counterclockwise from x axis
	double L1; //length of major axis
	double L2; //length of minor axis
	double eccentricity; // L1/L2 (1 for a perfect circle)
};

VipPolygonDescriptors vipPolygonDescriptors(const QPolygonF& poly)
{
	//see https://en.wikipedia.org/wiki/Second_moment_of_area
	//and https://www.ae.msstate.edu/vlsm/shape/area_moments_of_inertia/papmi.htm
	VipPolygonDescriptors res;

	const QPolygonF p = vipClosePolygon(vipSetPolygonOrientation(poly, false));
	double sum1 = 0;
	double sum2 = 0;
	double gx = 0;
	double gy = 0;

	for (int i = 0; i < p.size() - 1; ++i) {

		double xy1 = p[i].x() * p[i + 1].y();
		double yx1 = p[i].y() * p[i + 1].x();
		double x1y = p[i + 1].x() * p[i].y();

		sum1 += xy1;
		sum2 += yx1;

		gx += (p[i].x() + p[i + 1].x()) * (xy1 - x1y);
		gy += (p[i].y() + p[i + 1].y()) * (xy1 - x1y);
	}

	//Add xn.y1
	sum1 += p.last().x() * p[0].y();
	//Add x1.yn
	sum2 += p[0].x() * p.last().y();

	res.area = std::abs(sum1 - sum2) / 2;
	res.centroid.setX(gx / (6 * res.area));
	res.centroid.setY(gy / (6 * res.area));


	double Ix = 0, Iy = 0, Ixy = 0;

	for (int i = 0; i < p.size() -1; ++i) {
		double xy1 = p[i].x() * p[i + 1].y();
		double x1y = p[i + 1].x() * p[i].y();
		double factor = xy1 - x1y;

		Ix += (factor) * (p[i].y() * p[i].y() + p[i].y() * p[i + 1].y() + p[i + 1].y() * p[i + 1].y());
		Iy += (factor) * (p[i].x() * p[i].x() + p[i].x() * p[i + 1].x() + p[i + 1].x() * p[i + 1].x());
		Ixy += (factor) * (xy1 + 2*p[i].x() * p[i].y() + 2 * p[i+1].x() * p[i+1].y() + x1y);
	}
	res.Ix = Ix / 12;
	res.Iy = Iy / 12;
	res.Ixy = Ixy / 24;
	res.orientation = 0.5 * std::atan(2 * res.Ixy / (res.Iy - res.Ix)) * (180 / M_PI);


	//double a1 = (res.Iy + res.Ix) / 2;
	// double a2 = std::sqrt(4 * res.Ixy * res.Ixy + (res.Iy - res.Ix) * (res.Iy - res.Ix)) / 2;
//
	// double	minor_axis = a1 - a2;
	// double	major_axis = a1 + a2;
//
	// double common = std::sqrt((res.Ix - res.Iy) * (res.Ix - res.Iy) + 4 * (res.Ixy * res.Ixy));
	// double	majorAxisLength = 2 * std::sqrt(2) * std::sqrt(res.Ix + res.Iy + common);
	// double	minorAxisLength = 2 * std::sqrt(2) * std::sqrt(res.Ix + res.Iy - common);
//
	// double eccentricity = std::sqrt(1 - minor_axis / major_axis);
	// double orientation = (180 / M_PI) * std::atan2(2 * res.Ixy, (res.Iy - res.Ix)) / 2;
//
//
//
	// double cxx = 0, cyy = 0, cxy = 0, cyx = 0;
	// for (int i = 0; i < p.size() ; ++i) {
	// cxx += (p[i].x() - res.centroid.x()) * (p[i].x() - res.centroid.x());
	// cyy += (p[i].y() - res.centroid.y()) * (p[i].y() - res.centroid.y());
	// cxy += (p[i].x() - res.centroid.x()) * (p[i].y() - res.centroid.y());
	// cyx += (p[i].x() - res.centroid.x()) * (p[i].y() - res.centroid.y());
	// }
	// cxx /= p.size();
	// cyy /= p.size();
	// cxy /= p.size();
	// cyx /= p.size();
//
	// double l1 = 0.5 * (cxx + cyy + std::sqrt((cxx + cyy) * (cxx + cyy)  - 4 * (cxx*cyy - cxy*cxy)));
	// double l2 = 0.5 * (cxx + cyy - std::sqrt((cxx + cyy) * (cxx + cyy) - 4 * (cxx * cyy - cxy * cxy)));
	return res;
}






static double pow2(double x) {
	return x * x;
}

static double distSquared(const QPointF& p1, const QPointF& p2) {
	return pow2(p1.x() - p2.x()) + pow2(p1.y() - p2.y());
}

static double getRatio(const QLineF& self, const QPointF& point) {
	double segmentLength = distSquared(self.p1(), self.p2());
	if (segmentLength == 0 )
		return distSquared(point, self.p1());
	return ((point.x() - self.p1().x()) * (self.p2().x() - self.p1().x()) +
		(point.y() - self.p1().y()) * (self.p2().y() - self.p1().y())) / segmentLength;
}

static double distanceToSquared(const QLineF& self, const QPointF& point) {
	double t = getRatio(self, point);

	if (t < 0)
		return distSquared(point, self.p1());
	if (t > 1)
		return distSquared(point, self.p2());

	return distSquared(point, QPointF(
		self.p1().x() + t * (self.p2().x() - self.p1().x()),
		self.p1().y() + t * (self.p2().y() - self.p1().y())));
}

//static double distanceTo(const QLineF& self, const QPointF& point) {
// return std::sqrt(distanceToSquared(self, point));
// }

static void douglasPeucker(int start, int end, const QPolygonF& points, QVector<double>& weights) {
	if (end > start + 1) {
		QLineF line(points[start], points[end]);
		double maxDist = -1;
		int maxDistIndex = 0;

		for (int i = start + 1; i < end; ++i) {
			double dist = distanceToSquared(line, points[i]);
			if (dist > maxDist) {
				maxDist = dist;
				maxDistIndex = i;
			}
		}
		weights[maxDistIndex] = maxDist;

		douglasPeucker(start, maxDistIndex, points, weights);
		douglasPeucker(maxDistIndex, end, points, weights);
	}
}

QPolygonF vipRDPSimplifyPolygon2(const QPolygonF& _points, int max_points)
{
	//see https://gist.github.com/msbarry/9152218

	const QPolygonF points = vipRemoveConsecutiveDuplicates(_points);
	QVector<double> weights(points.size(),0.);

	douglasPeucker(0, points.size() - 1, points, weights);
	weights[0] = std::numeric_limits<double>::infinity();
	weights.last() = std::numeric_limits<double>::infinity();
	QVector<double> weightsDescending = weights;
	std::sort(weightsDescending.begin(), weightsDescending.end());
	//reverse
	//for (int i = 0; i < weightsDescending.size() / 2; ++i)
	//	std::swap(weightsDescending[i], weightsDescending[weightsDescending.size() - i - 1]);

	double maxTolerance = weightsDescending[weightsDescending.size() - max_points]; //max_points - 1];

	QPolygonF res;
	for (int i = 0; i < points.size(); ++i) {
		if (weights[i] >= maxTolerance)
			res.push_back(points[i]);
	}
	return res;
}
