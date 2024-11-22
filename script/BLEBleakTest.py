import asyncio
from bleak import BleakClient, discover

DEVICE_ADDRESS = "3c:84:27:cc:18:85"

BLE_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
BLE_CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

def get_data():
    data = input("What do you want to send? ")
    return data.encode('utf-8') 

async def send_data():
    async with BleakClient(DEVICE_ADDRESS) as client:
        print(f"Connected: {client.is_connected}")
        
        while(True):
            data_to_send = get_data()
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
