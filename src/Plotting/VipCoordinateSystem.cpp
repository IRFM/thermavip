#include <QMap>
#include <QList>

#include <math.h>
#include "VipCoordinateSystem.h"
#include "VipAxisBase.h"
#include "VipPolarAxis.h"
#include "VipPlotItem.h"

#include <QGraphicsScene>



QTransform VipCoordinateSystem::changeCoordinateSystem(const QPointF & origin, const QVector2D & x, const QVector2D & y)
{
	QTransform tr;

	static const double pi_2 = M_PI*2.0;
	static const double pi = M_PI;

	double x_scale = x.length();
	double y_scale = y.length();


	double x_angle = std::atan2(x.y(),x.x());
	if(x_angle < 0) x_angle += pi_2;
	if(x_angle >= pi) x_angle -= pi;

	double y_angle = std::atan2(y.y()/y.x(),1.);
	if(y_angle < 0) y_angle += pi_2;
	if(y_angle >= pi) y_angle -= pi;

	//angle between x and y
	double angle_diff = std::abs(x_angle - y_angle);
	//double angle_diff = std::abs(QVector2D::dotProduct(x,y) / (x_scale*y_scale));
	// if(angle_diff < 0) angle_diff += pi_2;
	// if(angle_diff >= pi) angle_diff -= pi;

	if(x.x() < 0) x_scale*=-1;
	if(y.y() < 0) y_scale*=-1;



	//use a x shear to represent the angle between x and y axis.
	//this shear will create an error on y coordinates which is
	//corrected by a y scaling.
	double dy =  std::sin(angle_diff);
	double shear_x = std::cos(angle_diff)/dy;

	//set the new origin postion with a translation
	tr.translate(origin.x(),origin.y());
	//set the new x axis orientation with a rotation
	tr.rotate(-x_angle*180.0/pi);



	//the x shear itself
	tr.shear(shear_x,0) ;
	//y correction
	tr.scale(1,dy);
	//coordinate scaling for non normalized axises
	tr.scale(x_scale,y_scale);

	return tr;
}



QTransform VipCoordinateSystem::changeCoordinateSystem(const QPointF & origin_x, const QVector2D & x, const QPointF & origin_y, const QVector2D & y)
{
	QLineF lx(QPointF(0,0),x.toPointF());
	QLineF ly(QPointF(0,0),y.toPointF());
	lx.translate(origin_y);
	ly.translate(origin_x);
	QPointF new_origin;
	lx.QLINE_INTERSECTS(ly, &new_origin);

	return changeCoordinateSystem(new_origin,x,y);
}

QPolygonF VipCoordinateSystem::transform(const QRectF & r) const
{
	QPolygonF polygon(4);
	polygon[0] = transform(r.topLeft());
	polygon[1] = transform(r.topRight());
	polygon[2] = transform(r.bottomRight());
	polygon[3] = transform(r.bottomLeft());
	return polygon;
}

QVector<QPointF> VipCoordinateSystem::transform(const VipPointVector & polygon) const
{
	QVector<QPointF> p(polygon.size());
	for(int i=0; i < polygon.size(); ++i)
		p[i] = transform(polygon[i]);

	return p;
}

QVector<QPointF> VipCoordinateSystem::transform(const QVector<QPointF> & polygon) const
{
	QVector<QPointF> p(polygon.size());
	for (int i = 0; i < polygon.size(); ++i)
		p[i] = transform(polygon[i]);

	return p;
}

QVector<QPointF> VipCoordinateSystem::transform(const QVector<QPoint> & polygon) const
{
	QVector<QPointF> p(polygon.size());
	for(int i=0; i < polygon.size(); ++i)
		p[i] = transform(VipPoint(polygon[i]));

	return p;
}

