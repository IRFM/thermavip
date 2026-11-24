# -*- coding: utf-8 -*-

"""Python interface to a Thermavip session.
"""

import sys
import builtins
import ctypes
import threading
import time as time_module
import types
import inspect
import io
import pickle
import struct
import numpy as np
from .Shape import VipShape

VipInvalidTime = -np.iinfo(np.int64).max
VipInvalidPosition = VipInvalidTime
VipMinTime = VipInvalidTime + 1
VipMaxTime = np.iinfo(np.int64).max


class VipAnyData:
    """A processing payload with a time in nanoseconds and a dictionary of attributes.
    Pass instances to open() to supply per-item metadata.
    """

    def __init__(self, *args, **kwargs):
        """Create an empty value or wrap one positional payload.

        Passing another VipAnyData reuses its payload and attributes dictionary and
        copies its time. Keywords are applied afterward. data and time override
        those fields (time is converted to numpy.int64). name, x_unit, y_unit,
        z_unit, and stylesheet use the corresponding property setters. All other
        keywords become entries in attributes. More than one positional argument
        raises AttributeError.
        """
        self.data = None
        self.time = VipInvalidTime
        self.attributes = {}

        if len(args) > 1:
            raise AttributeError("VipAnyData: invalid args")

        if len(args) == 1:
            d = args[0]
            if isinstance(d, VipAnyData):
                self.data = d.data
                self.time = d.time
                self.attributes = d.attributes
            else:
                self.data = args[0]

        for name, value in kwargs.items():
            if name == "data":
                self.data = value
            elif name == "time":
                self.time = np.int64(value)
            elif name == "name":
                self.name = value
            elif name == "x_unit":
                self.x_unit = value
            elif name == "y_unit":
                self.y_unit = value
            elif name == "z_unit":
                self.z_unit = value
            elif name == "stylesheet":
                self.stylesheet = value
            else:
                self.attributes[name] = value

    def __str__(self):
        """Format data, time, and attributes as a dictionary string."""
        return str(
            {"data": self.data, "time": self.time, "attributes": self.attributes}
        )

    def __repr__(self):
        """Return the dictionary representation of data, time, and attributes."""
        return str(
            {"data": self.data, "time": self.time, "attributes": self.attributes}
        )

    #
    # Several usefull properties stored within the 'attributes' dictionary
    #

    @property
    def name(self):
        """Metadata stored under 'Name'. Reading a missing entry returns an empty
        string.
        """
        try:
            return self.attributes["Name"]
        except:
            return str()

    @name.setter
    def name(self, n):
        """Store the string representation of n under 'Name' in attributes."""
        self.attributes["Name"] = str(n)

    @property
    def x_unit(self):
        """Metadata stored under 'XUnit'. Reading a missing entry returns an empty
        string.
        """
        try:
            return self.attributes["XUnit"]
        except:
            return str()

    @x_unit.setter
    def x_unit(self, n):
        """Store the string representation of n under 'XUnit' in attributes."""
        self.attributes["XUnit"] = str(n)

    @property
    def y_unit(self):
        """Metadata stored under 'YUnit'. Reading a missing entry returns an empty
        string.
        """
        try:
            return self.attributes["YUnit"]
        except:
            return str()

    @y_unit.setter
    def y_unit(self, n):
        """Store the string representation of n under 'YUnit' in attributes."""
        self.attributes["YUnit"] = str(n)

    @property
    def z_unit(self):
        """Metadata stored under 'ZUnit'. Reading a missing entry returns an empty
        string.
        """
        try:
            return self.attributes["ZUnit"]
        except:
            return str()

    @z_unit.setter
    def z_unit(self, n):
        """Store the string representation of n under 'ZUnit' in attributes."""
        self.attributes["ZUnit"] = str(n)

    @property
    def stylesheet(self):
        """Metadata stored under 'stylesheet'. Reading a missing entry returns an
        empty string.
        """
        try:
            return self.attributes["stylesheet"]
        except:
            return str()

    @stylesheet.setter
    def stylesheet(self, n):
        """Store the string representation of n under 'stylesheet' in attributes."""
        self.attributes["stylesheet"] = str(n)


class VipPointVector:
    """Curve coordinates for open(). x and y hold NumPy arrays of corresponding
    coordinates.
    """

    def __init__(self, x=None, y=None):
        """Convert x and y independently with numpy.array()."""
        self.x = np.array(x)
        self.y = np.array(y)


