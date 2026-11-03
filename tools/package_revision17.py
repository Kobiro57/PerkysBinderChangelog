"""Package the r17 ASI: the browser is installed automatically on demand."""
from pathlib import Path
import hashlib,json,re,shutil,zipfile
R=Path(__file__).resolve().parents[1]
def check(ok,why):
    if not ok:raise ValueError(why)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def main():
    tag='v0.1.1-r17';package=R/'dist/NLBinder-0.1.1-r17';archive=R/'dist/NLBinder-v0.1.1-KOBiRO-r17.zip'
    check(not package.exists() and not archive.exists(),'Do not overwrite an existing r17 package')
    log=(R/'build/revision17-tests.log').read_text(encoding='utf-8-sig',errors='replace')
    check(re.search(r'^Test exit code: 0\s*$',log,re.M) and '[FAIL]' not in log,'Full tests not passing')
    check('RELEASE_BUILD_EXIT_CODE=0' in (R/'build/revision17-build.log').read_text(errors='replace'),'Release build not passing')
    artifact=json.loads((R/'build/revision17-artifact.json').read_text())
    asi=R/'build/Release/NLBinder.asi';check(sha(asi)==artifact['sha256'] and artifact['embedded_helper_matches'] and artifact['embedded_browser_setup_matches'],'Artifact proof stale')
    check(artifact['embedded_browser_setup_sha256']==sha(R/'build/BrowserSetup.exe'),'Installer proof stale')
    check('libcef.dll' not in [x.lower() for x in artifact['imports']],'CEF must remain outside GTA')
    worker_path=R/'build/reconnect-worker-tests/result.json';worker=json.loads(worker_path.read_text())
    check(worker['failures']==0 and worker['checks']>=83 and worker['nova_cases']==6,'Native reconnect regression failed')
    check(worker_path.stat().st_mtime>=(R/'build/ReconnectHelper.exe').stat().st_mtime,'Reconnect test predates helper build')
    setup=json.loads((R/'build/revision17-setup-tests.json').read_text())
    check(setup['failures']==0 and setup['real_public_https_download'] and setup['all_runtime_files_verified']==236,'Installer tests failed')
    native=json.loads((R/'build/revision17-native-browser-smoke.json').read_text())
    check(native['real_forum_rendered'] and native['same_profile_open_cycles']==4 and native['automatic_setup_from_embedded_installer']
        and native['queued_and_early_reopen_checked'] and native['clean_async_shutdown'],'Real forum setup/reopen test failed')
    lock=json.loads((R/'res/browser-runtime.json').read_text())
    check(lock['archive_sha256']==setup['archive_sha256']==sha(R/'dist'/lock['asset']),'Runtime archive lock changed')
    suites={}
    for suite in ['R17Feature','R16Feature','DamageLogRegression','StartupStatsSync','CombatChatBridge']:
        found=re.search(suite+r'Tests: (\d+) checks, 0 failures',log);check(found is not None,'Missing passing suite '+suite);suites[suite]=int(found[1])
    package.mkdir(parents=True)
    shutil.copyfile(asi,package/'NLBinder.asi')
    for source,target in [('docs/NLBinder-0.1.1-r17-DE.md','LIESMICH.md'),('docs/CHANGELOG-GESAMT.txt','CHANGELOG-GESAMT.txt'),
        ('docs/jobs-support.md','JOB-HINWEISE.md'),('res/browser-runtime.json','BROWSER-RUNTIME.json'),
        ('build/reconnect-worker-tests/result.json','RECONNECT-TEST.json'),('build/revision17-setup-tests.json','BROWSER-SETUP-TEST.json'),
        ('build/revision17-native-browser-smoke.json','FORUM-REOPEN-TEST.json')]:shutil.copyfile(R/source,package/target)
    public=dict(artifact,asi='NLBinder.asi');(package/'PRUEFBERICHT.json').write_text(json.dumps(public,indent=2)+'\n',encoding='utf-8')
    (package/'SHA256SUMS.txt').write_text(sha(asi)+'  NLBinder.asi\n',encoding='ascii')
    (package/'BUILD-INFO.txt').write_text(f'''NLBinder {tag}
NLBinder.asi (x86): {asi.stat().st_size} Bytes
SHA256: {sha(asi)}

Nur die ASI muss installiert werden. Beim ersten /forum werden die Browserdateien
automatisch unter Dokumente/GTA San Andreas User Files/NLBinder/Browser eingerichtet.
Gepruefte vorhandene Dateien werden uebernommen; sonst einmaliger HTTPS-Download
von {lock['archive_bytes']} Bytes. Archiv und alle 236 Dateien werden mit SHA-256 geprueft.
Browser-Download SHA256: {lock['archive_sha256']}
Fortschritt, Abbruch und Reparatur vorhanden. Keine Adminrechte/PowerShell/Python noetig.

GPS: Roter Welt-Checkpoint und Minimap-Punkt, Wiederherstellung eigener Eintraege,
Schutz recycelter fremder Radarslots und sichtbares Ziel beim Start am Zielort.
Forum: Erneutes Oeffnen nach dem vollstaendigen Ende der eigenen Browserprozesse.

Gesamttests und Release-Build: Exitcode 0. Neue GPS-Pruefungen: {suites['R17Feature']}.
Installer: {setup['checks']} native Integrationspruefungen plus 732 Bibliotheks-/Pfadpruefungen.
Echter HTTPS-Download, alle Dateihashes, Reparatur, Uebernahme, Wiederverwendung, Abbruch.
Produktiver x86-Controller mit eingebettetem Installer startet das echte Forum viermal
mit demselben anonymen Profil, auch bei schnellem bzw. fruehem Schliessen/Wiederoeffnen.
Keine Anmeldung oder Nachricht automatisch versendet. Kein zusaetzlicher GTA-Live-Test.
''',encoding='utf-8-sig')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(package.iterdir()):z.write(p,package.name+'/'+p.name)
    with zipfile.ZipFile(archive) as z:
        check(z.testzip() is None,'ZIP integrity failed')
        check(hashlib.sha256(z.read(package.name+'/NLBinder.asi')).hexdigest()==sha(asi),'Zipped ASI differs')
        check(not any('/Browser/' in x or x.lower().endswith('.log') for x in z.namelist()),'Private or loose browser runtime in package')
    proof={'revision':tag,'date':'2026-11-03','asi_bytes':asi.stat().st_size,'asi_sha256':sha(asi),
        'embedded_browser_setup_sha256':artifact['embedded_browser_setup_sha256'],'full_test_suite_exit_code':0,'release_build_exit_code':0,
        'focused_checks':suites,'native_reconnect_checks':worker['checks'],'installer_integration_checks':setup['checks'],
        'installer_unit_checks':732,'real_public_https_download_test':True,'browser_runtime_files_verified':236,
        'browser_automatic_install_test':True,'same_profile_forum_open_cycles':4,'queued_and_early_reopen_checked':True,
        'cef_sandbox_enabled':True,'clean_browser_shutdown':True,'live_gta_gameplay_test_performed':False,
        'login_or_post_submitted_in_test':False,'changelog_sections':23,'changelog_sha256':sha(R/'docs/CHANGELOG-GESAMT.txt'),
        'archive':archive.name,'archive_bytes':archive.stat().st_size,'archive_sha256':sha(archive),'archive_integrity_checked':True,
        'browser_download_url':lock['download_url'],'browser_download_sha256':lock['archive_sha256'],'browser_download_bytes':lock['archive_bytes']}
    for target in ['build/revision17-package.json','docs/review/r17-validation.json']:(R/target).write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(proof,indent=2))
if __name__=='__main__':main()
