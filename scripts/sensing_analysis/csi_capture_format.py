#!/usr/bin/env python3
"""Strict codecs for MatrixHub lossless CSI captures.

This module only handles transport data.  It deliberately contains no motion
detector or detector expectation logic.  Detector verdicts belong to the native
C++ replay harness; human ground truth lives in ``scenario.json``.
"""

from __future__ import annotations

import os
import struct
from dataclasses import dataclass, replace
from pathlib import Path
from typing import BinaryIO, Iterable, Iterator


FORMAT_MAJOR = 1
FORMAT_MINOR = 0

MHCF_MAGIC = b"MHCF"
MHCF_HEADER_SIZE = 32
MHCF_ENDIAN_MARKER = 0x01020304
MHCF_HEADER = struct.Struct("<4sBBHIHHIIQ")

MHCB_MAGIC = b"MHCB"
MHCB_HEADER_SIZE = 16
MHCB_HEADER = struct.Struct("<4sBBBBHHI")

FRAME_HEADER_SIZE = 64
MAX_IQ_BYTES = 512
MAX_BATCH_RECORDS = 10
MAX_MHCB_MESSAGE_BYTES = (
    MHCB_HEADER_SIZE + MAX_BATCH_RECORDS * (FRAME_HEADER_SIZE + MAX_IQ_BYTES)
)
MAX_FRAME_COUNT = 1_000_000
MAX_CAPTURE_BYTES = 600 * 1024 * 1024
MAX_FRAMES_SECTION_BYTES = MAX_CAPTURE_BYTES - MHCF_HEADER_SIZE
REQUIRED_CAPABILITIES = 0x00FF

BATCH_HELLO = 1
BATCH_DATA = 2
BATCH_END = 3
BATCH_ERROR = 4

HELLO_PAYLOAD_SIZE = 40
END_PAYLOAD_SIZE = 64
HELLO_PAYLOAD = struct.Struct("<IIIIIIHHHHII")
END_PAYLOAD = struct.Struct("<IIIIIIIIIIIIIIII")
ERROR_PAYLOAD = struct.Struct("<HH")

FRAME_FLAG_FIRST_WORD_INVALID = 1 << 0
FRAME_FLAG_TRUNCATED = 1 << 1
FRAME_FLAG_OBSERVED_MOTION = 1 << 2
FRAME_FLAG_REPLAY_ORIGIN = 1 << 3
FRAME_KNOWN_FLAGS = (
    FRAME_FLAG_FIRST_WORD_INVALID
    | FRAME_FLAG_TRUNCATED
    | FRAME_FLAG_OBSERVED_MOTION
    | FRAME_FLAG_REPLAY_ORIGIN
)


class CaptureFormatError(ValueError):
    """Raised when a CSI capture violates the canonical v1 format."""


def _read_exact(handle: BinaryIO, size: int, what: str) -> bytes:
    data = handle.read(size)
    if len(data) != size:
        raise CaptureFormatError(
            f"truncated {what}: expected {size} byte(s), got {len(data)}"
        )
    return data


def float32_from_bits(value: int) -> float:
    return struct.unpack("<f", struct.pack("<I", value & 0xFFFFFFFF))[0]


def float32_bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def format_mac(value: bytes) -> str:
    if len(value) != 6:
        raise CaptureFormatError(f"MAC must contain 6 bytes, got {len(value)}")
    return ":".join(f"{byte:02x}" for byte in value)


@dataclass(frozen=True)
class MhcfHeader:
    session_id: int
    frame_count: int
    frames_section_bytes: int


