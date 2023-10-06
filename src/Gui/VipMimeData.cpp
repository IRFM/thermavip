#include "VipMimeData.h"
#include "VipXmlArchive.h"
#include "VipDisplayArea.h"
#include "VipLogging.h"
#include "VipIODevice.h"
#include "VipGui.h"

VipProcessingPool * VipMimeDataCoordinateSystem::fromWidget(QWidget * drop_widget)
{
	while (drop_widget) {
		if (qobject_cast<VipDisplayPlayerArea*>(drop_widget))
			return static_cast<VipDisplayPlayerArea*>(drop_widget)->processingPool();
		drop_widget = drop_widget->parentWidget();
	}
	return NULL;
}



QList<VipPlotItem*> VipMimeDataProcessingObjectList::plotData(VipPlotItem * drop_target, QWidget * drop_widget) const
{
	//we cannot drop on a different processing pool
	VipProcessingPool * target = fromWidget(drop_widget);
	VipProcessingPool * current = m_procs.size() ? m_procs.first()->parentObjectPool() : NULL;
	if (target != current) {
		VIP_LOG_ERROR("Cannot drop on a different workspace");
		return QList<VipPlotItem*>();
	}

	VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
	if (pl)
		setPlayers(vipCreatePlayersFromProcessings(m_procs, pl,target));
	else
		setPlayers(vipCreatePlayersFromProcessings(m_procs, NULL));
	return VipPlotMimeData::plotData(drop_target, drop_widget);
}




static VipOutput * findOutput_copy(VipDisplayObject * obj, VipProcessingPool * target_pool)
{
	if (!target_pool)
		return NULL;

	if (!obj)
		return NULL;

	if (obj->parentObjectPool() != target_pool)
	{
		//we drop to a new processing pool: try to copy the pipeline and drop this new pipeline
		//only copy pipeline if its size is <= 3


		//get the current output index
		if (obj->inputAt(0)->connection()->source()) {

			//try to set a valid output to the processing
			VipAnyData any;
			VipOutput * _out = obj->inputAt(0)->connection()->source();
			while (_out) {
				if (_out->data().isEmpty()) {
					if (_out->parentProcessing()->inputCount() > 0)
						_out = _out->parentProcessing()->inputAt(0)->connection()->source();
				}
				else {
					any = (_out->data());
					break;
				}
			}

			int o_index = obj->inputAt(0)->connection()->source()->parentProcessing()->indexOf(obj->inputAt(0)->connection()->source());
			QList<VipProcessingObject*> pipeline = obj->allSources();
			if (pipeline.size() < 3) {
				if (qobject_cast<VipIODevice*>(pipeline.last())) { //we want a simple pipeline, VipIODevice->VipProcessingList->display

					QList<VipProcessingObject*> new_pipeline = VipProcessingObjectList(pipeline).copy(target_pool);
					if (new_pipeline.size())
					{
						VipProcessingObject * o = new_pipeline.first();
						if (o->outputCount() > o_index) {
							//pipeline copied successfully, return the last processing output
							o->outputAt(o_index)->setData(any);
							return o->outputAt(o_index);
						}
					}

				}
			}
		}


		//we failed to copy the pipeline
		//just create a new one

		VipAnyResource * any = new VipAnyResource();
		any->setData(obj->inputAt(0)->data().data());
		any->mergeAttributes(obj->inputAt(0)->data().attributes());
		return any->outputAt(0);
	}

	//get the processing before the VipDisplayObject (skip the processing list if any)
	VipProcessingObject * proc = NULL;
	VipOutput * out = NULL;
	if (obj)
	{
		if (obj->inputAt(0)->connection()->source()) {
			out = obj->inputAt(0)->connection()->source();
			proc = out->parentProcessing();

			if (qobject_cast<VipProcessingList*>(proc)) {
				if (proc->inputAt(0)->connection()->source())
				{
					//copy the VipProcessingList and recreate the connections
					if (VipProcessingObject * copy = proc->copy())
					{
						copy->inputAt(0)->setConnection(proc->inputAt(0)->connection()->source());
						copy->setParent(proc->parentObjectPool());
						out = copy->outputAt(0);
						proc = copy;

						//try to set a valid output to the processing
						VipOutput * _out = proc->outputAt(0);
						while (_out) {
							if (_out->data().isEmpty()) {
								if (_out->parentProcessing()->inputCount() > 0)
									_out = _out->parentProcessing()->inputAt(0)->connection()->source();
							}
							else {
								out->setData(_out->data());
								break;
							}
						}

						if (target_pool)
							target_pool->reload();
					}
					else
						out = NULL;
				}
				else
					out = NULL;
			}
		}
	}
	return out;
}

static VipPlotPlayer * firstPlotPlayer(const QList<VipAbstractPlayer*> & players)
{
	for (int i = 0; i < players.size(); ++i)
		if (const VipPlotPlayer * p = qobject_cast<const VipPlotPlayer*>(players[i]))
			return const_cast<VipPlotPlayer*>(p);
	return NULL;
}




VipMimeDataDuplicatePlotItem::VipMimeDataDuplicatePlotItem(QObject * parent)
	:VipMimeDataCoordinateSystem(VipCoordinateSystem::Cartesian,parent)
{}

VipMimeDataDuplicatePlotItem::VipMimeDataDuplicatePlotItem(const QList<VipPlotItem*> & lst, QObject * parent)
	: VipMimeDataCoordinateSystem(VipCoordinateSystem::Cartesian,parent)
{
	setPlotItems(lst);
}

