/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
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

#include "VipFieldOfView.h"

#include "VipVTKObject.h"
#include "VipMath.h"

#include <qtextstream.h>
#include <QTextStream>
#include <vtkLine.h>
#include <vtkSphereSource.h>
#include <vtkPyramid.h>
#include <vtkUnstructuredGrid.h>
#include <vtkMatrix3x3.h>
#include <vtkMatrix4x4.h>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRendererCollection.h>
#include <vtkRenderer.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
Compute distance between 2 points
*/
static inline double dist(const double p1[3], const double p2[3])
{
	const double x = p1[0] - p2[0];
	const double y = p1[1] - p2[1];
	const double z = p1[2] - p2[2];
	return sqrt(x * x + y * y + z * z);
}


#define crossProduct(a, b, c)                                                                                                                                                                          \
	(a)[0] = (b)[1] * (c)[2] - (c)[1] * (b)[2];                                                                                                                                                    \
	(a)[1] = (b)[2] * (c)[0] - (c)[2] * (b)[0];                                                                                                                                                    \
	(a)[2] = (b)[0] * (c)[1] - (c)[0] * (b)[1];

#define innerProduct(v, q) ((v)[0] * (q)[0] + (v)[1] * (q)[1] + (v)[2] * (q)[2])

/* a = b - c */
#define vector(a, b, c)                                                                                                                                                                                \
	(a)[0] = (b)[0] - (c)[0];                                                                                                                                                                      \
	(a)[1] = (b)[1] - (c)[1];                                                                                                                                                                      \
	(a)[2] = (b)[2] - (c)[2];

#define SMALL_NUM 0.00000001 // anything that avoids division overflow
// dot product (3D) which allows vector operations in arguments
#define dot(u, v) ((u[0]) * (v[0]) + (u[1]) * (v[1]) + (u[2]) * (v[2]))
#define fuzzyCompare(d1, d2) (((d1) == 0 || (d2) == 0) ? qFuzzyCompare(1.0 + (d1), 1.0 + (d2)) : qFuzzyCompare((d1), (d2)))

/**
Compute the intersection between a triangle and a line
*/
static double intersectsTriangle(const double* P0, const double* P1, const double* V0, const double* V1, const double* V2, double* intersect = nullptr)
{
	double u[3], v[3], n[3];    // triangle vectors
	double dir[3], w0[3], w[3]; // ray vectors
	double r, a, b;		    // params to calc ray-plane intersect

	// get triangle edge vectors and plane normal
	vector(u, V1, V0);
	vector(v, V2, V0);
	crossProduct(n, u, v); // cross product

	vector(dir, P1, P0); // ray direction vector
	vector(w0, P0, V0);
	a = -dot(n, w0);
	b = dot(n, dir);
	if (fabs(b) < SMALL_NUM) { // ray is parallel to triangle plane
		if (a == 0)	   // ray lies in triangle plane
			return -1;
		else
			return -1; // ray disjoint from plane
	}

	// get intersect point of ray with triangle plane
	r = a / b;
	if (r < 0.0)	   // ray goes away from triangle
		return -1; // => no intersect
			   // for a segment, also test if (r > 1.0) => no intersect

	double I[3] = { P0[0] + r * dir[0], P0[1] + r * dir[1], P0[2] + r * dir[2] }; // intersect point of ray and plane

	// is I inside T?
	double uu, uv, vv, wu, wv, D;
	uu = dot(u, u);
	uv = dot(u, v);
	vv = dot(v, v);
	vector(w, I, V0);
	wu = dot(w, u);
	wv = dot(w, v);
	D = uv * uv - uu * vv;

	// get and test parametric coords
	double s, t;
	s = (uv * wv - vv * wu) / D;
	if (s < 0.0 - SMALL_NUM || s > 1.0 + SMALL_NUM) // I is outside T
		return -1;
	t = (uv * wu - uu * wv) / D;
	if (t < 0.0 - SMALL_NUM || (s + t) > 1.0 + SMALL_NUM) // I is outside T
		return -1;

	double vec[3] = { I[0] - P0[0], I[1] - P0[1], I[2] - P0[2] };
	double res = sqrt((vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2]) / (dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]));
	if (intersect)
		memcpy(intersect, I, sizeof(double) * 3);

	return res; // I is in T
}


bool VipFieldOfView::operator==(const VipFieldOfView& fov) const
{
	if (fov.name == this->name && memcmp(fov.pupil, this->pupil, sizeof(fov.pupil)) == 0 && memcmp(fov.target, this->target, sizeof(fov.target)) == 0 && fov.view_up == this->view_up &&
	    fov.rotation == this->rotation && fov.focal == this->focal && fov.horizontal_angle == this->horizontal_angle && fov.vertical_angle == this->vertical_angle && fov.width == this->width &&
	    fov.height == fov.height && fov.P1 == this->P1 && fov.P2 == this->P2 && fov.K2 == this->K2 && fov.K4 == this->K4 && fov.K6 == this->K6 && fov.time == this->time)
		return true;
	else
		return false;
}

