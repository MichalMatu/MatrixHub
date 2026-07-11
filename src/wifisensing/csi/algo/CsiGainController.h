#pragma once

#include <atomic>

#include "esp_wifi_types.h"
#include "../vendor/esp_csi_gain_ctrl.h"
#include "../../../system/logging/Logging.h"

namespace WIFISENSING {
namespace CSI {

class CsiGainController {
public:
    static constexpr int CALIBRATION_PACKETS = 30;

    CsiGainController() = default;

    void reset() {
        _calibrationCount.store(0, std::memory_order_relaxed);
        esp_csi_gain_ctrl_reset_rx_gain_baseline();
        esp_csi_gain_ctrl_set_rx_force_gain(0, 0);
        _state.store(RX_GAIN_COLLECT, std::memory_order_release);
    }

    float update(const wifi_pkt_rx_ctrl_t* rx_ctrl) {
        uint8_t agc_gain = 0;
        int8_t fft_gain = 0;
        float compensate_gain = 1.0f;

        esp_csi_gain_ctrl_get_rx_gain(rx_ctrl, &agc_gain, &fft_gain);
        rx_gain_status_t gainStatus = esp_csi_gain_ctrl_get_gain_status();
        _state.store(gainStatus, std::memory_order_release);
        
        if (gainStatus != RX_GAIN_FORCE) {
            const int calibrationCount =
                _calibrationCount.load(std::memory_order_relaxed);
            if (calibrationCount < CALIBRATION_PACKETS) {
                esp_csi_gain_ctrl_record_rx_gain(agc_gain, fft_gain);
                _calibrationCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                uint8_t baseAgc = 0;
                int8_t baseFft = 0;
                if (esp_csi_gain_ctrl_get_rx_gain_baseline(&baseAgc, &baseFft) == ESP_OK) {
                    if (baseAgc < 30) {
                        // Strong signal, retry later
                        _calibrationCount.store(0, std::memory_order_relaxed);
                    } else {
                        LOGI("Locking Gain to AGC: %d, FFT: %d", baseAgc, baseFft);
                        if (esp_csi_gain_ctrl_set_rx_force_gain(baseAgc, baseFft) == ESP_OK) {
                            _state.store(RX_GAIN_FORCE, std::memory_order_release);
                        }
                    }
                }
            }
        }

        esp_csi_gain_ctrl_get_gain_compensation(&compensate_gain, agc_gain, fft_gain);
        return compensate_gain;
    }

    int calibrationCount() const {
        return _calibrationCount.load(std::memory_order_relaxed);
    }
    bool isForced() const {
        return _state.load(std::memory_order_acquire) == RX_GAIN_FORCE;
    }

    const char* stateName() const {
        switch (_state.load(std::memory_order_acquire)) {
            case RX_GAIN_COLLECT:
                return "collecting";
            case RX_GAIN_READY:
                return "ready";
            case RX_GAIN_FORCE:
                return "forced";
            default:
                return "unknown";
        }
    }

private:
    std::atomic<int> _calibrationCount{0};
    std::atomic<rx_gain_status_t> _state{RX_GAIN_COLLECT};
};

} // namespace CSI
} // namespace WIFISENSING
