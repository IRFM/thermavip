# -*- coding: utf-8 -*-
"""
Created on Tue Nov 28 09:03:19 2017

@author: Moncada
"""


import ThermavipPyProcessing as th
import scipy
import scipy.signal as sig
import scipy.ndimage as nd
import scipy.fftpack as fftp
import numpy as np
import sys 

def any_float(default):
    return ("float",default,-sys.float_info.max,sys.float_info.max,0)
    
def any_int(default):
    return ("int",default,-sys.maxsize,sys.maxsize,1)
    
def structure(full_connectivity, data):
    if full_connectivity:
        return nd.generate_binary_structure(np.ndim(data), np.ndim(data))
    else:
        return nd.generate_binary_structure(np.ndim(data), np.ndim(data)-1)

class ThermavipBinaryClosing(th.ThermavipPyProcessing):
    """
    Multi-dimensional binary closing.

    The closing of an input image by a structuring element is the erosion of the dilation of 
    the image by the structuring element.
    
    Parameters:	
    - iterations : The dilation step of the closing, then the erosion step are each repeated iterations times (one, by default). 
    If iterations is less than 1, each operations is repeated until the result does not change anymore.
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    """
    iterations = 1
    FullConnectivity = True
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.binary_closing(np.array(data,dtype=np.bool),structure(self.FullConnectivity,data),self.iterations)
        
    def parameters(self):
        return {"Iterations":("int",self.iterations,0,20,1),\
                "FullConnectivity":("bool",self.FullConnectivity)}

    def setParameters(self,**kwargs):
        self.iterations = kwargs["Iterations"]
        self.FullConnectivity = kwargs["FullConnectivity"]

class ThermavipBinaryOpening(th.ThermavipPyProcessing):
    """
    Multi-dimensional binary opening.

    The opening of an input image by a structuring element is the dilation 
    of the erosion of the image by the structuring element.
    
    Parameters:	
    - iterations: The erosion step of the opening, then the dilation step are each repeated iterations times (one, by default). 
    If iterations is less than 1, each operation is repeated until the result does not change anymore.
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    """
    iterations = 1
    FullConnectivity = True
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.binary_opening(np.array(data,dtype=np.bool),structure(self.FullConnectivity,data),self.iterations)
        
    def parameters(self):
        return {"Iterations":("int",self.iterations,0,20,1),\
                "FullConnectivity":("bool",self.FullConnectivity)}

    def setParameters(self,**kwargs):
        self.iterations = kwargs["Iterations"]
        self.FullConnectivity = kwargs["FullConnectivity"]

class ThermavipBinaryDilation(th.ThermavipPyProcessing):
    """
    Multi-dimensional binary dilation.

    Parameters:	
    - iterations: The dilation is repeated iterations times (one, by default). 
    If iterations is less than 1, the dilation is repeated until the result does not change anymore.
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    """
    iterations = 1
    FullConnectivity = True
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.binary_dilation(np.array(data,dtype=np.bool),structure(self.FullConnectivity,data),self.iterations)
        
    def parameters(self):
        return {"Iterations":("int",self.iterations,0,20,1),\
                "FullConnectivity":("bool",self.FullConnectivity)}

    def setParameters(self,**kwargs):
        self.iterations = kwargs["Iterations"]
        self.FullConnectivity = kwargs["FullConnectivity"]
    
class ThermavipBinaryErosion(th.ThermavipPyProcessing):
    """
    Multi-dimensional binary erosion.

    Parameters:	
    - iterations: The erosion is repeated iterations times (one, by default). 
    If iterations is less than 1, the erosion is repeated until the result does not change anymore.
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    """
    iterations = 1
    FullConnectivity = True
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.binary_erosion(np.array(data,dtype=np.bool),structure(self.FullConnectivity,data),self.iterations)
        
    def parameters(self):
        return {"Iterations":("int",self.iterations,0,20,1),\
                "FullConnectivity":("bool",self.FullConnectivity)}

    def setParameters(self,**kwargs):
        self.iterations = kwargs["Iterations"]
        self.FullConnectivity = kwargs["FullConnectivity"]

class ThermavipBinaryFillHoles(th.ThermavipPyProcessing):
    """
    Fill the holes in binary objects.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    """
    FullConnectivity = True
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.binary_fill_holes(np.array(data,dtype=np.bool),structure(self.FullConnectivity,data),self.iterations)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity)}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]

class ThermavipGreyClosing(th.ThermavipPyProcessing):
    """
    Multi-dimensional greyscale closing.

    A greyscale closing consists in the succession of a greyscale dilation, and a greyscale erosion.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    - Mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    FullConnectivity = True
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.grey_closing(data, footprint=structure(self.FullConnectivity,data),mode = self.mode)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity),\
                "Mode": ("str",self.mode,"reflect","nearest","wrap")}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]
        self.mode = kwargs["Mode"]

class ThermavipGreyDilation(th.ThermavipPyProcessing):
    """
    Calculate a greyscale dilation.

    Grayscale dilation is a mathematical morphology operation. 
    For the simple case of a full and flat structuring element, 
    it can be viewed as a maximum filter over a sliding window.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    - Mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    FullConnectivity = True
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.grey_dilation(data, footprint=structure(self.FullConnectivity,data),mode = self.mode)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity),\
                "Mode": ("str",self.mode,"reflect","nearest","wrap")}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]
        self.mode = kwargs["Mode"]

class ThermavipGreyErosion(th.ThermavipPyProcessing):
    """
    Calculate a greyscale erosion.

    Grayscale dilation is a mathematical morphology operation. 
    For the simple case of a full and flat structuring element, 
    it can be viewed as a minimum filter over a sliding window.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    - Mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    FullConnectivity = True
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.grey_erosion(data, footprint=structure(self.FullConnectivity,data),mode = self.mode)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity),\
                "Mode": ("str",self.mode,"reflect","nearest","wrap")}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]
        self.mode = kwargs["Mode"]

class ThermavipGreyOpening(th.ThermavipPyProcessing):
    """
    Multi-dimensional greyscale opening.

    A greyscale opening consists in the succession of a greyscale erosion, and a greyscale dilation.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    - Mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    FullConnectivity = True
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.grey_opening(data, footprint=structure(self.FullConnectivity,data),mode = self.mode)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity),\
                "Mode": ("str",self.mode,"reflect","nearest","wrap")}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]
        self.mode = kwargs["Mode"]


class ThermavipMorphologicalGradient(th.ThermavipPyProcessing):
    """
    Multi-dimensional morphological gradient.

    The morphological gradient is calculated as the difference between a dilation and an erosion
    of the input with a given structuring element.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    - Mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    FullConnectivity = True
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.morphological_gradient(data, footprint=structure(self.FullConnectivity,data),mode = self.mode)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity),\
                "Mode": ("str",self.mode,"reflect","nearest","wrap")}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]
        self.mode = kwargs["Mode"]

class ThermavipMorphologicalLaplace(th.ThermavipPyProcessing):
    """
    Multi-dimensional morphological laplace.
    
    Parameters:
    - FullConnectivity: If true, diagonally-connected elements are considered neighbors.
    - Mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    FullConnectivity = True
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return nd.morphological_laplace(data, footprint=structure(self.FullConnectivity,data),mode = self.mode)
        
    def parameters(self):
        return {"FullConnectivity":("bool",self.FullConnectivity),\
                "Mode": ("str",self.mode,"reflect","nearest","wrap")}

    def setParameters(self,**kwargs):
        self.FullConnectivity = kwargs["FullConnectivity"]
        self.mode = kwargs["Mode"]