bool VipFieldOfView::operator!=(const VipFieldOfView& fov) const
{
	return !((*this) == fov);
}

bool VipFieldOfView::isNull() const
{
	return name.isEmpty();
}

QString VipFieldOfView::print() const
{

	QString res;
	QTextStream stream(&res);

	int count = 0;

	stream << "<b>Name</b>: " << this->name.toLatin1().data() << "<br>";
	stream << "<b>Pupil position</b>: " << pupil[0] << " " << pupil[1] << " " << pupil[2] << "<br>";
	stream << "<b>Target point</b>: " << target[0] << " " << target[1] << " " << target[2] << "<br>";
	stream << "<b>Vertical angle</b>: " << this->vertical_angle << "<br>";
	stream << "<b>Horizontal angle</b>: " << this->horizontal_angle << "<br>";
	stream << "<b>Rotation</b>: " << this->rotation << "<br>";
	stream << "<b>Width (pixels)</b>: " << this->width << "<br>";
	stream << "<b>Height (pixels)</b>: " << this->height << "<br>";
	stream << "<b>Horizontal crop (pixels)</b>: " << this->crop_x << "<br>";
	stream << "<b>Vertical crop (pixels)</b>: " << this->crop_y << "<br>";
	count += 10;

	if (this->K2 || this->K4 || this->K6 || this->P1 || this->P2) {
		stream << "<p>";
		stream << "<br><b>Optical distortions:</b><br>";
		stream << "<b>K2</b> : " << this->K2 << "<br>";
		stream << "<b>K4</b> : " << this->K4 << "<br>";
		stream << "<b>K6</b> : " << this->K6 << "<br>";
		stream << "<b>P1</b> : " << this->P1 << "<br>";
		stream << "<b>P2</b> : " << this->P2 << "<br>";
		stream << "<b>AlphaC</b> : " << this->AlphaC;
		stream << "</p>";
		count += 7;
	}
	if (attributes.size()) {
		stream << "<p>";
		stream << "<br><b>Attributes:</b><br>";
		for (QVariantMap::const_iterator it = attributes.begin(); it != attributes.end(); ++it, ++count) {
			if (count >= 30) {
				stream << "...";
				break;
			}
			stream << "<b>" << it.key() << "</b> : " << it.value().toString() << "<br>";
		}
		stream << "</p>";
	}
	return res;
}

void VipFieldOfView::focalPoint(double* pt) const
{
	double vec_direction[3] = { target[0] - pupil[0], target[1] - pupil[1], target[1] - pupil[1] };
	double vec_length = sqrt(vec_direction[0] * vec_direction[0] + vec_direction[1] * vec_direction[1] + vec_direction[2] * vec_direction[2]);
	double factor = this->focal / vec_length;
	vec_direction[0] *= factor;
	vec_direction[1] *= factor;
	vec_direction[2] *= factor;

	pt[0] = vec_direction[0] + pupil[0];
	pt[1] = vec_direction[1] + pupil[1];
	pt[2] = vec_direction[2] + pupil[2];
}

void VipFieldOfView::changePointOfView(vtkCamera* cam_cour, double target_dist) const
{
	cam_cour->SetPosition(this->pupil[0], this->pupil[1], this->pupil[2]);

	double target[3];
	memcpy(target, this->target, sizeof(target));

	// if target_dist > 0, set the target point at given distance
	if (target_dist > 0) {
		double direction[3] = { this->target[0] - this->pupil[0], this->target[1] - this->pupil[1], this->target[2] - this->pupil[2] };
		double factor = target_dist / dist(this->target, this->pupil);
		target[0] = this->pupil[0] + direction[0] * factor;
		target[1] = this->pupil[1] + direction[1] * factor;
		target[2] = this->pupil[2] + direction[2] * factor;
	}

	cam_cour->SetFocalPoint(target);
	if (view_up == 0)
		cam_cour->SetViewUp(1, 0, 0);
	else if (view_up == 1)
		cam_cour->SetViewUp(0, 1, 0);
	else if (view_up == 2)
		cam_cour->SetViewUp(0, 0, 1);

	cam_cour->SetViewAngle(qMax(this->vertical_angle, this->horizontal_angle));
	cam_cour->Roll(this->rotation);
	cam_cour->Modified();
}

void VipFieldOfView::changePointOfView(vtkRenderWindow* win, double target_dist) const
{
	vtkRendererCollection* col = win->GetRenderers();

	col->InitTraversal();
	while (vtkRenderer* tmp = col->GetNextItem()) {
		changePointOfView(tmp->GetActiveCamera(), target_dist);
		tmp->ResetCameraClippingRange();
		tmp->Modified();
	}
}

