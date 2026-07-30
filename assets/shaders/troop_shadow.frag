#version 330 core
#include "environment_lighting.glsl"

in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform float u_alpha;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform sampler2D u_texture;
uniform vec2 u_light_dir;

out vec4 frag_color;

void main() {

  vec2 uv = v_tex_coord * 2.0 - 1.0;

  vec2 dir = u_light_dir;
  if (length(dir) < 1e-4)
    dir = vec2(0.0, 1.0);
  dir = normalize(dir);
  vec2 tangent = vec2(-dir.y, dir.x);

  float along = dot(uv, dir);
  float across = dot(uv, tangent);

  float wobble = 0.04 * sin(uv.x * 5.3) * sin(uv.y * 4.7);
  vec2 elliptic = vec2(along, across) * (1.0 + wobble);

  float softness = environment_shadow_softness();

  vec2 contact_uv = elliptic / vec2(0.58, 0.44);
  float contact = exp(-dot(contact_uv, contact_uv) * mix(6.2, 4.0, softness));

  vec2 cast_uv = elliptic / vec2(1.18, 0.92);
  float cast_shadow = exp(-dot(cast_uv, cast_uv) * mix(2.6, 1.6, softness));

  float shadow_intensity = max(contact * 0.82, cast_shadow * 0.26);

  float height_fade = clamp(1.0 - max(v_world_pos.y, 0.0) * 0.08, 0.6, 1.0);
  shadow_intensity *= height_fade;

  vec3 tex_color = vec3(1.0);
  float tex_alpha = 1.0;
  if (u_use_texture) {
    vec4 tex = texture(u_texture, v_tex_coord);
    tex_color = tex.rgb;
    tex_alpha = tex.a;
  }

  shadow_intensity *= tex_alpha;

  vec3 shadow_color =
      environment_shadow_tint() * tex_color * (vec3(1.0) + u_color * 0.0001);

  float final_alpha = shadow_intensity * u_alpha * environment_shadow_strength();

  vec3 final_color = shadow_color * shadow_intensity;

  frag_color = vec4(final_color, final_alpha);
}
