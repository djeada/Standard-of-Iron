#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_pos_scale;
layout(location = 3) in vec4 a_color_rot;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};

uniform float u_time;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec3 v_local_pos;
out vec3 v_local_normal;
flat out float v_material;
flat out vec2 v_rot;
flat out vec2 v_anchor;

const float k_canvas_threshold = 0.91;
const float k_lining_threshold = 0.73;

const float k_half_depth = 0.58;
const float k_ridge_height = 0.86;
const float k_awning_extent = 0.26;

void main() {
  float scale = a_pos_scale.w;
  vec3 world_pos = a_pos_scale.xyz;
  float rotation = a_color_rot.a;

  float material = length(a_normal);
  vec3 local_normal = a_normal / max(material, 1.0e-4);

  float canvas = step(k_lining_threshold, material);
  float outward_sign = material > k_canvas_threshold ? 1.0 : -1.0;
  float height_free =
      smoothstep(0.0, 0.12, a_pos.y) *
      (1.0 - smoothstep(k_ridge_height - 0.14, k_ridge_height, a_pos.y));
  float span_free =
      sin(3.14159 * clamp((a_pos.z + k_half_depth) / (2.0 * k_half_depth), 0.0, 1.0));
  float awning_free = smoothstep(0.0, k_awning_extent, -(a_pos.z + k_half_depth));
  float freedom = canvas * max(height_free * span_free, awning_free);
  float phase = dot(world_pos.xz, vec2(0.37, 0.53)) + a_pos.y * 4.0 + a_pos.z * 3.0;
  float gust = sin(u_time * 1.9 + phase) * 0.6 + sin(u_time * 3.7 + phase * 1.7) * 0.4;
  vec3 local_pos = a_pos + local_normal * (outward_sign * gust * freedom * 0.006);

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat2 rot = mat2(cos_r, -sin_r, sin_r, cos_r);

  local_pos *= scale;
  vec2 rotated_xz = rot * local_pos.xz;
  local_pos = vec3(rotated_xz.x, local_pos.y, rotated_xz.y);

  v_world_pos = local_pos + world_pos;

  vec2 rotated_normal_xz = rot * local_normal.xz;
  v_normal = normalize(vec3(rotated_normal_xz.x, local_normal.y, rotated_normal_xz.y));

  v_color = a_color_rot.rgb;
  v_local_pos = a_pos;
  v_local_normal = local_normal;
  v_material = material;
  v_rot = vec2(cos_r, sin_r);
  v_anchor = world_pos.xz;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
