#!/usr/bin/env python3
"""Extract player-visible strings from assets/ into a stub lupdate can read.

Mission briefings, objective text, map names, unit names and campaign narration
mostly live in JSON, so lupdate never sees them -- it only parses source. This
script walks the tracked asset sources (and the committed campaign-map Python
definitions) and emits a generated translation unit full of QT_TRANSLATE_NOOP
entries, which gives lupdate exactly the source strings the runtime will look
up via Game::Util::tr_asset.

The generated file is compiled into the build so a malformed extraction fails
loudly at build time rather than silently producing an empty catalogue.

Usage:
    scripts/extract-asset-strings.py [--assets DIR] [--output FILE] [--check]

--check exits non-zero when the generated file is stale, for CI.
"""

from __future__ import annotations

import argparse
import ast
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

ASSET_CONTEXTS: list[dict] = [
    {
        "context": "Missions",
        "glob": "missions/*.json",
        "paths": [
            "title",
            "summary",
            "teaching_goal",
            "narrative_intent",
            "historical_context",
            "victory_conditions[]/description",
            "defeat_conditions[]/description",
            "optional_objectives[]/description",
            "stages[]/title",
            "stages[]/description",
            "stages[]/hint",
            "events[]/actions[]/text",
            "commander_messages[]/text",
        ],
    },
    {
        "context": "Campaigns",
        "glob": "campaigns/*.json",
        "paths": [
            "title",
            "description",
            "missions[]/intro_text",
            "missions[]/outro_text",
        ],
    },
    {
        "context": "Maps",
        "glob": "maps/*.json",
        "paths": ["name", "description"],
    },
    {
        "context": "Nations",
        "glob": "data/nations/*.json",
        "paths": [
            "display_name",
            "defensive_unit_layout/display_name",
        ],
    },
    {
        "context": "Units",
        "glob": ["data/nations/*.json", "data/troops/*.json"],
        "paths": [
            "troops[]/display_name",
            "troops[]/lore/role",
            "troops[]/lore/strengths",
            "troops[]/lore/weaknesses",
            "troops[]/lore/history",
            "abilities[]/display_name",
            "abilities[]/effect",
        ],
    },
    {
        "context": "Formation",
        "glob": "data/formations/army/*.json",
        "paths": [
            "display_name",
            "intents{}/requirement_hint",
        ],
    },
    {
        "context": "CommanderVoices",
        "glob": "data/commanders/voices/*.json",
        "paths": ["lines[]/variants[]"],
    },
    {
        "context": "LoadingTips",
        "glob": "data/loading_tips.json",
        "paths": ["tips[]/text"],
    },
]

CAMPAIGN_MAP_SOURCE = Path("tools/map_pipeline/provinces.py")

HEADER = """// GENERATED FILE -- DO NOT EDIT.
//
// Regenerate with:  scripts/extract-asset-strings.py
// Verify in CI with: scripts/extract-asset-strings.py --check
//
// Player-visible strings extracted from assets/. lupdate parses this file to
// discover source text it cannot find in hand-written code; the runtime looks
// the same strings up through Game::Util::tr_asset(). Nothing here is called.

#include <QtGlobal>

namespace {

// clang-format off
[[maybe_unused]] const char* const k_asset_strings[] = {
"""

FOOTER = """};
// clang-format on

} // namespace
"""


def resolve(node, path: str):
    """Yield every string reachable from `node` along `path`.

    A segment is either a plain object key, `key[]` to fan out over an array, or
    `key{}` to fan out over an object's values (formation intents are keyed by
    intent name rather than listed).
    """
    if not path:
        if isinstance(node, str):
            yield node
        return

    head, _, rest = path.partition("/")

    for suffix, container_type in (("[]", list), ("{}", dict)):
        if not head.endswith(suffix):
            continue
        container = node.get(head[: -len(suffix)]) if isinstance(node, dict) else None
        if not isinstance(container, container_type):
            return
        items = container if container_type is list else container.values()
        for item in items:
            yield from resolve(item, rest)
        return

    if not isinstance(node, dict) or head not in node:
        return
    yield from resolve(node[head], rest)


