from qtpy import QtCore, QtWidgets
from .QtCpp import *
from Thermavip import *
import builtins


def get_main_window() -> QtWidgets.QMainWindow:
    """
    Returns the main Thermavip window
    """
    return call_thermavip_fun("get_main_window")

def get_generate_menu() -> QtWidgets.QMenu:
    """
    Returns the "Generate signals" menu.
    Use this function to add new features to this menu.
    """
    return call_thermavip_fun("get_generate_menu")

def get_tool_widgets() -> list:
    """
    Returns the list of available tool widgets (VipToolWidget class) within Thermavip.
    """
    return call_thermavip_fun("get_tool_widgets")

def get_tool_widget(name) -> QtWidgets.QDockWidget:
    """
    Returns the tool widget (VipToolWidget class) with given name or None.
    """
    return call_thermavip_fun("get_tool_widget", (name,))