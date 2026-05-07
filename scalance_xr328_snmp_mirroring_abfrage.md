# SNMP-Zustandsabfrage fuer Siemens SCALANCE XR328-4C WG

## Zweck dieses Dokuments

Dieses Dokument dient als kompakter technischer Arbeitsauftrag fuer Codex in Visual Studio Code.

Ziel ist die Ermittlung und spaetere Implementierung einer SNMP-Zustandsabfrage fuer einen Siemens SCALANCE XR328-4C WG Switch. Die Abfrage soll Layer-2-Port-Mirroring, Portzuordnung, gelernte MAC-Adressen und optional indirekte Teilnehmerinformationen wie IP-Adresse, Hostname und LLDP-Nachbarn erfassen.

Wichtig: Der erste Prototyp soll lesend arbeiten, damit OIDs, Datenmodell und JSON-Ausgabe gegen reale Switch-Zustaende validiert werden koennen. Spaetere Verwaltungsfunktionen duerfen auch Konfigurationen aendern, wenn Zweck, Umfang, Rechte und Rueckfallweg bewusst festgelegt sind.

---

## Zielgeraet

Geraet:

- Siemens SCALANCE XR328-4C WG
- Siemens-Artikelnummer laut Datenblatt: `6GK5328-4FS00-3AR3`

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

---

## Ziel der Abfrage

Das zu entwickelnde Tool soll nach Moeglichkeit folgende Informationen ermitteln:

1. Ist Layer-2-Port-Mirroring global aktiviert?
2. Welcher Mirror-Zielport ist konfiguriert?
3. Bei welchen Source-Ports ist Ingress-Mirroring aktiviert?
4. Bei welchen Source-Ports ist Egress-Mirroring aktiviert?
5. Welche MAC-Adressen wurden an den betroffenen Ports gelernt?
6. Welche IP-Adressen lassen sich indirekt zu diesen MAC-Adressen korrelieren?
7. Welche Hostnamen lassen sich indirekt zu diesen MAC-Adressen oder IP-Adressen korrelieren?
8. Welche LLDP-Nachbarn sind an den betroffenen Ports sichtbar?
9. Welche Daten sind sicher direkt vom Switch gelesen und welche sind nur abgeleitet?

---

## Grundsaetzliche Einschaetzung

### Direkt per SNMP vom Switch realistisch auslesbar

- Globaler Mirroring-Status
- Mirror-Zielport
- Ingress-Mirroring je Source-Port
- Egress-Mirroring je Source-Port
- Interface-Namen und Interface-Status
- Gelernte MAC-Adressen je Port ueber Bridge-MIB oder Q-BRIDGE-MIB
- LLDP-Nachbarn, sofern das angeschlossene Geraet LLDP sendet

### Nur indirekt oder eingeschraenkt ableitbar

- IP-Adresse eines Teilnehmers an einem Port
- Hostname eines Teilnehmers an einem Port
- Ob ein Teilnehmer aktuell wirklich erreichbar ist
- Ob ein Teilnehmer direkt am Port angeschlossen ist oder hinter einem weiteren Switch, Router, Medienkonverter, Hypervisor oder WLAN-Bridge liegt

### Nicht aus der Mirroring-Konfiguration selbst ableitbar

Die Siemens-Mirroring-OIDs liefern nach aktuellem Kenntnisstand nur Informationen zur Mirroring-Konfiguration, nicht zu Teilnehmern am Port. Teilnehmerinformationen muessen ueber andere Tabellen oder externe Datenquellen korreliert werden.

---

## Wichtige Einschraenkungen

1. Die Mirroring-Konfiguration liegt voraussichtlich nicht in einer Standard-MIB, sondern in der Siemens Private MIB.
2. Die unten genannten OIDs stammen aus oeffentlichen OID-Repositories und sollten gegen die Private MIB des konkreten Firmwarestands validiert werden.
3. Siemens-Geraete koennen je nach Firmware und Geraetemodell nur Teilmengen der generischen Siemens Private MIB implementieren.
4. Das Datenblatt nennt fuer den XR328-4C WG `port mirroring: yes`, aber `multiport mirroring: no`.
5. Erweiterte Mirroring-Tabellen koennen in der MIB existieren, aber am konkreten Geraet leer oder nicht implementiert sein.
6. Eine gelernte MAC-Adresse ist ein FDB-Eintrag und kann altern, verschwinden oder von einem nachgelagerten Geraet stammen.
7. IP- und Hostname-Zuordnungen sind keine sicheren Layer-2-Informationen des Switchports, sondern Korrelationen.
8. Script-Ausgaben sollen ASCII-only sein: keine Emojis, keine Farbcodes, keine Unicode-Symbole.

---

## Betriebs- und Berechtigungsvorgaben

