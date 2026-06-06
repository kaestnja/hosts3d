import importlib.util
import sys
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "scripts" / "scalance_xr328_mirror_check.py"
SPEC = importlib.util.spec_from_file_location("scalance_xr328_mirror_check", SCRIPT_PATH)
mirror_check = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = mirror_check
SPEC.loader.exec_module(mirror_check)


class ScalanceMirrorCheckTests(unittest.TestCase):
    def test_siemens_global_status_uses_inverse_global_encoding(self):
        self.assertEqual(mirror_check.decode_mirror_status("INTEGER: 2"), "enabled")
        self.assertEqual(mirror_check.decode_mirror_status("INTEGER: 1"), "disabled")

    def test_siemens_port_flag_encoding_is_enabled_one_disabled_two(self):
        self.assertIs(mirror_check.decode_enabled_disabled("INTEGER: 1"), True)
        self.assertIs(mirror_check.decode_enabled_disabled("INTEGER: 2"), False)

    def test_lldp_index_and_management_address_suffix_parsing(self):
        key = mirror_check.lldp_index_from_suffix("131513879.10.141")
        self.assertEqual(key, (131513879, 10, 141))

        parsed = mirror_check.lldp_man_addr_from_suffix("131513879.10.141.1.4.192.168.6.39")
        self.assertEqual(parsed, ((131513879, 10, 141), "192.168.6.39"))

    def test_lldp_neighbors_group_by_local_port(self):
        rem_rows = {
            "1.0.8802.1.1.2.1.4.1.1.5.131513879.10.141": 'STRING: "w039s25"',
            "1.0.8802.1.1.2.1.4.1.1.7.131513879.10.141": 'STRING: "port-001"',
            "1.0.8802.1.1.2.1.4.1.1.8.131513879.10.141": 'STRING: "Ethernet Port"',
            "1.0.8802.1.1.2.1.4.1.1.9.131513879.10.141": 'STRING: "W039S25"',
            "1.0.8802.1.1.2.1.4.1.1.10.131513879.10.141": 'STRING: "HP Workstation"',
        }
        man_addr_rows = {
            "1.0.8802.1.1.2.1.4.2.1.3.131513879.10.141.1.4.192.168.6.39": "INTEGER: 2",
        }
        loc_port_ids = {"1.0.8802.1.1.2.1.3.7.1.3.10": 'STRING: "port-010"'}
        loc_port_descs = {"1.0.8802.1.1.2.1.3.7.1.4.10": 'STRING: "Ethernet Port P10"'}

        neighbors = mirror_check.build_lldp_neighbors(rem_rows, man_addr_rows, loc_port_ids, loc_port_descs)

        self.assertEqual(neighbors[10][0]["system_name"], "W039S25")
        self.assertEqual(neighbors[10][0]["management_address"], "192.168.6.39")
        self.assertEqual(neighbors[10][0]["local_port_id"], "port-010")

    def test_extended_mirroring_raw_groups_observed_indices(self):
        sessions = {"1.3.6.1.4.1.4329.20.1.1.1.1.1.6.6.1.2.1": "INTEGER: 1"}
        sources = {
            "1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.2.1.10": "INTEGER: 1",
            "1.3.6.1.4.1.4329.20.1.1.1.1.1.6.7.1.3.1.10": "INTEGER: 2",
        }
        destinations = {"1.3.6.1.4.1.4329.20.1.1.1.1.1.6.9.1.2.1.6": "INTEGER: 1"}
        interfaces = {6: {"if_index": 6, "if_name": "P0.6"}, 10: {"if_index": 10, "if_name": "P0.10"}}

        raw = mirror_check.build_extended_mirroring_raw(sessions, sources, destinations, interfaces)

        self.assertEqual(raw["interpretation"], "raw_grouped_by_observed_indices")
        self.assertEqual(raw["sources"][0]["session_id"], 1)
        self.assertEqual(raw["sources"][0]["source_id"], 10)
        self.assertEqual(raw["sources"][0]["interface"]["if_name"], "P0.10")
        self.assertEqual(raw["destinations"][0]["destination_id"], 6)


if __name__ == "__main__":
    unittest.main()
