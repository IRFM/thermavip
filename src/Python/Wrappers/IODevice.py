from .Processing import*
from abc import abstractmethod
import numpy as np


class VipIODevice(VipProcessingObject):
    """
    The VipIODevice class is the base interface class of all I/O processing objects.
    
    VipIODevice provides both a common implementation and an abstract interface for devices that support reading and writing of VipAnyData.
    VipIODevice is abstract and can not be instantiated, but it is common to use the interface it defines to provide device-independent I/O features.
    
    A VipIODevice can be of 3 types:
    - VipIODevice.Temporal: it defines a start and end time, a time window and might have a notion of position. A Temporal devices support both input and output operations.
    - VipIODevice.Sequential: input data can arrive at any time. It does not have a notion of time window or position. Sequential devices only support input operations. They are usually suited for
    streaming operations.
    - VipIODevice.Resource: the device holds one unique data. It does not have a notion of time and position at all. Resource devices are suited for non temporal data, like for instance an image file
    on the drive.
    
    A read-only VipIODevice should defines one or more outputs (see VipProcessingObject for more details).
    A write-only VipIODevice should defines one or more inputs.
    
    To open a VipIODevice, you should usually follow these steps:
    - Set all necessary parameters
    - Call VipIODevice.open() with the right path and open mode (usually VipIODevice.ReadOnly or VipIODevice.WriteOnly).
    
    WriteOnly devices behave like VipProcessingObject: you should reimplement the VipProcessingObject.apply() function to write input data.
    
    ReadOnly, Temporal or Resource devices don't care about the apply() function and must reimplement VipIODevice.read() instead.
    Sequential devices do not need to reimplement any of these functions. Instead, they must reimplement VipIODevice.set_streaming_enabled() to start/stop sending output data.
    
    """
    
    # Supported open modes
    NotOpen = 0x0000
    ReadOnly = 0x0001
    WriteOnly = 0x0002
    ReadWrite = ReadOnly | WriteOnly
    Append = 0x0004
    
    # Device types
    Temporal = 0   # A temporal device with a valid time window
    Sequential = 1 # A temporal device with no valid time window (data arrive when they arrive)
    Resource = 2    # A non temporal, unique ressource,
    
    def __init__(self):
        super().__init__()
        self.supported_modes = VipIODevice.ReadOnly
        self.device_type = VipIODevice.Resource
        self.file_filters = str()
        
        self.__mode = VipIODevice.NotOpen
        self.__path = str()
        self.__time = VipInvalidTime
        self.__size = 1
        self.__is_streaming = False
        
    def __del__(self):
        self.close()
        
    def probe(self, filename, first_bytes = bytes()) -> bool:
        """
        Tells if this device can open given file based on its filename and optional first bytes

        Parameters
        ----------
        filename : str
            Full path.
        first_bytes : bytes, optional
            File first bytes. The default is bytes().

        Returns
        -------
        bool
            True on success, False otherwise.
        """
        try:
            return filename.index(type(self).__name__ + ":") == 0
        except:
            return False
    
    @abstractmethod        
    def open(self, path, mode):
        """
        Open the device in given mode.
        Derived implementation must call the base version on success.
        Parameters
        ----------
        mode : int
            Combination of ReadOnly, WriteOnly and Append.

        Returns
        -------
        None.

        """
        self.__mode = mode
        self.__path = path
    
    def close(self):
        """
        Close the device.
        Overloads must call the base version.
        Returns
        -------
        None.

        """
        self.__mode = VipIODevice.NotOpen
        self.__path = str()
        
    def is_open(self) -> bool:
        """
        Tells whether this device is open.
        Returns
        -------
        bool
            True for open device, False otherwise.

        """
        return self.__mode != VipIODevice.NotOpen
    
    @property
    def open_mode(self) -> int:
        """
        Returns the device open mode.

        Returns
        -------
        int
            Device open mode.

        """
        return self.__mode
    
    @property
    def path(self) -> str:
        """
        Returns the device path.

        Returns
        -------
        str
            Device path.

        """
        return self.__path
    
    @property
    def time(self) -> int:
        """
        Returns the device current time.
        Sequential devices always return VipInvalidTime.

        Returns
        -------
        int
            Device time.

        """
        return self.__time
    
    @property
    def is_streaming(self) -> bool:
        return self.__is_streaming
        
    @property
    def size(self) -> int:
        """
        For open Temporal (random-access) devices, this function returns the size of the device.
        For sequential devices or if the size cannot be computed, returns a negative value.
        If the device is closed, the size returned will not reflect the actual size of the device.
        Note that some temporal devices might not have a notion of size or position.

        Returns
        -------
        int
            Device's size.

        """
        if self.device_type == VipIODevice.Resource:
            return 1
        if self.device_type == VipIODevice.Sequential:
            return VipInvalidPosition
        return self.__size
    
    @size.setter
    def size(self, v : int):
        self.__size = int(v)
        
    def next_time(self, time : int) -> int:
        """
        Return the next time after given one.
        If provided time is the last valid timestamp, return it.
        Return VipInvalidTime for sequential devices.
        Default implementation relies on time_to_pos() and pos_to_time().
        
        Parameters
        ----------
        time : int
            Input time.

        Returns
        -------
        int
            Next time.

        """
        pos = self.time_to_pos(time)
        if pos == VipInvalidPosition:
            return VipInvalidTime
        return self.pos_to_time(pos +1)
    
    def prev_time(self,time : int) -> int:
        """
        Return the previous time before given one.
        If provided time is the first valid timestamp, return it.
        Return VipInvalidTime for sequential devices.
        Default implementation relies on time_to_pos() and pos_to_time().
        
        Parameters
        ----------
        time : int
            Input time.

        Returns
        -------
        int
            Previous time.

        """
        pos = self.time_to_pos(time)
        if pos == 0:
            return time
        elif pos == VipInvalidPosition:
            return VipInvalidTime
        return self.pos_to_time(pos - 1)
    
    def closest_time(self, time : int) -> int:
        """
        Return the closest valid time to given one.
        Return VipInvalidTime for sequential devices.
        Default implementation relies on time_to_pos() and pos_to_time().
        
        Parameters
        ----------
        time : int
            Input time.

        Returns
        -------
        int
            Closest time.

        """
        pos = self.time_to_pos(time)
        if pos == VipInvalidPosition:
            return VipInvalidTime
        return self.pos_to_time(pos)
    
    def pos_to_time(self, pos : int) -> int:
        """
        Convert given position to its timestamp.
        Negative position should return the first time.
        Position >= size() should return the last valid timestamp.

        Parameters
        ----------
        pos : int
            Input position.

        Returns
        -------
        int
            Position timestamp.

        """
        return VipInvalidTime
    
    def time_to_pos(self, time : int) -> int:
        """
        Convert given time to its closest position.
        A time before the first valid timestamp should return 0.
        A time after the last valid timestamp should return the last position.

        Parameters
        ----------
        time : int
            Input time.

        Returns
        -------
        int
            Output position.

        """
        return VipInvalidPosition
    
    def time_window(self) :
        """
        For ReadOnly device, returns the valid time window as a list of (first_time, last_time)

        Returns
        -------
        list
            Device time window.

        """
        return [(VipInvalidTime,VipInvalidTime)]
    
    def set_streaming_enabled(self, enable : bool):
        """
        For sequential device only, start/stop streaming.
        Derived implementation must call the base version.

        Parameters
        ----------
        enable : bool
            Enable/Disable streaming.

        Returns
        -------
        None.

        """
        if self.device_type != VipIODevice.Sequential:
            raise RuntimeError("Cannot enable/disable streaming on a non sequential device")
        self.__is_streaming = enable
    
    def read(self, time : int):
        """
        For ReadOnly device, read the device at given time.
        This should set the device output(s).
        Derived implementation must call the base version.

        Parameters
        ----------
        time : int
            DESCRIPTION.

        Raises
        ------
        RuntimeError
            DESCRIPTION.

        Returns
        -------
        None.

        """
        if (self.__mode & VipIODevice.ReadOnly) == 0:
            raise RuntimeError("Device not open in read mode")
        if self.device_type == VipIODevice.Sequential:
            raise RuntimeError("Cannot read on a sequential device")
        self.__time = int(time)
        
    def apply(self):
        """
        Reimplemented from VipProcessingObject as a ReadOnly VipIODevice
        does not need to reimplement apply().
        """
        pass
        
        
