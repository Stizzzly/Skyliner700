"""Verify the separate system dump's legacy SIS and optional SDK payload identity.
Usage: python tests/verify_package.py SystemDump.sis [SDK]
"""
from pathlib import Path
import hashlib,struct,sys,zlib
sis=Path(sys.argv[1]);data=sis.read_bytes();payloads=[]
for offset in range(len(data)-2):
    if data[offset]!=0x78:continue
    try:
        raw=zlib.decompress(data[offset:])
        if raw:payloads.append(raw)
    except zlib.error:pass
assert len(payloads)==2, 'Expected APP and RSC payloads'
app=next(p for p in payloads if p[16:20]==b'EPOC')
assert struct.unpack_from('<III',app)==(0x10000079,0x100039ce,0x0f700663),'Probe UIDs'
for name in ('SystemDump.app','SystemDump.rsc'):
    assert name.lower().encode('utf-16le') in data.lower(),'Wrong destination'
if len(sys.argv)>2:
    sdk=Path(sys.argv[2])/'epoc32'
    for path in [sdk/'release/armi/urel/SystemDump.app',sdk/'data/z/system/apps/SystemDump/SystemDump.rsc']:
        assert path.read_bytes() in payloads,'SDK payload mismatch: '+str(path)
print('PASS: separate system dump UIDs, two-file legacy SIS and destinations; SDK identity checked if supplied.')
print('SHA256:',hashlib.sha256(data).hexdigest())