QPainterPath VipCoordinateSystem::transform(const QPainterPath & path, const QRectF & bounding_rect) const
{
	if (path.isEmpty())
		return QPainterPath();

	//return path;
	bool custom_transform = false;

	if (axes().size() < 2)
		return QPainterPath();

	if(axes()[0] && axes()[0]->transformation() && axes()[0]->transformation()->type() != VipValueTransform::Null)
		custom_transform = true;

	if(axes()[1] && axes()[1]->transformation() && axes()[1]->transformation()->type() != VipValueTransform::Null)
		custom_transform = true;

	if(!custom_transform)
	{
		QRectF p_rect = bounding_rect;
		if(bounding_rect.isEmpty())
			p_rect = path.boundingRect();
		VipPointVector target = transform(p_rect);

		QVector2D vx(target[1].x()-target[0].x(),target[1].y()-target[0].y());
		QVector2D vy(target[3].x()-target[0].x(),target[3].y()-target[0].y());
		QPointF origin(target[0]);
		vx /= p_rect.width();
		vy /= p_rect.height();

		QTransform tr = changeCoordinateSystem(origin,vx,vy);

		tr = QTransform().translate(-p_rect.left(),-p_rect.top()) * tr;
		return tr.map(path);

		//QPainterPath p(path);
		// p.translate(-p_rect.topLeft());
		// return tr.map(p);
	}
	else
	{
		QList<QPolygonF> polygons = path.toFillPolygons();
		QPainterPath p;

		for(int i=0; i < polygons.size(); ++i)
		{
			p.addPolygon( transform(polygons[i]) );
		}

		return p;
	}
}

QRectF VipCoordinateSystem::transformRect(const QRectF & r) const
{
	return (transform(r)).boundingRect();
}

VipPointVector VipCoordinateSystem::invTransform(const QRectF & r) const
{
	VipPointVector polygon(4);
	polygon[0] = invTransform(r.topLeft());
	polygon[1] = invTransform(r.topRight());
	polygon[2] = invTransform(r.bottomRight());
	polygon[3] = invTransform(r.bottomLeft());
	return polygon;
}

VipPointVector VipCoordinateSystem::invTransform(const VipPointVector & polygon) const
{
	VipPointVector p(polygon.size());
	for(int i=0; i < polygon.size(); ++i)
		p[i] = invTransform(polygon[i]);

	return p;
}

VipPointVector VipCoordinateSystem::invTransform(const QVector<QPointF> & polygon) const
{
	VipPointVector p(polygon.size());
	for(int i=0; i < polygon.size(); ++i)
		p[i] = invTransform(polygon[i]);

	return p;
}

QRectF VipCoordinateSystem::invTransformRect(const QRectF & r) const
{
	return invTransform(r).boundingRect();
}





QPainterPath VipNullCoordinateSystem::clipPath(const VipPlotItem * item) const
{
	if(item->parentItem())
		return item->parentItem()->shape();
	else if(item->scene())
	{
		QPainterPath path;
		path.addRect(item->scene()->sceneRect());
		return path;
	}
	else
	{
		return QPainterPath();
	}
}




VipCartesianSystem::VipCartesianSystem(const QList<VipAbstractScale*> & axes)
:VipCoordinateSystem(axes)
{
	QVector2D  vx(1,0);
	QVector2D  vy(0,1);
	QPointF start_x(0,0);
	QPointF start_y(0,0);
	QPointF end_x,end_y;

	const VipAxisBase * x = static_cast<const VipAxisBase*>(axes[0]);
	const VipAxisBase * y = static_cast<const VipAxisBase*>(axes[1]);

	if(x)
	{
		mx = x->constScaleDraw()->scaleMap();
		QTransform tr_x = VipAbstractScale::parentTransform(x);//x->parentTransform();
		if(x->orientation() == Qt::Horizontal)
		{
			start_x = tr_x.map(x->constScaleDraw()->pos());
			end_x = tr_x.map(x->constScaleDraw()->end());
		}
		else
		{
			start_x = tr_x.map(x->constScaleDraw()->end());
			end_x = tr_x.map(x->constScaleDraw()->pos());
		}
		vx = QVector2D( (end_x.x()-start_x.x()) , (end_x.y()-start_x.y()));
		vx /= x->constScaleDraw()->length();//vx.length();
	}

	if(y)
	{
		my = y->constScaleDraw()->scaleMap();
		QTransform tr_y = VipAbstractScale::parentTransform(y);//y->parentTransform();
		if(y->orientation() == Qt::Horizontal)
		{
			start_y = tr_y.map(y->constScaleDraw()->pos());
			end_y = tr_y.map(y->constScaleDraw()->end());
		}
		else
		{
			start_y = tr_y.map(y->constScaleDraw()->end());
			end_y = tr_y.map(y->constScaleDraw()->pos());
		}
		vy = QVector2D( (end_y.x()-start_y.x()), (end_y.y()-start_y.y()) );
		vy /= y->constScaleDraw()->length();//vy.length();
	}

	if(x || y)
	{
		axis_tr = changeCoordinateSystem(start_x,vx,start_y,vy);
		global_tr = axis_tr;
		inv_global_tr = global_tr.inverted();
	}

}


