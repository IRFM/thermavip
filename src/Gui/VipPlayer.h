#ifndef VIP_PLAYER_H
#define VIP_PLAYER_H

#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QStatusBar>
#include <QToolButton>
#include <QLabel>
#include <QPainter>

#include "VipNDArray.h"
#include "VipPlotWidget2D.h"
#include "VipPlotShape.h"
#include "VipDisplayObject.h"
#include "VipFunctional.h"
#include "VipToolTip.h"
#include "VipStandardWidgets.h"
#include "VipGui.h"

/// \addtogroup Gui
/// @{




class QVBoxLayout;
class VipDisplayObject;
class VipDisplayPlayerArea;
class VipSliderGrip;

/// The standard tool bar for all \a VipPlayer2D objects.
/// It provides actions for changing the selection mode, the zoom or the selection.
class VIP_GUI_EXPORT VipPlayerToolBar : public VipToolBar
{
public:

	QAction * saveItemAction;
	QToolButton * saveitem;
	QMenu * saveItemMenu;

	QAction * selectionModeAction;
	//QToolButton *	selectionMode;

	VipPlayerToolBar(QWidget * parent = NULL);
	~VipPlayerToolBar() {}
};



/// Singleton used to manage copy/paste of VipPlotItem objects between players.
/// Currently only works for VipPlotCurve, VipPlotHistogram, VipPlotSpectrogram and VipPlotShape.
/// Uses internally a VipMimeDataDuplicatePlotItem to duplicate plot items.
class VIP_GUI_EXPORT VipPlotItemClipboard : public QObject
{
	Q_OBJECT
public:
	~VipPlotItemClipboard();

	/// Copy a list of VipPlotItem to the clipboard.
	/// This will keep the item's connections (if any).
	static void copy(const PlotItemList &);

	/// Returns the list of source VipPlotItem that are in the clipboard
	static PlotItemList copiedItems();

	/// Paste copied items to a VipAbstractPlotArea
	static void paste(VipAbstractPlotArea * dst, QWidget * drop_target);

	/// Tells if at least one of current copied items can be dropped into target player.
	/// Given player might be NULL.
	static bool supportDestinationPlayer(VipAbstractPlayer * pl);

	static bool supportSourceItems();

	/// Returns the internal VipMimeDataDuplicatePlotItem
	static const QMimeData * mimeData();

Q_SIGNALS:
	void itemsCopied(const PlotItemList & src);
	void itemsPasted(const PlotItemList & dst);

private:
	VipPlotItemClipboard();
	static VipPlotItemClipboard & instance();

	class PrivateData;
	PrivateData * m_data;
};


/// Singleton class used to monitore the lifetime of VipAbstractPlayer objects.
class VIP_GUI_EXPORT VipPlayerLifeTime : public QObject
{
	Q_OBJECT
	VipPlayerLifeTime();
public:
	~VipPlayerLifeTime();

	static VipPlayerLifeTime * instance();
	///Returns all currently available players
	static QList<VipAbstractPlayer*> players();

	/// Call this function on VipAbstractPlayer derivated classes to emit the signal #created()
	static void emitCreated(VipAbstractPlayer*);
	/// Call this function on VipAbstractPlayer derivated classes to emit the signal #destroyed()
	static void emitDestroyed(VipAbstractPlayer*);

Q_SIGNALS:
	/// Emitted when a new VipAbstractPlayer is created.
	/// Can be emitted several times for a player since all constructors might call #emitCreated().
	void created(VipAbstractPlayer*); 
	/// Emitted when a VipAbstractPlayer object is destroyed.
	/// Guaranteed to be emitted only once per player.
	void destroyed(VipAbstractPlayer*);

private:
	class PrivateData;
	PrivateData *m_data;
};


class VIP_GUI_EXPORT VipPlotWidget
  : public QWidget
  , public VipRenderObject
{
	Q_OBJECT
public:
	VipPlotWidget(QWidget* parent = NULL);
	VipPlotWidget(VipAbstractPlotWidget2D* plot, QWidget* parent = NULL);

	virtual void setPlotWidget2D(VipAbstractPlotWidget2D* plot);
	VipAbstractPlotWidget2D* plotWidget2D() const;

	QGridLayout* gridLayout() const;
};


/// \a VipAbstractPlayer is the base class for all widgets representing 1D to 4D data in Thermavip.
/// It inherits the \a VipPlotWidget class from the #Plotting library of Thermavip.
///
/// Base implementation of Thermavip provides 2 classes inheriting VipAbstractPlayer for visualization of 2D plots and images: #VipPlotPlayer and #VipVideoPlayer.
/// \a VipAbstractPlayer is usually fitted to represent #VipDisplayObject instances.
///
/// VipAbstractPlayer objects can be serialized into a #VipArchive for session saving/loading.
/// By default, all #VipPlotItem inside this player will be saved, unless they define the property "_vip_no_serialize" set to true.
class VIP_GUI_EXPORT VipAbstractPlayer : public VipPlotWidget
{
	Q_OBJECT
		
public:
	VipAbstractPlayer(QWidget * parent = NULL);
	~VipAbstractPlayer();

