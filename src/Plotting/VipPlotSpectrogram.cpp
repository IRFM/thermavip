/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2023, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include <atomic>
#include <qnumeric.h>

#include "VipAxisColorMap.h"
#include "VipInterval.h"
#include "VipPainter.h"
#include "VipPlotSpectrogram.h"
#include "VipSliderGrip.h"

struct Point3D
{
	double x, y, z;

	Point3D(double x = 0., double y = 0., double z = 0.)
	  : x(x)
	  , y(y)
	  , z(z)
	{
	}

	QPointF toPoint() const { return QPointF(x, y); }
};

class ContourPlane
{
public:
	inline ContourPlane(double z)
	  : d_z(z)
	{
	}

	inline bool intersect(const Point3D vertex[3], QPointF line[2], bool ignoreOnPlane) const;

	inline double z() const { return d_z; }

private:
	inline int compare(double z) const;
	inline QPointF intersection(const Point3D& p1, const Point3D& p2) const;

	double d_z;
};

inline bool ContourPlane::intersect(const Point3D vertex[3], QPointF line[2], bool ignoreOnPlane) const
{
	bool found = true;

	// Are the vertices below (-1), on (0) or above (1) the plan ?
	const int eq1 = compare(vertex[0].z);
	const int eq2 = compare(vertex[1].z);
	const int eq3 = compare(vertex[2].z);

	//  (a) All the vertices lie below the contour level.
	//  (b) Two vertices lie below and one on the contour level.
	//  (c) Two vertices lie below and one above the contour level.
	//  (d) One vertex lies below and two on the contour level.
	//  (e) One vertex lies below, one on and one above the contour level.
	//  (f) One vertex lies below and two above the contour level.
	//  (g) Three vertices lie on the contour level.
	//  (h) Two vertices lie on and one above the contour level.
	//  (i) One vertex lies on and two above the contour level.
	//  (j) All the vertices lie above the contour level.

	static const int tab[3][3][3] = { // jump table to avoid nested case statements
					  { { 0, 0, 8 }, { 0, 2, 5 }, { 7, 6, 9 } },
					  { { 0, 3, 4 }, { 1, 10, 1 }, { 4, 3, 0 } },
					  { { 9, 6, 7 }, { 5, 2, 0 }, { 8, 0, 0 } }
	};

	const int edgeType = tab[eq1 + 1][eq2 + 1][eq3 + 1];
	switch (edgeType) {
		case 1:
			// d(0,0,-1), h(0,0,1)
			line[0] = vertex[0].toPoint();
			line[1] = vertex[1].toPoint();
			break;
		case 2:
			// d(-1,0,0), h(1,0,0)
			line[0] = vertex[1].toPoint();
			line[1] = vertex[2].toPoint();
			break;
		case 3:
			// d(0,-1,0), h(0,1,0)
			line[0] = vertex[2].toPoint();
			line[1] = vertex[0].toPoint();
			break;
		case 4:
			// e(0,-1,1), e(0,1,-1)
			line[0] = vertex[0].toPoint();
			line[1] = intersection(vertex[1], vertex[2]);
			break;
		case 5:
			// e(-1,0,1), e(1,0,-1)
			line[0] = vertex[1].toPoint();
			line[1] = intersection(vertex[2], vertex[0]);
			break;
		case 6:
			// e(-1,1,0), e(1,0,-1)
			line[0] = vertex[2].toPoint();
			line[1] = intersection(vertex[0], vertex[1]);
			break;
		case 7:
			// c(-1,1,-1), f(1,1,-1)
			line[0] = intersection(vertex[0], vertex[1]);
			line[1] = intersection(vertex[1], vertex[2]);
			break;
		case 8:
			// c(-1,-1,1), f(1,1,-1)
			line[0] = intersection(vertex[1], vertex[2]);
			line[1] = intersection(vertex[2], vertex[0]);
			break;
		case 9:
			// f(-1,1,1), c(1,-1,-1)
			line[0] = intersection(vertex[2], vertex[0]);
			line[1] = intersection(vertex[0], vertex[1]);
			break;
		case 10:
			// g(0,0,0)
			// The CONREC algorithm has no satisfying solution for
			// what to do, when all vertices are on the plane.

			if (ignoreOnPlane)
				found = false;
			else {
				line[0] = vertex[2].toPoint();
				line[1] = vertex[0].toPoint();
			}
			break;
		default:
			found = false;
	}

	return found;
}