void VipCartesianSystem::setAdditionalTransform(const QTransform & tr)
{
	VipCoordinateSystem::setAdditionalTransform(tr);
	global_tr = axis_tr *tr ;
	inv_global_tr = global_tr.inverted();
}

VipPoint VipCartesianSystem::invTransform(const QPointF & coordinate) const
{
	const VipPoint pt = inv_global_tr.map(coordinate);
	return VipPoint( mx.invDistanceToOrigin(pt.x()) , my.invDistanceToOrigin(pt.y()) );
}

QPointF VipCartesianSystem::transform(const VipPoint & value) const
{
	const VipPoint pt(mx.distanceToOrigin(value.x()) ,my.distanceToOrigin(value.y()) );
	return global_tr.map(pt.toPointF());
}

QPainterPath VipCartesianSystem::clipPath(const VipPlotItem * item) const
{
	if(!item->axes().size())
		return QPainterPath();

	QRectF bounding;
	if(item->parentItem())
		bounding = item->parentItem()->boundingRect();
	else if(item->scene())
		bounding = item->scene()->sceneRect();

	VipInterval x,y;

	if(item->axes()[0])
		x = item->axes()[0]->scaleDiv().bounds();
	else
		x = VipInterval(bounding.left(),bounding.right());

	if(item->axes()[1])
		y = item->axes()[1]->scaleDiv().bounds();
	else
		y = VipInterval(bounding.top(),bounding.bottom());

	QPolygonF polygon;
	polygon << item->sceneMap()->transform(x.minValue(),y.minValue())
		<< item->sceneMap()->transform(x.minValue(),y.maxValue())
		<< item->sceneMap()->transform(x.maxValue(),y.maxValue())
		<< item->sceneMap()->transform(x.maxValue(),y.minValue());

	QPainterPath path;
	path.addPolygon(polygon);
	return path;
}

VipCartesianSystem * VipCartesianSystem::copy() const
{
	VipCartesianSystem * map = new VipCartesianSystem(axes());
	map->mx = mx;
	map->my = my;
	map->axis_tr = axis_tr;
	map->global_tr = global_tr;
	map->inv_global_tr = inv_global_tr;
	map->setAdditionalTransform(this->additionalTransform());
	return map;
}

