# -*- coding: utf-8 -*-

"""Python interface to a Thermavip session.

Use open() to display data, workspace() to create or select a workspace, and the
player and ROI functions to inspect or update its contents. Session commands
delegate to the host through builtins.internal. VipAnyData carries a payload,
timestamp in nanoseconds, and metadata between processing objects. VipInvalidTime
and VipInvalidPosition mark unavailable times and positions.
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

VipInvalidTime = -np.iinfo(np.int64).max
VipInvalidPosition = VipInvalidTime
VipMinTime = VipInvalidTime + 1
VipMaxTime = np.iinfo(np.int64).max

class VipAnyData:
    """A processing payload with a time in nanoseconds and a dictionary of attributes.
    Pass instances to open() to supply per-item metadata.
    """
    
    def __init__(self,*args, **kwargs):
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
            if isinstance(d,VipAnyData):
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
        return str({"data":self.data, "time": self.time, "attributes": self.attributes})
    
    def __repr__(self):
        """Return the dictionary representation of data, time, and attributes."""
        return str({"data":self.data, "time": self.time, "attributes": self.attributes})
    
    
    ######################################################################
    # Several usefull properties stored within the 'attributes' dictionary
    ######################################################################
    
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
    def __init__(self, x = None, y = None):
        """Convert x and y independently with numpy.array()."""
        self.x = np.array(x)
        self.y = np.array(y)

class VipIntervalSampleVector:
    """Histogram samples for open(). Each intervals entry is a (minimum, maximum) pair
    with a corresponding entry in values.
    """
    def __init__(self, intervals = None, values = None):
        """Convert intervals and values to arrays. The constructor does not check
        their shapes.
        """
        self.intervals = np.array(intervals)
        self.values = np.array(values)


def __call_thermavip_fun(name, args, kwargs = {} ):
    """Dispatch name and the supplied arguments through
    builtins.internal.call_internal_func.
    """
    args = (name,) + args
    return builtins.internal.call_internal_func(*args, **kwargs)


def call_thermavip_fun(name, args = tuple(), kwargs = {}):
    """Call a function registered with vipRegisterFunction and return its result. Pass
    positional arguments as a tuple and keywords as a dictionary.
    """
    return __call_thermavip_fun(name, args, kwargs)
        


PLOT_PLAYER = 0 # a 2D player containing curves, histograms,...
VIDEO_PLAYER = 1 # a 2D player displaying an image or a video
_2D_PLAYER = 2 # other type of 2D player
OTHER_PLAYER = 3 # other type of player

def log_info(text : str):
    """Write text to the Thermavip log at info level."""
    call_thermavip_fun('log_info',(text,))

def log_warning(text : str):
    """Write text to the Thermavip log at warning level."""
    call_thermavip_fun('log_warning',(text,))

def log_error(text : str):
    """Write text to the Thermavip log at error level."""
    call_thermavip_fun('log_error',(text,))

def player_type(player):
    """Return the type code for a player ID (PLOT_PLAYER, VIDEO_PLAYER, _2D_PLAYER, or
    OTHER_PLAYER).
    """
    return call_thermavip_fun('player_type',(player,))

def item_list(player, selection = 2, name=str()):
    """List items in player whose names contain name. An empty name matches all. Use
    selection=1 for selected items, 0 for unselected, or 2 for both.
    """
    return call_thermavip_fun('item_list',(player,selection,name))

def set_selected(player, selected , name=str()):
    """Set the selection state of a player item identified by a partial name."""
    return call_thermavip_fun('set_selected',(player,selected,name))

def unselect_all(player):
    """Clear the selection in player."""
    return call_thermavip_fun('unselect_all',(player,))

def query(title = str(), default_value = str()):
    """Prompt for a pulse number or time range and return the result as a string.
    Available queries depend on installed plugins.
    """
    return call_thermavip_fun('query',(title,default_value))

def open(*args, **kwargs):
    """Open the positional data objects or paths together and return the player ID.

    Supported inputs include NumPy arrays (images), VipPointVector (curves),
    VipIntervalSampleVector (histograms), VTK objects, VipAnyData, and paths
    recognized by Thermavip, including session files.

    Supply player to target an existing player ID. With a player ID, side can be
    'left', 'right', 'top', or 'bottom' to place data beside it. name, x_unit,
    y_unit, z_unit, and stylesheet describe the displayed data. Wrap individual
    items in VipAnyData to specify distinct metadata.

    A successful load that creates no dedicated player returns 0. Errors from
    Thermavip propagate to the caller.
    """
    return call_thermavip_fun('open',args,kwargs)



def close(player):
    """Close the player identified by player."""
    return call_thermavip_fun('close',(player,))

def show(player):
    """Restore a maximized or minimized player to its normal state."""
    return call_thermavip_fun('show_normal',(player,))

def maximize(player):
    """Maximize the player with the given ID."""
    return call_thermavip_fun('show_maximized',(player,))

def minimize(player):
    """Minimize the player with the given ID."""
    return call_thermavip_fun('show_minimized',(player,))

def workspace(wks = 0):
    """Create and select a workspace when wks is 0, or select the workspace with the
    supplied positive ID. Return the current workspace ID.
    """
    return call_thermavip_fun('workspace',(wks,))

def workspaces():
    """Return the available workspaces."""
    return call_thermavip_fun('workspaces',())

def current_workspace():
    """Return the current workspace ID."""
    return call_thermavip_fun('current_workspace',())

def workspace_title(wks):
    """Return the title of workspace wks."""
    return call_thermavip_fun('workspace_title',(wks,))

def set_workspace_title(wks, title):
    """Set the title of workspace wks."""
    return call_thermavip_fun('set_workspace_title',(wks,title))

def resize_workspace():
    """Resize all windows in the current workspace."""
    return call_thermavip_fun('resize_workspace',())

def set_time_markers(start, end):
    """Set the start and end time markers in the current workspace."""
    return call_thermavip_fun('set_time_markers',(start,end))
    
def remove_time_markers():
    """Remove the current workspace's time markers."""
    return call_thermavip_fun('remove_time_markers',())

