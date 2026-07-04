# New Module Workflow

Use this checklist when adding a feature module. Keep the feature aligned with
the current `ServiceRegistry` + initializer architecture.

## 1. Define Persistent State

If the module needs retained configuration:

1. Add a POD struct in `src/system/rtc/types/Rtc<Feature>Types.h`.
2. Use fixed-size `char[]` buffers, enums, integers, and POD structs only.
3. Add the struct to `RTC::ConfigStore` in `src/system/rtc/RtcConfig.h`.
4. Increment `RTC::kSchemaVersion` when the RTC layout changes.
5. Add defaults in the RTC default/config store implementation.

Packed struct rule:

- Do not bind packed members by reference.
- Validate into a temporary value, then assign to the packed field.

## 2. Add JSON Serialization

Runtime JSON config lives under `src/config/json/`.

1. Define or reuse key constants in the appropriate config header.
2. Use `snake_case` JSON keys for new fields.
3. Add `<Feature>ConfigJson.h/.cpp`.
4. Keep validation shared between load/save and API update paths.
5. Register the loader/saver in `src/config/ConfigManager.cpp`.

Preferred update pattern:

```cpp
auto current = RTC::getConfigSafeCopy().feature;
auto next = current;
CONFIG::JSON::updateFeatureFromJson(obj, next);
const bool changed = memcmp(&current, &next, sizeof(next)) != 0;
```

## 3. Add Business Logic

Create the domain service under `src/<feature>/`.

Rules:

- Keep business logic out of API classes.
- Pass dependencies through constructor or `begin()`.
- Do not add new global `extern` dependencies.
- Do not add `Service::instance()` singletons for new modules.
- Use `LOGI`, `LOGW`, and `LOGE`; avoid `Serial.print()`.
- If the module starts a task, define stack, priority, and core affinity in
  `CONFIG::TASKS` in `src/config/TaskConfig.h`.

## 4. Add API Surface

Create API code under `src/api/<feature>/`.

Rules:

- Keep API services transport-focused: parse, validate, call service, serialize.
- Use existing auth wrappers and response helpers.
- Use shared ConfigJson validation for settings updates.
- Return explicit no-change/success/error responses, following nearby modules.
- Stream large JSON responses instead of building large temporary `String`
  buffers.

## 5. Register In Boot Wiring

Current composition path:

- `src/system/services/ServiceRegistry.h`
- `src/system/services/ServiceRegistryInitialization.cpp`
- `src/system/services/ServiceRegistryInitializationRuntime.*`
- `src/system/init/services/*Initializer.*`
- `src/system/services/ServiceRegistryApi.h`
- `src/system/init/services/ApiServicesInitializer.*`

Where to wire depends on ownership:

- Long-lived domain services belong in `ServiceRegistry`.
- Small construction/begin helpers can live in
  `ServiceRegistryInitializationRuntime.*` when they make host tests cleaner.
- API services belong in the opaque `ApiServices` container and are initialized
  through `ApiServicesInitializer`.
- Runtime tasks usually start from `RuntimeTasksInitializer`.

Document shutdown or restart implications when the feature owns tasks, sockets,
file handles, BLE/Wi-Fi state, or matrix display state.

## 6. Frontend

If the feature has UI:

1. Add or update TypeScript types using the same `snake_case` keys as firmware.
2. Add API client methods under `interface/src/lib/services/api/`.
3. Add routes/components under `interface/src/routes/` or
   `interface/src/lib/features/` following nearby feature structure.
4. Add Vitest coverage for parsers, stores, and save/error flows.

## 7. Tests And Docs

Recommended checks:

```bash
pio test -e native
./scripts/build-fast.sh
cd interface && npm run quality:frontend:fast
python scripts/contract/verify_api_contract.py
```

Update these when relevant:

- `docs/engineering/api-contract.json`
- `docs/engineering/api-contract.md`
- `docs/engineering/architecture/architecture.md`
- `docs/user-guide/README.md`
