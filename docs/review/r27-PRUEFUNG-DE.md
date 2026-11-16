# Prüfumfang r27 – 17. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die
PoliceLocateBridgeTests prüfen die Zuordnung des aktuellen Detektivvorgangs
mit alten und neuen normalen beziehungsweise Renn-Checkpoints. Insbesondere
müssen gleichzeitig aktive Marker auseinandergehalten werden. Ein unverändert
alter Marker darf nicht als beliebiges Ziel verwendet werden.

Die Ortsauflösung muss Stadt und Bezirk aus der Zielmarkerposition bilden.
CheckpointLocationTests prüfen die native 32-Byte-Zonenansicht und die
Stadt-/Bezirksauswahl aus den Ziel-X/Y-Koordinaten. Dabei wird der kleinste
passende Bezirk gewählt; auch Marker in Dachhöhe bleiben über X/Y zugeordnet.
Die Marker-Zeitfolge vor und nach der Erfolgsmeldung wird nach der behobenen
Zuordnungsregression geprüft. Los Santos Market ist der gemeldete Fehlerfall.
Die eigene Spielerposition ist
kein Ersatz. Die r26-Regressionen für persönliche Erfolgsmeldung, verzögerte
Marker, doppelte Meldungen, Rollen- und Sitzungswechsel gelten weiter.
Die bisherigen Wanted-Labels und übrigen Binder-Regressionssuiten bleiben Teil
der Gesamttests.

Der PE-Nachweis prüft die gebaute x86-ASI, ihre eingebetteten Helfer und die
erhaltene Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Browserquellen, Forum-Tastaturfix, Drogenzeiten und gepinnte Runtime
werden gegen r26 abgeglichen. FORUM-KEYBOARD-TEST.json bleibt der unveränderte
historische Browsernachweis aus r24; er wird nicht als neuer r27-Test ausgegeben.

Ein r27-GTA-Livetest wird nicht aus automatischen Tests abgeleitet. Tatsächlich
durchgeführte Prüfungen stehen in r27-validation.json. Der exportierte Quellcode
und die ASI werden mit dem veröffentlichten Commit abgeglichen. Private Profile,
Logs, Spielstände und Zugangsdaten gehören nicht zum Paket.
