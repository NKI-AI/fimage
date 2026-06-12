#!/usr/bin/env python3
"""Minimal test to debug fastslide loading."""

import sys

print("Starting...", flush=True)

import fim

print("Imported fim", flush=True)

if len(sys.argv) < 2:
    print("Usage: test_fastslide_minimal.py <slide_path>")
    sys.exit(1)

slide_path = sys.argv[1]
print(f"Loading: {slide_path}", flush=True)

try:
    img = fim.Image.from_fastslide(slide_path, level=0)
    print("Loaded successfully", flush=True)

    dims = img.dimensions
    print(f"Dimensions: {dims}", flush=True)

    print("Success!", flush=True)
except Exception as e:
    print(f"Error: {e}", flush=True)
    import traceback

    traceback.print_exc()
    sys.exit(1)