- Fuer produktionsnahe Umgebungen bevorzugt SNMPv3 mit `authPriv` verwenden.
- SNMPv1/v2c bewusst erlauben, wenn dies technisch erforderlich, im Labor sinnvoll oder am Geraet nur so verfuegbar ist.
- Default-Communitys nicht still voraussetzen; Communitys und Benutzer muessen konfigurierbar sein.
- SNMP-Credentials nicht unnoetig in Logs, Fehlermeldungen oder JSON-Ausgaben schreiben.
- SNMP-SETs sind nicht Teil des ersten Diagnose-Prototyps, aber fuer spaetere Mirror-Port-Verwaltung ausdruecklich moeglich.
- Least Privilege bedeutet: so wenig Rechte wie praktikabel, aber Schreib- und Konfigurationsrechte sind erlaubt, wenn sie fuer die Aufgabe benoetigt werden.
- Timeouts und Retries begrenzen.
- Nicht vorhandene OIDs sauber als `not_supported` melden.
- Leere Tabellen sauber als `empty_table` melden.
- Zwischen direkt gelesenen Daten und abgeleiteten Korrelationen unterscheiden.

---

## Primaerer Siemens-Mirroring-Zweig

Basiszweig:

```text
1.3.6.1.4.1.4329.20.1.1.1.1.1.6
```

Name laut oeffentlicher OID-Dokumentation:

```text
snMspsConfigMirror
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6

---

## Siemens Private MIB: klassische Port-Mirroring-OIDs

### Globaler Mirroring-Status

```text
OID:        1.3.6.1.4.1.4329.20.1.1.1.1.1.6.1.0
Name:       snMspsConfigMirrorStatus.0
Werte:      disabled(1), enabled(2)
Zugriff:    read-write laut MIB, im Tool nur lesen
Bedeutung:  Globaler Status der Mirroring-Funktion
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.1

Dekodierung:

```text
1 -> disabled
2 -> enabled
anderer Wert -> unknown
```

---

### Mirror-Zielport

```text
OID:        1.3.6.1.4.1.4329.20.1.1.1.1.1.6.2.0
Name:       snMspsConfigMirrorToPort.0
Typ:        Integer32
Zugriff:    read-write laut MIB, im Tool nur lesen
Bedeutung:  Port, zu dem der gespiegelte Traffic kopiert wird
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.2

Hinweis fuer die Implementierung:

- Den gelieferten numerischen Wert nicht blind als physische Portnummer interpretieren.
- Gegen `ifIndex`, `ifName`, `ifDescr`, `ifAlias` und gegebenenfalls Bridge-Port-Mapping validieren.

---

### Portbasierte Mirroring-Tabelle

Tabelle:

```text
snMspsConfigMirrorCtrlTable
```

Entry:

```text
snMspsConfigMirrorCtrlEntry
```

Basis:

```text
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1

Die Tabelle enthaelt laut oeffentlicher OID-Beschreibung einen Eintrag pro Interface. Der Index ist nach der Beschreibung der Mirror-Control-Index, in der Praxis voraussichtlich direkt oder indirekt dem Interface-Index zuzuordnen. Dies muss am realen Geraet verifiziert werden.

---

### Ingress Mirroring je Interface

```text
OID-Basis:  1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.2
Name:       snMspsConfigMirrorCtrlIngressMirroring
Index:      .<mirrorCtrlIndex oder ifIndex>
Werte:      enabled(1), disabled(2)
Bedeutung:  Eingehender Traffic dieses Interfaces wird zum Mirror-Zielport gespiegelt.
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.2

Dekodierung:

```text
1 -> true
2 -> false
anderer Wert -> unknown
```

---

### Egress Mirroring je Interface

```text
OID-Basis:  1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.3
Name:       snMspsConfigMirrorCtrlEgressMirroring
Index:      .<mirrorCtrlIndex oder ifIndex>
Werte:      enabled(1), disabled(2)
Bedeutung:  Ausgehender Traffic dieses Interfaces wird zum Mirror-Zielport gespiegelt.
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.3

Dekodierung:

```text
1 -> true
2 -> false
anderer Wert -> unknown
```

---

### Eintragsstatus der portbasierten Mirroring-Tabelle

```text
OID-Basis:  1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.4
Name:       snMspsConfigMirrorCtrlStatus
Index:      .<mirrorCtrlIndex oder ifIndex>
Bedeutung:  Status oder Validitaet des Tabelleneintrags
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1

Hinweis:

- Werte anhand der konkreten Siemens Private MIB des Geraets dekodieren.
- Wenn die Tabelle Werte liefert, aber kein Statusobjekt vorhanden ist, die Source-Ports trotzdem anhand Ingress/Egress auswerten und den Status als `unknown` melden.

---

## Erweiterte Siemens-Mirroring-Tabellen

Der XR328-4C WG unterstuetzt laut Datenblatt kein Multiport Mirroring. Trotzdem kann die Siemens Private MIB erweiterte Tabellen enthalten. Diese Tabellen sollten optional abgefragt werden. Falls nicht vorhanden oder leer, soll das Tool dies sauber melden.

### Erweiterte Mirroring-Session-Tabelle

```text
Tabelle: snMspsConfigMirrorCtrlExtnTable
Basis:   1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6
Entry:   1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1
Index:   snMspsConfigMirrorCtrlExtnSessionIndex
```

Quellen:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1

Wichtige Spalten:

```text
snMspsConfigMirrorCtrlExtnSessionIndex
OID-Basis: 1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1.1

