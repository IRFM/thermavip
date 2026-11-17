# -*- coding: utf-8 -*-

"""
Thermavip module

Provides high level functions to interact with a Thermavip session.
This module works within Thermavip as well as outside it, with another Python process
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


def __call_thermavip_fun(name, args, kwargs = {} ):
    args = (name,) + args
    return builtins.internal.call_internal_func(*args, **kwargs)


def call_thermavip_fun(name, args = tuple(), kwargs = {}):
    """
    Call a Thermavip function registered with vipRegisterFunction
    """
    return __call_thermavip_fun(name, args, kwargs)
        


PLOT_PLAYER = 0 # a 2D player containing curves, histograms,...
VIDEO_PLAYER = 1 # a 2D player displaying an image or a video
_2D_PLAYER = 2 # other type of 2D player
OTHER_PLAYER = 3 # other type of player

def player_type(player):
    """
    Returns type of given player (one of PLOT_PLAYER, VIDEO_PLAYER, _2D_PLAYER, OTHER_PLAYER)
    """
    return call_thermavip_fun('player_type',(player,))

def item_list(player, selection = 2, name=str()):
    """
    Returns the list of available items (images, curves...) within given player that partially match given name (all items if name is empty).
    If selection != 2, returns all selected (if selection == 1) or unselected (if selection == 0) items.
    """
    return call_thermavip_fun('item_list',(player,selection,name))

def set_selected(player, selected , name=str()):
    """
    Select or unselect an item in given player.
    The item is found based on given partial name.
    """
    return call_thermavip_fun('set_selected',(player,selected,name))

def unselect_all(player):
    """
    Unselect all items in given player.
    """
    return call_thermavip_fun('unselect_all',(player,))

def query(title = str(), default_value = str()):
    """
    Open a dialog box to query a pulse number or time range depending on the plugins installed.
    Returns the result as a string.
    """
    return call_thermavip_fun('query',(title,default_value))

def open(path, player = 0, side = ""):
    """
    Open given path in a new or existing player.
    The path could be a video/signal file or a signal from a database (like WEST and ITER ones).
    
    When called with one argument, the path is opened on a new player within the current workspace.
    
    If player is not 0, the path will be opened in an existing player with given ID.
    
    Optionally, you might specify a side to open the path around an existing player, and therefore
    create a multiplayer. The side argument could be either 'left', 'right', 'top' or 'bottom'.
    
    Returns the ID of the player on which the path was opened. Throw an exception on error.
    """
    return call_thermavip_fun('open',(path,player,side))
    
def open_data(data, player = 0, side = ""):
    """
    Open given data(s) in a new or existing player.
    
    When called with one argument, the data is opened on a new player within the current workspace.
    
    If player is not 0, the data will be opened in an existing player with given ID.
    
    Optionally, you might specify a side to open the data around an existing player, and therefore
    create a multiplayer. The side argument could be either 'left', 'right', 'top' or 'bottom'.
    
    Returns the ID of the player on which the data was opened. Throw an exception on error.
    """
    return call_thermavip_fun('open_data',(data,player,side))


def close(player):
    """
    Close the player within given ID.
    """
    return call_thermavip_fun('close',(player,))

def show(player):
    """
    Restaure the state of a player that was maximized or minimized.
    """
    return call_thermavip_fun('show_normal',(player,))

def maximize(player):
    """
    Show maximized given player.
    """
    return call_thermavip_fun('show_maximized',(player,))

def minimize(player):
    """
    Minimize given player.
    """
    return call_thermavip_fun('show_minimized',(player,))

def workspace(wks = 0):
    """
    Create or switch workspace.
    If wks is 0, create a new workspace and returns its ID. The current workspace is set to the new one.
    If wks is > 0, the function set the current workspace to given workspace ID.
    Returns the ID of the current workspace, or throw an exception on error.
    """
    return call_thermavip_fun('workspace',(wks,))

def workspaces():
    """
    Returns the list of all available workspaces.
    """
    return call_thermavip_fun('workspaces',())

def current_workspace():
    """
    Returns the ID of current workspace.
    """
    return call_thermavip_fun('current_workspace',())

def workspace_title(wks):
    """
    Returns the workspace title for given workspace id
    """
    return call_thermavip_fun('workspace_title',(wks,))

def set_workspace_title(wks, title):
    """
    Set the workspace title for given workspace id
    """
    return call_thermavip_fun('set_workspace_title',(wks,title))

def resize_workspace():
    """
    Resize all windows in the current workspace
    """
    return call_thermavip_fun('resize_workspace',())

def set_time_markers(start, end):
    """
    Set the current workspace time markers
    """
    return call_thermavip_fun('set_time_markers',(start,end))
    
def remove_time_markers():
    """
    Remove the time markers from the current workspace
    """
    return call_thermavip_fun('remove_time_markers',())

def time():
    """
    Returns the current time in nanoseconds within the current workspace.
    """
    return call_thermavip_fun('time',())

def set_time(time, ref = 'absolute'):
    """
    Set the time (in nanoseconds) in current workspace.
    If ref == 'relative', the time is considered as an offset since the minimum time of the workspace.
    """
    return call_thermavip_fun('set_time',(time,ref))

def next_time(time):
    """
    Returns the next valid time (in nanoseconds) for the current workspace.
    Returns an invalid time if no next time can be computed.
    """
    return call_thermavip_fun('next_time',(time,))

def previous_time(time):
    """
    Returns the previous valid time (in nanoseconds) for the current workspace.
    Returns an invalid time if no previous time can be computed.
    """
    return call_thermavip_fun('previous_time',(time,))

def closest_time(time):
    """
    Returns the closest valid time (in nanoseconds) for the current workspace.
    Returns an invalid time if no closest time can be computed.
    """
    return call_thermavip_fun('closest_time',(time,))

def is_valid_time(time):
    """
    Returns True if given time is valid, False otherwise.
    """
    return time != -9223372036854775807

def time_range():
    """
    Returns the time range [first,last] within the current workspace.
    """
    return call_thermavip_fun('time_range',())

def clamp_time( xy, min_time, max_time):
    """
    Clamp input signal to remove samples outside [min_time,max_time] interval.
    Input array should be a 2d numpy array of shape (2, number_of_samples)
    The first row should contain time values and the second row the sample values.
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
    
    the stylesheet a string value containing a CSS-like code. Example:
        
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
    
        11. text: additional text to display on top of the curve. You can define as many additional texts as you need using the *[]* operator. In addition to the text string, you can define the the optional text color and background color:
                	
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
    3. symbol: the symbol style, on of 'none' (no symbol, default), 'ellipse', 'rect', 'diamond', ''triangle', 'dtriangle', 'utriangle', 'rtriangle', 'ltriangle', 'cross', 'xcross', 'hline', 'vline', 'star1', 'star2', 'hexagon'.
    4. symbolsize: width and height of the symbol.
    5. symbolborder: outer pen of the symbol.
    6. symbolbackground: inner color of the symbol.
    7. baseline: curve baseline value (only used if a background color is set with the 'background' property).
    
    Images define the following additional attributes:
    
    1. colormap: the spectrogram color map, one of 'autumn', 'bone', 'cool', 'copper', 'gray', 'hot', 'hsv', 'jet', 'fusion', 'pink', 'spring', 'summer', 'white', 'winter'.
       

    """
    return call_thermavip_fun('set_stylesheet',(player, stylesheet,data_name))

def top_level(player):
    """
    For given player ID inside a multi-player, returns the ID of the top level window.
    This ID can be used to maximize/minimize the top level multi-player.
    """
    return call_thermavip_fun('top_level',(player,))

def load(path, time = None):
    """
    Loads and returns the data from given path.
    The path could be any path supported by Thermavip.
    The returned object is a dictionary containing:
    - All attributes of the loaded path
    - The data itself with the key 'data'
    - The data time with the key 'time'.
    """
    return call_thermavip_fun('load',(path,time))

def get(player, data_name = ""):
    """
    Returns the data (usually a numpy array) associated to given player and item data name.
    The plot item is found using the player ID and the plot item name. If no name is given, the 
    first item data found is retuned. Note that only a partial name is required.
    The returned data will be the one of the first item matching the partial name.
    """
    return call_thermavip_fun('get',(player,data_name))

def clicked_point():
    """
    Returns the last curve clicked point (list(x,y))
    """
    return call_thermavip_fun('clicked_point',tuple())

def get_attribute(player, attribute_name, data_name = ""):
    """
    Returns the data attribute associated to given player and item data name.
    The plot item is found using the player ID and the plot item name. If no name is given, the 
    first item data found is retuned. Note that only a partial name is required.
    The returned attribute will be the one of the first item matching the partial name.
    """
    return call_thermavip_fun('get_attribute',(player,attribute_name,data_name))

def get_attributes(player, data_name = ""):
    """
    Returns the attributes associated to given player and item data name.
    The plot item is found using the player ID and the plot item name. If no name is given, the 
    first item data found is retuned. Note that only a partial name is required.
    """
    return call_thermavip_fun('get_attributes',(player,data_name))

def set_attribute(player, attribute_name, attribute_value, data_name = ""):
    """
    Set the data attribute associated to given player and item data name.
    The plot item is found using the player ID and the plot item name. If no name is given, the 
    first item data found is retuned. Note that only a partial name is required.
    """
    return call_thermavip_fun('set_attribute',(player,attribute_name,attribute_value, data_name))

def get_roi(player, group, roi_id, yaxis = ""):
    """
    Returns the ROI polygon associated to given player, ROI group (usually 'ROI', 'Polylines' or 'Points')
    and ROI identifier.
    For plot player only, yaxis specifies on which left scale to search for the ROI (in case of stacked plots).
    The result is tuple of (y_values, x_values)
    """
    return call_thermavip_fun('get_roi',(player,group,roi_id,yaxis))
    
def get_roi_bounding_rect(player, group, roi_id, yaxis = ""):
    """
    Returns the ROI bounding rect as a tuple (left,top,width,height) associated to given player, ROI group (usually 'ROI', 'Polylines' or 'Points')
    and ROI identifier.
    For plot player only, yaxis specifies on which left scale to search for the ROI (in case of stacked plots).
    """
    return call_thermavip_fun('get_roi_bounding_rect',(player,group,roi_id,yaxis))

def get_roi_filled_points(player, group, roi_id):
    """
    Returns the ROI covered points associated to given video player, ROI group (usually 'ROI', 'Polylines' or 'Points')
    and ROI identifier.
    """
    return call_thermavip_fun('get_roi_filled_points',(player,group,roi_id))

def clear_roi(player, yaxis = ""):
    """
    Remove all ROIs (as well as annotations) in given player.
    For plot player only, yaxis specifies on which left scale to search for the ROI (in case of stacked plots).
    """
    return call_thermavip_fun('clear_roi',(player,yaxis))

def add_roi(player, roi,yaxis = ""):
    """
    Add a ROI inside given player based on a filename or a list of points.
    For plot player only, yaxis specifies on which left scale to search for the ROI (in case of stacked plots).
    The roi argument could be:
        - A 2D array (first row is polygon y, second is polygon x)
        - A list of 1D arrays (first index is polygon y, second is polygon x)
        - A ROI filename (usualy a xml file)
        
    If given polygon has one point only, it creates a point ROI (group 'Points').
    If given polygon does not end with the first point, it creates a polyline (group 'Polylines').
    Otherwise, it creates a polygon ROI (group 'ROI').
    
    Returns a string containing 'group:roi_id'.
    """
    return call_thermavip_fun('add_roi',(player,roi,yaxis))

def add_ellipse(player, ellipse, yaxis = ""):
    """
    Add a an ellipse inside given player based on the tuple (left,top,width,height).
    For plot player only, yaxis specifies on which left scale to search for the ROI (in case of stacked plots).
    Returns a string containing 'group:roi_id'.
    """
    return call_thermavip_fun('add_ellipse',(player,ellipse,yaxis))
    
def add_circle(player, y, x, radius, yaxis = ""):
    """
    Add a a circle inside given player based on the center (y and x) and its radius.
    For plot player only, yaxis specifies on which left scale to search for the ROI (in case of stacked plots).
    Returns a string containing 'group:roi_id'.
    """
    return call_thermavip_fun('add_circle',(player,y,x,radius,yaxis))

def time_trace(roi_player, rois, **kwargs ):
    """
    Extract the ROI(s) time trace.
    player is the input video player.
    rois is the list of ROIs (list of string of the form 'ROI_group:ROI_id') to extract the time trace from.
    
    This function can take extra arguments:
         - 'skip': number of frames to skip (1 out of skip). Default to 1.
         - 'multi': management of several shapes: 0 = union of all shapes, 1 = intersection, 2 = compute time traces separately (default).
         - 'player' : output plot player id.
         - 'statistics' : string that can mix 'min', 'max', 'mean' (like 'max|mean')
         
    Returns the id of output plot player
    """
    return call_thermavip_fun('time_trace',(roi_player,rois),kwargs)


def remove(player, data_name):
    """
    Remove, from given palyer, all plot items matching given (potentially partial) data name.
    Returns the number of item removed.
    """
    return call_thermavip_fun('remove',(player,data_name))

def set_time_marker(player, enable):
    """
    Show/hide the time marker for given plot player.
    """
    return call_thermavip_fun('set_time_marker',(player,enable))
    
def set_row_ratio(row, ratio):
    """
    For the current workspace, change the height of given row to represent given ratio based on the full height
    """
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
    """
    For given plot player, returns the list [min_x_value, max_x_value] for the union of all visible curves.
    """
    return call_thermavip_fun('x_range',(player,))

def auto_scale(player, enable):
    """
    Enable/disable automatic scaling for given player
    """
    return call_thermavip_fun('auto_scale',(player, enable))

def set_x_scale(player, minvalue, maxvalue):
    """
    Set the min and max value of the bottom scale for given plot player
    """
    return call_thermavip_fun('set_x_scale',(player, minvalue, maxvalue))

def set_y_scale(player, minvalue, maxvalue, unit=""):
    """
    Set the min and max value of the Y scale for given plot player.
    If the player has multiple stacked Y scales, unit is used to find the right one.
    """
    return call_thermavip_fun('set_y_scale',(player, minvalue, maxvalue,unit))

def player_range(player):
    """
    For plot player, returns the list [min, max] of the temporal scale.
    For video player, returns the list [min time,max time] of the underlying video device.
    """
    return call_thermavip_fun('player_range',(player,))

def set_title(player,title):
    """
    Set the given player title.
    """
    return call_thermavip_fun('set_title',(player,title))


def annotation(player,style,text,pos,**kwargs):
    """
    Create an annotation inside given player.

    @param player player id
    @param style annotation style: 'line', 'arrow', 'rectangle', 'ellipse', or 'textbox'
    @param text annotation text
    @param pos annotation position on the form [y1,x1,y2,x2] for 'line', 'arrow', 'rectangle' and 'ellipse,
    or [y1,x1] for 'line' (single point) and 'textbox' (top left position of the text)
    @param kwargs additional annotation attributes:
    	- "textcolor" : annotation text color (uses stylesheet mechanism)
    	- "textbackground" : annotation text background color
    	- "textborder" : annotation text outline (border box pen)
    	- "textradius" : annotation text border radius of the border box
    	- "textsize" : size in points of the text font
    	- "bold" : use bold font for the text
    	- "italic" : use italic font for the text
    	- "fontfamilly": font familly for the text
    	- "border" : shape pen
    	- "background" : shape brush
    	- "distance" : distance between the annotation text and the shape
    	- "alignment" : annotation text alignment around the shape (combination of 'left', 'right', 'top', 'bottom', 'hcenter', vcenter', 'center')
    	- "position" : text position around the shape (combination of 'xinside', 'yinside', 'inside', 'outside')
    	- "symbol" : for 'line' only, symbol for the end point (one of 'none', 'ellipse', 'rect', 'diamond', 'triangle', 'dtriangle', 'utriangle', 'ltriangle', 'rtriangle', 'cross', 'xcross', 'hline', 'vline', 'star1', 'star2', 'hexagon')
    	- "symbolsize" : for 'line' and 'arrow', symbol size for the end point

    Returns the annotation id on success.
    """
    return call_thermavip_fun('annotation',(player,style,text,pos),kwargs)

def remove_annotation(annotation_id):
    """
    Remove annotation with given id
    """
    return call_thermavip_fun('remove_annotation',(annotation_id,))

def clear_annotations(player, all_annotations = False):
    """
    Clear annotation for given player.
    If all_annotations is False, only annotations created with Python function annotation() are cleared.
    Otherwise, all annotations (including manual ones) are removed.
    """
    return call_thermavip_fun('clear_annotations',(player,all_annotations))

def imshow(array,**kwargs):
    """
    Display an image in a new or existing player.
    @param array the numpy image
    @param kwargs additional arguments:
        - 'title': the image title
        - 'unit': image unit (as displayed on the colorbar)
        - 'player': output player id (display in an existing player). A value of 0 means to create a new player.
        
    Returns the output player id on success.
    
    To control more features on how the image is displayed, use the stylesheet mechanism.
    """
    return call_thermavip_fun('imshow',(array,),kwargs)

def plot(xy,**kwargs):
    """
    Plot a xy signal.
    @param xy the signal to plot. It must be a 2D array or list of [x,y].
    @param kwargs additional arguments:
        - 'title': the plot title
        - 'unit': the plot Y unit
        - 'xunit': the plot X unit
        - 'player': output player id (display in an existing player). A value of 0 means to create a new player.
        - 'symbol': point symbol (see set_stylesheet function for more details)
        - 'symbolsize': point symbol size (see set_stylesheet function for more details)
        - 'symbolborder': point symbol border color (see set_stylesheet function for more details)
        - 'symbolbackground': point symbol background color (see set_stylesheet function for more details)
        - 'border': curve border color (see set_stylesheet function for more details)
        - 'background': curve background color (see set_stylesheet function for more details)
        - 'style': curve style (see set_stylesheet function for more details)
        - 'color': curve and symbol color all at once (see set_stylesheet function for more details)
        - 'baseline': curve baseline value used to draw the background (if any) (see set_stylesheet function for more details)
        
    Returns the output player id on success.
    
    To control more features on how the curve is displayed, use the stylesheet mechanism.
    """
    return call_thermavip_fun('plot',(np.array(xy),),kwargs)

def plots(xys,**kwargs):
    """
    Plot several xy signals in one pass.
    The arguments are the same as for plot function, but all of them must be a list of N elements, even the attributes.
    Returns a list of player id (one per curve).
    """	
    lst = tuple(np.array(xy) for xy in xys)
    return call_thermavip_fun('plots',(lst,),kwargs)

def add_function(player, fun, function_name, item_name = str()):
    """
    Add a function that will be applied in the object processing list.
    The function will be displayed in the processing list with given name.
    Use item_name to distinguish between curves.
    The function signature should be: def my_fun(data):->return data
    """
    return call_thermavip_fun('add_function',(player, fun, function_name, item_name))

def get_function(player, function_name, item_name = str()):
    """
    Returns a function object set with add_function
    """
    return call_thermavip_fun('get_function',(player, function_name, item_name))

def resample(signals, strategy = "union", step = 0, padd=0):
    """
    Resample multiple signals on the form [x1,y1,x2,y2,...] in order for them to share the same
    x values.
    """
    return call_thermavip_fun('resample',(signals, strategy, step,padd))
    
    
def user_input(title, *args):
    """
    Queries values from user using a dialog box.
    This function can query multiple values of type int, float, bool and str.
    Each value to query is represented by a tuple ('name','type',default_value, [optional arguments] )
    
    Examples:
    
    To query an int: user_input('my title', ('my label','int',2)) 
    To query an int with spin box range [0:10] and signle step of 1: user_input('my title', ('my label','int',2, (0,10,1))) 
    
    To query a float: user_input('my title', ('my label','float',2.0)) 
    To query a float with spin box range [0.1:10.1] and signle step of 0.1: user_input('my title', ('my label','float',2.1, (0.1,10.1,0.1)))

    To query a boolean value: user_input('my title', ('my label','bool',True))

    To query a string object: user_input('my title', ('my label','str','default value'))
    To query a string object among predefined values: user_input('my title', ('my label','str','value1',('value1','value2','value3')))
    
    To query a folder: user_input('my title', ('my label','folder','default_path'))
    To query a file for opening: user_input('my title', ('my label','ifile','default_file'))
    To query a file with filters: user_input('my title', ('my label','ifile','default_file','Image files (*.jpg *.png) ;; Data files (*.dat)'))
    To query a file for writing: user_input('my title', ('my label','ofiles,'default_file')). Note that filters can also be used.
    
    To query multiple values at once: user_input('my title', ('my label','str','default value'), ('my label','float',2.0), ('my label','int',2, (0,10,1)))
    
    Returns a list of values, or None is the user click on "Cancel".
    """
    return call_thermavip_fun('user_input',(title, *args))

def add_widget_to_player(player, widget, side):
    """
    Add a widget (from PySide2 or PyQt5) to a side of given player.
    side could be one of 'left', 'right', 'top', 'bottom'.

    Only works from inside Thermavip: the native side must provide
    'add_widget_to_player'. Raises AttributeError if it does not.
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
    """
    Returns the current player's id or 0 if no player available
    """
    return call_thermavip_fun('current_player')
        

def set_workspace_max_columns(max_cols):
    """
    Set the maximum number of players in one column for the current workspace.
    This applies to all new players, not the ones currently available.
    Call reorganize() to modify the workspace layout based on this maximum column size.
    """
    return call_thermavip_fun('setCurrentWorkspaceMaxColumns',(max_cols,))
    
def workspace_max_columns():
    """
    Returns the maximum column size for the current workspace.
    """
    return call_thermavip_fun('currentWorkspaceMaxColumns')
    
def current_pulse():
    """
    Returns the current selected pulse number
    """
    return call_thermavip_fun('getCurrentPulse')
    
