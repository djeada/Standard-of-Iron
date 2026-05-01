import pathlib
import sys

import bpy


IMPORTERS = {
    ".glb": bpy.ops.import_scene.gltf,
    ".gltf": bpy.ops.import_scene.gltf,
    ".fbx": bpy.ops.import_scene.fbx,
    ".obj": bpy.ops.wm.obj_import,
}


def parse_args() -> tuple[pathlib.Path, pathlib.Path]:
    argv = sys.argv
    argv = argv[argv.index("--") + 1 :] if "--" in argv else []
    if len(argv) != 2:
        raise SystemExit(
            "usage: blender -b -P tools/mesh_preview/convert_reference_mesh.py -- "
            "input_mesh output.obj"
        )

    input_path = pathlib.Path(argv[0]).expanduser().resolve()
    output_path = pathlib.Path(argv[1]).expanduser().resolve()
    if input_path.suffix.lower() not in IMPORTERS:
        raise SystemExit(
            f"unsupported input format '{input_path.suffix}'; expected one of: "
            f"{', '.join(sorted(IMPORTERS))}"
        )
    if output_path.suffix.lower() != ".obj":
        raise SystemExit("output path must end in .obj")
    return input_path, output_path


def main() -> None:
    input_path, output_path = parse_args()

    bpy.ops.wm.read_factory_settings(use_empty=True)
    IMPORTERS[input_path.suffix.lower()](filepath=str(input_path))

    mesh_objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
    if not mesh_objects:
        raise SystemExit("no mesh objects found in imported scene")

    for obj in bpy.data.objects:
        obj.select_set(obj in mesh_objects)
    bpy.context.view_layer.objects.active = mesh_objects[0]

    output_path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.obj_export(
        filepath=str(output_path),
        export_selected_objects=True,
        export_materials=False,
    )


if __name__ == "__main__":
    main()
