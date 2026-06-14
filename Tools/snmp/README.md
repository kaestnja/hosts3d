# Hosts3D SNMP Tools

Dieser Ordner enthaelt optionale, lesende SNMP-Diagnosehelfer fuer SCALANCE
Switches. Die Dateien sind paarweise benannt: Script und Geraetenotiz tragen
denselben Basisnamen und unterscheiden sich nur durch die Dateiendung.

Aktuelle Paare:

- `scalance_switches_refresh.py`
- `scalance_xr328_mirror_check.py`
- `scalance_xr328_mirror_check.md`
- `scalance_xc208g_mirror_check.py`
- `scalance_xc208g_mirror_check.md`

Die geraetespezifischen Markdown-Dateien enthalten nur konkrete Beobachtungen,
Geraetedaten und Abweichungen. Gemeinsame Bedienung, OIDs, Statuswerte,
JSON-Vertrag und Packaging-Hinweise stehen hier.

## Ziel der Abfrage

Die Helfer sollen nach Moeglichkeit folgende Informationen ermitteln:

1. Ist Layer-2-Port-Mirroring global aktiviert?
2. Welcher Mirror-/Monitor-Zielport ist konfiguriert?
3. Bei welchen Source-Ports ist Ingress-Mirroring aktiviert?
4. Bei welchen Source-Ports ist Egress-Mirroring aktiviert?
5. Welche MAC-Adressen wurden an den betroffenen Ports gelernt?
6. Welche LLDP-Nachbarn sind an den betroffenen Ports sichtbar?
7. Welche Teilnehmerdaten sind direkt gelesen und welche nur indirekt korreliert?

Die Mirroring-Konfiguration selbst liefert keine Teilnehmerinformationen.
Teilnehmerdaten werden ueber Bridge-FDB, LLDP oder externe Quellen korreliert.

## Betrieb

- Der aktuelle Prototyp arbeitet read-only und ruft kein `snmpset` auf.
- Fuer produktionsnahe Umgebungen SNMPv3 mit `authPriv` bevorzugen.
- SNMPv1/v2c ist fuer Laborbetrieb und vorhandene OT-Konfigurationen bewusst
  erlaubt.
- Wenn bei SNMPv1/v2c keine Community angegeben wird, probieren die Helfer
  read-only `private` und danach `public`.
- Fuer abweichende SNMPv1/v2c-Werte `--community COMMUNITY` oder
  `community=...` in `switches.txt` verwenden.
- Fuer SNMPv3-Credentials bevorzugt Umgebungsvariablen verwenden:
  `SNMP_USER`, `SNMP_AUTH_PASS`, `SNMP_PRIV_PASS`.
- Credentials werden nicht als Werte in JSON, Logs oder Fehlermeldungen
  ausgegeben.

## Net-SNMP und lokale Tools

Die Helfer sind Python-Wrapper um Net-SNMP (`snmpget` und `snmpwalk`). Dadurch
bleibt die Python-Seite dependency-arm, und SNMP-Fehler lassen sich direkt mit
CLI-Werkzeugen reproduzieren.

Windows-Builds koennen lokale Net-SNMP-Werkzeuge enthalten:

- `snmpget.exe`
- `snmpwalk.exe`
- `snmpset.exe`

Die XC208G- und XR328-Helfer koennen explizite Pfade ueber `--snmpget` und
`--snmpwalk` verwenden. Der XC208G-Helfer sucht beim direkten Repository-Lauf
ausserdem automatisch typische lokale Build-/Paketpfade wie
`Debug/windows/x64`, `Release/windows/x64`, `Release/windows/x86` und den
Paket-Root; `PATH` ist nur ein Fallback.

## Beispiele

SNMPv2c-Laborprobe:

```bash
python Tools/snmp/scalance_xr328_mirror_check.py SWITCH_IP --version 2c --community COMMUNITY --pretty
python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 2c --community COMMUNITY --pretty
```

Kurze Access-Probe:

```bash
python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 2c --check-access-only --pretty
```

SNMPv3 mit Umgebungsvariablen:

```bash
export SNMP_USER=USER
export SNMP_AUTH_PASS=AUTHPASS
export SNMP_PRIV_PASS=PRIVPASS
python3 Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 3 --check-access-only --pretty
```

PowerShell:

```powershell
$env:SNMP_USER = "USER"
$env:SNMP_AUTH_PASS = "AUTHPASS"
$env:SNMP_PRIV_PASS = "PRIVPASS"
python Tools/snmp/scalance_xc208g_mirror_check.py SWITCH_IP --version 3 --pretty
```

Optionales `switches.txt`-Beispiel:

```text
switch name=swxc208g type=scalance_xc208g host=SWITCH_IP version=2c community=COMMUNITY enabled=1 auto_refresh=1 refresh_seconds=60
```

`switches.txt` ist Runtime-/Anlagenkonfiguration und gehoert unter
`hsd-data/`, nicht in diesen Toolordner. Globale Defaults koennen in
`settings.ini` stehen; die wiederholbare Switch-Liste bleibt als eigene Datei
lesbarer.

`scalance_switches_refresh.py` fragt mehrere `enabled=1` Switch-Zeilen seriell
ab. Die Topologieausgabe wird als Snapshot eines einzelnen Refresh-Zeitpunkts
neu erzeugt. Die JSON-Dateien unter `hsd-data/snmp/` bleiben der rohe
Diagnosevertrag; `switch-topology.txt` bleibt der kleine Anzeigevertrag.

## Gemeinsame Siemens-Mirroring-OIDs

Primaerer Siemens-Mirroring-Zweig:

```text
1.3.6.1.4.1.4329.20.1.1.1.1.1.6
snMspsConfigMirror
```

Klassische Port-Mirroring-OIDs:

