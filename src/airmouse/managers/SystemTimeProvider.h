#pragma once

#include <Arduino.h>
#include "../interfaces/IAirMouseInterfaces.h"
#include "../../system/utils/Random.h"

namespace AIRMOUSE {

/**
 * @brief Time/random source for jiggler logic (system implementation).
 */
class SystemTimeProvider : public ITimeProvider {
public:
    uint32_t getMillis() override { return millis(); }
    int getRandomRange(int min, int max) override {
        return UTILS::RNG::rangeI32(min, max);
    }
};

} // namespace AIRMOUSE
