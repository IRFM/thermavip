#include <qimage.h>
#include <qpainter.h>
#include <qpainterpath.h>
#include <qregion.h>
#include <qvector.h>

#include "p_fixExtractShapePixels.h"

//#define QT_REGION_DEBUG
//  clip region

struct _QRegionPrivate
{
	int numRects;
	int innerArea;
	QVector<QRect> rects;
	QRect extents;
	QRect innerRect;

	inline _QRegionPrivate()
	  : numRects(0)
	  , innerArea(-1)
	{
	}
	inline _QRegionPrivate(const QRect& r)
	  : numRects(1)
	  , innerArea(r.width() * r.height())
	  , extents(r)
	  , innerRect(r)
	{
	}

	void intersect(const QRect& r);

	// Returns \c true if r is guaranteed to be fully contained in this region.
	// A false return value does not guarantee the opposite.
	inline bool contains(const _QRegionPrivate& r) const { return contains(r.extents); }

	inline bool contains(const QRect& r2) const
	{
		const QRect& r1 = innerRect;
		return r2.left() >= r1.left() && r2.right() <= r1.right() && r2.top() >= r1.top() && r2.bottom() <= r1.bottom();
	}

	// Returns \c true if this region is guaranteed to be fully contained in r.
	inline bool within(const QRect& r1) const
	{
		const QRect& r2 = extents;
		return r2.left() >= r1.left() && r2.right() <= r1.right() && r2.top() >= r1.top() && r2.bottom() <= r1.bottom();
	}

	inline void updateInnerRect(const QRect& rect)
	{
		const int area = rect.width() * rect.height();
		if (area > innerArea) {
			innerArea = area;
			innerRect = rect;
		}
	}

	inline void vectorize()
	{
		if (numRects == 1) {
			if (!rects.size())
				rects.resize(1);
			rects[0] = extents;
		}
	}

	inline void append(const QRect* r);
	void append(const _QRegionPrivate* r);
	void prepend(const QRect* r);
	void prepend(const _QRegionPrivate* r);
	inline bool canAppend(const QRect* r) const;
	inline bool canAppend(const _QRegionPrivate* r) const;
	inline bool canPrepend(const QRect* r) const;
	inline bool canPrepend(const _QRegionPrivate* r) const;

	inline bool mergeFromRight(QRect* left, const QRect* right);
	inline bool mergeFromLeft(QRect* left, const QRect* right);
	inline bool mergeFromBelow(QRect* top, const QRect* bottom, const QRect* nextToTop, const QRect* nextToBottom);
	inline bool mergeFromAbove(QRect* bottom, const QRect* top, const QRect* nextToBottom, const QRect* nextToTop);

#ifdef QT_REGION_DEBUG
	void selfTest() const;
#endif
};

static inline bool isEmptyHelper(const _QRegionPrivate* preg)
{
	return !preg || preg->numRects == 0;
}

static inline bool canMergeFromRight(const QRect* left, const QRect* right)
{
	return (right->top() == left->top() && right->bottom() == left->bottom() && right->left() <= (left->right() + 1));
}

static inline bool canMergeFromLeft(const QRect* right, const QRect* left)
{
	return canMergeFromRight(left, right);
}

bool _QRegionPrivate::mergeFromRight(QRect* left, const QRect* right)
{
	if (canMergeFromRight(left, right)) {
		left->setRight(right->right());
		updateInnerRect(*left);
		return true;
	}
	return false;
}

bool _QRegionPrivate::mergeFromLeft(QRect* right, const QRect* left)
{
	if (canMergeFromLeft(right, left)) {
		right->setLeft(left->left());
		updateInnerRect(*right);
		return true;
	}
	return false;
}

static inline bool canMergeFromBelow(const QRect* top, const QRect* bottom, const QRect* nextToTop, const QRect* nextToBottom)
{
	if (nextToTop && nextToTop->y() == top->y())
		return false;
	if (nextToBottom && nextToBottom->y() == bottom->y())
		return false;

	return ((top->bottom() >= (bottom->top() - 1)) && top->left() == bottom->left() && top->right() == bottom->right());
}

bool _QRegionPrivate::mergeFromBelow(QRect* top, const QRect* bottom, const QRect* nextToTop, const QRect* nextToBottom)
{
	if (canMergeFromBelow(top, bottom, nextToTop, nextToBottom)) {
		top->setBottom(bottom->bottom());
		updateInnerRect(*top);
		return true;
	}
	return false;
}

bool _QRegionPrivate::mergeFromAbove(QRect* bottom, const QRect* top, const QRect* nextToBottom, const QRect* nextToTop)
{
	if (canMergeFromBelow(top, bottom, nextToTop, nextToBottom)) {
		bottom->setTop(top->top());
		updateInnerRect(*bottom);
		return true;
	}
	return false;
}

static inline QRect qt_rect_intersect_normalized(const QRect& r1, const QRect& r2)
{
	QRect r;
	r.setLeft(qMax(r1.left(), r2.left()));
	r.setRight(qMin(r1.right(), r2.right()));
	r.setTop(qMax(r1.top(), r2.top()));
	r.setBottom(qMin(r1.bottom(), r2.bottom()));
	return r;
}

void _QRegionPrivate::intersect(const QRect& rect)
{
	Q_ASSERT(extents.intersects(rect));
	Q_ASSERT(numRects > 1);

#ifdef QT_REGION_DEBUG
	selfTest();
#endif

	const QRect r = rect.normalized();
	extents = QRect();
	innerRect = QRect();
	innerArea = -1;

	QRect* dest = rects.data();
	const QRect* src = dest;
	int n = numRects;
	numRects = 0;
	while (n--) {
		*dest = qt_rect_intersect_normalized(*src++, r);
		if (dest->isEmpty())
			continue;

		if (numRects == 0) {
			extents = *dest;
		}
		else {
			extents.setLeft(qMin(extents.left(), dest->left()));
			// hw: extents.top() will never change after initialization
			// extents.setTop(qMin(extents.top(), dest->top()));
			extents.setRight(qMax(extents.right(), dest->right()));
			extents.setBottom(qMax(extents.bottom(), dest->bottom()));

			const QRect* nextToLast = (numRects > 1 ? dest - 2 : 0);

			// mergeFromBelow inlined and optimized
			if (canMergeFromBelow(dest - 1, dest, nextToLast, 0)) {
				if (!n || src->y() != dest->y() || src->left() > r.right()) {
					QRect* prev = dest - 1;
					prev->setBottom(dest->bottom());
					updateInnerRect(*prev);
					continue;
				}
			}
		}
		updateInnerRect(*dest);
		++dest;
		++numRects;
	}
#ifdef QT_REGION_DEBUG
	selfTest();
#endif
}