snMspsConfigMirrorCtrlExtnMirrType
OID-Basis: 1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1.2

snMspsConfigMirrorCtrlExtnStatus
OID-Basis: 1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1.6
Typ:       RowStatus
```

Quelle fuer RowStatus-Spalte:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1.6

---

### Erweiterte Source-Tabelle

```text
Tabelle: snMspsConfigMirrorCtrlExtnSrcTable
Basis:   1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7
Entry:   1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1
Index:   snMspsConfigMirrorCtrlExtnSessionIndex, snMspsConfigMirrorCtrlExtnSrcId
```

Quellen:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1

Wichtige Spalten:

```text
snMspsConfigMirrorCtrlExtnSrcId
OID-Basis: 1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.1
Bedeutung: Source-ID. Bei portbasiertem Mirroring kann dies der Port-ifIndex sein.

snMspsConfigMirrorCtrlExtnSrcMode
OID-Basis: 1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.3
Werte:     ingress(1), egress(2), both(3)
```

Quellen:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.3

Dekodierung fuer `snMspsConfigMirrorCtrlExtnSrcMode`:

```text
1 -> ingress
2 -> egress
3 -> both
anderer Wert -> unknown
```

---

### Erweiterte Destination-Tabelle

```text
Tabelle: snMspsConfigMirrorCtrlExtnDestinationTable
Basis:   1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9
Entry:   1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1
Index:   snMspsConfigMirrorCtrlExtnSessionIndex, snMspsConfigMirrorCtrlExtnDestination
```

Quellen:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1

Wichtige Spalte:

```text
snMspsConfigMirrorCtrlExtnDestination
OID-Basis: 1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1.1
Bedeutung: Destination-Port-ID der Mirroring-Session
```

Quelle:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1.1

---

## Standard-MIBs fuer Interface-Mapping

Die Siemens-Mirroring-Tabellen liefern numerische Port-IDs oder ifIndex-Werte. Fuer eine brauchbare Ausgabe muessen diese Werte auf lesbare Portbezeichnungen gemappt werden.

### IF-MIB

Relevante OIDs:

```text
ifDescr
OID-Basis: 1.3.6.1.2.1.2.2.1.2

ifName
OID-Basis: 1.3.6.1.2.1.31.1.1.1.1

ifAlias
OID-Basis: 1.3.6.1.2.1.31.1.1.1.18

ifAdminStatus
OID-Basis: 1.3.6.1.2.1.2.2.1.7

ifOperStatus
OID-Basis: 1.3.6.1.2.1.2.2.1.8
```

Quelle:

- https://www.rfc-editor.org/rfc/rfc2863.html

Zuordnung:

```text
ifIndex -> ifName
ifIndex -> ifDescr
ifIndex -> ifAlias
ifIndex -> ifAdminStatus
ifIndex -> ifOperStatus
```

Empfohlene Ausgabe je Interface:

```json
{
  "if_index": 5,
  "if_name": "port_name_or_null",
  "if_descr": "description_or_null",
  "if_alias": "alias_or_null",
  "admin_status": "up_or_down_or_unknown",
  "oper_status": "up_or_down_or_unknown"
}
```

---

## MAC-Adressen pro Port

MAC-Adressen pro Port werden nicht aus der Siemens-Mirroring-Konfiguration gelesen, sondern aus der Forwarding Database des Switches.

### Bridge-MIB

Relevante OIDs:

```text
dot1dTpFdbAddress
OID-Basis: 1.3.6.1.2.1.17.4.3.1.1

dot1dTpFdbPort
OID-Basis: 1.3.6.1.2.1.17.4.3.1.2

dot1dTpFdbStatus
OID-Basis: 1.3.6.1.2.1.17.4.3.1.3