	/// Returns a new instance inheriting VipAbstractPlayer class
	virtual VipAbstractPlayer * createEmpty() const = 0;
	/// Retruns all instances of #VipDisplayObject this player contains
	virtual QList<VipDisplayObject*> displayObjects() const = 0;
	///Returns the main display object (if any) for this player
	virtual VipDisplayObject* mainDisplayObject() const { return NULL; }
	/// Set the #VipProcessingPool for this player.
	/// This function can be overloaded to provide a custom behavior based on the VipProcessingPool object.
	/// When subclassing VipAbstractPlayer, you must call VipAbstractPlayer::setProcessingPool() at the start of your function to ensure integrity with setProcessingPool's built-in processing pool
	virtual void setProcessingPool(VipProcessingPool * pool);
	VipProcessingPool * processingPool() const;
	/// Returns the best size for this widget.
	/// Default implementation uses #VipDisplayObject::sizeHint().
	virtual QSize sizeHint() const;

	/// Returns the tool bar associated to this player if any.
	/// This is mainly used by the AdvancedDisplay plugin.
	virtual QToolBar* playerToolBar() const { return NULL; }

	/// If set to true (default), the player's title is automatically set based on the displayed data name.
	/// Disable it if you want to give a custom window title (VipAbstractPlayer::setWindowTitle)
	void setAutomaticWindowTitle(bool);
	bool automaticWindowTitle() const;

	/// Returns the parent VipDisplayPlayerArea, if any
	VipDisplayPlayerArea * parentDisplayArea() const;

	/// Returns the parent VipBaseDragWidget id, if any. Returns 0 if no valid parent was found.
	int parentId() const;

	///Find the parent VipAbstractPlayer that contains given QGraphicsItem
	static VipAbstractPlayer * findAbstractPlayer(QGraphicsItem * child);
	///Find the parent VipAbstractPlayer that contains given VipDisplayObject
	static VipAbstractPlayer * findAbstractPlayer(VipDisplayObject * display);
	///Try to find all the VipAbstractPlayer that displays the outputs of given VipProcessingObject
	static QList<VipAbstractPlayer *> findOutputPlayers(VipProcessingObject * proc);

protected:

	virtual void dragEnterEvent(QDragEnterEvent *evt);
	virtual void dragLeaveEvent(QDragLeaveEvent *evt);
	virtual void dragMoveEvent(QDragMoveEvent *evt);
	virtual void dropEvent(QDropEvent *evt);
	virtual void showEvent(QShowEvent * evt);
	virtual void hideEvent(QHideEvent * evt);
	virtual void changeEvent(QEvent *e);

	/// Reimplemented from #VipRenderObject::startRender()
	virtual void startRender(VipRenderState &);
	/// Reimplemented from #VipRenderObject::endRender()
	virtual void endRender(VipRenderState &);

	bool inDestructor() const;

Q_SIGNALS:
	void renderStarted(const VipRenderState&);
	void renderEnded(const VipRenderState&);

private:
	class PrivateData;
	PrivateData * m_data;
};

//expose VipAbstractPlayer* and QList<VipAbstractPlayer*> to the meta type system for the function dispatchers
typedef QList<VipAbstractPlayer*> AbstractPlayerList;
Q_DECLARE_METATYPE(VipAbstractPlayer*)
Q_DECLARE_METATYPE(AbstractPlayerList)



/// A player that simply contains a widget
class VIP_GUI_EXPORT VipWidgetPlayer : public VipAbstractPlayer
{
	Q_OBJECT
public:
	VipWidgetPlayer(QWidget* w =NULL, QWidget* parent = NULL);
	~VipWidgetPlayer();

	virtual VipWidgetPlayer* createEmpty() const { return new VipWidgetPlayer(NULL); }
	virtual QList<VipDisplayObject*> displayObjects() const { return  QList<VipDisplayObject*>(); }
	virtual QSize sizeHint() const;

	QWidget* widget() const;
	virtual QWidget* widgetForMouseEvents() const{ return widget(); }
protected:
	virtual void resizeEvent(QResizeEvent* evt);
	virtual bool renderObject(QPainter* p, const QPointF& pos, bool draw_background);
private:
	void setWidget(QWidget*);
	class PrivateData;
	PrivateData* m_data;
};
VIP_REGISTER_QOBJECT_METATYPE(VipWidgetPlayer*)




class VipPlayer2D;
/// VipPlayerToolTip manages which information should be displayed in the tool tip of a VipPlayer2D.
/// Each type of player (VipVideoPlayer, VipPlotPlayer,...) has its own tool tip management. However, all players of the same type share
/// the same tool tip behavior.
class VIP_GUI_EXPORT VipPlayerToolTip
{
	VipPlayerToolTip();

public:
	~VipPlayerToolTip();
	static VipPlayerToolTip & instance();
	///Set the tool tip behavior of a specific type of player
	static void setToolTipFlags(VipToolTip::DisplayFlags, const QMetaObject * meta);
	///Returns the tool tip behavior of a specific type of player
	static VipToolTip::DisplayFlags toolTipFlags(const QMetaObject * meta);
	///Set the default tool tip behavior for a specific type of player.
	/// This can only be done once and before any call to #VipPlayerToolTip::setToolTipFlags for this player type.
	/// Usually, it should be called in the constructor of a player.
	/// Returns true on success (if called before #VipPlayerToolTip::setToolTipFlags), false otherwise.
	static bool setDefaultToolTipFlags(VipToolTip::DisplayFlags, const QMetaObject * meta);

