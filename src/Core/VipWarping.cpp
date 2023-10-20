/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Léo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QColor>
#include <QPoint>
#include <QVector>
#include <cmath>
#include <qmutex.h>

#include "VipWarping.h"
#include "p_Clarkson-Delaunay.h"

QVector<int> getDelaunayTriangles(const QVector<QPoint>& pts, int clockwise)
{
	static QMutex mutex;

	// Protect BuildTriangleIndexList which is NOT thread safe
	QMutexLocker lock(&mutex);

	int numTriangleVertices = 0;
	int* triangles = BuildTriangleIndexList(const_cast<QPoint*>(pts.data()), 0, pts.size(), 2, clockwise, &numTriangleVertices);
	if (!triangles)
		return QVector<int>();

	QVector<int> res(numTriangleVertices);
	std::copy(triangles, triangles + numTriangleVertices, res.data());
	free(triangles);
	return res;
}

static bool barycentric(const QPoint& p, const QPoint& p0, const QPoint& p1, const QPoint& p2, double& u, double& v, double& w)
{
	double Area = 0.5 * (-p1.y() * p2.x() + p0.y() * (-p1.x() + p2.x()) + p0.x() * (p1.y() - p2.y()) + p1.x() * p2.y());
	v = 1 / (2 * Area) * (p0.y() * p2.x() - p0.x() * p2.y() + (p2.y() - p0.y()) * p.x() + (p0.x() - p2.x()) * p.y());
	w = 1 / (2 * Area) * (p0.x() * p1.y() - p0.y() * p1.x() + (p0.y() - p1.y()) * p.x() + (p1.x() - p0.x()) * p.y());
	u = 1 - v - w;
	return (u >= 0 && v >= 0 && w >= 0);
}

VipPointVector vipWarping(QVector<QPoint> pts1, QVector<QPoint> pts2, int width, int height)
{
	if (pts1.size() != pts2.size())
		return VipPointVector();

	// add the corners
	QPoint c0(-1, -1);
	pts1 << c0;
	pts2 << c0;

	QPoint c1(width, -1);
	pts1 << c1;
	pts2 << c1;

	QPoint c2(width, height);
	pts1 << c2;
	pts2 << c2;

	QPoint c3(-1, height);
	pts1 << c3;
	pts2 << c3;

	// compute deformation from new to old
	VipPointVector defs(pts1.size());
	for (int i = 0; i < pts1.size(); ++i)
		defs[i] = pts1[i] - pts2[i];

	// compute delaunay triangle for new points
	QVector<int> delaunay = getDelaunayTriangles(pts2, 0);

	// for each point in the new image, compute the coordinate to use inside the old image
	VipPointVector res(width * height);
	int i = 0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x, ++i) {
			// initialize result if we don't find an enclosing triangle
			res[i] = QPointF(x, y);

			double u, v, w;
			// find the enclosing triangle
			for (int c = 0; c < delaunay.size(); c += 3) {
				int p0 = delaunay[c];
				int p1 = delaunay[c + 1];
				int p2 = delaunay[c + 2];
				if (barycentric(QPoint(x, y), pts2[p0], pts2[p1], pts2[p2], u, v, w)) {
					// we found it: compute the src point
					QPointF def0 = defs[p0];
					QPointF def1 = defs[p1];
					QPointF def2 = defs[p2];
					res[i] = QPointF(x + def0.x() * u + def1.x() * v + def2.x() * w, y + def0.y() * u + def1.y() * v + def2.y() * w);

					if (res[i].x() < 0)
						res[i].setX(0);
					if (res[i].y() < 0)
						res[i].setY(0);
					if (res[i].x() >= width)
						res[i].setX(width - 1);
					if (res[i].y() >= height)
						res[i].setY(height - 1);

					break;
				}
			}
		}
	}

	return res;
}

// used float for complex_f, double otherwise
template<class T>
struct select_type
{
	typedef double type;
};
template<>
struct select_type<complex_f>
{
	typedef float type;
};

