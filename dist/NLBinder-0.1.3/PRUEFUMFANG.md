# Prüfumfang v0.1.3 – 17. November 2026

Ein neuer vollständiger Testlauf und der aktuelle Release-Build müssen bestehen.
Die Spray-Tests prüfen bearbeitbare Punkte, lokale Speicherung und Laden,
Kapazität bis zu 340 Punkten, bedarfsgerechte Seiten mit 34 Punkten,
Entfernungssortierung, Berechtigungen und unveränderte normale GPS-Speicherplätze.
SprayCatalogTests prüfen außerdem das dauerhafte Entfernen von Standardpunkten
und das Laden einer absichtlich leeren Liste ohne erneute Standardbefüllung.
SprayAccessTests prüfen den bestätigten Zugriff von Fraktionen und Gangs sowie
den Ausschluss von Zivilisten. Die bisherigen Polizei- und RCB-Prüfungen anderer
Funktionen bleiben maßgeblich. WaypointTourTests prüfen das nächste unbesuchte
Ziel, Ankunftsübergänge, Beenden und den sicheren Wechsel zwischen Navigationen.
Die bestehenden Regressionssuiten für Kampf, Schadenslog, Zähler, Statistiken,
Jobskills, Wanted-Labels, GPS, Browser und übrige Funktionen gelten weiter.

Der PE-Nachweis prüft die x86-ASI und ihre tatsächliche v0.1.3-Startmeldung sowie
die eingebetteten Helfer. Der aktuelle Reconnect-Helfer wird mit lokalen
Fixture-Prozessen geprüft. Die frühere automatische Ortung darf weder in den
aktiven Quellen noch als [ORTUNG]-Vorlage in der gebauten ASI vorhanden sein.

Der unveränderte CEF-Eingabenachweis aus r24 bleibt ausdrücklich historisch.
Ein erneuter Browser- oder GTA-Livetest wird nicht behauptet. Tatsächliche
Ergebnisse stehen in r30-validation.json. Persönliche Konfiguration, GPS- und
Spray-Dateien, Logs und Zugangsdaten werden nicht veröffentlicht. Die Desktop-
Quellen und die ASI werden gegen den veröffentlichten Commit geprüft.
