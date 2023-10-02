#include "VipProcessingObjectInfo.h"
#include "VipStandardWidgets.h"
#include "VipDisplayArea.h"
#include "VipMimeData.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipProgress.h"
#include "VipTimer.h"
#include "VipTextOutput.h"
#include "VipStandardProcessing.h"

#include <qboxlayout.h>
#include <qtreewidget.h>
#include <qtoolbar.h>
#include <qheaderview.h>
#include <qdrag.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qtooltip.h>
#include <qitemdelegate.h>


static int registerVipProcInfo()
{
	qRegisterMetaType<VipProcInfo>();
	return 0;
}
static int _registerVipProcInfo = registerVipProcInfo();


static double ToDouble(const QVariant & var, bool * ok = NULL)
{
	bool work = false;
	double res = var.toDouble(&work);
	if (work)
	{
		if (ok)*ok = work;
		return res;
	}
	else
	{
		QString str = var.toString();
		QTextStream stream(&str, QIODevice::ReadOnly);
		if ((stream >> res).status() == QTextStream::Ok)
		{
			if (ok)*ok = true;
			return res;
		}
	}

	if (ok) *ok = false;
	return 0;
}



VipExtractAttributeFromInfo::VipExtractAttributeFromInfo(QObject * parent )
	:VipProcessingObject(parent),m_info(NULL)
{}

VipExtractAttributeFromInfo::~VipExtractAttributeFromInfo()
{
	if (m_info)
		delete m_info;
}

void VipExtractAttributeFromInfo::setVipAdditionalInfo(VipAdditionalInfo * info)
{
	if (m_info)
	{
		delete m_info;
		m_info = NULL;
	}
	if (info)
	{
		m_info = info->clone();
	}
}

void VipExtractAttributeFromInfo::setAttributeName(const QString & name)
{
	m_name = name;
	m_exactName.clear();
}
QString VipExtractAttributeFromInfo::attributeName() const
{
	return m_name;
}

void VipExtractAttributeFromInfo::apply()
{
	if (!m_info) {
		setError("no valid info object given");
		return;
	}

	VipAnyData tmp = inputAt(0)->data();
	m_info->inputAt(0)->setData(tmp);
	m_info->update();
	if (m_info->hasError()) {
		setError(m_info->error());
		return;
	}
	VipAnyData any = m_info->outputAt(0)->data();

	const VipProcInfo map = (any.data().userType() == qMetaTypeId<VipProcInfo>()) ?
		any.value<VipProcInfo>() : VipProcInfo().import(any.value<QVariantMap>());

	if (m_exactName.isEmpty())
	{
		for (int i=0; i < map.size(); ++i)
		{
			if (map[i].first.contains(m_name, Qt::CaseSensitive)) {
				m_exactName = map[i].first;
				break;
			}
		}
	}
	if (m_exactName.isEmpty()) {
		setError("wrong attribute name");
		return;
	}

	bool ok = false;
	double v = ToDouble(map[m_exactName], &ok);
	if (ok)
	{
		VipAnyData out = create(v);
		out.setTime(tmp.time());
		outputAt(0)->setData(out);
	}
	else {
		setError("cannot convert attribute value to double");
		return;
	}
}



void VipExtractImageInfos::apply()
{
	VipNDArray ar = inputAt(0)->data().value<VipNDArray>();
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("wrong input data");
		return;
	}

	VipProcInfo map;
	map.append("Global information/Matrix size", QString::number(ar.shape(1)) + "*" + QString::number(ar.shape(0)));
	map.append("Global information/Pixel count", QString::number(ar.shape(1)*ar.shape(0)));

	if (ar.canConvert<double>())
	{
		VipShape sh(QRect(0, 0, ar.shape(1), ar.shape(0)));
		VipShapeStatistics stats = sh.statistics(ar, QPoint(0, 0), NULL, VipShapeStatistics::Maximum | VipShapeStatistics::Minimum | VipShapeStatistics::Mean | VipShapeStatistics::Std);

		map.append("Global information/Image maximum", stats.max);
		map.append("Global information/Image minimum", stats.min);
		map.append("Global information/Image average", stats.average);
		map.append("Global information/Image std. dev.", stats.std);

	}

outputAt(0)->setData(create(QVariant::fromValue(map)));
}



VipExtractShapesInfos::VipExtractShapesInfos(VipAbstractPlayer * pl)
	:VipAdditionalInfo(pl)
{
	if(VipPlayer2D* p = qobject_cast<VipPlayer2D*>(pl))
	{
		connect(p,SIGNAL(sceneModelChanged(VipPlotSceneModel*)),this,SLOT(emitNeedUpdate()));
		connect(p->plotSceneModel()->scene(), SIGNAL(selectionChanged()), this, SLOT(emitNeedUpdate()));
	}
	if (VipPlotPlayer* p = qobject_cast<VipPlotPlayer*>(pl)) {
		connect(p, SIGNAL(timeUnitChanged(const QString&)), this, SLOT(emitNeedUpdate()));
		connect(p->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitNeedUpdate()));
		connect(p->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(emitNeedUpdate()));
	}
}


#if QT_VERSION_MAJOR >= 5 && QT_VERSION_MINOR >= 8
static int area(const QRegion & r)
{
	int res = 0;
	for(QRegion::const_iterator it = r.begin(); it != r.end(); ++it)
	{
		res += it->width()*it->height();
	}
	return res;
}
#endif