def time():
    """Return the current workspace time in nanoseconds."""
    return call_thermavip_fun('time',())

def set_time(time, ref = 'absolute'):
    """Set the workspace time in nanoseconds. With ref='relative', time is an offset
    from the workspace's minimum time.
    """
    return call_thermavip_fun('set_time',(time,ref))

def next_time(time):
    """Return the next valid workspace time in nanoseconds relative to time, or an
    invalid time if unavailable.
    """
    return call_thermavip_fun('next_time',(time,))

def previous_time(time):
    """Return the previous valid workspace time in nanoseconds relative to time, or an
    invalid time if unavailable.
    """
    return call_thermavip_fun('previous_time',(time,))

def closest_time(time):
    """Return the closest valid workspace time in nanoseconds relative to time, or an
    invalid time if unavailable.
    """
    return call_thermavip_fun('closest_time',(time,))

def is_valid_time(time):
    """Return whether time differs from the VipInvalidTime sentinel."""
    return time != VipInvalidTime

def time_range():
    """Return the current workspace's [first, last] time range."""
    return call_thermavip_fun('time_range',())

def play(enable: bool):
    """Start playback in the current workspace when enable is true, or stop it when
    false.
    """
    return call_thermavip_fun('play',(enable,))

def stream(enable: bool):
    """Start or stop streaming in the current workspace according to enable."""
    return call_thermavip_fun('stream',(enable,))

def repeat(enable: bool):
    """Enable or disable repeated playback in the current workspace."""
    return call_thermavip_fun('repeat',(enable,))

def clamp_time( xy, min_time, max_time):
    """Keep signal samples in the inclusive interval [min_time, max_time]. Supply a
    NumPy array of shape (2, n) with times in row 0 and values in row 1.
    """
    return call_thermavip_fun('clamp_time',(xy,min_time,max_time))