void VipFieldOfView::importCamera(vtkCamera* camera)
{
	double view_up[3];
	camera->GetPosition(this->pupil);
	camera->GetFocalPoint(this->target);
	camera->GetViewUp(view_up);
	// double fov_angle = camera->GetViewAngle();

	double view_up_cam_wanted[3];
	double view_up_cam_current[3];
	double view_up_machine[3] = { 0, 0, 1 };
	vtkMatrix4x4* view_tr = camera->GetViewTransformMatrix();
	vtkMatrix3x3* mat = vtkMatrix3x3::New();
	mat->SetElement(0, 0, view_tr->GetElement(0, 0));
	mat->SetElement(1, 0, view_tr->GetElement(1, 0));
	mat->SetElement(2, 0, view_tr->GetElement(2, 0));
	mat->SetElement(0, 1, view_tr->GetElement(0, 1));
	mat->SetElement(1, 1, view_tr->GetElement(1, 1));
	mat->SetElement(2, 1, view_tr->GetElement(2, 1));
	mat->SetElement(0, 2, view_tr->GetElement(0, 2));
	mat->SetElement(1, 2, view_tr->GetElement(1, 2));
	mat->SetElement(2, 2, view_tr->GetElement(2, 2));
	mat->MultiplyPoint(view_up_machine, view_up_cam_wanted);
	mat->MultiplyPoint(view_up, view_up_cam_current);
	mat->Delete();

	// compute rotation to a view up of 0,0,1
	double dot = view_up_cam_wanted[0] * view_up_cam_current[0] + view_up_cam_wanted[1] * view_up_cam_current[1]; // dot product
	double det = view_up_cam_wanted[0] * view_up_cam_current[1] - view_up_cam_wanted[1] * view_up_cam_current[0]; // determinant
	this->rotation = -std::atan2(det, dot) * 57.295779513;
	this->view_up = 2;

	// set view angle as they are NOT set with importCamera
	vertical_angle = camera->GetViewAngle();
	horizontal_angle = camera->GetViewAngle();
}

void VipFieldOfView::setViewUpZ()
{
	if (this->view_up == 2)
		return;

	// create and setup a vtkCamera
	vtkCamera* camera = vtkCamera::New();
	camera->SetPosition(this->pupil[0], this->pupil[1], this->pupil[2]);
	camera->SetFocalPoint(this->target[0], this->target[1], this->target[2]);
	if (view_up == 0)
		camera->SetViewUp(1, 0, 0);
	else if (view_up == 1)
		camera->SetViewUp(0, 1, 0);
	else if (view_up == 2)
		camera->SetViewUp(0, 0, 1);
	camera->SetViewAngle(qMax(this->vertical_angle, this->horizontal_angle));
	camera->Roll(this->rotation);

	double view_up[3];
	camera->GetPosition(pupil);
	camera->GetFocalPoint(target);
	camera->GetViewUp(view_up);

	double view_up_cam_wanted[3];
	double view_up_cam_current[3];
	double view_up_machine[3] = { 0, 0, 1 };
	vtkMatrix4x4* view_tr = camera->GetViewTransformMatrix();
	vtkMatrix3x3* mat = vtkMatrix3x3::New();
	mat->SetElement(0, 0, view_tr->GetElement(0, 0));
	mat->SetElement(1, 0, view_tr->GetElement(1, 0));
	mat->SetElement(2, 0, view_tr->GetElement(2, 0));
	mat->SetElement(0, 1, view_tr->GetElement(0, 1));
	mat->SetElement(1, 1, view_tr->GetElement(1, 1));
	mat->SetElement(2, 1, view_tr->GetElement(2, 1));
	mat->SetElement(0, 2, view_tr->GetElement(0, 2));
	mat->SetElement(1, 2, view_tr->GetElement(1, 2));
	mat->SetElement(2, 2, view_tr->GetElement(2, 2));
	mat->MultiplyPoint(view_up_machine, view_up_cam_wanted);
	mat->MultiplyPoint(view_up, view_up_cam_current);
	mat->Delete();

	// compute rotation to a view up of 0,0,1

	/*double rotation = std::acos( (view_up_cam_wanted[0]*view_up_cam_current[0]+view_up_cam_wanted[1]*view_up_cam_current[1]) /
	(sqrt(view_up_cam_wanted[0]*view_up_cam_wanted[0]+view_up_cam_wanted[1]*view_up_cam_wanted[1])*
	sqrt(view_up_cam_current[0]*view_up_cam_current[0]+view_up_cam_current[1]*view_up_cam_current[1]))) * 57.295779513;*/

	double dot = view_up_cam_wanted[0] * view_up_cam_current[0] + view_up_cam_wanted[1] * view_up_cam_current[1]; // dot product
	double det = view_up_cam_wanted[0] * view_up_cam_current[1] - view_up_cam_wanted[1] * view_up_cam_current[0]; // determinant
	double rot = -std::atan2(det, dot) * 57.295779513;
	;

	this->view_up = 2;
	this->rotation = rot;
	camera->Delete();
}

