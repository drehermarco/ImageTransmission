import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.signal import butter, filtfilt, hilbert, find_peaks

file_path = "serial_data_8bit.txt"
data = pd.read_csv(file_path, header=None, names=["Analog Value"])

sampling_rate = 1 / (277e-6)

signal = data["Analog Value"].values

def highpass_filter(data, cutoff, fs, order=5):
    nyquist = 0.5 * fs
    normal_cutoff = cutoff / nyquist
    b, a = butter(order, normal_cutoff, btype='high', analog=False)
    return filtfilt(b, a, data)

filtered_signal = highpass_filter(signal, cutoff=700, fs=sampling_rate)

segment_start = 63000
segment_end = 70000

segment_signal = filtered_signal[segment_start:segment_end]

analytic_signal = hilbert(segment_signal)
envelope = np.abs(analytic_signal)

peaks, _ = find_peaks(envelope, height=np.median(envelope) * 0.8, distance=1100 // 2)

dynamic_bit_durations = np.diff(peaks)

binary_sequence = []
bit_amplitudes = []
bit_positions = []

for i in range(len(peaks) - 1):
    start_idx = segment_start + peaks[i]
    end_idx = segment_start + peaks[i + 1]
    segment = envelope[peaks[i]:peaks[i + 1]]
    avg_amplitude = np.mean(segment)
    bit_amplitudes.append(avg_amplitude)
    bit_positions.append((start_idx, end_idx))

threshold = 590

binary_sequence = ['0' if amp > threshold else '1' for amp in bit_amplitudes]

first_bit_duration = dynamic_bit_durations[0]
median_bit_duration = np.median(dynamic_bit_durations)

first_bit_amplitude = bit_amplitudes[0]
median_amplitude = np.median(bit_amplitudes)

if (first_bit_duration < 0.8 * median_bit_duration) and (first_bit_amplitude < median_amplitude):
    print("First bit detected as an outlier (short duration and lower amplitude) → Correcting to '0'")
    binary_sequence[0] = '0'

binary_output = "".join(binary_sequence)

plt.figure(figsize=(12, 5))
plt.plot(range(segment_start, segment_end), segment_signal, linestyle="-", markersize=2, label="Filtered Signal")
plt.plot(range(segment_start, segment_end), envelope, color="red", linewidth=2, label="Envelope")
plt.axhline(threshold, color='blue', linestyle="--", label=f"Threshold: {threshold:.2f}")  # Show threshold

for i, (start_idx, end_idx) in enumerate(bit_positions):
    color = "gray"
    alpha = 0.3
    label = "Detected Bit Segments" if i == 0 else ""
    
    if i == 0 and binary_sequence[0] == '0':
        color = "yellow"
        alpha = 0.5
        label = "Corrected First Bit"

    plt.axvspan(start_idx, end_idx, color=color, alpha=alpha, label=label)

plt.xlabel("Sample Index")
plt.ylabel("Amplitude")
plt.title("Dynamic Bit Segmentation with Improved First Bit Correction")
plt.legend()
plt.grid()

plt.show()

print(f"Extracted Binary Sequence: {binary_output}")
print(f"Amplitude Threshold Used: {threshold:.2f}")
print(f"Bit Durations Detected: {dynamic_bit_durations}")



def plot_images(original, extracted, titles=("Original", "Extracted")):
    fig, axes = plt.subplots(1, 2, figsize=(8, 4))

    axes[0].imshow(original, cmap='gray', vmin=0, vmax=1)
    axes[0].set_title(titles[0])
    axes[0].axis("off")

    axes[1].imshow(extracted, cmap='gray', vmin=0, vmax=1)
    axes[1].set_title(titles[1])
    axes[1].axis("off")

    plt.show()


def binary_string_to_image(binary_string, width=3):
    height = len(binary_string) // width
    if len(binary_string) % width != 0:
        height += 1

    padded_binary_string = binary_string.ljust(width * height, '0')

    binary_array = np.array(list(map(int, padded_binary_string))).reshape((height, width))
    
    return binary_array

binary_image = binary_string_to_image(binary_output, width=3)
original_image = binary_string_to_image("01110101")


plot_images(original_image, binary_image)