def collect_campaign_map_strings(source: Path) -> list[str]:
    """Read province and city names from the committed generator source.

    ``assets/campaign_map/provinces.json`` is intentionally ignored because it
    is a build product. Reading it made the generated translation stub depend
    on whether a developer happened to have run the map pipeline, while a clean
    CI checkout necessarily produced a different result.
    """
    try:
        tree = ast.parse(source.read_text(encoding="utf-8"), filename=str(source))
    except (OSError, SyntaxError) as exc:
        raise SystemExit(f"error: cannot read campaign map definitions: {exc}") from exc

    literal = None
    for node in tree.body:
        if not isinstance(node, ast.FunctionDef) or node.name != "build_provinces":
            continue
        for statement in node.body:
            if not isinstance(statement, ast.Assign):
                continue
            if any(
                isinstance(target, ast.Name) and target.id == "provinces"
                for target in statement.targets
            ):
                literal = statement.value
                break
        break

    if literal is None:
        raise SystemExit(f"error: no provinces literal found in {source}")

    try:
        provinces = ast.literal_eval(literal)
    except (ValueError, TypeError, SyntaxError) as exc:
        raise SystemExit(
            f"error: provinces in {source} are not literal data: {exc}"
        ) from exc

    if not isinstance(provinces, list):
        raise SystemExit(f"error: provinces in {source} must be a list")

    found: set[str] = set()
    for province in provinces:
        if not isinstance(province, dict):
            raise SystemExit(f"error: malformed province entry in {source}")
        name = province.get("name")
        if isinstance(name, str) and name.strip():
            found.add(name.strip())
        cities = province.get("cities", [])
        if not isinstance(cities, list):
            raise SystemExit(f"error: malformed cities list in {source}")
        for city in cities:
            if not isinstance(city, dict):
                raise SystemExit(f"error: malformed city entry in {source}")
            city_name = city.get("name")
            if isinstance(city_name, str) and city_name.strip():
                found.add(city_name.strip())

    return sorted(found)


def collect(assets_dir: Path) -> list[tuple[str, list[str]]]:
    """Return [(context, sorted unique strings)] for every configured family."""
    out: list[tuple[str, list[str]]] = []
    for spec in ASSET_CONTEXTS:
        found: set[str] = set()
        patterns = spec["glob"]
        if isinstance(patterns, str):
            patterns = [patterns]
        files = sorted({path for p in patterns for path in assets_dir.glob(p)})
        if not files:
            print(
                f"warning: no files matched {spec['glob']} for context "
                f"{spec['context']}",
                file=sys.stderr,
            )
        for path in files:
            try:
                data = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError) as exc:
                raise SystemExit(f"error: cannot read {path}: {exc}") from exc
            for field in spec["paths"]:
                for value in resolve(data, field):
                    text = value.strip()
                    if text:
                        found.add(text)
        out.append((spec["context"], sorted(found)))
    campaign_source = assets_dir.resolve().parent / CAMPAIGN_MAP_SOURCE
    out.append(("CampaignMap", collect_campaign_map_strings(campaign_source)))
    return out


def escape(text: str) -> str:
    """Escape a Python string for a C++ string literal."""
    out = text.replace("\\", "\\\\").replace('"', '\\"')
    return out.replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")


def render(groups: list[tuple[str, list[str]]]) -> str:
    body: list[str] = []
    total = 0
    for context, strings in groups:
        body.append(f"    // ---- {context} ({len(strings)}) ----")
        for text in strings:
            body.append(f'    QT_TRANSLATE_NOOP("{context}", "{escape(text)}"),')
        body.append("")
        total += len(strings)
    body.insert(0, f"    // {total} strings across {len(groups)} contexts.")
    return HEADER + "\n".join(body) + FOOTER


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets", type=Path, default=REPO_ROOT / "assets")
    parser.add_argument(
        "--output",
        type=Path,
        default=REPO_ROOT / "translations" / "asset_strings_generated.cpp",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit non-zero if the output file is out of date",
    )
    args = parser.parse_args()

    if not args.assets.is_dir():
        raise SystemExit(f"error: assets directory not found: {args.assets}")

    groups = collect(args.assets)
    rendered = render(groups)

    if args.check:
        current = (
            args.output.read_text(encoding="utf-8") if args.output.exists() else ""
        )
        if current != rendered:
            print(
                f"error: {args.output} is stale; run scripts/extract-asset-strings.py",
                file=sys.stderr,
            )
            return 1
        print(f"[OK] {args.output.name} is up to date")
        return 0

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(rendered, encoding="utf-8")
    total = sum(len(s) for _, s in groups)
    print(f"[OK] wrote {total} asset strings to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
