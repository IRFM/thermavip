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
#include "VipFunctional.h"

/// Minimal accepted version of a session file in order to be properly loaded
#define VIP_MINIMAL_SESSION_VERSION "5.0.0"

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
class VipMainWindow;
class VipShowWidgetOnHover;
class VipMainWindow;
class VipXOArchive;
class QProgressBar;

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

	// Getter/setter for tab icons, mainly used for stylesheets

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

	bool streamingButtonEnabled() const;

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
	void setStreamingEnabled(bool);
	void updateStreamingButton();
	void updateStreamingButtonDelayed();

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

/// @brief Title bar of a workspace (VipDisplayPlayerArea),
/// only visible when the workspace is floating.
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

/// VipDisplayPlayerArea is the tab widget inside a VipDisplayTabWidget (also called a workspace).
/// 
/// It contains a VipDragWidgetArea which is a QScrollBar displaying the different players (inheriting VipAbstractPlayer) through VipMultiDragWidget instances.
/// It also displays a time scale slider (VipPlayWidget) to walk through temporal players.
/// The VipPlayWidget interact with the VipProcessingPool as returned by VipDisplayPlayerArea::processingPool. If a player displays the data of a VipIODevice
/// inheriting class, the device must be a child of the processing pool.
class VIP_GUI_EXPORT VipDisplayPlayerArea : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(bool focus READ hasFocus WRITE setFocus);
	friend class VipDisplayArea;
	friend class VipVideoPlayer;// for setColorMapToPlayer

public:
	/// @brief Operations supported by a VipDisplayPlayerArea
	enum Operation
	{
		NoOperation = 0x00,
		Closable = 0x01,	// The workspace is closable through a dedicated button
		Floatable = 0x02,	// The workspace is floatable through a dedicated button
		AllOperations = Closable | Floatable
	};
	typedef QFlags<Operation> Operations;

	VipDisplayPlayerArea(QWidget* parent = nullptr);
	~VipDisplayPlayerArea();

	/// Returns the workspace main VipMultiDragWidget.
	/// If the workspace does not contain a main VipMultiDragWidget yet,
	/// it will use take_this if not null. Otherwise, it will create
	/// a new one if create_if_null is true.
	/// 
	/// Returns null if no main VipMultiDragWidget can be found (or created).
	VipMultiDragWidget* mainDragWidget(VipMultiDragWidget * take_this = nullptr, bool create_if_null = true);

	/// @brief Get/Set supported operations
	void setSupportedOperation(Operation, bool on = true);
	/// @brief Get/Set supported operations
	bool testSupportedOperation(Operation) const;
	/// @brief Get/Set supported operations
	void setSupportedOperations(Operations);
	/// @brief Get/Set supported operations
	Operations supportedOperations() const;

	/// @brief Enable/disable the use of a global color map
	/// for all video players within this workspace.
	void setUseGlobalColorMap(bool enable);
	/// @brief Enable/disable the use of a global color map
	/// for all video players within this workspace.
	bool useGlobalColorMap() const;

	/// Returns the child VipPlayWidget used to walk through the temporal players
	VipPlayWidget* playWidget() const;
	/// Returns the VipDragWidgetArea which displays the players
	VipDragWidgetArea* dragWidgetArea() const;
	/// Returns the VipDragWidgetHandler associated to the VipDragWidgetArea::widget instance
	VipDragWidgetHandler* dragWidgetHandler() const;

	/// @brief Get/set/take the left tool bar displayed in the tab bar of this workspace (left to the workspace title)
	QToolBar* leftTabWidget() const;
	QToolBar* takeLeftTabWidget();
	void setLeftTabWidget(QToolBar* w);

	/// @brief Get/set/take the right tool bar displayed in the tab bar of this workspace (right to the workspace title)
	QToolBar* rightTabWidget() const;
	QToolBar* takeRightTabWidget();
	void setRightTabWidget(QToolBar* w);

	/// @brief Returns the VipScaleWidget used to display the global color map
	VipScaleWidget* colorMapScaleWidget() const;
	/// @brief Returns the global color map axis
	VipAxisColorMap* colorMapAxis() const;
	/// @brief Returns the global color map widget
	QWidget* colorMapWidget() const;
	/// @brief Returns the global color map tool bar
	QToolBar* colorMapToolBar() const;

	/// @brief Retruns the workspace title bar which is visible only when floating.
	VipPlayerAreaTitleBar* titleBar() const;

	/// @brief Returns the parent VipDisplayTabWidget
	VipDisplayTabWidget* parentTabWidget() const;

	/// @brief Returns the parent VipMainWindow if any.
	VipMainWindow* parentMainWindow() const;

	/// @brief Set the processing pool
	void setProcessingPool(VipProcessingPool* pool);
	/// @brief Returns the processing pool
	VipProcessingPool* processingPool() const;

	/// @brief Add a VipBaseDragWidget to the VipDragWidgetArea.
	/// If the widget is a VipDragWidget, it is first inserted in a VipMultiDragWidget.
	void addWidget(VipBaseDragWidget* w);

	/// @brief Returns true if this workspace is floating.
	bool isFloating() const;

	/// @brief Returns true if this workspace is the one displayed by its parent VipDisplayTabWidget
	bool hasFocus() const;

	/// @brief Get/set this workspace id.
	/// The id is mainly used to generate a unique workspace title.
	int id() const;

	/// @brief Returns true is the global colormap is automatic
	bool automaticColorScale() const;
	/// @brief Returns true if histogram flattening is enabled on the main colormap
	bool isFlatHistogramColorScale() const;
	/// @brief Returns the global colormap type (VipLinearColorMap::StandardColorMap)
	int colorMap() const;

	/// @brief Returns the maximum number of columns for this workspace.
	/// When adding new widget with addWidget(), the added widgets will
	/// always be placed on the bottom right part of the workspace, inside
	/// the main VipMultiDragWidget.
	/// A new column is added every time to add the new widget, until maxColumns()
	/// is reach, at which point a new row is created.
	int maxColumns() const;

	/// Return the VipDisplayPlayerArea (if any) ancestor of \a child.
	static VipDisplayPlayerArea* fromChildWidget(QWidget* child);

	static void setWorkspaceTitleEditable(const std::function<QVariantMap(const QString&)>& generate_editable_symbol);

	// Internal use only, returns the widget on top of the workspace used to display player's tool bars
	QWidget* topWidget() const;

