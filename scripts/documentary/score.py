"""The extended series score (#1537): plan, generation requests, stem checks.

Nothing here calls a music service. ``data/score_plan.json`` holds each
battle's palette and the cue template; this module expands it into the full
cue list (bed, tension, battle, climax, elegy... for all nine episodes), turns
that into a request manifest the author can feed to the music service, and,
once renders come back as stems, checks and loudness-normalises them.

Stems for a cue live in one folder::

    <stems>/ep07_battle_a/stems.json            metadata (data/stem_schema.json)
    <stems>/ep07_battle_a/ep07_battle_a.percussion.wav
    <stems>/ep07_battle_a/ep07_battle_a.strings.wav ...

``normalize-stems`` applies one common gain to every stem of a cue so their
*sum* sits at ``loudness.cue_lufs`` with the balance between stems untouched,
and records each stem's own loudness and true peak. ``validate-stems`` checks
the metadata against the plan (tempo, key, required stems), the files against
each other (rate, length) and the measured levels against the metadata.

``check-reuse`` reads episode edits and refuses any stretch of the same source
used twice in the series for more than ``REUSE_LIMIT`` seconds, except cues
the plan marks as the series' signature (the title ident).
"""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import DATA, EditError, media

REUSE_LIMIT = 10.0
NEGATIVE = (
    "no vocals with words, no lyrics, no modern drum kit, no synthesisers, no electric guitar, "
    "no piano ballad, no hybrid trailer braams, no sad solo piano"
)
ENDINGS = {
    "bed": "loopable, no hard ending",
    "tension": "loopable, can be cut at any bar",
    "battle": "ends on a hit",
    "climax": "opens from silence and ends on a sustained chord",
    "elegy": "fades out",
    "theme": "ends on a final hit",
    "reel": "ends on a hit at the last bar",
}


def load_plan(path: Path | None = None) -> dict:
    return json.loads((path or DATA / "score_plan.json").read_text())


def expand(plan: dict) -> list[dict]:
    """Every cue of the series, fully specified."""
    cues: list[dict] = []
    roles = plan["roles"]
    for cue in plan.get("series_cues", []):
        role = roles[cue["role"]]
        cues.append(
            {
                "episode": "series",
                "battle": "series",
                "stems": role["stems"],
                "under_vo": role["under_vo"],
                "intensity": role["intensity"],
                "colour": "the series' iron-and-ember identity",
                "instruments": "war drums, low strings, brass, full choir",
                **cue,
            }
        )
    for episode in plan["episodes"]:
        palette = episode["palette"]
        template = plan["episode_template"] + episode.get("extra_cues", [])
        for item in template:
            role = roles[item["role"]]
            cue_id = f"{episode['id']}_{item['cue']}"
            cues.append(
                {
                    "id": cue_id,
                    "episode": episode["id"],
                    "battle": episode["battle"],
                    "year": episode["year"],
                    "role": item["role"],
                    "section": item["section"],
                    "seconds": item["seconds"],
                    "tempo": palette["tempo"],
                    "key": palette["key"],
                    "colour": palette["colour"],
                    "instruments": palette["instruments"],
                    "library_reference": palette.get("library", []),
                    "theme": item.get("theme"),
                    "stems": role["stems"],
                    "under_vo": role["under_vo"],
                    "intensity": role["intensity"],
                    "notes": episode.get("cue_notes", {}).get(item["cue"], ""),
                    "signature": False,
                }
            )
    ids = [c["id"] for c in cues]
    if len(ids) != len(set(ids)):
        raise EditError("score plan has duplicate cue ids")
    return cues


def summary(cues: list[dict]) -> dict:
    by_role: dict[str, int] = {}
    by_episode: dict[str, dict] = {}
    for cue in cues:
        by_role[cue["role"]] = by_role.get(cue["role"], 0) + 1
        entry = by_episode.setdefault(cue["episode"], {"cues": 0, "seconds": 0})
        entry["cues"] += 1
        entry["seconds"] += cue["seconds"]
    return {
        "cues": len(cues),
        "seconds": sum(c["seconds"] for c in cues),
        "stems": sum(len(c["stems"]) for c in cues),
        "by_role": by_role,
        "by_episode": by_episode,
    }


