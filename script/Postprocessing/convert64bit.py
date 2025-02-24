import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt, hilbert, find_peaks

def compute_smoothed_envelope(signal, sampling_rate, cutoff=700, window_size=300):
    def highpass_filter(data, cutoff, fs, order=5):
        nyquist = 0.5 * fs
        normal_cutoff = cutoff / nyquist
        b, a = butter(order, normal_cutoff, btype='high', analog=False)
        return filtfilt(b, a, data)

    filtered_signal = highpass_filter(signal, cutoff, sampling_rate)

    analytic_signal = hilbert(filtered_signal)
    envelope = np.abs(analytic_signal)

    smoothed_envelope = np.convolve(envelope, np.ones(window_size)/window_size, mode='same')

    return smoothed_envelope

def extract_bits_from_envelope(smoothed_envelope, segment_start, segment_end, min_height=350, peak_distance=700):
    peaks, _ = find_peaks(smoothed_envelope[segment_start:segment_end], height=min_height, distance=peak_distance)

    bitstring = ""
    bit_positions = []
    for peak in peaks:
        amplitude = smoothed_envelope[segment_start + peak]
        if 370 <= amplitude <= 420:
            bitstring += "1"
            bit_positions.append((segment_start + peak, amplitude, '1'))
        elif 750 <= amplitude <= 800:
            bitstring += "0"
            bit_positions.append((segment_start + peak, amplitude, '0'))

    return bitstring, bit_positions

def plot_envelope_with_bits(smoothed_envelope, bit_positions, segment_start, segment_end):
    plt.figure(figsize=(18, 5))
    
    plt.plot(range(segment_start, segment_end), smoothed_envelope[segment_start:segment_end], 
             color="orange", linewidth=2, linestyle="-", label="Smoothed Envelope")

    for pos, amp, bit in bit_positions:
        color = 'red' if bit == '0' else 'blue'
        plt.scatter(pos, amp, color=color, label=f"Detected {bit}" if pos == bit_positions[0][0] else "", zorder=3)

    plt.xlabel("Sample Index")
    plt.ylabel("Amplitude")
    plt.title("Smoothed Envelope with Detected Bits")
    plt.legend()
    plt.grid()
    plt.show()

file_path = "serial_data_64bit.txt"
data = pd.read_csv(file_path, header=None, names=["Analog Value"])
sampling_rate = 1 / (277e-6)

signal = data["Analog Value"].values
segment_start = 29100
segment_end = 85400

smoothed_envelope = compute_smoothed_envelope(signal, sampling_rate, window_size=300)

bitstring, bit_positions = extract_bits_from_envelope(smoothed_envelope, segment_start, segment_end, min_height=300, peak_distance=600)

print(f"Extrahierter Bitstring: {bitstring}")

plot_envelope_with_bits(smoothed_envelope, bit_positions, segment_start, segment_end)


def plot_images(original, extracted, titles=("Original", "Extracted")):
    fig, axes = plt.subplots(1, 2, figsize=(8, 4))

    axes[0].imshow(original, cmap='gray', vmin=0, vmax=1)
    axes[0].set_title(titles[0])
    axes[0].axis("off")

    axes[1].imshow(extracted, cmap='gray', vmin=0, vmax=1)
    axes[1].set_title(titles[1])
    axes[1].axis("off")

    plt.show()


def binary_string_to_image(binary_string, width=8):
    height = len(binary_string) // width
    if len(binary_string) % width != 0:
        height += 1

    padded_binary_string = binary_string.ljust(width * height, '0')

    binary_array = np.array(list(map(int, padded_binary_string))).reshape((height, width))
    
    return binary_array

binary_image = binary_string_to_image(bitstring, width=8)
original_image = binary_string_to_image("1000011111100100001000001011011101110001111000011011011011010000")


plot_images(original_image, binary_image)
