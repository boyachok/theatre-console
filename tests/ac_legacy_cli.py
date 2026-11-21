#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
script=(root/'deploy').read_text();start=script.index('check_mac(){');end=script.index('\n}\ncase "$ACTION"',start)+2
function=script[start:end]
for mac,expected,ok in [('C0:4E:30:29:F4:8C','',True),('30:30:F9:32:02:30','30:30:F9:32:02:30',False),('A0:85:E3:E2:BC:D4','',False),('30:30:F9:1B:FD:6C','',False)]:
 body='set -e\nROLE=slave\nAC_PROFILE=legacy082\nEXPECTED_MAC="'+expected+'"\n'+function+'\ncheck_mac "'+mac+'"\n'
 p=subprocess.run(['bash','-c',body],capture_output=True,text=True);assert (p.returncode==0)==ok,(mac,p.stdout,p.stderr)
for profile,args in [('legacy082',['build','master']),('bad-profile',['build','slave']),('legacy082',['slave','/dev/not-a-real-port'])]:
 env={**os.environ,'AC_PROFILE':profile};p=subprocess.run(['bash','./deploy',*args],cwd=root,env=env,capture_output=True,text=True);assert p.returncode!=0 and 'FQBN'not in p.stdout,(args,p.stdout,p.stderr)
print('AC082 CLI guards: onlyconsole-slave profile, onlyfixture6 MAC even with explicit wrong EXPECTED_MAC, USBfull-layout refused; no HTTP/USB/device commands executed')
