#!/usr/bin/env python3
"""Render and audit Matrix native 3D effects on the host."""

from __future__ import annotations

import subprocess
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
BUILD_DIR = REPO_ROOT / ".pio" / "matrix3d_audit"
SOURCE_PATH = BUILD_DIR / "matrix3d_audit.cpp"
BINARY_PATH = BUILD_DIR / "matrix3d_audit"
PPM_PATH = BUILD_DIR / "matrix_3d_effects.ppm"
TEMPORAL_PPM_PATH = BUILD_DIR / "matrix_3d_temporal.ppm"


CPP_SOURCE = r'''
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "lib/matrix_service/effects/MatrixFxEngine3D.cpp"

namespace {

constexpr uint8_t kScale = 8;
constexpr uint8_t kTemporalScale = 4;
constexpr uint8_t kRows = 3;
constexpr uint8_t kModes = MATRIX_FX::kNative3DModeMax + 1;
constexpr uint8_t kTemporalFrames = 48;
constexpr uint16_t kImageWidth = MATRIX_FX::kMatrixFxWidth * kScale * kModes;
constexpr uint16_t kImageHeight = MATRIX_FX::kMatrixFxHeight * kScale * kRows;
constexpr uint16_t kTemporalImageWidth = MATRIX_FX::kMatrixFxWidth * kTemporalScale * kTemporalFrames;
constexpr uint16_t kTemporalImageHeight = MATRIX_FX::kMatrixFxHeight * kTemporalScale * kModes;

struct TemporalMetrics {
    float avgBrightnessDelta = 0.0f;
    float hardPixelRate = 0.0f;
    float onOffRate = 0.0f;
    uint8_t maxPixelDelta = 0;
    uint8_t minLit = MATRIX_FX::kMatrixFxPixelCount;
};

MATRIX_FX::MatrixFxConfig makeConfig(uint8_t mode, bool warm) {
    MATRIX_FX::MatrixFxConfig config;
    config.mode = mode;
    config.speedMs = 900;
    config.provider = MATRIX_FX::ReactiveProvider::Imu;
    config.reactivityGain = 125;
    if (warm) {
        config.color1 = 0x400020;
        config.color2 = 0xFF5030;
        config.color3 = 0xFFD060;
    } else {
        config.color1 = 0x0030FF;
        config.color2 = 0x00FF90;
        config.color3 = 0xB0F0FF;
    }
    return config;
}

MATRIX_FX::MatrixFxInput makeInput(bool tilted) {
    MATRIX_FX::MatrixFxInput input;
    if (tilted) {
        input.imuValid = true;
        input.ax = -0.72f;
        input.ay = 0.58f;
        input.az = 0.38f;
        input.gx = 90.0f;
        input.gy = -55.0f;
        input.gz = 40.0f;
        input.motionEnergy = 0.45f;
    }
    return input;
}

void renderFrame(uint8_t mode, bool warm, bool tilted, uint32_t* frame) {
    MATRIX_FX::MatrixFxEngine3D engine;
    engine.configure(makeConfig(mode, warm));
    engine.setInput(makeInput(tilted));
    for (uint8_t step = 0; step < 8; ++step) {
        if (!engine.render(1000 + step * 33, frame, MATRIX_FX::kMatrixFxPixelCount)) {
            std::fprintf(stderr, "render failed for mode %u\n", mode);
            std::exit(2);
        }
    }
}

uint8_t brightness(uint32_t color) {
    const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(color & 0xFF);
    return static_cast<uint8_t>((static_cast<uint16_t>(r) + g + b) / 3);
}

uint32_t litPixels(const uint32_t* frame) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        if ((frame[i] & 0x00FFFFFFu) != 0) {
            count++;
        }
    }
    return count;
}

uint32_t uniqueColors(const uint32_t* frame) {
    uint32_t unique = 0;
    uint32_t seen[MATRIX_FX::kMatrixFxPixelCount]{};
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        const uint32_t color = frame[i] & 0x00FFFFFFu;
        bool exists = false;
        for (uint8_t j = 0; j < unique; ++j) {
            if (seen[j] == color) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            seen[unique++] = color;
        }
    }
    return unique;
}

uint32_t changedPixels(const uint32_t* a, const uint32_t* b) {
    uint32_t count = 0;
    for (uint8_t i = 0; i < MATRIX_FX::kMatrixFxPixelCount; ++i) {
        if ((a[i] & 0x00FFFFFFu) != (b[i] & 0x00FFFFFFu)) {
            count++;
        }
    }
    return count;
}

uint8_t pixelDelta(uint32_t a, uint32_t b) {
    const uint8_t ar = static_cast<uint8_t>((a >> 16) & 0xFF);
    const uint8_t ag = static_cast<uint8_t>((a >> 8) & 0xFF);
    const uint8_t ab = static_cast<uint8_t>(a & 0xFF);
    const uint8_t br = static_cast<uint8_t>((b >> 16) & 0xFF);
    const uint8_t bg = static_cast<uint8_t>((b >> 8) & 0xFF);
    const uint8_t bb = static_cast<uint8_t>(b & 0xFF);
    const uint16_t dr = static_cast<uint16_t>(std::max(ar, br) - std::min(ar, br));
    const uint16_t dg = static_cast<uint16_t>(std::max(ag, bg) - std::min(ag, bg));
    const uint16_t db = static_cast<uint16_t>(std::max(ab, bb) - std::min(ab, bb));
    return static_cast<uint8_t>((dr + dg + db) / 3);
}

void writePixel(FILE* file, uint32_t color) {
    const uint8_t r = static_cast<uint8_t>((color >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((color >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(color & 0xFF);
    std::fprintf(file, "%u %u %u ", r, g, b);
}

TemporalMetrics renderTemporalSequence(uint8_t mode,
                                       uint32_t frames[kTemporalFrames][MATRIX_FX::kMatrixFxPixelCount]) {
    MATRIX_FX::MatrixFxEngine3D engine;
    MATRIX_FX::MatrixFxConfig config = makeConfig(mode, false);
    config.speedMs = 1000;
    config.reactivityGain = 95;
    engine.configure(config);
    engine.setInput(makeInput(true));

    TemporalMetrics metrics;
    uint32_t current[MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t totalBrightnessDelta = 0;
    uint32_t hardPixels = 0;
    uint32_t onOffTransitions = 0;

    for (uint8_t warmup = 0; warmup < 8; ++warmup) {
        const uint32_t nowMs = 1000 + static_cast<uint32_t>(warmup) * 33;
        if (!engine.render(nowMs, current, MATRIX_FX::kMatrixFxPixelCount)) {
            std::fprintf(stderr, "temporal warmup failed for mode %u\n", mode);
            std::exit(4);
        }
    }

    for (uint8_t frame = 0; frame < kTemporalFrames; ++frame) {
        const uint32_t nowMs = 1000 + static_cast<uint32_t>(frame + 8) * 33;
        if (!engine.render(nowMs, current, MATRIX_FX::kMatrixFxPixelCount)) {
            std::fprintf(stderr, "temporal render failed for mode %u\n", mode);
            std::exit(4);
        }
        std::memcpy(frames[frame], current, sizeof(current));
        metrics.minLit = std::min<uint8_t>(metrics.minLit, static_cast<uint8_t>(litPixels(frames[frame])));

        if (frame == 0) {
            continue;
        }
        for (uint8_t pixel = 0; pixel < MATRIX_FX::kMatrixFxPixelCount; ++pixel) {
            const uint8_t previousBrightness = brightness(frames[frame - 1][pixel]);
            const uint8_t currentBrightness = brightness(frames[frame][pixel]);
            const uint8_t delta = previousBrightness > currentBrightness
                ? previousBrightness - currentBrightness
                : currentBrightness - previousBrightness;
            const uint8_t colorDelta = pixelDelta(frames[frame - 1][pixel], frames[frame][pixel]);
            metrics.maxPixelDelta = std::max(metrics.maxPixelDelta, delta);
            totalBrightnessDelta += delta;
            if (delta > 70 || colorDelta > 95) {
                hardPixels++;
            }
            if ((previousBrightness <= 10 && currentBrightness >= 42) ||
                (previousBrightness >= 42 && currentBrightness <= 10)) {
                onOffTransitions++;
            }
        }
    }

    const float sampleCount = static_cast<float>((kTemporalFrames - 1) * MATRIX_FX::kMatrixFxPixelCount);
    metrics.avgBrightnessDelta = static_cast<float>(totalBrightnessDelta) / sampleCount;
    metrics.hardPixelRate = static_cast<float>(hardPixels) / sampleCount;
    metrics.onOffRate = static_cast<float>(onOffTransitions) / sampleCount;
    return metrics;
}

void writeTemporalSheet(
    const char* path,
    const uint32_t frames[kModes][kTemporalFrames][MATRIX_FX::kMatrixFxPixelCount]) {
    FILE* file = std::fopen(path, "w");
    if (!file) {
        std::perror(path);
        std::exit(5);
    }

    std::fprintf(file, "P3\n%u %u\n255\n", kTemporalImageWidth, kTemporalImageHeight);
    for (uint16_t outY = 0; outY < kTemporalImageHeight; ++outY) {
        const uint8_t mode = outY / (MATRIX_FX::kMatrixFxHeight * kTemporalScale);
        const uint8_t localY = (outY / kTemporalScale) % MATRIX_FX::kMatrixFxHeight;
        for (uint16_t outX = 0; outX < kTemporalImageWidth; ++outX) {
            const uint8_t frame = outX / (MATRIX_FX::kMatrixFxWidth * kTemporalScale);
            const uint8_t localX = (outX / kTemporalScale) % MATRIX_FX::kMatrixFxWidth;
            writePixel(file, frames[mode][frame][localY * MATRIX_FX::kMatrixFxWidth + localX]);
        }
        std::fputc('\n', file);
    }
    std::fclose(file);
}

void writeSheet(const char* path, const uint32_t frames[kRows][kModes][MATRIX_FX::kMatrixFxPixelCount]) {
    FILE* file = std::fopen(path, "w");
    if (!file) {
        std::perror(path);
        std::exit(3);
    }

    std::fprintf(file, "P3\n%u %u\n255\n", kImageWidth, kImageHeight);
    for (uint16_t outY = 0; outY < kImageHeight; ++outY) {
        const uint8_t row = outY / (MATRIX_FX::kMatrixFxHeight * kScale);
        const uint8_t localY = (outY / kScale) % MATRIX_FX::kMatrixFxHeight;
        for (uint16_t outX = 0; outX < kImageWidth; ++outX) {
            const uint8_t mode = outX / (MATRIX_FX::kMatrixFxWidth * kScale);
            const uint8_t localX = (outX / kScale) % MATRIX_FX::kMatrixFxWidth;
            writePixel(file, frames[row][mode][localY * MATRIX_FX::kMatrixFxWidth + localX]);
        }
        std::fputc('\n', file);
    }
    std::fclose(file);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: %s output.ppm temporal.ppm\n", argv[0]);
        return 2;
    }

    uint32_t frames[kRows][kModes][MATRIX_FX::kMatrixFxPixelCount]{};
    uint32_t temporalFrames[kModes][kTemporalFrames][MATRIX_FX::kMatrixFxPixelCount]{};
    bool ok = true;

    for (uint8_t mode = 0; mode < kModes; ++mode) {
        renderFrame(mode, false, false, frames[0][mode]);
        renderFrame(mode, false, true, frames[1][mode]);
        renderFrame(mode, true, true, frames[2][mode]);

        const uint32_t lit = litPixels(frames[0][mode]);
        const uint32_t unique = uniqueColors(frames[0][mode]);
        const uint32_t imuDelta = changedPixels(frames[0][mode], frames[1][mode]);
        const uint32_t paletteDelta = changedPixels(frames[1][mode], frames[2][mode]);

        std::printf("mode=%02u lit=%02lu unique=%02lu imu_delta=%02lu palette_delta=%02lu\n",
                    mode,
                    static_cast<unsigned long>(lit),
                    static_cast<unsigned long>(unique),
                    static_cast<unsigned long>(imuDelta),
                    static_cast<unsigned long>(paletteDelta));

        const bool expectsPalette =
            mode != static_cast<uint8_t>(MATRIX_FX::Native3DMode::CenterRipple) &&
            mode != static_cast<uint8_t>(MATRIX_FX::Native3DMode::LiquidWave);
        ok = ok && lit > 4 && unique > 3 && imuDelta > 0 && (!expectsPalette || paletteDelta > 4);

        const TemporalMetrics temporal = renderTemporalSequence(mode, temporalFrames[mode]);
        const bool sparseMode = mode == static_cast<uint8_t>(MATRIX_FX::Native3DMode::GravityParticles);
        const bool temporalOk =
            temporal.avgBrightnessDelta <= 34.0f &&
            temporal.hardPixelRate <= (sparseMode ? 0.18f : 0.10f) &&
            temporal.onOffRate <= (sparseMode ? 0.09f : 0.035f);

        std::printf("  temporal avg_delta=%5.2f max_delta=%3u hard_rate=%5.3f onoff_rate=%5.3f min_lit=%02u %s\n",
                    temporal.avgBrightnessDelta,
                    temporal.maxPixelDelta,
                    temporal.hardPixelRate,
                    temporal.onOffRate,
                    temporal.minLit,
                    temporalOk ? "OK" : "WARN");
    }

    writeSheet(argv[1], frames);
    writeTemporalSheet(argv[2], temporalFrames);
    std::printf("wrote %s (%ux%u)\n", argv[1], kImageWidth, kImageHeight);
    std::printf("wrote %s (%ux%u)\n", argv[2], kTemporalImageWidth, kTemporalImageHeight);
    return ok ? 0 : 1;
}
'''


def main() -> int:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    SOURCE_PATH.write_text(CPP_SOURCE, encoding="utf-8")

    compile_cmd = [
        "g++",
        "-std=c++17",
        "-O2",
        "-I",
        str(REPO_ROOT),
        str(SOURCE_PATH),
        "-o",
        str(BINARY_PATH),
    ]
    subprocess.run(compile_cmd, cwd=REPO_ROOT, check=True)
    subprocess.run([str(BINARY_PATH), str(PPM_PATH), str(TEMPORAL_PPM_PATH)], cwd=REPO_ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
