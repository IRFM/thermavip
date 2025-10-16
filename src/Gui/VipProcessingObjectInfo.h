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

#ifndef VIP_PROCESSING_OBJECT_INFO_H
#define VIP_PROCESSING_OBJECT_INFO_H

#include "VipPlayer.h"
#include "VipProcessingObject.h"
#include "VipToolWidget.h"

class VipMainWindow;
class VipMultiDragWidget;
class VipDisplayPlayerArea;
class QTreeWidgetItem;

/// @brief Default structure that should be returned by #VipAdditionalInfo objects
///
/// VipProcInfo represent a set of information about a player or a processing object.
/// These information are displayed in a VipProcessingObjectInfo widget.
/// 
/// Information are basically a list of key (string) ->attribute (variant).
/// We don't use a QVariantMap here as the attributes ordering might matter.
/// 
/// VipProcInfo embedds information on the potential tooltip content for each attribute.
/// 
struct VIP_GUI_EXPORT VipProcInfo
{
	typedef QPair<QString, QVariant> info;
	typedef QList<info> info_list;
	typedef QMap<QString, QString> key_tips;

	info_list infos;
	key_tips tool_tips;

	void clear()
	{
		infos.clear();
		tool_tips.clear();
	}

	int size() const { return infos.size(); }

	int indexOf(const QString& key, Qt::CaseSensitivity s = Qt::CaseSensitive) const
	{
		for (int i = 0; i < size(); ++i)
			if (infos[i].first.compare(key, s) == 0)
				return i;
		return -1;
	}

	void append(const QString& key, const QVariant& value) { infos.append(info(key, value)); }
	void addToolTip(const QString& key, const QString& tool_tip) { tool_tips[key] = tool_tip; }

	QString toolTip(const QString& key) const
	{
		key_tips::const_iterator it = tool_tips.find(key);
		if (it != tool_tips.end())
			return it.value();
		return key;
	}

	VipProcInfo& import(const QVariantMap& attributes)
	{
		for (QVariantMap::const_iterator it = attributes.begin(); it != attributes.end(); ++it)
			append(it.key(), it.value());
		return *this;
	}

	const info& operator[](int index) const { return infos[index]; }
	info& operator[](int index) { return infos[index]; }

	QVariant operator[](const QString& key) const
	{
		int index = indexOf(key);
		if (index < 0)
			return QVariant();
		return infos[index].second;
	}
};
Q_DECLARE_METATYPE(VipProcInfo)

/// Extract any kind of information from a processing output or from a player.
/// The output of this processing must always be a #VipProcInfo or a QVariantMap containing the extracted infos.
/// 
/// Use VipFDProcessingOutputInfo() to register new VipAdditionalInfo.
/// 
class VIP_GUI_EXPORT VipAdditionalInfo : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput data)
	VIP_IO(VipOutput attributes)

public:
	VipAdditionalInfo(VipAbstractPlayer* pl, QObject* parent = nullptr)
	  : VipProcessingObject(parent)
	  , m_player(pl)
	{
		// disable error logging for this type
		this->setLogErrors(QSet<int>());
	}
	/// @brief Clone the VipAdditionalInfo
	virtual VipAdditionalInfo* clone() const = 0;
	/// @brief Tells if this VipAdditionalInfo returns information on a player or on a processing object
	virtual bool isPlayerInfo() const = 0;
	/// @brief Returns the player used to extract information
	VipAbstractPlayer* player() const { return m_player; }

protected Q_SLOTS:
	/// Call this function if the processing needs to update its output based on the player.
	void emitNeedUpdate() { Q_EMIT needUpdate(); }

Q_SIGNALS:
	void needUpdate();

private:
	QPointer<VipAbstractPlayer> m_player;
};


/// Extract global info on a video player (matrix size, minimum value, maximum value, mean,...)
class VIP_GUI_EXPORT VipExtractImageInfos : public VipAdditionalInfo
{
public:
	VipExtractImageInfos(VipVideoPlayer* pl)
	  : VipAdditionalInfo(pl)
	{
	}

	virtual VipExtractImageInfos* clone() const { return new VipExtractImageInfos(qobject_cast<VipVideoPlayer*>(player())); }
	virtual bool isPlayerInfo() const { return true; }

protected:
	virtual void apply();
};

/// Extract info on a video player selected shapes (size, minimum value inside, maximum value, mean, polyline length and angles, area...)
class VIP_GUI_EXPORT VipExtractShapesInfos : public VipAdditionalInfo
{
public:
	VipExtractShapesInfos(VipAbstractPlayer* pl);
	virtual VipExtractShapesInfos* clone() const { return new VipExtractShapesInfos((player())); }
	virtual bool isPlayerInfo() const { return true; }

protected:
	virtual void apply();
};

