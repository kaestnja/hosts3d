# Auftrag: Capture- und Sensor-Verwaltung

## Zielbild

Hosts3D soll Capture-Infrastruktur nicht nur lokal starten koennen, sondern perspektivisch auch vorbereiten, verwalten und pruefen. Dazu gehoeren zwei zusammenhaengende Aufgabenbereiche:

1. Verwaltung und Ausrollen der HSEN Sensoren.
2. Verwalten und Pruefen der Mirroring-Faehigkeiten von Netzwerkgeraeten.

Beide Bereiche sollen zunaechst beobachtend und assistierend gedacht werden. Verwaltende Aenderungen an entfernten Systemen, Switches oder Sensor-Hosts sind spaeter ausdruecklich Teil des Zielbilds, sollen aber nachvollziehbar, begrenzt und mit bewusst gewaehlten Rechten erfolgen.

## Teil 1: Allgemeine Capture- und Sensor-Verwaltung

### HSEN Sensoren verwalten und ausrollen

Ziel ist eine Inventar- und Betriebsansicht fuer Sensoren, die lokal oder remote Pakete erfassen und an Hosts3D liefern koennen.

Zu klaerende Punkte:

1. Welche Sensor-Instanzen gibt es?
2. Auf welchem Host laufen sie?
3. Welche Version von `hsen` ist installiert?
4. Welche Capture-Interfaces sind verfuegbar?
5. Welche Interfaces sind aktuell aktiv?
6. Wohin sendet der Sensor seine Daten?
7. Welche Rechte oder Capabilities fehlen fuer Capture?
8. Wie wird der Sensor gestartet: manuell, durch Hosts3D, per Service, per Script oder per externem Deployment?
9. Wann wurde der Sensor zuletzt gesehen?
10. Welche Fehler oder Warnungen meldet der Sensor?

Moegliche Software-Erweiterungen:

- Sensor-Inventar in `settings.ini` oder in einer eigenen Runtime-Datei verwalten.
- Lokalen HSEN Editor um Remote-Sensor-Eintraege erweitern.
- Statusfelder im Editor anzeigen: installed, reachable, running, stale, wrong_version, missing_capture_rights.
- Ein Deployment-Profil definieren: Zielhost, OS, Architektur, Installationspfad, Startmethode, Zieladresse von Hosts3D.
- Paket oder Archiv fuer Sensor-Deployment erzeugen.
- Optional einen "readiness check" anbieten, der ohne dauerhafte Aenderung prueft, ob Capture starten koennte.
- Start/Stop/Restart nur fuer Sensoren anbieten, deren Verwaltungsmodus explizit bekannt ist.

Betriebs- und Berechtigungsvorgaben:

- Passwoerter oder Tokens nicht unnoetig im Klartext in Logs schreiben.
- Keine implizite Remote-Installation ohne explizite Aktion.
- Lokale Capture-Rechte nach Least-Privilege vergeben: so wenig Rechte wie praktikabel, aber ausreichend fuer die gewuenschte Capture- oder Verwaltungsaufgabe.
- Beispiele: Linux `setcap` fuer reinen Capture-Betrieb bevorzugen, root oder Administratorrechte aber zulassen, wenn Installation, Service-Verwaltung oder Geraetekonfiguration sie tatsaechlich erfordern.
- Sensor-Kommunikation und Verwaltungszugriffe getrennt betrachten.
- Alle automatisch ermittelten Zustaende mit Zeitstempel und Quelle kennzeichnen.

### Mirroring-Faehigkeiten von Netzwerkgeraeten verwalten

Ziel ist eine Ansicht, ob und wie Netzwerkgeraete Capture-Traffic fuer HSEN Sensoren bereitstellen koennen. Dabei geht es zunaechst um Erkennung und Plausibilisierung; spaeter soll daraus auch bewusst gesteuerte Switch-Konfiguration entstehen koennen.

