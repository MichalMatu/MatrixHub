#pragma once

namespace SYSTEM {

class MemoryConfig {
public:
    /**
     * @brief Apply aggressive PSRAM allocation strategy.
     * 
     * Forces all allocations (threshold 0) to PSRAM and registers
     * custom mbedTLS allocators to use PSRAM.
     * Safe to call multiple times (idempotent).
     */
    static void applyAggressivePsramStrategy();

    /**
     * @brief Print PSRAM memory statistics to log.
     */
    static void printStats();
};

} // namespace SYSTEM
