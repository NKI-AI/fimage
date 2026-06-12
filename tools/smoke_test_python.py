#!/usr/bin/env python3
"""On-target smoke test for an installed fimage (fim) Python wheel.

Run this with the interpreter of a venv that has the wheel installed (NOT from
the repo's ``python/`` source tree), e.g. in CI after
``uv pip install --find-links dist fimage-python``. It imports ``fim`` (which
loads the native extension via ``fim._fim``), builds a small in-memory image,
runs a lazy crop/downsample pipeline, and materializes it to a NumPy array --
proving the wheel's native library loads and executes on the real
OS/arch/Python of the runner.
"""

from __future__ import annotations

import sys


def main() -> int:
    # Imported from the installed wheel in the CI venv, not the repo source tree.
    import fim  # type: ignore  # pylint: disable=import-error,import-outside-toplevel

    version = getattr(fim, "__version__", "<unknown>")
    print(f"\u25b6\ufe0e Imported fim {version}")
    print(f"  interpreter: {sys.executable}")
    print(f"  python:      {sys.version.split()[0]} ({sys.implementation.name})")

    # Build a 16x16 RGB image in memory and run a tiny lazy pipeline.
    width, height, channels = 16, 16, 3
    data = [200] * (width * height * channels)
    img = fim.Image.from_memory(data, width, height, channels)
    if not img.is_valid():
        print("\u2717 from_memory produced an invalid image")
        return 1

    result = img.crop((0, 0), (8, 8)).downsample(2)
    array = result.to_numpy()
    print(f"\u2714 Ran crop->downsample pipeline; output array shape = {array.shape}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