bool VipMimeDataDuplicatePlotItem::supportSourceItems(const QList<VipPlotItem*> & lst)
{
	for (int i = 0; i < lst.size(); ++i)
	{
		VipPlotItem * it = lst[i];

		if (qobject_cast<VipPlotSpectrogram*>(it))
			return true;
		else if ((qobject_cast<VipPlotCurve*>(it) || qobject_cast<VipPlotHistogram*>(lst[i])))
			return true;
		else if (qobject_cast<VipPlotShape*>(it) )
			return true;
	}
	return false;
}


bool VipMimeDataDuplicatePlotItem::supportDestinationPlayer(VipAbstractPlayer * pl) const
{
	for (int i = 0; i < m_plots.size(); ++i)
	{
		VipPlotItem * it = m_plots[i];
		if (qobject_cast<VipPlotSpectrogram*>(it) && !pl)
			return true;
		else if ((qobject_cast<VipPlotCurve*>(it) || qobject_cast<VipPlotHistogram*>(m_plots[i])) && (!pl || qobject_cast<VipPlotPlayer*>(pl)))
			return true;
		else if (qobject_cast<VipPlotShape*>(it) && qobject_cast<VipPlayer2D*>(pl))
			return true;
	}
	return false;
}


void VipMimeDataDuplicatePlotItem::setPlotItems(const QList<VipPlotItem*> & lst)
{
	m_plots.clear();
	for (int i = 0; i < lst.size(); ++i)
		if (VipPlotItem * it = lst[i]) {
			if (VipResizeItem * r = qobject_cast<VipResizeItem*>(it)) {
				QList<VipPlotShape*> shs = vipListCast<VipPlotShape*>(r->managedItems());
				if (shs.size())
					it = shs.first();
				else
					continue;
			}
			if(m_plots.indexOf(it) < 0)
				m_plots.append(it);
		}
}
QList<VipPlotItem*> VipMimeDataDuplicatePlotItem::plotItems() const
{
	QList<VipPlotItem*> res;
	for (int i = 0; i < m_plots.size(); ++i)
		if (VipPlotItem * it = m_plots[i])
			res.append(it);
	return res;
}

QList<VipPlotItem*> VipMimeDataDuplicatePlotItem::plotData(VipPlotItem * drop_target, QWidget * drop_widget) const
{
	//clear internal players, if any
	this->setPlayers(QList<VipAbstractPlayer*>());

	VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
	VipProcessingPool * target = fromWidget(drop_widget);
	QList<VipPlotItem*> items = plotItems();

	QMultiMap<VipVideoPlayer*, VipPlotShape*> shapes;

	for (int i = 0; i < items.size(); ++i)
	{
		if (qobject_cast<VipPlotSpectrogram*>(items[i]))
		{
			if (pl) {
				//cannot display the spectrogram in an existing player
				continue;
			}
			else {
				//get the related VipDisplayObject, and get the processing before it (skip the processing list if any)
				VipOutput * out= findOutput_copy(items[i]->property("VipDisplayObject").value<VipDisplayObject*>(), target);
				if (out) {
					//create the new  player
					setPlayers(vipCreatePlayersFromProcessing(out->parentProcessing(), NULL,out,drop_target));
					pl = this->players().size() ? this->players().first() : pl; //get the new player if any
				}
			}
		}
		else if (qobject_cast<VipPlotCurve*>(items[i]) || qobject_cast<VipPlotHistogram*>(items[i]))
		{
			//save item state
			QByteArray item_state = vipSavePlotItemState(items[i]);

			//get the related VipDisplayObject, and get the processing before it (skip the processing list if any)
			VipOutput * out = findOutput_copy(items[i]->property("VipDisplayObject").value<VipDisplayObject*>(), target);
			if (out) {
				//create the player or use an existing one
				VipAbstractPlayer * p = firstPlotPlayer(players() << pl);
				setPlayers(vipCreatePlayersFromProcessing(out->parentProcessing(), p, out, drop_target));
			}

			//apply the XML archive to the new item (keep the same style)
			QList<VipDisplayObject*> displays = this->players().size() ? this->players().first()->displayObjects() : QList<VipDisplayObject*>();
			if (displays.size())
				if(VipDisplayPlotItem * disp = qobject_cast<VipDisplayPlotItem*>(displays.last())) //the last VipDisplayObject is the new one
				{
					//restore state
					vipRestorePlotItemState(disp->item(), item_state);
				}
		}
		else if (VipPlotShape * shape = qobject_cast<VipPlotShape*>(items[i]))
		{
			//keep the shapes for the end
			shapes.insert(qobject_cast<VipVideoPlayer*>(VipAbstractPlayer::findAbstractPlayer(items[i])), shape);

		}
	}

	if (pl)
	{
		//we keep the VipPlotShape for the end since we need a valid player to duplicate them
		if(VipPlayer2D * p2D = qobject_cast<VipPlayer2D*>(pl))
		{
			VipVideoPlayer * vpl = qobject_cast<VipVideoPlayer*>(p2D);

			//sort scene models by yunit
			QMap<QString,VipSceneModel> sm;
			for (QMultiMap<VipVideoPlayer*, VipPlotShape*>::iterator it = shapes.begin(); it != shapes.end(); ++it) {
				if (VipSceneModel s = it.value()->rawData().parent()) {
					QString unit = s.attribute("YUnit").toString();
					sm[unit].add(vipCopyVideoShape( it.value()->rawData(),it.key(),vpl));
					sm[unit].setAttribute("YUnit", unit);
				}
			}
			p2D->addSceneModels(sm.values(),false);
		}
	}

	return VipPlotMimeData::plotData(drop_target, drop_widget);
}
