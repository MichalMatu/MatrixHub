#include "TelegramUpdatesLiteParser.h"

#include "TelegramUpdatesLite.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/utils/json/JsonStreamReader.h"
#include "TelegramJsonParsers.h"

#include <NetworkClient.h>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "TgLiteJson"

namespace TELEGRAM {

bool TelegramUpdatesLiteParser::parse(NetworkClient& stream,
                                     UpdateLiteCallback cb,
                                     void* user,
                                     bool& outTelegramOk) {
    outTelegramOk = false;
    Utils::JsonStreamReader r(stream);

    int c = r.readSkipWs();
    if (c != '{') {
        LOGW("Root is not object");
        return false;
    }

    bool okFound = false;

    while (true) {
        int p;
        if (!r.skipWsPeek(p)) return false;
        if (p == '}') {
            (void)r.read();
            break;
        }

        char key[16] = {0};
        if (!r.readString(key, sizeof(key))) return false;
        if (!r.readExact(':')) return false;

        if (strcmp(key, "ok") == 0) {
            bool ok = false;
            if (!r.readBool(ok)) return false;
            outTelegramOk = ok;
            okFound = true;
        } else if (strcmp(key, "result") == 0) {
            // result: array of updates
            c = r.readSkipWs();
            // Expect array
            if (c != '[') return false;

            int pp;
            if (!r.skipWsPeek(pp)) return false;
            if (pp == ']') {
                (void)r.read(); // Empty array
            } else {
                while (true) {
                    UpdateLite upd;
                    if (!TelegramJsonParsers::parseUpdateObject(r, upd)) return false;

                    if (cb && upd.updateId != 0) {
                        if (!cb(upd, user)) {
                            // consumer requested stop; skip rest of array to be polite
                            int delim = r.readSkipWs();
                            if (delim == ',') {
                                while (true) {
                                    if (!r.skipValue()) return false;
                                    delim = r.readSkipWs();
                                    if (delim < 0) return false;
                                    if (delim == ',') continue;
                                    if (delim == ']') break;
                                    return false;
                                }
                            }
                            break;
                        }
                    }

                    int delim = r.readSkipWs();
                    if (delim < 0) return false;
                    if (delim == ',') continue;
                    if (delim == ']') break;
                    return false;
                }
            }
        } else {
            if (!r.skipValue()) return false;
        }

        c = r.readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == '}') break;
        return false;
    }

    if (!okFound) {
        LOGW("Missing ok field");
        return false;
    }

    return true;
}

}  // namespace TELEGRAM
