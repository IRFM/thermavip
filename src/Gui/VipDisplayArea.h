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

#ifndef VIP_DISPLAY_AREA_H
#define VIP_DISPLAY_AREA_H

#include <QMainWindow>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <qspinbox.h>

#include "VipDragWidget.h"
#include "VipMapFileSystem.h"

/// Minimal accepted version of a session file in order to be properly loaded
#define VIP_MINIMAL_SESSION_VERSION "2.2.5"

/// \addtogroup Gui
/// @{

class QTimer;
class VipIODevice;
class VipProcessingObject;
class VipDisplayTabWidget;
class VipArchive;
class VipAbstractPlayer;
class VipDisplayArea;
class VipGraphicsProcessingView;

// For compatibility with previous versions of Thermavip
using VipCustomDragWidget = VipDragWidget;
using VipCustomMultiDragWidget = VipMultiDragWidget;

/// Main tab bar of Thermavip. It allows creating new VipDisplayPlayerArea widgets through the '+' last tab.
/// Tabs also support dropping of any kind of data/player.
class VIP_GUI_EXPORT VipDisplayTabBar : public QTabBar
{
	Q_OBJECT
	Q_PROPERTY(QIcon closeIcon READ closeIcon WRITE setCloseIcon)
	Q_PROPERTY(QIcon floatIcon READ floatIcon WRITE setFloatIcon)
	Q_PROPERTY(QIcon hoverCloseIcon READ hoverCloseIcon WRITE setHoverCloseIcon)
	Q_PROPERTY(QIcon hoverFloatIcon READ hoverFloatIcon WRITE setHoverFloatIcon)
	Q_PROPERTY(QIcon selectedCloseIcon READ selectedCloseIcon WRITE setSelectedCloseIcon)
	Q_PROPERTY(QIcon selectedFloatIcon READ selectedFloatIcon WRITE setSelectedFloatIcon)

public:
	VipDisplayTabBar(VipDisplayTabWidget* parent);
	~VipDisplayTabBar();
	VipDisplayTabWidget* displayTabWidget() const;

	QIcon closeIcon() const;
	void setCloseIcon(const QIcon&);
	QIcon floatIcon() const;
	void setFloatIcon(const QIcon&);
	QIcon hoverCloseIcon() const;
	void setHoverCloseIcon(const QIcon&);
	QIcon hoverFloatIcon() const;
	void setHoverFloatIcon(const QIcon&);
	QIcon selectedCloseIcon() const;
	void setSelectedCloseIcon(const QIcon&);
	QIcon selectedFloatIcon() const;
	void setSelectedFloatIcon(const QIcon&);

	// QToolBar * tabButtons(int index = -1) const;
	//  QToolButton * floatButton(int index = -1) const;
	//  QToolButton * closeButton(int index = -1) const;
	//  QToolButton * streamingButton(int index = -1) const;

	void setStreamingEnabled(bool);
	bool streamingButtonEnabled() const;

public Q_SLOTS:
	void updateStreamingButton();
	void updateStreamingButtonDelayed();

protected:
	virtual void dragEnterEvent(QDragEnterEvent* evt);
	virtual void dragLeaveEvent(QDragLeaveEvent* evt);
	virtual void dragMoveEvent(QDragMoveEvent* evt);
	virtual void leaveEvent(QEvent* evt);
	virtual void mousePressEvent(QMouseEvent* event);
	virtual void mouseMoveEvent(QMouseEvent* event);
	virtual void mouseReleaseEvent(QMouseEvent* event);
	virtual void mouseDoubleClickEvent(QMouseEvent* evt);
	virtual void tabInserted(int index);

private Q_SLOTS:
	void dragLongEnough();
	void closeTab();
	void floatTab();
	void updateIcons();
	void enableStreaming();

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// A QTabWidget holding a VipDisplayTabBar.

class VIP_GUI_EXPORT VipDisplayTabWidget : public QTabWidget
{
	Q_OBJECT
public:
	VipDisplayTabWidget(QWidget* parent = nullptr);
	VipDisplayTabBar* displayTabBar() const;
	VipDisplayArea* displayArea() const;

public Q_SLOTS:
	void closeTab(int index);
	void renameWorkspace();

private Q_SLOTS:
	void tabChanged(int index);
	void closeTab();
	void closeAllTab();
	void closeAllButTab();
	void makeFloat(bool);
	void finishEditingTitle();

protected:
	virtual void mousePressEvent(QMouseEvent*);
	virtual void mouseDoubleClickEvent(QMouseEvent*);
};

class VipPlayWidget;
class VipProcessingPool;
class VipDragWidgetArea;
class VipDisplayPlayerArea;

class VIP_GUI_EXPORT VipPlayerAreaTitleBar : public QToolBar
{
	Q_OBJECT
	Q_PROPERTY(bool focus READ hasFocus WRITE setFocus)

public:
	VipPlayerAreaTitleBar(VipDisplayPlayerArea* win);
	~VipPlayerAreaTitleBar();

