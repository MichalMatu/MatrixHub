#!/usr/bin/env python3
"""
BLE Binary Protocol Test Client for MatrixHub device

Usage:
    python ble_test_client.py scan              # Scan for devices
    python ble_test_client.py read              # Read all characteristics
    python ble_test_client.py wifi              # Read WiFi status
    python ble_test_client.py sensors           # Read sensor data (with notify)
    python ble_test_client.py set-wifi SSID PASS # Set WiFi credentials

Requirements:
    pip install bleak

macOS note: May need to grant Bluetooth permissions to Terminal/IDE
"""

import asyncio
import struct
import sys
from datetime import timedelta

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("Install bleak: pip install bleak")
    sys.exit(1)

# Device config
DEVICE_NAME = "MatrixHub"
DEVICE_MAC = "40:4C:CA:5D:0A:02"  # Default MAC (can override via env or arg)

# UUIDs (full format)
# UUIDs (full format)
# Base: 12345678-1234-5678-1234-56789abcXXXX
UUID_BASE = "12345678-1234-5678-1234-56789abc"

# Services
SVC_DEVICE_INFO = "0000180a-0000-1000-8000-00805f9b34fb"  # Standard BLE Device Info
SVC_MATRIXHUB = f"{UUID_BASE}2000"
SVC_SYSTEM_STATUS = f"{UUID_BASE}3000"
SVC_WIFI_CONFIG = f"{UUID_BASE}4000"
SVC_THERMOMETERS = f"{UUID_BASE}5000"
SVC_RUNTIME_STATS = f"{UUID_BASE}6000"
SVC_SHELLY = f"{UUID_BASE}7000"
SVC_ALARM = f"{UUID_BASE}8000"
SVC_WIFI_SENSING = f"{UUID_BASE}9000"

# Characteristics
# MatrixHub
CHAR_SENSOR_DATA = f"{UUID_BASE}2001"      # READ, NOTIFY

# System Status
CHAR_SYSTEM_STATUS = f"{UUID_BASE}3001"    # READ, NOTIFY

# WiFi Config
CHAR_WIFI_STATUS = f"{UUID_BASE}4001"      # READ
CHAR_WIFI_CONFIG = f"{UUID_BASE}4002"      # WRITE

# BLE Thermometers
CHAR_THERMOMETERS = f"{UUID_BASE}5001"     # READ, NOTIFY

# Runtime Stats
CHAR_RUNTIME_STATS = f"{UUID_BASE}6001"    # READ, NOTIFY

# Shelly
CHAR_SHELLY_STATUS = f"{UUID_BASE}7001"    # READ, NOTIFY

# Alarm
CHAR_ALARM_STATUS = f"{UUID_BASE}8001"     # READ, NOTIFY

# WiFi Sensing
CHAR_WIFI_SENSING = f"{UUID_BASE}9001"     # READ, NOTIFY

# Standard Device Info characteristics
CHAR_MANUFACTURER = "00002a29-0000-1000-8000-00805f9b34fb"
CHAR_MODEL = "00002a24-0000-1000-8000-00805f9b34fb"
CHAR_FIRMWARE = "00002a26-0000-1000-8000-00805f9b34fb"


def decode_sensor_data(data: bytes) -> dict:
    """Decode SensorDataPacket (16 bytes)"""
    if len(data) < 16:
        return {"error": f"Too short: {len(data)} bytes"}
    
    version, flags, co2, temp_x10, humid_x10, timestamp = struct.unpack("<BBHhhI", data[:12])
    
    return {
        "version": version,
        "flags": {
            "valid": bool(flags & 0x01),
            "stale": bool(flags & 0x02),
            "error": bool(flags & 0x04),
            "raw": hex(flags)
        },
        "co2_ppm": co2,
        "temperature_c": temp_x10 / 10.0,
        "humidity_pct": humid_x10 / 10.0,
        "timestamp_sec": timestamp,
        "uptime": str(timedelta(seconds=timestamp))
    }


