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

from gwp import ROOT, load, record, lobby_membership_text, STAGE_OBJECT_ASSETS

DESTINATIONS = {'stage_lighting20':'stage/n022a.lighting.cfg','stage_collision20':'stage/n022a.collision.cfg',**{f'stage_env{i}':f'stage/audio/env_s01a30l_{i:02d}.gwa' for i in (1,4,5,7,8)},'stage_preview20':'stage/n022a.gwm','character_catalog':'character/appearance.gwc','network_keys':'network.gnk','login_background':'login/frame.m2pv',**{f'login_texture{i}':f'login/images/{i}.dds' for i in range(6)},'agreement_motion':'motion/animated.m2an',**{f'motion_texture{i}':f'motion/images/{i}.dds' for i in range(8)},'agreement_background':'agreement/frame.m2pv','lobby_bgm':'audio/lobby.gwa',**{f'agreement_texture{i}':f'agreement/images/{i}.dds' for i in range(6)},'menu92':'audio/92.gwa','menu93':'audio/93.gwa','menu94':'audio/94.gwa','scenario': 'title.gwp', 'animation': 'title/animated.m2an',
                'bgm23': 'audio/title.gwa', 'start18999':'audio/start.gwa','loading':'loading/loading.m2an','loading_texture0':'loading/images/0.dds','loading_texture1':'loading/images/1.dds', **{f'texture{i}': f'title/images/{i}.dds' for i in range(10)}}
DOCUMENTS = ('README.md', 'README.ja.md', 'THIRD_PARTY_NOTICES.md', 'PUBLICATION.md', 'LICENSE_STATUS.md', 'START_KEYBOARD.cmd', 'START_PAD_1.cmd')
DESTINATIONS.update({f'voice{g}_{v}':f'voice/{g}_{v}.gwa' for g in range(2) for v in range(8)})
DESTINATIONS.update({**{f'stage_item{i}':f'stage/items/{i}.gwm' for i in (113,140)},'bgm_catalog':'bgm/catalog.json','stage_placements20':'stage/n022a.placements.cfg',**{f'stage_prop{i}':f'stage/props/{i}.gwm' for i in range(6)}})
DESTINATIONS['stage_cbox20']='stage/n022a.cbox.cfg'
DESTINATIONS['stage_spawns20']='stage/n022a.tdm-spawns.cfg'
DESTINATIONS['player_motion']='character/player.gwmot'
DESTINATIONS['weapon_icons']='weapon-icons/index.tsv'
DESTINATIONS.update({f'weapon_icon{i}':f'weapon-icons/weapon_{i}.png' for i in range(256)})
DESTINATIONS.update({'combat_audio_index':'sfx/combat.txt',**{f'combat_body{i}':f'sfx/body_impact_{i}_v0.wav' for i in (1369,8168)}})
DESTINATIONS['combat_ak102_shot']='sfx/ak102_10002_v0.wav'
DESTINATIONS.update({role:'stage/'+name for role,name in STAGE_OBJECT_ASSETS.items()})
SKILL_ICON_FILES = ('skill_star.png', 'participants.png', 'rules.png', 'deploy.png',
                    'skills.png', 'equipment.png', 'options.png')
TITLE_MOVIE_BYTES = 116061027
TITLE_MOVIE_SHA256 = '499bbf7c678dd25918fc6fe33dc58e65f75a08c7fdbdbe49bdd171877e8272c5'


def title_movie_package_sources():
    """User-supplied local movie; preserve its original bytes and fixed name.

    Recorded source: outputs/title_movie_20260915/resource.json. Import never
    changes the Desktop original. Future full client packages include this
    resource in assets.sha256 and package.json without a new GWP asset role.
    """
    source = ROOT/'work/title/movie_01.mp4'
    if not source.is_file() or source.is_symlink() or source.stat().st_size != TITLE_MOVIE_BYTES:
        raise ValueError('Missing or changed reviewed title movie resource')
    if record(source)['sha256'] != TITLE_MOVIE_SHA256:
        raise ValueError('Reviewed title movie SHA-256 changed')
    with source.open('rb') as stream:
        header = stream.read(12)
    if header[4:8] != b'ftyp':
        raise ValueError('Reviewed title movie MP4 container header')
    return [(source, 'movie_01.mp4')]


def stage_pickup_package_sources():
    folder=ROOT/'outputs/stage_pickups_20260915/candidate'
    proof=json.loads((folder/'evidence.json').read_text(encoding='utf-8'))
    expected={name+'.gcx-items.cfg' for name in ('n001a','n004a','n007a','n022a','n023a')}
    if set(proof['files'])!=expected:
        raise ValueError('Stage pickup script coverage')
    result=[]
    for name,sha in proof['files'].items():
        source=folder/name
        if source.is_symlink() or record(source)['sha256']!=sha:
            raise ValueError('Stage pickup script changed: '+name)
        result.append((source,'stage/'+name))
    return result


