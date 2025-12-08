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

#ifndef VIP_VTK_REGION_H
#define VIP_VTK_REGION_H

#include "VipSceneModel.h"
#include "VipVTKGraphicsView.h"
#include "VipProcessingObject.h"


/// @brief Represents a Region Of Interset (ROI) over a VTK object
struct VIP_PLOTTING_EXPORT VipVTKRegionObject
{
	QPointer<VipPlotVTKObject> source;
	QVector<vtkIdType> pointIds;
	QVector<vtkIdType> cellIds;
	QVector<vtkIdType> completeCellIds;
};

/// @brief VipVTKRegion represents a Region Of Interset (ROI) over multiple VTK objects sharing the same renderer
struct VIP_PLOTTING_EXPORT VipVTKRegion
{
	vtkRenderer* renderer = nullptr;
	QVector<VipVTKRegionObject> regions;
	QString name;

	bool isEmpty() const { return regions.isEmpty(); }
};

Q_DECLARE_METATYPE(VipVTKRegionObject)
Q_DECLARE_METATYPE(VipVTKRegion)

/// @brief Extract ROI corresponding to a shape on given VipVTKGraphicsView and renderer.
/// 
/// Create a ROI based on given shape over a VipGraphicsView.
/// This will create a VipVTKRegion object that contains information
/// on all visible cells/points for any VipVTKObject on given renderer_level.
/// 
VIP_PLOTTING_EXPORT VipVTKRegion vipBuildRegion(const VipPlotShape* sh, VipVTKGraphicsView* view, int renderer_level = 0);

/// @brief Create a vkDataSet representing a VipVTKRegion object.
/// The result VipVTKObject contains the dataset and the property "VipVTKRegion" set to the input region.
/// 
VIP_PLOTTING_EXPORT VipVTKObject vipBuildRegionObject(const VipVTKRegion& r);

/// @brief Extract attributes of given VipVTKRegion
VIP_PLOTTING_EXPORT QVector<QPair<QString, QVariant>> vipBuildRegionAttributes(const VipVTKRegion& r, const QVector<VipVTKObject>& objects = QVector<VipVTKObject>());

/// @brief Processing object used to display a VipVTKRegion.
/// 
/// The VipVTKRegionProcessing updates itself when the source objects
/// of the VipVTKRegion are updated.
/// 
/// The VipVTKRegionProcessing destroys itself when all objects of the 
/// underlying VipVTKRegion are destroyed.
/// 
class VIP_PLOTTING_EXPORT VipVTKRegionProcessing : public VipProcessingObject
{
	Q_OBJECT

	VIP_IO(VipMultiInput inputs)
	VIP_IO(VipOutput output)
public:
	VipVTKRegionProcessing(QObject* parent = nullptr);
	~VipVTKRegionProcessing();

	void setRegion(const VipVTKRegion&);
	VipVTKRegion region();

protected:
	virtual void apply();

private Q_SLOTS:
	void recompute();

private:
	VIP_DECLARE_PRIVATE_DATA();
};
VIP_REGISTER_QOBJECT_METATYPE(VipVTKRegionProcessing*)

/// @brief Extract attribute histogram of one or more VipVTKObject
///
class VIP_PLOTTING_EXPORT VipVTKExtractHistogram : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty bins)

public:
	VipVTKExtractHistogram(QObject* parent = nullptr);
	~VipVTKExtractHistogram();

	void setArrayName(const QString& name);
	QString arrayName() const;

	void setComponent(int comp);
	int component() const;

	void setAttributeType(VipVTKObject::AttributeType);
	VipVTKObject::AttributeType attributeType() const;

protected:
	virtual void apply();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_REGISTER_QOBJECT_METATYPE(VipVTKExtractHistogram*)

#endif
