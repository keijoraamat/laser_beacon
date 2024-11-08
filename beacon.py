
# Not fully functional yet

import bluetooth
import struct
import time
from machine import Timer

class BluetoothBeacon:
    def __init__(self, uuid, manufacturer_id, data, advertise_interval=1000):
        # Initialize Bluetooth
        self.ble = bluetooth.BLE()
        self.ble.active(True)

        # Store UUID, Manufacturer ID, and data (as a list for mutability)
        self.uuid = uuid
        self.manufacturer_id = manufacturer_id
        self.data = list(data)  # Convert to list to allow mutability
        self.advertise_interval = advertise_interval

        # Setup timer for periodic advertisement
        self.advertise_timer = Timer(0)
        self.advertise_timer.init(
            period=self.advertise_interval,
            mode=Timer.PERIODIC,
            callback=self.advertise_data
        )

    def advertise_data(self, t=None):
        # Create payload with mixed types (integers and strings)
        payload = bytearray()

        for value in self.data:
            if isinstance(value, int):
                # Pack integer (2 bytes for each number, using little-endian format)
                payload.extend(struct.pack("<H", value))
            elif isinstance(value, str):
                # Encode string to bytes (UTF-8 or ASCII)
                encoded_str = value.encode('utf-8')
                # Add length prefix to indicate how long the string is
                payload.extend(struct.pack("B", len(encoded_str)))  # 1 byte for length
                payload.extend(encoded_str)
            else:
                raise TypeError("Values must be integers or strings.")


        # Advertising payload containing the manufacturer data and the numbers
        adv_data = bytearray()
        adv_data.extend(b'\x02\x01\x06')  # Flags: General Discoverable Mode, BR/EDR Not Supported
        adv_data.extend(b'\x03\x03')      # Incomplete List of 16-bit Service Class UUIDs
        adv_data.extend(self.uuid)        # UUID
        adv_data.extend(b'\x09\xFF')      # Manufacturer Specific Data
        adv_data.extend(struct.pack("<H", self.manufacturer_id))  # Manufacturer ID
        adv_data.extend(payload)  # The four numbers

        # Set the advertising payload
        self.ble.gap_advertise(100, adv_data)
        
    def stop_advertising(self):
        self.advertise_timer.deinit()
        self.ble.gap_advertise(None)

    def update_numbers(self, new_numbers):
        # Update the numbers being advertised
        if len(new_numbers) == 4:
            self.numbers = new_numbers
