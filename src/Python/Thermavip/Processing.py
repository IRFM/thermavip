"""Processing classes, port declarations, and registration with Thermavip.

Declare named ports on a VipProcessingObject subclass and implement apply() to read
inputs and set outputs. Single ports accept VipAnyData or a raw payload. Multi-port
descriptors hold a VipAnyDataList.
"""

import numpy as np
from collections import UserList
from enum import IntEnum
from abc import ABC
from abc import abstractmethod
import inspect
import weakref
import sys
from typing import Union
from .QtCpp import qobject_address, qobject_from_address
import Thermavip as th
from Thermavip import VipAnyData, VipInvalidTime, VipInvalidPosition, VipMinTime, VipMaxTime


class VipAnyDataList(UserList):
    """List wrapper for processing ports. Construction and individual assignments wrap
    raw payloads as VipAnyData.
    """
    
    def __init__(self, lst = None, parent_processing = None, io_name = None):
        """Wrap each initial value and optionally keep a weak reference to the
        processing object for indexed assignment notifications.
        """
        if parent_processing:
            self.__parent = weakref.ref(parent_processing)  # Create a weak reference
            self.__name = io_name
        else:
            self.__parent = None
        if lst is None:
            super().__init__(lst)
        else:
            super().__init__([VipAnyData(v) for v in lst])
            
    def __setitem__(self, i, item):
        """Assign an item, wrapping a raw payload, then notify the live parent of the
        port name and index.
        """
        if isinstance(item, VipAnyData):
            self.data[i] = item
        else:
            self.data[i] = VipAnyData(item)
        # Notify parent processing of an output modification
        if self.__parent:
            parent = self.__parent()
            if parent:
                parent._update_output(self.__name, i)
            
    def __add__(self, other):
        """Concatenate with another iterable and return a new wrapper."""
        if not isinstance(other, VipAnyDataList):
            other = [VipAnyData(v) for v in other]
        return super().__add__(other)
    
    def __radd__(self, other):
        """Prepend the other iterable and return a new wrapper."""
        if not isinstance(other, VipAnyDataList):
            other = [VipAnyData(v) for v in other]
        return super().__radd__(other)
    
    def __iadd__(self, other):
        """Extend this wrapper with the other iterable, converting raw payloads."""
        if not isinstance(other, VipAnyDataList):
            other = [VipAnyData(v) for v in other]
        return super().__iadd__(other)
    
    def __mul__(self, n):
        """Return a plain list of VipAnyData wrappers for the sequence repeated n
        times.
        """
        return [VipAnyData(v) for v in self.__class__(self.data * n)]
    
    def append(self, item):
        """Append item, wrapping it in VipAnyData when needed."""
        if not isinstance(item, VipAnyData):
            item = VipAnyData(item)
        self.data.append(item)

    def insert(self, i, item):
        """Insert item at index i, wrapping it in VipAnyData when needed."""
        if not isinstance(item, VipAnyData):
            item = VipAnyData(item)
        self.data.insert(i, item)
        
    def resize(self, size):
        """Grow to a nonnegative size with empty VipAnyData instances, or discard
        trailing items.
        """
        while len(self)  < size:
            self.append(VipAnyData())
        while len(self) > size:
            self.pop()


class VipIODescription:
    """Metadata for a declared processing port. The name is the class attribute used
    to access the port.
    """
    def __init__(self, name = str(), description = str(), editor = str()):
        """Store the port name and its optional description and editor metadata."""
        self.name = name
        self.description = description
        self.editor = editor
        
    def __str__(self):
        """Format the name, description, and editor as a dictionary string."""
        return str({"name":self.name, "description": self.description, "editor": self.editor})
    
    def __repr__(self):
        """Return the dictionary representation of the port metadata."""
        return str({"name":self.name, "description": self.description, "editor": self.editor})


