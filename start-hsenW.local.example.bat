@echo off
rem Copy this file to "start-hsenW.local.bat" and adjust values.
rem Interface list: run start-hsenW.bat once without local config.

set "HSEN_ID=1"
set "HSEN_IFACE=\Device\NPF_{REPLACE_WITH_YOUR_INTERFACE_GUID}"
set "HSEN_DEST=127.0.0.1"
set "HSEN_PROMISC=1"
