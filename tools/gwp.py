"""GWP v1: editable native launch/progress document; GCX remains script authority.

No eval, shell commands, or implicit hash refresh. A changed asset requires the
GWP's recorded digest to be deliberately updated after review.
"""
import argparse
import datetime
import hashlib
import json
import math
import re
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HOOKS = ['load_gcx', 'execute_entry', 'run_title', 'selected', 'completed']
STAGE_OBJECT_ASSETS = {
    'stage_objects20':'n022a.objects.cfg', 'stage_bindings20':'n022a.bindings.cfg',
    **{f'stage_object_{name}':f'objects/{name}.gwm' for name in (
        's01a_car_a0_sk','s01a_car_a0_glass','s01a_car_b0_sk','s01a_car_b0_glass',
        's01a_drum_a0_sk','cbox_a_sk','cbox_a_kuzure_sk',
        *[f's01a_btle_{c}0_sk' for c in 'abcde'])},
    **{f'stage_geom_{key}':f'objects/geom_{key}.collision.cfg' for key in ('982f38','982fb8','9ebb66','0ae4e5')},
    **{f'stage_hit_bottle_{c}':f'objects/s01a_btle_{c}0_sk.hit.cfg' for c in 'abcde'},
    **{f'stage_hit_drum_{i}':f'objects/blast_drum_{i}.hit.cfg' for i in range(6)},
}

def record(path):
    path = Path(path).resolve()
    return {'path': str(path), 'size': path.stat().st_size,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}

def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate GWP key: {key}')
        result[key] = value
    return result

def lobby_membership_text(document):
    """Compile the reviewed ID/port membership snapshot; no name heuristics."""
    m = document.get('lobby_membership', {'host':'49.212.132.180', 'rows':[]})
    if not isinstance(m,dict) or set(m) != {'host','rows'} or m['host'] != '49.212.132.180':
        raise ValueError('invalid lobby membership host/settings')
    rows = m['rows']
    if not isinstance(rows,list) or len(rows)>256:
        raise ValueError('invalid lobby membership count')
    ids = set()
    lines = ['MGO2WIN.LOBBIES 1', m['host'], str(len(rows))]
    for row in rows:
        if not isinstance(row,dict) or set(row) != {'id','port','subtype'}:
            raise ValueError('invalid lobby membership row')
        for key,low,high in [('id',1,65535),('port',1,65535),('subtype',0,255)]:
            if type(row[key]) is not int or not low <= row[key] <= high:
                raise ValueError('invalid lobby membership '+key)
        if row['id'] in ids:raise ValueError('duplicate lobby membership ID')
        ids.add(row['id'])
        lines.append(f"{row['id']} {row['port']} {row['subtype']}")
    return '\n'.join(lines)+'\n'