class VipTimeRangeBasedGenerator(VipIODevice):
    """
    Helper base class for Temporal VipIODevice having a notion of size and position.
    Set VipTimeRangeBasedGenerator.timestamps in the derive class to generate the time window.
    """
    
    def __init__(self):
        super().__init__()
        self.device_type = VipIODevice.Temporal
        self.__timestamps = []
        
    @property
    def timestamps(self):
        return self.__timestamps
    
    @timestamps.setter
    def timestamps(self, times):
        self.__timestamps = np.sort(np.array(times, dtype = np.int64))
        self.size = len(self.__timestamps)
        
    
    def pos_to_time(self, pos : int) -> int:
        if len(self.__timestamps) == 0:
            return VipInvalidTime
        if pos < 0:
            pos = 0
        elif pos >= len(self.__timestamps):
            pos = -1
        return self.__timestamps[pos]
    
    def time_to_pos(self, time : int) -> int:
        if len(self.__timestamps) == 0:
            return VipInvalidPosition
        
        if time <= self.__timestamps[0]:
            return 0
        if time >= self.__timestamps[-1]:
            return len(self.__timestamps) -1
        
        index = np.searchsorted(self.__timestamps, time)
        prev = self.__timestamps[index]
        next = self.__timestamps[index +1]
        if (time -prev) < (next -time):
            return index
        return index +1
    
    def time_window(self):
        if len(self.__timestamps) == 0:
            return super().time_window()
        return [(self.__timestamps[0],self.__timestamps[-1])]
        
        
    
        
    
        