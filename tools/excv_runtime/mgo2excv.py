# Generated converter-only source; historical analysis entry points omitted.
"""MGO2MTEXCV desktop entry point and local batch interface."""
from pathlib import Path
import argparse, contextlib, json, os, queue, sys, threading, traceback, uuid
from excv_core import VERSION, STAGES, discover, extract_stage, validate_output, save_json, file_record, configure_original_elf
from excv_stage import convert_stage
from excv_ui_resources import extract_ui_resources, verify_ui_resources
from excv_title_runtime import extract_title_runtime, verify_title_runtime
from excv_effect_resources import extract_effect_resources, verify_effect_resources
from excv_actor_resources import build_actor_resources, verify_actor_resources
from excv_character_resources import build_character_resources, verify_character_resources
from excv_audio_resources import extract_audio_resources, verify_audio_resources
from excv_gekko_resources import build_gekko_resources, verify_gekko_resources
from excv_ui_runtime import extract_ui_runtime, verify_ui_runtime
from excv_briefing_map import extract_briefing_map, verify_briefing_map
from excv_stage_runtime_batch import build_stage_data, verify_stage_data
from excv_native_resources import build_native_resources, verify_native_resources
from excv_runtime_assembly import assemble_runtime, verify_runtime, completed_module_files
from excv_install import install

def verify(folder):
    folder = Path(folder).resolve()
    if (folder / 'actor-result.json').is_file():
        return verify_actor_resources(folder)
    if (folder / 'character-result.json').is_file():
        return verify_character_resources(folder)
    if (folder / 'gekko-result.json').is_file():
        return verify_gekko_resources(folder)
    if (folder / 'native-result.json').is_file():
        return verify_native_resources(folder)
    if (folder / 'conversion-run.json').is_file():
        result = json.loads((folder / 'conversion-run.json').read_text(encoding='utf-8'))
        checked = []
        paths = ['stages/' + r['stage'] for r in result['stages'] if r['status'] != 'failed']
        paths.extend((k for k in ('ui', 'title-runtime') if (folder / k / 'manifest.json').is_file()))
        paths.extend((name for name, row in result.get('runtime', {}).items() if row.get('ready')))
        if result.get('assembled_runtime') is not None:
            paths.append('runtime')
        for relative in paths:
            if not (folder / relative).resolve().is_relative_to(folder):
                raise ValueError('Conversion path escape')
            value = verify(folder / relative)
            if value.get('passed') is False:
                raise ValueError('Verification failed: ' + relative)
            checked.append(relative)
        return dict(passed=bool(result.get('passed')), verified=checked)
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('format') == 'MGO2MTEXCV.LOCAL_UI.1':
        return verify_ui_resources(folder)
    if manifest.get('format') == 'MGO2MTEXCV.TITLE_RUNTIME.1':
        return verify_title_runtime(folder)
    if manifest.get('format') == 'MGO2MTEXCV.EFFECT_RESOURCES.1':
        return verify_effect_resources(folder)
    if manifest.get('format') == 'MGO2MTEXCV.AUDIO_RESOURCES.1':
        return verify_audio_resources(folder)
    if manifest.get('format') == 'MGO2MTEXCV.STAGE_DATA.1':
        return verify_stage_data(folder)
    if manifest.get('format') == 'MGO2MTEXCV.UI_RUNTIME.1':
        return verify_ui_runtime(folder)
    if manifest.get('format') == 'MGO2MTEXCV.BRIEFING_MAP.1':
        return verify_briefing_map(folder)
    if manifest.get('format') == 'MGO2MTEXCV.RUNTIME_ASSEMBLY.1':
        return verify_runtime(folder)
    if manifest.get('format') != 'MGO2MTEXCV.LOCAL_STAGE.1':
        raise ValueError('Unknown converted stage format')
    for row in manifest['files']:
        path = folder / row['path']
        resolved = path.resolve()
        if folder not in resolved.parents:
            raise ValueError('Manifest path escape')
        actual = file_record(path)
        if (actual['size'], actual['sha256']) != (row['size'], row['sha256']):
            raise ValueError('Converted file changed: ' + row['path'])
    return manifest