@dataclass(frozen=True)
class CsiFrame:
    accepted_sequence: int
    process_now_ms: int
    rx_timestamp_us: int
    gain_bits: int
    observed_motion_score_bits: int
    original_len: int
    rx_sequence: int
    signal_len: int
    source_mac: bytes
    destination_mac: bytes
    rssi: int
    noise_floor: int
    rate: int
    signal_mode: int
    mcs: int
    cwb: int
    smoothing: int
    not_sounding: int
    aggregation: int
    stbc: int
    fec: int
    sgi: int
    ampdu_count: int
    channel: int
    secondary_channel: int
    antenna: int
    rx_state: int
    flags: int
    iq: bytes

    @property
    def stored_len(self) -> int:
        return len(self.iq)

    @property
    def gain(self) -> float:
        return float32_from_bits(self.gain_bits)

    @property
    def observed_motion_score(self) -> float:
        return float32_from_bits(self.observed_motion_score_bits)

    @property
    def observed_motion(self) -> bool:
        return bool(self.flags & FRAME_FLAG_OBSERVED_MOTION)

    @property
    def first_word_invalid(self) -> bool:
        return bool(self.flags & FRAME_FLAG_FIRST_WORD_INVALID)

    @property
    def replay_origin(self) -> bool:
        return bool(self.flags & FRAME_FLAG_REPLAY_ORIGIN)

    @property
    def truncated(self) -> bool:
        return bool(self.flags & FRAME_FLAG_TRUNCATED)

    def with_anonymized_macs(self, source_mac: bytes, destination_mac: bytes) -> "CsiFrame":
        return replace(
            self,
            source_mac=source_mac,
            destination_mac=destination_mac,
        )


@dataclass(frozen=True)
class MhcbHeader:
    message_type: int
    record_count: int
    session_id: int


@dataclass(frozen=True)
class CaptureHello:
    start_now_ms: int
    rx_frames_start: int
    rx_accepted_start: int
    queued_start: int
    source_drops_start: int
    throttle_us: int
    max_iq_bytes: int
    max_batch_records: int
    capabilities: int
    session_limit_ms: int
    motion_control_epoch: int


@dataclass(frozen=True)
class CaptureEnd:
    stop_now_ms: int
    end_reason: int
    first_sequence: int
    last_sequence: int
    records_offered: int
    records_enqueued: int
    records_dropped: int
    batches_offered: int
    batches_enqueued: int
    batches_dropped: int
    truncated_records: int
    rx_frames_end: int
    rx_accepted_end: int
    queued_end: int
    source_drops_end: int
    error_flags: int


@dataclass(frozen=True)
class CaptureValidation:
    header: MhcfHeader
    first_sequence: int | None
    last_sequence: int | None
    first_process_now_ms: int | None
    last_process_now_ms: int | None
    observed_motion_frames: int
    first_word_invalid_frames: int
    initial_replay_origin: bool
    unique_source_macs: int
    unique_destination_macs: int


def encode_mhcf_header(header: MhcfHeader) -> bytes:
    if not 0 <= header.session_id <= 0xFFFFFFFF:
        raise CaptureFormatError("session_id must fit uint32")
    if not 0 <= header.frame_count <= MAX_FRAME_COUNT:
        raise CaptureFormatError(f"frame_count exceeds limit {MAX_FRAME_COUNT}")
    if not 0 <= header.frames_section_bytes <= MAX_FRAMES_SECTION_BYTES:
        raise CaptureFormatError(
            f"frames section exceeds limit {MAX_FRAMES_SECTION_BYTES}"
        )
    return MHCF_HEADER.pack(
        MHCF_MAGIC,
        FORMAT_MAJOR,
        FORMAT_MINOR,
        MHCF_HEADER_SIZE,
        MHCF_ENDIAN_MARKER,
        FRAME_HEADER_SIZE,
        0,
        header.session_id,
        header.frame_count,
        header.frames_section_bytes,
    )


def decode_mhcf_header(data: bytes) -> MhcfHeader:
    if len(data) != MHCF_HEADER_SIZE:
        raise CaptureFormatError(
            f"MHCF header must be {MHCF_HEADER_SIZE} bytes, got {len(data)}"
        )
    (
        magic,
        major,
        minor,
        header_size,
        endian_marker,
        frame_header_size,
        flags,
        session_id,
        frame_count,
        frames_section_bytes,
    ) = MHCF_HEADER.unpack(data)
    if magic != MHCF_MAGIC:
        raise CaptureFormatError(f"bad MHCF magic: {magic!r}")
    if (major, minor) != (FORMAT_MAJOR, FORMAT_MINOR):
        raise CaptureFormatError(f"unsupported MHCF version {major}.{minor}")
    if header_size != MHCF_HEADER_SIZE:
        raise CaptureFormatError(f"unexpected MHCF header size {header_size}")
    if endian_marker != MHCF_ENDIAN_MARKER:
        raise CaptureFormatError(f"bad MHCF endian marker 0x{endian_marker:08x}")
    if frame_header_size != FRAME_HEADER_SIZE:
        raise CaptureFormatError(f"unexpected frame header size {frame_header_size}")
    if flags != 0:
        raise CaptureFormatError(f"unknown MHCF flags 0x{flags:04x}")
    header = MhcfHeader(session_id, frame_count, frames_section_bytes)
    encode_mhcf_header(header)
    return header


