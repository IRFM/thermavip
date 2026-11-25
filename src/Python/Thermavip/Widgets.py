"""Qt access to the Thermavip main window, menus, and dock widgets.

Import raises ImportError when the host reports it cannot load qtpy.
"""

import Thermavip as th
if not th.call_thermavip_fun("_can_load_qtpy"):
    raise ImportError("cannot import qtpy as Thermavip was built with Qt debug")
    
from qtpy import QtCore, QtWidgets
from qtpy.QtCore import Qt, Q_ARG
from .QtCpp import *

import builtins


class VipMainWindow:
    """
    Class wrapping the C++ VipMainWindow one.
    This class always wrap the main Thermavip window, and multiple instances of VipMainWindow can live simultaneously.
    """

    def __init__(self):
        # Ensure we have a widget
        self.widget

    @property
    def widget(self) -> QtWidgets.QMainWindow:
        """Return the application's main QMainWindow."""
        return th.call_thermavip_fun("get_main_window")

    @property
    def generate_menu(self) -> QtWidgets.QMenu:
        """Return the Generate signals QMenu so a plugin can add actions to it."""
        return th.call_thermavip_fun("get_generate_menu")

    @property
    def tool_widgets(self) -> list:
        """Return the list of tool widgets available in Thermavip."""
        return th.call_thermavip_fun("get_tool_widgets")

    def tool_widget(self, name : str) -> QtWidgets.QDockWidget:
        """Return the tool widget identified by name, or None if it is unavailable."""
        return th.call_thermavip_fun("get_tool_widget", (name,))

    @property
    def icon_bar(self)  -> QtWidgets.QToolBar:
        """Return the icon tool bar (top left area)"""
        return th.call_thermavip_fun("main_child_widget", ("Icon bar",))

    @property
    def close_bar(self)  -> QtWidgets.QToolBar:
        """Return the close tool bar (top right area)"""
        return th.call_thermavip_fun("main_child_widget", ("Close bar",))

    @property
    def tools_bar(self)  -> QtWidgets.QToolBar:
        """Return the vertical tool widgets tool bar (left area)"""
        return th.call_thermavip_fun("main_child_widget", ("Tool widgets bar",))

    @property
    def file_bar(self)  -> QtWidgets.QToolBar:
        """Return the files tool bar (top left area)"""
        return th.call_thermavip_fun("main_child_widget", ("File tool bar",))

    @property
    def title(self):
        """Returns the main window title"""
        return self.widget.property("mainTitle")

    @title.setter
    def title(self, text):
        """Set the main window title"""
        return self.widget.metaObject().invokeMethod(self.widget,"setMainTitle",Qt.AutoConnection ,Q_ARG(str,text))

    def set_title_icon(self, path: str):
        """Set the icon displayed on the left of the title."""
        return self.widget.metaObject().invokeMethod(self.widget,"setMainIconPath",Qt.AutoConnection ,Q_ARG(str,path))

    @property
    def margin(self):
        """Returns the main window margin"""
        return self.widget.property("margin")

    @margin.setter
    def margin(self, m):
        """Set the main window margin"""
        return self.widget.metaObject().invokeMethod(self.widget,"setMargin",Qt.AutoConnection ,Q_ARG(int,m))

    
    @property
    def workspaces_maximized(self) -> bool:
        """Tell whether workspaces are maximized"""
        return self.widget.property("workspacesMaximized")

    @workspaces_maximized.setter
    def workspaces_maximized(self, maximize : bool):
        """Enable/disable workspace maximization"""
        self.widget.metaObject().invokeMethod(self.widget,"maximizeWorkspaces",Qt.AutoConnection ,Q_ARG(bool,maximize))

    def restart(self):
        """Restart the application"""
        self.widget.metaObject().invokeMethod(self.widget,"restart",Qt.AutoConnection)

    def as_session(self):
        """Save Thermavip as a session file and return the session content.
        """
        return th.call_thermavip_fun("main_as_session")

    def show_about_dialog(self):
        """Show the about dialog box"""
        self.widget.metaObject().invokeMethod(self.widget,"aboutDialog",Qt.AutoConnection)

    def show_options(self):
        """Open the preferences workspace"""
        self.widget.metaObject().invokeMethod(self.widget,"showOptions",Qt.AutoConnection)

    def add_tool_widget(self, widget, floating = False, orientation = Qt.Orientation.Horizontal, \
                        area = Qt.DockWidgetArea.LeftDockWidgetArea, allowed_areas = Qt.DockWidgetArea.LeftDockWidgetArea | Qt.DockWidgetArea.RightDockWidgetArea ):
        """Pass widget and its docking options to Thermavip and return the host result.

        floating defaults to False. orientation defaults to horizontal, area to the
        left dock area, and allowed_areas to the left and right areas. Supply Qt enum
        or flag values for these last three arguments.

        If widget has the member focusPlayerChanged(), this creates a VipToolWidgetPlayer instead of a regular VipToolWidget.
        focusPlayerChanged() will be called each time the focus player changes, and the widget will be enabled only if
        a focus player is available. Use VipPlayer class to get the focus player.
        """
    
        return th.call_thermavip_fun("add_tool_widget", (widget,floating,orientation.value, area.value, allowed_areas.value ))

    def add_page_option(self, widget, category : str, icon = str()):
        """Add a page to the global preferences.

        Passed widget must have the members 'applyPage()' to apply user inputs and 'updatePage()' 
        to update the widget based on actual options.
        """
        return th.call_thermavip_fun("add_page_option", (widget,category, icon ))