def original_ui_package_sources():
    proof=json.loads((ROOT/'outputs/original_ui_integration_20260915/resources.json').read_text(encoding='utf-8'))
    result=[]
    for row in proof['files']:
        source=ROOT/'work/original-ui-runtime'/row['path']
        if source.is_symlink() or record(source)['sha256']!=row['sha256']:
            raise ValueError('Original UI resource missing or changed: '+row['path'])
        result.append((source,row['path']))
    if len(result)!=24 or proof['shared_bank_candidates_used']:
        raise ValueError('Original UI runtime resource coverage')
    return result


def original_hold_box_package_sources():
    """Exact original MDN conversion and shared LA2 glyph inputs."""
    medium = ROOT/'work/stages/native/items/ibox_item_mid.gwm'
    if not medium.is_file() or medium.is_symlink() or record(medium)['sha256'] != '9307043f548645bb8b745c9cab86930203656290cd8d27e89d094266daad16c4':
        raise ValueError('Missing or changed original medium item box')
    folder = ROOT/'work/hold-font'
    proof = json.loads((ROOT/'outputs/original_hold_ui_20260915/font_manifest.json').read_text(encoding='utf-8'))
    index = folder/'index.tsv'
    expected = ['MGO2MT_WEAPON_ICONS\t1'] + [f'ICON\t{i}\tglyph_{i}.png' for i in range(32,127)]
    if index.is_symlink() or index.read_text(encoding='utf-8').splitlines() != expected:
        raise ValueError('Original hold font index mismatch')
    result = [(medium,'stage/items/ibox_item_mid.gwm'), (index,'hold-font/index.tsv')]
    for row in proof['glyphs']:
        source = folder/f"glyph_{row['code']}.png"
        if source.is_symlink() or record(source)['sha256'] != row['output']['sha256']:
            raise ValueError('Original hold glyph changed')
        result.append((source,'hold-font/'+source.name))
    if len(result) != 97 or {r['code'] for r in proof['glyphs']} != set(range(32,127)):
        raise ValueError('Original hold font extent mismatch')
    return result


def skill_package_sources():
    """Fixed reviewed inputs only; never copy preferences or an asset directory."""
    folder = ROOT/'work/skills-resources-20260913/icons'
    result = [(ROOT/'assets/skill_catalog.tsv', 'skill_catalog.tsv')]
    ui = ROOT/'outputs/briefing_ui_20260913'
    briefing = {(101 if selected else 1)+i: name+('_selected.png' if selected else '_normal.png')
                for selected in (False,True) for i,name in enumerate(('start','map','rules','skills','host','options','quit'))}
    for path, destination, expected in (
            (folder/'index.tsv', 'skills/index.tsv', {i: 'skill_star.png' for i in range(1, 26)}),
            (ui/'icons/index.tsv', 'skills/briefing.tsv', briefing),
            (ui/'map/index.tsv', 'briefing-map/index.tsv', {1:'n022a-online-map-0.png',2:'n022a-online-map-1.png'})):
        index_name = path.name
        if not path.is_file() or path.is_symlink() or path.stat().st_size > 16384:
            raise ValueError('Missing or invalid skill icon index: '+index_name)
        lines = path.read_text(encoding='utf-8').splitlines()
        if not lines or lines[0] != 'MGO2MT_WEAPON_ICONS\t1':
            raise ValueError('Skill icon index version')
        rows = {}
        for line in lines[1:]:
            match = re.fullmatch(r'ICON\t([1-9][0-9]*)\t([a-z0-9_-]+\.png)', line)
            if not match or int(match[1]) in rows:
                raise ValueError('Skill icon index row')
            rows[int(match[1])] = match[2]
        if rows != expected:
            raise ValueError('Skill icon index does not match reviewed input set')
        result.append((path, destination))
        if path.parent != folder:
            for name in expected.values():
                source=path.parent/name
                if not source.is_file() or source.is_symlink() or not 24 <= source.stat().st_size <= 4*1024*1024:
                    raise ValueError('Missing briefing image: '+name)
                if source.read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':raise ValueError('Briefing image must be PNG')
                result.append((source, str(Path(destination).parent/name).replace('\\','/')))
    for name in SKILL_ICON_FILES:
        source = folder/name
        if not source.is_file() or source.is_symlink() or not 24 <= source.stat().st_size <= 4*1024*1024:
            raise ValueError('Missing skill icon: '+name)
        with source.open('rb') as stream:
            header = stream.read(24)
        if header[:8] != b'\x89PNG\r\n\x1a\n' or header[12:16] != b'IHDR':
            raise ValueError('Skill icon must be a PNG: '+name)
        result.append((source, 'skills/'+name))
    catalog = result[0][0]
    if not catalog.is_file() or catalog.is_symlink() or not 1 <= catalog.stat().st_size <= 65536:
        raise ValueError('Missing reviewed skill catalog')
    if not catalog.read_text(encoding='utf-8').startswith('MGO2MT_SKILLS\t1\n'):
        raise ValueError('Skill catalog version')
    return result


