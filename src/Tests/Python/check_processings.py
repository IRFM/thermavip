#!/usr/bin/env python
"""Three processings that could not run at all: the transform on an image, the
fusion on anything but a one dimensional signal, and the Wiener filter asked to
estimate its own noise."""
import importlib.util
import os
import sys
import types

import numpy as np

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Python')
failures = []


def check(label, condition, detail=''):
    if not condition:
        failures.append('%s%s' % (label, (': ' + detail) if detail else ''))


def load(relative, name):
    base = os.path.join(ROOT, 'ThermavipPyProcessing.py')
    spec = importlib.util.spec_from_file_location('thermavip_base', base)
    mod = importlib.util.module_from_spec(spec)
    sys.modules['thermavip_base'] = mod
    sys.modules['Thermavip'] = types.ModuleType('Thermavip')
    spec.loader.exec_module(mod)
    sys.modules['ThermavipPyProcessing'] = mod

    path = os.path.join(ROOT, relative)
    spec = importlib.util.spec_from_file_location(name, path)
    out = importlib.util.module_from_spec(spec)
    sys.modules[name] = out
    spec.loader.exec_module(out)
    return out, mod


fft, base = load(os.path.join('Discrete Fourier Transform', 'FFT.py'), 'vip_fft')

# 1. the transform says what it supports, and says so before failing obscurely
for name in ('ThermavipRFFT', 'ThermavipIRFFT'):
    cls = getattr(fft, name)
    proc = cls()
    check('%s declares one dimension' % name, proc.dims() == (1, 1), str(proc.dims()))
    try:
        proc.apply(np.zeros((4, 4)), 0)
        failures.append('%s must refuse an image' % name)
    except RuntimeError:
        pass
    except AttributeError as exc:
        failures.append('%s fails on a missing symbol: %r' % (name, exc))

    out = proc.apply(np.linspace(0., 1., 16), 0)
    check('%s still transforms a signal' % name, len(out) == 16, str(len(out)))

# 2. the fusion of two images must reach the processing rather than a NameError
corr, base = load(os.path.join('Data Fusion', 'Correlation.py'), 'vip_corr')
proc = corr.ThermavipConvolveSignals()
try:
    proc._apply(np.zeros((4, 4)), 0)
except NameError as exc:
    failures.append('the fusion still uses an undefined name: %r' % exc)
except Exception:
    pass  # any other outcome is the processing itself talking

# 3. the Wiener filter estimates its noise when asked to
filt, base = load(os.path.join('Filters', 'Filtering.py'), 'vip_filt')
wiener = filt.ThermavipWienerFilter()
wiener.noise = 0
flat = np.zeros((16, 16), dtype=float)
flat[4:8, 4:8] = 10.
out = np.asarray(wiener.apply(flat, 0))
check('the Wiener filter leaves no invalid value on a flat area', np.all(np.isfinite(out)),
      'produced %d invalid values' % int(np.sum(~np.isfinite(out))))

if failures:
    for f in failures:
        print('FAIL:', f)
    sys.exit(1)
print('all processing checks passed')