class VipBaseProcessingIO(ABC):
    """Descriptor for a single processing input, output, or property. Each processing
    instance stores its own value.
    """
    def __init__(self, description =  str(), editor =  str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        self.description = description
        self.editor = editor
        
    def __get__(self, obj, objtype=None):
        """Return the descriptor on class access or the stored value on instance
        access, creating an empty VipAnyData if needed.
        """
        if obj is None:
            return self
        
        name = "_vip_" + self.__name__
        try:
            return obj.__dict__[name]
        except:
            obj.__dict__[name] = VipAnyData()
            return obj.__dict__[name]

    def __set__(self, obj, value):
        """Store value as VipAnyData and call the owner's update hook with index zero."""
        if isinstance(value,VipAnyData):
            obj.__dict__["_vip_" + self.__name__] = value
        else:
            obj.__dict__["_vip_" + self.__name__] = VipAnyData(value)
        # Notify parent processing of an output modification
        obj._update_output(self.__name__, 0)
            
    def __call__(self, function):
        """Create a descriptor of the same type with the same metadata for the
        decorated name.
        """
        return type(self)(self.description, self.editor)
            
    def __delete__(self, obj):
        """Leave the stored port value in place."""
        pass
    def getter(self, fget):
        """Raises TypeError because the implementation calls type() without arguments."""
        return type()()
    def setter(self, fset):
        """Return a fresh default descriptor of this type. The fset argument is
        unused.
        """
        return type(self)()
    def deleter(self, fdel):
        """Return a fresh default descriptor of this type. The fdel argument is
        unused.
        """
        return type(self)()
    
    def get_io_list(self, owner, name):
        """Look up name in the owner's dictionary, initializing an empty list if
        absent.
        """
        try:
            return owner.__dict__[name]
        except:
            owner.__dict__[name] = []
            return owner.__dict__[name]
    
    
class VipBaseProcessingMultiIO(VipBaseProcessingIO):
    """Descriptor for a named group of ports stored together in a VipAnyDataList."""
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description,editor)
        
    def __get__(self, obj, objtype=None):
       """Return the descriptor on class access or the instance's list, creating it on
       first access.
       """
       if obj is None:
           return self
       
       name = "_vip_" + self.__name__
       try:
           obj.__dict__[name]
       except:
           obj.__dict__[name] = VipAnyDataList(parent_processing = obj, io_name = self.__name__)
       return obj.__dict__[name] 

    def __set__(self, obj, value):
            """Replace the group with a new VipAnyDataList linked to this processing
            object.
            """
            obj.__dict__["_vip_" + self.__name__] = VipAnyDataList(value, parent_processing = obj, io_name = self.__name__ )
        



class VipInput(VipBaseProcessingIO):
    """Declare a processing input. Assign VipInput() to a class attribute or use
    @VipInput() on a declaration.
    """
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        """Register the input's name and metadata on the owner class."""
        try:
            owner._all_inputs
        except:
            owner._all_inputs = []
        owner._all_inputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name
        

class VipMultiInput(VipBaseProcessingMultiIO):
    """Declare a processing input group. Assign VipMultiInput() to a class attribute
    or use @VipMultiInput() on a declaration.
    """
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        """Register the input group's name and metadata on the owner class."""
        try:
            owner._all_multi_inputs
        except:
            owner._all_multi_inputs = []
        owner._all_multi_inputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name
        
   
class VipProperty(VipBaseProcessingIO):
    """Declare a processing property. Assign VipProperty() to a class attribute or use
    @VipProperty() on a declaration.
    """
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description, editor)

    def __set_name__(self, owner, name):
        """Register the property's name and metadata on the owner class."""
        try:
            owner._all_properties
        except:
            owner._all_properties = []
        owner._all_properties.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name

class VipMultiProperty(VipBaseProcessingMultiIO):
    """Declare a processing property group. Assign VipMultiProperty() to a class
    attribute or use @VipMultiProperty() on a declaration.
    """
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        """Register the property group's name and metadata on the owner class."""
        try:
            owner._all_multi_properties
        except:
            owner._all_multi_properties = []
        owner._all_multi_properties.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name

class VipOutput(VipBaseProcessingIO):
    """Declare a processing output. Assign VipOutput() to a class attribute or use
    @VipOutput() on a declaration.
    """
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        """Register the output's name and metadata on the owner class."""
        try:
            owner._all_outputs
        except:
            owner._all_outputs = []
        owner._all_outputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name

class VipMultiOutput(VipBaseProcessingMultiIO):
    """Declare a processing output group. Assign VipMultiOutput() to a class attribute
    or use @VipMultiOutput() on a declaration.
    """
    def __init__(self, description= str(), editor= str()):
        """Store the optional description and editor metadata for this port
        declaration.
        """
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        """Register the output group's name and metadata on the owner class."""
        try:
            owner._all_multi_outputs
        except:
            owner._all_multi_outputs = []
        owner._all_multi_outputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name
 
class VipDisplayHint(IntEnum):
    """Describe where an output can be displayed relative to its inputs. Set the
    processing class's display_hint to one of these values.
    """
    # The processing transform its unique input and returns an output of the same type and shape.
    # The processing output can replace the input for display.
    # This kind of processing should have only 1 input and 1 output.
    InputTransform = 0
    # The processing might create a different object type as its input(s), but can be displayed
    # on the same support as one of its input(s).
    DisplayOnSameSupport = 1
    # The processing create an output data unrelated to the input one(s), and
    # should be displayed on a different support.
    DisplayOnDifferentSupport = 2
    # Default DisplayHint
    UnknownDisplayHint = 3

