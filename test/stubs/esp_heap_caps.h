#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef MALLOC_CAP_INTERNAL
#define MALLOC_CAP_INTERNAL 0x01
#endif
#ifndef MALLOC_CAP_8BIT
#define MALLOC_CAP_8BIT 0x02
#endif
#ifndef MALLOC_CAP_SPIRAM
#define MALLOC_CAP_SPIRAM 0x04
#endif

#ifdef __cplusplus
extern "C" {
#endif
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
size_t heap_caps_get_total_size(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
uint32_t esp_get_minimum_free_heap_size();
uint32_t esp_get_free_heap_size();
inline void* heap_caps_malloc(size_t size, uint32_t caps) { return malloc(size); }
inline void heap_caps_free(void* ptr) { free(ptr); }
inline void* heap_caps_realloc(void* ptr, size_t size, uint32_t caps) { return realloc(ptr, size); }
#ifdef __cplusplus
}
#endif
