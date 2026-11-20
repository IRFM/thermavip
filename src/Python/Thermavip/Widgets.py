import Thermavip as th
if not th.call_thermavip_fun("_can_load_qtpy"):
    raise ImportError("cannot import qtpy as Thermavip was built with Qt debug")
    
from qtpy import QtCore, QtWidgets
from qtpy.QtCore import Qt
from .QtCpp import *

import builtins


def get_main_window() -> QtWidgets.QMainWindow:
    """
    Returns the main Thermavip window
    """
    return th.call_thermavip_fun("get_main_window")

def get_generate_menu() -> QtWidgets.QMenu:
    """
    Returns the "Generate signals" menu.
    Use this function to add new features to this menu.
    """
    return th.call_thermavip_fun("get_generate_menu")

def get_tool_widgets() -> list:
    """
    Returns the list of available tool widgets (VipToolWidget class) within Thermavip.
    """
    return th.call_thermavip_fun("get_tool_widgets")

def get_tool_widget(name) -> QtWidgets.QDockWidget:
    """
    Returns the tool widget (VipToolWidget class) with given name or None.
    """
    return th.call_thermavip_fun("get_tool_widget", (name,))

def add_tool_widget(widget, floating = False, orientation = Qt.Orientation.Horizontal, \
                    area = Qt.DockWidgetArea.LeftDockWidgetArea, allowed_areas = Qt.DockWidgetArea.LeftDockWidgetArea | Qt.DockWidgetArea.RightDockWidgetArea ):
    
    return th.call_thermavip_fun("add_tool_widget", (widget,floating,orientation.value, area.value, allowed_areas.value ))