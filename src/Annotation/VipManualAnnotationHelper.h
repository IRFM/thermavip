#ifndef VIP_MANUAL_ANNOTATION_HELPER_H
#define VIP_MANUAL_ANNOTATION_HELPER_H

#include "VipSqlQuery.h"
#include <qprocess.h>

/**
* Singleton class that creates annotations (bbox or segmented) from a user defined bounding rect.
* Uses the python script thermavip_interface.py
*/
class VIP_ANNOTATION_EXPORT ManualAnnotationHelper
{
	
	QProcess m_process;
	bool m_support_segm;
	ManualAnnotationHelper();
	

public:
	~ManualAnnotationHelper();

	Vip_event_list createFromUserProposal(const QList<QPolygonF>& polygons,
					      Vip_experiment_id pulse,
					      const QString& camera,
					      const QString& device,
					      const QString& userName,
					      qint64 time,
					      const QString& type = "bbox",
					      const QString& filename = QString());

	Vip_event_list createBBoxesFromUserProposal(const QList<QPolygonF>& polygons,
						Vip_experiment_id pulse,
						const QString& camera,
						const QString& device,
						const QString& userName,
						qint64 time,
						const QString& filename = QString());
	Vip_event_list createSegmentationFromUserProposal(const QList<QPolygonF>& polygons,
						      Vip_experiment_id pulse,
						      const QString& camera,
						      const QString& device,
						      const QString& userName,
						      qint64 time,
						      const QString& filename = QString());

	static bool isValidState();
	static bool supportBBox();
	static bool supportSegmentation();
	static ManualAnnotationHelper* instance();
	static void deleteInstance();
};


#endif