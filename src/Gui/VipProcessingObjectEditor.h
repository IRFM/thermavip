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

#ifndef VIP_PROCESSING_OBJECT_EDITOR_H
#define VIP_PROCESSING_OBJECT_EDITOR_H

#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qspinbox.h>
#include <qtoolbutton.h>

#include "VipExtractStatistics.h"
#include "VipFunctional.h"
#include "VipImageProcessing.h"
#include "VipMapFileSystem.h"
#include "VipPlotItem.h"
#include "VipProcessingObject.h"
#include "VipStandardProcessing.h"
#include "VipStandardWidgets.h"
#include "VipToolWidget.h"
#include "VipWarping.h"
#include "VipPimpl.h"

/// \addtogroup Gui
/// @{

class VipTextFileReader;
class VipTextFileWriter;
class VipImageWriter;
class VipDirectoryReader;
class VipProcessingList;
class VipExtractComponent;
class VipProcessingObject;
class VipIODevice;
class VipPlotItem;
class VipDisplayImage;
class VipDisplayCurve;
class VipOperationBetweenPlayers;
class VipConvert;
class VipCSVWriter;
class VipPlayer2D;

/// @brief Combobox used to select a player among all available players
/// within the current workspace
class VIP_GUI_EXPORT VipPlayerSelector : public VipComboBox
{
	Q_OBJECT

public:
	VipPlayerSelector(QWidget * parent = nullptr);
	~VipPlayerSelector();

	// Filter player list based on metaobject
	void setPlayerMetaObject(const QMetaObject*);
	const QMetaObject* playerMetaObject() const;

	// set/get the player
	void setPlayer(VipPlayer2D* pl);
	VipPlayer2D* player() const;


private Q_SLOTS:
	// populate all players within the current workspace
	void populatePlayers();

private:
	const QMetaObject* d_meta = nullptr;
};

/// Widget to edit a VipOtherPlayerData
class VIP_GUI_EXPORT VipOtherPlayerDataEditor : public QWidget
{
	Q_OBJECT
public:
	VipOtherPlayerDataEditor();
	~VipOtherPlayerDataEditor();
	void setValue(const VipOtherPlayerData& data);
	VipOtherPlayerData value() const;

	// void displayVLines(bool before, bool after);

public Q_SLOTS:
	void apply();

private Q_SLOTS:
	void showPlayers();
	void showDisplays();

Q_SIGNALS:
	void valueChanged(const QVariant&);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};


/// @brief Edit a data which either a 2D array manually edited (through VipPyArrayEditor),
/// or a data comming from another player (through VipOtherPlayerDataEditor)
class VIP_GUI_EXPORT Vip2DDataEditor : public QWidget
{
	Q_OBJECT
public:
	Vip2DDataEditor();
	~Vip2DDataEditor();

	VipOtherPlayerData value() const;
	void setValue(const VipOtherPlayerData& data);
	void displayVLines(bool before, bool after);

private Q_SLOTS:
	void emitChanged() { Q_EMIT changed(); }
Q_SIGNALS:
	void changed();

private:
	VIP_DECLARE_PRIVATE_DATA();
};


/// Tool button that displays a menu to select a VipDisplayObject input (final processing data)
class VIP_GUI_EXPORT VipFindDataButton : public QToolButton
{
	Q_OBJECT

public:
	VipFindDataButton(QWidget* parent = nullptr);
	~VipFindDataButton();

	VipOutput* selectedData() const;
	void setSelectedData(VipOutput*);

Q_SIGNALS:
	void selectionChanged(VipProcessingObject* p, int output);

protected:
	virtual void resizeEvent(QResizeEvent* evt);

private Q_SLOTS:
	void menuShow();
	void menuSelected(QAction*);
	void elideText();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Edit a VipBaseDataFusion processing.
/// This widget displays a VipUniqueProcessingObjectEditor for the processing
/// and a list of editable inputs.
class VIP_GUI_EXPORT VipEditDataFusionProcessing : public QWidget
{
	Q_OBJECT

public:
	VipEditDataFusionProcessing(QWidget* parent = nullptr);
	~VipEditDataFusionProcessing();

