import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "companion" / "production-baseline.json"


class CompanionBaselineContractTests(unittest.TestCase):
    def test_manifest_pins_upstream_and_board(self):
        manifest = json.loads(BASELINE.read_text(encoding="utf-8"))

        self.assertEqual(manifest["schema_version"], 1)
        self.assertEqual(
            manifest["xiaozhi"]["revision"],
            "8e2899dbc9249d9961b6dafc0c59f7bd7e72644d",
        )
        self.assertEqual(
            manifest["board"]["directory"],
            "waveshare/esp32-s3-touch-lcd-1.54",
        )
        self.assertEqual(
            manifest["board"]["variant"], "esp32-s3-touch-lcd-1.54"
        )
        self.assertEqual(manifest["board"]["target"], "esp32s3")

    def test_simulator_and_device_share_product_ui_source(self):
        product_ui = ROOT / "main" / "product_ui" / "product_ui.c"
        simulator_cmake = (ROOT / "simulator" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        device_cmake = (ROOT / "main" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )

        self.assertTrue(product_ui.is_file())
        self.assertIn("../main/product_ui/product_ui.c", simulator_cmake)
        self.assertIn('"product_ui/product_ui.c"', device_cmake)


if __name__ == "__main__":
    unittest.main()