	bool isFloating() const;
	bool hasFocus() const;

	QAction* floatAction() const;
	QAction* closeAction() const;

	QList<QWidget*> additionalWidgets() const;
	void setAdditionalWidget(const QList<QWidget*>& ws);

public Q_SLOTS:
	void setFloating(bool pin);
	void setFocus(bool);
	void setTitle(const QString& title);
	void maximizeOrShowNormal();

protected:
	virtual bool eventFilter(QObject*, QEvent* evt);
	virtual void mouseDoubleClickEvent(QMouseEvent* event);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseReleaseEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

class VipScaleWidget;
class VipAxisColorMap;
class VipVideoPlayer;

/// VipDisplayPlayerArea is the tab widget inside a VipDisplayTabWidget.
///  It contains a VipDragWidgetArea which is a QScrollBar displaying the different players (inheriting VipAbstractPlayer) through VipMultiDragWidget instances.
///  It also displays a time scale slider (VipPlayWidget) to walk through temporal players.
///  The VipPlayWidget interact with the VipProcessingPool as returned by VipDisplayPlayerArea::processingPool. If a player displays the data of a VipIODevice
///  inheriting class, the device must be a child of the processing pool.
class VIP_GUI_EXPORT VipDisplayPlayerArea : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(bool focus READ hasFocus WRITE setFocus);
	friend class VipDisplayArea;

public:
	enum Operation
	{
		NoOperation = 0x00,
		Closable = 0x01,
		Floatable = 0x02,
		AllOperations = Closable | Floatable
	};
	typedef QFlags<Operation> Operations;

	VipDisplayPlayerArea(QWidget* parent = nullptr);
	~VipDisplayPlayerArea();

	VipMultiDragWidget* mainDragWidget(const QWidgetList& widgets = QWidgetList(), bool create_if_null = true);

	void setSupportedOperation(Operation, bool on = true);
	bool testSupportedOperation(Operation) const;
	void setSupportedOperations(Operations);
	Operations supportedOperations() const;

	void setUseGlobalColorMap(bool enable);
	bool useGlobalColorMap() const;

	/// Returns the child VipPlayWidget used to walk through the temporal players
	VipPlayWidget* playWidget() const;
	/// Returns the VipDragWidgetArea which displays the players
	VipDragWidgetArea* dragWidgetArea() const;
	/// Returns the VipDragWidgetHandler associated to the VipDragWidgetArea::widget instance
	VipDragWidgetHandler* dragWidgetHandler() const;

	QToolBar* leftTabWidget() const;
	QToolBar* takeLeftTabWidget();
	void setLeftTabWidget(QToolBar* w);
	QToolBar* rightTabWidget() const;
	QToolBar* takeRightTabWidget();
	void setRightTabWidget(QToolBar* w);

	VipScaleWidget* colorMapScaleWidget() const;
	VipAxisColorMap* colorMapAxis() const;
	QWidget* colorMapWidget() const;
	QToolBar* colorMapToolBar() const;
	void layoutColorMap(const QList<VipVideoPlayer*>& players = QList<VipVideoPlayer*>());
	void setColorMapToPlayer(VipVideoPlayer* pl, bool enable);

	VipPlayerAreaTitleBar* titleBar() const;

	VipDisplayTabWidget* parentTabWidget() const;

	QWidget* topWidget() const;

	/// Set the processing pool
	void setProcessingPool(VipProcessingPool* pool);
	/// Returns the processing pool
	VipProcessingPool* processingPool() const;

	/// Add a VipBaseDragWidget to the VipDragWidgetArea.
	///  If the widget is a VipDragWidget, it is first inserted in a VipMultiDragWidget.
	void addWidget(VipBaseDragWidget* w);

