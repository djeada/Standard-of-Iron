#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_tex_coord;

uniform mat4 u_mvp;
uniform float u_time;
uniform float u_progress;
uniform vec3 u_start_pos;
uniform vec3 u_end_pos;
uniform float u_beam_width;

out vec3 v_world_pos;
out vec3 v_normal;
out vec2 v_tex_coord;
out float v_beam_t;
out float v_edge_dist;
out float v_glow_intensity;

vec3 bezier_arc(float t, vec3 start, vec3 end, float arc_height) {
  vec3 mid = (start + end) * 0.5;
  mid.y += arc_height;

  vec3 p0 = mix(start, mid, t);
  vec3 p1 = mix(mid, end, t);
  return mix(p0, p1, t);
}

vec3 bezier_tangent(float t, vec3 start, vec3 end, float arc_height) {
  vec3 mid = (start + end) * 0.5;
  mid.y += arc_height;

  vec3 d0 = mid - start;
  vec3 d1 = end - mid;
  return normalize(mix(d0, d1, t));
}

void main() {

  float t = a_position.z;
  v_beam_t = t;

  float visible_t = min(t, u_progress * 1.2);

  float dist = length(u_end_pos - u_start_pos);
  float arc_height = dist * 0.25;

  vec3 beam_center = bezier_arc(visible_t, u_start_pos, u_end_pos, arc_height);
  vec3 tangent = bezier_tangent(visible_t, u_start_pos, u_end_pos, arc_height);

  vec3 up = vec3(0.0, 1.0, 0.0);
  if (abs(dot(tangent, up)) > 0.99) {
    up = vec3(1.0, 0.0, 0.0);
  }
  vec3 right = normalize(cross(tangent, up));
  vec3 local_up = normalize(cross(right, tangent));

  float spiral_angle = t * 12.56637 + u_time * 4.0;
  float spiral_radius = u_beam_width * (0.3 + 0.7 * sin(t * 3.14159));

  vec3 spiral_offset = right * cos(spiral_angle) * spiral_radius +
                      local_up * sin(spiral_angle) * spiral_radius;

  float section_radius = u_beam_width * (1.0 - abs(a_position.x));
  vec3 section_offset = right * a_position.x * section_radius +
                       local_up * a_position.y * section_radius;

  vec3 world_pos = beam_center + section_offset * 0.3 + spiral_offset * 0.7;

  float wave = sin(t * 20.0 - u_time * 8.0) * 0.02 * u_beam_width;
  world_pos += local_up * wave;

  v_world_pos = world_pos;

  v_edge_dist =
      clamp(length(section_offset) / max(u_beam_width, 0.0001), 0.0, 1.0);

  float pulse = 0.7 + 0.3 * sin(u_time * 6.0 + t * 10.0);
  v_glow_intensity = (1.0 - v_edge_dist) * pulse;

  if (t > u_progress * 1.1) {
    v_glow_intensity *= 0.0;
  }

  v_normal = a_normal;
  v_tex_coord = vec2(t, a_position.x * 0.5 + 0.5);

  gl_Position = u_mvp * vec4(world_pos, 1.0);
}