def decode_system_status(data: bytes) -> dict:
    """Decode SystemStatusPacket (20 bytes)"""
    if len(data) < 20:
        return {"error": f"Too short: {len(data)} bytes"}
    
    
    # V1 (20 bytes) or V2 (32 bytes)
    if len(data) >= 32:
        # struct format: ver(B), wifi(B), rssi(b), res(B), uptime(I), free(I), min(I), ip(4s), peak(I), lowest(I), trend(i)
        version, wifi_status, rssi, _, uptime, free_heap, min_heap, ip_bytes, peak_free, lowest_free, heap_trend = struct.unpack("<BBbBIII4sIIi", data[:32])
        ip = ".".join(map(str, ip_bytes))
    else:
        version, wifi_status, rssi, _, uptime, free_heap, min_heap = struct.unpack("<BBbBIII", data[:16])
        peak_free = 0
        lowest_free = 0
        heap_trend = 0
        ip = f"{data[16]}.{data[17]}.{data[18]}.{data[19]}"
    wifi_status_str = {0: "disconnected", 1: "connected", 2: "connecting"}.get(wifi_status, f"unknown({wifi_status})")
    
    return {
        "version": version,
        "wifi_status": wifi_status_str,
        "rssi_dbm": rssi,
        "uptime_sec": uptime,
        "uptime": str(timedelta(seconds=uptime)),
        "free_heap_bytes": free_heap,
        "free_heap_kb": f"{free_heap / 1024:.1f} KB",
        "min_free_heap_bytes": min_heap,
        "min_free_heap_kb": f"{min_heap / 1024:.1f} KB",
        "peak_free_kb": f"{peak_free / 1024:.1f} KB",
        "lowest_free_kb": f"{lowest_free / 1024:.1f} KB",
        "heap_trend_rph": heap_trend,
        "ip_address": ip
    }


def decode_wifi_status(data: bytes) -> dict:
    """Decode WifiStatusPacket (64 bytes)"""
    if len(data) < 64:
        return {"error": f"Too short: {len(data)} bytes"}
    
    version = data[0]
    mode = data[1]
    connected = data[2]
    rssi = struct.unpack("<b", bytes([data[3]]))[0]
    
    sta_ip = f"{data[4]}.{data[5]}.{data[6]}.{data[7]}"
    gateway = f"{data[8]}.{data[9]}.{data[10]}.{data[11]}"
    ap_ip = f"{data[12]}.{data[13]}.{data[14]}.{data[15]}"
    
    ssid = data[16:49].split(b'\x00')[0].decode('utf-8', errors='replace')
    hostname = data[49:64].split(b'\x00')[0].decode('utf-8', errors='replace')
    
    mode_str = {0: "OFF", 1: "STA", 2: "AP", 3: "AP_STA"}.get(mode, f"unknown({mode})")
    
    return {
        "version": version,
        "mode": mode_str,
        "connected": bool(connected),
        "rssi_dbm": rssi,
        "sta_ip": sta_ip,
        "gateway": gateway,
        "ap_ip": ap_ip,
        "ssid": ssid,
        "hostname": hostname
    }


def decode_thermometers(data: bytes) -> dict:
    """Decode BleThermometersPacket (90 bytes)"""
    if len(data) < 2:
        return {"error": f"Too short: {len(data)} bytes"}
        
    version = data[0]
    count = data[1]
    sensors = []
    
    # 11 bytes per sensor
    for i in range(count):
        offset = 2 + (i * 11)
        if offset + 11 > len(data):
            break
            
        # struct format: id(B), batt(B), temp(h), humid(H), rssi(b), age(I)
        sid, batt, temp_x10, humid_x10, rssi, age = struct.unpack("<BBhH b I", data[offset:offset+11])
        
        sensors.append({
            "id": sid,
            "temp": temp_x10 / 10.0,
            "humid": humid_x10 / 10.0,
            "batt": batt,
            "rssi": rssi,
            "last_seen_sec": age
        })
        
    return {"version": version, "count": count, "sensors": sensors}


