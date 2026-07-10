import json
import struct
import tempfile
import unittest
from dataclasses import asdict
from pathlib import Path
from types import SimpleNamespace

from scripts.sensing_analysis.csi_capture import (
    CAPTURE_FILENAME,
    MANIFEST_FILENAME,
    MANIFEST_SCHEMA,
    SCENARIO_FILENAME,
    CaptureWorkflowError,
    annotate_capture,
    atomic_write_json,
    build_parser,
    initial_scenario,
    parse_milliseconds,
    promote_capture,
    require_safe_raw_root,
    sha256_file,
    verify_capture,
)
from scripts.sensing_analysis.csi_capture_format import (
    BATCH_DATA,
    BATCH_END,
    BATCH_ERROR,
    BATCH_HELLO,
    CaptureEnd,
    CaptureFormatError,
    CaptureHello,
    CsiFrame,
    END_PAYLOAD,
    FORMAT_MAJOR,
    FORMAT_MINOR,
    FRAME_FLAG_FIRST_WORD_INVALID,
    FRAME_FLAG_REPLAY_ORIGIN,
    FRAME_FLAG_OBSERVED_MOTION,
    FRAME_HEADER_SIZE,
    HELLO_PAYLOAD,
    MHCB_HEADER,
    MHCB_HEADER_SIZE,
    MhcfWriter,
    decode_capture_end,
    decode_capture_error,
    decode_capture_hello,
    decode_mhcb_frames,
    float32_bits,
    iter_mhcf_frames,
    validate_mhcf,
    write_mhcf,
)


def make_frame(
    sequence: int,
    process_ms: int,
    source: bytes,
    destination: bytes,
    *,
    replay_origin: bool = False,
) -> CsiFrame:
    return CsiFrame(
        accepted_sequence=sequence,
        process_now_ms=process_ms,
        rx_timestamp_us=(0xF1020304 + (sequence & 0xFF)) & 0xFFFFFFFF,
        gain_bits=float32_bits(1.234567),
        observed_motion_score_bits=float32_bits(91.125),
        original_len=16,
        rx_sequence=(0x4567 + (sequence & 0xFF)) & 0xFFFF,
        signal_len=256,
        source_mac=source,
        destination_mac=destination,
        rssi=-67,
        noise_floor=-96,
        rate=7,
        signal_mode=1,
        mcs=5,
        cwb=1,
        smoothing=1,
        not_sounding=1,
        aggregation=1,
        stbc=2,
        fec=1,
        sgi=1,
        ampdu_count=3,
        channel=6,
        secondary_channel=1,
        antenna=1,
        rx_state=4,
        flags=(
            FRAME_FLAG_FIRST_WORD_INVALID
            | FRAME_FLAG_OBSERVED_MOTION
            | (FRAME_FLAG_REPLAY_ORIGIN if replay_origin else 0)
        ),
        iq=bytes((10, 0, 10, 1, 10, 2, 10, 3, 10, 4, 10, 5, 10, 6, 10, 7)),
    )


def batch_header(message_type: int, record_count: int, session_id: int) -> bytes:
    return MHCB_HEADER.pack(
        b"MHCB",
        FORMAT_MAJOR,
        FORMAT_MINOR,
        message_type,
        MHCB_HEADER_SIZE,
        FRAME_HEADER_SIZE,
        record_count,
        session_id,
    )