void VipFieldOfView::setAngles(double pitch, double roll, double yaw)
{
	setViewUpZ();
	rotation = roll;

	double d = dist(pupil, target);
	if (d <= 0)
		d = 10;

	double is_90 = cos(pitch * Vip::ToRadian);
	if (qFuzzyCompare(is_90 + 1, 1)) {
		target[0] = pupil[0];
		target[1] = pupil[1];
		target[2] = pupil[2] + d * tan(pitch * Vip::ToRadian);
	}
	else {
		target[0] = pupil[0] + d * sin(yaw * Vip::ToRadian);
		target[1] = pupil[1] + d * cos(yaw * Vip::ToRadian);
		target[2] = pupil[2] + d * tan(pitch * Vip::ToRadian);
	}
}

void VipFieldOfView::computeAngles(double& pitch, double& roll, double& yaw)
{
	setViewUpZ();
	double direction[3] = { target[0] - pupil[0], target[1] - pupil[1], target[2] - pupil[2] };
	double dist_xy = qSqrt(direction[0] * direction[0] + direction[1] * direction[1]);
	double dist_xyz = qSqrt(direction[1] * direction[1] + direction[0] * direction[0] + direction[2] * direction[2]);
	yaw = acos(direction[1] / dist_xy) * Vip::ToDegree;
	if (yaw > 0 && direction[0] < 0)
		yaw = -yaw;
	pitch = asin(direction[2] / dist_xyz) * Vip::ToDegree;
	roll = rotation;
}

void VipFieldOfView::fieldOfViewCorners(double* topLeft, double* topRight, double* bottomRight, double* bottomLeft, double depth) const
{
	vtkCamera* cam = vtkCamera::New();
	changePointOfView(cam);

	vtkSmartPointer<vtkPoints> pts = getPyramidFOV(*this, cam->GetViewTransformMatrix(), depth);

	memcpy(topLeft, pts->GetPoint(0), sizeof(double) * 3);
	memcpy(topRight, pts->GetPoint(1), sizeof(double) * 3);
	memcpy(bottomRight, pts->GetPoint(2), sizeof(double) * 3);
	memcpy(bottomLeft, pts->GetPoint(3), sizeof(double) * 3);

	cam->Delete();
}

void VipFieldOfView::toFieldAttributes(VipVTKObject data) const
{
	data.setFieldAttribute("FOV horizontal", vtkVariantList() << horizontal_angle);
	data.setFieldAttribute("FOV rotation", vtkVariantList() << rotation);
	data.setFieldAttribute("FOV focal", vtkVariantList() << focal);
	data.setFieldAttribute("FOV width", vtkVariantList() << width);
	data.setFieldAttribute("FOV height", vtkVariantList() << height);
	data.setFieldAttribute("FOV start x", vtkVariantList() << crop_x);
	data.setFieldAttribute("FOV start y", vtkVariantList() << crop_y);
	data.setFieldAttribute("FOV zoom", vtkVariantList() << zoom);
	data.setFieldAttribute("FOV K2", vtkVariantList() << K2);
	data.setFieldAttribute("FOV K4", vtkVariantList() << K4);
	data.setFieldAttribute("FOV K6", vtkVariantList() << K6);
	data.setFieldAttribute("FOV P1", vtkVariantList() << P1);
	data.setFieldAttribute("FOV P2", vtkVariantList() << P2);
}

double VipFieldOfView::preferredDepth(double bounds[6]) const
{
	// check for valid bounds
	for (int i = 0; i < 3; ++i) {
		if (bounds[i * 2 + 1] <= bounds[i * 2])
			return 1;
	}

	double center[3];
	// worst case scenario: add the longest side
	double max_side = -std::numeric_limits<double>::max();
	for (int i = 0; i < 3; ++i) {
		center[i] = (bounds[i * 2 + 1] + bounds[i * 2]) * 0.5;
		max_side = qMax(max_side, (bounds[i * 2 + 1] - bounds[i * 2]));
	}

	double v[3] = { center[0] - pupil[0], center[1] - pupil[1], center[2] - pupil[2] };
	double len = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + max_side;
	return len;
}

void VipFieldOfView::opticalAxis(VipVTKObject& out, double depth) const
{
	if (!out)
		out = VipVTKObject();

	double v[3] = { target[0] - pupil[0], target[1] - pupil[1], target[2] - pupil[2] };
	double len_square = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
	double factor = sqrt(depth * depth / len_square);

	double start[3] = { pupil[0], pupil[1], pupil[2] };
	double end[3] = { pupil[0] + v[0] * factor, pupil[1] + v[1] * factor, pupil[2] + v[2] * factor };

	vtkPolyData* data = out.polyData();
	if (!data) {
		vtkSmartPointer<vtkPolyData> tmp = vtkSmartPointer<vtkPolyData>::New();
		out = VipVTKObject(tmp);
		data = tmp;
	}

	VipVTKObjectLocker lock = vipLockVTKObjects(out);
	createLine(start, end, out);
}

