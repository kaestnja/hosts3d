import importlib.util
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
            config_path.write_text(
                "switch name=sw1 type=scalance_xr328 host=192.0.2.1 enabled=1\n"
                "switch name=sw2 type=scalance_xc208g host=192.0.2.2 enabled=1\n",
                encoding="utf-8",
            )
            topology_path.write_text("old topology\n", encoding="utf-8")
            calls = []

            def fake_run(cmd):
                self.assertEqual(topology_path.read_text(encoding="utf-8"), "old topology\n")
                temp_path = Path(cmd[cmd.index("--write-topology") + 1])
                mode = "a" if "--append-topology" in cmd else "w"
                calls.append(cmd)
                with temp_path.open(mode, encoding="utf-8", newline="\n") as handle:
                    handle.write(f"switch call={len(calls)}\n")
                return SimpleNamespace(returncode=0)

            argv = [
                "scalance_switches_refresh.py",
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


if __name__ == "__main__":
    unittest.main()