QPolygonF VipCartesianSystem::transform(const QRectF & r) const
{
	QPolygonF polygon(4);
	polygon[0] = global_tr.map(QPointF(mx.distanceToOrigin(r.topLeft().x()), my.distanceToOrigin(r.topLeft().y())));
	polygon[1] = global_tr.map(QPointF(mx.distanceToOrigin(r.topRight().x()), my.distanceToOrigin(r.topRight().y())));
	polygon[2] = global_tr.map(QPointF(mx.distanceToOrigin(r.bottomRight().x()), my.distanceToOrigin(r.bottomRight().y())));
	polygon[3] = global_tr.map(QPointF(mx.distanceToOrigin(r.bottomLeft().x()), my.distanceToOrigin(r.bottomLeft().y())));
	return polygon;
}
QVector<QPointF> VipCartesianSystem::transform(const VipPointVector & polygon) const
{
	QVector<QPointF> res(polygon.size());
	for (int i = 0; i < res.size(); ++i)
		res[i] = global_tr.map(QPointF(mx.distanceToOrigin(polygon[i].x()), my.distanceToOrigin(polygon[i].y())));
	return res;
}
QVector<QPointF> VipCartesianSystem::transform(const QVector<QPointF> & polygon) const
{
	QVector<QPointF> res(polygon.size());
	for (int i = 0; i < res.size(); ++i)
		res[i] = global_tr.map(QPointF(mx.distanceToOrigin(polygon[i].x()), my.distanceToOrigin(polygon[i].y())));
	return res;
}
QVector<QPointF> VipCartesianSystem::transform(const QVector<QPoint> & polygon) const
{
	QVector<QPointF> res(polygon.size());
	for (int i = 0; i < res.size(); ++i)
		res[i] = global_tr.map(QPointF(mx.distanceToOrigin(polygon[i].x()), my.distanceToOrigin(polygon[i].y())));
	return res;
}
VipPointVector VipCartesianSystem::invTransform(const QRectF & r) const
{
	const VipPoint pt1 = inv_global_tr.map(r.topLeft());
	const VipPoint pt2 = inv_global_tr.map(r.topRight());
	const VipPoint pt3 = inv_global_tr.map(r.bottomRight());
	const VipPoint pt4 = inv_global_tr.map(r.bottomLeft());

	VipPointVector polygon(4);
	polygon[0] = VipPoint(mx.invDistanceToOrigin(pt1.x()), my.invDistanceToOrigin(pt1.y()));
	polygon[1] = VipPoint(mx.invDistanceToOrigin(pt2.x()), my.invDistanceToOrigin(pt2.y()));
	polygon[2] = VipPoint(mx.invDistanceToOrigin(pt3.x()), my.invDistanceToOrigin(pt3.y()));
	polygon[3] = VipPoint(mx.invDistanceToOrigin(pt4.x()), my.invDistanceToOrigin(pt4.y()));
	return polygon;
}
VipPointVector VipCartesianSystem::invTransform(const VipPointVector & polygon) const
{
	VipPointVector res(polygon.size());
	for (int i = 0; i < res.size(); ++i) {
		const VipPoint pt = inv_global_tr.map(polygon[i].toPointF());
		res[i] = VipPoint(mx.invDistanceToOrigin(pt.x()), my.invDistanceToOrigin(pt.y()));
	}
	return res;
}
VipPointVector VipCartesianSystem::invTransform(const  QVector<QPointF> & polygon) const
{
	VipPointVector res(polygon.size());
	for (int i = 0; i < res.size(); ++i) {
		const VipPoint pt = inv_global_tr.map(polygon[i]);
		res[i] = VipPoint(mx.invDistanceToOrigin(pt.x()), my.invDistanceToOrigin(pt.y()));
	}
	return res;
}





VipPolarSystem::VipPolarSystem(const QList<VipAbstractScale*> & axes)
:VipCoordinateSystem(axes),
 d_center(0,0),
 d_startRadius(0),
 d_endRadius(1),
 d_start_angle(0),
 d_end_angle(360),
 d_sweep_length(360),
 d_arc_length(360)
{}



VipPolarCoordinate VipPolarSystem::polarTransform(const VipPolarCoordinate & p) const
{
	const vip_double radius = d_mradius.transform(p.radius()) ;
	const vip_double angle = d_start_angle + (d_sweep_length * d_mangle.transform( p.angle() ) / d_arc_length);
	return VipPolarCoordinate(radius,angle);
}

VipPolarCoordinate VipPolarSystem::polarInvTransform(const VipPolarCoordinate & p) const
{
	const vip_double radius = d_mradius.invTransform(p.radius());
	const vip_double angle = d_mangle.invTransform((p.angle() - d_start_angle) * d_arc_length / d_sweep_length);
	return VipPolarCoordinate(radius,angle);
}

VipPie VipPolarSystem::polarTransform(const VipPie & p) const
{
	VipPolarCoordinate top_left = polarTransform(p.topLeft());
	VipPolarCoordinate bottom_right = polarTransform(p.bottomRight());
	VipPie res = VipPie(top_left,bottom_right).normalized();
	if(p.offsetToCenter())
	{
		//QPointF start(d_mradius.s1(), d_mangle.s1());
		//QPointF end(d_mradius.s1() + p.offsetToCenter(), d_mangle.s1());
		res.setOffsetToCenter(d_mradius.transform(d_mradius.s1() + p.offsetToCenter()) - d_mradius.p1());
	}
	return res;
}