template<class T, class U>
void applyWarping(const T* src, U* dst, int w, int h, const VipPointVector& warping)
{
	typedef typename select_type<T>::type type;

	int i = 0;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x, ++i) {
			const QPointF src_pt = warping[i];
			const int leftCellEdge = src_pt.x();
			int rightCellEdge = (src_pt.x()) + 1;
			if (rightCellEdge == w)
				rightCellEdge = leftCellEdge;
			const int topCellEdge = src_pt.y();
			int bottomCellEdge = (src_pt.y() + 1);
			if (bottomCellEdge == h)
				bottomCellEdge = topCellEdge;
			const T p1 = src[bottomCellEdge * w + leftCellEdge];
			const T p2 = src[topCellEdge * w + leftCellEdge];
			const T p3 = src[bottomCellEdge * w + rightCellEdge];
			const T p4 = src[topCellEdge * w + rightCellEdge];
			const type u = (src_pt.x() - leftCellEdge);   // / (rightCellEdge - leftCellEdge);
			const type v = (src_pt.y() - bottomCellEdge); // / (topCellEdge - bottomCellEdge);
			U value = (p1 * (1 - v) + p2 * v) * (1 - u) + (p3 * (1 - v) + p4 * v) * u;
			dst[i] = value;
		}
	}
}

template<class T, class U>
void applyWarpingNoInterpolation(const T* src, U* dst, int w, int h, const VipPointVector& warping)
{
	int i = 0;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x, ++i) {
			const QPointF src_pt = warping[i];
			const int leftCellEdge = qRound(src_pt.x());
			const int topCellEdge = qRound(src_pt.y());
			dst[i] = src[topCellEdge * w + leftCellEdge];
		}
	}
}

inline int clipRGB(double v)
{
	return v < 0 ? 0 : (v > 255 ? 255 : (int)v);
}
inline void applyWarpingRGB(const QRgb* src, QRgb* dst, int w, int h, const VipPointVector& warping)
{
	int size = w * h;
#pragma omp parallel for
	for (int i = 0; i < size; ++i) {
		const QPointF src_pt = warping[i];
		int leftCellEdge = src_pt.x();
		if (leftCellEdge < 0)
			leftCellEdge = 0;
		else if (leftCellEdge >= w)
			leftCellEdge = w - 1;
		int rightCellEdge = (src_pt.x()) + 1;
		if (rightCellEdge >= w)
			rightCellEdge = leftCellEdge;
		int topCellEdge = src_pt.y();
		if (topCellEdge < 0)
			topCellEdge = 0;
		else if (topCellEdge >= h)
			topCellEdge = h - 1;
		int bottomCellEdge = (src_pt.y() + 1);
		if (bottomCellEdge >= h)
			bottomCellEdge = topCellEdge;
		const QRgb p1 = src[bottomCellEdge * w + leftCellEdge];
		const QRgb p2 = src[topCellEdge * w + leftCellEdge];
		const QRgb p3 = src[bottomCellEdge * w + rightCellEdge];
		const QRgb p4 = src[topCellEdge * w + rightCellEdge];
		const double u = (src_pt.x() - leftCellEdge);	/// (rightCellEdge - leftCellEdge);
		const double v = (src_pt.y() - bottomCellEdge); /// (topCellEdge - bottomCellEdge);
		int a = clipRGB((qAlpha(p1) * (1 - v) + qAlpha(p2) * v) * (1 - u) + (qAlpha(p3) * (1 - v) + qAlpha(p4) * v) * u);
		int r = clipRGB((qRed(p1) * (1 - v) + qRed(p2) * v) * (1 - u) + (qRed(p3) * (1 - v) + qRed(p4) * v) * u);
		int g = clipRGB((qGreen(p1) * (1 - v) + qGreen(p2) * v) * (1 - u) + (qGreen(p3) * (1 - v) + qGreen(p4) * v) * u);
		int b = clipRGB((qBlue(p1) * (1 - v) + qBlue(p2) * v) * (1 - u) + (qBlue(p3) * (1 - v) + qBlue(p4) * v) * u);

		dst[i] = qRgba(r, g, b, a);
	}
}