public Q_SLOTS:
	/// @brief Enable/disable floating workspace, ignoring set supported operations.
	void setFloating(bool pin);

	/// @brief Set/unset the focus to this workspace.
	/// If enabled and if the parent VipDisplayTabWidget has multiple workspaces,
	/// this makes this workspace as the current one.
	/// If disable, it passes the focus to another workspace in the parent VipDisplayTabWidget.  
	void setFocus(bool);
	/// @brief Enable/disable automatic global colormap
	void setAutomaticColorScale(bool);
	/// @brief Adjust grips of the global colormap
	void fitColorScaleToGrips();
	/// @brief Enable/disable histogram flattening for the global colormap
	void setFlatHistogramColorScale(bool);
	/// @brief Set the global colormap type (VipLinearColorMap::StandardColorMap)
	void setColorMap(int);
	/// @brief Display the editor for the global colormap
	void editColorMap();
	void relayoutColorMap();
	/// @brief Set the maximum number of columns for this workspace.
	/// @sa maxColumns()
	void setMaxColumns(int);
	/// @brief Save the workspace as image
	void saveImage();
	/// @brief Save the workspace content in a file
	void saveSession();
	void copyImage();
	/// @brief Copy the workspace content in a temporary session file that is copied to the clipboard.
	/// This is usesfull to directly past it into PowerPoint for instance.
	void copySession();
	/// @brief Change the main drag widget orientation (vertical to horizontal or conversely)
	void changeOrientation();
	/// @brief Print the workspace content
	void print();

Q_SIGNALS:
	/// @brief Emitted when playing started from the processing pool
	void playingStarted();
	/// @brief Emitted when playing advanced one frame from the processing pool
	void playingAdvancedOneFrame();
	/// @brief Emitted when playing stopped from the processing pool
	void playingStopped();

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
	void emitWorkspaceContentChanged();

protected:
	virtual void changeEvent(QEvent* event);
	virtual void closeEvent(QCloseEvent* evt);
	virtual void showEvent(QShowEvent* evt);

