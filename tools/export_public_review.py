"""Copy the current native source set to a new directory; no remote operations."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
FIXED=('CMakeLists.txt','Build-Title.cmd','Allow-OpenMGO2.cmd','.gitignore',
       'README.md','README.ja.md','PUBLICATION.md','LICENSE_STATUS.md',
       'THIRD_PARTY_NOTICES.md','LOCAL_TWO_CLIENT_PLAYTEST.md','START_KEYBOARD.cmd','START_PAD_1.cmd',
       'tools/compile_title_program.cpp','tools/windows_sdk_probe.cpp','tools/package_title.py',
       'tools/gwp.py','tools/gcw.py','tools/export_public_review.py',
       'tools/allow_openmgo2.ps1','tools/allow_openmgo2_account.ps1','tools/allow_openmgo2_stun.ps1')
def source_map(root=ROOT):
    rows={name:root/name for name in FIXED}
    for directory,extensions in [('include',{'.h','.hpp','.in'}),('src',{'.cpp','.inc'}),
                                 ('tests',{'.cpp','.h'}),('assets',{'.json','.tsv'}),
                                 ('licenses',{'.txt'}),('docs',{'.md','.txt'}),('release',{'.json','.txt'}),
                                 ('distribution',{'.md','.cmd'}),('scenes',{'.md'})]:
        for p in (root/directory).rglob('*'):
            if p.is_file() and p.suffix in extensions:rows[p.relative_to(root).as_posix()]=p
    for name,p in rows.items():
        if p.is_symlink() or not p.is_file() or not p.resolve().is_relative_to(root.resolve()):raise ValueError('Invalid source path: '+name)
        b=p.read_bytes()
        if b'\0' in b or len(b)>1024*1024:raise ValueError('Binary or oversized input: '+name)
        text=b.decode('utf-8-sig')
        if re.search(r'(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)',text):raise ValueError('Credential-like input: '+name)
        if re.search(r'[A-Za-z]:[\\/]+(?:Users|App-Tool|Projects)[\\/]+',text):raise ValueError('Private absolute path: '+name)
    return rows
def export(output,root=ROOT):
    output=Path(output).resolve();rows=source_map(root)
    if output.exists():raise ValueError('Refusing an existing output directory')
    output.mkdir(parents=True)
    for name,p in rows.items():
        target=output/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,target)
    report={'status':'source_snapshot_not_published','files':[
        {'path':name,'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
        for name,p in sorted(rows.items())]}
    return report
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('output',type=Path)
    args=parser.parse_args();print(json.dumps(export(args.output),indent=2))
