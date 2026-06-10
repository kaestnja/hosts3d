# Siemens SCALANCE XR328-4C WG

Partnerdatei zu `scalance_xr328_mirror_check.py`.

Diese Datei enthaelt nur geraetespezifisches Know-how fuer den Siemens
SCALANCE XR328-4C WG. Gemeinsame SNMP-OIDs, Net-SNMP-Aufrufe, JSON-Vertrag,
Credential-Regeln und Packaging-Hinweise stehen in `README.md` im selben
Ordner.

## Geraet

- Modell: Siemens SCALANCE XR328-4C WG
- Siemens-Artikelnummer laut Datenblatt: `6GK5328-4FS00-3AR3`
- Real beobachtetes Geraet per `sysDescr`: `6GK5 328-4SS00-3AR3`
- Real beobachtete Firmware per `sysDescr`: `V04.07.00`
- Real beobachtete Hardware-Version per `sysDescr`: `Version 2`
- Real beobachteter `sysName`: `sw6248xr328`
- Real beobachteter `sysObjectID`: `.1.3.6.1.4.1.4329.6.1.2.1.2`

Bekannte Eigenschaften laut Siemens-Datenblatt:

- MIB support: yes
- Port mirroring: yes
- Multiport mirroring: no
- LLDP: yes
- SNMP v1: yes
- SNMP v2: yes
- SNMP v3: yes

Quelle:

- https://support.industry.siemens.com/teddatasheet/?caller=SIOS&format=pdf&language=en&mlfbs=6GK5328-4FS00-3AR3

## Beobachtetes Mirroring-Verhalten

Der reale Validierungslauf gegen `sw6248xr328` wurde mit SNMPv2c und Firmware
`V04.07.00` durchgefuehrt.

- `snMspsConfigMirrorStatus.0` antwortete mit `enabled`.
- Der globale Status wurde zu `mirroring.global_status = "enabled"` dekodiert.
- `snMspsConfigMirrorToPort.0` lieferte den Rohwert `6`.
- Rohwert `6` entsprach am beobachteten Geraet `ifIndex=6` und Port `P0.6`.
- `P0.6` war administrativ und operativ `up`.
- Die abgefragten Source-Ports meldeten `ingress_mirroring = false`.
- Die beobachtete Mirror-Konfiguration war egress-orientiert.
- Klassische portbezogene Mirroring-OIDs lieferten verwertbare Eintraege.
- Erweiterte Mirroring-Tabellen antworteten und werden im JSON als
  `extended_mirroring_raw` nach beobachteten Session-, Source- und
  Destination-Indizes gruppiert.

Wichtig fuer Codepflege: Der globale Mirroring-Status ist anders kodiert als
die portbezogenen Mirror-Flags. Global gilt `disabled(1)`, `enabled(2)`;
Portflags verwenden `enabled(1)`, `disabled(2)`. Dafuer existieren getrennte
Decoder und Offline-Tests.

## Port- und Interface-Mapping

Beim real beobachteten XR328 entsprach der Index der klassischen
Mirroring-OIDs direkt dem `ifIndex` der physischen Ports. Diese Beobachtung
sollte fuer andere Firmwarestaende nicht blind verallgemeinert werden.

Die Topologieausgabe verwendet fuer dieses Geraet derzeit mindestens 28 Ports.
Nicht-physische Interfaces wie VLANs oder Loopbacks werden durch die gemeinsame
Filterlogik nicht als physische Switchports ausgegeben.

## FDB und LLDP

- Die klassische Bridge-MIB lieferte verwertbare FDB-Daten.
- `dot1qTpFdbTable` antwortete am beobachteten XR328 mit `No Such Object`.
- LLDP lieferte verwertbare Daten fuer `lldpLocPortId`, `lldpLocPortDesc`,
  `lldpRemTable` und `lldpRemManAddrTable`.
- Der Prototyp zaehlte im dokumentierten Lauf `lldp_raw_count = 135` und
  `lldp_neighbor_count = 15`.
- Strukturierte LLDP-Nachbarn werden an die betroffenen Source-Ports
  angehaengt, sofern der lokale LLDP-Portindex zum physischen Portmapping passt.

## Grenzen und offene Punkte

- Ein echter SNMPv3-Lauf am beobachteten XR328 steht noch aus, bis passende
  SNMPv3-Credentials vorhanden sind.
- Die erweiterte Mirroring-Rohstruktur ist technisch lesbar, aber noch nicht
  vollstaendig gegen Siemens-MIB/WBM als gesicherte Session-Semantik validiert.
- Die fachliche Interpretation von Richtung, Source-Port-Liste und Zielport
  sollte bei Aenderungen weiter gegen das Web Based Management geprueft werden.

## Hosts3D-Lab-Default

Die aktuelle F9-Integration startet weiterhin diesen XR328-Helfer mit dem
eingebauten Lab-Default:

```text
name=sw6248xr328
type=scalance_xr328
host=192.168.6.248
version=2c
read-only community probe=private, then public
```
