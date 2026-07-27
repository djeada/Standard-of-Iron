#version 330 core
#include "environment_lighting.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_pos_height;
layout(location = 3) in vec4 a_color_width;
layout(location = 4) in vec4 a_sway_params;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;
uniform vec3 u_soil_color;
uniform vec2 u_viewport_size;
uniform vec3 u_camera_pos;
uniform float u_ambient_boost;

out vec3 v_color;
out float v_alpha;
out float v_edge;
out float v_edge_softness;

float hash11(float x) {
  return fract(sin(x * 78.233) * 43758.5453123);
}

void main() {
  vec3 base_pos = a_pos_height.xyz;
  float blade_height = a_pos_height.w;

  vec3 blade_color = a_color_width.xyz;
  float blade_width = a_color_width.w;

  float sway_strength = a_sway_params.x * u_wind_strength;
  float sway_speed = a_sway_params.y * u_wind_speed;
  float sway_phase = a_sway_params.z;
  float orientation = a_sway_params.w;

  float tip = clamp(a_uv.y, 0.0, 1.0);
  float sin_o = sin(orientation);
  float cos_o = cos(orientation);

  vec3 width_axis = vec3(cos_o, 0.0, sin_o);
  vec3 lean_axis = vec3(-sin_o, 0.0, cos_o);

  float lean = mix(-0.10, 0.16, hash11(sway_phase * 1.7 + 3.1));
  float sway = sin(u_time * sway_speed + sway_phase) * sway_strength;
  float bend = tip * tip;
  float arc = lean * bend + sway * bend;

  vec4 base_clip = u_view_proj * vec4(base_pos, 1.0);
  vec4 side_clip =
      base_clip + (u_view_proj[0] * cos_o + u_view_proj[2] * sin_o) * blade_width;
  vec4 top_clip = base_clip + u_view_proj[1] * blade_height;

  float safe_w = max(abs(base_clip.w), 1e-4);
  vec2 base_ndc = base_clip.xy / safe_w;
  float width_px = length((side_clip.xy / max(abs(side_clip.w), 1e-4) - base_ndc) *
                          u_viewport_size * 0.5);
  float height_px = length((top_clip.xy / max(abs(top_clip.w), 1e-4) - base_ndc) *
                           u_viewport_size * 0.5);

  const float k_min_width_px = 1.30;
  const float k_min_height_px = 2.20;
  float widen = clamp(k_min_width_px / max(width_px, 1e-4), 1.0, 6.0);

  float heighten = clamp(k_min_height_px / max(height_px, 1e-4), 1.0, 4.0);
  blade_height *= heighten;

  float coverage_fade = smoothstep(0.10, 0.60, max(height_px, width_px));
  if (coverage_fade <= 0.002) {
    gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
    v_color = vec3(0.0);
    v_alpha = 0.0;
    v_edge = 0.0;
    v_edge_softness = 1.0;
    return;
  }

  vec2 plane = a_position.xz * blade_width * widen;
  vec3 world_pos = base_pos + width_axis * plane.x + lean_axis * plane.y +
                   vec3(0.0, a_position.y * blade_height, 0.0) +
                   (width_axis * sin_o + lean_axis * cos_o) * arc * blade_height;

  vec2 out_dir = plane / max(length(plane), 1e-4);
  vec3 splay_normal = width_axis * out_dir.x + lean_axis * out_dir.y;
  float dome = smoothstep(0.0, 2.5 * blade_width, length(plane));
  vec3 normal = normalize(vec3(0.0, 1.0, 0.0) + splay_normal * dome * 0.85);

  vec3 light_dir = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_pos - world_pos);

  float diffuse = clamp(dot(normal, light_dir), 0.0, 1.0);

  float transmission =
      pow(clamp(dot(-view_dir, light_dir), 0.0, 1.0), 3.0) * mix(0.25, 0.75, tip);

  float blade_variation = hash11(sway_phase * 5.3 + 11.7);
  vec3 varied_color = blade_color * mix(0.86, 1.06, blade_variation);

  float root_occlusion = mix(0.82, 1.0, smoothstep(0.0, 0.55, tip));
  vec3 root_tint = mix(u_soil_color, varied_color, 0.86);
  vec3 shaft_color = mix(root_tint, varied_color, smoothstep(0.0, 0.60, tip));

  vec3 lit = shaft_color *
             (environment_ambient_light(normal) +
              environment_primary_color() * environment_primary_intensity() * diffuse) *
             root_occlusion;
  lit += varied_color * environment_primary_color() * transmission * 0.20;

  lit *= mix(0.90, 1.04, tip);

  float exposure =
      environment_exposure() * (u_ambient_boost > 0.001 ? u_ambient_boost : 1.0);
  v_color = lit * exposure;
  v_alpha = coverage_fade;
  v_edge = a_uv.x * 2.0 - 1.0;

  v_edge_softness = clamp(1.0 / max(width_px * widen, 0.35), 0.04, 1.0);

  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