class CsiCaptureFormatTest(unittest.TestCase):
    def test_timeline_parser_accepts_zero_origin(self):
        self.assertEqual(parse_milliseconds("0"), 0)
        self.assertEqual(parse_milliseconds("1.5s"), 1500)

    def test_mhcf_round_trip_preserves_exact_inputs_and_observed_diagnostics(self):
        source = bytes.fromhex("101122334455")
        destination = bytes.fromhex("aabbccddeeff")
        frames = [
            make_frame(0xFFFFFFFE, 1000, source, destination, replay_origin=True),
            make_frame(0xFFFFFFFF, 1100, source, destination),
        ]
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / CAPTURE_FILENAME
            write_mhcf(path, 0xA1B2C3D4, frames)
            validation = validate_mhcf(path)
            decoded = list(iter_mhcf_frames(path))

        self.assertEqual(validation.header.frame_count, 2)
        self.assertEqual(validation.first_sequence, 0xFFFFFFFE)
        self.assertEqual(validation.last_sequence, 0xFFFFFFFF)
        self.assertEqual(decoded, frames)
        self.assertEqual(decoded[0].gain_bits, float32_bits(1.234567))
        self.assertTrue(decoded[0].observed_motion)

    def test_mhcf_rejects_sequence_gap_and_trailing_bytes(self):
        source = bytes.fromhex("101122334455")
        destination = bytes.fromhex("aabbccddeeff")
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / CAPTURE_FILENAME
            write_mhcf(
                path,
                1,
                [
                    make_frame(10, 1000, source, destination, replay_origin=True),
                    make_frame(11, 1100, source, destination),
                ],
            )
            data = bytearray(path.read_bytes())
            second_offset = 32 + FRAME_HEADER_SIZE + 16
            struct.pack_into("<I", data, second_offset + 4, 12)
            path.write_bytes(data)
            with self.assertRaisesRegex(CaptureFormatError, "sequence gap"):
                list(iter_mhcf_frames(path))

            path.write_bytes(bytes(data) + b"x")
            with self.assertRaisesRegex(CaptureFormatError, "size mismatch"):
                validate_mhcf(path)

    def test_mhcf_requires_exactly_one_initial_replay_origin(self):
        source = bytes.fromhex("101122334455")
        destination = bytes.fromhex("aabbccddeeff")
        first = make_frame(10, 1000, source, destination, replay_origin=True)
        second_origin = make_frame(11, 1100, source, destination, replay_origin=True)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / CAPTURE_FILENAME
            with MhcfWriter(path, 1) as writer:
                writer.append(first)
                size_after_first = path.stat().st_size
                with self.assertRaisesRegex(CaptureFormatError, "only valid on the first"):
                    writer.append(second_origin)
                self.assertEqual(writer.frame_count, 1)
                self.assertEqual(path.stat().st_size, size_after_first)

            raw = bytearray()
            valid_path = Path(tmpdir) / "valid.mhcf"
            write_mhcf(valid_path, 2, [first])
            raw.extend(valid_path.read_bytes())
            raw[32 + 61] &= ~FRAME_FLAG_REPLAY_ORIGIN
            missing_path = Path(tmpdir) / "missing-origin.mhcf"
            missing_path.write_bytes(raw)
            with self.assertRaisesRegex(CaptureFormatError, "no replay-origin"):
                validate_mhcf(missing_path)

    def test_collect_parser_exposes_json_output_flag(self):
        args = build_parser().parse_args(
            [
                "collect",
                "--scenario-id",
                "smoke",
                "--firmware-commit",
                "a" * 40,
                "--json",
            ]
        )
        self.assertTrue(args.json)

    def test_raw_output_inside_repo_is_limited_to_ignored_artifacts(self):
        from scripts.sensing_analysis.csi_capture import REPO_ROOT

        allowed = REPO_ROOT / "artifacts" / "csi" / "raw"
        self.assertEqual(require_safe_raw_root(allowed), allowed.resolve())
        with self.assertRaisesRegex(CaptureWorkflowError, "must stay under ignored"):
            require_safe_raw_root(REPO_ROOT / "test" / "fixtures" / "csi")

    def test_mhcb_control_and_data_messages_use_canonical_record(self):
        session_id = 0x11223344
        hello_payload = HELLO_PAYLOAD.pack(
            100,
            10,
            5,
            5,
            0,
            60000,
            512,
            10,
            64,
            0x00FF,
            0,
            0,
        )
        hello_header, hello = decode_capture_hello(
            batch_header(BATCH_HELLO, 0, session_id) + hello_payload
        )
        self.assertEqual(hello_header.session_id, session_id)
        self.assertEqual(hello.max_iq_bytes, 512)

        frame = make_frame(
            7,
            200,
            bytes.fromhex("101122334455"),
            bytes.fromhex("aabbccddeeff"),
            replay_origin=True,
        )
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / CAPTURE_FILENAME
            with MhcfWriter(path, session_id) as writer:
                writer.append(frame)
                writer.finalize()
            raw_record = path.read_bytes()[32:]
        data_header, decoded = decode_mhcb_frames(
            batch_header(BATCH_DATA, 1, session_id) + raw_record
        )
        self.assertEqual(data_header.record_count, 1)
        self.assertEqual(decoded, (frame,))

        end_values = (200, 1, 7, 7, 1, 1, 0, 1, 1, 0, 0, 20, 6, 6, 0, 0)
        end_header, end = decode_capture_end(
            batch_header(BATCH_END, 0, session_id) + END_PAYLOAD.pack(*end_values)
        )
        self.assertEqual(end_header.session_id, session_id)
        self.assertEqual(end.records_enqueued, 1)

        error_header, error_code = decode_capture_error(
            batch_header(BATCH_ERROR, 0, session_id) + struct.pack("<HH", 1, 0)
        )
        self.assertEqual(error_header.session_id, session_id)
        self.assertEqual(error_code, 1)

        with self.assertRaisesRegex(CaptureFormatError, "exceeds v1 limit"):
            decode_mhcb_frames(batch_header(BATCH_DATA, 11, session_id))


class CsiCaptureWorkflowTest(unittest.TestCase):
    def create_raw_capture(self, root: Path) -> tuple[Path, bytes, bytes]:
        capture_dir = root / "raw-capture"
        capture_dir.mkdir()
        capture_path = capture_dir / CAPTURE_FILENAME
        source = bytes.fromhex("34123456789a")
        destination = bytes.fromhex("5cabcdef0123")
        frames = [
            make_frame(40, 1000, source, destination, replay_origin=True),
            make_frame(41, 1100, source, destination),
        ]
        header = write_mhcf(capture_path, 0xA1B2C3D4, frames)
        config = {
            "enabled": True,
            "csi_alarm": {
                "enabled": True,
                "bands": [{"start": 0, "end": 7}],
                "baseline_frames": 30,
                "top_k": 4,
                "auto_recalibration": False,
            },
        }
        system_info = {
            "firmware_version": "1.0.0-test",
            "firmware_built_target": "esp32s3",
            "esp_platform": "ESP32-S3",
            "sdk_version": "5.5.1",
            "arduino_version": "3.x",
            "mac_address": "34:12:34:56:78:9a",
        }
        scenario = initial_scenario(
            "walk-room-01",
            "Operator walked across the room.",
            capture_path,
            config,
            system_info,
            "a" * 40,
        )
        atomic_write_json(capture_dir / SCENARIO_FILENAME, scenario)
        hello = CaptureHello(900, 10, 39, 39, 3, 60000, 512, 10, 0x00FF, 0, 7)
        end = CaptureEnd(1200, 1, 40, 41, 2, 2, 0, 1, 1, 0, 0, 20, 41, 41, 3, 0)
        manifest = {
            "schema": MANIFEST_SCHEMA,
            "state": "complete",
            "source_kind": "real_device",
            "scenario_id": "walk-room-01",
            "file": {
                "name": CAPTURE_FILENAME,
                "sha256": f"sha256:{sha256_file(capture_path)}",
                "bytes": capture_path.stat().st_size,
                "frame_count": header.frame_count,
                "frames_section_bytes": header.frames_section_bytes,
            },
            "transport": {
                "batch_count": 1,
                "hello": asdict(hello),
                "end": asdict(end),
                "first_sequence": 40,
                "last_sequence": 41,
            },
            "snapshots": {
                "config_before": config,
                "config_after": json.loads(json.dumps(config)),
                "status_before": {},
                "status_after": {},
                "system_info": system_info,
            },
        }
        atomic_write_json(capture_dir / MANIFEST_FILENAME, manifest)
        return capture_dir, source, destination

    def mark_scenario_reviewed(self, capture_dir: Path) -> None:
        scenario_path = capture_dir / SCENARIO_FILENAME
        scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
        scenario["ground_truth"]["reviewed"] = True
        scenario["ground_truth"]["timeline"][0].update(
            {
                "motion": "none",
                "occupancy": "empty",
                "environment": "stable",
                "evidence": "operator",
                "confidence": "high",
            }
        )
        atomic_write_json(scenario_path, scenario)

    def test_annotate_review_promote_anonymizes_macs_and_keeps_ground_truth_separate(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, source, destination = self.create_raw_capture(root)
            annotation_args = SimpleNamespace(
                target=capture_dir,
                review=False,
                start_ms=10,
                end_ms=90,
                motion="present",
                occupancy="occupied",
                environment="stable",
                evidence="operator",
                confidence="high",
                note="Walk from left to right.",
                replace=False,
            )
            annotate_capture(annotation_args)
            review_args = SimpleNamespace(**{**vars(annotation_args), "review": True})
            annotate_capture(review_args)

            fixture_root = root / "fixtures"
            promoted = promote_capture(
                SimpleNamespace(
                    target=capture_dir,
                    fixture_id="walk-room-real-01",
                    output_root=fixture_root,
                    reviewed=True,
                )
            )
            result = verify_capture(promoted)
            promoted_bytes = (promoted / CAPTURE_FILENAME).read_bytes()
            promoted_frames = list(iter_mhcf_frames(promoted / CAPTURE_FILENAME))
            promoted_scenario = json.loads(
                (promoted / SCENARIO_FILENAME).read_text(encoding="utf-8")
            )

        self.assertTrue(result["ok"])
        self.assertNotIn(source, promoted_bytes)
        self.assertNotIn(destination, promoted_bytes)
        self.assertEqual(promoted_frames[0].source_mac, bytes.fromhex("020000000001"))
        self.assertEqual(promoted_frames[0].destination_mac, bytes.fromhex("020000000002"))
        self.assertTrue(promoted_scenario["ground_truth"]["reviewed"])
        self.assertEqual(promoted_scenario["ground_truth"]["timeline"][1]["motion"], "present")
        self.assertNotIn("mac_address", json.dumps(promoted_scenario))

    def test_promotion_rejects_transport_loss(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, _, _ = self.create_raw_capture(root)
            manifest_path = capture_dir / MANIFEST_FILENAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["transport"]["end"]["records_dropped"] = 1
            atomic_write_json(manifest_path, manifest)

            with self.assertRaisesRegex(CaptureWorkflowError, "loss/truncation"):
                promote_capture(
                    SimpleNamespace(
                        target=capture_dir,
                        fixture_id="invalid-loss-01",
                        output_root=root / "fixtures",
                        reviewed=True,
                    )
                )

    def test_promotion_requires_persisted_ground_truth_review(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, _, _ = self.create_raw_capture(root)
            with self.assertRaisesRegex(CaptureWorkflowError, "not been marked reviewed"):
                promote_capture(
                    SimpleNamespace(
                        target=capture_dir,
                        fixture_id="invalid-unreviewed-01",
                        output_root=root / "fixtures",
                        reviewed=True,
                    )
                )

    def test_promotion_rejects_sensitive_free_text(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, _, _ = self.create_raw_capture(root)
            self.mark_scenario_reviewed(capture_dir)
            scenario_path = capture_dir / SCENARIO_FILENAME
            scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
            scenario["description"] = "Captured on SSID PrivateHomeNetwork"
            atomic_write_json(scenario_path, scenario)

            with self.assertRaisesRegex(CaptureWorkflowError, "sensitive scenario metadata"):
                promote_capture(
                    SimpleNamespace(
                        target=capture_dir,
                        fixture_id="invalid-sensitive-text-01",
                        output_root=root / "fixtures",
                        reviewed=True,
                    )
                )

    def test_promotion_requires_clean_firmware_provenance(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, _, _ = self.create_raw_capture(root)
            self.mark_scenario_reviewed(capture_dir)
            scenario_path = capture_dir / SCENARIO_FILENAME
            scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
            scenario["source"]["firmware_commit"] = f"{'a' * 40}-dirty"
            atomic_write_json(scenario_path, scenario)

            with self.assertRaisesRegex(CaptureWorkflowError, "clean 40-hex"):
                promote_capture(
                    SimpleNamespace(
                        target=capture_dir,
                        fixture_id="invalid-provenance-01",
                        output_root=root / "fixtures",
                        reviewed=True,
                    )
                )

    def test_verify_rejects_manifest_sequence_mismatch(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, _, _ = self.create_raw_capture(root)
            manifest_path = capture_dir / MANIFEST_FILENAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["transport"]["end"]["last_sequence"] = 99
            atomic_write_json(manifest_path, manifest)

            with self.assertRaisesRegex(CaptureWorkflowError, "sequence bounds"):
                verify_capture(capture_dir)

    def test_verify_does_not_reject_drops_outside_fenced_window(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            capture_dir, _, _ = self.create_raw_capture(root)
            manifest_path = capture_dir / MANIFEST_FILENAME
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["transport"]["end"]["source_drops_end"] = 9
            atomic_write_json(manifest_path, manifest)

            self.assertTrue(verify_capture(capture_dir)["ok"])


if __name__ == "__main__":
    unittest.main()
