"""Show a compact result from the fixed r19 log paths."""
from pathlib import Path
import sys
root=Path(__file__).resolve().parents[1]
mode=sys.argv[1]
if mode not in {'tests','build'}:raise ValueError('Unexpected log kind')
lines=(root/('build/revision19-'+mode+'.log')).read_text(encoding='utf-8-sig',errors='replace').splitlines()
important=[line for line in lines if '[FAIL]' in line or 'error ' in line.lower() or 'DrugUseTrackerTests:' in line
           or 'Online Settings:' in line or 'OnlineOverlayRendererTests:' in line or 'Test exit code:' in line]
for line in (important if mode=='tests' else lines[-10:]):print(line)
