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

#ifndef VIP_TOOL_WIDGET_H
#define VIP_TOOL_WIDGET_H

#include <QDockWidget>
#include <QFrame>
#include <qpointer.h>
#include <qpushbutton.h>
#include <qscrollarea.h>
#include <qtimer.h>
#include <qtoolbar.h>

#include "VipConfig.h"
#include "VipCore.h"

/// \addtogroup Gui
/// @{

class QScrollArea;
class VipMainWindow;
class VipToolWidget;
class VipDisplayPlayerArea;
class VipAbstractPlayer;
class VipDragWidget;

/// @brief Title bar of a VipToolWidget
///
/// VipToolWidget represents the title bar of a VipToolWidget.
/// It provides optins to close the parent VipToolWidget,
/// maximize it, make it floatable.
///
/// Its GUI parameters (text color, icons...) are defined as Qt
/// properties in order to be controlled through style sheets.
///
class VIP_GUI_EXPORT VipToolWidgetTitleBar : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(QIcon closeButton READ closeButton WRITE setCloseButton)
	Q_PROPERTY(QIcon floatButton READ floatButton WRITE setFloatButton)
	Q_PROPERTY(QIcon maximizeButton READ maximizeButton WRITE setMaximizeButton)
	Q_PROPERTY(QIcon restoreButton READ restoreButton WRITE setRestoreButton)
	Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
	Q_PROPERTY(QColor patternColor READ patternColor WRITE setPatternColor)
	Q_PROPERTY(bool displayWindowIcon READ displayWindowIcon WRITE setDisplayWindowIcon)
public:
	VipToolWidgetTitleBar(VipToolWidget* parent);
	~VipToolWidgetTitleBar();

	VipToolWidget* parent() const;
	QToolBar* toolBar() const;
	QIcon closeButton() const;
	QIcon floatButton() const;
	QIcon maximizeButton() const;
	QIcon restoreButton() const;

	QColor textColor() const;
	QColor patternColor() const;
	bool displayWindowIcon() const;

public Q_SLOTS:
	void setCloseButton(const QIcon&);
	void setFloatButton(const QIcon&);
	void setMaximizeButton(const QIcon&);
	void setRestoreButton(const QIcon&);
	void setTextColor(const QColor&);
	void setPatternColor(const QColor&);
	void setDisplayWindowIcon(bool);

private Q_SLOTS:
	void updateTitleAndPosition();
	void updateTitle();

protected:
	virtual void paintEvent(QPaintEvent* evt);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	virtual void enterEvent(QEvent*);
#else
	virtual void enterEvent(QEnterEvent*);
#endif
	virtual void leaveEvent(QEvent* event);
	virtual void resizeEvent(QResizeEvent*);

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief Tool bar related to a VipToolWidget that provides a few shortcuts.
///
/// Since a VipToolWidget can be a wide widgets with a lot of options, it is sometimes convinent to provide
/// a small tool bar (displayed in the main window) that provides the most important tools that the VipToolWidget provides.
///
/// A VipToolWidgetToolBar is created through #VipToolWidget::toolBar().
///
class VIP_GUI_EXPORT VipToolWidgetToolBar : public QToolBar
{
	Q_OBJECT
	VipToolWidget* m_toolWidget;

public:
	VipToolWidgetToolBar(VipToolWidget* tool, QWidget* parent = nullptr)
	  : QToolBar(parent)
	  , m_toolWidget(tool)
	{
	}

	virtual void setDisplayPlayerArea(VipDisplayPlayerArea*) {}
	virtual bool setPlayer(VipAbstractPlayer*) { return false; }
	void setEnabled(bool enable);
	VipToolWidget* toolWidget() const { return m_toolWidget; }

protected:
	virtual void showEvent(QShowEvent* evt);
};

/// @brief Viewport widget used in VipToolWidgetScrollArea.
/// Just provided for style sheet customization.
class VIP_GUI_EXPORT VipViewport : public QWidget
{
	Q_OBJECT
public:
	VipViewport(QWidget* parent = nullptr)
	  : QWidget(parent)
	{
	}
};

