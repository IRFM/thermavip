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

#ifndef VTK_3D_PLAYER_H
#define VTK_3D_PLAYER_H

#include <QMimeData>
#include <QPointer>
#include <QSplitter>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidgetItem>
#include <qlabel.h>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>

#include "VipVTKImage.h"
#include "VipVTKDevices.h"

#include "VipToolWidget.h"
#include "VipColorMap.h"
#include "VipStandardWidgets.h"
#include "VipPlayer.h"
#include "VipDisplayVTKObject.h"
#include "VipOptions.h"

class QTreeWidget;
class QListWidget;
class QLabel;
class QPushButton;
class QSpinBox;
class QSlider;
class QToolButton;
class VipVTKWidget;
class VipVTKGraphicsView;
class VipVTKPlayer;
class VipPlotSpectrogram;
class VipDisplayFieldOfView;
class VipFOVSequence;
class OffscreenMappingToInputData;
class VipVideoPlayer;

/// @brief Image mapping dialog box
class VIP_GUI_EXPORT ApplyMappingDialog : public QDialog
{
	Q_OBJECT

public:
	ApplyMappingDialog(QWidget* parent = nullptr);
	~ApplyMappingDialog();

	void SetVideoPlayer(VipVideoPlayer*);
	VipVideoPlayer* VideoPlayer() const;