def run(source, output, selected=None, resume=False, progress=lambda _: None, cancel=lambda: False, include_ui=False, fonts_source=None, include_runtime=False, elf=None, decoder=None, mgs_source=None):
    configure_original_elf(elf)
    inventory = discover(source)
    output = validate_output(inventory['source'], output)
    available = {r['stage'] for r in inventory['stages']}
    stages = list(selected or [r['stage'] for r in inventory['stages']])
    if not stages or len(set(stages)) != len(stages) or any((s not in available for s in stages)):
        raise ValueError('指定ステージの原データが見つかりません。')
    output.mkdir(parents=True, exist_ok=True)
    result = dict(format='MGO2MTEXCV.LOCAL_CONVERSION.1', version=VERSION, source=inventory['source'], requested=stages, unavailable=inventory['missing'], stages=[], network_used=False, original_assets_in_executable=False)
    for index, stage in enumerate(stages):
        if cancel():
            raise InterruptedError('中止しました。')
        progress(f'{index + 1}/{len(stages)} {stage}')
        final = output / 'stages' / stage
        try:
            if final.exists():
                if not resume:
                    raise ValueError('既存の変換先は上書きしません。再開を指定してください: ' + str(final))
                manifest = verify(final)
                for row in manifest['sources']:
                    actual = file_record(Path(row['path']))
                    if actual != row:
                        raise ValueError('前回変換から原データが変更されています。別の出力先を選んでください。')
                result['stages'].append(dict(stage=stage, status='verified_existing', folder=str(final)))
                continue
            pending = output / 'working' / stage
            if pending.exists() and (not resume):
                raise ValueError('未完了の作業フォルダーがあります。再開を指定してください: ' + str(pending))
            work, extraction = extract_stage(inventory['source'], stage, pending / 'source', progress, cancel, resume)
            target = pending / ('converted-' + uuid.uuid4().hex[:12])
            target.mkdir()
            with (target / 'conversion.log').open('w', encoding='utf-8') as log, contextlib.redirect_stdout(log):
                summary = convert_stage(stage, work, target, progress, cancel)
            if cancel():
                raise InterruptedError('中止しました。')
            rows = []
            for path in sorted(target.rglob('*')):
                if path.is_file():
                    item = file_record(path)
                    item['path'] = path.relative_to(target).as_posix()
                    rows.append(item)
            missing = summary['missing_textures']
            unsupported = summary['unsupported_meshes']
            status = 'converted_with_notes' if missing or unsupported else 'converted'
            manifest = dict(format='MGO2MTEXCV.LOCAL_STAGE.1', version=VERSION, stage=stage, status=status, sources=[r['source'] for r in extraction['records']] + [r['source'] for r in summary.get('dependencies', [])], files=rows, missing_texture_count=len(missing), unsupported_mesh_count=len(unsupported), selection_policy=summary['selection']['policy'], originals_unchanged=True)
            save_json(target / 'manifest.json', manifest)
            verify(target)
            final.parent.mkdir(exist_ok=True)
            target.rename(final)
            result['stages'].append(dict(stage=stage, status=status, folder=str(final), files=len(rows) + 1, missing_textures=len(missing), unsupported_meshes=len(unsupported)))
        except InterruptedError:
            raise
        except Exception as error:
            result['stages'].append(dict(stage=stage, status='failed', error=str(error)))
            progress(stage + ' / ' + str(error))
        finally:
            save_json(output / 'conversion-run.json', result)
    result['passed'] = all((r['status'] != 'failed' for r in result['stages'])) and len(result['stages']) == len(stages)
    if include_ui:
        try:
            ui = extract_ui_resources(source, output / 'ui', fonts_source=fonts_source, progress=progress, cancel=cancel, resume=resume)
            result['ui'] = dict(folder=str(output / 'ui'), complete=ui['complete'], groups=ui['groups'], fonts=ui['fonts'], unsupported=ui['unsupported'])
            result['passed'] = result['passed'] and ui['complete']
            title = extract_title_runtime(source, output / 'title-runtime', progress=progress, cancel=cancel, resume=resume)
            result['title_runtime'] = dict(folder=str(output / 'title-runtime'), complete=title['complete'], assets=title['assets'], limitations=title['limitations'])
            result['passed'] = result['passed'] and title['complete']
        except InterruptedError:
            raise
        except Exception as error:
            result['ui'] = dict(error=str(error))
            result['passed'] = False
    if include_runtime:
        result['runtime'] = {}
        jobs = [('effects', lambda: extract_effect_resources(source, output / 'effects', progress=progress, cancel=cancel, resume=resume)), ('weapons', lambda: build_actor_resources(source, output / 'weapons', progress=progress, cancel=cancel, resume=resume)), ('characters', lambda: build_character_resources(source, output / 'characters', progress=progress, cancel=cancel, resume=resume)), ('audio', lambda: extract_audio_resources(source, output / 'audio', elf_path=elf, vgmstream=decoder, progress=progress, cancel=cancel, resume=resume)), ('gekko', lambda: build_gekko_resources(source, output / 'gekko', mgs_source=mgs_source, progress=progress, cancel=cancel, resume=resume)), ('stage-data', lambda: build_stage_data(source, output, output / 'stage-data', stages, elf_path=elf, progress=progress, cancel=cancel, resume=resume)), ('briefing-map', lambda: extract_briefing_map(source, output / 'briefing-map', progress=progress, cancel=cancel, resume=resume))]
        jobs.append(('native', lambda: build_native_resources(output / 'native', progress=progress, cancel=cancel, resume=resume)))
        if include_ui:
            jobs.append(('ui-runtime', lambda: extract_ui_runtime(source, output / 'ui', output / 'ui-runtime', progress=progress, cancel=cancel, resume=resume)))
        for name, job in jobs:
            try:
                value = job()
                ready = value.get('complete', value.get('weapon_complete', value.get('banks_complete', False)))
                missing = value.get('missing', [])
                outside = [r for r in missing if name == 'weapons' and r.get('kind', '').startswith('character/')]
                missing = [r for r in missing if r not in outside]
                if name == 'native':
                    outside = missing
                    missing = []
                ready = ready and (not missing) and (not value.get('source_errors'))
                partial = not ready and (not value.get('source_errors')) and completed_module_files(output / name, value)
                result['runtime'][name] = dict(folder=str(output / name), ready=ready, verified_partial_assets=partial, assets=value.get('assets', []), missing=missing, separately_generated=outside, limitations=value.get('limitations', value.get('native_boundaries', [])), unsupported_models=value.get('unsupported_models', []))
                result['passed'] = result['passed'] and ready
            except InterruptedError:
                raise
            except Exception as error:
                result['runtime'][name] = dict(error=str(error))
                result['passed'] = False
    if include_runtime:
        modules = (['title-runtime'] if result.get('title_runtime', {}).get('complete') else []) + [name for name, row in result['runtime'].items() if row.get('ready') or row.get('verified_partial_assets')]
        missing_modules = [dict(module=name, reason=row.get('error') or '; '.join((str(x) for x in row.get('missing', []))) or 'Conversion is incomplete') for name, row in result['runtime'].items() if not row.get('ready')]
        if include_ui and (not result.get('title_runtime', {}).get('complete')):
            missing_modules.append(dict(module='title-runtime', reason='Title conversion incomplete'))
        if modules:
            result['assembled_runtime'] = assemble_runtime(output, modules, resume=resume, cancel=cancel, missing_modules=missing_modules)
    save_json(output / 'conversion-run.json', result)
    progress('完了' if result['passed'] else '一部を変換できませんでした。記録を確認してください。')
    return result