private:
	void setPoolToPlayers();
	void setId(int id);
	void setInternalOperations();
	void layoutColorMap(const QList<VipVideoPlayer*>& players = QList<VipVideoPlayer*>());
	void setColorMapToPlayer(VipVideoPlayer* pl, bool enable);

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(VipDisplayPlayerArea::Operations)
VIP_REGISTER_QOBJECT_METATYPE(VipDisplayPlayerArea*)
VIP_GUI_EXPORT VipArchive& operator<<(VipArchive&, const VipDisplayPlayerArea*);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive&, VipDisplayPlayerArea*);

/// VipDisplayArea is the central widget of the main Thermavip interface (VipMainWindow).
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

	/// @brief Returns the current focus VipDragWidget, if any.
	VipDragWidget* focusWidget() const;

	/// @brief Returns the parent VipMainWindow, if any.
	VipMainWindow* parentMainWindow() const;

	/// Returns the number of tabs inside the VipDisplayTabWidget
	int count() const;
	/// Returns the widget (usually a VipDisplayPlayerArea) at given index
	VipDisplayPlayerArea* widget(int index) const;
	/// Add a tab to the VipDisplayTabWidget
	void addWidget(VipDisplayPlayerArea* widget);
	

	/// A lot of widgets rely on the item selection in players.
	/// This function unselect and reselect ALL VipPlotItem within players in order to trigger again
	/// the behaviors based on item selection.
	void resetItemSelection();

	/// @brief Set the current focus/visible workspace
	void setCurrentDisplayPlayerArea(VipDisplayPlayerArea*);

	/// @brief Returns true if the streaming button is enabled.
	/// The streaming button is enabled only if the current workspace processing pool
	/// contains at least on Sequential device (VipIODevice::Sequential).
	bool streamingButtonEnabled() const;

public Q_SLOTS:
	/// Remove all workspaces
	void clear();
	/// @brief Go to next workspace
	void nextWorkspace();
	/// @brief Go to previous workspace
	void previousWorkspace();
	
Q_SIGNALS:
	/// @brief Emitted whenever the VipDragWidget having the focus changed
	void focusWidgetChanged(VipDragWidget*);
	/// @brief Emitted when the current focus/visible VipDisplayPlayerArea changed
	void currentDisplayPlayerAreaChanged(VipDisplayPlayerArea*);
	/// @brief Emitted when a workspace is added
	void displayPlayerAreaAdded(VipDisplayPlayerArea*);
	/// @brief Emitted when a workspace is removed
	void displayPlayerAreaRemoved(VipDisplayPlayerArea*);
	/// @brief Emitted when a main (top level) VipMultiDragWidget is closed within a workspace
	void topLevelWidgetClosed(VipDisplayPlayerArea*, VipMultiDragWidget*);
	/// @brief Emitted when playing started from the processing pool
	void playingStarted();
	/// @brief Emitted when playing advanced one frame from the processing pool
	void playingAdvancedOneFrame();
	/// @brief Emitted when playing stopped from the processing pool
	void playingStopped();

private Q_SLOTS:
	void computeFocusWidget();
	void widgetClosed(VipMultiDragWidget*);
	void titleChanged(const QString& title);
	void emitPlayingStarted() { Q_EMIT playingStarted(); }
	void emitPlayingAdvancedOneFrame() { Q_EMIT playingAdvancedOneFrame(); }
	void emitPlayingStopped() { Q_EMIT playingStopped(); }
	void tabMoved(int from, int to);
	/// @brief Enable/disable streaming button for all workspaces
	void setStreamingEnabled(bool);

private:
	void removeWidget(VipDisplayPlayerArea* widget);
	QString generateWorkspaceName() const;

	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipDisplayArea*)
VIP_GUI_EXPORT VipArchive& operator<<(VipArchive&, const VipDisplayArea*);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive&, VipDisplayArea*);



/// @brief Icon bar displayed on the evry topleft of Thermavip main window.
///
/// VipIconBar displays Thermavip's title and main icon on the top left
/// of the main VipMainWindow.
/// 
/// It also display the current status of Thermavip updates (if available).
/// This tool bar can be used to move the software on the desktop and
/// to maximize/restore it.
/// 
class VIP_GUI_EXPORT VipIconBar : public QToolBar
{
	Q_OBJECT

