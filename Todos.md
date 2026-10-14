# Auftrag: Capture- und Sensor-Verwaltung

[English view](Todos.en.md). Both language views describe the same planning
record; keep their full meaning and status synchronized.

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

- `Tools/snmp/README.md`: gemeinsame SNMP-Helfer-Doku, OIDs, JSON-Vertrag, Statusmodell und Staging-Hinweise.
- `Tools/snmp/scalance_switches_refresh.py`: serieller SNMP-Refresh-Runner fuer alle aktivierten Switches aus `hsd-data/switches.txt`.
- `scalance_xr328_mirror_check.md` im konfigurierten externen Befehlsverzeichnis: geraetespezifische SNMP-Beobachtungen fuer Siemens SCALANCE XR328-4C WG.
- `scalance_xc208g_mirror_check.md` im konfigurierten externen Befehlsverzeichnis: geraetespezifische SNMP-Beobachtungen fuer Siemens SCALANCE XC208G.
- `scalance_xr328_mirror_check.py` im konfigurierten externen Befehlsverzeichnis: lesendes Werkzeug fuer die JSON-Diagnose des SCALANCE-Mirroring-Zustands.

## Planning ownership and current checkpoint

This record, maintained in `Todos.md` and its synchronized `Todos.en.md` view,
is the sole source of active Hosts3D planning and status. The capture/sensor
proposals above remain proposals, not evidence of delivered features. New entries
use English. Technical contracts and operating instructions
belong in the maintained sources listed in `docs/README.md`; completed session
notes do not create another planning authority.

Checkpoint: 2026-10-14. Independent agent support, runtime invariants and local
development/topology workflows are maintained in their owning documentation.
The optional refresh now preflights enabled command families and has an inert
unavailable result without changing saved topology. Validation results are
recorded below.

Verified: all seven offline Python refresh tests pass, including missing-provider
preservation and a single enabled command family; both local skill frontmatter
checks pass. The existing project-supported WinLibs compiler built the corrected
Debug x64 application, and the native path/argument test passes. The native launcher
also preflights all enabled families before starting the detached runner, so a
missing selected command reports unavailable instead of claiming refresh started.
Quoted switch types and enabled flags are handled consistently in native and
Python preflight; the corresponding native and offline selected-family fixtures
pass. The two planning views have been reviewed side by side for equivalent tasks,
scope, status, constraints, checkpoint facts and next action.
The standard MSYS2 compiler/debugger packages are absent on this machine; this
verification used the already installed supported alternative, with no tooling
installation. Windows development now discovers existing compatible tools;
MSYS2 is a recommended setup only when no suitable environment exists. Build and
debug checks require only the selected architecture and operation. The resolver
fixtures pass for native/PATH/WinLibs discovery, explicit override rejection and
operation-specific dependencies. The automatic Debug x64 build passes with the
installed WinLibs compiler, and its GDB loads the executable and C++ debug sources.
GUI rendering, interactive breakpoint binding and real device queries were not
exercised by these checks.

### Deferred runtime issues

Status: deferred; no broad redesign is authorized by this list.

- `DrawList()` / `DrawView()` reread backing files each render frame, which is a
  performance concern for larger help/info/list windows.
- Info/export host-table traversal still lacks a complete lock/snapshot model for
  `hstsByIp` / `hstsByPos`; atomic `goHosts` addresses only the control-flag race.
- Legacy `goHosts` busy-wait loops could later use condition-based pause/resume.

### Application launcher and updates

Status: design proposal, not implemented. If approved later, prepare a small
Windows launcher before adding an OSD update action. Its initial scope is Start,
Check for Updates, Help and possibly Open Data Folder; defer tray behavior and
additional styling until useful. Use packaged release archives, preserve
`hsd-data`, stage replacement for restart and include bounded rollback/error
handling. Do not overwrite the running application or introduce another source
checkout/update system. Name, icon, explanatory text and window details remain
design decisions rather than completed work.

### Special-address and identity behavior

Status: design discussion, not implemented. Confirm the current first-match
`netpos.txt` contract before designing precedence; historical `/32` precedence
claims are superseded.

| Case | Candidate behavior |
|---|---|
| `0.0.0.0` | Grey reserved meta-host; no DNS or ordinary host confirmation |
| `255.255.255.255` | Dedicated global broadcast marker |
| Mask-like addresses | Suspicious/meta treatment unless explicit configuration says otherwise |
| Loopback `127.0.0.0/8` | Dedicated marker or zone |
| One MAC with several IPs | Separate hosts, nearby placement or a later relation marker; no automatic merge |
| One IP with several MACs | One IP host with conflict/anomaly status |

Last verified step: independent agent support, optional command preflight and
optional Windows toolchain discovery are implemented with the bounded checks
above. Next action: select the next pending
capture/sensor task or explicitly deferred proposal with the owner.
