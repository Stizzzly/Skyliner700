"""Restore a direct-OBEX SystemDump 0.2.0 transfer; Python 3 standard library."""
from pathlib import Path, PureWindowsPath
import argparse, json, re, shutil, zlib

def destination(root, encoded):
    text=bytes.fromhex(encoded).decode('utf-16le')
    path=PureWindowsPath(text)
    if path.drive.upper()!='Z:' or path.root!='\\' or '\x00' in text:
        raise ValueError('Manifest path is not on Z:')
    parts=path.parts[1:]
    if any(p in ('.','..') or ':' in p for p in parts):
        raise ValueError('Unsafe manifest path')
    result=root.joinpath('Z',*parts)
    if not result.resolve().is_relative_to(root.resolve()):
        raise ValueError('Path leaves output folder')
    return result

def restore(received, manifest, output):
    received=received.resolve();manifest=manifest.resolve()
    output.mkdir(parents=True,exist_ok=False)
    errors=[];skipped=[];restored=0;seen=set();complete=False
    for number,line in enumerate(manifest.read_text(encoding='ascii').splitlines(),1):
        if line.startswith('# END '):complete=True
        if not line or line.startswith('#'):continue
        part=None;created_part=False
        try:
            fields=line.split('\t')
            if len(fields)!=5:raise ValueError('Wrong manifest field count')
            kind,name,size,crc,encoded=fields
            target=destination(output,encoded)
            if kind=='D':target.mkdir(parents=True,exist_ok=True);continue
            if kind=='E':skipped.append({'path':str(target.relative_to(output)),'code':int(size)});continue
            if kind!='F':raise ValueError('Unknown record type')
            if not re.fullmatch(r'N6600_\d{8}_\d{6}_\d{6}\.bin',name):raise ValueError('Unsafe incoming filename')
            if name in seen:raise ValueError('Duplicate incoming filename')
            seen.add(name);source=received/name
            if source.is_symlink():raise ValueError('Incoming symlink refused')
            expected=int(size);checksum=int(crc,16)
            if expected<0 or source.stat().st_size!=expected:raise ValueError('Missing/incomplete file or wrong size')
            target.parent.mkdir(parents=True,exist_ok=True)
            if target.exists():raise ValueError('Duplicate output path')
            part=target.with_name(target.name+'.systemdump-part')
            value=0;length=0
            with source.open('rb') as src,part.open('xb') as dst:
                created_part=True
                while True:
                    block=src.read(65536)
                    if not block:break
                    value=zlib.crc32(block,value);length+=len(block);dst.write(block)
            if value!=checksum or length!=expected:raise ValueError('CRC32 mismatch')
            part.rename(target);part=None;restored+=1
        except (OSError,ValueError,UnicodeError) as error:
            if part is not None and created_part:part.unlink(missing_ok=True)
            errors.append({'line':number,'error':str(error)})
    if not complete:errors.append({'error':'Manifest is unfinished (missing END record)'})
    report={'restored':restored,'source_skipped':skipped,'errors':errors,'manifest_complete':complete}
    (output/'restore-report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    shutil.copy2(manifest,output/manifest.name)
    return report

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('received',type=Path,help='Folder where Windows saved Bluetooth files')
    parser.add_argument('--manifest',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    candidates=sorted(args.received.glob('N6600_*_manifest.tsv'))
    manifest=args.manifest
    if manifest is None:
        if len(candidates)!=1:parser.error('Specify --manifest when the folder has zero or multiple transfer manifests')
        manifest=candidates[0]
    output=args.output or args.received/(manifest.stem.replace('_manifest','')+'_restored')
    report=restore(args.received,manifest,output)
    print('Restored:',report['restored'],'; source skipped:',len(report['source_skipped']),'; errors:',len(report['errors']))
    print('Output:',output)
    return 1 if report['errors'] or report['source_skipped'] else 0
if __name__=='__main__':raise SystemExit(main())