def prompt(cue: dict) -> str:
    theme = ""
    if cue.get("theme") == "kingdom_of_iron":
        theme = " A variation on the series theme, Kingdom of Iron (reference: menu/main_theme_iron_kingdom.ogg)."
    where = f"for a history documentary about {cue['battle']} ({cue.get('year', '')})".replace(
        " ()", ""
    )
    note = f" Moment: {cue['notes']}." if cue.get("notes") else ""
    under = (
        " It sits under narration, so keep the mid-range clear for a voice."
        if cue["under_vo"]
        else ""
    )
    return (
        f"Orchestral score cue, {cue['role']}, {where}. {cue['intensity'].capitalize()}. "
        f"Colour: {cue['colour']}. Instrumentation: {cue['instruments']}. "
        f"{cue['key']}, {cue['tempo']} BPM, {cue['seconds']} seconds, {ENDINGS[cue['role']]}."
        f"{note}{theme}{under} Instrumental only."
    )


def manifest(cues: list[dict], plan: dict) -> dict:
    """Generation requests, one per cue, in the order they are needed (ep07 first)."""
    order = {"ep07": 0, "series": 1}
    requests = []
    for cue in sorted(
        cues, key=lambda c: (order.get(c["episode"], 2), c["episode"], c["id"])
    ):
        requests.append(
            {
                "request_id": f"soi-doc-{cue['id']}",
                "cue": cue["id"],
                "render_name": f"SOI_Doc_{cue['id']}_v1",
                "episode": cue["episode"],
                "role": cue["role"],
                "seconds": cue["seconds"],
                "tempo_bpm": cue["tempo"],
                "key": cue["key"],
                "stems": cue["stems"],
                "prompt": prompt(cue),
                "negative": NEGATIVE,
                "reference_tracks": cue.get("library_reference", []),
                "deliver_as": f"{cue['id']}/{cue['id']}.<stem>.wav, 48 kHz 24-bit stereo, plus the full mix",
                "loudness_target_lufs": plan["loudness"]["cue_lufs"],
            }
        )
    return {
        "schema": "soi-documentary-score-requests/1",
        "service": plan["service"],
        "status": "not generated; feed these to the music service by hand",
        "count": len(requests),
        "requests": requests,
    }


