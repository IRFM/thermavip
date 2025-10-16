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
import scipy.misc as misc
import numpy as np
import sys 

def any_float(default):
    return ("float",default,-sys.float_info.max,sys.float_info.max,0)


class ThermavipMedianFilter(th.ThermavipPyProcessing):
    """Simple median filter"""
    kernel_size = 3
    
    def __init__(self):
        pass

    def apply(self, data, time):
        """Apply the processing for given data and time"""
        if len(data.shape) != 2:
            return sig.medfilt(data,self.kernel_size)
        else:
            return sig.medfilt2d(np.array(data,dtype=np.float64),self.kernel_size)
        
    def parameters(self):
        return {"KernelSize":("int",self.kernel_size,1,101,2) }

    def setParameters(self,**kwargs):
        self.kernel_size = kwargs["KernelSize"];
    
class ThermavipConvolve(th.ThermavipPyProcessing):
    """Convolve two N-dimensional arrays.
    Mode indicates the output size:
        'full': The output is the full discrete linear convolution of the inputs. (Default)
        'valid': The output consists only of those elements that do not rely on the zero-padding.
        'same': The output is the same size as array 1, centered with respect to the 'full' output.
    """
    other_array = 0
    mode = "full"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        if type(self.other_array) == int and self.other_array == 0:
            kernel = np.ones((1,) * len(data.shape))
        elif self.other_array is None:
             kernel = np.ones((1,) * len(data.shape))
        elif len(self.other_array.shape) < len(data.shape):
            kernel = self.other_array.reshape(self.other_array.shape + (1,) * (len(data.shape) - len(self.other_array.shape)))
        elif len(self.other_array.shape) == 2 and len(data.shape) == 1: #other_array is a 2D signal
            kernel = self.other_array[0,:]
        elif len(self.other_array.shape) == len(data.shape)+1 and self.other_array.shape[-1] == 1:
            kernel = self.other_array.reshape(self.other_array.shape[0:len(data.shape)-1])
        elif len(self.other_array.shape) == len(data.shape)+1 and self.other_array.shape[0] == 1:
            kernel = self.other_array.reshape(self.other_array.shape[1:])
        else:
            kernel = self.other_array
        m = self.mode
        if len(kernel.shape) == 1 and m == "full":
                m = "same"
        return sig.fftconvolve(data, kernel,m)
        
    def parameters(self):
        return {"Other_array":("other") , \
        "Mode":("str",self.mode,"full","valid","same") }

    def setParameters(self,**kwargs):
        self.other_array = kwargs["Other_array"];
        self.mode = kwargs["Mode"];

class ThermavipCorrelate(th.ThermavipPyProcessing):
    """Correlate two N-dimensional arrays.
    Mode indicates the output size:
        'full': The output is the full discrete linear cross-correlation of the inputs. (Default)
        'valid': The output consists only of those elements that do not rely on the zero-padding.
        'same': The output is the same size as array 1, centered with respect to the 'full' output.
    """
    other_array = 0
    mode = "full"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        if type(self.other_array) == int and self.other_array == 0:
            kernel = np.ones((1,) * len(data.shape))
        elif self.other_array is None:
             kernel = np.ones((1,) * len(data.shape))
        elif len(self.other_array.shape) < len(data.shape):
            kernel = self.other_array.reshape(self.other_array.shape + (1,) * (len(data.shape) - len(self.other_array.shape)))
        elif len(self.other_array.shape) == 2 and len(data.shape) == 1: #other_array is a 2D signal
            kernel = self.other_array[0,:]
        elif len(self.other_array.shape) == len(data.shape)+1 and self.other_array.shape[-1] == 1:
            kernel = self.other_array.reshape(self.other_array.shape[0:len(data.shape)-1])
        elif len(self.other_array.shape) == len(data.shape)+1 and self.other_array.shape[0] == 1:
            kernel = self.other_array.reshape(self.other_array.shape[1:])
        else:
            kernel = self.other_array
        m = self.mode
        if len(kernel.shape) == 1 and m == "full":
                m = "same"
        if len(kernel.shape) == 1:
                return sig.fftconvolve(data, kernel[::-1],m)
        elif len(kernel.shape) == 2:
                return sig.fftconvolve(data, kernel[::-1, ::-1],m)
        
    def parameters(self):
        return {"Other_array":("other") , \
        "Mode":("str",self.mode,"full","valid","same") }

    def setParameters(self,**kwargs):
        self.other_array = kwargs["Other_array"];
        self.mode = kwargs["Mode"];

