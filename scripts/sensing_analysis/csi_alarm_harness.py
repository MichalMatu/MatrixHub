#!/usr/bin/env python3
"""Offline CSI motion alarm harness.

This is deliberately not production alarm code. It is a falsification harness for
candidate CSI alarm algorithms before any firmware/UI alarm source is exposed.
"""

from __future__ import annotations

import argparse
import base64
import json
import math
import random
import statistics
import sys
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


STATE_CALIBRATING = "calibrating"
STATE_MONITORING = "monitoring"
STATE_MOTION_CANDIDATE = "motion_candidate"
STATE_MOTION_CONFIRMED = "motion_confirmed"
STATE_NOISY = "noisy_environment"
STATE_UNAVAILABLE = "unavailable"


@dataclass(frozen=True)
class CsiFrame:
    timestamp_ms: int
    rssi: int
    payload: tuple[int, ...]


@dataclass
class DetectorConfig:
    warmup_frames: int = 30
    calibration_frames: int = 150
    score_window: int = 40
    selected_subcarriers: int = 16
    enter_threshold: float = 6.0
    clear_threshold: float = 3.0
    noisy_threshold: float = 80.0
    min_duration_ms: int = 1500
    cooldown_ms: int = 5000


@dataclass
class DetectorSnapshot:
    timestamp_ms: int
    state: str
    score: float
    confidence: float
    baseline_ready: bool
    frames_seen: int
    reason: str = ""


def _signed_bytes_from_hex(value: str) -> tuple[int, ...]:
    raw = bytes.fromhex(value)
    return tuple(byte if byte < 128 else byte - 256 for byte in raw)


def _signed_bytes_from_base64(value: str) -> tuple[int, ...]:
    raw = base64.b64decode(value)
    return tuple(byte if byte < 128 else byte - 256 for byte in raw)


def parse_jsonl(path: Path) -> list[CsiFrame]:
    frames: list[CsiFrame] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON: {exc}") from exc

            timestamp = record.get("timestamp_ms", record.get("timestamp", len(frames) * 100))
            rssi = record.get("rssi", 0)
            if "payload" in record:
                payload = tuple(int(v) for v in record["payload"])
            elif "payload_hex" in record:
                payload = _signed_bytes_from_hex(str(record["payload_hex"]))
            elif "payload_base64" in record:
                payload = _signed_bytes_from_base64(str(record["payload_base64"]))
            else:
                raise ValueError(f"{path}:{line_no}: missing payload/payload_hex/payload_base64")

            frames.append(CsiFrame(timestamp_ms=int(timestamp), rssi=int(rssi), payload=payload))
    return frames


