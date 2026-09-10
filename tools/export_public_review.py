"""Export an explicit source-only review set. Never creates/pushes a Git remote."""
import datetime
import json
from pathlib import Path
import re
import shutil
import sys

from gwp import ROOT, record
from package_title import DOCUMENTS

SOURCE_FILES = (
    'tools/compile_title_program.cpp',
    'tests/character_registration_test.cpp',
    'include/character_creation.h','src/character_creation.cpp','tests/character_creation_test.cpp',
    'include/character_catalog.h','src/character_catalog.cpp','tests/character_catalog_test.cpp',
    'include/character_model.h','src/character_model.cpp','include/character_renderer.h','src/character_renderer.cpp','tests/character_model_test.cpp',
    'include/character_slots.h','tests/character_slots_test.cpp',
    'src/character_client.cpp','include/character_client.h','src/character_screen.cpp','include/character_screen.h','tests/character_client_test.cpp','tools/allow_openmgo2_account.ps1',
    'src/graphics_settings.cpp','include/graphics_settings.h','tests/graphics_settings_test.cpp',
    'src/controller_input.cpp','include/controller_input.h','src/controller_panel.cpp','include/controller_panel.h','tests/controller_input_test.cpp',
    'src/stun.cpp','include/stun.h','tests/stun_test.cpp','tools/allow_openmgo2_stun.ps1',
    'src/port_settings.cpp','include/port_settings.h','src/port_screen.cpp','include/port_screen.h','tests/port_settings_test.cpp',
    'src/authentication.cpp','include/authentication.h','tests/authentication_test.cpp','tests/login_auth_flow_test.cpp',
    'CMakeLists.txt', 'Build-Title.cmd', 'Allow-OpenMGO2.cmd', 'tools/allow_openmgo2.ps1',
    'src/gcl_lengths.cpp', 'src/gcx_runtime.cpp', 'src/title_gcx.cpp', 'src/title_animation.cpp',
    'src/login_store.cpp','include/login_store.h','tests/login_store_test.cpp','src/login_screen.cpp','include/login_screen.h','include/login_form.h','tests/login_form_test.cpp',
    'src/http_text.cpp','src/agreement_screen.cpp','include/http_text.h','include/agreement_screen.h','tests/http_text_test.cpp',
    'src/windows_audio.cpp', 'src/audio_main.cpp', 'src/title_preview.cpp', 'src/title_main.cpp', 'src/title_desktop_main.cpp',
    'include/audio_control.h', 'include/audio_fade.h', 'include/gcx_runtime.h', 'include/title_gcx.h',
    'include/title_animation.h', 'include/mgo2win/gcl_lengths.hpp',
    'tests/background_animation_test.cpp', 'tests/audio_fade_test.cpp', 'tests/gcl_lengths_test.cpp', 'tests/gcx_runtime_test.cpp', 'tests/title_animation_test.cpp',
    'tools/windows_sdk_probe.cpp', 'tools/gwp.py', 'tools/gcw.py', 'tools/package_title.py', 'tools/export_public_review.py',
    'licenses/vgmstream-r2117-COPYING.txt', 'licenses/Solid4-local-README.txt',
)


def source_map(root=ROOT):
    rows = {name: root/name for name in SOURCE_FILES}
    for name in DOCUMENTS:
        rows[name] = root/'distribution'/name
        rows['distribution/'+name] = root/'distribution'/name
    for name, source in rows.items():
        if not source.is_file() or source.is_symlink() or not source.resolve().is_relative_to(root.resolve()):
            raise ValueError('Invalid export input: '+name)
        data = source.read_bytes()
        if b'\0' in data or len(data)>1024*1024:
            raise ValueError('Unexpected binary/large source: '+name)
        # This is a narrow content screen, not an assertion of exhaustive rights/secret review.
        text = data.decode('utf-8')
        if re.search(r'(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)', text):
            raise ValueError('Credential-like content: '+name)
        if re.search(r'[A-Za-z]:[\\/](?:Users|App-Tool|Projects)[\\/]', text):
            raise ValueError('Private absolute path: '+name)
    return rows


def export(output, root=ROOT):
    output = Path(output).resolve()
    rows = source_map(root)
    if output.exists():
        raise ValueError('Refusing to overwrite an existing review export')
    output.mkdir(parents=True)
    for name, source in rows.items():
        target = output/name; target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    (output/'scenes').mkdir()
    (output/'scenes/README.md').write_text(
        '# Local assets / ローカル資産\n\n'
        'Supply your own prepared `nttitle.gwp` and its 61 assets. They are not distributed here. '
        'You can also pass `--gwp` to the packager. See README.\n\n'
        '準備済みのnttitle.gwpと61資産をローカルで指定してください。配布物には含みません。'
        '出力ツールの--gwpでも指定できます。READMEを参照してください。\n', encoding='utf-8')
    (output/'.gitignore').write_text(
        '/build/\n/dist/\n/work/\n/outputs/\n/publication/\n/data/\n/tools/vendor/\n'
        '__pycache__/\n*.pyc\n*.exe\n*.dll\n*.pdb\n*.gwp\n*.gcw\n*.gwc\n*.gcx\n*.gcl\n*.gwa\n*.gws\n'
        '*.gwm\n*.gnk\n*.m2an\n*.dds\n*.wav\n*.bgm\n*.i64\n*.idb\n*.elf\n*.self\n*.pdf\n.env\n.env.*\n', encoding='ascii')
    files = [{**record(p), 'path': p.relative_to(output).as_posix()} for p in sorted(output.rglob('*')) if p.is_file()]
    report = {'status': 'source_review_only_not_cleared_or_published', 'files': files,
              'exclusions': ['all game assets and converted assets', 'original ELF/IDA databases',
                             'assembly/decompilation dumps and research outputs', 'PDFs and private logs',
                             'reference source copies without verified license', 'historical crypto/decompiler helpers',
                             'third-party executable/DLL tools', 'asset conversion scripts pending origin review'],
              'remaining': ['source origin and redistribution rights review', 'project license selection',
                            'clean-machine executable test', 'portable game-dump asset preparation']}
    (output/'EXPORT_REVIEW.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report


if __name__ == '__main__':
    folder = ROOT/'publication'/('github-review-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
    report = export(folder)
    print(json.dumps({'folder': str(folder), 'files': len(report['files']), 'status': report['status']}, indent=2))
