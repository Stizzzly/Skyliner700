from pathlib import Path
import re,subprocess,sys
sdk=Path(sys.argv[1])/'Epoc32';obj=Path(sys.argv[2])
nm=subprocess.check_output([str(sdk/'gcc/bin/nm.exe'),str(obj)],text=True)
assert not re.search(r'_GLOBAL_.*(?:sub_I|I_)|__(?:add|sub|mul|div|fix|float)\w*(?:sf|df)\w*',nm)
for symbol in ('NewL__11CObexClient','InitFromFileL__15CObexFileObject','Put__11CObexClient','NewL__9CSdpAgent'):
    assert symbol in nm,symbol
assert 'CSendAppUi' not in nm, 'Messaging attachment path remains in direct-OBEX build'
asm=subprocess.check_output([str(sdk/'gcc/bin/objdump.exe'),'-d',str(obj)],text=True)
words=re.findall(r'^\s*[0-9a-f]+:\s+([0-9a-f]+)\s+\S+',asm,re.M)
assert words and all(len(w)==8 for w in words)
print('PASS: ARM32, no FP/global constructors, direct OBEX/SDP/file streaming, no SendUI.')