VipPie VipPolarSystem::polarInvTransform(const VipPie & p) const
{
	VipPolarCoordinate top_left = polarInvTransform(p.topLeft());
	VipPolarCoordinate bottom_right = polarInvTransform(p.bottomRight());
	VipPie res = VipPie(top_left,bottom_right).normalized();
	if(p.offsetToCenter())
	{
		res.setOffsetToCenter(d_mradius.invTransform(p.offsetToCenter()) - d_mradius.s1());
	}
	return res;
}

QPointF VipPolarSystem::polarTransformToPoint(const VipPolarCoordinate & p) const
{
	VipPolarCoordinate polar = polarTransform(p);
	return polar.position(d_center);
}




VipRadialPolarSystem::VipRadialPolarSystem(const QList<VipAbstractScale*> & axes)
:VipPolarSystem(axes)
{
	if(axes[0])
	{
		d_mradius = axes[0]->constScaleDraw()->scaleMap();
		d_center = static_cast<const VipRadialAxis*>(axes[0])->center();
		d_startRadius = static_cast<const VipRadialAxis*>(axes[0])->constScaleDraw()->startRadius();
		d_endRadius = static_cast<const VipRadialAxis*>(axes[0])->constScaleDraw()->endRadius();
	}

	if(axes[1])
	{
		const VipPolarScaleDraw * psd = static_cast<const VipPolarScaleDraw *>(axes[1]->constScaleDraw());
		d_mangle = psd->scaleMap();
		d_center = psd->center();
		d_start_angle = psd->startAngle();
		d_end_angle = psd->endAngle();
		d_sweep_length = d_end_angle - d_start_angle;
		d_arc_length = psd->arcLength();
	}
}

QPainterPath VipRadialPolarSystem::clipPath(const VipPlotItem * item) const
{
	if(!item->axes().size())
		return QPainterPath();

	VipRadialPolarSystem system(item->axes());

	VipBoxStyle box;
	box.computePie((QPointF)system.d_center,VipPie(system.d_start_angle,system.d_end_angle,system.d_startRadius,system.d_endRadius));
	return box.background();
}

VipPolarCoordinate VipRadialPolarSystem::toPolar(const VipPoint & polar) const
{
	return VipPolarCoordinate(polar.x(), polar.y());
}

QPointF VipRadialPolarSystem::transform(const VipPoint & polar) const
{
	return polarTransformToPoint(VipPolarCoordinate(polar.x(), polar.y()));
}

VipPoint VipRadialPolarSystem::invTransform(const QPointF & pt) const
{
	QLineF line((QPointF)d_center, pt);
	const VipPolarCoordinate p(line.length(),line.angle());
	const VipPolarCoordinate res(polarInvTransform(p));
	return VipPoint(res.radius(),res.angle());
}

VipRadialPolarSystem * VipRadialPolarSystem::copy() const
{
	VipRadialPolarSystem * map = new VipRadialPolarSystem(axes());
	map->setAdditionalTransform(this->additionalTransform());
	return map;
}





VipPolarRadialSystem::VipPolarRadialSystem(const QList<VipAbstractScale*> & axes)
:VipPolarSystem(axes)
{
	if(axes[1])
	{
		d_mradius = axes[1]->constScaleDraw()->scaleMap();
		d_center = static_cast<const VipRadialAxis*>(axes[1])->center();
		d_startRadius = static_cast<const VipRadialAxis*>(axes[1])->constScaleDraw()->startRadius();
		d_endRadius = static_cast<const VipRadialAxis*>(axes[1])->constScaleDraw()->endRadius();
	}

	if(axes[0])
	{
		const VipPolarScaleDraw * psd = static_cast<const VipPolarScaleDraw *>(axes[0]->constScaleDraw());
		d_mangle = psd->scaleMap();
		d_center = psd->center();
		d_start_angle = psd->startAngle();
		d_end_angle = psd->endAngle();
		d_sweep_length = d_end_angle - d_start_angle;
		d_arc_length = psd->arcLength();
	}
}

