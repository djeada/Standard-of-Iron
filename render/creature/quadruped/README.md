# Quadruped rendering

Horse and elephant now share the same high-level flow:

1. **Authoring:** each species defines a `SpeciesManifest` with:
   - topology from `skeleton_factory`
   - whole-mesh node graphs from `mesh_graph`
   - clip metadata for BPAT/BPSM baking
2. **Build time:** `tools/bpat_baker` walks the manifest and writes:
   - `*.bpat` animation palettes
   - `*_minimal.bpsm` snapshot meshes
3. **Runtime:** prepare code only resolves:
   - current animation state
   - clip phase
   - grounded world transform
   - role colors

Runtime quadruped prep must not rebuild species meshes or evaluate per-bone pose trees just to render. It should emit a `CreatureRenderRequest` plus any shadow/prewarm metadata and let the shared creature pipeline pull the baked clip and mesh data.

## Adding a new quadruped species

1. Add a manifest in `render/<species>/<species>_manifest.cpp`.
2. Reuse `Quadruped::MeshNode` shapes for the whole-mesh LODs.
3. Provide clip descriptors and a bake callback for BPAT generation.
4. Expose the manifest through the species spec/asset registry.
5. Wire the species runtime prep to emit a shared quadruped request instead of a species-specific render batch path.
