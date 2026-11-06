from pathlib import Path
import sys, zipfile, zlib
p=Path(sys.argv[1])
with zipfile.ZipFile(p) as z:
    assert z.testzip() is None
    assert len(z.infolist())==6
    assert 'Z/failed.dll' not in z.namelist()
    assert z.getinfo('Z/system/empty/').is_dir()
    assert z.read('Z/system/zero.bin')==b''
    assert z.read('Z/system/тест.bin')==b'123456789'
    assert z.getinfo('Z/system/тест.bin').CRC==0xcbf43926
    expected=bytes((i*31+(i>>8))&255 for i in range(300001))
    assert z.read('Z/system/pattern.dll')==expected
    assert z.read('Z/after.bin')==b'end'
    assert z.read('a'*800)==b'long'
    for entry in z.infolist():
        assert entry.compress_type==zipfile.ZIP_STORED
        assert entry.flag_bits & 0x808 == 0x808
print('PASS: independent Python ZIP parsing, CRC32 and every fixture byte; no source file loss after aborted entry.')
