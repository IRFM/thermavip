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

#ifndef VIP_IMAGE_PROCESSING_H
#define VIP_IMAGE_PROCESSING_H

#include "VipProcessingHelper.h"
#include "VipProcessingObject.h"
#include "VipTransform.h"

/// @brief Base processing class for image processing that take 1 image as input and output only one image (most filtering ones)
class VIP_CORE_EXPORT VipStdImageProcessing : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
public:
	VipStdImageProcessing(QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	{
		this->outputAt(0)->setData(VipNDArray());
	}

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipNDArray>(); }

	virtual VipNDArray applyProcessing(const VipNDArray& ar) = 0;

protected:
	virtual void apply()
	{
		try {
			VipAnyData any = inputAt(0)->data();
			VipNDArray ar = any.value<VipNDArray>();

			if (ar.isEmpty()) {
				setError("Empty input array", VipProcessingObject::WrongInput);
				return;
			}
			ar = applyProcessing(ar);
			if (!hasError()) {
				VipAnyData odata = create(QVariant::fromValue(ar));
				odata.setTime(any.time());
				outputAt(0)->setData(odata);
			}
		}
		catch (const std::exception& e) {
			setError(e.what());
		}
	}
};

/// @brief Base processing class for image or 2d signal processing that take 1 input and output only one data (most filtering ones)
class VIP_CORE_EXPORT VipStdImageAndPlotProcessing : public VipStdImageProcessing
{
	Q_OBJECT
public:
	VipStdImageAndPlotProcessing(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
	}
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipNDArray>() || v.userType() == qMetaTypeId<VipPointVector>(); }

	VipNDArray extractY(const VipPointVector& v);
	VipPointVector resetY(const VipNDArray& y_values, const VipPointVector& initial_vector);

protected:
	virtual void apply()
	{
		try {
			VipAnyData any = inputAt(0)->data();
			QVariant out;

			if (any.data().userType() == qMetaTypeId<VipNDArray>()) {
				VipNDArray ar = any.value<VipNDArray>();
				if (ar.isEmpty()) {
					setError("Empty input array", VipProcessingObject::WrongInput);
					return;
				}
				ar = applyProcessing(ar);
				if (!hasError())
					out = QVariant::fromValue(ar);
			}
			else if (any.data().userType() == qMetaTypeId<VipPointVector>()) {
				VipPointVector ar = any.value<VipPointVector>();
				if (ar.isEmpty()) {
					setError("Empty input array", VipProcessingObject::WrongInput);
					return;
				}
				ar = resetY(applyProcessing(extractY(ar)), ar);
				if (!hasError())
					out = QVariant::fromValue(ar);
			}
			else {
				setError("wrong input type", VipProcessingObject::WrongInput);
				return;
			}

			if (!hasError()) {
				VipAnyData odata = create(out);
				odata.setTime(any.time());
				outputAt(0)->setData(odata);
			}
		}
		catch (const std::exception& e) {
			setError(e.what());
		}
	}
};

class VIP_CORE_EXPORT VipRotate90Right : public VipStdImageProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description", "Clockwise image rotation of 90 degrees")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/rotate90.png")

public:
	VipRotate90Right(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
	}

	virtual VipNDArray applyProcessing(const VipNDArray& ar);
	virtual QTransform imageTransform(bool* from_center) const
	{
		*from_center = true;
		QTransform tr;
		tr.rotate(90);
		return tr;
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipRotate90Right*)

class VIP_CORE_EXPORT VipRotate90Left : public VipStdImageProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description", "Anticlockwise image rotation of 90 degrees")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/rotate-90.png")

public:
	VipRotate90Left(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
	}
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
	virtual QTransform imageTransform(bool* from_center) const
	{
		*from_center = true;
		QTransform tr;
		tr.rotate(-90);
		return tr;
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipRotate90Left*)

class VIP_CORE_EXPORT VipRotate180 : public VipStdImageProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description", "Anticlockwise image rotation of 90 degrees")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/rotate180.png")

public:
	VipRotate180(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
	}
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
	virtual QTransform imageTransform(bool* from_center) const
	{
		*from_center = true;
		QTransform tr;
		tr.rotate(180);
		return tr;
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipRotate180*)

class VIP_CORE_EXPORT VipMirrorH : public VipStdImageProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description", "Horizontal mirror")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/mirror_h.png")

public:
	VipMirrorH(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
	}
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
	virtual QTransform imageTransform(bool* from_center) const
	{
		*from_center = true;
		QTransform tr;
		tr.scale(-1, 1);
		return tr;
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipMirrorH*)

