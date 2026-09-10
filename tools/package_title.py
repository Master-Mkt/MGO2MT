"""Build a local-only, relocatable title package from a validated GWP.
No network access, automatic dependency installation, decryption or publishing.
"""
import argparse
import copy
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from gwp import ROOT, load, record

DESTINATIONS = {'character_catalog':'character/appearance.gwc','network_keys':'network.gnk','login_background':'login/frame.m2pv',**{f'login_texture{i}':f'login/images/{i}.dds' for i in range(6)},'agreement_motion':'motion/animated.m2an',**{f'motion_texture{i}':f'motion/images/{i}.dds' for i in range(8)},'agreement_background':'agreement/frame.m2pv','lobby_bgm':'audio/lobby.gwa',**{f'agreement_texture{i}':f'agreement/images/{i}.dds' for i in range(6)},'menu93':'audio/93.gwa','menu94':'audio/94.gwa','scenario': 'title.gwp', 'animation': 'title/animated.m2an',
                'bgm23': 'audio/title.gwa', 'start18999':'audio/start.gwa','loading':'loading/loading.m2an','loading_texture0':'loading/images/0.dds','loading_texture1':'loading/images/1.dds', **{f'texture{i}': f'title/images/{i}.dds' for i in range(10)}}
DOCUMENTS = ('README.md', 'README.ja.md', 'THIRD_PARTY_NOTICES.md', 'PUBLICATION.md', 'LICENSE_STATUS.md')
DESTINATIONS.update({f'voice{g}_{v}':f'voice/{g}_{v}.gwa' for g in range(2) for v in range(8)})


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
    output.mkdir(parents=True)
    data = output/'data'; data.mkdir()
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
        else:shutil.copyfile(source, target)
        hashes[name] = record(target)['sha256']
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
                'limitations': ['START native stereo mix, original DSP parity pending', 'OpenMGO2 login and native TCP character-list adapter implemented; real-account list validation and remote selection pending', 'selected appearance and original lobby motion implemented; some equipment/materials and exact original clip assignment pending', 'no PS3 audiovisual parity claim'],
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