	bool isFloating() const;
	bool hasFocus() const;
	int id() const;

	bool automaticColorScale() const;
	bool isFlatHistogramColorScale() const;
	int colorMap() const;

	int maxColumns() const;

	/// Return the VipDisplayPlayerArea (if any) ancestor of \a child.
	static VipDisplayPlayerArea* fromChildWidget(QWidget* child);

	static void setWorkspaceTitleEditable(const std::function<QVariantMap(const QString&)>& generate_editable_symbol);

public Q_SLOTS:
	void setFloating(bool pin);
	void setFocus(bool);
	void setAutomaticColorScale(bool);
	void fitColorScaleToGrips();
	void setFlatHistogramColorScale(bool);
	void setColorMap(int);
	void editColorMap();
	void relayoutColorMap();
	void setMaxColumns(int);
	void saveImage();
	void saveSession();
	void copyImage();
	void copySession();
	void changeOrientation();
	void print();

private Q_SLOTS:
	void added(VipMultiDragWidget*);
	void contentChanged(VipMultiDragWidget*);
	void reloadPool();
	void focusChanged(QWidget* old_w, QWidget* new_w);
	void textDropped(const QStringList& lst, const QPoint& pos);
	void updateStreamingButton();
	void receiveMouseReleased(int);
	void pasteItems();
	void internalLayoutColorMap();
	void internalLayoutColorMapDelay();

	void emitPlayingStarted() { Q_EMIT playingStarted(); }
	void emitPlayingAdvancedOneFrame() { Q_EMIT playingAdvancedOneFrame(); }
	void emitPlayingStopped() { Q_EMIT playingStopped(); }

Q_SIGNALS:
	void playingStarted();
	void playingAdvancedOneFrame();
	void playingStopped();

protected:
	virtual void changeEvent(QEvent* event);
	virtual void closeEvent(QCloseEvent* evt);
	virtual void showEvent(QShowEvent* evt);

private:
	void setPoolToPlayers();
	void setId(int id);
	void setInternalOperations();

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipDisplayPlayerArea::Operations)
VIP_REGISTER_QOBJECT_METATYPE(VipDisplayPlayerArea*)
VIP_GUI_EXPORT VipArchive& operator<<(VipArchive&, const VipDisplayPlayerArea*);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive&, VipDisplayPlayerArea*);

/// VipDisplayArea is the central widget of the main Thermavip interface (VipMainWidget).
/// It basically only contains a VipDisplayTabWidget instance.
class VIP_GUI_EXPORT VipDisplayArea : public QWidget
{
	Q_OBJECT
	friend class VipDisplayPlayerArea;

public:
	VipDisplayArea(QWidget* parent = nullptr);
	~VipDisplayArea();

	/// Returns the child VipDisplayTabWidget
	VipDisplayTabWidget* displayTabWidget() const;
	/// Returns VipDisplayPlayerArea tab at given index
	VipDisplayPlayerArea* displayPlayerArea(int index) const;
	/// Returns the current VipDisplayPlayerArea tab (if any)
	VipDisplayPlayerArea* currentDisplayPlayerArea() const;
	/// Returns the child VipDragWidgetArea at given index
	VipDragWidgetArea* dragWidgetArea(int index) const;
	/// Returns the child VipPlayWidget at given index
	VipPlayWidget* playWidget(int index) const;

	VipDragWidget* focusWidget() const;

	/// Returns the number of tabs inside the VipDisplayTabWidget
	int count() const;
	/// Returns the widget (usually a VipDisplayPlayerArea) at given index
	VipDisplayPlayerArea* widget(int index) const;
	/// Add a tab to the VipDisplayTabWidget
	void addWidget(VipDisplayPlayerArea* widget);
	/// Remove all tabs
	void clear();

	/// A lot of widgets rely on the item selection in players.
	/// This function unselect and reselect ALL VipPlotItem within players in order to trigger again
	/// the behaviors based on item selection.
	void resetItemSelection();

	void setCurrentDisplayPlayerArea(VipDisplayPlayerArea*);

