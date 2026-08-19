#version 330 core
#include "directional_shadows.glsl"
#include "visibility_mask.glsl"

in vec3 v_color;
in vec3 v_world_pos;
in float v_alpha;
in float v_edge;
in float v_edge_softness;

out vec4 frag_color;

void main() {

  float edge = 1.0 - smoothstep(1.0 - v_edge_softness, 1.0, abs(v_edge));
  float alpha = v_alpha * edge;
  if (alpha <= 0.02)
    discard;

  float across = clamp(abs(v_edge), 0.0, 1.0);
  float fold = 1.0 - across * across;
  vec3 color = v_color * mix(0.82, 1.12, fold);

#if SOI_ULTRA_EFFECTS

  vec3 sun_dir = environment_primary_direction();
  vec3 view_dir = normalize(u_shadow_camera_position.xyz - v_world_pos);
  float backlit =
      pow(max(dot(view_dir, -sun_dir), 0.0), 3.0) * clamp(sun_dir.y * 2.0, 0.0, 1.0);
  vec3 sun = environment_primary_color() * environment_primary_intensity();
  color = apply_directional_shadow(color, v_world_pos, vec3(0.0, 1.0, 0.0));
  color += v_color * sun * backlit * 0.45 * fold;
#endif

  color = apply_visibility_memory(color, v_world_pos.xz);
  frag_color = vec4(color, alpha);
}
