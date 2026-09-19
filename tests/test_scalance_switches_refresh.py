import importlib.util
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "Tools" / "snmp" / "scalance_switches_refresh.py"
SPEC = importlib.util.spec_from_file_location("scalance_switches_refresh", SCRIPT_PATH)
switches_refresh = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = switches_refresh
SPEC.loader.exec_module(switches_refresh)


class ScalanceSwitchesRefreshTests(unittest.TestCase):
    def test_topology_is_replaced_only_after_all_helpers_succeed(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            config_path = base / "switches.txt"
            json_dir = base / "snmp"
            topology_path = base / "switch-topology.txt"
            commands = base / "shared commands [fixture]"
            commands.mkdir()
            for name in ("scalance_xr328_mirror_check.py", "scalance_xc208g_mirror_check.py"):
                (commands / name).write_text("# inert fixture", encoding="utf-8")
            config_path.write_text(
                "switch name=sw1 type=scalance_xr328 host=192.0.2.1 enabled=1\n"
                "switch name=sw2 type=scalance_xc208g host=192.0.2.2 enabled=1\n",
                encoding="utf-8",
            )
            topology_path.write_text("old topology\n", encoding="utf-8")
            calls = []

            def fake_run(cmd):
                self.assertEqual(topology_path.read_text(encoding="utf-8"), "old topology\n")
                self.assertEqual(Path(cmd[1]).parent, commands.resolve())
                self.assertNotIn("--config-file", cmd)
                self.assertIn(cmd[2], ("192.0.2.1", "192.0.2.2"))
                temp_path = Path(cmd[cmd.index("--write-topology") + 1])
                mode = "a" if "--append-topology" in cmd else "w"
                calls.append(cmd)
                with temp_path.open(mode, encoding="utf-8", newline="\n") as handle:
                    handle.write(f"switch call={len(calls)}\n")
                return SimpleNamespace(returncode=0)

            argv = [
                "scalance_switches_refresh.py",
                "--scriptcommands-path", str(commands),
                "--config-file",
                str(config_path),
                "--json-dir",
                str(json_dir),
                "--write-topology",
                str(topology_path),
            ]
            with patch.object(sys, "argv", argv), patch.object(switches_refresh.subprocess, "run", side_effect=fake_run):
                self.assertEqual(switches_refresh.main(), 0)

            self.assertEqual(len(calls), 2)
            self.assertEqual(topology_path.read_text(encoding="utf-8"), "switch call=1\nswitch call=2\n")
            self.assertFalse(any(topology_path.parent.glob(topology_path.name + ".*.refreshing")))

    def test_relative_checkout_is_based_on_application_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            self.assertEqual(switches_refresh.resolve_commands_path("../shared [commands]", str(base / "app")),
                             (base / "shared [commands]").resolve())
            with self.assertRaises(ValueError):
                switches_refresh.resolve_commands_path("", str(base))

    def test_connection_parameters_are_owned_by_hosts3d(self):
        with patch.dict("os.environ", {"FIXTURE_SNMP_USER": "fixture-user"}):
            args = switches_refresh.connection_arguments({"host": "192.0.2.1", "name": "Switch with spaces",
                "version": "3", "port": "1161", "user_env": "FIXTURE_SNMP_USER", "auth_proto": "SHA"})
        self.assertEqual(args, ["192.0.2.1", "--name", "Switch with spaces", "--version", "3", "--port", "1161",
                                "--user", "fixture-user", "--auth-proto", "SHA"])

    def test_no_configured_switch_does_not_query_a_default(self):
        with tempfile.TemporaryDirectory() as tmp:
            argv = ["refresh", "--scriptcommands-path", tmp, "--config-file", str(Path(tmp) / "missing.txt")]
            with patch.object(sys, "argv", argv), patch.object(switches_refresh.subprocess, "run") as run, contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(switches_refresh.main(), 1)
                run.assert_not_called()

    def test_direct_checkout_edits_and_failure_preserve_last_topology(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            commands = base / "commands [with spaces]"
            commands.mkdir()
            helper = commands / "scalance_xr328_mirror_check.py"
            config = base / "switches.txt"
            config.write_text("switch name=fixture host=192.0.2.1 version=2c community=fixture\n", encoding="utf-8")
            topology = base / "topology.txt"
            argv = ["refresh", "--scriptcommands-path", str(commands), "--config-file", str(config),
                    "--json-dir", str(base / "json"), "--write-topology", str(topology)]
            for value in ("first source", "edited source"):
                helper.write_text("import sys\nfrom pathlib import Path\nPath(sys.argv[sys.argv.index('--write-topology')+1]).write_text(" + repr(value) + ")\n", encoding="utf-8")
                with patch.object(sys, "argv", argv):
                    self.assertEqual(switches_refresh.main(), 0)
                self.assertEqual(topology.read_text(), value)
            helper.write_text("raise SystemExit(1)\n", encoding="utf-8")
            with patch.object(sys, "argv", argv):
                self.assertEqual(switches_refresh.main(), 1)
            self.assertEqual(topology.read_text(), "edited source")
            self.assertFalse(list(base.glob("*.refreshing")))


if __name__ == "__main__":
    unittest.main()
