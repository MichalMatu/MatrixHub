#pragma once

#include <cstdint>

class NetworkClient;

namespace TELEGRAM {

struct UpdateLite;
using UpdateLiteCallback = bool (*)(const UpdateLite& update, void* user);

class TelegramUpdatesLiteParser {
public:
    // Parses Telegram getUpdates JSON response from stream and emits updates via callback.
    // Returns true if parsed successfully (including ok=true), false on parse errors.
    // On ok=false, returns true but emits no updates and sets outTelegramOk=false.
    static bool parse(NetworkClient& stream,
                      UpdateLiteCallback cb,
                      void* user,
                      bool& outTelegramOk);
};

}  // namespace TELEGRAM
