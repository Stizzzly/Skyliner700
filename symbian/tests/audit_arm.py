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

objdump = sdk / 'epoc32/gcc/bin/objdump.exe'
for path in paths:
    assembly = subprocess.check_output([str(objdump), '-d', str(path)], text=True, errors='replace')
    instructions = re.findall(r'^\s*[0-9a-f]+:\s+([0-9a-f]+)\s+\S+', assembly, re.M | re.I)
    assert instructions, 'No decoded instructions: ' + path.name
    assert all(len(word) == 8 for word in instructions), 'Non-32-bit instruction: ' + path.name
    symbols = subprocess.check_output([str(nm), '-u', str(path)], text=True, errors='replace')
    division = [line.strip() for line in symbols.splitlines() if re.search(r'__(?:u?div|u?mod|aeabi_.*div)', line)]
    print(path.name + ': ARM32; integer division dependencies: ' + (', '.join(division) or 'none'))
print('PASS: all six objects disassemble as 32-bit ARM instructions; no Thumb instructions found.')

assembly = subprocess.check_output([str(objdump), '-dr', str(objects / 'SOFT3D.o')], text=True, errors='replace')
# Keep local .L labels inside the body; stop at the next named C++ function.
project = re.search(r'<Project__FRC3Vec>:\n(.*?)(?=^[0-9a-f]+ <[^.][^>]*>:\n|\Z)', assembly, re.M | re.S)
assert project, 'Missing native Project function; inspect compiler inlining before updating audit'
assert not re.search(r'ARM_26\s+__(?:u?div|u?mod|lshrdi)', project.group(1)), 'Projection calls division/modulo/variable-wide-shift helper'
print('PASS: native Project has no division, modulo or variable 64-bit shift helper calls.')
