#!/usr/bin/env python3
"""
Single-shot pipeline for the campaign map. No CLI flags; always:
- download Natural Earth land + rivers (if missing)
- clip/project to UV bounds (from map_bounds.json)
- triangulate land -> land_mesh.bin
- export coastlines/rivers UV polylines
- render preview PNG
- render base textures (sea/land/ink)
- export OBJ files for mesh/coastlines/rivers

Outputs go to assets/campaign_map/.
"""

from __future__ import annotations

import json
import logging
import struct
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Tuple
from urllib.request import Request, urlopen

import numpy as np
import mapbox_earcut as earcut
from PIL import Image, ImageDraw, ImageFilter
import fiona
from shapely.geometry import box, mapping, shape
from shapely.ops import transform

ROOT = Path(__file__).resolve().parents[2]
ASSETS_DIR = ROOT / "assets" / "campaign_map"
BOUNDS_PATH = Path(__file__).with_name("map_bounds.json")
WORK_DIR = Path(__file__).with_name("build")

NE_DATASETS = {
    "land": {
        "urls": [
            "https://naciscdn.org/naturalearth/10m/physical/ne_10m_land.zip",
            "https://naturalearth.s3.amazonaws.com/10m/physical/ne_10m_land.zip",
            "https://www.naturalearthdata.com/http//www.naturalearthdata.com/download/10m/physical/ne_10m_land.zip",
        ],
        "archive": "ne_10m_land.zip",
        "shapefile": "ne_10m_land.shp",
        "extract_dir": "ne_10m_land",
    },
    "rivers": {
        "urls": [
            "https://naciscdn.org/naturalearth/10m/physical/ne_10m_rivers_lake_centerlines.zip",
            "https://naturalearth.s3.amazonaws.com/10m/physical/ne_10m_rivers_lake_centerlines.zip",
            "https://www.naturalearthdata.com/http//www.naturalearthdata.com/download/10m/physical/ne_10m_rivers_lake_centerlines.zip",
        ],
        "archive": "ne_10m_rivers_lake_centerlines.zip",
        "shapefile": "ne_10m_rivers_lake_centerlines.shp",
        "extract_dir": "ne_10m_rivers_lake_centerlines",
    },
}


@dataclass(frozen=True)
class MapBounds:
    lon_min: float
    lon_max: float
    lat_min: float
    lat_max: float

    @staticmethod
    def from_json(path: Path) -> "MapBounds":
        data = json.loads(path.read_text())
        return MapBounds(
            lon_min=float(data["lon_min"]),
            lon_max=float(data["lon_max"]),
            lat_min=float(data["lat_min"]),
            lat_max=float(data["lat_max"]),
        )

    def to_uv(self, lon: float, lat: float) -> Tuple[float, float]:
        u = (lon - self.lon_min) / (self.lon_max - self.lon_min)
        v = (lat - self.lat_min) / (self.lat_max - self.lat_min)
        return (u, v)

    def clip_box(self):
        return box(self.lon_min, self.lat_min, self.lon_max, self.lat_max)


def ensure_dataset(dataset: str, work_dir: Path) -> Path:
    meta = NE_DATASETS[dataset]
    work_dir.mkdir(parents=True, exist_ok=True)
    archive_path = work_dir / meta["archive"]
    if not archive_path.exists():
        logging.info("Downloading %s to %s", dataset, archive_path)
        last_error: Exception | None = None
        for url in meta["urls"]:
            try:
                logging.info("  trying %s", url)
                req = Request(url, headers={"User-Agent": "Mozilla/5.0"})
                with urlopen(req) as resp, open(archive_path, "wb") as out:
                    out.write(resp.read())
                last_error = None
                break
            except Exception as exc:  # noqa: BLE001
                last_error = exc
                logging.warning("  download failed: %s", exc)
        if last_error:
            raise last_error
    extract_dir = work_dir / meta["extract_dir"]
    shapefile_path = extract_dir / meta["shapefile"]
    if not shapefile_path.exists():
        logging.info("Extracting %s", archive_path)
        with zipfile.ZipFile(archive_path, "r") as zf:
            zf.extractall(extract_dir)
    return shapefile_path