class VIP_CORE_EXPORT VipMirrorV : public VipStdImageProcessing
{
	Q_OBJECT
	Q_CLASSINFO("description", "Vertical mirror")
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("icon", "Icons/mirror_h.png")

public:
	VipMirrorV(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
	}
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
	virtual QTransform imageTransform(bool* from_center) const
	{
		*from_center = true;
		QTransform tr;
		tr.scale(1, -1);
		return tr;
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipMirrorV*)

class VIP_CORE_EXPORT VipThresholding : public VipStdImageAndPlotProcessing
{
	Q_OBJECT
	VIP_IO(VipProperty threshold)
	VIP_DESCRIPTION("Basic thresholding")
	VIP_CATEGORY("Segmentation & Labelling")
	VIP_PROPERTY_EDIT(threshold, VIP_DOUBLE_EDIT(0, "%g"))

public:
	VipThresholding(QObject* parent = nullptr)
	  : VipStdImageAndPlotProcessing(parent)
	{
		propertyAt(0)->setData(0);
	}
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
};

VIP_REGISTER_QOBJECT_METATYPE(VipThresholding*)

class VIP_CORE_EXPORT VipImageCrop : public VipSceneModelBasedProcessing
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	VIP_IO(VipProperty Top_left)
	VIP_IO(VipProperty Bottom_right)

	VIP_DESCRIPTION("VipImageCrop ND arrays")
	VIP_CATEGORY("Miscellaneous")
	VIP_IO_DESCRIPTION(Top_left, "Top left corner of the crop with a comma separator\nExample: '10' or '10, 20' or '10, 20, 15',...")
	VIP_IO_DESCRIPTION(Bottom_right, "Bottom right corner (excluded) of the crop with a comma separator\nExample: '10' or '10, 20' or '10; 20, 15',...")

public:
	VipImageCrop(QObject* parent = nullptr)
	  : VipSceneModelBasedProcessing(parent)
	{
		this->outputAt(0)->setData(VipNDArray());
		this->propertyName("Top_left")->setData(QString());
		this->propertyName("Bottom_right")->setData(QString());
	}

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipNDArray>(); }

	void setStartPosition(const VipNDArrayShape& sh);
	void setEndPosition(const VipNDArrayShape& sh);

	virtual QTransform imageTransform(bool* from_center) const;

protected:
	void cropping(const VipNDArray& src, VipNDArrayShape& start, VipNDArrayShape& end) const;
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipImageCrop*)

class VIP_CORE_EXPORT VipResize : public VipStdImageProcessing
{
	Q_OBJECT
	VIP_IO(VipProperty New_size)
	VIP_IO(VipProperty Interpolation)

	VIP_DESCRIPTION("VipResize ND arrays")
	VIP_CATEGORY("Miscellaneous")
	VIP_IO_DESCRIPTION(New_size, "New ND array size with a comma separator\nExample: '10' or '10, 20' or '10, 20, 15',...")
	VIP_IO_DESCRIPTION(Interpolation, "Interpolation (0 = None, 1 = Linear, 2 = Cubic)")

public:
	VipResize(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
		propertyAt(0)->setData(QString());
		propertyAt(1)->setData(0);
	}
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
};

VIP_REGISTER_QOBJECT_METATYPE(VipResize*)

struct Transform
{
	enum TrType
	{
		Translate,
		Rotate,
		Scale,
		Shear
	};
	Transform(TrType t = Translate, double _x = 0, double _y = 0)
	  : type(t)
	  , x(_x)
	  , y(_y)
	{
	}
	TrType type;
	double x, y;
};
typedef QList<Transform> TransformList;
Q_DECLARE_METATYPE(Transform);
Q_DECLARE_METATYPE(TransformList);
Q_DECLARE_METATYPE(QTransform);

class VIP_CORE_EXPORT VipGenericImageTransform : public VipStdImageProcessing
{
	Q_OBJECT

	VIP_IO(VipProperty transform)
	VIP_IO(VipProperty interpolation)
	VIP_IO(VipProperty size)
	VIP_IO(VipProperty background)
	Q_CLASSINFO("description", "Apply a generic image transformation combining scaling, translation, rotation and shear")
	Q_CLASSINFO("category", "Miscellaneous")

public:
	VipGenericImageTransform(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
		this->propertyAt(0)->setData(QTransform());
		this->propertyAt(1)->setData((int)Vip::LinearInterpolation);
		this->propertyAt(2)->setData((int)Vip::SrcSize);
		this->propertyAt(3)->setData(0.);
	}

protected:
	virtual VipNDArray applyProcessing(const VipNDArray& ar);
};

VIP_REGISTER_QOBJECT_METATYPE(VipGenericImageTransform*)

class VIP_CORE_EXPORT VipComponentLabelling : public VipStdImageProcessing
{
	Q_OBJECT
	VIP_IO(VipProperty connectivity_8)
	Q_CLASSINFO("description", "Connected-component labelling algorithm.\nThe image background value should be set to 0.")
	Q_CLASSINFO("category", "Segmentation & Labelling")

public:
	VipComponentLabelling(QObject* parent = nullptr)
	  : VipStdImageProcessing(parent)
	{
		this->propertyAt(0)->setData(true);
	}

	void setConnectivity8(bool);
	bool connectivity8() const;

protected:
	virtual VipNDArray applyProcessing(const VipNDArray& ar);

private:
	QVector<qsizetype> m_buffer;
};

#endif