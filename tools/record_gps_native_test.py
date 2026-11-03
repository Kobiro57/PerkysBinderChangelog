"""Run the isolated native GPS adapter fixture and record its result."""
from pathlib import Path
import json,re,subprocess
R=Path(__file__).resolve().parents[1]
p=subprocess.run([str(R/'build/GpsNativeAdapterTests.exe')],cwd=R,capture_output=True,text=True)
print(p.stdout)
match=re.search(r'GpsNativeAdapterTests: (\d+) checks, 0 failures',p.stdout)
if p.returncode or not match:raise SystemExit(p.returncode or 2)
(R/'build/revision18-gps-native.json').write_text(json.dumps({'checks':int(match[1]),'failures':0,
    'production_adapter_code':True,'gta_structures_with_replaced_engine_calls':True,'world_checkpoint_and_radar_checked':True,
    'live_gta_gameplay_test':False},indent=2)+'\n',encoding='utf-8')