void VipFieldOfView::pyramid(VipVTKObject& out, double depth) const
{
	if (!out)
		out = VipVTKObject();

	if (vtkUnstructuredGrid* grid = out.unstructuredGrid()) {
		vtkSmartPointer<vtkPoints> pt = getPyramidFOV(*this, depth);

		vtkSmartPointer<vtkPyramid> pyramid = vtkSmartPointer<vtkPyramid>::New();
		pyramid->GetPointIds()->SetId(0, 0);
		pyramid->GetPointIds()->SetId(1, 1);
		pyramid->GetPointIds()->SetId(2, 2);
		pyramid->GetPointIds()->SetId(3, 3);
		pyramid->GetPointIds()->SetId(4, 4);

		vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
		cells->InsertNextCell(pyramid);

		grid->Reset();
		grid->SetPoints(pt);
		grid->InsertNextCell(pyramid->GetCellType(), pyramid->GetPointIds());
	}
	else {
		out = VipVTKObject(createPyramid(*this, depth));
	}
}

bool VipFieldOfView::intersectWith(const VipFieldOfView& fov, double depth) const
{
	vtkSmartPointer<vtkPoints> this_points = getPyramidFOV(*this, depth);
	vtkSmartPointer<vtkPoints> other_points = getPyramidFOV(fov, depth);

	// check all lines of this into other
	for (int i = 0; i < 4; ++i) {
		double p1[3], p2[3];
		this_points->GetPoint(4, p1);
		this_points->GetPoint(i, p2);

		double tr1[3][3], tr2[3][3], tr3[3][3], tr4[3][3];
		other_points->GetPoint(4, tr1[0]);
		other_points->GetPoint(0, tr1[1]);
		other_points->GetPoint(1, tr1[2]);

		other_points->GetPoint(4, tr2[0]);
		other_points->GetPoint(1, tr2[1]);
		other_points->GetPoint(2, tr2[2]);

		other_points->GetPoint(4, tr3[0]);
		other_points->GetPoint(2, tr3[1]);
		other_points->GetPoint(3, tr3[2]);

		other_points->GetPoint(4, tr4[0]);
		other_points->GetPoint(3, tr4[1]);
		other_points->GetPoint(0, tr4[2]);

		if (intersectsTriangle(p1, p2, tr1[0], tr1[1], tr1[2]) > 0)
			return true;
		if (intersectsTriangle(p1, p2, tr2[0], tr2[1], tr2[2]) > 0)
			return true;
		if (intersectsTriangle(p1, p2, tr3[0], tr3[1], tr3[2]) > 0)
			return true;
		if (intersectsTriangle(p1, p2, tr4[0], tr4[1], tr4[2]) > 0)
			return true;
	}

	// check all lines of this into other
	for (int i = 0; i < 4; ++i) {
		double p1[3], p2[3];
		other_points->GetPoint(4, p1);
		other_points->GetPoint(i, p2);

		double tr1[3][3], tr2[3][3], tr3[3][3], tr4[3][3];
		this_points->GetPoint(4, tr1[0]);
		this_points->GetPoint(0, tr1[1]);
		this_points->GetPoint(1, tr1[2]);

		this_points->GetPoint(4, tr2[0]);
		this_points->GetPoint(1, tr2[1]);
		this_points->GetPoint(2, tr2[2]);

		this_points->GetPoint(4, tr3[0]);
		this_points->GetPoint(2, tr3[1]);
		this_points->GetPoint(3, tr3[2]);

		this_points->GetPoint(4, tr4[0]);
		this_points->GetPoint(3, tr4[1]);
		this_points->GetPoint(0, tr4[2]);

		if (intersectsTriangle(p1, p2, tr1[0], tr1[1], tr1[2]) > 0)
			return true;
		if (intersectsTriangle(p1, p2, tr2[0], tr2[1], tr2[2]) > 0)
			return true;
		if (intersectsTriangle(p1, p2, tr3[0], tr3[1], tr3[2]) > 0)
			return true;
		if (intersectsTriangle(p1, p2, tr4[0], tr4[1], tr4[2]) > 0)
			return true;
	}

	// check if other pupil is inside this pyramid
	{
		vtkSmartPointer<vtkPyramid> pyramid = vtkSmartPointer<vtkPyramid>::New();
		pyramid->GetPointIds()->SetId(0, 0);
		pyramid->GetPointIds()->SetId(1, 1);
		pyramid->GetPointIds()->SetId(2, 2);
		pyramid->GetPointIds()->SetId(3, 3);
		pyramid->GetPointIds()->SetId(4, 4);

		vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
		cells->InsertNextCell(pyramid);

		vtkSmartPointer<vtkUnstructuredGrid> ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
		ug->SetPoints(this_points);
		ug->InsertNextCell(pyramid->GetCellType(), pyramid->GetPointIds());
		vtkPyramid* cell = static_cast<vtkPyramid*>(ug->GetCell(0));
		double dist, closest[3], pcoord[3], weights[5];
		int sub_id;
		if (cell->EvaluatePosition((double*)fov.pupil, closest, sub_id, pcoord, dist, weights))
			return true;
	}
	// check if ths pupil is inside other pyramid
	{
		vtkSmartPointer<vtkPyramid> pyramid = vtkSmartPointer<vtkPyramid>::New();
		pyramid->GetPointIds()->SetId(0, 0);
		pyramid->GetPointIds()->SetId(1, 1);
		pyramid->GetPointIds()->SetId(2, 2);
		pyramid->GetPointIds()->SetId(3, 3);
		pyramid->GetPointIds()->SetId(4, 4);

		vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
		cells->InsertNextCell(pyramid);

		vtkSmartPointer<vtkUnstructuredGrid> ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
		ug->SetPoints(other_points);
		ug->InsertNextCell(pyramid->GetCellType(), pyramid->GetPointIds());
		vtkPyramid* cell = static_cast<vtkPyramid*>(ug->GetCell(0));
		double dist, closest[3], pcoord[3], weights[5];
		int sub_id;
		if (cell->EvaluatePosition((double*)pupil, closest, sub_id, pcoord, dist, weights))
			return true;
	}

	return false;
}

