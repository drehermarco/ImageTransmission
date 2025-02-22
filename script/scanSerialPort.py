import serial
import time
import threading
import queue

def serial_reader(ser, data_queue):
    """Continuously read data from the serial port and put it into the queue."""
    while True:
        if ser.in_waiting > 0:
            try:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    data_queue.put(line)
            except Exception as e:
                print(f"Error reading line: {e}")
        else:
            time.sleep(0.1)  # avoid busy-waiting

def file_writer(data_queue, filename):
    """Continuously get data from the queue and write it to a file."""
    with open(filename, 'a') as file:
        while True:
            try:
                # Wait for a new line to be available
                line = data_queue.get(timeout=1)
                file.write(line + '\n')
                file.flush()  # flush after each write
            except queue.Empty:
                continue  # No new data; check again

def main():
    port = 'COM3'  # Update this to your correct port
    baud_rate = 115200
    output_file = "serial_data.txt"

    try:
        ser = serial.Serial(port, baud_rate, timeout=1)
        print(f"Connected to {port} at {baud_rate} baud.")
    except Exception as e:
        print(f"Error opening serial port: {e}")
        return

    # Create a thread-safe queue to hold serial data
    data_queue = queue.Queue()

    # Start the serial reader thread
    reader_thread = threading.Thread(target=serial_reader, args=(ser, data_queue), daemon=True)
    reader_thread.start()

    # Start the file writer thread
    writer_thread = threading.Thread(target=file_writer, args=(data_queue, output_file), daemon=True)
    writer_thread.start()

    try:
        # Main thread can perform other tasks or simply wait
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("Exiting program.")
    finally:
        ser.close()

if __name__ == '__main__':
    main()
