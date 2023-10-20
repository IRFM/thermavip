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

#include "VipFunctional.h"
#include <qmutex.h>

namespace details
{
	static QMutex& registerMutex()
	{
		static QMutex inst(QMutex::Recursive);
		return inst;
	}
	static QMap<int, create_fun>& variantCreator()
	{
		static QMap<int, create_fun> inst;
		return inst;
	}
	void registerCreateVariant(int metatype, create_fun fun)
	{
		QMutexLocker lock(&registerMutex());
		variantCreator()[metatype] = fun;
	}
}

QVariant vipCreateVariant(int id)
{
	QMutexLocker lock(&details::registerMutex());
	QMap<int, details::create_fun>::const_iterator it = details::variantCreator().find(id);
	if (it != details::variantCreator().end())
		return it.value()();
	return QVariant(id, nullptr);
}

void vipReleaseVariant(const QVariant& v)
{
	// delete contained pointer if necessary
	if (QObject* obj = v.value<QObject*>())
		delete obj;
}

QList<int> vipUserTypes(int type)
{
	QList<int> types;
	int id = QMetaType::User;
	// int type = qMetaTypeId<T>();
	if (!type)
		return types;

	while (id) {
		// const char *name = QMetaType::typeName(id);
		if (QMetaType::isRegistered(id)) {
			if (vipIsConvertible(VipType(id), VipType(type)))
				types << id;
			++id;
		}
		else {
			id = 0;
		}
	}

	return types;
}