def set_stylesheet(player, stylesheet, data_name = ""):
    """
    Set the stylesheet for a curve/histogram/image within a player.
    The stylesheet is used to customize the look'n feel of a plot item (pen, brush, symbol, color map, 
    title, axis units,...)
    
    The plot item is found using the player ID and the plot item name. If no name is given, the 
    style sheet is applied to the first item found (normally the last inserted one). 
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
    return call_thermavip_fun('set_stylesheet',(player, stylesheet,data_name))

def top_level(player):
    """Return the enclosing top-level window ID for a player in a multi-player. Use
    that ID to minimize or maximize the window.
    """
    return call_thermavip_fun('top_level',(player,))

def load(path, time = None):
    """Load a supported path with the optional time argument. Return a dictionary with
    the payload under 'data', its timestamp under 'time', and the loaded
    attributes.
    """
    return call_thermavip_fun('load',(path,time))

def get(player, data_name = ""):
    """Return the payload for the first matching item in player. data_name can be a
    partial name. An empty name selects the first item.
    """
    return call_thermavip_fun('get',(player,data_name))

def clicked_point():
    """Return the last clicked curve point as [x, y]."""
    return call_thermavip_fun('clicked_point',tuple())

def get_attribute(player, attribute_name, data_name = ""):
    """Return attribute_name for the first matching item in player. data_name can be a
    partial name. An empty name selects the first item.
    """
    return call_thermavip_fun('get_attribute',(player,attribute_name,data_name))

def get_attributes(player, data_name = ""):
    """Return the attributes for the first matching item in player. data_name can be a
    partial name. An empty name selects the first item.
    """
    return call_thermavip_fun('get_attributes',(player,data_name))

def set_attribute(player, attribute_name, attribute_value, data_name = ""):
    """Set attribute_name to attribute_value for the first matching item in player.
    data_name can be a partial name. An empty name selects the first item.
    """
    return call_thermavip_fun('set_attribute',(player,attribute_name,attribute_value, data_name))

def get_roi(player, group, roi_id, yaxis = ""):
    """Return the polygon for group and roi_id as (y_values, x_values). For a stacked
    plot, yaxis identifies the left scale to search.
    """
    return call_thermavip_fun('get_roi',(player,group,roi_id,yaxis))
    
def get_roi_bounding_rect(player, group, roi_id, yaxis = ""):
    """Return (left, top, width, height) for the ROI identified by group and roi_id.
    For a stacked plot, yaxis selects the left scale.
    """
    return call_thermavip_fun('get_roi_bounding_rect',(player,group,roi_id,yaxis))

def get_roi_filled_points(player, group, roi_id):
    """Return the points covered by group and roi_id in a video player."""
    return call_thermavip_fun('get_roi_filled_points',(player,group,roi_id))

def clear_roi(player, yaxis = ""):
    """Remove all ROIs and annotations from player. For a stacked plot, yaxis selects
    the left scale.
    """
    return call_thermavip_fun('clear_roi',(player,yaxis))

def add_roi(player, roi,yaxis = ""):
    """Add a region from a file or coordinates and return its 'group:roi_id' string.

    Pass a filename, a two-row array, or two coordinate arrays. Supply y
    coordinates first and x coordinates second. One point creates a 'Points' ROI.
    An open path creates a 'Polylines' ROI. A path that repeats its first point at
    the end creates a polygon in 'ROI'. For stacked plots, yaxis identifies the
    left scale.
    """
    return call_thermavip_fun('add_roi',(player,roi,yaxis))

def add_ellipse(player, ellipse, yaxis = ""):
    """Add an ellipse specified as (left, top, width, height) and return
    'group:roi_id'. For stacked plots, yaxis selects the left scale.
    """
    return call_thermavip_fun('add_ellipse',(player,ellipse,yaxis))
    
def add_circle(player, y, x, radius, yaxis = ""):
    """Add a circle centered at (y, x) with the supplied radius and return
    'group:roi_id'. For stacked plots, yaxis selects the left scale.
    """
    return call_thermavip_fun('add_circle',(player,y,x,radius,yaxis))

def time_trace(roi_player, rois, **kwargs ):
    """Extract ROI statistics over time from a video player and return the output plot
    player ID.

    rois contains 'group:roi_id' strings. Use skip to sample one frame in every
    skip frames (default 1). Set multi to 0 for the union of the regions, 1 for
    their intersection, or 2 for separate traces (default). player selects the
    output plot. statistics combines 'min', 'max', and 'mean' with a vertical bar,
    for example 'max|mean'.
    """
    return call_thermavip_fun('time_trace',(roi_player,rois),kwargs)


def remove(player, data_name):
    """Remove all items in player matching the partial data_name and return the number
    removed.
    """
    return call_thermavip_fun('remove',(player,data_name))

def set_time_marker(player, enable):
    """Show or hide the time marker in a plot player."""
    return call_thermavip_fun('set_time_marker',(player,enable))
    
def set_row_ratio(row, ratio):
    """Set a workspace row's height as a fraction of the full workspace height."""
    return call_thermavip_fun('set_row_ratio',(row,ratio))

def zoom(player, x1, x2, y1 = 0, y2 = 0, unit = ""):
    """
    Zoom/unzoom on a specific area for a video/plot player.
    
    The zoom is applied on the rectangle defined by x1, x2, y1 and y2.
    If x1 == x2, the zoom is only applied on y component.
    If y1 == y2, the zoom is only applied on x component.
    
    For a plot player with multiple stacked y scales, the unit parameter
    tells which y scale to use for the zoom.
    
    Note that, for plot players displaying a time scale, the x values provided
    should be in nanoseconds.
    """
    return call_thermavip_fun('zoom',(player,x1,x2,y1,y2,unit))

def set_color_map_scale(player, vmin, vmax, gripMin = 0, gripMax = 0):
    """
    Change the color map scale for given video player.
    
    vmin and vmax are the new scale boundaries. If vmin == vmax, the current
    scale boundaries are kept unchanged.
    
    gripMin and gripMax are the new slider grip boundaries. If gripMin == gripMax,
    the grip boundaries are kept unchanged.
    """
    return call_thermavip_fun('set_color_map_scale',(player,vmin,vmax,gripMin,gripMax))


