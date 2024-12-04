import numpy as np
from scipy.io import wavfile

# Modulation parameters
sampling_rate = 5000       # Sampling rate in Hz
bit_duration = 0.01          # Bit duration in seconds (100 ms)
freq_one = 2000             # Frequency for binary "1" in Hz
freq_zero = 1000             # Frequency for binary "0" in Hz

# Binary data to transmit
binary_data = [0b10101010, 0b11001100, 0b11110000]

def generate_fsk_signal(binary_data, freq_one, freq_zero, bit_duration, sampling_rate):
    signal = np.array([], dtype=np.float32)
    t = np.linspace(0, bit_duration, int(sampling_rate * bit_duration), endpoint=False)

    for byte in binary_data:
        for bit in range(7, -1, -1):
            if (byte >> bit) & 1:
                # Generate tone for "1"
                freq = freq_one
            else:
                # Generate tone for "0"
                freq = freq_zero
            # Generate sine wave for the bit
            sine_wave = np.sin(2 * np.pi * freq * t)
            signal = np.concatenate((signal, sine_wave))
    return signal

# Generate the FSK signal
fsk_signal = generate_fsk_signal(binary_data, freq_one, freq_zero, bit_duration, sampling_rate)

# Normalize the signal to 16-bit integer range
max_int16 = np.iinfo(np.int16).max
fsk_signal_int16 = np.int16(fsk_signal / np.max(np.abs(fsk_signal)) * max_int16)

# Save the signal to a WAV file
wavfile.write('fsk_signal.wav', sampling_rate, fsk_signal_int16)

print("FSK WAV file generated successfully.")
