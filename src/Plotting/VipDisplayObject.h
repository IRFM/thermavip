#ifndef VIP_DISPLAY_OBJECT_H
#define VIP_DISPLAY_OBJECT_H

#include <QPointer>
#include <QWidget>

#include "VipProcessingObject.h"
#include "VipPlotCurve.h"
#include "VipPlotSpectrogram.h"
#include "VipPlotHistogram.h"
#include "VipPlotShape.h"
#include "VipPlotBarChart.h"
#include "VipExtractStatistics.h"
#include "VipArchive.h"
#include "VipFunctional.h"
#include "VipSymbol.h"
#include "VipPlotGrid.h"
#include "VipPlotMarker.h"
#include "VipPlotScatter.h"
#include "VipPlotQuiver.h"
#include "VipAxisBase.h"
#include "VipAxisColorMap.h"
#include "VipColorMap.h"
#include "VipPlotWidget2D.h"

/// \addtogroup Plotting
/// @{


/// VipDisplayObject is the base class for VipProcessingObject designed to display data. 
/// 
/// One instance of VipDisplayObject should display only one input data.
/// The display operation must be performed in the reimplementation of VipDisplayObject::displayData() and/or prepareForDisplay().
/// 
/// Since drawing operations are usually only allowed within the main thread, VipDisplayObject let you dispatch the display
/// operation between the internal task pool thread and the main GUI thread.
/// 
/// VipDisplayObject::prepareForDisplay() is always called from the task pool thread first, 
/// and VipDisplayObject::displayData() is always be called from the main thread afterward if
/// VipDisplayObject::prepareForDisplay() returns false.
/// 
/// By default, VipDisplayObject is asynchronous.
/// 
class VIP_PLOTTING_EXPORT VipDisplayObject : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput data)
	VIP_IO(VipProperty numThreads)
public:

	VipDisplayObject(QObject * parent = nullptr);
	~VipDisplayObject();

	/// @brief Returns the VipAbstractPlayer displaying the data of this VipDisplayObject.
	virtual QWidget* widget() const {
		return nullptr;
	}
	/// @brief Returns true if the displayed data is currently visible.
	virtual bool isVisible() const {
		return false;
	}
	/// @brief Returns true if the display operation is currently in progress.
	virtual bool displayInProgress() const;

	/// @brief Returned the preferred size for the display object.
	virtual QSize sizeHint() const {
		return QSize();
	}

	/// @brief Return the display object title.
	/// This could be any string that contains a human readable text describing this display object.
	virtual QString title() const {
		return QString();
	}

	/// @brief Select whever the displayed object uses input data attributes for its formatting.
	///
	/// For instance, VipDisplayPlotItem might use input VipAnyData attributes 'Name' to set the VipPlotItem title,
	/// 'stylesheet' to set the VipPlotItem style sheet, 'XUnit' and 'YUnit' to set the VipPlotItem axes units,
	/// 'ZUnit' to set the VipPlotItem colormap unit.
	void setFormattingEnabled(bool);
	bool formattingEnabled() const;

	/// @brief Reimplemented from VipProcessingObject
	virtual bool useEventLoop() const { return true; }

protected:
	///Reimplement this function to perform the drawing based on \a data.
	virtual void displayData(const VipAnyDataList & ) { }
	/// This function is called in the processing thread just before launching the display.
	/// It can be used to performs some time consuming operations in the processing thread insteed of the GUI one (like
	/// converting a numeric image into a RGB one).
	/// Returns false to tell that displayData() should be called afterward, true otherwise (display finished)
	virtual bool prepareForDisplay(const VipAnyDataList&) { return false; }

	///Tis function is called whenever a new input data is available (see VipProcessingObject for more details).
	virtual void apply() ;

Q_SIGNALS:
	///Emitted by the display(VipAnyDataList&) function
	void displayed(const VipAnyDataList &);

private Q_SLOTS:
	void display( const VipAnyDataList &);
	void checkVisibility();
private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayObject*)
VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & stream, const VipDisplayObject * r);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & stream, VipDisplayObject * r);


/// VipDisplayPlotItem is a VipDisplayObject that displays its data through a VipPlotItem object.
/// If the VipDisplayPlotItem is destroyed, the VipPlotItem itself won't be destroyed.
/// However, destroying the VipPlotItem will destroy the VipDisplayPlotItem.
/// 
class VIP_PLOTTING_EXPORT VipDisplayPlotItem : public VipDisplayObject
{
	Q_OBJECT
	friend VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & stream, const VipDisplayPlotItem * r);
	friend VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & stream, VipDisplayPlotItem * r);

