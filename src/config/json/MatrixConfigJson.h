/**
 * @file MatrixConfigJson.h
 * @brief JSON serialization for Matrix LED configuration
 */

#pragma once

#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcMatrixTypes.h"

namespace CONFIG {
namespace JSON {

    void deserializeMatrix(JsonObject& obj, RTC::MatrixData& data);
    void serializeMatrix(const RTC::MatrixData& data, JsonObject& obj);
    void loadMatrixPsram(JsonObject& obj);
    bool matrixCustomIconsChanged(JsonObject& obj);
    void loadMatrix(JsonObject& obj);
    void saveMatrix(JsonObject& obj);

} // namespace JSON
} // namespace CONFIG
