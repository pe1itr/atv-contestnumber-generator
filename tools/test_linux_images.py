"""Run the renderer integration path without retaining the generated JPEGs."""
import subprocess
import tempfile
import sys

with tempfile.TemporaryDirectory(prefix="atv-jpg-check-") as directory:
    subprocess.run(["dist/atv-contestnummer", "--smoke-test", directory], check=True)
    subprocess.run([sys.executable, "checks/check_images.py", directory], check=True)