/// @brief QScrollArea used in VipToolWidget
class VIP_GUI_EXPORT VipToolWidgetScrollArea : public QScrollArea
{
	Q_OBJECT
	Q_PROPERTY(bool floatingTool READ floatingTool WRITE setFloatingTool)
public:
	VipToolWidgetScrollArea(QWidget* parent = nullptr)
	  : QScrollArea(parent)
	{
		setViewport(new VipViewport());
	}

	bool floatingTool() const;
	void setFloatingTool(bool f);

protected:
	virtual void resizeEvent(QResizeEvent* evt);
};

class VipWidgetResizer;

/// @brief A QDockWidget with additional features.
///
/// VipToolWidget is the base class for all QDockWidget
///
class VIP_GUI_EXPORT VipToolWidget : public QDockWidget
{
	Q_OBJECT
	Q_PROPERTY(bool floatingTool READ floatingTool WRITE setFloatingTool)
	Q_PROPERTY(bool enableOpacityChange READ enableOpacityChange WRITE setEnableOpacityChange)
	Q_PROPERTY(bool displayWindowIcon READ displayWindowIcon WRITE setDisplayWindowIcon)

	friend class VipToolWidgetTitleBar;

public:
	VipToolWidget(VipMainWindow* window);
	~VipToolWidget();

	/// @brief Set the internal widget with its preferred orientation.
	void setWidget(QWidget* widget, Qt::Orientation widget_orientation = Qt::Vertical);
	QWidget* widget() const;

	/// @brief Take and return the internal widget.
	QWidget* takeWidget();

	/// @brief Returns the title bar
	VipToolWidgetTitleBar* titleBarWidget() const;
	/// @brief Returns the internal scroll area
	QScrollArea* scrollArea() const;

	/// @brief Returns the VipWidgetResizer object used to resize this tool widget
	VipWidgetResizer* resizer() const;

	/// @brief Enable/disable opacity change.
	/// If true, the widget will be semi transparent if it loses the focus.
	bool enableOpacityChange() const;

	/// @brief Set/get floating.
	/// Make the tool widget floating.
	bool floatingTool() const;

	/// @brief Set an action that will trigger the tool widget visibility.
	/// If take_icon is true, the tool widget top left icon will be set
	/// to the QAction's icon.
	void setAction(QAction* action, bool take_icon = true);
	QAction* action() const;

	/// @brief Set button that will trigger the tool widget visibility.
	void setButton(QAbstractButton* action, bool take_icon = true);
	QAbstractButton* button() const;

	/// @brief Enable/disable top left icon display
	bool displayWindowIcon() const;
	void setDisplayWindowIcon(bool);

	/// When floating, tells if the widget should keep the size set manually by the user.
	///  Otherwise, when hidding and showing back the widget, its size will be set to its sizeHint() (default behavior).
	///  A few tool widgets need this, like the console that might be expanded by the user for a better reading.
	void setKeepFloatingUserSize(bool enable);
	bool keepFloatingUserSize() const;

	void raise();
	void setFocus();

	virtual QSize sizeHint() const;

	/// Reimplement this function if the tool widget should handle a change in the current #VipDisplayPlayerArea.
	///  When reimplementing this function, the new implementation should call the base class implementation to keep built-in compatibility.
	virtual void setDisplayPlayerArea(VipDisplayPlayerArea*) {}

	/// @brief Returns the VipToolWidgetToolBar (if any) associated with this tool widget.
	/// It will be displayed on the top part of the main window (VipMainWindow).
	virtual VipToolWidgetToolBar* toolBar() { return nullptr; }

public Q_SLOTS:
	void setEnableOpacityChange(bool enable);
	void setFloatingTool(bool f);
	void floatWidget() { setFloatingTool(true); }
	void unfloatWidget() { setFloatingTool(false); }
	void showAndRaise()
	{
		show();
		raise();
	}
	/// @brief Reset the size of this tool widget
	/// and make sure it stays in the creen boundaries.
	/// Should be called by inheriting classes when they
	/// change their contents.
	void resetSize();

private Q_SLOTS:
	void displayPlayerAreaChanged();
	void internalResetSize();
	void focusChanged(QWidget* old, QWidget* now);
	void setVisibleInternal(bool vis);
	void floatingChanged(bool);
	void polish();

protected:
	void setStyleProperty(const char* name, bool value);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	virtual void enterEvent(QEvent*);
#else
	virtual void enterEvent(QEnterEvent*);
#endif
	virtual void leaveEvent(QEvent* event);
	virtual void closeEvent(QCloseEvent* evt);
	// virtual void paintEvent(QPaintEvent * evt);
	virtual void showEvent(QShowEvent*);
	virtual void hideEvent(QHideEvent*);
	virtual void resizeEvent(QResizeEvent* evt);
	virtual void moveEvent(QMoveEvent* evt);

private:
	VIP_DECLARE_PRIVATE_DATA();
};