	friend class UpdateThread;

public:
	
	VipIconBar(VipMainWindow* win);
	~VipIconBar();

	/// @brief Set the main top-left icon
	void setTitleIcon(const QPixmap& pix);
	QPixmap titleIcon() const;

	/// @brief Set the main Thermavip title
	void setMainTitle(const QString& title);
	QString mainTitle() const;

private Q_SLOTS:
	void setUpdateProgress(int value);
	void updateTitle();

private:
	QAction* icon;		      // Main icon action
	QAction* title;		      // Main title action
	QLabel* labelIcon;	      // Main icon widget
	QLabel* titleLabel;	      // Main title widget
	QAction* update;	      // Update action
	QProgressBar* updateProgress; // Progress bar used to display update progress
	QAction* updateIconAction;    // Progress bar action
	VipMainWindow* mainWindow;    // Parent VipMainWindow
	QString customTitle;	      // Custom title
	QPoint pt, previous_pos;
};

/// @brief Customize the global Help menu.
/// 
/// Register a function that will customize and add actions to the main
/// Thermavip 'help' menu.
/// 
VIP_GUI_EXPORT void vipExtendHelpMenu(const std::function<void(QMenu*)>& fun);


/// @brief Close tool bar, display on the top right of Thermavip.
///
/// 
class VIP_GUI_EXPORT VipCloseBar : public QToolBar
{
	Q_OBJECT
	friend class VipMainWindow;

public:
	
	VipCloseBar(VipMainWindow* win);
	~VipCloseBar();

	/// @brief Helper function, compute the global tools menu that will be added 
	/// to a QToolButton.
	void computeToolsMenu(QToolButton* button);

	// Get the widgets/actions available within this tool bar.
	// This is mostly usefull to insert new actions within plugins.

	QSpinBox* maxColumsWidget() { return maxCols; }
	QAction* maxColumsAction() { return maxColsAction; }
	QAction* maximizeOrShowNormalAction() { return maximize; }
	QAction* gloablHelpAction() { return help; }
	QToolButton* gloablHelpButton() { return helpButton; }
	QAction* minimizeAction() { return minimizeButton; }
	QAction* maximizeAction() { return maximizeButton; }
	QAction* closeAction() {return closeButton;}

private Q_SLOTS:
	void maximizeOrShowNormal();
	void computeWindowState();
	void computeHelpMenu();
	void computeToolsMenu();
	void startDetectState();

private:
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
};

class VipToolWidget;

/// @brief The top level Thermavip widget.
/// 
/// VipMainWindow is a QMainWindow which central widget is a VipDisplayArea
/// that displays one or more workspaces.
/// 
/// It also displays several tool bars accessible through member functions;
/// 
/// Only one VipMainWindow is allowed to exist at a given time.
/// 
/// The global VipMainWindow must ALWAYS be access/created using vipGetMainWindow() function.
/// 
class VIP_GUI_EXPORT VipMainWindow : public QMainWindow
{
	Q_OBJECT
	Q_PROPERTY(int margin READ margin WRITE setMargin)
	Q_PROPERTY(int adjustColorPalette READ adjustColorPalette WRITE setAdjustColorPalette)

	friend class VipDisplayTabWidget;
	friend VIP_GUI_EXPORT VipMainWindow* vipGetMainWindow();
	VipMainWindow();

public:
	/// @brief Select the different elements to save when saving a Thermavip session.
	enum SessionContent
	{
		MainWindowState = 0x001,			// Save the VipMainWindow state (size, disposition of the tool widgets, etc.)
		Plugins = 0x002,					// Save the plugins states (see VipPluginInterface::save)
		Settings = 0x004,					// Global settings
		DisplayAreas = 0x008,				// Save the central VipDisplayArea state (processing pool and players)
		All = MainWindowState | Plugins | Settings | DisplayAreas //! Save all
	};

	/// @brief Type of session to save
	enum SessionType
	{
		MainWindow,		// The main Thermavip window
		CurrentArea,	// The current workspace
		DragWidget		// A unique player
	};