	static QMap<QString, VipToolTip::DisplayFlags> allToolTipFlags();
	static void setAllToolTipFlags(const QMap<QString, VipToolTip::DisplayFlags> &);

private:
	QMap<QString, VipToolTip::DisplayFlags> m_flags;
};

Q_DECLARE_METATYPE(VipPlayerToolTip*)





/// Base class for all \a VipAbstractPlayer representing 2D + time data based on #VipPlotItem objects.
/// It provides additional widgets like a tool bar (#VipPlayerToolBar) and a status bar.
/// It also provides functions to save its content as a \a QPixmap.
///
/// VipPlayer2D handles mouse click, item selection changes and item addtion/removal and forward them to the function dispatchers
/// #VipFDItemRightClick, #VipFDItemSelected, #VipFDItemAddedOnPlayer and #VipFDItemRemovedFromPlayer.
class VIP_GUI_EXPORT VipPlayer2D : public VipAbstractPlayer
{
	Q_OBJECT
		friend class VipPlayerToolTip;
public:
	enum ClipboardOperation
	{
		Copy,
		Cut,
		Paste
	};

	VipPlayer2D(QWidget * parent = NULL);
	~VipPlayer2D();

	virtual QMenu * generateToolTipMenu();

	///Returns, in scene coordinates (see #VipPlotWidget class for more information), the rectangle containing the displayed data
	//virtual QRectF plotAreaRect() const;
	///Render the player's content into given QPainter
	virtual void draw(QPainter * p, const QRectF & dst, Qt::AspectRatioMode aspectRatioMode = Qt::KeepAspectRatio) const;
	/// Returns the current player's content into a QPixmap
	virtual QPixmap currentPixmap(QPainter::RenderHints hints = QPainter::Antialiasing | QPainter::TextAntialiasing) const;

	//Returns the default editable object when double clicking inside the player (if any)
	virtual QGraphicsObject *  defaultEditableObject() const = 0;

	///Reinplemented from #VipAbstractPlayer::displayObjects()
	virtual QList<VipDisplayObject*> displayObjects() const;
	///Set the underlying #VipAbstractPlotWidget2D object
	virtual void setPlotWidget2D(VipAbstractPlotWidget2D * plot);
	///Returns the VipPlayer2D's VipPlotSceneModel object.
	/// Each \a VipPlayer2D have an internal #VipPlotSceneModel used to draw and represent Regions Of Interest (ROI)
	VipPlotSceneModel * plotSceneModel() const;
	///Returns the VipPlayer2D's VipPlotSceneModel objects.
	/// Each \a VipPlayer2D have an internal #VipPlotSceneModel used to draw and represent Regions Of Interest (ROI).
	/// However, a VipPlotPlayer might have multiples VipPlotSceneModel if it defines multiple left scales (for different Y units).
	/// Note that the first element in the list is always the one returned by #VipPlayer2D::plotSceneModel().
	virtual QList<VipPlotSceneModel *> plotSceneModels() const;

	/// Find a VipPlayer2D's VipPlotSceneModel object based on a VipSceneModel.
	/// Returns a NULL VipPlotSceneModel is no scene model has been found.
	VipPlotSceneModel * findPlotSceneModel(const VipSceneModel & scene) const;

	/// Find a VipPlayer2D's VipPlotSceneModel object based on its X and Y scales.
	/// Returns a NULL VipPlotSceneModel is no valid object has been found.
	VipPlotSceneModel * findPlotSceneModel(const QList<VipAbstractScale*> & scales) const;

	/// Create a new VipPlotSceneModel for given scales and returns it.
	/// The plot scene model is created in order to use the 'standard' parameters (pen, brush, ...)
	/// and to emit the right signals (#sceneModelAdded(VipPlotSceneModel*), #sceneModelRemoved(VipPlotSceneModel*) and
	/// #sceneModelGroupsChanged(VipPlotSceneModel*) ).
	VipPlotSceneModel * createPlotSceneModel(const QList<VipAbstractScale*> & scales, VipCoordinateSystem::Type type);

	/// Returns all selected/unselected and/or visible/hidden  VipPlotShape objects within this player
	QList<VipPlotShape*> findSelectedPlotShapes(int selected = 2, int visible = 2) const;
	/// Returns all selected/unselected and/or visible/hidden  VipShape objects within this player
	QList<VipShape> findSelectedShapes(int selected = 2, int visible = 2) const;

	/// Helper function.
	/// Returns the VipDisplaySceneModel corresponding to a given scene model or shape, or NULL if no VipDisplaySceneModel was found.
	VipDisplaySceneModel * findDisplaySceneModel(const VipSceneModel & scene) const;
	/// Helper function.
	/// Returns the VipDisplaySceneModel corresponding to a given scene model or shape, or NULL if no VipDisplaySceneModel was found.
	VipDisplaySceneModel * findDisplaySceneModel(const VipShape & scene) const;

