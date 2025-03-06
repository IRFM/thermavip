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

#ifndef VIP_DRAG_WIDGET_H
#define VIP_DRAG_WIDGET_H

#include <QBrush>
#include <QFrame>
#include <QIcon>
#include <QLabel>
#include <QMap>
#include <QPainter>
#include <QPen>
#include <QPointer>
#include <QScrollArea>
#include <qmimedata.h>
#include <qrubberband.h>
#include <qsplitter.h>
#include <qstyleoption.h>
#include <qtabwidget.h>

#include "VipConfig.h"
#include "VipFunctional.h"
#include "VipRenderObject.h"
#include "VipText.h"

/// \addtogroup Gui
/// @{

class VipMultiDragWidget;
class QSplitter;
class QTabWidget;
class QSizeGrip;
class QGridLayout;
class QToolBar;
class QAction;
class QMimeData;
class QToolButton;
class QMenu;

class VipBaseDragWidget;
class VipDragWidget;
class VipMultiDragWidget;
class VipArchive;

/// VipDragWidgetHandler manage all instances of VipMultiDragWidget sharing the same parent widget.
/// To retrieve a VipDragWidgetHandler from this parent widget, use VipDragWidgetHandler::find static function. You cannot create an instance
/// of VipDragWidgetHandler by yourself, this is automatically managed within the VipMultiDragWidget class.
///
/// You can customize the VipMultiDragWidget objects by setting a VipDragWidgetParameters ot the handler (see VipDragWidgetHandler::setParameters).
class VIP_GUI_EXPORT VipDragWidgetHandler : public QObject
{
	Q_OBJECT

	friend class VipBaseDragWidget;
	friend class VipMultiDragWidget;
	friend class VipDragWidget;

	VipDragWidgetHandler();

public:
	/// Returns the parent widget managed by this VipDragWidgetHandler (parent of the VipMultiDragWidget handled by this object)
	QWidget* parentWidget();
	/// Returns the current focus widget from within all managed VipMultiDragWidget instances
	VipDragWidget* focusWidget();
	/// Returns all top level VipMultiDragWidget (which direct parent widget is parentWidget())
	QList<VipMultiDragWidget*> topLevelMultiDragWidgets();
	/// Returns all instances inheriting VipBaseDragWidget managed by this object
	QList<VipBaseDragWidget*> baseDragWidgets();
	/// Returns the currently maximized VipMultiDragWidget (if any)
	VipMultiDragWidget* maximizedMultiDragWidgets();

	/// Returns the VipDragWidgetHandler associated to \a parent
	static VipDragWidgetHandler* find(QWidget* parent);
	static VipDragWidgetHandler* find(VipBaseDragWidget* widget);

Q_SIGNALS:

	/// Emitted when the focus VipDragWidget changes
	void focusChanged(VipDragWidget* old, VipDragWidget* current);
	void minimized(VipMultiDragWidget*);
	void restored(VipMultiDragWidget*);
	void maximized(VipMultiDragWidget*);
	void closed(VipMultiDragWidget*);
	void added(VipMultiDragWidget*);
	void removed(VipMultiDragWidget*);
	void geometryChanged(VipMultiDragWidget*);
	void contentChanged(VipMultiDragWidget*);
	void moving(VipMultiDragWidget*);
	void visibilityChanged(VipBaseDragWidget*);

private:
	static void setParent(VipMultiDragWidget* top_level, QWidget* parent);
	static void remove(VipMultiDragWidget* top_level);

	QPointer<QWidget> d_parent;
	QPointer<VipDragWidget> d_focus;
	QList<QPointer<VipMultiDragWidget>> d_widgets;
};

