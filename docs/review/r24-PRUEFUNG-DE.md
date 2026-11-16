# Prüfumfang r24 – 16. November 2026

Die Tastaturregressionen prüfen die Weitergabe von Zeichen im geöffneten,
fokussierten Quick Forum, einschließlich der Sonderzeichen über AltGr.
Normale Texteingabe, die vorhandenen Kopieren-/Einfügen-Kombinationen,
Tastenfreigaben und das Beenden der Eingabe müssen erhalten bleiben.

Der Paketbau verlangt erfolgreiche Gesamttests und Release-Build sowie eine
erfolgreiche neue Suite `ForumKeyboardInputTests` und die Anbindung über
`ForumKeyboardRoutingTests`. Die bisherigen Prüfungen
für SAPD-/LSPD-Wanted-Labels, Spieler-IDs, Ortung und Server-Checkpoints müssen
ebenfalls weiterhin bestehen. Der PE-Nachweis prüft die x86-ASI, bytegleiche
eingebettete Helfer, Lizenz und erhaltene v.0.1.2-Startmeldung.

Der Tastaturfix liegt in der ASI. Der Browserhost und die 236 Dateien des
gepinnten CEF-Browserpakets werden gegen den vorherigen Stand verifiziert.
Die Browser-Runtime cef156-r1 und ihr Archiv bleiben unverändert. Die
veränderte `src/ForumBrowser.cpp` wird ausdrücklich nicht als unveränderte
Quelle behandelt. Die vorherigen Polizei-, GPS-, Zähler- und Overlayprüfungen
sind von den neuen Tastaturregressionen getrennt dokumentiert.

Ein zusätzlicher nativer Browser-Eingabetest verwendet den bestehenden
gepinnten CEF-Host mit einem isolierten anonymen Profil und synthetischen
Texten. Sein Ergebnis ist in `FORUM-KEYBOARD-TEST.json` im Release-Paket und
im r24-Prüfnachweis enthalten. Es wird kein Forumbeitrag abgeschickt.
Der Fall `legacy_altgr_characters` verwendet absichtlich die frühere fehlerhafte
Weiterleitung. Sein `passed: false` belegt die nachgestellte Ursache. Die
korrigierten Fälle müssen vollständig erfolgreich sein. Vor jeder Eingabe
werden der gezeichnete Browserinhalt, der Fokus und die Verarbeitung der
Tastenereignisse geprüft; ausbleibende Eingabe beim Start gilt nicht als
erfolgreich nachgestellter AltGr-Fehler.
Automatische Prüfungen und dieser native Test außerhalb von GTA werden nicht
als beobachteter Ingame-Sichttest bezeichnet. Ein neuer GTA-Live-Test wird
erst nach tatsächlicher Beobachtung dokumentiert.

Der fokussierte Forum-Quellcodeauszug wird aus den aktuellen eigenen Quellen
einschließlich der benötigten Tastaturanbindung neu erstellt und vollständig
mit SHA-256 abgeglichen. Der Desktop-Export liest die unveränderlichen
veröffentlichten Git-Blobs und prüft jede exportierte Datei. Persönliche
Konfigurationen, Profile und Logs werden nicht mitverteilt.
