#!/usr/bin/env python3
"""Embed the production React build as gzip assets in the application image."""
from pathlib import Path
import gzip
root = Path(__file__).resolve().parents[1]
files = sorted(p for p in (root / 'score-ui' / 'dist').rglob('*') if p.is_file())
out = ['#pragma once\n#include "control_web.h"\n']
assets = []
for i, path in enumerate(files):
    uri = '/' + path.relative_to(root / 'score-ui' / 'dist').as_posix()
    data = gzip.compress(path.read_bytes(), mtime=0)
    mime = {'.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.css': 'text/css'}.get(path.suffix, 'application/octet-stream')
    out.append(f'const uint8_t console_asset_{i}[] PROGMEM={{' + ','.join(str(v) for v in data) + '};\n')
    assets.append((uri, mime, i, len(data)))
out.append('inline bool serveConsoleAsset(ControlWebServer &web,const String &uri){\n')
for uri, mime, i, count in assets:
    out.append(f'if(uri=="{uri}"){{web.sendHeader("Content-Encoding","gzip");web.sendHeader("Cache-Control","no-store");web.send_P(200,"{mime}",(PGM_P)console_asset_{i},{count});return true;}}\n')
out.append('return false;}\n')
(root / 'console' / 'web_assets.h').write_text(''.join(out))
print(f'Embedded {len(assets)} assets ({sum(a[3] for a in assets)} compressed bytes).')
