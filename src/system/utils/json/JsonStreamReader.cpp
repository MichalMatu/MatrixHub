#include "JsonStreamReader.h"
#include <cstring>

namespace Utils {


static constexpr int MAX_JSON_DEPTH = 20;

JsonStreamReader::JsonStreamReader(NetworkClient& s) : _stream(s) {}

int JsonStreamReader::peek() { return _stream.peek(); }

int JsonStreamReader::read() { return _stream.read(); }

int JsonStreamReader::readSkipWs() {
    int c;
    do {
        c = read();
    } while (c >= 0 && (c == ' ' || c == '\n' || c == '\r' || c == '\t'));
    return c;
}

bool JsonStreamReader::readExact(char expected) {
    int c = readSkipWs();
    return c == expected;
}

bool JsonStreamReader::skipWsPeek(int& out) {
    int c;
    do {
        c = peek();
        if (c < 0) return false;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
            (void)read();
            continue;
        }
        break;
    } while (true);
    out = c;
    return true;
}

bool JsonStreamReader::isHex(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool JsonStreamReader::readLiteral(const char* lit) {
    for (size_t i = 0; lit[i]; i++) {
        int c = read();
        if (c != (unsigned char)lit[i]) return false;
    }
    return true;
}

bool JsonStreamReader::readBool(bool& out) {
    int c = readSkipWs();
    if (c == 't') {
        return readLiteral("rue") ? (out = true, true) : false;
    }
    if (c == 'f') {
        return readLiteral("alse") ? (out = false, true) : false;
    }
    return false;
}

bool JsonStreamReader::readInt64(int64_t& out) {
    int c = readSkipWs();
    if (c < 0) return false;

    bool neg = false;
    if (c == '-') {
        neg = true;
        c = read();
    }
    if (c < '0' || c > '9') return false;

    int64_t val = 0;
    do {
        val = val * 10 + (c - '0');
        c = peek();
        if (c < 0) break;
        if (c >= '0' && c <= '9') {
            (void)read();
            continue;
        }
        break;
    } while (true);

    out = neg ? -val : val;
    return true;
}

bool JsonStreamReader::readString(char* out, size_t outSize) {
    if (!out || outSize == 0) return false;
    out[0] = '\0';

    int c = readSkipWs();
    if (c != '"') return false;

    size_t pos = 0;
    while (true) {
        c = read();
        if (c < 0) return false;
        if (c == '"') {
            break;
        }
        if (c == '\\') {
            int esc = read();
            if (esc < 0) return false;
            switch (esc) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u': {
                    // Skip \uXXXX (we don't decode fully in this lite version, but skipped ok)
                    for (int i = 0; i < 4; i++) {
                        int h = read();
                        if (!isHex(h)) return false;
                    }
                    c = '?';
                    break;
                }
                default:
                    return false;
            }
        }

        if (pos + 1 < outSize) {
            out[pos++] = (char)c;
            out[pos] = '\0';
        }
        // else truncate silently
    }

    return true;
}

bool JsonStreamReader::skipString() {
    int c = readSkipWs();
    if (c != '"') return false;
    while (true) {
        c = read();
        if (c < 0) return false;
        if (c == '"') return true;
        if (c == '\\') {
            int esc = read();
            if (esc < 0) return false;
            if (esc == 'u') {
                for (int i = 0; i < 4; i++) {
                    int h = read();
                    if (!isHex(h)) return false;
                }
            }
        }
    }
}

bool JsonStreamReader::skipNumber() {
    int c = readSkipWs();
    if (c < 0) return false;
    if (!(c == '-' || (c >= '0' && c <= '9'))) return false;
    // consume rest of number (int or float)
    while (true) {
        int p = peek();
        if (p < 0) break;
        if ((p >= '0' && p <= '9') || p == '.' || p == 'e' || p == 'E' || p == '+' || p == '-') {
            (void)read();
            continue;
        }
        break;
    }
    return true;
}

bool JsonStreamReader::skipArray(int depth) {
    int c = readSkipWs();
    if (c != '[') return false;
    int p;
    if (!skipWsPeek(p)) return false;
    if (p == ']') {
        (void)read();
        return true;
    }
    while (true) {
        if (!skipValue(depth + 1)) return false;
        c = readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == ']') return true;
        return false;
    }
}

bool JsonStreamReader::skipObject(int depth) {
    int c = readSkipWs();
    if (c != '{') return false;
    int p;
    if (!skipWsPeek(p)) return false;
    if (p == '}') {
        (void)read();
        return true;
    }
    while (true) {
        if (!skipString()) return false;
        if (!readExact(':')) return false;
        if (!skipValue(depth + 1)) return false;
        c = readSkipWs();
        if (c < 0) return false;
        if (c == ',') continue;
        if (c == '}') return true;
        return false;
    }
}

bool JsonStreamReader::skipValue(int depth) {
    if (depth > MAX_JSON_DEPTH) return false;
    int p;
    if (!skipWsPeek(p)) return false;
    if (p == '{') return skipObject(depth);
    if (p == '[') return skipArray(depth);
    if (p == '"') return skipString();
    if (p == 't') { (void)read(); return readLiteral("rue"); }
    if (p == 'f') { (void)read(); return readLiteral("alse"); }
    if (p == 'n') { (void)read(); return readLiteral("ull"); }
    if (p == '-' || (p >= '0' && p <= '9')) return skipNumber();
    return false;
}

} // namespace Utils