dot1dBasePortIfIndex
OID-Basis: 1.3.6.1.2.1.17.1.4.1.2
```

Quelle:

- https://datatracker.ietf.org/doc/html/rfc4188

Auswertelogik:

```text
1. dot1dTpFdbAddress lesen.
2. dot1dTpFdbPort lesen.
3. FDB-MAC-Adresse dem Bridge-Port zuordnen.
4. dot1dBasePortIfIndex lesen.
5. Bridge-Port auf ifIndex mappen.
6. ifIndex auf ifName, ifDescr und ifAlias mappen.
7. MAC-Adresse dem Switchport zuordnen.
```

Einschraenkungen:

- FDB-Eintraege sind gelernte MAC-Adressen, keine garantierten Endgeraete.
- MACs koennen altern und verschwinden.
- Hinter einem Port koennen mehrere MACs erscheinen.
- Mehrere MACs an einem Port koennen auf einen weiteren Switch, Router, Hypervisor, WLAN-Bridge oder Medienkonverter hindeuten.
- Eine einzelne MAC an einem Port beweist nicht zwingend einen direkt angeschlossenen Teilnehmer.

---

### Q-BRIDGE-MIB fuer VLAN-aware FDB

Wenn VLANs relevant sind, sollte zusaetzlich oder alternativ die Q-BRIDGE-MIB abgefragt werden.

Relevante Tabelle:

```text
dot1qTpFdbTable
OID-Basis: 1.3.6.1.2.1.17.7.1.2.2
```

Quelle:

- https://www.rfc-editor.org/rfc/rfc4363.html

Empfohlene Logik:

```text
1. Erst klassische Bridge-MIB pruefen.
2. Falls VLANs vorhanden oder Bridge-MIB unvollstaendig wirkt, Q-BRIDGE-MIB pruefen.
3. VLAN-ID, MAC-Adresse und Port gemeinsam auswerten.
4. Ergebnisse als vlan_aware markieren.
```

---

## IP-Adressen und Hostnamen

### Grundsatz

IP-Adressen und Hostnamen sind nicht direkt Bestandteil der L2-Mirroring-Konfiguration. Sie muessen aus anderen Tabellen oder externen Datenquellen korreliert werden.

Moeglicher Korrelationspfad:

```text
Switch-Port -> MAC:      Bridge-MIB oder Q-BRIDGE-MIB
MAC -> IP:               Gateway-ARP, ipNetToPhysicalTable, DHCP-Leases
IP -> Hostname:          DNS, DHCP, Asset-Datenbank
Switch-Port -> Neighbor: LLDP-MIB
```

---

### IP-MIB

Relevante Tabelle:

```text
ipNetToPhysicalTable
OID-Basis: 1.3.6.1.2.1.4.35
```

Bedeutung:

- Standardisierte Tabelle fuer IP-Adresse zu physischer Adresse.
- Nur hilfreich, wenn das SNMP-Ziel entsprechende Neighbor- oder ARP-Informationen besitzt.

Quelle:

- https://datatracker.ietf.org/doc/html/rfc4293

Einschaetzung fuer SCALANCE XR328-4C WG:

- Der Switch ist primaer ein Layer-2-Geraet.
- Es ist nicht sicher, dass er vollstaendige IP-zu-MAC-Informationen fuer angeschlossene Teilnehmer besitzt.
- Fuer MAC-zu-IP ist der Default-Gateway oder Router meist die bessere Quelle.
- DHCP-Leases sind oft eine sehr gute Quelle fuer MAC, IP und Hostname.
- DNS ist eine moegliche Quelle fuer IP zu Hostname, aber in OT-Netzen nicht immer vollstaendig oder aktuell.

---

## LLDP-Nachbarn

Der XR328-4C WG unterstuetzt laut Siemens-Datenblatt LLDP. Die LLDP-MIB kann Nachbargeraete liefern, sofern diese LLDP senden.

Relevante OIDs:

```text
lldpRemTable
OID-Basis: 1.0.8802.1.1.2.1.4.1

lldpRemChassisId
OID-Basis: 1.0.8802.1.1.2.1.4.1.1.5

lldpRemPortId
OID-Basis: 1.0.8802.1.1.2.1.4.1.1.7

lldpRemPortDesc
OID-Basis: 1.0.8802.1.1.2.1.4.1.1.8

lldpRemSysName
OID-Basis: 1.0.8802.1.1.2.1.4.1.1.9

lldpRemSysDesc
OID-Basis: 1.0.8802.1.1.2.1.4.1.1.10

lldpRemManAddrTable
OID-Basis: 1.0.8802.1.1.2.1.4.2
```

Hinweise:

- LLDP liefert nur Daten, wenn der Nachbar LLDP aktiv sendet.
- Viele OT-Endgeraete senden kein LLDP oder keinen brauchbaren Systemnamen.
- In PROFINET-Umgebungen kann der DCP-Name aussagekraeftiger sein als ein DNS-Hostname.
- PROFINET DCP ist aber kein generischer SNMP-Standardpfad.

---

## Empfohlene Discovery-Strategie

### Phase 1: SNMP-Erreichbarkeit und Basiserkennung

Ziel:

- SNMP-Erreichbarkeit pruefen.
- Geraet grob identifizieren.
- Interface-Tabelle aufbauen.

Minimal-OIDs:

```text
sysDescr
OID: 1.3.6.1.2.1.1.1.0

sysObjectID
OID: 1.3.6.1.2.1.1.2.0

sysName
OID: 1.3.6.1.2.1.1.5.0

ifDescr
OID-Basis: 1.3.6.1.2.1.2.2.1.2

ifName
OID-Basis: 1.3.6.1.2.1.31.1.1.1.1

ifAlias
OID-Basis: 1.3.6.1.2.1.31.1.1.1.18
```

Ergebnis:

```text
- device identity
- interface map
- SNMP status
```

---

### Phase 2: Klassische Siemens-Mirroring-Abfrage

Ziel:

- Globalen Mirroring-Status lesen.
- Mirror-Zielport lesen.
- Ingress-/Egress-Mirroring pro Source-Port lesen.

Abfragen:

```text
snMspsConfigMirrorStatus.0
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.1.0

