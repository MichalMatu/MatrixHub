#include "IpValidator.h"
#include "../../system/logging/Logging.h"
#include <cstdio>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "IpValidator"

namespace SHELLY {

bool IpValidator::parseOctets(const char* ip, int& o1, int& o2, int& o3, int& o4) {
    if (!ip || strlen(ip) == 0 || strlen(ip) > 15) {
        return false;
    }
    
    char extra;  // To detect trailing garbage like "192.168.1.1x"
    int parsed = sscanf(ip, "%d.%d.%d.%d%c", &o1, &o2, &o3, &o4, &extra);
    
    // Should parse exactly 4 octets, no extra characters
    return (parsed == 4);
}

bool IpValidator::isValidFormat(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    // Validate each octet is in valid range
    if (o1 < 0 || o1 > 255 || 
        o2 < 0 || o2 > 255 || 
        o3 < 0 || o3 > 255 || 
        o4 < 0 || o4 > 255) {
        return false;
    }
    
    return true;
}

bool IpValidator::isLoopback(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    return (o1 == 127);
}

bool IpValidator::isPrivateRange(const char* ip) {
    int o1, o2, o3, o4;
    
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        return false;
    }
    
    // Private network ranges:
    // 10.0.0.0/8     - Class A private
    // 172.16.0.0/12  - Class B private  
    // 192.168.0.0/16 - Class C private
    
    return (o1 == 10) ||
           (o1 == 172 && o2 >= 16 && o2 <= 31) ||
           (o1 == 192 && o2 == 168);
}

bool IpValidator::isValidPrivateIp(const char* ip) {
    int o1, o2, o3, o4;
    
    // Check format
    if (!parseOctets(ip, o1, o2, o3, o4)) {
        LOGW("Invalid IP format: %s", ip ? ip : "null");
        return false;
    }
    
    // Validate octet ranges
    if (o1 < 0 || o1 > 255 || 
        o2 < 0 || o2 > 255 || 
        o3 < 0 || o3 > 255 || 
        o4 < 0 || o4 > 255) {
        LOGW("IP octet out of range: %s", ip);
        return false;
    }
    
    // Block loopback (127.x.x.x)
    if (o1 == 127) {
        LOGW("Rejected loopback IP: %s", ip);
        return false;
    }
    
    // Block broadcast and any-address
    if ((o1 == 0 && o2 == 0 && o3 == 0 && o4 == 0) ||
        (o1 == 255 && o2 == 255 && o3 == 255 && o4 == 255)) {
        LOGW("Rejected broadcast/any IP: %s", ip);
        return false;
    }
    
    // Allow only private network ranges
    bool isPrivate = 
        (o1 == 10) ||
        (o1 == 172 && o2 >= 16 && o2 <= 31) ||
        (o1 == 192 && o2 == 168);
    
    if (!isPrivate) {
        LOGW("Rejected non-private IP (potential SSRF): %s", ip);
        return false;
    }
    
    return true;
}

} // namespace SHELLY