def gui(smoke_report=None):
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    window = tk.Tk()
    window.title('MGO2MT EXCV — ローカル資産変換')
    window.geometry('790x760')
    window.minsize(650, 620)
    controls = ttk.Frame(window, padding=(20, 10))
    controls.pack(side='bottom', fill='x')
    form = ttk.Frame(window)
    form.pack(fill='both', expand=True)
    canvas = tk.Canvas(form, highlightthickness=0)
    scroll = ttk.Scrollbar(form, orient='vertical', command=canvas.yview)
    scroll.pack(side='right', fill='y')
    canvas.pack(side='left', fill='both', expand=True)
    canvas.configure(yscrollcommand=scroll.set)
    frame = ttk.Frame(canvas, padding=20)
    item = canvas.create_window((0, 0), window=frame, anchor='nw')
    frame.bind('<Configure>', lambda _: canvas.configure(scrollregion=canvas.bbox('all')))
    canvas.bind('<Configure>', lambda e: canvas.itemconfigure(item, width=e.width))
    ttk.Label(frame, text='MGO2の原データから、ローカルで資産を作成', font=('Yu Gothic UI', 16, 'bold')).pack(anchor='w')
    ttk.Label(frame, text='原データの「o」フォルダーと保存先を選択してください。画像・モデル・音声をネットから取得しません。', wraplength=710).pack(anchor='w', pady=(6, 18))
    source = tk.StringVar()
    destination = tk.StringVar()
    fontdir = tk.StringVar()
    elfpath = tk.StringVar()
    decoderpath = tk.StringVar()
    mgspath = tk.StringVar()
    clientdir = tk.StringVar()
    hostdir = tk.StringVar()
    localdir = tk.StringVar()
    status = tk.StringVar(value='原データを選択してください。')
    events = queue.Queue()
    stop = threading.Event()
    busy = False

    def browse(var):
        path = filedialog.askdirectory(parent=window)
        if path:
            var.set(path)
    for title, var in [('原データの場所', source), ('変換したデータの保存先', destination), ('原日本語フォントの場所（任意）', fontdir)]:
        ttk.Label(frame, text=title).pack(anchor='w')
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=(3, 10))
        ttk.Entry(row, textvariable=var).pack(side='left', fill='x', expand=True)
        ttk.Button(row, text='選択…', command=lambda v=var: browse(v)).pack(side='right', padx=(8, 0))

    def browse_file(var):
        path = filedialog.askopenfilename(parent=window)
        if path:
            var.set(path)
    for title, var in [('ゲーム本体の原ELF（暗号化原データ・音声・ステージ用）', elfpath), ('音楽デコーダー vgmstream（BGM用）', decoderpath), ('MGS4のstage02.dat（月光用・後から指定可）', mgspath)]:
        ttk.Label(frame, text=title).pack(anchor='w')
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=(3, 8))
        ttk.Entry(row, textvariable=var).pack(side='left', fill='x', expand=True)
        ttk.Button(row, text='選択…', command=lambda v=var: browse_file(v)).pack(side='right', padx=(8, 0))
    resume = tk.BooleanVar(value=True)
    ttk.Checkbutton(frame, text='変換済みのデータを照合して再開する', variable=resume).pack(anchor='w')
    ui_resources = tk.BooleanVar(value=True)
    ttk.Checkbutton(frame, text='画面素材・武器と装備アイコン・原フォントも抽出する', variable=ui_resources).pack(anchor='w')
    runtime_resources = tk.BooleanVar(value=True)
    ttk.Checkbutton(frame, text='ゲーム用の武器・モーション・エフェクトなども生成する', variable=runtime_resources).pack(anchor='w')
    ttk.Separator(frame).pack(fill='x', pady=12)
    ttk.Label(frame, text='変換後にゲームへ適用', font=('Yu Gothic UI', 12, 'bold')).pack(anchor='w')
    ttk.Label(frame, text='配布ソフトのフォルダーを指定します。既存設定は保持し、変更前のdataをバックアップします。', wraplength=710).pack(anchor='w', pady=(4, 8))
    for title, var in [('MGO2MT.exe のフォルダー（クライアント）', clientdir), ('MGO2MTHOST.exe のフォルダー（HOST・任意）', hostdir), ('利用者の既存data（通信設定・動画・追加資産、任意）', localdir)]:
        ttk.Label(frame, text=title).pack(anchor='w')
        row = ttk.Frame(frame)
        row.pack(fill='x', pady=(3, 8))
        ttk.Entry(row, textvariable=var).pack(side='left', fill='x', expand=True)
        ttk.Button(row, text='選択…', command=lambda v=var: browse(v)).pack(side='right', padx=(8, 0))
    log = tk.Text(frame, height=8, wrap='word', state='disabled')
    log.pack(fill='both', expand=True, pady=12)
    ttk.Label(frame, textvariable=status, wraplength=710).pack(anchor='w')

    def start():
        nonlocal busy
        try:
            inventory = discover(source.get())
            validate_output(inventory['source'], destination.get())
            if not destination.get():
                raise ValueError('保存先を指定してください。')
        except Exception as e:
            messagebox.showerror('場所を確認してください', str(e), parent=window)
            return
        busy = True
        stop.clear()
        begin.config(state='disabled')
        apply_button.config(state='disabled')
        cancel.config(state='normal')
        inputs = (source.get(), destination.get(), resume.get(), ui_resources.get(), fontdir.get() or None, runtime_resources.get(), elfpath.get() or None, decoderpath.get() or None, mgspath.get() or None)

        def worker():
            try:
                events.put(('done', run(inputs[0], inputs[1], resume=inputs[2], progress=lambda s: events.put(('progress', s)), cancel=stop.is_set, include_ui=inputs[3], fonts_source=inputs[4], include_runtime=inputs[5], elf=inputs[6], decoder=inputs[7], mgs_source=inputs[8])))
            except Exception as e:
                events.put(('error', str(e)))
        threading.Thread(target=worker, daemon=True).start()

    def apply_runtime():
        nonlocal busy
        if not destination.get() or not (clientdir.get() or hostdir.get()):
            messagebox.showerror('適用先を指定してください', '変換先と、クライアントまたはHOSTのフォルダーを指定してください。', parent=window)
            return
        busy = True
        stop.clear()
        begin.config(state='disabled')
        apply_button.config(state='disabled')
        cancel.config(state='normal')
        source_runtime = destination.get()
        selected = [(role, folder) for role, folder in [('client', clientdir.get()), ('host', hostdir.get())] if folder]
        local = localdir.get() or None

        def worker():
            try:
                results = {}
                for role, folder in selected:
                    results[role] = install(source_runtime, folder, role, local_data=local, progress=lambda text: events.put(('progress', text)), cancel=stop.is_set)
                events.put(('installed', results))
            except Exception as error:
                events.put(('error', str(error)))
        threading.Thread(target=worker, daemon=True).start()
    begin = ttk.Button(controls, text='変換を開始', command=start)
    begin.pack(side='right')
    apply_button = ttk.Button(controls, text='ゲームへ適用', command=apply_runtime)
    apply_button.pack(side='right', padx=8)
    cancel = ttk.Button(controls, text='中止', command=stop.set, state='disabled')
    cancel.pack(side='right', padx=8)

    def poll():
        nonlocal busy
        try:
            while True:
                kind, value = events.get_nowait()
                if kind == 'progress':
                    status.set(value)
                    log.config(state='normal')
                    log.insert('end', value + '\n')
                    log.see('end')
                    log.config(state='disabled')
                else:
                    busy = False
                    begin.config(state='normal')
                    apply_button.config(state='normal')
                    cancel.config(state='disabled')
                    if kind == 'error':
                        status.set(value)
                    elif kind == 'installed':
                        missing = [role + ': ' + row['path'] for role, result in value.items() for row in result['missing'] if not row['optional']]
                        status.set('適用完了。不足: ' + ', '.join(missing[:8]) if missing else '適用完了。必要ファイルが揃いました。ゲームを起動して確認してください。')
                        log.config(state='normal')
                        log.insert('end', json.dumps(value, ensure_ascii=False, indent=2) + '\n')
                        log.see('end')
                        log.config(state='disabled')
                    else:
                        missing = value.get('assembled_runtime', {}).get('missing', [])
                        if missing:
                            status.set('生成完了。追加指定が必要なファイル: ' + ', '.join((r['path'] for r in missing)))
                        else:
                            status.set(f"{len(value['stages'])}ステージの処理完了。保存先のconversion-run.jsonに結果を記録しました。" if value['passed'] else '変換できなかったデータがあります。記録を確認してください。')
        except queue.Empty:
            pass
        window.after(100, poll)

    def close():
        if busy:
            stop.set()
            status.set('停止を待っています。完了後に閉じてください。')
        else:
            window.destroy()
    window.protocol('WM_DELETE_WINDOW', close)
    poll()
    if smoke_report:
        window.update()

        def children(widget):
            return [child for item in widget.winfo_children() for child in [item, *children(item)]]
        entries = sum((isinstance(item, ttk.Entry) for item in children(window)))
        controls_laid_out = begin.winfo_width() > 1 and cancel.winfo_width() > 1 and (controls.winfo_height() > 1)
        report = dict(passed=entries == 9 and controls_laid_out and apply_button.winfo_ismapped(), entries=entries, tk_version=window.tk.call('info', 'patchlevel'), start_visible=bool(begin.winfo_ismapped()), cancel_visible=bool(cancel.winfo_ismapped()), controls_laid_out=controls_laid_out, scrollable=True, conversion_started=False, apply_visible=bool(apply_button.winfo_ismapped()), title=window.title())
        save_json(smoke_report, report)
        window.destroy()
        if not report['passed']:
            raise ValueError('GUI smoke failed')
    else:
        window.mainloop()

