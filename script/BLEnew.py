#!/usr/bin/env python3
import asyncio
from bleak import BleakClient
import sys
import os

# Change these UUIDs if desired—they must match the ones in your LilyGo code.
SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0"
CHARACTERISTIC_UUID = "12345678-1234-5678-1234-56789abcdef1"

# Replace with your LilyGo T-TWR's BLE MAC address
DEVICE_ADDRESS = "XX:XX:XX:XX:XX:XX"  

# Chunk size for BLE transfer (often 20 bytes is safe, but you can experiment with a higher value if MTU is negotiated)
CHUNK_SIZE = 20

async def send_file(file_path):
    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return

    with open(file_path, "rb") as f:
        data = f.read()

    async with BleakClient(DEVICE_ADDRESS) as client:
        # Optionally, wait for services to be discovered
        print("Connected to device!")
        await asyncio.sleep(1)

        total_length = len(data)
        print(f"Sending file ({total_length} bytes) in chunks of {CHUNK_SIZE} bytes...")

        for i in range(0, total_length, CHUNK_SIZE):
            chunk = data[i : i + CHUNK_SIZE]
            try:
                # Write without response
                await client.write_gatt_char(CHARACTERISTIC_UUID, chunk, response=False)
            except Exception as e:
                print(f"Error sending chunk starting at {i}: {e}")
            # Small delay to avoid overloading BLE
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