def build_release(root=ROOT):
    env = {k.upper(): v for k, v in os.environ.items()}
    cmake = shutil.which('cmake')
    if not cmake:
        raise ValueError('Install CMake and Visual Studio C++ Build Tools with Windows SDK first. / CMakeとC++開発環境が必要です。')
    folder = root/'build/package'
    logs = root/'outputs/package'; logs.mkdir(parents=True, exist_ok=True)
    commands = [('configure', [cmake, '-S', str(root), '-B', str(folder), '-A', 'x64',
                               '-DMGO2MT_STATIC_RUNTIME=ON',
                               '-DMGO2MT_BUILD_TIMESTAMP='+datetime.datetime.now().strftime('%Y%m%d%H%M%S')]),
                ('build', [cmake, '--build', str(folder), '--config', 'Release']),
                ('test', [str(Path(cmake).with_name('ctest.exe')), '--test-dir', str(folder), '-C', 'Release', '--output-on-failure'])]
    for name, command in commands:
        print('Running / 実行中: '+name, flush=True)
        result = subprocess.run(command, cwd=root, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (logs/(name+'.log')).write_bytes(result.stdout)
        if result.returncode:
            print(result.stdout.decode('utf-8', errors='replace'))
            raise ValueError('Build/test failed: '+str(logs/(name+'.log')))
    return folder/'Release/MGO2MT.exe'


def package(gwp_path, exe, output, seconds=None):
    document, assets, inputs = load(gwp_path)
    if not {'start18999','loading','loading_texture0','loading_texture1','menu92','menu93','menu94','agreement_background','lobby_bgm','agreement_motion','login_background','network_keys','character_catalog',*[f'voice{g}_{v}' for g in range(2) for v in range(8)]}<=set(assets):
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
    skill_sources = skill_package_sources()
    skill_sources.extend(title_movie_package_sources())
    skill_sources.extend(original_hold_box_package_sources())
    skill_sources.extend(original_ui_package_sources())
    skill_sources.extend(stage_pickup_package_sources())
    skill_sources.append((ROOT/"work/special/gekko_salute.wav", "special/gekko_salute.wav"))
    for cue in range(17000,17008):
        skill_sources.append((ROOT/"work/ak102-reload-audio"/f"ak102_reload_{cue}.wav", f"sfx/ak102_reload_{cue}.wav"))
    for name in ("hands.gwh", "ak102.gwm", "operator.gwm", "ak102_secondary.gwm", "operator_secondary.gwm"):
        skill_sources.append((ROOT/"work/weapon_hand"/name, "weapons/"+name))
    for name, target in (('selection0.gwmot', 'character/selection0.gwmot'),
                         ('selection1.gwmot', 'character/selection1.gwmot'),
                         ('salute.gwa', 'audio/salute.gwa')):
        source = ROOT/'work/pc-selection'/name
        if not source.is_file() or source.is_symlink():
            raise ValueError('Missing reviewed PC selection asset: '+name)
        skill_sources.append((source, target))
    music=[];catalog=None
    if 'bgm_catalog' in assets:
        catalog=json.loads(assets['bgm_catalog'].read_text(encoding='utf-8'))
        if catalog.get('format')!='MGO2MT.BGM_CATALOG' or catalog.get('version')!=1 or not 0<len(catalog['tracks'])<=256:
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
    shutil.copyfile(exe, output/'MGO2MT.exe')
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
    for source, name in skill_sources:
        target = data/name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        hashes[name] = record(target)['sha256']
    runtime = document['runtime']
    index=data/'sfx/combat.txt'
    if index.exists():
        rows={int(line.split()[0]):line.split()[1] for line in index.read_text().splitlines()[1:] if line.strip()}
        rows.update({cue:f'ak102_reload_{cue}.wav' for cue in range(17000,17008)})
        index.write_text(f'MGO2MT.COMBAT_AUDIO 1 {len(rows)}\n'+''.join(f'{cue} {name}\n' for cue,name in sorted(rows.items())),encoding='ascii')
        hashes['sfx/combat.txt']=record(index)['sha256']
    duration = runtime['preview_seconds'] if seconds is None else seconds
    (data/'launch.cfg').write_text(f"MGO2MT.TITLE 7\n{duration} {runtime['entry_procedure']} {int(runtime['audio_enabled'])}\n{document['network']['policy_url']}\n", encoding='ascii')
    hashes['launch.cfg'] = record(data/'launch.cfg')['sha256']
    (data/'assets.sha256').write_text(''.join(f'{value}  {name}\n' for name, value in sorted(hashes.items())), encoding='ascii')
    # The GWP remains the editable source for a future build; the desktop reads a
    # fixed, hashed launch snapshot, not arbitrary JSON commands.
    gwp = copy.deepcopy(document)
    gwp['format'] = 'MGO2MT.GWP'
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
    version_output = subprocess.run([str(exe.resolve()), '--version'], capture_output=True, text=True, check=True, timeout=10).stdout.strip()
    if not re.fullmatch(r'MGO2MT v0\.01-[0-9]{14}', version_output):
        raise ValueError('Executable build version is missing or invalid')
    manifest = {'format': 'MGO2MT.LOCAL_PACKAGE', 'version': 1, 'build_version': version_output.split()[1], 'distribution': 'local_only_contains_game_assets',
                'created_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'source_gwp_sha256': record(gwp_path)['sha256'],
                'packager_sha256': record(Path(__file__))['sha256'],
                'input_asset_hashes': {**{x['role']: x['sha256'] for x in inputs if 'role' in x},
                                       **{'native:'+name: record(source)['sha256'] for source, name in skill_sources}},
                'runtime': gwp['runtime'], 'files': files,
                'limitations': ['START native stereo mix, original DSP parity pending', 'OpenMGO2 login, character selection, host admission, roster and map metadata implemented; full gameplay and peer mesh pending', 'n022a/TDM reviewed32 object snapshot/delta applies stable models, GEOM walking collision and GM_HIT targets; fragment FX, other profiles and full original scene behavior pending', 'GWM v2 authored RGB restored for reviewed 0x120000 materials; full original prelighting, transparency, reflection and material constants pending', 'Local native capsule rigid body and13-body12-joint ragdoll use matched current MGO2 skeleton data; original solver parity, self collision and networked death pending', 'selected appearance and original lobby motion implemented; some equipment/materials and exact original clip assignment pending', 'Native GWCB v5 READY/cancel, host DP/loadout and respawn approval; native three-second death wait, original spawn placement and same-track music continuity; n022a/map20 TDM/rule1 flags0 supports only native basic AK102', 'Host-scheduled trigger/automatic fire, host-driven reload display, collision/hit/HP/ammo and reliable combat effects implemented and tested with two local protocol peers; AK102 uses original30tick threshold at native nominal100.1ms and normal reload motion; limited live path enabled, actual two-PC combat untested; GWAV v1 remote appearance and interpolated original pose clips implemented with limited state coverage', 'Original body-impact WAV cues1369/8168 variant0 restored; AK102 normal cue10002 restored as a native near-distance choice; random variation, original spatial DSP and other effects pending', 'Original skill cost budget defaults to four, future server entitlement may allow eight; native database endpoint candidate not deployed, no purchase UI, skill combat modifiers pending', 'HUD uses Japanese text, verified 64x64 EM64 clan downloads and a host-owned native DM/TDM round clock; absent values remain blank/unknown. Original glyphs have native skill/briefing assignments', 'Native single-rotation n022a/TDM timeout stops combat, displays ended state and rebuilds the next epoch after three seconds; original ticket/winner/rotation rules pending', 'Explicit isolated keyboard and background XInput local test profiles use UDP5730/5731 with manual session-only login; physical dual-client trial pending', 'no PS3 audiovisual parity claim'],
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
    exe = ROOT/'build/package/Release/MGO2MT.exe' if args.skip_build else build_release()
    output = args.output or ROOT/'dist'/('MGO2MT-local-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    manifest = package(args.gwp, exe, output, args.seconds)
    checked = subprocess.run([str(output.resolve()/'MGO2MT.exe'), '--check'], cwd=ROOT, timeout=30)
    if checked.returncode:
        raise ValueError('Desktop package validation failed / パッケージ検証に失敗しました')
    print('Ready / 作成完了: '+str(output.resolve()/'MGO2MT.exe'))
    print('Local only: contains game assets / ゲームデータを含むためローカル専用です。')
    (ROOT/'outputs/package').mkdir(parents=True, exist_ok=True)
    (ROOT/'outputs/package/latest.json').write_text(json.dumps({'folder': str(output.resolve()),
        'exe': record(output/'MGO2MT.exe'), 'manifest': record(output/'package.json'),
        'validation_exit': checked.returncode}, indent=2), encoding='utf-8')


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError, KeyError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
