import unittest

from scripts.diagnostics.check_macros import script_list_contains, validate_state


def minimal_state():
    return {
        "settings": {
            "enabled": False,
            "boot_script": "",
            "boot_delay": 5000,
        },
        "status": {
            "current_script": "",
            "status": "IDLE",
            "current_line": 0,
            "uptime_ms": 0,
            "last_error": "",
        },
        "scripts": [{"name": "demo.txt"}],
    }


class MacroDiagnosticTest(unittest.TestCase):
    def test_validate_state_accepts_minimal_contract_shape(self):
        self.assertEqual(validate_state(minimal_state()), [])

    def test_validate_state_reports_invalid_status_and_script_shape(self):
        state = minimal_state()
        state["status"]["status"] = "BROKEN"
        state["scripts"] = [{"title": "missing-name"}]

        errors = validate_state(state)

        self.assertIn("/api/macros/status invalid status 'BROKEN'", errors)
        self.assertIn("/api/macros script[0] missing string name", errors)

    def test_validate_state_reports_missing_sections_without_traceback(self):
        errors = validate_state({})

        self.assertIn("state missing object settings", errors)
        self.assertIn("state missing object status", errors)
        self.assertIn("state missing array scripts", errors)

    def test_script_list_contains_uses_list_membership(self):
        state = minimal_state()

        self.assertTrue(script_list_contains(state, "demo.txt"))
        self.assertFalse(script_list_contains(state, "missing.txt"))
        self.assertFalse(script_list_contains({"scripts": "not-a-list"}, "demo.txt"))


if __name__ == "__main__":
    unittest.main()