	void setStreamingEnabled(bool);
	bool streamingButtonEnabled() const;

Q_SIGNALS:
	/// This signal is emitted whenever the VipDragWidget having the focus changed
	void focusWidgetChanged(VipDragWidget*);
	void currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*);
	void displayPlayerAreaAdded(VipDisplayPlayerArea*);
	void displayPlayerAreaRemoved(VipDisplayPlayerArea*);
	void topLevelWidgetClosed(VipDisplayPlayerArea*, VipMultiDragWidget*);
	void playingStarted();
	void playingAdvancedOneFrame();
	void playingStopped();

private Q_SLOTS:
	void computeFocusWidget();
	void widgetClosed(VipMultiDragWidget*);
	void titleChanged(const QString& title);
	void emitPlayingStarted() { Q_EMIT playingStarted(); }
	void emitPlayingAdvancedOneFrame() { Q_EMIT playingAdvancedOneFrame(); }
	void emitPlayingStopped() { Q_EMIT playingStopped(); }
	void tabMoved(int from, int to);

private:
	void removeWidget(VipDisplayPlayerArea* widget);
	QString generateWorkspaceName() const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayArea*)
VIP_GUI_EXPORT VipArchive& operator<<(VipArchive&, const VipDisplayArea*);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive&, VipDisplayArea*);

class VipShowWidgetOnHover;
class VipMainWindow;
class VipXOArchive;
class QProgressBar;

class VIP_GUI_EXPORT VipIconBar : public QToolBar
{
	Q_OBJECT

public:
	QAction* icon;
	QAction* title;
	QLabel* labelIcon;
	QLabel* titleLabel;
	QAction* update;
	QProgressBar* updateProgress;
	QAction* updateIconAction;
	VipMainWindow* mainWindow;
	QString customTitle;

	VipIconBar(VipMainWindow* win);
	~VipIconBar();

	void updateTitle();

	void setTitleIcon(const QPixmap& pix);
	QPixmap titleIcon() const;

	/// Set the main Thermavip title
	void setMainTitle(const QString& title);
	QString mainTitle() const;

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent* evt);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseReleaseEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);

private Q_SLOTS:
	void setUpdateProgress(int value);

private:
	QPoint pt, previous_pos;
};

/// Customize the global Help menu
VIP_GUI_EXPORT void vipExtendHelpMenu(const std::function<void(QMenu*)>& fun);

class VIP_GUI_EXPORT VipCloseBar : public QToolBar
{
	Q_OBJECT

public:
	QAction* spacer;
	QSpinBox* maxCols;
	QAction* maxColsAction;
	QAction* maximize;

	QToolButton* toolsButton;
	QAction* help;
	QToolButton* helpButton;
	QAction* minimizeButton;
	QAction* maximizeButton;
	QAction* closeButton;
	VipMainWindow* mainWindow;
	QTimer stateTimer;
	bool hasFrame;

	VipCloseBar(VipMainWindow* win);
	~VipCloseBar();

	void startDetectState();

private Q_SLOTS:
	void maximizeOrShowNormal();
	void onMaximized();
	void onRestored();
	void onMinimized();
	void computeWindowState();
	void computeHelpMenu();
	void computeToolsMenu();
};

class VipToolWidget;

/// The top level Thermavip widget.
/// It is a QMainWindow which central widget is a VipDisplayArea.
/// It defines 3 different tool bars that can be extended through plugins:
/// <ul>
/// <li>VipMainWindow::fileToolBar provides a few actions to open files/directories.
/// <li>VipMainWindow::toolsToolBar displays actions to show/hide the different VipToolWidget instances.
/// <li>VipMainWindow::closeToolBar. VipMainWindow is a framless widget to gain some space, and this tool bar displays the actions to minimize/maximize/normalize the window.
/// </ul>
///
/// You can save a Thermavip session using VipMainWindow::saveSession and restore it with VipMainWindow::loadSession.
class VIP_GUI_EXPORT VipMainWindow : public QMainWindow
{
	Q_OBJECT
	friend class VipDisplayTabWidget;

	Q_PROPERTY(int margin READ margin WRITE setMargin)
	Q_PROPERTY(int adjustColorPalette READ adjustColorPalette WRITE setAdjustColorPalette)

public:
	/// Select the different elements to save when saving a Thermavip session.
	enum SessionContent
	{
		MainWindowState = 0x001,				  //! Save the VipMainWindow state (size, disposition of the tool widgets, etc.)
		Plugins = 0x002,					  //! Save the plugins states (see VipPluginInterface::save)
		Settings = 0x004,					  //! Global settings
		DisplayAreas = 0x008,					  //! Save the central VipDisplayArea state (processing pool and players)
		All = MainWindowState | Plugins | Settings | DisplayAreas //! Save all
	};