class ThermavipAutoCorrelate(th.ThermavipPyProcessing):
    """Correlate a signal with itself.
    Mode indicates the output size:
        'full': The output is the full discrete linear cross-correlation of the inputs. (Default)
        'valid': The output consists only of those elements that do not rely on the zero-padding.
        'same': The output is the same size as array 1, centered with respect to the 'full' output.
    """
    other_array = 0
    mode = "full"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        m = self.mode
        if len(data.shape) == 1 and m == "full":
                m = "same"
        if len(data.shape) == 1:
                return sig.fftconvolve(data, data[::-1],m)
        elif len(data.shape) == 2:
                return sig.fftconvolve(data, data[::-1, ::-1],m)
        
    def parameters(self):
        return {"Mode":("str",self.mode,"full","valid","same") }

    def setParameters(self,**kwargs):
        self.mode = kwargs["Mode"];


class ThermavipOrderFilter(th.ThermavipPyProcessing):
    """Perform an order filter on the input array. 
    The domain argument acts as a mask centered over each pixel. 
    The non-zero elements of domain are used to select elements surrounding each input pixel which are placed in a list. 
    The list is sorted, and the output for that pixel is the element corresponding to rank in the sorted list.
    """
    kernel_size = 3
    rank = 0
    
    def __init__(self):
        pass

    def apply(self, data, time):
        kernel_shape = ((self.kernel_size,)*len(data.shape))
        kernel = np.ones(kernel_shape)
        return sig.order_filter(data, kernel, self.rank)
        
    def parameters(self):
        return {"KernelSize":("int",self.kernel_size,3,31,2) , \
        "Rank":("int",self.rank,0,1000) }

    def setParameters(self,**kwargs):
        self.kernel_size = kwargs["KernelSize"];
        self.rank = kwargs["Rank"];


class ThermavipWienerFilter(th.ThermavipPyProcessing):
    """Perform a Wiener filter on an N-dimensional array.
    It uses a kernel size and a noise-power to use. 
    If null, then noise is estimated as the average of the local variance of the input.
    """
    kernel_size = 3
    noise = 0.1
    
    def __init__(self):
        pass

    def apply(self, data, time):
        Noise = self.noise
        if Noise == 0:
            Noise = None
        return sig.wiener(data, self.kernel_size,self.noise)
        
    def parameters(self):
        return {"KernelSize":("int",self.kernel_size,3,31,2) , \
        "Noise":any_float(self.noise) }

    def setParameters(self,**kwargs):
        self.kernel_size = kwargs["KernelSize"];
        self.noise = kwargs["Noise"];