void VipExtractShapesInfos::apply()
{
	VipNDArray ar = inputAt(0)->data().value<VipNDArray>();
	if (ar.isEmpty() || ar.shapeCount() != 2) {
		setError("wrong input data");
		return;
	}

	VipProcInfo map;

	//retrieve the shapes
	QList<VipShape> shapes;
	VipAbstractPlayer * pl = player();
	if (!pl || !player()->plotWidget2D())
		return;
	QList<VipPlotShape*> psh = player()->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
	for (int i = 0; i < psh.size(); ++i) {
		VipShape sh = psh[i]->rawData();
		QRectF unused = sh.boundingRect();
		if (VipPlotSceneModel * psm = psh[i]->property("VipPlotSceneModel").value<VipPlotSceneModel*>()) {
			if (VipDisplaySceneModel * disp = psm->property("VipDisplayObject").value<VipDisplaySceneModel*>()) {
				if (disp->inputAt(0)->connection()->source()) {
					sh = disp->inputAt(0)->connection()->source()->data().value<VipSceneModel>().find(sh.identifier()).copy();
					if (!disp->transform().isIdentity())
						sh.transform(disp->transform());
				}
			}
		}
		unused = sh.boundingRect();
		if (!sh.isNull())
			shapes.append(sh);
	}

	if (shapes.isEmpty()) {
		setError("empty input shapes");
		return;
	}

	if (VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(player()))
	{
		//const VipSceneModel sm = pl->plotSceneModel()->sceneModel();
		//QList<VipPlotShape*> shapes = pl->plotSceneModel()->shapes();
		for (int i = 0; i < shapes.size(); ++i)
		{
			//extract shape stats
			VipShape sh = shapes[i];
			QString name = sh.attribute("Name").toString();
			if (name.isEmpty())
				name = sh.group() + " " + QString::number(sh.id()) + " (shape)";
			else
				name += +" (shape)";

			//special case: points
			if (sh.type() == VipShape::Point) {
				QPoint pt = sh.point().toPoint();
				map.append(name + "/Position", QString::number(pt.x()) + ", " + QString::number(pt.y()));
				if (pt.x() >= 0 && pt.y() >= 0 && pt.x() < ar.shape(1) && pt.y() < ar.shape(0)) {
					map.append(name + "/Value", ar.value(vipVector(pt.y(), pt.x())));
				}
			}
			else
			{
				QRect r = sh.boundingRect().toRect();
				map.append(name + "/Bounding rect", "x:" + QString::number(r.left()) + ", y:" + QString::number(r.top()) + ", w:" + QString::number(r.width()) + ", h:" + QString::number(r.height()));
#if QT_VERSION_MAJOR >= 5 && QT_VERSION_MINOR >= 8
				map[name + "/Area"] = QString::number(area(sh.region())) + " pixels�";
#endif
				VipShapeStatistics stats = sh.statistics(ar, QPoint(0, 0), NULL, VipShapeStatistics::Maximum | VipShapeStatistics::Minimum | VipShapeStatistics::Mean | VipShapeStatistics::Std | VipShapeStatistics::PixelCount);
				if (ar.canConvert<double>()) {
					map.append(name + "/Maximum", stats.max);
					map.append(name + "/Minimum", stats.min);
					map.append(name + "/Average", stats.average);
					map.append(name + "/Std. dev.", stats.std);
					map.append(name + "/Pixel count", stats.pixelCount);
				}

				//special case: polyline
				//if (sh.type() == VipShape::Polyline) {
				// QPolygonF p = sh.polyline();
				// double len = 0;
				// for (int i = 1; i < p.size(); ++i) {
				// len += QLineF(p[i - 1].toPoint(), p[i].toPoint()).length();
				// }
				// map[name + "/Length"] = QString::number(len) + " pixels";
				// }
			}

		}
	}
	else if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(player()))
	{
		if(VipPlotArea2D * area = qobject_cast<VipPlotArea2D *>(pl->plotWidget2D()->area()))
		{
			QString xunit = area->bottomAxis()->title().text();
			QString yunit = area->leftAxis()->title().text();

			for (int i = 0; i < shapes.size(); ++i)
			{
				QString tmp = shapes[i].parent() ? shapes[i].parent().attribute("YUnit").toString() : QString();
				if (!tmp.isEmpty()) yunit = tmp;
				VipShape sh = shapes[i];
				QString name = sh.attribute("Name").toString();
				if (name.isEmpty())
					name = sh.group() + " " + QString::number(sh.id()) + " (shape)";
				else
					name += +" (shape)";


				//special case: points
				if (sh.type() == VipShape::Point) {
					QPoint pt = sh.point().toPoint();
					map.append(name + "/Position X", pl->formatXValue(pt.x()) + " " + pl->timeUnit()) ;
					map.append(name + "/Position Y", QString::number(pt.y()) + " " + yunit );
				}
				else
				{
					QRectF r = sh.boundingRect().normalized();
					map.append(name + "/Bounding rect left", pl->formatXValue(r.left()) + " " + pl->timeUnit()) ;
					map.append(name + "/Bounding rect top", QString::number(r.bottom()) + " " + yunit );
					map.append(name + "/Bounding rect right", pl->formatXValue(r.right()) + " " + pl->timeUnit()) ;
					map.append(name + "/Bounding rect bottom", QString::number(r.top()) + " " + yunit );
					map.append(name + "/Bounding rect width", QString::number(r.width() / (double)pl->timeFactor()) + " " + pl->timeUnit() );
					map.append(name + "/Bounding rect height", QString::number(r.height()) + " " + yunit);
				}



			}
		}
	}

	//polyline and polygon: compute the length
	QString tunit;
	double time_factor = 1.;
	if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(player())) {
		time_factor = (double)pl->timeFactor();
		tunit = pl->timeUnit();
	}
	for (int i = 0; i < shapes.size(); ++i) {
		const VipShape sh = shapes[i];
		QString name = sh.attribute("Name").toString();
		if (name.isEmpty()) name = sh.group() + " " + QString::number(sh.id()) + " (shape)";
		else name += +" (shape)";

		if (sh.type() == VipShape::Polyline || sh.type() == VipShape::Polygon) {
			const QPolygonF p = sh.type() == VipShape::Polyline ? sh.polyline() : sh.polygon();
			double len = 0;
			for (int i = 1; i < p.size(); ++i) {
				len += QLineF(QPointF(p[i - 1].x() /time_factor, p[i-1].y()), QPointF(p[i].x()/time_factor,p[i].y())).length();
			}
			map.append(name + "/Border length", QString::number(len) + (tunit.size() ? (" (time: " +tunit +")") : ""));
		}

		QPolygonF p;
		if (sh.type() == VipShape::Polyline) p = sh.polyline();
		else if (sh.type() == VipShape::Point) p << sh.point();
		else p = p = sh.polygon();
		//compute barycentre
		double x = 0, y = 0;
		for(int j=0; j < p.size(); ++j){
			x += p[j].x()/ time_factor; y += p[j].y();
		}

		map.append(name + "/Barycentre X", QString::number(x/p.size()) + (tunit.size() ? (" (time: " + tunit + ")") : ""));
		map.append(name + "/Barycentre Y", QString::number(y / p.size()) );
	}

	//add shape attributes
	for (int i = 0; i < shapes.size(); ++i){
		VipShape sh = shapes[i];
		//get shape name
		QString name = sh.attribute("Name").toString();
		if (name.isEmpty()) name = sh.group() + " " + QString::number(sh.id()) + " (shape)";
		else name += +" (shape)";
		const QVariantMap attrs = sh.attributes();
		for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
			if(!it.key().startsWith("_vip_")) //private properties
				map.append(name + "/" + it.key(), it.value());
	}

	outputAt(0)->setData(create(QVariant::fromValue(map)));
}



VipExtractCurveInfos::VipExtractCurveInfos(VipPlotPlayer * pl)
	:VipAdditionalInfo(pl)
{
	connect(pl, SIGNAL(sceneModelChanged(VipPlotSceneModel*)), this, SLOT(emitNeedUpdate()));
	connect(pl->plotSceneModel()->scene(), SIGNAL(selectionChanged()), this, SLOT(emitNeedUpdate()));

	//update the info when the time scale changes or when the time unit changes
	if(VipPlotArea2D * area = qobject_cast<VipPlotArea2D*>(pl->plotWidget2D()->area()))
		connect(area->bottomAxis(), SIGNAL(scaleDivChanged(bool)), this, SLOT(emitNeedUpdate()));

	connect(pl,SIGNAL(timeUnitChanged(const QString &)),this, SLOT(emitNeedUpdate()));
	connect(pl->verticalWindow()->rawData().shapeSignals(), SIGNAL(sceneModelChanged(const VipSceneModel&)), this, SLOT(emitNeedUpdate()));
	connect(pl->verticalWindow(), SIGNAL(visibilityChanged(VipPlotItem*)), this, SLOT(emitNeedUpdate()));
}

