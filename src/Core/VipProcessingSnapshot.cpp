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

#include "VipProcessingSnapshot.h"
#include "VipXmlArchive.h"

#include <qtcpserver.h>
#include <qtimer.h>

bool vipSaveProcessingPoolSnapshot(VipProcessingPool* pool, VipArchive& arch)
{
	QList<VipProcessingObject*> objects = pool->findChildren<VipProcessingObject*>();

	arch.start("ProcessingPoolSnapshot");

	// save the processing pool attributes
	arch.content("attributes", pool->attributes());

	arch.content("count", objects.size());

	for (int i = 0; i < objects.size(); ++i) {
		VipProcessingObject* p = objects[i];
		VipProcessingObject::Info info = p->info();

		// save most parameters of all VipProcessingObject in the pool

		arch.start("ProcessingSnapshot");

		// save the general information
		arch.content("objectname", p->objectName());
		arch.content("classname", info.classname);
		arch.content("metatype", info.metatype);
		arch.content("description", info.description);
		arch.content("category", info.category);
		arch.content("displayHint", (int)info.displayHint);
		arch.content("icon", info.icon);

		// save the processing time
		arch.content("ptime", p->processingTime());
		arch.content("ltime", p->lastProcessingTime());

		// save the last errors
		const QList<VipErrorData> errors = p->lastErrors();
		arch.start("errors");
		int count = errors.size();
		arch.content("count", count);
		for (int e = 0; e < count; ++e) {
			const VipErrorData tmp = errors[e];
			arch.content("string", tmp.errorString());
			arch.content("code", tmp.errorCode());
			arch.content("time", tmp.msecsSinceEpoch());
		}
		arch.end();

		// save the inputs
		arch.start("inputs");
		count = p->inputCount();
		arch.content("count", count);
		for (int j = 0; j < count; ++j) {
			VipInput* input = p->inputAt(j);
			arch.content("name", input->name());
			arch.content("connection", input->connection()->address());
			arch.content("descr", p->inputDescription(input->name()));
		}
		arch.end();

		// save the properties
		arch.start("properties");
		count = p->propertyCount();
		arch.content("count", count);
		for (int j = 0; j < count; ++j) {
			VipProperty* prop = p->propertyAt(j);
			arch.content("name", prop->name());
			arch.content("connection", prop->connection()->address());
			arch.content("descr", p->propertyDescription(prop->name()));
		}
		arch.end();

		// save the outputs
		arch.start("outputs");
		count = p->outputCount();
		arch.content("count", count);
		for (int j = 0; j < count; ++j) {
			VipOutput* out = p->outputAt(j);
			arch.content("name", out->name());
			arch.content("connection", out->connection()->address());
			arch.content("descr", p->propertyDescription(out->name()));
		}
		arch.end();

		arch.end();
	}

	arch.end();

	return true;
}

