#pragma once

#include "TelegramUpdatesLite.h"

namespace Utils {
class JsonStreamReader;
}

namespace TELEGRAM {

/**
 * @brief Helper functions for parsing Telegram API JSON objects.
 * 
 * These parsers use JsonStreamReader for zero-allocation streaming JSON parsing.
 * All functions return true on success, false on parse error.
 */
class TelegramJsonParsers {
public:
    /**
     * @brief Parse Telegram "chat" object.
     * 
     * Extracts chat ID into UpdateLite.chatId (formatted as string to support large negative group IDs).
     * 
     * @param r JsonStreamReader positioned at start of chat object
     * @param out UpdateLite to populate with chat data
     * @return true if parsed successfully, false on error
     */
    static bool parseChatObject(Utils::JsonStreamReader& r, UpdateLite& out);
    
    /**
     * @brief Parse Telegram "from" object.
     * 
     * Extracts username into UpdateLite.fromUsername.
     * 
     * @param r JsonStreamReader positioned at start of from object
     * @param out UpdateLite to populate with from data
     * @return true if parsed successfully, false on error
     */
    static bool parseFromObject(Utils::JsonStreamReader& r, UpdateLite& out);
    
    /**
     * @brief Parse Telegram "message" object (or edited_message, channel_post, etc).
     * 
     * Extracts text, chat, from, date fields. Sets hasMessage flag.
     * 
     * @param r JsonStreamReader positioned at start of message object
     * @param out UpdateLite to populate with message data
     * @return true if parsed successfully, false on error
     */
    static bool parseMessageObject(Utils::JsonStreamReader& r, UpdateLite& out);
    
    /**
     * @brief Parse Telegram "update" object.
     * 
     * Extracts update_id and delegates to parseMessageObject for message types.
     * 
     * @param r JsonStreamReader positioned at start of update object
     * @param out UpdateLite to populate (reset to empty first)
     * @return true if parsed successfully, false on error
     */
    static bool parseUpdateObject(Utils::JsonStreamReader& r, UpdateLite& out);
};

} // namespace TELEGRAM
