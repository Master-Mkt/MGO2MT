"""Build with a case-normalized process environment (MSBuild rejects Path/PATH duplicates)."""
import os,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
env={key.upper():value for key,value in os.environ.items()}
steps=[('configure',['cmake','--fresh','--preset','windows-x64']),
       ('build',['cmake','--build','--preset','windows-x64']),
       ('test',['ctest','--preset','windows-x64'])]
(ROOT/'outputs').mkdir(exist_ok=True)
for name,cmd in steps:
    result=subprocess.run(cmd,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (ROOT/'outputs'/f'{name}.log').write_bytes(result.stdout)
    print(result.stdout.decode('utf-8',errors='replace'),flush=True)
    if result.returncode: sys.exit(result.returncode)
