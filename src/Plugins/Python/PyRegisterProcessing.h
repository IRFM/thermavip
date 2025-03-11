#ifndef PY_REGISTER_PROCESSING_H
#define PY_REGISTER_PROCESSING_H

#include "VipProcessingObject.h"
#include <qwidget.h>

class QPushButton;
class QToolButton;
class QTreeWidgetItem;

/// @brief Class managing registered Python processings
/// (PySignalFusionProcessing and PyProcessing) using
/// PySignalFusionProcessing::registerThisProcessing() or
/// PyProcessing::registerThisProcessing().
///
class PyRegisterProcessing
{
public:
	/// @brief Save given processings in the Python custom processing XML file.
	static bool saveCustomProcessings(const QList<VipProcessingObject::Info>& infos);
	/// @brief Save all registered processings in the Python custom processing XML file.
	static bool saveCustomProcessings();
	/// @brief Returns all registered custom processings
	static QList<VipProcessingObject::Info> customProcessing();

	/// @brief Load the custom processing from the XML file and add them to the global VipProcessingObject system.
	/// If \a overwrite is true, this will overwrite already existing processing with the same name and category.
	static int loadCustomProcessings(bool overwrite);
};

#endif