vtkSmartPointer<vtkUnstructuredGrid> VipFieldOfView::createPyramid(const VipFieldOfView& cam, double depth)
{
	vtkSmartPointer<vtkPoints> pt = getPyramidFOV(cam, depth);

	vtkSmartPointer<vtkPyramid> pyramid = vtkSmartPointer<vtkPyramid>::New();
	pyramid->GetPointIds()->SetId(0, 0);
	pyramid->GetPointIds()->SetId(1, 1);
	pyramid->GetPointIds()->SetId(2, 2);
	pyramid->GetPointIds()->SetId(3, 3);
	pyramid->GetPointIds()->SetId(4, 4);

	vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();
	cells->InsertNextCell(pyramid);

	vtkSmartPointer<vtkUnstructuredGrid> ug = vtkSmartPointer<vtkUnstructuredGrid>::New();
	ug->SetPoints(pt);
	ug->InsertNextCell(pyramid->GetCellType(), pyramid->GetPointIds());

	return ug;
}

vtkSmartPointer<vtkPoints> VipFieldOfView::getPyramidFOV(const VipFieldOfView& cam, double depth)
{
	vtkCamera* view = vtkCamera::New();
	cam.changePointOfView(view);
	vtkMatrix4x4* view_tr = view->GetViewTransformMatrix();
	vtkSmartPointer<vtkPoints> res = getPyramidFOV(cam, view_tr, depth);
	view->Delete();
	return res;
}

vtkSmartPointer<vtkPoints> VipFieldOfView::getPyramidFOV(const VipFieldOfView& cam, const vtkMatrix4x4* view_tr, double depth)
{
	vtkMatrix3x3* mat = vtkMatrix3x3::New();

	mat->SetElement(0, 0, view_tr->GetElement(0, 0));
	mat->SetElement(1, 0, view_tr->GetElement(1, 0));
	mat->SetElement(2, 0, view_tr->GetElement(2, 0));

	mat->SetElement(0, 1, view_tr->GetElement(0, 1));
	mat->SetElement(1, 1, view_tr->GetElement(1, 1));
	mat->SetElement(2, 1, view_tr->GetElement(2, 1));

	mat->SetElement(0, 2, view_tr->GetElement(0, 2));
	mat->SetElement(1, 2, view_tr->GetElement(1, 2));
	mat->SetElement(2, 2, view_tr->GetElement(2, 2));

	// CHECK cancel
	/*if(this->cont == false)
	return;*/

	vtkMatrix3x3* P_Inv = vtkMatrix3x3::New();
	mat->Invert(mat, P_Inv);

	const double* pupil = cam.pupil;
	double vertical_angle = cam.vertical_angle;
	double horizontal_angle = cam.horizontal_angle;

	// Depth of the temporary pyramid
	double prof = -depth;

	// Initialization of the array of selected points
	/*for(int i = 0; i<points->GetAllPoints()->GetNumberOfPoints(); i++){
	points->setSelected(i,0);
	}*/

	// Coordinates of the points in the camera coordinate system
	const double p0[3] = { tan((horizontal_angle / 2) * M_PI / 180) * prof, tan((vertical_angle / 2) * M_PI / 180) * prof, prof };
	const double p1[3] = { tan((horizontal_angle / 2) * M_PI / 180) * prof, -tan((vertical_angle / 2) * M_PI / 180) * prof, prof };
	const double p2[3] = { -tan((horizontal_angle / 2) * M_PI / 180) * prof, -tan((vertical_angle / 2) * M_PI / 180) * prof, prof };
	const double p3[3] = { -tan((horizontal_angle / 2) * M_PI / 180) * prof, tan((vertical_angle / 2) * M_PI / 180) * prof, prof };
	const double p4[3] = { 0.0, 0.0, 0.0 };

	double f0[3];
	double f1[3];
	double f2[3];
	double f3[3];
	double f4[3];

	// Rotation of the coordinates
	P_Inv->MultiplyPoint(p0, f0);
	P_Inv->MultiplyPoint(p1, f1);
	P_Inv->MultiplyPoint(p2, f2);
	P_Inv->MultiplyPoint(p3, f3);
	P_Inv->MultiplyPoint(p4, f4);

	// Translation
	f0[0] += pupil[0];
	f0[1] += pupil[1];
	f0[2] += pupil[2];

	f1[0] += pupil[0];
	f1[1] += pupil[1];
	f1[2] += pupil[2];

	f2[0] += pupil[0];
	f2[1] += pupil[1];
	f2[2] += pupil[2];

	f3[0] += pupil[0];
	f3[1] += pupil[1];
	f3[2] += pupil[2];

	f4[0] += pupil[0];
	f4[1] += pupil[1];
	f4[2] += pupil[2];

	// Creation of the pyramid
	vtkSmartPointer<vtkPoints> points_pyra = vtkSmartPointer<vtkPoints>::New();
	points_pyra->InsertNextPoint(f0);
	points_pyra->InsertNextPoint(f1);
	points_pyra->InsertNextPoint(f2);
	points_pyra->InsertNextPoint(f3);
	points_pyra->InsertNextPoint(f4);

	mat->Delete();
	P_Inv->Delete();

	return points_pyra;
}
/*
vtkSmartPointer<vtkActor> drawSphere(vtkRenderer * r, const double * pt, double radius, double * color)
{
	// Create a sphere
	vtkSmartPointer<vtkSphereSource> sphereSource =
		vtkSmartPointer<vtkSphereSource>::New();
	sphereSource->SetCenter(const_cast<double*>(pt));
	sphereSource->SetRadius(radius);

	vtkSmartPointer<vtkPolyDataMapper> mapper =
		vtkSmartPointer<vtkPolyDataMapper>::New();
	mapper->SetInputConnection(sphereSource->GetOutputPort());

	vtkSmartPointer<vtkActor> actor =
		vtkSmartPointer<vtkActor>::New();
	actor->SetMapper(mapper);

	if (color)
		actor->GetProperty()->SetColor(color);
	else
		actor->GetProperty()->SetColor(0, 1, 0);

	r->AddActor(actor);

	return actor;
}
*/

