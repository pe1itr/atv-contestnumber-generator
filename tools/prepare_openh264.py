"""Fetch the pinned source archive once; prepare separate native/cross builds."""
import hashlib
import os
from pathlib import Path
import sys
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
VERSION = "2.6.0"
SHA256 = "558544ad358283a7ab2930d69a9ceddf913f4a51ee9bf1bfb9e377322af81a69"
platform = sys.argv[1]
if platform not in ("linux", "windows"):
    raise SystemExit("Expected linux or windows")
archive = ROOT / "build" / "downloads" / f"openh264-{VERSION}.tar.gz"
archive.parent.mkdir(parents=True, exist_ok=True)
if not archive.exists():
    temporary = archive.with_suffix(f".{platform}.download")
    try:
        urllib.request.urlretrieve(
            f"https://github.com/cisco/openh264/archive/refs/tags/v{VERSION}.tar.gz", temporary
        )
        if hashlib.sha256(temporary.read_bytes()).hexdigest() != SHA256:
            raise SystemExit("OpenH264 archive checksum mismatch")
        temporary.replace(archive)
    finally:
        temporary.unlink(missing_ok=True)
if hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
    raise SystemExit("OpenH264 archive checksum mismatch")
destination = ROOT / "build" / f"openh264-{platform}"
destination.mkdir(parents=True, exist_ok=True)
if not (destination / "Makefile").exists():
    with tarfile.open(archive) as source:
        prefix = f"openh264-{VERSION}/"
        for member in source.getmembers():
            if not member.name.startswith(prefix):
                continue
            member.name = member.name[len(prefix):]
            if member.name:
                source.extract(member, destination, filter="data")
os.utime(destination / "Makefile", None)
