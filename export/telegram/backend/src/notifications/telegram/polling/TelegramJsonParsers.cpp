#include "TelegramJsonParsers.h"
#include "../../../system/utils/json/JsonStreamReader.h"
#include <cstring>

namespace TELEGRAM {

bool TelegramJsonParsers::parseChatObject(Utils::JsonStreamReader& r, UpdateLite& out) {
    int c = r.readSkipWs();
    if (c != '{') return false;

    while (true) {
        int p;
        if (!r.skipWsPeek(p)) return false;
        if (p == '}') {
            (void)r.read();
            return true;
        }

        char key[20] = {0};
        if (!r.readString(key, sizeof(key))) return false;
        if (!r.readExact(':')) return false;

        if (strcmp(key, "id") == 0) {
            int64_t chatIdNum = 0;
            if (!r.readInt64(chatIdNum)) return false;
            
            // Format ID into fixed size buffer (supports large negative group IDs)
            snprintf(out.chatId, sizeof(out.chatId), "%lld", (long long)chatIdNum);
        } else {
            if (!r.skipValue()) return false;
        }

        c = r.readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == '}') return true;
        return false;
    }
}

bool TelegramJsonParsers::parseFromObject(Utils::JsonStreamReader& r, UpdateLite& out) {
    int c = r.readSkipWs();
    if (c != '{') return false;

    while (true) {
        int p;
        if (!r.skipWsPeek(p)) return false;
        if (p == '}') {
            (void)r.read();
            return true;
        }

        char key[20] = {0};
        if (!r.readString(key, sizeof(key))) return false;
        if (!r.readExact(':')) return false;

        if (strcmp(key, "username") == 0) {
            if (!r.readString(out.fromUsername, sizeof(out.fromUsername))) return false;
        } else {
            if (!r.skipValue()) return false;
        }

        c = r.readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == '}') return true;
        return false;
    }
}

bool TelegramJsonParsers::parseMessageObject(Utils::JsonStreamReader& r, UpdateLite& out) {
    int c = r.readSkipWs();
    if (c != '{') return false;

    out.hasMessage = true;

    while (true) {
        int p;
        if (!r.skipWsPeek(p)) return false;
        if (p == '}') {
            (void)r.read();
            return true;
        }

        char key[24] = {0};
        if (!r.readString(key, sizeof(key))) return false;
        if (!r.readExact(':')) return false;

        if (strcmp(key, "chat") == 0) {
            if (!parseChatObject(r, out)) return false;
        } else if (strcmp(key, "text") == 0) {
            if (!r.readString(out.text, sizeof(out.text))) return false;
        } else if (strcmp(key, "from") == 0) {
            if (!parseFromObject(r, out)) return false;
        } else if (strcmp(key, "date") == 0) {
            int64_t dateVal = 0;
            if (!r.readInt64(dateVal)) return false;
            out.date = dateVal;
        } else {
            if (!r.skipValue()) return false;
        }

        c = r.readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == '}') return true;
        return false;
    }
}

bool TelegramJsonParsers::parseUpdateObject(Utils::JsonStreamReader& r, UpdateLite& out) {
    int c = r.readSkipWs();
    if (c != '{') return false;

    out = UpdateLite{};

    while (true) {
        int p;
        if (!r.skipWsPeek(p)) return false;
        if (p == '}') {
            (void)r.read();
            return true;
        }

        char key[32] = {0};
        if (!r.readString(key, sizeof(key))) return false;
        if (!r.readExact(':')) return false;

        if (strcmp(key, "update_id") == 0) {
            int64_t id = 0;
            if (!r.readInt64(id)) return false;
            out.updateId = id;
        } else if (strcmp(key, "message") == 0 || strcmp(key, "edited_message") == 0 ||
                   strcmp(key, "channel_post") == 0 || strcmp(key, "edited_channel_post") == 0) {
            if (!parseMessageObject(r, out)) return false;
        } else {
            if (!r.skipValue()) return false;
        }

        c = r.readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == '}') return true;
        return false;
    }
}

} // namespace TELEGRAM