def load(path):
    path = Path(path).resolve()
    if path.suffix.lower() not in ('.gwp', '.gcw') or path.stat().st_size > 1024 * 1024:
        raise ValueError('expected GWP document, maximum 1 MiB')
    d = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    expected_format = {'.gwp':'MGO2WIN.GWP', '.gcw':'MGO2WIN.GCW'}[path.suffix.lower()]
    if d['format'] != expected_format or type(d['version']) is not int or d['version'] != 1:
        raise ValueError('unsupported GWP format/version')
    lobby_membership_text(d)
    runtime = d['runtime']
    if runtime['adapter'] != 'native_title_gcx_subset_v1':
        raise ValueError('unsupported native adapter')
    if set(runtime) != {'adapter', 'normal_edition', 'initial_title_state', 'entry_procedure', 'preview_seconds', 'audio_enabled'}:
        raise ValueError('unknown or missing runtime setting')
    if type(runtime['normal_edition']) is not int or type(runtime['initial_title_state']) is not int or runtime['normal_edition'] != 0 or runtime['initial_title_state'] != 2:
        raise ValueError('only the explicit normal-edition/title bootstrap is implemented')
    duration = runtime['preview_seconds']
    if type(duration) not in (float, int) or not math.isfinite(duration) or not 0 < duration <= 600:
        raise ValueError('preview_seconds must be > 0 and <= 600')
    if type(runtime['audio_enabled']) is not bool:
        raise ValueError('audio_enabled must be boolean')
    if type(runtime['entry_procedure']) is not int or not 1 <= runtime['entry_procedure'] <= 32767:
        raise ValueError('entry_procedure out of range')
    if [node['hook'] for node in d['flow']] != HOOKS:
        raise ValueError('GWP v1 requires the five title lifecycle hooks in order')
    ids = [node['id'] for node in d['flow']]
    if len(set(ids)) != len(ids) or any(not isinstance(x, str) or not x for x in ids):
        raise ValueError('flow ids must be unique nonempty strings')
    if any(node['status'] not in ('implemented', 'partial', 'pending') for node in d['flow']):
        raise ValueError('invalid progress status')
    assets = d['assets']
    object_roles=set(STAGE_OBJECT_ASSETS)
    if set(assets)&object_roles and not object_roles|{'stage_preview20','stage_collision20','stage_lighting20','stage_cbox20'}<=set(assets):
        raise ValueError('Complete reviewed stage object bundle required')
    prop_roles={'stage_placements20',*[f'stage_prop{i}' for i in range(6)]}
    item_roles={'stage_item113','stage_item140'}
    if set(assets)&item_roles and (not item_roles<=set(assets) or 'stage_preview20' not in assets):
        raise ValueError('Complete stage item model bundle required')
    if set(assets)&prop_roles and (not prop_roles<=set(assets) or 'stage_preview20' not in assets):
        raise ValueError('Complete stage placement bundle required')
    if 'stage_cbox20' in assets and 'stage_preview20' not in assets:
        raise ValueError('Stage CBOX layout requires the stage preview')
    if 'stage_spawns20' in assets and not {'stage_preview20','stage_collision20'}<=set(assets):
        raise ValueError('TDM spawn profile requires its stage and collision data')
    if 'player_motion' in assets and 'character_catalog' not in assets:
        raise ValueError('Player motion requires the character skeleton catalog')
    combat_roles={'combat_audio_index','combat_body1369','combat_body8168'}
    if set(assets)&combat_roles and not combat_roles<=set(assets):
        raise ValueError('Complete combat effect bundle required')
    if 'combat_ak102_shot' in assets and not combat_roles<=set(assets):
        raise ValueError('AK102 shot requires the combat effect bundle')
    icon_roles={f'weapon_icon{i}' for i in range(256)}
    if set(assets)&icon_roles and 'weapon_icons' not in assets:
        raise ValueError('Weapon images require their index')
    asset_roles=set(assets)-prop_roles-item_roles-object_roles-combat_roles-icon_roles-{'bgm_catalog','stage_cbox20','stage_spawns20','player_motion','weapon_icons','combat_ak102_shot'}
    required = {'scenario', 'animation', 'bgm23', *[f'texture{i}' for i in range(10)]}
    extra={'start18999','loading','loading_texture0','loading_texture1'}
    agreement_roles={'menu92','menu93','menu94'}
    login_roles={'login_background',*[f'login_texture{i}' for i in range(6)]}
    motion_roles={'agreement_motion',*[f'motion_texture{i}' for i in range(8)]}
    original_roles={'agreement_background','lobby_bgm',*[f'agreement_texture{i}' for i in range(6)]}
    voice_roles={f'voice{g}_{v}' for g in range(2) for v in range(8)}
    stage_roles={'stage_lighting20','stage_collision20',*[f'stage_env{i}' for i in (1,4,5,7,8)]}
    if asset_roles not in (required,required|extra,required|extra|agreement_roles,required|extra|agreement_roles|original_roles,required|extra|agreement_roles|original_roles|motion_roles,required|extra|agreement_roles|original_roles|motion_roles|login_roles,required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys'},required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_model'},required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_catalog'},required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_catalog'}|voice_roles,required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_catalog','stage_preview20'}|voice_roles,required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_catalog','stage_preview20'}|voice_roles|stage_roles):
        raise ValueError('missing or unknown GWP asset role')
    if 'menu93' in assets:
        network=d.get('network',{})
        base={'policy_url','method','service'}
        login_keys={'login_url','login_method'};stun_keys={'stun_server','stun_port'}
        if set(network) not in (base,base|login_keys,base|login_keys|stun_keys) or network['method']!='GET' or network['service']!='OpenMGO2':
            raise ValueError('OpenMGO2 GET configuration required')
        if 'login_url' in network and (network['login_url']!='https://openmgo2.com/index.php' or network['login_method']!='POST'):
            raise ValueError('Only the implemented OpenMGO2 login POST endpoint is allowed')
        if 'stun_server' in network and (network['stun_server']!='49.212.132.180' or type(network['stun_port']) is not int or network['stun_port']!=3478):
            raise ValueError('Only the reviewed OpenMGO2 STUN endpoint is implemented')
        url=network['policy_url']
        if not isinstance(url,str) or not url.startswith('https://openmgo2.com/') or len(url)>2048 or any(ord(c)<=32 or ord(c)>=127 or c in '#\\"' for c in url):
            raise ValueError('Only OpenMGO2 HTTPS policy URLs are allowed')
    elif 'network' in d: raise ValueError('Network settings require agreement audio assets')
    resolved = {}
    inputs = [record(path)]
    for role, item in assets.items():
        asset = (path.parent / item['path']).resolve()
        actual = record(asset)
        if actual['sha256'] != item['sha256']:
            raise ValueError(f'asset SHA-256 mismatch: {role}')
        resolved[role] = asset
        inputs.append({'role': role, **actual})
    for i in range(10):
        if resolved[f'texture{i}'] != resolved['animation'].parent / 'images' / f'{i}.dds':
            raise ValueError('v1 texture paths must match the animation adapter directory')
    if 'weapon_icons' in resolved:
        index=resolved['weapon_icons']
        if index.stat().st_size>16384:raise ValueError('Weapon icon index extent')
        lines=index.read_text(encoding='ascii').splitlines()
        if not lines or lines[0]!='MGO2WIN_WEAPON_ICONS\t1' or not 1<=len(lines)-1<=128:raise ValueError('Weapon icon index contract')
        seen=set()
        for line in lines[1:]:
            parts=line.split('\t')
            if len(parts)!=3 or parts[0]!='ICON' or not re.fullmatch(r'[0-9]{1,3}',parts[1]) or int(parts[1])>255:raise ValueError('Weapon icon row')
            ident=int(parts[1]);role=f'weapon_icon{ident}'
            if ident in seen or parts[2]!=f'weapon_{ident}.png' or role not in resolved or resolved[role]!=index.parent/parts[2]:raise ValueError('Weapon icon bundle mismatch')
            seen.add(ident)
        if set(resolved)&icon_roles!={f'weapon_icon{i}' for i in seen}:raise ValueError('Unindexed weapon icon')
    if 'combat_audio_index' in resolved:
        for cue in (1369,8168):
            if resolved[f'combat_body{cue}']!=resolved['combat_audio_index'].parent/f'body_impact_{cue}_v0.wav':
                raise ValueError('Combat effect bundle directory mismatch')
        if 'combat_ak102_shot' in resolved and resolved['combat_ak102_shot'] != resolved['combat_audio_index'].parent/'ak102_10002_v0.wav':
            raise ValueError('AK102 shot bundle directory mismatch')
        index = resolved['combat_audio_index']
        if index.stat().st_size > 16384:
            raise ValueError('Combat effect index too large')
        expected = ['1369 body_impact_1369_v0.wav', '8168 body_impact_8168_v0.wav']
        if 'combat_ak102_shot' in resolved:
            expected.append('10002 ak102_10002_v0.wav')
        if index.read_text(encoding='ascii').splitlines() != [f'MGO2WIN.COMBAT_AUDIO 1 {len(expected)}', *expected]:
            raise ValueError('Combat effect index and hashed assets disagree')
    if 'stage_objects20' in resolved:
        for role,name in STAGE_OBJECT_ASSETS.items():
            if resolved[role]!=resolved['stage_preview20'].parent/name:
                raise ValueError('Stage object bundle directory mismatch')
    if 'loading' in resolved and resolved['loading_texture0']!=resolved['loading'].parent/'images/0.dds':
        raise ValueError('loading texture path must match its animation directory')
    if 'loading' in resolved and resolved['loading_texture1']!=resolved['loading'].parent/'images/1.dds':
        raise ValueError('loading texture path must match its animation directory')
    if 'agreement_background' in resolved:
        for i in range(6):
            if resolved[f'agreement_texture{i}']!=resolved['agreement_background'].parent/'images'/f'{i}.dds':
                raise ValueError('agreement texture directory mismatch')
    if 'agreement_motion' in resolved:
        for i in range(8):
            if resolved[f'motion_texture{i}']!=resolved['agreement_motion'].parent/'images'/f'{i}.dds':raise ValueError('motion texture directory mismatch')
    if 'login_background' in resolved:
        for i in range(6):
            if resolved[f'login_texture{i}']!=resolved['login_background'].parent/'images'/f'{i}.dds':raise ValueError('login texture directory mismatch')
    if d['audio_contract'] != {'cue': 23, 'loop_start_frame': 369920, 'loop_end_frame': 3028480,
                               'sample_rate': 48000, 'channels': 4}:
        raise ValueError('unsupported v1 BGM adapter contract')
    if 'voice0_0' in resolved:
        for g in range(2):
            for v in range(8):
                if resolved[f'voice{g}_{v}']!=resolved['voice0_0'].parent/f'{g}_{v}.gwa':raise ValueError('voice directory mismatch')
    return d, resolved, inputs

