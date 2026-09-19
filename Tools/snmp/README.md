# Hosts3D switch refresh

Setup is described in the root [README](../../README.md#shared-scriptcommands).
`scalance_switches_refresh.py` belongs to Hosts3D. It reads `switches.txt`, passes
explicit parameters to the device commands in the configured shared checkout,
and replaces the topology only after every enabled query succeeds.

The device commands, device-specific protocol notes and their tests now live in
`sec/scriptcommands`, selected by `scriptcommands_path` in `settings.ini`.
F9 passes the executable directory as the base for a relative checkout path.
For manual refresh, supply `--scriptcommands-path` and optionally
`--application-dir` along with `--config-file`, `--json-dir`, `--write-topology`.
Use `--snmpget` and `--snmpwalk` for explicit Net-SNMP executable paths.

The runner does not supply an implicit laboratory switch or credentials. New
switch types need a mapping in `helper_for_type`; shared device implementation
changes belong in the configured checkout. Run `python -B -m unittest discover
-s tests` from the Hosts3D root to check the refresh flow without querying devices.