	void setDataFusionProcessing(VipBaseDataFusion*);
	VipBaseDataFusion* dataFusionProcessing() const;

private Q_SLOTS:
	void updateProcessing();
	void addInput();
	void removeInput();

private:
	int indexOfInput(QObject*);
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipProcessingObjectEditor
class VIP_GUI_EXPORT VipProcessingObjectEditor : public QWidget
{
	Q_OBJECT

public:
	VipProcessingObjectEditor();
	~VipProcessingObjectEditor();
	void setProcessingObject(VipProcessingObject* obj);

private Q_SLOTS:
	void updateProcessingObject();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipIODeviceEditor
class VIP_GUI_EXPORT VipIODeviceEditor : public QWidget
{
	Q_OBJECT

public:
	VipIODeviceEditor();
	~VipIODeviceEditor();
	void setIODevice(VipIODevice* obj);

private Q_SLOTS:
	void updateIODevice();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class QListWidgetItem;
class QListWidget;
class VipUniqueProcessingObjectEditor;

/// Widget to edit a VipProcessingList
class VIP_GUI_EXPORT VipProcessingListEditor : public QWidget
{
	Q_OBJECT
	friend class ListWidget;

public:
	VipProcessingListEditor();
	~VipProcessingListEditor();

	void setProcessingList(VipProcessingList* lst);
	VipProcessingList* processingList() const;
	VipUniqueProcessingObjectEditor* editor() const;
	QToolButton* addProcessingButton() const;
	QListWidget* list() const;

	void addProcessings(const QList<VipProcessingObject::Info>&);
	void selectObject(VipProcessingObject* obj);
	void setProcessingVisible(VipProcessingObject* obj, bool visible);
	void clearEditor();

	QListWidgetItem* item(VipProcessingObject* obj) const;