/// Extract curve statistics inside selected shapes (if any):  number of points, minimum value, maximum value, mean, integral, length,...
class VIP_GUI_EXPORT VipExtractCurveInfos : public VipAdditionalInfo
{
public:
	VipExtractCurveInfos(VipPlotPlayer* pl);
	virtual VipExtractCurveInfos* clone() const { return new VipExtractCurveInfos(qobject_cast<VipPlotPlayer*>(player())); }
	virtual bool isPlayerInfo() const { return false; }

protected:
	virtual void apply();
};

/// Extract histogram statistics:  number of samples, minimum value, maximum value, mean, sum,...
class VIP_GUI_EXPORT VipExtractHistogramInfos : public VipAdditionalInfo
{
public:
	VipExtractHistogramInfos(VipPlotPlayer* pl)
	  : VipAdditionalInfo(pl)
	{
	}
	virtual VipExtractHistogramInfos* clone() const { return new VipExtractHistogramInfos(qobject_cast<VipPlotPlayer*>(player())); }
	virtual bool isPlayerInfo() const { return false; }

protected:
	virtual void apply();
};


namespace detail
{
	class VIP_GUI_EXPORT VipExtractAttributeFromInfo : public VipProcessingObject
	{
		Q_OBJECT
		VIP_IO(VipInput attributes)
		VIP_IO(VipOutput value)

	public:
		VipExtractAttributeFromInfo(QObject* parent = nullptr);
		~VipExtractAttributeFromInfo();

		void setVipAdditionalInfo(VipAdditionalInfo*);

		void setAttributeName(const QString&);
		QString attributeName() const;

	protected:
		virtual void apply();

		VipAdditionalInfo* m_info;
		QString m_name;
		QString m_exactName;
	};

}



/// This function dispatcher is called by the VipProcessingObjectInfo to display additional informations on a processing output.
/// Its signature is VipAdditionalInfo* (VipAbstractPlayer* player, VipOutput * src, const QVariant & data);
/// The last argument is the output data type.
VIP_GUI_EXPORT VipFunctionDispatcher<3>& VipFDProcessingOutputInfo();

/// @brief A tool widget that displays information on a processing object or a plyer.
///
/// The source VipProcessingObject is set through #VipProcessingObjectInfo::setProcessingObject or #VipProcessingObjectInfo::setPlotItem.
/// The source VipProcessingObject is also modified when the focus player changes.
/// 
class VIP_GUI_EXPORT VipProcessingObjectInfo : public VipToolWidgetPlayer
{
	Q_OBJECT
	Q_PROPERTY(QPen itemBorderPen READ itemBorderPen WRITE setItemBorderPen)
	Q_PROPERTY(QColor itemBorderColor READ itemBorderColor WRITE setItemBorderColor)

public:
	VipProcessingObjectInfo(VipMainWindow*);
	~VipProcessingObjectInfo();

	/// Reimplemented from VipToolWidgetPlayer.
	virtual bool setPlayer(VipAbstractPlayer*);

	/// Set the source VipProcessingObject. The widget will display the attributes of \a output if provided.
	/// If \a output is nullptr, the widget will display all outputs of the processing.
	void setProcessingObject(VipProcessingObject* obj, VipOutput* output = nullptr);

	/// Try to set the source processing object through a #VipPlotItem.
	/// This function retrieve the #VipDisplayObject stored in \a item and ,if not null, find its source processing.
	void setPlotItem(VipPlotItem* item);

	///\internal Helper function, extract the time trace for selected attributes and returns the related processing object
	QList<VipProcessingObject*> plotAttributes(QList<VipOutput*> outputs, const QStringList& attributes, VipProcessingPool* pool);
	QList<VipProcessingObject*> plotSelectedAttributes();

	QPen itemBorderPen() const;
	void setItemBorderPen(const QPen& pen);

	QColor itemBorderColor() const;
	void setItemBorderColor(const QColor& c);

private Q_SLOTS:
	void copyToClipboard();
	void saveToFile();
	void plotSelection();
	void updateInfos();
	void updateInfos(VipProcessingIO*, const VipAnyData&);
	void itemSelected(VipPlotItem*);
	void search();

	void currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*);
	void topLevelWidgetClosed(VipDisplayPlayerArea*, VipMultiDragWidget*);

	void delayedItemSelected();

protected:
	virtual void showEvent(QShowEvent*);

private:
	QString content() const;
	
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_GUI_EXPORT VipProcessingObjectInfo* vipGetProcessingObjectInfo(VipMainWindow* window = nullptr);

#endif
