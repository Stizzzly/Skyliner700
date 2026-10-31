"""Reject floating-point helpers/libm and writable-global constructors in ARM objects.
Usage: python tests/audit_arm.py path/to/SDK path/to/ARMI/UREL
"""
from pathlib import Path
import re
import subprocess
import sys

sdk, objects = map(Path, sys.argv[1:3])
nm = sdk / 'epoc32/gcc/bin/nm.exe'
expected = ['FIXED.o', 'PHYSICS.o', 'SOFT3D.o', 'TERRAIN.o', 'SYMBIAN.o','MENU.o']
paths = [objects / name for name in expected]
assert all(p.is_file() for p in paths), 'Missing native object'
output = subprocess.check_output([str(nm), *map(str, paths)], text=True, errors='replace')
forbidden = re.compile(r'\b__(?:add|sub|mul|div|neg|cmp|eq|ne|lt|le|gt|ge|unord)(?:sf|df|xf|tf)\d|\b__(?:float|fix|extend|trunc)\w*(?:sf|df)\w*|\b(?:sin|cos|tan|atan2|sqrt|floor|ceil|pow|fmod)f?\b|_GLOBAL_.*(?:sub_I|I_)')
bad = [line for line in output.splitlines() if forbidden.search(line)]
assert not bad, 'Floating-point/helper/global-init symbols:\n' + '\n'.join(bad)
print('PASS: all six native ARM objects contain no floating-point/libm helpers or global constructors.')
print('Integer 64-bit division helpers are allowed; these are not floating point.')