	void SetConstructNewObject(bool);
	bool ConstructNewObject() const;

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

///@brief QTreeWidgetItem representing a VipFieldOfView.
/// Should be created using VipFOVTreeWidget::addFieldOfView().
///
class VIP_GUI_EXPORT VipFOVItem
  : public QObject
  , public QTreeWidgetItem
{
	Q_OBJECT

public:
	/// @brief Construct from rendering VipVTKGraphicsView and parent item
	VipFOVItem(VipVTKGraphicsView* v, QTreeWidgetItem* parent);
	~VipFOVItem();

	/// @brief Remove and destroy all created objects (FOV pyramid, optical axis,...)
	void clear();

	VipVTKGraphicsView* view() const;

	bool FOVPyramidVisible() const;

	/// @brief Set the sink VipDisplayFieldOfView that displays the FOV.
	/// This will recompute the FOV path (if the source is a VipFOVSequence)
	void setPlotFov(VipPlotFieldOfView*);
	VipPlotFieldOfView* plotFov() const;

	VipDisplayFieldOfView* display() const;

	/// @brief Returns the source VipFOVSequence for this FOV.
	/// If the unique VipIODevice source (as returned by uniqueSource()) is a VipFOVSequence, just return it.
	/// However, if the source is a FOVFileReader, try to find the internal VipFOVSequence and return it.
	/// If the source is a VipDirectoryReader, try to find the internal VipFOVSequence and return it.
	/// Otherwise, return nullptr.
	VipFOVSequence* source() const;

	/// @brief Returns the internal mapping object is created
	OffscreenMappingToInputData* mapping() const;

	/// @brief Returns the FOV pyramid
	VipPlotVTKObject* FOVPyramid();
	/// @brief Returns the FOV optical axis
	VipPlotVTKObject* opticalAxis();

	void applyMapping(bool enable, bool createNewObject = false, VipPlotSpectrogram* spec = nullptr, bool remove_mapped_data = true);

public Q_SLOTS:

	void resetPyramid();
	void moveCamera();
	void showFOVPyramid(bool);
	void showVisiblePointsInFOVPyramid(bool);
	void applyImageMapping(bool);
	void importCamera();
	void saveSpatialCalibrationFile();
	void displaySpatialCalibration();

private Q_SLOTS:
	void newData();
	void updateColor();

private:
	/// @brief Set the field of view associated with this item.
	/// This will recompute the FOV pyramid and optical axis.
	void setFieldOfView(const VipFieldOfView&);
	void buildPyramid();
	OffscreenMappingToInputData* buildMapping(bool createNewObject, VipOutput* image);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Widget displaying a list of VipFieldOfView as a QTreeWidget.
/// VipFieldOfView items are displayed inside a VipVTKGraphicsView.
///
///
class VIP_GUI_EXPORT VipFOVTreeWidget : public QWidget
{
	Q_OBJECT
	friend class TreeWidget;

public:
	VipFOVTreeWidget(VipVTKGraphicsView* view, QWidget* parent = nullptr);
	~VipFOVTreeWidget();

	VipVTKGraphicsView* view() const;
	QTreeWidget* tree() const;

	VipFOVItem* addFieldOfView(VipPlotFieldOfView*);
	void removeFieldOfView(VipFOVItem* item);

	VipFOVItem* currentFieldOfViewItem() const;
	VipFOVItem* fieldOfViewItem(const VipPlotFieldOfView* display) const;
	VipFOVItem* fieldOfViewItem(const QString& name) const;
	QList<VipFOVItem*> fieldOfViewItems() const;
	QList<VipFOVItem*> selectedFieldOfViewItems() const;
	QStringList visibleFOVPyramidNames() const;

	VipFieldOfViewList fieldOfViews() const;
	VipFieldOfViewList selectedFieldOfViews() const;
	QList<VipPlotFieldOfView*> plotFieldOfViews() const;
	QList<VipPlotFieldOfView*> selectedPlotFieldOfViews() const;
	QList<VipDisplayFieldOfView*> displayObjects();

	const VipColorPalette& itemColorPalette() const;
	void setItemColorPalette(const VipColorPalette&);

	virtual bool eventFilter(QObject* watched, QEvent* evt);

public Q_SLOTS:
	/// @brief Remove all items
	void clear();
	/// @brief Load a field of view xml file
	void loadFOVFile();
	/// @brief Load a field of view xml file
	void loadFOVFile(const QString& filename);
	/// @brief Delete selected items
	void deleteSelection();
	/// @brief Save selected items
	void saveSelection();
	/// @brief Save an image for this FOV with current attributes
	void saveAttributeFieldOfView();
	/// @brief Edit FOV items
	void edit();
	/// @brief Create a new FOV
	void create();
	/// @brief Reset camera view
	void resetCamera();
	void computeOverlapping(bool);
	void duplicateSelection();

private Q_SLOTS:
	void itemDoubleClicked();
	void itemPressed(QTreeWidgetItem* item, int column);
	void saveSpatialCalibrationFile();
	void displaySpatialCalibration();
	void dataChanged();
	void dataChangedInternal();
	void selectionChanged();
	void plotItemAdded(VipPlotItem*);
	void plotItemRemoved(VipPlotItem*);

protected:
	VipPlotSpectrogram* acceptDrop(const QMimeData*);
	virtual void dragMoveEvent(QDragMoveEvent* event);
	virtual void dragEnterEvent(QDragEnterEvent* event);
	virtual void dropEvent(QDropEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void keyPressEvent(QKeyEvent* evt);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Item representing a VipVTKObject in a QTeeWidget
class VIP_GUI_EXPORT VipVTKObjectItem
  : public QObject
  , public QTreeWidgetItem
{
	Q_OBJECT
	friend class VipVTKObjectTreeWidget;

public:
	enum State
	{
		Visible,
		Hidden
	};

	VipVTKObjectItem(QTreeWidgetItem* parent);

	/// @brief Set VipVTKObject object
	void setPlotObject(VipPlotVTKObject* obj);
	VipPlotVTKObject* plotObject() const;

	/// @brief Returns the opacity slider
	QSlider* opacitySlider();

	/// @brief Returns the children VipVTKObject objects
	PlotVipVTKObjectList children() const;

	void setVisibility(State state);
	void setDrawEdge(State state);
	void setOpacity(double opacity);

	void setText(const QString& str);
	QString text() const;

protected Q_SLOTS:

	void visibilityChanged();
	void drawEdgeChanged();
	void opacityChanged();
	void updateItem();

private:
	void visibilityChanged(VipVTKObjectItem* item, State state);
	void drawEdgeChanged(VipVTKObjectItem* item, State state);
	void opacityChanged(VipVTKObjectItem* item, double opacity);
	void updateView();

	// check if the internal data is file and name correspond to the item location
	bool isSync() const;

	QPointer<VipPlotVTKObject> plot;
	QToolBar* toolBar;
	QLabel* label;
	QAction* visibility;
	QAction* drawEdge;
	QAction* opacity;
	State visible;
	State edge;
	QString name;

public:
};

/// @brief Tree widget that represents all VipVTKObject displayed in a VipVTKGraphicsView.
///
/// The tree widget represents VipVTKObject as tree items stored in 2 top level parent:
/// one for file objects (VipVTKObject::isFileData() is true) and one for in-memory (temporary) objects
/// (VipVTKObject::isFileData() is false).
///
/// The tree is automatically synchronized with the VipVTKGraphicsView widget.
/// Selecting an item in the tree will select the underlying VipVTKObject.
///
class VIP_GUI_EXPORT VipVTKObjectTreeWidget : public QWidget
{
	Q_OBJECT

	void findInItem(VipVTKObjectItem* item, PlotVipVTKObjectList& lst) const;
	VipVTKObjectItem* findInfo(VipVTKObjectItem* item, const QFileInfo& path) const;

public:
	VipVTKObjectTreeWidget(VipVTKGraphicsView* view, QWidget* parent = nullptr);
	~VipVTKObjectTreeWidget();

	/// @brief Returns the VipVTKGraphicsView view
	VipVTKGraphicsView* view();

	int maxDepth() const;

	/// @brief Add a VipVTKObject to the tree, and add it as well to the VipVTKGraphicsView
	void addObject(const PlotVipVTKObjectList&);
	void addObject(VipPlotVTKObject*, const QString& name = QString());

	/// @brief Retrieve a VipVTKObject based on its name.
	/// Search first in file objects, then in memory objects.
	VipPlotVTKObject* objectByName(const QString& name);

	/// @brief Remove a VipVTKObject object from the tree and the VipVTKGraphicsView
	bool remove(VipPlotVTKObject*);
	/// @brief Remove a VipVTKObject object from the tree and the VipVTKGraphicsView.
	/// Only remove the first object with given name in file objects first,
	/// and memory objects then if not found in file objects.
	bool remove(const QString& name);

	/// @brief Returns all objects
	PlotVipVTKObjectList objects() const;
	/// @brief Returns selected objects only (in both file and in-memory parent)
	PlotVipVTKObjectList selectedObjects() const;

	/// @brief Find a VipVTKObjectItem based on provided name. Create the item if not found and create_if_needed is true.
	VipVTKObjectItem* find(QTreeWidgetItem* root, const QString& path, bool create_if_needed = true);

	virtual bool eventFilter(QObject* watched, QEvent* evt);

public Q_SLOTS:

	void setMaxDepth(int);

	/// @brief synchronize the VipVTKObjectTreeWidget with its VipVTKGraphicsView.
	/// Periodically called by the VipVTKObjectTreeWidget.
	void synchronize();

	void resynchronize();

	void resetSelection();

	/// @brief Clear the tree. The tree will be populated again in the next call to synchronize().
	void clear();

	/// @brief Unselect all items
	void unselectAll();

	/// @brief Expand all items
	void expandAll();

	/// @brief Save selected item in file.
	void saveInFile();
	/// @brief Save selected items in directory. Might keep the tree structure.
	void saveInDir();
	/// @brief Remove selected items from the tree and the VipVTKGraphicsView.s
	void deleteSelection();
	void copySelection();
	/// @brief Hide all items but the selected ones.
	void hideAllButSelection();
	/// @brief Show all items but the selected ones.
	void showAllButSelection();
	/// @brief Show all items
	void showAll();

	/// @brief Save the XYZ points of selected items in a text file
	void saveXYZ();

	/// @brief Save the XYZ points with attribute values in a text or CSV file
	void saveAttributeXYZValue();

private Q_SLOTS:

	bool cleanItem(VipVTKObjectItem* item);
	void itemPressed(QTreeWidgetItem* item, int column);
	void selectionChanged();
	// check if the internal data is file and name correspond to the each item location
	bool isSync();
	void synchronizeInternal(bool force);

protected:
	virtual void mousePressEvent(QMouseEvent* e);
	virtual void keyPressEvent(QKeyEvent* evt);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Small widget used to select attributes to save from a VipVTKPlayer
///
class VipXYZValueWidget : public QWidget
{
	Q_OBJECT

	QLabel fieldAttributes;
	QLabel pointAttributes;
	QList<QCheckBox*> fieldBoxes;
	QList<QCheckBox*> pointBoxes;

public:
	using Attribute = VipXYZAttributesWriter::Attribute;
	using AttributeList = QList<Attribute>;

	VipXYZValueWidget(QWidget* parent = nullptr);

	void SetDataObjects(const VipVTKObjectList& lst);
	AttributeList SelectedAttributes() const;

private Q_SLOTS:
	void Checked(bool);
};

/// @brief Small widget used to select which fielf/point/cell attribute to display using a dedicated legend
/// (see VipVTKGraphicsView::annotationLegend()) or the VipVTKGraphicsView color scale.
///
/// The widget also provides tools to add/remove attributes.
///
/// Use VipSelectDisplayedAttributeWidget::setDisplayedAttribute() to programatically select which attribute to display.
///
class VIP_GUI_EXPORT VipSelectDisplayedAttributeWidget : public QToolBar
{
	Q_OBJECT

public:
	/// @brief Construct from a VipVTKGraphicsView
	VipSelectDisplayedAttributeWidget(VipVTKGraphicsView* view, QWidget* parent = nullptr);
	~VipSelectDisplayedAttributeWidget();

	/// @brief Set attribute to display
	/// @param type attribute type: Field, Point or Cell
	/// @param attribute attribute name, or "None" for no attribute
	/// @param component component for multi component attributes
	/// @param update_widget update this widget
	/// @return true on success
	bool setDisplayedAttribute(VipVTKObject::AttributeType type, const QString& attribute, int component, bool update_widget = true);
	/// @brief Unset currently displayed attribute
	void unsetDisplayedAttribute() { setDisplayedAttribute(VipVTKObject::Unknown, "None", 0); }
	/// @brief Returns true if an attribute is currently displayed
	bool isDisplayingAttribute() const;

	/// @brief Returns displayed attribute type
	VipVTKObject::AttributeType currentAttributeType() const;
	/// @brief Returns displayed attribute name
	QString currentAttribute() const;
	/// @brief Returns displayed attribute component
	int currentComponent() const;

	/// @brief Returns the list of all available point attributes names
	QStringList pointsAttributes() const;
	/// @brief Returns the list of all available cell attributes names
	QStringList cellsAttributes() const;
	/// @brief Returns the list of all available field attributes names
	QStringList fieldAttributes() const;

	// For session loading only
	void setPendingDisplayedAttribute(VipVTKObject::AttributeType type, const QString& name, int comp);

public Q_SLOTS:
	void updateContent();

private Q_SLOTS:

	void DisplaySelectedAttribue(bool);
	void DeleteSelectedAttribue();
	void CreatePointsAttribute();
	void CreateInterpolatedPointsAttribute();
	void CreateAttribute();

	void dataChanged();
	void selectionChanged();
	void AttributeSelectionChanged();

private:
	void ClearAnnotations();
	void DisplaySelectedAnnotatedAttribue(bool display);
	void DisplaySelectedScalarAttribue(bool display);

	QString TypeToString(VipVTKObject::AttributeType) const;
	VipVTKObject::AttributeType StringToType(const QString&) const;
	void UpdateAttributeWidgets(VipVTKObject::AttributeType, const QString& attr = "None", int comp = 0);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VIP_GUI_EXPORT VipVTKPlayerToolWidget : public VipToolWidgetPlayer
{
	Q_OBJECT

public:
	VipVTKPlayerToolWidget(VipMainWindow* parent = nullptr);
	virtual bool setPlayer(VipAbstractPlayer*);

private:
	QPointer<VipAbstractPlayer> m_player;
};

VIP_GUI_EXPORT VipVTKPlayerToolWidget* vipGetVTKPlayerToolWidget(VipMainWindow* parent = nullptr);



class VIP_GUI_EXPORT VipCubeAxesActorWidget : public QWidget
{
	Q_OBJECT

public:
	VipCubeAxesActorWidget(QWidget* parent = nullptr);
	~VipCubeAxesActorWidget();

	void setView(VipVTKGraphicsView* actor);
	VipVTKGraphicsView* view();

private Q_SLOTS:
	void updateWidget();
	void updateActor();

private:
	void connectWidget(QWidget* src, const char* signal);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};



/// @brief Global display options for all VipVTKPlayer
class VIP_GUI_EXPORT VipVTKPlayerOptions
{
public:
	bool lighting = true;	// Add lighting by default to all VipVTKPlayer
	bool orientationWidget = true; // Show orientation widget by default to all VipVTKPlayer
	bool showHideFOVItems = false; // Show cameras in 3D object browser by default

	void save(VipArchive&) const;
	void restore(VipArchive&);

	static void set(const VipVTKPlayerOptions&);
	static const VipVTKPlayerOptions& get();
};


/// @brief Player class for representing VTK 3D objects.
///
/// Inherits VipVideoPlayer to provide a color scale and some
/// video related features (like image superimposition).
///
/// The inner viewport is a VipVTKGraphicsView. The top toolbar
/// contains a VipSelectDisplayedAttributeWidget for attribute
/// selection.
/// The left area contains a VipVTKObjectTreeWidget as well as a
/// VipFOVTreeWidget to display the list of displayed 3D objects
/// as well as the list of Fields Of View.
///
class VIP_GUI_EXPORT VipVTKPlayer : public VipVideoPlayer
{
	Q_OBJECT

public:
	VipVTKPlayer(QWidget* parent = nullptr);
	~VipVTKPlayer();

	static VipVTKPlayer* fromChild(QWidget* w) noexcept
	{
		while (w) {
			if (qobject_cast<VipVTKPlayer*>(w))
				return static_cast<VipVTKPlayer*>(w);
			w = w->parentWidget();
		}
		return nullptr;
	}

	VipVTKGraphicsView* view() const;
	VipVTKObjectTreeWidget* tree() const;
	VipFOVTreeWidget* fov() const;
	QSplitter* verticalSplitter() const;
	QWidget* leftWidget() const;
	VipSelectDisplayedAttributeWidget* attributes() const;
	VipCubeAxesActorWidget* cubeAxesActorEditor() const;

	virtual QSize sizeHint() const;
	virtual QList<VipDisplayObject*> displayObjects() const;
	void setProcessingPool(VipProcessingPool* pool);

	virtual void showAxes(bool);
	bool isSharedCamera() const;
	bool isAutoCamera() const;

	// For session saving/loading only
	void setPendingVisibleFOV(const QStringList& names);
	void setPendingCurrentCamera(const VipFieldOfView& fov);
	void setPendingVisibleAttribute(VipVTKObject::AttributeType type, const QString& name, int comp);
	void applyThisCameraToAll();

public Q_SLOTS:

	void setAutoCamera(bool);
	void setSharedCamera(bool enable);
	void loadCadDirectory();
	void loadCadFiles();
	void setTrackingEnable(bool);
	void saveImage();
	void setLegendVisible(bool visible);
	void setOrientationMarkerWidgetVisible(bool);
	void setAxesVisible(bool);
	void setLighting(bool);
	
private Q_SLOTS:
	void timeChanged(qint64);
	void applyDelayedPendingActions(const VipFieldOfView& fov);
	void cameraChanged();
	void applyPendingActions();

protected:
	virtual void startRender(VipRenderState& state);
	virtual void endRender(VipRenderState& state);

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipVTKPlayer*)


/// @brief Options for VTK players and 3D objects display
class VIP_GUI_EXPORT VipVTKPlayerOptionPage : public VipPageOption
{
	Q_OBJECT

public:
	VipVTKPlayerOptionPage(QWidget* parent = nullptr);
	~VipVTKPlayerOptionPage();

	virtual void applyPage();
	virtual void updatePage();

private:
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

#endif
