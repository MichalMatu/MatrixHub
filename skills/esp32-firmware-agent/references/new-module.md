# New Module Protocol

## When To Read This File

- Read this file before adding a new feature module or moving an existing feature into the preferred service and API split.

## Design Rules

- Keep concerns separate: service for business logic, API service for transport, config helpers for persistence.
- Use dependency injection. Do not introduce new feature-level singletons or `getInstance()` accessors.
- Keep headers lean with forward declarations where possible.
- Preserve user-visible behavior while moving the touched area toward the target architecture.

## Step 1: Define Persistent Data

- Create `src/system/rtc/types/Rtc<Feature>Types.h`.
- Use a packed POD struct with fixed-size buffers only. No `String`, `std::vector`, or raw pointers.
- When parsing JSON into packed members, use temporary variables instead of binding references.
- Update `src/system/rtc/RtcConfig.h` to include the new type, add it to `ConfigStore`, and increment `RTC::kSchemaVersion`.
- Update `src/system/rtc/RtcConfig.cpp` defaults.

Example:

```cpp
namespace RTC {
struct __attribute__((packed)) ThermostatData {
    bool enabled = false;
    float target_temp = 21.5f;
    char name[32] = "Living Room";
    uint8_t mode = 0;
};
}
```

## Step 2: Implement JSON Serialization

- Create `src/config/json/<Feature>ConfigJson.h` and `.cpp`.
- Keep JSON keys snake_case and aligned across backend, API, and frontend.
- Define key constants in `src/config/App.h`; do not hardcode strings repeatedly.
- Implement shared `deserialize`, `load`, and `save` helpers. Add `serialize` when GET responses or exports benefit from it.
- `deserialize` should support partial updates and be reused by file loading and API updates.
- Register load and save in `src/config/ConfigManager.cpp`.

## Step 3: Implement The Service

- Create `src/<feature>/<Feature>Service.h` and `.cpp`.
- Inject dependencies through the constructor or explicit initialization.
- Do not create feature dependencies internally with `new`.
- Do not depend on PsychicHttp or API-layer classes.
- If the feature needs a task, define stack, priority, and core in `CONFIG::TASKS`.
- If the service caches config-backed state, expose a clear reapply path such as `applySettings()`.
- Use `LOGI`, `LOGW`, and `LOGE` with a local `LOG_TAG`.

## Step 4: Implement The API Service

- Create `src/api/<feature>/<Feature>ApiService.h` and `.cpp`.
- Inherit from `BaseApiService` when possible.
- Inject `<Feature>Service*` in the constructor.
- Register routes in `begin()`.
- Use existing auth wrappers like `wrapAuth` or `wrapAdmin` instead of hand-rolled access checks.
- Reuse the JSON config deserializer for PUT or PATCH request validation.
- Keep response shapes machine-readable and stable.
- Detect no-change updates before writing.

Example no-change check:

```cpp
auto next_cfg = current_cfg;
CONFIG::JSON::deserializeThermostat(obj, next_cfg);
bool changed = memcmp(&current_cfg, &next_cfg, sizeof(RTC::ThermostatData)) != 0;
```

## Step 5: Register In ServiceRegistry

- Add owned members or getters in `src/system/services/ServiceRegistry.h` as needed.
- If the feature exposes HTTP endpoints, add the API `PsramStaticService` slot in `src/system/services/ServiceRegistryApi.h`.
- Wire the feature through the appropriate initializer under `src/system/init/services/`.
- Update `src/system/services/ServiceRegistry*.cpp` only where ownership or sequencing belongs.
- Follow existing cleanup patterns: RAII for `std::unique_ptr` and `.destroy()` or `destroyAll()` for `PsramStaticService`.

## Step 6: Memory Rules

- Use `SYSTEM::SpiRamJsonDocument` for larger API documents.
- Use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` for large user-space buffers.
- Prefer `xTaskCreateStatic` with `EXT_RAM_BSS_ATTR` task stacks when safe.
- Never place DMA or ISR-accessed buffers in PSRAM.

## Step 7: Frontend Sync

- Put transport logic in `interface/src/lib/services/api/...`.
- Put shared DTO and type definitions in `interface/src/lib/types/...`.
- Put route-level orchestration in `interface/src/routes/.../use*.svelte.ts`.
- For settings forms, prefer `interface/src/lib/utils/api/useSettings.svelte.ts`.
- Keep field names aligned with backend JSON unless there is a strong UI-only reason to diverge.

## Step 8: Verify

- Run `pio test -e native` if host-side tests were added.
- Run relevant frontend checks such as `npm run check`, `npm run test:run`, and `npm run lint`.
- Upload and validate on hardware when the change affects runtime behavior, peripherals, networking, timing, or memory.
- Verify config persistence after restart when settings were changed.

## Maintenance Rule

- Only document stable repo-wide conventions here. Do not leave placeholders, speculative notes, or wish-list architecture.