def load_geometries(shapefile_path: Path) -> Iterable:
    with fiona.open(shapefile_path) as src:
        for feat in src:
            yield shape(feat["geometry"])


def clip_and_project(geoms: Iterable, bounds: MapBounds) -> list:
    bbox = bounds.clip_box()
    projected = []
    for geom in geoms:
        clipped = geom.intersection(bbox)
        if clipped.is_empty:
            continue
        uv_geom = transform(lambda x, y, z=None: bounds.to_uv(x, y), clipped)
        projected.append(uv_geom)
    return projected


def write_geojson(geoms: Iterable, out_path: Path):
    features = [
        {"type": "Feature", "geometry": mapping(geom), "properties": {}}
        for geom in geoms
    ]
    collection = {"type": "FeatureCollection", "features": features}
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(collection, indent=2))


def polygon_to_triangles(poly) -> List[float]:
    rings: List[List[Tuple[float, float]]] = []
    rings.append(list(poly.exterior.coords)[:-1])
    for interior in poly.interiors:
        rings.append(list(interior.coords)[:-1])

    verts: List[Tuple[float, float]] = []
    ring_end_indices: List[int] = []
    for ring in rings:
        verts.extend((float(u), float(v)) for (u, v) in ring)
        ring_end_indices.append(len(verts))

    if not verts:
        return []

    v_arr = np.array(verts, dtype=np.float32).reshape((-1, 2))
    end_arr = np.array(ring_end_indices, dtype=np.uint32)
    try:
        indices = earcut.triangulate_float32(v_arr, end_arr)
    except Exception:
        return []

    triangles: List[float] = []
    for i in indices:
        u, v = verts[int(i)]
        triangles.extend([u, v])
    return triangles


def triangulate_geoms(geoms: Iterable) -> List[float]:
    all_tris: List[float] = []
    for geom in geoms:
        if geom.is_empty:
            continue
        if geom.geom_type == "Polygon":
            all_tris.extend(polygon_to_triangles(geom))
        elif geom.geom_type == "MultiPolygon":
            for poly in geom.geoms:
                all_tris.extend(polygon_to_triangles(poly))
    return all_tris


def write_triangle_bin(tris: List[float], out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "wb") as f:
        for val in tris:
            f.write(struct.pack("<f", val))


def collect_coastlines(geoms: Iterable) -> List[List[Tuple[float, float]]]:
    lines: List[List[Tuple[float, float]]] = []
    for geom in geoms:
        if geom.is_empty:
            continue
        polys = []
        if geom.geom_type == "Polygon":
            polys = [geom]
        elif geom.geom_type == "MultiPolygon":
            polys = list(geom.geoms)
        for poly in polys:
            ring = list(poly.exterior.coords)[:-1]
            if ring:
                lines.append([(float(u), float(v)) for (u, v) in ring])
    return lines


def clip_project_lines(
    geoms: Iterable, bounds: MapBounds
) -> List[List[Tuple[float, float]]]:
    bbox = bounds.clip_box()
    lines: List[List[Tuple[float, float]]] = []
    for geom in geoms:
        clipped = geom.intersection(bbox)
        if clipped.is_empty:
            continue
        parts = []
        if clipped.geom_type == "LineString":
            parts = [clipped]
        elif clipped.geom_type == "MultiLineString":
            parts = list(clipped.geoms)
        for line in parts:
            uv_line = transform(lambda x, y, z=None: bounds.to_uv(x, y), line)
            coords = [(float(u), float(v)) for (u, v) in uv_line.coords]
            if coords:
                lines.append(coords)
    return lines


