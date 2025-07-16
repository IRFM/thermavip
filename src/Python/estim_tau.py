# -*- coding: utf-8 -*-
"""
Created on Thu Jul 10 14:20:31 2025

@author: VM213788
"""

import numpy as np
from scipy.signal import medfilt
from scipy.interpolate import interp1d
from scipy.optimize import fmin


def estimt(tau_up,tau_down,t_p,p):
    """
    Estimate the temperature time trace from 
    the power one and the up/down decay times
    """
    n = len(t_p)
    cadence = np.max(t_p[1:] - t_p[0:n-1])
    gain_up = cadence / tau_up 
    gain_down=cadence / tau_down 
    t= np.zeros((n,), dtype = np.float64)
    
    incr = 0
    for i in range(1,n):
        p1 = p[i-1]
        p2 = p[i]
        tim1 = t[i-1]
        dp = p2 - tim1
        incr_up = gain_up * dp
        incr_down = gain_down * dp
        if p2 > tim1:
            incr = incr_up
        if p2 < tim1:
            incr = incr_down 
        t[i] = tim1 + incr
    
    return t



def resample_all(signals : tuple):
    """
    Resample all signals on the time intersection.
    Takes as input a tuple of (times1, values1, times2, values2,...)
    and returns the same tuple with resampled signals.
    """
    
    # First, compute the minimum sample count,
    # and the time interval intersection.
    min_sample = len(signals[0])
    min_time = signals[0][0]
    max_time = signals[0][-1]
    
    for i in range(2,len(signals),2):
        min_sample = min(min_sample,len(signals[i]))
        min_time = max(min_time,signals[i][0])
        max_time = min(max_time,signals[i][-1])
        
    # Create the new time vector
    new_t = np.linspace(min_time,max_time,min_sample)
    
   
    
    # Interpolate the signals with the new time basis.
    res = []
    for i in range(len(signals)):
        if i % 2 == 0:
            res.append(new_t)
        else:
            # Make the interpolator function.
            func = interp1d(signals[i-1], signals[i], kind="previous")
            res.append(func(new_t))
    
    return tuple(res)


def estimate_tau(times, val_temps, pow_vals):
    """
    Estimate the up and down decay times using given total power signal (any unit)
    and the temperature signal.
    
    Input arrays must have the same shape. 
    The vector must be in seconds.
    
    Returns (decay_time_up, decay_time_down).
    """
    times = np.array(times, dtype=np.float64) 
    
    vp = np.array(pow_vals, dtype=np.float64)
    vt = np.array(val_temps, dtype=np.float64)
    
    # Filter inputs
    vp = medfilt(vp,19)
    vt = medfilt(vt,19)
    
    
    #
    # Normalize both signals

    #max_p = np.max(medfilt(vp,151))
    max_p = np.max(vp)
    vp = vp/max_p
    
    # ...then temperatures
    
    #vt_filter = medfilt(vt,151)
    vt_filter = vt
    vmin = np.min(vt_filter)
    vmax = np.max(vt_filter)
    
    
    vt = (vt - vmin)/(vmax - vmin)
    
    power = vp
    temperature = vt
    
    # Minimize error function
    estimt_error = lambda x: np.linalg.norm(temperature - estimt(x[0], x[1],times, power))
    xopt = fmin(func=estimt_error, x0=[1,1])
    
    return xopt #(tau_up, tau_down)



def estimate_tau_for_pulse(pulse: int, times , temperatures):
    """
    Estimate the up and down decay times using given pulse and temperature time trace.
    """
    import librir_west as w
    
    # Load all power signals
    powers=[]
    try:
        #Hybrid power
        hyb_power = w.ts_read_signal(pulse,"SHYBPTOT")
        powers.append(hyb_power[0])
        powers.append(hyb_power[1])
    except:
        pass
    
    try:
        # ICRH power
        ich_power = w.ts_read_signal(pulse,"SICHPTOT")
        powers.append(ich_power[0])
        powers.append(ich_power[1]*1e-3) # switch to MW
    except:
        pass
    
    try:
        # IP power with a vloop of 1
        smag_power = w.ts_read_signal(pulse,"SMAG_IP")
        powers.append(smag_power[0])
        powers.append(smag_power[1]*1e-3) # switch to MW considering a vloop of 1
    except:
        pass
    
    # Add time trace for resampling
    powers.append(times)
    powers.append(temperatures)
    
    # Resample all signals based on time intersection
    tmp = resample_all(powers)
    
    # Compute total power
    pow_t = tmp[0]
    pow_v = tmp[1]
    for i in range(3,len(tmp)-2,2): # add all values axcept the last signals (temperature)
        pow_v += tmp[i]
        
    import time
    t = time.time()
    taus = estimate_tau(pow_t * 1e-9,powers[-1],pow_v)
    t = time.time() - t
    print("elapsed:",t)
    print(taus)
    return taus
    
    


# import time_trace as trace

# pulse = 60702
# tr = trace.extract_time_traces(pulse, "DIVQ1B", (381,103))
# t_v = tr[pulse]['values']
# t_t =  tr[pulse]['times']

# estimate_tau_for_pulse(pulse,t_t,t_v)