void _QRegionPrivate::append(const QRect* r)
{
	Q_ASSERT(!r->isEmpty());

	QRect* myLast = (numRects == 1 ? &extents : rects.data() + (numRects - 1));
	if (mergeFromRight(myLast, r)) {
		if (numRects > 1) {
			const QRect* nextToTop = (numRects > 2 ? myLast - 2 : 0);
			if (mergeFromBelow(myLast - 1, myLast, nextToTop, 0))
				--numRects;
		}
	}
	else if (mergeFromBelow(myLast, r, (numRects > 1 ? myLast - 1 : 0), 0)) {
		// nothing
	}
	else {
		vectorize();
		++numRects;
		updateInnerRect(*r);
		if (rects.size() < numRects)
			rects.resize(numRects);
		rects[numRects - 1] = *r;
	}
	extents.setCoords(qMin(extents.left(), r->left()), qMin(extents.top(), r->top()), qMax(extents.right(), r->right()), qMax(extents.bottom(), r->bottom()));

#ifdef QT_REGION_DEBUG
	selfTest();
#endif
}

void _QRegionPrivate::append(const _QRegionPrivate* r)
{
	Q_ASSERT(!isEmptyHelper(r));

	if (r->numRects == 1) {
		append(&r->extents);
		return;
	}

	vectorize();

	QRect* destRect = rects.data() + numRects;
	const QRect* srcRect = r->rects.constData();
	int numAppend = r->numRects;

	// try merging
	{
		const QRect* rFirst = srcRect;
		QRect* myLast = destRect - 1;
		const QRect* nextToLast = (numRects > 1 ? myLast - 1 : 0);
		if (mergeFromRight(myLast, rFirst)) {
			++srcRect;
			--numAppend;
			const QRect* rNextToFirst = (numAppend > 1 ? rFirst + 2 : 0);
			if (mergeFromBelow(myLast, rFirst + 1, nextToLast, rNextToFirst)) {
				++srcRect;
				--numAppend;
			}
			if (numRects > 1) {
				nextToLast = (numRects > 2 ? myLast - 2 : 0);
				rNextToFirst = (numAppend > 0 ? srcRect : 0);
				if (mergeFromBelow(myLast - 1, myLast, nextToLast, rNextToFirst)) {
					--destRect;
					--numRects;
				}
			}
		}
		else if (mergeFromBelow(myLast, rFirst, nextToLast, rFirst + 1)) {
			++srcRect;
			--numAppend;
		}
	}

	// append rectangles
	if (numAppend > 0) {
		const int newNumRects = numRects + numAppend;
		if (newNumRects > rects.size()) {
			rects.resize(newNumRects);
			destRect = rects.data() + numRects;
		}
		memcpy(destRect, srcRect, numAppend * sizeof(QRect));

		numRects = newNumRects;
	}

	// update inner rectangle
	if (innerArea < r->innerArea) {
		innerArea = r->innerArea;
		innerRect = r->innerRect;
	}

	// update extents
	destRect = &extents;
	srcRect = &r->extents;
	extents.setCoords(qMin(destRect->left(), srcRect->left()), qMin(destRect->top(), srcRect->top()), qMax(destRect->right(), srcRect->right()), qMax(destRect->bottom(), srcRect->bottom()));

#ifdef QT_REGION_DEBUG
	selfTest();
#endif
}

void _QRegionPrivate::prepend(const _QRegionPrivate* r)
{
	Q_ASSERT(!isEmptyHelper(r));

	if (r->numRects == 1) {
		prepend(&r->extents);
		return;
	}

	vectorize();

	int numPrepend = r->numRects;
	int numSkip = 0;

	// try merging
	{
		QRect* myFirst = rects.data();
		const QRect* nextToFirst = (numRects > 1 ? myFirst + 1 : 0);
		const QRect* rLast = r->rects.constData() + r->numRects - 1;
		const QRect* rNextToLast = (r->numRects > 1 ? rLast - 1 : 0);
		if (mergeFromLeft(myFirst, rLast)) {
			--numPrepend;
			--rLast;
			rNextToLast = (numPrepend > 1 ? rLast - 1 : 0);
			if (mergeFromAbove(myFirst, rLast, nextToFirst, rNextToLast)) {
				--numPrepend;
				--rLast;
			}
			if (numRects > 1) {
				nextToFirst = (numRects > 2 ? myFirst + 2 : 0);
				rNextToLast = (numPrepend > 0 ? rLast : 0);
				if (mergeFromAbove(myFirst + 1, myFirst, nextToFirst, rNextToLast)) {
					--numRects;
					++numSkip;
				}
			}
		}
		else if (mergeFromAbove(myFirst, rLast, nextToFirst, rNextToLast)) {
			--numPrepend;
		}
	}

	if (numPrepend > 0) {
		const int newNumRects = numRects + numPrepend;
		if (newNumRects > rects.size())
			rects.resize(newNumRects);

		// move existing rectangles
		memmove(rects.data() + numPrepend, rects.constData() + numSkip, numRects * sizeof(QRect));

		// prepend new rectangles
		memcpy(rects.data(), r->rects.constData(), numPrepend * sizeof(QRect));

		numRects = newNumRects;
	}

	// update inner rectangle
	if (innerArea < r->innerArea) {
		innerArea = r->innerArea;
		innerRect = r->innerRect;
	}

	// update extents
	extents.setCoords(qMin(extents.left(), r->extents.left()), qMin(extents.top(), r->extents.top()), qMax(extents.right(), r->extents.right()), qMax(extents.bottom(), r->extents.bottom()));

#ifdef QT_REGION_DEBUG
	selfTest();
#endif
}

void _QRegionPrivate::prepend(const QRect* r)
{
	Q_ASSERT(!r->isEmpty());

	QRect* myFirst = (numRects == 1 ? &extents : rects.data());
	if (mergeFromLeft(myFirst, r)) {
		if (numRects > 1) {
			const QRect* nextToFirst = (numRects > 2 ? myFirst + 2 : 0);
			if (mergeFromAbove(myFirst + 1, myFirst, nextToFirst, 0)) {
				--numRects;
				memmove(rects.data(), rects.constData() + 1, numRects * sizeof(QRect));
			}
		}
	}
	else if (mergeFromAbove(myFirst, r, (numRects > 1 ? myFirst + 1 : 0), 0)) {
		// nothing
	}
	else {
		vectorize();
		++numRects;
		updateInnerRect(*r);
		rects.prepend(*r);
	}
	extents.setCoords(qMin(extents.left(), r->left()), qMin(extents.top(), r->top()), qMax(extents.right(), r->right()), qMax(extents.bottom(), r->bottom()));

#ifdef QT_REGION_DEBUG
	selfTest();
#endif
}

bool _QRegionPrivate::canAppend(const QRect* r) const
{
	Q_ASSERT(!r->isEmpty());

	const QRect* myLast = (numRects == 1) ? &extents : (rects.constData() + (numRects - 1));
	if (r->top() > myLast->bottom())
		return true;
	if (r->top() == myLast->top() && r->height() == myLast->height() && r->left() > myLast->right()) {
		return true;
	}

	return false;
}