	~VipMainWindow();


	/// @brief Get/Set the main Thermavip title (see VipIconBar)
	void setMainTitle(const QString& title);
	QString mainTitle() const;

	/// @brief Returns the central VipDisplayArea
	VipDisplayArea* displayArea() const;

	/// @brief Returns the file tool bar,
	/// used to open files/folders, generate signals or save sessions.
	QToolBar* fileToolBar() const;
	/// @brief Returns the generate menu used within the file tool bar.
	/// The generate menu can be extended by plugins to provide a quick
	/// way to generate new signals.
	/// 
	QMenu* generateMenu() const;
	/// @brief Returns the generate button associated with the generate menu.
	QToolButton* generateButton() const;

	/// @brief Returns button used to open files within the file tool bar.
	QToolButton* openFileButton() const;
	/// @brief Returns button used to open a directory within the file tool bar.
	QToolButton* openDirButton() const;
	/// @brief Returns button used to save session files within the file tool bar.
	QToolButton* saveFileButton() const;

	/// @brief Returns the tool bar displaying the actions to show/hide the VipToolWidget instances
	QToolBar* toolsToolBar() const;

	/// @brief Returns the icon bar used to display Thermavip title and icon,
	/// as well as maximize/minimize/move Thermavip main widget.
	VipIconBar* iconBar() const;
	/// @brief Returns the close bar.
	VipCloseBar* closeBar() const;

	/// @brief Returns true if workspaces are currently maximized, false otherwise.
	bool workspacesMaximized() const;

	/// @brief Returns the margin (in pixels) around this widget
	/// @sa setMargin 
	int margin() const;
	

	/// @brief Save a Thermavip session into a XML file.
	/// Returns true on success, file otherwise.
	/// Only MainWindow and CurrentArea types are supported.
	bool saveSession(const QString& filename, SessionType session_type = MainWindow, int session_content = All);
	/// @brief Save a Thermavip session into a VipArchive object.
	/// Returns true on success, file otherwise.
	/// Only MainWindow and CurrentArea types are supported.
	bool saveSession(VipArchive& arch, SessionType session_type = MainWindow, int session_content = All);

	/// @brief Open given data/signals files in Thermavip
	/// @param paths List of paths
	/// @param player Player to open signals/data in, or nullptr to open in a new player
	/// @param area Workspace to open the player in, or nullptr to use the current workspace.
	/// @return The list of created players
	QList<VipAbstractPlayer*> openPaths(const VipPathList& paths, VipAbstractPlayer* player, VipDisplayPlayerArea* area = nullptr);

	/// @brief Open devices in Thermavip
	/// @param devices List of devices. The devices must already be open in read mode.
	/// @param player Player to open signals/data in, or nullptr to open in a new player
	/// @param area Workspace to open the player in, or nullptr to use the current workspace.
	/// @return The list of created players
	QList<VipAbstractPlayer*> openDevices(const QList<VipIODevice*>& devices, VipAbstractPlayer* player, VipDisplayPlayerArea* area = nullptr);

	/// @brief Open (display) players in the current workspace.
	void openPlayers(const QList<VipAbstractPlayer*> players);

	/// @brief There is a Qt bug that causes a crash when trying to render a widget while it's parent is being destroyed.
	/// This causes the VipMultiWidgetIcons to crash when closing a VipDisplayPlayerArea.
	/// Therefore, currentTabDestroying tells if a tab is being closed.
	bool currentTabDestroying() const;

	/// @brief Returns true if we are currently loading a session file through #VipMainWindow::loadSessionShowProgress.
	/// 
	bool isLoadingSession();

	/// @brief Enable/disable the saving session feature
	bool sessionSavingEnabled() const;

	/// @brief Equivalent to VipGuiDisplayParamaters::instance()->itemPaletteFactor().
	/// Only usefull for skins that wish to modify the default item color palette.
	int adjustColorPalette() const;

	/// @brief Reimplemented from QMainWindow
	virtual QMenu* createPopupMenu();

public Q_SLOTS:
	/// @brief Restart Thermavip
	void restart();

	/// @brief Raise the main window on top of the other
	void raiseOnTop();

