#!/usr/bin/env python3
"""Final checks after bridge-only changes; old contest regression logs are retained."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[2]
logs=root/'streambridge/build/verification'
failed=[]
for name,args in [('streambridge-fallback',['python3','-u','streambridge/checks/integration.py']),('gtk-fallback',['make','-C','streambridge','test-ui'])]:
 print('Controle:',name,flush=True)
 with (logs/(name+'.log')).open('w') as log:
  p=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT)
 print(name,'OK' if p.returncode==0 else 'MISLUKT',flush=True)
 if p.returncode:failed.append(name)
sys.exit(bool(failed))
