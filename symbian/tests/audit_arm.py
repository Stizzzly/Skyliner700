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

soft_symbols = subprocess.check_output([str(nm), '-u', str(objects / 'SOFT3D.o')], text=True, errors='replace')
assert not re.search(r'\b__u?divsi3\b', soft_symbols), '32-bit division helper remains in renderer'
print('PASS: renderer has no 32-bit division helper; edge bounds use cached reciprocals.')

raster = re.search(r'<RasterScreen__6Soft3DRC6ScreenN21Us>:\n(.*?)(?=^[0-9a-f]+ <[^.][^>]*>:\n|\Z)', assembly, re.M | re.S)
assert raster, 'Missing RasterScreen function; inspect compiler inlining before updating audit'
assert not re.search(r'ARM_26\s+__(?:u?div|u?mod)', raster.group(1)), 'Division/modulo helper in triangle setup or raster loop'
print('PASS: entire native RasterScreen (Z setup and rows) has no division/modulo helper calls.')

for name in ('PHYSICS.o', 'FIXED.o', 'TERRAIN.o'):
    symbols = subprocess.check_output([str(nm), '-u', str(objects / name)], text=True, errors='replace')
    assert not re.search(r'\b__(?:u?div|u?mod)\w*', symbols), 'Division/modulo helper remains in ' + name
print('PASS: PHYSICS.o, FIXED.o and TERRAIN.o import no integer division/modulo helpers.')

render = re.search(r'<Render__6Soft3DPUsiRC11FlightState>:\n(.*?)(?=^[0-9a-f]+ <[^.][^>]*>:\n|\Z)', assembly, re.M | re.S)
assert render, 'Missing native Render function'
assert not re.search(r'ARM_26\s+__(?:u?div|u?mod)', render.group(1)), 'Division/modulo helper remains in grid setup or Render'
print('PASS: native Render/grid setup has no division/modulo helper calls.')

kernel = re.search(r'<SkyDrawPairsArm>:\n(.*?)(?=^[0-9a-f]+ <DrawSpan)', assembly, re.M | re.S)
assert kernel, 'Missing ARM pair kernel'
ops=re.findall(r'^\s*[0-9a-f]+:\s+[0-9a-f]{8}\s+(\w+)\s+(.*)',kernel.group(1),re.M)
start=next(i for i,(op,args) in enumerate(ops) if op=='ldrh')
end=next(i for i,(op,args) in enumerate(ops) if op=='ldmia')
loop=ops[start:end]
assert [op for op,args in loop[:2]]==['ldrh','ldrh'], 'Pair Z loads are not first'
assert next(i for i,(op,args) in enumerate(loop) if op.startswith('cmp'))>=8, 'Z load-use gap lost'
assert not any(re.search(r'\b(?:sp|r13)\b',args) or op in ('bl','blx') for op,args in loop), 'Pair-loop stack traffic or call'
assert sum(op=='str' for op,args in loop)==2, 'Missing packed framebuffer/depth stores'
assert sum(op=='strgth' for op,args in loop)==4, 'Missing mixed-result halfword stores'
print('PASS: pair kernel loads Z early, uses packed/mixed stores, and has no loop stack traffic/calls.')

ui = subprocess.check_output([str(nm), str(objects / 'SYMBIAN.o')], text=True, errors='replace')
for symbol in ('EventReady__10RWsSessionP14TRequestStatus', 'GetEvent__10RWsSessionR8TWsEvent',
               'EventReadyCancel__10RWsSession', 'WaitForAnyRequest__4User',
               'RunIfReady__16CActiveSchedulerRii', 'OwnRequestsL__9CFlightUi'):
    assert symbol in ui, 'Missing owned request pump symbol: ' + symbol
assert 'CPeriodic' not in ui, 'CPeriodic still drives the native application'
assert 'OfferKeyEventL__11CFlightView' not in ui, 'Control stack still owns game input'
ui_asm = subprocess.check_output([str(objdump), '-dr', str(objects / 'SYMBIAN.o')], text=True, errors='replace')
owned = re.search(r'<OwnRequestsL__9CFlightUi>:\n(.*?)(?=^[0-9a-f]+ <[^.][^>]*>:\n|\Z)', ui_asm, re.M | re.S)
assert owned, 'Missing owned loop body'
assert owned.group(1).index('ARM_26\tCancel__7CActive') < owned.group(1).index('ARM_26\tEventReady__10RWsSession'), 'CONE must be cancelled before owning WS requests'
print('PASS: native owned WS pump, CONE cancel before EventReady, direct GetEvent/WaitForAnyRequest/RunIfReady, no CPeriodic.')