def decode_runtime_stats(data: bytes) -> dict:
    """Decode RuntimeStatsPacket (29 bytes)"""
    if len(data) < 29:
        return {"error": f"Too short: {len(data)} bytes"}
        
    # struct format: ver(B), 7 * uint32(I)
    # V1 (29 bytes) or V2 (41 bytes)
    version = data[0]
    
    if len(data) >= 41:
        stats = struct.unpack("<IIIIIIIIII", data[1:41])
        # stats: 0-6 (V1), 7-9 (V2)
    else:
        stats = struct.unpack("<IIIIIII", data[1:29])
        # Extend with 0s
        stats = stats + (0, 0, 0)

    return {
        "version": version,
        "webhook_sent": stats[0],
        "webhook_failed": stats[1],
        "telegram_sent": stats[2],
        "sensor_reads": stats[3],
        "alarm_triggers": stats[4],
        "ble_adv_total": stats[5],
        "uptime_sec": stats[6],
        "ble_adv_matched": stats[7],
        "ble_adv_parsed": stats[8],
        "ble_adv_callback": stats[9]
    }


def decode_shelly_status(data: bytes) -> dict:
    """Decode ShellyStatusPacket (130 bytes)"""
    if len(data) < 2:
        return {"error": f"Too short: {len(data)} bytes"}
        
    version = data[0]
    count = data[1]
    devices = []
    
    # 16 bytes per device (V2)
    for i in range(count):
        offset = 2 + (i * 16)
        if offset + 16 > len(data):
            break
            
        # struct format: id(B), flags(B), power(f), energy(f), voltX10(H), currX1000(H), tempX100(h)
        did, flags, power, energy, volt_x10, curr_x1000, temp_x100 = struct.unpack("<BBffHHh", data[offset:offset+16])
        
        f_list = []
        if flags & 0x01: f_list.append("enabled")
        if flags & 0x02: f_list.append("online")
        if flags & 0x04: f_list.append("on")
        
        devices.append({
            "id": did,
            "flags": f_list,
            "power_w": power,
            "energy_wm": energy,
            "voltage": volt_x10 / 10.0,
            "current_ma": curr_x1000,
            "temp": temp_x100 / 100.0 if temp_x100 != -32768 else None
        })
        
    return {"version": version, "count": count, "devices": devices}


def decode_alarm_status(data: bytes) -> dict:
    """Decode AlarmStatusPacket (82 bytes)"""
    if len(data) < 2:
        return {"error": f"Too short: {len(data)} bytes"}
        
    version = data[0]
    count = data[1]
    alarms = []
    
    # 10 bytes per alarm
    for i in range(count):
        offset = 2 + (i * 10)
        if offset + 10 > len(data):
            break
            
        # struct format: id(B), flags(B), lastTriggerSec(I), value(f)
        aid, flags, last_trig, value = struct.unpack("<BBIf", data[offset:offset+10])
        
        alarms.append({
            "id": aid,
            "triggered": bool(flags & 0x01),
            "enabled": bool(flags & 0x02),
            "last_trigger_sec": last_trig,
            "value": value
        })
        
    return {"version": version, "count": count, "alarms": alarms}


def decode_wifi_sensing(data: bytes) -> dict:
    """Decode WifiSensingPacket (12 bytes)"""
    if len(data) < 12:
        return {"error": f"Too short: {len(data)} bytes"}
        
    # struct format: ver(B), motion(B), variance(f), min(b), max(b), moves(I)
    ver, motion, variance, min_rssi, max_rssi, moves = struct.unpack("<BBfbbI", data[:12])
    
    return {
        "version": ver,
        "motion_detected": bool(motion),
        "variance": variance,
        "min_rssi": min_rssi,
        "max_rssi": max_rssi,
        "moves_detected": moves
    }


def create_wifi_config_command(ssid: str, password: str, go_online: bool = True) -> bytes:
    """Create WifiConfigCommand packet (100 bytes)"""
    data = bytearray(100)
    data[0] = 0x03  # SET_ALL
    data[1] = 1 if go_online else 0
    
    # SSID at offset 2 (max 32 chars)
    ssid_bytes = ssid.encode('utf-8')[:32]
    data[2:2+len(ssid_bytes)] = ssid_bytes
    
    # Password at offset 35 (max 64 chars)
    pass_bytes = password.encode('utf-8')[:64]
    data[35:35+len(pass_bytes)] = pass_bytes
    
    return bytes(data)


