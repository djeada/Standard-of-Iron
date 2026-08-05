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
  vec2 uv = world_xz * u_height_uv_scale + u_height_uv_offset;
  if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
    return fallback;
  }
  return texture(u_height_tex, uv).r * u_height_to_world;
}

void main() {
  vec3 center = i_center_radius.xyz;
  float outer_radius = i_center_radius.w;
  float thickness = max(i_shape.x, 0.0001);

  float radial = a_tex_coord.y;
  float angle = a_tex_coord.x;

  float world_radius = max(outer_radius + (radial - 1.0) * thickness, 0.0);
  vec2 dir = vec2(a_position.x, a_position.z);
  vec2 world_xz = center.xz + dir * world_radius;

  float height = sample_terrain_height(world_xz, center.y);

  v_shape_coord = vec2(angle, radial);
  v_color = i_color_alpha.rgb;
  v_alpha = i_color_alpha.a;
  v_pattern = i_shape.y;
  v_flags = i_shape.z;
  v_phase = i_shape.w;

  gl_Position =
      u_view_proj * vec4(world_xz.x, height + u_ground_offset, world_xz.y, 1.0);
}
