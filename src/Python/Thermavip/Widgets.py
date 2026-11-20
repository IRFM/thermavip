"""Qt access to the Thermavip main window, menus, and dock widgets.

Import raises ImportError when the host reports it cannot load qtpy.
"""

import Thermavip as th
if not th.call_thermavip_fun("_can_load_qtpy"):
    raise ImportError("cannot import qtpy as Thermavip was built with Qt debug")
    
from qtpy import QtCore, QtWidgets
from qtpy.QtCore import Qt
from .QtCpp import *

import builtins


def get_main_window() -> QtWidgets.QMainWindow:
    """Return the application's main QMainWindow."""
    return th.call_thermavip_fun("get_main_window")

def get_generate_menu() -> QtWidgets.QMenu:
    """Return the Generate signals QMenu so a plugin can add actions to it."""
    return th.call_thermavip_fun("get_generate_menu")

def get_tool_widgets() -> list:
    """Return the list of tool widgets available in Thermavip."""
    return th.call_thermavip_fun("get_tool_widgets")

def get_tool_widget(name) -> QtWidgets.QDockWidget:
    """Return the tool widget identified by name, or None if it is unavailable."""
    return th.call_thermavip_fun("get_tool_widget", (name,))

def add_tool_widget(widget, floating = False, orientation = Qt.Orientation.Horizontal, \
                    area = Qt.DockWidgetArea.LeftDockWidgetArea, allowed_areas = Qt.DockWidgetArea.LeftDockWidgetArea | Qt.DockWidgetArea.RightDockWidgetArea ):
    """Pass widget and its docking options to Thermavip and return the host result.

    floating defaults to False. orientation defaults to horizontal, area to the
    left dock area, and allowed_areas to the left and right areas. Supply Qt enum
    or flag values for these last three arguments.
    """
    
    return th.call_thermavip_fun("add_tool_widget", (widget,floating,orientation.value, area.value, allowed_areas.value ))