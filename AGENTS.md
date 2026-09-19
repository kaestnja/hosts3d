# Hosts3D development

For shared device queries, first read the **Shared ScriptCommands** section in
[README.md](README.md#shared-scriptcommands). The existing `settings.ini` owns
`scriptcommands_path`; relative paths resolve against the executable directory.
Read `CONTRACT.md` and `AGENTS.md` in that checkout before changing a shared command.

Edit, test, commit and push common command corrections in `sec/scriptcommands`.
Use its existing Git checkout directly. Do not introduce package/update management,
another invocation framework or private copies of shared device commands.
Hosts3D owns `hsd-data/switches.txt`, passes explicit method parameters to the
commands, and renders their observations. New device families also need a type
mapping in `Tools/snmp/scalance_switches_refresh.py` and suitable rendering.

Keep real laboratory settings in local configuration. Preserve configured devices
and access data; real non-secret examples in documentation/tests are allowed.
Never restore a specific laboratory host or implicit credentials as a code default.
Run focused tests with `python -B -m unittest discover -s tests`. Build Windows
with `Tools/Hosts3D-DevTools.ps1 -Task BuildHosts3D -Config Debug -Arch x64`.
The C++ path test is `tests/test_scriptcommands_path.cpp`.
