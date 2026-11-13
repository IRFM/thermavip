import numpy as np
from collections import UserList
from enum import IntEnum
from abc import ABC
from abc import abstractmethod
import sys
from typing import Union
from .QtCpp import qobject_address, qobject_from_address

VipInvalidTime = -np.iinfo(np.int64).max
VipInvalidPosition = VipInvalidTime
VipMinTime = VipInvalidTime + 1
VipMaxTime = np.iinfo(np.int64).max



class VipAnyData:
    """
    Input/output type for VipProcessingObject.
    Concatenation of any data type, a timestamp in nanoseconds and attributes.
    """
    
    def __init__(self,*args, **kwargs):
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
            elif name == "attributes":
                self.attributes = dict(value)
                
    def __str__(self):
        return str({"data":self.data, "time": self.time, "attributes": self.attributes})
    
    def __repr__(self):
        return str({"data":self.data, "time": self.time, "attributes": self.attributes})
    
    
    ######################################################################
    # Several usefull properties stored within the 'attributes' dictionary
    ######################################################################
    
    @property
    def name(self):
        try:
            return self.attributes["Name"]
        except:
            return str()
        
    @name.setter
    def name(self, n):
        self.attributes["Name"] = str(n)
        
    @property
    def x_unit(self):
        try:
            return self.attributes["XUnit"]
        except:
            return str()
        
    @name.setter
    def x_unit(self, n):
        self.attributes["XUnit"] = str(n) 
        
    @property
    def y_unit(self):
        try:
            return self.attributes["YUnit"]
        except:
            return str()
        
    @name.setter
    def y_unit(self, n):
        self.attributes["YUnit"] = str(n) 
        
    @property
    def z_unit(self):
        try:
            return self.attributes["ZUnit"]
        except:
            return str()
        
    @name.setter
    def z_unit(self, n):
        self.attributes["ZUnit"] = str(n) 


class VipAnyDataList(UserList):
    """
    List of VipAnyData.
    Behaves like a standard list(), but ensures that it ONLY contains VipAnyData instances.
    """
    
    def __init__(self, lst = None):
        if lst is None:
            super().__init__(lst)
        else:
            super().__init__([VipAnyData(v) for v in lst])
            
    def __setitem__(self, i, item):
        if isinstance(item, VipAnyData):
            self.data[i] = item
        else:
            self.data[i] = VipAnyData(item)
            
    def __add__(self, other):
        if not isinstance(other, VipAnyDataList):
            other = [VipAnyData(v) for v in other]
        return super().__add__(other)
    
    def __radd__(self, other):
        if not isinstance(other, VipAnyDataList):
            other = [VipAnyData(v) for v in other]
        return super().__radd__(other)
    
    def __iadd__(self, other):
        if not isinstance(other, VipAnyDataList):
            other = [VipAnyData(v) for v in other]
        return super().__iadd__(other)
    
    def __mul__(self, n):
        return [VipAnyData(v) for v in self.__class__(self.data * n)]
    
    def append(self, item):
        if not isinstance(item, VipAnyData):
            item = VipAnyData(item)
        self.data.append(item)

    def insert(self, i, item):
        if not isinstance(item, VipAnyData):
            item = VipAnyData(item)
        self.data.insert(i, item)
        
    def resize(self, size):
        while len(self)  < size:
            self.append(VipAnyData())
        while len(self) > size:
            self.pop()


class VipIODescription:
    """
    Inout/output description: name (as defined in the processing class),
    optional description and editor.
    """
    def __init__(self, name = str(), description = str(), editor = str()):
        self.name = name
        self.description = description
        self.editor = editor
        
    def __str__(self):
        return str({"name":self.name, "description": self.description, "editor": self.editor})
    
    def __repr__(self):
        return str({"name":self.name, "description": self.description, "editor": self.editor})


class VipBaseProcessingIO(ABC):
    """
    Base class for processing input/output decorator
    """
    def __init__(self, description =  str(), editor =  str()):
        self.description = description
        self.editor = editor
        
    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        
        name = "_vip_" + self.__name__
        try:
            return obj.__dict__[name]
        except:
            obj.__dict__[name] = VipAnyData()
            return obj.__dict__[name]

    def __set__(self, obj, value):
        if isinstance(value,VipAnyData):
            obj.__dict__["_vip_" + self.__name__] = value
        else:
            obj.__dict__["_vip_" + self.__name__] = VipAnyData(value)
            
    def __call__(self, function):
        return type(self)(self.description, self.editor)
            
    def __delete__(self, obj):
        pass
    def getter(self, fget):
        return type()()
    def setter(self, fset):
        return type(self)()
    def deleter(self, fdel):
        return type(self)()
    
    def get_io_list(self, owner, name):
        try:
            return owner.__dict__[name]
        except:
            owner.__dict__[name] = []
            return owner.__dict__[name]
    
    
class VipBaseProcessingMultiIO(VipBaseProcessingIO):
    """
    Base class for processing multi input/output decorator
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description,editor)
        
    def __get__(self, obj, objtype=None):
       if obj is None:
           return self
       
       name = "_vip_" + self.__name__
       try:
           obj.__dict__[name]
       except:
           obj.__dict__[name] = VipAnyDataList()
       return obj.__dict__[name] 

    def __set__(self, obj, value):
            obj.__dict__["_vip_" + self.__name__] = VipAnyDataList(value)
        



class VipInput(VipBaseProcessingIO):
    """
    Processing input decorator.
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        try:
            owner._all_inputs
        except:
            owner._all_inputs = []
        owner._all_inputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name
        