bool _QRegionPrivate::canAppend(const _QRegionPrivate* r) const
{
	return canAppend(r->numRects == 1 ? &r->extents : r->rects.constData());
}

bool _QRegionPrivate::canPrepend(const QRect* r) const
{
	Q_ASSERT(!r->isEmpty());

	const QRect* myFirst = (numRects == 1) ? &extents : rects.constData();
	if (r->bottom() < myFirst->top()) // not overlapping
		return true;
	if (r->top() == myFirst->top() && r->height() == myFirst->height() && r->right() < myFirst->left()) {
		return true;
	}

	return false;
}

bool _QRegionPrivate::canPrepend(const _QRegionPrivate* r) const
{
	return canPrepend(r->numRects == 1 ? &r->extents : r->rects.constData() + r->numRects - 1);
}

#ifdef QT_REGION_DEBUG
void _QRegionPrivate::selfTest() const
{
	if (numRects == 0) {
		Q_ASSERT(extents.isEmpty());
		Q_ASSERT(innerRect.isEmpty());
		return;
	}

	Q_ASSERT(innerArea == (innerRect.width() * innerRect.height()));

	if (numRects == 1) {
		Q_ASSERT(innerRect == extents);
		Q_ASSERT(!innerRect.isEmpty());
		return;
	}

	for (int i = 0; i < numRects; ++i) {
		const QRect r = rects.at(i);
		if ((r.width() * r.height()) > innerArea)
			qDebug() << "selfTest(): innerRect" << innerRect << '<' << r;
	}

	QRect r = rects.first();
	for (int i = 1; i < numRects; ++i) {
		const QRect r2 = rects.at(i);
		Q_ASSERT(!r2.isEmpty());
		if (r2.y() == r.y()) {
			Q_ASSERT(r.bottom() == r2.bottom());
			Q_ASSERT(r.right() < (r2.left() + 1));
		}
		else {
			Q_ASSERT(r2.y() >= r.bottom());
		}
		r = r2;
	}
}
#endif // QT_REGION_DEBUG

typedef void (*OverlapFunc)(_QRegionPrivate& dest, const QRect* r1, const QRect* r1End, const QRect* r2, const QRect* r2End, int y1, int y2);
typedef void (*NonOverlapFunc)(_QRegionPrivate& dest, const QRect* r, const QRect* rEnd, int y1, int y2);

static bool EqualRegion(const _QRegionPrivate* r1, const _QRegionPrivate* r2);
static void UnionRegion(const _QRegionPrivate* reg1, const _QRegionPrivate* reg2, _QRegionPrivate& dest);
static void miRegionOp(_QRegionPrivate& dest, const _QRegionPrivate* reg1, const _QRegionPrivate* reg2, OverlapFunc overlapFunc, NonOverlapFunc nonOverlap1Func, NonOverlapFunc nonOverlap2Func);

#define RectangleOut 0
#define RectangleIn 1
#define RectanglePart 2
#define EvenOddRule 0
#define WindingRule 1

// START OF region.h extract
// $XConsortium: region.h,v 11.14 94/04/17 20:22:20 rws Exp $
//***********************************************************************
//
// Copyright (c) 1987  X Consortium
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of the X Consortium shall not be
// used in advertising or otherwise to promote the sale, use or other dealings
// in this Software without prior written authorization from the X Consortium.
//
//
// Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
//
// All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of Digital not be
// used in advertising or publicity pertaining to distribution of the
// software without specific, written prior permission.
//
// DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
// ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
// DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
// ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
// SOFTWARE.
//
//***********************************************************************

#ifndef _XREGION_H
#define _XREGION_H

QT_BEGIN_INCLUDE_NAMESPACE
#include <limits.h>
QT_END_INCLUDE_NAMESPACE

//  1 if two BOXes overlap.
// 0 if two BOXes do not overlap.
// Remember, x2 and y2 are not in the region
#define EXTENTCHECK(r1, r2) ((r1)->right() >= (r2)->left() && (r1)->left() <= (r2)->right() && (r1)->bottom() >= (r2)->top() && (r1)->top() <= (r2)->bottom())

// update region extents
#define EXTENTS(r, idRect)                                                                                                                                                                             \
	{                                                                                                                                                                                              \
		if ((r)->left() < (idRect)->extents.left())                                                                                                                                            \
			(idRect)->extents.setLeft((r)->left());                                                                                                                                        \
		if ((r)->top() < (idRect)->extents.top())                                                                                                                                              \
			(idRect)->extents.setTop((r)->top());                                                                                                                                          \
		if ((r)->right() > (idRect)->extents.right())                                                                                                                                          \
			(idRect)->extents.setRight((r)->right());                                                                                                                                      \
		if ((r)->bottom() > (idRect)->extents.bottom())                                                                                                                                        \
			(idRect)->extents.setBottom((r)->bottom());                                                                                                                                    \
	}

//  Check to see if there is enough memory in the present region.
#define MEMCHECK(dest, rect, firstrect)                                                                                                                                                                \
	{                                                                                                                                                                                              \
		if ((dest).numRects >= ((dest).rects.size() - 1)) {                                                                                                                                    \
			firstrect.resize(firstrect.size() * 2);                                                                                                                                        \
			(rect) = (firstrect).data() + (dest).numRects;                                                                                                                                 \
		}                                                                                                                                                                                      \
	}

// number of points to buffer before sending them off
// to scanlines(): Must be an even number
#define NUMPTSTOBUFFER 200

// used to allocate buffers for points and link
// the buffers together
typedef struct _POINTBLOCK
{
	char data[NUMPTSTOBUFFER * sizeof(QPoint)];
	QPoint* pts;
	struct _POINTBLOCK* next;
} POINTBLOCK;

#endif
// END OF region.h extract

// START OF Region.c extract
// $XConsortium: Region.c /main/30 1996/10/22 14:21:24 kaleb $
//***********************************************************************
//
// Copyright (c) 1987, 1988  X Consortium
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of the X Consortium shall not be
// used in advertising or otherwise to promote the sale, use or other dealings
// in this Software without prior written authorization from the X Consortium.
//
//
// Copyright 1987, 1988 by Digital Equipment Corporation, Maynard, Massachusetts.
//
// All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of Digital not be
// used in advertising or publicity pertaining to distribution of the
// software without specific, written prior permission.
//
// DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
// ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
// DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
// ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
// SOFTWARE.
//
//***********************************************************************
// The functions in this file implement the Region abstraction, similar to one
// used in the X11 sample server. A Region is simply an area, as the name
// implies, and is implemented as a "y-x-banded" array of rectangles. To
// explain: Each Region is made up of a certain number of rectangles sorted
// by y coordinate first, and then by x coordinate.
//
// Furthermore, the rectangles are banded such that every rectangle with a
// given upper-left y coordinate (y1) will have the same lower-right y
// coordinate (y2) and vice versa. If a rectangle has scanlines in a band, it
// will span the entire vertical distance of the band. This means that some
// areas that could be merged into a taller rectangle will be represented as
// several shorter rectangles to account for shorter rectangles to its left
// or right but within its "vertical scope".
//
// An added constraint on the rectangles is that they must cover as much
// horizontal area as possible. E.g. no two rectangles in a band are allowed
// to touch.
//
// Whenever possible, bands will be merged together to cover a greater vertical
// distance (and thus reduce the number of rectangles). Two bands can be merged
// only if the bottom of one touches the top of the other and they have
// rectangles in the same places (of the same width, of course). This maintains
// the y-x-banding that's so nice to have...
// $XFree86: xc/lib/X11/Region.c,v 1.1.1.2.2.2 1998/10/04 15:22:50 hohndel Exp $

