#include "MemoryConfig.h"
#include <Arduino.h>
#include "../../logging/Logging.h"
#include "../../../config/System.h" // Added for MEMORY config

#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <mbedtls/platform.h>
#include <cstdlib>

// Custom allocator for mbedtls to force PSRAM
static void* psram_calloc(size_t n, size_t size) {
    return heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

static void psram_free(void* ptr) {
    heap_caps_free(ptr);
}

// Priority 200 runs AFTER Arduino's PSRAM init (priority ~100)
// This ensures PSRAM is available when we configure the threshold
void __attribute__((constructor(200))) enablePsramLateInit() {
    if (!esp_psram_is_initialized()) {
        ESP_LOGE("MemCfg", "PSRAM is required but not initialized");
        std::abort();
    }

    // [FIX] Ustawienie progu z pliku konfiguracyjnego (System.h).
    // 0 = Force PSRAM, >0 = Mixed Mode
    heap_caps_malloc_extmem_enable(MEMORY::PSRAM_ALLOC_THRESHOLD);
    
    // Force mbedTLS to use PSRAM
    if (MEMORY::USE_PSRAM_FOR_MBEDTLS) {
        mbedtls_platform_set_calloc_free(psram_calloc, psram_free);
    }
}

namespace SYSTEM {

#undef LOG_TAG
#define LOG_TAG "MemCfg"

void MemoryConfig::applyAggressivePsramStrategy() {
    if (!esp_psram_is_initialized()) {
        LOGE("PSRAM is required but not initialized");
        std::abort();
    }

    // Re-apply to be absolutely sure (idempotent)
    heap_caps_malloc_extmem_enable(MEMORY::PSRAM_ALLOC_THRESHOLD);
    
    if (MEMORY::USE_PSRAM_FOR_MBEDTLS) {
        mbedtls_platform_set_calloc_free(psram_calloc, psram_free);
    }
}

void MemoryConfig::printStats() {
    if (esp_psram_is_initialized()) {
        uint32_t psram_total = esp_psram_get_size();
        uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        uint32_t dram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        
        LOGI("[Mem] PSRAM: %uMB (%uKB free), DRAM: %uKB free | Strategy: Mixed > %uB", 
             psram_total / (1024*1024), psram_free / 1024, dram_free / 1024, MEMORY::PSRAM_ALLOC_THRESHOLD);
    } else {
        LOGE("[Mem] PSRAM missing or init failed (unsupported configuration)");
    }
}

} // namespace SYSTEM
