#version 330 core
#include "environment_lighting.glsl"
#include "ground_readability.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_uv;
layout(location = 2) in vec4 a_pos_height;
layout(location = 3) in vec4 a_color_width;
layout(location = 4) in vec4 a_sway_params;

#include "frame_data.glsl"
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;
uniform vec3 u_soil_color;
uniform vec2 u_viewport_size;
uniform vec3 u_camera_pos;
uniform float u_ambient_boost;
uniform int u_has_cover_tex;
uniform sampler2D u_cover_tex;
uniform vec2 u_cover_uv_scale;
uniform vec2 u_cover_uv_offset;

out vec3 v_color;
out vec3 v_world_pos;
out float v_alpha;
out float v_edge;
out float v_edge_softness;

float hash11(float x) {
  return fract(sin(x * 78.233) * 43758.5453123);
}

const float k_kind_stride = 4.0;
const float k_built_ground_clear_start = 0.06;
const float k_built_ground_clear_end = 0.40;
const float k_flower_stem_top = 0.40;
const float k_flower_petal_mid = 0.74;
const vec3 k_flower_stem_color = vec3(0.21, 0.34, 0.13);
const vec3 k_flower_dark_eye = vec3(0.09, 0.06, 0.08);
const vec3 k_flower_gold_eye = vec3(0.96, 0.74, 0.16);
const vec3 k_blade_tip_gain = vec3(1.20, 1.19, 0.96);
const vec3 k_blade_body_gain = vec3(1.16, 1.18, 1.06);
const float k_blade_root_shade = 0.66;

