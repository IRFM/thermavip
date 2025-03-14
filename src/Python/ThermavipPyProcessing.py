# -*- coding: utf-8 -*-
"""
Created on Mon Nov 27 13:27:02 2017

@author: Moncada
"""
import numpy as np


InputTransform = 0
DisplayOnSameSupport = 1
DisplayOnDifferentSupport = 2
UnknowDisplayHint = 3

class ThermavipPyProcessing:
    """Base class for Python processings in Thermavip
    All Thermavip processing classes must start with "Thermavip"
    """
    
    def __init__(self):
        pass
    
    def reset(self, data, time):
        """Reset the processing based on the current data and time"""
        return self._apply(data,time)
    
    def apply(self, data, time):
        """Apply the processing for given data and time"""
        return data
        
    def parameters():
        """Returns the processing parameters on the form dict{"name1":(type1,default_val1,min1,max1,step1), "name2":...)}
        or dict{"name1":(str,("default_value","val1","val2",...), "name2":...)} for enumerations.
        The only supported types are int, bool, float, str and other (an array comming from another player or edited manually by the user)
		Example:
		return {"KernelSize":("int",self.kernel_size,1,101,2) , \
				"test1":("float",2.3,1,101) ,\
				"test2":("bool",1) ,\
				"test3":("str","toto") ,\
				"test4":("str","toto","toto","tutu","tata"), \
				"other_data":("other")}
        """
        return dict()
        
    def setParameters(**kwargs):
        """Set the processing parameters as a dictionnary"""
        pass
        
    def _apply(self,data,time):
        """
        Internal function. Apply the processing after reformatting the input signals.
        You should overload this function only if you don't want a pre reformatting of input
        signals and you know what you are doing.
        """
        try:
            #try to convert float 128 that make most processings from scipy crash
            if type(data) == np.ndarray and data.dtype == np.float128:
                data = np.array(data,dtype=np.float64)
        except:
            pass
            
        if type(data) == np.ndarray:
            if len(data.shape) == 2 and data.shape[0] == 2:
                tmp = data[1,:]
                tmp_res = self.apply(tmp,time)
                if tmp_res.dtype == np.complex128:
                    res = np.array(data, dtype = np.complex128)
                elif tmp_res.dtype == np.complex64:
                    res = np.array(data, dtype = np.complex64)
                elif data.dtype == np.complex64 or data.dtype == np.complex128 :
                    res = data.real;#np.real(data)
                else:
                    res = data				
                res[1,:] = tmp_res
                return res#.copy()
            #case RGBA images
            elif len(data.shape) == 3 and (data.shape[2] == 3 or data.shape[2] == 4 ):
                res = data.copy()
                for i in range(data.shape[2]):
                    res[:,:,i] = np.clip(self.apply(data[:,:,i],time),0,255)
                return res;
        return self.apply(data,time)

    def _reset(self,data,time):
        """
        Internal function. reset the processing after reformatting the input signals.
        You should overload this function only if you don't want a pre reformatting of input
        signals and you know what you are doing.
        """
        return self.reset(data,time) 		
    
    def unit(self, index, name):
        """Returns the unit for given dimension based on its original unit name"""
        return name
        
    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,3)

    def inputCount(self):
        return (1,1) #min and max input count, always (1,1) for ThermavipPyProcessing
    def displayHint(self):
        return InputTransform #input transform (same input and output type), always true for ThermavipPyProcessing







