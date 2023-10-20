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

#ifndef VIS_MIME_DATA_H
#define VIS_MIME_DATA_H

#include "VipMapFileSystem.h"
#include "VipPlayer.h"
#include "VipPlotMimeData.h"

/// \addtogroup Gui
/// @{

class VIP_GUI_EXPORT VipMimeDataCoordinateSystem : public VipPlotMimeData
{
	Q_OBJECT

public:
	VipMimeDataCoordinateSystem(VipCoordinateSystem::Type type = VipCoordinateSystem::Cartesian, QObject* parent = nullptr)
	  : VipPlotMimeData()
	  , m_type(type)
	{
		setParent(parent);
	}

	virtual VipCoordinateSystem::Type coordinateSystemType() const { return m_type; }
	void setCoordinateSystem(VipCoordinateSystem::Type type) { m_type = type; }
	QList<VipAbstractPlayer*> players() const { return m_players; }

	// help function, get the processing pool from a widget
	static VipProcessingPool* fromWidget(QWidget* drop_widget);

protected:
	void setPlayers(const QList<VipAbstractPlayer*>& players) const { const_cast<QList<VipAbstractPlayer*>&>(m_players) = players; }

private:
	VipCoordinateSystem::Type m_type;
	QList<VipAbstractPlayer*> m_players;
};

class VIP_GUI_EXPORT VipMimeDataProcessingObjectList : public VipMimeDataCoordinateSystem
{
	Q_OBJECT
public:
	virtual QList<VipPlotItem*> plotData(VipPlotItem* drop_target, QWidget* drop_widget) const;

	void setProcessing(const VipProcessingObjectList& lst) { m_procs = lst; }
	VipProcessingObjectList processings() const { return m_procs; }

private:
	VipProcessingObjectList m_procs;
};

class VipMimeDataPaths : public VipMimeDataCoordinateSystem
{
public:
	virtual QList<VipPlotItem*> plotData(VipPlotItem* drop_target, QWidget* drop_widget) const
	{
		VipAbstractPlayer* pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
		if (pl)
			setPlayers(vipCreatePlayersFromStringList(m_paths, pl, drop_target));
		else
			setPlayers(vipCreatePlayersFromStringList(m_paths, nullptr));
		return VipPlotMimeData::plotData(drop_target, drop_widget);
	}

	void setPaths(const QStringList& lst) { m_paths = lst; }

private:
	QStringList m_paths;
};

class VipMimeDataMapFile : public VipMimeDataCoordinateSystem
{
public:
	virtual QList<VipPlotItem*> plotData(VipPlotItem* drop_target, QWidget* drop_widget) const
	{
		VipAbstractPlayer* pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
		if (pl)
			setPlayers(vipCreatePlayersFromPaths(m_paths, pl, drop_target));
		else
			setPlayers(vipCreatePlayersFromPaths(m_paths, nullptr));
		return VipPlotMimeData::plotData(drop_target, drop_widget);
	}

	void setPaths(const VipPathList& paths) { m_paths = paths; }
	const VipPathList& paths() const { return m_paths; }

private:
	VipPathList m_paths;
};

#include <functional>

template<class Return> // Return must inherit VipProcessingObject or be a QList<VipProcessingObject*>
class VipMimeDataLazyEvaluation : public VipMimeDataCoordinateSystem
{
	template<class T>
	struct is_list
	{
		static QList<VipAbstractPlayer*> create(const T& proc, VipAbstractPlayer* pl, QObject* target) { return vipCreatePlayersFromProcessing(proc, pl, nullptr, target); }
	};
	template<class T>
	struct is_list<QList<T>>
	{
		static QList<VipAbstractPlayer*> create(const QList<T>& proc, VipAbstractPlayer* pl, QObject* target) { return vipCreatePlayersFromProcessings(proc, pl, target); }
	};

public:
	VipMimeDataLazyEvaluation(VipCoordinateSystem::Type type = VipCoordinateSystem::Null, QObject* parent = nullptr)
	  : VipMimeDataCoordinateSystem(type, parent)
	{
	}

	VipMimeDataLazyEvaluation(const std::function<Return()>& fun, VipCoordinateSystem::Type type = VipCoordinateSystem::Null, QObject* parent = nullptr)
	  : VipMimeDataCoordinateSystem(type, parent)
	{
		setFunction(fun);
	}

	~VipMimeDataLazyEvaluation() {}

	virtual QList<VipPlotItem*> plotData(VipPlotItem* drop_target, QWidget* drop_widget) const
	{
		VipAbstractPlayer* pl = VipAbstractPlayer::findAbstractPlayer(drop_target);
		Return ret = const_cast<std::function<Return()>&>(m_function)();
		setPlayers(is_list<Return>::create(ret, pl, drop_target) // vipCreatePlayersFromProcessing(ret,pl, drop_target)
		);

		return VipPlotMimeData::plotData(drop_target, drop_widget);
	}

	template<class Callable>
	void setFunction(const Callable& fun)
	{
		m_function = std::function<Return()>(fun);
	}

private:
	std::function<Return()> m_function;
};

class VIP_GUI_EXPORT VipMimeDataDuplicatePlotItem : public VipMimeDataCoordinateSystem
{
	Q_OBJECT
public:
	VipMimeDataDuplicatePlotItem(QObject* parent = nullptr);
	VipMimeDataDuplicatePlotItem(const QList<VipPlotItem*>& lst, QObject* parent = nullptr);

	virtual QList<VipPlotItem*> plotData(VipPlotItem* drop_target, QWidget* drop_widget) const;

	void setPlotItems(const QList<VipPlotItem*>& lst);
	QList<VipPlotItem*> plotItems() const;
	void clearItems() { setPlotItems(QList<VipPlotItem*>()); }

	static bool supportSourceItems(const QList<VipPlotItem*>& lst);
	bool supportDestinationPlayer(VipAbstractPlayer*) const;

private:
	QList<QPointer<VipPlotItem>> m_plots;
};

/// @}
// end Gui

#endif