static void UnionRectWithRegion(const QRect* rect, const _QRegionPrivate* source, _QRegionPrivate& dest)
{
	if (rect->isEmpty())
		return;

	Q_ASSERT(EqualRegion(source, &dest));

	if (dest.numRects == 0) {
		dest = _QRegionPrivate(*rect);
	}
	else if (dest.canAppend(rect)) {
		dest.append(rect);
	}
	else {
		_QRegionPrivate p(*rect);
		UnionRegion(&p, source, dest);
	}
}

//======================================================================
//         Region Intersection
// ====================================================================

//======================================================================
//         Generic Region Operator
// ====================================================================

//-
// -----------------------------------------------------------------------
// miCoalesce --
//     Attempt to merge the boxes in the current band with those in the
//     previous one. Used only by miRegionOp.
//
// Results:
//     The new index for the previous band.
//
// Side Effects:
//     If coalescing takes place:
//         - rectangles in the previous band will have their y2 fields
//           altered.
//         - dest.numRects will be decreased.
//
// -----------------------------------------------------------------------
static int miCoalesce(_QRegionPrivate& dest, int prevStart, int curStart)
{
	QRect* pPrevBox;  // Current box in previous band
	QRect* pCurBox;	  // Current box in current band
	QRect* pRegEnd;	  // End of region
	int curNumRects;  // Number of rectangles in current band
	int prevNumRects; // Number of rectangles in previous band
	int bandY1;	  // Y1 coordinate for current band
	QRect* rData = dest.rects.data();

	pRegEnd = rData + dest.numRects;

	pPrevBox = rData + prevStart;
	prevNumRects = curStart - prevStart;

	// Figure out how many rectangles are in the current band. Have to do
	// this because multiple bands could have been added in miRegionOp
	// at the end when one region has been exhausted.
	pCurBox = rData + curStart;
	bandY1 = pCurBox->top();
	for (curNumRects = 0; pCurBox != pRegEnd && pCurBox->top() == bandY1; ++curNumRects) {
		++pCurBox;
	}

	if (pCurBox != pRegEnd) {
		// If more than one band was added, we have to find the start
		// of the last band added so the next coalescing job can start
		// at the right place... (given when multiple bands are added,
		// this may be pointless -- see above).
		--pRegEnd;
		while ((pRegEnd - 1)->top() == pRegEnd->top())
			--pRegEnd;
		curStart = pRegEnd - rData;
		pRegEnd = rData + dest.numRects;
	}

	if (curNumRects == prevNumRects && curNumRects != 0) {
		pCurBox -= curNumRects;
		// The bands may only be coalesced if the bottom of the previous
		// matches the top scanline of the current.
		if (pPrevBox->bottom() == pCurBox->top() - 1) {
			// Make sure the bands have boxes in the same places. This
			// assumes that boxes have been added in such a way that they
			// cover the most area possible. I.e. two boxes in a band must
			// have some horizontal space between them.
			do {
				if (pPrevBox->left() != pCurBox->left() || pPrevBox->right() != pCurBox->right()) {
					// The bands don't line up so they can't be coalesced.
					return curStart;
				}
				++pPrevBox;
				++pCurBox;
				--prevNumRects;
			} while (prevNumRects != 0);

			dest.numRects -= curNumRects;
			pCurBox -= curNumRects;
			pPrevBox -= curNumRects;

			// The bands may be merged, so set the bottom y of each box
			// in the previous band to that of the corresponding box in
			// the current band.
			do {
				pPrevBox->setBottom(pCurBox->bottom());
				dest.updateInnerRect(*pPrevBox);
				++pPrevBox;
				++pCurBox;
				curNumRects -= 1;
			} while (curNumRects != 0);

			// If only one band was added to the region, we have to backup
			// curStart to the start of the previous band.
			//
			// If more than one band was added to the region, copy the
			// other bands down. The assumption here is that the other bands
			// came from the same region as the current one and no further
			// coalescing can be done on them since it's all been done
			// already... curStart is already in the right place.
			if (pCurBox == pRegEnd) {
				curStart = prevStart;
			}
			else {
				do {
					*pPrevBox++ = *pCurBox++;
					dest.updateInnerRect(*pPrevBox);
				} while (pCurBox != pRegEnd);
			}
		}
	}
	return curStart;
}