class ThermavipSavgolFilter(th.ThermavipPyProcessing):
    """
    Apply a Savitzky-Golay filter to an array.
    
    - window_length : The length of the filter window (i.e. the number of coefficients). window_length must be a positive odd integer.
    - polyorder : The order of the polynomial used to fit the samples. polyorder must be less than window_length.
    - deriv : The order of the derivative to compute. This must be a nonnegative integer. The default is 0, which means to filter the data without differentiating.
    - delta : The spacing of the samples to which the filter will be applied. This is only used if deriv > 0. Default is 1.0.
    - axis : The axis of the array x along which the filter is to be applied. Default is -1.
    - mode : Must be 'mirror', 'constant', 'nearest', 'wrap' or 'interp'. This determines the type of extension to use for the padded signal 
      to which the filter is applied. When mode is 'constant', the padding value is given by cval. See the Notes for more details on 'mirror', 
      'constant', 'wrap', and 'nearest'. When the 'interp' mode is selected (the default), no extension is used. Instead, a degree polyorder 
      polynomial is fit to the last window_length values of the edges, and this polynomial is used to evaluate the last window_length // 2 output values.
    - cval : Value to fill past the edges of the input if mode is 'constant'. Default is 0.0.
    """
    window_length =3
    polyorder = 2
    deriv=0
    delta=1.0
    mode='interp'
    cval=0.0
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return sig.savgol_filter(data, self.window_length,self.polyorder,self.deriv,self.delta,-1,self.mode,self.cval)
        
    def parameters(self):
        return {"Window_length":("int",self.window_length,3,31,2) , \
        "Polyorder":("int",self.polyorder,0,31,1) ,\
        "Deriv":("int",self.deriv,0,6,1) , \
        "Delta":any_float(self.delta) , \
        "Mode": ("str",self.mode,"mirror","constant","nearest","wrap","interp"), \
        "Cval":any_float(self.cval) }

    def setParameters(self,**kwargs):
        self.window_length = kwargs["Window_length"];
        self.polyorder = kwargs["Polyorder"];
        self.deriv = kwargs["Deriv"];
        self.delta = kwargs["Delta"];
        self.mode = kwargs["Mode"];
        self.cval = kwargs["Cval"];


class ThermavipGaussianFilter(th.ThermavipPyProcessing):
    """
    Multidimensional Gaussian filter.

    Parameters:	
    - sigma : The standard deviations of the Gaussian filter are given for each axis as a sequence, or as a single number, 
      in which case it is equal for all axes.
    - order : The order of the filter along each axis is given as a sequence of integers, or as a single number. 
      An order of 0 corresponds to convolution with a Gaussian kernel. A positive order corresponds to convolution 
      with that derivative of a Gaussian.
    """
    sigma = 1
    order = 0
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.gaussian_filter(data, self.sigma,self.order)
        
    def parameters(self):
        return {"Sigma":("float",self.sigma,0,sys.float_info.max,0) , \
        "Order": ("int",self.order,0,10,1) }

    def setParameters(self,**kwargs):
        self.sigma = kwargs["Sigma"];
        self.order = kwargs["Order"];

