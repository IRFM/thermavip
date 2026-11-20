"""Display progress and read cancellation requests through the Thermavip host."""

import Thermavip as th

class VipProgress:
    """A host-managed progress display with optional modality and cancellation. Nested
    displays stack vertically.
    """
    
    def __init__(self, **kwargs):
        """Create the display and retain the handle returned by Thermavip.

        Keyword arguments are forwarded to make_progress. Use range=(low, high) to
        show a bar, value for its initial position, modal to block application
        input, and cancelable to allow cancellation. Set the text property to
        update the message after construction.
        """
        self.__id = th.call_thermavip_fun("make_progress",(), kwargs)
        
    def __del__(self):
        """Request destruction of the host display, suppressing errors during
        finalization.
        """
        try:
            th.call_thermavip_fun("destroy_progress",(self.__id,))
        except:
            pass
    
    @property
    def value(self):
        """Current progress value reported by Thermavip. Assign a value to update the
        display.
        """
        return th.call_thermavip_fun("progress_value",(self.__id,))
    
    @value.setter
    def value(self, v):
        """Send the new progress value to the host."""
        th.call_thermavip_fun("progress_set_value",(self.__id, v))
        
    @property
    def text(self):
        """Message displayed alongside the progress indicator. Assign a string to
        update it.
        """
        return th.call_thermavip_fun("progress_text",(self.__id,))
    
    @text.setter
    def text(self, msg):
        """Send the new message to the host."""
        th.call_thermavip_fun("progress_set_text",(self.__id, msg))
        
    @property
    def canceled(self):
        """Whether the user has canceled this display. Poll this property during work
        to handle cancellation.
        """
        return th.call_thermavip_fun("progress_canceled",(self.__id,))