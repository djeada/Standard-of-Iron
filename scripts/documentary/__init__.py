"""Post-production tooling for *The Barcid Road: Hannibal's War* (epic #1547).

An episode is one JSON edit file (see ``docs/DOCUMENTARY.md``): sections,
shots cut from arena captures, numbered narration paragraphs, music cues,
graphics and chapters. Everything downstream -- the VO bus and its subtitles,
the series graphics, the 4K picture, the mix, the upload package and the
vertical reel -- is rebuilt from that file and the recordings it names, so a
new narration take re-times the whole episode.

Run it as ``python3 scripts/documentary <command>``; ``--help`` lists the
commands. Other tooling imports the series look from
``documentary.style`` after putting ``scripts/`` on ``sys.path``.
"""

from __future__ import annotations

from pathlib import Path

PACKAGE = Path(__file__).resolve().parent
REPO = PACKAGE.parents[1]
DATA = PACKAGE / "data"
SAMPLES = PACKAGE / "samples"


class EditError(Exception):
    """A problem with an edit, its inputs or a delivery check, worded for the editor."""
