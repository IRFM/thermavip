#!/usr/bin/env python
"""Exercise the first order model of estim_tau on the degenerate inputs a real
acquisition produces: a gap in the time base, a plateau, a null power and a
constant temperature."""
import importlib.util
import os
import sys

import numpy as np

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Python', 'estim_tau.py')
spec = importlib.util.spec_from_file_location('estim_tau', SRC)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

failures = []


def check(label, condition, detail=''):
    if not condition:
        failures.append('%s%s' % (label, (': ' + detail) if detail else ''))


# 1. a plateau must not push the simulated trace past the set point
times = np.array([0., 1., 2., 3., 4.])
power = np.array([0., 1., 1., 1., 0.5])
trace = mod.estimt(1., 1., times, power)
check('the simulated trace stays under the set point', float(np.max(trace)) <= float(np.max(power)) + 1e-12,
      'peak %.6g for a power of at most %.6g' % (float(np.max(trace)), float(np.max(power))))
check('a plateau holds the value', abs(trace[2] - 1.) < 1e-12, 'got %.6g' % trace[2])

# 2. one gap in the time base must not make the scheme diverge
gapped = np.array([0., 0.01, 0.02, 5.0, 5.01, 5.02, 5.03])
p = np.array([0., 1., 1., 1., 1., 1., 1.])
trace = mod.estimt(0.1, 0.1, gapped, p)
check('a gap in the time base does not make the trace diverge', np.all(np.isfinite(trace)) and float(np.max(np.abs(trace))) < 10.,
      'peak %.6g' % float(np.max(np.abs(trace))))

# 3. degenerate signals are refused rather than turned into NaN
flat_time = np.linspace(0., 1., 50)
constant_temperature = np.full(50, 3.0)
varying_power = np.linspace(0., 1., 50)
try:
    mod.estimate_tau(flat_time, constant_temperature, varying_power)
    failures.append('a constant temperature must be refused')
except ValueError:
    pass

try:
    mod.estimate_tau(flat_time, np.linspace(0., 1., 50), np.zeros(50))
    failures.append('a null power must be refused')
except ValueError:
    pass

# 4. a decay time is positive, and an ordinary fit still works
temperature = mod.estimt(0.2, 0.4, flat_time, varying_power)
taus = mod.estimate_tau(flat_time, temperature, varying_power)
check('the fit gives two positive decay times', len(taus) == 2 and taus[0] > 0 and taus[1] > 0, str(taus))

# 5. too few samples is an error, not a silent result
try:
    mod.estimt(1., 1., np.array([0.]), np.array([1.]))
    failures.append('a single sample must be refused')
except ValueError:
    pass

if failures:
    for f in failures:
        print('FAIL:', f)
    sys.exit(1)
print('all estim_tau checks passed')
