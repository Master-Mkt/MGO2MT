"""Build a local-only, relocatable title package from a validated GWP.
No network access, automatic dependency installation, decryption or publishing.
"""
import argparse
import copy
import datetime
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

from gwp import ROOT, load, record, lobby_membership_text

DESTINATIONS = {'stage_lighting20':'stage/n022a.lighting.cfg','stage_collision20':'stage/n022a.collision.cfg',**{f'stage_env{i}':f'stage/audio/env_s01a30l_{i:02d}.gwa' for i in (1,4,5,7,8)},'stage_preview20':'stage/n022a.gwm','character_catalog':'character/appearance.gwc','network_keys':'network.gnk','login_background':'login/frame.m2pv',**{f'login_texture{i}':f'login/images/{i}.dds' for i in range(6)},'agreement_motion':'motion/animated.m2an',**{f'motion_texture{i}':f'motion/images/{i}.dds' for i in range(8)},'agreement_background':'agreement/frame.m2pv','lobby_bgm':'audio/lobby.gwa',**{f'agreement_texture{i}':f'agreement/images/{i}.dds' for i in range(6)},'menu93':'audio/93.gwa','menu94':'audio/94.gwa','scenario': 'title.gwp', 'animation': 'title/animated.m2an',
                'bgm23': 'audio/title.gwa', 'start18999':'audio/start.gwa','loading':'loading/loading.m2an','loading_texture0':'loading/images/0.dds','loading_texture1':'loading/images/1.dds', **{f'texture{i}': f'title/images/{i}.dds' for i in range(10)}}
DOCUMENTS = ('README.md', 'README.ja.md', 'THIRD_PARTY_NOTICES.md', 'PUBLICATION.md', 'LICENSE_STATUS.md')
DESTINATIONS.update({f'voice{g}_{v}':f'voice/{g}_{v}.gwa' for g in range(2) for v in range(8)})
DESTINATIONS.update({**{f'stage_item{i}':f'stage/items/{i}.gwm' for i in (113,140)},'bgm_catalog':'bgm/catalog.json','stage_placements20':'stage/n022a.placements.cfg',**{f'stage_prop{i}':f'stage/props/{i}.gwm' for i in range(6)}})
DESTINATIONS['stage_cbox20']='stage/n022a.cbox.cfg'


def build_release(root=ROOT):
    env = {k.upper(): v for k, v in os.environ.items()}
    cmake = shutil.which('cmake')
    if not cmake:
        raise ValueError('Install CMake and Visual Studio C++ Build Tools with Windows SDK first. / CMakeとC++開発環境が必要です。')
    folder = root/'build/package'
    logs = root/'outputs/package'; logs.mkdir(parents=True, exist_ok=True)
    commands = [('configure', [cmake, '-S', str(root), '-B', str(folder), '-A', 'x64',
                               '-DMGO2WIN_STATIC_RUNTIME=ON']),
                ('build', [cmake, '--build', str(folder), '--config', 'Release']),
                ('test', [str(Path(cmake).with_name('ctest.exe')), '--test-dir', str(folder), '-C', 'Release', '--output-on-failure'])]
    for name, command in commands:
        print('Running / 実行中: '+name, flush=True)
        result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (logs/(name+'.log')).write_bytes(result.stdout)
        if result.returncode:
            print(result.stdout.decode('utf-8', errors='replace'))
            raise ValueError('Build/test failed: '+str(logs/(name+'.log')))
    return folder/'Release/MGO2WIN.exe'