async def scan_devices():
    """Scan for BLE devices"""
    print("🔍 Scanning for BLE devices (5 seconds)...\n")
    
    devices = await BleakScanner.discover(timeout=5.0, return_adv=True)
    
    matrixhub_found = False
    sorted_devices = sorted(devices.items(), key=lambda x: x[1][1].rssi or -999, reverse=True)
    
    for address, (device, adv_data) in sorted_devices:
        name = device.name or adv_data.local_name or "(unknown)"
        rssi = adv_data.rssi or "?"
        marker = "👉 " if name == DEVICE_NAME else "   "
        print(f"{marker}{address}  RSSI: {rssi:>4}  Name: {name}")
        if name == DEVICE_NAME:
            matrixhub_found = True
    
    if matrixhub_found:
        print(f"\n✅ Found {DEVICE_NAME} device!")
    else:
        print(f"\n⚠️  {DEVICE_NAME} not found. Make sure BLE is advertising.")
    
    return devices


async def find_device() -> str | None:
    """Find MatrixHub device by name or service UUID"""
    print(f"🔍 Searching for MatrixHub...")
    
    # Scan for all devices
    devices = await BleakScanner.discover(timeout=5.0, return_adv=True)
    
    # First try to find by device name
    for address, (device, adv_data) in devices.items():
        name = device.name or adv_data.local_name or ""
        if name == DEVICE_NAME:
            print(f"✅ Found MatrixHub: {address} (Name: {name})")
            return address
    
    # Fallback: Try to find by service UUID
    print("⚠️  Not found by name, trying service UUID filter...")
    devices_by_svc = await BleakScanner.discover(
        timeout=5.0, 
        return_adv=True,
        service_uuids=[SVC_MATRIXHUB]
    )
    
    if devices_by_svc:
        for address, (device, adv_data) in devices_by_svc.items():
            name = device.name or adv_data.local_name or "n/a"
            print(f"✅ Found MatrixHub: {address} (Name: {name})")
            return address
    
    # Last resort: probe n/a devices
    print("⚠️  Not found via service filter, trying to probe n/a devices...")
    
    all_devices = await BleakScanner.discover(timeout=5.0, return_adv=True)
    
    for address, (device, adv_data) in all_devices.items():
        name = device.name or adv_data.local_name
        if name and name != "n/a":
            continue  # Skip known devices
        
        print(f"   Probing {address}...", end=" ", flush=True)
        try:
            async with BleakClient(address, timeout=3.0) as client:
                services = client.services
                for svc in services:
                    if "56789abc2000" in svc.uuid.lower():
                        print(f"✅ Found MatrixHub!")
                        return address
                print("not MatrixHub")
        except Exception as e:
            print(f"failed ({type(e).__name__})")
    
    print(f"\n❌ MatrixHub not found.")
    return None