void VipExtractCurveInfos::apply()
{
	VipAnyData any = inputAt(0)->data();
	VipPointVector pt = any.value<VipPointVector>();
	if (pt.isEmpty()) {
		setError("Empty input data");
		return;
	}

	QString name = any.name();
	VipProcInfo map;

	if (VipPlotPlayer * pl = qobject_cast<VipPlotPlayer*>(player()))
	{
		//extract union shape
		VipShape shape;
		QList<VipPlotShape*> shapes = player()->plotWidget2D()->area()->findItems<VipPlotShape*>(QString(), 1, 1);
		for (int i = 0; i < shapes.size(); ++i){
			VipShape sh = shapes[i]->rawData();
			if (sh.type() == VipShape::Polygon || sh.type() == VipShape::Path)
			{
				if (shape.isNull())
					shape = sh.copy();
				else
					shape.unite(sh);
			}
		}

		if(!shape.isNull())
			name += " (curve-sub ROI)";
		else
			name += " (curve)";

		map.addToolTip(name, "<b>" + name + "</b><br>"
			"Displays curve statistics for the part of the curve which is inside the X (time) scale");

		//use the player scale boundaries
		VipAbstractScale * x = pl->xScale();
		if (!x)
			return;

		QPainterPath p_shape;
		p_shape.addRect(QRectF(x->scaleDiv().bounds().minValue(), 0,x->scaleDiv().bounds().width(), 1));

		//extract curve
		QList<VipPointVector> vectors;
		QList<bool> continuous;
		bool full_continuous, sub_continuous;
		QList<VipInterval> r;
		if (shape.isNull())
			r = VipPlotCurve::dataBoundingRect(pt, vectors, continuous, full_continuous, sub_continuous, p_shape, 1);
		else
			r = VipPlotCurve::dataBoundingRect(pt, vectors, continuous, full_continuous, sub_continuous, shape.shape(), 1);


		int count = 0;
		//double area = full_continuous ? 0 : vipNan();
		//double len = 0;
		QVector<vip_double> points;
		for (int i = 0; i < vectors.size(); ++i)
		{
			//compute point count
			count += vectors[i].size();
			if (full_continuous)
			{
				//compute length and area
				const VipPointVector vec = vectors[i];
				if(vec.size())
					points.append(vec[0].y());
				for (int j = 1; j < vec.size(); ++j)
				{
					points.append(vec[j].y());
					//QLineF line(vec[j - 1], vec[j]);
					// len += line.length();
					// double min_y = qMin(line.p1().y(), line.p2().y());
					// double max_y = qMax(line.p1().y(), line.p2().y());
					// double w = line.p2().x() - line.p1().x();
					// if (min_y >= 0) {
					// area += min_y * w + (max_y - min_y)*w*0.5;
					// }
					// else if (max_y > 0) {
					// area += (min_y + max_y)*w;
					// }
					// else {
					// area += max_y * w + (min_y - max_y)*w*0.5;
					// }
				}
			}
		}

		//static double year2000ns = QDateTime::fromString("2000","yyyy").toMSecsSinceEpoch()* 1000000;

		map.append(name + "/Point number", count);
		map.append(name + "/X minimum", pl->formatXValue(r[0].minValue()));//r.left() > year2000ns ? QDateTime::fromMSecsSinceEpoch(r.left()/1000000).toString("dd/MM/yyyy hh:mm:ss.zzz"): QString::number(r.left());
		map.append(name + "/X maximum", pl->formatXValue(r[0].maxValue()));//r.left() > year2000ns ? QDateTime::fromMSecsSinceEpoch(r.right()/1000000).toString("dd/MM/yyyy hh:mm:ss.zzz"): QString::number(r.right());
		map.append(name + "/Y minimum", QVariant::fromValue( r[1].minValue()));
		map.append(name + "/Y maximum", QVariant::fromValue(r[1].maxValue()));
		if (full_continuous && points.size())
		{
			VipShape sh(QRect(0, 0, points.size(), 1));
			VipShapeStatistics stats = sh.statistics(VipNDArrayTypeView<vip_double>(points.data(), vipVector(1, points.size())), QPoint(0, 0), NULL, VipShapeStatistics::Mean | VipShapeStatistics::Std);
			map.append(name + "/Y average", stats.average);
			map.append(name + "/Y std. dev.", stats.std);
		}
		//map.append(name + "/Length", len);
		// if(full_continuous)
		// map.append(name + "/Integral", QString::number(area) + " ns." + (any.yUnit().isEmpty() ? "y" : any.yUnit()));
	}

	outputAt(0)->setData(create(QVariant::fromValue(map)));
}


void VipExtractHistogramInfos::apply()
{
	VipAnyData any = inputAt(0)->data();
	VipIntervalSampleVector pt = any.value<VipIntervalSampleVector>();
	if (pt.isEmpty()) {
		setError("Empty input data");
		return;
	}

	QString name = any.name() + " (histogram)";
	VipProcInfo map;
	map.addToolTip(name, "<b>" + name + "</b><br>Displays information on the full histogram");

	if (//VipPlotPlayer * pl = 
qobject_cast<VipPlotPlayer*>(player()))
	{
		if (pt.size() > 0)
		{
			double sum = pt.first().value;
			double min = pt.first().value;
			double max = pt.first().value;
			VipInterval max_interval = pt.first().interval;
			for (int i = 1; i < pt.size(); ++i)
			{
				sum += pt[i].value;
				min = qMin(min, (double)pt[i].value);
				if (pt[i].value > max) {
					max = pt[i].value;
					max_interval = pt[i].interval;
				}
			}
			double avg = sum / pt.size();

			map.append(name + "/Sum value", sum);
			map.append(name + "/Maximum value", max);
			map.append(name + "/Minimum value", min);
			map.append(name + "/Average value", avg);
			map.append(name + "/Maximum value interval", QString::number((double)max_interval.minValue()) + " -> " + QString::number((double)max_interval.maxValue()));
		}
	}

	outputAt(0)->setData(create(QVariant::fromValue(map)));
}








VipFunctionDispatcher<3> & VipFDProcessingOutputInfo()
{
	static VipFunctionDispatcher<3> disp;
	return disp;
}


static VipExtractImageInfos* extractImageInfos(VipVideoPlayer * p, VipOutput *, const QVariant &)
{
	return new VipExtractImageInfos(p);
}
static VipExtractShapesInfos* extractShapesInfos(VipAbstractPlayer * p, VipOutput *,const QVariant &)
{
	return new VipExtractShapesInfos(p);
}
static VipExtractCurveInfos* extractCurveInfos(VipPlotPlayer * p, VipOutput *,const VipPointVector &)
{
	return new VipExtractCurveInfos(p);
}
static VipExtractHistogramInfos* extractHistInfos(VipPlotPlayer * p, VipOutput *,const VipIntervalSampleVector &)
{
	return new VipExtractHistogramInfos(p);
}

static int registerAdditionalInfos()
{
	VipFDProcessingOutputInfo().append<VipExtractImageInfos*(VipVideoPlayer *, VipOutput *,const QVariant &)>(extractImageInfos);
	VipFDProcessingOutputInfo().append<VipExtractShapesInfos*(VipAbstractPlayer *, VipOutput *,const QVariant &)>(extractShapesInfos);
	VipFDProcessingOutputInfo().append<VipExtractCurveInfos*(VipPlotPlayer *, VipOutput *,const VipPointVector &)>(extractCurveInfos);
	VipFDProcessingOutputInfo().append<VipExtractHistogramInfos*(VipPlotPlayer *, VipOutput *,const VipIntervalSampleVector &)>(extractHistInfos);
	return 0;
}
static int _registerAdditionalInfos = vipAddInitializationFunction( registerAdditionalInfos);









class BorderItemDelegate : public QItemDelegate
{
public:

