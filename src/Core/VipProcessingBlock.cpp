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


#include <qevent.h>
#include "VipProcessingBlock.h"
#include "VipIODevice.h"
#include "VipLock.h"

class VipProcessingBlock::PrivateData
{
public:
	QVector<VipProcessingObject*> children;
	QVector<VipProcessingObject*> leaves;
	QVector<QVector<VipInput*>> inputs;
	QVector<QVector<VipProperty*>> properties;
	QVector<VipOutput*> outputs;
	VipSpinlock lock;


	VipProcessingObject::DisplayHint hint = VipProcessingObject::UnknownDisplayHint;
	bool useEventLoop = false;
	bool dirtyChildren = true;
	QTransform imageTransform;
};


VipProcessingBlock::VipProcessingBlock(QObject * parent)
  : VipProcessingObject(parent)
{
	VIP_CREATE_PRIVATE_DATA();
}
VipProcessingBlock::~VipProcessingBlock()
{
}

bool VipProcessingBlock::setInputConnection(int index, VipInput* input)
{
	if (index >= inputCount()) {
		setError("Invalid input index");
		return false;
	}
	if (d_data->children.indexOf(input->parentProcessing()) < 0) {
		setError("Input does not belong to this block");
		return false;
	}
	if (inputCount() != d_data->inputs.size())
		d_data->inputs.resize(inputCount());

	d_data->inputs[index].push_back(input);
	return true;
}
QVector<VipInput*> VipProcessingBlock::inputConnection(int index) const
{
	if (index < d_data->inputs.size())
		return d_data->inputs[index];
	return QVector<VipInput*>();
}

bool VipProcessingBlock::setOutputConnection(int index, VipOutput* output)
{
	if (index >= outputCount()) {
		setError("Invalid output index");
		return false;
	}
	if (d_data->children.indexOf(output->parentProcessing()) < 0) {
		setError("Output does not belong to this block");
		return false;
	}
	if (outputCount() != d_data->outputs.size()){ 
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		auto old = d_data->outputs.size();
		d_data->outputs.resize(outputCount());
		for(; old < d_data->outputs.size(); ++old )
			d_data->outputs[old] = nullptr; 
#else
		d_data->outputs.resize(outputCount(),(VipOutput*)nullptr);
#endif	
	}


	d_data->outputs[index] = output;
	computeLeaves();
	return true;
}
VipOutput* VipProcessingBlock::outputConnection(int index) const
{
	if (index < d_data->outputs.size())
		return d_data->outputs[index];
	return nullptr;
}

bool VipProcessingBlock::setPropertyConnection(int index, VipProperty* prop)
{
	if (index >= propertyCount()) {
		setError("Invalid property index");
		return false;
	}
	if (d_data->children.indexOf(prop->parentProcessing()) < 0) {
		setError("Property does not belong to this block");
		return false;
	}
	if (propertyCount() != d_data->properties.size())
		d_data->properties.resize(propertyCount());

	d_data->properties[index].push_back( prop);
	return true;
}
QVector<VipProperty*> VipProcessingBlock::propertyConnection(int index) const
{
	if (index < d_data->properties.size())
		return d_data->properties[index];
	return QVector<VipProperty*>();
}


VipProcessingObject::DisplayHint VipProcessingBlock::displayHint() const
{
	return d_data->hint;
}
void VipProcessingBlock::setDisplayHint(VipProcessingObject::DisplayHint hint)
{
	d_data->hint = hint;
}

bool VipProcessingBlock::useEventLoop() const
{
	return d_data->useEventLoop;
}
bool VipProcessingBlock::acceptInput(int index, const QVariant& v) const
{
	if (index >= d_data->inputs.size())
		return false;

	const QVector<VipInput*>& inputs = d_data->inputs[index];
	for (const VipInput* in : inputs) {
		if (VipProcessingObject* obj = in->parentProcessing()) {
			int idx = obj->indexOf(in);
			if (!obj->acceptInput(idx, v))
				return false;
		}
	}
	
	return true;
}
bool VipProcessingBlock::acceptProperty(int index, const QVariant& v) const
{
	if (index >= d_data->properties.size())
		return false;

	const QVector<VipProperty*>& properties = d_data->properties[index];
	for (const VipProperty* in : properties) {
		if (VipProcessingObject* obj = in->parentProcessing()) {
			int idx = obj->indexOf(in);
			if (!obj->acceptProperty(idx, v))
				return false;
		}
	}
	return true;
}
void VipProcessingBlock::setSourceProperty(const char* name, const QVariant& value)
{
	VipProcessingObject::setSourceProperty(name, value);
	for (int i = 0; i < d_data->children.size(); ++i)
		d_data->children[i]->setProperty(name, value);
}


QTransform VipProcessingBlock::computeTransform() const
{
	QTransform tr;

	QSet<VipProcessingObject*> layer;
	for (qsizetype i = 0; i < d_data->inputs.size(); ++i) {
		for (const VipInput* in : d_data->inputs[i])
			layer.insert(in->parentProcessing());
	}

	while (layer.size()) {

		// walk through this layer, take the first non identity transform
		bool multiply = false;
		for (VipProcessingObject* o : layer) {
			QTransform t = o->imageTransform();
			if (!multiply  && !t.isIdentity()) {
				tr *= t;
				multiply = true;
			}
			const auto sinks = o->directSinks();
			layer.clear();
			for (const auto* o : sinks)
				layer.insert((VipProcessingObject*)o);
		}
	}

	return tr;
}

