"""GWP v1: editable native launch/progress document; GCX remains script authority.

No eval, shell commands, or implicit hash refresh. A changed asset requires the
GWP's recorded digest to be deliberately updated after review.
"""
import argparse
import datetime
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HOOKS = ['load_gcx', 'execute_entry', 'run_title', 'selected', 'completed']

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

def load(path):
    path = Path(path).resolve()
    if path.suffix.lower() not in ('.gwp', '.gcw') or path.stat().st_size > 1024 * 1024:
        raise ValueError('expected GWP document, maximum 1 MiB')
    d = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    expected_format = {'.gwp':'MGO2WIN.GWP', '.gcw':'MGO2WIN.GCW'}[path.suffix.lower()]
    if d['format'] != expected_format or type(d['version']) is not int or d['version'] != 1:
        raise ValueError('unsupported GWP format/version')
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
    required = {'scenario', 'animation', 'bgm23', *[f'texture{i}' for i in range(10)]}
    extra={'start18999','loading','loading_texture0','loading_texture1'}
    agreement_roles={'menu93','menu94'}
    login_roles={'login_background',*[f'login_texture{i}' for i in range(6)]}
    motion_roles={'agreement_motion',*[f'motion_texture{i}' for i in range(8)]}
    original_roles={'agreement_background','lobby_bgm',*[f'agreement_texture{i}' for i in range(6)]}
    voice_roles={f'voice{g}_{v}' for g in range(2) for v in range(8)}
    if set(assets) not in (required,required|extra,required|extra|agreement_roles,required|extra|agreement_roles|original_roles,required|extra|agreement_roles|original_roles|motion_roles,required|extra|agreement_roles|original_roles|motion_roles|login_roles,required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys'},required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_model'},required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_catalog'},required|extra|agreement_roles|original_roles|motion_roles|login_roles|{'network_keys','character_catalog'}|voice_roles):
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
        command.extend(['--policy-url',d['network']['policy_url'],'--menu-confirm',str(assets['menu93']),'--menu-move',str(assets['menu94'])])
    if 'agreement_background' in assets:
        command.extend(['--agreement-background',str(assets['agreement_background']),'--lobby-music',str(assets['lobby_bgm'])])
    if 'login_background' in assets:command.extend(['--login-background',str(assets['login_background'])])
    if 'character_catalog' in assets:command.extend(['--character-catalog',str(assets['character_catalog'])])
    if 'voice0_0' in assets:command.extend(['--voice-directory',str(assets['voice0_0'].parent)])
    if 'character_model' in assets:command.extend(['--character-model',str(assets['character_model'])])
    if 'network_keys' in assets:command.extend(['--network-keys',str(assets['network_keys'])])
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
