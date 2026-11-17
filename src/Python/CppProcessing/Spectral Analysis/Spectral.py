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


class ThermavipPeriodogramWelch(th.ThermavipPyProcessing):
    """
    Estimate power spectral density using a periodogram.
    Welch’s method computes an estimate of the power spectral density by dividing the data 
    into overlapping segments, computing a modified periodogram for each segment and averaging the periodograms.
    Parameters:	
    - window : Desired window to use, default to 'boxcar' (DFT-even by default)
    - detrend : Specifies how to detrend each segment, default to 'constant'
    - scaling : Selects between computing the power spectral density ('density') 
      where Pxx has units of V**2/Hz and computing the power spectrum ('spectrum') 
      where Pxx has units of V**2, if x is measured in V and fs is measured in Hz. 
      Defaults to 'density'
    """
    
    
    def __init__(self):
        self.window = "hann"
        self.detrend = "constant"
        self.scaling = "density"

    def _apply(self, data, time):
        if len(data) < 2 or len(data[0]) < 2:
            raise RuntimeError("Welch: wrong or empty input data")
        y = data[1,:]
        x = data[0,:]
        sampling = 1000000000.0/(x[1] - x[0])
        #x = x / 1000000000.0 #pass x in seconds
        detr = False
        if self.detrend != "No detrend":
            detr = self.detrend
        #print(y)
        #print(sampling)
        f, Pxx_den = sig.welch(y, sampling, window=self.window,  noverlap=None, nfft=None, detrend=detr, return_onesided=True, scaling=self.scaling)
        res= np.array([f,Pxx_den])
        #print(res)
        return res
    
    def dims(self): return (1,1)
    def unit(self, index, name):
        if index == 0:
            return 'Frequency [Hz]'
        elif index == 1:
            res = 'PSD'
            if self.scaling == 'density': res += " ["+name+"**2/Hz]"
            else: res +=  " ["+name+"**2]"
            return res
        else:
            return name
            
            
    def parameters(self):
        return {"Window":("str",self.window,"hann", "triang", "blackman", "hamming", "boxcar", "bartlett", "flattop", "parzen", "bohman", "blackmanharris", "nuttall", "barthann") , \
        "Detrend" : ("str",self.detrend,"No detrend","constant","linear"),\
        "Scaling" : ("str",self.scaling,"density","spectrum") }

    def setParameters(self,**kwargs):
        self.window = kwargs["Window"]
        self.detrend = kwargs["Detrend"]
        self.scaling = kwargs["Scaling"]

    def displayHint(self):
        return th.DisplayOnDifferentSupport 

