# Hosts3D switch refresh

Setup is described in the root [README](../../README.md#shared-scriptcommands).
`scalance_switches_refresh.py` belongs to Hosts3D. It reads `switches.txt`, passes
explicit parameters to the device commands in the configured shared checkout,
and replaces the topology only after every enabled query succeeds.

The device commands, device-specific protocol notes and their tests live in an
optional external script directory, selected by `scriptcommands_path` in `settings.ini`.
F9 passes the executable directory as the base for a relative checkout path.
For manual refresh, supply `--scriptcommands-path` and optionally
`--application-dir` along with `--config-file`, `--json-dir`, `--write-topology`.
Use `--snmpget` and `--snmpwalk` for explicit Net-SNMP executable paths.

## Optional availability

The command directory is an optional enhancement. Without it, host traffic,
capture/replay and existing or manually edited topology remain usable. The runner
accepts an omitted `--scriptcommands-path` as an inert unavailable call: exit code
`2`, no device query and no output-file changes. Missing commands for enabled
families have the same result. Only selected families are required; disabled
entries and unused command families do not block a refresh.

Exit code `0` means the complete query pass succeeded; `1` means no enabled
switches or a failed query. An unavailable result never creates demo observations
or reports a successful live refresh. Offline tests use explicitly labelled
fixtures and retain the last complete topology on failure.

The runner does not supply an implicit laboratory switch or credentials. New
switch types need a mapping in `helper_for_type`; shared device implementation
changes belong in the configured checkout. Run `python -B -m unittest discover
-s tests` from the Hosts3D root to check the refresh flow without querying devices.
