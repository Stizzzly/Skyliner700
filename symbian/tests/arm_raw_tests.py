"""Execute actual SDK-built RawAdd/RawSub instruction triples with Unicorn.
Usage: python arm_raw_tests.py SDK SOFT3D.o
Checks arithmetic results; does not measure ARM920T timing.
"""
from pathlib import Path
import random,re,struct,subprocess,sys
from unicorn import Uc,UC_ARCH_ARM,UC_MODE_ARM
from unicorn.arm_const import *
sdk,obj=map(Path,sys.argv[1:3])
assembly=subprocess.check_output([str(sdk/'epoc32/gcc/bin/objdump.exe'),'-d',str(obj)],text=True)
ops=re.findall(r'^\s*([0-9a-f]+):\s+([0-9a-f]{8})\s+(\w+)\s+(.*)',assembly,re.M)
fragments={}
for i in range(len(ops)-2):
    triple=ops[i:i+3];op=triple[0][2]
    if op not in ('adds','subs') or [o[2] for o in triple[1:]]!=['movvs','eorvs']:continue
    if [int(o[0],16) for o in triple]!=[int(triple[0][0],16)+j*4 for j in range(3)]:continue
    words=[int(o[1],16) for o in triple];word=words[0]
    rn=(word>>16)&15;rm=word&15;rd=(word>>12)&15
    if rn==rm or max(rn,rm,rd)>=15:continue
    assert not word&0xff0, 'Expected unshifted register operand'
    fragments.setdefault(op,(struct.pack('<3I',*words),rn,rm,rd))
assert set(fragments)=={'adds','subs'}, 'Missing native 32-bit saturating triples'
uc=Uc(UC_ARCH_ARM,UC_MODE_ARM);base=0x100000;uc.mem_map(base,4096)
regs=[UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11,UC_ARM_REG_R12,UC_ARM_REG_SP,UC_ARM_REG_LR]
limits=[-2147483648,-2147483647,-131072,-1,0,1,131072,2147483646,2147483647]
rng=random.Random(920)
pairs=[(a,b) for a in limits for b in limits]+[(rng.randrange(-2147483648,2147483648),rng.randrange(-2147483648,2147483648)) for _ in range(10000)]
for op,(code,rn,rm,rd) in fragments.items():
    uc.mem_write(base,code)
    for a,b in pairs:
        uc.reg_write(regs[rn],a&0xffffffff);uc.reg_write(regs[rm],b&0xffffffff)
        uc.reg_write(UC_ARM_REG_CPSR,0x10)
        uc.emu_start(base,base+12,count=3)
        expected=max(-2147483648,min(2147483647,a+b if op=='adds' else a-b))
        actual=uc.reg_read(regs[rd]);actual=actual if actual<0x80000000 else actual-0x100000000
        assert actual==expected,(op,a,b,actual,expected)
print('PASS: native ADDS/SUBS + MOVVS/EORVS, 20162 signed boundary/random results match wide saturation.')
