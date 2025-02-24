import asyncio
from bleak import BleakClient, discover

DEVICE_ADDRESS = "3c:84:27:cc:18:85"

BLE_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
BLE_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
FILE_PATH = "Preprocessing/binary.txt"

def get_data_from_file(file_path):
    with open(file_path, 'r') as file:
        for line in file:
            yield line.strip().encode('utf-8')

async def send_data():
    async with BleakClient(DEVICE_ADDRESS) as client:
        print(f"Connected: {client.is_connected}")

        data_generator = get_data_from_file(FILE_PATH)

        for data_to_send in data_generator:
            if data_to_send == b'quit':
                return
            await client.write_gatt_char(BLE_CHARACTERISTIC_UUID, data_to_send)
            print(f"Data sent: {data_to_send}")

async def discover_devices():
    devices = await discover()
    for device in devices:
        print(device)

async def run():
    await send_data()

asyncio.run(run())