.. _WEST_browse:
 
Browse WEST signals/videos
=========================================================

.. |database_icon| image:: images/icons/database.png
.. |close_icon| image:: images/icons/close.png

To browse WEST signal, you need to show the :ref:`WEST tool widget <tools>` by clicking on the |database_icon| icon in Thermavip main tool bar (element (3) in :numref:`Fig. %s <main_window>`). 

.. _WEST:  

.. figure:: images/WEST.png
   :alt: WEST signal browser
   :figclass: align-center
   :align: center
   :scale: 50%
   
   *WEST signal browser*

WEST videos and 2D signals from every diagnostic can be opened from there. This panel displays:

1. Pulse number selection.
2. Go to the last available pulse.
3. Reload all opened WEST signals or videos within the current workspace with the pulse number set in (1).
4. Start date of the pulse set in (1). This value in not precise enough to be used as an absolute time reference, but gives a rough idea of when the pulse started. The date is updated when changing the pulse number in (1). You can enter a custom date that will set the pulse number (1) to the closest one from given date.
5. Search field. Enter any kind of text to be searched in available videos/signals for the current pulse number (1). You can search for multiple text matches using space separator (star separator can be used for incomplete words). The search will look for all valid videos, diagnostics, signals and signal descriptions matching given pattern. The result of the search is displayed in the panel (8). Updating the search field with a new pattern will automatically trigger a new search. 
6. Display the shortcuts panel. This panel is very close to the (7) one, and you can drop their any signal/video you use in a regular basis. The shortcuts are saved within your session.
7. The list of available signals. This list is splitted in 3 parts:

	1. The infrared videos (the green light means that this video is available for this pulse, as opposed to the grey light),
	2. The operation cameras (always 2 of them),
	3. The diagnostics and their signals.
	
   To open signals/videos in a new player, just double click on them. You can also drag/drop signals in an existing plot player to superimpose multiple signals.
8. Results of the search (see (5) for more details). Use the |close_icon| icon to close this panel and stop the search.
	


   