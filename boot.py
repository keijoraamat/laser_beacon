import machine
import time
from laser import LaserSensor
from beacon import BluetoothBeacon

UUID = b'\xfe\xed\x80\x06\x02\x78'  # Custom UUID (16-bit)
MANUFACTURER_ID = 0xFFFF            # Example manufacturer ID
numbers = [999, 999, 999, 999]      # Default advertised numbers

sensor = LaserSensor(tx_pin=21, rx_pin=20)
beacon = BluetoothBeacon(UUID, MANUFACTURER_ID, numbers)

def save_readings(interval_seconds=30, num_readings=100):
    """Collect and send distance readings to USB serial at set intervals."""
    new_numbers = [888,888,888,888]
    sensor.configure_sensor()
    beacon.advertise_data()
    for i in range(num_readings):
        print(f"\nReading {i + 1} of {num_readings}")
        distance = sensor.get_distance()
        if distance:
            print(f"Distance: {distance} m")
            new_numbers[0]=distance
            #beacon.update_numbers(new_numbers)
        else:
            print("Failed to read distance.")
        time.sleep(interval_seconds)
    beacon.stop_advertising()

# Start saving readings every 30 seconds
save_readings(interval_seconds=10, num_readings=50)

