#include "VipRgb.h"
#include "VipInternalConvert.h"
#include <qvariant.h>


static int registerConverters()
{
	qRegisterMetaType<VipRGB>();
	qRegisterMetaTypeStreamOperators<VipRGB>();
	QMetaType::registerConverter<VipRGB, QColor>();
	QMetaType::registerConverter<QColor, VipRGB>();
	QMetaType::registerConverter<VipRGB, QString>(detail::typeToString<VipRGB>);
	QMetaType::registerConverter<VipRGB, QByteArray>(detail::typeToByteArray<VipRGB>);
	QMetaType::registerConverter<QString, VipRGB>(detail::stringToType<VipRGB>);
	QMetaType::registerConverter<QByteArray, VipRGB>(detail::byteArrayToType<VipRGB>);
	return 0;
}
static int _registerConverters = registerConverters();
