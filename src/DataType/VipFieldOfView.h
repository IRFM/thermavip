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

#ifndef VIP_FIELD_OF_VIEW_H
#define VIP_FIELD_OF_VIEW_H

#include "VipVTKObject.h"

#include <QTextStream>

#include <vtkCamera.h>
#include <vtkMatrix3x3.h>

#include <cmath>

class vtkRenderWindow;

/// @brief Class representing a camera field of view.
///
/// VipFieldOfView is very similar to vtkCamera, but is supposed to
/// represent all camera properties, including the matrix size
/// and optical distortions using a barrel model.
/// 
/// VipFieldOfView can be converted from/to vtkCamera, and
/// is serializable to/from a VipArchive
/// 
class VIP_DATA_TYPE_EXPORT VipFieldOfView
{

public:
	VipFieldOfView() = default;
	VipFieldOfView(const VipFieldOfView& other) = default;
	VipFieldOfView& operator=(const VipFieldOfView& other) = default;
	bool operator==(const VipFieldOfView& fov) const;
	bool operator!=(const VipFieldOfView& fov) const;

	/// @brief Returns true if the VipFieldOfView has a null name
	bool isNull() const;

	/// @brief Print the camera's informations
	QString print() const;

	/// @brief Compute the focal point position
	void focalPoint(double* pt) const;

	/// @brief Modify the point of view of the 3D scene
	/// @param cam_cour VTK camera object which will be modified by the current camera parameters
	void changePointOfView(vtkCamera* cam_cour, double target_dist = -1) const;
	void changePointOfView(vtkRenderWindow* win, double target_dist = -1) const;

	/// @brief Import camera settings except view angles, width, height, name, optical distortions, time and attributes
	void importCamera(vtkCamera* camera);

	/// @brief Keep the camera position and angles unchanged, but express them with a Z view up
	void setViewUpZ();

	/// @brief Set exactly the pitch, roll and yaw angles, considering a Z view up and a yaw(0) = Y axis
	void setAngles(double pitch, double roll, double yaw);

	/// @brief Compute the pitch, roll and yaw angles based on the pupil position, target point and rotation.
	/// This switches the camera to a Z view up.
	void computeAngles(double& pitch, double& roll, double& yaw);

	/// @brief Based on given depth, compute the FOV corners of the FOV pyramid
	void fieldOfViewCorners(double* topLeft, double* topRight, double* bottomRight, double* bottomLeft, double depth = 1000) const;

	/// @brief Add this FOV parameters as field attributes to data
	void toFieldAttributes(VipVTKObject data) const;

	/// @brief Given 3D bounds, compute the preffered depth for this FOV.
	/// This is used to build the FOV pyramid.
	double preferredDepth(double bounds[6]) const;

	/// @brief Build the FOV optical axis based on provided depth
	void opticalAxis(VipVTKObject& out, double depth) const;
	/// @brief Build the FOV pyramid based on provided depth
	void pyramid(VipVTKObject& out, double depth) const;

	/// @brief Check if this FOV pyramid intersects with fov for given depth
	bool intersectWith(const VipFieldOfView& fov, double depth) const;

	QString name;			//! Name of the camera
	double pupil[3] = { 0, 0, 0 };	//! Coordinates of the position of the pupil
	double target[3] = { 1, 1, 1 }; //! Coordinates of the target point
	double vertical_angle = 60;	//! Vertical field of view
	double horizontal_angle = 60;	//! Horizontal field of view
	double rotation = 0;		//! Rotation of the camera
	double focal = 0.01;		//! Focal length of the camera
	int view_up = 2;		//! Camera axis view up (0 for x, 1 for y, 2 for z)
	int width = 100;		//! Number of horizontal pixels of a picture recorded by the camera
	int height = 100;		//! Number of vertical pixels of a picture recorded by the camera
	int crop_x = 0;			//! Horizontal coordinate of the top left corner of the cropped picture in the entire picture
	int crop_y = 0;			//! Vertical coordinate of the top left corner of the cropped picture in the entire picture
	double zoom = 1;		//! Zoom parameter to create the exact texture*/