def validate_frame(frame: CsiFrame, *, reject_truncated: bool = True) -> None:
    if len(frame.source_mac) != 6 or len(frame.destination_mac) != 6:
        raise CaptureFormatError("source and destination MAC fields must contain 6 bytes")
    if frame.stored_len == 0 or frame.stored_len > MAX_IQ_BYTES:
        raise CaptureFormatError(
            f"stored IQ length must be 1..{MAX_IQ_BYTES}, got {frame.stored_len}"
        )
    if frame.stored_len % 2 != 0:
        raise CaptureFormatError(f"stored IQ length must be even, got {frame.stored_len}")
    if not 0 <= frame.original_len <= 0xFFFF:
        raise CaptureFormatError("original IQ length must fit uint16")
    if frame.original_len < frame.stored_len:
        raise CaptureFormatError("original IQ length is smaller than stored IQ length")
    if frame.original_len % 2 != 0:
        raise CaptureFormatError(f"original IQ length must be even, got {frame.original_len}")
    if frame.flags & ~FRAME_KNOWN_FLAGS:
        raise CaptureFormatError(f"unknown frame flags 0x{frame.flags:02x}")
    length_is_truncated = frame.original_len != frame.stored_len
    if frame.truncated != length_is_truncated:
        raise CaptureFormatError("truncated flag disagrees with original/stored IQ lengths")
    if reject_truncated and frame.truncated:
        raise CaptureFormatError("truncated CSI frames are not replayable")
    for name in (
        "cwb",
        "smoothing",
        "not_sounding",
        "aggregation",
        "fec",
        "sgi",
        "antenna",
    ):
        value = getattr(frame, name)
        if value not in (0, 1):
            raise CaptureFormatError(f"{name} must be encoded as 0 or 1, got {value}")
    if not -128 <= frame.rssi <= 127 or not -128 <= frame.noise_floor <= 127:
        raise CaptureFormatError("RSSI and noise floor must fit int8")


def encode_frame(frame: CsiFrame, *, reject_truncated: bool = True) -> bytes:
    validate_frame(frame, reject_truncated=reject_truncated)
    record_size = FRAME_HEADER_SIZE + frame.stored_len
    header = bytearray(FRAME_HEADER_SIZE)
    struct.pack_into("<HH", header, 0, record_size, FRAME_HEADER_SIZE)
    struct.pack_into(
        "<IIIII",
        header,
        4,
        frame.accepted_sequence & 0xFFFFFFFF,
        frame.process_now_ms & 0xFFFFFFFF,
        frame.rx_timestamp_us & 0xFFFFFFFF,
        frame.gain_bits & 0xFFFFFFFF,
        frame.observed_motion_score_bits & 0xFFFFFFFF,
    )
    struct.pack_into(
        "<HHHH",
        header,
        24,
        frame.original_len,
        frame.stored_len,
        frame.rx_sequence & 0xFFFF,
        frame.signal_len & 0xFFFF,
    )
    header[32:38] = frame.source_mac
    header[38:44] = frame.destination_mac
    struct.pack_into("<bb", header, 44, frame.rssi, frame.noise_floor)
    header[46:62] = bytes(
        (
            frame.rate,
            frame.signal_mode,
            frame.mcs,
            frame.cwb,
            frame.smoothing,
            frame.not_sounding,
            frame.aggregation,
            frame.stbc,
            frame.fec,
            frame.sgi,
            frame.ampdu_count,
            frame.channel,
            frame.secondary_channel,
            frame.antenna,
            frame.rx_state,
            frame.flags,
        )
    )
    struct.pack_into("<H", header, 62, 0)
    return bytes(header) + frame.iq


