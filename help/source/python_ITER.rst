Python scripting
================

.. |python_editor| image:: images/icons/start_streaming.png

Thermavip provides several ways to use custom Python scripts to process signals or interact with the main interface. To apply a Python script to one or multiple curves/images, see the :ref:`processing <processings>` section.

Thermavip also provides a minimal Python editor as a :ref:`tool widget <tools>`. Use the |python_editor| icon from the main tool bar to show it:

.. _python_editor:  

.. figure:: images/python_editor.png
   :alt: Thermavip Python editor
   :figclass: align-center
   :align: center
   :scale: 50%
   
   *Thermavip Python editor*

1. Create new file,
2. Open Python file,
3. Save current file,
4. Save current file as,
5. Save all files,
6. Unindent selected lines,
7. Indent selected lines,
8. Comment selected lines,
9. Uncomment selected lines,
10. Save and Run current file,
11. Stop running file,
12. Python code editor,
13. Search panel, open with CTRL+F,
14. Python console. The Python file is launch within this console.

When saving a new file, the default path is the *Scripts* one within the *Thermavip* user folder (usually ~/.thermavip/Python/Scripts on linux). If you save the Python file inside the *Scripts* folder, it will be available as a shortcut from the |python_editor| button from the main tool bar (press the bottom arrow).

Any Python code could be launched from the Python editor, but its main goal is to run code that interact with the main Thermavip interface.
For this purpose, the *Thermavip* Python module is provided with the following functions:

.. code-block:: python

	# -*- coding: utf-8 -*-
	"""

	"""


	import sys
	import builtins


	def query(title = str(), default_value = str()):
		"""
		Open a dialog box to query a pulse number or time range depending on the plugins installed.
		Returns the result as a string.
		"""
		return builtins.internal.query(title,default_value)

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
		return builtins.internal.open(path,player,side)


	def close(player):
		"""
		Close the player within given ID.
		"""
		return builtins.internal.close(player)

	def show(player):
		"""
		Restaure the state of a player that was maximized or minimized.
		"""
		return builtins.internal.show_normal(player)

	def maximize(player):
		"""
		Show maximized given player.
		"""
		return builtins.internal.show_maximized(player)

	def minimize(player):
		"""
		Minimize given player.
		"""
		return builtins.internal.show_minimized(player)

	def workspace(wks = 0):
		"""
		Create or switch workspace.
		If wks is 0, create a new workspace and returns its ID. The current workspace is set to the new one.
		If wks is > 0, the function set the current workspace to given workspace ID.
		Returns the ID of the current workspace, or throw an exception on error.
		"""
		return builtins.internal.workspace(wks)

	def workspaces():
		"""
		Returns the list of all available workspaces.
		"""
		return builtins.internal.workspaces()

	def current_workspace():
		"""
		Returns the ID of current workspace.
		"""
		return builtins.internal.current_workspace()

	def reorganize():
		"""
		Reorganize all players within the current workspace in order to use all the available space.
		"""
		return builtins.internal.reorganize()

	def time():
		"""
		Returns the current time in nanoseconds within the current workspace.
		"""
		return builtins.internal.time()

	def set_time(time, ref = 'absolute'):
		"""
		Set the time (in nanoseconds) in current workspace.
		If ref == 'relative', the time is considered as an offset since the minimum time of the workspace.
		"""
		return builtins.internal.set_time(time,ref)

	def time_range():
		"""
		Returns the time range [first,last] within the current workspace.
		"""
		return builtins.internal.time_range()

	def set_stylesheet(player, data_name, stylesheet):
		"""
		Set the stylesheet for a curve/histogram/image within a player.
		The stylesheet is used to customize the look'n feel of a plot item (pen, brush, symbol, color map, 
		title, axis units,...)
		
		The plot item is found using the player ID and the plot item name. If no name is given, the 
		style sheet is applied to the first item found. Note that only a partial name is required.
		The stylesheet will be applied to the first item matching the partial name.
		"""
		return builtins.internal.set_stylesheet(player, data_name, stylesheet)

	def top_level(player):
		"""
		For given player ID inside a multi-player, returns the ID of the top level window.
		This ID can be used to maximize/minimize the top level multi-player.
		"""
		return builtins.internal.top_level(player)

	def get(player, data_name = ""):
		"""
		Returns the data (usually a numpy array) associated to given player and item data name.
		The plot item is found using the player ID and the plot item name. If no name is given, the 
		first item data found is retuned. Note that only a partial name is required.
		The returned data will be the one of the first item matching the partial name.
		"""
		return builtins.internal.get(player,data_name)

	def remove(player, data_name):
		"""
		Remove, from given palyer, all plot items matching given (potentially partial) data name.
		Returns the number of item removed.
		"""
		return builtins.internal.remove(player,data_name)

	def set_time_marker(player, enable):
		"""
		Show/hide the time marker for given plot player.
		"""
		return builtins.internal.set_time_marker(player,enable)

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
		return builtins.internal.zoom(player,x1,x2,y1,y2,unit)

	def set_color_map_scale(player, vmin, vmax, gripMin = 0, gripMax = 0):
		"""
		Change the color map scale for given video player.
		
		vmin and vmax are the new scale boundaries. If vmin == vmax, the current
		scale boundaries are kept unchanged.
		
		gripMin and gripMax are the new slider grip boundaries. If gripMin == gripMax,
		the grip boundaries are kept unchanged.
		"""
		return builtins.internal.set_color_map_scale(player,vmin,vmax,gripMin,gripMax)


	def x_range(player):
		"""
		For given plot player, returns the list [min_x_value, max_x_value] for the union of all curves.
		"""
		return builtins.internal.x_range(player)


   
The sample code below uses these functions to open several signals within a new workspace (example on ITER). The code opens several signals displayed on 4 :ref:`players <players>` organized in a 2*2 grid. A :ref:`style sheet <stylesheets>` is used to customize the look & feel of the last signal.

.. code-block:: python

	#runs only on server fc17
	import Thermavip as th

	#create new workspace
	th.workspace()

	#request a time range
	time = th.query("Signal time range" ,\
	"1545330343874000000;1545333943874000000;-1")

	#open signal in new player
	pl = th.open("/ITER/TEST:AUX:CWS:QT0116:XI0:VAL/val_full;" + time)
	#open another signal in the same player
	th.open("/ITER/TEST:AUX:CWS:QT0216:XI0:VAL/val_full;" + time,pl)


	#open in a new player on the right
	th.open("/ITER/TEST:AUX:CWS:QT0218:XI0:VAL/val_full;"+time,pl,"right")
	#open in a new player on the bottom
	pl = th.open("/ITER/TEST:AUX:CWS:QT0408:XI0:VAL/val_full;"+time,pl,"bottom")
	pl = th.open("/ITER/TEST:AUX:CWS:QT0416:XI0:VAL/val_full;"+time,pl,"right")

	#maximize top level window
	th.maximize(th.top_level(pl))

	#set stylesheet for one signal
	th.set_stylesheet(pl, \
	"""
	border: 1.5px dash red;
	symbol:ellipse;
	symbolsize: 7;
	symbolborder:magenta;
	symbolbackground:transparent;
	title:'my new title'
	"""\
	,"TEST:AUX:CWS:QT0416:XI0")

   
