#!/usr/bin/env python3
"""Compile an authoring glTF into a deterministic internal creature package."""

from __future__ import annotations

import argparse
import json
import pathlib
import struct
import zlib


MAGIC = b"SOICM001"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--species", choices=("horse", "elephant"), required=True)
    return parser.parse_args()


def sanitize(document: dict, species: str) -> dict:
    document["asset"] = {
        "version": "2.0",
        "generator": "Standard of Iron creature compiler",
    }
    document["scenes"][document.get("scene", 0)]["name"] = f"{species}_production"
    for index, mesh in enumerate(document.get("meshes", [])):
        mesh["name"] = f"{species}_production_mesh_{index}"
    for index, material in enumerate(document.get("materials", [])):
        material["name"] = f"{species}_production_material_{index}"
    for container_name in ("nodes", "skins", "animations"):
        for item in document.get(container_name, []):
            item.pop("extras", None)
    return document


def main() -> None:
    args = parse_args()
    document = json.loads(args.input.read_text(encoding="utf-8"))
    payload = json.dumps(
        sanitize(document, args.species),
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    package = MAGIC + struct.pack(">I", len(payload)) + zlib.compress(payload, 9)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(package)


if __name__ == "__main__":
    main()
