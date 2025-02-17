import numpy as np
from scipy.io import wavfile
from PIL import Image

# Parameters
sample_rate = 8000
baud_rate = 4000
carrier_freq = 5000
f00 = 1000
f01 = 2000
f10 = 3000
f11 = 4000
marker_freq = 500
marker_duration = 1.0

# Step 1: Load image and convert to binary (grayscale)
def image_to_binary(image_path):
    img = Image.open(image_path).convert('L')  # Convert to grayscale
    img_array = np.array(img)
    height, width = img_array.shape
    binary_data = ''.join(format(pixel, '08b') for pixel in img_array.flatten())
    return binary_data, height, width

binary_data, height, width = image_to_binary('images/thisisfine.png')

# Step 2: Generate FSK signal with grayscale support
def generate_fsk_signal(binary_data, f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration):
    bit_duration = 1 / baud_rate
    samples_per_bit = int(sample_rate * bit_duration)
    t = np.linspace(0, bit_duration, samples_per_bit, endpoint=False)

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

    for i in range(0, len(binary_data), 2):
        pair = binary_data[i:i+2]
        if pair not in waves:
            continue
        wave = waves[pair]
        signal = np.concatenate((signal, wave))
        if i % 10000 == 0:
            print(f"Processed {i}/{len(binary_data)} bits")

    signal = np.concatenate((signal, marker_wave))
    signal = (signal / np.max(np.abs(signal)) * 32767).astype(np.int16)
    wavfile.write('fsk_image_gray.wav', sample_rate, signal)
    print("FSK-modulated grayscale WAV file saved as 'fsk_image_gray.wav'")

generate_fsk_signal(binary_data, f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration)

# Step 3: Demodulate FSK signal and reconstruct the grayscale image
def demodulate_fsk_signal(wav_file, f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration, height, width):
    sample_rate, signal = wavfile.read(wav_file)
    signal = signal / np.max(np.abs(signal))

    bit_duration = 1 / baud_rate
    samples_per_bit = int(sample_rate * bit_duration)
    t = np.linspace(0, bit_duration, samples_per_bit, endpoint=False)

    marker_samples = int(sample_rate * marker_duration)
    marker_wave = np.concatenate([
        np.sin(2 * np.pi * marker_freq * np.linspace(0, marker_duration/2, marker_samples//2, endpoint=False)),
        np.sin(2 * np.pi * (marker_freq*2) * np.linspace(0, marker_duration/2, marker_samples//2, endpoint=False))
    ])

    correlation = np.correlate(signal, marker_wave, mode='valid')
    threshold = 0.7 * np.max(correlation)
    start_indices = np.where(correlation > threshold)[0]
    start_index = start_indices[0] if len(start_indices) > 0 else 0
    end_index = len(signal) - marker_samples - start_index

    signal = signal[start_index + marker_samples:end_index]

    window = np.hamming(samples_per_bit)
    waves = {
        '00': np.sin(2 * np.pi * f00 * t) * window,
        '01': np.sin(2 * np.pi * f01 * t) * window,
        '10': np.sin(2 * np.pi * f10 * t) * window,
        '11': np.sin(2 * np.pi * f11 * t) * window
    }

    binary_data = ''
    for i in range(0, len(signal), samples_per_bit):
        segment = signal[i:i+samples_per_bit]
        if len(segment) < samples_per_bit:
            break
        correlations = {pair: np.sum(segment * wave) for pair, wave in waves.items()}
        detected_pair = max(correlations, key=correlations.get)
        binary_data += detected_pair
        if i % 10000 == 0:
            print(f"Processed {i}/{len(signal)} samples")

    binary_data = binary_data[:len(binary_data) - len(binary_data) % 8]
    pixel_values = [int(binary_data[i:i+8], 2) for i in range(0, len(binary_data), 8)]

    total_pixels = height * width
    pixel_values = pixel_values[:total_pixels] + [0]*(total_pixels - len(pixel_values))
    img_array = np.array(pixel_values, dtype=np.uint8).reshape(height, width)
    img = Image.fromarray(img_array)
    img.save('reconstructed_gray_image.png')
    print("Grayscale image reconstructed and saved as 'reconstructed_gray_image.png'")

demodulate_fsk_signal('fsk_image_gray.wav', f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration, height, width)

