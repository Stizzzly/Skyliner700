"""Execute actual SDK-built RawAdd/RawSub/RawMul instruction sequences with Unicorn.
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
    uc=Uc(UC_ARCH_ARM,UC_MODE_ARM);uc.mem_map(base,4096);uc.mem_write(base,code)
    for a,b in pairs:
        uc.reg_write(regs[rn],a&0xffffffff);uc.reg_write(regs[rm],b&0xffffffff)
        uc.reg_write(UC_ARM_REG_CPSR,0x10)
        uc.emu_start(base,base+12,count=3)
        expected=max(-2147483648,min(2147483647,a+b if op=='adds' else a-b))
        actual=uc.reg_read(regs[rd]);actual=actual if actual<0x80000000 else actual-0x100000000
        assert actual==expected,(op,a,b,actual,expected)
print('PASS: native ADDS/SUBS + MOVVS/EORVS, 20162 signed boundary/random results match wide saturation.')

# Find the exact inlined six-instruction multiply in the SDK object.
multiply=None
for i in range(len(ops)-5):
    seq=ops[i:i+6]
    if [o[2] for o in seq]!=['smull','mov','orr','teq','mvnne','eorne']:continue
    if [int(o[0],16) for o in seq]!=[int(seq[0][0],16)+j*4 for j in range(6)]:continue
    words=[int(o[1],16) for o in seq];w=words[0]
    lo=(w>>12)&15;hi=(w>>16)&15;rm=w&15;rs=(w>>8)&15
    assert len({lo,hi,rm,rs})==4, 'Early-clobber constraints lost'
    assert 'asr #16' in seq[3][3], 'Incorrect saturation sign-extension check'
    multiply=(struct.pack('<6I',*words),rm,rs,lo);break
assert multiply,'Missing native branch-free RawMul'
code,rm,rs,lo=multiply
uc=Uc(UC_ARCH_ARM,UC_MODE_ARM);uc.mem_map(base,4096);uc.mem_write(base,code)
mul_limits=limits+[-65537,-65536,-65535,32767,32768,65535,65536,65537]
mul_pairs=[(a,b) for a in mul_limits for b in mul_limits]
mul_pairs += [(rng.randrange(-2147483648,2147483648),rng.randrange(-2147483648,2147483648)) for _ in range(10000)]
# Straddle both exact saturation boundaries in the 64-bit product.
for _ in range(1000):
    a=rng.randrange(65537,2147483648)
    for boundary in ((2147483648<<16),(-2147483648<<16)):
        b=boundary//a
        for delta in (-2,-1,0,1,2):
            if -2147483648<=b+delta<2147483648:mul_pairs.append((a,b+delta))
# Non-saturated products and negative arithmetic-shift rounding.
mul_pairs += [(rng.randrange(-10000000,10000001),rng.randrange(-131072,131073)) for _ in range(10000)]
for a,b in mul_pairs:
    uc.reg_write(regs[rm],a&0xffffffff);uc.reg_write(regs[rs],b&0xffffffff)
    uc.reg_write(UC_ARM_REG_CPSR,0x10)
    uc.emu_start(base,base+24,count=6)
    expected=max(-2147483648,min(2147483647,(a*b)>>16))
    actual=uc.reg_read(regs[lo]);actual=actual if actual<0x80000000 else actual-0x100000000
    assert actual==expected,('mul',a,b,actual,expected)
print('PASS: native six-instruction RawMul, %d full-range/boundary/non-saturated products match wide reference.'%len(mul_pairs))
