#!/usr/bin/env python3

import configparser
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image


SCRIPT = Path(__file__).resolve().parents[1] / "scripts" / "package-dunecity-skin.py"
SPEC = importlib.util.spec_from_file_location("package_dunecity_skin", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class DuneCitySkinPackagingTests(unittest.TestCase):
    def test_high_detail_compact_keeps_pixels_and_logical_footprint(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset_root = root / "dune2"
            unit = asset_root / "units" / "dunecity_harkonnen_residential_zone"
            compact = unit / "categories" / "building_idle" / "states" / "d0_v0" / "processed.png"
            compact.parent.mkdir(parents=True)
            Image.new("RGBA", (64, 64), (100, 80, 40, 255)).save(compact)
            metadata = {
                "target_game": "dunecity",
                "slug": "dunecity_harkonnen_residential_zone",
                "dunecity": {
                    "compact_pixels_per_tile": 32,
                    "zone_atlas": {"density_columns": 4, "value_tier_rows": 4},
                },
                "render_profile": {
                    "logical_footprint_tiles": [2, 2],
                    "compact_frame_pixels": [64, 64],
                },
                "categories": {
                    "building_idle": {
                        "states": {
                            "d0_v0": {
                                "assets": {
                                    "processed": {
                                        "file": compact.relative_to(asset_root).as_posix(),
                                    }
                                }
                            }
                        }
                    }
                },
            }
            (unit / "unit.json").write_text(json.dumps(metadata), encoding="utf-8")
            output = root / "output"

            self.assertEqual(MODULE.package(unit, output, 20, 0), 1)

            with Image.open(output / "atlases" / "idle" / "d0_v0" / "00.png") as packaged:
                self.assertEqual(packaged.size, (64, 64))
            manifest = configparser.ConfigParser()
            manifest.optionxform = str
            manifest.read(output / "zone.ini", encoding="ascii")
            self.assertEqual(manifest.getint("Zone", "FootprintWidth"), 2)
            self.assertEqual(manifest.getint("Zone", "FootprintHeight"), 2)
            self.assertEqual(manifest.getint("Render", "PixelsPerTile"), 32)
            self.assertEqual(manifest.getint("Render", "LogicalPixelsPerTile"), 16)
            self.assertEqual(manifest.getint("Cell.0.0.Idle", "FrameWidth"), 64)
            self.assertEqual(manifest.getint("Cell.0.0.Idle", "AnchorX"), 32)
            self.assertEqual(manifest.getint("Cell.0.0.Idle", "AnchorY"), 64)

    def test_high_detail_special_building_frames_are_copied_verbatim(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset_root = root / "dune2"
            unit = asset_root / "units" / "dunecity_harkonnen_stadium"
            compact = unit / "categories" / "building_idle" / "states" / "frame_0" / "processed.png"
            compact.parent.mkdir(parents=True)
            Image.new("RGBA", (192, 192), (120, 60, 30, 255)).save(compact)
            metadata = {
                "target_game": "dunecity",
                "slug": "dunecity_harkonnen_stadium",
                "categories": {
                    "building_idle": {
                        "states": {
                            "frame_0": {
                                "assets": {
                                    "processed": {
                                        "file": compact.relative_to(asset_root).as_posix(),
                                    }
                                }
                            }
                        }
                    }
                },
            }
            (unit / "unit.json").write_text(json.dumps(metadata), encoding="utf-8")
            output = root / "building-output"

            self.assertEqual(MODULE.package_building(unit, output, "Stadium", 0), 1)

            with Image.open(output / "frames" / "00_frame_0.png") as packaged:
                self.assertEqual(packaged.size, (192, 192))
            manifest = configparser.ConfigParser()
            manifest.optionxform = str
            manifest.read(output / "building.ini", encoding="ascii")
            self.assertEqual(manifest.getint("Building", "Frames"), 1)
            self.assertEqual(manifest.get("Building", "ObjPic"), "Stadium")
            self.assertEqual(manifest.get("Frame.0", "SourceSlot"), "frame_0")


if __name__ == "__main__":
    unittest.main()