async def read_all_characteristics():
    """Read all characteristics from MatrixHub device"""
    address = await find_device()
    if not address:
        return
    
    print(f"\n📡 Connecting to {address}...")
    
    async with BleakClient(address) as client:
        print(f"✅ Connected! MTU: {client.mtu_size}\n")
        
        # Device Info (standard service)
        print("=" * 50)
        print("📱 DEVICE INFORMATION (0x180A)")
        print("=" * 50)
        
        try:
            manufacturer = await client.read_gatt_char(CHAR_MANUFACTURER)
            print(f"  Manufacturer: {manufacturer.decode()}")
        except Exception as e:
            print(f"  Manufacturer: (error: {e})")
        
        try:
            model = await client.read_gatt_char(CHAR_MODEL)
            print(f"  Model:        {model.decode()}")
        except Exception as e:
            print(f"  Model: (error: {e})")
        
        try:
            firmware = await client.read_gatt_char(CHAR_FIRMWARE)
            print(f"  Firmware:     {firmware.decode()}")
        except Exception as e:
            print(f"  Firmware: (error: {e})")
        
        # Sensor Data
        print("\n" + "=" * 50)
        print(f"🌡️  SENSOR DATA ({CHAR_SENSOR_DATA[-4:]})")
        print("=" * 50)
        
        try:
            data = await client.read_gatt_char(CHAR_SENSOR_DATA)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_sensor_data(data)
            for k, v in decoded.items():
                print(f"  {k}: {v}")
        except Exception as e:
            print(f"  Error: {e}")
        
        # System Status
        print("\n" + "=" * 50)
        print(f"📊 SYSTEM STATUS ({CHAR_SYSTEM_STATUS[-4:]})")
        print("=" * 50)
        
        try:
            data = await client.read_gatt_char(CHAR_SYSTEM_STATUS)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_system_status(data)
            for k, v in decoded.items():
                print(f"  {k}: {v}")
        except Exception as e:
            print(f"  Error: {e}")
        
        # WiFi Status
        print("\n" + "=" * 50)
        print(f"📶 WIFI STATUS ({CHAR_WIFI_STATUS[-4:]})")
        print("=" * 50)
        
        try:
            data = await client.read_gatt_char(CHAR_WIFI_STATUS)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_wifi_status(data)
            for k, v in decoded.items():
                print(f"  {k}: {v}")
        except Exception as e:
            print(f"  Error: {e}")

        # Thermometers
        print("\n" + "=" * 50)
        print(f"🌡️  THERMOMETERS ({CHAR_THERMOMETERS[-4:]})")
        print("=" * 50)
        try:
            data = await client.read_gatt_char(CHAR_THERMOMETERS)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_thermometers(data)
            print(f"  Version: {decoded.get('version')}, Count: {decoded.get('count')}")
            for s in decoded.get('sensors', []):
                print(f"    #{s['id']}: {s['temp']}°C, {s['humid']}%, Batt: {s['batt']}%, RSSI: {s['rssi']}, LastSeen: {s['last_seen_sec']}s")
        except Exception as e:
            print(f"  Error: {e}")

        # Runtime Stats
        print("\n" + "=" * 50)
        print(f"⏱️  RUNTIME STATS ({CHAR_RUNTIME_STATS[-4:]})")
        print("=" * 50)
        try:
            data = await client.read_gatt_char(CHAR_RUNTIME_STATS)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_runtime_stats(data)
            for k, v in decoded.items():
                print(f"  {k}: {v}")
        except Exception as e:
            print(f"  Error: {e}")

        # Shelly
        print("\n" + "=" * 50)
        print(f"🔌 SHELLY STATUS ({CHAR_SHELLY_STATUS[-4:]})")
        print("=" * 50)
        try:
            data = await client.read_gatt_char(CHAR_SHELLY_STATUS)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_shelly_status(data)
            for d in decoded.get('devices', []):
                print(f"    #{d['id']}: Flags={d['flags']}, {d['power_w']}W, {d['energy_wm']}Em")
        except Exception as e:
            print(f"  Error: {e}")

        # Alarms
        print("\n" + "=" * 50)
        print(f"🚨 ALARM STATUS ({CHAR_ALARM_STATUS[-4:]})")
        print("=" * 50)
        try:
            data = await client.read_gatt_char(CHAR_ALARM_STATUS)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_alarm_status(data)
            for a in decoded.get('alarms', []):
                print(f"    #{a['id']}: Triggered={a['triggered']}, Enabled={a['enabled']}, LastTrig={a['last_trigger_sec']}s")
        except Exception as e:
            print(f"  Error: {e}")

        # Wifi Sensing
        print("\n" + "=" * 50)
        print(f"📡 WIFI SENSING ({CHAR_WIFI_SENSING[-4:]})")
        print("=" * 50)
        try:
            data = await client.read_gatt_char(CHAR_WIFI_SENSING)
            print(f"  Raw bytes: {data.hex()}")
            decoded = decode_wifi_sensing(data)
            for k, v in decoded.items():
                print(f"  {k}: {v}")
        except Exception as e:
            print(f"  Error: {e}")
        
        print("\n✅ Done!")


