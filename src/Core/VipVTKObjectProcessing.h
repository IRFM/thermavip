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

#ifndef VIP_VTK_OBJECT_PROCESSING_H
#define VIP_VTK_OBJECT_PROCESSING_H
 
#include "VipVTKObject.h"
#include "VipProcessingObject.h"
#include "VipProcessingHelper.h"

/// @brief Base class for VTK algorithm taking one or more input and having one output of type VipVTKObject
///
class VIP_CORE_EXPORT VipVTKObjectProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipMultiInput input)
	VIP_IO(VipOutput output)
public:
	VipVTKObjectProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		outputAt(0)->setData(VipVTKObject());
		// Set at least one input
		topLevelInputAt(0)->toMultiInput()->resize(1);
	}

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipVTKObject>(); }

	/// @brief Inheriting classes must reimplement this function.
	/// Apply the algorithm on provided list on inputs and at given time, and return the resulting VipVTKObject
	virtual VipVTKObject applyAlgorithm(const VipVTKObjectList& inputs, qint64 time) = 0;

protected:
	virtual void apply();
};

namespace detail
{
	/// @brief Helper class used to wrap VTK algorithm
	/// @tparam VTKAlgorithm
	template<class VTKAlgorithm>
	class VTKHelperProcessing : public VipVTKObjectProcessing
	{
		using function_type = std::function<void(VTKAlgorithm*, VipProcessingObject*, int)>;
		using function_pair_type = std::pair<function_type, function_type>;

		std::vector<function_type> m_get_properties;
		std::vector<function_type> m_set_properties;
		std::vector<std::string> m_accepted_input_names;
		vtkSmartPointer<VTKAlgorithm> m_algo;

	protected:
		template<class Pair>
		int register_property(const Pair& p)
		{
			// register a getter and setter property.
			// called with VTK_DECLARE_PROPERTY familly of macros.
			m_get_properties.push_back(p.first);
			m_set_properties.push_back(p.second);
			return 0;
		}

		void initializeIO()
		{
			// Initialize properties with default VTK algorithm properties values
			if (propertyCount() >= m_get_properties.size()) {

				// Set processing properties
				for (int i = 0; i < m_get_properties.size(); ++i) {
					m_get_properties[i](m_algo, this, i);
				}
			}
		}

	public:
		using base_type = VTKHelperProcessing<VTKAlgorithm>;
		using algorithm_type = VTKAlgorithm;

		template<class InputType>
		VTKHelperProcessing(std::initializer_list<InputType> input_type_names)
		  : VipVTKObjectProcessing()
		{
			m_algo = vtkSmartPointer<VTKAlgorithm>::New();

			// Add accepted input class names
			for (auto it = input_type_names.begin(); it != input_type_names.end(); ++it)
				m_accepted_input_names.push_back(*it);

			setIOInitializeFunction([this]() { this->initializeIO(); });
		}

		virtual VipVTKObject transformInput(const VipVTKObject& input, int idx) const
		{
			(void)idx;
			return input;
		}

		virtual bool acceptInput(int /*index*/, const QVariant& v) const
		{
			VipVTKObject obj = v.value<VipVTKObject>();
			for (const std::string& n : m_accepted_input_names)
				if (obj.isA(n.c_str()))
					return true;
			return false;
		}

		virtual VipVTKObject applyAlgorithm(const VipVTKObjectList& inputs, qint64 time)
		{
			VipVTKObjectList tmp(inputs.size());

			for (int i = 0; i < inputs.size(); ++i) {
				tmp[i] = transformInput(inputs[i], i);
				if (!tmp[i]) {
					setError("wrong input type");
					return VipVTKObject();
				}
				m_algo->SetInputData(i, tmp[i].data());
			}
			// Set vtk algorithm properties
			for (int i = 0; i < m_set_properties.size(); ++i) {
				m_set_properties[i](m_algo, this, i);
			}
			m_algo->Update();
			if (m_algo->GetOutput())
				return VipVTKObject(m_algo->GetOutput());
			else
				return VipVTKObject();
		}
	};
}

