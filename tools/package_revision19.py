"""Package the tested r19 ASI and record only checks actually run for this release."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess,zipfile
R=Path(__file__).resolve().parents[1]
PREVIOUS='b3ca24e43a8603d98215ca6b559adad16f3d41fe'

def check(ok,why):
    if not ok:raise ValueError(why)

def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    tag='v0.1.1-r19';package=R/'dist/NLBinder-0.1.1-r19';archive=R/'dist/NLBinder-v0.1.1-KOBiRO-r19.zip'
    check(not package.exists() and not archive.exists(),'Existing r19 package is preserved; do not overwrite')
    log_path=R/'build/revision19-tests.log'
    log=log_path.read_text(encoding='utf-8-sig',errors='replace')
    check(re.search(r'^Test exit code: 0\s*$',log,re.M) and '[FAIL]' not in log,'Full tests not passing')
    check('RELEASE_BUILD_EXIT_CODE=0' in (R/'build/revision19-build.log').read_text(errors='replace'),'Release build not passing')
    suites={}
    for suite in ['DrugUseTracker','R18Feature','R17Feature','R16Feature','DamageLogRegression','StartupStatsSync','CombatChatBridge']:
        found=re.search(suite+r'Tests: (\d+) checks, 0 failures',log)
        check(found is not None,'Missing passing suite '+suite);suites[suite]=int(found[1])
    for name,pattern in [('OnlineSettings',r'Online Settings: (\d+) passed, 0 failed'),
                         ('OnlineOverlayRenderer',r'OnlineOverlayRendererTests: (\d+) passed, 0 failed')]:
        found=re.search(pattern,log);check(found is not None,'Missing passing suite '+name);suites[name]=int(found[1])
    artifact=json.loads((R/'build/revision19-artifact.json').read_text(encoding='utf-8'))
    asi=R/'build/Release/NLBinder.asi'
    check(artifact['revision']==tag and sha(asi)==artifact['sha256'] and artifact['embedded_helper_matches']
          and artifact['embedded_browser_setup_matches'],'Artifact proof stale')
    check(artifact['embedded_browser_setup_sha256']==sha(R/'build/BrowserSetup.exe'),'Installer proof stale')
    check('libcef.dll' not in [x.lower() for x in artifact['imports']],'CEF must remain outside GTA')
    worker_path=R/'build/reconnect-worker-tests/result.json';worker=json.loads(worker_path.read_text(encoding='utf-8'))
    check(worker['failures']==0 and worker['checks']>=83 and worker['nova_cases']==6,'Native reconnect regression failed')
    check(worker_path.stat().st_mtime>=(R/'build/ReconnectHelper.exe').stat().st_mtime,'Reconnect test predates helper build')
    lock=json.loads((R/'res/browser-runtime.json').read_text(encoding='utf-8'))
    check(lock['archive_sha256']=='01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45',
          'Browser runtime pin changed')
    unchanged=['browser/ForumBrowserHost.cpp','src/ForumBrowser.cpp','src/GpsGameAdapter.cpp','src/GpsManager.cpp',
               'tools/BrowserSetup.cpp','tools/ReconnectHelper.cpp','include/BrowserSetupProtocol.h',
               'include/BrowserRuntimeLock.h','res/browser-runtime.json']
    repo=R/'build/github-publication/repository'
    for name in unchanged:
        before=subprocess.check_output(['git','show',PREVIOUS+':'+name],cwd=repo)
        check((R/name).read_bytes().replace(b'\r\n',b'\n')==before.replace(b'\r\n',b'\n'),
              'Previously tested browser/GPS/helper source changed: '+name)
    history=(R/'docs/review/CHANGELOG-DE.md').read_text(encoding='utf-8')
    check(all(f'## {n}. ' in history for n in range(1,26)),'Full history sections 1-25 required')
    package.mkdir(parents=True)
    shutil.copyfile(asi,package/'NLBinder.asi')
    for source,target in [('docs/NLBinder-0.1.1-r19-DE.md','LIESMICH.md'),('docs/CHANGELOG-GESAMT.txt','CHANGELOG-GESAMT.txt'),
        ('third_party/miniz/LICENSE','MINIZ-LICENSE.txt'),('res/browser-runtime.json','BROWSER-RUNTIME.json'),
        ('build/reconnect-worker-tests/result.json','RECONNECT-TEST.json'),('docs/review/r19-PRUEFUNG-DE.md','PRUEFUMFANG.md')]:
        shutil.copyfile(R/source,package/target)
    public=dict(artifact,asi='NLBinder.asi')
    (package/'PRUEFBERICHT.json').write_text(json.dumps(public,indent=2)+'\n',encoding='utf-8')
    (package/'SHA256SUMS.txt').write_text(sha(asi)+'  NLBinder.asi\n',encoding='ascii')
    (package/'BUILD-INFO.txt').write_text(f'''NLBinder {tag}
NLBinder.asi (x86): {asi.stat().st_size} Bytes
SHA256: {sha(asi)}

Use Cooldown & Nachwirkung: lokale eigene Einnahmen, parallele Echtzeit-Timer
in Sekunden, eigene Overlay-Position, bestehender Move-Modus mit ESC-Speichern.
Ohne aktive Timer unsichtbar; im Move-Modus eine verschiebbare Vorschau.
Gesamttests und Release-Build Exitcode 0. DrugUseTracker: {suites['DrugUseTracker']} Checks.
Online Settings: {suites['OnlineSettings']} Checks. Overlay Renderer: {suites['OnlineOverlayRenderer']} Checks.
Nativer Reconnect-Helfer: {worker['checks']} Checks, {worker['nova_cases']} Nova-Faelle.
ASI-Ressourcen 300/301 stimmen mit den gebauten nativen Helfern ueberein.
Browser-/GPS-/Helferquellen und Browsermanifest sind gegen r18 unveraendert geprueft.
Die frueheren Browser-Download-/GPS-Tests stehen getrennt unter docs/review/r18-validation.json.
Diese Integrationstests wurden fuer r19 nicht erneut ausgefuehrt. Kein r19-GTA-Live-Test behauptet.

Nur NLBinder.asi bei beendetem GTA ersetzen. Browserdateien weiterhin automatisch
unter Dokumente/GTA San Andreas User Files/NLBinder/Browser; kein Browserordner mitzusenden.
Browser-Download SHA256: {lock['archive_sha256']}
''',encoding='utf-8-sig')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as zipped:
        for path in sorted(package.iterdir()):zipped.write(path,package.name+'/'+path.name)
    with zipfile.ZipFile(archive) as zipped:
        check(zipped.testzip() is None,'ZIP integrity failed')
        check(hashlib.sha256(zipped.read(package.name+'/NLBinder.asi')).hexdigest()==sha(asi),'Zipped ASI differs')
        check(not any('/Browser/' in x or x.lower().endswith('.log') for x in zipped.namelist()),'Private runtime data in package')
    proof={'revision':tag,'date':'2026-11-09','asi_bytes':asi.stat().st_size,'asi_sha256':sha(asi),
        'embedded_browser_setup_sha256':artifact['embedded_browser_setup_sha256'],
        'full_test_suite_exit_code':0,'full_test_log_sha256':sha(log_path),'release_build_exit_code':0,
        'focused_checks':suites,'native_reconnect_checks':worker['checks'],'native_reconnect_nova_cases':worker['nova_cases'],
        'unchanged_prior_tested_sources':unchanged,'unchanged_sources_baseline':PREVIOUS,
        'previous_browser_gps_validation':'r18-validation.json','browser_gps_native_integration_rerun':False,
        'live_gta_gameplay_test_performed':False,'changelog_sections':25,'changelog_sha256':sha(R/'docs/CHANGELOG-GESAMT.txt'),
        'archive':archive.name,'archive_bytes':archive.stat().st_size,'archive_sha256':sha(archive),'archive_integrity_checked':True,
        'browser_download_url':lock['download_url'],'browser_download_sha256':lock['archive_sha256'],'browser_download_bytes':lock['archive_bytes']}
    for target in ['build/revision19-package.json','docs/review/r19-validation.json']:
        (R/target).write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(proof,indent=2))

if __name__=='__main__':main()