def write_lines_json(lines: List[List[Tuple[float, float]]], out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    json_lines = [[[u, v] for (u, v) in line] for line in lines]
    out_path.write_text(json.dumps({"lines": json_lines}, indent=2))


def uv_to_px(u: float, v: float, width: int, height: int) -> tuple[int, int]:
    x = int(u * (width - 1))
    y = int((1.0 - v) * (height - 1))
    return (x, y)


def draw_polygon(draw: ImageDraw.ImageDraw, poly, width: int, height: int, fill):
    rings = [poly.exterior] + list(poly.interiors)
    for ring in rings:
        pts = [uv_to_px(u, v, width, height) for (u, v) in ring.coords[:-1]]
        if pts:
            draw.polygon(pts, fill=fill)


def draw_lines(
    draw: ImageDraw.ImageDraw, lines, width: int, height: int, color, stroke: int
):
    for line in lines:
        pts = [uv_to_px(u, v, width, height) for (u, v) in line]
        if len(pts) >= 2:
            draw.line(pts, fill=color, width=stroke, joint="curve")


def render_preview(
    uv_geoms, coast_lines, river_lines, size: int = 4096, oversample: int = 2
):
    w = h = size * oversample
    sea_color = (40, 68, 92, 255)
    land_color = (231, 216, 182, 255)
    coast_color = (64, 56, 48, 255)
    river_color = (82, 121, 156, 255)

    base = Image.new("RGBA", (w, h), sea_color)
    draw = ImageDraw.Draw(base, "RGBA")

    for geom in uv_geoms:
        if geom.is_empty:
            continue
        if geom.geom_type == "Polygon":
            draw_polygon(draw, geom, w, h, fill=land_color)
        elif geom.geom_type == "MultiPolygon":
            for poly in geom.geoms:
                draw_polygon(draw, poly, w, h, fill=land_color)

    draw_lines(draw, coast_lines, w, h, color=coast_color, stroke=3 * oversample)
    draw_lines(draw, river_lines, w, h, color=river_color, stroke=2 * oversample)

    out = base.resize((size, size), Image.Resampling.LANCZOS)
    out.save(ASSETS_DIR / "campaign_preview.png")


def make_noise(size: int, scale: float = 1.0) -> Image.Image:
    noise = np.random.default_rng(42).random((size, size)).astype(np.float32)
    noise = (noise * 255 * scale).clip(0, 255).astype(np.uint8)
    return Image.fromarray(noise, mode="L")


def render_textures(
    uv_geoms, coast_lines, river_lines, size: int = 4096, oversample: int = 2
):
    w = h = size * oversample
    sea_top = (54, 90, 120)
    sea_bottom = (28, 58, 82)
    land_fill = (232, 218, 193, 255)
    coast_color = (56, 48, 42, 255)
    river_color = (86, 126, 156, 255)

    water = Image.new("RGBA", (w, h))
    for y in range(w):
        t = y / max(1, w - 1)
        r = int(sea_top[0] * (1 - t) + sea_bottom[0] * t)
        g = int(sea_top[1] * (1 - t) + sea_bottom[1] * t)
        b = int(sea_top[2] * (1 - t) + sea_bottom[2] * t)
        ImageDraw.Draw(water).line([(0, y), (w, y)], fill=(r, g, b, 255))
    grain = make_noise(w, scale=0.12).filter(ImageFilter.GaussianBlur(radius=0.8))
    water = Image.composite(water, water, grain)

    base = water.copy()
    draw = ImageDraw.Draw(base, "RGBA")
    for geom in uv_geoms:
        if geom.is_empty:
            continue
        if geom.geom_type == "Polygon":
            draw_polygon(draw, geom, w, h, fill=land_fill)
        elif geom.geom_type == "MultiPolygon":
            for poly in geom.geoms:
                draw_polygon(draw, poly, w, h, fill=land_fill)

    draw_lines(draw, coast_lines, w, h, color=coast_color, stroke=3 * oversample)
    draw_lines(draw, river_lines, w, h, color=river_color, stroke=2 * oversample)

    base.resize((size, size), Image.Resampling.LANCZOS).save(
        ASSETS_DIR / "campaign_base_color.png"
    )
    water.resize((size, size), Image.Resampling.LANCZOS).save(
        ASSETS_DIR / "campaign_water.png"
    )


def write_obj_mesh(tris: List[float], out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# land mesh exported from land_mesh.bin"]
    verts = list(zip(tris[0::2], tris[1::2]))
    for u, v in verts:
        lines.append(f"v {u:.6f} {v:.6f} 0.0")
    tri_count = len(verts) // 3
    for i in range(tri_count):
        a = 3 * i + 1
        b = a + 1
        c = a + 2
        lines.append(f"f {a} {b} {c}")
    out_path.write_text("\n".join(lines) + "\n")


def write_obj_lines(lines, out_path: Path):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    verts = []
    obj_lines = []
    for line in lines:
        start_idx = len(verts)
        verts.extend([(u, v) for (u, v) in line])
        idxs = [start_idx + i + 1 for i in range(len(line))]
        if len(idxs) >= 2:
            obj_lines.append(idxs)
    with out_path.open("w", encoding="utf-8") as f:
        f.write("# polyline export\n")
        for u, v in verts:
            f.write(f"v {u:.6f} {v:.6f} 0.0\n")
        for idxs in obj_lines:
            f.write("l " + " ".join(str(i) for i in idxs) + "\n")


def main():
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
    bounds = MapBounds.from_json(BOUNDS_PATH)
    logging.info(
        "Bounds: lon %.2f..%.2f lat %.2f..%.2f",
        bounds.lon_min,
        bounds.lon_max,
        bounds.lat_min,
        bounds.lat_max,
    )

    land_shp = ensure_dataset("land", WORK_DIR)
    rivers_shp = ensure_dataset("rivers", WORK_DIR)

    land_geoms = list(load_geometries(land_shp))
    uv_geoms = clip_and_project(land_geoms, bounds)
    uv_geojson_path = ASSETS_DIR / "land_uv.geojson"
    write_geojson(uv_geoms, uv_geojson_path)
    logging.info("land_uv.geojson written")

    tris = triangulate_geoms(uv_geoms)
    land_mesh_bin = ASSETS_DIR / "land_mesh.bin"
    write_triangle_bin(tris, land_mesh_bin)
    logging.info("land_mesh.bin written")

    coast_lines = collect_coastlines(uv_geoms)
    coast_json = ASSETS_DIR / "coastlines_uv.json"
    write_lines_json(coast_lines, coast_json)
    logging.info("coastlines_uv.json written")

    river_geoms = list(load_geometries(rivers_shp))
    river_lines = clip_project_lines(river_geoms, bounds)
    rivers_json = ASSETS_DIR / "rivers_uv.json"
    write_lines_json(river_lines, rivers_json)
    logging.info("rivers_uv.json written")

    render_preview(uv_geoms, coast_lines, river_lines)
    logging.info("campaign_preview.png written")

    render_textures(uv_geoms, coast_lines, river_lines)
    logging.info("campaign_base_color.png and campaign_water.png written")

    write_obj_mesh(tris, ASSETS_DIR / "land_mesh.obj")
    write_obj_lines(coast_lines, ASSETS_DIR / "coastlines.obj")
    write_obj_lines(river_lines, ASSETS_DIR / "rivers.obj")
    logging.info("OBJ exports written")

    manifest = {
        "source": "Natural Earth 10m",
        "bounds": {
            "lon_min": bounds.lon_min,
            "lon_max": bounds.lon_max,
            "lat_min": bounds.lat_min,
            "lat_max": bounds.lat_max,
        },
        "projection": "equirectangular",
        "outputs": {
            "land_uv_geojson": str(uv_geojson_path),
            "land_mesh_bin": str(land_mesh_bin),
            "coastlines_uv_json": str(coast_json),
            "rivers_uv_json": str(rivers_json),
            "campaign_preview_png": str(ASSETS_DIR / "campaign_preview.png"),
            "campaign_base_color_png": str(ASSETS_DIR / "campaign_base_color.png"),
            "campaign_water_png": str(ASSETS_DIR / "campaign_water.png"),
            "land_mesh_obj": str(ASSETS_DIR / "land_mesh.obj"),
            "coastlines_obj": str(ASSETS_DIR / "coastlines.obj"),
            "rivers_obj": str(ASSETS_DIR / "rivers.obj"),
        },
    }
    (ASSETS_DIR / "manifest.json").write_text(json.dumps(manifest, indent=2))
    logging.info("manifest.json written")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        sys.stderr.write(f"Error: {exc}\n")
        sys.exit(1)
