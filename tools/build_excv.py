"""Build a standalone converter from program files only; never bundle originals."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,shutil
from excv_public_snapshot import snapshot,audit
ROOT=Path(__file__).resolve().parents[1]
OUTPUT=ROOT/'outputs/excv_20260921/build'
FILES=('mgo2excv.py','excv_core.py','excv_stage.py','archive_inventory.py','title_assets.py','stage_scene.py',
       'gcx_inspect.py','gwp.py','convert_stage_model.py','convert_character_model.py','native_assets.py',
       'extract_stage_normals.py','original_material.py','compile_stage_scene.py','stage_geometry.py','weapon_connect_points.py','excv_profiles_b.py','stage_texture_context.py',
       'excv_ui_resources.py','excv_ui_icon.py','excv_ui_profile.py','la2_inspect.py','excv_title_runtime.py','export_title_preview.py','export_title_animation.py',
       'excv_effect_resources.py','excv_actor_resources.py','excv_actor_profile.json','weapon_attachment.py','character_motion.py','prepare_gekko_model.py',
       'excv_character_resources.py','excv_character_profile.json','excv_audio_resources.py','excv_audio_profile.py',
       'excv_gekko_resources.py','excv_gekko_profile.json','excv_ui_runtime.py','excv_ui_runtime_profile.json','extract_briefing_icons.py','excv_briefing_map.py',
       'excv_stage_runtime_batch.py','excv_stage_runtime.py','compile_combat_spawn.py','compile_tdm_spawn.py','compile_stage_pickups.py','prepare_stage_water.py',
       'extract_authored_water_surface.py','compile_stage_objects.py','compile_object_registry.py','audit_object_registry.py','audit_texture_alpha.py','elf_image.py',
       'excv_native_resources.py','excv_native_profile.json','excv_runtime_assembly.py','excv_install.py','excv_damage_effect_resources.py')
def main(output=None,build_version=''):
    output=Path(output).resolve() if output is not None else OUTPUT
    if ROOT not in output.parents:raise ValueError('Build output must be inside the workspace')
    if output.exists():raise ValueError('Existing build is preserved; select a new --output directory')
    output.mkdir(parents=True)
    source=output/'public-source'
    portable=ROOT/'tools/excv_runtime'
    if (portable/'source-manifest.json').is_file():
        snapshot_report=json.loads((portable/'source-manifest.json').read_text(encoding='utf-8'))
        if snapshot_report.get('format')!='MGO2MT.EXCV_PUBLIC_SOURCE.1':raise ValueError('Unsupported public source snapshot')
        source.mkdir()
        for row in snapshot_report['files']:
            name=row['file']
            if Path(name).name!=name or name.endswith(('.exe','.dll')):raise ValueError('Unsafe snapshot member')
            original=portable/name;raw=original.read_bytes()
            if len(raw)!=row['size'] or hashlib.sha256(raw).hexdigest()!=row['sha256']:raise ValueError('Public source snapshot changed: '+name)
            if name.endswith('.py'):audit(name,raw.decode('utf-8'))
            shutil.copy2(original,source/name)
        shutil.copy2(portable/'source-manifest.json',source/'source-manifest.json')
    else:snapshot_report=snapshot(ROOT/'tools',source,FILES)
    args=[sys.executable,'-m','PyInstaller','--onefile','--windowed','--noupx','--name','MGO2MTEXCV',
          '--distpath',str(output/'app'),'--workpath',str(output/'temp'),'--specpath',str(output),
          '--paths',str(source),'--noconfirm','--clean','--hidden-import','tkinter','--hidden-import','tkinter.ttk']
    if build_version:
        import re
        if not re.fullmatch(r'v[0-9]+\.[0-9]+-[0-9]{14}',build_version):raise ValueError('Expected version vN.N-YYYYMMDDhhmmss')
        metadata=output/'excv-build.json';metadata.write_text(json.dumps(dict(product='MGO2MT EXCV',build_version=build_version))+'\n',encoding='utf-8');args+=['--add-data',str(metadata)+':.']
    sources=[]
    for row in snapshot_report['files']:
        name=row['file'];p=source/name;args+=['--add-data',str(p)+':.']
        sources.append(dict(file=name,size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    args.append(str(source/'mgo2excv.py'))
    subprocess.run(args,check=True,cwd=ROOT)
    exe=output/'app/MGO2MTEXCV.exe'
    (output/'manifest.json').write_text(json.dumps(dict(executable=str(exe),size=exe.stat().st_size,
        sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),bundled_program_sources=sources,
        original_game_assets_bundled=False,network_runtime=False,build_version=build_version,product='MGO2MT EXCV',source_snapshot='public-source',source_snapshot_manifest_sha256=hashlib.sha256((source/'source-manifest.json').read_bytes()).hexdigest()),indent=2)+'\n',encoding='utf-8')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path);parser.add_argument('--build-version',default='')
    args=parser.parse_args();main(args.output,args.build_version)
