#!/usr/bin/env python3
"""Convert a GLB into a single-file, base64-buffer glTF without changing data."""

import argparse
import base64
import json
import pathlib
import struct


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    blob = args.input.read_bytes()
    magic, version, declared_length = struct.unpack_from("<III", blob)
    if magic != 0x46546C67 or version != 2 or declared_length != len(blob):
        raise SystemExit("input is not a valid glTF 2.0 GLB")

    offset = 12
    chunks: dict[int, bytes] = {}
    while offset < len(blob):
        length, kind = struct.unpack_from("<II", blob, offset)
        offset += 8
        chunks[kind] = blob[offset : offset + length]
        offset += length
    document = json.loads(chunks[0x4E4F534A].decode("utf-8"))
    binary = chunks.get(0x004E4942, b"")
    document["buffers"][0]["uri"] = (
        "data:application/octet-stream;base64,"
        + base64.b64encode(binary).decode("ascii")
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, separators=(",", ":")))


if __name__ == "__main__":
    main()
