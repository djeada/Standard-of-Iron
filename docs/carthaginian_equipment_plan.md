# Carthaginian Equipment Rendering Plan

The goal is to minimise CPU-side geometry complexity while maximising visual fidelity through vertex and fragment shaders. Each unit relies on the shared humanoid torso mesh and light-weight accessory primitives; all richness comes from GPU shading.

---

## Archers

**Geometry Simplification**
- Reuse the standard torso mesh for the linothorax body; no layered shells or extra spheres.
- Submit a single leather cap mesh instance aligned to the head attachment frame.
- Avoid per-frame mesh duplication—cache transforms and colours.

**Vertex Shader Requirements**
- Offset armor vertices slightly along normals (≈4%) to prevent z-fighting with the base body mesh.
- Pass custom varyings for leather tension: use noise-driven perturbations and store both world normal and bitangent for anisotropic highlights.
- Encode bow-hand segment IDs (0=upper torso, 1=waist pteruges) to enable fragment blending.

**Fragment Shader Requirements**
- Procedural leather grain using triplanar noise; modulate roughness with stretch maps from vertex shader.
- Add hand-stitched seams along UV seamlines using a sinusoidal mask on v_armorLayer.
- Apply rain darkening and edge wear using curvature approximations from normal derivatives.
- Integrate specular response for waxed leather (Fresnel-Schlick with tuned F0 ≈ 0.04).

---

## Spearmen

**Geometry Simplification**
- Submit one torso mesh for the linothorax shell and a shared bronze helmet mesh; omit separate skirts—pteruges will be shader-driven.
- Use thin cylindrical primitives only for the spear and shield handles; the armor stays single-surface.

**Vertex Shader Requirements**
- Generate layered linen pteruges procedurally: displace torso mesh lower vertices using a deterministic pattern (integer IDs from bone weights).
- Supply tangent-space directions for simulated cloth fray; compute secondary normal for frayed edges.
- Output armor layer IDs (0=helmet, 1=torso linen, 2=pteruges) and world-space height for weathering gradients.

**Fragment Shader Requirements**
- Implement complex linen weave via coupled sine functions (warp/weft) combined with stochastic fuzz.
- Blend mud/dust accumulation by sampling a standing water occlusion map derived from world height.
- Add bronze helmet shading with hammered microfacets, verdigris streaks, and specular anisotropy.
- Mix subsurface scattering approximation for linen highlights (Burley diffuse tweak).

---

## Swordsmen

**Geometry Simplification**
- Employ a single torso mesh offset for chainmail and a shared bronze helmet primitive; no extra thigh guards.
- Rely on shader-based displacement to imply mail rings and lamellar segments.

**Vertex Shader Requirements**
- Calculate mail sag by biasing torso vertices downward along gravity with bone weight falloff.
- Provide ring orientation vectors to the fragment shader using procedural hash of world position.
- Emit mask values for belt region and shoulder flanges; these control metallic sheen variations.

**Fragment Shader Requirements**
- Realise chainmail with high-frequency normal perturbation (hexagonal tiling) and parallax occlusion.
- Add differential roughness for rubbed vs. untouched areas using cavity and curvature masks.
- Introduce moisture streaks that react to movement speed (pass velocity magnitude via uniform).
- Implement dual-layer specular (steel + underlying dark cloth) with energy-conserving blend.

---

## Shared Implementation Notes

- All unit shaders should share a common include defining noise, Fresnel, and curvature helpers.
- Keep CPU-side equipment renderers responsible only for transform submission and base colour tagging.
- Cache shader uniforms for weather state (rain, dust) and lighting to avoid per-draw recomputation.
- Validate visual output with unit tests comparing shader screenshots to reference LUTs.
