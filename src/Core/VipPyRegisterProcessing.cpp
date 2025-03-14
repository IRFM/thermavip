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

#include "VipPyRegisterProcessing.h"
#include "VipPyProcessing.h"
#include "VipPySignalFusionProcessing.h"

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

bool VipPyRegisterProcessing::saveCustomProcessings(const QList<VipProcessingObject::Info>& infos)
{
	// generate xml file contianing the processings
	VipXOStringArchive arch;
	arch.start("processings");
	for (int i = 0; i < infos.size(); ++i) {
		if (VipPySignalFusionProcessingPtr ptr = infos[i].init.value<VipPySignalFusionProcessingPtr>()) {
			arch.content("name", infos[i].classname);
			arch.content("category", infos[i].category);
			arch.content("description", infos[i].description);
			arch.content(ptr.data());
		}
		else if (VipPyProcessingPtr ptr = infos[i].init.value<VipPyProcessingPtr>()) {
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

QList<VipProcessingObject::Info> VipPyRegisterProcessing::customProcessing()
{
	// get additional VipPySignalFusionProcessing
	QList<VipProcessingObject::Info> infos1 = VipProcessingObject::additionalInfoObjects(qMetaTypeId<VipPySignalFusionProcessing*>());

	// get additional VipPyProcessing
	QList<VipProcessingObject::Info> infos2 = VipProcessingObject::additionalInfoObjects(qMetaTypeId<VipPyProcessing*>());
	// remove VipPyProcessing that rely on Python files
	for (int i = 0; i < infos2.size(); ++i) {
		if (!infos2[i].init.value<VipPyProcessingPtr>()) {
			infos2.removeAt(i);
			--i;
		}
	}

	return infos1 + infos2;
}

bool VipPyRegisterProcessing::saveCustomProcessings()
{
	return saveCustomProcessings(customProcessing());
}

int VipPyRegisterProcessing::loadCustomProcessings(bool overwrite)
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

		if (VipPySignalFusionProcessingPtr ptr = VipPySignalFusionProcessingPtr(proc.value<VipPySignalFusionProcessing*>())) {
			info.init = QVariant::fromValue(ptr);
			info.displayHint = VipProcessingObject::DisplayOnSameSupport;
			info.metatype = qMetaTypeId<VipPySignalFusionProcessing*>();
		}
		else if (VipPyProcessingPtr ptr2 = VipPyProcessingPtr(proc.value<VipPyProcessing*>())) {
			info.init = QVariant::fromValue(ptr2);
			info.displayHint = VipProcessingObject::InputTransform;
			info.metatype = qMetaTypeId<VipPyProcessing*>();
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

	// QMultiMap<QString, VipProcessingObject::Info> infos =  _validProcessingObjects<VipPyProcessing*>(QVariantList() << QVariant::fromValue(VipPointVector()), 1,
	// VipProcessingObject::InputTransform);

	return res;
}