inline int ContourPlane::compare(double z) const
{
	if (z > d_z)
		return 1;

	if (z < d_z)
		return -1;

	return 0;
}

inline QPointF ContourPlane::intersection(const Point3D& p1, const Point3D& p2) const
{
	const double h1 = p1.z - d_z;
	const double h2 = p2.z - d_z;

	const double x = (h2 * p1.x - h1 * p2.x) / (h2 - h1);
	const double y = (h2 * p1.y - h1 * p2.y) / (h2 - h1);

	return QPointF(x, y);
}

ContourLines VipPlotSpectrogram::contourLines(const VipNDArray& array_2D, const QRectF& rect, const QList<vip_double>& levels, bool IgnoreAllVerticesOnLevel)
{
	ContourLines contourLines;

	const VipNDArrayType<double, 2> value = array_2D.convert<double>();

	if (levels.size() == 0 || !rect.isValid() || value.isEmpty())
		return contourLines;

	const double dx = rect.width() / value.shape(1);
	const double dy = rect.height() / value.shape(0);

	const bool ignoreOnPlane = IgnoreAllVerticesOnLevel;

	// const VipInterval range = interval( Qt::ZAxis );
	//  bool ignoreOutOfRange = false;
	//  if ( range.isValid() )
	//   ignoreOutOfRange = flags & IgnoreOutOfRange;

	for (int y = 0; y < value.shape(0) - 1; y++) {
		enum Position
		{
			Center,

			TopLeft,
			TopRight,
			BottomRight,
			BottomLeft,

			NumPositions
		};

		Point3D xy[NumPositions];

		for (int x = 0; x < value.shape(1) - 1; x++) {
			const QPointF pos(rect.x() + x * dx, rect.y() + y * dy);

			if (x == 0) {
				xy[TopRight].x = (pos.x());
				xy[TopRight].y = (pos.y());
				xy[TopRight].z = (value(xy[TopRight].y, xy[TopRight].x));

				xy[BottomRight].x = (pos.x());
				xy[BottomRight].y = (pos.y() + dy);
				xy[BottomRight].z = (value(xy[BottomRight].y, xy[BottomRight].x));
			}

			xy[TopLeft] = xy[TopRight];
			xy[BottomLeft] = xy[BottomRight];

			xy[TopRight].x = (pos.x() + dx);
			xy[TopRight].y = (pos.y());
			xy[BottomRight].x = (pos.x() + dx);
			xy[BottomRight].y = (pos.y() + dy);

			xy[TopRight].z = (value(xy[TopRight].y, xy[TopRight].x));
			xy[BottomRight].z = (value(xy[BottomRight].y, xy[BottomRight].x));

			double zMin = xy[TopLeft].z;
			double zMax = zMin;
			double zSum = zMin;

			for (int i = TopRight; i <= BottomLeft; i++) {
				const double z = xy[i].z;

				zSum += z;
				if (z < zMin)
					zMin = z;
				if (z > zMax)
					zMax = z;
			}

			if (qIsNaN(zSum)) {
				// one of the points is NaN
				continue;
			}

			// if ( ignoreOutOfRange )
			//  {
			//   if ( !range.contains( zMin ) || !range.contains( zMax ) )
			//       continue;
			//  }

			if (zMax < levels[0] || zMin > levels[levels.size() - 1]) {
				continue;
			}

			xy[Center].x = (pos.x() + 0.5 * dx);
			xy[Center].y = (pos.y() + 0.5 * dy);
			xy[Center].z = (0.25 * zSum);

			const int numLevels = levels.size();
			for (int l = 0; l < numLevels; l++) {
				const double level = levels[l];
				if (level < zMin || level > zMax)
					continue;
				QPolygonF& lines = contourLines[level];
				const ContourPlane plane(level);

				QPointF line[2];
				Point3D vertex[3];

				for (int m = TopLeft; m < NumPositions; m++) {
					vertex[0] = xy[m];
					vertex[1] = xy[0];
					vertex[2] = xy[m != BottomLeft ? m + 1 : TopLeft];

					const bool intersects = plane.intersect(vertex, line, ignoreOnPlane);
					if (intersects) {
						lines += line[0];
						lines += line[1];
					}
				}
			}
		}
	}

	return contourLines;
}

