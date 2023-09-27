import ThermavipPyProcessing as th
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
    """Simple correlation of 2 signals"""

    def __init__(self):
        pass

    def apply(self, data_list, time):
        """Apply the processing for given data and time"""
        return sig.correlate(data_list[0],data_list[1],mode="same",method="fft")
        
    def name(self,input_names):
        pref = self.startPrefix(input_names)
        for i in range(len(input_names)):
            input_names[i] = input_names[i][len(pref):]
        return "correlate(" + " , ".join(input_names) + ")"