bool VipFieldOfView::createLine(const double* pt1, const double* pt2, VipVTKObject& data)
{
	if (!data.polyData())
		return false;
	vtkPolyData* poly = data.polyData();

	bool is_valid = false;
	if (poly->GetPoints() && poly->GetPoints()->GetNumberOfPoints() == 2) {
		if (memcmp(pt1, poly->GetPoints()->GetPoint(0), 3 * sizeof(double)) == 0)
			if (memcmp(pt2, poly->GetPoints()->GetPoint(1), 3 * sizeof(double)) == 0)
				is_valid = true;
	}

	if (!is_valid) {
		// Create a vtkPoints object and store the points in it
		vtkSmartPointer<vtkPoints> pts = vtkSmartPointer<vtkPoints>::New();
		pts->InsertNextPoint(pt1);
		pts->InsertNextPoint(pt2);

		// Create the first line (between Origin and P0)
		vtkSmartPointer<vtkLine> line0 = vtkSmartPointer<vtkLine>::New();
		line0->GetPointIds()->SetId(0, 0); // the second 0 is the index of the Origin in the vtkPoints
		line0->GetPointIds()->SetId(1, 1); // the second 1 is the index of P0 in the vtkPoints

		// Create a cell array to store the lines in and add the lines to it
		vtkSmartPointer<vtkCellArray> line = vtkSmartPointer<vtkCellArray>::New();
		line->InsertNextCell(line0);

		// Add the points to the dataset
		poly->SetPoints(pts);

		// Add the lines to the dataset
		poly->SetLines(line);

		/*// Visualize
		vtkSmartPointer<vtkPolyDataMapper> mapper =
			vtkSmartPointer<vtkPolyDataMapper>::New();
	#if VTK_MAJOR_VERSION <= 5
		mapper->SetInputData(data);
	#else
		mapper->SetInputData(data);
	#endif

		vtkSmartPointer<vtkActor> actor =
			vtkSmartPointer<vtkActor>::New();
		actor->SetMapper(mapper);
		*/
	}

	return true;
}


#define GD_semiMajorAxis 6378137.000000
#define GD_TranMercB 6356752.314245
#define GD_geocentF 0.003352810664