	static QString copiedItems();
	static QList<VipProcessingObject*> copiedProcessing();

public Q_SLOTS:
	void copySelection();
	void cutSelection();
	void pasteCopiedItems();

Q_SIGNALS:
	void selectionChanged(VipUniqueProcessingObjectEditor* ed);

private Q_SLOTS:
	void itemChanged(QListWidgetItem* item);
	void updateProcessingList();
	void addSelectedProcessing();
	void updateProcessingTree();
	void selectedItemChanged();
	void resetProcessingList();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VipSplitAndMergeEditor : public QWidget
{
	Q_OBJECT

public:
	VipSplitAndMergeEditor(QWidget* parent = nullptr);
	~VipSplitAndMergeEditor();

	void setProcessing(VipSplitAndMerge* proc);
	VipSplitAndMerge* processing() const;

private Q_SLOTS:
	void newMethod(QAction* act);
	void receiveSelectionChanged(VipUniqueProcessingObjectEditor* ed);
	void computeMethods();
Q_SIGNALS:
	void selectionChanged(VipUniqueProcessingObjectEditor* ed);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class QComboBox;
/// Widget to edit a VipExtractComponent
class VIP_GUI_EXPORT VipExtractComponentEditor : public QWidget
{
	Q_OBJECT

public:
	VipExtractComponentEditor();
	~VipExtractComponentEditor();

	void setExtractComponent(VipExtractComponent*);
	int componentCount() const;

	QComboBox* choices() const;

public Q_SLOTS:
	void updateComponentChoice();

private Q_SLOTS:
	void updateExtractComponent();
	void extractComponentChanged();

Q_SIGNALS:
	void componentChanged(const QString&);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipConvert
class VIP_GUI_EXPORT VipConvertEditor : public QWidget
{
	Q_OBJECT

public:
	VipConvertEditor();
	~VipConvertEditor();

	void setConvert(VipConvert*);
	VipConvert* convert() const;

	QComboBox* types() const;

private Q_SLOTS:
	void updateConversion();
	void conversionChanged();

Q_SIGNALS:
	void conversionChanged(int);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipDisplayImage.
///
class VIP_GUI_EXPORT VipDisplayImageEditor : public QWidget
{
	Q_OBJECT

public:
	VipDisplayImageEditor();
	~VipDisplayImageEditor();

	void setDisplayImage(VipDisplayImage*);
	VipDisplayImage* displayImage() const;

	VipExtractComponentEditor* editor() const;

private Q_SLOTS:
	void updateDisplayImage();
	void updateExtractorVisibility();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipDisplayCurve.
///
class VIP_GUI_EXPORT VipDisplayCurveEditor : public QWidget
{
	Q_OBJECT

public:
	VipDisplayCurveEditor();
	~VipDisplayCurveEditor();

	void setDisplay(VipDisplayCurve*);
	VipDisplayCurve* display() const;

private Q_SLOTS:
	void updateDisplayCurve();
	void updateExtractorVisibility();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipSwitch
class VIP_GUI_EXPORT VipSwitchEditor : public QWidget
{
	Q_OBJECT

public:
	VipSwitchEditor();
	~VipSwitchEditor();

	void setSwitch(VipSwitch*);

public Q_SLOTS:
	void updateSwitch();
	void resetSwitch();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipClamp
class VIP_GUI_EXPORT VipClampEditor : public QWidget
{
	Q_OBJECT

public:
	VipClampEditor();
	~VipClampEditor();

	void setClamp(VipClamp*);
	VipClamp* clamp() const;

public Q_SLOTS:
	void updateClamp();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipTextFileReader
class VIP_GUI_EXPORT VipTextFileReaderEditor : public QWidget
{
	Q_OBJECT
public:
	VipTextFileReaderEditor();
	~VipTextFileReaderEditor();
	void setTextFileReader(VipTextFileReader*);

private Q_SLOTS:
	void updateTextFileReader();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipTextFileWriter
class VIP_GUI_EXPORT VipTextFileWriterEditor : public QWidget
{
	Q_OBJECT
public:
	VipTextFileWriterEditor();
	~VipTextFileWriterEditor();
	void setTextFileWriter(VipTextFileWriter*);

private Q_SLOTS:
	void updateTextFileWriter();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipImageWriter
class VIP_GUI_EXPORT VipImageWriterEditor : public QWidget
{
	Q_OBJECT
public:
	VipImageWriterEditor();
	~VipImageWriterEditor();
	void setImageWriter(VipImageWriter*);

private Q_SLOTS:
	void updateImageWriter();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VIP_GUI_EXPORT VipCSVWriterEditor : public QWidget
{
	Q_OBJECT
public:
	VipCSVWriterEditor();
	~VipCSVWriterEditor();
	void setCSVWriter(VipCSVWriter*);

private Q_SLOTS:
	void updateCSVWriter();
	void updateWidgets();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget to edit a VipDirectoryReader
class VIP_GUI_EXPORT VipDirectoryReaderEditor : public QWidget
{
	Q_OBJECT
public:
	VipDirectoryReaderEditor();
	~VipDirectoryReaderEditor();
	void setDirectoryReader(VipDirectoryReader*);

public Q_SLOTS:
	void apply();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VipConcatenateVideos;

  /// Widget to open a VipConcatenateVideos
class VIP_GUI_EXPORT VipConcatenateVideosOpenEditor : public QWidget
{
	Q_OBJECT
public:
	VipConcatenateVideosOpenEditor();
	~VipConcatenateVideosOpenEditor();
	void setDevice(VipConcatenateVideos*);

public Q_SLOTS:
	void apply();

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Widget to edit an already open VipConcatenateVideos
class VIP_GUI_EXPORT VipConcatenateVideosEditor : public QToolBar
{
	Q_OBJECT
public:
	VipConcatenateVideosEditor();
	~VipConcatenateVideosEditor();
	void setDevice(VipConcatenateVideos*);
	VipConcatenateVideos* device() const;

public Q_SLOTS:
	void removeCurrentDevice();
	void undo();
	void redo();

private Q_SLOTS:
	void updateIcons();

private:
	VIP_DECLARE_PRIVATE_DATA();
};





/// Widget to edit a VipOperationBetweenPlayers
class VIP_GUI_EXPORT VipOperationBetweenPlayersEditor : public QWidget
{
	Q_OBJECT
public:
	VipOperationBetweenPlayersEditor();
	~VipOperationBetweenPlayersEditor();
	void setProcessing(VipOperationBetweenPlayers*);

public Q_SLOTS:
	void apply();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VIP_GUI_EXPORT VipCropEditor : public QWidget
{
	Q_OBJECT

	QPointer<VipImageCrop> m_crop;
	QLineEdit start;
	QLineEdit end;
	QSpinBox shape;
	QToolButton apply;

public:
	VipCropEditor();
	void setCrop(VipImageCrop* th);

public Q_SLOTS:
	void updateCrop();
};

class VIP_GUI_EXPORT VipResizeEditor : public QWidget
{
	Q_OBJECT

	QPointer<VipResize> m_resize;
	QLineEdit shape;
	QComboBox interp;

public:
	VipResizeEditor();
	void setResize(VipResize* th);

public Q_SLOTS:
	void updateResize();
};

class VIP_GUI_EXPORT VipGenericImageTransformEditor : public QWidget
{
	Q_OBJECT
public:
	VipGenericImageTransformEditor();
	~VipGenericImageTransformEditor();

	void setProcessing(VipGenericImageTransform*);
	VipGenericImageTransform* processing() const;

	// virtual bool eventFilter(QObject* obj, QEvent * evt);

public Q_SLOTS:

	void updateProcessing();
	void updateWidget();

	void addTransform(Transform::TrType type);
	void addTranslation();
	void addScaling();
	void addRotation();
	void addShear();
	void removeSelectedTransform();
	void recomputeSize();

protected:
	// virtual void mousePressEvent(QMouseEvent * evt);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VIP_GUI_EXPORT VipComponentLabellingEditor : public QCheckBox
{
	Q_OBJECT

	QPointer<VipComponentLabelling> m_ccl;

public:
	VipComponentLabellingEditor()
	  : QCheckBox()
	{
		this->setText("Connectivity 8");
		connect(this, SIGNAL(clicked(bool)), this, SLOT(updateComponentLabelling()));
	}

	void setComponentLabelling(VipComponentLabelling* th)
	{
		if (th) {
			m_ccl = th;
			this->setChecked(th->connectivity8());
		}
	}

public Q_SLOTS:

	void updateComponentLabelling()
	{
		if (m_ccl) {
			m_ccl->setConnectivity8(this->isChecked());
			m_ccl->reload();
		}
	}
};

class PlotWarpingPoints;
class VipVideoPlayer;
class VIP_GUI_EXPORT VipWarpingEditor : public QWidget
{
	Q_OBJECT
	friend class DrawWarpingPoints;

public:
	VipWarpingEditor(QWidget* parent = nullptr);
	~VipWarpingEditor();

	void setWarpingTransform(VipWarping*);
	VipWarping* warpingTransform() const;

	VipVideoPlayer* FindOutputPlayer() const;
	PlotWarpingPoints* PlotPoints() const;

public Q_SLOTS:

	void SaveTransform();
	void LoadTransform();

	void SetSourcePointsFromPlayers();
	void SetSourcePointsFromDrawing();
	void ResetWarping();

	// apply the warping based on a set of points defined in 2 players
	void ChangeWarping();
	void ComputePlayerList();

	// apply the warping based on manually drawn deformation field
	void SetDrawingEnabled(bool);
	void Undo();
	void SetDrawnPointsVisible(bool visible);
	bool DrawnPointsVisible() const;
	void ComputeWarpingFromDrawnPoints();

private:
	void SetSourcePointsFromPlayers(bool from_players);

	void StartDeformation(const QPoint& src);
	void MovePoint(const QPoint& dst);
	void EndDeformation();

	void SaveParametersToWarpingObject();
	void LoadParametersFromWarpingObject();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

class PropertyWidget;
/// The "standard" property editor for VipProperty objects.
/// This class is used internally, you do not need to manipulate it directly.
class PropertyEditor : public QWidget
{
	Q_OBJECT

public:
	QWidget* editor;
	QString property;
	QString category;
	QPointer<VipProcessingObject> object;
	PropertyWidget* parent;

	PropertyEditor(VipProcessingObject* obj, const QString& property);

public Q_SLOTS:
	void updateProperty();
};

/// Widget to edit any kind of VipProcessingObject.
/// It relies on vipFDObjectEditor to generate the editor widgets for a given VipProcessingObject.
/// Use VipUniqueProcessingObjectEditor::setProcessingObject to modify the widget's content and display the editors for this VipProcessingObject.
class VIP_GUI_EXPORT VipUniqueProcessingObjectEditor : public QWidget
{
	Q_OBJECT

public:
	VipUniqueProcessingObjectEditor(QWidget* parent = nullptr);
	~VipUniqueProcessingObjectEditor();

	/// Modify the widget's content and display the editors for this VipProcessingObject.
	/// If \a exact_processing_class is true, only display the editor for the exact class of given object inheritance tree (i.e. the last class defining the Q_OBJECT macro).
	/// Otherwise, this widget will displays all found editors for the whole inheritance tree of \a obj.
	bool setProcessingObject(VipProcessingObject* obj);
	VipProcessingObject* processingObject() const;

	void setShowExactProcessingOnly(bool exact_proc);
	bool isShowExactProcessingOnly() const;

	/// Update processing properties that rely on the default editor
	void tryUpdateProcessing();

	static void geometryChanged(QWidget* widget);

private Q_SLOTS:
	void emitEditorVisibilityChanged();

Q_SIGNALS:
	void editorVisibilityChanged();

private:
	void removeEndStretch();
	
	VIP_DECLARE_PRIVATE_DATA();
};

class VIP_GUI_EXPORT VipProcessingLeafSelector : public QToolButton
{
	Q_OBJECT
public:
	VipProcessingLeafSelector(QWidget* parent = nullptr);
	~VipProcessingLeafSelector();

	void setProcessingPool(VipProcessingPool* pool);
	VipProcessingPool* processingPool() const;

	QList<VipProcessingObject*> leafs() const;
	VipProcessingObject* processing() const;

public Q_SLOTS:
	void setProcessing(VipProcessingObject*);

Q_SIGNALS:
	void processingChanged(VipProcessingObject*);

private Q_SLOTS:
	void aboutToShow();
	void processingSelected(QAction*);

private:
	QString title(VipProcessingObject* obj, QString& tool_tip) const;
	
	VIP_DECLARE_PRIVATE_DATA();
};

/// Widget used to hide/show a VipProcessingObject editor in a VipMultiProcessingObjectEditor.
class VIP_GUI_EXPORT VipProcessingTooButton : public QWidget
{
	Q_OBJECT
public:
	VipProcessingTooButton(VipProcessingObject* obj);
	~VipProcessingTooButton();
	QToolButton* showButton() const;
	QToolButton* resetButton() const;

	void setEditor(VipUniqueProcessingObjectEditor*);
	VipUniqueProcessingObjectEditor* editor() const;

public Q_SLOTS:
	void resetProcessing();
	void showError(bool);

private Q_SLOTS:
	void emitClicked(bool click) { Q_EMIT clicked(click); }
	void updateText();

Q_SIGNALS:
	void clicked(bool);

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};
/// Displays all the editors for a chain of VipProcessingObject.
/// Use VipMultiProcessingObjectEditor::setProcessingObjects to set the widget's content and display all the editors for a list of VipProcessingObject.
class VIP_GUI_EXPORT VipMultiProcessingObjectEditor : public QWidget
{
	Q_OBJECT

public:
	VipMultiProcessingObjectEditor(QWidget* parent = nullptr);
	~VipMultiProcessingObjectEditor();

	bool setProcessingObjects(const QList<VipProcessingObject*>& obj);
	QList<VipProcessingObject*> processingObjects() const;

	VipUniqueProcessingObjectEditor* processingEditor(VipProcessingObject* obj) const;
	template<class T>
	T processingEditor(VipProcessingObject* obj) const
	{
		if (VipUniqueProcessingObjectEditor* ed = processingEditor(obj))
			return ed->findChild<T>();
		return nullptr;
	}

	/// Show/hide the editor for a given processing.
	///  The editor can still be shown/hidden through its tool button
	void setProcessingObjectVisible(VipProcessingObject* object, bool visible);
	/// Completely hide/show a processing editor and its tool button. If hidden the processing cannot be edited at all.
	void setFullEditorVisible(VipProcessingObject* object, bool visible);

	void setShowExactProcessingOnly(bool exact_proc);
	bool isShowExactProcessingOnly() const;

	void setVisibleProcessings(const QList<const QMetaObject*>& proc_class_names);
	void setHiddenProcessings(const QList<const QMetaObject*>& proc_class_names);
	QList<const QMetaObject*> visibleProcessings() const;
	QList<const QMetaObject*> hiddenProcessings() const;

	// virtual QSize sizeHint() const;
private Q_SLOTS:
	void updateEditorsVisibility();
	void emitEditorVisibilityChanged();

Q_SIGNALS:
	void editorVisibilityChanged();
	void processingsChanged();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};


class VIP_GUI_EXPORT VipProcessingEditorToolWidget : public VipToolWidgetPlayer
{
	Q_OBJECT

public:
	VipProcessingEditorToolWidget(VipMainWindow* window);
	~VipProcessingEditorToolWidget();

	virtual bool setPlayer(VipAbstractPlayer*);

	VipProcessingObject* processingObject() const;
	VipMultiProcessingObjectEditor* editor() const;
	VipProcessingLeafSelector* leafSelector() const;

	void setShowExactProcessingOnly(bool exact_proc);
	bool isShowExactProcessingOnly() const;

	void setVisibleProcessings(const QList<const QMetaObject*>& proc_class_names);
	void setHiddenProcessings(const QList<const QMetaObject*>& proc_class_names);
	QList<const QMetaObject*> visibleProcessings() const;
	QList<const QMetaObject*> hiddenProcessings() const;

public Q_SLOTS:
	void setProcessingObject(VipProcessingObject* object);
	void setPlotItem(VipPlotItem* item);

private Q_SLOTS:
	void itemClicked(const VipPlotItemPointer&, int);
	void itemSelectionChanged(const VipPlotItemPointer&, bool);
	void itemSelectionChangedDirect(const VipPlotItemPointer& item, bool);
	void workspaceChanged();
	void emitProcessingsChanged() { Q_EMIT processingsChanged(); }

Q_SIGNALS:
	void processingsChanged();

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};

VIP_GUI_EXPORT VipProcessingEditorToolWidget* vipGetProcessingEditorToolWidget(VipMainWindow* window = nullptr);

#include <QDialog>


/// @brief Singleton class used to remember user choices on 
/// device type to open specific file formats,
/// and to remember device options when opening a file.
class VipRememberDeviceOptions
{
	VIP_DECLARE_PRIVATE_DATA_NO_QOBJECT();
	VipRememberDeviceOptions();

public:
	~VipRememberDeviceOptions();

	/// Returns a map of 'file_suffix' -> 'class_name'
	const QMap<QString, QString>& suffixToDeviceType() const;
	QString deviceTypeForSuffix(const QString & suffix) const;
	VipIODevice* deviceForSuffix(const QString& suffix) const;
	void setSuffixToDeviceType(const QMap<QString, QString>&);
	void addSuffixAndDeviceType(const QString& suffix, const QString& device_type);
	void clearSuffixAndDeviceType();

	/// Returns a map of 'class_name' -> 'VipIODevice*'
	const QMap<QString, VipIODevice*>& deviceOptions() const;
	void setDeviceOptions(const QMap<QString, VipIODevice*>&);
	void addDeviceOptions(const QString& device_type, VipIODevice* device);
	bool addDeviceOptionsCopy(const VipIODevice* src_device);
	bool applyDefaultOptions(VipIODevice* device);
	void clearDeviceOptions();

	void clearAll();

	static VipRememberDeviceOptions &instance();
};



/// @brief Edit device parameters with the possibility to
/// remember the user choices.
class VipSelectDeviceParameters : public QDialog
{
	Q_OBJECT
public:
	VipSelectDeviceParameters(VipIODevice* device, QWidget* editor, QWidget* parent = nullptr);
	~VipSelectDeviceParameters();

	bool remember() const;

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// Select a device among all possible ones
class VipDeviceChoiceDialog : public QDialog
{
	Q_OBJECT
public:
	VipDeviceChoiceDialog(QWidget* parent = nullptr);
	~VipDeviceChoiceDialog();
	void setChoices(const QList<VipIODevice*>& devices);
	void setPath(const QString& path);
	VipIODevice* selection() const;
	bool remember() const;

private:
	
	VIP_DECLARE_PRIVATE_DATA();
};




/// @brief Create a VipIODevice from a path.
/// 
class VIP_GUI_EXPORT VipCreateDevice
{

public:
	
	/// Create a VipIODevice based on a list of possible devices.
	/// If the device define an editor widget through vipFDObjectEditor() dispatcher and show_device_options is true,
	/// A dialog box containing the device editor is displayed.
	/// If the device editor widget defines the "apply" slot, it will be called.
	///
	/// If path is not empty, it is set to the device before displaying the editor.
	static VipIODevice* create(const QList<VipProcessingObject::Info>& dev, const VipPath& path = VipPath(), bool show_device_options = true);

	/// @brief Create device from a path.
	/// Show device selection dialog if necessary,
	/// as well as device editor widget.
	static VipIODevice* create(const VipPath& path, bool show_device_options = true);
};

/// @}
// end Gui

#endif