inline VipNDArray warpSimpleArray(const VipNDArray& ar, const VipPointVector& warping)
{
	if (warping.size() != ar.size())
		return VipNDArray();

	if (ar.isNumeric()) {
		VipNDArray out = ar;
		switch (ar.dataType()) {
			case QMetaType::Bool:
				applyWarpingNoInterpolation((bool*)ar.data(), (bool*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::SChar:
				applyWarpingNoInterpolation((signed char*)ar.data(), (signed char*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::Char:
				applyWarpingNoInterpolation((char*)ar.data(), (char*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::UChar:
				applyWarpingNoInterpolation((unsigned char*)ar.data(), (unsigned char*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::UShort:
				applyWarpingNoInterpolation((unsigned short*)ar.data(), (unsigned short*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::Short:
				applyWarpingNoInterpolation((short*)ar.data(), (short*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::UInt:
				applyWarpingNoInterpolation((quint32*)ar.data(), (quint32*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::Int:
				applyWarpingNoInterpolation((qint32*)ar.data(), (qint32*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::ULong:
				applyWarpingNoInterpolation((unsigned long*)ar.data(), (unsigned long*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::Long:
				applyWarpingNoInterpolation((long*)ar.data(), (long*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::ULongLong:
				applyWarpingNoInterpolation((quint64*)ar.data(), (quint64*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::LongLong:
				applyWarpingNoInterpolation((qint64*)ar.data(), (qint64*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::Double:
				applyWarping((double*)ar.data(), (double*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
			case QMetaType::Float:
				applyWarping((float*)ar.data(), (float*)out.data(), ar.shape(1), ar.shape(0), warping);
				break;
		}
		return out;
	}
	else if (ar.dataType() == qMetaTypeId<long double>()) {
		VipNDArray out = ar;
		applyWarping((long double*)ar.data(), (long double*)out.data(), ar.shape(1), ar.shape(0), warping);
	}
	else if (ar.isComplex()) {
		VipNDArray out = ar;
		if (ar.dataType() == qMetaTypeId<complex_f>())
			applyWarping((complex_f*)ar.data(), (complex_f*)out.data(), ar.shape(1), ar.shape(0), warping);
		else
			applyWarping((complex_d*)ar.data(), (complex_d*)out.data(), ar.shape(1), ar.shape(0), warping);
		return out;
	}
	else if (vipIsImageArray(ar)) {
		const QImage in = vipToImage(ar);
		QImage out(in.width(), in.height(), QImage::Format_ARGB32);

		const QRgb* in_p = (QRgb*)in.bits();
		QRgb* out_p = (QRgb*)out.bits();

		applyWarpingRGB(in_p, out_p, ar.shape(1), ar.shape(0), warping);
		return vipToArray(out);
	}
	return VipNDArray();
}

inline VipNDArray warpAnyArray(const VipNDArray& ar, const VipPointVector& warping)
{
	if (vipIsMultiNDArray(ar)) {
		VipMultiNDArray multi(ar);
		VipMultiNDArray res;
		const QMap<QString, VipNDArray> arrays = multi.namedArrays();

		for (QMap<QString, VipNDArray>::const_iterator it = arrays.begin(); it != arrays.end(); ++it) {
			VipNDArray tmp = warpAnyArray(it.value(), warping);
			if (!tmp.isEmpty())
				res.addArray(it.key(), tmp);
		}
		return res;
	}
	else {
		return warpSimpleArray(ar, warping);
	}
}

VipWarping::VipWarping(QObject* parent)
  : VipSceneModelBasedProcessing(parent)
{
	outputAt(0)->setData(QVariant::fromValue(VipNDArray()));
}

void VipWarping::apply()
{
	VipAnyData any = inputAt(0)->data();
	VipNDArray ar = any.value<VipNDArray>();

	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("wrong input data", VipProcessingObject::WrongInput);
		return;
	}

	if (!m_warping.size() || m_warping.size() != ar.size()) {
		// setError("wrong warping size");
		outputAt(0)->setData(any);
		return;
	}

	ar = warpAnyArray(ar, m_warping);
	VipAnyData out = create(QVariant::fromValue(ar));
	out.setTime(any.time());
	outputAt(0)->setData(out);
}

#include "VipArchive.h"

VipArchive& operator<<(VipArchive& ar, const VipWarping* tr)
{
	return ar.content("warping", tr->warping());
}

VipArchive& operator>>(VipArchive& ar, VipWarping* tr)
{
	tr->setWarping(ar.read("warping").value<VipPointVector>());
	return ar;
}

static int _registerStreamOperator = vipRegisterArchiveStreamOperators<VipWarping*>();