	/// @brief Equivalent to VipGuiDisplayParamaters::instance()->setItemPaletteFactor(factor).
	/// Only usefull for skins that wish to modify the default item color palette.
	void setAdjustColorPalette(int factor);

	/// @brief Restore a Thermavip session from a XML file.
	/// Returns true on success, file otherwise.
	bool loadSessionShowProgress(const QString& filename, VipProgress*);
	bool loadSession(const QString& filename);
	
	/// @brief Open given data/signals files within the current workspace in new players.
	QList<VipAbstractPlayer*> openPaths(const QStringList& filenames);
	/// @brief Displays a dialog box to open one or multiple files
	QList<VipAbstractPlayer*> openFiles();
	/// @brief Displays a dialog box to open a directory
	QList<VipAbstractPlayer*> openDir();

	/// @brief Display the options dialog box
	/// @sa vipGetOptions
	void showOptions();

	/// @brief Save the current session
	void saveSession();

	/// @brief Open Thermavip help in a web browser if available
	void showHelp();

	/// @brief Set the margin (in pixels) around the main interface
	void setMargin(int m);

	/// @brief Enable/disable workspace maximization.
	/// Maximizing workspace is usefull to hide all non mandatory widgets
	/// in order to have the widest possible area to display signals.
	void maximizeWorkspaces(bool enable);

	/// @brief Set the maximum number of columns for the current workspace.
	void setMaxColumnsForWorkspace(int);

	/// @brief Enable/disable the saving session feature
	void setSessionSavingEnabled(bool enable);

	/// @brief Display the about dialog.
	void aboutDialog();

	// Internal use only, start/stop the update thread
	void startUpdateThread();
	void stopUpdateThread();

	/// @brief Quick session saving (F5 key)
	void quickSave();
	/// @brief Quick session loading (f9 key)
	void quickLoad();
	/// @brief Quick session saving (F5 key)Tells whether next call to openPaths should display a dialog box on error or not.
	void setOpenPathShowDialogOnError(bool);

	/// @brief Start of stop playing/streaming for the current active workspace (Space key)
	void startStopPlaying();
	/// @brief Go to next time for the current active workspace
	void nextTime();
	/// @brief Go to previous time for the current active workspace
	void previousTime();
	/// @brief Go to first time for the current active workspace
	void firstTime();
	/// @brief Go to last time for the current active workspace
	void lastTime();
	/// @brief Advance time by 10% of the time range for the current active workspace
	void forward10Time();
	/// @brief Go backward in time by 10% of the time range for the current active workspace
	void backward10Time();
	void nextWorkspace();
	void previousWorkspace();
	void newWorkspace();
	void closeWorkspace();
	void focusToSearchLine();
	void toogleFullScreen();
	void exitFullScreen();

	void restoreOrMaximizeCurrentPlayer();
	void restoreOrMinimizeCurrentPlayer();
	void closeCurrentPlayer();
	void toggleWorkspaceFlatHistogram();

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
	void finalizeToolsToolBar();
	// Go to next time for the current active workspace when receiving right arrow,
	// or move selected shapes
	void nextTimeOnKeyRight();
	// Go to previous time for the current active workspace when receiving left arrow,
	// or move selected shapes
	void previousTimeOnKeyLeft();
Q_SIGNALS:
	void aboutToClose();
	void sessionLoaded();
	void workspaceLoaded(VipDisplayPlayerArea*);

	void workspaceCreated(VipDisplayPlayerArea*);
	void workspaceDestroyed(VipDisplayPlayerArea*);
	void workspaceContentChanged(VipDisplayPlayerArea*);

protected:
	virtual void closeEvent(QCloseEvent* );
	virtual void showEvent(QShowEvent*);
	virtual void keyPressEvent(QKeyEvent*);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseReleaseEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);
	virtual void mouseDoubleClickEvent(QMouseEvent* evt);

private:
	QAction* addToolWidget(VipToolWidget* widget, const QIcon& icon, const QString& text, bool set_tool_icon = false);
	void setCurrentTabDestroy(bool);
	bool loadSessionShowProgress(VipArchive& arch, VipProgress* progress);

	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// @brief Returns (and create if necessary) the main unique VipMainWindow
VIP_GUI_EXPORT VipMainWindow* vipGetMainWindow();



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