	enum SessionType
	{
		MainWindow,
		CurrentArea,
		DragWidget
	};

	VipMainWindow();
	~VipMainWindow();

	void setMainTitle(const QString& title);
	QString mainTitle() const;

	/// Returns the central VipDisplayArea
	VipDisplayArea* displayArea() const;

	/// Returns the file tool bar
	QToolBar* fileToolBar() const;
	QMenu* generateMenu() const;
	QMenu* fileMenu() const;
	QToolButton* generateButton() const;
	QToolButton* openFileButton() const;
	QToolButton* openDirButton() const;
	QToolButton* saveFileButton() const;
	VipShowWidgetOnHover* showTabBar() const;

	/// Returns the tool bar displaying the actions to show/hide the VipToolWidget instances
	QToolBar* toolsToolBar() const;

	/// Returns the tool bar used to maximize/minimize/restore the main window
	// VipTitleBar * titleBar() const;

	VipIconBar* iconBar() const;
	VipCloseBar* closeBar() const;

	bool workspacesMaximized() const;

	int margin() const;
	void setMargin(int m);

	QString customTitle() const;
	void setCustomTitle(const QString& title);

	/// Save a Thermavip session into a XML file.
	///  Returns true on success, file otherwise.
	bool saveSession(const QString& filename, int session_type = MainWindow, int session_content = All, const QByteArray& state = QByteArray());
	bool saveSession(VipXOArchive& arch, int session_type = MainWindow, int session_content = All, const QByteArray& state = QByteArray());

	QList<VipAbstractPlayer*> openPaths(const VipPathList& paths, VipAbstractPlayer* player, VipDisplayPlayerArea* area = nullptr);
	QList<VipAbstractPlayer*> openDevices(const QList<VipIODevice*>& devices, VipAbstractPlayer* player, VipDisplayPlayerArea* area = nullptr);
	void openPlayers(const QList<VipAbstractPlayer*> players);
	/// There is a Qt bug that causes a crash when trying to render a widget while it's parent is being destroyed.
	/// This causes the VipMultiWidgetIcons to crash when closing a VipDisplayPlayerArea.
	/// Therefore, currentTabDestroying tells if a tab is being closed.
	bool currentTabDestroying() const;

	/// Returns true if we are currently loading a session file through #VipMainWindow::loadSessionShowProgress
	bool isLoadingSession();
	bool sessionSavingEnabled() const;

	/// Equivalent to VipGuiDisplayParamaters::instance()->itemPaletteFactor().
	/// Only usefull for skins that wish to modify the default item color palette.
	int adjustColorPalette() const;

	virtual QMenu* createPopupMenu();

public Q_SLOTS:
	/// Restart Thermavip
	void restart();

	/// Raise the main window on top of the other
	void raiseOnTop();
	/// Equivalent to VipGuiDisplayParamaters::instance()->setItemPaletteFactor(factor).
	/// Only usefull for skins that wish to modify the default item color palette.
	void setAdjustColorPalette(int factor);

	/// Restore a Thermavip session from a XML file.
	///  Returns true on success, file otherwise.
	bool loadSessionShowProgress(const QString& filename, VipProgress*);
	bool loadSession(const QString& filename);
	/// Restore a Thermavip session from a XML file.
	///  Returns true on success, file otherwise.
	///  If \a filename session cannot be loaded, this function tries then to open \a fallback session file
	bool loadSessionFallback(const QString& filename, const QString& fallback, VipProgress*);

	QList<VipAbstractPlayer*> openPaths(const QStringList& filenames);
	/// Displays a dialog box to open one or multiple files
	QList<VipAbstractPlayer*> openFiles();
	/// Displays a dialog box to open a directory
	QList<VipAbstractPlayer*> openDir();

	/// Display the options dialog box
	void showOptions();
	/// Save the current session
	void saveSession();

	void showHelp();

	void maximizeWorkspaces(bool enable);

	void setMaxColumnsForWorkspace(int);

	void displayGraphicsProcessingPlayer();

