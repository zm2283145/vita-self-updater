#!/usr/bin/env python3
"""End-to-end tests for the release key and manifest tools."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        private_key = directory / "release.pem"
        public_key = directory / "release-public.hex"
        asset = directory / "ExampleApp.vpk"
        manifest = directory / "vita-update-manifest.txt"
        asset.write_bytes(b"test VPK bytes")

        subprocess.run(
            [
                sys.executable,
                str(root / "tools/generate-keypair.py"),
                "--private-key",
                str(private_key),
                "--public-key-hex",
                str(public_key),
            ],
            check=True,
        )
        subprocess.run(
            [
                sys.executable,
                str(root / "tools/sign-manifest.py"),
                "--version",
                "1.2.3",
                "--asset",
                str(asset),
                "--private-key",
                str(private_key),
                "--output",
                str(manifest),
            ],
            check=True,
        )

        lines = manifest.read_bytes().splitlines(keepends=True)
        assert len(lines) == 6
        assert lines[0] == b"format=1\n"
        assert lines[1] == b"version=1.2.3\n"
        signature = bytes.fromhex(lines[5].decode("ascii").removeprefix("signature=").strip())
        public = Ed25519PublicKey.from_public_bytes(
            bytes.fromhex(public_key.read_text(encoding="ascii").strip())
        )
        public.verify(signature, b"".join(lines[:5]))

        duplicate = subprocess.run(
            [
                sys.executable,
                str(root / "tools/generate-keypair.py"),
                "--private-key",
                str(private_key),
                "--public-key-hex",
                str(public_key),
            ],
            capture_output=True,
            text=True,
        )
        assert duplicate.returncode != 0

    print("PASS: release signing tools")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
