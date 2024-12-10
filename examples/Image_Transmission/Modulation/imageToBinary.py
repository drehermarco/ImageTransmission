from PIL import Image
import numpy as np

def image_to_binary(image_path, output_txt):
    # Load and convert image to grayscale
    img = Image.open(image_path).convert('L')
    
    img = img.resize((50, 50))  # Ensure it's 50x50
    pixels = np.array(img)
    
    # Convert pixel values to binary
    binary_data = ''.join(f'{pixel:08b}' for pixel in pixels.flatten())
    
    # Save binary data to a text file
    with open(output_txt, 'w') as file:
        file.write(binary_data)
    print(f"Binary data saved to {output_txt}")
    return binary_data


binary_data = image_to_binary('image.png', 'binary.txt')