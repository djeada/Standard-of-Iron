#include "environment_lighting.glsl"

float contact_shadow_intensity(vec2 uv, vec2 light_dir, float cast_weight) {
  float softness = environment_shadow_softness();

  vec2 contact_uv = uv / vec2(0.46, 0.40);
  float d2 = dot(contact_uv, contact_uv);

  float contact = exp(-d2 * d2 * 0.9) * 0.78 + exp(-d2 * 2.2) * 0.22;

  vec2 dir = light_dir;
  if (dot(dir, dir) < 1e-6)
    dir = vec2(0.0, 1.0);
  dir = normalize(dir);
  vec2 tangent = vec2(-dir.y, dir.x);
  vec2 cast_centre = dir * 0.30;
  vec2 local = uv - cast_centre;
  vec2 cast_uv = vec2(dot(local, dir), dot(local, tangent)) / vec2(0.62, 0.42);
  float cast_lobe = exp(-dot(cast_uv, cast_uv) * mix(2.4, 1.6, softness)) * 0.55;

  return max(contact, cast_lobe * cast_weight);
}

vec4 contact_shadow_color(
    vec2 tex_coord, vec3 world_pos, vec2 light_dir, float cast_weight, float alpha) {
  vec2 uv = tex_coord * 2.0 - 1.0;
  float intensity = contact_shadow_intensity(uv, light_dir, cast_weight);

  float height_fade = clamp(1.0 - max(world_pos.y, 0.0) * 0.05, 0.7, 1.0);
  intensity *= height_fade;
  vec3 tint = environment_shadow_tint();
  float strength = max(environment_shadow_strength(), 0.35);
  return vec4(tint * intensity, intensity * alpha * strength);
}