/// Base class for any movable/dropable objects.
///
/// Its goal is to replace the QMdiSubWindow class which is a non top level window displaying a title bar and movable/resizable exactly like a top level window.
///
/// The goal of VipBaseDragWidget is to provide a widget with a slightly different behavior than the standard QMdiSubWindow:
/// <ul>
/// <li>Its look is highly customizable
/// <li>It is movable through drag & drop, which offers way more possibilities. For instance, instances of VipMultiDragWidget (inheriting VipBaseDragWidget)
/// can be drop upon each other in order to be displayed in separate tabs.
/// <li>It does not rely on a QMdiArea as parent widget, but can use any QWidget object
/// <li>Instances of VipMultiDragWidget can contain any number of children VipBaseDragWidget organized in any number of resizable rows/columns.
///
/// VipBaseDragWidget is an abstract class. The ones you should directly use are VipDragWidget and VipMultiDragWidget.
class VIP_GUI_EXPORT VipBaseDragWidget
  : public QFrame
  , public VipRenderObject
{
	Q_OBJECT

	friend class VipMultiDragWidget;
	friend class VipDragWidget;
	friend VIP_GUI_EXPORT VipArchive& operator<<(VipArchive& ar, VipBaseDragWidget* w);
	friend VIP_GUI_EXPORT VipArchive& operator>>(VipArchive& ar, VipBaseDragWidget* w);

public:
	/// Supported operations
	enum Operation
	{
		Move = 0x0001,		    //! The widget is movable
		Drop = 0x0002,		    //! The widget is droppable
		ReceiveDrop = 0x0004,	    //! The widget accept drops of other widgets
		Maximize = 0x0008,	    //! The widget can be maximized
		Minimize = 0x0010,	    //! The widget can be minimized
		Closable = 0x0020,	    //! The widget can be closed
		DragWidgetExtract = 0x0080, //! A drag widget can be extracted from its parent to make it free
		NoHideOnMaximize = 0x0100,  //! cannot hide widget when another is maximized
		AllOperations = Move | Drop | ReceiveDrop | Maximize | Minimize | Closable | DragWidgetExtract
	};
	//! Supported operations attributes
	typedef QFlags<Operation> Operations;

	/// Widget visibility state.
	enum VisibilityState
	{
		Normal,	   //! Normal state
		Maximized, //! Widget is maximized
		Minimized, //! Widget is minimized
	};

	VipBaseDragWidget(QWidget* parent = nullptr);
	virtual ~VipBaseDragWidget();

	/// Returns he parent VipMultiDragWidget, if this widget belongs to any. Might be nullptr, meaning that this widget is a top level widget.
	VipMultiDragWidget* parentMultiDragWidget() const;

	/// Returns the top level VipMultiDragWidget.
	///  Returns a nullptr VipMultiDragWidget if this widget IS the top level VipMultiDragWidget.
	VipMultiDragWidget* topLevelMultiDragWidget() const;

	/// Returns a valid (not nullptr) top level VipMultiDragWidget. If this widget is the top level VipMultiDragWidget, returns this.
	VipMultiDragWidget* validTopLevelMultiDragWidget() const;

	/// Returns the top level parent (i.e. the parent widget of the top level VipMultiDragWidget). Might be nullptr.
	virtual QWidget* topLevelParent();

	/// Returns true if this widget is a top level VipMultiDragWidget.
	bool isTopLevel() const;

	/// Returns the current visibility state.
	VisibilityState visibility() const;

	/// Equivalent to
	/// \code
	/// visibility() == Maximized
	/// \endcode
	bool isMaximized() const;

	/// Equivalent to
	/// \code
	/// visibility() == Minimized
	/// \endcode
	bool isMinimized() const;

	/// Returns the current mouse position relative to the top level parent (as returned by #topLevelParent()) or to the screen
	/// if no top level parent is found.
	QPoint topLevelPos();

	/// Set the operations supported by this widget
	void setSupportedOperations(Operations);
	/// Enable/disable an operation
	void setSupportedOperation(Operation, bool on = true);
	/// Returns true if given operation is supported, false otherwise
	bool testSupportedOperation(Operation) const;
	/// Returns all supported operations
	Operations supportedOperations() const;

	/// Returns true if the widget is dropable
	bool isDropable() const;
	/// Returns true if the widget is movable
	bool isMovable() const;
	/// Returns true if the widget can be maximized
	bool supportMaximize() const;
	/// Returns true if the widget can be minimized
	bool supportMinimize() const;
	/// Returns true if the widget can be closed through its title bar
	bool supportClose() const;
	/// Returns true if the widget supports dropping of other widgets
	bool supportReceiveDrop() const;

	/// Returns true if this widget or one of its VipBaseDragWidget parent is in the process of being destroyed
	bool isDestroying() const;

	void setShowIDInTitle(bool enable);
	bool showIdInTitle() const;
	QString title() const;

	/// Returns the first VipBaseDragWidget parent found for child \a child
	static VipBaseDragWidget* fromChild(QWidget* child);

	/// Internal use only
	bool dragThisWidget(QObject* watched, const QPoint& mouse_pos);

public Q_SLOTS:

	/// Display the widget maximized.
	/// The behavior depends on this widget hierarchy position:
	/// - It is a top level VipMultiDragWidget or the unique widget of a top level VipMultiDragWidget: maximize inside the #topLevelParent().
	/// - Otherwise, maximize inside its VipMultiDragWidget parent.
	virtual void showMaximized();

	/// Minimizes the widget. Only work if this widget is a top level VipMultiDragWidget or the unique widget of a top level VipMultiDragWidget.
	virtual void showMinimized();

	/// Restore the widget state after a call to #showMaximized() or #showMinimized().
	virtual void showNormal();

	/// Set the widget visibility state (call either #showMaximized(), #showMinimized() or #showNormal() ).
	void setVisibility(VisibilityState);

	/// Pass the focus to this widget
	virtual void setFocusWidget() = 0;

	/// Equivalent of QWidget::resize but as slot
	void setSize(const QSize& s) { resize(s); }

protected:
	/// Set the inner visibility state flag and send the #visibilityChanged() signal if needed.
	virtual void setInternalVisibility(VisibilityState);
	virtual bool event(QEvent* event);
	virtual void dragEnterEvent(QDragEnterEvent* evt);
	virtual void dropEvent(QDropEvent* evt);
	virtual void changeEvent(QEvent* evt);
	virtual void closeEvent(QCloseEvent* evt);

	void setTitleWithId(const QString& text);

Q_SIGNALS:

	void visibilityChanged(VisibilityState);
	void operationsChanged(Operations);

private Q_SLOTS:
	void addIdToTitle();

private:
	struct PrivateData;
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipBaseDragWidget*)

