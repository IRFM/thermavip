#ifndef VIS_MIME_DATA_H
#define VIS_MIME_DATA_H

#include "VipPlotMimeData.h"
#include "VipPlayer.h"

/// \addtogroup Gui
/// @{

class VIP_GUI_EXPORT VipMimeDataCoordinateSystem : public VipPlotMimeData
{
	Q_OBJECT

public:
	VipMimeDataCoordinateSystem(VipCoordinateSystem::Type type = VipCoordinateSystem::Cartesian, QObject * parent = NULL)
	:VipPlotMimeData(),m_type(type)
	{
		setParent(parent);
	}

	virtual VipCoordinateSystem::Type coordinateSystemType() const {return m_type;}
	void setCoordinateSystem(VipCoordinateSystem::Type type) {m_type = type;}
	QList<VipAbstractPlayer*> players() const {return m_players;}

	//help function, get the processing pool from a widget
	static VipProcessingPool * fromWidget(QWidget * drop_widget) ;

protected:
	void setPlayers(const QList<VipAbstractPlayer*>  & players) const { const_cast<QList<VipAbstractPlayer*>&>(m_players) = players;}

private:
	VipCoordinateSystem::Type m_type;
	QList<VipAbstractPlayer*> m_players;
};


class VIP_GUI_EXPORT VipMimeDataProcessingObjectList : public VipMimeDataCoordinateSystem
{
	Q_OBJECT
public:
	virtual QList<VipPlotItem*> plotData(VipPlotItem * drop_target, QWidget * drop_widget) const;

	void setProcessing(const VipProcessingObjectList & lst) {m_procs = lst;}
	VipProcessingObjectList processings() const {return m_procs;}
private:
	VipProcessingObjectList m_procs;
};

class VipMimeDataPaths : public VipMimeDataCoordinateSystem
{
public:
	virtual QList<VipPlotItem*> plotData(VipPlotItem * drop_target, QWidget * drop_widget) const {
		VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
		if(pl)
			setPlayers(vipCreatePlayersFromStringList(m_paths,pl,drop_target));
		else
			setPlayers(vipCreatePlayersFromStringList(m_paths,NULL));
		return VipPlotMimeData::plotData(drop_target, drop_widget);
	}

	void setPaths(const QStringList & lst) {m_paths = lst;}

private:
	QStringList m_paths;
};

class VipMimeDataMapFile : public VipMimeDataCoordinateSystem
{
public:
	virtual QList<VipPlotItem*> plotData(VipPlotItem * drop_target, QWidget * drop_widget) const {
		VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
		if (pl)
			setPlayers(vipCreatePlayersFromPaths(m_paths, pl, drop_target));
		else
			setPlayers(vipCreatePlayersFromPaths(m_paths, NULL));
		return VipPlotMimeData::plotData(drop_target, drop_widget);
	}

	void setPaths(const VipPathList & paths) { m_paths = paths; }
	const VipPathList & paths() const { return m_paths; }

private:
	VipPathList m_paths;
};


#include <functional>

template<class Return> //Return must inherit VipProcessingObject or be a QList<VipProcessingObject*>
class VipMimeDataLazyEvaluation : public VipMimeDataCoordinateSystem
{
	template<class T>
	struct is_list {
		static QList<VipAbstractPlayer*> create(const T & proc, VipAbstractPlayer * pl, QObject * target) {
			return vipCreatePlayersFromProcessing(proc, pl, NULL, target);
		}
	};
	template<class T>
	struct is_list<QList<T> > {
		static QList<VipAbstractPlayer*> create(const QList<T> & proc, VipAbstractPlayer * pl, QObject * target) {
			return vipCreatePlayersFromProcessings(proc, pl, target);
		}
	};
public:

	VipMimeDataLazyEvaluation(VipCoordinateSystem::Type type = VipCoordinateSystem::Null, QObject * parent = NULL)
	:VipMimeDataCoordinateSystem(type,parent)
	{}

	VipMimeDataLazyEvaluation(const std::function<Return()> & fun, VipCoordinateSystem::Type type = VipCoordinateSystem::Null, QObject * parent = NULL)
	:VipMimeDataCoordinateSystem(type,parent)
	{
		setFunction(fun);
	}

	~VipMimeDataLazyEvaluation()
	{
	}

	virtual QList<VipPlotItem*> plotData(VipPlotItem * drop_target, QWidget * drop_widget) const {
		VipAbstractPlayer * pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
		Return ret = const_cast<std::function<Return()>&>(m_function)();
		setPlayers(is_list<Return>::create(ret, pl, drop_target)//vipCreatePlayersFromProcessing(ret,pl, drop_target)
);

		return VipPlotMimeData::plotData(drop_target, drop_widget);
	}

	template< class Callable>
	void setFunction(const Callable & fun) {
		m_function = std::function<Return()>(fun);
	}

private:
	std::function<Return()> m_function;
};




class VIP_GUI_EXPORT VipMimeDataDuplicatePlotItem : public VipMimeDataCoordinateSystem
{
	Q_OBJECT
public:

	VipMimeDataDuplicatePlotItem(QObject * parent = NULL);
	VipMimeDataDuplicatePlotItem(const QList<VipPlotItem*> & lst, QObject * parent = NULL);

	virtual QList<VipPlotItem*> plotData(VipPlotItem * drop_target, QWidget * drop_widget) const;

	void setPlotItems(const QList<VipPlotItem*> & lst);
	QList<VipPlotItem*> plotItems() const;
	void clearItems() { setPlotItems(QList<VipPlotItem*>()); }

	static bool supportSourceItems(const QList<VipPlotItem*> & lst);
	bool supportDestinationPlayer(VipAbstractPlayer *) const;

private:
	QList<QPointer<VipPlotItem> > m_plots;
};

/// @}
//end Gui

#endif