	/// Add the content of given VipSceneModels to the current editable scene models (used for ROI edition).
	/// For video player, this function adds all VipSceneModel to the one returned by #plotSceneModel().
	/// For plot player, this function adds the VipSceneModel to the right one based on the "YUnit" scene model attribute.
	/// If the yunit (corresponding to a left scale) is not found, the content of the VipSceneModel is added to the one returned
	/// by #plotSceneModel().
///
	/// If \a remove_old_shapes is true, each VipSceneModel is cleared before adding the new shapes. For plot player having multiple left scales,
	/// a VipSceneModel that is not concerned by the adding operation will never be cleared.
///
	/// Not that each shape added to an internal VipSceneModel will be automatically removed from the input one.
	void addSceneModels(const VipSceneModelList & lst, bool remove_old_shapes);

	///Returns the VipPlayer2D's standard toolbar
	virtual QToolBar* playerToolBar() const { return toolBar(); }
	VipPlayerToolBar * toolBar() const;
	QWidget * toolBarWidget() const;
	void insertToolBar(int index, QToolBar* bar);
	void addToolBar(QToolBar * bar);
	int toolBarCount() const;
	QToolBar* toolBarAt(int index);
	int indexOfToolBar(QToolBar * bar) const;
	QToolBar * takeToolBar(int index) const;
	QList<QToolBar*> toolBars() const;

	VipToolBar* afterTitleToolBar() const;

	///Returns the QLabel that the status bar contained by default
	QLabel * statusText() const;

	void setToolTipFlags(VipToolTip::DisplayFlags);
	VipToolTip::DisplayFlags toolTipFlags() const;

	virtual bool saveItemContent(VipPlotItemData * data, const QString & path);

	bool isSelectionZoomAreaEnabled() const;

	/// Returns the screen coordinates of the last mouse button press
	QPoint lastMousePressScreenPos() const;

	///Try to find a VipPlayer2D that displays given VipSceneModel
	static VipPlayer2D * findPlayer2D(const VipSceneModel & scene);

	///Returns the current drop player when dropping a mime data inside a VipPlayer2D
	static VipPlayer2D * dropTarget();

protected Q_SLOTS:
	virtual void plotItemAdded(VipPlotItem *);
	virtual void plotItemRemoved(VipPlotItem *);
	virtual void plotItemSelectionChanged(VipPlotItem*);
	virtual void plotItemAxisUnitChanged(VipPlotItem *);
	virtual void toolTipFlagsChanged(VipToolTip::DisplayFlags);
	virtual void itemsDropped(VipPlotItem*, QMimeData*);

	/// Called by mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton) , can be reimplemented to provide custom behavior on mouse click.
	/// Can return true to tell that the click has been handle and the mouseButtonRelease function should stop (menu on right click won't be displayed).
	virtual bool plotItemClicked(VipPlotItem*, VipPlotItem::MouseButton) { return false; }

	/// Reimplement to provide undo/redo operations based on #VipSceneModelState
	virtual void keyPressEvent(QKeyEvent* evt);

	/// This function is called whenever the player is created (after the constructor),
	/// and, if reloading a session, just after the processing pool has been loaded.
	/// Default implementation does nothing.
	virtual void onPlayerCreated() {}

public Q_SLOTS:
	/// Called by a tool bar action.
	/// Have a different behavior on plot player (zoom selection) and video player (item selection).
	virtual void selectionZoomArea(bool enable);
	void copySelectedItems();
	void pasteItems();

	/// Unselect and reselect again currently selected items.
	/// This will trigger again the itemSelectionChanged() function
	/// that will update several internal properties, like the processing menus.
	void resetSelection();


	///Set the default plot scene model as returned by #plotSceneModel()
	void setPlotSceneModel(VipPlotSceneModel *);

	/// Select next VipPlotItem. This behave like the TAB key to select the next widget.
	/// If no item is selected, select the first one.
	/// Note that it won't select a spectrogram, grid, canvas or scene model.
	void nextSelection(bool keep_previous_selection = false);

	/// Call resetSelection() for all VipPlayer2D in \a area
	static void resetSelection(VipDisplayPlayerArea * area);

protected Q_SLOTS:
	//detect key event for clipboard operation
	void keyPress(VipPlotItem * item, qint64 id, int key, int modifiers);
	//detect left click to display a menu
	void mouseButtonRelease(VipPlotItem*, VipPlotItem::MouseButton);
	//detect change of selection
	void itemSelectionChanged(VipPlotItem*);
	//detect new item
	void itemAdded(VipPlotItem *);
	//detect item removed
	void itemRemoved(VipPlotItem*);
	void itemAxisUnitChanged(VipPlotItem *);
	void playerCreated();
	void toolTipChanged();
	PlotItemList savableItems() const;
	void saveMenuPopup();
	void saveMenuClicked(QAction*);
	void emitSceneModelGroupsChanged();
	void emitSceneModelChanged();
	void recordLastMousePress();