def package(gwp_path, exe, output, seconds=None):
    document, assets, inputs = load(gwp_path)
    if not {'start18999','loading','loading_texture0','loading_texture1','menu93','menu94','agreement_background','lobby_bgm','agreement_motion','login_background','network_keys','character_catalog',*[f'voice{g}_{v}' for g in range(2) for v in range(8)]}<=set(assets):
        raise ValueError('Desktop v7 requires START, loading and agreement assets in the GWP.')
    output = Path(output).resolve()
    if output.exists():
        raise ValueError('Output already exists; choose a new folder. / 既存フォルダーは上書きしません。')
    if seconds is not None and not 0 < seconds <= 600:
        raise ValueError('Duration must be > 0 and <= 600')
    # Validate everything before creating output. Never copy source directories wholesale.
    if not Path(exe).is_file():
        raise ValueError('Release executable missing')
    documents = {name: ROOT/'distribution'/name for name in DOCUMENTS}
    for path in documents.values():
        if not path.is_file():
            raise ValueError('Missing distribution document: '+path.name)
    music=[];catalog=None
    if 'bgm_catalog' in assets:
        catalog=json.loads(assets['bgm_catalog'].read_text(encoding='utf-8'))
        if catalog.get('format')!='MGO2WIN.BGM_CATALOG' or catalog.get('version')!=1 or not 0<len(catalog['tracks'])<=256:
            raise ValueError('BGM catalog contract')
        names=set()
        for track in catalog['tracks']:
            name=track['file']
            if not re.fullmatch(r'[a-z0-9_]{1,100}\.wav',name) or name in names or track['id']!='original:'+name[:-4]:raise ValueError('BGM file identity')
            names.add(name);source=assets['bgm_catalog'].parent/name
            if not source.is_file() or source.is_symlink() or record(source)['sha256']!=track['sha256']:raise ValueError('BGM input hash mismatch')
            music.append((source,name))
    output.mkdir(parents=True)
    data = output/'data'; data.mkdir()
    (data/'bgm/additional').mkdir(parents=True)
    (data/'bgm/additional/README.txt').write_text('Add up to 32 PCM 16-bit WAV tracks (8 kHz–192 kHz, 1–8 channels, up to 256 MiB each). Restart to rescan. The filename is the displayed title.\n16-bit PCM WAVを最大32曲追加できます。8～192 kHz・1～8ch・1曲256 MiB以下。追加後は再起動。ファイル名が表示名になります。\nTracks use content SHA-256 IDs, not list positions. Missing forced tracks fall back to local selection, then an available original track, then silence.\n',encoding='utf-8')
    shutil.copyfile(exe, output/'MGO2WIN.exe')
    for name, source in documents.items():
        shutil.copyfile(source, output/name)
    for source in (ROOT/'licenses').glob('*.txt'):
        (output/'licenses').mkdir(exist_ok=True)
        shutil.copyfile(source, output/'licenses'/source.name)
    hashes = {}
    for role, source in assets.items():
        name = DESTINATIONS[role]
        target = data/name; target.parent.mkdir(parents=True, exist_ok=True)
        if role=='scenario':
            compiler=ROOT/'build/package/Release/compile_title_program.exe'
            subprocess.run([str(compiler),str(source),str(target)],check=True)
        elif role=='bgm_catalog':
            public_catalog={'format':catalog['format'],'version':catalog['version'],'mix':catalog['mix'],'tracks':[{k:v for k,v in t.items() if k!='source'} for t in catalog['tracks']]}
            target.write_text(json.dumps(public_catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        else:shutil.copyfile(source, target)
        hashes[name] = record(target)['sha256']
    # Music is optional at runtime: deleting an unavailable song must not block
    # boot. Per-track provenance is in catalog.json and package.json instead.
    for source,name in music:shutil.copyfile(source,data/'bgm'/name)
    # Editable display names are deliberately outside the mandatory runtime
    # hash manifest. The immutable audio IDs do not depend on these titles.
    playlist_header='# UTF-8: filename.wav=Display title / ファイル名.wav=表示名\n# Reopen F12 debug to refresh names. / F12を閉じて開き直すと曲名を再読込します。\n# Blank lines and # comments are ignored. / 空行・#行はコメント。\n'
    for extra in (False,True):
        relative='additional/playlist2.txt' if extra else 'playlist1.txt'
        source=assets['bgm_catalog'].parent/relative if catalog else None
        target=data/'bgm'/relative
        if source and source.is_file():shutil.copyfile(source,target)
        else:target.write_text(playlist_header+('' if extra else ''.join(f"{name}={Path(name).stem}\n" for _,name in music)),encoding='utf-8')
    (data/'lobbies.cfg').write_text(lobby_membership_text(document), encoding='ascii')
    hashes['lobbies.cfg'] = record(data/'lobbies.cfg')['sha256']
    weapon_catalog = ROOT/'assets/weapon_catalog.tsv'
    if weapon_catalog.is_file():
        shutil.copyfile(weapon_catalog, data/'weapon_catalog.tsv')
        hashes['weapon_catalog.tsv'] = record(data/'weapon_catalog.tsv')['sha256']
    runtime = document['runtime']
    duration = runtime['preview_seconds'] if seconds is None else seconds
    (data/'launch.cfg').write_text(f"MGO2WIN.TITLE 7\n{duration} {runtime['entry_procedure']} {int(runtime['audio_enabled'])}\n{document['network']['policy_url']}\n", encoding='ascii')
    hashes['launch.cfg'] = record(data/'launch.cfg')['sha256']
    (data/'assets.sha256').write_text(''.join(f'{value}  {name}\n' for name, value in sorted(hashes.items())), encoding='ascii')
    # The GWP remains the editable source for a future build; the desktop reads a
    # fixed, hashed launch snapshot, not arbitrary JSON commands.
    gwp = copy.deepcopy(document)
    gwp['format'] = 'MGO2WIN.GWP'
    gwp['runtime']['preview_seconds'] = duration
    for role in assets:
        gwp['assets'][role]['path'] = './data/'+DESTINATIONS[role]
        gwp['assets'][role]['sha256'] = hashes[DESTINATIONS[role]]
    # Analysis-only paths are not package runtime inputs.
    for row in gwp.get('backlog', []):
        row.pop('analysis_asset', None); row.pop('analysis_sha256', None)
    (output/'nttitle.gwp').write_text(json.dumps(gwp, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    (output/'LOCAL_ONLY.txt').write_text(
        'LOCAL BUILD WITH GAME-DERIVED ASSETS. Do not upload this folder or its data to GitHub.\n'
        'ゲーム由来のデータを含むローカル専用ビルドです。このフォルダーをGitHubにアップロードしないでください。\n', encoding='utf-8')
    files = [{**record(p), 'path': p.relative_to(output).as_posix()} for p in sorted(output.rglob('*')) if p.is_file()]
    manifest = {'format': 'MGO2WIN.LOCAL_PACKAGE', 'version': 1, 'distribution': 'local_only_contains_game_assets',
                'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'source_gwp_sha256': record(gwp_path)['sha256'],
                'packager_sha256': record(Path(__file__))['sha256'],
                'input_asset_hashes': {x['role']: x['sha256'] for x in inputs if 'role' in x},
                'runtime': gwp['runtime'], 'files': files,
                'limitations': ['START native stereo mix, original DSP parity pending', 'OpenMGO2 login, character selection, host admission, roster and map metadata implemented; gameplay and peer mesh pending', 'Map20 textured static preview, spatial hemisphere approximation and world collision query; debug bind-pose placements and local reset; original actor activation, full material/prelighting and gameplay pending', 'selected appearance and original lobby motion implemented; some equipment/materials and exact original clip assignment pending', 'F4 ordinary n022a/TDM weapon draft with recovered prices/restrictions; live DP balance, START readiness, loadout transmission and spawning pending', 'no PS3 audiovisual parity claim'],
                'runtime_dependencies': ['Windows 10/11 x64 system D3D11, D3DCompiler, XAudio2, BCrypt; MSVC runtime statically linked'],
                'not_bundled': ['IDA', 'vgmstream/FFmpeg DLLs', 'Noesis', 'Drebin', 'Python', 'SDK installers']}
    (output/'package.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gwp', '--gcw', dest='gwp', type=Path, default=ROOT/'scenes/nttitle.gwp')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--seconds', type=float, default=120, help='package host limit; original actor timeout remains unchanged')
    parser.add_argument('--skip-build', action='store_true', help='reuse the existing Release build')
    args = parser.parse_args()
    if not args.gwp.is_file():
        raise ValueError('A prepared local GWP with 61 assets is required. / 準備済みのGWPと61資産を指定してください。')
    load(args.gwp)
    if not 0 < args.seconds <= 600:
        raise ValueError('Invalid seconds')
    exe = ROOT/'build/package/Release/MGO2WIN.exe' if args.skip_build else build_release()
    output = args.output or ROOT/'dist'/('MGO2WIN-local-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    manifest = package(args.gwp, exe, output, args.seconds)
    checked = subprocess.run([str(output.resolve()/'MGO2WIN.exe'), '--check'], cwd=ROOT, timeout=30)
    if checked.returncode:
        raise ValueError('Desktop package validation failed / パッケージ検証に失敗しました')
    print('Ready / 作成完了: '+str(output.resolve()/'MGO2WIN.exe'))
    print('Local only: contains game assets / ゲームデータを含むためローカル専用です。')
    (ROOT/'outputs/package').mkdir(parents=True, exist_ok=True)
    (ROOT/'outputs/package/latest.json').write_text(json.dumps({'folder': str(output.resolve()),
        'exe': record(output/'MGO2WIN.exe'), 'manifest': record(output/'package.json'),
        'validation_exit': checked.returncode}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