public:
	VipDisplayPlotItem(QObject * parent = nullptr);
	~VipDisplayPlotItem();

	virtual QWidget* widget() const ;
	virtual bool isVisible() const ;
	virtual bool displayInProgress() const;

	virtual QString title() const;

	///Returns the internal VipPlotItem.
	virtual VipPlotItem* item() const;
	///Set the internal VipPlotItem. This will destroy the previous one, if any.
	/// This will also setting the property "VipDisplayObject" to the plot item containing a pointer to this VipDisplayPlotItem.
	virtual void setItem(VipPlotItem * item);

	/// @brief Remove and returns the internal item
	VipPlotItem* takeItem();

	///Equivalent to:
	/// \code
	/// item()->setItemAttribute(VipPlotItem::IsSuppressable,enable);
	/// \endcode
	void setItemSuppressable(bool enable);
	/// Equivalent to:
	/// \code
	/// item()->itemAttribute(VipPlotItem::IsSuppressable);
	/// \endcode
	bool itemSuppressable() const;

	/// Format the item based on given data.
	/// The standard implementation set the item's title to the data property "Name", and set the item's axis unit based on the data properties "XUnit and "YUnit".
	virtual void formatItem(VipPlotItem * item, const VipAnyData & any, bool force = false);
	void formatItemIfNecessary(VipPlotItem * item, const VipAnyData & any);

private Q_SLOTS:
	void setItemProperty();
	void internalFormatItem();
private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayPlotItem*)
VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & stream, const VipDisplayPlotItem * r);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & stream, VipDisplayPlotItem * r);

/// A VipDisplayPlotItem that displays a curve based on VipPlotCurve.
/// It accepts as input data either a VipPointVector, VipComplexPointVector, VipPoint
///  or a value convertible to double (in which case the VipAnyData time is used as X value).
class VIP_PLOTTING_EXPORT VipDisplayCurve : public VipDisplayPlotItem
{
	Q_OBJECT
	VIP_IO(VipProperty Sliding_time_window)
	VIP_IO_DESCRIPTION(Sliding_time_window,"Temporal window of the curve (seconds).\nThis is only used when plotting a continuous curve (streaming)")

public:

	VipDisplayCurve(QObject * parent = nullptr);
	~VipDisplayCurve();

	VipExtractComponent * extractComponent() const;

	virtual bool acceptInput(int top_level_index, const QVariant & v) const;
	virtual VipPlotCurve* item() const { return static_cast<VipPlotCurve*>(VipDisplayPlotItem::item()); }
	virtual void setItem(VipPlotCurve* item);

protected:
	virtual bool prepareForDisplay(const VipAnyDataList& data);
	virtual void displayData(const VipAnyDataList& data);

private:
	class PrivateData;
	PrivateData *m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayCurve*)


/// A VipDisplayPlotItem that displays a spectrogram based on VipPlotSpectrogram.
/// It accepts as input data either a VipNDArray or a VipRasterData.
/// This is the standard display object for displaying images and movies.
class VIP_PLOTTING_EXPORT VipDisplayImage : public VipDisplayPlotItem
{
	Q_OBJECT
public:

	VipDisplayImage(QObject * parent = nullptr);
	~VipDisplayImage();

	virtual bool acceptInput(int top_level_index, const QVariant & v) const;
	virtual VipPlotSpectrogram* item() const { return static_cast<VipPlotSpectrogram*>(VipDisplayPlotItem::item()); }

	virtual QSize sizeHint() const ;

	VipExtractComponent * extractComponent() const;

	/// Returns true if the VipDisplayImage can display \a ar as is, without extracting a component.
	/// Currently, this returns true for all images except complex ones.
	static bool canDisplayImageAsIs(const VipNDArray & ar);

protected:
	virtual void displayData(const VipAnyDataList& data);
	virtual bool prepareForDisplay(const VipAnyDataList& data);


private:
	class PrivateData;
	PrivateData * m_data;

};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayImage*)



/// A VipDisplayPlotItem that displays a scene model based on VipPlotSceneModel.
/// It accepts as input a VipSceneModel or a VipShape.
class VIP_PLOTTING_EXPORT VipDisplaySceneModel : public VipDisplayPlotItem
{
	Q_OBJECT
public:

	VipDisplaySceneModel(QObject * parent = nullptr);

	virtual bool acceptInput(int top_level_index, const QVariant & v) const;
	VipPlotSceneModel* item() const {return static_cast<VipPlotSceneModel*>(VipDisplayPlotItem::item());}

