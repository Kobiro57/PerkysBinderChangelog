# Prüfumfang r26 – 16. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die erweiterten
PoliceLocateTrackerTests und PoliceLocateBridgeTests prüfen die persönliche
Erfolgsmeldung, Zielnamen, aktuelle normale und Renn-Checkpoints, später
verfügbare Marker, doppelte Meldungen und Rollen-/Sitzungswechsel. Die Ausgabe
muss den Bezirk des Markers verwenden; die eigene Position ist kein Ersatz.

NearbyWantedLabelsTests prüfen die native Sichtbarkeitsbindung. Die zusätzliche
alte Fünf-Meter-Grenze wird nicht mehr verlangt. Der Produktionspfad verwendet
die tatsächliche native Namenszeichnung und damit deren Sichtbarkeit und
Reichweite, einschließlich Spielern im Fahrzeug. Die freigegebene serverseitige
Namensanzeige wird nicht verändert. Ein sichtbares Zusatzlabel ohne nativen
Nametag ist nicht vorgesehen. Die übrigen Binder-Regressionssuiten gelten weiter.

Der PE-Nachweis prüft die gebaute x86-ASI, ihre eingebetteten Helfer und die
erhaltene Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Browserquellen, Forum-Tastaturfix, Drogenzeiten und gepinnte Runtime
werden gegen r25 abgeglichen. FORUM-KEYBOARD-TEST.json bleibt der unveränderte
historische Browsernachweis aus r24; er wird nicht als neuer r26-Test ausgegeben.

Ein r26-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r26-validation.json. Der exportierte Quellcode
und die ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket.