def decode_frame_header(data: bytes, *, reject_truncated: bool = True) -> tuple[CsiFrame, int]:
    if len(data) != FRAME_HEADER_SIZE:
        raise CaptureFormatError(
            f"frame header must be {FRAME_HEADER_SIZE} bytes, got {len(data)}"
        )
    record_size, header_size = struct.unpack_from("<HH", data, 0)
    if header_size != FRAME_HEADER_SIZE:
        raise CaptureFormatError(f"unexpected frame header size {header_size}")
    original_len, stored_len, rx_sequence, signal_len = struct.unpack_from("<HHHH", data, 24)
    if record_size != FRAME_HEADER_SIZE + stored_len:
        raise CaptureFormatError(
            f"record size {record_size} disagrees with stored IQ length {stored_len}"
        )
    if stored_len == 0 or stored_len > MAX_IQ_BYTES or stored_len % 2:
        raise CaptureFormatError(f"invalid stored IQ length {stored_len}")
    if struct.unpack_from("<H", data, 62)[0] != 0:
        raise CaptureFormatError("frame reserved field must be zero")
    values = data[46:62]
    frame = CsiFrame(
        accepted_sequence=struct.unpack_from("<I", data, 4)[0],
        process_now_ms=struct.unpack_from("<I", data, 8)[0],
        rx_timestamp_us=struct.unpack_from("<I", data, 12)[0],
        gain_bits=struct.unpack_from("<I", data, 16)[0],
        observed_motion_score_bits=struct.unpack_from("<I", data, 20)[0],
        original_len=original_len,
        rx_sequence=rx_sequence,
        signal_len=signal_len,
        source_mac=bytes(data[32:38]),
        destination_mac=bytes(data[38:44]),
        rssi=struct.unpack_from("<b", data, 44)[0],
        noise_floor=struct.unpack_from("<b", data, 45)[0],
        rate=values[0],
        signal_mode=values[1],
        mcs=values[2],
        cwb=values[3],
        smoothing=values[4],
        not_sounding=values[5],
        aggregation=values[6],
        stbc=values[7],
        fec=values[8],
        sgi=values[9],
        ampdu_count=values[10],
        channel=values[11],
        secondary_channel=values[12],
        antenna=values[13],
        rx_state=values[14],
        flags=values[15],
        iq=b"",
    )
    # Validate everything that does not depend on the payload itself.
    placeholder = replace(frame, iq=bytes(stored_len))
    validate_frame(placeholder, reject_truncated=reject_truncated)
    return frame, stored_len


def decode_frame(data: bytes, *, reject_truncated: bool = True) -> CsiFrame:
    if len(data) < FRAME_HEADER_SIZE:
        raise CaptureFormatError("truncated frame record")
    frame, stored_len = decode_frame_header(
        data[:FRAME_HEADER_SIZE], reject_truncated=reject_truncated
    )
    expected = FRAME_HEADER_SIZE + stored_len
    if len(data) != expected:
        raise CaptureFormatError(f"frame record must be {expected} bytes, got {len(data)}")
    frame = replace(frame, iq=bytes(data[FRAME_HEADER_SIZE:]))
    validate_frame(frame, reject_truncated=reject_truncated)
    return frame


def read_mhcf_header(path: Path) -> MhcfHeader:
    with path.open("rb") as handle:
        header = decode_mhcf_header(_read_exact(handle, MHCF_HEADER_SIZE, "MHCF header"))
    expected_size = MHCF_HEADER_SIZE + header.frames_section_bytes
    actual_size = path.stat().st_size
    if actual_size != expected_size:
        raise CaptureFormatError(
            f"MHCF size mismatch: header declares {expected_size}, file has {actual_size}"
        )
    return header


