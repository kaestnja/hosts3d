# Siemens SCALANCE XC208G

Partnerdatei zu `scalance_xc208g_mirror_check.py`.

Diese Datei enthaelt nur geraetespezifisches Know-how fuer Siemens SCALANCE
XC208G Switches. Gemeinsame SNMP-OIDs, Net-SNMP-Aufrufe, JSON-Vertrag,
Credential-Regeln und Packaging-Hinweise stehen in `README.md` im selben
Ordner.

## Geraet

- Modell: Siemens SCALANCE XC208G
- Produktfamilie: SCALANCE XC-200, Managed Layer 2 IE Switch
- Typischer Ausbau: 8 elektrische RJ45-Ports
- Uplink/Portgeschwindigkeit laut XC-200-Betriebsanleitung: 10 / 100 / 1000 Mbps
- Real beobachtete Firmware per `sysDescr`: `V04.07.00`
- Real beobachtete Hardware-Version per `sysDescr`: `Version 3`
- Real beobachtete Artikelnummer per `sysDescr`: `6GK5 208-0GA00-2AC2`
- Real beobachteter `sysObjectID`: `.1.3.6.1.4.1.4329.6.1.2.1.2`

Bekannte Eigenschaften laut Siemens-/Produktunterlagen:

- MIB support: yes
- Port mirroring: yes
- Multiport mirroring: yes fuer XC208G-Varianten in verfuegbaren Datenblaettern
- LLDP: yes
- SNMP v1: yes
- SNMP v2: yes
- SNMP v3: yes

Quellen:

- https://support.industry.siemens.com/cs/attachments/109743149/BA_SCALANCE-XC-200_76.pdf
- https://docs.tia.siemens.cloud/r/en-us/v20/configuring-scalance-x/w/m/configuring-scalance-x/configuring-layer-2-functions/mirroring/basics

## Real beobachtete XC208G

Erster erfolgreicher Lauf: 2026-06-10

SNMPv2c ueber UDP/161 funktionierte, nachdem auf den Switches eine Rueckroute
eingetragen wurde. Die erfolgreiche Community-Quelle war `default_private`.

```text
192.168.4.241 sysName=sw4241xc208g serial=SVPR8209635 lldp_neighbor_count=4
192.168.4.242 sysName=sw4242xc208g serial=SVPR8209628 lldp_neighbor_count=6
192.168.4.243 sysName=sw4243xc208g serial=SVPR8209631 lldp_neighbor_count=4
```

Gemeinsame Beobachtungen:

- `sysDescr` meldete `SCALANCE XC208G`, `6GK5 208-0GA00-2AC2`,
  `HW: Version 3`, `FW: Version V04.07.00`.
- Physische Interfaces kamen als `P0.1` bis `P0.8`.
- Zusaetzlich wurden `vlan1` mit `ifIndex=61` und `loopback0` mit
  `ifIndex=105` gesehen.
- `dot1dTpFdbAddress`, `dot1dTpFdbPort` und `dot1dBasePortIfIndex`
  antworteten mit verwertbaren Bridge-MIB-Daten.
- `dot1qTpFdbTable` antwortete mit `No Such Object available on this agent at this OID`.
- LLDP lieferte verwertbare lokale Portdaten und Nachbarn.

## Beobachtetes Mirroring-Verhalten

Beim ersten Lauf war Mirroring auf allen drei XC208G deaktiviert:

- `snMspsConfigMirrorStatus.0` wurde als `disabled` dekodiert.
- `snMspsConfigMirrorToPort.0` lieferte `0`.
- Klassische portbezogene Mirroring-OIDs antworteten in diesem Zustand mit
  `No Such Instance currently exists at this OID`.
- Erweiterte Mirroring-Tabellen lieferten in diesem Zustand keine Eintraege.

Nach Aktivierung von Mirroring auf `192.168.4.242` / `sw4242xc208g`:

- `mirroring.global_status = "enabled"`
- Ziel-/Monitor-Port: `P0.3`, raw/ifIndex `3`
- Source-Ports:
  - `P0.1`: `egress_mirroring = true`, `ingress_mirroring = false`
  - `P0.4`: `egress_mirroring = true`, `ingress_mirroring = false`
  - `P0.6`: `egress_mirroring = true`, `ingress_mirroring = false`
  - `P0.8`: `egress_mirroring = true`, `ingress_mirroring = false`
- Die klassischen portbezogenen Mirroring-OIDs lieferten Eintraege.
- Die erweiterten Mirroring-Tabellen lieferten eine Session `1`, Source-IDs
  `1`, `4`, `6`, `8` und Destination-ID `3`.

Damit ist fuer den XC208G beobachtet: `No Such Instance` bei den
portbezogenen Mirroring-OIDs kann durch deaktiviertes Mirroring entstehen und
darf nicht automatisch als fehlende MIB-Unterstuetzung interpretiert werden.

## Port- und Topologie-Mapping

Der XC208G-Helfer schreibt im Topologieformat `ports=8`. Die physische
Portdarstellung ist auf `P0.1` bis `P0.8` begrenzt; VLAN- und Loopback-
Interfaces werden nicht als physische Ports gezeichnet.

## Grenzen und offene Punkte

- SNMPv3 wurde an den drei beobachteten XC208G noch nicht praktisch validiert.
- Die erweiterte Mirroring-Rohstruktur ist technisch lesbar, aber noch nicht
  vollstaendig gegen Siemens-MIB/WBM als gesicherte Session-Semantik validiert.
- Detailvalidierung von FDB- und LLDP-Daten gegen die reale Verkabelung bleibt
  sinnvoll.
- Die Hosts3D-F9-Integration startet aktuell weiterhin den XR328-Prototyp; der
  XC208G-Helfer ist vorbereitet, aber noch nicht die F9-Default-Auswahl.
