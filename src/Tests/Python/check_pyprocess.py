#!/usr/bin/env python
"""The interpreter process must end when its input is closed.

The parent talks to it through a pipe. When the parent dies, the read returns
nothing, and that used to be treated as an absence of data rather than as the
end of the conversation: the process stayed alive for ever, waking a hundred
times a second, holding a whole interpreter with no window to close it from.
"""
import os
import subprocess
import sys
import time

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'Python', 'pyprocess.py')
LIMIT = 20

# The module imports numpy at the top. Without it the process would exit at once
# and this check would pass for the wrong reason.
probe = subprocess.run([sys.executable, '-c', 'import numpy'], capture_output=True)
if probe.returncode != 0:
    print('skipped: numpy is not available to %s' % sys.executable)
    sys.exit(0)

start = time.time()
proc = subprocess.Popen([sys.executable, SRC],
                        stdin=subprocess.DEVNULL,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE)
try:
    _, err = proc.communicate(timeout=LIMIT)
except subprocess.TimeoutExpired:
    proc.kill()
    proc.communicate()
    print('FAIL: the interpreter was still running %d s after its input was closed' % LIMIT)
    sys.exit(1)

if proc.returncode != 0:
    print('FAIL: the interpreter ended with %d' % proc.returncode)
    print(err.decode(errors='replace')[:2000])
    sys.exit(1)

print('the interpreter ended %.2f s after its input was closed' % (time.time() - start))
