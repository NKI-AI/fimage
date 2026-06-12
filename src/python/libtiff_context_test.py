#!/usr/bin/env python3
"""Tests for fim libtiff context manager and TIFF properties."""

from __future__ import annotations

from collections.abc import Iterator
import os
import tempfile

import numpy as np
import pytest

import fim


@pytest.fixture
def temp_tiff_path() -> Iterator[str]:
    """Create a temporary TIFF path."""
    fd, path = tempfile.mkstemp(suffix=".tiff")
    os.close(fd)
    yield path
    if os.path.exists(path):
        os.remove(path)


def test_open_libtiff_properties_types(temp_tiff_path: str) -> None:
    """`open_libtiff` should expose typed properties and per-page access."""
    data = np.zeros((16, 32, 3), dtype=np.uint8)
    img = fim.Image.from_numpy(data)
    img.write_tiff(temp_tiff_path)

    with fim.open_libtiff(temp_tiff_path) as t:
        props = t.properties
        assert isinstance(props["num_pages"], int)
        assert isinstance(props["x_res"], float)
        assert isinstance(props["y_res"], float)

        assert props["num_pages"] == t.level_count
        assert len(t.level_dimensions) == t.level_count
        assert t.level_dimensions[0] == (32, 16)

        page0 = t.at_level(0)
        assert page0.dimensions == (32, 16, 3)


def test_image_from_tiff_has_properties(temp_tiff_path: str) -> None:
    """`Image.from_tiff` should expose `.properties` for libtiff metadata."""
    data = np.zeros((8, 9, 1), dtype=np.uint8)
    img = fim.Image.from_numpy(data)
    img.write_tiff(temp_tiff_path)

    img2 = fim.Image.from_libtiff(temp_tiff_path, page=0)
    props = img2.properties
    assert isinstance(props["num_pages"], int)
    assert isinstance(props["x_res"], float)
    assert isinstance(props["y_res"], float)
    assert props["num_pages"] >= 1
