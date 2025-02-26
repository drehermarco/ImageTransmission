import serial
import time
import threading
import queue

def serial_reader(ser, data_queue):
    while True:
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    data_queue.put(line)
            except Exception as e:
                print(f"Error reading line: {e}")
        else:
            time.sleep(0.1)

def file_writer(data_queue, filename):
    with open(filename, 'a') as file:
        while True:
            try:
                line = data_queue.get(timeout=1)
                file.write(line + '\n')
                file.flush()
            except queue.Empty:
                continue

def main():
    port = '/dev/cu.usbmodem101'
    baud_rate = 115200
    output_file = "serial_data.txt"

    try:
        ser = serial.Serial(port, baud_rate, timeout=0.1)
        print(f"Connected to {port} at {baud_rate} baud.")
    except Exception as e:
        print(f"Error opening serial port: {e}")
        return

    data_queue = queue.Queue()

    reader_thread = threading.Thread(target=serial_reader, args=(ser, data_queue), daemon=True)
    reader_thread.start()

    writer_thread = threading.Thread(target=file_writer, args=(data_queue, output_file), daemon=True)
    writer_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Exiting program.")
    finally:
        ser.close()

if __name__ == '__main__':
    main()