class VipIntervalSampleVector:
    """Histogram samples for open(). Each intervals entry is a (minimum, maximum) pair
    with a corresponding entry in values.
    """

    def __init__(self, intervals=None, values=None):
        """Convert intervals and values to arrays. The constructor does not check
        their shapes.
        """
        self.intervals = np.array(intervals)
        self.values = np.array(values)


def __call_thermavip_fun(name, args, kwargs={}):
    """Dispatch name and the supplied arguments through
    builtins.internal.call_internal_func.
    """
    args = (name,) + args
    return builtins.internal.call_internal_func(*args, **kwargs)


def call_thermavip_fun(name, args=tuple(), kwargs={}):
    """Call a function registered with vipRegisterFunction and return its result. Pass
    positional arguments as a tuple and keywords as a dictionary.
    """
    return __call_thermavip_fun(name, args, kwargs)




class Player:
    """
    Class wrapping a VipAbstractPlayer C++ instance within Thermavip
    """

    # Legend position, use with the 'legend_position' property
    LegendHidden = 0
    LegendBottom = 1
    LegendInnerTopLeft = 2
    LegendInnerTopRight = 3
    LegendInnerBottomLeft = 4
    LegendInnerBottomRight = 5

    def __init__(self, player_id=0):
        """
        Construct from a player Id.
        Construct from the current focus player if given Id is null.
        """
        if player_id == 0:
            player_id = call_thermavip_fun("current_player")

        wks = call_thermavip_fun("player_workspace", (player_id,))
        if wks == 0:
            raise RuntimeError("invalid player id")
        self.__id = player_id

    @staticmethod
    def current():
        """Return the current player"""
        return Player(call_thermavip_fun("current_player"))

    @property
    def id(self):
        """Returns the player id within Thermavip"""
        return self.__id

    @property
    def workspace(self) -> int:
        """ Returns given player workspace
        """
        return Workspace(call_thermavip_fun("player_workspace", (self.__id,)))

    @property
    def title(self):
        """Return the player title.
        """
        return call_thermavip_fun("title", (self.__id,))

    @title.setter
    def title(self, title):
        """Set the player title.
        """
        return call_thermavip_fun("set_title", (self.__id, title))

    def close(self):
        """Close the player"""
        call_thermavip_fun("close", (self.__id,))
        self.__id = 0

    def show(self):
        """Restore a maximized or minimized player to its normal state."""
        return call_thermavip_fun("show_normal", (self.__id,))

    def maximize(self):
        """Maximize the player"""
        return call_thermavip_fun("show_maximized", (self.__id,))

    def minimize(self):
        """Minimize the player"""
        return call_thermavip_fun("show_minimized", (self.__id,))

    def select(self) -> int:
        """ Make this player the focus one.
        This might change the current workspace.
        """
        return call_thermavip_fun("set_current_player", (self.__id,))

    def item_list(self, selection=2, name=str()):
        """List items in player whose names contain name. An empty name matches all. Use
        selection=1 for selected items, 0 for unselected, or 2 for both.
        """
        return call_thermavip_fun("item_list", (self.__id, selection, name))

    def set_selected(self, selected: bool, name=str()):
        """Set the selection state of a player item identified by a partial name."""
        return call_thermavip_fun("set_selected", (self.__id, selected, name))

    def unselect_all(self):
        """Clear the selection in player."""
        return call_thermavip_fun("unselect_all", (self.__id,))

    @property
    def type_name(self) -> str:
        """Returns the player type name (C++ class)"""
        return call_thermavip_fun("player_type", (self.__id,))

    @property
    def tool_bar(self):
        """Returns the player's tool bar (if any)"""
        return call_thermavip_fun("player_tool_bar", (self.__id,))

    def as_image(self) -> np.ndarray:
        """Returns the player's content as an image"""
        return call_thermavip_fun("player_as_image", (self.__id,))

    
    def remove(self, data_name : str):
        """Remove all items in player matching the partial data_name and return the number
        removed.
        """
        return call_thermavip_fun("remove", (self.__id, data_name))

    def set_color_map_scale(self, vmin, vmax, gripMin=0, gripMax=0):
        """
        Change the color map scale (video player only).
    
        vmin and vmax are the new scale boundaries. If vmin == vmax, the current
        scale boundaries are kept unchanged.
    
        gripMin and gripMax are the new slider grip boundaries. If gripMin == gripMax,
        the grip boundaries are kept unchanged.
        """
        return call_thermavip_fun(
            "set_color_map_scale", (self.__id, vmin, vmax, gripMin, gripMax)
        )

    @property
    def color_map(self) -> str:
        """Returns the current video player color map
        """
        return call_thermavip_fun("player_color_map", (self.__id, ))

    @color_map.setter
    def color_map(self, name : str):
        """Set the video player color map (like 'jet', 'sunset' ...)
        """
        return call_thermavip_fun("set_player_color_map", (self.__id, name ))

    @property
    def color_map_flat_histogram(self) -> int:
        """Returns the current video player color map histogram flattening strength (0 to 5).
        0 means that histogram flattening is disabled.
        """
        return call_thermavip_fun("color_map_flat_histogram", (self.__id, ))

    @color_map_flat_histogram.setter
    def color_map_flat_histogram(self, strength : int):
        """Set the video player color map histogram flattening strength (0 to 5).
        0 means that histogram flattening is disabled.
        """
        return call_thermavip_fun("set_color_map_flat_histogram", (self.__id, strength ))

    @property
    def time_marker(self) -> bool:
        return call_thermavip_fun("time_marker", (self.__id,))

    @time_marker.setter
    def time_marker(self, enable : bool):
        """Show or hide the time marker in a plot player."""
        call_thermavip_fun("set_time_marker", (self.__id, enable))

    @property
    def canvas(self):
        """For a plot player, return the number of vertically stacked plot area.
        Return 1 for other type of player.
        """
        return call_thermavip_fun("plot_canvas", (self.__id,))

    def x_scale(self):
        """For plot/video player, return the x scale range.
        """
        return call_thermavip_fun("x_scale", (self.__id,))

    def y_scale(self, canvas = 0):
        """For plot/video player, return the y scale range.
        For a stacked plot, canvas identifies the vertically stacked index from the top.
        """
        return call_thermavip_fun("y_scale", (self.__id,canvas))

    def zoom(self, x1, x2, y1 , y2 , canvas = 0):
        """
        Zoom/unzoom on a specific area for a video/plot player.
    
        The zoom is applied on the rectangle defined by x1, x2, y1 and y2.
        If x1 == x2, the zoom is only applied on y component.
        If y1 == y2, the zoom is only applied on x component.
    
        For a plot player with multiple stacked y scales, the unit parameter
        tells which y scale to use for the zoom.
    
        Note that, for plot players displaying a time scale, the x values provided
        should be in nanoseconds.

        For a stacked plot, canvas identifies the vertically stacked index from the top.
        """
        return call_thermavip_fun("zoom", (self.__id, x1, x2, y1, y2, canvas))

    
    def get(self, data_name= str()) -> VipAnyData:
        """Return the payload for the first matching item in player. data_name can be a
        partial name. An empty name selects the first item.
        """
        return call_thermavip_fun("get", (self.__id, data_name))

    def get_shape(self, shape_id : str, canvas = 0):
        """Return the VipShape with given id ('group:id').
        For a stacked plot, canvas identifies the vertically stacked index from the top.
        """
        return call_thermavip_fun("get_shape", (self.__id,shape_id, canvas))

    def clear_shapes(self, canvas = 0):
        """Remove all shapes from player. 
        For a stacked plot, canvas identifies the vertically stacked index from the top.
        """
        return call_thermavip_fun("clear_shapes", (self.__id, canvas))


    def add_shape(self, shape : VipShape, shape_id = str(), canvas = 0):
        """Add or replace a shape and returns its 'group:roi_id' string.
        For a stacked plot, canvas identifies the vertically stacked index from the top.
        """
        return call_thermavip_fun("add_shape", (self.__id, shape, shape_id, canvas ))

    def remove_shape(self, shape_id : str, canvas = 0):
        """Remove a shape using its 'group:roi_id' string.
        For a stacked plot, canvas identifies the vertically stacked index from the top.
        """
        return call_thermavip_fun("remove_shape", (self.__id, shape_id, canvas ))

    def auto_scale(self, enable):
        """Enable or disable automatic scaling for player."""
        return call_thermavip_fun("auto_scale", (self.__id, enable))

    def time_trace(self, rois, **kwargs):
        """Extract ROI statistics over time from a video player and return the output plot
        player ID.

        rois contains 'group:roi_id' strings. Use skip to sample one frame in every
        skip frames (default 1). Set multi to 0 for the union of the regions, 1 for
        their intersection, or 2 for separate traces (default). player selects the
        output plot. statistics combines 'min', 'max', and 'mean' with a vertical bar,
        for example 'max|mean'.
        """
        return call_thermavip_fun("time_trace", (self.__id, rois), kwargs)

    
    @property
    def legend_position(self) -> int:
        """Return the current legend position.
        """
        return call_thermavip_fun("legend_position", (self.__id, ))

    @legend_position.setter
    def legend_position(self, position : int):
        """Return whether the player's legend is visible.
        """
        return call_thermavip_fun("set_legend_position", (self.__id, position))

    def grid_visible(self, canvas = 0) -> int:
        """Return whether the player's grid is visible.
        """
        return call_thermavip_fun("grid_visible", (self.__id,canvas))

    def set_grid_visible(self, visible : bool, canvas = 0):
        """Set the player's grid visibility.
        """
        return call_thermavip_fun("set_grid_visible", (self.__id, visible, canvas))

    @property
    def auto_color_map(self):
        """Return whether the player's color map adjust itself automatically.
        """
        return call_thermavip_fun("auto_color_map", (self.__id,))

    @auto_color_map.setter
    def auto_color_map(self, enable):
        """Enable/disable automatic color map.
        """
        return call_thermavip_fun("set_auto_color_map", (self.__id,enable))
    
    def add_processing(self, proc, processing_name : str, item_name = str()):
        """Add a VipProcessingObject to an item's processing list under function_name. 
        Use item_name to distinguish curves/itms.
        """
        return call_thermavip_fun("add_processing", (self.__id, proc, processing_name, item_name))


    def get_processing(self, processing_name = str, item_name = str()):
        """Retrieve a VipProcessingObject added with add_processing() by player, function_name, and
        optional item_name.
        """
        return call_thermavip_fun("get_processing", (self.__id, processing_name, item_name))

    def remove_processing(self, processing_name = str, item_name = str()):
        """Remove and return a VipProcessingObject added with add_processing() by player, function_name, and
        optional item_name. Return None is the player does not contain such processing.
        """
        return call_thermavip_fun("remove_processing", (self.__id, processing_name, item_name))


    def set_stylesheet(self, stylesheet, data_name=""):
        """
        Set the stylesheet for a curve/histogram/image/other-object within a player.
        The stylesheet is used to customize the look'n feel of a plot item (pen, brush, symbol, color map, 
        title, axis units,...)
    
        The plot item is found using the player ID and the plot item name. If no name is given, the 
        style sheet is applied to the full plot area. 
        Note that only a partial name is required. The stylesheet will be applied to the first item matching the partial name.
    
        The stylesheet is a string of CSS-like declarations. Example:
        
           #Set the output curve style sheet
           stylesheet = \
           \"\"\"
           border: 1.5px dash red;
           symbol:ellipse;
           symbolsize: 7;
           symbolborder: magenta;
           symbolbackground: transparent;
           title:'my new title';
           \"\"\"
       
        Note that each attribute is separated by a semicolon.
       
        For any kind of item (image, curve, histogram,...) the style sheet defines the following attributes:
    
            1. renderhint: rendering property, one of 'antialiasing', 'highQualityAntialiasing' or 'noAntialiasing'
            2. color: the item global color. The color could be a predefined color (among 'black', 'white', 'red', 'darkRed', 'green', 'darkGreen', 'blue', 'darkBlue', 'cyan', 'darkCyan', 'magenta', 'darkMagenta', 'yellow', 'darkYellow', 'gray', 'darkGray', 'lightGray', 'transparent') or a custom one. A custom color is set through its RGBA values (like 'rgb(128,125,8)' or 'rgba(128,125,8,255)') or using its hexadecimal representation (like '#5634CC').
            3. border: the item outer pen. The pen is a combination of width (in pixels), style (among 'solid', 'dash', 'dot', 'dashdot' and 'dashdotdot') and color. Example:
           	
    	         border: 1.5px solid blue;
    
               Note that the pen can contain any combination of width, style and color. At least one value (a color, a width or a style) is mandatory.
            4. selectionborder: the pen used when the item is selected.
            5. background: the background color.
            6. selected: a boolean value (either '0', '1', 'true' or 'false') telling if the item is selected or not.
            7. visible: a boolean value (either '0', '1', 'true' or 'false') telling if the item is visible or not.
            8. tooltip: a string value representing the item tool tip (displayed when hovering the item).
            9. title: the item's title.
            10. axisunit: the unit of given axis. Example for a curve:
        	
                axisunit[0]: 'time'; /*set the X unit*/ 
                axisunit[1]: 'MW'; /*set the Y unit*/ 
    
            11. text: additional text to display on top of the curve. You can define as many additional texts as you need using the *[]* operator. In addition to the text string, you can define the optional text color and background color:
                	
        	        text[0] : 'my first text' red blue; /*Red text on blue background*/
        	        text[1] : 'my second text'; /*simple black text*/
        		
        	        /*Set the first text position and alignment*/
        	        textposition[0]: xinside|yinside;
        	        textalignment[0]: top|right;
        
            12. textposition: position of additional text as regards to the item bounding rect. Could be a combination of *outside*, *xinside*, *yinside*, *inside*, *xautomatic*, *yautomatic*, *automatic*, with a '|' separator. 
            13. textalignment: additional text alignment, combination of *left*, *right*, *hcenter*, *top*, *bottom*, *vcenter*, *center*, with a '|' separator.
    
        Curves define the following additional attributes:
    
        1. style: style of the curve line, one of 'noCurve', 'lines', 'sticks', 'steps', 'dots'.
        2. attribute: additional curve attributes, combination of 'inverted' (for steps only), 'closePolyline' (join first and last curve points).
        3. symbol: the symbol style, one of 'none' (no symbol, default), 'ellipse', 'rect', 'diamond', 'triangle', 'dtriangle', 'utriangle', 'rtriangle', 'ltriangle', 'cross', 'xcross', 'hline', 'vline', 'star1', 'star2', 'hexagon'.
        4. symbolsize: width and height of the symbol.
        5. symbolborder: outer pen of the symbol.
        6. symbolbackground: inner color of the symbol.
        7. baseline: curve baseline value (only used if a background color is set with the 'background' property).
    
        Images define the following additional attributes:
    
        1. colormap: the spectrogram color map, one of 'autumn', 'bone', 'cool', 'copper', 'gray', 'hot', 'hsv', 'jet', 'fusion', 'pink', 'spring', 'summer', 'white', 'winter'.
       

        """
        return call_thermavip_fun("set_stylesheet", (self.__id, stylesheet, data_name))

    def add_widget_to_player(self, widget, side):
        """Attach a Qt widget to a player inside Thermavip, then show it.

        side is 'left', 'right', 'top', or 'bottom'. The native hook receives the
        widget with a generated object name and its original name. If the hook raises,
        the wrapper restores the original name and propagates the exception. A missing
        native hook raises AttributeError.
        """
        import uuid

        oname = widget.objectName()
        # A unique name, where a millisecond timestamp gave two widgets added in the
        # same millisecond the same one.
        wname = "_vip_pywidget_" + uuid.uuid4().hex
        add = getattr(builtins.internal, "add_widget_to_player", None)
        if add is None:
            # The name used to be set first, so a call that could not work left the
            # user's widget carrying a generated objectName and nothing restored it.
            raise AttributeError(
                "the native module does not provide 'add_widget_to_player'"
            )
        widget.setObjectName(wname)
        try:
            add(self.__id, side, wname, oname, widget)
        except BaseException:
            widget.setObjectName(oname)
            raise
        widget.show()



