"""Device interfaces for Thermavip plugins and wrappers for native devices.

Subclass VipIODevice to supply a device, VipTimeRangeBasedGenerator for
timestamp-indexed data, or VipFileHandler for an action on opening a file. Use
VipInputDevice and VipOutputDevice to drive existing native devices.
"""

from .Processing import*
from .QtCpp import qobject_address, qobject_from_address
import Thermavip as th
from Thermavip import VipInvalidTime, VipInvalidPosition
from abc import abstractmethod
import numpy as np
import ctypes
import os


class VipIODevice(VipProcessingObject):
    """Base class for devices that participate in a processing pipeline.

    Set the device parameters and path before opening. A temporal device exposes a
    time window and may support random access. A resource holds one data item. A
    sequential device supplies data as it arrives. A file handler performs an
    action when a file opens.

    Declare outputs for a reader and inputs for a writer. Implement read() to
    populate temporal or resource outputs, apply() to consume writer inputs, and
    set_streaming_enabled() to control a sequential source. Subclasses implement
    open() and call its base implementation on success.
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
        """Notify Thermavip of an assignment only when this sequential device is open."""
        if self.device_type == VipIODevice.Sequential and self.is_open():
            th.call_thermavip_fun("_update_device_output", (int(id(self)), name, idx))
            
    
    def __init__(self):
        """Initialize a closed, read-only resource with an empty path and invalid
        time.
        """
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
        """Close the device during finalization."""
        self.close()
        
    def probe(self, filename, first_bytes = bytes()) -> bool:
        """Return whether filename starts with the concrete class name followed by a
        colon.

        Override to recognize other paths or inspect the optional first_bytes.
        """
        try:
            return filename.index(type(self).__name__ + ":") == 0
        except:
            return False
        
    def tool_bar(self):
        """Return a new QToolBar for editing this device in a player, or None by
        default.
        """
        pass
    
    def tool_bar_address(self) -> int:
        """Return the cached toolbar address, recreating the toolbar if needed. Return
        0 if absent or unaddressable.
        """
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
        """Record the open mode. Call from a subclass after opening its underlying
        device.
        """
        self.__mode = mode
        
    
    def close(self):
        """Mark the device as NotOpen. Overrides should also release their device
        resources.
        """
        self.__mode = VipIODevice.NotOpen
        
    def is_open(self) -> bool:
        """Return whether the recorded mode differs from NotOpen."""
        return self.__mode != VipIODevice.NotOpen
    
    @property
    def open_mode(self) -> int:
        """Mode recorded by open(), or NotOpen after initialization or close()."""
        return self.__mode
    
    @property
    def path(self) -> str:
        """Device path. Assign it before calling open()."""
        return self.__path
    
    @path.setter
    def path(self, p):
        """Store the string representation of the device path."""
        self.__path = str(p)
    
    @property
    def time(self) -> int:
        """Last time recorded by the base read() method, initially VipInvalidTime."""
        return self.__time
    
    @property
    def is_streaming(self) -> bool:
        """Streaming flag recorded by set_streaming_enabled(), initially False."""
        return self.__is_streaming
        
    @property
    def size(self) -> int:
        """Return 1 for a resource, VipInvalidPosition for a sequential device, or the
        stored size otherwise.
        """
        if self.device_type == VipIODevice.Resource:
            return 1
        if self.device_type == VipIODevice.Sequential:
            return VipInvalidPosition
        return self.__size
    
    @size.setter
    def size(self, v : int):
        """Store an integer size for device types that use the stored count."""
        self.__size = int(v)
        
    def next_time(self, time : int) -> int:
        """Map time to a position and return the timestamp one position later. An
        invalid position yields VipInvalidTime.
        """
        pos = self.time_to_pos(time)
        if pos == VipInvalidPosition:
            return VipInvalidTime
        return self.pos_to_time(pos +1)
    
    def prev_time(self,time : int) -> int:
        """Return time at position zero, VipInvalidTime for an invalid position, or
        the preceding position's timestamp.
        """
        pos = self.time_to_pos(time)
        if pos == 0:
            return time
        elif pos == VipInvalidPosition:
            return VipInvalidTime
        return self.pos_to_time(pos - 1)
    
    def closest_time(self, time : int) -> int:
        """Convert time to a position and back. Return VipInvalidTime if the position
        is invalid.
        """
        pos = self.time_to_pos(time)
        if pos == VipInvalidPosition:
            return VipInvalidTime
        return self.pos_to_time(pos)
    
    def pos_to_time(self, pos : int) -> int:
        """Return VipInvalidTime by default. Temporal overrides should clamp positions
        to the first or last timestamp.
        """
        return VipInvalidTime
    
    def time_to_pos(self, time : int) -> int:
        """Return VipInvalidPosition by default. Temporal overrides should map times
        to positions and clamp at the endpoints.
        """
        return VipInvalidPosition
    
    def time_window(self) :
        """Return a list of (first_time, last_time) pairs. The base pair contains two
        VipInvalidTime values.
        """
        return [(VipInvalidTime,VipInvalidTime)]
    
    def set_streaming_enabled(self, enable : bool):
        """Record the streaming flag for a sequential device. Other device types raise
        RuntimeError. Call from streaming overrides.
        """
        if self.device_type != VipIODevice.Sequential:
            raise RuntimeError("Cannot enable/disable streaming on a non sequential device")
        self.__is_streaming = enable
    
    def read(self, time : int):
        """Record the requested time as an integer. Reader subclasses populate the
        outputs.

        Call this implementation from the override. It raises RuntimeError if the
        mode lacks ReadOnly or the device is sequential or a file handler.
        """
        if (self.__mode & VipIODevice.ReadOnly) == 0:
            raise RuntimeError("Device not open in read mode")
        if self.device_type == VipIODevice.Sequential or self.device_type == VipIODevice.FileHandler:
            raise RuntimeError("Cannot read on a sequential / file handler device")
        self.__time = int(time)
        
    def apply(self):
        """Writer hook for consuming inputs. The base device takes no action."""
        pass
        
        
class VipTimeRangeBasedGenerator(VipIODevice):
    """Base class for a temporal reader indexed by a one-dimensional sequence of
    timestamps in nanoseconds.
    """
    
    def __init__(self):
        """Select the Temporal device type and initialize an empty timestamp list."""
        super().__init__()
        self.device_type = VipIODevice.Temporal
        self.__timestamps = []
        
    @property
    def timestamps(self):
        """Timestamp sequence used for position lookup. Assignment sorts the values
        and updates size.
        """
        return self.__timestamps
    
    @timestamps.setter
    def timestamps(self, times):
        """Convert the sequence to a sorted int64 array and record its length."""
        self.__timestamps = np.sort(np.array(times, dtype = np.int64))
        self.size = len(self.__timestamps)
        
    
    def pos_to_time(self, pos : int) -> int:
        """Return the timestamp at pos, clamped to either endpoint, or VipInvalidTime
        for an empty sequence.
        """
        if len(self.__timestamps) == 0:
            return VipInvalidTime
        if pos < 0:
            pos = 0
        elif pos >= len(self.__timestamps):
            pos = -1
        return self.__timestamps[pos]
    
    def time_to_pos(self, time : int) -> int:
        """Return a position using the sorted timestamps, or VipInvalidPosition if
        empty.

        Times at or beyond an endpoint map to that endpoint. For interior times the
        implementation compares the insertion index and the following index. It can
        raise IndexError when the insertion index is the last slot.
        """
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
        """Return one pair spanning the first and last timestamps, or the base invalid
        pair when empty.
        """
        if len(self.__timestamps) == 0:
            return super().time_window()
        return [(self.__timestamps[0],self.__timestamps[-1])]
        



class VipFileHandler(VipIODevice):
    """Base class for opening a file to perform an action. Implement open_file() using
    self.path.
    """
    def __init__(self):
        """Initialize the device with the FileHandler type."""
        super().__init__()
        self.device_type = VipIODevice.FileHandler
    
    def open(self, mode):
        """Run open_file() and record the mode on success. Any mode other than
        ReadOnly raises RuntimeError.
        """
        if mode != VipIODevice.ReadOnly:
            raise RuntimeError("VipFileHandler only supports ReadOnly mode")

        self.open_file()
        super().open(mode)

    @abstractmethod        
    def open_file(self):
        """Perform the subclass-specific action for the current path."""
        pass
        


class _VipDeviceFunctions(VipIODevice):
    """Common bridge to the native device selected by path and mode. It exposes
    resizable properties, inputs, and outputs.
    """
    @VipMultiProperty
    def properties(self):
        """Native device properties in index order."""
        pass

    @VipMultiOutput
    def outputs(self):
        """Native device outputs in index order."""
        pass

    @VipMultiInput
    def inputs(self):
        """Native device inputs in index order."""
        pass


    @VipIODevice.path.setter
    def path(self, p):
        """Close and destroy the previous native device, then create one for p.

        Read its properties, inputs, outputs, and device type. Configure the new
        device's ports and properties after assigning this path.
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
        """Load the VipGui device bindings and retain mode for native device creation."""
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
        """Close and destroy the native device."""
        self.close()
        self.dll._py_destroy(self._device)
        
    def close(self):
        """Close the native device if present and reset the Python open mode."""
        if self._device:
            self.dll._py_close(self._device)
        super().close()

    def pos_to_time(self, pos : int) -> int:
        """Ask the native device for the timestamp at pos."""
        return self.dll._py_pos_to_time(self._device, pos)
    
    def time_to_pos(self, time : int) -> int:
        """Ask the native device for the position corresponding to time."""
        return self.dll._py_time_to_pos(self._device, time)
    
    def time_window(self) :
        """Return the time window reported by the native device."""
        return self.dll._py_time_window(self._device)
    
    def set_property(self, name, value):
        """Set the data of the first property named name. Raise RuntimeError if none
        matches.
        """
        for p in self.properties:
            if p.name == name:
                p.data = value
                return None
        raise RuntimeError("invalid property name")
    
    
    
    
