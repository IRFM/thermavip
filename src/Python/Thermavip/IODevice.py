from .Processing import*
from .QtCpp import qobject_address, qobject_from_address
import Thermavip as th
from Thermavip import VipInvalidTime, VipInvalidPosition
from abc import abstractmethod
import numpy as np
import ctypes
import os


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
    - Set the device path property
    - Call VipIODevice.open() with the right open mode (usually VipIODevice.ReadOnly or VipIODevice.WriteOnly).
    
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
    Resource = 2    # A non temporal, unique ressource
    FileHandler = 3 # File handler
    
    
    def _update_output(self, name, idx):
        """
        Called when an output is updated.
        Reimplemented for streaming purpose.
        """
        if self.device_type == VipIODevice.Sequential and self.is_open():
            th.call_thermavip_fun("_update_device_output", (int(id(self)), name, idx))
            
    
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
        
    def tool_bar(self):
        """
        Returns a QToolBar that should be displayed as part of the player's toolbar.
        Reimplement in subclasses to provide custom device edition from the player toolbar (if any).
        """
        pass
    
    def tool_bar_address(self) -> int:
        try:
            # This will throw on the first call, or if the C++ widget has been deleted
            return qobject_address(self.__tool_bar)
        except:
            pass
        # (Re)create the editor
        obj = self.tool_bar()
        if obj is None:
            return 0
        try:
            ret = qobject_address(obj)
            self.__tool_bar = obj
            return ret
        except:
            return 0
    
    @abstractmethod        
    def open(self, mode):
        """
        Open the device in given mode using the device path decorator.
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
        
    
    def close(self):
        """
        Close the device.
        Overloads must call the base version.
        Returns
        -------
        None.

        """
        self.__mode = VipIODevice.NotOpen
        
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
    
    @path.setter
    def path(self, p):
        """
        Set the device path.
        This property must be set before calling VipIODevice.open().
        """
        self.__path = str(p)
    
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
        if self.device_type == VipIODevice.Sequential or self.device_type == VipIODevice.FileHandler:
            raise RuntimeError("Cannot read on a sequential / file handler device")
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
        



class VipFileHandler(VipIODevice):
    """
    Base file handler class.
    A file handler is meant to perform any kind of action when opening a file,
    but does not have a notion of output data.
    For instance, Thermavip has a file handler class that opens Python files into the dedicated editor.

    Subclasses must override open_file() method.
    """
    def __init__(self):
        super().__init__()
        self.device_type = VipIODevice.FileHandler
    
    def open(self, mode):
        if mode != VipIODevice.ReadOnly:
            raise RuntimeError("VipFileHandler only supports ReadOnly mode")

        self.open_file()
        super().open(mode)

    @abstractmethod        
    def open_file(self):
        """
        Actual action performed when opening a file
        """
        pass
        


class _VipDeviceFunctions(VipIODevice):
    """
    Base VipIODevice class for VipInputDevice and VipOutputDevice.
    Reimplement the members close(), pos_to_time(), time_to_pos(), time_window() and 'path' property setter.
    
    _VipDeviceFunctions provides multi-input/property/output.
    
    """
    @VipMultiProperty
    def properties(self): pass

    @VipMultiOutput
    def outputs(self): pass

    @VipMultiInput
    def inputs(self): pass


    @VipIODevice.path.setter
    def path(self, p):
        """
        'path' property setter, overridden from VipIODevice.
        Setting the path will create the internal C++ VipIODevice and initialize 
        this device inputs/outputs/properties. Therefore, it is useless to manipulate
        inputs/outputs/properties before calling this property.
        """
        # Create the internal device
        self.close()
        self.dll._py_destroy(self._device)
        self._device = 0
        self._device = ctypes.c_uint64(self.dll._py_create(str(p).encode(), int(self._mode)))
        VipIODevice.path.fset(self, p)
        
        # Initialize properties and set their values
        prop_count = self.dll._py_property_count(self._device)
        self.properties.resize(prop_count)
        for i in range(prop_count):
            self.properties[i] = self.dll._py_get_property(self._device, i)
            self.properties[i].name = self.dll._py_property_name(self._device, i)
            
        # Initialize inputs and set their values
        in_count = self.dll._py_input_count(self._device)
        self.inputs.resize(in_count)
        for i in range(in_count):
            self.inputs[i] = self.dll._py_get_input(self._device, int(i))
            
        # Initialize outputs and set their values
        out_count = self.dll._py_output_count(self._device)
        self.outputs.resize(out_count)
        for i in range(out_count):
            self.outputs[i] = self.dll._py_get_output(self._device, int(i))
        
        # Set the device type
        self.device_type = self.dll._py_device_type(self._device)

    def __init__(self, mode):
        super().__init__()
        self._device = 0
        self._mode = mode
        import distutils.ccompiler
        if os.name == 'nt':
            self.dll = ctypes.PyDLL("VipGui" + distutils.ccompiler.new_compiler().shared_lib_extension)
        else:
            self.dll = ctypes.PyDLL("libVipGui" + distutils.ccompiler.new_compiler().shared_lib_extension)
        
        self.dll._py_create.restype = ctypes.c_uint64
        self.dll._py_open.argtypes = [ctypes.c_uint64, ctypes.c_int32]
        
        self.dll._py_output_count.argtypes = [ctypes.c_uint64]
        self.dll._py_input_count.argtypes = [ctypes.c_uint64]
        self.dll._py_property_count.argtypes = [ctypes.c_uint64]
        
        self.dll._py_set_input.argtypes = [ctypes.c_uint64, ctypes.c_int32]
        
        self.dll._py_set_input_count.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_set_property_count.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_set_output_count.argtype = [ctypes.c_uint64, ctypes.c_int32]
        
        self.dll._py_input_name.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_input_name.restype = ctypes.py_object
        
        self.dll._py_output_name.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_output_name.restype = ctypes.py_object
        
        self.dll._py_property_name.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_property_name.restype = ctypes.py_object
        
        self.dll._py_get_input.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_get_input.restype = ctypes.py_object
        
        self.dll._py_get_output.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_get_output.restype = ctypes.py_object
        
        self.dll._py_get_property.argtype = [ctypes.c_uint64, ctypes.c_int32]
        self.dll._py_get_property.restype = ctypes.py_object
        
        self.dll._py_set_property.argtypes = [ctypes.c_uint64, ctypes.c_int32, ctypes.py_object]
        self.dll._py_set_input.argtypes = [ctypes.c_uint64, ctypes.c_int32, ctypes.py_object]
        
        self.dll._py_time_window.argtypes= [ctypes.c_uint64]
        self.dll._py_time_window.restype = ctypes.py_object
        
        self.dll._py_pos_to_time.argtypes= [ctypes.c_uint64,ctypes.c_int64]
        self.dll._py_pos_to_time.restype = ctypes.c_int64
        
        self.dll._py_time_to_pos.argtypes= [ctypes.c_uint64,ctypes.c_int64]
        self.dll._py_time_to_pos.restype = ctypes.c_int64
        
        self.dll._py_get_size.argtypes= [ctypes.c_uint64]
        self.dll._py_get_size.restype = ctypes.c_int64
        
        self.dll._py_device_type.argtypes= [ctypes.c_uint64]
        self.dll._py_get_size.restype = ctypes.c_int32
        
        self.dll._py_apply.argtypes= [ctypes.c_uint64]
        
        
    def __del__(self):
        self.close()
        self.dll._py_destroy(self._device)
        
    def close(self):
        if self._device:
            self.dll._py_close(self._device)
        super().close()

    def pos_to_time(self, pos : int) -> int:
        return self.dll._py_pos_to_time(self._device, pos)
    
    def time_to_pos(self, time : int) -> int:
        return self.dll._py_time_to_pos(self._device, time)
    
    def time_window(self) :
        return self.dll._py_time_window(self._device)
    
    def set_property(self, name, value):
        """
        Convenient member that set a property value based on its name.
        """
        for p in self.properties:
            if p.name == name:
                p.data = value
                return None
        raise RuntimeError("invalid property name")
    
    
    
    
class VipInputDevice(_VipDeviceFunctions):
    """
    VipIODevice class wrapping a C++ VipIODevice, and allowing to access read-only devices available within Thermavip.
    
    To use this class, you must:
        1. Set the device path using the 'path' property
        2. Modify if needed the device properties. The set_property() member is provided to simplify this task.
        3. Open the device using VipInputDevice.open(). This will initialize/create the VipInputDevice outputs.
        4. For temporal devices, call VipInputDevice.read() before accessing the outputs.
    
    Usage:
        
    ```python
    
    # Open an mp4 video in read-only
    d = VipInputDevice()
    d.path = "C:/Users/VM213788/Desktop/test.mp4"
    d.open()
    
    # Open another mp4 video in write-only
    # and write all frames from input device
    out = VipOutputDevice()
    out.path = "C:/Users/VM213788/Desktop/test_lonnnnng.mp4"
    out.set_property("threads", 8)
    out.open()
    for i in range(d.size):
        d.read(d.pos_to_time(i))
        out.inputs[0] = d.outputs[0]
        out.apply()
    out.close()
    ```
    """
    def __init__(self):
        super().__init__(VipIODevice.ReadOnly)
        
    def open(self, mode = VipIODevice.ReadOnly):
        
        if len(self.path) == 0:
            raise RuntimeError("no valid path provided")
            
        # Set the property before
        self.dll._py_set_property_count(self._device, len(self.properties))
        for i in range(len(self.properties)):
            self.dll._py_set_property(self._device, i, self.properties[i])
            self.properties[i].name = self.dll._py_property_name(self._device, i)
            
        
        # Open the device
        self.dll._py_open(self._device, int(mode))

        # Initialize outputs and set their values
        out_count = self.dll._py_output_count(self._device)
        self.outputs.resize(out_count)
        for i in range(out_count):
            self.outputs[i] = self.dll._py_get_output(self._device, int(i))
            
        # Set the device type and size
        self.device_type = self.dll._py_device_type(self._device)
        self.size = self.dll._py_get_size(self._device)
        
        super().open(mode)
    
    
    def read(self, time : int):
        
        # Set the property before
        self.dll._py_set_property_count(self._device, len(self.properties))
        for i in range(len(self.properties)):
            self.dll._py_set_property(self._device, i, self.properties[i])
            self.properties[i].name = self.dll._py_property_name(self._device, i)

        
        self.dll._py_read(self._device, time)
        
        # Copy outputs
        for i in range(len(self.outputs)):
            self.outputs[i] = self.dll._py_get_output(self._device, i)
            
        super().read(time)
        
class VipOutputDevice(_VipDeviceFunctions):
    
    def __init__(self):
        super().__init__(VipIODevice.WriteOnly)
    
    
    def open(self, mode = VipIODevice.WriteOnly):
        
        if len(self.path) == 0:
            raise RuntimeError("no valid path provided")
            
        # Set the property before
        self.dll._py_set_property_count(self._device, len(self.properties))
        for i in range(len(self.properties)):
            self.dll._py_set_property(self._device, i, self.properties[i])
            self.properties[i].name = self.dll._py_property_name(self._device, i)

            
        # Set the property before
        self.dll._py_set_input_count(self._device, len(self.inputs))
        for i in range(len(self.inputs)):
            self.dll._py_set_input(self._device, i, self.inputs[i])
            
        # Open the device
        self.dll._py_open(self._device, int(mode))
        self.device_type = self.dll._py_device_type(self._device)
        
        # Set the device size
        #self.size = self.dll._py_get_size(self._device)
        
        super().open(mode)
    
    def apply(self):
        
        # Set the property before
        self.dll._py_set_property_count(self._device, len(self.properties))
        for i in range(len(self.properties)):
            self.dll._py_set_property(self._device, i, self.properties[i])
            self.properties[i].name = self.dll._py_property_name(self._device, i)

            
        # Set the property before
        self.dll._py_set_input_count(self._device, len(self.inputs))
        for i in range(len(self.inputs)):
            self.dll._py_set_input(self._device, i, self.inputs[i])
            
        self.dll._py_apply(self._device)
        self.size = self.dll._py_get_size(self._device)