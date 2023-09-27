#include "VipPlotMimeData.h"
#include "VipPlotItem.h"
#include <QPointer>

class VipPlotMimeData::PrivateData
{
public:
	QList<QPointer<VipPlotItem> > plotData;
};


VipPlotMimeData::VipPlotMimeData()
{
	d_data = new PrivateData();
	setText("VipPlotMimeData");
}

VipPlotMimeData::~VipPlotMimeData()
{
	delete d_data;
}

void VipPlotMimeData::setPlotData(const QList<VipPlotItem*> & items)
{
	d_data->plotData.clear();
	for(int i=0; i < items.size(); ++i)
		d_data->plotData << items[i];
}

QList<VipPlotItem*> VipPlotMimeData::plotData(VipPlotItem * drop_target, QWidget * ) const
{
	Q_UNUSED(drop_target)
	QList<VipPlotItem*> res;
	for(int i=0; i < d_data->plotData.size(); ++i)
		if(d_data->plotData[i])
			res << d_data->plotData[i].data();
	return res;
}

VipCoordinateSystem::Type VipPlotMimeData::coordinateSystemType() const
{
	for(int i=0; i < d_data->plotData.size(); ++i)
		if(d_data->plotData[i])
			return d_data->plotData[i]->coordinateSystemType();
	return VipCoordinateSystem::Null;
}
