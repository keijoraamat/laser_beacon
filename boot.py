import machine
import time
from laser import LaserSensor
from beacon import BluetoothBeacon

beacon_name = "Laser Beacon"

sensor = LaserSensor(tx_pin=21, rx_pin=20)
beacon = BluetoothBeacon(beacon_name)

def save_readings(interval_seconds=30, num_readings=100):
    """Collect and send distance readings to USB serial at set intervals."""
    sensor.configure_sensor()
    for i in range(num_readings):
        print(f"\nReading {i + 1} of {num_readings}")
        distance = sensor.get_distance()
        if distance:
            print(f"Distance: {distance} m")
            beacon.advertise(distance)
        else:
            print("Failed to read distance.")
        time.sleep(interval_seconds)
    beacon.stop_advertising()

# Start saving readings every 30 seconds
save_readings(interval_seconds=10, num_readings=50)
