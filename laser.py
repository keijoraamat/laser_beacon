import bluetooth
import struct
import time
import ubinascii
import machine

class BluetoothBeacon:
    def __init__(self, name, interval_ms=500):
        self.ble = bluetooth.BLE()
        self.ble.active(True)
        self.name = name
        self.interval_ms = interval_ms

    def encode_adv_payload(self, sensor_data):
        name_bytes = self.name.encode('utf-8')
        name_length = len(name_bytes)
        data_bytes = sensor_data.encode('utf-8')
        data_length = len(data_bytes)
        payload = (
            struct.pack('BB', name_length + 1, 0x09) + name_bytes +  # 0x09 indicates complete local name
            struct.pack('BB', data_length + 1, 0xFF) + data_bytes    # 0xFF indicates manufacturer specific data
        )
        return payload

    def advertise(self, sensor_data):
        payload = self.encode_adv_payload(sensor_data)
        self.ble.gap_advertise(self.interval_ms, adv_data=payload)
        print("Advertising as:", self.name, "and data:", sensor_data)

    def stop_advertising(self):
        self.ble.gap_advertise(None)
        print("Stopped advertising")