	QPen pen;

	BorderItemDelegate(QObject * parent)
		:QItemDelegate(parent),pen(Qt::lightGray)
	{}

	virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
	{
		QSize size = QItemDelegate::sizeHint(option, index);
		if (!(pen.color().alpha() == 0 || pen.style() == Qt::NoPen))
		{
			int width = qMax(pen.width(), 1);
			size = size + QSize(2 * width, 2 * width);
		}
		return size;
	}

	virtual void	paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
	{
		QItemDelegate::paint(painter, option, index);

		if (!index.parent().isValid())
			return;

		QRect rect = option.rect;
		if (!(pen.color().alpha() == 0 || pen.style() == Qt::NoPen))
		{
			//int width = qMax(pen.width(), 1);

			if(index.column() == 1)
				rect = option.rect.adjusted(0, 0, -1, 0);

			painter->save();
			painter->setPen(pen);
			painter->drawRect(rect);
			painter->restore();
		}
	}
};


class InfoTreeWidgetItem : public QTreeWidgetItem
{
public:
	InfoTreeWidgetItem() : output(NULL){}
	VipOutput * output;
};

class InfoTreeWidget : public QTreeWidget
{
	int m_row_height;
public:

	BorderItemDelegate * delegate;

	InfoTreeWidget() : QTreeWidget(), m_row_height(0)
	{
		setItemDelegate(delegate = new BorderItemDelegate(this));
	}

	QSize sizeHint() const
	{
		//optimized version of sizeHint()

		if (m_row_height == 0)
		{
			if(topLevelItemCount())
				const_cast<int&>(m_row_height) = rowHeight(indexFromItem(this->topLevelItem(0), 0))+2;
		}
		if (m_row_height)
		{
			int h = 0, w = 0;
			for (int i = 0; i < columnCount(); ++i)
				w += sizeHintForColumn(i);

			for (int i = 0; i < topLevelItemCount(); ++i)
			{
				QTreeWidgetItem * top = this->topLevelItem(i);
				h += m_row_height;
				if (top->isExpanded())
				{
					for (int i = 0; i < top->childCount(); ++i)
						h += m_row_height;
				}
			}
			return QSize(w, h);
		}
		else
			return QTreeWidget::sizeHint();

		return QTreeWidget::sizeHint();
	}

	QSize itemSizeHint(QTreeWidgetItem * item,int ) const
	{
		if (!item->isHidden())
		{
			int h = 0, w = 0;
			for (int i = 0; i < columnCount(); ++i)
			{
				w += sizeHintForColumn(i);
				h = qMax(h,rowHeight(indexFromItem(item, i)) + 3);
			}

			if (item->isExpanded())
			{
				for (int i = 0; i < item->childCount(); ++i)
				{
					QSize s = itemSizeHint(item->child(i),i);
					h += s.height();
					w = qMax(w, s.width());
				}
			}
			return QSize(w, h);
		}
		return QSize(0, 0);
	}

	QTreeWidgetItem * findTopLevel(const QString & name) const
	{
		for (int i = 0; i < topLevelItemCount(); ++i)
			if (topLevelItem(i)->text(0) == name)
				return topLevelItem(i);
		return NULL;
	}

	virtual void	mouseMoveEvent(QMouseEvent * evt)
	{
		if (evt->buttons() & Qt::LeftButton)
		{
			QDrag drag(this);

			//create the mime data
			QMimeData * mime = new VipMimeDataLazyEvaluation<QList<VipProcessingObject*> >(
				std::bind(&VipProcessingObjectInfo::plotSelectedAttributes, vipGetProcessingObjectInfo()),
				VipCoordinateSystem::Cartesian,
				this
				);

			drag.setMimeData(mime);
			drag.exec();
		}
	}

	virtual void	dragMoveEvent(QDragMoveEvent * evt)
	{
		const QMimeData * mime = evt->mimeData();
		if (qobject_cast<const VipMimeDataCoordinateSystem*>(mime))
		{
			evt->acceptProposedAction();
		}
	}

	virtual void	dragEnterEvent(QDragEnterEvent * evt)
	{
		const QMimeData * mime = evt->mimeData();
		if (qobject_cast<const VipMimeDataCoordinateSystem*>(mime))
		{
			evt->acceptProposedAction();
		}
	}

	virtual void dropEvent(QDropEvent * evt)
	{
		evt->setAccepted(false);
	}
};


class VipProcessingObjectInfo::PrivateData
{
public:
	PrivateData() : output(NULL), lastUpdate(0), plotting(0)
	{
		filters.append(QPair<QString, QVector<QString> >("Global information", QVector<QString>() << "name" << "unit" << "title" << "date" << "size"<<"duration"));
		filters.append(QPair<QString, QVector<QString> >("Camera", QVector<QString>() << "Filter" << "Camera" << "Focal" << "Focus" << "Field of"<<"Detector"<<"Lens"));
		filters.append(QPair<QString, QVector<QString> >("GPS", QVector<QString>() << "GPS"));
		filters.append(QPair<QString, QVector<QString> >("Orientation", QVector<QString>() << "yaw" << "pitch" << "roll"));
		filters.append(QPair<QString, QVector<QString> >("Parameters", QVector<QString>()));
	}

	QPointer<VipAbstractPlayer> player;
	QPointer<VipProcessingObject> processing;
	VipOutput * output;

	QToolBar tools;
	InfoTreeWidget attributes;
	QLineEdit search;
	VipTimer searchTimer;

	QVector<QPair<QString, QVector<QString> > > filters;
	QMap<QString, QVariantMap> categories;

	QList<QPointer<VipAdditionalInfo> > infos;

	VipTimer timer;
	qint64 lastUpdate;

	int plotting;
};

VipProcessingObjectInfo::VipProcessingObjectInfo(VipMainWindow * window)
	:VipToolWidgetPlayer(window)
{
	setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

	m_data = new PrivateData();

	setWindowTitle("Player properties");
	setObjectName("Processing infos");

	m_data->timer.setSingleShot(true);
	m_data->timer.setInterval(200);

	m_data->searchTimer.setSingleShot(true);
	m_data->searchTimer.setInterval(300);

	m_data->attributes.setAcceptDrops(true);
	m_data->attributes.header()->show();
	m_data->attributes.setColumnCount(2);
	m_data->attributes.setColumnWidth(0, 100);
	m_data->attributes.setColumnWidth(1, 100);
	m_data->attributes.setHeaderLabels(QStringList() << "Name" << "Value");
	m_data->attributes.setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_data->attributes.setFrameShape(QFrame::NoFrame);
	m_data->attributes.setIndentation(10);
	//m_data->attributes.setStyleSheet("QTreeWidget {background: transparent;}");

	m_data->search.setPlaceholderText("Search property");

	connect(m_data->tools.addAction(vipIcon("copy.png"), "Copy content to clipboard"), SIGNAL(triggered(bool)), this, SLOT(copyToClipboard()));
	connect(m_data->tools.addAction(vipIcon("save.png"), "Save content to file..."), SIGNAL(triggered(bool)), this, SLOT(saveToFile()));
	m_data->tools.addSeparator();
	connect(m_data->tools.addAction(vipIcon("plot.png"), "Plot time trace of selected parameters"), SIGNAL(triggered(bool)), this, SLOT(plotSelection()));
	m_data->tools.setIconSize(QSize(18, 18));

	m_data->tools.addWidget(&m_data->search);

	QVBoxLayout * lay = new QVBoxLayout();

	lay->addWidget(&m_data->tools);
	lay->addWidget(&m_data->attributes);
	//lay->setContentsMargins(0,0,0,0);

	QWidget * w = new QWidget();
	w->setLayout(lay);
	this->setWidget(w);
	this->setMinimumWidth(250);
	this->setAutomaticTitleManagement(false);

	connect(&m_data->timer, SIGNAL(timeout()), this, SLOT(updateInfos()),Qt::QueuedConnection);
	connect(&m_data->attributes, SIGNAL(itemExpanded(QTreeWidgetItem *)), this, SLOT(resetSize()));
	connect(&m_data->attributes, SIGNAL(itemCollapsed(QTreeWidgetItem *)), this, SLOT(resetSize()));
	connect(&m_data->search, SIGNAL(textChanged(const QString &)), &m_data->searchTimer, SLOT(start()));
	connect(&m_data->searchTimer, SIGNAL(timeout()), this, SLOT(search()), Qt::QueuedConnection);


	connect(vipGetMainWindow()->displayArea(), SIGNAL(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)), this, SLOT(currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*)));
	connect(vipGetMainWindow()->displayArea(), SIGNAL(topLevelWidgetClosed(VipDisplayPlayerArea *, VipMultiDragWidget *)), this, SLOT(topLevelWidgetClosed(VipDisplayPlayerArea *, VipMultiDragWidget *)));
}