class Workspace:
    """
    Class representing a workspace within Thermavip.
    """

    RowMajor = 0
    ColumnMajor = 1

    def __init__(self, _id = 0):

        if _id == 0:
            _id = call_thermavip_fun("current_workspace", ())
        # Check workspace
        call_thermavip_fun("workspace_title", (_id,))
        self.__id = _id

    @staticmethod
    def make() :
        """Create and select a workspace.
        """
        return Workspace(call_thermavip_fun("workspace", (0,)))

    @staticmethod
    def all():
        """Return the available workspaces.
        """
        return [Workspace(i) for i in call_thermavip_fun("workspaces", ())]

    @staticmethod
    def set_max_columns(max_cols : int):
        """Set all workspaces maximum column size for newly added players.
        """
        call_thermavip_fun("setWorkspaceMaxColumns", (max_cols,))

    @staticmethod
    def max_columns() -> int:
        """Return the maximum column size configured for all workspaces.
        """
        return call_thermavip_fun("workspaceMaxColumns")

    @property
    def id(self) -> int:
        """Returns the player id within Thermavip"""
        return self.__id

    @property
    def pos(self) -> int:
        """Returns the workspace position (index) from left side.
        """
        return call_thermavip_fun("workspace_pos", (self.__id,))

    @pos.setter
    def pos(self, idx):
        """Change workspace position (index) from the left side.
        """
        call_thermavip_fun("set_workspace_pos", (self.__id, idx))

    @property
    def orientation(self) -> int:
        """Get the workspace orientation: RowMajor (default) or ColumnMajor.
        """
        return call_thermavip_fun("workspace_orientation", (self.__id, ))

    @orientation.setter
    def orientation(self, o):
        """Set the workspace orientation: RowMajor (default) or ColumnMajor.
        """
        call_thermavip_fun("set_workspace_orientation", (self.__id, o))

    @property
    def floating(self) -> bool:
        """Return whether this workspace is floating or not.
        """
        return call_thermavip_fun("workspace_floating", (self.__id, ))

    @floating.setter
    def floating(self, enable: bool):
        """Make this workspace floating or attach it to the main widget.
        """
        self.__id = call_thermavip_fun("set_workspace_floating", (self.__id, enable ))

    @property
    def left_tool_bar(self):
        """Returns the workspace tab bar left tool bar (left to workspace title).
        """
        return call_thermavip_fun("workspace_left_tool_bar", (self.__id,))

    @property
    def right_tool_bar(self):
        """Returns the workspace tab bar right tool bar (right to workspace title).
        """
        return call_thermavip_fun("workspace_right_tool_bar", (self.__id,))

    def select(self):
        """Select this workspace.
        """
        call_thermavip_fun("workspace", (self.__id,))

    @property
    def players(self) -> list:
        """Returns available players within this workspace
        """
        return [Player(i)  for i in call_thermavip_fun("workspace_players", (self.__id,))]

    def player(self, player_title : str):
        """Return player with given name.
        """
        for p in self.players:
            if p.title == player_title:
                return p
        raise RuntimeError("player not found")

    def as_session(self):
        """Save this workspace as a sessoin file and return the session content.
        """
        return call_thermavip_fun("workspace_as_session", (self.__id,))

    def as_image(self) -> np.ndarray:
        """Return this workspace content as an image.
        """
        return call_thermavip_fun("workspace_as_image", (self.__id,))

    def close(self):
        """Close this workspace.
        """
        call_thermavip_fun("close_workspace", (self.__id,))
        self.__id = 0

    @property
    def title(self):
        """Return the workspace title.
        """
        return call_thermavip_fun("workspace_title", (self.__id,))

    @title.setter
    def title(self, title: str):
        """Set the workspace title.
        """
        call_thermavip_fun("set_workspace_title", (self.__id, title))

    def resize(self):
        """Resize all windows in this workspace."""
        call_thermavip_fun("resize_workspace", (self.__id,))

    
    def set_row_ratio(self, row, ratio):
        """Set a workspace row's height as a fraction of the full workspace height."""
        return call_thermavip_fun("set_row_ratio", (self.__id, row, ratio))

    def set_time_markers(self, start: int, end: int):
        """Set the start and end time markers in this workspace.
        """
        call_thermavip_fun("set_time_markers", (self.__id, start, end))

    def remove_time_markers(self):
        """Remove time markers.
        """
        call_thermavip_fun("remove_time_markers", (self.__id,))


    def time(self) -> int:
        """Return the workspace time in nanoseconds.
        """
        return call_thermavip_fun("time", (self.__id,))

    def set_time(self, time: int, ref="absolute"):
        """Set the workspace time in nanoseconds. With ref='relative', time is an offset
        from the workspace's minimum time.
        """
        return call_thermavip_fun("set_time", (self.__id, time, ref))

    def next_time(self, time: int) -> int:
        """Return the next valid workspace time in nanoseconds relative to time, or an
        invalid time if unavailable.
        """
        return call_thermavip_fun("next_time", (self.__id, time,))

    def previous_time(self, time: int) -> int:
        """Return the previous valid workspace time in nanoseconds relative to time, or an
        invalid time if unavailable.
        """
        return call_thermavip_fun("previous_time", (self.__id, time,))

    def closest_time(self, time: int) -> int:
        """Return the closest valid workspace time in nanoseconds relative to time, or an
        invalid time if unavailable.
        """
        return call_thermavip_fun("closest_time", (self.__id, time,))

    @property
    def time_range(self):
        """Return the current workspace's [first, last] time range."""
        return call_thermavip_fun("time_range", (self.__id,))

    def play(self, enable: bool):
        """Start playback in the workspace when enable is true, or stop it when
        false.
        """
        call_thermavip_fun("play", (self.__id, enable,))

    def stream(self, enable: bool):
        """Start or stop streaming in the current workspace according to enable.
        """
        call_thermavip_fun("stream", (self.__id, enable,))

    def repeat(self, enable: bool):
        """Enable or disable repeated playback in the current workspace.
        """
        call_thermavip_fun("repeat", (self.__id, enable,))

    def widget(self):
        """Return the workspace widget using qtpy.
        """
        return call_thermavip_fun("workspace_widget", (self.__id,))

    





