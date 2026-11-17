"""Package the verified v0.1.3 ASI, forum disclosure and inspectable installer sources."""
from pathlib import Path
import hashlib,json,re,shutil,subprocess,sys,zipfile
from revision30_scope import REMOVED_LOCATE_FILES, assert_locator_removed, assert_sprays_sources
R=Path(__file__).resolve().parents[1]
PREVIOUS='88ffe248874451babb4b13f5cfb7cfb6cfbf2685'
TAG='v0.1.3'

def check(ok,why):
    if not ok:raise ValueError(why)

def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def main():
    assert_sprays_sources(R)
    package=R/'dist/NLBinder-0.1.3'
    archive=R/'dist/NLBinder-v0.1.3-KOBiRO.zip'
    check(not package.exists() and not archive.exists(),'Existing v0.1.3 package is preserved; do not overwrite')
    log_path=R/'build/revision30-tests.log'
    log=log_path.read_text(encoding='utf-8-sig',errors='replace')
    check(re.search(r'^Test exit code: 0\s*$',log,re.M) and '[FAIL]' not in log,'Full tests not passing')
    check('[PASS] TestDetectiveMessagesAndCheckpointsDoNotSendAutomaticRadio' in log,
          'Missing passing regression for detective chat and checkpoints without automatic radio')
    check('RELEASE_BUILD_EXIT_CODE=0' in (R/'build/revision30-build.log').read_text(errors='replace'),'Release build not passing')
    suites={}
    for suite in ['WaypointTour','SprayAccess','SprayCatalog','SpraysNavigator','GpsTemporary','DrugUseTracker','R18Feature','R17Feature','R16Feature','DamageLogRegression','StartupStatsSync','CombatChatBridge','CombatChatParser','ServerStatsParser','LocalFactionAccess','NearbyWantedLabels','ForumKeyboardInput','ForumKeyboardRouting']:
        found=re.search(suite+r'Tests: (\d+) checks, 0 failures',log)
        check(found is not None,'Missing passing suite '+suite);suites[suite]=int(found[1])
    for name,pattern in [('OnlineSettings',r'Online Settings: (\d+) passed, 0 failed'),
                         ('OnlineOverlayRenderer',r'OnlineOverlayRendererTests: (\d+) passed, 0 failed'),
                         ('QuickMap',r'QuickMapTests: (\d+) passed, 0 failed')]:
        found=re.search(pattern,log);check(found is not None,'Missing passing suite '+name);suites[name]=int(found[1])
    check(suites['CombatChatParser']>190 and suites['CombatChatBridge']>238,'TakeOver regression checks missing')
    check(suites['DrugUseTracker']>=207,'Existing drug parser regressions missing')
    check(suites['QuickMap']>149,'Missing claims QuickMap regression checks')
    check(suites['ServerStatsParser']>156,'Missing own-faction parsing regression checks')
    check(suites['LocalFactionAccess']>=18,'Missing local/API faction reconciliation checks')
    check(suites['StartupStatsSync']>401,'Missing speed command and role-loss regression checks')
    for suite in ['WaypointTour','SprayAccess','SprayCatalog','SpraysNavigator','GpsTemporary','NearbyWantedLabels','ForumKeyboardInput','ForumKeyboardRouting']:
        check(suites[suite]>0,'Missing r30 regression checks: '+suite)
    artifact=json.loads((R/'build/revision30-artifact.json').read_text(encoding='utf-8'))
    asi=R/'build/Release/NLBinder.asi'
    check(artifact['revision']==TAG and sha(asi)==artifact['sha256'] and artifact['embedded_helper_matches']
          and artifact['embedded_browser_setup_matches'] and artifact['requested_startup_message_present'] and artifact.get('automatic_police_locate_removed')
          and artifact.get('automatic_locate_radio_template_absent'),'Artifact proof stale')
    check(artifact['embedded_browser_setup_sha256']==sha(R/'build/BrowserSetup.exe'),'Installer proof stale')
    check('libcef.dll' not in [x.lower() for x in artifact['imports']],'CEF must remain outside GTA')
    worker_path=R/'build/reconnect-worker-tests/result.json'
    worker=json.loads(worker_path.read_text(encoding='utf-8'))
    check(worker['failures']==0 and worker['checks']>=83 and worker['nova_cases']==6,'Native reconnect regression failed')
    check(worker_path.stat().st_mtime>=(R/'build/ReconnectHelper.exe').stat().st_mtime,'Reconnect test predates helper build')
    native_path=R/'dist/NLBinder-0.1.2-r24/FORUM-KEYBOARD-TEST.json'
    native=json.loads(native_path.read_text(encoding='utf-8'))
    check(native.get('passed') is True,'Native CEF forum keyboard test not passing')
    check(native.get('renderer_input_readiness_checked') is True
          and native.get('initial_offscreen_frames',0)>=2,
          'Native keyboard test did not establish renderer readiness and input focus')
    check(native.get('runtime_id')=='cef156-r1' and native.get('protocol_version')==1,
          'Native keyboard test did not use the pinned browser and existing IPC protocol')
    check(native.get('anonymous_isolated_profile') is True and native.get('login_or_post_submitted') is False
          and native.get('live_gta_gameplay_test_performed') is False and native.get('clean_exit_code')==0,
          'Native keyboard test scope or browser cleanup differs')
    check(native.get('fixed_cases',0)>0 and native.get('fixed_cases_passed')==native['fixed_cases']
          and native.get('actual_cef_ipc_keyboard_path') is True and native.get('local_dom_fixture') is True
          and native.get('network_blocked_during_fixture') is True
          and native.get('initial_page_loaded_read_only') is True
          and native.get('viewport_paint_barriers_checked') is True
          and native.get('user_clipboard_modified') is False,
          'Native keyboard editing cases or isolated fixture scope differ')
    check(native.get('legacy_regression_reproduced') is True,
          'Native keyboard test did not reproduce the reported AltGr regression with the old route')
    check(native.get('translator_sha256')==sha(R/'include/ForumKeyboardInput.h'),
          'Native keyboard proof predates the current translator or probe')
    before_native=subprocess.check_output(['git','show',PREVIOUS+':dist/NLBinder-0.1.2-r24/FORUM-KEYBOARD-TEST.json'],cwd=R/'build/github-publication/repository')
    check(native_path.read_bytes().replace(b'\r\n',b'\n')==before_native.replace(b'\r\n',b'\n'),
          'Historical r24 keyboard proof changed')
    lock=json.loads((R/'res/browser-runtime.json').read_text(encoding='utf-8'))
    check(lock['archive_sha256']=='01fc7f1d4dcec644a191e9c63da7088a73aa81a63c9a4c207001f7038a70ff45',
          'Browser runtime pin changed')
    check(len(lock['files'])==236 and sum(x['bytes'] for x in lock['files'])==429252102,'Runtime manifest file set changed')
    runtime_files={entry['path']:entry for entry in lock['files']}
    check(native.get('cef_host_sha256')==runtime_files['NLBinderBrowser.exe']['sha256']
          and native.get('cef_host_dll_sha256')==runtime_files['NLBinderBrowser.dll']['sha256'],
          'Native keyboard proof does not match the unchanged pinned browser host')
    unchanged=['src/ForumBrowser.cpp','include/ForumBrowser.h','include/ForumKeyboardInput.h','include/ForumBrowserProtocol.h','tests/ForumKeyboardProbe.cpp','tools/test_forum_keyboard.py','browser/ForumBrowserHost.cpp','src/GpsGameAdapter.cpp',
        'tools/BrowserSetup.cpp','tools/ReconnectHelper.cpp','include/BrowserSetupProtocol.h','include/BrowserRuntimeLock.h',
        'res/browser-runtime.json','include/DrugUseTracker.h',
        'src/OnlineOverlayRenderer.cpp','src/SettingsManager.cpp',
        'include/SettingsManager.h','src/MainWindowUI.cpp',
        'include/NearbyWantedLabels.h','src/NearbyWantedLabels.cpp','tests/NearbyWantedLabelsTests.cpp',
        'include/CombatChatParser.h','src/CombatChatParser.cpp','tests/CombatChatParserTests.cpp','tests/CombatChatBridgeTests.cpp']
    repo=R/'build/github-publication/repository'
    for name in unchanged:
        before=subprocess.check_output(['git','show',PREVIOUS+':'+name],cwd=repo)
        check((R/name).read_bytes().replace(b'\r\n',b'\n')==before.replace(b'\r\n',b'\n'),
              'Previously tested runtime source changed: '+name)
    history=(R/'docs/review/CHANGELOG-DE.md').read_text(encoding='utf-8')
    check(all(f'## {n}. ' in history for n in range(1,37)),'Full history sections 1-36 required')
    post=R/'docs/FORUM-CHANGELOG-v0.1.3.txt'
    check('0.1.3' in post.read_text(encoding='utf-8-sig') and 'perkys & edited by KOBiRO' in post.read_text(encoding='utf-8-sig'),
          'Postable forum changelog has the wrong version or credits')
    file_list=R/'docs/FORUM-DATEILISTE.txt'
    listed=file_list.read_text(encoding='utf-8-sig')
    for item in lock['files']:
        check(item['path'] in listed and item['sha256'] in listed,'Disclosed runtime file missing: '+item['path'])
    review_parent=R/'build/revision30-forum-review'
    review_parent.mkdir(parents=True,exist_ok=True)
    review=review_parent/'FORUM-INSTALLATION-QUELLCODE'
    subprocess.run([sys.executable,str(R/'tools/export_forum_installation_review.py'),
                    '--destination',str(review)],cwd=R,check=True)
    review_files={}
    for path in review.rglob('*'):
        check(not path.is_symlink() and not path.is_junction(),'Redirected focused review file')
        if path.is_file():
            check(path.suffix.lower() not in {'.asi','.exe','.dll','.log','.pyc','.obj'},'Unexpected binary/private focused file')
            review_files[path.relative_to(review).as_posix()]=sha(path)
    check('include/ForumKeyboardInput.h' in review_files and 'tools/BrowserSetup.cpp' in review_files and 'browser/ForumBrowserHost.cpp' in review_files
          and 'SHA256SUMS.txt' in review_files and 'SOURCE-MANIFEST.json' in review_files,
          'The focused own installer/browser sources or their checksums are missing')
    package.mkdir(parents=True)
    shutil.copyfile(asi,package/'NLBinder.asi')
    for source,target in [('docs/NLBinder-0.1.3-DE.md','LIESMICH.md'),('docs/CHANGELOG-GESAMT.txt','CHANGELOG-GESAMT.txt'),
        ('docs/FORUM-CHANGELOG-v0.1.3.txt','FORUM-CHANGELOG.txt'),('docs/FORUM-INSTALLATION-DE.md','FORUM-INSTALLATION.txt'),
        ('docs/FORUM-DATEILISTE.txt','FORUM-DATEILISTE.txt'),('third_party/miniz/LICENSE','MINIZ-LICENSE.txt'),
        ('res/browser-runtime.json','BROWSER-RUNTIME.json'),('build/reconnect-worker-tests/result.json','RECONNECT-TEST.json'),
        ('dist/NLBinder-0.1.2-r24/FORUM-KEYBOARD-TEST.json','FORUM-KEYBOARD-TEST.json'),
        ('docs/review/r30-PRUEFUNG-DE.md','PRUEFUMFANG.md')]:
        shutil.copyfile(R/source,package/target)
    shutil.copytree(review,package/'FORUM-INSTALLATION-QUELLCODE')
    public=dict(artifact,asi='NLBinder.asi')
    (package/'PRUEFBERICHT.json').write_text(json.dumps(public,indent=2)+'\n',encoding='utf-8')
    (package/'SHA256SUMS.txt').write_text(sha(asi)+'  NLBinder.asi\n',encoding='ascii')
    (package/'BUILD-INFO.txt').write_text(f'''NLBinder v.0.1.3 by perkys & edited by KOBiRO (r30)
NLBinder.asi (x86): {asi.stat().st_size} Bytes
SHA256: {sha(asi)}

Gewuenschte farbige Startmeldung ist im finalen PE enthalten.
Gesamttests und Release-Build: Exitcode 0. Nativer Reconnect-Worker: {worker['checks']} Checks.
ASI-Ressourcen 300/301 stimmen mit den gebauten nativen Helfern ueberein.
Browserhost, GPS-Spieladapter, Wanted-/Drogenoverlay und Browsermanifest sind gegen r29 unveraendert geprueft.
TakeOver-Killerkennung aus r25 beibehalten: eigener Taetername, bestehender Killtext und Zaehler.
Quick-Forum-Sonderzeichenkorrektur aus r24 beibehalten.
SAPD-/LSPD-Wantedlabels: nur zusammen mit dem tatsaechlich gezeichneten nativen Nametag, auch im Fahrzeug.
Reichweite und Sichtbarkeit folgen dem nativen Nametag; die fruehere eigene 5-Meter-Grenze entfaellt.
Vorhandene Nametag-Freigabe, Wantedlisten-Schalter und frische Wanteddaten erforderlich.
Wantedtabelle: aktueller SA-MP-Spieler-ID nach Name; keine API-Account-ID.
Automatische Polizeifunk-Ortung vollstaendig entfernt: kein Mitlesen der Ortungsmeldungen,
keine Ortungs-Checkpoint-Auswertung und keine automatische /r-Ortungsmeldung.
Die ausschliesslich dafuer verwendeten Quellen, Hooks, Zustaende und Tests sind entfernt.
Manuelle serverseitige Ortung und die normale GPS-/Quick-Map-Anzeige bleiben erhalten.
/sprays: bearbeitbare lokale Spray-Liste, anfangs 100 Standardpunkte, fuer bestaetigte Nicht-Zivilisten.
Fraktionen und Gangs koennen die Spray-Navigation verwenden; Zivilisten erhalten keinen Zugriff.
Entfernung ab aktueller Spielerposition dreidimensional berechnet; naechste Stelle gruen.
Punkte entfernen und am aktuellen Standort mit Namen hinzufuegen; bis zu 340 Punkte.
Bis zu zehn Seiten mit jeweils 34 Eintraegen; Auswahl startet die vorhandene rote GPS-Navigation.
Persoenliche Liste wird lokal unter Dokumente\\GTA San Andreas User Files\\NLBinder\\sprays.xml gespeichert, nicht veroeffentlicht.
Voruebergehende Spray-Ziele belegen keinen der 20 gespeicherten GPS-Plaetze.
/spraytour startet die Navigation zum naechsten unbesuchten Punkt der persoenlichen Spray-Liste.
Nach Checkpointankunft geht es zum naechsten unbesuchten Ziel; /sprayend beendet die Tour.
/claimstour navigiert RCB-Mitglieder durch die 28 Flaggenpunkte; /claimsend beendet diese Tour.
Die Touren steuern nur die Navigation. Spray- und Flaggenaktionen werden weiterhin im Spiel ausgefuehrt.
Fruehere Browser-/GPS-Pruefungen sind separat historisch dokumentiert.
Der unveraenderte historische Browser-Eingabetest aus r24 ist in FORUM-KEYBOARD-TEST.json enthalten.
Dieser Browser-Test wurde fuer r30 nicht erneut ausgefuehrt.
Er verwendet nur synthetische Texte in einem isolierten anonymen Profil, ohne Forumbeitrag.
Ein neuer r30-GTA-Live-Test wird nicht behauptet.

FORUM-INSTALLATION.txt beschreibt Einrichtung und Speicherorte. Die Dateiliste
enthaelt alle 236 festgelegten Runtime-Dateien. FORUM-INSTALLATION-QUELLCODE
enthaelt die eigenen Originalquellen und ihre SHA-256-Nachweise, keine privaten Profile.
FORUM-CHANGELOG.txt ist der fertige Text fuer die Veroeffentlichung im Forum.
Der vollstaendige Binder-Quellcode ist im neuen Desktop-Ordner und auf GitHub enthalten.
Der komplette Drittanbieter-Chromium-/CEF-Upstreamcode wird nicht als enthalten ausgegeben.

Nur NLBinder.asi bei beendetem GTA ersetzen. Browserstand unveraendert: cef156-r1.
Browser-Download SHA256: {lock['archive_sha256']}
''',encoding='utf-8-sig')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as zipped:
        for path in sorted(package.rglob('*')):
            if path.is_file():zipped.write(path,package.name+'/'+path.relative_to(package).as_posix())
    with zipfile.ZipFile(archive) as zipped:
        check(zipped.testzip() is None,'ZIP integrity failed')
        check(hashlib.sha256(zipped.read(package.name+'/NLBinder.asi')).hexdigest()==sha(asi),'Zipped ASI differs')
        for name,digest in review_files.items():
            check(hashlib.sha256(zipped.read(package.name+'/FORUM-INSTALLATION-QUELLCODE/'+name)).hexdigest()==digest,
                  'Zipped focused installer source differs: '+name)
        check(not any(x.lower().endswith(('.log','.dll','.exe','.obj','.pyc')) for x in zipped.namelist()),
              'Runtime binary or private data in package')
    proof={'revision':TAG,'date':'2026-11-17','asi_bytes':asi.stat().st_size,'asi_sha256':sha(asi),
        'version_and_credit':artifact['version_and_credit'],'requested_startup_message_present':True,
        'embedded_browser_setup_sha256':artifact['embedded_browser_setup_sha256'],
        'full_test_suite_exit_code':0,'full_test_log_sha256':sha(log_path),'release_build_exit_code':0,
        'focused_checks':suites,'wanted_label_native_draw_call_required':True,'wanted_label_follows_server_nametag_range':True,'wanted_label_in_vehicle_supported':True,
        'wanted_label_native_visibility_required':True,'wanted_table_current_samp_id':True,
        'sprays_navigation_default_coordinate_count':100,
        'sprays_navigation_editable':True,
        'sprays_navigation_deleted_defaults_persist':True,
        'sprays_navigation_empty_catalog_does_not_reseed':True,
        'sprays_navigation_controls_visible_in_resized_window':True,
        'sprays_navigation_local_persistence':'Documents/GTA San Andreas User Files/NLBinder/sprays.xml',
        'sprays_navigation_max_points':340,
        'sprays_navigation_noncivilian_role_required':True,
        'sprays_navigation_factions_and_gangs_allowed':True,
        'sprays_navigation_distance_sort':'ascending_3d_from_current_player',
        'sprays_navigation_nearest_highlight':'green',
        'sprays_navigation_rows_per_page':34,
        'sprays_navigation_max_pages':10,
        'sprays_navigation_existing_red_gps_checkpoint':True,
        'sprays_navigation_temporary_target_preserves_20_saved_slots':True,
        'spray_tour_commands':['/spraytour','/sprayend'],
        'claim_tour_commands':['/claimstour','/claimsend'],
        'claim_tour_rcb_only':True,'claim_tour_static_flag_count':28,
        'waypoint_tour_next_target':'nearest_unvisited_after_checkpoint_arrival',
        'waypoint_tours_navigation_only':True,
        'automatic_police_locate_removed':True,'automatic_locate_radio_template_absent':True,
        'detective_chat_and_normal_racing_checkpoints_send_no_automatic_radio_tested':True,
        'manual_server_detective_command_pass_through_tested':True,
        'removed_locator_source_files':list(REMOVED_LOCATE_FILES),
        'local_faction_guards_tested':True,'forum_keyboard_input_regression_tested':True,
        'forum_browser_runtime_changed':False,'native_forum_keyboard_test':native,'native_forum_keyboard_test_revision':'v0.1.2-r24','native_forum_keyboard_rerun':False,
        'native_reconnect_checks':worker['checks'],'native_reconnect_nova_cases':worker['nova_cases'],
        'unchanged_prior_tested_sources':unchanged,'unchanged_sources_baseline':PREVIOUS,
        'previous_browser_gps_validation':'r18-validation.json','browser_gps_native_integration_rerun':False,
        'live_gta_gameplay_test_performed':False,'changelog_sections':36,'changelog_sha256':sha(R/'docs/CHANGELOG-GESAMT.txt'),
        'forum_changelog_sha256':sha(post),'forum_installation_sha256':sha(R/'docs/FORUM-INSTALLATION-DE.md'),
        'forum_file_list_sha256':sha(file_list),'forum_runtime_files_disclosed':len(lock['files']),
        'forum_review_files':review_files,'forum_review_file_count':len(review_files),
        'archive':archive.name,'archive_bytes':archive.stat().st_size,'archive_sha256':sha(archive),'archive_integrity_checked':True,
        'browser_download_url':lock['download_url'],'browser_download_sha256':lock['archive_sha256'],'browser_download_bytes':lock['archive_bytes']}
    for target in ['build/revision30-package.json','docs/review/r30-validation.json']:
        (R/target).write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    compact=dict(proof,forum_review_files='Recorded '+str(len(review_files))+' verified source/document files')
    print(json.dumps(compact,indent=2))

if __name__=='__main__':main()