class ThermavipPyDataFusionProcessing:
    """Base class for Python processings based on data fusion in Thermavip (multiple inputs, one output).
    All Thermavip processing classes must start with "Thermavip"
    """

    def __init__(self):
        pass

    def __commonprefix(self,m):
        """Given a list of pathnames, returns the longest common leading component"""
        if len(m)==0: return str()
        first = m[0]
        all_same=True
        for s in m:
            if s != first:
                all_same=False
                break
        if all_same: return str()

        if not m: return ''
        s1 = min(m)
        s2 = max(m)
        for i, c in enumerate(s1):
            if c != s2[i]:
                return s1[:i]
        return s1

    def startPrefix(self,names):
        """
        Returns the common prefix for a list of names.
        This function can be used in self.name() to return a proper processing name.
        """
        pref = self.__commonprefix(names)
        if len(pref) == 0: return pref
        if pref[-1] != '/':
            index = pref.rfind('/')
            if index >= 0:
                pref = pref[0:index]
        return pref

    
    def reset(self, data_list, time):
        """Reset the processing based on the current data and time"""
        return self._apply(data_list,time)
    
    def apply(self, data_list, time):
        """Apply the processing for given list of data and time"""
        return data_list[0]
        
    def parameters():
        """Returns the processing parameters on the form dict{"name1":(type1,default_val1,min1,max1,step1), "name2":...)}
        or dict{"name1":(str,("default_value","val1","val2",...), "name2":...)} for enumerations.
        The only supported types are int, bool, float, str and other (an array comming from another player or edited manually by the user)
		Example:
		return {"KernelSize":("int",self.kernel_size,1,101,2) , \
				"test1":("float",2.3,1,101) ,\
				"test2":("bool",1) ,\
				"test3":("str","toto") ,\
				"test4":("str","toto","toto","tutu","tata"), \
				"other_data":("other")}
        """
        return dict()
        
    def setParameters(**kwargs):
        """Set the processing parameters as a dictionnary"""
        pass
        
    def _apply(self,data_list,time):
        """
        Internal function. Apply the processing after reformatting the input signals.
        You should overload this function only if you don't want a pre reformatting of input
        signals and you know what you are doing.
        """
        self.data = []
        self.sig1D = []
        #First, format input data. Time component of 1D signals is removed.
        #We keep the first 1D signal found to rebuild the time component afterward.
        i=0
        for data in data_list:
            if type(data) == np.ndarray:
                if len(data.shape) == 2 and data.shape[0] == 2:
                        self.data.append(data[1,:])
                        self.sig1D.append(data)
                        print("lens "+str(i) +": " + str(len(data[0])))
                        i+=1
                else:
                        self.data.append(data)
                        tmp_res = self.apply(tmp,time)
		        
        #get the result
        tmp_res = self.apply(self.data,time)
        #1D result
        if len(tmp_res.shape) == 1:
            #find an input signal with the same number of sample
            sig = None
            for s in self.sig1D: 
                if len(s[0]) == len(tmp_res):
                    sig = s
                    break
            if sig is not None:
                if tmp_res.dtype == np.complex128:
                    res = np.array(sig, dtype = np.complex128)
                elif tmp_res.dtype == np.complex64:
                    res = np.array(sig, dtype = np.complex64)
                elif sig.dtype == np.complex64 or sig.dtype == np.complex128 :
                    res = sig.real;#np.real(data)
                else:
                    res = sig				
                res[1,:] = tmp_res
                return res
        return tmp_res

    def _reset(self,data_list,time):
        """
        Internal function. Reset the processing after reformatting the input signals.
        You should overload this function only if you don't want a pre reformatting of input
        signals and you know what you are doing.
        """
        return self.reset(data_list,time) 		
    
    def unit(self, index, unit_list):
        """Returns the unit for given dimension based on the input unit names"""
        return unit_list[0]

    def name(self,input_names):
        """Returns the output name based one the input ones"""
        return input_names[0]
        
    def dims(self):
        """Returns a tuple (minimum input dimension supported, maximum input dimension supported)"""
        return (1,3)

    def needResampling(self):
        return True      #Tells if input signals must be resamples to have the same shapes
    def needSameType(self):
        return True       #Tells if input signals must be of the same types (all 1D signals, all images, or all numeric values)
    def needSameSubType(self):
        return False      #Tells if input signals must have the same data type (all float, double, int, complex64,...)
    def inputCount(self):
        return (2,2)      #Tells the number of supported input count (min,max)
    def displayHint(self):
        return DisplayOnSameSupport     #display hint, DisplayOnSameSupport by default