snMspsConfigMirrorToPort.0
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.2.0

snMspsConfigMirrorCtrlIngressMirroring
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.2

snMspsConfigMirrorCtrlEgressMirroring
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.3
```

Ergebnis:

```text
- global mirror enabled/disabled
- destination port raw ID
- source ports with ingress mirror enabled
- source ports with egress mirror enabled
- source ports with both enabled
```

---

### Phase 3: Optionale erweiterte Mirroring-Abfrage

Ziel:

- Falls vom Geraet unterstuetzt, erweiterte Mirroring-Sessions erfassen.

Abfragen:

```text
snMspsConfigMirrorCtrlExtnTable
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6

snMspsConfigMirrorCtrlExtnSrcTable
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7

snMspsConfigMirrorCtrlExtnDestinationTable
1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9
```

Ergebnis:

```text
- extended_mirroring: supported / not_supported / empty_table
- sessions if present
- source IDs if present
- source modes ingress/egress/both if present
- destination IDs if present
```

---

### Phase 4: MAC-Adressen je Port

Ziel:

- Gelernte MAC-Adressen an Mirroring-Source-Ports und am Mirror-Zielport ermitteln.

Abfragen:

```text
dot1dTpFdbAddress
1.3.6.1.2.1.17.4.3.1.1

dot1dTpFdbPort
1.3.6.1.2.1.17.4.3.1.2

dot1dBasePortIfIndex
1.3.6.1.2.1.17.1.4.1.2
```

Optional VLAN-aware:

```text
dot1qTpFdbTable
1.3.6.1.2.1.17.7.1.2.2
```

Ergebnis:

```text
- port -> learned MAC list
- optional VLAN -> port -> MAC list
```

---

### Phase 5: LLDP-Nachbarn

Ziel:

- Nachbargeraete an Mirroring-relevanten Ports erfassen.

Abfragen:

```text
lldpRemTable
1.0.8802.1.1.2.1.4.1

lldpRemManAddrTable
1.0.8802.1.1.2.1.4.2
```

Ergebnis:

```text
- port -> LLDP chassis ID
- port -> LLDP port ID
- port -> LLDP system name
- port -> LLDP system description
- port -> LLDP management address if present
```

---

### Phase 6: IP- und Hostname-Korrelation

Ziel:

- MAC-Adressen aus der FDB optional mit IP-Adressen und Hostnamen korrelieren.

Moegliche Quellen:

```text
- ipNetToPhysicalTable auf dem Switch
- ipNetToPhysicalTable auf dem Gateway/Router
- ARP-Tabelle des Gateways/Routers
- DHCP-Leases
- DNS
- Asset-Datenbank
- passive Auswertung am Mirror-Port, falls separat implementiert
```

Wichtig:

- Diese Daten als `indirect` kennzeichnen.
- Quelle pro Korrelation angeben.
- Keine sichere Aussage erzeugen, wenn nur unsichere Daten vorhanden sind.

---

## Manuelle SNMP-Vorpruefung

### SNMPv3-Beispiele

Platzhalter:

```text
USER      -> SNMPv3 user
AUTHPASS  -> SNMPv3 authentication password
PRIVPASS  -> SNMPv3 privacy password
SWITCH_IP -> IP address of the SCALANCE switch
```

Befehle:

```bash
snmpget -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.1.1.0
snmpget -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.1.2.0
snmpget -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.1.5.0

snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.31.1.1.1.1
snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.2.2.1.2
snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.2.2.1.8

snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.4.1.4329.20.1.1.1.1.1.6

snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.17.4.3.1.1
snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.17.4.3.1.2
snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.3.6.1.2.1.17.1.4.1.2