//-
// -----------------------------------------------------------------------
// miRegionOp --
//     Apply an operation to two regions. Called by miUnion, miInverse,
//     miSubtract, miIntersect...
//
// Results:
//     None.
//
// Side Effects:
//     The new region is overwritten.
//
// Notes:
//     The idea behind this function is to view the two regions as sets.
//     Together they cover a rectangle of area that this function divides
//     into horizontal bands where points are covered only by one region
//     or by both. For the first case, the nonOverlapFunc is called with
//     each the band and the band's upper and lower extents. For the
//     second, the overlapFunc is called to process the entire band. It
//     is responsible for clipping the rectangles in the band, though
//     this function provides the boundaries.
//     At the end of each band, the new region is coalesced, if possible,
//     to reduce the number of rectangles in the region.
//
// -----------------------------------------------------------------------
static void miRegionOp(_QRegionPrivate& dest, const _QRegionPrivate* reg1, const _QRegionPrivate* reg2, OverlapFunc overlapFunc, NonOverlapFunc nonOverlap1Func, NonOverlapFunc nonOverlap2Func)
{
	const QRect* r1;	// Pointer into first region
	const QRect* r2;	// Pointer into 2d region
	const QRect* r1End;	// End of 1st region
	const QRect* r2End;	// End of 2d region
	int ybot;		// Bottom of intersection
	int ytop;		// Top of intersection
	int prevBand;		// Index of start of previous band in dest
	int curBand;		// Index of start of current band in dest
	const QRect* r1BandEnd; // End of current band in r1
	const QRect* r2BandEnd; // End of current band in r2
	int top;		// Top of non-overlapping band
	int bot;		// Bottom of non-overlapping band

	// Initialization:
	// set r1, r2, r1End and r2End appropriately, preserve the important
	// parts of the destination region until the end in case it's one of
	// the two source regions, then mark the "new" region empty, allocating
	// another array of rectangles for it to use.
	if (reg1->numRects == 1)
		r1 = &reg1->extents;
	else
		r1 = reg1->rects.constData();
	if (reg2->numRects == 1)
		r2 = &reg2->extents;
	else
		r2 = reg2->rects.constData();

	r1End = r1 + reg1->numRects;
	r2End = r2 + reg2->numRects;

	dest.vectorize();

	QVector<QRect> oldRects = dest.rects;

	dest.numRects = 0;

	// Allocate a reasonable number of rectangles for the new region. The idea
	// is to allocate enough so the individual functions don't need to
	// reallocate and copy the array, which is time consuming, yet we don't
	// have to worry about using too much memory. I hope to be able to
	// nuke the realloc() at the end of this function eventually.
	dest.rects.resize(qMax(reg1->numRects, reg2->numRects) * 2);

	// Initialize ybot and ytop.
	// In the upcoming loop, ybot and ytop serve different functions depending
	// on whether the band being handled is an overlapping or non-overlapping
	// band.
	// In the case of a non-overlapping band (only one of the regions
	// has points in the band), ybot is the bottom of the most recent
	// intersection and thus clips the top of the rectangles in that band.
	// ytop is the top of the next intersection between the two regions and
	// serves to clip the bottom of the rectangles in the current band.
	// For an overlapping band (where the two regions intersect), ytop clips
	// the top of the rectangles of both regions and ybot clips the bottoms.
	if (reg1->extents.top() < reg2->extents.top())
		ybot = reg1->extents.top() - 1;
	else
		ybot = reg2->extents.top() - 1;

	// prevBand serves to mark the start of the previous band so rectangles
	// can be coalesced into larger rectangles. qv. miCoalesce, above.
	// In the beginning, there is no previous band, so prevBand == curBand
	// (curBand is set later on, of course, but the first band will always
	// start at index 0). prevBand and curBand must be indices because of
	// the possible expansion, and resultant moving, of the new region's
	// array of rectangles.
	prevBand = 0;

	do {
		curBand = dest.numRects;

		// This algorithm proceeds one source-band (as opposed to a
		// destination band, which is determined by where the two regions
		// intersect) at a time. r1BandEnd and r2BandEnd serve to mark the
		// rectangle after the last one in the current band for their
		// respective regions.
		r1BandEnd = r1;
		while (r1BandEnd != r1End && r1BandEnd->top() == r1->top())
			++r1BandEnd;

		r2BandEnd = r2;
		while (r2BandEnd != r2End && r2BandEnd->top() == r2->top())
			++r2BandEnd;

		// First handle the band that doesn't intersect, if any.
		//
		// Note that attention is restricted to one band in the
		// non-intersecting region at once, so if a region has n
		// bands between the current position and the next place it overlaps
		// the other, this entire loop will be passed through n times.
		if (r1->top() < r2->top()) {
			top = qMax(r1->top(), ybot + 1);
			bot = qMin(r1->bottom(), r2->top() - 1);

			if (nonOverlap1Func != 0 && bot >= top)
				(*nonOverlap1Func)(dest, r1, r1BandEnd, top, bot);
			ytop = r2->top();
		}
		else if (r2->top() < r1->top()) {
			top = qMax(r2->top(), ybot + 1);
			bot = qMin(r2->bottom(), r1->top() - 1);

			if (nonOverlap2Func != 0 && bot >= top)
				(*nonOverlap2Func)(dest, r2, r2BandEnd, top, bot);
			ytop = r1->top();
		}
		else {
			ytop = r1->top();
		}

		// If any rectangles got added to the region, try and coalesce them
		// with rectangles from the previous band. Note we could just do
		// this test in miCoalesce, but some machines incur a not
		// inconsiderable cost for function calls, so...
		if (dest.numRects != curBand)
			prevBand = miCoalesce(dest, prevBand, curBand);

		// Now see if we've hit an intersecting band. The two bands only
		// intersect if ybot >= ytop
		ybot = qMin(r1->bottom(), r2->bottom());
		curBand = dest.numRects;
		if (ybot >= ytop)
			(*overlapFunc)(dest, r1, r1BandEnd, r2, r2BandEnd, ytop, ybot);

		if (dest.numRects != curBand)
			prevBand = miCoalesce(dest, prevBand, curBand);

		// If we've finished with a band (y2 == ybot) we skip forward
		// in the region to the next band.
		if (r1->bottom() == ybot)
			r1 = r1BandEnd;
		if (r2->bottom() == ybot)
			r2 = r2BandEnd;
	} while (r1 != r1End && r2 != r2End);

	// Deal with whichever region still has rectangles left.
	curBand = dest.numRects;
	if (r1 != r1End) {
		if (nonOverlap1Func != 0) {
			do {
				r1BandEnd = r1;
				while (r1BandEnd < r1End && r1BandEnd->top() == r1->top())
					++r1BandEnd;
				(*nonOverlap1Func)(dest, r1, r1BandEnd, qMax(r1->top(), ybot + 1), r1->bottom());
				r1 = r1BandEnd;
			} while (r1 != r1End);
		}
	}
	else if ((r2 != r2End) && (nonOverlap2Func != 0)) {
		do {
			r2BandEnd = r2;
			while (r2BandEnd < r2End && r2BandEnd->top() == r2->top())
				++r2BandEnd;
			(*nonOverlap2Func)(dest, r2, r2BandEnd, qMax(r2->top(), ybot + 1), r2->bottom());
			r2 = r2BandEnd;
		} while (r2 != r2End);
	}

	if (dest.numRects != curBand)
		(void)miCoalesce(dest, prevBand, curBand);

	// A bit of cleanup. To keep regions from growing without bound,
	// we shrink the array of rectangles to match the new number of
	// rectangles in the region.
	//
	// Only do this stuff if the number of rectangles allocated is more than
	// twice the number of rectangles in the region (a simple optimization).
	if (qMax(4, dest.numRects) < (dest.rects.size() >> 1))
		dest.rects.resize(dest.numRects);
}

//======================================================================
//         Region Union
// ====================================================================

//-
// -----------------------------------------------------------------------
// miUnionNonO --
//     Handle a non-overlapping band for the union operation. Just
//     Adds the rectangles into the region. Doesn't have to check for
//     subsumption or anything.
//
// Results:
//     None.
//
// Side Effects:
//     dest.numRects is incremented and the final rectangles overwritten
//     with the rectangles we're passed.
//
// -----------------------------------------------------------------------

static void miUnionNonO(_QRegionPrivate& dest, const QRect* r, const QRect* rEnd, int y1, int y2)
{
	QRect* pNextRect;

	pNextRect = dest.rects.data() + dest.numRects;

	Q_ASSERT(y1 <= y2);

	while (r != rEnd) {
		Q_ASSERT(r->left() <= r->right());
		MEMCHECK(dest, pNextRect, dest.rects)
		pNextRect->setCoords(r->left(), y1, r->right(), y2);
		dest.numRects++;
		++pNextRect;
		++r;
	}
}