VipProcessingObjectInfo::~VipProcessingObjectInfo()
{
	m_data->timer.stop();
	if (m_data->processing)
		disconnect(m_data->processing.data(), SIGNAL(dataSent(VipProcessingIO *, const VipAnyData &)), this, SLOT(updateInfos(VipProcessingIO *, const VipAnyData &)));

	delete m_data;
}


bool VipProcessingObjectInfo::setPlayer(VipAbstractPlayer * player)
{
	if (m_data->player)
		if (VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(m_data->player))
			disconnect(pl->plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem *)), this, SLOT(itemSelected(VipPlotItem*)));

	if (!player)
		return false;

	if (player)
		if (VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(player))
			connect(pl->plotWidget2D()->area(), SIGNAL(childSelectionChanged(VipPlotItem *)), this, SLOT(itemSelected(VipPlotItem*)));


	if (VipVideoPlayer * pl = qobject_cast<VipVideoPlayer*>(player))
	{
		m_data->player = player;
		//set the spectrogram as the information source
		this->setPlotItem(pl->spectrogram());
	}
	else
	{
		//get all selected items in selection order
		QList<VipDisplayObject*> displays;
		QList<VipPlotItem*> items;
		if (player->plotWidget2D())
			items = vipCastItemListOrdered<VipPlotItem*>(player->plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1), QString(), 2, 2);
		for (int i = 0; i < items.size(); ++i)
			if (VipDisplayObject * d = items[i]->property("VipDisplayObject").value<VipDisplayObject*>())
				displays.append(d);

		if (displays.isEmpty()) {
			//if we are on the same player with already a valid processing object displayed and no item selected, do nothing
			if (m_data->player == player && m_data->processing)
				return true;
			//try to take the main display object (if any)
			if (VipDisplayObject * disp = player->mainDisplayObject())
				displays << disp;
			else
				displays = player->displayObjects();
		}
		m_data->player = player;
		for (int i = 0; i < displays.size(); ++i)
		{
			if (VipOutput * src = displays[i]->inputAt(0)->connection()->source())
			{
				if(displays[i]->isVisible()) {
					setProcessingObject(src->parentProcessing(), src);
					break;
				}
			}
		}
	}

	return true;
}

void VipProcessingObjectInfo::setProcessingObject(VipProcessingObject * obj, VipOutput * output)
{
	if (obj)
	{
		//for VipDisplayObject, use the source VipProcessingObject
		if (VipDisplayObject * display = qobject_cast<VipDisplayObject*>(obj)){
			if (VipOutput * src = display->inputAt(0)->connection()->source()){
				setProcessingObject(src->parentProcessing(), src);
				return;
			}
		}

		//set the window title
		QString title;
		if (obj->outputCount() == 1)
			title = obj->outputAt(0)->data().name();
		else if (obj->inputCount() == 1)
			title = obj->inputAt(0)->probe().name();
		if (title.isEmpty()) {
			if (m_data->player)
				title = QString::number(m_data->player->parentId()) + " " + m_data->player->QWidget::windowTitle();
		}
		if (title.isEmpty())
			title = vipSplitClassname(obj->objectName());
		this->setWindowTitle("Player properties - " + title);

		//update the list of VipAdditionalInfo
		for (int i = 0; i < m_data->infos.size(); ++i)
			if(m_data->infos[i])
				delete m_data->infos[i].data();
		m_data->infos.clear();
		const auto funs = VipFDProcessingOutputInfo().match(m_data->player.data(),output, output->data().data());
		for (int i = 0; i < funs.size(); ++i)
			if (VipAdditionalInfo * info = funs[i](m_data->player.data(),output, output->data().data()).value<VipAdditionalInfo*>())
			{
				info->setProperty("_vip_no_serialize", true);
				m_data->infos.append(info);
				connect(info,SIGNAL(needUpdate()),&m_data->timer,SLOT(start()));
			}

		if (m_data->processing)
		{
			disconnect(m_data->processing.data(), SIGNAL(dataSent(VipProcessingIO *, const VipAnyData &)), this, SLOT(updateInfos(VipProcessingIO *, const VipAnyData &)));
		}

		m_data->processing = obj;
		m_data->output = output;

		connect(m_data->processing.data(), SIGNAL(dataSent(VipProcessingIO *, const VipAnyData &)), this, SLOT(updateInfos(VipProcessingIO *, const VipAnyData &)), Qt::DirectConnection);
		updateInfos();
	}
}

void VipProcessingObjectInfo::setPlotItem(VipPlotItem * item)
{
	if (!item)
		return;

	if(VipDisplayObject * display = item->property("VipDisplayObject").value<VipDisplayObject*>())
		if (VipOutput * src = display->inputAt(0)->connection()->source())
		{
			setProcessingObject(src->parentProcessing(), src);
		}
}


void VipProcessingObjectInfo::itemSelected(VipPlotItem* plot_item)
{

	//add the plot item from the list if: this is a left click, the item is selected and not already added to the list
	if (plot_item && plot_item->isSelected() )
	{
		setPlotItem(plot_item);
	}
	else if (plot_item)
	{
		//the plot item has been unselected
		//find another valid selected plot_item
		if (VipPlayer2D * pl = qobject_cast<VipPlayer2D*>(m_data->player))
		{
			QList<VipPlotItem*> items = pl->plotWidget2D()->area()->findItems<VipPlotItem*>(QString(), 1, 1);
			if (items.size())
			{
				for (int i = 0; i < items.size(); ++i) {
					if (VipDisplayObject * display = items[i]->property("VipDisplayObject").value<VipDisplayObject*>())
						if (VipOutput * src = display->inputAt(0)->connection()->source())
						{
							setProcessingObject(src->parentProcessing(), src);
						}
				}
			}
			else if(m_data->player)
			{
				//check if the current processing is within this player
				QList<VipDisplayObject*> displays = m_data->player->displayObjects();
				VipProcessingObject * first = NULL;
				VipOutput * src = NULL;
				for (int i = 0; i < displays.size(); ++i) {
					if (VipOutput * s = displays[i]->inputAt(0)->connection()->source()) {
						if (!first) {
							src = s;
							first = s->parentProcessing();
						}
						if (s->parentProcessing() == m_data->processing)
							return; //the current displayed processing is within this player, nothing to do
					}
				}

				//find the first VipDisplayObject having a valid source processing object, and use this source to display informations
				if(first)
					setProcessingObject(first, src);
			}
		}
	}
}

