#version 330 core

// Stage 16.4 — instanced GPU-skinning fragment shader.
//
// Mirrors character_skinned.frag but reads color/alpha (and material
// id, currently unused but reserved) from per-instance varyings so a
// single instanced draw can render N creatures with distinct tints.
// Texturing is intentionally not supported on the instanced path —
// textured rigged commands fall back to the per-cmd shader. See
// RiggedCharacterPipeline::draw_instanced.

in vec3 v_normal_ws;
in vec2 v_tex;
in vec3 v_pos_ws;
flat in vec3 v_color;
flat in float v_alpha;
flat in int v_material_id;

out vec4 FragColor;

void main() {
  vec3 base = v_color;

  vec3 normal = normalize(v_normal_ws);
  vec3 light_dir = normalize(vec3(1.0, 1.15, 1.0));
  float wrap = 0.35;
  float diff = max(dot(normal, light_dir) * (1.0 - wrap) + wrap, 0.25);

  vec3 color = clamp(base * diff, 0.0, 1.0);
  FragColor = vec4(color, v_alpha);
}
