# Prüfumfang r23 – 16. November 2026

Die neuen Regressionen prüfen die feste Fünf-Meter-Grenze einschließlich
Höhenunterschied, freie Sicht, Bildschirm-/Kameraprüfung, gestreamte Identität,
aktuelle Wanteddaten, aktivierte Wanted-Liste und bestätigte Polizeifraktion.
Das native Zeichnen verwendet ausschließlich die tatsächlich sichtbaren
Spieler-Peds und beachtet die vorhandene Nametag-Freigabe.
Für die Wanted-Tabelle werden aktuelle SA-MP-IDs einschließlich der gültigen
ID 0 geprüft; fehlende Zuordnungen dürfen keine fremde/API-Account-ID anzeigen.

Die Ortungsregressionen behandeln den dynamischen Zielnamen, Start-/Erfolgs-
meldungen, neue Server-Checkpoints, beide möglichen Ereignisreihenfolgen,
vorhandene alte Marker, Duplikate, Zeitablauf sowie Rollen-/Sitzungswechsel.
Der Bezirk muss aus den Markerkoordinaten statt der eigenen Position stammen.
Ein eigener GPS-Checkpoint darf keine automatische Ortungsfunkmeldung auslösen.
Die RPC-Adapterprüfung muss die Koordinaten vollständig und mit gültigen
Werten lesen und unvollständige/ungültige Daten verwerfen.

Der Paketbau verlangt erfolgreiche Gesamttests und Release-Build, die neuen
Wantedlabel-/Ortungs-/Checkpoint-Prüfungen sowie den nach dem Helferbau
geprüften nativen Reconnect-Worker. Der PE-Nachweis prüft x86-DLL,
bytegleiche eingebettete Helfer, ZIP-Lizenz und die erhaltene v.0.1.2-Startmeldung.

Unveränderte Browser-, GPS-, Drogenoverlay- und Helferquellen werden gegen den
veröffentlichten r22-Commit 0b93ba47f646b8cfea8da59996a7f170c2d5dfa4
verglichen. Die neue Polizeifunktion samt Bridge-/Main-/Online-UI-Anbindung
ist ausdrücklich nicht als unveränderte Quelle behandelt. Die früheren
Browser-/GPS-Liveprüfungen werden für r23 nicht als erneut ausgeführt dargestellt.

Der fokussierte Forum-Quellcodeauszug wird aus dem aktuellen Baum neu gebildet
und vollständig mit SHA-256 abgeglichen. Die darin enthaltene Main-Anbindung
kann sich ändern, ohne dass eine neue Browser-Runtime installiert wird.

Der Desktop-Export liest die unveränderlichen veröffentlichten Git-Blobs und
prüft jede Datei. Persönliche Konfigurationen, Profile und Logs werden nicht
mitverteilt. Ein erfolgreicher neuer Sichttest im laufenden Spiel wird erst
nach einer tatsächlichen Beobachtung dokumentiert. Die automatischen Tests
allein werden nicht als beobachtetes GTA-Spielverhalten bezeichnet.
