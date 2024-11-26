import asyncio
from bleak import BleakScanner

TARGET_BEACON_NAME = "LBcn"

def parse_custom_data(advertisement_data):
    print(f"raw data {advertisement_data}")
    """
    Beacon Found: LBcn (D8:3B:DA:11:DE:BE)
    raw data AdvertisementData(local_name='LBcn', manufacturer_data={12336: b'0.34800:003.830', 14649: b'9.999:003.831'}, tx_power=9, rssi=-76)
    """
    return
    # Access the raw bytes of the advertisement data
    adv_bytes = advertisement_data.bytes

    # The structure is:
    # [Length][AD Type][Data]...

    # We need to find the AD Type 0xFF and extract the measurements
    i = 0
    while i < len(adv_bytes):
        length = adv_bytes[i]
        if length == 0:
            break  # Reached the end of advertisement data
        ad_type = adv_bytes[i + 1]
        if ad_type == 0xFF:  # Manufacturer Specific Data
            # Extract the measurement data
            data_start = i + 2
            data_end = data_start + (length - 1)
            measurements_data = adv_bytes[data_start:data_end]

            if len(measurements_data) >= 4:
                # Parse the measurements
                measurement_0 = int.from_bytes(measurements_data[0:2], byteorder='little')
                measurement_1 = int.from_bytes(measurements_data[2:4], byteorder='little')
                print(f"Measurement 0: {measurement_0}")
                print(f"Measurement 1: {measurement_1}")
            else:
                print("Measurement data too short.")
            break  # Exit after finding the custom data
        else:
            # Move to the next AD structure
            i += length + 1  # Length byte + length of data
    else:
        print("Custom data not found in advertisement.")

def detection_callback(device, advertisement_data):
    if advertisement_data.local_name == TARGET_BEACON_NAME:
        print(f"Beacon Found: {device.name} ({device.address})")
        #print(f"ad data: {advertisement_data}")
        #return
        # Parse custom data from the advertisement
        parse_custom_data(advertisement_data)

async def scan_and_listen():
    print("Scanning for BLE devices...")

    # Pass the detection_callback directly to the BleakScanner constructor
    scanner = BleakScanner(detection_callback=detection_callback)
    await scanner.start()

    try:
        while True:
            await asyncio.sleep(0.2)  # Keep the scan running
    except KeyboardInterrupt:
        print("Stopping scan...")
        await scanner.stop()

if __name__ == "__main__":
    asyncio.run(scan_and_listen())