void VipProcessingBlock::apply()
{
	// Set properties
	for (int i = 0; i < propertyCount(); ++i) {
		if (i < d_data->properties.size()) {
			for (VipProperty* in : d_data->properties[i])
				in->setData(propertyAt(i)->data());
		}
	}
	
	// Set input data
	for (int i = 0; i < inputCount(); ++i) {
		if (i < d_data->inputs.size()) {
			for (VipInput* in : d_data->inputs[i])
				in->setData(inputAt(i)->data());
		}
	}

	// Update leaves
	for (VipProcessingObject* o : d_data->leaves)
		o->update();

	if (hasError())
		return;

	// Set output
	for (int i = 0; i < outputCount(); ++i) {
		if (i < d_data->outputs.size()) {
			if (VipOutput* out = d_data->outputs[i]) {
				VipAnyData any = out->data();
				any.mergeAttributes(this->attributes());
				any.setSource(this);
				outputAt(i)->setData(any);
			}
		}
	}
}


void VipProcessingBlock::resetProcessing()
{
	for (VipProcessingObject* o : d_data->children) {
		o->reset();
	}
}
QTransform VipProcessingBlock::imageTransform(bool* from_center) const
{
	*from_center = false;
	return d_data->imageTransform;
}

static VipProcessingObject* findChildWithName(const QVector<VipProcessingObject*>& children, VipProcessingObject* child, const QString& name)
{
	for (VipProcessingObject* o : children) {
		if (o != child && o->objectName() == name)
			return o;
	}
	return nullptr;
}

template<class IO>
static void cleanupIO(QVector<QVector<IO*>>& io_vector, const QVector<VipProcessingObject*>& children)
{
	for (qsizetype i = 0; i < io_vector.size(); ++i) {
		QVector<IO*>& ios = io_vector[i];
		for (qsizetype j = 0; j < ios.size(); ++j) {
			// Search for input in children
			bool found = false;
			for (VipProcessingObject* o : children)
				if (o->indexOf(ios[j]) >= 0) {
					found = true;
					break;
				}
			if (!found) {
				ios.removeAt(j);
				--j;
			}
				
		}
	}
}
static void cleanupOutputs(QVector<VipOutput*>& io_vector, const QVector<VipProcessingObject*>& children)
{
	for (qsizetype i = 0; i < io_vector.size(); ++i) {
		if (auto* io = io_vector[i]) {
			// Search for input in children
			bool found = false;
			for (VipProcessingObject* o : children)
				if (o->indexOf(io) >= 0) {
					found = true;
					break;
				}
			if (!found)
				io_vector[i] = nullptr;
		}
	}
}

template<class T>
static QList<T> findDirectChildren(const QObject* obj)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	return obj->findChildren<T>(QAnyStringView(), Qt::FindDirectChildrenOnly);
#else
	return obj->findChildren<T>(QString(), Qt::FindDirectChildrenOnly);
#endif
}
 

void VipProcessingBlock::childEvent(QChildEvent* evt)
{
	auto lst = findDirectChildren<VipProcessingObject*>(this); //this->findChildren<VipProcessingObject*>(Qt::FindDirectChildrenOnly);

	if (evt->added()) {
		if (auto* o = qobject_cast<VipProcessingObject*>(evt->child())) {
			// make sure to get child errors
			connect(o, &VipProcessingObject::error, this, &VipProcessingBlock::receiveNewError);
			// remove the Asynchronous flag
			o->setScheduleStrategy(VipProcessingObject::Asynchronous, false);

			d_data->children.resize(lst.size());
			for (qsizetype i = 0; i < lst.size(); ++i)
				d_data->children[i] = lst[i];

			if (o->objectName().isEmpty())
				o->setObjectName(QString(o->info().classname));
		}
	}
	else {
		if (auto* o = qobject_cast<VipProcessingObject*>(evt->child())) {
			disconnect(o, &VipProcessingObject::error, this, &VipProcessingBlock::receiveNewError);

			for (VipProcessingObject* c : lst)
				if (c != 0)
					d_data->children.push_back(c);
		}
	}

	// Cleanup invalid inputs
	if (evt->removed()) {
		cleanupIO(d_data->inputs, d_data->children);
		cleanupIO(d_data->properties, d_data->children);
		cleanupOutputs(d_data->outputs, d_data->children);
	}

	// Compute useEventLoop
	d_data->useEventLoop = false;
	for (VipProcessingObject* c : d_data->children) {
		if ((d_data->useEventLoop = c->useEventLoop()))
			break;
	}

	// Compute image transform
	d_data->imageTransform = computeTransform();

	// Compute leaves
	computeLeaves();

	// Ensure unique names
	for (VipProcessingObject* o : d_data->children) {
		if (o->objectName().isEmpty())
			o->setObjectName(QString(o->info().classname));

		VipProcessingObject* found = findChildWithName(d_data->children, o, o->objectName());
		int count = 1;
		while (found) {
			QString name = QString(o->info().classname) + "_" + QString::number(count);
			o->setObjectName(name);
			found = findChildWithName(d_data->children, o, name);
			++count;
		}
	}
}

