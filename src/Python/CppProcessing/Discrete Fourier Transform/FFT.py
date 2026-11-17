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
#import numpy.fft as fft
import sys 

    
class ThermavipFFT(th.ThermavipPyProcessing):
    """
    Compute the N-dimensional discrete Fourier Transform.
    This function computes the N-dimensional discrete Fourier Transform over any number of axes 
    in an M-dimensional array by means of the Fast Fourier Transform (FFT).
    """
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        
        if len(data.shape) == 1:
            res =fftp.fft(data)
        elif len(data.shape) == 2:
            res =fftp.fft2(data)
        else:
            res =fftp.fftn(data)
        return res
        
    def unit(self, index, name):
        # A transform changes the quantity: the abscissa was left labelled with the
        # unit of the input, and the ordinate too.
        return 'Frequency [Hz]' if index == 0 else name + '.s'

    def parameters(self):
        return {}

    def setParameters(self,**kwargs):
        pass

class ThermavipIFFT(th.ThermavipPyProcessing):
    """
    Compute the N-dimensional inverse discrete Fourier Transform.
    This function computes the inverse of the N-dimensional discrete Fourier Transform over 
    any number of axes in an M-dimensional array by means of the Fast Fourier Transform (FFT). 
    In other words, ifftn(fftn(a)) == a to within numerical accuracy. 
    
    The input, analogously to ifft, should be ordered in the same way as is returned by fftn, i.e. 
    it should have the term for zero frequency in all axes in the low-order corner, the positive 
    frequency terms in the first half of all axes, the term for the Nyquist frequency in the middle 
    of all axes and the negative frequency terms in the second half of all axes, in order of decreasingly 
    negative frequency.
    """
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        
        # The real part, not the modulus. The docstring says a forward transform
        # followed by this one returns the original signal; the modulus folds every
        # negative sample onto its opposite, so a signal crossing zero came back
        # rectified and the round trip was not one.
        if len(data.shape) == 1:
            res = np.real(fftp.ifft(data))
        elif len(data.shape) == 2:
            res = np.real(fftp.ifft2(data))
        else:
            res = np.real(fftp.ifftn(data))
        return res
        
    def unit(self, index, name):
        # A transform changes the quantity: the abscissa was left labelled with the
        # unit of the input, and the ordinate too.
        return 'Time [s]' if index == 0 else name + '.s'

    def parameters(self):
        return {}

    def setParameters(self,**kwargs):
        pass

    
class ThermavipRFFT(th.ThermavipPyProcessing):
    """
    Compute the N-dimensional discrete Fourier Transform for real input.
    This function computes the N-dimensional discrete Fourier Transform over any number of axes 
    in an M-dimensional real array by means of the Fast Fourier Transform (FFT). By default, 
    all axes are transformed, with the real transform performed over the last axis, while the 
    remaining transforms are complex.
    """
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        # The module this file transforms with exposes no rfft2 and no rfftn: those
        # two branches raised an attribute error on every image. The one dimensional
        # transform is the one that is implemented, and the length it returns is the
        # one the base class writes back.
        if len(data.shape) != 1:
            raise RuntimeError('RFFT: only 1D signals are supported, got shape ' + str(data.shape))
        return fftp.rfft(data)
        
    def dims(self):
        # One dimension only, so the interface stops offering this on an image.
        return (1,1)

    def unit(self, index, name):
        # A transform changes the quantity: the abscissa was left labelled with the
        # unit of the input, and the ordinate too.
        return 'Frequency [Hz]' if index == 0 else name + '.s'

    def parameters(self):
        return {}

    def setParameters(self,**kwargs):
        pass

class ThermavipIRFFT(th.ThermavipPyProcessing):
    """
    Compute the inverse of the N-dimensional FFT of real input.

    This function computes the inverse of the N-dimensional discrete Fourier Transform for real input 
    over any number of axes in an M-dimensional array by means of the Fast Fourier Transform (FFT). 
    In other words, irfftn(rfftn(a), a.shape) == a to within numerical accuracy. (The a.shape is 
    necessary like len(a) is for irfft, and for the same reason.)

    The input should be ordered in the same way as is returned by rfftn, i.e. as for irfft for the final 
    transformation axis, and as for ifftn along all the other axes.
    """
    
    def __init__(self):
        pass

    def apply(self, data, time):
        
        
        # irfft already returns a real array, so taking the modulus was not only
        # wrong but redundant: it rectified the result.
        if len(data.shape) != 1:
            raise RuntimeError('IRFFT: only 1D signals are supported, got shape ' + str(data.shape))
        res = fftp.irfft(data)
        return res
        
    def dims(self):
        # One dimension only, so the interface stops offering this on an image.
        return (1,1)

    def unit(self, index, name):
        # A transform changes the quantity: the abscissa was left labelled with the
        # unit of the input, and the ordinate too.
        return 'Time [s]' if index == 0 else name + '.s'

    def parameters(self):
        return {}

    def setParameters(self,**kwargs):
        pass


class ThermavipFFTShift(th.ThermavipPyProcessing):
    """
    Shift the zero-frequency component to the center of the spectrum.
    
    This function swaps half-spaces for all axes listed (defaults to all). 
    Note that y[0] is the Nyquist component only if len(x) is even.
    """
    
    def __init__(self):
        pass

    def apply(self, data, time):
                
        res =fftp.fftshift(data)  
        return res
        
    def parameters(self):
        return {}

    def setParameters(self,**kwargs):
        pass

class ThermavipIFFTShift(th.ThermavipPyProcessing):
    """
    The inverse of fftshift. Although identical for even-length x, the functions differ by one sample for odd-length x.
    """
    
    def __init__(self):
        pass

    def apply(self, data, time):
                
        res =fftp.ifftshift(data)  
        return res
        
    def parameters(self):
        return {}

    def setParameters(self,**kwargs):
        pass
