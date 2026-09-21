"""Create a software-only local distribution with exact file hashes."""
from pathlib import Path
import argparse,hashlib,json,shutil,sys,importlib.metadata,ssl

ROOT=Path(__file__).resolve().parents[1]

def record(path,root):
    return dict(path=path.relative_to(root).as_posix(),size=path.stat().st_size,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())

def package(destination,client=None,host=None,build=None):
    destination=Path(destination).resolve()
    if ROOT not in destination.parents or destination.exists():raise ValueError('Use a new distribution directory inside the workspace')
    destination.mkdir(parents=True)
    build=Path(build).resolve() if build is not None else ROOT/'outputs/excv_20260921/build'
    if ROOT not in build.parents:raise ValueError('Build input must be inside the workspace')
    manifest=json.loads((build/'manifest.json').read_text(encoding='utf-8'))
    exe=build/'app/MGO2MTEXCV.exe'
    if hashlib.sha256(exe.read_bytes()).hexdigest()!=manifest['sha256']:raise ValueError('Converter build changed')
    shutil.copy2(exe,destination/exe.name)
    for path in (client,host):
        if path:
            path=Path(path).resolve()
            if path.name not in ('MGO2MT.exe','MGO2MTHOST.exe'):raise ValueError('Unexpected executable')
            shutil.copy2(path,destination/path.name)
    shutil.copy2(ROOT/'notes/MGO2MTEXCV_GUIDE.ja.md',destination/'使い方.md')
    (destination/'配布内容.md').write_text('# MGO2MT EXCV ソフトのみ配布\n\n実行ソフトと使い方・ライセンス通知のみです。原作の画像・モデル・音声・フォント・原機コード・復号キー・network.gnk は含みません。変換には利用者が指定したローカルの原データを使用します。vgmstream は同梱せず、必要な場合のみ利用者が指定します。\n',encoding='utf-8')
    # License texts are copied from the actual interpreter/dependencies used
    # for this build. Neither site-packages nor game folders are bundled.
    license_root=destination/'licenses';license_root.mkdir()
    python=Path(sys.base_prefix)
    for rel in ('LICENSE.txt','tcl/tcl8.6/license.terms','tcl/tk8.6/license.terms'):
        source=python/rel
        if not source.is_file() and rel=='tcl/tcl8.6/license.terms':
            # The current official Python install incorporates Tcl's full
            # license into its aggregate LICENSE.txt instead of this sidecar.
            source=python/'LICENSE.txt'
            if 'Scriptics Corporation' not in source.read_text(encoding='utf-8'):raise ValueError('Missing Tcl license text')
        if not source.is_file():raise ValueError('Missing interpreter license: '+str(source))
        out=license_root/'Python'/rel;out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,out)
    for folder in sorted((Path(sys.prefix)/'Lib/site-packages').glob('*.dist-info')):
        if folder.name.startswith('pip-'):continue
        for source in sorted(folder.rglob('*')):
            if source.is_file() and source.name.upper().startswith(('LICENSE','COPYING','NOTICE')):
                out=license_root/folder.name/source.relative_to(folder);out.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,out)
    extra=ROOT/'tools/excv_licenses'
    for version in ('4.0.2',ssl.OPENSSL_VERSION.split()[1]):
        if not (extra/('OpenSSL-'+version)/'LICENSE.txt').is_file():raise ValueError('OpenSSL license is required for '+version)
    shutil.copytree(extra,license_root/'additional')
    from cryptography.hazmat.backends.openssl.backend import backend
    packages=[dict(name=d.metadata['Name'],version=d.version,license=d.metadata.get('License-Expression') or d.metadata.get('License','')) for d in importlib.metadata.distributions() if d.metadata['Name'].lower()!='pip']
    notice=dict(format='MGO2MT.THIRD_PARTY_NOTICES.1',python=sys.version.split()[0],openssl=backend.openssl_version_text(),python_openssl=ssl.OPENSSL_VERSION,packages=sorted(packages,key=lambda x:x['name'].lower()),decoder_bundled=False,game_payloads_bundled=False,distribution='Python/PyInstaller runtime and licensed dependencies; corresponding complete license texts are in licenses.')
    (destination/'third-party-notices.json').write_text(json.dumps(notice,indent=2)+'\n',encoding='utf-8')
    (destination/'converter-build.json').write_text(json.dumps(dict(size=manifest['size'],sha256=manifest['sha256'],
        bundled_program_sources=manifest['bundled_program_sources'],original_game_assets_bundled=False,product='MGO2MT EXCV',build_version=manifest.get('build_version',''),source_snapshot_manifest_sha256=manifest['source_snapshot_manifest_sha256']),indent=2)+'\n',encoding='utf-8')
    files=[record(p,destination) for p in sorted(destination.rglob('*')) if p.is_file()]
    report=dict(format='MGO2MT.SOFTWARE_ONLY.1',original_game_assets_bundled=False,published=False,files=files)
    (destination/'manifest.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for row in files:
        if record(destination/row['path'],destination)!=row:raise ValueError('Distribution changed during verification')
    print(json.dumps(dict(folder=str(destination),files=len(files)+1,bytes=sum(r['size'] for r in files),published=False)))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('destination');parser.add_argument('--client',type=Path);parser.add_argument('--host',type=Path);parser.add_argument('--build',type=Path)
    args=parser.parse_args();package(args.destination,args.client,args.host,args.build)