void main() {
  vec3 base_pos = a_pos_height.xyz;
  float blade_height = a_pos_height.w;

  vec3 blade_color = a_color_width.xyz;
  float blade_width = a_color_width.w;

  if (u_has_cover_tex == 1) {
    vec4 cover = textureLod(
        u_cover_tex, base_pos.xz * u_cover_uv_scale + u_cover_uv_offset, 0.0);
    float built_ground =
        max(clamp(cover.g - 1.0, 0.0, 1.0), clamp(cover.a - 1.0, 0.0, 1.0));
    float keep =
        1.0 -
        smoothstep(k_built_ground_clear_start, k_built_ground_clear_end, built_ground);
    if (keep <= 0.02) {
      gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
      v_color = vec3(0.0);
      v_world_pos = base_pos;
      v_alpha = 0.0;
      v_edge = 0.0;
      v_edge_softness = 1.0;
      return;
    }
    blade_height *= keep;
    blade_width *= mix(0.55, 1.0, keep);
  }

  float kind = floor(a_sway_params.x / k_kind_stride + 0.01);
  bool is_flower = kind > 0.5;
  float sway_strength = (a_sway_params.x - kind * k_kind_stride) * u_wind_strength;
  float sway_speed = a_sway_params.y * u_wind_speed;
  float sway_phase = a_sway_params.z;
  float orientation = a_sway_params.w;

  float ring = clamp(a_uv.y, 0.0, 1.0);
  float tip = is_flower ? smoothstep(0.0, k_flower_stem_top, ring) : ring;
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

  const float k_min_width_px = 0.85;
  const float k_min_height_px = 1.20;
  const float k_min_head_px = 1.55;
  float min_width = is_flower ? k_min_head_px : k_min_width_px;
  float widen = clamp(min_width / max(width_px, 1e-4), 1.0, is_flower ? 3.0 : 6.0);

  float heighten = clamp(k_min_height_px / max(height_px, 1e-4), 1.0, 4.0);
  blade_height *= heighten;

  float tactical = ground_tactical_distance(length(u_camera_pos - base_pos));
  float coverage_fade = smoothstep(0.35, 1.40, max(height_px, width_px * widen));
  coverage_fade *= mix(1.0, is_flower ? 0.80 : 0.45, tactical);
  if (coverage_fade <= 0.002) {
    gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
    v_color = vec3(0.0);
    v_alpha = 0.0;
    v_edge = 0.0;
    v_edge_softness = 1.0;
    return;
  }

  vec2 blade_dir = a_uv.zw;
  vec2 blade_right = vec2(-blade_dir.y, blade_dir.x);
  float along = dot(a_position.xz, blade_dir);
  float across = dot(a_position.xz, blade_right);

  vec2 plane;
  float lift;
  float taper;
  vec3 surface_color;
  float root_occlusion;
  if (is_flower) {

    float head = smoothstep(k_flower_stem_top, k_flower_petal_mid, ring);
    float petal_tip = smoothstep(k_flower_petal_mid, 1.0, ring);
    float reach = (0.58 * head + 0.42 * petal_tip) * blade_width * widen;
    float petal_half_width =
        blade_width * widen * mix(0.07, mix(0.36, 0.13, petal_tip), head);
    plane = blade_dir * reach + blade_right * sign(across) * petal_half_width;
    lift = blade_height * mix(0.94 * tip, 1.0 + 0.10 * petal_tip, head);
    taper = mix(0.30, 1.0, head);
    vec3 eye = (blade_color.r > blade_color.g * 1.45 && blade_color.r > blade_color.b)
                   ? k_flower_dark_eye
                   : k_flower_gold_eye;
    vec3 petal = blade_color * mix(0.90, 1.08, petal_tip);
    surface_color = mix(k_flower_stem_color,
                        mix(eye, petal, smoothstep(0.0, 0.55, head)),
                        step(k_flower_stem_top - 0.01, ring));
    root_occlusion = mix(0.80, 1.0, tip);
  } else {
    plane = (blade_dir * along + blade_right * across * widen) * blade_width;
    lift = a_position.y * blade_height;
    taper = max(1.0 - 0.86 * pow(tip, 1.35), 0.06);
    float blade_variation =
        hash11(sway_phase * 5.3 + 11.7 + dot(blade_dir, vec2(3.1, 7.7)));
    vec3 varied_color =
        blade_color * k_blade_body_gain * mix(0.86, 1.10, blade_variation);
    vec3 root_color = mix(u_soil_color, varied_color, 0.62) * k_blade_root_shade;
    vec3 tip_color = varied_color * k_blade_tip_gain;
    surface_color = mix(root_color, varied_color, smoothstep(0.0, 0.50, tip));
    surface_color = mix(surface_color, tip_color, smoothstep(0.45, 1.0, tip));
    root_occlusion = mix(0.74, 1.0, smoothstep(0.0, 0.55, tip));
  }

  vec3 world_pos = base_pos + width_axis * plane.x + lean_axis * plane.y +
                   vec3(0.0, lift, 0.0) +
                   (width_axis * sin_o + lean_axis * cos_o) * arc * blade_height;

  vec2 out_dir = plane / max(length(plane), 1e-4);
  vec3 splay_normal = width_axis * out_dir.x + lean_axis * out_dir.y;
  float dome = smoothstep(0.0, 2.5 * blade_width, length(plane));
  vec3 normal = is_flower ? vec3(0.0, 1.0, 0.0)
                          : normalize(vec3(0.0, 1.0, 0.0) + splay_normal * dome * 0.85);

  vec3 light_dir = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_pos - world_pos);

  float diffuse = clamp(dot(normal, light_dir), 0.0, 1.0);

  float transmission =
      pow(clamp(dot(-view_dir, light_dir), 0.0, 1.0), 3.0) * mix(0.25, 0.75, tip);

  vec3 lit = surface_color *
             (environment_ambient_light(normal) +
              environment_primary_color() * environment_primary_intensity() * diffuse) *
             root_occlusion;
  lit += surface_color * environment_primary_color() * transmission * 0.20;

  float exposure =
      environment_exposure() * (u_ambient_boost > 0.001 ? u_ambient_boost : 1.0);
  v_color = lit * exposure;
  v_alpha = coverage_fade;
  v_edge = a_uv.x * 2.0 - 1.0;

  v_edge_softness = clamp(1.0 / max(width_px * widen * taper, 0.35), 0.04, 1.0);

  v_world_pos = world_pos;
  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