snmpwalk -v3 -l authPriv -u USER -a SHA -A AUTHPASS -x AES -X PRIVPASS SWITCH_IP 1.0.8802.1.1.2.1.4.1
```

---

## Beschaffungs- und Implementierungswege fuer SNMP

Die SNMP-Funktionalitaet ist genormt. Fuer Hosts3D ist deshalb nicht entscheidend, ob die Daten aus Net-SNMP, einem anderen CLI-Werkzeug, einer Bibliothek oder einem kleinen Verwaltungsdienst kommen. Entscheidend ist ein stabiles, einheitliches Ergebnisformat, das Hosts3D, `hsen` oder ein separates Verwaltungstool verwenden koennen.

Parallel zu pruefende Wege:

1. Net-SNMP als externes Werkzeug:
   - Offizielle Quellen und aktuelle Build-/Binary-Lage pruefen.
   - Falls Windows-Binaries fehlen oder veraltet sind, eigenen reproduzierbaren Build pruefen.
   - Optional ein lokal abgelegtes Release-Artefakt verwenden und den Anwenderpfad dokumentieren.
2. Funktional aequivalente SNMP-Werkzeuge:
   - Nicht nur nach `net-snmp` suchen, sondern nach SNMPv1/v2c/v3 CLI-Tools mit `get`, `walk` und spaeter optional `set`.
   - Windows-Beschaffung, Lizenz, Pflegezustand und Scriptbarkeit bewerten.
3. Bibliothek oder internes Modul:
   - Fuer spaetere UI-nahe Integration denkbar, wenn CLI-Abhaengigkeiten zu unkomfortabel werden.
   - Muss das gleiche JSON- oder API-Ergebnis liefern wie der externe Prototyp.
4. Separates Verwaltungs-CLI:
   - Kann Switch-Zustaende abfragen und spaeter Mirror-Konfigurationen setzen.
   - Hosts3D muss dann nicht selbst SNMP sprechen, sondern nur das definierte Ergebnisformat konsumieren.

Sobald ein Weg robust genug ist, werden die anderen Wege in der Dokumentation als Alternativen festgehalten, aber nicht parallel weiter ausgebaut.

---

## Prototyp im Repository

Ein erster lesender Prototyp liegt unter:

```text
scripts/scalance_xr328_mirror_check.py
```

Der Prototyp nutzt Net-SNMP (`snmpget` und `snmpwalk`) als externe Werkzeuge und gibt JSON aus. Er ist bewusst als Diagnose- und Vertragswerkzeug gebaut: erst lesen und Format stabilisieren, danach koennen kontrollierte Verwaltungsfunktionen wie SNMP-SET, SSH-CLI oder API-basierte Switch-Aenderungen folgen.

Die Net-SNMP-Werkzeuge muessen nicht zwingend systemweit installiert sein. Vergleichbar mit der GLFW3-Laufzeitloesung ist auch ein lokaler Release-Pfad denkbar: `snmpget.exe`, `snmpwalk.exe` und ihre benoetigten DLLs liegen dann neben dem Verwaltungswerkzeug oder in einem dokumentierten Unterordner des Release-Pakets. Der aktuelle Prototyp kann solche lokalen Werkzeuge ueber `--snmpget` und `--snmpwalk` explizit verwenden; ohne diese Angaben sucht er die Befehle wie ueblich ueber die Kommandoauflösung des Systems. Wenn kein passendes Werkzeug gefunden wird, meldet das Tool dies im JSON-Statusmodell als `not_implemented`.

Der Windows-Buildweg fuer lokale Net-SNMP-Werkzeuge ist im Repository vorbereitet:

```text
compile-net-snmp-windows.bat NET_SNMP_SOURCE_ROOT
third_party/net-snmp/README.md
third_party/openssl/windows/<arch>/
```

Der getestete Build erzeugt `snmpget.exe`, `snmpwalk.exe` und `snmpset.exe` fuer `x64` und `x86` unter `Release/windows/<arch>/`. OpenSSL wird statisch gelinkt; `dumpbin /dependents` zeigte nur Windows-System-DLLs, keine OpenSSL- oder MSVC-Runtime-DLLs.

Beispiel mit SNMPv3 und Umgebungsvariablen, um Credentials nicht unnoetig in der Shell-History abzulegen:

```bash
export SNMP_USER=USER
export SNMP_AUTH_PASS=AUTHPASS
export SNMP_PRIV_PASS=PRIVPASS
python3 scripts/scalance_xr328_mirror_check.py SWITCH_IP --pretty
```

Beispiel mit SNMPv2c nur fuer Laborbetrieb:

```bash
SNMP_COMMUNITY=COMMUNITY python3 scripts/scalance_xr328_mirror_check.py SWITCH_IP --version 2c --pretty
```

Aktuelle Grenzen des Prototyps:

- Er liest bereits Device Identity, IF-MIB, klassische Siemens-Mirroring-OIDs, optionale erweiterte Siemens-Mirroring-Tabellen, Bridge-FDB und LLDP-Rohdaten.
- Er korreliert Bridge-FDB-MACs ueber Bridge-Port zu `ifIndex`.
- LLDP wird im ersten Schritt nur als Rohdatenzaehlung erfasst; die Port-zu-Nachbar-Korrelation ist ein naechster Ausbauschritt.
- IP-/Hostname-Korrelationen aus Gateway-ARP, DHCP, DNS oder Asset-Datenbank sind noch nicht implementiert.
- Das Tool ist als Diagnose- und JSON-Vertragsgrundlage fuer eine spaetere Hosts3D-UI-Integration gedacht.

---

## Gewuenschte Programmausgabe

Das Tool soll bevorzugt JSON ausgeben. Die JSON-Struktur soll maschinenlesbar und fuer spaetere Automatisierung stabil sein.

Beispiel:

```json
{
  "status": "ok",
  "device": {
    "host": "SWITCH_IP",
    "sys_name": "string_or_null",
    "sys_descr": "string_or_null",
    "sys_object_id": "string_or_null"
  },
  "snmp": {
    "version": "v3",
    "security_level": "authPriv",
    "mirror_branch_supported": true,
    "extended_mirror_supported": false
  },
  "mirroring": {
    "global_status": "enabled",
    "destination_port": {
      "raw_id": 10,
      "if_index": 10,
      "if_name": "port_name_or_null",
      "if_descr": "description_or_null",
      "if_alias": "alias_or_null",
      "admin_status": "up_or_down_or_unknown",
      "oper_status": "up_or_down_or_unknown"
    },
    "source_ports": [
      {
        "raw_id": 5,
        "if_index": 5,
        "if_name": "port_name_or_null",
        "if_descr": "description_or_null",
        "if_alias": "alias_or_null",
        "ingress_mirroring": true,
        "egress_mirroring": false,
        "admin_status": "up_or_down_or_unknown",
        "oper_status": "up_or_down_or_unknown",
        "learned_macs": [
          {
            "mac": "00:11:22:33:44:55",
            "vlan": null,
            "source": "bridge_mib",
            "confidence": "learned_fdb"
          }
        ],
        "lldp_neighbors": [
          {
            "chassis_id": "string_or_null",
            "port_id": "string_or_null",
            "port_description": "string_or_null",
            "system_name": "string_or_null",
            "system_description": "string_or_null",
            "management_address": "string_or_null"
          }
        ],
        "ip_correlations": [
          {
            "ip": "192.0.2.10",
            "mac": "00:11:22:33:44:55",
            "hostname": "name_or_null",
            "source": "dhcp_or_arp_or_dns_or_assetdb",
            "confidence": "indirect"
          }
        ]
      }
    ]
  },
  "notes": [
    "MAC-to-port values are learned FDB data and may age out.",
    "IP and hostname data are indirect correlations unless provided by LLDP or another explicit source.",
    "Mirroring OIDs are based on public Siemens Private MIB information and should be validated against the device firmware."
  ]
}
```

---

## Fehler- und Statusmodell

Das Tool soll mindestens folgende Statuswerte unterscheiden:

```text
ok
snmp_unreachable
auth_failed
oid_not_supported
empty_table
parse_error
partial_data
not_implemented
unknown
```

Beispiel fuer Teilergebnis:

```json
{
  "status": "partial_data",
  "details": [
    {
      "component": "siemens_mirror_oids",
      "status": "ok"
    },
    {
      "component": "q_bridge_mib",
      "status": "oid_not_supported"
    },
    {
      "component": "lldp_mib",
      "status": "empty_table"
    }
  ]
}
```

---

## Empfohlene Implementierungsoptionen

### Option A: Python mit pysnmp

Vorteile:

- Plattformunabhaengig
- Gute JSON-Verarbeitung
- Direkte Integration in groessere Tools moeglich

Nachteile:

- SNMPv3 authPriv kann je nach pysnmp-Version und Algorithmus fehleranfaellig sein
- Dependency-Management erforderlich

### Option B: Python als Wrapper um Net-SNMP

Vorteile:

- Net-SNMP ist robust und in Netzwerk-/OT-Umgebungen verbreitet
- `snmpget` und `snmpwalk` lassen sich vorab manuell testen
- Fehlerbilder sind oft leichter mit CLI zu reproduzieren

Nachteile:

- Externe Tools muessen installiert sein
- CLI-Ausgaben muessen sauber geparst werden

### Option C: PowerShell mit externem snmpwalk

Vorteile:

- Gut fuer Windows-Adminumgebungen
- Einfache Integration in bestehende Windows-Prozesse

Nachteile:

- SNMP-Bibliotheken fuer PowerShell sind uneinheitlich
- Auch hier ist Net-SNMP oder ein anderes CLI-Tool oft praktischer

Empfehlung fuer den ersten Prototyp:

```text
Python-Wrapper um Net-SNMP, Ausgabe als JSON, zunaechst ohne Schreiboperationen.
```

---

## Validierung gegen Web Based Management des Switches

Nach Implementierung sollte das Ergebnis gegen die Anzeige im Web Based Management des Switches validiert werden.

Zu pruefen:

1. Mirroring global aktiv/inaktiv
2. Mirror-Zielport
3. Source-Port mit Ingress-Mirroring
4. Source-Port mit Egress-Mirroring
5. Interface-Namen
6. Interface-Status
7. Gelernte MAC-Adressen je Port
8. LLDP-Nachbarn
9. Verhalten bei deaktiviertem Mirroring
10. Verhalten bei nicht vorhandenen oder leeren Tabellen

---

## Akzeptanzkriterien

Das Tool gilt als brauchbarer Prototyp, wenn es folgende Anforderungen erfuellt:

1. Es arbeitet im ersten Schritt ohne SNMP-SETs und schafft eine stabile Grundlage fuer spaetere Schreiboperationen.
2. Es unterstuetzt SNMPv3 authPriv.
3. Es kann den Siemens-Mirroring-Basiszweig automatisch testen.
4. Es dekodiert `enabled(1)` und `disabled(2)` korrekt.
5. Es liest globalen Mirroring-Status und Mirror-Zielport.
6. Es liest Ingress- und Egress-Mirroring je Source-Port.
7. Es mappt numerische Port-IDs auf `ifName`, `ifDescr` und `ifAlias`.
8. Es korreliert gelernte MAC-Adressen per Bridge-MIB oder Q-BRIDGE-MIB.
9. Es gibt LLDP-Nachbarn optional aus.
10. Es kennzeichnet IP- und Hostname-Zuordnungen als indirekte Korrelationen.
11. Es meldet nicht unterstuetzte OIDs sauber.
12. Es liefert JSON als maschinenlesbare Ausgabe.
13. Es gibt Credentials nicht unnoetig aus.
14. Es verwendet in Script-Ausgaben nur ASCII-Zeichen.

---

## Moeglicher Entwicklungsplan fuer Codex

### Schritt 1: SNMP-Grundmodul

Implementiere:

- [x] Konfigurationsaufnahme fuer Host, Port, SNMP-Version, Timeout, Retries
- [x] SNMPv3 authPriv Parameter
- [x] `snmpget` und `snmpwalk` Hilfsfunktionen
- [x] Saubere Fehlerklassifikation fuer Basispfade
- [x] Keine unnoetige Ausgabe von Credentials

### Schritt 2: Interface-Discovery

Implementiere:

- [x] Lesen von `sysDescr`, `sysObjectID`, `sysName`
- [x] Walk von `ifDescr`, `ifName`, `ifAlias`, `ifAdminStatus`, `ifOperStatus`
- [x] Aufbau einer internen `ifIndex`-Map

### Schritt 3: Siemens-Mirroring klassisch

Implementiere:

- [x] Lesen von `snMspsConfigMirrorStatus.0`
- [x] Lesen von `snMspsConfigMirrorToPort.0`
- [x] Walk von `snMspsConfigMirrorCtrlIngressMirroring`
- [x] Walk von `snMspsConfigMirrorCtrlEgressMirroring`
- [x] Dekodierung von enabled/disabled
- [x] Mapping der Indizes auf Interface-Daten

### Schritt 4: Bridge-FDB

Implementiere:

- [x] Walk von `dot1dTpFdbAddress`
- [x] Walk von `dot1dTpFdbPort`
- [x] Walk von `dot1dBasePortIfIndex`
- [x] Zuordnung MAC -> Bridge-Port -> ifIndex -> Portname anhand der FDB-OID-Suffixe

### Schritt 5: LLDP optional

Implementiere:

- [x] Walk von `lldpRemTable`
- [ ] Zuordnung zu lokalen Ports
- [ ] Ausgabe von Chassis-ID, Port-ID, Systemname, Beschreibung, Management-Adresse falls vorhanden

### Schritt 6: Erweiterte Mirroring-Tabellen optional

Implementiere:

- [x] Walk der Tabellen unter `1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6`, `.6.7`, `.6.9`
- [x] Falls nicht vorhanden: `not_supported`
- [x] Falls leer: `empty_table`
- [ ] Falls vorhanden: Sessions, Source-IDs, Source-Mode und Destination strukturiert ausgeben

### Schritt 7: Ausgabeformat

Implementiere:

- [x] Stabile JSON-Ausgabe
- [ ] Optional zusaetzlich tabellarische ASCII-Ausgabe fuer Menschen
- [x] In der JSON-Ausgabe direkte Messwerte und indirekte Korrelationen trennen

---

## Quellen

### Siemens

Siemens XR328-4C WG Datenblatt:

- https://support.industry.siemens.com/teddatasheet/?caller=SIOS&format=pdf&language=en&mlfbs=6GK5328-4FS00-3AR3

Siemens SNMP-Diagnose allgemein:

- https://support.industry.siemens.com/cs/attachments/21566216/21566216_Overview_Diagnostic_V22_en.pdf
- https://support.industry.siemens.com/cs/attachments/109781207/GS_SCALANCE-M800-SNMP_76.pdf

Siemens Mirroring im TIA-Dokumentationsportal:

- https://docs.tia.siemens.cloud/r/en-us/v21/configuring-scalance-x/w/m/configuring-scalance-x/useful-information/mirroring/l2_mirroring-einleitung_x500?contentId=Km3e4DM82Pi96DDmTUvmqA

### Siemens Private MIB / OIDBase

Siemens Enterprise-Zweig:

- https://oid-base.com/get/1.3.6.1.4.1.4329

Siemens Mirroring-Basiszweig:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6

Klassische Mirroring-OIDs:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.2
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.2
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.3.1.3

Erweiterte Mirroring-OIDs:

- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1.6
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.3
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1
- https://oid-base.com/get/1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1.1

### Standard-MIBs

IF-MIB:

- https://www.rfc-editor.org/rfc/rfc2863.html

BRIDGE-MIB:

- https://datatracker.ietf.org/doc/html/rfc4188

Q-BRIDGE-MIB:

- https://www.rfc-editor.org/rfc/rfc4363.html

IP-MIB:

- https://datatracker.ietf.org/doc/html/rfc4293
