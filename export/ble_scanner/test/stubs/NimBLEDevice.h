#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class NimBLEUUID {
public:
    NimBLEUUID() = default;
    explicit NimBLEUUID(const char* value) : _value(value ? value : "") {}
    explicit NimBLEUUID(uint16_t value) : _value(std::to_string(value)) {}

private:
    std::string _value;
};

class NimBLEAddress {
public:
    NimBLEAddress() = default;
    explicit NimBLEAddress(const std::array<uint8_t, 6>& value) : _value(value) {}

    const uint8_t* getVal() const { return _value.data(); }

    std::string toString() const {
        char buf[18];
        std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                      _value[5], _value[4], _value[3], _value[2], _value[1], _value[0]);
        return std::string(buf);
    }

private:
    std::array<uint8_t, 6> _value{{0, 0, 0, 0, 0, 0}};
};

class NimBLEConnInfo {
public:
    NimBLEAddress getAddress() const { return _address; }
    bool isEncrypted() const { return _encrypted; }
    bool isBonded() const { return _bonded; }

    void setAddress(const NimBLEAddress& address) { _address = address; }
    void setEncrypted(bool encrypted) { _encrypted = encrypted; }
    void setBonded(bool bonded) { _bonded = bonded; }

private:
    NimBLEAddress _address;
    bool _encrypted = false;
    bool _bonded = false;
};

class NimBLEAttValue {
public:
    NimBLEAttValue() = default;
    explicit NimBLEAttValue(const std::string& value) : _value(value) {}

    const uint8_t* data() const {
        return reinterpret_cast<const uint8_t*>(_value.data());
    }

    size_t size() const { return _value.size(); }
    size_t length() const { return _value.size(); }
    const char* c_str() const { return _value.c_str(); }
    bool empty() const { return _value.empty(); }

    void assign(const uint8_t* data, size_t size) {
        _value.assign(reinterpret_cast<const char*>(data), size);
    }

    void assign(const char* value) {
        _value = value ? value : "";
    }

private:
    std::string _value;
};

class NimBLECharacteristic;
class NimBLEServer;
class NimBLEAdvertisedDevice;
class NimBLEScanResults;

class NimBLECharacteristicCallbacks {
public:
    virtual ~NimBLECharacteristicCallbacks() = default;
    virtual void onConnect(const NimBLEConnInfo& connInfo) { (void)connInfo; }
    virtual void onDisconnect(const NimBLEConnInfo& connInfo, int reason) { (void)connInfo; (void)reason; }
    virtual void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) {
        (void)characteristic;
        (void)connInfo;
    }
    virtual void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) {
        (void)characteristic;
        (void)connInfo;
    }
};

class NimBLEServerCallbacks {
public:
    virtual ~NimBLEServerCallbacks() = default;
    virtual void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) {
        (void)server;
        (void)connInfo;
    }
    virtual void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) {
        (void)server;
        (void)connInfo;
        (void)reason;
    }
    virtual uint32_t onPassKeyDisplay() { return 0; }
    virtual void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) {
        (void)connInfo;
        (void)pin;
    }
    virtual void onAuthenticationComplete(NimBLEConnInfo& connInfo) { (void)connInfo; }
};

class NimBLEDescriptor {
public:
    void setValue(const char* value) { _value.assign(value ? value : ""); }

private:
    std::string _value;
};

class NimBLECharacteristic {
public:
    void setCallbacks(NimBLECharacteristicCallbacks* callbacks) { _callbacks = callbacks; }

    NimBLEDescriptor* createDescriptor(const NimBLEUUID& uuid, uint32_t properties) {
        (void)uuid;
        (void)properties;
        _descriptors.push_back(std::make_unique<NimBLEDescriptor>());
        return _descriptors.back().get();
    }

    void setValue(const uint8_t* data, size_t size) { _value.assign(data, size); }
    void setValue(const char* value) { _value.assign(value); }
    NimBLEAttValue getValue() const { return _value; }

    bool notify() { return true; }

private:
    NimBLECharacteristicCallbacks* _callbacks = nullptr;
    NimBLEAttValue _value;
    std::vector<std::unique_ptr<NimBLEDescriptor>> _descriptors;
};

class NimBLEService {
public:
    NimBLECharacteristic* createCharacteristic(const NimBLEUUID& uuid, uint32_t properties) {
        (void)uuid;
        (void)properties;
        _characteristics.push_back(std::make_unique<NimBLECharacteristic>());
        return _characteristics.back().get();
    }

    NimBLECharacteristic* createCharacteristic(const char* uuid, uint32_t properties) {
        return createCharacteristic(NimBLEUUID(uuid), properties);
    }

    bool start() { return true; }

private:
    std::vector<std::unique_ptr<NimBLECharacteristic>> _characteristics;
};

class NimBLEServer {
public:
    NimBLEService* createService(const NimBLEUUID& uuid) {
        (void)uuid;
        _services.push_back(std::make_unique<NimBLEService>());
        return _services.back().get();
    }

    NimBLEService* createService(const char* uuid) {
        return createService(NimBLEUUID(uuid));
    }

