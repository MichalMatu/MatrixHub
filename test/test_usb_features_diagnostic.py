import unittest

from scripts.diagnostics.check_usb_features import exercise_safe_actions, validate_state


def minimal_state():
    return {
        "keyboard": {"enabled": False},
        "airmouse": {
            "movement_enabled": False,
            "click_enabled": False,
            "running": False,
            "calibrating": False,
            "jiggler": {"enabled": False},
        },
        "usb_terminal": {
            "enabled": False,
            "idle_timeout_ms": 2000,
            "target_port": "/dev/ttyUSB0",
        },
    }


class UsbFeaturesDiagnosticTest(unittest.TestCase):
    def test_validate_state_accepts_minimal_contract_shape(self):
        self.assertEqual(validate_state(minimal_state()), [])

    def test_validate_state_reports_missing_and_mistyped_fields(self):
        state = minimal_state()
        del state["airmouse"]["running"]
        state["keyboard"]["enabled"] = "yes"
        state["usb_terminal"]["idle_timeout_ms"] = "2000"
        state["usb_terminal"]["target_port"] = None

        errors = validate_state(state)

        self.assertIn("/api/keyboard/config missing boolean enabled", errors)
        self.assertIn("/api/airmouse/status missing running", errors)
        self.assertIn("/api/usbterminal/config missing integer idle_timeout_ms", errors)
        self.assertIn("/api/usbterminal/config missing string target_port", errors)

    def test_validate_state_reports_missing_sections_without_traceback(self):
        errors = validate_state({})

        self.assertIn("state missing object keyboard", errors)
        self.assertIn("state missing object airmouse", errors)
        self.assertIn("state missing object usb_terminal", errors)

    def test_exercise_safe_actions_skips_disabled_features(self):
        self.assertEqual(
            exercise_safe_actions(None, minimal_state()),
            {
                "keyboard_press_f13": {"skipped": "keyboard_disabled"},
                "airmouse_calibrate": {"skipped": "airmouse_not_running"},
            },
        )


if __name__ == "__main__":
    unittest.main()
