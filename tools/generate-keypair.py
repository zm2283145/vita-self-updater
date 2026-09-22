#!/usr/bin/env python3
"""Generate an Ed25519 release-signing keypair."""

from __future__ import annotations

import argparse
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--private-key", required=True, type=Path)
    parser.add_argument("--public-key-hex", required=True, type=Path)
    args = parser.parse_args()
    if args.private_key.exists() or args.public_key_hex.exists():
        raise FileExistsError("refusing to overwrite an existing key")

    private_key = Ed25519PrivateKey.generate()
    args.private_key.write_bytes(
        private_key.private_bytes(
            serialization.Encoding.PEM,
            serialization.PrivateFormat.PKCS8,
            serialization.NoEncryption(),
        )
    )
    public_key = private_key.public_key().public_bytes(
        serialization.Encoding.Raw,
        serialization.PublicFormat.Raw,
    )
    args.public_key_hex.write_text(public_key.hex() + "\n", encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
