"""Run the existing renderer integration path without retaining 680 JPEGs."""
import subprocess
import tempfile
import sys

with tempfile.TemporaryDirectory(prefix="atv-jpg-check-") as directory:
    subprocess.run(["dist/atv-contestnummer", "--smoke-test", directory], check=True)
    subprocess.run([sys.executable, "checks/check_images.py", directory], check=True)