def iter_mhcf_frames(path: Path, *, reject_truncated: bool = True) -> Iterator[CsiFrame]:
    header = read_mhcf_header(path)
    with path.open("rb") as handle:
        _read_exact(handle, MHCF_HEADER_SIZE, "MHCF header")
        consumed = 0
        previous_sequence: int | None = None
        for index in range(header.frame_count):
            raw_header = _read_exact(handle, FRAME_HEADER_SIZE, f"frame {index} header")
            frame, stored_len = decode_frame_header(
                raw_header, reject_truncated=reject_truncated
            )
            iq = _read_exact(handle, stored_len, f"frame {index} IQ payload")
            frame = replace(frame, iq=iq)
            validate_frame(frame, reject_truncated=reject_truncated)
            if previous_sequence is not None:
                expected_sequence = (previous_sequence + 1) & 0xFFFFFFFF
                if frame.accepted_sequence != expected_sequence:
                    raise CaptureFormatError(
                        "accepted sequence gap at frame "
                        f"{index}: expected {expected_sequence}, got {frame.accepted_sequence}"
                    )
            previous_sequence = frame.accepted_sequence
            consumed += FRAME_HEADER_SIZE + stored_len
            if consumed > header.frames_section_bytes:
                raise CaptureFormatError("frame records exceed declared MHCF section size")
            yield frame
        if consumed != header.frames_section_bytes:
            raise CaptureFormatError(
                "frame section size mismatch: "
                f"decoded {consumed}, header declares {header.frames_section_bytes}"
            )
        if handle.read(1):
            raise CaptureFormatError("unexpected trailing bytes after MHCF frame section")


def validate_mhcf(path: Path) -> CaptureValidation:
    header = read_mhcf_header(path)
    first_sequence = None
    last_sequence = None
    first_process_now_ms = None
    last_process_now_ms = None
    observed_motion_frames = 0
    first_word_invalid_frames = 0
    initial_replay_origin = False
    source_macs: set[bytes] = set()
    destination_macs: set[bytes] = set()
    actual_count = 0
    for frame in iter_mhcf_frames(path):
        actual_count += 1
        if first_sequence is None:
            first_sequence = frame.accepted_sequence
            first_process_now_ms = frame.process_now_ms
            initial_replay_origin = frame.replay_origin
        elif frame.replay_origin:
            raise CaptureFormatError(
                f"unexpected replay-origin flag on frame {actual_count - 1}"
            )
        last_sequence = frame.accepted_sequence
        last_process_now_ms = frame.process_now_ms
        observed_motion_frames += int(frame.observed_motion)
        first_word_invalid_frames += int(frame.first_word_invalid)
        source_macs.add(frame.source_mac)
        destination_macs.add(frame.destination_mac)
    if actual_count != header.frame_count:
        raise CaptureFormatError(
            f"decoded frame count {actual_count} disagrees with header {header.frame_count}"
        )
    if actual_count == 0:
        raise CaptureFormatError("capture contains no CSI frames")
    if not initial_replay_origin:
        raise CaptureFormatError("first captured frame has no replay-origin flag")
    return CaptureValidation(
        header=header,
        first_sequence=first_sequence,
        last_sequence=last_sequence,
        first_process_now_ms=first_process_now_ms,
        last_process_now_ms=last_process_now_ms,
        observed_motion_frames=observed_motion_frames,
        first_word_invalid_frames=first_word_invalid_frames,
        initial_replay_origin=initial_replay_origin,
        unique_source_macs=len(source_macs),
        unique_destination_macs=len(destination_macs),
    )


