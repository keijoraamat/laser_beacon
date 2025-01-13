import asyncio
from bleak import BleakScanner
from confluent_kafka import Producer
import binascii
import struct
import json
import time
import logging
import logging.config
import signal
import os


BEACON_NAME = "LB"
MANUFACTURER_DATA_TYPE = 0x02E5  # Manufacturer Specific Data (AD Type)
BROKER_LOCATION = "192.168.0.247:9092"
MESSAGES_TOPIC = "beacon_data"
LOG_CONFIG_FILE = "logging_config.ini"

if os.path.exists(LOG_CONFIG_FILE):
    logging.config.fileConfig(LOG_CONFIG_FILE)
else:
    logging.basicConfig(level=logging.DEBUG)
    logging.warning(f"Logging configuration file '{LOG_CONFIG_FILE}' not found. Using basic configuration.")

logger = logging.getLogger("reciever_logger")

last_timestamp = None


class KafkaHandler:
    def __init__(self, broker, topic):
        self.producer = Producer({
            'bootstrap.servers': broker
        })
        self.topic = topic

    def send_data(self, data):
        try:
            self.producer.produce(self.topic, value=json.dumps(data).encode('utf-8'))
            self.producer.flush()
        except Exception as e:
            logger.error(f"Failed to send data to Kafka: {e}")

kafka_handler = KafkaHandler(broker=BROKER_LOCATION, topic=MESSAGES_TOPIC)

async def beacon_callback(device, advertisement_data):
    global last_timestamp

    if advertisement_data.local_name == BEACON_NAME:

        logger.debug(f"found beacon named: {BEACON_NAME}")

        logger.debug(f"ad_data: {advertisement_data}")
        #for key, value in advertisement_data.manufacturer_data.items():
        #    logger.debug(f"Manufacturer data key: {hex(key)}, value: {binascii.hexlify(value)}")
        manufacturer_data = next(iter(advertisement_data.manufacturer_data.values()), None)

        if manufacturer_data:
            # Add leading two zero bytes
            if len(manufacturer_data) == 10:
                manufacturer_data = b'\x00\x00' + manufacturer_data
            logger.debug(f"Raw Manufacturer_data (hex): {binascii.hexlify(manufacturer_data)}")

            try:
                timestamp, number1, number2 = struct.unpack(
                    '>I3s3s', manufacturer_data[:10]
                )
                number1 = int.from_bytes(number1, byteorder='big') / 1000.0
                number2 = int.from_bytes(number2, byteorder='big') / 1000.0
                logger.debug(f"n1: {number1}, n2: {number2}")

                if last_timestamp is None or timestamp != last_timestamp:
                    last_timestamp = timestamp
                    local_timestamp = int(time.time())

                    data = {
                        "ts": local_timestamp,
                        "x": number1,
                        "y": number2
                    }

                    logger.debug(f"data from {device.address} to kafka: {data}")
                    kafka_handler.send_data(data)

            except struct.error as e:
                logger.error(f"Failed to unpack manufacturer data: {e}")

async def scan_beacon():
    scanner = BleakScanner()
    scanner.register_detection_callback(beacon_callback)
    logger.info("Scanning for BLE beacons...")
    await scanner.start()
    try:
        while True:
            await asyncio.sleep(1)
    except asyncio.CancelledError:
        logger.info("Stopping scanner...")
        await scanner.stop()

async def main():
    task = asyncio.create_task(scan_beacon())
    try:
        await task
    except KeyboardInterrupt:
        logger.info("KeyboardInterrupt detected. Stopping...")
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            logger.info("Scanner stopped.")

if __name__ == "__main__":
    asyncio.run(main())
