import unittest

from scripts.diagnostics.check_heartbeat import (
    active_slot_count,
    heartbeat_payload_for_restore,
    validate_settings as validate_heartbeat_settings,
)
from scripts.diagnostics.check_udp import (
    udp_payload_for_restore,
    validate_settings as validate_udp_settings,
)


def valid_udp_settings():
    return {
        "enabled": True,
        "host": "192.168.0.25",
        "port": 8094,
        "format": "json",
        "interval_ms": 60000,
    }


def valid_heartbeat_settings():
    return {
        "interval_ms": 300000,
        "slots": [
            {
                "enabled": True,
                "name": "Diagnostics",
                "url": "https://example.com/ping",
                "allow_insecure": False,
            },
            {"enabled": False, "name": "", "url": "", "allow_insecure": False},
            {"enabled": False, "name": "", "url": "", "allow_insecure": False},
            {"enabled": False, "name": "", "url": "", "allow_insecure": False},
        ],
    }


class UdpDiagnosticContractTest(unittest.TestCase):
    def test_udp_validator_accepts_minimal_valid_settings(self):
        self.assertEqual(validate_udp_settings(valid_udp_settings()), [])

    def test_udp_validator_reports_invalid_enabled_target(self):
        settings = valid_udp_settings()
        settings["host"] = ""
        settings["port"] = 0

        errors = validate_udp_settings(settings)

        self.assertIn("/api/udp missing valid integer port", errors)
        self.assertIn("/api/udp enabled but host is empty", errors)

    def test_udp_restore_payload_preserves_known_fields(self):
        settings = valid_udp_settings()

        self.assertEqual(udp_payload_for_restore(settings), settings)


class HeartbeatDiagnosticContractTest(unittest.TestCase):
    def test_heartbeat_validator_accepts_four_slot_contract(self):
        self.assertEqual(validate_heartbeat_settings(valid_heartbeat_settings()), [])

    def test_heartbeat_validator_reports_empty_enabled_url(self):
        settings = valid_heartbeat_settings()
        settings["slots"][0]["url"] = ""

        errors = validate_heartbeat_settings(settings)

        self.assertIn("/api/heartbeat slot[0] enabled but url is empty", errors)

    def test_heartbeat_restore_payload_pads_slots_and_counts_active(self):
        settings = {
            "interval_ms": 60000,
            "slots": [
                {
                    "enabled": True,
                    "name": "A",
                    "url": "https://example.com/a",
                    "allow_insecure": False,
                }
            ],
        }

        payload = heartbeat_payload_for_restore(settings)

        self.assertEqual(4, len(payload["slots"]))
        self.assertEqual(1, active_slot_count(payload))


if __name__ == "__main__":
    unittest.main()
