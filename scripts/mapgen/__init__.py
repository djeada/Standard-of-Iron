"""Seeded battlefield generator for Standard of Iron maps.

The package authors the intent the map pipeline already consumes - hills,
river guides, settlement anchors, road endpoints, landmarks, dressing - and
runs the existing materialisers over it. See docs/MAP_GENERATOR.md for the
contract the editor and the tests code against.
"""

GENERATOR = "soi_mapgen"
GENERATOR_VERSION = 1
