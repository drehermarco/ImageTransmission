import numpy as np
from scipy.io import wavfile
from PIL import Image

# Parameters
sample_rate = 8000
baud_rate = 4000  # Maximum safe baud rate for this sample rate
carrier_freq = 5000  # Carrier frequency (Hz)
f00 = 1000  # Frequency for bit pair 00 (Hz)
f01 = 2000  # Frequency for bit pair 01 (Hz)
f10 = 3000  # Frequency for bit pair 10 (Hz)
f11 = 4000  # Frequency for bit pair 11 (Hz)
marker_freq = 500  # Frequency for synchronization marker (Hz)
marker_duration = 1.0  # Increased duration for better synchronization (seconds)

# Step 1: Load image and convert to binary
def image_to_binary(image_path):
    img = Image.open(image_path).convert('L')
    img_array = np.array(img).flatten()
    binary_data = ''.join(format(pixel, '08b') for pixel in img_array)
    return binary_data

binary_data = image_to_binary('images/thisisfine.png')

# Step 2: Generate FSK signal with four frequencies and carrier modulation
def generate_fsk_signal(binary_data, f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration):
    bit_duration = 1 / baud_rate
    samples_per_bit = int(sample_rate * bit_duration)
    t = np.linspace(0, bit_duration, samples_per_bit, endpoint=False)

    # Generate marker tone for synchronization (distinct pattern)
    marker_samples = int(sample_rate * marker_duration)
    marker_wave = np.concatenate([
        np.sin(2 * np.pi * marker_freq * np.linspace(0, marker_duration/2, marker_samples//2, endpoint=False)),
        np.sin(2 * np.pi * (marker_freq*2) * np.linspace(0, marker_duration/2, marker_samples//2, endpoint=False))
    ])

    signal = np.concatenate((marker_wave,))

    window = np.hamming(len(t))
    waves = {
        '00': np.sin(2 * np.pi * f00 * t) * np.cos(2 * np.pi * carrier_freq * t) * window,
        '01': np.sin(2 * np.pi * f01 * t) * np.cos(2 * np.pi * carrier_freq * t) * window,
        '10': np.sin(2 * np.pi * f10 * t) * np.cos(2 * np.pi * carrier_freq * t) * window,
        '11': np.sin(2 * np.pi * f11 * t) * np.cos(2 * np.pi * carrier_freq * t) * window
    }


    # Generate signal based on binary data
    for i in range(0, len(binary_data), 2):
        pair = binary_data[i:i+2]
        if pair not in waves:
            print(f"Invalid bit pair '{pair}' encountered, skipping.")
            continue
        wave = waves[pair]
        signal = np.concatenate((signal, wave))

    # Append marker tone at the end
    signal = np.concatenate((signal, marker_wave))

    # Normalize signal
    signal = (signal / np.max(np.abs(signal)) * 32767).astype(np.int16)
    wavfile.write('fsk_image.wav', sample_rate, signal)
    print("FSK-modulated WAV file with markers saved as 'fsk_image.wav'")

generate_fsk_signal(binary_data, f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration)