import asyncio
import json
import struct
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from dataclasses import asdict
from io import StringIO
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from scripts.sensing_analysis.csi_capture import (
    CAPTURE_FILENAME,
    MANIFEST_FILENAME,
    MANIFEST_SCHEMA,
    SCENARIO_FILENAME,
    CaptureWorkflowError,
    annotate_capture,
    atomic_write_json,
    build_parser,
    capture_cleanup_complete,
    collect_capture,
    firmware_provenance,
    initial_scenario,
    parse_milliseconds,
    promote_capture,
    receive_data_until_end,
    require_capture_ready_config,
    require_capture_ready_status,
    require_safe_raw_root,
    set_acceptance,
    sha256_file,
    verify_capture,
    wait_for_capture_cleanup,
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
from scripts.tests.run_csi_fixture_replay import (
    CorpusError,
    discover_fixture_directories,
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

    def test_release_corpus_discovery_rejects_an_empty_root(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            with self.assertRaisesRegex(CorpusError, "corpus is empty"):
                discover_fixture_directories(Path(tmpdir))

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

    def test_mhcf_process_time_is_monotonic_modulo_uint32(self):
        source = bytes.fromhex("101122334455")
        destination = bytes.fromhex("aabbccddeeff")
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / CAPTURE_FILENAME
            write_mhcf(
                path,
                1,
                [
                    make_frame(
                        10,
                        0xFFFFFFF0,
                        source,
                        destination,
                        replay_origin=True,
                    ),
                    make_frame(11, 0x00000010, source, destination),
                ],
            )
            self.assertEqual(validate_mhcf(path).header.frame_count, 2)

            with self.assertRaisesRegex(
                CaptureFormatError, "non-monotonic modular processNowMs"
            ):
                write_mhcf(
                    path,
                    1,
                    [
                        make_frame(
                            20,
                            1000,
                            source,
                            destination,
                            replay_origin=True,
                        ),
                        make_frame(21, 900, source, destination),
                    ],
                )

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
        self.assertEqual(args.cleanup_timeout, 15.0)

        configured = build_parser().parse_args(
            [
                "collect",
                "--scenario-id",
                "smoke",
                "--firmware-commit",
                "a" * 40,
                "--cleanup-timeout",
                "750ms",
            ]
        )
        self.assertEqual(configured.cleanup_timeout, 0.75)

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
    @staticmethod
    def capture_ready_status() -> dict:
        return {
            "csi": {
                "enabled": True,
                "runtime_fault": False,
                "runtime_reconcile_pending": False,
                "queue_allocated": True,
                "calibration_state": "forced",
                "motion": {
                    "enabled": True,
                    "state": "calibrating",
                    "has_frame": True,
                    "data_fresh": True,
                },
            }
        }

    @staticmethod
    def capture_cleanup_status(*, active: bool = False) -> dict:
        return {
            "csi": {
                "diagnostic_capture_consumer_active": active,
                "capture": {
                    "queue_enabled": active,
                    "client_count": 1 if active else 0,
                    "starting": False,
                    "accepting": active,
                    "stopping": False,
                },
            }
        }

    def test_capture_cleanup_requires_exact_typed_quiescent_fields(self):
        self.assertTrue(capture_cleanup_complete(self.capture_cleanup_status()))
        self.assertFalse(
            capture_cleanup_complete(self.capture_cleanup_status(active=True))
        )

        invalid_statuses = []
        missing_consumer = self.capture_cleanup_status()
        del missing_consumer["csi"]["diagnostic_capture_consumer_active"]
        invalid_statuses.append(missing_consumer)
        numeric_consumer = self.capture_cleanup_status()
        numeric_consumer["csi"]["diagnostic_capture_consumer_active"] = 0
        invalid_statuses.append(numeric_consumer)
        for field in ("queue_enabled", "starting", "accepting", "stopping"):
            status = self.capture_cleanup_status()
            status["csi"]["capture"][field] = 0
            invalid_statuses.append(status)
        boolean_client_count = self.capture_cleanup_status()
        boolean_client_count["csi"]["capture"]["client_count"] = False
        invalid_statuses.append(boolean_client_count)

        for status in invalid_statuses:
            with self.subTest(status=status), self.assertRaisesRegex(
                CaptureWorkflowError,
                "capture cleanup status",
            ):
                capture_cleanup_complete(status)

    def test_capture_cleanup_poll_records_immediate_and_settled_status(self):
        class FakeClock:
            def __init__(self):
                self.now = 10.0

            def monotonic(self):
                return self.now

            async def sleep(self, seconds):
                self.now += seconds

        clock = FakeClock()
        active = self.capture_cleanup_status(active=True)
        settled = self.capture_cleanup_status()
        statuses = iter((active, settled))
        request_timeouts = []
        updates = []

        def fetch_status(request_timeout):
            request_timeouts.append(request_timeout)
            return next(statuses)

        evidence = asyncio.run(
            wait_for_capture_cleanup(
                fetch_status,
                1.0,
                poll_interval=0.25,
                evidence_hook=updates.append,
                monotonic=clock.monotonic,
                sleep=clock.sleep,
            )
        )

        self.assertTrue(evidence["verified"])
        self.assertEqual(evidence["immediate_status"], active)
        self.assertEqual(evidence["settled_status"], settled)
        self.assertEqual(evidence["poll_count"], 2)
        self.assertEqual(evidence["elapsed_ms"], 250)
        self.assertEqual(request_timeouts, [1.0, 0.75])
        self.assertFalse(updates[0]["verified"])
        self.assertTrue(updates[-1]["verified"])

    def test_capture_cleanup_timeout_and_malformed_status_fail_closed(self):
        class FakeClock:
            def __init__(self):
                self.now = 0.0

            def monotonic(self):
                return self.now

            async def sleep(self, seconds):
                self.now += seconds

        clock = FakeClock()
        active = self.capture_cleanup_status(active=True)
        timeout_updates = []
        with self.assertRaisesRegex(CaptureWorkflowError, "timed out"):
            asyncio.run(
                wait_for_capture_cleanup(
                    lambda _request_timeout: active,
                    0.5,
                    poll_interval=0.25,
                    evidence_hook=timeout_updates.append,
                    monotonic=clock.monotonic,
                    sleep=clock.sleep,
                )
            )
        self.assertEqual(timeout_updates[-1]["immediate_status"], active)
        self.assertEqual(timeout_updates[-1]["settled_status"], active)
        self.assertEqual(timeout_updates[-1]["poll_count"], 2)
        self.assertEqual(timeout_updates[-1]["elapsed_ms"], 500)
        self.assertFalse(timeout_updates[-1]["verified"])

        late_clock = FakeClock()
        late_updates = []

        def late_clean_status(_request_timeout):
            late_clock.now = 0.6
            return self.capture_cleanup_status()

        with self.assertRaisesRegex(CaptureWorkflowError, "timed out"):
            asyncio.run(
                wait_for_capture_cleanup(
                    late_clean_status,
                    0.5,
                    evidence_hook=late_updates.append,
                    monotonic=late_clock.monotonic,
                    sleep=late_clock.sleep,
                )
            )
        self.assertFalse(late_updates[-1]["verified"])
        self.assertEqual(late_updates[-1]["elapsed_ms"], 600)

        malformed = self.capture_cleanup_status()
        del malformed["csi"]["capture"]["accepting"]
        malformed_updates = []
        with self.assertRaisesRegex(CaptureWorkflowError, "accepting must be boolean"):
            asyncio.run(
                wait_for_capture_cleanup(
                    lambda _request_timeout: malformed,
                    1.0,
                    evidence_hook=malformed_updates.append,
                )
            )
        self.assertEqual(malformed_updates[-1]["immediate_status"], malformed)
        self.assertEqual(malformed_updates[-1]["settled_status"], malformed)
        self.assertEqual(malformed_updates[-1]["poll_count"], 1)
        self.assertFalse(malformed_updates[-1]["verified"])

    def test_first_data_frame_emits_stderr_origin_and_invokes_hook_once(self):
        class FakeWebSocket:
            def __init__(self, messages):
                self.messages = list(messages)
                self.sent = []

            async def send(self, message):
                self.sent.append(message)

            async def recv(self):
                return self.messages.pop(0)

        session_id = 0x11223344
        source = bytes.fromhex("101122334455")
        destination = bytes.fromhex("aabbccddeeff")
        frames = (
            make_frame(7, 200, source, destination, replay_origin=True),
            make_frame(8, 250, source, destination),
        )
        end_values = (250, 1, 7, 8, 2, 2, 0, 1, 1, 0, 0, 20, 6, 8, 0, 0)

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            encoded = root / "encoded.mhcf"
            write_mhcf(encoded, session_id, frames)
            data = (
                batch_header(BATCH_DATA, len(frames), session_id)
                + encoded.read_bytes()[32:]
            )
            end_message = (
                batch_header(BATCH_END, 0, session_id)
                + END_PAYLOAD.pack(*end_values)
            )
            websocket = FakeWebSocket((data, end_message))
            writer = MhcfWriter(root / "received.mhcf.partial", session_id)
            origins = []
            stderr = StringIO()
            stdout = StringIO()
            try:
                with patch(
                    "scripts.sensing_analysis.csi_capture.capture_origin_utc",
                    return_value="2026-07-11T12:34:56.123456Z",
                ), patch(
                    "scripts.sensing_analysis.csi_capture.time.monotonic_ns",
                    return_value=987654321,
                ), redirect_stderr(stderr), redirect_stdout(stdout):
                    _, batches = asyncio.run(
                        receive_data_until_end(
                            websocket,
                            writer,
                            session_id,
                            deadline=0.0,
                            stop_timeout=1.0,
                            origin_hook=origins.append,
                        )
                    )
            finally:
                writer.abort()

        self.assertEqual(batches, 1)
        self.assertEqual(websocket.sent, ["STOP"])
        self.assertEqual(stdout.getvalue(), "")
        self.assertEqual(
            stderr.getvalue(),
            "CAPTURE_ORIGIN session=287454020 acceptedSequence=7 "
            "processNowMs=200 utc=2026-07-11T12:34:56.123456Z "
            "monotonicNs=987654321\n",
        )
        self.assertEqual(
            origins,
            [
                {
                    "session": session_id,
                    "accepted_sequence": 7,
                    "process_now_ms": 200,
                    "host_utc": "2026-07-11T12:34:56.123456Z",
                    "host_monotonic_ns": 987654321,
                }
            ],
        )

    def test_capture_requires_enabled_alarm_with_at_least_one_valid_band(self):
        require_capture_ready_config(
            {
                "csi_alarm": {
                    "enabled": True,
                    "bands": [
                        {"start": "invalid", "end": 7},
                        {"start": 8, "end": 15},
                    ],
                }
            }
        )

        invalid_configs = (
            {},
            {"csi_alarm": {"enabled": False, "bands": [{"start": 0, "end": 7}]}},
            {"csi_alarm": {"enabled": True, "bands": []}},
            {"csi_alarm": {"enabled": True, "bands": [{"start": 8, "end": 7}]}},
            {"csi_alarm": {"enabled": True, "bands": [{"start": 0, "end": 256}]}},
            {"csi_alarm": {"enabled": True, "bands": [{"start": True, "end": 7}]}},
        )
        for config in invalid_configs:
            with self.subTest(config=config), self.assertRaises(CaptureWorkflowError):
                require_capture_ready_config(config)

    def test_capture_requires_stable_forced_gain(self):
        require_capture_ready_status(self.capture_ready_status())
        unstable_gain = self.capture_ready_status()
        unstable_gain["csi"]["calibration_state"] = "collecting"
        with self.assertRaises(CaptureWorkflowError):
            require_capture_ready_status(unstable_gain)
        disabled = self.capture_ready_status()
        disabled["csi"]["enabled"] = False
        with self.assertRaises(CaptureWorkflowError):
            require_capture_ready_status(disabled)

    def test_capture_rejects_faulted_reconciling_or_stale_alarm_runtime(self):
        invalid_statuses = []
        for field, value in (
            ("runtime_fault", True),
            ("runtime_reconcile_pending", True),
            ("queue_allocated", False),
        ):
            status = self.capture_ready_status()
            status["csi"][field] = value
            invalid_statuses.append(status)

        for motion_update in (
            {"enabled": False},
            {"state": "unavailable"},
            {"has_frame": False},
            {"data_fresh": False},
        ):
            status = self.capture_ready_status()
            status["csi"]["motion"].update(motion_update)
            invalid_statuses.append(status)

        for status in invalid_statuses:
            with self.subTest(status=status), self.assertRaises(CaptureWorkflowError):
                require_capture_ready_status(status)

    def test_collect_preflight_failure_creates_no_capture_artifact(self):
        valid_config = {
            "csi_alarm": {
                "enabled": True,
                "bands": [{"start": 0, "end": 7}],
            }
        }
        valid_status = self.capture_ready_status()
        cases = (
            (
                "disabled-alarm",
                {"csi_alarm": {"enabled": False, "bands": [{"start": 0, "end": 7}]}},
                valid_status,
            ),
            (
                "missing-band",
                {"csi_alarm": {"enabled": True, "bands": []}},
                valid_status,
            ),
            (
                "unstable-gain",
                valid_config,
                {
                    **valid_status,
                    "csi": {**valid_status["csi"], "calibration_state": "collecting"},
                },
            ),
            (
                "runtime-fault",
                valid_config,
                {**valid_status, "csi": {**valid_status["csi"], "runtime_fault": True}},
            ),
        )

        with tempfile.TemporaryDirectory() as tmpdir:
            for name, config, status in cases:
                with self.subTest(name=name):
                    output_root = Path(tmpdir) / name

                    def response(_client, path):
                        if path == "/api/wifisensing/config":
                            return config
                        if path == "/api/wifisensing/status":
                            return status
                        self.fail(f"unexpected preflight request: {path}")

                    args = SimpleNamespace(
                        scenario_id=name,
                        output_root=output_root,
                    )
                    with patch(
                        "scripts.sensing_analysis.csi_capture.DeviceClient.from_args",
                        return_value=object(),
                    ), patch(
                        "scripts.sensing_analysis.csi_capture.get_json_object",
                        side_effect=response,
                    ):
                        with self.assertRaises(CaptureWorkflowError):
                            asyncio.run(collect_capture(args))

                    self.assertFalse(output_root.exists())

    def test_collect_identity_mismatch_creates_no_capture_artifact(self):
        config = {
            "csi_alarm": {
                "enabled": True,
                "bands": [{"start": 0, "end": 7}],
            }
        }
        status = self.capture_ready_status()
        system_info = {
            "firmware_commit": "b" * 40,
            "firmware_dirty": False,
        }

        def response(_client, path):
            if path == "/api/wifisensing/config":
                return config
            if path == "/api/wifisensing/status":
                return status
            if path == "/api/system/info":
                return system_info
            self.fail(f"unexpected preflight request: {path}")

        with tempfile.TemporaryDirectory() as tmpdir:
            output_root = Path(tmpdir) / "identity-mismatch"
            args = SimpleNamespace(
                scenario_id="identity-mismatch",
                output_root=output_root,
                firmware_commit="a" * 40,
            )
            with patch(
                "scripts.sensing_analysis.csi_capture.DeviceClient.from_args",
                return_value=object(),
            ), patch(
                "scripts.sensing_analysis.csi_capture.get_json_object",
                side_effect=response,
            ):
                with self.assertRaisesRegex(CaptureWorkflowError, "does not match expected"):
                    asyncio.run(collect_capture(args))

            self.assertFalse(output_root.exists())

    def test_collect_cleanup_gate_controls_finalization_and_failure_evidence(self):
        class FakeWebSocket:
            def __init__(self, hello_message):
                self.hello_message = hello_message

            async def send(self, _message):
                return None

            async def recv(self):
                return self.hello_message

        class FakeConnection:
            def __init__(self, websocket):
                self.websocket = websocket

            async def __aenter__(self):
                return self.websocket

            async def __aexit__(self, _exc_type, _exc, _traceback):
                return False

        config = {
            "csi_alarm": {
                "enabled": True,
                "bands": [{"start": 0, "end": 7}],
            }
        }
        ready_status = self.capture_ready_status()
        system_info = {
            "firmware_commit": "a" * 40,
            "firmware_dirty": False,
        }
        session_id = 0xA1B2C3D4
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
        hello_message = (
            batch_header(BATCH_HELLO, 0, session_id) + hello_payload
        )
        source = bytes.fromhex("101122334455")
        destination = bytes.fromhex("aabbccddeeff")

        async def fake_receive(
            _websocket,
            writer,
            received_session_id,
            **kwargs,
        ):
            frame = make_frame(
                6,
                200,
                source,
                destination,
                replay_origin=True,
            )
            writer.append(frame)
            origin_hook = kwargs.get("origin_hook")
            if origin_hook is not None:
                origin_hook(
                    {
                        "session": received_session_id,
                        "accepted_sequence": frame.accepted_sequence,
                        "process_now_ms": frame.process_now_ms,
                        "host_utc": "2026-07-11T12:34:56.123456Z",
                        "host_monotonic_ns": 987654321,
                    }
                )
            return CaptureEnd(
                250,
                1,
                6,
                6,
                1,
                1,
                0,
                1,
                1,
                0,
                0,
                20,
                6,
                6,
                0,
                0,
            ), 1

        self_outer = self

        class FakeClient:
            def __init__(self, cleanup_status):
                self.cleanup_status = cleanup_status
                self.status_calls = 0

            def json(self, method, path, **_kwargs):
                self_outer.assertEqual(method, "GET")
                if path == "/api/wifisensing/config":
                    return config
                if path == "/api/wifisensing/status":
                    self.status_calls += 1
                    if self.status_calls == 1:
                        return ready_status
                    return self.cleanup_status
                if path == "/api/system/info":
                    return system_info
                self_outer.fail(f"unexpected request: {path}")

            def ws_url(self, path):
                self_outer.assertEqual(path, "/ws/csi-capture/v1")
                return "wss://device.test/ws/csi-capture/v1"

            def ws_cookie_header(self):
                return {"Cookie": "test"}

            def ws_ssl_context(self):
                return None

        malformed = self.capture_cleanup_status()
        del malformed["csi"]["capture"]["accepting"]
        cases = (
            ("malformed-cleanup", malformed, 1.0, "accepting must be boolean"),
            (
                "cleanup-timeout",
                self.capture_cleanup_status(active=True),
                0.001,
                "timed out",
            ),
        )

        for scenario_id, cleanup_status, cleanup_timeout, error_pattern in cases:
            with self.subTest(scenario_id=scenario_id), tempfile.TemporaryDirectory() as tmpdir:
                fake_client = FakeClient(cleanup_status)
                output_root = Path(tmpdir) / "raw"
                args = SimpleNamespace(
                    scenario_id=scenario_id,
                    output_root=output_root,
                    firmware_commit="a" * 40,
                    duration=1.0,
                    start_timeout=1.0,
                    stop_timeout=1.0,
                    cleanup_timeout=cleanup_timeout,
                    description="Lifecycle failure proof.",
                )
                with patch(
                    "scripts.sensing_analysis.csi_capture.DeviceClient.from_args",
                    return_value=fake_client,
                ), patch(
                    "scripts.sensing_analysis.csi_capture.connect_ws",
                    return_value=FakeConnection(FakeWebSocket(hello_message)),
                ), patch(
                    "scripts.sensing_analysis.csi_capture.receive_data_until_end",
                    new=fake_receive,
                ):
                    with self.assertRaisesRegex(CaptureWorkflowError, error_pattern):
                        asyncio.run(collect_capture(args))

                outputs = list(output_root.iterdir())
                self.assertEqual(len(outputs), 1)
                self.assertTrue(outputs[0].name.endswith(".partial"))
                manifest = json.loads(
                    (outputs[0] / MANIFEST_FILENAME).read_text(encoding="utf-8")
                )
                cleanup = manifest["collector_cleanup"]
                self.assertEqual(manifest["state"], "incomplete")
                self.assertFalse(cleanup["verified"])
                self.assertEqual(cleanup["immediate_status"], cleanup_status)
                self.assertEqual(cleanup["settled_status"], cleanup_status)
                self.assertGreaterEqual(cleanup["poll_count"], 1)
                self.assertRegex(manifest["error"], error_pattern)

        with tempfile.TemporaryDirectory() as tmpdir:
            output_root = Path(tmpdir) / "raw"
            fake_client = FakeClient(self.capture_cleanup_status())
            args = SimpleNamespace(
                scenario_id="verified-cleanup",
                output_root=output_root,
                firmware_commit="a" * 40,
                duration=1.0,
                start_timeout=1.0,
                stop_timeout=1.0,
                cleanup_timeout=1.0,
                description="Verified collector lifecycle.",
            )
            with patch(
                "scripts.sensing_analysis.csi_capture.DeviceClient.from_args",
                return_value=fake_client,
            ), patch(
                "scripts.sensing_analysis.csi_capture.connect_ws",
                return_value=FakeConnection(FakeWebSocket(hello_message)),
            ), patch(
                "scripts.sensing_analysis.csi_capture.receive_data_until_end",
                new=fake_receive,
            ):
                final_dir = asyncio.run(collect_capture(args))

            self.assertTrue(final_dir.is_dir())
            self.assertFalse(final_dir.name.endswith(".partial"))
            self.assertEqual(list(output_root.glob("*.partial")), [])
            manifest = json.loads(
                (final_dir / MANIFEST_FILENAME).read_text(encoding="utf-8")
            )
            self.assertEqual(manifest["state"], "complete")
            self.assertTrue(manifest["collector_cleanup"]["verified"])
            self.assertTrue(
                capture_cleanup_complete(
                    manifest["collector_cleanup"]["settled_status"]
                )
            )
            self.assertEqual(manifest["capture_origin"]["accepted_sequence"], 6)

        async def failing_receive(*_args, **_kwargs):
            raise CaptureWorkflowError("synthetic receive failure")

        with tempfile.TemporaryDirectory() as tmpdir:
            output_root = Path(tmpdir) / "raw"
            fake_client = FakeClient(self.capture_cleanup_status())
            args = SimpleNamespace(
                scenario_id="failed-receive-cleanup",
                output_root=output_root,
                firmware_commit="a" * 40,
                duration=1.0,
                start_timeout=1.0,
                stop_timeout=1.0,
                cleanup_timeout=1.0,
                description="Cleanup after receive failure.",
            )
            with patch(
                "scripts.sensing_analysis.csi_capture.DeviceClient.from_args",
                return_value=fake_client,
            ), patch(
                "scripts.sensing_analysis.csi_capture.connect_ws",
                return_value=FakeConnection(FakeWebSocket(hello_message)),
            ), patch(
                "scripts.sensing_analysis.csi_capture.receive_data_until_end",
                new=failing_receive,
            ):
                with self.assertRaisesRegex(
                    CaptureWorkflowError,
                    "synthetic receive failure",
                ):
                    asyncio.run(collect_capture(args))

            partial_dirs = list(output_root.glob("*.partial"))
            self.assertEqual(len(partial_dirs), 1)
            manifest = json.loads(
                (partial_dirs[0] / MANIFEST_FILENAME).read_text(encoding="utf-8")
            )
            self.assertEqual(manifest["state"], "incomplete")
            self.assertTrue(manifest["collector_cleanup"]["verified"])

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
            "firmware_commit": "a" * 40,
            "firmware_dirty": False,
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
            "collector_cleanup": {
                "verified": True,
                "timeout_ms": 15000,
                "poll_interval_ms": 250,
                "immediate_status": self.capture_cleanup_status(active=True),
                "settled_status": self.capture_cleanup_status(),
                "poll_count": 2,
                "elapsed_ms": 250,
            },
            "capture_origin": {
                "session": 0xA1B2C3D4,
                "accepted_sequence": 40,
                "process_now_ms": 1000,
                "host_utc": "2026-07-11T12:34:56.123456Z",
                "host_monotonic_ns": 987654321,
            },
            "snapshots": {
                "config_before": config,
                "config_after": json.loads(json.dumps(config)),
                "status_before": {},
                "status_after": self.capture_cleanup_status(),
                "system_info": system_info,
            },
        }
        atomic_write_json(capture_dir / MANIFEST_FILENAME, manifest)
        return capture_dir, source, destination

    def test_firmware_provenance_uses_and_verifies_device_identity(self):
        system_info = {
            "firmware_version": "1.0.0-test",
            "firmware_built_target": "esp32s3",
            "firmware_commit": "a" * 40,
            "firmware_dirty": False,
            "esp_platform": "ESP32-S3",
            "sdk_version": "5.5.1",
            "arduino_version": "3.x",
        }
        provenance = firmware_provenance(system_info, "a" * 40)
        self.assertEqual(provenance["firmware_commit"], "a" * 40)
        self.assertFalse(provenance["firmware_dirty"])
        self.assertTrue(provenance["firmware_identity_verified"])

        dirty_info = {**system_info, "firmware_dirty": True}
        dirty = firmware_provenance(dirty_info, f"{'a' * 40}-dirty")
        self.assertEqual(dirty["firmware_commit"], f"{'a' * 40}-dirty")

        with self.assertRaisesRegex(CaptureWorkflowError, "does not match expected"):
            firmware_provenance(system_info, "b" * 40)
        with self.assertRaisesRegex(CaptureWorkflowError, "does not match expected"):
            firmware_provenance(dirty_info, "a" * 40)
        with self.assertRaisesRegex(CaptureWorkflowError, "no exact 40-hex"):
            firmware_provenance({**system_info, "firmware_commit": "unknown"}, "a" * 40)

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
        scenario["acceptance"] = {
            "reviewed": True,
            "max_false_positive_ms": 100,
            "max_false_negative_ms": 100,
            "max_invalid_decision_ms": 100,
            "max_detection_latency_ms": 100,
            "max_clear_latency_ms": 100,
            "max_motion_dropout_ms": 100,
            "max_missed_motion_intervals": 0,
            "max_uncleared_transitions": 0,
        }
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
            set_acceptance(
                SimpleNamespace(
                    target=capture_dir,
                    max_false_positive_ms=100,
                    max_false_negative_ms=100,
                    max_invalid_decision_ms=100,
                    max_detection_latency_ms=100,
                    max_clear_latency_ms=100,
                    max_motion_dropout_ms=100,
                    max_missed_motion_intervals=0,
                    max_uncleared_transitions=0,
                )
            )

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
            invalid_promoted_scenario = json.loads(json.dumps(promoted_scenario))
            del invalid_promoted_scenario["source"]["firmware_identity_verified"]
            atomic_write_json(
                promoted / SCENARIO_FILENAME,
                invalid_promoted_scenario,
            )
            with self.assertRaisesRegex(
                CaptureWorkflowError,
                "collector-verified firmware identity",
            ):
                verify_capture(promoted)

        self.assertTrue(result["ok"])
        self.assertNotIn(source, promoted_bytes)
        self.assertNotIn(destination, promoted_bytes)
        self.assertEqual(promoted_frames[0].source_mac, bytes.fromhex("020000000001"))
        self.assertEqual(promoted_frames[0].destination_mac, bytes.fromhex("020000000002"))
        self.assertTrue(promoted_scenario["ground_truth"]["reviewed"])
        self.assertEqual(promoted_scenario["ground_truth"]["timeline"][1]["motion"], "present")
        self.assertEqual(
            set(promoted_scenario["source"]),
            {
                "kind",
                "board_env",
                "firmware_version",
                "firmware_commit",
                "firmware_dirty",
                "firmware_identity_verified",
                "build_target",
                "esp_platform",
                "sdk_version",
                "arduino_version",
            },
        )
        self.assertIs(promoted_scenario["source"]["firmware_dirty"], False)
        self.assertIs(
            promoted_scenario["source"]["firmware_identity_verified"],
            True,
        )
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

    def test_promotion_requires_verified_collector_cleanup_and_origin(self):
        mutations = (
            (
                "missing-cleanup",
                lambda manifest: manifest.pop("collector_cleanup"),
                "collector cleanup lifecycle evidence",
            ),
            (
                "unverified-cleanup",
                lambda manifest: manifest["collector_cleanup"].update(
                    {"verified": False}
                ),
                "collector_cleanup.verified=true",
            ),
            (
                "active-settled-status",
                lambda manifest: manifest["collector_cleanup"].update(
                    {"settled_status": self.capture_cleanup_status(active=True)}
                ),
                "fully quiescent settled",
            ),
            (
                "missing-origin",
                lambda manifest: manifest.pop("capture_origin"),
                "CAPTURE_ORIGIN evidence",
            ),
        )
        for fixture_id, mutate, error_pattern in mutations:
            with self.subTest(fixture_id=fixture_id), tempfile.TemporaryDirectory() as tmpdir:
                root = Path(tmpdir)
                capture_dir, _, _ = self.create_raw_capture(root)
                self.mark_scenario_reviewed(capture_dir)
                manifest_path = capture_dir / MANIFEST_FILENAME
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                mutate(manifest)
                atomic_write_json(manifest_path, manifest)
                if fixture_id == "missing-cleanup":
                    # Legacy raw artifacts remain inspectable, but cannot be
                    # promoted into the release corpus without lifecycle proof.
                    self.assertTrue(verify_capture(capture_dir)["ok"])

                with self.assertRaisesRegex(CaptureWorkflowError, error_pattern):
                    promote_capture(
                        SimpleNamespace(
                            target=capture_dir,
                            fixture_id=fixture_id,
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

    def test_acceptance_requires_reviewed_ground_truth(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            capture_dir, _, _ = self.create_raw_capture(Path(tmpdir))
            with self.assertRaisesRegex(CaptureWorkflowError, "not been marked reviewed"):
                set_acceptance(
                    SimpleNamespace(
                        target=capture_dir,
                        max_false_positive_ms=100,
                        max_false_negative_ms=100,
                        max_invalid_decision_ms=100,
                        max_detection_latency_ms=100,
                        max_clear_latency_ms=100,
                        max_motion_dropout_ms=100,
                        max_missed_motion_intervals=0,
                        max_uncleared_transitions=0,
                    )
                )

    def test_acceptance_command_upgrades_a_reviewed_g0_raw_scenario(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            capture_dir, _, _ = self.create_raw_capture(Path(tmpdir))
            self.mark_scenario_reviewed(capture_dir)
            scenario_path = capture_dir / SCENARIO_FILENAME
            scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
            del scenario["acceptance"]
            atomic_write_json(scenario_path, scenario)

            set_acceptance(
                SimpleNamespace(
                    target=capture_dir,
                    max_false_positive_ms=100,
                    max_false_negative_ms=100,
                    max_invalid_decision_ms=100,
                    max_detection_latency_ms=100,
                    max_clear_latency_ms=100,
                    max_motion_dropout_ms=100,
                    max_missed_motion_intervals=0,
                    max_uncleared_transitions=0,
                )
            )
            upgraded = json.loads(scenario_path.read_text(encoding="utf-8"))
            self.assertTrue(upgraded["acceptance"]["reviewed"])
            self.assertEqual(upgraded["acceptance"]["max_invalid_decision_ms"], 100)

    def test_ground_truth_edit_invalidates_prior_acceptance_review(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            capture_dir, _, _ = self.create_raw_capture(Path(tmpdir))
            self.mark_scenario_reviewed(capture_dir)
            annotate_capture(
                SimpleNamespace(
                    target=capture_dir,
                    review=False,
                    start_ms=20,
                    end_ms=30,
                    motion="present",
                    occupancy="occupied",
                    environment="stable",
                    evidence="operator",
                    confidence="high",
                    note="Replace reviewed unit interval.",
                    replace=True,
                )
            )
            scenario = json.loads(
                (capture_dir / SCENARIO_FILENAME).read_text(encoding="utf-8")
            )
            self.assertFalse(scenario["ground_truth"]["reviewed"])
            self.assertFalse(scenario["acceptance"]["reviewed"])

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
        for invalid_commit in (f"{'a' * 40}-dirty", "A" * 40, 123):
            with self.subTest(invalid_commit=invalid_commit):
                with tempfile.TemporaryDirectory() as tmpdir:
                    root = Path(tmpdir)
                    capture_dir, _, _ = self.create_raw_capture(root)
                    self.mark_scenario_reviewed(capture_dir)
                    scenario_path = capture_dir / SCENARIO_FILENAME
                    scenario = json.loads(scenario_path.read_text(encoding="utf-8"))
                    scenario["source"]["firmware_commit"] = invalid_commit
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
