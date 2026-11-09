# Prüfumfang r21 – 9. November 2026

Auslöser ist die tatsächliche vom Nutzer gemeldete Zeile
`*** KOBiRO nimmt Heroin zu sich.`. Der bisherige Parser erwartete den Namen
direkt am Zeilenanfang. Die zusätzlichen Regressionen prüfen genau diese Zeile
mit 120/60 Sekunden sowie alle Drogen mit und ohne Präfix, dynamische eigene
Namen, fremde Namen, Namensvorsilben, Chat-Zitate, Farben, Zeitstempel und
einmalige Bereit-Ereignisse. Die vollständige vorhandene Testsuite bleibt Teil
des Release-Nachweises; die genaue Prüfzahl entsteht aus dem tatsächlichen Log.

Der Paketbau verlangt erfolgreiche Gesamttests, einen erfolgreichen Release-Build,
zusätzliche Drogenparser-Prüfungen und den nach dem finalen Helferbau ausgeführten
nativen Reconnect-Worker-Test. PE32/x86, exakte eingebettete Helfer und die unveränderte
gewünschte v0.1.2-Startmeldung werden an der fertigen ASI geprüft.

Browser-, GPS-, UI-, Versions- und Helferquellen werden gegen den veröffentlichten
r20-Commit 9a728abaf27b1784cf3b6a65beff4746add2120e auf unveränderte Bytes geprüft.
Der Drogenparser ist ausdrücklich eine beabsichtigte Änderung. Browser-Download-
und Browser-/GPS-Livetests werden nicht erneut als ausgeführt behauptet.

Alle 41 mitgelieferten Forum-Prüfauszugsdateien werden mit ihren aufgezeichneten
SHA-256-Werten verglichen. Vollständige Quellen und Texte des Desktop-Exports
werden aus dem veröffentlichten Git-Commit gelesen und geprüft.

Die Nutzerbeobachtung belegt den früheren Fehler. Eine erfolgreiche erneute
Einnahme im laufenden GTA ist erst nach einer tatsächlichen Rückmeldung ein
Live-Nachweis; die automatischen Parser-/UI-Tests werden davon getrennt.