/// @brief A VipToolWidget which is linked to a VipDisplayPlayerArea or to a VipAbstractPlayer
///
/// Subclasses must reimplement setPlayer() member to update the widget's content based on provided player.
///
class VIP_GUI_EXPORT VipToolWidgetPlayer : public VipToolWidget
{
	Q_OBJECT

public:
	VipToolWidgetPlayer(VipMainWindow*);

	VipAbstractPlayer* currentPlayer() const;
	virtual void setDisplayPlayerArea(VipDisplayPlayerArea*);
	virtual bool setPlayer(VipAbstractPlayer*) = 0;

	void setAutomaticTitleManagement(bool);
	bool automaticTitleManagement() const;

private Q_SLOTS:
	void focusWidgetChanged(VipDragWidget*);

protected:
	virtual void showEvent(QShowEvent*);

private:
	QPointer<VipAbstractPlayer> m_player;
	QPointer<VipDisplayPlayerArea> m_area;
	bool m_automaticTitleManagement;
};

class QBoxLayout;
class QGraphicsObject;

/// @brief A VipToolWidgetPlayer that displays edition widget
/// in order to customize the look of QGraphicsObject (usually
/// a VipPlotItem object).
class VIP_GUI_EXPORT VipPlotToolWidgetPlayer : public VipToolWidgetPlayer
{
	Q_OBJECT

public:
	VipPlotToolWidgetPlayer(VipMainWindow*);
	~VipPlotToolWidgetPlayer();

	virtual bool setPlayer(VipAbstractPlayer*);

	void setItem(QGraphicsObject* item);

	virtual bool eventFilter(QObject* watched, QEvent* evt);

private:
	VIP_DECLARE_PRIVATE_DATA();
};
/// @brief Returns the VipPlotToolWidgetPlayer tool widget
VipPlotToolWidgetPlayer* vipGetPlotToolWidgetPlayer(VipMainWindow* window = nullptr);

class VipProgress;
class VipProgressWidget;

/// @brief A VipToolWidget that displays one or more progress bars.
///
/// These progress bars are created/removed on creation/destruction
/// of VipProgress objects.
///
class VIP_GUI_EXPORT VipMultiProgressWidget : public VipToolWidget
{
	Q_OBJECT
	friend class VipProgress;
	friend class VipProgressWidget;

public:
	VipMultiProgressWidget(VipMainWindow* window);
	~VipMultiProgressWidget();

	/// Returns, for the current progress bars displayed (from top to bottom),
	/// their current text and value (between 0 and 100)
	QMultiMap<QString, int> currentProgresses() const;

protected:
	virtual void closeEvent(QCloseEvent* evt);
	virtual void showEvent(QShowEvent*);
	virtual bool eventFilter(QObject * obj, QEvent * evt);
private Q_SLOTS:
	void addProgress(QObjectPointer p);
	void removeProgress(QObjectPointer p);
	void setText(QObjectPointer p, const QString& text);
	void setValue(QObjectPointer p, int value);
	void setCancelable(QObjectPointer p, bool cancelable);
	void setModal(QObjectPointer p, bool modal);

	void updateModality();
	void updateScrollBars();
	void cancelRequested();

private:
	void changeModality(Qt::WindowModality);

	void addProgressWidget(VipProgressWidget*, QThread* th);
	void removeProgressWidget(VipProgressWidget*);

	VIP_DECLARE_PRIVATE_DATA();
};
/// @brief Returns the VipMultiProgressWidget tool widget
VIP_GUI_EXPORT VipMultiProgressWidget* vipGetMultiProgressWidget(VipMainWindow* window = nullptr);

/// @}
// end Gui

#endif
