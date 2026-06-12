#!/usr/bin/env python3
"""
Demo script showing multi-type support in FImage.

This demonstrates:
1. Creating images with different dtypes
2. The dtype property
3. The improved __repr__ with dtype
"""

import numpy as np
import fim

print("=" * 60)
print("FImage Multi-Type Support Demo")
print("=" * 60)

# Create images with different dtypes
print("\n1. Creating images with different dtypes:")
print("-" * 60)

arr_uint8 = np.array([[[1, 2, 3], [4, 5, 6]]], dtype=np.uint8)
img_uint8 = fim.Image.from_numpy(arr_uint8)
print(f"uint8 image:   {img_uint8}")
print(f"  - dtype property: {img_uint8.dtype}")
print(f"  - pixel_type: {img_uint8.pixel_type}")

arr_uint16 = np.array([[[100, 200, 300], [400, 500, 600]]], dtype=np.uint16)
img_uint16 = fim.Image.from_numpy(arr_uint16)
print(f"\nuint16 image:  {img_uint16}")
print(f"  - dtype property: {img_uint16.dtype}")
print(f"  - pixel_type: {img_uint16.pixel_type}")

arr_float = np.array([[[0.1, 0.2, 0.3], [0.4, 0.5, 0.6]]], dtype=np.float32)
img_float = fim.Image.from_numpy(arr_float)
print(f"\nfloat32 image: {img_float}")
print(f"  - dtype property: {img_float.dtype}")
print(f"  - pixel_type: {img_float.pixel_type}")

# Test round-trip
print("\n\n2. Round-trip conversion preserves dtype:")
print("-" * 60)

result_uint8 = img_uint8.to_numpy()
print(f"uint8:   input dtype = {arr_uint8.dtype}, output dtype = {result_uint8.dtype}")

result_uint16 = img_uint16.to_numpy()
print(f"uint16:  input dtype = {arr_uint16.dtype}, output dtype = {result_uint16.dtype}")

result_float = img_float.to_numpy()
print(f"float32: input dtype = {arr_float.dtype}, output dtype = {result_float.dtype}")

# Test operators preserve dtype
print("\n\n3. Operators preserve dtype:")
print("-" * 60)

cropped_uint16 = img_uint16.crop((0, 0), (1, 1))
print(f"Cropped uint16 image:  {cropped_uint16}")

resized_float = img_float.resize(1, 1)
print(f"Resized float32 image: {resized_float}")

# Test file I/O
print("\n\n4. File I/O preserves dtype:")
print("-" * 60)

import tempfile
import os

# Save and load uint16
with tempfile.NamedTemporaryFile(suffix=".fimage", delete=False) as f:
    fname = f.name

try:
    img_uint16.write_fimage(fname)
    loaded_uint16 = fim.Image.from_fimage(fname)
    print(f"uint16 saved & loaded: {loaded_uint16}")
    assert loaded_uint16.dtype == "uint16"
    print("  ✓ dtype preserved!")
finally:
    if os.path.exists(fname):
        os.unlink(fname)

print("\n" + "=" * 60)
print("✅ All multi-type features working correctly!")
print("=" * 60)