	void setTransform(const QTransform & tr);
	QTransform transform() const;

protected:
	virtual bool prepareForDisplay(const VipAnyDataList& lst);
	virtual void displayData(const VipAnyDataList& data);

private:
	QTransform m_transform;
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplaySceneModel*)





namespace detail
{
	/// @brief Base class for display objects using a VipPlotItemDataType
	/// @tparam PlotItemType VipPlotItemDataType type
	/// @tparam Data data type
	/// @tparam Sample sample type
	template<class PlotItemType, class Data , class Sample >
	class VipBaseDisplayPlotItemData : public VipDisplayPlotItem
	{
	public:
		VipBaseDisplayPlotItemData(QObject* parent = nullptr)
		  : VipDisplayPlotItem(parent)
		{
			setItem(new PlotItemType());
			item()->setAutoMarkDirty(false);
		}

		virtual bool acceptInput(int top_level_index, const QVariant& v) const { 
			return v.canConvert<Data>() || (!std::is_same<Data,Sample>::value && v.canConvert<Sample>());
		}
		virtual PlotItemType* item() const { 
			return static_cast<PlotItemType*>(VipDisplayPlotItem::item()); 
		}
		virtual void setItem(PlotItemType* it) {
			if (it && it != item()) {
				it->setAutoMarkDirty(false);
				VipDisplayPlotItem::setItem(it);
			}
		}

	protected:
		virtual void displayData(const VipAnyDataList& lst) {
			if (PlotItemType* it = item()) {
				it->markDirty();
				// format the item
				if (lst.size())
					formatItemIfNecessary(it, lst.back());
			}
		}
	};

	template<class PlotItemType, class Data = typename PlotItemType::data_type, class Sample = typename PlotItemType::sample_type>
	class VipDisplayPlotItemData : public VipBaseDisplayPlotItemData<PlotItemType,Data,Sample>
	{
	public:
		VipDisplayPlotItemData(QObject* parent = nullptr)
		  : VipBaseDisplayPlotItemData<PlotItemType, Data, Sample>(parent)
		{}

	protected:
		virtual bool prepareForDisplay(const VipAnyDataList& lst)
		{
			if (PlotItemType* curve = this->item()) {

				Data data;
				bool full_data = false;
				for (int i = 0; i < lst.size(); ++i) {
					const VipAnyData & any = lst[i];
					QVariant v = any.data();

					if (v.userType() == qMetaTypeId<Data>()) {
						data = v.value<Data>();
						full_data = true;
					}
					else if (v.userType() == qMetaTypeId<Sample>())
						data.push_back(v.value<Sample>());
				}
				if (full_data)
					curve->setRawData(data);
				else
					curve->updateData([&](Data& d) { d.append(data); });
			}
			return false;
		}
	};

	template<class PlotItemType, class Data>
	class VipDisplayPlotItemData<PlotItemType,Data,Data> : public VipBaseDisplayPlotItemData<PlotItemType, Data, Data>
	{
	public:
		VipDisplayPlotItemData(QObject* parent = nullptr)
		  : VipBaseDisplayPlotItemData<PlotItemType, Data, Data>(parent)
		{
		}

	protected:
		virtual bool prepareForDisplay(const VipAnyDataList& lst)
		{
			if (lst.size()) {
				if (PlotItemType* curve = this->item()) {
					curve->setRawData(lst.back().value<Data>());
				}
			}
			return false;
		}
	};
}



