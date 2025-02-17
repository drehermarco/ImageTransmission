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
marker_duration = 1.0  # Duration of marker tone in seconds

# Step 1: Demodulate FSK signal and reconstruct the image
def demodulate_fsk_signal(wav_file, f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration):
    sample_rate, signal = wavfile.read(wav_file)

    # Normalize the signal to match the modulator's scale
    signal = signal / np.max(np.abs(signal))

    bit_duration = 1 / baud_rate
    samples_per_bit = int(sample_rate * bit_duration)
    t = np.linspace(0, bit_duration, samples_per_bit, endpoint=False)

    # Detect marker tones for synchronization with threshold-based approach
    marker_samples = int(sample_rate * marker_duration)
    marker_wave = np.concatenate([
        np.sin(2 * np.pi * marker_freq * np.linspace(0, marker_duration/2, marker_samples//2, endpoint=False)),
        np.sin(2 * np.pi * (marker_freq*2) * np.linspace(0, marker_duration/2, marker_samples//2, endpoint=False))
    ])

    correlation = np.correlate(signal, marker_wave, mode='valid')
    threshold = 0.7 * np.max(correlation)
    start_indices = np.where(correlation > threshold)[0]
    if len(start_indices) == 0:
        print("Marker not found; synchronization may be off.")
        start_index = 0
    else:
        start_index = start_indices[0]
    end_index = len(signal) - marker_samples - start_index

    # Extract the modulated data between the markers
    signal = signal[start_index + marker_samples:end_index]

    # Precompute reference waves with windowing for improved accuracy
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
            print(f"Segment {i // samples_per_bit} incomplete, skipping.")
            break
        correlations = {pair: np.sum(segment * wave) for pair, wave in waves.items()}
        print(f"Segment {i // samples_per_bit} correlations: {correlations}")
        detected_pair = max(correlations, key=correlations.get)
        binary_data += detected_pair
        print(f"Segment {i // samples_per_bit}: Detected pair {detected_pair} with correlation {correlations[detected_pair]}")

    # Ensure binary data length is a multiple of 8
    if len(binary_data) % 8 != 0:
        binary_data = binary_data[:len(binary_data) - len(binary_data) % 8]
        print("Binary data truncated to fit byte boundaries.")

    # Convert binary data to image without reversing bits
    pixel_values = [int(binary_data[i:i+8], 2) for i in range(0, len(binary_data), 8)]
    if len(pixel_values) != 784:
        print(f"Expected 784 pixels but got {len(pixel_values)}. Padding/truncating as needed.")
        pixel_values = pixel_values[:784] + [0]*(784 - len(pixel_values))

    # Invert pixel values
    pixel_values = [255 - p for p in pixel_values]

    img_array = np.array(pixel_values, dtype=np.uint8).reshape(28, 28)
    img = Image.fromarray(img_array)
    img.save('reconstructed_image.png')
    print("Image reconstructed and saved as 'reconstructed_image.png'")

demodulate_fsk_signal('fsk_image.wav', f00, f01, f10, f11, carrier_freq, baud_rate, sample_rate, marker_freq, marker_duration)