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

float sample_terrain_height(vec2 world_xz, float fallback) {
  if (u_has_height_tex != 1) {
    return fallback;
  }

  ivec2 size = textureSize(u_height_tex, 0);
  if (size.x < 2 || size.y < 2) {
    return texelFetch(u_height_tex, ivec2(0), 0).r * u_height_to_world;
  }
  vec2 uv = world_xz * u_height_uv_scale + u_height_uv_offset;
  vec2 grid = clamp(uv * vec2(size) - 0.5, vec2(0.0), vec2(size - ivec2(1)));
  ivec2 cell = min(ivec2(floor(grid)), size - ivec2(2));
  vec2 t = grid - vec2(cell);
  float h10 = texelFetch(u_height_tex, cell + ivec2(1, 0), 0).r;
  float h01 = texelFetch(u_height_tex, cell + ivec2(0, 1), 0).r;
  float h;
  if (t.x + t.y <= 1.0) {
    float h00 = texelFetch(u_height_tex, cell, 0).r;
    h = h00 + (h10 - h00) * t.x + (h01 - h00) * t.y;
  } else {
    float h11 = texelFetch(u_height_tex, cell + ivec2(1, 1), 0).r;
    h = h11 + (h01 - h11) * (1.0 - t.x) + (h10 - h11) * (1.0 - t.y);
  }
  return h * u_height_to_world;
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

  float center_ground = sample_terrain_height(center.xz, center.y);
  float height = max(center.y, center_ground);

  bool drape = u_has_height_tex == 1 && center.y <= center_ground + 0.25;

  float band_thickness = thickness;
  if (u_min_marker_pixels > 0.0 && u_half_viewport.y > 0.0) {
    vec2 anchor_xz = center.xz + dir * outer_radius;
    vec3 anchor = vec3(anchor_xz.x, height, anchor_xz.y);
    float scale = pixels_per_world(anchor, dir);
    if (scale > 0.0001) {
      band_thickness = clamp(u_min_marker_pixels / scale,
                             thickness,
                             thickness * max(u_max_thickness_gain, 1.0));
    }
  }

  float world_radius = max(outer_radius + (radial - 1.0) * band_thickness, 0.0);
  vec2 world_xz = center.xz + dir * world_radius;

  if (drape) {
    height = sample_terrain_height(world_xz, center_ground);
  }

  float lift = u_ground_offset;

  if (u_has_height_tex == 1) {

    vec2 step_world = max(u_height_world_per_texel, vec2(0.0001));
    vec2 probe = drape ? world_xz : center.xz;
    float dh_x = sample_terrain_height(probe + vec2(step_world.x, 0.0), center.y) -
                 sample_terrain_height(probe - vec2(step_world.x, 0.0), center.y);
    float dh_z = sample_terrain_height(probe + vec2(0.0, step_world.y), center.y) -
                 sample_terrain_height(probe - vec2(0.0, step_world.y), center.y);
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