def _check(value, schema: dict, where: str, problems: list[str]) -> None:
    """The subset of JSON Schema used by ``stem_schema.json``."""
    kind = schema.get("type")
    types = {"object": dict, "string": str, "number": (int, float), "array": list}
    if kind and not isinstance(value, types[kind]):
        problems.append(f"{where}: expected {kind}")
        return
    if "const" in schema and value != schema["const"]:
        problems.append(f"{where}: must be {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        problems.append(f"{where}: {value!r} not one of {schema['enum']}")
    if (
        "pattern" in schema
        and isinstance(value, str)
        and not re.search(schema["pattern"], value)
    ):
        problems.append(f"{where}: {value!r} does not match {schema['pattern']}")
    if isinstance(value, (int, float)):
        if "minimum" in schema and value < schema["minimum"]:
            problems.append(f"{where}: below {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            problems.append(f"{where}: above {schema['maximum']}")
        if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
            problems.append(f"{where}: must be above {schema['exclusiveMinimum']}")
    if isinstance(value, dict):
        for key in schema.get("required", []):
            if key not in value:
                problems.append(f"{where}: missing '{key}'")
        if "minProperties" in schema and len(value) < schema["minProperties"]:
            problems.append(
                f"{where}: needs at least {schema['minProperties']} entries"
            )
        names = schema.get("propertyNames")
        for key, item in value.items():
            if names:
                _check(key, names, f"{where}.{key} (name)", problems)
            sub = schema.get("properties", {}).get(key) or schema.get(
                "additionalProperties"
            )
            if isinstance(sub, dict):
                _check(item, sub, f"{where}.{key}", problems)


def validate_meta(meta: dict, cue: dict | None) -> list[str]:
    schema = json.loads((DATA / "stem_schema.json").read_text())
    problems: list[str] = []
    _check(meta, schema, "stems.json", problems)
    if cue is None:
        problems.append(f"cue '{meta.get('cue')}' is not in the score plan")
        return problems
    if (
        meta.get("tempo_bpm") is not None
        and abs(float(meta["tempo_bpm"]) - float(cue["tempo"])) > 6
    ):
        problems.append(
            f"tempo {meta['tempo_bpm']} BPM is far from the plan's {cue['tempo']}"
        )
    if meta.get("key") and meta["key"].lower() != str(cue["key"]).lower():
        problems.append(f"key {meta['key']} differs from the plan's {cue['key']}")
    missing = [s for s in cue["stems"] if s not in meta.get("stems", {})]
    if missing:
        problems.append(f"missing stems {missing} the plan asks for")
    return problems


def _sum_loudness(paths: list[Path], gain_db: float = 0.0) -> dict:
    inputs: list[str] = []
    for path in paths:
        inputs += ["-i", str(path)]
    labels = "".join(f"[{i}:a]" for i in range(len(paths)))
    out = media.ffmpeg(
        [
            *inputs,
            "-filter_complex",
            f"{labels}amix=inputs={len(paths)}:normalize=0,volume={gain_db:.3f}dB,ebur128=peak=true[o]",
            "-map",
            "[o]",
            "-f",
            "null",
            "-",
        ],
        "stem sum loudness",
    )
    summary_text = out[out.rfind("Summary:") :]
    i = re.findall(r"I:\s+(-?[0-9.]+) LUFS", summary_text)
    p = re.findall(r"Peak:\s+(-?[0-9.]+|-inf) dBFS", summary_text)
    return {
        "integrated_lufs": float(i[-1]) if i else -70.0,
        "true_peak_dbtp": float(p[-1]) if p and "inf" not in p[-1] else -70.0,
    }


def cue_folders(root: Path) -> list[Path]:
    return sorted(p.parent for p in root.glob("*/stems.json"))


def normalize(folder: Path, out_root: Path, plan: dict) -> dict:
    """Common gain so the stems' sum hits the cue target; never above the peak ceiling."""
    meta = json.loads((folder / "stems.json").read_text())
    target = float(plan["loudness"]["cue_lufs"])
    ceiling = float(plan["loudness"]["true_peak_dbtp"])
    files = {name: folder / entry["file"] for name, entry in meta["stems"].items()}
    measured = _sum_loudness(list(files.values()))
    gain = target - measured["integrated_lufs"]
    headroom = ceiling - 0.2 - (measured["true_peak_dbtp"] + gain)
    limited = headroom < 0
    if limited:
        gain += headroom
    out = out_root / folder.name
    out.mkdir(parents=True, exist_ok=True)
    for name, path in files.items():
        target_path = out / path.name
        media.ffmpeg(
            [
                "-i",
                str(path),
                "-af",
                f"volume={gain:.3f}dB",
                "-ar",
                "48000",
                "-c:a",
                "pcm_s24le",
                str(target_path),
            ],
            f"normalising {path.name}",
        )
        level = media.loudness(target_path)
        meta["stems"][name].update(
            {
                "file": target_path.name,
                "lufs": round(level["integrated_lufs"], 2),
                "true_peak_dbtp": round(level["true_peak_dbtp"], 2),
            }
        )
    after = _sum_loudness([out / meta["stems"][n]["file"] for n in files])
    meta["mix_lufs"] = round(after["integrated_lufs"], 2)
    meta["normalised"] = {"gain_db": round(gain, 2), "peak_limited": limited}
    (out / "stems.json").write_text(json.dumps(meta, indent=1))
    return {
        "cue": meta["cue"],
        "gain_db": round(gain, 2),
        "mix_lufs": meta["mix_lufs"],
        "peak_limited": limited,
    }


def validate(folder: Path, plan: dict, cues: dict[str, dict]) -> list[str]:
    meta = json.loads((folder / "stems.json").read_text())
    problems = validate_meta(meta, cues.get(meta.get("cue")))
    loud = plan["loudness"]
    lengths = {}
    paths = []
    for name, entry in meta.get("stems", {}).items():
        path = folder / entry["file"]
        if not path.exists():
            problems.append(f"{name}: {entry['file']} is missing")
            continue
        info = media.probe(path)
        if info.get("sample_rate") != loud["sample_rate"]:
            problems.append(
                f"{name}: {info.get('sample_rate')} Hz, need {loud['sample_rate']}"
            )
        lengths[name] = info["duration"]
        paths.append(path)
        level = media.loudness(path)
        if level["true_peak_dbtp"] > loud["true_peak_dbtp"] + 0.1:
            problems.append(f"{name}: true peak {level['true_peak_dbtp']} dBTP")
        if "lufs" in entry and abs(entry["lufs"] - level["integrated_lufs"]) > 0.5:
            problems.append(
                f"{name}: metadata says {entry['lufs']} LUFS, measured {level['integrated_lufs']}"
            )
    if lengths and max(lengths.values()) - min(lengths.values()) > 0.01:
        problems.append(f"stems differ in length: {lengths}")
    if lengths and abs(max(lengths.values()) - float(meta.get("duration_s", 0))) > 0.05:
        problems.append(f"duration_s {meta.get('duration_s')} does not match the audio")
    if paths:
        total = _sum_loudness(paths)
        if abs(total["integrated_lufs"] - loud["cue_lufs"]) > loud["tolerance_lu"]:
            problems.append(
                f"stem sum at {total['integrated_lufs']} LUFS, target {loud['cue_lufs']} (run normalize-stems)"
            )
    return problems


def passages(episode, timeline) -> list[dict]:
    """Every stretch of source audio a cut uses: file, source window, where it came from."""
    out = []
    for span in timeline.music:
        data = span.data
        files = list(data["stems"].values()) if "stems" in data else [data["file"]]
        for name in files:
            start = float(data.get("src_in", 0.0))
            out.append(
                {
                    "file": str(episode.resolve(name)),
                    "src": (start, start + span.dur),
                    "episode": episode.id,
                    "cue": span.id,
                    "plan_cue": data.get("plan_cue", ""),
                }
            )
    return out


def reuse(
    passages_list: list[dict], signatures: set[str], limit: float = REUSE_LIMIT
) -> list[str]:
    problems = []
    for i, a in enumerate(passages_list):
        for b in passages_list[i + 1 :]:
            if (
                a["file"] != b["file"]
                or a["plan_cue"] in signatures
                and b["plan_cue"] in signatures
            ):
                continue
            shared = min(a["src"][1], b["src"][1]) - max(a["src"][0], b["src"][0])
            if shared > limit:
                problems.append(
                    f"{Path(a['file']).name} {max(a['src'][0], b['src'][0]):.0f}-{min(a['src'][1], b['src'][1]):.0f}s "
                    f"is used by {a['episode']}:{a['cue']} and {b['episode']}:{b['cue']} ({shared:.0f}s)"
                )
    return problems


PROVENANCE_STEPS = """\
After a batch of renders comes back:
1. Put each cue's stems in <stems>/<cue>/ with stems.json (data/stem_schema.json), keeping
   the service's render name in source.render and the request_id from the manifest.
2. python3 scripts/documentary score normalize-stems <stems> --out <stems-normalised>
3. python3 scripts/documentary score validate-stems <stems-normalised>
4. Record the batch in scripts/documentary/data/score_provenance.json (render name -> cue,
   date, licence), mirroring BATCHES in tools/audio_import/import_music.py.
5. The stems stay outside the repository (they are not game assets). Only if a cue is also
   shipped in the game: import it with tools/audio_import/import_music.py, give it a
   provenance block in assets/audio/audio_manifest.json, re-render docs/AUDIO_LICENSES.md
   with scripts/audio_provenance.py --doc, and extend the Music section of
   THIRD_PARTY_LICENSES.md (track count, batch, licence).
6. Every episode description carries the AI-audio disclosure (delivery.py does this).
"""
