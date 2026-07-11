import subprocess
import unittest
from pathlib import Path
from types import SimpleNamespace

from scripts.build.firmware_identity import UNKNOWN_COMMIT, read_firmware_identity


class FirmwareIdentityBuildScriptTest(unittest.TestCase):
    def test_reads_clean_and_dirty_git_identity(self):
        calls = []

        def clean_runner(command, **kwargs):
            calls.append((command, kwargs))
            output = "a" * 40 + "\n" if command[1] == "rev-parse" else ""
            return SimpleNamespace(stdout=output)

        self.assertEqual(
            read_firmware_identity(Path("."), runner=clean_runner),
            ("a" * 40, False),
        )
        self.assertEqual(len(calls), 2)

        def dirty_runner(command, **_kwargs):
            output = "b" * 40 + "\n" if command[1] == "rev-parse" else " M src/main.cpp\n"
            return SimpleNamespace(stdout=output)

        self.assertEqual(
            read_firmware_identity(Path("."), runner=dirty_runner),
            ("b" * 40, True),
        )

    def test_fails_closed_when_git_identity_is_missing_or_invalid(self):
        def failing_runner(_command, **_kwargs):
            raise subprocess.CalledProcessError(1, "git")

        self.assertEqual(
            read_firmware_identity(Path("."), runner=failing_runner),
            (UNKNOWN_COMMIT, True),
        )

        def invalid_runner(_command, **_kwargs):
            return SimpleNamespace(stdout="short\n")

        self.assertEqual(
            read_firmware_identity(Path("."), runner=invalid_runner),
            (UNKNOWN_COMMIT, True),
        )


if __name__ == "__main__":
    unittest.main()
