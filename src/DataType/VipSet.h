#ifndef VIP_SET_H
#define VIP_SET_H

#include <qset.h>
#include <qlist.h>

/// @brief Convert QList to QSet
/// @tparam T 
/// @param lst 
/// @return 
template<class T>
QSet<T> vipToSet(const QList<T> & lst)
{
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
	return lst.toSet();
#else
	return QSet<T>(lst.cbegin(), lst.cend());
#endif
}

#endif