def log_info(text: str):
    """Write text to the Thermavip log at info level."""
    call_thermavip_fun("log_info", (text,))


def log_warning(text: str):
    """Write text to the Thermavip log at warning level."""
    call_thermavip_fun("log_warning", (text,))


def log_error(text: str):
    """Write text to the Thermavip log at error level."""
    call_thermavip_fun("log_error", (text,))


def query(title=str(), default_value=str()):
    """Prompt for a pulse number or time range and return the result as a string.
    Available queries depend on installed plugins.
    """
    return call_thermavip_fun("query", (title, default_value))


def open(*args, **kwargs):
    """Open the positional data objects or paths together and return the player ID.

    Supported inputs include NumPy arrays (images), VipPointVector (curves),
    VipIntervalSampleVector (histograms), VTK objects, VipShape, VipSceneModel, VipAnyData, and paths
    recognized by Thermavip, including session files.

    Supply player to target an existing player ID. With a player ID, side can be
    'left', 'right', 'top', or 'bottom' to place data beside it. name, x_unit,
    y_unit, z_unit, and stylesheet describe the displayed data. Wrap individual
    items in VipAnyData to specify distinct metadata.

    For plot player only, additional argument 'canvas' represents the vertically stacked
    canvas index starting from 0 (top).

    A successful load that creates no dedicated player returns 0. Errors from
    Thermavip propagate to the caller.
    """
    try:
        if type(kwargs['player']) == Player:
            kwargs['player'] = kwargs['player'].id
    except:
        pass
    return call_thermavip_fun("open", args, kwargs)