	/// Reimplemented from #VipRenderObject::startRender()
	virtual void startRender(VipRenderState &);
	/// Reimplemented from #VipRenderObject::endRender()
	virtual void endRender(VipRenderState &);

Q_SIGNALS:
	void sceneModelAdded(VipPlotSceneModel*);
	void sceneModelRemoved(VipPlotSceneModel*);
	void sceneModelGroupsChanged(VipPlotSceneModel*);
	void sceneModelChanged(VipPlotSceneModel*);
	void mouseSelectionChanged(bool);

private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlayer2D*)


class VipAnyResource;
class VipGenericRecorder;
class VipRecordWidget;

/// \internal Used in video player status bar to display mouse position and pixel value
class VIP_GUI_EXPORT ImageAndText : public QWidget
{
	Q_OBJECT
public:
	QLabel image;
	QLabel text;
	ImageAndText();
};
 
/// \a VipVideoPlayer is a \a VipPlayer2D used to represent movies.
class VIP_GUI_EXPORT VipVideoPlayer : public VipPlayer2D
{
	Q_OBJECT
		Q_PROPERTY(QPen defaultContourPen READ defaultContourPen WRITE setDefaultContourPen)

public:
	VipVideoPlayer(VipImageWidget2D * img = NULL, QWidget * parent = NULL);
	~VipVideoPlayer();

	///Returns the #VipImageWidget2D widget
	VipImageWidget2D * viewer() const;

	QAction * showAxesAction() const;
	//QAction * recordAction() const;

	///Returns the zoom factor (1 correspond to a zoom of 100%)
	double zoomFactor() const;
	///Set the zoom factor
	void setZoomFactor(double);

	/// Returns the image transform combining all transformations from the source processing.
	/// If this player's pipeline has multiple branches, returns a null QTransform.
	/// Returned transform has its origin to the top left corner of the image.
	QTransform imageTransform() const;

	QRectF visualizedImageRect() const;
	void setPendingVisualizedImageRect(const QRectF &);

	VipNDArray array() const;

	/// Returns true is the image currently being displayed is a color one (RGB format)
	bool isColorImage() const;

	virtual QGraphicsObject *  defaultEditableObject() const;

	///Set the #VipPlotSpectrogram object used to display images
	void setSpectrogram(VipPlotSpectrogram * spectrogram);
	///Returns the current #VipPlotSpectrogram object
	VipPlotSpectrogram * spectrogram() const;

	QToolButton * superimposeButton() const;
	QAction * superimposeAction() const;
	QComboBox * zoomWidget() const;
	QAction * frozenAction() const;

	virtual void setProcessingPool(VipProcessingPool * pool);

	///Returns the VipVideoPlayer's recorder.
	/// All VipVideoPlayer instances internally store  a #VipGenericRecorder object to record at will incoming images.
	/// The VipGenericRecorder is already connected to the VipPlotSpectrogram's source, so all you have to do to record incoming images
	/// is to enable it, set a path and open it.
	/// The VipGenericRecorder can be manually handled through a #VipRecordWidget widget.
	//VipGenericRecorder * recorder() const;
	///Returns the #VipRecordWidget widget used to handle the VipGenericRecorder
	//VipRecordWidget * recordWidget() const;
	///Returns the source #VipProcessingList, which must be a source of the inner #VipDisplayImage.
	/// Returns a NULL pointer if this player does not have a #VipDisplayImage, or if the #VipDisplayImage does not have a source #VipProcessingList.
	VipProcessingList * sourceProcessingList() const;

	///Returns a new, empty VipVideoPlayer instance
	VipVideoPlayer * createEmpty() const { return new VipVideoPlayer(); }

	virtual VipDisplayObject* mainDisplayObject() const;

	///Extract the pixel values along a polyline shape
	QList<VipDisplayCurve*> extractPolylines(const VipShapeList & shs, const QString & method) const;

	VipAnyResource* extractPolylineValuesAlongTime(const VipShape& shape) const;

	///Extract the histograms of a shape
	QList<VipDisplayHistogram*> extractHistograms(const VipShape & sh, const QString & method) const;




	struct ShapeInfo
	{
		VipShapeList shapes;
		QList<QPair<VipDisplaySceneModel*, QString> > identifiers;
		ShapeInfo(const VipShapeList& lst) : shapes(lst) {}
		ShapeInfo(const QList<QPair<VipDisplaySceneModel*, QString> >& lst) : identifiers(lst) {}
	};

	/// Extract the temporal evolution inside one or several shapes.
	/// If \a stats is 0, a dialog bow will be promped to ask the user for required statistics.
	/// \a one_frame_out_of specifies how many frames should be skipped in the computation.
	/// \a multi_shape specifies how multiple shapes should handled (0: union, 1: intersection, 2: extract time trace for all shapes separately).
///
	/// Time statistics can be extract from a list of VipShape (that could be merged with union or intersection) or a list of shape identifier
	/// and VipDisplaySceneModel (in which case they cannot be merged, and multi_shape is forced to 2).
	QList<VipProcessingObject*> extractTimeEvolution(const ShapeInfo& infos, VipShapeStatistics::Statistics stats = 0, int one_frame_out_of = 1, int multi_shape = -1, const QVector<double> & quantiles = QVector<double>());