static int registerSpectrogramKeyWords()
{
	static VipKeyWords keywords;
	if (keywords.isEmpty()) {

		keywords["default-contour-pen"] = VipParserPtr(new PenParser());

		vipSetKeyWordsForClass(&VipPlotSpectrogram::staticMetaObject, keywords);
	}
	return 0;
}
static int _registerSpectrogramKeyWords = registerSpectrogramKeyWords();

class VipPlotSpectrogram::PrivateData
{
public:
	PrivateData()
	  : ignoreAllVerticesOnLevel(false)
	  , dirtyContourLines(true)
	{
	}

	QList<VipColorMapGrip*> contourGrip;
	QList<vip_double> contourLevels;
	ContourLines lines;
	QPen defaultContourPen;
	bool ignoreAllVerticesOnLevel;
	std::atomic<bool> dirtyContourLines;
};

VipPlotSpectrogram::VipPlotSpectrogram(const VipText& title)
  : VipPlotRasterData(title)
  , d_data(new PrivateData())
{
	// disable antialiazing by default
	setRenderHints(QPainter::RenderHints());
}

VipPlotSpectrogram::~VipPlotSpectrogram()
{
	emitItemDestroyed();
}

void VipPlotSpectrogram::setDefaultContourPen(const QPen& pen)
{
	if (pen != d_data->defaultContourPen) {
		d_data->defaultContourPen = pen;

		emitItemChanged();
	}
}

QPen VipPlotSpectrogram::defaultContourPen() const
{
	return d_data->defaultContourPen;
}

QPen VipPlotSpectrogram::contourPen(double level) const
{
	QPen pen = d_data->defaultContourPen;
	pen.setColor(this->color(level, pen.color()));
	return pen;
}

void VipPlotSpectrogram::setIgnoreAllVerticesOnLevel(bool ignore)
{
	d_data->ignoreAllVerticesOnLevel = ignore;

	emitItemChanged();
}

bool VipPlotSpectrogram::ignoreAllVerticesOnLevel() const
{
	return d_data->ignoreAllVerticesOnLevel;
}

void VipPlotSpectrogram::levelGripChanged(double value)
{
	int index = d_data->contourGrip.indexOf(static_cast<VipColorMapGrip*>(sender()));
	d_data->contourLevels[index] = value;
	d_data->dirtyContourLines = 1;
	emitItemChanged();
}

void VipPlotSpectrogram::setContourLevels(const QList<vip_double>& levels, bool add_grip, const QPixmap& grip_pixmap)
{
	d_data->contourLevels = levels;
	d_data->dirtyContourLines = 1;

	if (add_grip && colorMap()) {
		// remove previous grip
		for (int i = 0; i < d_data->contourGrip.size(); ++i) {
			colorMap()->removeGrip(d_data->contourGrip[i]);
			delete d_data->contourGrip[i];
		}

		d_data->contourGrip.clear();

		// create the new grip
		for (int i = 0; i < levels.size(); ++i) {
			d_data->contourGrip.push_back(colorMap()->addGrip());
			d_data->contourGrip.back()->setValue(levels[i]);
			d_data->contourGrip.back()->setImage(grip_pixmap.toImage());

			connect(d_data->contourGrip.back(), SIGNAL(valueChanged(double)), this, SLOT(levelGripChanged(double)));
		}
	}
	else {
		// remove previous grip
		for (int i = 0; i < d_data->contourGrip.size(); ++i) {
			colorMap()->removeGrip(d_data->contourGrip[i]);
			delete d_data->contourGrip[i];
		}
		d_data->contourGrip.clear();
	}

	emitItemChanged();
}

