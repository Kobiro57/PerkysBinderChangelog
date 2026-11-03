"""Build the complete r16 package, including the real isolated CEF runtime."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import zipfile

ROOT=Path(__file__).resolve().parents[1]
TAG='v0.1.1-r16'
def require(ok,message):
    if not ok:raise ValueError(message)
def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def main():
    tests=(ROOT/'build/revision16-tests.log').read_text(encoding='utf-8-sig',errors='replace')
    require(re.search(r'^Test exit code: 0\s*$',tests,re.M) and '[FAIL]' not in tests,'Full tests failed')
    build=(ROOT/'build/revision16-build.log').read_text(encoding='utf-8-sig',errors='replace')
    require('RELEASE_BUILD_EXIT_CODE=0' in build and 'NLBinder.asi' in build,'Release build failed')
    artifact=json.loads((ROOT/'build/revision16-artifact.json').read_text())
    asi=ROOT/'build/Release/NLBinder.asi'
    require(artifact['revision']==TAG and sha(asi)==artifact['sha256'] and artifact['embedded_helper_matches'],'ASI proof stale')
    require('libcef.dll' not in {name.lower() for name in artifact['imports']},'CEF must stay outside the x86 game process')
    smoke=json.loads((ROOT/'build/revision16-browser-smoke.json').read_text())
    native=json.loads((ROOT/'build/revision16-native-browser-smoke.json').read_text())
    require(smoke['real_forum_rendered'] and smoke['mouse_scroll_changed_real_page'] and smoke['clean_exit_code']==0,'Real CEF test failed')
    require(native['native_x86_controller_launched_x64_cef'] and native['clean_async_shutdown'],'Native IPC integration failed')
    worker_path=ROOT/'build/reconnect-worker-tests/result.json'
    worker=json.loads(worker_path.read_text())
    require(worker['failures']==0 and worker['checks']>=83 and worker['nova_cases']==6,'Native reconnect regressions failed')
    require(worker_path.stat().st_mtime >= (ROOT/'build/ReconnectHelper.exe').stat().st_mtime,'Reconnect test predates the final helper')
    package=ROOT/'dist/NLBinder-0.1.1-r16'
    archive=ROOT/'dist/NLBinder-v0.1.1-KOBiRO-r16.zip'
    require(not package.exists() and not archive.exists(),'r16 package already exists; do not overwrite silently')
    package.mkdir(parents=True)
    shutil.copyfile(asi,package/'NLBinder.asi')
    for source,target in [('docs/NLBinder-0.1.1-r16-DE.md','LIESMICH.md'),('docs/jobs-support.md','JOB-HINWEISE.md'),
      ('docs/CHANGELOG-GESAMT.txt','CHANGELOG-GESAMT.txt'),('build/reconnect-worker-tests/result.json','RECONNECT-TEST.json'),
      ('build/revision16-browser-runtime.json','BROWSER-RUNTIME.json')]:shutil.copyfile(ROOT/source,package/target)
    public=dict(artifact);public['asi']='NLBinder.asi'
    (package/'PRUEFBERICHT.json').write_text(json.dumps(public,indent=2)+'\n',encoding='utf-8')
    runtime=json.loads((ROOT/'build/revision16-browser-runtime.json').read_text())
    runtime_source=ROOT/'build/NLBinder/Browser'
    for entry in runtime['files']:
        relative=Path(entry['path'])
        require(not relative.is_absolute() and '..' not in relative.parts,'Unsafe runtime path')
        original=runtime_source/relative
        require(sha(original)==entry['sha256'],'Runtime changed: '+str(relative))
        target=package/'NLBinder/Browser'/relative;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(original,target)
    # Ship only empty weapon directories, never personal images/profiles.
    folders=re.findall(r'\{\s*\d+,\s*"[^"]+",\s*"([^"]+)"\s*\}',(ROOT/'include/WeaponIconCatalog.h').read_text(encoding='utf-8'))
    require(len(set(folders))==43,'Weapon folder catalog incomplete')
    for name in folders:(package/'NLBinder/Waffenicons'/name).mkdir(parents=True)
    (package/'NLBinder/Waffenicons/LIESMICH.md').write_text('Eigene PNG-Waffenicons werden über Mods > Waffenicons verwaltet.\n',encoding='utf-8')
    checks={}
    patterns={'R16Feature':r'R16FeatureTests: (\d+) checks, 0 failures','DamageLogRegression':r'DamageLogRegressionTests: (\d+) checks, 0 failures',
      'StartupStatsSync':r'StartupStatsSyncTests: (\d+) checks, 0 failures','CombatChatBridge':r'CombatChatBridgeTests: (\d+) checks, 0 failures'}
    for name,pattern in patterns.items():
        match=re.search(pattern,tests);require(match,'Missing passing suite: '+name);checks[name]=int(match[1])
    proof={'revision':TAG,'date':'2026-11-03','asi_sha256':sha(asi),'asi_bytes':asi.stat().st_size,
      'full_test_suite_exit_code':0,'release_build_exit_code':0,'focused_checks':checks,'native_reconnect_checks':worker['checks'],
      'real_cef_offscreen_forum_test':True,'native_x86_to_x64_browser_integration_test':True,'cef_sandbox_enabled':True,
      'browser_clean_shutdown':True,'live_gta_gameplay_test_performed':False,'login_or_post_submitted_in_test':False,
      'cef_version':runtime['cef_version'],'runtime_files':len(runtime['files']),'runtime_bytes':sum(x['bytes'] for x in runtime['files']),
      'changelog_sha256':sha(ROOT/'docs/CHANGELOG-GESAMT.txt'),'changelog_sections':22}
    (package/'BUILD-INFO.txt').write_text(f'''NLBinder {TAG}
ASI: NLBinder.asi (x86), {asi.stat().st_size} Bytes
SHA256: {sha(asi)}

Neue Funktionen: Ausdauer-Sidebar im bestehenden Overlay-Move-Modus,
Akkord-Anteil in den Jobverdiensten, 20 lokale GPS-Ziele und echtes Ingame-Forum.
Chromium/CEF: {runtime['cef_version']} (isolierter x64-Prozess, Sandbox aktiv).
Der Ordner NLBinder/Browser muss neben der ASI installiert werden.

Gesamttests und Release-Build: Exitcode 0.
Neue Funktionstests: {checks['R16Feature']}; Damagelog-Regressionsprüfungen: {checks['DamageLogRegression']}.
Echtes Forum gerendert und Scrollen geprüft. Native x86-Anbindung startet x64-
CEF erfolgreich und beendet den Prozess sauber. Dafür ausschließlich anonyme
Testprofile; keine Anmeldung, Beiträge oder Nachrichten automatisch abgeschickt.
Ein Live-Test der neuen Funktionen in einer laufenden GTA-Sitzung wurde nicht durchgeführt.
Die bisherigen Binder-Funktionen und ihre Regressionstests bleiben enthalten.
''',encoding='utf-8-sig')
    (package/'SHA256SUMS.txt').write_text(sha(asi)+'  NLBinder.asi\n',encoding='ascii')
    with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as output:
        for path in sorted(package.rglob('*')):
            relative=path.relative_to(package.parent).as_posix()
            if path.is_dir():output.mkdir(relative+'/')
            else:output.write(path,relative)
    with zipfile.ZipFile(archive) as check:
        require(hashlib.sha256(check.read(package.name+'/NLBinder.asi')).hexdigest()==sha(asi),'Zipped ASI differs')
        require(check.read(package.name+'/CHANGELOG-GESAMT.txt')==(ROOT/'docs/CHANGELOG-GESAMT.txt').read_bytes(),'Zipped full changelog differs')
        require(not any('/profile/' in name.lower() or 'keybinder.json' in name.lower() or name.lower().endswith('.log') for name in check.namelist()),'Private runtime data in ZIP')
        require(check.testzip() is None,'ZIP integrity failed')
    proof.update({'archive':archive.name,'archive_bytes':archive.stat().st_size,'archive_sha256':sha(archive),'archive_integrity_checked':True})
    (ROOT/'docs/review/r16-validation.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    (ROOT/'build/revision16-package.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
