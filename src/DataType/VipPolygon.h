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

#ifndef VIP_POLYGON_H
#define VIP_POLYGON_H

#include <set>

#include "VipNDArray.h"

/// @brief Close Component Labelling algorithm
template<class T, class U>
int vipLabelImage(const VipNDArrayType<T>& input, VipNDArrayType<U>& output, T background, bool connectivity_8 = false, int* relabel = nullptr)
{
	QVector<int> buffer;
	if (!relabel) {
		buffer.resize(input.size());
		relabel = buffer.data();
	}

	const int size = input.size();
	for (int i = 0; i < size; ++i)
		relabel[i] = i;

	output.fill(0);
	int h = input.shape(0);
	int w = input.shape(1);
	int next_label = 1;

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const T value = input(vipVector(y, x));

			// ignore background value
			if (value == background)
				continue;

			// check neighbors

			U label = 0;

			// check left
			if (x > 0 && input(vipVector(y, x - 1)) == value) {
				label = output(vipVector(y, x - 1));
			}

			// check top
			if (y > 0 && input(vipVector(y - 1, x)) == value) {
				if (const U other_label = output(vipVector(y - 1, x))) {
					if (label != other_label && label) {
						// if we already found a left neighbor with a different label, tells to relabel
						// always keep the smallest label
						if (label > other_label) {
							relabel[label] = relabel[other_label];
							label = other_label;
						}
						else
							relabel[other_label] = relabel[label];
					}
					else
						label = other_label;
				}
			}

			if (connectivity_8) {
				// check top left
				if (y > 0 && x > 0 && input(vipVector(y - 1, x - 1)) == value) {
					if (const U other_label = output(vipVector(y - 1, x - 1))) {
						if (label != other_label && label) {
							if (label > other_label) {
								relabel[label] = relabel[other_label];
								label = other_label;
							}
							else
								relabel[other_label] = relabel[label];
						}
						else
							label = other_label;
					}
				}

				// check top right
				if (y > 0 && x + 1 < w && input(vipVector(y - 1, x + 1)) == value) {
					if (const U other_label = output(vipVector(y - 1, x + 1))) {
						if (label != other_label && label) {
							if (label > other_label) {
								relabel[label] = relabel[other_label];
								label = other_label;
							}
							else
								relabel[other_label] = relabel[label];
						}
						else
							label = other_label;
					}
				}
			}

			if (!label)
				label = next_label++;

			output(vipVector(y, x)) = label;
		}
	}

	// at this point, we could relable and we would have isolated all components.
	// however, their labels would not necessarly be consecutive.
	// so we need to modify the relabel vector to have consecutive ids

	// just find the labels that don't move: they are the remaining ones
	QVector<int> final_labels(next_label);
	for (int i = 0; i < next_label; ++i)
		final_labels[i] = i;

	int label_count = 0;
	for (int i = 0; i < next_label; ++i) {
		if (relabel[i] == i && i != 0)
			final_labels[i] = ++label_count;
	}

	// second pass: relabel output
	U* out = output.ptr();

#pragma omp parallel for
	for (int i = 0; i < size; ++i) {
		// follow all indirections
		U value = out[i];
		while (value != relabel[value])
			value = relabel[value];

		out[i] = final_labels[value];
	}

	return label_count;
}

/// @brief Tells if given polygon is a rectangle, and stores it in rect (if not nullptr)
VIP_DATA_TYPE_EXPORT bool vipIsRect(const QPolygon& p, QRect* rect = nullptr);
/// @brief Tells if given polygon is a rectangle, and stores it in rect (if not nullptr)
VIP_DATA_TYPE_EXPORT bool vipIsRect(const QPolygonF& p, QRectF* rect = nullptr);

/// @brief Tells if given polygon is a rectangle, and stores it in rect (if not nullptr)
template<class Point>
bool vipIsRect(const QVector<Point>& poly, QRectF* rect = nullptr)
{

	if (poly.size() == 4 || poly.size() == 5) {
		std::set<double> x, y;
		for (int i = 0; i < poly.size(); ++i) {
			x.insert(poly[i].x());
			y.insert(poly[i].y());
		}
		if (x.size() == 2 && y.size() == 2) {
			if ((poly[0].x() != poly[2].x()) && (poly[0].y() != poly[2].y()) && (poly[1].x() != poly[3].x()) && (poly[1].y() != poly[3].y())) {
				if (rect) {
					QPointF p1(*x.begin(), *y.begin());
					QPointF p2(*(++x.begin()), *(++y.begin()));
					*rect = QRectF(p1, p2).normalized();
				}
				return true;
			}
		}
	}
	return false;
}

/// Remove points from polygon that do not change the overall shape (basically points within a straigth line).
VIP_DATA_TYPE_EXPORT QPolygonF vipSimplifyPolygon(const QPolygonF& polygon);
VIP_DATA_TYPE_EXPORT QPolygon vipSimplifyPolygon(const QPolygon& polygon);

/// Simplify given polygon using Ramer-Douglas-Peucker algorithm (see https://github.com/prakol16/rdp-expansion-only for more details).
/// @param  epsilon: epsilon value used to simplify the polygon by the Ramer-Douglas-Peucker algorithm.
VIP_DATA_TYPE_EXPORT QPolygonF vipRDPSimplifyPolygon(const QPolygonF& polygon, double epsilon);