class VipProcessingObject(ABC):
    """Base class for Python processing algorithms registered with Thermavip.

    Declare inputs, properties, and outputs using descriptor instances. Implement
    apply() to read the current inputs and assign the results. Set description,
    category, and display_hint as class attributes. Override reset(),
    copy_parameters(), or editor() when needed.

        class Scale(VipProcessingObject):
            source = VipInput()
            factor = VipProperty()
            result = VipOutput()

            def apply(self):
                self.result = self.source.data * self.factor.data

    The caller supplies numeric source and factor values before apply(). Pass the
    class itself to register_processing(Scale).
    """
    
    
    ######################################
    # Static description of the processing
    ######################################
    description = str()
    category = str()
    use_event_loop = False
    display_hint = VipDisplayHint.UnknownDisplayHint
    
    @classmethod
    def get_display_hint(cls):
        """Return the class's display_hint as an integer."""
        return int(cls.display_hint)
    
    @classmethod
    def get_any_data_type(cls):
        """Return the VipAnyData class used for individual port values."""
        return VipAnyData
    
    
    #
    # Private members
    #
    
    def __set_attribute(self, key, value):
        """Wrap value if needed and store it under the internal key for the port."""
        if not isinstance(value,VipAnyData):
            value = VipAnyData(value)
        self.__dict__["_vip_" + key] = value
        
    def __attribute(self, key):
        """Return the stored port value by its declared name."""
        return self.__dict__["_vip_" + key]
    
    def __initializeIO(self, lst, val_type):
        """Initialize each described port with a single value or a parent-linked list."""
        try:
            for io in lst:
                if val_type is VipAnyData:
                    value = val_type()
                else:
                    value = VipAnyDataList(parent_processing = self, io_name = io.name)
                self.__dict__["_vip_" + io.name] = value
        except: 
            pass
        
    def _update_output(self, name, idx):
        """Hook called on single-port assignment and indexed list assignment. The base
        implementation takes no action.
        """
        pass
    
    #
    # Public members
    #
    
    def __init__(self):
        """Initialize instance storage from the registered port descriptions."""
        # Initialize all inputs/properties/outputs
        try: self.__initializeIO(type(self)._all_inputs, VipAnyData)
        except: pass
        try: self.__initializeIO(type(self)._all_multi_inputs, VipAnyDataList)
        except: pass
        try: self.__initializeIO(type(self)._all_properties, VipAnyData)
        except: pass
        try: self.__initializeIO(type(self)._all_multi_properties, VipAnyDataList)
        except: pass
        try: self.__initializeIO(type(self)._all_outputs, VipAnyData())
        except: pass
        try: self.__initializeIO(type(self)._all_multi_outputs, VipAnyDataList)
        except: pass
    
    def get_io(self, name) -> Union[VipAnyData , VipAnyDataList] :
        """Return the stored input, output, or property by name. An unknown name
        raises KeyError.
        """
        return self.__attribute(name)
    
    def set_io(self, name, index, value):
        """Assign a value by port name and index, wrapping raw data as VipAnyData.

        Use index zero for a stored single value. Other indices raise
        AttributeError for that value. For a multi-port, index selects an existing
        list entry. An unknown port name raises KeyError.
        """
        if not isinstance(value,VipAnyData):
            value = VipAnyData(value)
        key = "_vip_" + name
        if isinstance(self.__dict__[key], VipAnyData):
            if index != 0:
                raise AttributeError("non 0 index for VipAnyData type")
            self.__dict__[key] = value
        else:
            self.__dict__[key][index] = value
        
    @abstractmethod     
    def apply(self):
        """Implement the algorithm using current inputs and properties. Assign results
        to outputs and raise an exception on failure.
        """
        pass
    
    def reset(self):
        """Override to reset processing state. The base implementation takes no
        action.
        """
        
    def copy_parameters(self, dst):
        """Override to copy parameters into dst, a processing object of the same type.
        The base implementation takes no action.
        """
        pass
            
    
    def editor(self):
        """Override to return a new Qt widget bound to this processing instance. The
        default returns None.
        """
        pass
    
    def editor_address(self) -> int:
        """Return the cached editor's native address, recreating it if needed. Return
        0 if no editor is supplied or its address cannot be obtained.
        """
        try:
            # This will throw on the first call, or if the C++ widget has been deleted
            return qobject_address(self.__editor)
        except:
            pass
        # (Re)create the editor
        obj = self.editor()
        if obj is None:
            return 0
        try:
            ret = qobject_address(obj)
            self.__editor = obj
            return ret
        except:
            return 0
    
    
def register_processing(proc_type):
    """Register proc_type with Thermavip and return the host result. A class outside
    the VipProcessingObject hierarchy raises RuntimeError.
    """
    
    if not VipProcessingObject in inspect.getmro(proc_type):
        raise RuntimeError("Invalid processing type")
    return th.call_thermavip_fun('register_processing', (proc_type,))

    

