# Prüfumfang r22 – 9. November 2026

Die neuen Regressionen prüfen die 28 festen Flaggenkoordinaten, ausgeschalteten
Anfangszustand, berechtigtes Umschalten, Entzug der Freigabe, bestehende
Quick-Map-Projektion sowie unveränderte fremde Kartenmarker. Die verwendete
Bildressource ist das bereits vorhandene rote ENEMYATTACK-Flaggensymbol aus
beiden eingebetteten Radar-Paketen.

Die Fraktionsprüfungen behandeln die eigene Identität, bestätigte und fehlende
Fraktion, RCB sowie SAPD-/LSPD-Aliase, Fraktionswerte aus eigenen Statistiken
und Abweisung fremder Statistiken. Zusätzliche reine Tests behandeln frische
Online-Zuordnung, Stats-Fallback und Ablehnung widersprüchlicher bekannter Rollen. Negative /speed-Fälle dürfen weder den
Wantedkatalog öffnen noch Dialogantworten auslösen; positive Fälle erhalten den
bisherigen automatischen Ablauf. Neue Claims-Markierungen entstehen nur im
Quick-Map-Zeichenpfad und nicht über GTA-/Server-Radar- oder Checkpoint-APIs.

Der Paketbau verlangt erfolgreiche Gesamttests und Release-Build, zusätzliche
Quick-Map- und Statistikparser-Prüfungen sowie den nach dem Helferbau geprüften
nativen Reconnect-Worker. Der PE-Nachweis prüft x86-DLL, eingebettete Helfer auf
Bytegleichheit, ZIP-Lizenz und die erhaltene v.0.1.2-Startmeldung.

Unveränderte Browser-, GPS-, Drogenoverlay- und Helferquellen werden gegen den
veröffentlichten r21-Commit c7b4cb0a55d80cc7a32582cf9d08ed4bb8566f44
verglichen. Die neue Fraktions-/Speed-/Quick-Map-Integration ist ausdrücklich
nicht als unveränderte Quelle behandelt. Die frühere aufwendige Browser-
Download-/Liveprüfung wird für r22 nicht als erneut ausgeführt dargestellt.

Der fokussierte Forum-Quellcodeauszug wird aus dem aktuellen Quellbaum neu
gebildet und vollständig mit seinen SHA-256-Werten abgeglichen. Die Main-Datei
enthält nun auch die neue Befehlsanbindung; dies ist eine Quelländerung des
Prüfauszugs, kein neues Browser-Runtimepaket.

Der neue Desktop-Ordner wird aus den unveränderlichen veröffentlichten Git-Blobs
gelesen und dateiweise geprüft. Persönliche Konfigurationen, Profile und Logs
werden nicht mitverteilt. Ein erfolgreicher neuer Sichttest im laufenden Spiel
wird erst nach einer entsprechenden tatsächlichen Beobachtung dokumentiert.
