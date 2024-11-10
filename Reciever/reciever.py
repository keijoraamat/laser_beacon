import asyncio
from bleak import BleakScanner

TARGET_BEACON_NAME = "Laser Beacon"

async def scan_and_listen():
    scanner = BleakScanner()
    print("Scanning for BLE devices...")

    def detection_callback(device, advertisement_data):
        if TARGET_BEACON_NAME == advertisement_data.local_name:
            print(f"Beacon Found: {device.name} ({device.address})")
            print(f"Advertisement Data: {advertisement_data}")

    scanner.register_detection_callback(detection_callback)
    await scanner.start()

    try:
        while True:
            await asyncio.sleep(0.2)
    except KeyboardInterrupt:
        print("Stopping scan...")
        await scanner.stop()
