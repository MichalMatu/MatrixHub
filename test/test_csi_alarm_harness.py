import json
import tempfile
import unittest
from pathlib import Path

from scripts.sensing_analysis.csi_alarm_harness import (
    check_expectation,
    parse_jsonl,
    run_detector,
    synthetic_frames,
)


class CsiAlarmHarnessTest(unittest.TestCase):
    def test_quiet_synthetic_does_not_confirm_motion(self):
        summary = run_detector(synthetic_frames("quiet"))
        ok, _message = check_expectation(summary, "quiet")

        self.assertTrue(ok)
        self.assertEqual(summary["motion_confirmed_frames"], 0)
        self.assertEqual(summary["noisy_frames"], 0)
        self.assertTrue(summary["baseline_ready"])

    def test_motion_synthetic_confirms_motion_without_noisy_state(self):
        summary = run_detector(synthetic_frames("motion"))
        ok, _message = check_expectation(summary, "motion")

        self.assertTrue(ok)
        self.assertGreater(summary["motion_confirmed_frames"], 0)
        self.assertEqual(summary["noisy_frames"], 0)

    def test_noisy_synthetic_is_not_treated_as_motion(self):
        summary = run_detector(synthetic_frames("noisy"))
        ok, _message = check_expectation(summary, "noisy")

        self.assertTrue(ok)
        self.assertEqual(summary["motion_confirmed_frames"], 0)
        self.assertGreater(summary["noisy_frames"], 0)

    def test_jsonl_loader_accepts_hex_payload(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "capture.jsonl"
            path.write_text(
                json.dumps({"timestamp_ms": 100, "rssi": -66, "payload_hex": "0102fd04"}) + "\n",
                encoding="utf-8",
            )

            frames = parse_jsonl(path)

        self.assertEqual(len(frames), 1)
        self.assertEqual(frames[0].timestamp_ms, 100)
        self.assertEqual(frames[0].payload, (1, 2, -3, 4))


if __name__ == "__main__":
    unittest.main()