//-
// -----------------------------------------------------------------------
// miUnionO --
//     Handle an overlapping band for the union operation. Picks the
//     left-most rectangle each time and merges it into the region.
//
// Results:
//     None.
//
// Side Effects:
//     Rectangles are overwritten in dest.rects and dest.numRects will
//     be changed.
//
// -----------------------------------------------------------------------

static void miUnionO(_QRegionPrivate& dest, const QRect* r1, const QRect* r1End, const QRect* r2, const QRect* r2End, int y1, int y2)
{
	QRect* pNextRect;

	pNextRect = dest.rects.data() + dest.numRects;

#define MERGERECT(r)                                                                                                                                                                                   \
	if ((dest.numRects != 0) && (pNextRect[-1].top() == y1) && (pNextRect[-1].bottom() == y2) && (pNextRect[-1].right() >= r->left() - 1)) {                                                       \
		if (pNextRect[-1].right() < r->right()) {                                                                                                                                              \
			pNextRect[-1].setRight(r->right());                                                                                                                                            \
			dest.updateInnerRect(pNextRect[-1]);                                                                                                                                           \
			Q_ASSERT(pNextRect[-1].left() <= pNextRect[-1].right());                                                                                                                       \
		}                                                                                                                                                                                      \
	}                                                                                                                                                                                              \
	else {                                                                                                                                                                                         \
		MEMCHECK(dest, pNextRect, dest.rects)                                                                                                                                                  \
		pNextRect->setCoords(r->left(), y1, r->right(), y2);                                                                                                                                   \
		dest.updateInnerRect(*pNextRect);                                                                                                                                                      \
		dest.numRects++;                                                                                                                                                                       \
		pNextRect++;                                                                                                                                                                           \
	}                                                                                                                                                                                              \
	r++;

	Q_ASSERT(y1 <= y2);
	while (r1 != r1End && r2 != r2End) {
		if (r1->left() < r2->left()) {
			MERGERECT(r1)
		}
		else {
			MERGERECT(r2)
		}
	}

	if (r1 != r1End) {
		do {
			MERGERECT(r1)
		} while (r1 != r1End);
	}
	else {
		while (r2 != r2End) {
			MERGERECT(r2)
		}
	}
}

static void UnionRegion(const _QRegionPrivate* reg1, const _QRegionPrivate* reg2, _QRegionPrivate& dest)
{
	Q_ASSERT(!isEmptyHelper(reg1) && !isEmptyHelper(reg2));
	Q_ASSERT(!reg1->contains(*reg2));
	Q_ASSERT(!reg2->contains(*reg1));
	Q_ASSERT(!EqualRegion(reg1, reg2));
	Q_ASSERT(!reg1->canAppend(reg2));
	Q_ASSERT(!reg2->canAppend(reg1));

	if (reg1->innerArea > reg2->innerArea) {
		dest.innerArea = reg1->innerArea;
		dest.innerRect = reg1->innerRect;
	}
	else {
		dest.innerArea = reg2->innerArea;
		dest.innerRect = reg2->innerRect;
	}
	miRegionOp(dest, reg1, reg2, miUnionO, miUnionNonO, miUnionNonO);

	dest.extents.setCoords(qMin(reg1->extents.left(), reg2->extents.left()),
			       qMin(reg1->extents.top(), reg2->extents.top()),
			       qMax(reg1->extents.right(), reg2->extents.right()),
			       qMax(reg1->extents.bottom(), reg2->extents.bottom()));
}

//======================================================================
//       Region Subtraction
// ====================================================================

//-
// -----------------------------------------------------------------------
// miSubtract --
//     Subtract regS from regM and leave the result in regD.
//     S stands for subtrahend, M for minuend and D for difference.
//
// Side Effects:
//     regD is overwritten.
//
// -----------------------------------------------------------------------

//     Check to see if two regions are equal
static bool EqualRegion(const _QRegionPrivate* r1, const _QRegionPrivate* r2)
{
	if (r1->numRects != r2->numRects) {
		return false;
	}
	else if (r1->numRects == 0) {
		return true;
	}
	else if (r1->extents != r2->extents) {
		return false;
	}
	else if (r1->numRects == 1 && r2->numRects == 1) {
		return true; // equality tested in previous if-statement
	}
	else {
		const QRect* rr1 = (r1->numRects == 1) ? &r1->extents : r1->rects.constData();
		const QRect* rr2 = (r2->numRects == 1) ? &r2->extents : r2->rects.constData();
		for (int i = 0; i < r1->numRects; ++i, ++rr1, ++rr2) {
			if (*rr1 != *rr2)
				return false;
		}
	}

	return true;
}

// END OF Region.c extract
// START OF poly.h extract
// $XConsortium: poly.h,v 1.4 94/04/17 20:22:19 rws Exp $
//***********************************************************************
//
// Copyright (c) 1987  X Consortium
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of the X Consortium shall not be
// used in advertising or otherwise to promote the sale, use or other dealings
// in this Software without prior written authorization from the X Consortium.
//
//
// Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
//
// All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of Digital not be
// used in advertising or publicity pertaining to distribution of the
// software without specific, written prior permission.
//
// DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
// ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
// DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
// ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
// SOFTWARE.
//
//***********************************************************************

//    This file contains a few macros to help track
//    the edge of a filled object.  The object is assumed
//    to be filled in scanline order, and thus the
//    algorithm used is an extension of Bresenham's line
//    drawing algorithm which assumes that y is always the
//    major axis.
//    Since these pieces of code are the same for any filled shape,
//    it is more convenient to gather the library in one
//    place, but since these pieces of code are also in
//    the inner loops of output primitives, procedure call
//    overhead is out of the question.
//    See the author for a derivation if needed.

// In scan converting polygons, we want to choose those pixels
// which are inside the polygon.  Thus, we add .5 to the starting
// x coordinate for both left and right edges.  Now we choose the
// first pixel which is inside the pgon for the left edge and the
// first pixel which is outside the pgon for the right edge.
// Draw the left pixel, but not the right.
//
// How to add .5 to the starting x coordinate:
//     If the edge is moving to the right, then subtract dy from the
// error term from the general form of the algorithm.
//     If the edge is moving to the left, then add dy to the error term.
//
// The reason for the difference between edges moving to the left
// and edges moving to the right is simple:  If an edge is moving
// to the right, then we want the algorithm to flip immediately.
// If it is moving to the left, then we don't want it to flip until
// we traverse an entire pixel.
#define BRESINITPGON(dy, x1, x2, xStart, d, m, m1, incr1, incr2)                                                                                                                                       \
	{                                                                                                                                                                                              \
		int dx; /* local storage*/                                                                                                                                                             \
                                                                                                                                                                                                       \
		/*if the edge is horizontal, then it is ignored  and assumed not to be processed.  Otherwise, do this stuff. */                                                                        \
		if ((dy) != 0) {                                                                                                                                                                       \
			xStart = (x1);                                                                                                                                                                 \
			dx = (x2)-xStart;                                                                                                                                                              \
			if (dx < 0) {                                                                                                                                                                  \
				m = dx / (dy);                                                                                                                                                         \
				m1 = m - 1;                                                                                                                                                            \
				incr1 = -2 * dx + 2 * (dy)*m1;                                                                                                                                         \
				incr2 = -2 * dx + 2 * (dy)*m;                                                                                                                                          \
				d = 2 * m * (dy)-2 * dx - 2 * (dy);                                                                                                                                    \
			}                                                                                                                                                                              \
			else {                                                                                                                                                                         \
				m = dx / (dy);                                                                                                                                                         \
				m1 = m + 1;                                                                                                                                                            \
				incr1 = 2 * dx - 2 * (dy)*m1;                                                                                                                                          \
				incr2 = 2 * dx - 2 * (dy)*m;                                                                                                                                           \
				d = -2 * m * (dy) + 2 * dx;                                                                                                                                            \
			}                                                                                                                                                                              \
		}                                                                                                                                                                                      \
	}

