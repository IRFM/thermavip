#include "PyRegisterProcessing.h"
#include "PyProcessing.h"
#include "PySignalFusionProcessing.h"

#include "VipXmlArchive.h"

#include <QFile>

template<class ProcessingType>
static QMultiMap<QString, VipProcessingObject::Info> _validProcessingObjects(const QVariantList& lst, int output_count, VipProcessingObject::DisplayHint maxDisplayHint)
{
	QMultiMap<QString, VipProcessingObject::Info> res;
	const QList<const VipProcessingObject*> all = VipProcessingObject::allObjects();

	for (int i = 0; i < all.size(); ++i) {
		const VipProcessingObject* obj = all[i];
		if (!qobject_cast<ProcessingType>((VipProcessingObject*)obj))
			continue;

		if (lst.isEmpty() && output_count < 0) {
			res.insert(obj->category(), obj->info());
			continue;
		}

		// First check the input and output count just based on the meta object, without actually creating an instance of VipProcessingObject
		const QMetaObject* meta = obj->metaObject();
		if (!meta)
			continue;

		int in_count, out_count;
		VipProcessingObject::IOCount(meta, &in_count, nullptr, &out_count);
		if (lst.size() && !in_count)
			continue;
		else if (output_count && !out_count)
			continue;
		if (lst.size() > 1 && obj->displayHint() == VipProcessingObject::InputTransform)
			continue;

		VipProcessingObject::Info info = obj->info();
		if (info.displayHint > maxDisplayHint)
			continue;

		if (lst.size() == 0) {
			if (output_count < 0)
				res.insert(obj->category(), info);
			else if (output_count == 0) {
				if (obj->outputCount() == 0)
					res.insert(obj->category(), info);
			}
			else if (obj->outputCount() == output_count) {
				res.insert(obj->category(), info);
			}
			else {
				VipMultiOutput* out = obj->topLevelOutputAt(0)->toMultiOutput();
				if (out && out->minSize() <= output_count && out->maxSize() >= output_count)
					res.insert(obj->category(), info);
			}
		}
		else {
			if (obj->topLevelInputCount() > 0)
				if (VipMultiInput* multi = obj->topLevelInputAt(0)->toMultiInput()) {
					if (multi->minSize() > lst.size() || lst.size() > multi->maxSize()) {
						// min/max size of VipMultiInput not compatible with the input list
						continue;
					}
					multi->resize(lst.size());
				}

			if (lst.size() == obj->inputCount()) {
				bool accept_all = true;
				for (int j = 0; j < lst.size(); ++j) {
					if (!obj->acceptInput(j, lst[j]) && lst[j].userType() != 0) {
						accept_all = false;
						break;
					}
				}
				if (accept_all) {
					if (output_count < 0)
						res.insert(obj->category(), info);
					else if (output_count == 0) {
						if (obj->outputCount() == 0)
							res.insert(obj->category(), info);
					}
					else if (obj->outputCount() == output_count) {
						res.insert(obj->category(), info);
					}
					else {
						VipMultiOutput* out = obj->topLevelOutputAt(0)->toMultiOutput();
						if (out && out->minSize() <= output_count && out->maxSize() >= output_count)
							res.insert(obj->category(), info);
					}
				}
			}
		}
	}

	return res;
}

bool PyRegisterProcessing::saveCustomProcessings(const QList<VipProcessingObject::Info>& infos)
{
	// generate xml file contianing the processings
	VipXOStringArchive arch;
	arch.start("processings");
	for (int i = 0; i < infos.size(); ++i) {
		if (PySignalFusionProcessingPtr ptr = infos[i].init.value<PySignalFusionProcessingPtr>()) {
			arch.content("name", infos[i].classname);
			arch.content("category", infos[i].category);
			arch.content("description", infos[i].description);
			arch.content(ptr.data());
		}
		else if (PyProcessingPtr ptr = infos[i].init.value<PyProcessingPtr>()) {
			arch.content("name", infos[i].classname);
			arch.content("category", infos[i].category);
			arch.content("description", infos[i].description);
			arch.content(ptr.data());
		}
	}
	arch.end();

	// write registered processing to custom_data_fusion.xml
	QString proc_file = vipGetPythonDirectory() + "custom_python_processing.xml";
	QFile out(proc_file);
	if (out.open(QFile::WriteOnly | QFile::Text)) {
		out.write(arch.toString().toLatin1());
		return true;
	}
	return false;
}

QList<VipProcessingObject::Info> PyRegisterProcessing::customProcessing()
{
	// get additional PySignalFusionProcessing
	QList<VipProcessingObject::Info> infos1 = VipProcessingObject::additionalInfoObjects(qMetaTypeId<PySignalFusionProcessing*>());

	// get additional PyProcessing
	QList<VipProcessingObject::Info> infos2 = VipProcessingObject::additionalInfoObjects(qMetaTypeId<PyProcessing*>());
	// remove PyProcessing that rely on Python files
	for (int i = 0; i < infos2.size(); ++i) {
		if (!infos2[i].init.value<PyProcessingPtr>()) {
			infos2.removeAt(i);
			--i;
		}
	}

	return infos1 + infos2;
}

bool PyRegisterProcessing::saveCustomProcessings()
{
	return saveCustomProcessings(customProcessing());
}

int PyRegisterProcessing::loadCustomProcessings(bool overwrite)
{
	// returns the number of read procesings, or -1 on error

	QFile in(vipGetPythonDirectory() + "custom_python_processing.xml");
	if (!in.open(QFile::ReadOnly | QFile::Text))
		return 0; // no file, no processings

	QString content = in.readAll();
	VipXIStringArchive arch(content);
	arch.start("processings");

	int res = 0;
	while (true) {
		QString name = arch.read("name").toString();
		QString category = arch.read("category").toString();
		QString description = arch.read("description").toString();
		if (!arch)
			break;
		QVariant proc = arch.read();
		if (!arch || proc.userType() == 0)
			break;

		VipProcessingObject::Info info;
		info.classname = name;
		info.category = category.split("/", VIP_SKIP_BEHAVIOR::SkipEmptyParts).join("/");
		info.description = description;

		if (PySignalFusionProcessingPtr ptr = PySignalFusionProcessingPtr(proc.value<PySignalFusionProcessing*>())) {
			info.init = QVariant::fromValue(ptr);
			info.displayHint = VipProcessingObject::DisplayOnSameSupport;
			info.metatype = qMetaTypeId<PySignalFusionProcessing*>();
		}
		else if (PyProcessingPtr ptr2 = PyProcessingPtr(proc.value<PyProcessing*>())) {
			info.init = QVariant::fromValue(ptr2);
			info.displayHint = VipProcessingObject::InputTransform;
			info.metatype = qMetaTypeId<PyProcessing*>();
		}
		else // error while loading processing, return -1
			return -1;

		// check if already registerd
		if (!overwrite) {
			bool found = false;
			QList<VipProcessingObject::Info> infos = VipProcessingObject::additionalInfoObjects();
			for (int i = 0; i < infos.size(); ++i) {
				if (infos[i].classname == name && infos[i].category == info.category) {
					found = true;
					break;
				}
			}
			if (found)
				continue;
		}
		VipProcessingObject::registerAdditionalInfoObject(info);
		++res;
	}

	// QMultiMap<QString, VipProcessingObject::Info> infos =  _validProcessingObjects<PyProcessing*>(QVariantList() << QVariant::fromValue(VipPointVector()), 1,
	// VipProcessingObject::InputTransform);

	return res;
}
