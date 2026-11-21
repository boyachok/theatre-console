#!/usr/bin/env python3
"""Compare the selected legacy phase machine against the exact 0.8.2 source fixture."""
from pathlib import Path
import hashlib,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
fixture=root/'tests/fixtures/ac_phase_082.h'
assert hashlib.sha256(fixture.read_bytes()).hexdigest()=='a8146aec936a93d2c80e2cf8ff0ff7b518e4d7e24f3366ab13c588359d6935b8'
def phase(path):
 with tempfile.TemporaryDirectory(prefix='ac082-equivalence-')as temp:
  src=Path(temp)/'phase.cpp';src.write_text('#include "'+str(path)+'"\n')
  out=subprocess.check_output(['c++','-std=c++17','-DTHEATRE_AC_LEGACY_082=1','-E','-P',str(src)],text=True)
 start=re.search(r'namespace\s+Mains\s*\{',out)
 assert start
 return re.sub(r'\s+','',out[start.start():])
assert phase(fixture)==phase(root/'console/ac_phase.h'),'Legacy phase differs from 0.8.2'
print('AC082 profile: preprocessed Mains namespace is token-identical to exact0.8.2 fixture (filter, watchdog, acquisition, timing, authorization, faults); STOP driver serialization retained separately')