def clicked_point():
    """Return the last clicked curve point as [x, y]."""
    return call_thermavip_fun("clicked_point", tuple())


def resample(signals, strategy="union", step=0, padd=0):
    """Resample alternating coordinate and value arrays [x1, y1, x2, y2, ...] onto
    shared x coordinates. strategy, step, and padd are passed to the host.
    """
    return call_thermavip_fun("resample", (signals, strategy, step, padd))


def user_input(title, *args):
    """Prompt for one or more values and return them as a list, or None on
    cancellation.

    Each positional field is a tuple (label, type, default[, options]). Use 'int'
    or 'float' with an optional (minimum, maximum, step) tuple, 'bool' for a
    checkbox, or 'str' with an optional sequence of choices. Path fields include
    'folder' for a directory and 'ifile' for an input file. An input file field can
    include a filter string.

        user_input("Settings", ("Count", "int", 2, (0, 10, 1)),
                   ("Mode", "str", "fast", ("fast", "slow")))
        user_input("Image", ("Path", "ifile", "", "Images (*.jpg *.png)"))
    """
    return call_thermavip_fun("user_input", (title, *args))

def current_pulse():
    """Return the currently selected pulse number."""
    return call_thermavip_fun("getCurrentPulse")

def set_current_pulse(pulse):
    """Set the current pulse number."""
    return call_thermavip_fun("setCurrentPulse", (pulse,))

def pulse_option_menu():
    """Return the QMenu used to display pulse-related features
    """
    return call_thermavip_fun("getPulseOptionMenu")