	/// Etract the temporal statistics for the full image:
	/// - Maximum image
	/// - Minimum image
	/// - Mean image
	/// - Std image
	/// Returns a VipAnyResource that outputs a VipMultiNDArray with all images
	VipProcessingObject * extractTimeStatistics();

	bool isShowAxes() const;
	bool isAutomaticColorScale() const;
	bool isFlatHistogramColorScale() const;
	int flatHistogramStrength() const;
	bool isColorScaleVisible() const;
	bool isFrozen() const;
	bool isSharedZoom() const;

	void addContourLevel(double);
	void removeContourLevel(double);
	void setContourLevels(const QList<vip_double> & levels);
	QList<vip_double> contourLevels() const;
	QPen 	defaultContourPen() const;

	int colorMap() const;

	/// Convert gobal screen coordinates to this image position
	QPoint globalPosToImagePos(const QPoint& global);

public Q_SLOTS:
	///Show/hide the axes
	virtual void showAxes(bool);
	virtual void showColorScaleParameters();
	virtual void setColorScaleVisible(bool);
	virtual void setAutomaticColorScale(bool);
	virtual void setFlatHistogramColorScale(bool);
	virtual void setFlatHistogramStrength(int strength);
	virtual void enableAutomaticColorScale();
	virtual void disableAutomaticColorScale();
	virtual void fitColorScaleToGrips();
	virtual void removeAllContourLevels();
	virtual void setDefaultContourPen(const QPen &);
	virtual void setColorMap(int);
	virtual void setFrozen(bool);
	virtual void setSharedZoom(bool);
	//add processings to the processing list (if any)
	virtual void addSelectedProcessing(const VipProcessingObject::Info &);
	virtual void setColorMapOptionsVisible(bool);
	void setVisualizedImageRect(const QRectF&);
	/// If exatcly on contour level is present, increase it by 1
	bool increaseContour();

	/// If exatcly on contour level is present, decrese it by 1
	bool decreaseContour();

	//internal use only
	void createShapeFromIsoLine(const QPoint& img_pos);
	void updateShapeFromIsoLine(const QPoint& img_pos);
	void updateSelectedShapesFromIsoLine();
Q_SIGNALS:
	void displayImageChanged();
	void colorMapChanged(int);
	void imageTransformChanged(const QTransform & _new);

private Q_SLOTS:

	void toolBarZoomChanged();
	void viewerZoomChanged();
	void updateStatusInfo();
	void imageChanged();

	///Start/stop recording using the VipGenericRecorder object
	//void setRecording(bool record);

	//superimpose image
	void computeSuperimposeMenu();
	void setSuperimposeOpacity(int);
	void superimposeTriggered(QAction*);

	//recompute extract compoent widget, processing list widget,...
	void updateContent();

	//update ROI based on pipeline transform
	void updateImageTransform();

	//update transform of scene models
	void sceneModelAdded(VipPlotSceneModel*) { this->updateImageTransform(); }
	void sceneModelRemoved(VipPlotSceneModel*) { this->updateImageTransform(); }

	//update processing menu
	void updateProcessingMenu();

	//manage contour levels
	void colorMapClicked(VipAbstractScale*, VipPlotItem::MouseButton, double);
	void contourClicked(VipSliderGrip*, VipPlotItem::MouseButton);
	void handleContour(QAction*);
	void updateContourLevels();

	void receivedColorMapChanged(int);

	//Update the tool tip when the image content changes
	void refreshToolTip();

	//for shared zoom
	void visualizedAreaChanged();
protected:
	virtual void resizeEvent(QResizeEvent * evt);
	virtual void onPlayerCreated();

	/// Reimplemented from #VipRenderObject::startRender()
	virtual void startRender(VipRenderState &);
	/// Reimplemented from #VipRenderObject::endRender()
	virtual void endRender(VipRenderState &);
	virtual void keyPressEvent(QKeyEvent* evt);
	QTransform computeImageTransform() const;

	/// Enable/disable zoom features (basically disable/enable content of visualizedAreaChanged() function)
	void setZoomFeaturesEnabled(bool enable);
	bool zoomFeaturesEnabled() const;

private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipVideoPlayer*)


/// Attribute set to a VipPlotCurve when extracting the shape time trace in order to retrieve the source ROI (in case the shape was moved)
struct VipSourceROI
{
	QPointer<VipVideoPlayer> player;
	QPolygonF polygon;
};
Q_DECLARE_METATYPE(VipSourceROI)



/// Create a copy of \a shape and returns it.
/// If \a src_player is not null, its image transform is removed to the output shape.
/// If \a dst_player is not null, its image transform is added to the output shape.
VIP_GUI_EXPORT VipShape vipCopyVideoShape(const VipShape & shape, const VipVideoPlayer * src_player, const VipVideoPlayer * dst_player);
/// Create a copy of \a sm and returns it.
/// If \a src_player is not null, its image transform is removed to the output scene model.
/// If \a dst_player is not null, its image transform is added to the output scene model.
VIP_GUI_EXPORT VipSceneModel vipCopyVideoSceneModel(const VipSceneModel & sm, const VipVideoPlayer * src_player, const VipVideoPlayer * dst_player);


