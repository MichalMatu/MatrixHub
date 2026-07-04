#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>

class NimBLEData {
public:
    NimBLEData(const std::vector<uint8_t>& data) : _data(data) {}
    const uint8_t* data() const { return _data.data(); }
    size_t size() const { return _data.size(); }
private:
    std::vector<uint8_t> _data;
};

class NimBLEAddress {
public:
    NimBLEAddress(const std::string& addr) : _addr(addr) {
        // Dummy conversion of string MAC to bytes for getVal()
        // AA:BB:CC:DD:EE:FF
        memset(_val, 0, 6);
        if (addr.length() == 17) {
            // Very primitive parser for test
            _val[5] = 0xAA; _val[4] = 0xBB; _val[3] = 0xCC; 
            _val[2] = 0xDD; _val[1] = 0xEE; _val[0] = 0xFF;
        }
    }
    std::string toString() const { return _addr; }
    const uint8_t* getVal() const { return _val; }
private:
    std::string _addr;
    uint8_t _val[6];
};

class NimBLEAdvertisedDevice {
public:
    NimBLEAdvertisedDevice() : _payload({}), _addr("AA:BB:CC:DD:EE:FF") {}
    
    NimBLEData getPayload() const { return NimBLEData(_payload); }
    void setPayload(const std::vector<uint8_t>& p) { _payload = p; }
    
    int getRSSI() const { return -70; }
    NimBLEAddress getAddress() const { return _addr; }
    void setAddress(const std::string& a) { _addr = NimBLEAddress(a); }
    
    bool haveName() const { return true; }
    std::string getName() const { return "TP357"; }
    bool haveServiceData() const { return true; }
    bool haveManufacturerData() const { return true; }

    const uint8_t* getManufacturerDataRaw(size_t index, size_t* outLen) const {
        if (_payload.size() > 2) {
            *outLen = _payload.size() - 2;
            return _payload.data() + 2;
        }
        *outLen = 0;
        return nullptr;
    }

private:
    std::vector<uint8_t> _payload;
    NimBLEAddress _addr;
};

class NimBLEScanResults {
public:
    size_t getCount() const { return 0; }
};

class NimBLEScanCallbacks {
public:
    virtual ~NimBLEScanCallbacks() {}
    virtual void onResult(const NimBLEAdvertisedDevice* device) {}
    virtual void onScanComplete(const NimBLEScanResults& results) {}
    virtual void onScanEnd(const NimBLEScanResults& results, int reason) {}
};

class NimBLEDevice {
public:
    static void init(const std::string& name) {}
};
