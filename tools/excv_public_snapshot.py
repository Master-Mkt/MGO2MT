"""Generate the audited converter-only source set without changing analysis tools.

Explicit policy: retain bounded readers and serializers, omit historical CLI
entry points and private defaults, and allow only the user-selected local audio
 decoder process. Source snapshots are deterministic and contain no game data.
"""
from pathlib import Path
import ast,hashlib,json,re

KEEP_MAIN={'mgo2excv.py','convert_character_catalog.py','convert_stage_model.py'}
DROP_GLOBALS={
 'archive_inventory.py':{'SOURCE','LEGACY'},
 'stage_scene.py':{'REFERENCE'},
 'prepare_normal_ak102_audio.py':{'SOURCE'},
 'prepare_all_weapon_audio.py':{'SOURCE','SOCOM'},
 'prepare_weapon_extension_audio.py':{'SOURCE'},
}
BLOCKED_IMPORTS={'socket','requests','urllib','http','httpx','ftplib','webbrowser','importlib'}

def assignments(node):
    if isinstance(node,ast.Assign):return {n.id for target in node.targets for n in ast.walk(target) if isinstance(n,ast.Name)}
    if isinstance(node,ast.AnnAssign) and isinstance(node.target,ast.Name):return {node.target.id}
    return set()

def sanitize(name,raw):
    tree=ast.parse(raw,filename=name);removed=[];body=[]
    if name=='gwp.py':
        kept={'record'}
        for node in tree.body:
            if isinstance(node,ast.FunctionDef) and node.name in kept:body.append(node)
            elif isinstance(node,ast.Import):
                imports=[n for n in node.names if n.name=='hashlib']
                if imports:body.append(ast.Import(names=imports))
            elif isinstance(node,ast.ImportFrom) and node.module=='pathlib':body.append(node)
            else:removed.append({'line':node.lineno,'kind':type(node).__name__,'name':getattr(node,'name','')})
    else:
        for node in tree.body:
            drop=(isinstance(node,ast.FunctionDef) and node.name=='main' and name not in KEEP_MAIN)
            drop=drop or (isinstance(node,ast.If) and '__name__' in ast.unparse(node.test) and name!='mgo2excv.py')
            drop=drop or bool(assignments(node)&DROP_GLOBALS.get(name,set()))
            if drop:removed.append({'line':node.lineno,'kind':type(node).__name__,'name':getattr(node,'name',','.join(sorted(assignments(node))))});continue
            if isinstance(node,ast.Import):
                node.names=[n for n in node.names if n.name!='importlib.util']
                if not node.names:continue
            body.append(node)
    tree.body=body;ast.fix_missing_locations(tree)
    result='# Generated converter-only source; historical analysis entry points omitted.\n'+ast.unparse(tree)+'\n'
    audit(name,result)
    return result,removed

def audit(name,raw):
    tree=ast.parse(raw,filename=name);processes=[]
    for node in ast.walk(tree):
        if isinstance(node,ast.Constant) and isinstance(node.value,str):
            if re.search(r'[A-Za-z]:[/\\]',node.value):raise ValueError('Private absolute path in '+name+':'+str(node.lineno))
            if 'decrypt_stage_file' in node.value:raise ValueError('Private crypto dependency in '+name)
        imports=([a.name for a in node.names] if isinstance(node,ast.Import) else [node.module] if isinstance(node,ast.ImportFrom) and node.module else [])
        if any(v.split('.')[0] in BLOCKED_IMPORTS for v in imports):raise ValueError('Network/dynamic dependency in '+name)
        if isinstance(node,ast.Call):
            function=ast.unparse(node.func)
            if function in ('eval','exec','__import__','os.system','os.popen'):raise ValueError('Dynamic process/code call in '+name)
            if function.startswith('subprocess.'):
                if name!='excv_audio_resources.py' or function!='subprocess.Popen':raise ValueError('Unexpected process call in '+name+':'+function)
                processes.append(node.lineno)
    compile(tree,name,'exec')
    return {'no_absolute_paths':True,'no_network_imports':True,'no_dynamic_original_code':True,'local_decoder_process_lines':processes}

def snapshot(tools,destination,files):
    tools=Path(tools).resolve();destination=Path(destination).resolve()
    if destination.exists():raise ValueError('Snapshot destination must be new')
    todo=[n for n in files if n.endswith('.py')];seen={};removed={}
    while todo:
        name=todo.pop()
        if name in seen:continue
        raw=(tools/name).read_text(encoding='utf-8');sanitized,changes=sanitize(name,raw)
        seen[name]=sanitized;removed[name]=changes
        for node in ast.walk(ast.parse(sanitized)):
            imports=([a.name for a in node.names] if isinstance(node,ast.Import) else [node.module] if isinstance(node,ast.ImportFrom) and node.module else [])
            for module in imports:
                child=module.split('.')[0]+'.py'
                if (tools/child).is_file() and child not in seen:todo.append(child)
    destination.mkdir(parents=True)
    rows=[]
    for name in sorted(set(files)|set(seen)):
        raw=seen[name].encode('utf-8') if name in seen else (tools/name).read_bytes()
        (destination/name).write_bytes(raw)
        rows.append({'file':name,'size':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),
                     'source_sha256':hashlib.sha256((tools/name).read_bytes()).hexdigest(),
                     'omitted_analysis_nodes':removed.get(name,[]),
                     **({'audit':audit(name,seen[name])} if name in seen else {'kind':'bounded resource recipe metadata'})})
    report={'format':'MGO2MT.EXCV_PUBLIC_SOURCE.1','original_assets':False,'embedded_archive_keys':False,
            'network_runtime':False,'policy':'Converter-only AST snapshot; only selected local decoder may run; original analysis files preserved.', 'files':rows}
    (destination/'source-manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report