/// This is the mime data exchanged when dragging/dropping VipBaseDragWidget objects
class VIP_GUI_EXPORT VipBaseDragWidgetMimeData : public QMimeData
{
	Q_OBJECT

public:
	QPointer<VipBaseDragWidget> dragWidget;
	VipBaseDragWidgetMimeData()
	  : QMimeData()
	{
	}
};

/// A standard VipBaseDragWidget containing any kind of widget.
/// This is the equivalent of QMdiSubWidow, excpet that it cannot be used as a top level window, but always inside a VipMultiDragWidget object.
///
/// A VipDragWidget has a notion of focus, which is different from the QWidget focus one and coexists with it. It does not imply, for instance, that keyboard inputs will go to the widget.
/// Usually, it is only used to display a different title bar and by the VipToolWidget instances operating on players.
/// Within a VipDragWidgetHandler (and therefore within the same top level parent widget) only one VipDragWidget can have the focus at a time.
/// You can click on another VipDragWidget to pass the focus.
class VIP_GUI_EXPORT VipDragWidget : public VipBaseDragWidget
{
	Q_OBJECT
	friend class VipMultiDragWidget;

public:
	VipDragWidget(QWidget* parent = nullptr);
	~VipDragWidget();

	/// Returns true if the widget has the focus
	bool isFocusWidget() const;

	/// Set the focus to this widget
	virtual void setFocusWidget();

	/// Set the inner widget.
	///  This function can be overloaded, but the new implementation must call the base version to ensure internal integrity.
	virtual void setWidget(QWidget* widget);
	/// Returns the inner widget
	QWidget* widget() const;

	VipDragWidget* next() const;
	VipDragWidget* prev() const;

	virtual QSize sizeHint() const;

private Q_SLOTS:
	void titleChanged();

private:
	void relayout();
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipDragWidget*)

/// @brief QTabWidget that hides its tab bar when  containing one widget
class VipDragTabWidget : public QTabWidget
{
	Q_OBJECT
public:
	VipDragTabWidget(QWidget* parent = nullptr);
	~VipDragTabWidget();

	QTabBar* TabBar() const;

	virtual QSize sizeHint() const;

protected:
	virtual void tabInserted(int index);
	virtual void tabRemoved(int index);
};

