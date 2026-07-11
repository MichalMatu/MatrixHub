#pragma once

#ifndef MATRIXHUB_GIT_SHA
#define MATRIXHUB_GIT_SHA "unknown"
#endif

#ifndef MATRIXHUB_GIT_DIRTY
#define MATRIXHUB_GIT_DIRTY 1
#endif

namespace SYSTEM {
namespace BUILD {

inline constexpr const char* FIRMWARE_COMMIT = MATRIXHUB_GIT_SHA;
inline constexpr bool FIRMWARE_DIRTY = MATRIXHUB_GIT_DIRTY != 0;

} // namespace BUILD
} // namespace SYSTEM