// Internal, create a pair of functors for getting/setting VTK algorithm property based on a VipProcessingObject
#define _VTK_PROPERTY(name)                                                                                                                                                                            \
	std::pair<std::function<void(algorithm_type*, VipProcessingObject*, int)>, std::function<void(algorithm_type*, VipProcessingObject*, int)>>                                                    \
	{                                                                                                                                                                                              \
		[](algorithm_type* vtk_algo, VipProcessingObject* proc, int idx) { proc->propertyAt(idx)->setData(vtk_algo->Get##name()); },                                                           \
		  [](algorithm_type* vtk_algo, VipProcessingObject* proc, int idx) { vtk_algo->Set##name(proc->propertyAt(idx)->value<decltype(((algorithm_type*)nullptr)->Get##name())>()); }         \
	}

/// @brief Declare a processing object class based on a vtkAlgorithm
#define VTK_DECLARE_ALGORITHM(classname, ...) class __VA_ARGS__ Vip##classname : public detail::VTKHelperProcessing<vtk##classname>

/// @brief Declare a processing property related to a VTK algorithm property
#define VTK_DECLARE_PROPERTY(name)                                                                                                                                                                     \
	Q_PROPERTY(VipProperty name READ io)                                                                                                                                                           \
	int _init_##name = register_property(_VTK_PROPERTY(name))

/// @brief Declare a processing property related to a VTK algorithm property
#define VTK_DECLARE_PROPERTY_2(name, ...)                                                                                                                                                              \
	Q_PROPERTY(VipProperty name READ io)                                                                                                                                                           \
	Q_CLASSINFO(#name, __VA_ARGS__)                                                                                                                                                                \
	int _init_##name = register_property(_VTK_PROPERTY(name))

/// @brief Declare a processing property related to a VTK algorithm property
#define VTK_DECLARE_PROPERTY_3(name, description, ...)                                                                                                                                                 \
	Q_PROPERTY(VipProperty name READ io)                                                                                                                                                           \
	Q_CLASSINFO(#name, description)                                                                                                                                                                \
	Q_CLASSINFO("edit_" #name, __VA_ARGS__)                                                                                                                                                        \
	int _init_##name = register_property(_VTK_PROPERTY(name))

#define VTK_CREATE_ALGORITHM(classname, ...)                                                                                                                                                           \
	Vip##classname()                                                                                                                                                                                    \
	  : base_type({ VIP_FOR_EACH_COMMA(VIP_STRINGIZE, __VA_ARGS__) })

////////////////////////////////////////////////
// Collection of VTK algorithms
////////////////////////////////////////////////

#include <vtkDecimatePro.h>
#include <vtkTriangleFilter.h>
#include <vtkLoopSubdivisionFilter.h>
#include <vtkButterflySubdivisionFilter.h>
#include <vtkLinearSubdivisionFilter.h>
#include <vtkDelaunay2D.h>
#include <vtkDelaunay3D.h>
#include <vtkSmoothPolyDataFilter.h>
#include <vtkAdaptiveSubdivisionFilter.h>
#include <vtkUnstructuredGrid.h>

/*vtkDecimatePro algorithm*/
VTK_DECLARE_ALGORITHM(DecimatePro, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Decimate a polydata with triangular cells. Based on vtkDecimatePro class.")
	Q_CLASSINFO("category", "3D/Polydata");
	VTK_DECLARE_PROPERTY(TargetReduction);
	VTK_DECLARE_PROPERTY(PreserveTopology);
	VTK_DECLARE_PROPERTY(Splitting);
	VTK_DECLARE_PROPERTY(SplitAngle);
	VTK_DECLARE_PROPERTY(PreSplitMesh);
	VTK_DECLARE_PROPERTY(MaximumError);
	VTK_DECLARE_PROPERTY(AccumulateError);
	VTK_DECLARE_PROPERTY(ErrorIsAbsolute);
	VTK_DECLARE_PROPERTY(BoundaryVertexDeletion);
	VTK_DECLARE_PROPERTY(Degree);
	VTK_DECLARE_PROPERTY(InflectionPointRatio);
	VTK_DECLARE_PROPERTY(OutputPointsPrecision);

public:
	VTK_CREATE_ALGORITHM(DecimatePro, vtkPolyData) {}
};

VIP_REGISTER_QOBJECT_METATYPE(VipDecimatePro*)

VTK_DECLARE_ALGORITHM(AdaptiveSubdivisionFilter, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "")
	Q_CLASSINFO("category", "3D/Polydata/Subdivision");
	VTK_DECLARE_PROPERTY_3(MaximumEdgeLength, "Specify the maximum edge length that a triangle may have.", VIP_DOUBLE_EDIT(0, 'g'));
	VTK_DECLARE_PROPERTY_3(MaximumTriangleArea, "Specify the maximum area that a triangle may have.", VIP_DOUBLE_EDIT(0, 'g'));

public:
	VTK_CREATE_ALGORITHM(AdaptiveSubdivisionFilter, vtkPolyData) {}
};
VIP_REGISTER_QOBJECT_METATYPE(VipAdaptiveSubdivisionFilter*)

/*vtkTriangleFilter algorithm*/
VTK_DECLARE_ALGORITHM(TriangleFilter, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Convert input polygons and strips to triangles. Based on vtkTriangleFilter class.")
	Q_CLASSINFO("category", "3D/Polydata")
	//VTK_DECLARE_PROPERTY(PreservePolys);
	VTK_DECLARE_PROPERTY(PassVerts);
	VTK_DECLARE_PROPERTY(PassLines);
	VTK_DECLARE_PROPERTY(Tolerance);

public:
	VTK_CREATE_ALGORITHM(TriangleFilter, vtkPolyData) {}
};
VIP_REGISTER_QOBJECT_METATYPE(VipTriangleFilter*)

/*LoopSubdivisionFilter algorithm*/
VTK_DECLARE_ALGORITHM(LoopSubdivisionFilter, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Generate a subdivision surface using the Loop Scheme. Based on vtkLoopSubdivisionFilter class.")
	Q_CLASSINFO("category", "3D/Polydata/Subdivision");
	VTK_DECLARE_PROPERTY(NumberOfSubdivisions);
	VTK_DECLARE_PROPERTY(CheckForTriangles);

public:
	VTK_CREATE_ALGORITHM(LoopSubdivisionFilter, vtkPolyData) {}
};
VIP_REGISTER_QOBJECT_METATYPE(VipLoopSubdivisionFilter*)

/*vtkButterflySubdivisionFilter algorithm*/
VTK_DECLARE_ALGORITHM(ButterflySubdivisionFilter, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Generate a subdivision surface using the Butterfly Scheme. Based on vtkButterflySubdivisionFilter class.")
	Q_CLASSINFO("category", "3D/Polydata/Subdivision");
	VTK_DECLARE_PROPERTY(NumberOfSubdivisions);
	VTK_DECLARE_PROPERTY(CheckForTriangles);

public:
	VTK_CREATE_ALGORITHM(ButterflySubdivisionFilter, vtkPolyData) {}
};

VIP_REGISTER_QOBJECT_METATYPE(VipButterflySubdivisionFilter*)

/*vtkLinearSubdivisionFilter algorithm*/
VTK_DECLARE_ALGORITHM(LinearSubdivisionFilter, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Generate a subdivision surface using the Linear Scheme. Based on vtkLinearSubdivisionFilter class.")
	Q_CLASSINFO("category", "3D/Polydata/Subdivision");
	VTK_DECLARE_PROPERTY(NumberOfSubdivisions);
	VTK_DECLARE_PROPERTY(CheckForTriangles);

public:
	VTK_CREATE_ALGORITHM(LinearSubdivisionFilter, vtkPolyData) {}
};
VIP_REGISTER_QOBJECT_METATYPE(VipLinearSubdivisionFilter*)

/*vtkDelaunay2D algorithm*/
VTK_DECLARE_ALGORITHM(Delaunay2D, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Create 2D Delaunay triangulation of input points. Based on vtkDelaunay2D class.")
	Q_CLASSINFO("category", "3D/Polydata");
	VTK_DECLARE_PROPERTY(Alpha);
	VTK_DECLARE_PROPERTY(Tolerance);
	VTK_DECLARE_PROPERTY(Offset);
	VTK_DECLARE_PROPERTY(BoundingTriangulation);
	VTK_DECLARE_PROPERTY(ProjectionPlaneMode);

public:
	VTK_CREATE_ALGORITHM(Delaunay2D, vtkPolyData) {}
};
VIP_REGISTER_QOBJECT_METATYPE(VipDelaunay2D*)

/*vtkDelaunay3D algorithm*/
VTK_DECLARE_ALGORITHM(Delaunay3D, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Create 3D Delaunay triangulation of input points. Based on vtkDelaunay3D class.")
	Q_CLASSINFO("category", "3D/UnstructuredGrid");
	VTK_DECLARE_PROPERTY(Alpha);
	VTK_DECLARE_PROPERTY(AlphaTets);
	VTK_DECLARE_PROPERTY(AlphaTris);
	VTK_DECLARE_PROPERTY(AlphaLines);
	VTK_DECLARE_PROPERTY(AlphaVerts);
	VTK_DECLARE_PROPERTY(Tolerance);
	VTK_DECLARE_PROPERTY(Offset);
	VTK_DECLARE_PROPERTY(BoundingTriangulation);
	VTK_DECLARE_PROPERTY(OutputPointsPrecision);

public:
	VTK_CREATE_ALGORITHM(Delaunay3D) {}
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.value<VipVTKObject>().pointSet(); }
	virtual VipVTKObject transformInput(const VipVTKObject& obj, int) const
	{
		VipVTKObject in = obj;
		if (!in.unstructuredGrid()) {
			vtkPointSet* set = in.pointSet();
			if (!set)
				return VipVTKObject();
			vtkUnstructuredGrid* grid = vtkUnstructuredGrid::New();
			vtkPoints* pts = vtkPoints::New();
			pts->DeepCopy(set->GetPoints());
			grid->SetPoints(pts);
			pts->Delete();
			in = VipVTKObject(grid);
			grid->Delete();
		}
		return in;
	}
};
VIP_REGISTER_QOBJECT_METATYPE(VipDelaunay3D*)

/*vtkSmoothPolyDataFilter algorithm*/
VTK_DECLARE_ALGORITHM(SmoothPolyDataFilter, VIP_CORE_EXPORT)
{
	Q_OBJECT
	Q_CLASSINFO("description", "Adjust point positions using Laplacian smoothing")
	Q_CLASSINFO("category", "3D/Polydata")
	VTK_DECLARE_PROPERTY(Convergence);
	VTK_DECLARE_PROPERTY(NumberOfIterations);
	VTK_DECLARE_PROPERTY(RelaxationFactor);
	VTK_DECLARE_PROPERTY(FeatureEdgeSmoothing);
	VTK_DECLARE_PROPERTY(FeatureAngle);
	VTK_DECLARE_PROPERTY(EdgeAngle);
	VTK_DECLARE_PROPERTY(BoundarySmoothing);
	VTK_DECLARE_PROPERTY(OutputPointsPrecision);

public:
	VTK_CREATE_ALGORITHM(SmoothPolyDataFilter, vtkPolyData) {}
};
 
VIP_REGISTER_QOBJECT_METATYPE(VipSmoothPolyDataFilter*)

// TEST
class VIP_CORE_EXPORT CosXYZ : public VipVTKObjectProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description", "TEST")
	Q_CLASSINFO("category", "3D/Polydata")
	VipVTKObject data;

public:
	CosXYZ()
	  : VipVTKObjectProcessing()
	{
	}
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.value<VipVTKObject>().polyData(); }
	virtual VipVTKObject applyAlgorithm(const VipVTKObjectList& inputs, qint64 time);
};
VIP_REGISTER_QOBJECT_METATYPE(CosXYZ*)

/*Apply an affine transform to X,Y and/or Z coordinate*/
class VIP_CORE_EXPORT VipLinearTransform : public VipVTKObjectProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description",
		    "Apply an affine transform to X,Y and/or Z coordinate\n."
		    "The transformation origin is given by the property 'Transformation_origin'.\n"
		    "0 = axis origin, 1 = object bounding box origin, 2 = object bounding box center, 3 = object barycentre.")
	Q_CLASSINFO("category", "3D/Pointset");

	VIP_IO(VipProperty Transformation_origin)
	VIP_IO(VipProperty X_offset)
	VIP_IO(VipProperty Y_offset)
	VIP_IO(VipProperty Z_offset)
	VIP_IO(VipProperty X_multiplication_factor)
	VIP_IO(VipProperty Y_multiplication_factor)
	VIP_IO(VipProperty Z_multiplication_factor)
	VIP_IO(VipProperty Rotation_angle)
	VIP_IO(VipProperty Axis)

	VIP_PROPERTY_EDIT(Transformation_origin, VIP_ENUM_EDIT("Axis origin,Object bounding box origin,Object bounding box center,Object barycentre", "0,1,2,3", "0"))
	VIP_PROPERTY_EDIT(Axis, VIP_ENUM_EDIT("X,Y,Z", "0,1,2", "0"))

	VIP_PROPERTY_CATEGORY(X_offset, "Translation")
	VIP_PROPERTY_CATEGORY(Y_offset, "Translation")
	VIP_PROPERTY_CATEGORY(Z_offset, "Translation")

	VIP_PROPERTY_CATEGORY(X_multiplication_factor, "Scaling")
	VIP_PROPERTY_CATEGORY(Y_multiplication_factor, "Scaling")
	VIP_PROPERTY_CATEGORY(Z_multiplication_factor, "Scaling")

	VIP_PROPERTY_CATEGORY(Rotation_angle, "Rotation")
	VIP_PROPERTY_CATEGORY(Axis, "Rotation")
public:
	VipLinearTransform();
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.value<VipVTKObject>().pointSet(); }
	virtual VipVTKObject applyAlgorithm(const VipVTKObjectList& inputs, qint64 time);
};

VIP_REGISTER_QOBJECT_METATYPE(VipLinearTransform*)





#endif