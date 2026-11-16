# Prüfumfang r25 – 16. November 2026

Die Gesamttests und der Release-Build müssen erfolgreich sein. Die erweiterten
CombatChatParserTests und CombatChatBridgeTests prüfen die TakeOver-Meldung mit
dynamischen Namen, Farben, Zeitstempeln, fremden Tätern, Spielerpräfixen,
Chatkanälen und Mehrfachbestätigungen. Die übrigen Binder-Prüfungen gelten weiter.

Der vorhandene Killzähler-/Killbind-Ablauf wird weiterverwendet; die neue
Nachrichtenfamilie hat einen eigenen Wert und nutzt die vorhandene Zuordnung.
Die ASI wird als x86-PE geprüft, einschließlich ihrer eingebetteten Helfer und
der erhaltenen Startmeldung. Der Reconnect-Helfer wird mit lokalen Testprozessen
geprüft. Die Browserquellen, der Sonderzeichenfix und die gepinnte Runtime werden
gegen r24 abgeglichen. FORUM-KEYBOARD-TEST.json ist der unveränderte historische
Browsernachweis aus r24; dieser Test wird für r25 nicht erneut behauptet.

Ein r25-GTA-Livetest wurde nicht durchgeführt. Die tatsächlichen automatischen
Ergebnisse stehen in r25-validation.json. Der exportierte Quellcode und die ASI
werden mit dem veröffentlichten Commit abgeglichen. Private Profile, Logs,
Spielstände und Zugangsdaten gehören nicht zum Paket.