Zu klaerende Punkte:

1. Welche Netzwerkgeraete sind fuer Capture relevant?
2. Welcher Geraetetyp, Hersteller und Firmwarestand liegt vor?
3. Unterstuetzt das Geraet Port-Mirroring, SPAN, RSPAN, ERSPAN oder TAP-nahe Funktionen?
4. Ist Mirroring global aktiv?
5. Welche Zielports sind konfiguriert?
6. Welche Quellports, VLANs oder Sessions werden gespiegelt?
7. Wird Ingress, Egress oder beides gespiegelt?
8. Kann das Geraet mehrere Mirror-Sessions oder nur eine Session?
9. Welche Ports sind mit Sensoren, Uplinks oder Endgeraeten verbunden?
10. Welche Nachbarschaftsdaten aus LLDP, MAC-Tabellen, ARP/DNS/DHCP oder Asset-Daten koennen helfen?

Moegliche Software-Erweiterungen:

- Neues Datenmodell fuer "Capture Sources", "Capture Sensors" und "Mirror Providers".
- Im Local HSEN Editor einen Bereich "Capture vorbereiten" oder "Mirror Check" ergaenzen.
- Fuer ein ausgewaehltes Sensor-Interface anzeigen, welche Switchports als Mirror-Zielport in Frage kommen.
- Zunaechst lesende SNMP-Abfragen fuer bekannte Geraeteklassen als Diagnose-Script oder internes Modul bereitstellen.
- Ergebnisformat fuer Switch-Abfragen definieren, z. B. JSON mit sicheren Werten und abgeleiteten Korrelationen.
- Warnungen anzeigen, wenn ein Sensor aktiv ist, aber am Switch kein passendes Mirroring sichtbar ist.
- Warnungen anzeigen, wenn Mirroring aktiv ist, aber der Zielport nicht zum erwarteten Sensor passt.

Konkreter erster Umsetzungspfad:

1. Geraetespezifische lesende Diagnose als externes JSON-Tool bereitstellen.
2. JSON-Struktur gegen reale Switch-Ausgaben stabilisieren.
3. Parallel Beschaffungs- und Implementierungswege fuer SNMP pruefen: Net-SNMP-Binary, eigener reproduzierbarer Build, funktional gleichwertiges SNMP-CLI, Bibliothek oder separates Verwaltungs-CLI.
4. Danach im Local HSEN Editor nur eine leichte UI-Integration ergaenzen: "Mirror Check" ausfuehren, letzte JSON-Datei anzeigen oder zusammenfassen, Credentials nicht unnoetig dauerhaft in Hosts3D speichern.
5. Nach erfolgreicher Diagnosephase verwaltete Switch-Aenderungen planen, z. B. SNMP-SET, SSH-Konfiguration oder API-basierte Mirror-Port-Konfiguration.

Abgrenzung:

- SNMP-SET, SSH-Konfiguration oder API-basierte Switch-Aenderungen sind ein spaeterer, aber ausdruecklich gewuenschter Schritt.
- Hersteller-Private-MIBs muessen geraete- und firmwarebezogen validiert werden.
- Korrelationen zwischen MAC, IP und Hostname sind Hinweise, keine Beweise.

## Spezifische Geraeteauftraege

Geraetespezifische Recherche, OID-Listen, Validierungsschritte und konkrete Implementierungsplaene sollen in eigenen Dateien gepflegt werden. Dadurch bleibt diese Todo-Datei der allgemeine Arbeitsrahmen fuer Capture- und Sensor-Verwaltung.

Aktueller spezifischer Auftrag:

- `scalance_xr328_snmp_mirroring_abfrage.md`: SNMP-Zustandsabfrage fuer Siemens SCALANCE XR328-4C WG.
- `scripts/scalance_xr328_mirror_check.py`: erster lesender Prototyp fuer die JSON-Diagnose des SCALANCE-Mirroring-Zustands.
