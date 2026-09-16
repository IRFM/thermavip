# -*- coding: utf-8 -*-
"""
Created on Thu Nov 30 07:57:46 2017

@author: Moncada
"""


import ThermavipPyProcessing as th
import scipy
import scipy.signal as sig
import scipy.fftpack as fftp
import numpy as np
import sys 

def any_float(default):
    return ("float",default,-sys.float_info.max,sys.float_info.max,0)
    
def any_int(default):
    return ("int",default,-sys.maxsize,sys.maxsize,1)

    
    
class ThermavipAnalyticSignal(th.ThermavipPyProcessing):
    """Compute the analytic signal, using the Hilbert transform.
    N is the number of Fourier components. Default to the input shape (N == 0)
    The analytic signal x_a(t) of signal x(t) is:

    x_a = F^{-1}(F(x) 2U) = x + i y
    
    where F is the Fourier transform, U the unit step function, and y the Hilbert transform of x.
    In other words, the negative half of the frequency spectrum is zeroed out, turning the 
    real-valued signal into a complex signal. 
    """
    N = 0
    axis = "horizontal"
    output = "real"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        if self.axis == "horizontal":
            if len(data.shape) == 1:
                ax = -1
            else:
                ax = 1
        else:
            if len(data.shape) == 1:
                ax = -1
            else:
                ax = 0
        if self.N == 0:
            N = None
        else:
            N = self.N
        
        r = sig.hilbert(data, N, ax)
        if self.output == "real":
            r = np.real(r)
        elif self.output == "imag":
            r = np.imag(r)
        elif self.output == "amplitude":
            r = np.absolute(r)
        elif self.output == "argument":
            r = np.angle(r, deg=True)  
        return r
        
    def parameters(self):
        return {"N":("int",self.N,0,sys.maxsize,1) , \
        "Axis":("str",self.axis,"horizontal","vertical"),\
        "Output" : ("str",self.output,"real","imag","amplitude","argument") }

    def setParameters(self,**kwargs):
        self.N = kwargs["N"]
        self.axis = kwargs["Axis"]
        self.output = kwargs["Output"]

    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,1)


class ThermavipHilbert(th.ThermavipPyProcessing):
    """Return Hilbert transform of a periodic sequence x.
        If x_j and y_j are Fourier coefficients of periodic functions x and y, respectively, then:
        
        y_j = sqrt(-1)*sign(j) * x_j
        y_0 = 0
    """
    output = "real"
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        r = fftp.hilbert(data)
        if self.output == "real":
            r = np.real(r)
        elif self.output == "imag":
            r = np.imag(r)
        elif self.output == "amplitude":
            r = np.absolute(r)
        elif self.output == "argument":
            r = np.angle(r, deg=True)  
        return r
        
    def parameters(self):
        return {"Output" : ("str",self.output,"real","imag","amplitude","argument") }

    def setParameters(self,**kwargs):
        self.output = kwargs["Output"]

    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,1)


class ThermavipDecimate(th.ThermavipPyProcessing):
    """Downsample the signal by using a filter.
        By default, an order 8 Chebyshev type I filter is used. A 30 point FIR filter with hamming window is used if type is "FIR".
    """
    ftype = "Chebyshev"
    q = 2
    n = None
    
    def __init__(self):
        pass

    def _apply(self, data, time):
        # Decimating shortens the signal, so the time base has to be decimated with
        # it. The base class keeps the length: it writes the result back into
        # res[1,:], which raises on a shorter array, after which the caller
        # republishes the untouched input. Overriding _apply is the way out the
        # base class documents.
        t = "iir" if self.ftype == "Chebyshev" else "fir"
        if type(data) == np.ndarray and len(data.shape) == 2 and data.shape[0] == 2:
            y = sig.decimate(data[1,:], self.q, self.n, t)
            x = data[0, ::self.q][:len(y)]
            return np.vstack((x, y))
        return sig.decimate(data, self.q, self.n, t)

    def apply(self, data, time):
        t = "iir" if self.ftype == "Chebyshev" else "fir"
        return sig.decimate(data, self.q, self.n, t)
        
    def parameters(self):
        # From 2: a factor of 0 divides by zero inside decimate, and 1 applies the
        # low pass without decimating, which narrows the band of the signal with
        # nothing in the interface saying so.
        return {"Type":("str",self.ftype,"Chebyshev","FIR") , \
        "Factor":("int",self.q,2,100,1) }

    def setParameters(self,**kwargs):
        self.ftype = kwargs["Type"];
        self.q = kwargs["Factor"];

    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,1)


class ThermavipSymiirorder1(th.ThermavipPyProcessing):
    """
    Implement a smoothing IIR filter with mirror-symmetric boundary conditions 
    using a cascade of first-order sections. The second section uses a reversed sequence. 
    This implements a system with the following transfer function and mirror-symmetric boundary conditions:
                        c0              
        H(z) = ---------------------    
                (1-z1/z) (1 - z1 z)     
        The resulting signal will have mirror symmetric boundary conditions as well.
    """
    # c0 is not independent: the filter has unit gain only when it equals
    # (1 - z1)^2. Left at 1.0 with z1 = 0.1, a constant signal came out scaled by
    # about 1.23, which reads as a measurement.
    z1 = 0.1
    precision = 1.
    
    def __init__(self):
        pass

    def apply(self, data, time):
        c0 = (1.0 - self.z1) ** 2
        return sig.symiirorder1(data, c0, self.z1, self.precision)
        
    def parameters(self):
        # |z1| < 1 is the stability condition of the first order section.
        return {"z1":("float",self.z1,-0.999,0.999,0.01) , \
        "precision":any_float(self.precision) }

    def setParameters(self,**kwargs):
        self.z1 = kwargs["z1"];
        self.precision = kwargs["precision"];

    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,1)

class ThermavipSymiirorder2(th.ThermavipPyProcessing):
    """
    Implement a smoothing IIR filter with mirror-symmetric boundary conditions using a cascade of 
    second-order sections. The second section uses a reversed sequence. This implements the following 
    transfer function:

                                 cs^2
        H(z) = ---------------------------------------
               (1 - a2/z - a3/z^2) (1 - a2 z - a3 z^2 )
        where:
        
        a2 = (2 r cos omega)
        a3 = - r^2
        cs = 1 - 2 r cos omega + r^2
    """
    # Strictly inside the unit circle: r = 1.0 put both poles exactly on it, so
    # the section shipped was not stable.
    r = 0.9
    omega = 1.
    precision = 1.
    
    def __init__(self):
        pass

    def apply(self, data, time):
        return sig.symiirorder2(data, self.r,self.omega,self.precision)
        
    def parameters(self):
        # r < 1 is the stability condition; omega is an angle, so 0 to pi.
        return {"r":("float",self.r,0.0,0.999,0.01) , \
                "omega":("float",self.omega,0.0,3.141592653589793,0.01) , \
        "precision":any_float(self.precision) }

    def setParameters(self,**kwargs):
        self.r = kwargs["r"];
        self.omega = kwargs["omega"];
        self.precision = kwargs["precision"];

    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,1)