class VipPlotPlayer;
/// High level function.
/// Extract the time trace of a shape from a video player, and display it in a new plot player inside the current display area.
VIP_GUI_EXPORT VipPlotPlayer* vipExtractTimeTrace(const VipShapeList & shapes, VipVideoPlayer * pl, VipShapeStatistics::Statistics stats = 0, int one_frame_out_of = 1, int multi_shapes = -1, VipPlotPlayer* out = NULL);

class VipPlotMarker;

/// A \a VipPlayer2D used to represent any kind of 2D plots
class VIP_GUI_EXPORT VipPlotPlayer : public VipPlayer2D
{
	Q_OBJECT

public:
	typedef VipValueToTime::TimeType(*function_type)(VipPlotPlayer *);

	VipPlotPlayer(VipAbstractPlotWidget2D* viewer = NULL, QWidget * parent = NULL);
	~VipPlotPlayer();

	///Returns the underlying #VipPlotWidget2D
	VipAbstractPlotWidget2D * viewer() const;
	///Returns a new, empty VipPlotPlayer
	VipPlotPlayer * createEmpty() const;

	VipAbstractScale* defaultXAxis() const;
	VipAbstractScale* defaultYAxis() const;
	VipCoordinateSystem::Type defaultCoordinateSystem() const;

	///Returns the #VipValueToTimeButton button in the tool bar used to modify the bottom time scale
	VipValueToTimeButton * valueToTimeButton() const;
	///Returns the time marker
	VipPlotMarker * timeMarker() const;

	///Returns a vertical marker that is used to display the tool tip position
	VipPlotMarker * xMarker() const;

	VipPlotShape* verticalWindow() const;

	/// Returns the tool bar button that displays a menu with advanced features (like normalize, start at 0, distance between points,...)
	QToolButton * advancedTools() const;

	//void setDisplayTimeAsInteger(bool);
	bool displayTimeAsInteger() const;



	///Returns the source #VipProcessingList for the selected #VipPlotItemData.
	/// Returns a NULL pointer if the VipProcessingList cannot be found, if there are several or no VipPlotItemData selected.
	VipProcessingList * currentProcessingList() const;
	///Returns the #VipPlotItemData that is currently selected.
	/// Returns NULL if there are several or no VipPlotItemData selected.
	VipPlotItemData * currentPlotItem() const;

	virtual QGraphicsObject *  defaultEditableObject() const;
	virtual QList<VipPlotSceneModel *> plotSceneModels() const;
	///Returns the main display object for this player
	virtual VipDisplayObject* mainDisplayObject() const;

	bool isNormalized() const;
	bool isStartAtZero() const;
	bool isStartYAtZero() const;
	bool gridVisible() const;
	bool legendVisible() const;
	bool isAutoScale() const;
	bool isHZoomEnabled() const;
	bool isVZoomEnabled() const;
	bool displayVerticalWindow() const;

	void setTimeType(VipValueToTime::TimeType);
	VipValueToTime::TimeType timeType() const;

	void setDisplayType(VipValueToTime::DisplayType type);
	VipValueToTime::DisplayType displayType() const;

	///Returns true is all plot item in this player are time based
	bool haveTimeUnit() const;
	QString timeUnit() const;
	QString formatXValue(vip_double value) const;
	///Returns the factor to convert x unit from the current time unit to nanoseconds
	qint64 timeFactor() const;

	///Returns the min/max x unit for all plot items combined
	VipInterval itemsInterval() const;

	QList<VipAbstractScale*> leftScales() const;
	VipAbstractScale* findYScale(const QString & title) const;
	VipAbstractScale* xScale() const;

	void setLegendPosition(Vip::PlayerLegendPosition pos);
	Vip::PlayerLegendPosition legendPosition() const;
	VipLegend * innerLegend() const;

	/// Remove a left scale.
	/// The left scale is removed only if it is not the last one.
	/// This function takes care of resetting (if necessary) the default VipPlotSceneModel
	/// and moves (if necessary) the different markers.
	/// On success, the scale is deleted and the function returns true.
	bool removeLeftScale(VipAbstractScale * scale);
	VipAbstractScale* addLeftScale();
	VipAbstractScale* addLeftScale(VipAbstractScale * scale);
	VipAbstractScale* insertLeftScale(int index);
	VipAbstractScale* insertLeftScale(int index, VipAbstractScale * scale);
	int leftScaleCount() const;

	///Reinplemented from #VipAbstractPlayer::setProcessingPool().
	/// Set a time marker (#VipPlotMarker) used to display the current time.
	virtual void setProcessingPool(VipProcessingPool * pool);

	static void setTimeUnitFunction(function_type fun);
	static function_type timeUnitFunction();

	void removeStyleSheet(VipPlotItem*);

