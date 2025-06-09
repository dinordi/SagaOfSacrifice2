#!/usr/bin/env python3
# filepath: png_storage_comparison.py

import os
import sys
from PIL import Image
import glob

def analyze_png_directory(directory_path):
    """Analyze all PNG files in a directory to compare storage sizes"""
    
    if not os.path.isdir(directory_path):
        print(f"Error: {directory_path} is not a valid directory")
        return
    
    png_files = glob.glob(os.path.join(directory_path, "**/*.png"), recursive=True)
    
    if not png_files:
        print(f"No PNG files found in {directory_path}")
        return
    
    print(f"Found {len(png_files)} PNG files in {directory_path}")
    print("-" * 80)
    print("| {:<40} | {:>10} | {:>10} | {:>10} |".format(
        "Filename", "File Size", "RGBA Size", "Ratio"))
    print("-" * 80)
    
    total_file_size = 0
    total_rgba_size = 0
    startaddress = 0x30000000
    page_size = 4096
    
    for png_file in png_files:
        filename = os.path.basename(png_file)
        display_name = filename if len(filename) <= 40 else filename[:37] + "..."
        
        # Get actual file size
        file_size = os.path.getsize(png_file)
        total_file_size += file_size
        
        # Calculate uncompressed RGBA size (4 bytes per pixel)
        try:
            with Image.open(png_file) as img:
                width, height = img.size
                rgba_size = width * height * 4  # 4 bytes per pixel (RGBA)
                pages_needed = (rgba_size + page_size - 1) // page_size
                address = hex(startaddress + pages_needed * page_size)
                startaddress += pages_needed * page_size
                total_rgba_size += rgba_size
                
                ratio = rgba_size / file_size if file_size > 0 else 0
                
                print("| {:<40} | {:>10} | {:>10} | {:>10.2f} | {:<10}".format(
                    display_name, 
                    format_size(file_size), 
                    format_size(rgba_size),
                    ratio,
                    address
                ))
        except Exception as e:
            print(f"Error processing {png_file}: {e}")
    
    print("-" * 80)
    print("| {:<40} | {:>10} | {:>10} | {:>10.2f} |".format(
        "TOTAL", 
        format_size(total_file_size), 
        format_size(total_rgba_size),
        total_rgba_size / total_file_size if total_file_size > 0 else 0
    ))
    print("-" * 80)
    
    print(f"\nSummary:")
    print(f"- Total PNG file size: {format_size(total_file_size)}")
    print(f"- Uncompressed RGBA size: {format_size(total_rgba_size)}")
    print(f"- Space saved by PNG compression: {format_size(total_rgba_size - total_file_size)}")
    print(f"- Compression ratio: {total_rgba_size / total_file_size:.2f}x")
    
    if total_rgba_size > 1024 * 1024 * 1024:  # If more than 1GB
        print(f"\nNOTE: If stored uncompressed, these images would require {total_rgba_size / (1024*1024*1024):.2f} GB of memory!")

def format_size(size_in_bytes):
    """Format file size in human-readable format"""
    if size_in_bytes < 1024:
        return f"{size_in_bytes} B"
    elif size_in_bytes < 1024 * 1024:
        return f"{size_in_bytes / 1024:.1f} KB"
    elif size_in_bytes < 1024 * 1024 * 1024:
        return f"{size_in_bytes / (1024 * 1024):.1f} MB"
    else:
        return f"{size_in_bytes / (1024 * 1024 * 1024):.2f} GB"

if __name__ == "__main__":
    if len(sys.argv) > 1:
        directory_path = sys.argv[1]
    else:
        directory_path = input("Enter the path to the directory containing PNG files: ")
    
    analyze_png_directory(directory_path)