/**
Widget displayed on place of a VipDragWidget when minimized
*/
class VipMinimizeWidget
  : public QFrame
  , public VipRenderObject
{
	Q_OBJECT
	Q_PROPERTY(QColor background READ background WRITE setBackground)
	Q_PROPERTY(QColor backgroundHover READ backgroundHover WRITE setBackgroundHover)
	Q_PROPERTY(QColor closeBackground READ closeBackground WRITE setCloseBackground)
	Q_PROPERTY(QColor closeBackgroundHover READ closeBackgroundHover WRITE setCloseBackgroundHover)
	Q_PROPERTY(int extent READ extent WRITE setExtent)
public:
	VipMinimizeWidget(VipBaseDragWidget* widget);
	~VipMinimizeWidget();

	QColor background() const;
	void setBackground(const QColor&);
	QColor backgroundHover() const;
	void setBackgroundHover(const QColor&);
	QColor closeBackground() const;
	void setCloseBackground(const QColor&);
	QColor closeBackgroundHover() const;
	void setCloseBackgroundHover(const QColor&);

	int extent() const;
	void setExtent(int);

	void reorganize();
	virtual bool eventFilter(QObject* w, QEvent* evt);
	virtual void mousePressEvent(QMouseEvent* evt);
	virtual void mouseMoveEvent(QMouseEvent* evt);
	virtual void paintEvent(QPaintEvent*);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	virtual void enterEvent(QEvent*);
#else
	virtual void enterEvent(QEnterEvent*);
#endif
	virtual void leaveEvent(QEvent*);
	virtual bool event(QEvent*);

protected:
	virtual void startRender(VipRenderState&);
	virtual void endRender(VipRenderState&);

private:
	
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

/// Custom QSplitterHandle class. In addition to its normal role, it provides several functionalities:
/// - It cannot be hidden
/// - It handles the dropping of VipBaseDragWidget objects
/// - It can be used to resize the whole VipMultiDragWidget it belongs to if it is located to an extremity.
class VIP_GUI_EXPORT VipDragWidgetHandle : public QSplitterHandle
{
	Q_OBJECT

public:
	VipDragWidgetHandle(VipMultiDragWidget* multiDragWidget, Qt::Orientation orientation, QSplitter* parent);
	Qt::Alignment HandleAlignment();

	void setMaximumHandleWidth(int);
	int maximumHandleWidth() const;

	virtual QSize sizeHint() const override;

	/// Drop mime data on handle
	bool dropMimeData(const QMimeData* mime);

protected:
	virtual void paintEvent(QPaintEvent* evt);

private:
	QPoint mouse;
	QRect rect;	
	VipMultiDragWidget* multiDragWidget;
	int maxWidth;
};

/// Custom QSplitter, which only goal is to use the VipDragWidgetSplitter class
class VIP_GUI_EXPORT VipDragWidgetSplitter : public QSplitter
{
	Q_OBJECT

public:
	VipDragWidgetSplitter(VipMultiDragWidget* multiDragWidget, Qt::Orientation orientation, QWidget* parent = 0);

	void setMaximumHandleWidth(int);
	int maximumHandleWidth() const;

Q_SIGNALS:
	void childChanged(QSplitter* s, QWidget* w, bool added);

private Q_SLOTS:
	void emitChildChanged(QSplitter* s, QWidget* w, bool added) { Q_EMIT childChanged(s, w, added); }

protected:
	virtual QSplitterHandle* createHandle();
	virtual void paintEvent(QPaintEvent* evt);
	virtual void childEvent(QChildEvent* evt);

private:
	VipMultiDragWidget* multiDragWidget;
	int maxWidth;
};

/// @brief Custom rubber band displayed to highlight potential drop areas
class VIP_GUI_EXPORT VipDragRubberBand : public QRubberBand
{
	Q_OBJECT
	Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
	Q_PROPERTY(double borderWidth READ borderWidth WRITE setBorderWidth)

	QStyleOptionRubberBand options;
	QPen pen;

public:
	QString text;

	VipDragRubberBand(QWidget* parent = nullptr);

	void setBorderColor(const QColor&);
	QColor borderColor() const;

	void setBorderWidth(double);
	double borderWidth() const;

protected:
	virtual void paintEvent(QPaintEvent*);
};

/// Base class for all VipBaseDragWidget containing several other VipBaseDragWidget organized horizontally and/or vertically.
/// The widget itself is organized as follow:
///
///  __ Main vertical QSplitter
///  |__ Horizontal QSplitter 1
///  |	|__ QTabWidget 1 containing zero or more VipBaseDragWidget
///  |	|__ QTabWidget 2 ...
///  |__ Horizontal QSplitter 2 ...
///  |
///  ...
///
/// It contains internally vertical and horizontal splitters which handles support VipBaseDragWidget dropping.
/// A VipBaseDragWidget can be dropped inside another VipBaseDragWidget, leading to a representation based on several pages of a QTabWidget.
/// A VipMultiDragWidget object might be a top level window (without parent) or inside another widget.
/// Use VipDragWidgetHandler class to manage all VipMultiDragWidget sharing the same top level parent.
///
/// When constructing a VipMultiDragWidget, it already has a width and height of 1 and use just have to call VipMultiDragWidget::setWidget to
/// set an inner widget.
///
/// Call VipMultiDragWidget::subResize or VipMultiDragWidget::mainResize to add horizontal or vertical panels.
class VIP_GUI_EXPORT VipMultiDragWidget : public VipBaseDragWidget
{
	Q_OBJECT

	friend class VipBaseDragWidget;
	friend class VipDragWidget;
	friend class CustomHandle;
	friend VIP_GUI_EXPORT VipArchive& operator<<(VipArchive& ar, VipMultiDragWidget* w);
	friend VIP_GUI_EXPORT VipArchive& operator>>(VipArchive& ar, VipMultiDragWidget* w);

public:
	using reparent_function = std::function<bool(VipMultiDragWidget*, QWidget*)>;

	enum VerticalSide
	{
		Top,
		Bottom
	};

	enum HorizontalSide
	{
		Left,
		Right
	};

	VipMultiDragWidget(QWidget* parent = nullptr);
	~VipMultiDragWidget();

	/// @brief Set a custom reparent function used by VipMultiDragWidget::supportReparent()
	static void setReparentFunction(reparent_function);
	static reparent_function reparentFunction();

	/// @brief Register a function that will be called each time a VipMultiDragWidget is created
	static void onCreated(const std::function<void(VipMultiDragWidget*)>& fun);

	Qt::Orientation orientation() const;
	void setOrientation(Qt::Orientation);

	/// Returns the main vertical splitter.
	/// This splitter only contains horizontal ones.
	QSplitter* mainSplitter() const;

	/// Returns the horizontal splitter  inside the main vertical one at given index.
	QSplitter* subSplitter(int y) const;

	/// Returns the QTabWidget at given index.
	QTabWidget* tabWidget(int y, int x) const;

	/// Returns vertical splitter handle at given position
	VipDragWidgetHandle* mainSplitterHandle(int y) const;

	/// Returns horizontal splitter handle at given position
	VipDragWidgetHandle* subSplitterHandle(int y, int x) const;

	/// Returns the parent QTabWidget for given VipBaseDragWidget inside this widget.
	QTabWidget* parentTabWidget(VipBaseDragWidget* widget) const;

	/// Returns VipBaseDragWidget object at given index.
	/// \param y position in the main vertical splitter.
	/// \param x position in the horizontal splitter.
	/// \param index position in the QTabWidget.
	VipBaseDragWidget* widget(int y, int x, int index) const;

	/// @brief Returns the first VipDragWidget this widget contains, or nullptr
	VipDragWidget* firstDragWidget() const;
	/// @brief Returns the first non minimized VipDragWidget this widget contains, or nullptr
	VipDragWidget* firstVisibleDragWidget() const;
	/// @brief Returns the last VipDragWidget this widget contains, or nullptr
	VipDragWidget* lastDragWidget() const;
	/// @brief Returns the last non minimized VipDragWidget this widget contains, or nullptr
	VipDragWidget* lastVisibleDragWidget() const;

	/// Returns the number of horizontal splitters in the main vertical one.
	int mainCount() const;

	/// Returns the number of QTabWidget objects in the horizontal splitter at given vertical position.
	int subCount(int y) const;

	/// Return the total number (without recursion) of VipBaseDragWidget objects inside this VipMultiDragWidget.
	int count() const;

	/// Returns the position of given VipBaseDragWidget inside this VipMultiDragWidget.
	/// \param index if not nullptr, filled with the index of given VipBaseDragWidget inside its parent QTabWidget (usually 0).
	QPoint indexOf(VipBaseDragWidget* w, int* index = nullptr) const;

	/// Vertically resize the VipMultiDragWidget from the top or the bottom by adding horizontal splitters containing one QTabWidget object.
	/// You can safely fill the new slots by calling #setWidget().
	void mainResize(int new_size, VipMultiDragWidget::VerticalSide side = VipMultiDragWidget::Bottom);

	/// Horizontally resize the VipMultiDragWidget at given vertical position (from the left or the right) by adding QTabWidget objects.
	/// You can safely fill the new slots by calling #setWidget().
	void subResize(int y, int new_size, VipMultiDragWidget::HorizontalSide side = VipMultiDragWidget::Right);

	/// Set the given VipBaseDragWidget to given position.
	/// The VipBaseDragWidget is added to a new page of the QTabWidget at this position.
	void setWidget(int y, int x, VipBaseDragWidget* widget, bool update_content = true);

	void swapWidgets(VipDragWidget* from, VipDragWidget* to);
	/// Insert orizontally a VipBaseDragWidget before given position.
	bool insertSub(int y, int x, VipBaseDragWidget* widget);

	/// Insert vertically a VipBaseDragWidget before given position.
	bool insertMain(int y, VipBaseDragWidget* widget);

	/// Hide all QTabWidget except the one containing given VipBaseDragWidget.
	void hideAllExcept(VipBaseDragWidget* widget);

	/// Show all QTabWidget objects.
	void showAll();

	/// Resize the internal splitters so that all VipDragWidget objects have the same size
	void reorganizeGrid();

	void setMaximumHandleWidth(int);
	int maximumHandleWidth() const;

	virtual void showMaximized();
	virtual void showMinimized();
	virtual void showNormal();

	virtual void setFocusWidget();

	virtual QSize sizeHint() const;
	virtual void startRender(VipRenderState&);
	virtual void endRender(VipRenderState&);

	static VipMultiDragWidget* fromChild(QWidget* child);

public Q_SLOTS:

	/// Create a new VipMultiDragWidget of the same type and with the same options as this one.
	/// You should always reimplement this function when subclassing VipMultiDragWidget.
	virtual VipMultiDragWidget* create(QWidget* parent) const;

	/// Whenever we try to change the top level parent of this VipMultiDragWidget using drag & drop, this function tells if this operation is supported.
	/// Default implementation just returns true if no custom reparent function is set with setReparentFunction() (changing parent is always possible),
	/// or returns the result of the custom reparent function.
	virtual bool supportReparent(QWidget* new_parent);

	/// Create a new VipBaseDragWidget inheriting object based on a QMimeData object.
	/// This allows to add new VipBaseDragWidget objects whene droping any kind of data on a droppable area.
	/// Default implementation handles the ropping of existing VipDragWidget or VipMultiDragWidget objects.
	VipBaseDragWidget* createFromMimeData(const QMimeData* data);

	/// Returns true if the QMimeData represents a data that can dropped on a VipMultiDragWidget.
	/// This fmemeber function works in conjunction with #createFromMimeData(), so you must reimplement both of them
	/// or none.
	bool supportDrop(const QMimeData* data);

	/// Returns the QGridLayout containing the main vertical splitter.
	/// The vertical splitter is inserted into a QGridLayout at index (10,10) to ease the customization of a VipMultiDragWidget.
	/// You can add additional items into the QGridLayout to  provide your own, custom widget.
	///
	/// The customization should only be done inside the constructor of derived class.
	QGridLayout* mainSplitterLayout() const;

	void passFocus();

	/// Reszize the widget and internal splitters to ensure the best visual aspect.
	void resizeBest();

	/// Automatically called when a new QTabWidget is created.
	/// Can be used to apply a different style to all QTabWidget.
	virtual void onTabWidgetCreated(QTabWidget*) {}

	/// Automatically called when a new horizontal splitter is created.
	/// Can be used to apply a different style to all horizontal splitters.
	virtual void onSplitterCreated(QSplitter*) {}

	//
	// internal use only
	//

	// move the grip to the top left corner on resize
	virtual void resizeEvent(QResizeEvent*);
	virtual void moveEvent(QMoveEvent* event);
	virtual void closeEvent(QCloseEvent* evt);

	// take into account a parent change when maximized
	virtual bool event(QEvent* event);

private Q_SLOTS:
	/// Resize the widget according to its content, and resize the internal splitters.
	/// If \a enable_resize is false, the widget itself won't be resized, only the internal widgets will be.
	void updateSizes(bool enable_resize);

	/// Update the widget. Destroy all empty QTabWidget/Horizontal splitters
	void updateContent();
	// intercept QApplication::focusChanged signal. Used to raise the right top level VipMultiDragWidget when clicking on a widget.
	void focusChanged(QWidget*, QWidget*);
	void receivedSplitterMoved(int pos, int index);

	void reorganizeMinimizedChildren();
Q_SIGNALS:
	void contentChanged();
	void widgetDestroyed(VipMultiDragWidget*);
	void splitterMoved(QSplitter* splitter, int pos, int index);

protected:
	virtual void setInternalVisibility(VisibilityState);

private:
	/// Returns the maximum width
	int maxWidth(int* row = nullptr, int* row_count = nullptr) const;
	// create a new QTabWidget
	QTabWidget* createTabWidget();
	// create a new horizontal splitter
	QSplitter* createHSplitter();

	struct PrivateData;
	VIP_DECLARE_PRIVATE_DATA(d_data);
};

VIP_REGISTER_QOBJECT_METATYPE(VipMultiDragWidget*)

class VipArchive;
VIP_GUI_EXPORT VipArchive& operator<<(VipArchive& ar, VipBaseDragWidget* w);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive& ar, VipBaseDragWidget* w);

