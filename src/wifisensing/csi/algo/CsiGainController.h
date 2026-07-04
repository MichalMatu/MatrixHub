#pragma once

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
        _calibrationCount = 0;
        esp_csi_gain_ctrl_reset_rx_gain_baseline();
        esp_csi_gain_ctrl_set_rx_force_gain(0, 0);
    }

    float update(const wifi_pkt_rx_ctrl_t* rx_ctrl) {
        uint8_t agc_gain = 0;
        int8_t fft_gain = 0;
        float compensate_gain = 1.0f;

        esp_csi_gain_ctrl_get_rx_gain(rx_ctrl, &agc_gain, &fft_gain);
        rx_gain_status_t gainStatus = esp_csi_gain_ctrl_get_gain_status();
        
        if (gainStatus != RX_GAIN_FORCE) {
            if (_calibrationCount < CALIBRATION_PACKETS) {
                esp_csi_gain_ctrl_record_rx_gain(agc_gain, fft_gain);
                _calibrationCount++;
            } else {
                uint8_t baseAgc = 0;
                int8_t baseFft = 0;
                if (esp_csi_gain_ctrl_get_rx_gain_baseline(&baseAgc, &baseFft) == ESP_OK) {
                    if (baseAgc < 30) {
                        // Strong signal, retry later
                        _calibrationCount = 0;
                    } else {
                        LOGI("Locking Gain to AGC: %d, FFT: %d", baseAgc, baseFft);
                        esp_csi_gain_ctrl_set_rx_force_gain(baseAgc, baseFft);
                    }
                }
            }
        }

        esp_csi_gain_ctrl_get_gain_compensation(&compensate_gain, agc_gain, fft_gain);
        return compensate_gain;
    }

    int calibrationCount() const { return _calibrationCount; }

    const char* stateName() const {
        switch (esp_csi_gain_ctrl_get_gain_status()) {
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
    int _calibrationCount = 0;
};

} // namespace CSI
} // namespace WIFISENSING
