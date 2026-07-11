import unittest
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from types import SimpleNamespace
from unittest.mock import patch

from scripts.tests.device_smoke import (
    firmware_identity_allows_mutation,
    main,
    validate_expected_firmware_identity,
    validate_system_info,
)


EXPECTED_SHA = "0123456789abcdef0123456789abcdef01234567"


def system_info(**overrides):
    payload = {
        "firmware_version": "1.0.0",
        "firmware_commit": EXPECTED_SHA,
        "firmware_dirty": False,
        "free_heap": 100,
        "total_heap": 200,
        "uptime": 30,
    }
    payload.update(overrides)
    return payload


class DeviceSmokeFirmwareIdentityTests(unittest.TestCase):
    def test_system_info_requires_typed_build_identity(self):
        self.assertEqual(validate_system_info(system_info()), [])

        missing = system_info()
        del missing["firmware_commit"]
        self.assertIn("system_info.firmware_commit is missing", validate_system_info(missing))
        self.assertIn(
            "system_info.firmware_commit must be a 40-character hexadecimal SHA",
            validate_system_info(system_info(firmware_commit="unknown")),
        )
        self.assertIn(
            "system_info.firmware_dirty must be bool",
            validate_system_info(system_info(firmware_dirty=1)),
        )

    def test_expected_identity_rejects_wrong_or_dirty_firmware(self):
        other_sha = "f" * 40
        mismatch = validate_expected_firmware_identity(system_info(), other_sha)
        self.assertEqual(len(mismatch), 1)
        self.assertIn("firmware commit mismatch", mismatch[0])

        self.assertEqual(
            validate_expected_firmware_identity(system_info(firmware_dirty=True), EXPECTED_SHA),
            ["firmware reports a dirty build"],
        )
        self.assertEqual(
            validate_expected_firmware_identity(
                system_info(firmware_dirty=True),
                EXPECTED_SHA.upper(),
                allow_dirty=True,
            ),
            [],
        )

    def test_mutation_gate_requires_both_successful_identity_checks(self):
        passed = [
            {"name": "firmware.identity.read", "ok": True},
            {"name": "firmware.identity.verify", "ok": True},
        ]
        self.assertTrue(firmware_identity_allows_mutation(passed))
        self.assertFalse(firmware_identity_allows_mutation(passed[:1]))
        self.assertFalse(
            firmware_identity_allows_mutation(
                [passed[0], {"name": "firmware.identity.verify", "ok": False}]
            )
        )

    def test_main_never_runs_mutations_after_identity_failure(self):
        failure_sets = {
            "mismatch": [
                {"name": "firmware.identity.read", "ok": True, "latency_ms": 0.0},
                {
                    "name": "firmware.identity.verify",
                    "ok": False,
                    "latency_ms": 0.0,
                    "error": "firmware commit mismatch",
                },
            ],
            "dirty": [
                {"name": "firmware.identity.read", "ok": True, "latency_ms": 0.0},
                {
                    "name": "firmware.identity.verify",
                    "ok": False,
                    "latency_ms": 0.0,
                    "error": "firmware reports a dirty build",
                },
            ],
            "failed_read": [
                {
                    "name": "firmware.identity.read",
                    "ok": False,
                    "latency_ms": 0.0,
                    "error": "malformed response",
                }
            ],
        }

        for label, identity_results in failure_sets.items():
            with self.subTest(label=label):
                client = SimpleNamespace(base_url="https://matrixhub.test", retries=0)
                ok_result = {
                    "name": "probe",
                    "method": "GET",
                    "path": "",
                    "ok": True,
                    "latency_ms": 0.0,
                }
                with (
                    patch("scripts.tests.device_smoke.DeviceClient.from_args", return_value=client),
                    patch("scripts.tests.device_smoke.enforce_https"),
                    patch("scripts.tests.device_smoke.login_with_retry"),
                    patch("scripts.tests.device_smoke.run_reauth_probe", return_value=ok_result),
                    patch(
                        "scripts.tests.device_smoke.run_firmware_identity_check",
                        return_value=identity_results,
                    ),
                    patch(
                        "scripts.tests.device_smoke.http_json_check",
                        return_value=(ok_result, {}),
                    ),
                    patch("scripts.tests.device_smoke.run_read_only", return_value=([], {})),
                    patch("scripts.tests.device_smoke.run_safe_writes") as safe_writes,
                    patch("scripts.tests.device_smoke.run_power_sleep_smoke") as sleep_smoke,
                    patch("scripts.tests.device_smoke.run_restart_check") as restart,
                    redirect_stdout(StringIO()),
                    redirect_stderr(StringIO()),
                ):
                    exit_code = main(
                        [
                            "--device-url",
                            "https://matrixhub.test",
                            "--expected-firmware-commit",
                            EXPECTED_SHA,
                            "--safe-writes",
                            "--power-sleep-smoke",
                            "--restart",
                            "--no-report-files",
                        ]
                    )

                self.assertEqual(exit_code, 1)
                safe_writes.assert_not_called()
                sleep_smoke.assert_not_called()
                restart.assert_not_called()


if __name__ == "__main__":
    unittest.main()