VIP_GUI_EXPORT VipArchive& operator<<(VipArchive& ar, VipDragWidget* w);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive& ar, VipDragWidget* w);

VIP_GUI_EXPORT VipArchive& operator<<(VipArchive& ar, VipMultiDragWidget* w);
VIP_GUI_EXPORT VipArchive& operator>>(VipArchive& ar, VipMultiDragWidget* w);

class VIP_GUI_EXPORT VipViewportArea
  : public QWidget
  , public VipRenderObject
{
	Q_OBJECT
public:
	VipViewportArea();

	void dropMimeData(const QMimeData* mime, const QPoint& pos);

protected:
	void dragEnterEvent(QDragEnterEvent* evt);
	void dragMoveEvent(QDragMoveEvent* evt);
	void dropEvent(QDropEvent* evt);
};

/// VipMultiDragWidget instances can have any kind of parent widget, but we provide the VipDragWidgetArea class in order to have an equivalent to QMdiArea.
/// In this case the parent widget of the VipMultiDragWidget should be the one returned by VipDragWidgetArea::widget() function.
class VIP_GUI_EXPORT VipDragWidgetArea : public QWidget//QScrollArea
{
	Q_OBJECT
	friend class VipViewportArea;
	VipViewportArea* d_area;

public:
	VipDragWidgetArea(QWidget* parent = nullptr);
	~VipDragWidgetArea();