#define BRESINCRPGON(d, minval, m, m1, incr1, incr2)                                                                                                                                                   \
	{                                                                                                                                                                                              \
		if (m1 > 0) {                                                                                                                                                                          \
			if (d > 0) {                                                                                                                                                                   \
				minval += m1;                                                                                                                                                          \
				d += incr1;                                                                                                                                                            \
			}                                                                                                                                                                              \
			else {                                                                                                                                                                         \
				minval += m;                                                                                                                                                           \
				d += incr2;                                                                                                                                                            \
			}                                                                                                                                                                              \
		}                                                                                                                                                                                      \
		else {                                                                                                                                                                                 \
			if (d >= 0) {                                                                                                                                                                  \
				minval += m1;                                                                                                                                                          \
				d += incr1;                                                                                                                                                            \
			}                                                                                                                                                                              \
			else {                                                                                                                                                                         \
				minval += m;                                                                                                                                                           \
				d += incr2;                                                                                                                                                            \
			}                                                                                                                                                                              \
		}                                                                                                                                                                                      \
	}

//    This structure contains all of the information needed
//    to run the bresenham algorithm.
//    The variables may be hardcoded into the declarations
//    instead of using this structure to make use of
//    register declarations.
typedef struct
{
	int minor_axis;	  // minor axis
	int d;		  // decision variable
	int m, m1;	  // slope and slope+1
	int incr1, incr2; // error increments
} BRESINFO;

#define BRESINITPGONSTRUCT(dmaj, min1, min2, bres) BRESINITPGON(dmaj, min1, min2, bres.minor_axis, bres.d, bres.m, bres.m1, bres.incr1, bres.incr2)

#define BRESINCRPGONSTRUCT(bres) BRESINCRPGON(bres.d, bres.minor_axis, bres.m, bres.m1, bres.incr1, bres.incr2)

//    These are the data structures needed to scan
//    convert regions.  Two different scan conversion
//    methods are available -- the even-odd method, and
//    the winding number method.
//    The even-odd rule states that a point is inside
//    the polygon if a ray drawn from that point in any
//    direction will pass through an odd number of
//    path segments.
//    By the winding number rule, a point is decided
//    to be inside the polygon if a ray drawn from that
//    point in any direction passes through a different
//    number of clockwise and counter-clockwise path
//    segments.
//
//    These data structures are adapted somewhat from
//    the algorithm in (Foley/Van Dam) for scan converting
//    polygons.
//    The basic algorithm is to start at the top (smallest y)
//    of the polygon, stepping down to the bottom of
//    the polygon by incrementing the y coordinate.  We
//    keep a list of edges which the current scanline crosses,
//    sorted by x.  This list is called the Active Edge Table (AET)
//    As we change the y-coordinate, we update each entry in
//    in the active edge table to reflect the edges new xcoord.
//    This list must be sorted at each scanline in case
//    two edges intersect.
//    We also keep a data structure known as the Edge Table (ET),
//    which keeps track of all the edges which the current
//    scanline has not yet reached.  The ET is basically a
//    list of ScanLineList structures containing a list of
//    edges which are entered at a given scanline.  There is one
//    ScanLineList per scanline at which an edge is entered.
//    When we enter a new edge, we move it from the ET to the AET.
//
//    From the AET, we can implement the even-odd rule as in
//    (Foley/Van Dam).
//    The winding number rule is a little trickier.  We also
//    keep the EdgeTableEntries in the AET linked by the
//    nextWETE (winding EdgeTableEntry) link.  This allows
//    the edges to be linked just as before for updating
//    purposes, but only uses the edges linked by the nextWETE
//    link as edges representing spans of the polygon to
//    drawn (as with the even-odd rule).

// for the winding number rule
#define CLOCKWISE 1
#define COUNTERCLOCKWISE -1

typedef struct _EdgeTableEntry
{
	int ymax;			  // ycoord at which we exit this edge.
	int ClockWise;			  // flag for winding number rule
	BRESINFO bres;			  // Bresenham info to run the edge
	struct _EdgeTableEntry* next;	  // next in the list
	struct _EdgeTableEntry* back;	  // for insertion sort
	struct _EdgeTableEntry* nextWETE; // for winding num rule
} EdgeTableEntry;

typedef struct _ScanLineList
{
	int scanline;		    // the scanline represented
	EdgeTableEntry* edgelist;   // header node
	struct _ScanLineList* next; // next in the list
} ScanLineList;

typedef struct
{
	int ymax;		// ymax for the polygon
	int ymin;		// ymin for the polygon
	ScanLineList scanlines; // header node
} EdgeTable;

// Here is a struct to help with storage allocation
// so we can allocate a big chunk at a time, and then take
// pieces from this heap when we need to.
#define SLLSPERBLOCK 25

typedef struct _ScanLineListBlock
{
	ScanLineList SLLs[SLLSPERBLOCK];
	struct _ScanLineListBlock* next;
} ScanLineListBlock;

//
//    a few macros for the inner loops of the fill code where
//    performance considerations don't allow a procedure call.
//
//    Evaluate the given edge at the given scanline.
//    If the edge has expired, then we leave it and fix up
//    the active edge table; otherwise, we increment the
//    x value to be ready for the next scanline.
//    The winding number rule is in effect, so we must notify
//    the caller when the edge has been removed so he
//    can reorder the Winding Active Edge Table.
#define EVALUATEEDGEWINDING(pAET, pPrevAET, y, fixWAET)                                                                                                                                                \
	{                                                                                                                                                                                              \
		if (pAET->ymax == y) { /* leaving this edge */                                                                                                                                         \
			pPrevAET->next = pAET->next;                                                                                                                                                   \
			pAET = pPrevAET->next;                                                                                                                                                         \
			fixWAET = 1;                                                                                                                                                                   \
			if (pAET)                                                                                                                                                                      \
				pAET->back = pPrevAET;                                                                                                                                                 \
		}                                                                                                                                                                                      \
		else {                                                                                                                                                                                 \
			BRESINCRPGONSTRUCT(pAET->bres)                                                                                                                                                 \
			pPrevAET = pAET;                                                                                                                                                               \
			pAET = pAET->next;                                                                                                                                                             \
		}                                                                                                                                                                                      \
	}

