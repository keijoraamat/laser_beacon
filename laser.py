import machine
import time

class LaserSensor:
    # Laser sensor command definitions
    CMD_OPEN_LASER = bytes([0x80, 0x06, 0x05, 0x01, 0x74])    # Open laser
    CMD_GET_SINGLE = bytes([0x80, 0x06, 0x02, 0x78])          # Single measurement
    CMD_RANGE_10M = bytes([0xFA, 0x04, 0x09, 0x0A, 0xEF])     # Set range to 10m
    CMD_RESOLUTION_1MM = bytes([0xFA, 0x04, 0x0C, 0x01, 0xF5]) # Set resolution to 1mm

    def __init__(self, uart_id=1, baudrate=9600, tx_pin=None, rx_pin=None):
        # Initialize the serial connection to the sensor
        if tx_pin is None or rx_pin is None:
            raise ValueError("TX and RX pins must be specified.")
        self.uart = machine.UART(uart_id, baudrate=baudrate, tx=tx_pin, rx=rx_pin)

    def configure_sensor(self):
        """Initialize and configure the sensor settings."""
        

        # Set resolution to 1mm
        print("Setting resolution to 1mm...")
        self.uart.write(self.CMD_RESOLUTION_1MM)
        time.sleep(0.5)

        # Set range to 10m
        print("Setting range to 10m...")
        self.uart.write(self.CMD_RANGE_10M)
        time.sleep(0.5)

        # Open the laser
        print("Turning on the laser...")
        self.uart.write(self.CMD_OPEN_LASER)
        time.sleep(0.5)

        # flush the cache
        self.uart.read()

    def get_distance(self):
        """Send the single measurement command and read distance data."""
        print("Sending distance measurement command.")
        self.uart.write(self.CMD_GET_SINGLE)
        time.sleep(1)  # Increased delay for response

        try:
            dist_raw = self.uart.read(60)
            if dist_raw:
                print("Raw Data:", dist_raw)
                dist = ""

                # Iterate through each byte and filter out non-ASCII characters
                for byte in dist_raw:
                    if 48 <= byte <= 57 or byte == 46:  # ASCII for '0'-'9' and '.'
                        dist += chr(byte)

                return dist
            else:
                print("No data received.")
                # flush the cache
                self.uart.read()
                return None
        except Exception as e:
            print("Error reading data:", e)
            return None