class VipInputDevice(_VipDeviceFunctions):
    """Read through a native Thermavip device selected by path.

    Assign path, set any device-specific properties, and call open(). For a
    temporal device, read a timestamp before accessing its outputs.

        reader = VipInputDevice()
        reader.path = "input.mp4"
        reader.open()
        try:
            for i in range(reader.size):
                reader.read(reader.pos_to_time(i))
                frame = reader.outputs[0].data
        finally:
            reader.close()

    This example assumes a supported video file with indexed frames.
    """
    def __init__(self):
        """Select ReadOnly for native device creation."""
        super().__init__(VipIODevice.ReadOnly)
        
    def open(self, mode = VipIODevice.ReadOnly):
        """Send properties, open the native device, and refresh outputs, type, and
        size. An empty path raises RuntimeError.
        """
        
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
        """Send current properties, read at time, copy native outputs, then call the
        base read() method.
        """
        
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
    """Write through a native Thermavip device selected by path.

    Assign path before configuring properties and inputs. Call open(), assign the
    data to inputs, and call apply() for each write. Call close() when finished.
    """
    
    def __init__(self):
        """Select WriteOnly for native device creation."""
        super().__init__(VipIODevice.WriteOnly)
    
    
    def open(self, mode = VipIODevice.WriteOnly):
        """Send current properties and inputs, open the native device, and record its
        type and mode. An empty path raises RuntimeError.
        """
        
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
        """Send properties and inputs, invoke the native writer, and refresh the
        stored size.
        """
        
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