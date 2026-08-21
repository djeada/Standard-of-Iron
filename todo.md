I checked the actual full-LOD shader paths, especially `terrain_chunk.frag`, `character_skinned.frag`, `basic.frag`, the fog/visibility ordering, and local-light selection. The biggest opportunity is **removing repeated procedural fragment work**, not reducing mesh or texture quality.

# Standard of Iron — Full-LOD GPU Optimization Issues

1. **Terrain Rebuilds Static Detail Per Pixel**
   **Problem:** `terrain_chunk.frag` repeatedly evaluates 5-octave FBM for domain warp, soil, moisture, meadow, thatch, surface detail, speckle, rock breakup, snow, and other fields. Each FBM itself performs five gradient-noise evaluations using multiple hash operations. Much of this depends only on world position and static terrain properties.
   **Fix:** Generate the same static fields during map generation/loading and pack them into 1–2 RGBA terrain-mask textures. Keep dynamic snow amount, wetness, lighting, fog, and similar effects runtime. This can preserve essentially the same visual result while replacing a large amount of fragment ALU with a few texture reads.

2. **Heightmap Normal and Curvature Are Reconstructed Per Fragment**
   **Problem:** With the height texture enabled, `heightmap_normal()` performs four height samples and `compute_curvature()` performs five more. That is up to **nine heightmap samples per terrain fragment** before the rest of the material work.
   **Fix:** Generate a terrain normal/curvature texture whenever the heightmap changes. Pack normal XY plus curvature/AO into channels and replace the nine height samples with one texture fetch.

3. **Fog-of-War Discard Happens Far Too Late**
   **Problem:** Terrain performs procedural material generation, relief normal generation, environment lighting, local lighting, and directional shadowing **before** sampling the visibility texture. Completely unseen pixels are finally discarded at the end.
   **Fix:** Sample visibility immediately at the beginning of `main()`. If `vis_sample < 0.25`, discard before doing any terrain shading. Store the explored-state multiplier and apply it at the end. This should be visually identical and can eliminate nearly all fragment work for hidden terrain.

4. **Fully Fogged Terrain Still Receives Full Shading**
   **Problem:** Atmospheric fog is calculated only after terrain material generation, relief, local lights, and directional shadows. A pixel that ultimately becomes essentially pure fog still pays for the entire terrain shader first.
   **Fix:** Calculate `view_distance` and fog amount near the start. When fog is effectively opaque, directly output `environment_fog_color()` and return. For partially fogged pixels, keep the existing final `mix()`. Full-LOD geometry remains untouched.

5. **Fog Distance Is Calculated After It Could Already Be Useful**
   **Problem:** `view_distance` is calculated near the end for sheen/fog even though distance could drive safe early rejection and other shader decisions.
   **Fix:** Calculate camera vector, distance, and normalized view direction once near the beginning and reuse them for fog, grazing reflections, and any distance-dependent work.

6. **Band-Limited Noise Is Still Evaluated When Its Contribution Is Zero**
   **Problem:** The shader correctly calculates `grain_fade`, `granular_fade`, and `speck_fade`, but expressions such as `gradient_fbm(...) * speck_fade` still evaluate the expensive noise before multiplying it by zero. Your `relief_octave()` already does this better by returning before calculating noise when its fade is negligible.
   **Fix:** Apply the `relief_octave()` pattern everywhere:

    ```cpp
    float speckle = 0.0;
    if (speck_fade > 0.001)
        speckle = gradient_fbm(...) * speck_fade;
    ```

    This preserves the existing result where the feature is band-limited away.

7. **Rock Shading Runs on Non-Rock Terrain**
   **Problem:** After calculating `rock_mask`, the shader still performs two cellular-distance searches, FBM, grain noise, strata noise, and bedding noise regardless of whether the final pixel has meaningful rock coverage.
   **Fix:** Calculate the cheap rock mask first, then execute fracture/chipping/strata work only when rock contribution is non-negligible. The branch should also be spatially coherent because neighboring terrain pixels generally share rock coverage.

8. **Tiny Terrain Detail Uses Disproportionately Expensive Noise**
   **Problem:** Several subtle surface variations use full procedural FBM/noise even though they only make small changes to final color or normal. The terrain is spending significant GPU ALU on microvariation that could come from a reusable texture.
   **Fix:** Create a tileable RGBA microdetail texture containing grain, speckle, weave/roughness, and relief noise. Sample it at several scales instead of independently synthesizing each signal. Keep the same modulation strengths so the visual character remains.

