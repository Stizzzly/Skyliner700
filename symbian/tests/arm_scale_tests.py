"""Run SDK-built SkyScale2x; check pixels, ABI, alignment and zero framebuffer reads.
Usage: python arm_scale_tests.py SDK SOFT3D.o (requires Unicorn).
This verifies instructions/results, not ARM920T timing or cache behavior.
"""
from pathlib import Path
import random,re,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM,UC_HOOK_MEM_READ
from unicorn.arm_const import *
sdk,obj=map(Path,sys.argv[1:3])
symbols=subprocess.check_output([str(sdk/'epoc32/gcc/bin/nm.exe'),str(obj)],text=True)
entry=int(re.search(r'^([0-9a-f]+) T SkyScale2x__FPCUsiPUsi$',symbols,re.M).group(1),16)
data=obj.read_bytes();header=20+struct.unpack_from('<H',data,16)[0]
for i in range(struct.unpack_from('<H',data,2)[0]):
    off=header+i*40
    if data[off:off+8].rstrip(b'\0')==b'.text':
        length,pointer=struct.unpack_from('<II',data,off+16)
        code=data[pointer:pointer+length];break
else: raise AssertionError('Missing .text')
base=0x100000;size=(len(code)+4095)//4096*4096;stop=base+size-4
uc=Uc(UC_ARCH_ARM,UC_MODE_ARM);uc.mem_map(base,size);uc.mem_write(base,code)
uc.mem_map(0x200000,0x4000);uc.mem_map(0x300000,0x10000);uc.mem_map(0x400000,0x20000)
regs=[UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11]
rng=random.Random(6600);counts={}
def read(uc,access,address,size,value,user):
    assert not 0x400000<=address<0x420000, 'Framebuffer read'
    if 0x300000<=address<0x310000:
        assert size in (2,4) and address%size==0, 'Misaligned source read'
        counts[size]=counts.get(size,0)+1
uc.hook_add(UC_HOOK_MEM_READ,read)
for src_stride in (88,89):
 for src_offset in (0,2):
  for dst_stride in (176,177):
   for dst_offset in (0,2):
    source=[rng.randrange(65536) for _ in range(src_stride*104+4)]
    uc.mem_write(0x300000,struct.pack('<%dH'%len(source),*source))
    output=bytearray(b'\xa5'*0x20000);expected=output[:]
    for y in range(208):
     for x in range(176):
      pixel=source[src_offset//2+(y//2)*src_stride+x//2]
      struct.pack_into('<H',expected,dst_offset+(y*dst_stride+x)*2,pixel)
    uc.mem_write(0x400000,bytes(output));counts.clear()
    sp=0x203000
    for i,reg in enumerate(regs):uc.reg_write(reg,0x660000+i)
    for reg,value in [(UC_ARM_REG_R0,0x300000+src_offset),(UC_ARM_REG_R1,src_stride),(UC_ARM_REG_R2,0x400000+dst_offset),(UC_ARM_REG_R3,dst_stride),(UC_ARM_REG_SP,sp),(UC_ARM_REG_LR,stop)]:uc.reg_write(reg,value)
    uc.emu_start(base+entry,stop,count=2000000)
    assert uc.reg_read(UC_ARM_REG_PC)==stop, 'Did not return'
    assert uc.mem_read(0x400000,len(expected))==expected, 'Pixels or guards differ'
    assert uc.reg_read(UC_ARM_REG_SP)==sp, 'Stack not restored'
    for i,reg in enumerate(regs):assert uc.reg_read(reg)==0x660000+i, 'Callee-saved register changed'
    if src_stride==88 and src_offset==0 and dst_stride==176 and dst_offset==0:
        assert counts=={4:4576}, counts
print('PASS: 16 native ARM scaler cases, exact pixels/guards/ABI, aligned source reads; zero framebuffer reads.')
print('PASS: normal path loads 4576 source words for 9152 pixels; no source halfword reads.')