def main(argv=None):
    ap = argparse.ArgumentParser(description='MGO2MT EXCV local resource extractor/converter')
    ap.add_argument('--version', action='version', version='MGO2MT EXCV ' + VERSION)
    ap.add_argument('--source', type=Path)
    ap.add_argument('--output', type=Path)
    ap.add_argument('--stages', nargs='+', choices=STAGES)
    ap.add_argument('--scan', action='store_true')
    ap.add_argument('--resume', action='store_true')
    ap.add_argument('--report', type=Path)
    ap.add_argument('--verify', type=Path)
    ap.add_argument('--with-ui', action='store_true')
    ap.add_argument('--ui-only', action='store_true')
    ap.add_argument('--title-only', action='store_true')
    ap.add_argument('--fonts', type=Path)
    ap.add_argument('--with-runtime', action='store_true')
    ap.add_argument('--elf', type=Path)
    ap.add_argument('--decoder', type=Path)
    ap.add_argument('--mgs-source', type=Path)
    ap.add_argument('--smoke-gui', type=Path)
    ap.add_argument('--install', type=Path, help='Verified conversion/runtime folder to apply locally')
    ap.add_argument('--client', type=Path, help='Folder containing MGO2MT.exe')
    ap.add_argument('--host', type=Path, help='Folder containing MGO2MTHOST.exe')
    ap.add_argument('--local-data', type=Path, help='User-selected local data containing network.gnk, optional movie and supplementary motion/CNP resources')
    args = ap.parse_args(argv)
    if args.smoke_gui:
        gui(args.smoke_gui)
        return 0
    if args.install:
        if not args.client and (not args.host):
            ap.error('--install requires --client and/or --host')
        installed = {role: install(args.install, path, role, local_data=args.local_data) for role, path in [('client', args.client), ('host', args.host)] if path}
        result = dict(format='MGO2MTEXCV.APPLICATION.1', passed=True, applications=installed, resources_ready=all((v['resource_files_complete'] for v in installed.values())))
    elif args.client or args.host or args.local_data:
        ap.error('--client/--host/--local-data require --install')
    elif args.verify:
        result = verify(args.verify)
    elif args.scan:
        if not args.source:
            ap.error('--scan requires --source')
        result = discover(args.source)
    elif args.source or args.output:
        if not args.source or not args.output:
            ap.error('--source and --output are required together')
        if args.title_only or args.ui_only:
            configure_original_elf(args.elf)
        if args.title_only:
            result = extract_title_runtime(args.source, args.output / 'title-runtime', resume=args.resume)
        elif args.ui_only:
            result = extract_ui_resources(args.source, args.output / 'ui', fonts_source=args.fonts, resume=args.resume)
        else:
            result = run(args.source, args.output, args.stages, args.resume, include_ui=args.with_ui, fonts_source=args.fonts, include_runtime=args.with_runtime, elf=args.elf, decoder=args.decoder, mgs_source=args.mgs_source)
    else:
        gui()
        return 0
    if args.report:
        save_json(args.report, result)
    if sys.stdout is not None:
        print(json.dumps(result, ensure_ascii=True, indent=2))
    return 0 if result.get('passed', True) else 1
if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as e:
        if sys.stderr is not None:
            traceback.print_exc()
        raise SystemExit(1)