9. **Character Wear Is Procedurally Generated Per Fragment**
   **Problem:** `character_skinned.frag` generates hoof texture, horse coat variation, streaks, wear, bruising, blood patterns, etc. using repeated hashes, `sin`, `smoothstep`, and material branches for every covered character pixel.
   **Fix:** Pack stable wear/grain patterns into a small texture array or mask atlas. Continue using the existing per-instance wear parameters to control intensity and variation. Full-LOD geometry, colors, blood/wear amounts, and material response remain intact while expensive procedural pattern generation disappears.

10. **Generic Materials Generate Wood/Cloth/Leather Detail in the Pixel Shader**
    **Problem:** `basic.frag` procedurally calculates wood grain, knots, cloth weave, leather blotches, metal noise, soot, and several `pow()` effects per fragment. It even has `u_material_id` but still contains one large generic material shader. ([GitHub][1])
    **Fix:** Use small full-LOD material variants or packed detail textures. Wood, cloth, leather, and metal can retain exactly the same full-resolution geometry and visual features without synthesizing their texture structure mathematically for every pixel.

11. **Material Detection Does Work That Is Already Known on the CPU**
    **Problem:** When `u_material_id` does not identify certain materials, `basic.frag` attempts to infer wood/metal/cloth from base-color relationships and luminance. That is unnecessary classification work inside a fragment shader. ([GitHub][1])
    **Fix:** Resolve material type when building the draw/material data and always send an explicit material ID or feature mask. Let the fragment shader execute only the known material path.

12. **Local Lights Are Selected Globally by Camera, Not by Draw Region**
    **Problem:** The renderer selects up to eight local lights according to camera-distance score. Meanwhile terrain, characters, and basic materials invoke `local_lighting(...)` from their fragment shaders. A camera-global light set is less efficient than knowing which selected lights can actually intersect each terrain chunk or entity batch. ([GitHub][2])
    **Fix:** Keep the same selected lights, but build a compact per-draw/per-terrain-chunk list using light radius versus draw bounds. Upload only intersecting lights plus a count. This preserves the same visible lights while avoiding irrelevant local-light calculations over large parts of the screen.

13. **Lighting and Shadows Are Paid Before Visibility/Fog Rejection**
    **Problem:** Terrain calls `environment_ambient_light()`, `local_lighting()`, and `apply_directional_shadow()` before checking fog-of-war visibility and before applying atmospheric fog. Even without making assumptions about the internal cost of the shadow include, this ordering guarantees unnecessary lighting work on pixels that will subsequently disappear or become fog.
    **Fix:** Order the terrain fragment path roughly as: visibility → distance/fog test → cheap material masks → expensive material detail → lighting → shadows → partial fog blend.

14. **Character Material Paths Should Be Specialized**
    **Problem:** The full character shader contains general character lighting plus special horse and metal paths, procedural wear, rim lighting, metal glint, horse sheen, and multiple `pow()` operations in one program.
    **Fix:** Create a small number of full-LOD shader permutations such as humanoid-cloth/leather, humanoid-metal, and horse. Batch by material as you already do. This does **not** mean lowering LOD; it means compiling out irrelevant code for each full-quality material.

15. **Terrain Relief Is Reconstructed With More Procedural Noise**
    **Problem:** After all the earlier terrain noise, the lighting stage generates three additional relief octaves. Each active octave samples `gradient_noise()` three times to derive a procedural normal perturbation.
    **Fix:** Put the same relief signal into the terrain detail-normal texture. Let the fragment shader combine the sampled detail normal with the macro terrain normal rather than reconstructing relief derivatives procedurally.

16. **Static and Dynamic Terrain Work Are Mixed Together**
    **Problem:** Static geography, static biome variation, dynamic moisture, snow, fog, lighting, visibility, and shadows all live in one giant terrain fragment program. This forces the GPU to reconstruct static information every frame.
    **Fix:** Split the conceptual data into **precomputed terrain material data** and **dynamic frame shading**. Full LOD should mean full visual quality—not recomputing immutable terrain noise millions of times per second.

## Priority

The first changes I would make are **early visibility discard**, **early fully-fogged return**, **precomputed normal/curvature**, and **baking the static terrain FBM fields**. Those attack work on the terrain shader, which covers the largest number of screen pixels, while requiring essentially no visible downgrade.

After that, move **character wear and generic material procedural texture generation into packed detail textures**, then make **local-light lists spatially relevant per draw/chunk**.

I would **not** reduce full-LOD mesh complexity, remove the current lighting model, lower texture resolution, or weaken the fog/shadow look until these waste-removal changes have been profiled first.

[1]: https://github.com/djeada/Standard-of-Iron/blob/main/assets/shaders/basic.frag "Standard-of-Iron/assets/shaders/basic.frag at main · djeada/Standard-of-Iron · GitHub"
[2]: https://github.com/djeada/Standard-of-Iron/blob/main/render/local_lighting.h "Standard-of-Iron/render/local_lighting.h at main · djeada/Standard-of-Iron · GitHub"
