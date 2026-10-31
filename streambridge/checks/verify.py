#!/usr/bin/env python3
"""Run final checks serially, keeping timing tests free of competing test work."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[2]
logs=root/'streambridge/build/verification'
logs.mkdir(exist_ok=True)
commands=[
 ('streambridge', ['python3','-u','streambridge/checks/integration.py']),
 ('gtk', ['make','-C','streambridge','test-ui']),
 ('contest-ts', ['make','test-ts']),
 ('contest-udp', ['make','test-udp']),
 ('contest-windows-build', ['make','build/udp-sender.exe','build/test-udp-errors.exe']),
 ('contest-windows-udp', ['python3','checks/check_udp.py','--windows']),
]
failed=[]
for name,args in commands:
 print('Controle:',name,flush=True)
 with (logs/(name+'.log')).open('w') as log:
  p=subprocess.run(args,cwd=root,stdout=log,stderr=subprocess.STDOUT)
 print(name, 'OK' if p.returncode==0 else 'MISLUKT',flush=True)
 if p.returncode:failed.append(name)
print('Logs:',logs,flush=True)
sys.exit(bool(failed))
