#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

layout(location = 3) in vec4 i_center_radius;
layout(location = 4) in vec4 i_color_alpha;
layout(location = 5) in vec4 i_shape;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};

uniform int u_has_height_tex;
uniform sampler2D u_height_tex;
uniform vec2 u_height_uv_scale;
uniform vec2 u_height_uv_offset;
uniform float u_height_to_world;
uniform float u_ground_offset;

uniform float u_angular_step;

uniform vec2 u_height_world_per_texel;

uniform float u_slope_clearance;

uniform float u_max_slope_lift;

uniform vec2 u_half_viewport;
uniform float u_min_marker_pixels;
uniform float u_max_thickness_gain;

out vec2 v_shape_coord;
out vec3 v_color;
out float v_alpha;
out float v_pattern;
out float v_flags;
out float v_phase;

const float k_two_pi = 6.28318530718;

float sample_terrain_height(vec2 world_xz, float fallback) {
  if (u_has_height_tex != 1) {
    return fallback;
  }

  vec2 uv = clamp(world_xz * u_height_uv_scale + u_height_uv_offset, 0.0, 1.0);
  return texture(u_height_tex, uv).r * u_height_to_world;
}

float pixels_per_world(vec3 anchor, vec2 dir) {
  vec4 near_clip = u_view_proj * vec4(anchor, 1.0);
  vec4 far_clip = u_view_proj * vec4(anchor + vec3(dir.x, 0.0, dir.y), 1.0);
  if (near_clip.w <= 0.0001 || far_clip.w <= 0.0001) {
    return 0.0;
  }
  vec2 near_px = (near_clip.xy / near_clip.w) * u_half_viewport;
  vec2 far_px = (far_clip.xy / far_clip.w) * u_half_viewport;
  return length(far_px - near_px);
}

void main() {
  vec3 center = i_center_radius.xyz;
  float outer_radius = i_center_radius.w;
  float thickness = max(i_shape.x, 0.0001);

  float radial = a_tex_coord.y;
  float angle = a_tex_coord.x;

  vec2 dir = vec2(a_position.x, a_position.z);

  float band_thickness = thickness;
  if (u_min_marker_pixels > 0.0 && u_half_viewport.y > 0.0) {
    vec2 anchor_xz = center.xz + dir * outer_radius;
    vec3 anchor =
        vec3(anchor_xz.x, sample_terrain_height(anchor_xz, center.y), anchor_xz.y);
    float scale = pixels_per_world(anchor, dir);
    if (scale > 0.0001) {
      band_thickness = clamp(u_min_marker_pixels / scale,
                             thickness,
                             thickness * max(u_max_thickness_gain, 1.0));
    }
  }

  float world_radius = max(outer_radius + (radial - 1.0) * band_thickness, 0.0);
  vec2 world_xz = center.xz + dir * world_radius;

  float height = sample_terrain_height(world_xz, center.y);
  float lift = u_ground_offset;

  if (u_has_height_tex == 1) {

    float half_step = u_angular_step * 0.5;
    float theta = angle * k_two_pi;
    vec2 behind = vec2(cos(theta - half_step), sin(theta - half_step));
    vec2 ahead = vec2(cos(theta + half_step), sin(theta + half_step));
    height =
        max(height, sample_terrain_height(center.xz + behind * world_radius, center.y));
    height =
        max(height, sample_terrain_height(center.xz + ahead * world_radius, center.y));

    vec2 step_world = max(u_height_world_per_texel, vec2(0.0001));
    float dh_x = sample_terrain_height(world_xz + vec2(step_world.x, 0.0), center.y) -
                 sample_terrain_height(world_xz - vec2(step_world.x, 0.0), center.y);
    float dh_z = sample_terrain_height(world_xz + vec2(0.0, step_world.y), center.y) -
                 sample_terrain_height(world_xz - vec2(0.0, step_world.y), center.y);
    vec2 gradient = vec2(dh_x / (2.0 * step_world.x), dh_z / (2.0 * step_world.y));

    float secant = sqrt(1.0 + dot(gradient, gradient));
    float sine_slope = clamp(length(gradient) / secant, 0.0, 1.0);
    lift = u_ground_offset * min(secant, max(u_max_slope_lift, 1.0)) +
           u_slope_clearance * smoothstep(0.04, 0.40, sine_slope);
  }

  v_shape_coord = vec2(angle, radial);
  v_color = i_color_alpha.rgb;
  v_alpha = i_color_alpha.a;
  v_pattern = i_shape.y;
  v_flags = i_shape.z;
  v_phase = i_shape.w;

  gl_Position = u_view_proj * vec4(world_xz.x, height + lift, world_xz.y, 1.0);
}