QString VipProcessingObjectInfo::content() const
{
	//create the string with the INI format
	QString res;
	for (int i = 0; i < m_data->attributes.topLevelItemCount(); ++i)
	{
		QTreeWidgetItem * top = m_data->attributes.topLevelItem(i);
		if (!res.isEmpty())
			res += "\n";

		res += "[" + top->text(0) + "]\n";

		for (int j = 0; j < top->childCount(); ++j)
		{
			QTreeWidgetItem * item = top->child(j);
			if (!item->text(1).isEmpty())
				res += item->text(0) + " = " + item->text(1) + "\n";
		}
	}
	return res;
}

void VipProcessingObjectInfo::copyToClipboard()
{
	QApplication::clipboard()->setText(content());
	QPoint pos = m_data->tools.mapToGlobal(QPoint(0,0));
	QToolTip::showText(pos, "Processing information copied to clipboard");
}

void VipProcessingObjectInfo::saveToFile()
{
	QString filename = VipFileDialog::getSaveFileName(NULL, "Save processing information", "INI file (*.ini)");
	if (!filename.isEmpty())
	{
		QFile out(filename);
		if (out.open(QFile::WriteOnly | QFile::Text))
		{
			out.write(content().toLatin1());
		}
	}
	QPoint pos = m_data->tools.mapToGlobal(QPoint(0, 0));
	QToolTip::showText(pos, "Processing information saved to file");
}

QPen VipProcessingObjectInfo::itemBorderPen() const
{
	return m_data->attributes.delegate->pen;
}
void VipProcessingObjectInfo::setItemBorderPen(const QPen & pen)
{
	m_data->attributes.delegate->pen = pen;
	update();
}
QColor VipProcessingObjectInfo::itemBorderColor() const
{
	return m_data->attributes.delegate->pen.color();
}
void VipProcessingObjectInfo::setItemBorderColor(const QColor & c)
{
	m_data->attributes.delegate->pen.setColor(c);
	update();
}