class ThermavipGaussianGradientMagnitude(th.ThermavipPyProcessing):
    """
    Multidimensional gradient magnitude using Gaussian derivatives.

    Parameters:	
    - sigma : The standard deviations of the Gaussian filter are given for each axis as a sequence
    - mode : The mode parameter determines how the array borders are handled. 
      Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}.
    """
    sigma = 1
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.gaussian_gradient_magnitude(data, self.sigma,None,self.mode)
        
    def parameters(self):
        return {"Sigma":("float",self.sigma,0,sys.float_info.max,0) , \
        "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.sigma = kwargs["Sigma"];
        self.mode = kwargs["Mode"];

class ThermavipLaplace(th.ThermavipPyProcessing):
    """
    N-dimensional Laplace filter based on approximate second derivatives.

    Parameters:	
    - mode : The mode parameter determines how the array borders are handled. 
    Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
    """
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.laplace(data, mode=self.mode)
        
    def parameters(self):
        return { "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.mode = kwargs["Mode"];

class ThermavipMaximumFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional maximum filter.

    Parameters:	
     - size : Gives the shape that is taken from the input array, at every element position, to define the input to the filter function.
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    size = 3
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.maximum_filter(data, self.size,None,None,self.mode)
        
    def parameters(self):
        return {"Size":("int",self.size,1,31,1) , \
        "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.size = kwargs["Size"];
        self.mode = kwargs["Mode"];

class ThermavipMinimumFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional minimum filter.

    Parameters:	
     - size : Gives the shape that is taken from the input array, at every element position, to define the input to the filter function.
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    size = 3
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.minimum_filter(data, self.size,None,None,self.mode)
        
    def parameters(self):
        return {"Size":("int",self.size,1,31,1) , \
        "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.size = kwargs["Size"];
        self.mode = kwargs["Mode"];

class ThermavipPercentileFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional percentile filter.

    Parameters:	
     - size : Gives the shape that is taken from the input array, at every element position, to define the input to the filter function.
     - percentile : The percentile parameter may be less then zero, i.e., percentile = -20 equals percentile = 80
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    size = 3
    percentile = 20
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.percentile_filter(data,self.percentile, self.size,None,None,self.mode)
        
    def parameters(self):
        return {"Size":("int",self.size,1,31,1) , \
                "Percentile":("int",self.percentile,-100,100,1) ,\
                "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.size = kwargs["Size"];
        self.mode = kwargs["Mode"];

class ThermavipPrewittFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional Prewitt filter.

    Parameters:	
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.prewitt(data, -1,None,self.mode)
        
    def parameters(self):
        return {"Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.mode = kwargs["Mode"];

class ThermavipRankFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional Rank filter.

    Parameters:
     - rank: The rank parameter may be less then zero, i.e., rank = -1 indicates the largest element
     - size : Gives the shape that is taken from the input array, at every element position, to define the input to the filter function.
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    rank = 0
    size = 3
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.rank_filter(data, self.rank,self.size,None,None,self.mode)
        
    def parameters(self):
        return {"Rank":("int",self.rank) ,\
        "Size":("int",self.size,1,31,1) , \
        "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.rank = kwargs["Rank"];
        self.size = kwargs["Size"];
        self.mode = kwargs["Mode"];

class ThermavipSobelFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional Sobel filter.

    Parameters:	
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.sobel(data, -1,None,self.mode)
        
    def parameters(self):
        return {"Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.mode = kwargs["Mode"];

class ThermavipUniformFilter(th.ThermavipPyProcessing):
    """
    Calculate a multi-dimensional Rank filter.

    Parameters:
     - size : Gives the shape that is taken from the input array, at every element position, to define the input to the filter function.
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    size = 3
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.uniform_filter(data, self.size,None,self.mode)
        
    def parameters(self):
        return {"Size":("int",self.size,1,31,1) , \
        "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.size = kwargs["Size"];
        self.mode = kwargs["Mode"];

class ThermavipShiftFilter(th.ThermavipPyProcessing):
    """
    Shift an array.

    The array is shifted using spline interpolation of the requested order. 
    Points outside the boundaries of the input are filled according to the given mode.

    Parameters:
     - shift : The shift value.
     - order : The order of the spline interpolation, default is 3. The order has to be in the range 0-5.
     - mode : The mode parameter determines how the array borders are handled. 
     Valid modes are {'reflect', 'nearest', 'mirror', 'wrap'}. 
     """
    shift= 1.
    order = 3
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.shift(data,self.shift, None, self.order, self.mode)
        
    def parameters(self):
        return {"Shift":("float",self.shift) , \
        "Order":("int",self.order,0,5,1) , \
        "Mode": ("str",self.mode,"reflect","nearest","wrap") }

    def setParameters(self,**kwargs):
        self.shift = kwargs["Shift"];
        self.mode = kwargs["Mode"];

class ThermavipSplineFilter(th.ThermavipPyProcessing):
    """
    Calculates a one-dimensional spline filter along the given axis.

    The lines of the array are filtered by a spline filter. The order of the spline must be >= 2 and <= 5.

    Parameters:
     - order : The order of the spline interpolation, default is 3.
     """
    order = 3
    mode = "reflect"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        return nd.spline_filter(data, self.order)
        
    def parameters(self):
        return {"Order":("int",self.order,2,5,1) }

    def setParameters(self,**kwargs):
        self.order = kwargs["Order"];
