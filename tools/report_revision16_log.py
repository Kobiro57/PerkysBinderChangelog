from pathlib import Path
import sys

mode=sys.argv[1]
if mode not in {'tests','build','browser-build','browser-smoke','native-smoke','repository'}:raise SystemExit(2)
path=Path(__file__).resolve().parents[1]/'build'/f'revision16-{mode}.log'
lines=path.read_text(encoding='utf-8-sig',errors='replace').splitlines()
important=[line for line in lines if '[FAIL]' in line or 'error C' in line or 'error L' in line or 'fatal error' in line]
for line in important[:12]:print(line)
for line in lines[-8:]:print(line)