QList<VipColorMapGrip*> VipPlotSpectrogram::contourGrips() const
{
	return d_data->contourGrip;
}

/// \return Levels of the contour lines.
///
/// \sa contourLevels(), renderContourLines(),
///    QwtRasterData::contourLines()
QList<vip_double> VipPlotSpectrogram::contourLevels() const
{
	return d_data->contourLevels;
}

ContourLines VipPlotSpectrogram::contourLines() const
{
	if (d_data->dirtyContourLines) {
		d_data->lines.clear();
		if (d_data->contourLevels.size()) {
			QRectF rect = VipInterval::toRect(VipAbstractScale::scaleIntervals(axes()));
			rect.adjust(-1, -1, 1, 1);

			VipNDArray ar = this->rawData().extract(QRectF(rect), &rect);
			if (ar.isNull())
				return d_data->lines = ContourLines();

			QList<vip_double> levels = d_data->contourLevels;
			std::sort(levels.begin(), levels.end());

			ContourLines lines = contourLines(ar, QRectF(0, 0, ar.shape(1), ar.shape(0)), levels, d_data->ignoreAllVerticesOnLevel);

			// adjust lines according to rect top left
			for (ContourLines::iterator it = lines.begin(); it != lines.end(); ++it) {
				QPolygonF& p = it.value();
				QPointF topLeft = rect.topLeft();

				for (int i = 0; i < p.size(); ++i) {
					p[i] += topLeft;
				}
			}

			d_data->lines = lines;
		}
		d_data->dirtyContourLines = 0;
	}

	return d_data->lines;
}

void VipPlotSpectrogram::setData(const QVariant& v)
{
	d_data->dirtyContourLines = 1;
	VipPlotRasterData::setData(v);
}

void VipPlotSpectrogram::scaleDivChanged()
{
	d_data->dirtyContourLines = 1;
	// VipPlotItem::scaleDivChanged();
}

bool VipPlotSpectrogram::setItemProperty(const char* name, const QVariant& value, const QByteArray& index)
{
	if (value.userType() == 0)
		return false;

	if (strcmp(name, "default-contour-pen") == 0) {
		setDefaultContourPen(value.value<QPen>());
		return true;
	}
	return VipPlotItem::setItemProperty(name, value, index);
}

void VipPlotSpectrogram::draw(QPainter* painter, const VipCoordinateSystemPtr& m) const
{

	VipPlotRasterData::draw(painter, m);

	// draw the contour lines
	ContourLines lines = contourLines();
	painter->setPen(defaultContourPen());
	painter->setBrush(QBrush(Qt::transparent));
	painter->setRenderHint(QPainter::Antialiasing, true);
	for (ContourLines::iterator it = d_data->lines.begin(); it != d_data->lines.end(); ++it) {
		const QPolygonF l = m->transform(it.value());
		painter->drawLines(l.data(), l.size() / 2);
	}
}

VipArchive& operator<<(VipArchive& arch, const VipPlotSpectrogram* value)
{
	arch.content("defaultContourPen", value->defaultContourPen());
	arch.content("ignoreAllVerticesOnLevel", value->ignoreAllVerticesOnLevel());
	QList<vip_double> levels = value->contourLevels();
	for (int i = 0; i < levels.size(); ++i)
		arch.content("level", levels[i]);
	return arch;
}

VipArchive& operator>>(VipArchive& arch, VipPlotSpectrogram* value)
{
	value->setDefaultContourPen(arch.read("defaultContourPen").value<QPen>());
	value->setIgnoreAllVerticesOnLevel(arch.read("ignoreAllVerticesOnLevel").value<bool>());
	QList<vip_double> levels;
	while (true) {
		QVariant tmp = arch.read();
		if (tmp.userType() == 0)
			break;
		levels.append(tmp.toDouble());
	}
	value->setContourLevels(levels);
	arch.resetError();
	return arch;
}

static int registerStreamOperators()
{
	qRegisterMetaType<VipPlotSpectrogram*>();
	vipRegisterArchiveStreamOperators<VipPlotSpectrogram*>();
	return 0;
}

static int _registerStreamOperators = registerStreamOperators();