QList<VipProcessingObject*> VipProcessingObjectInfo::plotSelectedAttributes()
{
	m_data->plotting = 1;
	if (!m_data->processing || !m_data->player) {
		m_data->plotting = 0;
		return QList<VipProcessingObject*>();
	}

	//first retrieve all VipOutput to plot with their attribute name
	QList<QTreeWidgetItem*> items = m_data->attributes.selectedItems();
	QList<VipOutput*> outputs;
	QStringList attributes;

	qint64 time = VipInvalidTime;
	VipProcessingPool * pool = NULL;

	for (int i = 0; i < items.size(); ++i)
	{
		if (VipOutput * out = static_cast<InfoTreeWidgetItem*>(items[i])->output)
		{
			if (!pool) {
				pool = out->parentProcessing()->parentObjectPool();
				if (pool)
					time = pool->time();
			}

			outputs.append(out);
			attributes.append(items[i]->text(0));
		}
	}

	//by default we can take the current workspace pool
	if (!pool)
		if (vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
			pool = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea()->processingPool();


	QList<VipProcessingObject*> res=  plotAttributes(outputs, attributes,pool);
	m_data->plotting = 0;

	//reload time
	if (pool)
		pool->read(time);

	return res;
}

void VipProcessingObjectInfo::plotSelection()
{
	if (VipDisplayPlayerArea * disp = vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
	{
		//extract time trace
		QList<VipProcessingObject*> curves = this->plotSelectedAttributes();

		//plot the result
		QList<VipAbstractPlayer*> players = vipCreatePlayersFromProcessings((curves), NULL);
		VipBaseDragWidget * drag = vipCreateFromWidgets(vipListCast<QWidget*>(players));

		//display the new widget
		if (VipMultiDragWidget * w = vipCreateFromBaseDragWidget(drag))
		{
			disp->addWidget(w);
		}
	}

}

void VipProcessingObjectInfo::updateInfos(VipProcessingIO * out, const VipAnyData &)
{
	if (this->isHidden())
		return;

	if (m_data->output && m_data->output != out)
		return;

	//schedule an update in 200ms
	m_data->timer.start();

	//if last update is older than 200ms and and update in not in progress, update
	if (m_data->lastUpdate < 0)
		return;

	if (QDateTime::currentMSecsSinceEpoch() - m_data->lastUpdate > 200)
	{
		m_data->lastUpdate = -1;
		m_data->timer.stop();
		QMetaObject::invokeMethod(this, "updateInfos", Qt::QueuedConnection);
	}

}

static bool Filter(const QString & name, const QVector<QString> & filters)
{
	if (filters.isEmpty())
		return true; //null filter
	for (int i = 0; i < filters.size(); ++i)
		if (name.contains(filters[i], Qt::CaseInsensitive))
			return true;
	return false;
}

void VipProcessingObjectInfo::updateInfos()
{
	if (m_data->plotting)
		return;
	if (!m_data->output)
		return;
	if (!m_data->processing)
		return;
	if (m_data->processing->indexOf(m_data->output) < 0)
		return;

	QSize previous_size = m_data->attributes.sizeHint();

	m_data->attributes.blockSignals(true);

	//get the VipOutput
	QList<VipOutput*> outs;
	if (VipProcessingObject * obj = m_data->processing)
	{
		//retrieve VipOutput objects
		if (m_data->output)
			outs << m_data->output;
		else
		{
			for (int i = 0; i < obj->outputCount(); ++i)
				outs << obj->outputAt(i);
		}
	}

	//compute all attributes
	QMap<QString, VipProcInfo> attributes;
	QMap<QString, QList<QPair<QString,VipOutput*> > > outputs;
	VipProcInfo filters_tooltips;

	for (int o = 0; o < outs.size(); ++o)
	{
		const QVariantMap attrs = outs[o]->data().attributes();
		//QStringList keys = attrs.keys();
		//for (int i = 0; i < keys.size(); ++i)
		//	printf("%s\n", keys[i].toLatin1().data());
		for (QVariantMap::const_iterator it = attrs.begin(); it != attrs.end(); ++it)
		{
			const QString key = it.key();
			//printf("%s\n", key.toLatin1().data());
			for (int i = 0; i < m_data->filters.size(); ++i)
			{
				if (Filter(key, m_data->filters[i].second))
				{
					//check value size when converted to string
					int size = 0;
					const QVariant v = it.value();

					if (v.userType() == QMetaType::QString) size = v.value<QString>().size();
					else if (v.userType() == QMetaType::QByteArray) size = v.value<QByteArray>().size();
					else size = v.toString().size();
					if (size) {
						attributes[m_data->filters[i].first].append(it.key(), it.value());
						outputs[m_data->filters[i].first].append(QPair<QString, VipOutput*>(it.key(), outs[o]));
						break;
					}
				}
			}
		}
	}

	QVector<QPair<QString, QVector<QString> > > filters = m_data->filters;

	//compute additional attributes
	for (int i = 0; i < m_data->infos.size(); ++i)
	{
		if (!m_data->infos[i])
			continue;
		//set the VipAdditinalInfo processing pool, we need it to plot the time evolution
		//if (m_data->processing)
		// m_data->infos[i]->setParent(m_data->processing->parentObjectPool());
		// else if (m_data->player)
		// m_data->infos[i]->setParent(m_data->player->processingPool());

		m_data->infos[i]->inputAt(0)->setData(m_data->output->data());
		m_data->infos[i]->update();
		const VipAnyData any = m_data->infos[i]->outputAt(0)->data();
		const VipProcInfo map = any.data().userType() == qMetaTypeId<VipProcInfo>() ? any.value<VipProcInfo>() : VipProcInfo().import(any.value<QVariantMap>());
		for (int m = 0; m < map.size(); ++m)
		{
			const VipProcInfo::info info = map[m];

			//split the key based on "/" character
			QStringList lst;
			int index = info.first.lastIndexOf("/");
			if(index > 0 && index < info.first.size()-1)
				lst << info.first.mid(0,index) << info.first.mid(index+1);
			else
				lst << info.first;

			if (lst.size() != 2 ) {
				if(lst.size())
					lst = QStringList() << "Global information" << lst[0];
				else
					lst = QStringList() << "Global information" << info.first;
			}
			filters.append(QPair<QString, QVector<QString> >(lst[0],QVector<QString>()));
			filters_tooltips.addToolTip(lst[0], map.toolTip(lst[0]));
			attributes[lst[0]].append(lst[1], info.second);
			attributes[lst[0]].addToolTip(lst[1], map.toolTip(info.first));
			outputs[lst[0]].append(QPair<QString, VipOutput*>(lst[1], m_data->infos[i]->outputAt(0)));
		}
	}

	bool modified = false;

	//get the list of expanded items
	QMap<QString, bool> expanded;
	for (int i = 0; i < m_data->attributes.topLevelItemCount(); ++i)
		expanded[m_data->attributes.topLevelItem(i)->text(0)] = m_data->attributes.topLevelItem(i)->isExpanded();

	//update nodes
	int index = 0;
	for (int i = 0; i < filters.size(); ++i)
	{
		const QString category = filters[i].first;

		//find or create top level item with the right name
		QTreeWidgetItem * item = m_data->attributes.findTopLevel(category);
		if (!item) {
			m_data->attributes.insertTopLevelItem(index, item = new QTreeWidgetItem(QStringList() << category));
			item->setToolTip(0,category);
			modified = true;
		}

		item->setToolTip(0, filters_tooltips.toolTip(category));
		item->setToolTip(1, item->toolTip(0));

		index = m_data->attributes.invisibleRootItem()->indexOfChild(item) +1;

		//add attributes in existing nodes if possible
		const VipProcInfo map = attributes[category];
		const QList<QPair<QString, VipOutput*> > out = outputs[category];

		for (int pos = 0; pos < map.size(); ++pos)
		{
			InfoTreeWidgetItem * child;
			if (item->childCount() <= pos)
			{
				item->addChild(child = new InfoTreeWidgetItem());
				QFont font = item->font(0);
				font.setBold(true);
				child->setFont(0, font);
				modified = true;
			}
			else
				child = static_cast<InfoTreeWidgetItem*>(item->child(pos));

			child->setText(0, map[pos].first);
			QString value = map[pos].second.toString();
			if (value.size() > 100)
				value = value.mid(0, 97) + "...";
			child->setText(1, value);
			child->setToolTip(0, map.toolTip(map[pos].first));
			QString tip = map[pos].second.toString();
			//make sure tool tip does not exceed 2000 characters or else, possible crash
			if (tip.size() > 2000)
				tip = tip.mid(0, 2000) + "...";
			child->setToolTip(1, tip);
			child->output = out[pos].second; //set the VipOutput as data to retrieve it when plotting time trace

			//printf("'%s' '%s'\n", it.key().toLatin1().data(), it.value().toString().toLatin1().data());

			bool to_double = false;
			ToDouble(map[pos].second, &to_double);
			if (!to_double)
				child->setFlags(item->flags() & (~Qt::ItemIsSelectable) & (~Qt::ItemIsDragEnabled));
		}

		//remove additional children
		while (item->childCount() > map.size()) {
			delete item->takeChild(item->childCount() - 1);
			modified = true;
		}
	}

	const QStringList categories = attributes.keys();
	//remove top level items that do not belong to a category
	for (int i = 0; i < m_data->attributes.topLevelItemCount(); ++i)
	{
		QTreeWidgetItem * item = m_data->attributes.topLevelItem(i);
		if (!categories.contains(item->text(0))) {
			delete m_data->attributes.takeTopLevelItem(i);
			modified = true;
			--i;
		}
		else {
			//restore expanded state (true by default)
			QMap<QString, bool>::const_iterator found = expanded.find(item->text(0));
			item->setExpanded((found != expanded.end() ? found.value() : true));
			//hide if necessary
			item->setHidden(item->childCount() == 0);
		}
	}

	if (modified)
	{
		m_data->attributes.blockSignals(false);

		if (m_data->attributes.sizeHint() != previous_size)
			resetSize();
	}

	m_data->attributes.blockSignals(false);
	m_data->lastUpdate = QDateTime::currentMSecsSinceEpoch();
}

void VipProcessingObjectInfo::showEvent(QShowEvent * evt)
{
	VipToolWidgetPlayer::showEvent(evt);

	if (m_data->output && m_data->processing)
	{
		setProcessingObject(m_data->processing, m_data->output);
	}

	updateInfos();
}

void VipProcessingObjectInfo::search()
{
	QRegExp exp(m_data->search.text());
	exp.setCaseSensitivity(Qt::CaseInsensitive);
	exp.setPatternSyntax(QRegExp::Wildcard);

	bool restore = m_data->search.text().isEmpty();

	for (int i = 0; i < m_data->attributes.topLevelItemCount(); ++i)
	{
		QTreeWidgetItem * item = m_data->attributes.topLevelItem(i);
		if (restore)
			item->setHidden(item->childCount() == 0);

		bool found = false;
		for (int j = 0; j < item->childCount(); ++j)
		{
			QTreeWidgetItem * child = item->child(j);
			if(restore)
				child->setHidden(false);
			else {
				if (exp.indexIn(child->text(0)) >= 0 || exp.indexIn(child->text(1)) >= 0) {
					child->setHidden(false);
					found = true;
				}
				else
					child->setHidden(true);
			}
		}

		if (!restore) {
			if (found) {
				item->setHidden(false);
				item->setExpanded(true);
			}
			else {
				item->setHidden(true);
			}
		}
	}
}

void VipProcessingObjectInfo::currentDisplayPlayerAreaChanged(VipDisplayPlayerArea* area)
{
	if (area) {
		//hide this tool widget if their are no valid top level widget anymore in the current workspace
		QList<VipMultiDragWidget*> ws = area->dragWidgetHandler()->topLevelMultiDragWidgets();
		if (ws.isEmpty())
			vipGetProcessingObjectInfo()->hide();
	}
}

void VipProcessingObjectInfo::topLevelWidgetClosed(VipDisplayPlayerArea * area, VipMultiDragWidget * w)
{
	if (area) {
		//hide this tool widget if their are no valid top level widget anymore in the current workspace
		QList<VipMultiDragWidget*> ws = area->dragWidgetHandler()->topLevelMultiDragWidgets();
		if (ws.isEmpty() || (ws.size()==1 && ws.first() == w))
			vipGetProcessingObjectInfo()->hide();
	}
}

QList<VipProcessingObject*> VipProcessingObjectInfo::plotAttributes( QList<VipOutput*> outputs, const QStringList & attributes, VipProcessingPool * pool)
{
	if (outputs.size() != attributes.size())
		return  QList<VipProcessingObject*>();
	if (!pool)
		return QList<VipProcessingObject*>();
	//a VipProcessingPool which is not of type Resource is mandatory
	if (pool->deviceType() == VipIODevice::Resource)
		return QList<VipProcessingObject*>();

	pool->stop(); //stop playing

	//find all processings within the player
	QList<VipDisplayObject*> displays = m_data->player->displayObjects();
	QList<VipProcessingObject*> sources, leafs;
	for (int i = 0; i < displays.size(); ++i) {
		if (VipOutput * src = displays[i]->inputAt(0)->connection()->source()) {
			leafs.append(src->parentProcessing());
			sources << src->parentProcessing() << src->parentProcessing()->allSources();
		}
	}
	//make unqiue
	sources = sources.toSet().toList();
	leafs = leafs.toSet().toList();

	//create one VipExtractAttribute for each output
	QVector<VipProcessingObject *> extract(outputs.size());
	for (int i = 0; i < outputs.size(); ++i)
	{
		VipProcessingObject * parent = outputs[i]->parentProcessing();
		if (VipAdditionalInfo * info = qobject_cast<VipAdditionalInfo*>(parent))
		{
			//additional attributes
			VipExtractAttributeFromInfo * extr = new VipExtractAttributeFromInfo();
			//disable error logging
			extr->setLogErrors(QSet<int>());
			extr->setVipAdditionalInfo(info);
			extr->setAttributeName(attributes[i]);
			extr->setAttribute("Name", attributes[i]);
			extr->setAttribute("XUnit", "Time");
			extract[i] = extr;
		}
		else
		{
			extract[i] = new VipExtractAttribute();
			//disable error logging
			extract[i]->setLogErrors(QSet<int>());
			extract[i]->propertyAt(0)->setData(attributes[i]);
			extract[i]->setAttribute("Name", attributes[i]);
			extract[i]->setAttribute("XUnit", "Time");
		}
		//replace output by the right one
		outputs[i] = m_data->output;
	}

	//look into the display object sources for VipIODevice, find if the source is Temporal, Sequential or a Resource
	QList<VipIODevice*> devices = vipListCast<VipIODevice*>(sources);
	VipIODevice::DeviceType type = VipIODevice::Resource;
	for (int i = 0; i < devices.size(); ++i)
		if (devices[i]->deviceType() == VipIODevice::Sequential)
		{
			//Sequential device has the priority
			type = VipIODevice::Sequential;
			break;
		}
		else if (devices[i]->deviceType() == VipIODevice::Temporal)
			type = VipIODevice::Temporal;

	if (type == VipIODevice::Resource) {
		for (int i = 0; i < extract.size(); ++i)
			delete extract[i];
		return QList<VipProcessingObject*>();
	}


	//for Sequential device only:

	if (type == VipIODevice::Sequential)
	{
		QList<VipProcessingObject*> result;
		for (int i = 0; i < outputs.size(); ++i)
		{

			extract[i]->setParent(pool);
			outputs[i]->setConnection(extract[i]->inputAt(0));
			extract[i]->inputAt(0)->setData(outputs[i]->data());
			extract[i]->update();
			extract[i]->setDeleteOnOutputConnectionsClosed(true);
			extract[i]->setScheduleStrategies(VipProcessingObject::Asynchronous);

			VipNumericValueToPointVector * ConvertToPointVector = new VipNumericValueToPointVector(pool);
			ConvertToPointVector->setScheduleStrategies(VipProcessingObject::Asynchronous);
			ConvertToPointVector->setDeleteOnOutputConnectionsClosed(true);
			ConvertToPointVector->inputAt(0)->setConnection(extract[i]->outputAt(i));

			VipProcessingList * ProcessingList = new VipProcessingList(pool);
			ProcessingList->setScheduleStrategies(VipProcessingObject::Asynchronous);
			ProcessingList->setDeleteOnOutputConnectionsClosed(true);
			ProcessingList->inputAt(0)->setConnection(ConvertToPointVector->outputAt(0));
			ProcessingList->outputAt(0)->setData(extract[i]->outputAt(0)->data());

			result.append(ProcessingList);
		}
		return result;
	}


	//for Temporal device only:

	VipProgress progress;
	progress.setModal(true);
	progress.setCancelable(true);
	progress.setText("Extract attribute time traces...");

	//now, save the current VipProcessingPool state, because we are going to modify it heavily
	pool->save();

	//disable all processing except the sources, remove the Automatic flag from the sources
	pool->disableExcept(sources);
	foreach(VipProcessingObject * obj, sources) { obj->setScheduleStrategy(VipProcessingObject::Asynchronous, false); }

	//create the VipExtractStatistics object and connect it to the display source object
	for (int i = 0; i < outputs.size(); ++i)
	{
		outputs[i]->setConnection(extract[i]->inputAt(0));
		extract[i]->inputAt(0)->setData(outputs[i]->data());
		extract[i]->update();
	}

	//extract the values

	QVector<VipPointVector > values(extract.size());
	qint64 pool_time = pool->time();
	qint64 time = pool->firstTime();
	qint64 end_time = pool->lastTime();
	//qint64 current_time = pool->time();
	int skip = 1;// options->skip.value();
	progress.setRange(time, end_time);

	pool->blockSignals(true);

	while (time != VipInvalidTime && time <= end_time)
	{
		progress.setValue(time);

		pool->read(time, true);

		//update leafs
		for (int i = 0; i < leafs.size(); ++i)
			leafs[i]->update();

		//update attribute extraction
		for (int i = 0; i < extract.size(); ++i)
		{
			extract[i]->update();
			if (!extract[i]->hasError()) {
				bool to_double = false;
				double val = ToDouble(extract[i]->outputAt(0)->data().data(), &to_double);
				if (to_double)
					values[i].append(QPointF(time, val));
			}
		}


		//skip frames
		bool end_loop = false;
		for (int i = 0; i < skip; ++i)
		{
			qint64 next = pool->nextTime(time);
			if (next == time || progress.canceled() || next == VipInvalidTime)
			{
				end_loop = true;
				break;
			}
			time = next;
		}
		if (end_loop)
			break;
	}


	pool->blockSignals(false);

	//delete the attribute extraction objects
	for (int i = 0; i < extract.size(); ++i)
		delete extract[i];

	//store the result
	QList<VipProcessingObject*> res;

	for (int i = 0; i < values.size(); ++i)
	{
		VipAnyResource * any = new VipAnyResource( pool);
		any->setAttribute("XUnit", QString("Time"));
		any->setPath(attributes[i]);
		any->setData(QVariant::fromValue(values[i]));
		res << any;
	}

	//restore the VipProcessingPool
	pool->restore();
	pool->read(pool_time);


	return res;
}



VipProcessingObjectInfo * vipGetProcessingObjectInfo(VipMainWindow * window)
{
	static VipProcessingObjectInfo * infos = new VipProcessingObjectInfo(window);
	return infos;
}
