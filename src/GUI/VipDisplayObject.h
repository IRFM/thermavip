#ifndef VIP_DISPLAY_OBJECT_H
#define VIP_DISPLAY_OBJECT_H

#include <QPointer>
#include <QWidget>

#include "VipUniqueId.h"
#include "VipProcessingObject.h"
#include "VipPlotCurve.h"
#include "VipPlotSpectrogram.h"
#include "VipPlotHistogram.h"
#include "VipPlotShape.h"
#include "VipMapFileSystem.h"
#include "VipProcessingHelper.h"
#include "VipExtractStatistics.h"

/// \addtogroup Gui
/// @{

class VipAbstractPlayer;
class VipArchive;

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
class VIP_GUI_EXPORT VipDisplayObject : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput data)
	VIP_IO(VipProperty numThreads)
public:

	VipDisplayObject(QObject * parent = NULL);
	~VipDisplayObject();

	/// @brief Returns the VipAbstractPlayer displaying the data of this VipDisplayObject.
	virtual VipAbstractPlayer* widget() const {
		return NULL;
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
	/// It sends the data to VipDisplayObject::displayData through the main thread if \a displayInGuiThread is true.
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
VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & stream, const VipDisplayObject * r);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & stream, VipDisplayObject * r);


/// VipDisplayPlotItem is a VipDisplayObject that displays its data through a VipPlotItem object.
/// If the VipDisplayPlotItem is destroyed, the VipPlotItem itself won't be destroyed.
/// However, destroying the VipPlotItem will destroy the VipDisplayPlotItem.
/// 
class VIP_GUI_EXPORT VipDisplayPlotItem : public VipDisplayObject
{
	Q_OBJECT
	friend VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & stream, const VipDisplayPlotItem * r);
	friend VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & stream, VipDisplayPlotItem * r);

public:
	VipDisplayPlotItem(QObject * parent = NULL);
	~VipDisplayPlotItem();

	virtual VipAbstractPlayer* widget() const ;
	virtual bool isVisible() const ;
	virtual bool displayInProgress() const;

	virtual QString title() const;

	///Returns the internal VipPlotItem.
	VipPlotItem* item() const;
	///Set the internal VipPlotItem. This will destroy the previous one, if any.
	/// This will also setting the property "VipDisplayObject" to the plot item containing a pointer to this VipDisplayPlotItem.
	void setItem(VipPlotItem * item);

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
VIP_GUI_EXPORT VipArchive & operator<<(VipArchive & stream, const VipDisplayPlotItem * r);
VIP_GUI_EXPORT VipArchive & operator>>(VipArchive & stream, VipDisplayPlotItem * r);

/// A VipDisplayPlotItem that displays a curve based on VipPlotCurve.
/// It accepts as input data either a VipPointVector, VipComplexPointVector, VipPoint
///  or a value convertible to double (in which case the VipAnyData time is used as X value).
class VIP_GUI_EXPORT VipDisplayCurve : public VipDisplayPlotItem
{
	Q_OBJECT
	VIP_IO(VipProperty Sliding_time_window)
	VIP_IO_DESCRIPTION(Sliding_time_window,"Temporal window of the curve (seconds).\nThis is only used when plotting a continuous curve (streaming)")

public:

	VipDisplayCurve(QObject * parent = NULL);
	~VipDisplayCurve();

	VipExtractComponent * extractComponent() const;

	virtual bool acceptInput(int top_level_index, const QVariant & v) const;
	VipPlotCurve* item() const {return static_cast<VipPlotCurve*>(VipDisplayPlotItem::item());}

protected:
	virtual bool prepareForDisplay(const VipAnyDataList& data);
	virtual void displayData(const VipAnyDataList& data);

private:
	class PrivateData;
	PrivateData *m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayCurve*)

/// A VipDisplayPlotItem that displays a histogram based on VipPlotHistogram.
/// It accepts as input data either a VipIntervalSampleVector or a VipIntervalSample.
class VIP_GUI_EXPORT VipDisplayHistogram : public VipDisplayPlotItem
{
	Q_OBJECT
public:

	VipDisplayHistogram(QObject * parent = NULL);

	virtual bool acceptInput(int top_level_index, const QVariant & v) const;
	VipPlotHistogram* item() const {return static_cast<VipPlotHistogram*>(VipDisplayPlotItem::item());}

protected:
	virtual bool prepareForDisplay(const VipAnyDataList& data);
	virtual void displayData(const VipAnyDataList& data);

private:
	QVariant m_previous;
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayHistogram*)

/// A VipDisplayPlotItem that displays a spectrogram based on VipPlotSpectrogram.
/// It accepts as input data either a VipNDArray or a VipRasterData.
/// This is the standard display object for displaying images and movies.
class VIP_GUI_EXPORT VipDisplayImage : public VipDisplayPlotItem
{
	Q_OBJECT
public:

	VipDisplayImage(QObject * parent = NULL);
	~VipDisplayImage();

	virtual bool acceptInput(int top_level_index, const QVariant & v) const;
	VipPlotSpectrogram* item() const {return static_cast<VipPlotSpectrogram*>(VipDisplayPlotItem::item());}

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
class VIP_GUI_EXPORT VipDisplaySceneModel : public VipDisplayPlotItem
{
	Q_OBJECT
public:

	VipDisplaySceneModel(QObject * parent = NULL);

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

Q_DECLARE_METATYPE(QList<VipDisplayObject*> )







/// Function dispatcher which create a VipDisplayObject object from a QVariant and a VipAbstractPlayer.
/// This dispatcher is called within vipCreateDisplayFromData to provide a custom behavior. Its goal is to create a new instance of VipDisplayObject
/// able to display given data into a specific player.
/// Its signature is:
/// \code
/// VipDisplayObject*(const QVariant &, VipAbstractPlayer *, const VipAnyData & any)
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher & vipFDCreateDisplayFromData();

/// Creates a VipDisplayObject able to display the data from \a any into \a player. The player might be NULL.
VIP_GUI_EXPORT VipDisplayObject * vipCreateDisplayFromData(const VipAnyData & any, VipAbstractPlayer *);



/// Function dispatcher which create a list of VipAbstractPlayer instances that will display the given data.
/// This dispatcher is called within vipCreatePlayersFromData and vipCreatePlayersFromProcessing to provide a custom behavior.
/// If a non null player is given, this function will try to display the processing outputs into the player, and a list containing only the player is returned on success. An empty list will be returned in case of failure.
///
///
/// Its signature is:
/// \code
/// QList<VipAbstractPlayer *> (const QVariant &, VipAbstractPlayer *, const VipAnyData & any, QObject * target)
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher & vipFDCreatePlayersFromData();
/// Creates a list of VipAbstractPlayer instances that will display the given data.
/// If a non null player is given, this function will try to display the data into the player, and a list containing only the player is returned on success. An empty list will be returned in case of failure.
///
/// \a target is a QObject used to define the target in drop situations (in case of dropping, it is usually a #VipPlotItem).
VIP_GUI_EXPORT QList<VipAbstractPlayer*> vipCreatePlayersFromData(const VipAnyData & any, VipAbstractPlayer * pl, VipOutput * src = NULL, QObject * target = NULL, QList<VipDisplayObject*>* outputs = NULL);




/// Function dispatcher which create a list of VipAbstractPlayer instances that will display the output(s) of a VipProcessingObject.
/// This dispatcher is called within vipCreatePlayersFromProcessing to provide a custom behavior.
/// If a non null player is given, this function will try to display the processing outputs into the player, and a list containing only the player is returned on success. An empty list will be returned in case of failure.
/// Its signature is:
/// \code
/// QList<VipAbstractPlayer *> (VipProcessingObject *, VipAbstractPlayer*,VipOutput*, QObject * target );
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher & vipFDCreatePlayersFromProcessing();



/// Creates a list of VipAbstractPlayer instances that will display the output(s) of a VipProcessingObject.
/// If a non null player is given, this function will try to display the processing outputs into the player, and a list containing only the player is returned on success. An empty list will be returned in case of failure.
/// If a non null VipOutput is given, only the data of this output will be displayed in the player.
///
/// This function try to minimize the number of returned players. Therefore, if the processing has multiple outputs that can be displayed into the same player
/// (like for instance multiple floating point values), only one player is created for all of them (displaying for instance multiple curves).
///
/// \a target is a QObject used to define the target in drop situations (in case of dropping, it is usually a #VipPlotItem).
VIP_GUI_EXPORT QList<VipAbstractPlayer *> vipCreatePlayersFromProcessing(VipProcessingObject * proc, VipAbstractPlayer * player, VipOutput * src = NULL, QObject * target = NULL, QList<VipDisplayObject*> * outputs = NULL);

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of a several VipProcessingObject instances.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
VIP_GUI_EXPORT QList<VipAbstractPlayer *> vipCreatePlayersFromProcessings(const QList<VipProcessingObject *> &, VipAbstractPlayer *, QObject * target = NULL, QList<VipDisplayObject*>* outputs = NULL);

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of a several VipProcessingObject instances.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
template< class T>
QList<VipAbstractPlayer *> vipCreatePlayersFromProcessings(const QList<T *> & lst, VipAbstractPlayer * player, QObject * target = NULL, QList<VipDisplayObject*>* outputs = NULL) {
	return vipCreatePlayersFromProcessings(vipListCast<VipProcessingObject*>(lst),player,target, outputs);
}


/// Creates a list of VipAbstractPlayer instances that will display the output(s) of the VipIODevice objects created from all the given strings.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
VIP_GUI_EXPORT QList<VipAbstractPlayer *> vipCreatePlayersFromStringList(const QStringList & lst, VipAbstractPlayer * player, QObject * target = NULL, QList<VipDisplayObject*>* outputs = NULL);

/// Creates a list of VipAbstractPlayer instances that will display the output(s) of the VipIODevice objects created from all the given paths and QIODevice objetcs.
/// This function try to minimize the number of returned players by inserting several display objects into the same player.
VIP_GUI_EXPORT QList<VipAbstractPlayer *> vipCreatePlayersFromPaths(const VipPathList & paths, VipAbstractPlayer * player, QObject * target = NULL, QList<VipDisplayObject*>* outputs = NULL);

/// Helper function.
/// Create a new processing object (\a info) based on given VipOutput.
/// A VipProcessingList is added just after the processing.
VIP_GUI_EXPORT VipProcessingObject * vipCreateProcessing(VipOutput* output, const VipProcessingObject::Info & info);

/// Helper function.
/// Create a new data fusion processing (\a info) based on given VipOutput.
/// A VipProcessingList is added just after the data fusion processing.
VIP_GUI_EXPORT VipProcessingObject * vipCreateDataFusionProcessing(const QList<VipOutput*> & outputs, const VipProcessingObject::Info & info);
VIP_GUI_EXPORT VipProcessingObject * vipCreateDataFusionProcessing(const QList<VipPlotItem*> & items, const VipProcessingObject::Info & info);

/// @}
//end Gui

#endif