class MhcfWriter:
    """Streaming writer that patches the final header only after a clean END."""

    def __init__(self, path: Path, session_id: int) -> None:
        self.path = path
        self.session_id = session_id
        self.frame_count = 0
        self.frames_section_bytes = 0
        self.first_sequence: int | None = None
        self.last_sequence: int | None = None
        self.initial_replay_origin = False
        self._handle = path.open("w+b")
        os.chmod(path, 0o600)
        self._handle.write(encode_mhcf_header(MhcfHeader(session_id, 0, 0)))
        self._finalized = False

    def append(self, frame: CsiFrame) -> None:
        if self._finalized:
            raise CaptureFormatError("cannot append to a finalized MHCF file")
        if self.frame_count >= MAX_FRAME_COUNT:
            raise CaptureFormatError(f"capture exceeds {MAX_FRAME_COUNT} frames")
        if self.frame_count == 0:
            if not frame.replay_origin:
                raise CaptureFormatError(
                    "first captured frame has no replay-origin flag"
                )
        elif frame.replay_origin:
            raise CaptureFormatError("replay-origin flag is only valid on the first frame")
        if self.last_sequence is not None:
            expected = (self.last_sequence + 1) & 0xFFFFFFFF
            if frame.accepted_sequence != expected:
                raise CaptureFormatError(
                    f"accepted sequence gap: expected {expected}, got {frame.accepted_sequence}"
                )
        encoded = encode_frame(frame)
        if self.frames_section_bytes + len(encoded) > MAX_FRAMES_SECTION_BYTES:
            raise CaptureFormatError(f"capture exceeds {MAX_CAPTURE_BYTES} total bytes")
        self._handle.write(encoded)
        self.frame_count += 1
        self.frames_section_bytes += len(encoded)
        if self.first_sequence is None:
            self.first_sequence = frame.accepted_sequence
            self.initial_replay_origin = frame.replay_origin
        self.last_sequence = frame.accepted_sequence

    def finalize(self) -> MhcfHeader:
        if self._finalized:
            raise CaptureFormatError("MHCF file was already finalized")
        if self.frame_count == 0:
            raise CaptureFormatError("refusing to finalize an empty CSI capture")
        if not self.initial_replay_origin:
            raise CaptureFormatError("refusing to finalize without replay-origin")
        header = MhcfHeader(self.session_id, self.frame_count, self.frames_section_bytes)
        self._handle.flush()
        self._handle.seek(0)
        self._handle.write(encode_mhcf_header(header))
        self._handle.flush()
        os.fsync(self._handle.fileno())
        self._handle.close()
        self._finalized = True
        return header

    def abort(self) -> None:
        if not self._handle.closed:
            self._handle.flush()
            self._handle.close()

    def __enter__(self) -> "MhcfWriter":
        return self

    def __exit__(self, exc_type, exc, _traceback) -> None:
        if exc_type is not None or not self._finalized:
            self.abort()


def write_mhcf(path: Path, session_id: int, frames: Iterable[CsiFrame]) -> MhcfHeader:
    with MhcfWriter(path, session_id) as writer:
        for frame in frames:
            writer.append(frame)
        return writer.finalize()


def decode_mhcb_header(data: bytes) -> MhcbHeader:
    if len(data) < MHCB_HEADER_SIZE:
        raise CaptureFormatError("truncated MHCB header")
    (
        magic,
        major,
        minor,
        message_type,
        header_size,
        frame_header_size,
        record_count,
        session_id,
    ) = MHCB_HEADER.unpack_from(data)
    if magic != MHCB_MAGIC:
        raise CaptureFormatError(f"bad MHCB magic: {magic!r}")
    if (major, minor) != (FORMAT_MAJOR, FORMAT_MINOR):
        raise CaptureFormatError(f"unsupported MHCB version {major}.{minor}")
    if header_size != MHCB_HEADER_SIZE:
        raise CaptureFormatError(f"unexpected MHCB header size {header_size}")
    if frame_header_size != FRAME_HEADER_SIZE:
        raise CaptureFormatError(f"unexpected MHCB record header size {frame_header_size}")
    if message_type not in {BATCH_HELLO, BATCH_DATA, BATCH_END, BATCH_ERROR}:
        raise CaptureFormatError(f"unknown MHCB message type {message_type}")
    if message_type == BATCH_DATA and record_count == 0:
        raise CaptureFormatError("MHCB DATA message contains no records")
    if record_count > MAX_BATCH_RECORDS:
        raise CaptureFormatError(
            f"MHCB record count {record_count} exceeds v1 limit {MAX_BATCH_RECORDS}"
        )
    if message_type != BATCH_DATA and record_count != 0:
        raise CaptureFormatError("MHCB control message must have record_count=0")
    return MhcbHeader(message_type, record_count, session_id)


