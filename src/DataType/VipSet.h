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

#ifndef VIP_SET_H
#define VIP_SET_H

#include <qlist.h>
#include <qset.h>

/// @brief Convert QList to QSet
/// @tparam T
/// @param lst
/// @return
template<class T>
QSet<T> vipToSet(const QList<T>& lst)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	return lst.toSet();
#else
	return QSet<T>(lst.cbegin(), lst.cend());
#endif
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
template<class T>
QSet<T> vipToSet(const QVector<T>& lst)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	return lst.toSet();
#else
	return QSet<T>(lst.cbegin(), lst.cend());
#endif
}
#endif

template<class Container>
Container& vipUnique(Container & c)
{
	using value_type = typename Container::value_type;
	QSet<value_type> tmp(c.begin(), c.end());
	return c = Container(tmp.begin(), tmp.end());
}

#endif