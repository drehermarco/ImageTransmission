import asyncio
import sys
import os
from bleak import BleakClient


SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"


DEVICE_ADDRESS = "3c:84:27:cc:18:85"

# Chunk size to send at one time (adjust if your BLE connection negotiates a larger MTU)
CHUNK_SIZE = 128

async def send_file(file_path):
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    with open(file_path, "rb") as f:
        data = f.read()

    async with BleakClient(DEVICE_ADDRESS) as client:
        print("Connected to device!")
        # Wait a short time for services and MTU negotiation (if applicable)
        await asyncio.sleep(1)
        
        total_length = len(data)
        print(f"Sending file ({total_length} bytes) in chunks of {CHUNK_SIZE} bytes...")

        for i in range(0, total_length, CHUNK_SIZE):
            chunk = data[i : i + CHUNK_SIZE]
            try:
                # Write without response for speed
                await client.write_gatt_char(CHARACTERISTIC_UUID, chunk, response=False)
            except Exception as e:
                print(f"Error sending chunk starting at byte {i}: {e}")
            # A short delay to avoid overwhelming the BLE stack
            await asyncio.sleep(0.01)

        print("File sent.")

def main():
    if len(sys.argv) < 2:
        print("Usage: python send_wav.py <path_to_wav_file>")
        sys.exit(1)
    file_path = sys.argv[1]
    asyncio.run(send_file(file_path))

if __name__ == "__main__":
    main()