bool vipLoadProcessingPoolSnapshot(VipProcessingPool* pool, VipArchive& arch)
{
	if (!arch.start("ProcessingPoolSnapshot"))
		return false;

	// read back the processing pool attributes
	pool->setAttributes(arch.read("attributes").value<QVariantMap>());

	// retrieve all current processings in the pool into a map
	QMap<QString, VipProcessingSnapshot*> procs;
	QList<VipProcessingSnapshot*> objects = pool->findChildren<VipProcessingSnapshot*>();
	for (int i = 0; i < objects.size(); ++i)
		procs[objects[i]->objectName()] = objects[i];
	objects.clear();

	// start reading the pool
	int p_count = arch.read("count").toInt();

	bool has_new_connections = false;

	for (int i = 0; i < p_count; ++i) {
		if (!arch.start("ProcessingSnapshot"))
			return false;

		// load the processing name
		QString name = arch.read("objectname").toString();
		QMap<QString, VipProcessingSnapshot*>::iterator it = procs.find(name);
		VipProcessingSnapshot* p = nullptr;
		if (it == procs.end()) {
			p = new VipProcessingSnapshot();
			p->setObjectName(name);
			p->setParent(pool);

			// load general information
			p->m_info.classname = arch.read("classname").toString();
			p->m_info.metatype = arch.read("metatype").toInt();
			p->m_info.description = arch.read("description").toString();
			p->m_info.category = arch.read("category").toString();
			p->m_info.displayHint = (VipProcessingObject::DisplayHint)arch.read("displayHint").toInt();
			p->m_info.icon = arch.read("icon").value<QIcon>();
		}
		else {
			p = it.value();
			procs.erase(it);
		}

		objects.append(p);

		// load the processing time
		p->m_processingTime = arch.read("ptime").toLongLong();
		p->m_lastProcessingTime = arch.read("ltime").toLongLong();

		// load the last errors
		if (!arch.start("errors"))
			return false;
		int count = arch.read("count").toInt();
		for (int j = 0; j < count; ++j) {
			QString str = arch.read("string").toString();
			int code = arch.read("code").toInt();
			qint64 time = arch.read("time").toLongLong();
			VipErrorData err(str, code, time);
			p->setError(err);
		}
		arch.end();

		// load the inputs
		if (!arch.start("inputs"))
			return false;
		count = arch.read("count").toInt();
		if (p->topLevelInputAt(0)->toMultiInput()->count() != count)
			p->topLevelInputAt(0)->toMultiInput()->resize(count);
		for (int j = 0; j < count; ++j) {
			QString _name = arch.read("name").toString();
			QString connection = arch.read("connection").toString();
			QString descr = arch.read("descr").toString();
			VipInput* input = p->inputAt(j);
			input->setName(_name);
			p->m_inputDescriptions[_name] = descr;
			if (input->connection()->address() != connection) {
				has_new_connections = true;
				input->connection()->setupConnection(connection);
			}
		}
		arch.end();

		// load the properties
		if (!arch.start("properties"))
			return false;
		count = arch.read("count").toInt();
		if (p->topLevelPropertyAt(0)->toMultiProperty()->count() != count)
			p->topLevelPropertyAt(0)->toMultiProperty()->resize(count);
		for (int j = 0; j < count; ++j) {
			QString _name = arch.read("name").toString();
			QString connection = arch.read("connection").toString();
			QString descr = arch.read("descr").toString();
			VipProperty* prop = p->propertyAt(j);
			prop->setName(_name);
			p->m_propertyDescriptions[_name] = descr;
			if (prop->connection()->address() != connection) {
				has_new_connections = true;
				prop->connection()->setupConnection(connection);
			}
		}
		arch.end();

		// load the outputs
		if (!arch.start("outputs"))
			return false;
		count = arch.read("count").toInt();
		if (p->topLevelOutputAt(0)->toMultiOutput()->count() != count)
			p->topLevelOutputAt(0)->toMultiOutput()->resize(count);
		for (int j = 0; j < count; ++j) {
			QString _name = arch.read("name").toString();
			QString connection = arch.read("connection").toString();
			QString descr = arch.read("descr").toString();
			VipOutput* out = p->outputAt(j);
			out->setName(_name);
			p->m_outputDescriptions[_name] = descr;
			if (out->connection()->address() != connection) {
				has_new_connections = true;
				out->connection()->setupConnection(connection);
			}
		}
		arch.end();

		arch.end();
	}

	arch.end();

	// remove all unused VipProcessingSnapshot
	for (QMap<QString, VipProcessingSnapshot*>::iterator it = procs.begin(); it != procs.end(); ++it)
		it.value()->deleteLater();

	// open connections
	if (has_new_connections) {
		for (int i = 0; i < objects.size(); ++i)
			objects[i]->openAllConnections();
	}

	return true;
}

QByteArray vipSaveBinarySnapshot(VipProcessingPool* pool)
{
	QByteArray res;
	VipBinaryArchive arch(&res, QIODevice::WriteOnly);
	if (vipSaveProcessingPoolSnapshot(pool, arch))
		return res;
	return QByteArray();
}

QString vipSaveXMLSnapshot(VipProcessingPool* pool)
{
	VipXOStringArchive arch;
	if (vipSaveProcessingPoolSnapshot(pool, arch))
		return arch.toString();
	return QString();
}

bool vipLoadBinarySnapshot(VipProcessingPool* pool, const QByteArray& ar)
{
	VipBinaryArchive arch(ar);
	return vipLoadProcessingPoolSnapshot(pool, arch);
}

bool vipLoadXMLSnapshot(VipProcessingPool* pool, const QString& text)
{
	VipXIStringArchive arch(text);
	return vipLoadProcessingPoolSnapshot(pool, arch);
}