def decode_mhcb_frames(data: bytes) -> tuple[MhcbHeader, tuple[CsiFrame, ...]]:
    header = decode_mhcb_header(data)
    if header.message_type != BATCH_DATA:
        raise CaptureFormatError("expected MHCB DATA message")
    offset = MHCB_HEADER_SIZE
    frames: list[CsiFrame] = []
    for index in range(header.record_count):
        if offset + FRAME_HEADER_SIZE > len(data):
            raise CaptureFormatError(f"truncated MHCB frame {index} header")
        _, stored_len = decode_frame_header(data[offset : offset + FRAME_HEADER_SIZE])
        end = offset + FRAME_HEADER_SIZE + stored_len
        if end > len(data):
            raise CaptureFormatError(f"truncated MHCB frame {index} IQ payload")
        frames.append(decode_frame(data[offset:end]))
        offset = end
    if offset != len(data):
        raise CaptureFormatError(f"MHCB DATA has {len(data) - offset} trailing byte(s)")
    return header, tuple(frames)


def decode_capture_hello(data: bytes) -> tuple[MhcbHeader, CaptureHello]:
    header = decode_mhcb_header(data)
    if header.message_type != BATCH_HELLO:
        raise CaptureFormatError("expected MHCB HELLO message")
    payload = data[MHCB_HEADER_SIZE:]
    if len(payload) != HELLO_PAYLOAD_SIZE:
        raise CaptureFormatError(
            f"HELLO payload must be {HELLO_PAYLOAD_SIZE} bytes, got {len(payload)}"
        )
    (
        start_now_ms,
        rx_frames_start,
        rx_accepted_start,
        queued_start,
        source_drops_start,
        throttle_us,
        max_iq_bytes,
        max_batch_records,
        record_header_size,
        capabilities,
        session_limit_ms,
        motion_control_epoch,
    ) = HELLO_PAYLOAD.unpack(payload)
    if record_header_size != FRAME_HEADER_SIZE:
        raise CaptureFormatError(f"HELLO record header size is {record_header_size}")
    if max_iq_bytes > MAX_IQ_BYTES:
        raise CaptureFormatError(f"HELLO max IQ length {max_iq_bytes} exceeds v1 limit")
    if not 1 <= max_batch_records <= MAX_BATCH_RECORDS:
        raise CaptureFormatError(
            "HELLO max batch record count must be "
            f"1..{MAX_BATCH_RECORDS}, got {max_batch_records}"
        )
    if capabilities & REQUIRED_CAPABILITIES != REQUIRED_CAPABILITIES:
        raise CaptureFormatError(
            f"HELLO capabilities 0x{capabilities:04x} miss required v1 fields"
        )
    return header, CaptureHello(
        start_now_ms,
        rx_frames_start,
        rx_accepted_start,
        queued_start,
        source_drops_start,
        throttle_us,
        max_iq_bytes,
        max_batch_records,
        capabilities,
        session_limit_ms,
        motion_control_epoch,
    )


def decode_capture_end(data: bytes) -> tuple[MhcbHeader, CaptureEnd]:
    header = decode_mhcb_header(data)
    if header.message_type != BATCH_END:
        raise CaptureFormatError("expected MHCB END message")
    payload = data[MHCB_HEADER_SIZE:]
    if len(payload) != END_PAYLOAD_SIZE:
        raise CaptureFormatError(
            f"END payload must be {END_PAYLOAD_SIZE} bytes, got {len(payload)}"
        )
    return header, CaptureEnd(*END_PAYLOAD.unpack(payload))


def decode_capture_error(data: bytes) -> tuple[MhcbHeader, int]:
    header = decode_mhcb_header(data)
    if header.message_type != BATCH_ERROR:
        raise CaptureFormatError("expected MHCB ERROR message")
    payload = data[MHCB_HEADER_SIZE:]
    if len(payload) != ERROR_PAYLOAD.size:
        raise CaptureFormatError(
            f"ERROR payload must be {ERROR_PAYLOAD.size} bytes, got {len(payload)}"
        )
    error_code, reserved = ERROR_PAYLOAD.unpack(payload)
    if reserved != 0:
        raise CaptureFormatError("ERROR reserved field must be zero")
    if error_code not in {1, 2, 3, 4, 5}:
        raise CaptureFormatError(f"unknown capture error code {error_code}")
    return header, error_code