/// A VipDisplayPlotItem that displays a histogram based on VipPlotHistogram.
/// It accepts as input data either a VipIntervalSampleVector or a VipIntervalSample.
class VIP_PLOTTING_EXPORT VipDisplayHistogram : public detail::VipDisplayPlotItemData<VipPlotHistogram>
{
	Q_OBJECT
public:
	VipDisplayHistogram(QObject* parent = nullptr)
	  : detail::VipDisplayPlotItemData<VipPlotHistogram>(parent)
	{
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayHistogram*)


/// A VipDisplayPlotItem that displays a scatter plot based on VipPlotScatter.
/// It accepts as input data of type VipScatterPointVector or VipScatterPoint.
class VIP_PLOTTING_EXPORT VipDisplayScatterPoints : public detail::VipDisplayPlotItemData<VipPlotScatter>
{
	Q_OBJECT
public:
	VipDisplayScatterPoints(QObject* parent = nullptr)
	  : detail::VipDisplayPlotItemData<VipPlotScatter>(parent)
	{
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayScatterPoints*)



/// A VipDisplayPlotItem that displays arrows based on VipPlotQuiver.
/// It accepts as input data of type VipQuiverPointVector or VipQuiverPoint.
class VIP_PLOTTING_EXPORT VipDisplayQuiver : public detail::VipDisplayPlotItemData<VipPlotQuiver>
{
	Q_OBJECT
public:
	VipDisplayQuiver(QObject* parent = nullptr)
	  : detail::VipDisplayPlotItemData<VipPlotQuiver>(parent)
	{
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayQuiver*)


/// A VipDisplayPlotItem that displays bars based on VipPlotBarChart.
/// It accepts as input data of type VipBar or VipBarVector.
class VIP_PLOTTING_EXPORT VipDisplayBars : public detail::VipDisplayPlotItemData<VipPlotBarChart>
{
	Q_OBJECT
public:
	VipDisplayBars(QObject* parent = nullptr)
	  : detail::VipDisplayPlotItemData<VipPlotBarChart>(parent)
	{
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayBars*)



/// A VipDisplayPlotItem that displays a marker based on VipPlotMarker.
/// It accepts as input data of type VipPoint.
class VIP_PLOTTING_EXPORT VipDisplayMarker : public detail::VipDisplayPlotItemData<VipPlotMarker>
{
	Q_OBJECT
public:
	VipDisplayMarker(QObject* parent = nullptr)
	  : detail::VipDisplayPlotItemData<VipPlotMarker>(parent)
	{
	}
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayMarker*)





Q_DECLARE_METATYPE(QList<VipDisplayObject*> )
typedef QList<vip_double> DoubleList;
Q_DECLARE_METATYPE(DoubleList)
typedef QVector<vip_double> DoubleVector;
Q_DECLARE_METATYPE(DoubleVector)



VIP_REGISTER_QOBJECT_METATYPE_NO_DECLARE(VipPlotItem*);

VIP_REGISTER_QOBJECT_METATYPE(VipPlotItemData*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotCurve*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotHistogram*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotGrid*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotCanvas*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotMarker*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotScatter*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotQuiver*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotBarChart*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotRasterData*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotSpectrogram*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotShape*);
VIP_REGISTER_QOBJECT_METATYPE(VipPlotSceneModel*);
VIP_REGISTER_QOBJECT_METATYPE(VipAxisBase*);
VIP_REGISTER_QOBJECT_METATYPE(VipAxisColorMap*);
VIP_REGISTER_QOBJECT_METATYPE(VipColorMap*);
VIP_REGISTER_QOBJECT_METATYPE(VipLinearColorMap*);
VIP_REGISTER_QOBJECT_METATYPE(VipAlphaColorMap*);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotItem * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotItem * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotItemData * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotItemData * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotCurve * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotCurve * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotHistogram * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotHistogram * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotGrid * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotGrid * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotCanvas * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotCanvas * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotMarker * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotMarker * value);

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotBarChart* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotBarChart* value);

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotQuiver* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotQuiver* value);

VIP_PLOTTING_EXPORT VipArchive& operator<<(VipArchive& arch, const VipPlotScatter* value);
VIP_PLOTTING_EXPORT VipArchive& operator>>(VipArchive& arch, VipPlotScatter* value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotRasterData * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotRasterData * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotSpectrogram * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotSpectrogram * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotShape * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotShape * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotSceneModel * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotSceneModel * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAbstractScale * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipAbstractScale * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAxisBase * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipAxisBase * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipColorMap * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipColorMap * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipLinearColorMap * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipLinearColorMap * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAlphaColorMap * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipAlphaColorMap * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipAxisColorMap * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipAxisColorMap * value);

VIP_PLOTTING_EXPORT VipArchive & operator<<(VipArchive & arch, const VipPlotArea2D * value);
VIP_PLOTTING_EXPORT VipArchive & operator>>(VipArchive & arch, VipPlotArea2D * value);


/// Returns a copy of given item.
/// This function uses the serialize/deserialize mechanism to produce a copy of input VipPlotItem.
/// You should always use this function to copy an item, as it will take care of internal IDs used to identify each item.
/// Note that the output item will NOT share the input item axes and will have a different ID (as in #VipUniqueId::id()).
VIP_PLOTTING_EXPORT VipPlotItem * vipCopyPlotItem(const VipPlotItem* item);

/// Save the current item state, except its ID (as in #VipUniqueId::id()) and its axises.
VIP_PLOTTING_EXPORT QByteArray vipSavePlotItemState(const VipPlotItem* item);
/// Restore an item state previously saved with #vipSavePlotItemState.
VIP_PLOTTING_EXPORT bool vipRestorePlotItemState(VipPlotItem* item, const QByteArray & state);


/// @}
//end Plotting

#endif
