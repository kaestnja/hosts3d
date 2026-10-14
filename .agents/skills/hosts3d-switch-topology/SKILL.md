---
name: hosts3d-switch-topology
description: Change or diagnose Hosts3D switch inventory, optional device-command invocation, refresh or topology rendering. Does not authorize switch configuration changes.
---

# Hosts3D switch topology

Start with [switch refresh](../../../Tools/snmp/README.md) and the optional
integration section of [README.md](../../../README.md#shared-scriptcommands).
Hosts3D owns `settings.ini`, `switches.txt`, invocation and rendering. Device
implementation, its paired protocol notes and command contract stay in the
configured external directory. Read that contract before editing the command;
inspect other consumers there and coordinate affected callers/tests/packages.

Keep configured devices and saved observations intact. No external directory or
missing selected capability means unavailable and no network query, not a live
success. Core traffic/capture/replay and saved/manual topology must remain useful.
Use only enabled inventory rows, explicit connection values and executable-based
relative paths; do not introduce a lab default or duplicate command implementation.

The serial refresh builds temporary topology and replaces the visible file only
after all selected helpers succeed. F9 retains the last valid parsed state.
Consult [runtime invariants](../../../docs/architecture/runtime.md#optional-switch-topology)
for raw JSON versus editable topology ownership and rendering constraints.

Run the relevant cases in `tests/test_scalance_switches_refresh.py` without live
devices. Use the existing C++ path/quoting test when invocation changes; compile
the app when its native launcher or renderer changes. Preserve legitimate device
manufacturer/model names. Do not add internal repository names or workplace paths
to this public repository; preserve the descriptive examples explicitly approved
in `AGENTS.md`.