def x_range(player):
    """Return [min_x, max_x] spanning all visible curves in a plot player."""
    return call_thermavip_fun('x_range',(player,))

def auto_scale(player, enable):
    """Enable or disable automatic scaling for player."""
    return call_thermavip_fun('auto_scale',(player, enable))

def set_x_scale(player, minvalue, maxvalue):
    """Set the lower and upper limits of a plot player's bottom scale."""
    return call_thermavip_fun('set_x_scale',(player, minvalue, maxvalue))

def set_y_scale(player, minvalue, maxvalue, unit=""):
    """Set the vertical scale limits of a plot player. Use unit to select among
    stacked scales.
    """
    return call_thermavip_fun('set_y_scale',(player, minvalue, maxvalue,unit))

def player_range(player):
    """Return [min, max] for a plot player's time scale or a video player's underlying
    device.
    """
    return call_thermavip_fun('player_range',(player,))

def set_title(player,title):
    """Set the title of player."""
    return call_thermavip_fun('set_title',(player,title))


def annotation(player,style,text,pos,**kwargs):
    """Create an annotation in player and return its annotation ID.

    style accepts 'line', 'arrow', 'rectangle', 'ellipse', or 'textbox'. Supply pos
    in [y1, x1, y2, x2] order for a two-point shape. Use [y1, x1] for a
    single-point line or the top-left corner of a textbox.

    Keyword options are forwarded to the host. Text options include textcolor,
    textbackground, textborder, textradius, textsize (points), bold, and italic.
    Use border and background for the shape pen and brush, distance for the gap
    between text and shape, and alignment and position for text placement.
    alignment combines 'left', 'right', 'top', 'bottom', 'hcenter', 'vcenter', or
    'center'. position combines 'xinside', 'yinside', 'inside', or 'outside'.

    For a line, symbol selects the endpoint marker. Choices include 'none',
    'ellipse', 'rect', 'diamond', 'triangle', 'dtriangle', 'utriangle',
    'ltriangle', 'rtriangle', 'cross', 'xcross', 'hline', 'vline', 'star1',
    'star2', and 'hexagon'. symbolsize sets the endpoint size for lines and arrows.
    """
    return call_thermavip_fun('annotation',(player,style,text,pos),kwargs)

def remove_annotation(annotation_id):
    """Remove the annotation identified by annotation_id."""
    return call_thermavip_fun('remove_annotation',(annotation_id,))

def clear_annotations(player, all_annotations = False):
    """Clear annotations created with annotation() from player. Pass
    all_annotations=True to include manually created annotations.
    """
    return call_thermavip_fun('clear_annotations',(player,all_annotations))

def add_function(player, fun, function_name, item_name = str()):
    """Add fun(data) to an item's processing list under function_name. The callback
    returns the processed data. Use item_name to distinguish curves.
    """
    return call_thermavip_fun('add_function',(player, fun, function_name, item_name))

def get_function(player, function_name, item_name = str()):
    """Retrieve a callback added with add_function() by player, function_name, and
    optional item_name.
    """
    return call_thermavip_fun('get_function',(player, function_name, item_name))

def resample(signals, strategy = "union", step = 0, padd=0):
    """Resample alternating coordinate and value arrays [x1, y1, x2, y2, ...] onto
    shared x coordinates. strategy, step, and padd are passed to the host.
    """
    return call_thermavip_fun('resample',(signals, strategy, step,padd))
    
    
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
    return call_thermavip_fun('user_input',(title, *args))

def add_widget_to_player(player, widget, side):
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
    wname = '_vip_pywidget_' + uuid.uuid4().hex
    add = getattr(builtins.internal, 'add_widget_to_player', None)
    if add is None:
        # The name used to be set first, so a call that could not work left the
        # user's widget carrying a generated objectName and nothing restored it.
        raise AttributeError("the native module does not provide 'add_widget_to_player'")
    widget.setObjectName(wname)
    try:
        add(player, side, wname, oname, widget)
    except BaseException:
        widget.setObjectName(oname)
        raise
    widget.show()



def current_player():
    """Return the current player ID, or 0 if no player is available."""
    return call_thermavip_fun('current_player')
        

def set_workspace_max_columns(max_cols):
    """Set the current workspace's maximum column size for newly added players."""
    return call_thermavip_fun('setCurrentWorkspaceMaxColumns',(max_cols,))
    
def workspace_max_columns():
    """Return the maximum column size configured for the current workspace."""
    return call_thermavip_fun('currentWorkspaceMaxColumns')
    
def current_pulse():
    """Return the currently selected pulse number."""
    return call_thermavip_fun('getCurrentPulse')
    