def progress(document, events):
    """Observed progression, independent of editable implementation status labels."""
    entries = {e.get('gcx_event'): e for e in events if 'gcx_event' in e}
    audio = next((e for e in events if 'samples_played' in e and e.get('channels') == 4 and e.get('stream')!='lobby_bgm'), None)
    effect = next((e for e in events if 'samples_played' in e and e.get('channels') == 2 and e.get('cue',18999)==18999), None)
    loading = next((e for e in events if 'loading_frames' in e), {})
    rendered = next((e for e in events if 'frames' in e), None)
    deferred = {'selected': [], 'completed': []}
    phase = None
    for event in events:
        if 'gcx_event' in event:
            phase = event['gcx_event']
        if phase in deferred and event.get('disposition', '').startswith('deferred_'):
            deferred[phase].append(event)
    return [
        {'id': node['id'], 'hook': node['hook'], 'declared_status': node['status'],
         'observed': ({'load_gcx': 'start' in entries,
                       'execute_entry': any(e.get('gcx_title_config') for e in events),
                       'run_title': bool(rendered and rendered['frames'] > 0),
                       'selected': 'selected' in entries, 'completed': 'completed' in entries}[node['hook']]),
         'deferred_effects': deferred.get(node['hook'], []),
         **({'start_sound_frames_played': effect['samples_played'] if effect else 0,
             'start_sound_completed': bool(effect and effect.get('stream_completed'))}
             if node['hook'] == 'selected' else {}),
         **({'loading_frames': loading.get('loading_frames', 0),
             'loading_callback_observed': 'loading_ready' in entries,
             'lobby_request_observed': any(e.get('disposition') == 'lobby_requested_not_implemented' for e in events),
             'policy_get_succeeded': any(e.get('policy_ready') for e in events),
             'login_presented': any(e.get('login_visible') for e in events),
             'agreement_presented': any(e.get('agreement_visible') for e in events),
             'agreement_choice': next((e['agreement_choice'] for e in events if 'agreement_choice' in e),None)}
             if node['hook'] == 'completed' else {}),
         **({'audio_samples_played': audio['samples_played'] if audio else 0,
             'frames': rendered['frames'] if rendered else 0,
             'occluded_presents': rendered['occluded_presents'] if rendered else 0,
             'non_occluded_present_observed': bool(rendered and rendered['frames'] > rendered['occluded_presents'])}
             if node['hook'] == 'run_title' else {})}
        for node in document['flow']]