	void setSessionSavingEnabled(bool enable);

	void aboutDialog();

	void startUpdateThread();
	void stopUpdateThread();

	void autoSave();
	void autoLoad();
	/// Tells whether next call to openPaths should display a dialog box on error or not.
	void setOpenPathShowDialogOnError(bool);

	void resetStyleSheet();

private Q_SLOTS:
	void init();
	void openSharedMemoryFiles();
	void computeSessions();
	void sessionTriggered(QAction*);
	void showHelpCustom();
	void restoreDockState(const QByteArray& state);
	void applicationStateChanged(Qt::ApplicationState state);
	void setFlatHistogramStrength(); // internal use only, for AdvancedDisplay plugin
	void tabChanged();
Q_SIGNALS:
	void aboutToClose();
	void sessionLoaded();
	void workspaceLoaded(VipDisplayPlayerArea*);

protected:
	virtual void closeEvent(QCloseEvent* evt);
	virtual void showEvent(QShowEvent*);
	virtual void keyPressEvent(QKeyEvent* evt);
	// virtual void paintEvent(QPaintEvent *);
private:
	QAction* addToolWidget(VipToolWidget* widget, const QIcon& icon, const QString& text, bool set_tool_icon = false);
	void setCurrentTabDestroy(bool);
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// Returns the main unique VipMainWindow
VIP_GUI_EXPORT VipMainWindow* vipGetMainWindow();

#include "VipFunctional.h"

/// Function dispatcher which create a VipBaseDragWidget object from an input VipIODevice object
/// The default behavior would normally use the dispatcher vipCreatePlayersFromProcessing() to create a VipDragWidget instance.
/// This dispatcher bypasses the default behavior by providing a custom one.
/// Its signature is:
/// \code
/// VipBaseDragWidget*(VipIODevice*);
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<1>& vipFDCreateWidgetFromIODevice();

/// Function dispatcher which turns on/off the minimal display for a widget
/// The minimal display is used to only display the visualized data without all the fanzy stuff/controls.
/// Its signature is:
/// \code
/// void (QWidget*, bool);
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<2>& vipFDSwitchToMinimalDisplay();

/// Create a VipBaseDragWidget from a VipProcessingObject.
/// If \a object is a VipIODevice (the device must be opened) and the function dispatcher vipFDCreateWidgetFromIODevice has a corresponding match, it will be used to create the widget.
/// Otherwise, this function uses vipCreatePlayersFromProcessing to generate the players and vipCreateFromWidgets to generate the VipBaseDragWidget.
VIP_GUI_EXPORT VipBaseDragWidget* vipCreateWidgetFromProcessingObject(VipProcessingObject* object);

/// Creates an instance of VipMultiDragWidget from a VipBaseDragWidget.
/// If \a w is already a VipMultiDragWidget, it is returned. Otherwise, it will be inserted into a new VipMultiDragWidget.
VIP_GUI_EXPORT VipMultiDragWidget* vipCreateFromBaseDragWidget(VipBaseDragWidget* w);

/// Returns a VipBaseDragWidget from a list of widgets. If the list is empty, nullptr is returned.
/// The widgets are inserted into a VipDragWidget objects and, if the list has a size of 2 or more, they are inserted into a VipMultiDragWidget.
VIP_GUI_EXPORT VipBaseDragWidget* vipCreateFromWidgets(const QList<QWidget*>& widgets);

VIP_GUI_EXPORT bool vipSaveImage(VipBaseDragWidget* w);

VIP_GUI_EXPORT bool vipSaveSession(VipBaseDragWidget* w);

VIP_GUI_EXPORT bool vipPrint(VipBaseDragWidget* w);

/// Function dispatcher called before a VipDragWidget is rendered (to save an image or print).
/// It take one argument which is the VipDragWidget internal widget (usually a player).
/// Signature:
/// void (QWidget *);
VIP_GUI_EXPORT VipFunctionDispatcher<1>& vipFDAboutToRender();

VIP_GUI_EXPORT VipArchive& vipSaveBaseDragWidget(VipArchive& arch, VipBaseDragWidget* w);
VIP_GUI_EXPORT VipBaseDragWidget* vipLoadBaseDragWidget(VipArchive& arch, VipDisplayPlayerArea* target);

/// @}
// end Gui

#endif
