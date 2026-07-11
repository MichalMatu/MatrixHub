from __future__ import annotations

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from collections import defaultdict, deque
from pathlib import Path
from typing import Any, Callable

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts" / "tests"))
import csi_alarm_hardware_gate as gate  # noqa: E402


SHA = "0123456789abcdef0123456789abcdef01234567"
BOOT_ID = "0123456789abcdef"


class FakeResponse:
    def __init__(self, payload: Any, status_code: int = 200):
        self.payload = payload
        self.status_code = status_code

    def json(self) -> Any:
        if isinstance(self.payload, BaseException):
            raise self.payload
        return self.payload


class FakeDeviceClient:
    def __init__(
        self,
        responses: dict[str, list[Any]],
        on_get: Callable[[str], None] | None = None,
    ):
        self.responses: dict[str, deque[Any]] = defaultdict(deque)
        self.on_get = on_get
        for path, values in responses.items():
            self.responses[path].extend(values)

    def get(self, path: str, **_kwargs: Any) -> FakeResponse:
        if self.on_get is not None:
            self.on_get(path)
        if not self.responses[path]:
            raise AssertionError(f"unexpected request: {path}")
        value = self.responses[path].popleft()
        if isinstance(value, BaseException):
            raise value
        if isinstance(value, FakeResponse):
            return value
        return FakeResponse(value)


def system_info(uptime: int = 100) -> dict[str, Any]:
    return {
        "firmware_commit": SHA,
        "firmware_dirty": False,
        "uptime": uptime,
        "mac_address": "AA:BB:CC:DD:EE:FF",
    }


def alarm_rules(
    triggered: bool,
    transition_seq: int = 1,
    device_millis: int = 100,
    **overrides: Any,
) -> dict[str, Any]:
    rule = {
        "id": "private-rule-id",
        "name": "Private room alarm",
        "enabled": True,
        "source": "wifi_csi_motion",
        "operator": "above",
        "threshold": 0.5,
        "triggered": triggered,
        "transition_seq": transition_seq,
        "device_millis": device_millis,
        "boot_id": BOOT_ID,
    }
    rule.update(overrides)
    return {"schema_version": 1, "rules": [rule]}


def wifi_status(
    detected: bool,
    *,
    decision_valid: bool = True,
    data_fresh: bool = True,
    queue_drops: int = 0,
    batch_drops: int = 0,
    capture_drops: int = 0,
    truncated_records: int = 0,
    runtime_fault: bool = False,
) -> dict[str, Any]:
    return {
        "connectedSSID": "PrivateNetwork",
        "csi": {
            "enabled": True,
            "runtime_fault": runtime_fault,
            "runtime_reconcile_pending": False,
            "queue_allocated": True,
            "calibration_state": "forced",
            "queue_drops_total": queue_drops,
            "batches_dropped_total": batch_drops,
            "capture": {
                "records_dropped": capture_drops,
                "truncated_records": truncated_records,
            },
            "motion": {
                "enabled": True,
                "state": "motion_confirmed" if detected else "monitoring",
                "has_frame": True,
                "detected": detected,
                "decision_valid": decision_valid,
                "data_fresh": data_fresh,
            },
        },
    }


def network(connected: bool = True) -> dict[str, Any]:
    return {
        "wifi": {
            "sta_connected": connected,
            "sta_ip": "192.168.0.18",
            "last_recovery_reason": "private-reason",
        },
        "ap": {},
        "http": {},
        "forwarding": {},
    }


def health(uptime: int = 100, boot_count: int = 4, unexpected: int = 0) -> dict[str, Any]:
    return {
        "uptimeSec": uptime,
        "boot": {"bootCount": boot_count, "unexpectedRestarts": unexpected},
        "http": {"wsQueueDrops": 0},
        "heap": {},
    }


def mutexes(standard_timeouts: int = 0, recursive_timeouts: int = 0) -> dict[str, Any]:
    return {
        "runtime": {
            "standard": {"timeouts": standard_timeouts},
            "recursive": {"timeouts": recursive_timeouts},
        }
    }


def wifi_config(hold_ms: int = 1200, clear_hold_ms: int = 2500) -> dict[str, Any]:
    return {
        "enabled": True,
        "csi_alarm": {
            "enabled": True,
            "hold_ms": hold_ms,
            "clear_hold_ms": clear_hold_ms,
        },
    }


def preflight(
    rule_payload: dict[str, Any] | None = None,
    wifi_payload: dict[str, Any] | None = None,
    health_payload: dict[str, Any] | None = None,
    mutex_payload: dict[str, Any] | None = None,
    config_payload: dict[str, Any] | None = None,
) -> gate.PreflightResult:
    rule_payload = rule_payload or alarm_rules(False)
    wifi_payload = wifi_payload or wifi_status(False)
    health_payload = health_payload or health()
    mutex_payload = mutex_payload or mutexes()
    config_payload = config_payload or wifi_config()
    return gate.PreflightResult(
        firmware_commit=SHA,
        firmware_dirty=False,
        hold_ms=config_payload["csi_alarm"]["hold_ms"],
        clear_hold_ms=config_payload["csi_alarm"]["clear_hold_ms"],
        health_endpoint=gate.HEALTH_PATH,
        mutex_endpoint=gate.MUTEX_PATH,
        rule_contract={"source": "wifi_csi_motion", "operator": "above", "threshold": 0.5},
        raw_system_info=system_info(),
        raw_wifi_status=wifi_payload,
        raw_wifi_config=config_payload,
        raw_alarm_rules=rule_payload,
        raw_health=health_payload,
        raw_mutexes=mutex_payload,
    )


def capture_states(states: list[dict[str, Any]]) -> list[dict[str, Any]]:
    responses: dict[str, list[Any]] = {
        gate.SYSTEM_INFO_PATH: [],
        gate.WIFI_STATUS_PATH: [],
        gate.WIFI_CONFIG_PATH: [],
        gate.ALARM_RULES_PATH: [],
        gate.NETWORK_PATH: [],
        gate.HEALTH_PATH: [],
        gate.MUTEX_PATH: [],
    }
    for index, state in enumerate(states):
        responses[gate.SYSTEM_INFO_PATH].append(state.get("info", system_info(100 + index)))
        responses[gate.WIFI_STATUS_PATH].append(state["wifi"])
        responses[gate.WIFI_CONFIG_PATH].append(state.get("config", wifi_config()))
        responses[gate.ALARM_RULES_PATH].append(state["alarms"])
        responses[gate.NETWORK_PATH].append(state.get("network", network()))
        responses[gate.HEALTH_PATH].append(state.get("health", health(100 + index)))
        responses[gate.MUTEX_PATH].append(state.get("mutexes", mutexes()))
    client = FakeDeviceClient(responses)
    return [
        gate.capture_sample(
            client,
            index,
            gate.HEALTH_PATH,
            gate.MUTEX_PATH,
            consistency_rereads=0,
            utc_factory=lambda index=index: f"2026-07-11T12:00:0{index}.000Z",
            monotonic_ns_factory=lambda index=index: index * 1_000_000_000,
        )
        for index in range(len(states))
    ]


def set_sample_window(
    record: dict[str, Any],
    started_ns: int,
    completed_ns: int,
) -> None:
    record["host"]["started_monotonic_ns"] = started_ns
    record["host"]["completed_monotonic_ns"] = completed_ns
    record["host"]["monotonic_ns"] = completed_ns


def broad_transport_outage() -> dict[str, Any]:
    return {
        "info": transport_error(),
        "wifi": transport_error(),
        "config": transport_error(),
        "alarms": transport_error(),
        "network": transport_error(),
    }


def transport_error() -> BaseException:
    return gate.RequestsConnectionError("offline")


def finding_codes(report: dict[str, Any]) -> set[str]:
    return {item["code"] for item in report["findings"]}


def operator_marker(kind: str, sequence: int, action_ns: int) -> dict[str, Any]:
    return gate.marker_record(
        {
            "valid": True,
            "kind": kind,
            "created_utc": "2026-07-11T12:00:00.000Z",
            "created_monotonic_ns": action_ns,
        },
        sequence,
        utc_factory=lambda: "2026-07-11T12:00:09.000Z",
        monotonic_ns_factory=lambda: action_ns + 9_000_000,
    )


