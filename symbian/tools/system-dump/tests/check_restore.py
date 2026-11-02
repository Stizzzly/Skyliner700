from pathlib import Path
import importlib.util,tempfile,zlib
module_path=Path(__file__).resolve().parents[1]/'restore_systemdump.py'
spec=importlib.util.spec_from_file_location('restore',module_path);tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
def code(path):return path.encode('utf-16le').hex()
with tempfile.TemporaryDirectory() as temp:
    root=Path(temp);received=root/'incoming';received.mkdir();manifest=received/'N6600_20261102_120000_manifest.tsv'
    rows=['# SystemDump 0.2.0','D\t-\t0\t00000000\t'+code('Z:\\system\\empty\\')]
    for i,(path,data) in enumerate([('Z:\\system\\libs\\euser.dll',bytes(range(256))*1000),('Z:\\ресурсы\\тест.bin',b''),('Z:\\system\\x.dll',b'other')],1):
        name=f'N6600_20261102_120000_{i:06}.bin';(received/name).write_bytes(data)
        rows.append(f'F\t{name}\t{len(data)}\t{zlib.crc32(data):08x}\t{code(path)}')
    manifest.write_text('\n'.join(rows+['# END files 3 skipped 0']),encoding='ascii')
    report=tool.restore(received,manifest,root/'good');assert report['restored']==3 and not report['errors']
    assert (root/'good/Z/ресурсы/тест.bin').read_bytes()==b''
    assert (root/'good/Z/system/empty').is_dir()
    assert (root/'good/Z/system/libs/euser.dll').read_bytes()==bytes(range(256))*1000
    (received/'N6600_20261102_120000_000001.bin').write_bytes(b'X'*(256*1000))
    report=tool.restore(received,manifest,root/'badcrc');assert report['restored']==2 and len(report['errors'])==1
    assert not (root/'badcrc/Z/system/libs/euser.dll').exists()
    for invalid in ['C:\\secret','Z:\\..\\escape','\\\\server\\share\\file']:
        try:tool.destination(root/'safe',code(invalid))
        except ValueError:pass
        else:raise AssertionError(invalid)
print('PASS: direct manifest tree/Unicode/empty-file restoration, CRC failure, missing partial output and traversal protection.')