class VipMultiInput(VipBaseProcessingMultiIO):
    """
    Processing multiinput decorator.
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        try:
            owner._all_multi_inputs
        except:
            owner._all_multi_inputs = []
        owner._all_multi_inputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name
        
   
class VipProperty(VipBaseProcessingIO):
    """
    Processing property decorator.
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description, editor)

    def __set_name__(self, owner, name):
        try:
            owner._all_properties
        except:
            owner._all_properties = []
        owner._all_properties.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name

class VipMultiProperty(VipBaseProcessingMultiIO):
    """
    Processing multiproperty decorator.
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        try:
            owner._all_multi_properties
        except:
            owner._all_multi_properties = []
        owner._all_multi_properties.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name

class VipOutput(VipBaseProcessingIO):
    """
    Processing output decorator.
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        try:
            owner._all_outputs
        except:
            owner._all_outputs = []
        owner._all_outputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name

class VipMultiOutput(VipBaseProcessingMultiIO):
    """
    Processing multioutput decorator.
    """
    def __init__(self, description= str(), editor= str()):
        super().__init__( description,editor)

    def __set_name__(self, owner, name):
        try:
            owner._all_multi_outputs
        except:
            owner._all_multi_outputs = []
        owner._all_multi_outputs.append(VipIODescription(name, self.description, self.editor))
        self.__name__ = name
 
class VipDisplayHint(IntEnum):
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
    """
    Base class for all processing embedded within Thermavip.
    
    A processing can take any number of input(s), output(s) and property(ies).
    All I/O functions must be decorated with the right class: VipInput, VipOutput, VipProperty...
    These decorators automatically create the internal object variable and register themselves
    within the class available inputs, outputs...
    
    Dervied class must define its own inputs/outputs/properties, and can override the following members:
        - apply(): apply the algorithm based on provided inputs/properties, and update potential outputs.
        - reset(): reset the processing parameter(s).
        - editor(): returns a Qt based editor widget used to edit this processing parameters.
        - display_hint(): returns the processing display hint
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
        return int(cls.display_hint)
    
    @classmethod
    def get_any_data_type(cls):
        return VipAnyData
    
    
    ######################################
    # Private members
    ######################################
    
    def __set_attribute(self, key, value):
        if not isinstance(value,VipAnyData):
            value = VipAnyData(value)
        self.__dict__["_vip_" + key] = value
        
    def __attribute(self, key):
        return self.__dict__["_vip_" + key]
    
    def __initializeIO(self, lst, value):
        try:
            for io in lst:
                self.__dict__["_vip_" + io.name] = value
        except: 
            pass
    
    ######################################
    # Public members
    ######################################
    
    def __init__(self):
        # Initialize all inputs/properties/outputs
        try: self.__initializeIO(type(self)._all_inputs, VipAnyData())
        except: pass
        try: self.__initializeIO(type(self)._all_multi_inputs, VipAnyDataList())
        except: pass
        try: self.__initializeIO(type(self)._all_properties, VipAnyData())
        except: pass
        try: self.__initializeIO(type(self)._all_multi_properties, VipAnyDataList())
        except: pass
        try: self.__initializeIO(type(self)._all_outputs, VipAnyData())
        except: pass
        try: self.__initializeIO(type(self)._all_multi_outputs, VipAnyDataList())
        except: pass
    
    def get_io(self, name) -> Union[VipAnyData , VipAnyDataList] :
        """
        Returns input/output/property with given name

        Parameters
        ----------
        name : str
            IO name.

        Returns
        -------
        VipAnyData or VipAnyDataList
            Corresponding input(s)/property(ies)/output(s).

        """
        return self.__attribute(name)
    
    def set_io(self, name, index, value):
        """
        Set the input/output/property value for given io name and index (for multi-io)

        Parameters
        ----------
        name : str
            IO name.
        index : int
            Index for IO of type VipAnyDataList.
        value : VipAnyData
            value.

        Returns
        -------
        None.

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
        """
        Processing algorithm implementation.
        Should throw on error.
        """
        pass
    
    def reset(self):
        """
        Reset the processing parameters
        Returns
        -------
        None.

        """
        
    def copy_parameters(self, dst):
        """
        Copy this processing parameters to another one of same type

        Parameters
        ----------
        dst : VipProcessingObject
            Output processing object of same type.

        Returns
        -------
        None.

        """
        pass
            
    
    def editor(self):
        """
        Returns the processing editor widget.
        The widget must already know the processing it is working on.
        This function must return a new widget.
        
        Returns
        -------
        Qt widget used to edit this processing.

        """
        pass
    
    def editor_address(self) -> int:
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
        except:
            return 0
    
    
    
class Processing(VipProcessingObject):
    
    def __init__(self):
        super().__init__()
        self.custom_input = [45.3]
    
    @VipInput("test", "tutu")
    def inputVal(self):
        """
        """
        pass
    
    @VipMultiInput
    def custom_input(self): pass
    
# =============================================================================
# print("start:", Processing._all_inputs)    
# a = Processing()
# print(a._all_inputs)
# print(a.input)
# a.input = VipAnyData(3, time=0)
# print(a.input)
# print(a.custom_input[0])
# a.custom_input.append(41.2)
# print(a.custom_input[1])
# =============================================================================