async def read_sensors_with_notify():
    """Subscribe to sensor data notifications"""
    address = await find_device()
    if not address:
        return
    
    print(f"\n📡 Connecting to {address}...")
    
    async with BleakClient(address) as client:
        print(f"✅ Connected!")
        
        # First, do an initial READ to get current values
        print("📖 Reading current sensor values...")
        try:
            data = await client.read_gatt_char(CHAR_SENSOR_DATA)
            decoded = decode_sensor_data(data)
            print(f"🌡️  CO2: {decoded['co2_ppm']} ppm | "
                  f"Temp: {decoded['temperature_c']:.1f}°C | "
                  f"Humid: {decoded['humidity_pct']:.1f}% | "
                  f"Uptime: {decoded['uptime']}")
            print(f"   Flags: valid={decoded['flags']['valid']}, stale={decoded['flags']['stale']}, error={decoded['flags']['error']}")
        except Exception as e:
            print(f"⚠️  Initial read failed: {e}")
        
        print("\n📊 Subscribing to sensor notifications (Ctrl+C to stop)...\n")
        
        def notification_handler(sender, data):
            decoded = decode_sensor_data(data)
            print(f"🌡️  CO2: {decoded['co2_ppm']} ppm | "
                  f"Temp: {decoded['temperature_c']:.1f}°C | "
                  f"Humid: {decoded['humidity_pct']:.1f}% | "
                  f"Uptime: {decoded['uptime']}")
        
        await client.start_notify(CHAR_SENSOR_DATA, notification_handler)
        
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("\n\n⏹️  Stopped.")
        
        await client.stop_notify(CHAR_SENSOR_DATA)


async def set_wifi_credentials(ssid: str, password: str):
    """Set WiFi credentials via BLE"""
    address = await find_device()
    if not address:
        return
    
    print(f"\n📡 Connecting to {address}...")
    
    async with BleakClient(address) as client:
        print(f"✅ Connected!")
        
        # Read current status first
        print("\n📶 Current WiFi status:")
        try:
            data = await client.read_gatt_char(CHAR_WIFI_STATUS)
            decoded = decode_wifi_status(data)
            print(f"   Mode: {decoded['mode']}, Connected: {decoded['connected']}, SSID: {decoded['ssid']}")
        except Exception as e:
            print(f"   Error reading status: {e}")
        
        # Create and send command
        command = create_wifi_config_command(ssid, password, go_online=True)
        print(f"\n📤 Sending WiFi config...")
        print(f"   SSID: {ssid}")
        print(f"   Password: {'*' * len(password)}")
        print(f"   Command bytes: {command[:10].hex()}...")
        
        try:
            await client.write_gatt_char(CHAR_WIFI_CONFIG, command, response=True)
            print("\n✅ WiFi credentials sent!")
            print("⚠️  Device will restart in ~2 seconds to apply changes.")
            print("   Connection will be lost.")
        except Exception as e:
            print(f"\n❌ Error: {e}")


async def read_wifi_status():
    """Read WiFi status only"""
    address = await find_device()
    if not address:
        return
    
    async with BleakClient(address) as client:
        print(f"✅ Connected to {address}\n")
        
        data = await client.read_gatt_char(CHAR_WIFI_STATUS)
        print(f"Raw bytes ({len(data)}): {data.hex()}\n")
        
        decoded = decode_wifi_status(data)
        print("📶 WiFi Status:")
        for k, v in decoded.items():
            print(f"   {k}: {v}")


def print_usage():
    print("""
🔌 BLE Test Client for MatrixHub

Usage:
    python ble_test_client.py scan              Scan for BLE devices
    python ble_test_client.py read              Read all characteristics
    python ble_test_client.py wifi              Read WiFi status only
    python ble_test_client.py sensors           Monitor sensor data (notify)
    python ble_test_client.py set-wifi SSID PASS  Set WiFi credentials

Examples:
    python ble_test_client.py scan
    python ble_test_client.py read
    python ble_test_client.py set-wifi "MyNetwork" "password123"
    """)


def main():
    if len(sys.argv) < 2:
        print_usage()
        return
    
    cmd = sys.argv[1].lower()
    
    if cmd == "scan":
        asyncio.run(scan_devices())
    elif cmd == "read":
        asyncio.run(read_all_characteristics())
    elif cmd == "wifi":
        asyncio.run(read_wifi_status())
    elif cmd == "sensors":
        asyncio.run(read_sensors_with_notify())
    elif cmd == "set-wifi":
        if len(sys.argv) < 4:
            print("Usage: python ble_test_client.py set-wifi SSID PASSWORD")
            return
        ssid = sys.argv[2]
        password = sys.argv[3]
        asyncio.run(set_wifi_credentials(ssid, password))
    else:
        print(f"Unknown command: {cmd}")
        print_usage()


if __name__ == "__main__":
    main()