	// Optical distortions
	double K2 = 0;
	double K4 = 0;
	double K6 = 0;
	double P1 = 0;
	double P2 = 0;
	double AlphaC = 0;

	// Time and attributes
	qint64 time = 0;
	QVariantMap attributes;

	/// @brief Helper function, convert a 3D point to QString
	static QString pointToString(const double* coord) { return QString::number(coord[0], 'g', 18) + " " + QString::number(coord[1], 'g', 18) + " " + QString::number(coord[2], 'g', 18); }
	/// @brief Helper function, convert QString to 3D point
	static void pointFromString(double* coord, const QString& str)
	{
		QTextStream stream(str.toLatin1());
		stream >> coord[0] >> coord[1] >> coord[2];
	}

	/// @brief Helper function, create a line
	static bool createLine(const double* pt1, const double* pt2, VipVTKObject& data);
	/// @brief Helper function, create a pyramid based on a FOV
	static vtkSmartPointer<vtkUnstructuredGrid> createPyramid(const VipFieldOfView& cam, double depth);
	static vtkSmartPointer<vtkPoints> getPyramidFOV(const VipFieldOfView& cam, double depth);
	static vtkSmartPointer<vtkPoints> getPyramidFOV(const VipFieldOfView& cam, const vtkMatrix4x4* view_tr, double depth);

	static void extractZBounds(const VipVTKObjectList& pt, const VipFieldOfView& cam, double& min, double& max);
};

Q_DECLARE_METATYPE(VipFieldOfView);

typedef QVector<VipFieldOfView> VipFieldOfViewList;
Q_DECLARE_METATYPE(VipFieldOfViewList);


/// @brief Helper class used to check if a 3D point is inside a field of view.
///
class VIP_DATA_TYPE_EXPORT FieldOfViewPyramid
{
	double start[3];
	double xAngles[2];
	double yAngles[2];
	vtkSmartPointer<vtkMatrix3x3> tr;

	static VIP_ALWAYS_INLINE void Angles(const double* v, double& a, double& b)
	{
		a = std::asin(v[0] / std::sqrt(v[0] * v[0] + v[2] * v[2]));
		b = std::asin(v[1] / std::sqrt(v[1] * v[1] + v[2] * v[2]));
	}

	VIP_ALWAYS_INLINE bool Angle(const double* pt, double& x_angle, double& y_angle) const
	{
		double v[3] = { pt[0] - start[0], pt[1] - start[1], pt[2] - start[2] };
		double p[3];
		tr->MultiplyPoint(v, p);

		double a, b;
		Angles(p, a, b);
		x_angle = a;
		y_angle = b;
		return p[2] < 0;
	}

public:
	/// @brief Check if a point is inside the FOV pyramid
	VIP_ALWAYS_INLINE bool isInside(const double* pt) const
	{
		double a, b;
		bool pt_rigth_side = Angle(pt, a, b);
		bool res = (a >= xAngles[0] && a <= xAngles[1] && b >= yAngles[0] && b <= yAngles[1]);
		return res && pt_rigth_side;
	}

	/// @brief Construct a FieldOfViewPyramid from a VipFieldOfView
	static FieldOfViewPyramid fromFieldOfView(const VipFieldOfView& fov);
};

/**
Helper function,.
Convert geodetic coordinates in radian and output the result in cartesian system in meters.
Longitude are considered to be the x axis, and Latitude the y axis (which points to the true north).
\param refLat,refLon : geodetic coordinate which you defined as 0,0 in cartesian coordinate (the unit is in radian)
\param lat,lon : geodetic coordinate which you want to calculate its cartesian coordinate (the unit is in radian)
\param xOffset,yOffset : the result in cartesian coordinate x,y (the unit is in meters)

See http://stackoverflow.com/questions/4953150/convert-lat-longs-to-x-y-co-ordinates for more details
*/
VIP_DATA_TYPE_EXPORT void geodeticOffsetInv(double refLon, double refLat, double lon, double lat, double& xOffset, double& yOffset);

#endif