/// Simplify given polygon using Ramer-Douglas-Peucker algorithm.
/// @param  max_points: unlinke vipRDPSimplifyPolygon, this function takes a maximum number of points as input.
VIP_DATA_TYPE_EXPORT QPolygonF vipRDPSimplifyPolygon2(const QPolygonF& polygon, int max_points);

/// This is an overload function.
/// Returns the minimum distance between a point and a segment.
VIP_DATA_TYPE_EXPORT double vipDistanceToSegment(double x, double y, double x1, double y1, double x2, double y2);
/// Returns the minimum distance between a point and a segment.
VIP_DATA_TYPE_EXPORT double vipDistanceToSegment(const QPointF& pt, const QLineF& segment);

/// Extract bounding polygon for given mask.
/// The mask is an image where foreground pixels have a value of \a foreground.
/// The surrounding polygon is simplified using #vipRDPSimplifyPolygon if \a epsilon > 0.
///
/// If the image contains several closed regions with given foreground value, the algorithm only process the region containing the pixel \a pt.
///
/// If no point is providen, or if given point is inside the background, only the first encountered region (from the top left corner) is extracted.
VIP_DATA_TYPE_EXPORT QPolygonF vipExtractMaskPolygon(const VipNDArray& ar, double foreground, double epsilon = 0, const QPoint& pt = QPoint(-1, -1));

/// Interpolate 2 polygons based on the advance parameter [0,1].
/// If advance == 0, p1 is returned as is, and if advance ==1 p2 is returned as is.
///
/// The interpolated polygon is guaranteed to have a maximum size of (p1.size()+p2.size() -2)
VIP_DATA_TYPE_EXPORT QPolygonF vipInterpolatePolygons(const QPolygonF& p1, const QPolygonF& p2, double advance);

/// Reorder polygon based on its new starting point (>=0).
/// The output polygon keeps the original size.
VIP_DATA_TYPE_EXPORT QPolygonF vipReorderPolygon(const QPolygonF& p, int new_start);

/// Tells if given polygon's points are given in clockwise order.
VIP_DATA_TYPE_EXPORT bool vipIsClockwise(const QPolygonF& poly);

/// Compute polygon area using the Shoelace Algorithm
VIP_DATA_TYPE_EXPORT double vipPolygonArea(const QPolygonF& poly);

/// Compute polygon area using rasterization
VIP_DATA_TYPE_EXPORT int vipPolygonAreaRasterize(const QPolygonF& poly);

/// Returns the polygon centroid point
VIP_DATA_TYPE_EXPORT QPointF vipPolygonCentroid(const QPolygonF& poly);

/// Reverse given polygon
VIP_DATA_TYPE_EXPORT QPolygonF vipReversePolygon(const QPolygonF& poly);

/// Set the polygon orientation (clockwise or anti-clockwise).
/// This function reverses the polygon if its orientation is not the right one.
VIP_DATA_TYPE_EXPORT QPolygonF vipSetPolygonOrientation(const QPolygonF& poly, bool clockwise);

/// Close the polygon if necessary (last point == first point)
VIP_DATA_TYPE_EXPORT QPolygonF vipClosePolygon(const QPolygonF& poly);

/// Open the polygon if necessary (last point != first point)
VIP_DATA_TYPE_EXPORT QPolygonF vipOpenPolygon(const QPolygonF& poly);

/// Remove consecutive duplicated points
VIP_DATA_TYPE_EXPORT QPolygonF vipRemoveConsecutiveDuplicates(const QPolygonF& poly);

/// Returns the convex hull polygon of given points.
/// The result is in anti-clockwise order, and not necessarly closed.
/// This algorithm performs in O(n*ln(n)).
VIP_DATA_TYPE_EXPORT QPolygonF vipConvexHull(const QPolygonF& poly);

// Return True if the polynomial defined by the sequence of 2D
// points is not concave: points are valid (maybe duplicated), interior angles are strictly between zero and a straight
// angle, and the polygon does not intersect itself.
//
// NOTES:  Algorithm: the signed changes of the direction angles
//	from one side to the next side must be all positive or
//	all negative (or null), and their sum must equal plus-or-minus
//	one full turn (2 pi radians).
VIP_DATA_TYPE_EXPORT bool vipIsNonConcave(const QPolygonF& poly);

/// Oriented rect structure, as returned by vipMinimumAreaBBox
struct VipOrientedRect
{
	QPolygonF boundingPoints;
	QPolygonF hullPoints;
	QPointF center;
	// smaller box side
	double width;
	// larger box side
	double height;
	// angle between smaller box side and X axis in radians,
	// positive value means box orientation from bottom right to top left,
	// negative value means opposite
	double widthAngle;
	// angle between larger box side and X axis in radians
	// positive value means box orientation from bottom left to top right,
	// negative value means opposite
	double heightAngle;

	VipOrientedRect(const QPolygonF& bp = QPolygonF(), const QPolygonF& hp = QPolygonF(), const QPointF& c = QPointF(), double w = 0, double h = 0, double wa = 0, double ha = 0)
	  : boundingPoints(bp)
	  , hullPoints(hp)
	  , center(c)
	  , width(w)
	  , height(h)
	  , widthAngle(wa)
	  , heightAngle(ha)
	{
	}
};

/// Returns the minimum area oriented bounding box around a set of points.
///
/// This function is based the convex hull of given points.
/// Set check_convex to false if input polygon is already a convex one.
VIP_DATA_TYPE_EXPORT VipOrientedRect vipMinimumAreaBBox(const QPolygonF& poly, bool check_convex = true);

#endif