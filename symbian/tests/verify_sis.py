"""Check a legacy SIS contains the expected ARM application and resource.
Usage: python tests/verify_sis.py [path/to/Skyliner700.sis]
"""
from pathlib import Path
import hashlib
import struct
import sys
import zlib

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'Skyliner700.sis'
data = path.read_bytes()
streams = []
for offset in range(len(data) - 2):
    if data[offset] == 0x78:
        try:
            payload = zlib.decompress(data[offset:])
            if payload:
                streams.append(payload)
        except zlib.error:
            pass
assert len(streams) == 3, 'Expected application, resource and icon payloads'
apps = [p for p in streams if p[16:20] == b'EPOC']
assert len(apps) == 1, 'Missing or ambiguous E32 application'
assert struct.unpack_from('<III', apps[0]) == (0x10000079, 0x100039CE, 0x0F700660), 'Incorrect E32 UIDs'
for name in ('Skyliner700.app', 'Skyliner700.rsc','Skyliner700.aif'):
    assert name.lower().encode('utf-16le') in data.lower(), 'Missing destination: ' + name
print('PASS: legacy SIS, three compressed files, EPOC ARM application UIDs and destinations.')
print('Size:', len(data), 'bytes; SHA256:', hashlib.sha256(data).hexdigest())