	static void setNewItemBehaviorEnabled(bool);
	static bool newItemBehaviorEnabled();

public Q_SLOTS:
	///Show/hide the time marker
	void setTimeMarkerVisible(bool); //show/hide the time marker
	void normalize(bool);
	void startAtZero(bool);
	void startYAtZero(bool);
	void showGrid(bool);
	void showParameters();
	void autoScale();
	void setAutoScale(bool);
	void setDisplayVerticalWindow(bool);
	void resetVerticalWindow();
	void enableHZoom(bool);
	void enableVZoom(bool);
	void xScaleToAll();
	void yScaleToAll();
	void autoScaleX();
	void autoScaleY();
	void setTimeMarkerAlwaysVisible(bool enable);
	void removeStyleSheet();
	void poolTypeChanged();

private Q_SLOTS:
	void timeUnitChanged(); //update the bottom axis scale draw
	void timeChanged(); //update the time marker value
	void computeDeleteMenu();
	void deleteItem(QAction *);
	void computeSelectionMenu();
	void hideAllItems();
	void selectItem(QAction *);
	void addSelectedProcessing(const VipProcessingObject::Info &);
	void histBinsChanged(int value);
	void updateCurveEditor();
	void updateSlidingTimeWindow();
	void setSlidingTimeWindow();
	void computeStartDate();
	void delayedComputeStartDate();
	void toolTipStarted(const QPointF & pos);
	void toolTipMoved(const QPointF & pos);
	void toolTipEnded(const QPointF & pos);
	///Update the tool tip when the image content changes
	void refreshToolTip(VipPlotItem*);
	void computeZoom();
	void undoMenuShow();
	void legendTriggered();
Q_SIGNALS:
	void timeUnitChanged(const QString &);

protected:
	virtual void plotItemAdded(VipPlotItem *);
	virtual void plotItemRemoved(VipPlotItem *);
	virtual void plotItemSelectionChanged(VipPlotItem*);
	virtual void plotItemAxisUnitChanged(VipPlotItem *);
	virtual void computePlayerTitle();
	virtual void onPlayerCreated();

	// Reimplemented to change current processing pool time
	virtual bool plotItemClicked(VipPlotItem*, VipPlotItem::MouseButton);

	/// Reimplemented from #VipRenderObject::startRender()
	virtual void startRender(VipRenderState &);
	/// Reimplemented from #VipRenderObject::endRender()
	virtual void endRender(VipRenderState &);

private:
	class PrivateData;
	PrivateData * m_data;
};

VIP_REGISTER_QOBJECT_METATYPE(VipPlotPlayer*)






/// This function dispatcher is called every time an \a VipAbstractPlayer instance is created.
/// Its signature is: void(VipAbstractPlayer*);
VIP_GUI_EXPORT VipFunctionDispatcher<1> & vipFDPlayerCreated();

/// This function dispatcher is called every time a #VipPlotItem is added to an #VipAbstractPlayer.
/// Its signature is void(VipPlotItem*, VipAbstractPlayer*);
VIP_GUI_EXPORT VipFunctionDispatcher<2> & VipFDItemAddedOnPlayer();

/// This function dispatcher is called every time a #VipPlotItem is removed from an #VipAbstractPlayer.
/// Its signature is void(VipPlotItem*, VipAbstractPlayer*);
/// Do not use the VipPlotItem since it might already destroyed.
VIP_GUI_EXPORT VipFunctionDispatcher<2> & VipFDItemRemovedFromPlayer();

/// This function dispatcher is called every time a #VipPlotItem's axis unit changes.
/// Its signature is void(VipPlotItem*, VipAbstractPlayer*);
VIP_GUI_EXPORT VipFunctionDispatcher<2> & VipFDItemAxisUnitChanged();

/// This function dispatcher is called every time a #VipPlotItem's selection changes.
/// Its signature is void(VipPlotItem*, VipAbstractPlayer*);
VIP_GUI_EXPORT VipFunctionDispatcher<2> & VipFDItemSelected();

/// This function dispatcher is called every time the user right click on a #VipPlotItem.
/// It is used to generate the entries of a contextual menu.
/// Its signature is QList<QAction*>(VipPlotItem*, VipAbstractPlayer*);
VIP_GUI_EXPORT VipFunctionDispatcher<2>& VipFDItemRightClick();

/// This function dispatcher is called every time the user open the processing menu or the data fusion processing menu.
/// The dispatcher is called for every action in the menu. If a function is found (and called), the action won't
/// be triggered if clicked and won't be draggable. This a useful way to override the processing menu behavior.
/// Its signature is bool(QAction*, VipAbstractPlayer*);
VIP_GUI_EXPORT VipFunctionDispatcher<2>& VipFDAddProcessingAction();



/// This function dispatcher is called whenever the user drop a QMimeData on a VipPlayer2D which cannot be handled by the standard drag & drop mechanism.
/// If a valid action is performed, the function must return true.
/// Its signature is bool(VipPlayer2D*, VipPlotItem*,QMimeData*);
VIP_GUI_EXPORT VipFunctionDispatcher<3>& VipFDDropOnPlotItem();


typedef QList<QAction*> ActionList;
Q_DECLARE_METATYPE(ActionList)

/// @}
//end Gui

#endif