	virtual VipMultiDragWidget* createMultiDragWidget() const { return new VipMultiDragWidget(); }

	VipViewportArea* widget() const { return d_area; }
	
	void dropMimeData(const QMimeData* mime, const QPoint& pos);

	static VipDragWidgetArea* fromChildWidget(QWidget* widget);

protected:
	virtual void resizeEvent(QResizeEvent* evt);
	virtual bool eventFilter(QObject* watched, QEvent* event);
	virtual void keyPressEvent(QKeyEvent* evt);
private Q_SLOTS:
	void recomputeSize();
	void moving(VipMultiDragWidget*);

Q_SIGNALS:
	void textDropped(const QStringList& lst, const QPoint& pos);
	void mousePressed(int);
	void mouseReleased(int);
};

/// Function dispatcher which tells if a QMimeData can be dropped on a VipBaseDragWidget or inside a VipDragWidgetArea.
/// By using this dispatcher and the vipDropMimeData one, you can support other dropping behavior within VipBaseDragWidget.
/// Its signature is:
/// \code
/// bool(QMimeData*, QWidget * drop_target);
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<2>& vipAcceptDragMimeData();

/// Function dispatcher which drops a QMimeData on a VipBaseDragWidget or inside a VipDragWidgetArea.
/// By using this dispatcher and the vipAcceptDragMimeData one, you can support other dropping behavior within VipBaseDragWidget.
/// Its signature is:
/// \code
/// VipBaseDragWidget *(QMimeData*, QWidget * drop_target);
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<2>& vipDropMimeData();

/// Function dispatcher which provides a custom behavior when setting the inner widget of a VipDragWidget.
/// Its signature is:
/// \code
/// void (VipDragWidget*, QWidget*);
/// \endcode
VIP_GUI_EXPORT VipFunctionDispatcher<2>& vipSetDragWidget();

/// @}
// end Gui

#endif
