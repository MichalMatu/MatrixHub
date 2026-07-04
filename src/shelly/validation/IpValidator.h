#pragma once

namespace SHELLY {

/**
 * IP Address Validator with SSRF Protection.
 * 
 * Validates IP addresses and ensures only private network ranges
 * are allowed to prevent Server-Side Request Forgery attacks.
 */
class IpValidator {
public:
    /**
     * Validate IP address format and check for SSRF vulnerabilities.
     * 
     * Allows only private network ranges:
     * - 10.0.0.0/8     (10.0.0.0 - 10.255.255.255)
     * - 172.16.0.0/12  (172.16.0.0 - 172.31.255.255)
     * - 192.168.0.0/16 (192.168.0.0 - 192.168.255.255)
     * 
     * Blocks:
     * - Loopback (127.x.x.x)
     * - Broadcast (0.0.0.0, 255.255.255.255)
     * - Public IP addresses
     * - Invalid formats
     * 
     * @param ip IP address string to validate
     * @return true if valid private IP, false otherwise
     */
    static bool isValidPrivateIp(const char* ip);

    /**
     * Check if IP format is valid (without private range check)
     * 
     * @param ip IP address string to validate
     * @return true if valid IPv4 format, false otherwise
     */
    static bool isValidFormat(const char* ip);

    /**
     * Check if IP is in a private network range
     * 
     * @param ip IP address string (must be valid format)
     * @return true if in private range, false otherwise
     */
    static bool isPrivateRange(const char* ip);

    /**
     * Check if IP is a loopback address (127.x.x.x)
     * 
     * @param ip IP address string (must be valid format)
     * @return true if loopback, false otherwise
     */
    static bool isLoopback(const char* ip);

private:
    /**
     * Parse IP string into octets
     * 
     * @param ip IP address string
     * @param o1, o2, o3, o4 Output octets
     * @return true if parsing successful, false otherwise
     */
    static bool parseOctets(const char* ip, int& o1, int& o2, int& o3, int& o4);
};

} // namespace SHELLY
