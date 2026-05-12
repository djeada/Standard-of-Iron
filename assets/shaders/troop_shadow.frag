#version 330 core

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

  float along_scale = 1.15;
  float across_scale = 0.95;

  float ax = along / along_scale;
  float ay = across / across_scale;

  float r = length(vec2(ax, ay));

  float wobble = 0.04 * sin(uv.x * 5.3) * sin(uv.y * 4.7);
  r = max(0.0, r + wobble);

  float gaussian = exp(-r * r * 2.2);
  float feather = clamp(1.0 - r, 0.0, 1.0);
  float shadow_intensity = mix(feather, gaussian, 0.7);
  shadow_intensity = pow(shadow_intensity, 1.35);

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

  vec3 shadow_color = vec3(0.013) * u_color * tex_color;

  float final_alpha = shadow_intensity * u_alpha * 0.95;

  vec3 final_color = shadow_color * shadow_intensity;

  frag_color = vec4(final_color, final_alpha);
}