class CsiAlarmHardwareGateTests(unittest.TestCase):
    def test_cli_defaults_require_operator_cycle_reconnect_and_soak_coverage(self) -> None:
        args = gate.build_parser().parse_args(
            [
                "run",
                "--expected-firmware-sha",
                SHA,
                "--marker-file",
                "/tmp/matrixhub-test-markers.jsonl",
            ]
        )

        self.assertEqual(args.minimum_operator_cycles, 1)
        self.assertEqual(args.minimum_verified_reconnect_gaps, 1)
        self.assertEqual(args.minimum_motion_during_reconnect, 1)
        self.assertEqual(args.minimum_duration_seconds, 3600.0)
        self.assertEqual(args.minimum_sample_coverage_ratio, 0.8)
        self.assertEqual(args.max_sample_gap_seconds, 90.0)
        self.assertEqual(args.sample_timeout_seconds, 1.0)
        self.assertEqual(args.max_reconnect_gap_seconds, 60.0)
        self.assertEqual(args.max_cumulative_reconnect_gap_seconds, 120.0)

        with contextlib.redirect_stderr(io.StringIO()):
            result = gate.main(["run", "--expected-firmware-sha", SHA])
        self.assertEqual(result, 2)

    def test_soak_polling_client_uses_short_timeout_without_retries(self) -> None:
        preflight_client = gate.DeviceClient(
            base_url="https://127.0.0.1",
            timeout=10.0,
            retries=3,
        )
        preflight_client.set_token("private-test-token")
        polling_client = gate._make_polling_client(preflight_client, 1.0)
        try:
            self.assertEqual(polling_client.timeout, 1.0)
            self.assertEqual(polling_client.retries, 0)
            self.assertEqual(polling_client.token, "private-test-token")
            for adapter in polling_client.session.adapters.values():
                self.assertEqual(adapter.max_retries.total, 0)
        finally:
            polling_client.session.close()
            preflight_client.session.close()

    def test_preflight_accepts_exact_clean_sha_and_one_canonical_rule(self) -> None:
        client = FakeDeviceClient(
            {
                gate.SYSTEM_INFO_PATH: [system_info()],
                gate.ALARM_RULES_PATH: [alarm_rules(False)],
                gate.WIFI_STATUS_PATH: [wifi_status(False)],
                gate.WIFI_CONFIG_PATH: [wifi_config()],
                gate.HEALTH_PATH: [health()],
                gate.MUTEX_PATH: [mutexes()],
            }
        )

        result = gate.run_preflight(client, SHA)

        self.assertEqual(result.firmware_commit, SHA)
        self.assertFalse(result.firmware_dirty)
        self.assertEqual(result.rule_contract["operator"], "above")
        self.assertEqual(result.rule_contract["threshold"], 0.5)
        self.assertEqual((result.hold_ms, result.clear_hold_ms), (1200, 2500))

    def test_preflight_requires_exact_detector_hold_configuration(self) -> None:
        for name, config_payload, expected in (
            ("disabled", {"csi_alarm": {"enabled": False}}, "enabled csi_alarm"),
            (
                "boolean_hold",
                wifi_config(hold_ms=True),
                "hold_ms",
            ),
            (
                "missing_clear",
                {"csi_alarm": {"enabled": True, "hold_ms": 1200}},
                "clear_hold_ms",
            ),
        ):
            with self.subTest(name=name):
                with self.assertRaisesRegex(gate.GateError, expected):
                    gate.run_preflight(
                        FakeDeviceClient(
                            {
                                gate.SYSTEM_INFO_PATH: [system_info()],
                                gate.ALARM_RULES_PATH: [alarm_rules(False)],
                                gate.WIFI_STATUS_PATH: [wifi_status(False)],
                                gate.WIFI_CONFIG_PATH: [config_payload],
                            }
                        ),
                        SHA,
                    )

    def test_preflight_fails_closed_for_dirty_or_noncanonical_firmware_and_rule(self) -> None:
        dirty = system_info()
        dirty["firmware_dirty"] = True
        with self.assertRaisesRegex(gate.GateError, "firmware_dirty=false"):
            gate.run_preflight(
                FakeDeviceClient({gate.SYSTEM_INFO_PATH: [dirty]}),
                SHA,
            )
        with self.assertRaisesRegex(gate.GateError, "canonical 'above'"):
            gate.run_preflight(
                FakeDeviceClient(
                    {
                        gate.SYSTEM_INFO_PATH: [system_info()],
                        gate.ALARM_RULES_PATH: [alarm_rules(False, operator="below")],
                    }
                ),
                SHA,
            )

    def test_preflight_requires_a_fresh_valid_forced_runtime(self) -> None:
        stale = wifi_status(False, decision_valid=False, data_fresh=True)
        with self.assertRaisesRegex(gate.GateError, "decision_valid must be true"):
            gate.run_preflight(
                FakeDeviceClient(
                    {
                        gate.SYSTEM_INFO_PATH: [system_info()],
                        gate.ALARM_RULES_PATH: [alarm_rules(False)],
                        gate.WIFI_STATUS_PATH: [stale],
                    }
                ),
                SHA,
            )
        with self.assertRaisesRegex(gate.GateError, "detector.*triggered state must match"):
            gate.run_preflight(
                FakeDeviceClient(
                    {
                        gate.SYSTEM_INFO_PATH: [system_info()],
                        gate.ALARM_RULES_PATH: [alarm_rules(True)],
                        gate.WIFI_STATUS_PATH: [wifi_status(False)],
                        gate.WIFI_CONFIG_PATH: [wifi_config()],
                    }
                ),
                SHA,
            )

    def test_preflight_rejects_boolean_or_zero_transition_metadata(self) -> None:
        for field, value in (("transition_seq", True), ("device_millis", False)):
            with self.subTest(field=field, value=value):
                with self.assertRaisesRegex(gate.GateError, field):
                    gate.run_preflight(
                        FakeDeviceClient(
                            {
                                gate.SYSTEM_INFO_PATH: [system_info()],
                                gate.ALARM_RULES_PATH: [alarm_rules(False, **{field: value})],
                            }
                        ),
                        SHA,
                    )
        with self.assertRaisesRegex(gate.GateError, "seq 0 baseline"):
            gate.run_preflight(
                FakeDeviceClient(
                    {
                        gate.SYSTEM_INFO_PATH: [system_info()],
                        gate.ALARM_RULES_PATH: [alarm_rules(False, 0, 1)],
                    }
                ),
                SHA,
            )

    def test_boot_id_is_required_nonzero_lowercase_and_stable(self) -> None:
        for value in (None, "0000000000000000", "0123456789ABCDEF", "abc"):
            with self.subTest(value=value):
                with self.assertRaisesRegex(gate.GateError, "boot_id"):
                    gate.run_preflight(
                        FakeDeviceClient(
                            {
                                gate.SYSTEM_INFO_PATH: [system_info()],
                                gate.ALARM_RULES_PATH: [alarm_rules(False, boot_id=value)],
                            }
                        ),
                        SHA,
                    )

        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, boot_id="fedcba9876543210"),
                },
            ]
        )
        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
        )
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("rule_boot_id_changed", finding_codes(report))

    def test_preflight_accepts_zero_sequence_boot_baseline(self) -> None:
        client = FakeDeviceClient(
            {
                gate.SYSTEM_INFO_PATH: [system_info()],
                gate.ALARM_RULES_PATH: [alarm_rules(False, 0, 0)],
                gate.WIFI_STATUS_PATH: [wifi_status(False)],
                gate.WIFI_CONFIG_PATH: [wifi_config()],
                gate.HEALTH_PATH: [health()],
                gate.MUTEX_PATH: [mutexes()],
            }
        )

        result = gate.run_preflight(client, SHA)

        self.assertEqual(result.raw_alarm_rules["rules"][0]["transition_seq"], 0)

    def test_preflight_requires_summary_and_mutex_diagnostics(self) -> None:
        common = {
            gate.SYSTEM_INFO_PATH: [system_info()],
            gate.ALARM_RULES_PATH: [alarm_rules(False)],
            gate.WIFI_STATUS_PATH: [wifi_status(False)],
            gate.WIFI_CONFIG_PATH: [wifi_config()],
        }
        with self.assertRaisesRegex(gate.GateError, "diagnostics/summary"):
            gate.run_preflight(
                FakeDeviceClient(
                    {**common, gate.HEALTH_PATH: [FakeResponse({}, status_code=404)]}
                ),
                SHA,
            )
        with self.assertRaisesRegex(gate.GateError, "diagnostics/mutexes"):
            gate.run_preflight(
                FakeDeviceClient(
                    {
                        **common,
                        gate.HEALTH_PATH: [health()],
                        gate.MUTEX_PATH: [FakeResponse({}, status_code=404)],
                    }
                ),
                SHA,
            )

    def test_preflight_rejects_missing_or_boolean_observability_counters(self) -> None:
        bad_status = wifi_status(False)
        bad_status["csi"]["queue_drops_total"] = True
        bad_health = health()
        bad_health["boot"]["bootCount"] = False
        bad_mutexes = mutexes()
        del bad_mutexes["runtime"]["recursive"]["timeouts"]
        cases = (
            ("status", bad_status, health(), mutexes(), "queue_drops_total"),
            ("health", wifi_status(False), bad_health, mutexes(), "boot.bootCount"),
            ("mutexes", wifi_status(False), health(), bad_mutexes, "recursive.timeouts"),
        )
        for name, status_payload, health_payload, mutex_payload, expected in cases:
            with self.subTest(name=name):
                with self.assertRaisesRegex(gate.GateError, expected):
                    gate.run_preflight(
                        FakeDeviceClient(
                            {
                                gate.SYSTEM_INFO_PATH: [system_info()],
                                gate.ALARM_RULES_PATH: [alarm_rules(False)],
                                gate.WIFI_STATUS_PATH: [status_payload],
                                gate.WIFI_CONFIG_PATH: [wifi_config()],
                                gate.HEALTH_PATH: [health_payload],
                                gate.MUTEX_PATH: [mutex_payload],
                            }
                        ),
                        SHA,
                    )
    def test_transition_metadata_accepts_zero_to_first_event(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 0, 0)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 1, 12)},
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(alarm_rules(False, 0, 0), wifi_status(False)),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["transitions"]["rule_metadata"], 1)

    def test_transition_metadata_accepts_reserved_sequence_and_millis_wrap(self) -> None:
        records = capture_states(
            [
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, gate.UINT32_MAX, gate.UINT32_MAX - 5),
                },
                {
                    "wifi": wifi_status(True),
                    "alarms": alarm_rules(True, 1, 3),
                },
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(
                alarm_rules(False, gate.UINT32_MAX, gate.UINT32_MAX - 5),
                wifi_status(False),
            ),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["transitions"]["rule_metadata"], 1)

    def test_transition_metadata_fails_closed_on_sequence_anomalies(self) -> None:
        cases = (
            (
                "missed",
                alarm_rules(False, 1, 100),
                alarm_rules(True, 3, 200),
                "missed_rule_transition",
            ),
            (
                "regression",
                alarm_rules(False, 10, 100),
                alarm_rules(True, 9, 200),
                "transition_sequence_invalid",
            ),
            (
                "ambiguous",
                alarm_rules(False, 1, 100),
                alarm_rules(True, 1 + gate.UINT32_HALF_RANGE, 200),
                "transition_sequence_invalid",
            ),
            (
                "same_seq_changed_state",
                alarm_rules(False, 5, 100),
                alarm_rules(True, 5, 100),
                "transition_metadata_inconsistent",
            ),
            (
                "next_seq_same_state",
                alarm_rules(False, 5, 100),
                alarm_rules(False, 6, 200),
                "transition_state_did_not_toggle",
            ),
            (
                "next_seq_stale_millis",
                alarm_rules(False, 5, 100),
                alarm_rules(True, 6, 100),
                "transition_millis_not_advanced",
            ),
        )
        for name, first_rule, second_rule, expected_code in cases:
            with self.subTest(name=name):
                first_triggered = first_rule["rules"][0]["triggered"]
                second_triggered = second_rule["rules"][0]["triggered"]
                records = capture_states(
                    [
                        {"wifi": wifi_status(first_triggered), "alarms": first_rule},
                        {"wifi": wifi_status(second_triggered), "alarms": second_rule},
                    ]
                )

                report = gate.analyze_records(
                    records,
                    preflight(first_rule, wifi_status(first_triggered)),
                    require_state_cycle=False,
                )

                self.assertEqual(report["verdict"], "fail")
                self.assertIn(expected_code, finding_codes(report))

    def test_correct_clear_motion_clear_transition_passes(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )

        report = gate.analyze_records(records, preflight())

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(
            report["transitions"],
            {"detector": 2, "alarm": 2, "rule_metadata": 2},
        )
        self.assertNotIn("false_clear", finding_codes(report))
        self.assertNotIn("false_trigger", finding_codes(report))

    def test_gate_requires_a_complete_clear_motion_clear_cycle(self) -> None:
        records = capture_states(
            [{"wifi": wifi_status(False), "alarms": alarm_rules(False)}]
        )

        report = gate.analyze_records(records, preflight())

        self.assertEqual(report["verdict"], "fail")
        self.assertIn("insufficient_state_cycle_coverage", finding_codes(report))

    def test_gate_requires_configured_host_monotonic_duration(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_duration_seconds=3.0,
        )

        self.assertEqual(report["observation"]["duration_seconds"], 2.0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("observation_duration_too_short", finding_codes(report))

    def test_sample_window_contract_is_required_and_monotonic(self) -> None:
        mutators = {
            "missing_start": lambda host: host.pop("started_monotonic_ns"),
            "completion_before_start": lambda host: host.update(
                started_monotonic_ns=2,
                completed_monotonic_ns=1,
                monotonic_ns=1,
            ),
            "legacy_completion_mismatch": lambda host: host.update(monotonic_ns=7),
        }
        for name, mutate in mutators.items():
            with self.subTest(name=name):
                record = capture_states(
                    [{"wifi": wifi_status(False), "alarms": alarm_rules(False)}]
                )[0]
                mutate(record["host"])

                report = gate.analyze_records(
                    [record],
                    preflight(),
                    require_state_cycle=False,
                )

                self.assertEqual(report["verdict"], "fail")
                self.assertIn("host_timestamp_invalid", finding_codes(report))

    def test_nan_or_infinite_timing_cannot_disable_cli_gate(self) -> None:
        for option, value in (
            ("--duration-seconds", "nan"),
            ("--minimum-duration-seconds", "nan"),
            ("--poll-interval-seconds", "inf"),
            ("--sample-timeout-seconds", "nan"),
            ("--max-sample-gap-seconds", "inf"),
            ("--max-reconnect-gap-seconds", "nan"),
            ("--max-cumulative-reconnect-gap-seconds", "inf"),
        ):
            with self.subTest(option=option):
                with contextlib.redirect_stderr(io.StringIO()):
                    result = gate.main(
                        [
                            "run",
                            "--expected-firmware-sha",
                            SHA,
                            "--marker-file",
                            "/tmp/matrixhub-test-markers.jsonl",
                            option,
                            value,
                        ]
                    )
                self.assertEqual(result, 2)

    def test_sparse_hour_cannot_fake_soak_coverage(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
                {
                    "wifi": wifi_status(False, decision_valid=False, data_fresh=False),
                    "alarms": alarm_rules(False, 3, 300),
                },
            ]
        )
        for record, timestamp in zip(
            records,
            (0, 1_000_000_000, 2_000_000_000, 3_600_000_000_000),
            strict=True,
        ):
            set_sample_window(record, timestamp, timestamp)

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_duration_seconds=3600.0,
            max_sample_gap_seconds=5.0,
            minimum_sample_coverage_ratio=0.8,
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertTrue(
            {"sample_cadence_gap_exceeded", "sample_coverage_too_low"}.issubset(
                finding_codes(report)
            )
        )

    def test_inversion_flags_both_polarities_and_inverted_semantics(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(True, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(False, 2, 200)},
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(alarm_rules(True), wifi_status(True)),
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertTrue(
            {"false_clear", "false_trigger", "inverted_alarm_semantics"}.issubset(
                finding_codes(report)
            )
        )

    def test_one_sample_wifi_alarm_race_converges_via_persisted_reread(self) -> None:
        client = FakeDeviceClient(
            {
                gate.SYSTEM_INFO_PATH: [system_info()],
                gate.WIFI_STATUS_PATH: [wifi_status(True), wifi_status(True)],
                gate.WIFI_CONFIG_PATH: [wifi_config()],
                gate.ALARM_RULES_PATH: [alarm_rules(False, 1, 100), alarm_rules(True, 2, 200)],
                gate.NETWORK_PATH: [network()],
                gate.HEALTH_PATH: [health()],
                gate.MUTEX_PATH: [mutexes()],
            }
        )
        monotonic_values = iter((1_000_000_000, 1_100_000_000, 1_200_000_000))
        record = gate.capture_sample(
            client,
            0,
            gate.HEALTH_PATH,
            gate.MUTEX_PATH,
            consistency_rereads=1,
            consistency_reread_delay_seconds=0,
            utc_factory=lambda: "2026-07-11T12:00:01.000Z",
            monotonic_ns_factory=lambda: next(monotonic_values),
        )

        report = gate.analyze_records(
            [record],
            preflight(),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(len(record["snapshots"]["consistency_attempts"]), 1)
        self.assertEqual(report["consistency"], {"rereads": 1, "resolved_races": 1})
        self.assertNotIn("false_clear", finding_codes(report))

    def test_persistent_mismatch_after_bounded_rereads_fails(self) -> None:
        client = FakeDeviceClient(
            {
                gate.SYSTEM_INFO_PATH: [system_info()],
                gate.WIFI_STATUS_PATH: [wifi_status(True), wifi_status(True), wifi_status(True)],
                gate.WIFI_CONFIG_PATH: [wifi_config()],
                gate.ALARM_RULES_PATH: [
                    alarm_rules(False, 1, 100),
                    alarm_rules(False, 1, 100),
                    alarm_rules(False, 1, 100),
                ],
                gate.NETWORK_PATH: [network()],
                gate.HEALTH_PATH: [health()],
                gate.MUTEX_PATH: [mutexes()],
            }
        )
        record = gate.capture_sample(
            client,
            0,
            gate.HEALTH_PATH,
            gate.MUTEX_PATH,
            consistency_rereads=2,
            consistency_reread_delay_seconds=0,
            utc_factory=lambda: "2026-07-11T12:00:01.000Z",
            monotonic_ns_factory=lambda: 1_000_000_000,
        )

        report = gate.analyze_records(
            [record],
            preflight(),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertEqual(report["consistency"]["rereads"], 2)
        self.assertIn("false_clear", finding_codes(report))

    def test_invalid_or_stale_detector_retains_previous_alarm_state(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {
                    "wifi": wifi_status(True, decision_valid=False, data_fresh=False),
                    "alarms": alarm_rules(True, 2, 200),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )

        report = gate.analyze_records(records, preflight())

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["samples"]["invalid_or_stale"], 1)
        self.assertIn("detector_unobservable", finding_codes(report))
        self.assertNotIn("invalid_retention_violation", finding_codes(report))

    def test_invalid_decision_duration_and_cumulative_budget_are_gated(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {
                    "wifi": wifi_status(False, decision_valid=False, data_fresh=False),
                    "alarms": alarm_rules(False),
                },
                {
                    "wifi": wifi_status(False, decision_valid=False, data_fresh=False),
                    "alarms": alarm_rules(False),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
            ]
        )
        for record, timestamp in zip(
            records,
            (0, 1_000_000_000, 7_000_000_000, 9_000_000_000),
            strict=True,
        ):
            set_sample_window(record, timestamp, timestamp)

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            max_invalid_run_seconds=5.0,
            max_cumulative_invalid_seconds=7.0,
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertEqual(report["detector_observability"]["max_invalid_run_seconds"], 8.0)
        self.assertTrue(
            {
                "invalid_decision_run_exceeded",
                "invalid_decision_cumulative_exceeded",
            }.issubset(finding_codes(report))
        )

    def test_runtime_readiness_drift_fails_after_clean_preflight(self) -> None:
        degraded = wifi_status(False)
        degraded["csi"]["queue_allocated"] = False
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {"wifi": degraded, "alarms": alarm_rules(False)},
            ]
        )

        report = gate.analyze_records(records, preflight())

        self.assertEqual(report["verdict"], "fail")
        self.assertIn("csi_runtime_not_ready", finding_codes(report))

    def test_runtime_motion_frame_and_state_drift_fail_after_preflight(self) -> None:
        for field, value in (("has_frame", False), ("state", "motion_confirmed")):
            with self.subTest(field=field):
                degraded = wifi_status(False)
                degraded["csi"]["motion"][field] = value
                record = capture_states(
                    [{"wifi": degraded, "alarms": alarm_rules(False)}]
                )[0]

                report = gate.analyze_records(
                    [record],
                    preflight(),
                    require_state_cycle=False,
                )

                self.assertEqual(report["verdict"], "fail")
                self.assertIn("csi_runtime_not_ready", finding_codes(report))

    def test_mistyped_motion_runtime_fields_are_hard_contract_errors(self) -> None:
        for field, value in (
            ("detected", 1),
            ("decision_valid", 1),
            ("data_fresh", 1),
            ("has_frame", 1),
            ("state", 1),
        ):
            with self.subTest(field=field):
                degraded = wifi_status(False)
                degraded["csi"]["motion"][field] = value
                records = capture_states(
                    [
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                        {"wifi": degraded, "alarms": alarm_rules(False)},
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                    ]
                )

                report = gate.analyze_records(
                    records,
                    preflight(),
                    require_state_cycle=False,
                )

                self.assertEqual(report["verdict"], "fail")
                self.assertIn("csi_runtime_contract_invalid", finding_codes(report))

    def test_detector_config_drift_during_soak_fails(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False),
                    "config": wifi_config(hold_ms=1800),
                },
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertIn("detector_config_drift", finding_codes(report))

    def test_available_sample_with_mistyped_gated_field_fails_contract(self) -> None:
        mutators = {
            "csi_counter": lambda snapshots: snapshots["wifi_sensing"]["data"]["csi"].__setitem__(
                "queue_drops_total", True
            ),
            "health_boot": lambda snapshots: snapshots["health"]["data"]["boot"].__setitem__(
                "bootCount", False
            ),
            "mutex_counter": lambda snapshots: snapshots["mutexes"]["data"]["runtime"][
                "standard"
            ].pop("timeouts"),
            "network_connected": lambda snapshots: snapshots["network"]["data"]["wifi"].__setitem__(
                "sta_connected", 1
            ),
        }
        for name, mutate in mutators.items():
            with self.subTest(name=name):
                record = capture_states(
                    [{"wifi": wifi_status(False), "alarms": alarm_rules(False)}]
                )[0]
                mutate(record["snapshots"])

                report = gate.analyze_records(
                    [record],
                    preflight(),
                    require_state_cycle=False,
                )

                self.assertEqual(report["verdict"], "fail")
                self.assertIn("observability_contract_invalid", finding_codes(report))

    def test_reconnect_request_gap_is_explicitly_unobservable(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 7, 700)},
                {
                    "wifi": transport_error(),
                    "alarms": alarm_rules(True, 7, 700),
                },
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 7, 700)},
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(alarm_rules(True, 7, 700), wifi_status(True)),
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertEqual(report["samples"]["endpoint_gap"], 1)
        gap = next(item for item in report["findings"] if item["code"] == "unobservable_gap")
        self.assertEqual(gap["missing_endpoints"], ["wifi_sensing"])

    def test_missing_read_before_reconnect_marker_cannot_create_verified_gap(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            marker_file = Path(temporary) / "markers.jsonl"
            marker_tail = gate.MarkerTail(marker_file)
            marker_appended = False

            def append_reconnect_after_wifi_read(path: str) -> None:
                nonlocal marker_appended
                if path == gate.WIFI_CONFIG_PATH and not marker_appended:
                    marker_appended = True
                    gate.append_marker(
                        marker_file,
                        "reconnect_start",
                        utc_factory=lambda: "2026-07-11T12:00:02.500Z",
                        monotonic_ns_factory=lambda: 2_500_000_000,
                    )

            client = FakeDeviceClient(
                {
                    gate.SYSTEM_INFO_PATH: [transport_error()],
                    gate.WIFI_STATUS_PATH: [transport_error()],
                    gate.WIFI_CONFIG_PATH: [transport_error()],
                    gate.ALARM_RULES_PATH: [transport_error()],
                    gate.NETWORK_PATH: [transport_error()],
                    gate.HEALTH_PATH: [health()],
                    gate.MUTEX_PATH: [mutexes()],
                },
                on_get=append_reconnect_after_wifi_read,
            )
            capture_times = iter((2_000_000_000, 3_000_000_000))
            crossing = gate.capture_sample(
                client,
                0,
                gate.HEALTH_PATH,
                gate.MUTEX_PATH,
                consistency_rereads=0,
                utc_factory=lambda: "2026-07-11T12:00:03.000Z",
                monotonic_ns_factory=lambda: next(capture_times),
            )
            reconnect_start = gate.marker_record(
                marker_tail.drain()[0],
                1,
                utc_factory=lambda: "2026-07-11T12:00:03.100Z",
                monotonic_ns_factory=lambda: 3_100_000_000,
            )

        reconnect_end = operator_marker("reconnect_end", 2, 3_500_000_000)
        recovery = capture_states(
            [{"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)}]
        )[0]
        recovery["sequence"] = 3
        set_sample_window(recovery, 4_000_000_000, 4_100_000_000)

        report = gate.analyze_records(
            [crossing, reconnect_start, reconnect_end, recovery],
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("reconnect_gap_temporally_ambiguous", finding_codes(report))

    def test_marked_reconnect_gap_passes_with_exact_rest_continuity(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 7, 700)},
                broad_transport_outage(),
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 7, 700)},
            ]
        )
        for sequence, sample in zip((0, 2, 4), samples, strict=True):
            sample["sequence"] = sequence
        start = gate.marker_record(
            {"valid": True, "kind": "reconnect_start", "created_monotonic_ns": 1_000_000_000},
            1,
            utc_factory=lambda: "2026-07-11T12:00:01.000Z",
            monotonic_ns_factory=lambda: 1_000_000_000,
        )
        end = gate.marker_record(
            {"valid": True, "kind": "reconnect_end", "created_monotonic_ns": 3_000_000_000},
            3,
            utc_factory=lambda: "2026-07-11T12:00:03.000Z",
            monotonic_ns_factory=lambda: 3_000_000_000,
        )

        report = gate.analyze_records(
            [samples[0], start, samples[1], end, samples[2]],
            preflight(alarm_rules(True, 7, 700), wifi_status(True)),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["runtime"]["minimum_required_reconnect_gaps"], 1)
        self.assertIn("planned_reconnect_gap", finding_codes(report))

    def test_marked_single_endpoint_failure_is_not_verified_as_reconnect(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {
                    "wifi": transport_error(),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 4),
            samples,
            (0, 2_000_000_000, 4_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            operator_marker("reconnect_end", 3, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("reconnect_network_evidence_missing", finding_codes(report))

    def test_marked_network_endpoint_only_failure_is_not_reconnect_proof(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": transport_error(),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 4),
            samples,
            (0, 2_000_000_000, 4_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            operator_marker("reconnect_end", 3, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("reconnect_network_evidence_missing", finding_codes(report))

    def test_network_http_or_shape_error_is_not_reconnect_proof(self) -> None:
        for name, network_response in (
            ("http_status", FakeResponse({}, status_code=503)),
            ("invalid_shape", FakeResponse([], status_code=200)),
        ):
            with self.subTest(name=name):
                samples = capture_states(
                    [
                        {
                            "wifi": wifi_status(False),
                            "alarms": alarm_rules(False, 1, 100),
                        },
                        {
                            "wifi": wifi_status(False),
                            "alarms": alarm_rules(False, 1, 100),
                            "network": network_response,
                        },
                        {
                            "wifi": wifi_status(False),
                            "alarms": alarm_rules(False, 1, 100),
                        },
                    ]
                )
                for sequence, sample, timestamp in zip(
                    (0, 2, 4),
                    samples,
                    (0, 2_000_000_000, 4_000_000_000),
                    strict=True,
                ):
                    sample["sequence"] = sequence
                    set_sample_window(sample, timestamp, timestamp)
                records = [
                    samples[0],
                    operator_marker("reconnect_start", 1, 1_000_000_000),
                    samples[1],
                    operator_marker("reconnect_end", 3, 3_000_000_000),
                    samples[2],
                ]

                report = gate.analyze_records(
                    records,
                    preflight(),
                    require_state_cycle=False,
                    minimum_verified_reconnect_gaps=1,
                )

                self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 0)
                self.assertEqual(report["verdict"], "fail")
                self.assertIn(
                    "reconnect_network_evidence_missing",
                    finding_codes(report),
                )

    def test_two_non_transport_core_errors_are_not_reconnect_proof(self) -> None:
        cases = {
            "invalid_json": (
                FakeResponse(ValueError("invalid json")),
                FakeResponse(ValueError("invalid json")),
            ),
            "client_error": (
                gate.DeviceClientError("authentication failed"),
                gate.DeviceClientError("authentication failed"),
            ),
        }
        for name, (info_error, wifi_error) in cases.items():
            with self.subTest(name=name):
                samples = capture_states(
                    [
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                        {
                            "info": info_error,
                            "wifi": wifi_error,
                            "alarms": alarm_rules(False),
                            "network": network(True),
                        },
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                    ]
                )
                for sequence, sample, timestamp in zip(
                    (0, 2, 4),
                    samples,
                    (0, 2_000_000_000, 4_000_000_000),
                    strict=True,
                ):
                    sample["sequence"] = sequence
                    set_sample_window(sample, timestamp, timestamp)
                records = [
                    samples[0],
                    operator_marker("reconnect_start", 1, 1_000_000_000),
                    samples[1],
                    operator_marker("reconnect_end", 3, 3_000_000_000),
                    samples[2],
                ]

                report = gate.analyze_records(
                    records,
                    preflight(),
                    require_state_cycle=False,
                    minimum_verified_reconnect_gaps=1,
                )

                self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 0)
                self.assertEqual(report["verdict"], "fail")
                self.assertIn(
                    "reconnect_network_evidence_missing",
                    finding_codes(report),
                )

    def test_non_transport_failure_inside_verified_reconnect_gap_is_hard_error(
        self,
    ) -> None:
        cases = {
            "wifi_invalid_json": ("wifi", FakeResponse(ValueError("invalid json"))),
            "wifi_http_status": ("wifi", FakeResponse({}, status_code=503)),
            "wifi_invalid_shape": ("wifi", FakeResponse([], status_code=200)),
            "wifi_client_error": (
                "wifi",
                gate.DeviceClientError("authentication failed"),
            ),
            "health_invalid_json": (
                "health",
                FakeResponse(ValueError("invalid json")),
            ),
            "mutex_client_error": (
                "mutexes",
                gate.DeviceClientError("authentication failed"),
            ),
        }
        for name, (endpoint, failure) in cases.items():
            with self.subTest(name=name):
                failure_state = {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                }
                failure_state[endpoint] = failure
                samples = capture_states(
                    [
                        {
                            "wifi": wifi_status(False),
                            "alarms": alarm_rules(False, 1, 100),
                            "network": network(True),
                        },
                        {
                            "wifi": wifi_status(False),
                            "alarms": alarm_rules(False, 1, 100),
                            "network": network(False),
                        },
                        failure_state,
                        {
                            "wifi": wifi_status(False),
                            "alarms": alarm_rules(False, 1, 100),
                            "network": network(True),
                        },
                    ]
                )
                for sequence, sample, timestamp in zip(
                    (0, 2, 3, 5),
                    samples,
                    (0, 2_000_000_000, 3_000_000_000, 5_000_000_000),
                    strict=True,
                ):
                    sample["sequence"] = sequence
                    set_sample_window(sample, timestamp, timestamp)
                records = [
                    samples[0],
                    operator_marker("reconnect_start", 1, 1_000_000_000),
                    samples[1],
                    samples[2],
                    operator_marker("reconnect_end", 4, 4_000_000_000),
                    samples[3],
                ]

                report = gate.analyze_records(
                    records,
                    preflight(),
                    require_state_cycle=False,
                    minimum_verified_reconnect_gaps=1,
                )

                self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
                self.assertEqual(report["verdict"], "fail")
                self.assertIn("endpoint_non_transport_failure", finding_codes(report))

    def test_non_transport_failure_cannot_hide_in_sta_false_proof_sample(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False),
                    "network": network(False),
                    "health": FakeResponse(ValueError("invalid json")),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 4),
            samples,
            (0, 2_000_000_000, 4_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            operator_marker("reconnect_end", 3, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("endpoint_non_transport_failure", finding_codes(report))

    def test_explicit_network_disconnect_can_prove_marked_reconnect(self) -> None:
        samples = capture_states(
            [
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(False),
                },
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 4),
            samples,
            (0, 2_000_000_000, 4_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            operator_marker("reconnect_end", 3, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertIn("planned_reconnect_gap", finding_codes(report))

    def test_two_core_transport_errors_can_prove_partial_reconnect_outage(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {
                    "info": transport_error(),
                    "wifi": transport_error(),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 4),
            samples,
            (0, 2_000_000_000, 4_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            operator_marker("reconnect_end", 3, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)

    def test_broad_transport_proof_does_not_mask_health_client_error(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                {
                    "info": transport_error(),
                    "wifi": transport_error(),
                    "alarms": alarm_rules(False),
                    "network": network(True),
                    "health": gate.DeviceClientError("authentication failed"),
                },
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 4),
            samples,
            (0, 2_000_000_000, 4_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            operator_marker("reconnect_end", 3, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("endpoint_non_transport_failure", finding_codes(report))

    def test_marked_reconnect_fails_when_transition_was_hidden_in_gap(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 7, 700)},
                broad_transport_outage(),
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 8, 900)},
            ]
        )
        for sequence, sample in zip((0, 2, 4), samples, strict=True):
            sample["sequence"] = sequence
        start = gate.marker_record(
            {"valid": True, "kind": "reconnect_start", "created_monotonic_ns": 1_000_000_000},
            1,
            utc_factory=lambda: "2026-07-11T12:00:01.000Z",
            monotonic_ns_factory=lambda: 1_000_000_000,
        )
        end = gate.marker_record(
            {"valid": True, "kind": "reconnect_end", "created_monotonic_ns": 3_000_000_000},
            3,
            utc_factory=lambda: "2026-07-11T12:00:03.000Z",
            monotonic_ns_factory=lambda: 3_000_000_000,
        )

        report = gate.analyze_records(
            [samples[0], start, samples[1], end, samples[2]],
            preflight(alarm_rules(True, 7, 700), wifi_status(True)),
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertIn("reconnect_hidden_or_unverified_transition", finding_codes(report))

    def test_motion_during_reconnect_preserves_exact_edge_and_closes_cycle(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                broad_transport_outage(),
                broad_transport_outage(),
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (1, 3, 5, 7, 9),
            samples,
            (
                100_000_000,
                2_000_000_000,
                3_000_000_000,
                4_000_000_000,
                6_000_000_000,
            ),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            operator_marker("quiet_start", 0, 0),
            samples[0],
            operator_marker("reconnect_start", 2, 1_000_000_000),
            samples[1],
            operator_marker("motion_start", 4, 2_500_000_000),
            samples[2],
            operator_marker("reconnect_end", 6, 3_500_000_000),
            samples[3],
            operator_marker("motion_stop", 8, 4_500_000_000),
            samples[4],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_operator_cycles=1,
            minimum_verified_reconnect_gaps=1,
            minimum_motion_during_reconnect=1,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["operator"]["verified_cycles"], 1)
        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["runtime"]["motion_markers_during_reconnect"], 1)

    def test_motion_reconnect_requires_fresh_network_proof_after_motion_marker(
        self,
    ) -> None:
        samples = capture_states(
            [
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(False),
                },
                {
                    "wifi": transport_error(),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False, 1, 100),
                    "network": network(True),
                },
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (1, 3, 5, 7, 9, 11),
            samples,
            (
                100_000_000,
                2_000_000_000,
                3_000_000_000,
                4_000_000_000,
                5_000_000_000,
                7_000_000_000,
            ),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            operator_marker("quiet_start", 0, 0),
            samples[0],
            operator_marker("reconnect_start", 2, 1_000_000_000),
            samples[1],
            operator_marker("motion_start", 4, 2_500_000_000),
            samples[2],
            operator_marker("reconnect_end", 6, 3_500_000_000),
            samples[3],
            samples[4],
            operator_marker("motion_stop", 10, 5_500_000_000),
            samples[5],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_operator_cycles=1,
            minimum_verified_reconnect_gaps=1,
            minimum_motion_during_reconnect=1,
        )

        self.assertEqual(report["operator"]["verified_cycles"], 1)
        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["runtime"]["motion_markers_during_reconnect"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("motion_during_reconnect_requirement_not_met", finding_codes(report))

    def test_missing_only_before_motion_marker_does_not_qualify_reconnect_edge(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                broad_transport_outage(),
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (1, 3, 5, 7, 9),
            samples,
            (
                100_000_000,
                2_000_000_000,
                3_000_000_000,
                4_000_000_000,
                6_000_000_000,
            ),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            operator_marker("quiet_start", 0, 0),
            samples[0],
            operator_marker("reconnect_start", 2, 1_000_000_000),
            samples[1],
            operator_marker("motion_start", 4, 2_500_000_000),
            samples[2],
            operator_marker("reconnect_end", 6, 3_500_000_000),
            samples[3],
            operator_marker("motion_stop", 8, 4_500_000_000),
            samples[4],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_operator_cycles=1,
            minimum_verified_reconnect_gaps=1,
            minimum_motion_during_reconnect=1,
        )

        self.assertEqual(report["operator"]["verified_cycles"], 1)
        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["runtime"]["motion_markers_during_reconnect"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("motion_during_reconnect_requirement_not_met", finding_codes(report))

    def test_motion_after_observed_recovery_does_not_count_as_during_reconnect(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                broad_transport_outage(),
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (1, 3, 4, 6, 9),
            samples,
            (
                100_000_000,
                2_000_000_000,
                3_000_000_000,
                5_000_000_000,
                8_000_000_000,
            ),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            operator_marker("quiet_start", 0, 0),
            samples[0],
            operator_marker("reconnect_start", 2, 1_000_000_000),
            samples[1],
            samples[2],
            operator_marker("motion_start", 5, 4_000_000_000),
            samples[3],
            operator_marker("reconnect_end", 7, 6_000_000_000),
            operator_marker("motion_stop", 8, 7_000_000_000),
            samples[4],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_operator_cycles=1,
            minimum_verified_reconnect_gaps=1,
            minimum_motion_during_reconnect=1,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["runtime"]["motion_markers_during_reconnect"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("motion_during_reconnect_requirement_not_met", finding_codes(report))

    def test_planned_reconnect_gap_has_finite_continuous_and_cumulative_budget(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
                broad_transport_outage(),
                broad_transport_outage(),
                {"wifi": wifi_status(False), "alarms": alarm_rules(False)},
            ]
        )
        for sequence, sample, timestamp in zip(
            (0, 2, 3, 5),
            samples,
            (0, 10_000_000_000, 180_000_000_000, 200_000_000_000),
            strict=True,
        ):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            samples[0],
            operator_marker("reconnect_start", 1, 1_000_000_000),
            samples[1],
            samples[2],
            operator_marker("reconnect_end", 4, 190_000_000_000),
            samples[3],
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_verified_reconnect_gaps=1,
            max_reconnect_gap_seconds=60.0,
            max_cumulative_reconnect_gap_seconds=120.0,
        )

        self.assertEqual(report["runtime"]["verified_reconnect_gaps"], 1)
        self.assertEqual(report["reconnect_observability"]["max_gap_seconds"], 190.0)
        self.assertEqual(report["verdict"], "fail")
        self.assertTrue(
            {
                "reconnect_gap_run_exceeded",
                "reconnect_gap_cumulative_exceeded",
            }.issubset(finding_codes(report))
        )

    def test_motion_marker_without_quiet_is_rejected_not_a_zero_latency_oracle(self) -> None:
        marker = gate.marker_record(
            {"valid": True, "kind": "motion_start", "created_monotonic_ns": 0},
            0,
            utc_factory=lambda: "2026-07-11T12:00:00.000Z",
            monotonic_ns_factory=lambda: 0,
        )
        sample = capture_states(
            [{"wifi": wifi_status(False), "alarms": alarm_rules(False)}]
        )[0]
        sample["sequence"] = 1

        report = gate.analyze_records(
            [marker, sample],
            preflight(),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertEqual(report["markers"]["kinds"], {"motion_start": 1})
        self.assertIn("operator_marker_out_of_order", finding_codes(report))
        self.assertNotIn("detector_ground_truth_mismatch", finding_codes(report))

    def test_ordered_operator_cycle_meets_configured_hold_deadlines(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        sample_times = (500_000_000, 2_500_000_000, 5_000_000_000)
        for sequence, sample, timestamp in zip((1, 3, 5), samples, sample_times, strict=True):
            sample["sequence"] = sequence
            set_sample_window(sample, timestamp, timestamp)
        records = [
            operator_marker("quiet_start", 0, 0),
            samples[0],
            samples[1],
            # The marker was drained after this in-flight sample, but its action
            # timestamp places it before the sample in the analysis timeline.
            operator_marker("motion_start", 2, 1_000_000_000),
            operator_marker("motion_stop", 4, 3_000_000_000),
            samples[2],
        ]

        report = gate.analyze_records(
            records,
            preflight(config_payload=wifi_config(1200, 2500)),
            minimum_operator_cycles=1,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertEqual(report["operator"]["verified_cycles"], 1)
        self.assertEqual(
            report["operator"]["latencies_ms"],
            {"motion_start": [1500], "motion_stop": [2000], "quiet_start": [500]},
        )

    def test_crossing_motion_stop_target_is_ambiguous_even_if_next_sample_stays_clear(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            marker_file = Path(temporary) / "markers.jsonl"
            marker_tail = gate.MarkerTail(marker_file)
            marker_appended = False

            def append_stop_after_alarm_read(path: str) -> None:
                nonlocal marker_appended
                if path == gate.NETWORK_PATH and not marker_appended:
                    marker_appended = True
                    gate.append_marker(
                        marker_file,
                        "motion_stop",
                        utc_factory=lambda: "2026-07-11T12:00:02.500Z",
                        monotonic_ns_factory=lambda: 2_500_000_000,
                    )

            client = FakeDeviceClient(
                {
                    gate.SYSTEM_INFO_PATH: [system_info()],
                    gate.WIFI_STATUS_PATH: [wifi_status(False)],
                    gate.WIFI_CONFIG_PATH: [wifi_config()],
                    gate.ALARM_RULES_PATH: [alarm_rules(False, 3, 300)],
                    gate.NETWORK_PATH: [network()],
                    gate.HEALTH_PATH: [health()],
                    gate.MUTEX_PATH: [mutexes()],
                },
                on_get=append_stop_after_alarm_read,
            )
            capture_times = iter((2_000_000_000, 3_000_000_000))
            crossing = gate.capture_sample(
                client,
                4,
                gate.HEALTH_PATH,
                gate.MUTEX_PATH,
                consistency_rereads=0,
                utc_factory=lambda: "2026-07-11T12:00:03.000Z",
                monotonic_ns_factory=lambda: next(capture_times),
            )
            stop = gate.marker_record(
                marker_tail.drain()[0],
                5,
                utc_factory=lambda: "2026-07-11T12:00:03.100Z",
                monotonic_ns_factory=lambda: 3_100_000_000,
            )

        baseline, motion, still_clear = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        set_sample_window(baseline, 100_000_000, 200_000_000)
        set_sample_window(motion, 1_000_000_000, 1_100_000_000)
        set_sample_window(still_clear, 4_000_000_000, 4_100_000_000)
        baseline["sequence"] = 1
        motion["sequence"] = 3
        still_clear["sequence"] = 6
        records = [
            operator_marker("quiet_start", 0, 0),
            baseline,
            operator_marker("motion_start", 2, 500_000_000),
            motion,
            crossing,
            stop,
            still_clear,
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            minimum_operator_cycles=1,
        )

        self.assertEqual(crossing["host"]["started_monotonic_ns"], 2_000_000_000)
        self.assertEqual(crossing["host"]["completed_monotonic_ns"], 3_000_000_000)
        self.assertEqual(report["operator"]["verified_cycles"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("operator_response_temporally_ambiguous", finding_codes(report))

    def test_crossing_missing_motion_sample_cannot_be_rescued_by_next_target(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            marker_file = Path(temporary) / "markers.jsonl"
            marker_tail = gate.MarkerTail(marker_file)
            marker_appended = False

            def append_motion_after_wifi_read(path: str) -> None:
                nonlocal marker_appended
                if path == gate.WIFI_CONFIG_PATH and not marker_appended:
                    marker_appended = True
                    gate.append_marker(
                        marker_file,
                        "motion_start",
                        utc_factory=lambda: "2026-07-11T12:00:02.500Z",
                        monotonic_ns_factory=lambda: 2_500_000_000,
                    )

            client = FakeDeviceClient(
                {
                    gate.SYSTEM_INFO_PATH: [system_info()],
                    gate.WIFI_STATUS_PATH: [transport_error()],
                    gate.WIFI_CONFIG_PATH: [wifi_config()],
                    gate.ALARM_RULES_PATH: [alarm_rules(False, 1, 100)],
                    gate.NETWORK_PATH: [network()],
                    gate.HEALTH_PATH: [health()],
                    gate.MUTEX_PATH: [mutexes()],
                },
                on_get=append_motion_after_wifi_read,
            )
            capture_times = iter((2_000_000_000, 3_000_000_000))
            crossing = gate.capture_sample(
                client,
                2,
                gate.HEALTH_PATH,
                gate.MUTEX_PATH,
                consistency_rereads=0,
                utc_factory=lambda: "2026-07-11T12:00:03.000Z",
                monotonic_ns_factory=lambda: next(capture_times),
            )
            motion_start = gate.marker_record(
                marker_tail.drain()[0],
                3,
                utc_factory=lambda: "2026-07-11T12:00:03.100Z",
                monotonic_ns_factory=lambda: 3_100_000_000,
            )

        baseline, motion = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
            ]
        )
        set_sample_window(baseline, 100_000_000, 200_000_000)
        set_sample_window(motion, 4_000_000_000, 4_100_000_000)
        baseline["sequence"] = 1
        motion["sequence"] = 4
        records = [
            operator_marker("quiet_start", 0, 0),
            baseline,
            crossing,
            motion_start,
            motion,
        ]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
            minimum_operator_cycles=1,
        )

        self.assertEqual(report["operator"]["verified_cycles"], 0)
        self.assertEqual(report["verdict"], "fail")
        self.assertIn("operator_response_temporally_ambiguous", finding_codes(report))

    def test_crossing_quiet_baseline_waits_for_first_complete_post_action_sample(
        self,
    ) -> None:
        crossing, after = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
            ]
        )
        set_sample_window(crossing, 2_000_000_000, 3_000_000_000)
        set_sample_window(after, 4_000_000_000, 4_100_000_000)
        records = [operator_marker("quiet_start", 0, 2_500_000_000), crossing, after]

        report = gate.analyze_records(
            records,
            preflight(),
            require_state_cycle=False,
        )

        self.assertEqual(report["verdict"], "pass")
        self.assertNotIn("operator_response_temporally_ambiguous", finding_codes(report))
        self.assertNotIn("operator_response_unverified", finding_codes(report))

    def test_operator_motion_deadline_miss_fails_cycle(self) -> None:
        samples = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
            ]
        )
        set_sample_window(samples[0], 100_000_000, 100_000_000)
        set_sample_window(samples[1], 3_000_000_000, 3_000_000_000)
        records = [
            operator_marker("quiet_start", 0, 0),
            samples[0],
            operator_marker("motion_start", 2, 200_000_000),
            samples[1],
        ]

        report = gate.analyze_records(
            records,
            preflight(config_payload=wifi_config(100, 100)),
            require_state_cycle=False,
            minimum_operator_cycles=1,
            poll_interval_seconds=1.0,
            operator_deadline_margin_seconds=0.0,
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertIn("operator_response_deadline_missed", finding_codes(report))

    def test_csi_drop_counter_increase_fails_gate_with_exact_delta(self) -> None:
        records = capture_states(
            [
                {
                    "wifi": wifi_status(
                        False,
                        queue_drops=2,
                        batch_drops=4,
                        truncated_records=1,
                    ),
                    "alarms": alarm_rules(False),
                },
                {
                    "wifi": wifi_status(
                        True,
                        queue_drops=5,
                        batch_drops=99,
                        truncated_records=3,
                    ),
                    "alarms": alarm_rules(True, 2, 200),
                },
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(
                alarm_rules(False),
                wifi_status(False, queue_drops=2, batch_drops=4, truncated_records=1),
            ),
        )

        self.assertEqual(report["verdict"], "fail")
        self.assertEqual(
            report["runtime"]["drop_deltas"],
            {"capture_truncated_records": 2, "queue_drops_total": 3},
        )
        self.assertNotIn("batches_dropped_total", report["runtime"]["drop_deltas"])
        self.assertIn("drop_counter_delta", finding_codes(report))

    def test_ws_queue_drops_and_lock_timeouts_are_gated(self) -> None:
        first_health = health(100)
        second_health = health(101)
        second_health["http"]["wsQueueDrops"] = 2
        records = capture_states(
            [
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False),
                    "health": first_health,
                    "mutexes": mutexes(4, 7),
                },
                {
                    "wifi": wifi_status(True),
                    "alarms": alarm_rules(True, 2, 200),
                    "health": second_health,
                    "mutexes": mutexes(5, 9),
                },
            ]
        )

        report = gate.analyze_records(
            records,
            preflight(
                alarm_rules(False),
                wifi_status(False),
                first_health,
                mutexes(4, 7),
            ),
        )

        self.assertEqual(report["runtime"]["drop_deltas"], {"http_ws_queue_drops": 2})
        self.assertEqual(
            report["runtime"]["lock_timeout_deltas"],
            {"lock_recursive_timeouts": 2, "lock_standard_timeouts": 1},
        )
        self.assertTrue(
            {"drop_counter_delta", "lock_timeout_delta"}.issubset(finding_codes(report))
        )

    def test_restart_is_detected_from_health_and_uptime(self) -> None:
        records = capture_states(
            [
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False),
                    "health": health(500, 4, 0),
                },
                {
                    "wifi": wifi_status(False),
                    "alarms": alarm_rules(False),
                    "health": health(2, 5, 1),
                    "info": system_info(2),
                },
            ]
        )

        report = gate.analyze_records(records, preflight())

        self.assertEqual(report["runtime"]["restarts"], 1)
        self.assertIn("device_restart", finding_codes(report))
        self.assertIn("boot_id_not_rotated_after_reboot", finding_codes(report))

    def test_reports_and_closure_summary_do_not_leak_raw_device_values(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
            ]
        )
        report = gate.analyze_records(records, preflight())
        report["raw_snapshots"] = {
            "ssid": "PrivateNetwork",
            "mac": "AA:BB:CC:DD:EE:FF",
            "ip": "192.168.0.18",
            "token": "super-secret-token",
            "rule_name": "Private room alarm",
        }

        closure = gate.build_closure_summary(report)
        rendered = json.dumps(closure, sort_keys=True) + gate.render_report_markdown(report)

        for secret in (
            "PrivateNetwork",
            "AA:BB:CC:DD:EE:FF",
            "192.168.0.18",
            "super-secret-token",
            "Private room alarm",
            "private-rule-id",
        ):
            self.assertNotIn(secret, rendered)

    def test_report_artifacts_are_deterministic_for_the_same_records(self) -> None:
        records = capture_states(
            [
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
            ]
        )
        first_report = gate.analyze_records(records, preflight())
        second_report = gate.analyze_records(records, preflight())
        self.assertEqual(first_report, second_report)

        with tempfile.TemporaryDirectory() as temporary:
            roots = [Path(temporary) / name for name in ("first", "second")]
            for root, report in zip(roots, (first_report, second_report), strict=True):
                root.mkdir(mode=0o700)
                gate.write_reports(root, report)
            for name in ("report.json", "report.md", "closure-summary.json"):
                self.assertEqual((roots[0] / name).read_bytes(), (roots[1] / name).read_bytes())

    def test_closure_is_bound_to_private_trace_sha256_without_circular_data(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            trace = Path(temporary) / "trace.jsonl"
            trace.write_bytes(b'{"private":"ssid-and-rule-name"}\n')
            digest = gate.trace_sha256(trace)
            report = gate.analyze_records(
                capture_states(
                    [
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                        {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False, 3, 300)},
                    ]
                ),
                preflight(),
            )
            report["evidence"] = {"trace_sha256": digest}

            closure = gate.build_closure_summary(report)
            rendered = json.dumps(closure, sort_keys=True) + gate.render_report_markdown(report)

            self.assertRegex(digest, r"^sha256:[0-9a-f]{64}$")
            self.assertEqual(closure["trace_sha256"], digest)
            self.assertNotIn("ssid-and-rule-name", rendered)

    def test_trace_marker_and_reports_use_private_permissions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "evidence"
            marker_file = root / "markers.jsonl"
            gate.append_marker(marker_file, "motion_start", "operator note")
            self.assertEqual(os.stat(marker_file).st_mode & 0o777, 0o600)
            tail = gate.MarkerTail(marker_file)
            gate.append_marker(marker_file, "motion_stop")
            self.assertEqual([item["kind"] for item in tail.drain()], ["motion_stop"])

            output = root / "report"
            output.mkdir(mode=0o700)
            report = gate.analyze_records(
                capture_states(
                    [
                        {"wifi": wifi_status(False), "alarms": alarm_rules(False, 1, 100)},
                        {"wifi": wifi_status(True), "alarms": alarm_rules(True, 2, 200)},
                    ]
                ),
                preflight(),
            )
            gate.write_reports(output, report)
            for name in ("report.json", "report.md", "closure-summary.json"):
                self.assertEqual(os.stat(output / name).st_mode & 0o777, 0o600)

    def test_delayed_marker_drain_preserves_action_monotonic_time(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            marker_file = Path(temporary) / "markers.jsonl"
            tail = gate.MarkerTail(marker_file)
            gate.append_marker(
                marker_file,
                "motion_start",
                utc_factory=lambda: "2026-07-11T12:00:01.000Z",
                monotonic_ns_factory=lambda: 123_000_000,
            )

            drained = tail.drain()
            record = gate.marker_record(
                drained[0],
                7,
                utc_factory=lambda: "2026-07-11T12:00:09.000Z",
                monotonic_ns_factory=lambda: 9_000_000_000,
            )

            self.assertEqual(record["marker"]["created_monotonic_ns"], 123_000_000)
            self.assertEqual(record["host"]["monotonic_ns"], 9_000_000_000)


if __name__ == "__main__":
    unittest.main()
