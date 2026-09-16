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
    if n < 2:
        raise ValueError("estimt: at least two samples are needed")
    # The median of the steps, not their maximum: this is the step of an explicit
    # Euler scheme, which needs step < tau to be stable, and a single gap in the
    # acquisition made the maximum dwarf the real step and the whole trace diverge.
    deltas = t_p[1:] - t_p[0:n-1]
    cadence = float(np.median(deltas))
    if not (cadence > 0):
        raise ValueError("estimt: the time base does not increase")
    if not (tau_up > 0) or not (tau_down > 0):
        raise ValueError("estimt: the decay times must be positive")
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
        # elif and else: the two tests were independent and neither covered
        # equality, so the increment of the previous step was replayed and the
        # simulated trace climbed past the set point it cannot exceed.
        if p2 > tim1:
            incr = incr_up
        elif p2 < tim1:
            incr = incr_down
        else:
            incr = 0.0
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
    # A power that is identically zero, which is what no heating and a failed read
    # both give, used to turn the whole signal into NaN, and the fit below then
    # returned an arbitrary point as a decay time.
    if not (max_p > 0):
        raise ValueError("estimate_tau: the power signal is null")
    vp = vp/max_p
    
    # ...then temperatures
    
    #vt_filter = medfilt(vt,151)
    vt_filter = vt
    vmin = np.min(vt_filter)
    vmax = np.max(vt_filter)
    
    
    if not (vmax > vmin):
        raise ValueError("estimate_tau: the temperature signal is constant")
    vt = (vt - vmin)/(vmax - vmin)
    
    power = vp
    temperature = vt
    
    # Minimize error function
    # A decay time is positive: the simplex walks freely and used to reach zero or
    # a negative value, where the model has no meaning. Fit on the logarithm, so
    # the constraint holds by construction, and say when it did not converge.
    def estimt_error(x):
        tau_up = float(np.exp(x[0]))
        tau_down = float(np.exp(x[1]))
        try:
            simulated = estimt(tau_up, tau_down, times, power)
        except ValueError:
            return np.inf
        error = np.linalg.norm(temperature - simulated)
        return error if np.isfinite(error) else np.inf

    xopt, fopt, iterations, calls, warnflag = fmin(func=estimt_error, x0=[0.0, 0.0], full_output=True, disp=False)
    if warnflag != 0:
        raise RuntimeError("estimate_tau: the fit did not converge")
    if not np.isfinite(fopt):
        raise RuntimeError("estimate_tau: the fit could not be evaluated")

    return np.exp(xopt) #(tau_up, tau_down)



def estimate_tau_for_pulse(pulse: int, times , temperatures):
    """
    Estimate the up and down decay times using given pulse and temperature time trace.
    """
    import librir_west as w
    
    # Load all power signals
    # One loop instead of three copies, a narrowed catch, and a trace for each
    # failure: a bare except also swallows an interruption, and losing all three
    # signals used to leave the temperature compared with itself, which returns a
    # decay time that measures nothing.
    import logging

    powers=[]
    for signal_name, factor, label in (("SHYBPTOT", 1.0, "hybrid"),
                                       ("SICHPTOT", 1e-3, "ICRH"),          # switch to MW
                                       ("SMAG_IP", 1e-3, "IP, vloop of 1")): # switch to MW
        try:
            signal = w.ts_read_signal(pulse, signal_name)
        except Exception as error:
            logging.warning("estimate_tau_for_pulse: %s (%s) unreadable for pulse %s: %s",
                            signal_name, label, pulse, error)
            continue
        powers.append(signal[0])
        powers.append(signal[1] * factor)

    if not powers:
        raise RuntimeError("estimate_tau_for_pulse: no power signal could be read for pulse %s" % pulse)
    
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
    # tmp[-1], not powers[-1]: resample_all put every signal on one time base, and
    # the temperature is the reference the fit is measured against. Passing the raw
    # array threw that away for the one signal that matters — either the shapes
    # disagree and the subtraction raises, or they happen to match and two signals
    # offset in time are compared, which yields a wrong tau with no message.
    taus = estimate_tau(pow_t * 1e-9,tmp[-1],pow_v)
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
