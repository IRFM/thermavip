#ifndef VIP_WARPING_H
#define VIP_WARPING_H


#include "VipNDArray.h"
#include "VipMultiNDArray.h"
#include "VipNDArrayImage.h"
#include "VipImageProcessing.h"

VIP_CORE_EXPORT VipPointVector vipWarping(QVector<QPoint> pts1, QVector<QPoint> pts2, int width, int height);


/// @brief Image warping processing based on Delaunay triangulation
class VIP_CORE_EXPORT VipWarping : public VipSceneModelBasedProcessing
{
	Q_OBJECT
	VIP_IO(VipInput input)
	VIP_IO(VipOutput output)
	Q_CLASSINFO("category", "Miscellaneous")
	Q_CLASSINFO("description", "Image warping based on Delaunay triangulation")

	VipPointVector m_warping;

public:
	VipWarping(QObject * parent = nullptr);

	virtual DisplayHint displayHint() const { return InputTransform; }
	virtual bool acceptInput(int /*index*/, const QVariant& v) const { return v.userType() == qMetaTypeId<VipNDArray>(); }

	const VipPointVector& warping() const { return m_warping; }
	void setWarping(const VipPointVector& warp) {
		m_warping = warp;
		emitProcessingChanged();
	}

protected:
	virtual void apply();
};

VIP_REGISTER_QOBJECT_METATYPE(VipWarping*)

class VipArchive;

VIP_CORE_EXPORT VipArchive& operator<<(VipArchive&, const VipWarping* tr);
VIP_CORE_EXPORT VipArchive& operator>>(VipArchive&, VipWarping* tr);

#endif
