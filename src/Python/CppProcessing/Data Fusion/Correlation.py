import ThermavipPyProcessing as th
import numpy as np
import scipy
import scipy.signal as sig
import scipy.ndimage as nd
import scipy.fftpack as fftp

class ThermavipConvolveSignals(th.ThermavipPyDataFusionProcessing):
    """Simple convolution of 2 signals"""

    def __init__(self):
        pass

    def apply(self, data_list, time):
        """Apply the processing for given data and time"""
        return sig.fftconvolve(data_list[0],data_list[1],mode="same")
        
    def name(self,input_names):
        pref = self.startPrefix(input_names)
        for i in range(len(input_names)):
            input_names[i] = input_names[i][len(pref):]
        return "convolve(" + " , ".join(input_names) + ")"


class ThermavipCorrelateSignals(th.ThermavipPyDataFusionProcessing):
    """Cross-correlation of 2 signals.

    Both signals are centred first, and the result is normalised by default, so
    that the peak marks the lag between them and the value is comparable from one
    pair of signals to another.
    """
    normalize = True

    def __init__(self):
        pass

    def apply(self, data_list, time):
        """Apply the processing for given data and time"""
        # Correlating raw signals makes the product of their means dominate: it is
        # a triangular envelope peaking at zero lag, so the measured lag came out
        # as zero whatever the real one, on any signal with an offset — degrees
        # Celsius, absolute pressure, a polarised voltage. Measured on two
        # identical pulses 60 samples apart with an offset of 20, the peak moves
        # from lag 0 to lag -60 once the means are removed.
        a = np.asarray(data_list[0], dtype=float)
        b = np.asarray(data_list[1], dtype=float)
        a = a - np.mean(a)
        b = b - np.mean(b)
        c = sig.correlate(a, b, mode="same", method="fft")
        if self.normalize:
            d = np.std(a) * np.std(b) * len(a)
            c = c / d if d > 0 else c * 0.0
        return c

    def parameters(self):
        return {"Normalize":("bool",self.normalize)}

    def setParameters(self,**kwargs):
        self.normalize = kwargs["Normalize"]
        
    def name(self,input_names):
        pref = self.startPrefix(input_names)
        for i in range(len(input_names)):
            input_names[i] = input_names[i][len(pref):]
        return "correlate(" + " , ".join(input_names) + ")"