QPainterPath VipPolarRadialSystem::clipPath(const VipPlotItem * item) const
{
	if(!item->axes().size())
		return QPainterPath();

	VipPolarRadialSystem system(item->axes());

	VipBoxStyle box;
	box.computePie((QPointF)system.d_center,VipPie(system.d_start_angle,system.d_end_angle,system.d_startRadius,system.d_endRadius));
	return box.background();
}

VipPolarCoordinate VipPolarRadialSystem::toPolar(const VipPoint & polar) const
{
	return VipPolarCoordinate(polar.y(), polar.x());
}

QPointF VipPolarRadialSystem::transform(const VipPoint & polar) const
{
	return polarTransformToPoint(VipPolarCoordinate(polar.y(), polar.x()));
}

VipPoint VipPolarRadialSystem::invTransform(const QPointF & pt) const
{
	QLineF line((QPointF)d_center, pt);
	const VipPolarCoordinate p(line.length(),line.angle());
	const VipPolarCoordinate res(polarInvTransform(p));
	return VipPoint(res.angle(),res.radius());
}

VipPolarRadialSystem * VipPolarRadialSystem::copy() const
{
	VipPolarRadialSystem * map = new VipPolarRadialSystem(axes());
	map->setAdditionalTransform(this->additionalTransform());
	return map;
}





VipMonoAxisSystem::VipMonoAxisSystem(const QList<VipAbstractScale*> & axes)
:VipCoordinateSystem(axes)
{}

QPointF VipMonoAxisSystem::transform(const VipPoint & value) const
{
	return axes().first()->constScaleDraw()->position(value.x(),value.y());
}

VipPoint VipMonoAxisSystem::invTransform(const QPointF & position) const
{
	return VipPoint(axes().first()->constScaleDraw()->value(position),0);
}

QPainterPath VipMonoAxisSystem::clipPath(const VipPlotItem *) const
{
	return QPainterPath();
}

VipMonoAxisSystem * VipMonoAxisSystem::copy() const
{
	return new VipMonoAxisSystem(axes());
}



typedef QList<QSharedPointer<VipHandleCoordinateSystem> >  CoordinateSystemList;
typedef QMap<int,CoordinateSystemList> CoordinateSystemMap;

static CoordinateSystemMap & coordinateSystems()
{
	static CoordinateSystemMap instance;
	return instance;
}

void vipRegisterCoordinateSystem(VipHandleCoordinateSystem * system)
{
	coordinateSystems()[system->type()].push_front(QSharedPointer<VipHandleCoordinateSystem>(system));
}



VipCoordinateSystem * vipBuildCoordinateSystem(const QList<VipAbstractScale*> & axes, int type)
{
	//first, search for a valid VipHandleCoordinateSystem

	if(coordinateSystems().size())
	{
		CoordinateSystemMap::const_iterator it = coordinateSystems().find(type);
		if(it != coordinateSystems().end())
		{
			const CoordinateSystemList  & lst = it.value();
			for(CoordinateSystemList::const_iterator lit = lst.begin(); lit != lst.end(); ++lit)
			{
				if(VipCoordinateSystem * sys = (*lit)->build(axes,type))
				{
					return sys;
				}
			}
		}
	}

	//apply the standard approach

	if(type == VipCoordinateSystem::Null)
	{
		return new VipNullCoordinateSystem(axes);
	}
	if(type == VipCoordinateSystem::Cartesian)
	{
		return new VipCartesianSystem(axes);
	}
	else if(type == VipCoordinateSystem::Polar)
	{
		if(qobject_cast<const VipRadialAxis*>(axes[0]))
			return new VipRadialPolarSystem(axes);
		else
			return new VipPolarRadialSystem(axes);
	}
	else if(type == VipCoordinateSystem::MonoAxis)
	{
		return new VipMonoAxisSystem(axes);
	}

	return NULL;
}
