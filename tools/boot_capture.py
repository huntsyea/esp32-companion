"""Hard-reset the board with esptool, then immediately capture the boot log.

esptool's reset sequence is more reliable than a bare RTS pulse on the
ESP32-S3's internal USB-Serial/JTAG bridge, so we shell out to it and reopen
the port as soon as it releases the device.
"""

import subprocess
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem3101"
SECONDS = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0

subprocess.run(
    ["uvx", "--from", "esptool", "esptool", "--port", PORT, "--after", "hard-reset",
     "--no-stub", "read-mac"],
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
    check=False,
)

port = serial.Serial(PORT, 115200, timeout=0.1)
chunks = []
deadline = time.time() + SECONDS
while time.time() < deadline:
    chunks.append(port.read(4096))
port.close()

data = b"".join(chunks)
print(f"captured {len(data)} bytes\n", flush=True)
sys.stdout.write(data.decode("utf-8", "replace"))