void geodeticOffsetInv(double refLon, double refLat, double lon, double lat, double& xOffset, double& yOffset)
{
	double a = GD_semiMajorAxis;
	double b = GD_TranMercB;
	double f = GD_geocentF;

	double L = lon - refLon;
	double U1 = atan((1 - f) * tan(refLat));
	double U2 = atan((1 - f) * tan(lat));
	double sinU1 = sin(U1);
	double cosU1 = cos(U1);
	double sinU2 = sin(U2);
	double cosU2 = cos(U2);

	double lambda = L;
	double lambdaP;
	double sinSigma;
	double sigma;
	double cosSigma;
	double cosSqAlpha;
	double cos2SigmaM;
	double sinLambda;
	double cosLambda;
	double sinAlpha;
	int iterLimit = 100;
	do {
		sinLambda = sin(lambda);
		cosLambda = cos(lambda);
		sinSigma = sqrt((cosU2 * sinLambda) * (cosU2 * sinLambda) + (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda) * (cosU1 * sinU2 - sinU1 * cosU2 * cosLambda));
		if (sinSigma == 0) {
			xOffset = 0.0;
			yOffset = 0.0;
			return; // co-incident points
		}
		cosSigma = sinU1 * sinU2 + cosU1 * cosU2 * cosLambda;
		sigma = atan2(sinSigma, cosSigma);
		sinAlpha = cosU1 * cosU2 * sinLambda / sinSigma;
		cosSqAlpha = 1 - sinAlpha * sinAlpha;
		cos2SigmaM = cosSigma - 2 * sinU1 * sinU2 / cosSqAlpha;
		if (cos2SigmaM != cos2SigmaM) // isNaN
		{
			cos2SigmaM = 0; // equatorial line: cosSqAlpha=0 (§6)
		}
		double C = f / 16 * cosSqAlpha * (4 + f * (4 - 3 * cosSqAlpha));
		lambdaP = lambda;
		lambda = L + (1 - C) * f * sinAlpha * (sigma + C * sinSigma * (cos2SigmaM + C * cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM)));
	} while (fabs(lambda - lambdaP) > 1e-12 && --iterLimit > 0);

	if (iterLimit == 0) {
		xOffset = 0.0;
		yOffset = 0.0;
		return; // formula failed to converge
	}

	double uSq = cosSqAlpha * (a * a - b * b) / (b * b);
	double A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
	double B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
	double deltaSigma =
	  B * sinSigma * (cos2SigmaM + B / 4 * (cosSigma * (-1 + 2 * cos2SigmaM * cos2SigmaM) - B / 6 * cos2SigmaM * (-3 + 4 * sinSigma * sinSigma) * (-3 + 4 * cos2SigmaM * cos2SigmaM)));
	double s = b * A * (sigma - deltaSigma);

	double bearing = atan2(cosU2 * sinLambda, cosU1 * sinU2 - sinU1 * cosU2 * cosLambda);
	yOffset = cos(bearing) * s;
	xOffset = sin(bearing) * s;
}






FieldOfViewPyramid FieldOfViewPyramid::fromFieldOfView(const VipFieldOfView& fov)
{
	vtkCamera* view = vtkCamera::New();
	fov.changePointOfView(view);
	vtkMatrix4x4* view_tr = view->GetViewTransformMatrix();

	FieldOfViewPyramid res;
	res.tr = vtkSmartPointer<vtkMatrix3x3>::New();
	vtkMatrix3x3* mat = res.tr.GetPointer();

	mat->SetElement(0, 0, view_tr->GetElement(0, 0));
	mat->SetElement(1, 0, view_tr->GetElement(1, 0));
	mat->SetElement(2, 0, view_tr->GetElement(2, 0));

	mat->SetElement(0, 1, view_tr->GetElement(0, 1));
	mat->SetElement(1, 1, view_tr->GetElement(1, 1));
	mat->SetElement(2, 1, view_tr->GetElement(2, 1));

	mat->SetElement(0, 2, view_tr->GetElement(0, 2));
	mat->SetElement(1, 2, view_tr->GetElement(1, 2));
	mat->SetElement(2, 2, view_tr->GetElement(2, 2));

	const double* pupil = fov.pupil;
	double vertical_angle = fov.vertical_angle;
	double horizontal_angle = fov.horizontal_angle;

	memcpy(res.start, pupil, 3 * sizeof(double));
	res.xAngles[0] = -(horizontal_angle / 2) * M_PI / 180;
	res.xAngles[1] = (horizontal_angle / 2) * M_PI / 180;
	res.yAngles[0] = -(vertical_angle / 2) * M_PI / 180;
	res.yAngles[1] = (vertical_angle / 2) * M_PI / 180;
	res.tr = mat;

	view->Delete();
	return res;
}


void VipFieldOfView::extractZBounds(const VipVTKObjectList& pt, const VipFieldOfView& cam, double& min, double& max)
{
	auto pyramid_ = FieldOfViewPyramid::fromFieldOfView(cam);
	auto locks = vipLockVTKObjects(pt);
	min = std::numeric_limits<double>::max();
	max = -std::numeric_limits<double>::max();

	for (int i = 0; i < pt.size(); ++i) {
		VipVTKObject ptr = pt[i];

		vtkDataSet* set = ptr.dataSet();
		int num_points = set->GetNumberOfPoints();

#pragma omp parallel for shared(pyramid_)
		for (int j = 0; j < num_points; ++j) {
			double point[3];
			set->GetPoint(j, point);
			if (pyramid_.isInside(point)) {
				point[0] = point[0] - cam.pupil[0];
				point[1] = point[1] - cam.pupil[1];
				point[2] = point[2] - cam.pupil[2];
				double dist = sqrt(point[0] * point[0] + point[1] * point[1] + point[2] * point[2]);

#pragma omp critical
				{
					min = qMin(min, dist);
					max = qMax(max, dist);
				}
			}
		}
	}
}