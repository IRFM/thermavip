#include "VipVectors.h"

//#define VIP_DEBUG

#ifdef VIP_DEBUG
#include <atomic>
#include <qdatetime.h>
static std::atomic<int> _VipPointVector_count = 0;
static std::atomic<qint64> _VipPointVector_print = 0;
#endif


VipPointVector::VipPointVector() : QVector<VipPoint>()
{
#ifdef VIP_DEBUG
	++_VipPointVector_count;
#endif
}
VipPointVector::VipPointVector(int size) : QVector<VipPoint>(size)
{
#ifdef VIP_DEBUG
	++_VipPointVector_count;
#endif
}
VipPointVector::VipPointVector(const QVector<VipPoint>& other) : QVector<VipPoint>(other)
{
#ifdef VIP_DEBUG
	++_VipPointVector_count;
#endif
}
VipPointVector::VipPointVector(const QVector<QPointF>& other) : QVector<VipPoint>(other.size()) {
	std::copy(other.begin(), other.end(), begin());
#ifdef VIP_DEBUG
	++_VipPointVector_count;
#endif
}
VipPointVector::VipPointVector(const VipPointVector& other) : QVector<VipPoint>(other)
{
#ifdef VIP_DEBUG
	++_VipPointVector_count;
#endif
}
VipPointVector::~VipPointVector()
{
	clear();
#ifdef VIP_DEBUG
	--_VipPointVector_count;
	if (QDateTime::currentMSecsSinceEpoch() - (qint64)_VipPointVector_print > 1000) {
		_VipPointVector_print = QDateTime::currentMSecsSinceEpoch();
		printf("VipPointVector: %d\n", (int)_VipPointVector_count);
	}
#endif
}