def main():
    parser = argparse.ArgumentParser(description='Validate or launch a native GWP progression document')
    parser.add_argument('gwp', type=Path)
    parser.add_argument('--check', action='store_true', help='validate only; no window or playback')
    parser.add_argument('--scripted-input', action='store_true', help='send explicit test Enter messages at ticks 100 and 900')
    args = parser.parse_args()
    d, assets, inputs = load(args.gwp)
    if args.check:
        print(f'GWP v1 and all {len(assets)} asset digests validated')
        return 0
    exe = ROOT / ('build/package/Release/mgo2win_title_preview.exe' if 'loading' in assets else 'build/Debug/mgo2win_title_preview.exe')
    inputs.append({'role': 'executable', **record(exe)})
    began = datetime.datetime.now(datetime.timezone.utc)
    out = ROOT / 'outputs/gwp' / began.strftime('%Y%m%dT%H%M%S%fZ')
    out.mkdir(parents=True)
    r = d['runtime']
    command = [str(exe), str(assets['animation']), str(r['preview_seconds']), str(out/'title.bmp'),
               '--gcx', str(assets['scenario']), '--entry', str(r['entry_procedure']), '--wav', str(assets['bgm23'])]
    if r['audio_enabled']:
        command.append('--audio')
    if 'loading' in assets:
        command.extend(['--loading',str(assets['loading']),'--se',str(assets['start18999'])])
    if 'network' in d:
        command.extend(['--policy-url',d['network']['policy_url'],'--menu-cancel',str(assets['menu92']),'--menu-confirm',str(assets['menu93']),'--menu-move',str(assets['menu94'])])
    if 'agreement_background' in assets:
        command.extend(['--agreement-background',str(assets['agreement_background']),'--lobby-music',str(assets['lobby_bgm'])])
    if 'login_background' in assets:command.extend(['--login-background',str(assets['login_background'])])
    if 'character_catalog' in assets:command.extend(['--character-catalog',str(assets['character_catalog'])])
    if 'voice0_0' in assets:command.extend(['--voice-directory',str(assets['voice0_0'].parent)])
    if 'character_model' in assets:command.extend(['--character-model',str(assets['character_model'])])
    if 'network_keys' in assets:
        # Keep the same sibling layout as a desktop package for direct GWP runs.
        launch_keys=out/'network.gnk';launch_keys.write_bytes(assets['network_keys'].read_bytes())
        membership=out/'lobbies.cfg';membership.write_text(lobby_membership_text(d),encoding='ascii')
        inputs.append({'role':'lobby_membership',**record(membership)})
        command.extend(['--network-keys',str(launch_keys)])
    if 'agreement_motion' in assets:command.extend(['--agreement-motion',str(assets['agreement_motion'])])
    if args.scripted_input:
        command.append('--scripted-input')
    result = subprocess.run(command, capture_output=True, timeout=r['preview_seconds'] + 20)
    stdout = result.stdout.decode('utf-8', errors='replace')
    stderr = result.stderr.decode('utf-8', errors='replace')
    events = []
    for line in stdout.splitlines():
        if line.startswith('{'):
            events.append(json.loads(line))
    report = {'gwp': str(args.gwp.resolve()), 'started_utc': began.isoformat(),
              'scope': 'native GCX title subset, explicit proc entry, full boot pending',
              'input_mode': 'test_win32_enter_messages' if args.scripted_input else 'interactive_keyboard',
              'command': command, 'inputs': inputs, 'exit_code': result.returncode,
              'stdout': stdout, 'stderr': stderr, 'events': events,
              'progress': progress(d, events), 'captures': [record(p) for p in sorted(out.glob('*.bmp'))]}
    (out/'run.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    (out/'run.log').write_bytes(result.stdout + result.stderr)
    print(stdout, stderr)
    print('GWP run report:', out/'run.json')
    return result.returncode

if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    try:
        sys.exit(main())
    except (ValueError, KeyError, OSError, subprocess.TimeoutExpired) as error:
        print(f'GWP: {error}', file=sys.stderr)
        sys.exit(1)