```text
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.1.0
snMspsConfigMirrorStatus.0
Werte: disabled(1), enabled(2)

1.3.6.1.4.1.4329.20.1.1.1.1.1.6.2.0
snMspsConfigMirrorToPort.0

1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.2.<idx>
snMspsConfigMirrorCtrlIngressMirroring
Werte: enabled(1), disabled(2)

1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.3.<idx>
snMspsConfigMirrorCtrlEgressMirroring
Werte: enabled(1), disabled(2)

1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.4.<idx>
snMspsConfigMirrorCtrlStatus
Werte: enabled(1), disabled(2)
```

Wichtig: Der globale Status ist anders kodiert als die Portflags.

Erweiterte Mirroring-Tabellen werden optional roh gelesen:

```text
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9
```

Die Rohstruktur wird gruppiert, aber noch nicht als vollstaendig validierte
Siemens-Session-Semantik behandelt.

## Gemeinsame Standard-MIBs

Systemidentitaet:

```text
sysDescr:    1.3.6.1.2.1.1.1.0
sysObjectID: 1.3.6.1.2.1.1.2.0
sysName:     1.3.6.1.2.1.1.5.0
```

Interfaces:

```text
ifDescr:       1.3.6.1.2.1.2.2.1.2
ifAdminStatus: 1.3.6.1.2.1.2.2.1.7
ifOperStatus:  1.3.6.1.2.1.2.2.1.8
ifName:        1.3.6.1.2.1.31.1.1.1.1
ifAlias:       1.3.6.1.2.1.31.1.1.1.18
```

Bridge-FDB:

```text
dot1dTpFdbAddress:    1.3.6.1.2.1.17.4.3.1.1
dot1dTpFdbPort:       1.3.6.1.2.1.17.4.3.1.2
dot1dBasePortIfIndex: 1.3.6.1.2.1.17.1.4.1.2
dot1qTpFdbTable:      1.3.6.1.2.1.17.7.1.2.2
```

LLDP:

```text
lldpLocPortId:       1.0.8802.1.1.2.1.3.7.1.3
lldpLocPortDesc:     1.0.8802.1.1.2.1.3.7.1.4
lldpRemTable:        1.0.8802.1.1.2.1.4.1
lldpRemManAddrTable: 1.0.8802.1.1.2.1.4.2
```

## JSON-Vertrag

Die Helfer geben JSON aus. Die Struktur bleibt fuer Hosts3D und spaetere
Automatisierung stabil:

```json
{
  "status": "ok",
  "device": {
    "host": "SWITCH_IP",
    "sys_name": "switch_name",
    "sys_descr": "device description",
    "sys_object_id": ".1.3.6.1..."
  },
  "snmp": {
    "version": "v2c",
    "port": 161,
    "timeout": 3,
    "retries": 1,
    "credential_state": {
      "community": "provided"
    }
  },
  "mirroring": {
    "global_status": "enabled",
    "destination_port": {
      "raw_id": 3,
      "if_index": 3,
      "if_name": "P0.3"
    },
    "source_ports": []
  },
  "interfaces": []
}
```

Statuswerte:

```text
ok
partial_data
snmp_unreachable
auth_failed
oid_not_supported
empty_table
parse_error
not_implemented
unknown
```

## Hosts3D und Packaging

`Tools/snmp/README.md` ist die Quelle fuer die SNMP-Ordnerdoku. Windows
Runtime-Ordner wie `Release/windows/x64/Tools/snmp/README.md` sind gestagte
Kopien und sollen nicht direkt gepflegt werden.

`Tools/Stage-RuntimePayload.ps1` kopiert diese README, den seriellen
Refresh-Runner, die Script-/Markdown-Paare und die Wrapper
`run-scalance-check.ps1` / `run-scalance-check.cmd` in den Runtime- oder
Paketordner.

Die F9-Integration startet den seriellen Refresh-Runner
`scalance_switches_refresh.py`. Der Runner liest `hsd-data/switches.txt`, fragt
alle aktivierten Switches nacheinander ab, schreibt pro Switch ein JSON unter
`hsd-data/snmp/` und erzeugt eine gemeinsame `switch-topology.txt`.

In der F9-Ansicht werden nur Hosts gezeichnet, die einem Switch-Port zugeordnet
werden konnten. LLDP-Nachbarn und gelernte MAC-Adressen werden pro Port
konservativ zusammengefuehrt, damit ein Rechner nicht als zwei Host-Kuben
erscheint. Neue Topologie-Daten werden erst nach erfolgreichem Einlesen
uebernommen; waehrend eines Refreshs bleibt die letzte gueltige Darstellung
sichtbar.

Mehrere Switches werden als getrennte Port-Reihen dargestellt. Fuer 28-Port-
XR328-Bloecke nutzt Hosts3D die Frontplatten-Reihenfolge P1-P12 ueber P13-P24
und P25/P26 ueber P27/P28; wegen der Default-Kamera wird die obere physische
Reihe auf die positive Z-Reihe gelegt. Hosts an der oberen und unteren XR328-
Portreihe werden auf entgegengesetzte Seiten gelegt. Mirroring wird ueber
Portfarben und die F9-OSD-Legende angezeigt; direkte Mirror-Verbindungslinien
werden bewusst nicht gezeichnet.

## Quellen

- https://oid-base.com/get/1.3.6.1.4.1.4329
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6
- https://support.industry.siemens.com/cs/document/109765124/which-oids-can-you-use-to-configure-diagnose-and-control-scalance-devices-via-snmp-?lc=en-ru
- https://www.rfc-editor.org/rfc/rfc2863.html
- https://datatracker.ietf.org/doc/html/rfc4188
- https://www.rfc-editor.org/rfc/rfc4363.html
