"""Execute the actual SDK-built ARM pair kernel with Unicorn (pip install unicorn).
Usage: python tests/arm_span_tests.py SDK path/to/SOFT3D.o
This checks results/ABI/guards, not ARM920T cache or cycle timing.
"""
from pathlib import Path
import random, re, struct, subprocess, sys
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import *
sdk, obj = map(Path, sys.argv[1:3])
tools = sdk/'epoc32/gcc/bin'
symbols = subprocess.check_output([str(tools/'nm.exe'), str(obj)], text=True)
match = re.search(r'^([0-9a-fA-F]+) T SkyDrawPairsArm$', symbols, re.M)
assert match, 'Missing ARM pair kernel'
# Read the COFF .text section directly (this SDK objcopy predates -j).
data=obj.read_bytes()
sections=struct.unpack_from('<H',data,2)[0]
header=20+struct.unpack_from('<H',data,16)[0]
code=None
for section in range(sections):
    offset=header+section*40
    if data[offset:offset+8].rstrip(b'\0')==b'.text':
        length,pointer=struct.unpack_from('<II',data,offset+16)
        code=data[pointer:pointer+length]
assert code, 'Missing COFF .text section'
wrapper = re.search(r'^([0-9a-fA-F]+) [tT] DrawSpan__FPUsT0iiiUs$', symbols, re.M)
assert wrapper, 'Missing native span wrapper'
# Resolve the object's C-call relocation for native DrawSpan -> SkyDrawPairsArm.
assembly=subprocess.check_output([str(tools/'objdump.exe'), '-dr', str(obj)], text=True)
code=bytearray(code)
for relocation in re.finditer(r'([0-9a-f]+): ARM_26\s+SkyDrawPairsArm',assembly):
    offset=int(relocation.group(1),16)
    instruction=struct.unpack_from('<I',code,offset)[0]
    branch=(int(match.group(1),16)-offset-8)//4
    struct.pack_into('<I',code,offset,(instruction&0xff000000)|(branch&0xffffff))
code=bytes(code)

base=0x100000
size=((len(code)+4095)//4096+1)*4096
stop=base+size-4
uc=Uc(UC_ARCH_ARM, UC_MODE_ARM)
uc.mem_map(base,size);uc.mem_write(base,code)
uc.mem_map(0x200000,0x10000);uc.mem_map(0x300000,0x4000)
registers=[UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]
rng=random.Random(6600)
for sample in range(10000):
    full_span=sample>=5000
    case=sample%5000
    count=case%209 if full_span else 2+case%207
    output_offset=8+(case&1) if full_span else 8
    depth_offset=8+((case>>1)&1) if full_span else 8
    z=rng.randrange(-20000000,20000000);dz=rng.randrange(-10000,10001);bias=rng.randrange(-1000,1001)
    color=rng.randrange(65536)
    depth=[rng.randrange(65536) for _ in range(256)]
    pixels=[rng.randrange(65536) for _ in range(256)]
    if case<4:
        z=100<<8;dz=0;bias=0
        for i in range(count):depth[depth_offset+i]=0 if (sample>>(i&1))&1 else 100
    if 4<=case<40:
        # Both sides of signed/16-bit depth boundaries, nonzero Q8 fractions.
        z=((case%3)*65535-1)*256+(case%256);bias=(0,2,4)[case%3]
        dz=(-257,-1,0,1,257)[case%5]
    expected_pixels=pixels[:];expected_depth=depth[:]
    current=z
    for i in range(count):
        d=(current>>8)+bias
        if d>expected_depth[depth_offset+i]:expected_depth[depth_offset+i]=d&65535;expected_pixels[output_offset+i]=color
        current+=dz
    pack=lambda values:struct.pack('<256H',*values)
    uc.mem_write(0x300000,pack(pixels));uc.mem_write(0x302000,pack(depth))
    sp=0x208000
    uc.mem_write(sp,struct.pack('<iI',dz,color if full_span else color|(color<<16)))
    for reg,value in [(UC_ARM_REG_R0,0x300000+output_offset*2),(UC_ARM_REG_R1,0x302000+depth_offset*2),(UC_ARM_REG_R2,count),(UC_ARM_REG_R3,(z+bias*256)&0xffffffff),(UC_ARM_REG_SP,sp),(UC_ARM_REG_LR,stop)]:uc.reg_write(reg,value)
    saved=[0x66000000+i for i in range(8)]
    for reg,value in zip(registers,saved):uc.reg_write(reg,value)
    uc.emu_start(base+int((wrapper if full_span else match).group(1),16),stop,count=20000)
    assert uc.reg_read(UC_ARM_REG_PC)==stop, 'Kernel did not return'
    assert uc.reg_read(UC_ARM_REG_SP)==sp, 'Stack pointer changed'
    assert [uc.reg_read(r) for r in registers]==saved, 'Callee-saved registers changed'
    assert uc.mem_read(0x300000,512)==pack(expected_pixels), 'Framebuffer/guard mismatch: '+str(sample)
    assert uc.mem_read(0x302000,512)==pack(expected_depth), 'Depth/guard mismatch: '+str(sample)
print('PASS: SDK ARM machine code, 5000 aligned kernel (even/odd counts) + 5000 full-span cases, Z masks/alignment/tails/guards and callee-saved ABI.')