void VipProcessingBlock::computeLeaves()
{
	// compute leaves
	QSet<VipProcessingObject*> leaves;

	for (VipOutput* out : d_data->outputs) {
		if (out)
			leaves.insert(out->parentProcessing());
	}

	d_data->leaves.clear();
	for (VipProcessingObject* o : leaves)
		if (o)
			d_data->leaves.push_back(o);
}

void VipProcessingBlock::receiveNewError(QObject * , const VipErrorData& error)
{
	setError(error);
}



 

VipArchive& operator<<(VipArchive& stream, const VipProcessingBlock* r)
{
	stream.start("processings");
	QList<VipProcessingObject*> lst = findDirectChildren<VipProcessingObject*>(r);
	for (int i = 0; i < lst.size(); ++i) {
		if (!lst[i]->property("_vip_no_serialize").toBool())
			stream.content(lst[i]);
	}
	stream.end();

	stream.start("inputs");
	for (int i = 0; i < r->inputCount(); ++i) {
		for (VipInput* in : r->inputConnection(i)) {
			stream.content("IOIndex", i);
			stream.content("processingName", in->parentProcessing()->objectName());
			stream.content("processingIOName", in->name());
		}
	}
	stream.end();

	stream.start("properties");
	for (int i = 0; i < r->propertyCount(); ++i) {
		for (VipProperty* in : r->propertyConnection(i)) {
			stream.content("IOIndex", i);
			stream.content("processingName", in->parentProcessing()->objectName());
			stream.content("processingIOName", in->name());
		}
	}
	stream.end();

	stream.start("outputs");
	for (int i = 0; i < r->outputCount(); ++i) {
		if (VipOutput* out = r->outputConnection(i)) {
			stream.content("IOIndex", i);
			stream.content("processingName", out->parentProcessing()->objectName());
			stream.content("processingIOName", out->name());
		}
	}
	stream.end();

	return stream;
}


VipArchive& operator>>(VipArchive& stream, VipProcessingBlock* r)
{
	stream.start("processings");
	// load all VipProcessingObject
	while (!stream.hasError()) {
		if (VipProcessingObject* obj = stream.read().value<VipProcessingObject*>()) {
			obj->setParent(r);

			// open the read only devices
			if (VipIODevice* device = qobject_cast<VipIODevice*>(obj)) {
				if (device->supportedModes() & VipIODevice::ReadOnly)
					device->open(VipIODevice::ReadOnly);
			}
		}
	}
	stream.resetError();
	stream.end();

	stream.start("inputs");
	while (!stream.hasError()) {
		int IOIndex = 0;
		QString procName;
		QString IOName;
		stream.content("IOIndex", IOIndex);
		stream.content("processingName", procName);
		stream.content("processingIOName", IOName);
		if (!stream.hasError()) {
			if (VipProcessingObject* o = r->findChild<VipProcessingObject*>(procName, Qt::FindDirectChildrenOnly)) {
				if (auto* io = o->inputName(IOName))
					r->setInputConnection(IOIndex, io);
			}
		}
	}
	stream.resetError();
	stream.end();

	stream.start("properties");
	while (!stream.hasError()) {
		int IOIndex = 0;
		QString procName;
		QString IOName;
		stream.content("IOIndex", IOIndex);
		stream.content("processingName", procName);
		stream.content("processingIOName", IOName);
		if (!stream.hasError()) {
			if (VipProcessingObject* o = r->findChild<VipProcessingObject*>(procName, Qt::FindDirectChildrenOnly)) {
				if (auto* io = o->propertyName(IOName))
					r->setPropertyConnection(IOIndex, io);
			}
		}
	}
	stream.resetError();
	stream.end();

	stream.start("outputs");
	while (!stream.hasError()) {
		int IOIndex = 0;
		QString procName;
		QString IOName;
		stream.content("IOIndex", IOIndex);
		stream.content("processingName", procName);
		stream.content("processingIOName", IOName);
		if (!stream.hasError()) {
			if (VipProcessingObject* o = r->findChild<VipProcessingObject*>(procName, Qt::FindDirectChildrenOnly)) {
				if (auto* io = o->outputName(IOName))
					r->setOutputConnection(IOIndex, io);
			}
		}
	}
	stream.resetError();
	stream.end();

	// open all connections
	QList<VipProcessingObject*> children = findDirectChildren<VipProcessingObject*>(r);

	for (int i = 0; i < children.size(); ++i) {
		children[i]->openAllConnections();
	}

	r->reload();
	return stream;
}

static int registerStreamOperators()
{
	vipRegisterArchiveStreamOperators<VipProcessingBlock*>();
	return 0;
}

static int _registerStreamOperators = vipAddInitializationFunction(registerStreamOperators);