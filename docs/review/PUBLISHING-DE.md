# Veröffentlichung weiterer Binder-Updates

Ziel des Benutzers ist ausschließlich dieses Repository:

https://github.com/Kobiro57/PerkysBinderChangelog

Der Benutzer hat ausdrücklich angewiesen, alle hier erarbeiteten Änderungen
und spätere Binder-Updates dort für die Server-Projektleitung nachvollziehbar
zu veröffentlichen. Diese Anweisung ist keine Erlaubnis, andere Repositories
oder persönliche Laufzeitdaten zu veröffentlichen.

Für jedes Update:

1. Den bestehenden Repository-Stand lesen und erhalten.
2. Quelländerungen und eine konkrete deutschsprachige Änderungsübersicht
   ergänzen; ursprüngliche Urheberhinweise erhalten.
3. Passende Tests ausführen, Release bauen und ASI/enthaltenen Helfer prüfen.
4. Den neuen Binärstand mit SHA-256 und Testergebnissen dokumentieren.
5. Nur freigegebene Projektdateien übernehmen. Keine persönliche
   `keybinder.json`, `jobs_data.json`, `imgui.ini`, Chat-/Crashlogs, Zugangsdaten,
   verschachtelte `.git`-Verzeichnisse oder lokale Compiler-Zwischenstände.
6. Als neuen Commit veröffentlichen und den Betrachter-/Commitlink mitteilen.
   Bestehende Historie nicht durch einen erzwungenen Push ersetzen.

Die erste Veröffentlichung verwendet den read-only gelesenen Original-ZIP als
nachprüfbare Basis. Das Vorbereitungsskript
[prepare_github_review.py](../../tools/prepare_github_review.py) kopiert gefilterte
Snapshots unter `build/github-publication/`. Es meldet sich nicht bei GitHub an
und führt keinen Upload aus. Eine bestätigte GitHub-Verbindung wird für das
Veröffentlichen benötigt.

Bei einem privaten Repository kann die Projektleitung den Link nur mit einem
berechtigten GitHub-Konto lesen. Die Sichtbarkeit wird nicht stillschweigend
geändert.