//    Evaluate the given edge at the given scanline.
//    If the edge has expired, then we leave it and fix up
//    the active edge table; otherwise, we increment the
//    x value to be ready for the next scanline.
//    The even-odd rule is in effect.
#define EVALUATEEDGEEVENODD(pAET, pPrevAET, y)                                                                                                                                                         \
	{                                                                                                                                                                                              \
		if (pAET->ymax == y) { /* leaving this edge */                                                                                                                                         \
			pPrevAET->next = pAET->next;                                                                                                                                                   \
			pAET = pPrevAET->next;                                                                                                                                                         \
			if (pAET)                                                                                                                                                                      \
				pAET->back = pPrevAET;                                                                                                                                                 \
		}                                                                                                                                                                                      \
		else {                                                                                                                                                                                 \
			BRESINCRPGONSTRUCT(pAET->bres)                                                                                                                                                 \
			pPrevAET = pAET;                                                                                                                                                               \
			pAET = pAET->next;                                                                                                                                                             \
		}                                                                                                                                                                                      \
	}
// END OF poly.h extract
// START OF PolyReg.c extract
// $XConsortium: PolyReg.c,v 11.23 94/11/17 21:59:37 converse Exp $
//***********************************************************************
//
// Copyright (c) 1987  X Consortium
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
// AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name of the X Consortium shall not be
// used in advertising or otherwise to promote the sale, use or other dealings
// in this Software without prior written authorization from the X Consortium.
//
//
// Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
//
// All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of Digital not be
// used in advertising or publicity pertaining to distribution of the
// software without specific, written prior permission.
//
// DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
// ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
// DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
// ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
// WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
// ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
// SOFTWARE.
//
//***********************************************************************
// $XFree86: xc/lib/X11/PolyReg.c,v 1.1.1.2.8.2 1998/10/04 15:22:49 hohndel Exp $

#define LARGE_COORDINATE INT_MAX
#define SMALL_COORDINATE INT_MIN

struct QRegionSpan
{
	QRegionSpan() {}
	QRegionSpan(int x1_, int x2_)
	  : x1(x1_)
	  , x2(x2_)
	{
	}

	int x1;
	int x2;
	int width() const { return x2 - x1; }
};

Q_DECLARE_TYPEINFO(QRegionSpan, Q_PRIMITIVE_TYPE);

static inline void flushRow(const QRegionSpan* spans, int y, int numSpans, _QRegionPrivate* reg, int* lastRow, int* extendTo, bool* needsExtend)
{
	QRect* regRects = reg->rects.data() + *lastRow;
	bool canExtend = reg->rects.size() - *lastRow == numSpans && !(*needsExtend && *extendTo + 1 != y) && (*needsExtend || regRects[0].y() + regRects[0].height() == y);

	for (int i = 0; i < numSpans && canExtend; ++i) {
		if (regRects[i].x() != spans[i].x1 || regRects[i].right() != spans[i].x2 - 1)
			canExtend = false;
	}

	if (canExtend) {
		*extendTo = y;
		*needsExtend = true;
	}
	else {
		if (*needsExtend) {
			for (int i = 0; i < reg->rects.size() - *lastRow; ++i)
				regRects[i].setBottom(*extendTo);
		}

		*lastRow = reg->rects.size();
		reg->rects.reserve(*lastRow + numSpans);
		for (int i = 0; i < numSpans; ++i)
			reg->rects << QRect(spans[i].x1, y, spans[i].width(), 1);

		if (spans[0].x1 < reg->extents.left())
			reg->extents.setLeft(spans[0].x1);

		if (spans[numSpans - 1].x2 - 1 > reg->extents.right())
			reg->extents.setRight(spans[numSpans - 1].x2 - 1);

		*needsExtend = false;
	}
}

_QRegionPrivate* qt_imageToRegion(const QImage& image)
{
	_QRegionPrivate* region = new _QRegionPrivate;

	QRect xr;

#define AddSpan                                                                                                                                                                                        \
	{                                                                                                                                                                                              \
		xr.setCoords(prev1, y, x - 1, y);                                                                                                                                                      \
		UnionRectWithRegion(&xr, region, *region);                                                                                                                                             \
	}

	const uchar zero = 0;
	bool little = image.format() == QImage::Format_MonoLSB;

	int x, y;
	for (y = 0; y < image.height(); ++y) {
		const uchar* line = image.constScanLine(y);
		int w = image.width();
		uchar all = zero;
		int prev1 = -1;
		for (x = 0; x < w;) {
			uchar byte = line[x / 8];
			if (x > w - 8 || byte != all) {
				if (little) {
					for (int b = 8; b > 0 && x < w; --b) {
						if (!(byte & 0x01) == !all) {
							// More of the same
						}
						else {
							// A change.
							if (all != zero) {
								AddSpan all = zero;
							}
							else {
								prev1 = x;
								all = static_cast<uchar>(~zero);
							}
						}
						byte >>= 1;
						++x;
					}
				}
				else {
					for (int b = 8; b > 0 && x < w; --b) {
						if (!(byte & 0x80) == !all) {
							// More of the same
						}
						else {
							// A change.
							if (all != zero) {
								AddSpan all = zero;
							}
							else {
								prev1 = x;
								all =static_cast<uchar>( ~zero);
							}
						}
						byte <<= 1;
						++x;
					}
				}
			}
			else {
				x += 8;
			}
		}
		if (all != zero) {
			AddSpan
		}
	}
#undef AddSpan

	return region;
}

/// Returns a QImage::Format_Mono image with the given size, and set the palette to Qt::white and Qt::black
static QImage createEmptyMask(int width, int height)
{
	QImage bit(width, height, QImage::Format_MonoLSB);
	bit.fill(Qt::color0);
	return bit;
}

QRegion vipExtractRegion(const QPainterPath& p)
{
	if (p.isEmpty()) {
		return QRegion();
	}
	QPainterPath temp(p);
	QRectF rect = (temp.boundingRect());
	QPoint top_left = rect.topLeft().toPoint();
	temp.translate(rect.topLeft() * -1.0);
	rect = temp.boundingRect();
	QImage bit = createEmptyMask(qRound(rect.width()) + 1, qRound(rect.height()) + 1);
	{
		QPainter painter(&bit);
		// VERY STRANGE
		painter.fillPath(temp, Qt::color0);
	}

	_QRegionPrivate* rnp = qt_imageToRegion(bit);
	rnp->vectorize();

	QRegion res;
	res.setRects(rnp->rects.data(), rnp->rects.size());
	delete rnp;

	if (res.rectCount())
		return res.translated(top_left);
	else {
		// make sure the region contains at least one pixel
		res = QRegion(top_left.x(), top_left.y(), 1, 1);
		return res;
	}
}