    void setCallbacks(NimBLEServerCallbacks* callbacks) { _callbacks = callbacks; }
    size_t getConnectedCount() const { return 0; }
    NimBLEConnInfo getPeerInfo(uint16_t index) const {
        (void)index;
        return NimBLEConnInfo();
    }

private:
    NimBLEServerCallbacks* _callbacks = nullptr;
    std::vector<std::unique_ptr<NimBLEService>> _services;
};

class NimBLEAdvertising {
public:
    void clearData() {}
    void setName(const char* name) { _name = name ? name : ""; }
    bool addServiceUUID(const NimBLEUUID& uuid) {
        (void)uuid;
        return true;
    }
    bool addServiceUUID(const char* uuid) {
        return addServiceUUID(NimBLEUUID(uuid));
    }
    void setMinInterval(uint16_t interval) { (void)interval; }
    void setMaxInterval(uint16_t interval) { (void)interval; }
    bool start(uint32_t duration = 0) {
        (void)duration;
        _started = true;
        return true;
    }
    void stop() { _started = false; }

private:
    std::string _name;
    bool _started = false;
};

class NimBLEScanResults {};

class NimBLEScanCallbacks {
public:
    virtual ~NimBLEScanCallbacks() = default;
    virtual void onResult(const NimBLEAdvertisedDevice* advertisedDevice) { (void)advertisedDevice; }
    virtual void onScanComplete(const NimBLEScanResults& results) { (void)results; }
    virtual void onScanEnd(const NimBLEScanResults& results, int reason) {
        (void)results;
        (void)reason;
    }
};

class NimBLEAdvertisedDevice {
public:
    const NimBLEAddress& getAddress() const { return _address; }
    int getRSSI() const { return _rssi; }
    const std::vector<uint8_t>& getPayload() const { return _payload; }

    const uint8_t* getManufacturerDataRaw(size_t index, size_t* length) const {
        (void)index;
        if (length) {
            *length = _manufacturerData.size();
        }
        return _manufacturerData.empty() ? nullptr : _manufacturerData.data();
    }

    void setAddress(const NimBLEAddress& address) { _address = address; }
    void setRSSI(int rssi) { _rssi = rssi; }
    void setPayload(std::vector<uint8_t> payload) { _payload = std::move(payload); }
    void setManufacturerData(std::vector<uint8_t> data) { _manufacturerData = std::move(data); }

private:
    NimBLEAddress _address;
    int _rssi = 0;
    std::vector<uint8_t> _payload;
    std::vector<uint8_t> _manufacturerData;
};

class NimBLEScan {
public:
    void setInterval(uint32_t ms) { (void)ms; }
    void setWindow(uint32_t ms) { (void)ms; }
    void setScanCallbacks(NimBLEScanCallbacks* callbacks) { _callbacks = callbacks; }
    void setActiveScan(bool active) { (void)active; }
    void setMaxResults(uint32_t maxResults) { (void)maxResults; }
    bool isScanning() const { return _scanning; }
    bool start(uint32_t duration, bool isContinue) {
        (void)duration;
        (void)isContinue;
        _scanning = true;
        return true;
    }
    void stop() { _scanning = false; }
    void clearResults() {}

private:
    NimBLEScanCallbacks* _callbacks = nullptr;
    bool _scanning = false;
};

namespace NIMBLE_PROPERTY {
constexpr uint32_t READ = 1u << 0;
constexpr uint32_t READ_ENC = 1u << 1;
constexpr uint32_t READ_AUTHEN = 1u << 2;
constexpr uint32_t WRITE = 1u << 3;
constexpr uint32_t WRITE_ENC = 1u << 4;
constexpr uint32_t WRITE_AUTHEN = 1u << 5;
constexpr uint32_t WRITE_NR = 1u << 6;
constexpr uint32_t NOTIFY = 1u << 7;
}

#ifndef BLE_HS_IO_DISPLAY_ONLY
#define BLE_HS_IO_DISPLAY_ONLY 0
#endif

#ifndef ESP_PWR_LVL_P9
#define ESP_PWR_LVL_P9 9
#endif

class NimBLEDevice {
public:
    static void init(const char* deviceName) { (void)deviceName; }
    static int deinit(bool clearAll = false) {
        (void)clearAll;
        return 0;
    }
    static void setPower(int power) { (void)power; }
    static void setMTU(uint16_t mtu) { (void)mtu; }
    static void setSecurityAuth(bool bonding, bool mitm, bool sc) {
        (void)bonding;
        (void)mitm;
        (void)sc;
    }
    static void setSecurityIOCap(int ioCap) { (void)ioCap; }
    static void setSecurityPasskey(uint32_t passkey) { (void)passkey; }
    static void injectConfirmPasskey(const NimBLEConnInfo& connInfo, bool accept) {
        (void)connInfo;
        (void)accept;
    }

    static NimBLEScan* getScan() {
        static NimBLEScan scan;
        return &scan;
    }

    static NimBLEAdvertising* getAdvertising() {
        static NimBLEAdvertising advertising;
        return &advertising;
    }

    static NimBLEServer* createServer() {
        static NimBLEServer server;
        return &server;
    }

    static NimBLEServer* getServer() {
        return createServer();
    }
};