def iq_energy(payload: Sequence[int], subcarriers: int = 64) -> list[float]:
    if len(payload) < 2:
        return []
    pair_count = min(len(payload) // 2, subcarriers)
    energies: list[float] = []
    for index in range(pair_count):
        real = int(payload[index * 2])
        imag = int(payload[index * 2 + 1])
        energies.append(float(real * real + imag * imag))
    return energies


def _mean(values: Sequence[float]) -> float:
    return sum(values) / len(values) if values else 0.0


class CsiAlarmDetector:
    def __init__(self, config: DetectorConfig | None = None) -> None:
        self.config = config or DetectorConfig()
        self.state = STATE_CALIBRATING
        self.frames_seen = 0
        self._calibration_vectors: list[list[float]] = []
        self._baseline: list[float] = []
        self._noise: list[float] = []
        self._score_window: deque[float] = deque(maxlen=self.config.score_window)
        self._candidate_since_ms: int | None = None
        self._clear_since_ms: int | None = None
        self._cooldown_until_ms = 0

    @property
    def baseline_ready(self) -> bool:
        return bool(self._baseline)

    def process(self, frame: CsiFrame) -> DetectorSnapshot:
        self.frames_seen += 1
        energies = iq_energy(frame.payload)
        if len(energies) < 8:
            self.state = STATE_UNAVAILABLE
            return DetectorSnapshot(
                timestamp_ms=frame.timestamp_ms,
                state=self.state,
                score=0.0,
                confidence=0.0,
                baseline_ready=self.baseline_ready,
                frames_seen=self.frames_seen,
                reason="malformed_or_empty_payload",
            )

        if self.frames_seen <= self.config.warmup_frames:
            self.state = STATE_CALIBRATING
            return self._snapshot(frame.timestamp_ms, 0.0, "warmup")

        if not self.baseline_ready:
            self._calibration_vectors.append(energies)
            if len(self._calibration_vectors) >= self.config.calibration_frames:
                self._finalize_calibration()
                self.state = STATE_MONITORING
                return self._snapshot(frame.timestamp_ms, 0.0, "calibration_complete")
            self.state = STATE_CALIBRATING
            return self._snapshot(frame.timestamp_ms, 0.0, "calibrating")

        score = self._score(energies)
        self._score_window.append(score)
        smoothed_score = _mean(tuple(self._score_window))
        self._update_state(frame.timestamp_ms, smoothed_score)
        return self._snapshot(frame.timestamp_ms, smoothed_score)

    def _finalize_calibration(self) -> None:
        width = min(len(vector) for vector in self._calibration_vectors)
        self._baseline = []
        self._noise = []
        for index in range(width):
            values = [vector[index] for vector in self._calibration_vectors]
            mean_value = _mean(values)
            self._baseline.append(mean_value)
            if len(values) > 1:
                self._noise.append(max(1.0, statistics.pstdev(values)))
            else:
                self._noise.append(1.0)
        self._calibration_vectors.clear()

    def _score(self, energies: Sequence[float]) -> float:
        width = min(len(energies), len(self._baseline), len(self._noise))
        z_scores = [
            abs(float(energies[index]) - self._baseline[index]) / max(1.0, self._noise[index])
            for index in range(width)
        ]
        if not z_scores:
            return 0.0
        selected = sorted(z_scores, reverse=True)[: self.config.selected_subcarriers]
        return _mean(selected)

    def _update_state(self, timestamp_ms: int, score: float) -> None:
        if score >= self.config.noisy_threshold:
            if self._candidate_since_ms is None:
                self._candidate_since_ms = timestamp_ms
            if timestamp_ms - self._candidate_since_ms >= self.config.min_duration_ms:
                self.state = STATE_NOISY
            return

        if timestamp_ms < self._cooldown_until_ms:
            self.state = STATE_MONITORING
            return

        if score >= self.config.enter_threshold:
            if self._candidate_since_ms is None:
                self._candidate_since_ms = timestamp_ms
            if timestamp_ms - self._candidate_since_ms >= self.config.min_duration_ms:
                self.state = STATE_MOTION_CONFIRMED
            else:
                self.state = STATE_MOTION_CANDIDATE
            self._clear_since_ms = None
            return

        self._candidate_since_ms = None

        if self.state in {STATE_MOTION_CONFIRMED, STATE_MOTION_CANDIDATE, STATE_NOISY}:
            if score <= self.config.clear_threshold:
                if self._clear_since_ms is None:
                    self._clear_since_ms = timestamp_ms
                if timestamp_ms - self._clear_since_ms >= self.config.min_duration_ms:
                    if self.state == STATE_MOTION_CONFIRMED:
                        self._cooldown_until_ms = timestamp_ms + self.config.cooldown_ms
                    self.state = STATE_MONITORING
                    self._clear_since_ms = None
                return

        self.state = STATE_MONITORING

    def _snapshot(self, timestamp_ms: int, score: float, reason: str = "") -> DetectorSnapshot:
        confidence = 0.0
        if self.config.enter_threshold > self.config.clear_threshold:
            confidence = (score - self.config.clear_threshold) / (
                self.config.enter_threshold - self.config.clear_threshold
            )
            confidence = max(0.0, min(1.0, confidence))
        return DetectorSnapshot(
            timestamp_ms=timestamp_ms,
            state=self.state,
            score=score,
            confidence=confidence,
            baseline_ready=self.baseline_ready,
            frames_seen=self.frames_seen,
            reason=reason,
        )


def _payload(base: int, changed: int = 0, noisy: bool = False) -> tuple[int, ...]:
    values: list[int] = []
    for carrier in range(64):
        if noisy:
            real = random.randint(-80, 80)
            imag = random.randint(-80, 80)
        else:
            local = base + int(2 * math.sin(carrier / 5.0))
            if 18 <= carrier < 34:
                local += changed
            real = local
            imag = 3 + int(math.cos(carrier / 7.0))
        values.extend([max(-127, min(127, real)), max(-127, min(127, imag))])
    return tuple(values)


def synthetic_frames(kind: str, count: int = 360, seed: int = 42) -> list[CsiFrame]:
    random.seed(seed)
    frames: list[CsiFrame] = []
    for index in range(count):
        timestamp_ms = index * 100
        if kind == "quiet":
            payload = _payload(12 + (index % 3) - 1)
        elif kind == "motion":
            changed = 18 if 220 <= index < 285 else 0
            payload = _payload(12 + (index % 3) - 1, changed=changed)
        elif kind == "noisy":
            payload = _payload(12, noisy=index >= 210)
        else:
            raise ValueError(f"unknown synthetic scenario: {kind}")
        frames.append(CsiFrame(timestamp_ms=timestamp_ms, rssi=-65, payload=payload))
    return frames


def run_detector(frames: Iterable[CsiFrame], config: DetectorConfig | None = None) -> dict:
    detector = CsiAlarmDetector(config)
    snapshots: list[DetectorSnapshot] = []
    for frame in frames:
        snapshots.append(detector.process(frame))

    state_counts: dict[str, int] = {}
    max_score = 0.0
    for snapshot in snapshots:
        state_counts[snapshot.state] = state_counts.get(snapshot.state, 0) + 1
        max_score = max(max_score, snapshot.score)

    final = snapshots[-1] if snapshots else DetectorSnapshot(
        timestamp_ms=0,
        state=STATE_UNAVAILABLE,
        score=0.0,
        confidence=0.0,
        baseline_ready=False,
        frames_seen=0,
        reason="no_frames",
    )
    return {
        "frames": len(snapshots),
        "final_state": final.state,
        "state_counts": state_counts,
        "max_score": round(max_score, 3),
        "final_score": round(final.score, 3),
        "final_confidence": round(final.confidence, 3),
        "baseline_ready": final.baseline_ready,
        "motion_confirmed_frames": state_counts.get(STATE_MOTION_CONFIRMED, 0),
        "noisy_frames": state_counts.get(STATE_NOISY, 0),
    }


def check_expectation(summary: dict, expectation: str) -> tuple[bool, str]:
    if expectation == "any":
        return summary["frames"] > 0, "expected at least one frame"
    if expectation == "quiet":
        ok = summary["motion_confirmed_frames"] == 0 and summary["noisy_frames"] == 0
        return ok, "expected no motion and no noisy-environment state"
    if expectation == "motion":
        ok = summary["motion_confirmed_frames"] > 0
        return ok, "expected confirmed motion frames"
    if expectation == "noisy":
        ok = summary["noisy_frames"] > 0 and summary["motion_confirmed_frames"] == 0
        return ok, "expected noisy environment without confirmed motion"
    raise ValueError(f"unknown expectation: {expectation}")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--input", type=Path, help="JSONL capture with CSI payloads.")
    source.add_argument("--synthetic", choices=("quiet", "motion", "noisy"))
    parser.add_argument("--expect", choices=("any", "quiet", "motion", "noisy"), default="any")
    parser.add_argument("--json", action="store_true", help="Emit JSON only.")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        frames = parse_jsonl(args.input) if args.input else synthetic_frames(args.synthetic)
        summary = run_detector(frames)
        ok, message = check_expectation(summary, args.expect)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        print(json.dumps(summary, indent=2, sort_keys=True))
        print("PASS" if ok else f"FAIL: {message}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
