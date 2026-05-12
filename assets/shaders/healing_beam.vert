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

vec3 bezierArc(float t, vec3 start, vec3 end, float arcHeight) {
  vec3 mid = (start + end) * 0.5;
  mid.y += arcHeight;

  vec3 p0 = mix(start, mid, t);
  vec3 p1 = mix(mid, end, t);
  return mix(p0, p1, t);
}

vec3 bezierTangent(float t, vec3 start, vec3 end, float arcHeight) {
  vec3 mid = (start + end) * 0.5;
  mid.y += arcHeight;

  vec3 d0 = mid - start;
  vec3 d1 = end - mid;
  return normalize(mix(d0, d1, t));
}

void main() {

  float t = a_position.z;
  v_beam_t = t;

  float visibleT = min(t, u_progress * 1.2);

  float dist = length(u_end_pos - u_start_pos);
  float arcHeight = dist * 0.25;

  vec3 beamCenter = bezierArc(visibleT, u_start_pos, u_end_pos, arcHeight);
  vec3 tangent = bezierTangent(visibleT, u_start_pos, u_end_pos, arcHeight);

  vec3 up = vec3(0.0, 1.0, 0.0);
  if (abs(dot(tangent, up)) > 0.99) {
    up = vec3(1.0, 0.0, 0.0);
  }
  vec3 right = normalize(cross(tangent, up));
  vec3 localUp = normalize(cross(right, tangent));

  float spiralAngle = t * 12.56637 + u_time * 4.0;
  float spiralRadius = u_beam_width * (0.3 + 0.7 * sin(t * 3.14159));

  vec3 spiralOffset = right * cos(spiralAngle) * spiralRadius +
                      localUp * sin(spiralAngle) * spiralRadius;

  float sectionRadius = u_beam_width * (1.0 - abs(a_position.x));
  vec3 sectionOffset = right * a_position.x * sectionRadius +
                       localUp * a_position.y * sectionRadius;

  vec3 worldPos = beamCenter + sectionOffset * 0.3 + spiralOffset * 0.7;

  float wave = sin(t * 20.0 - u_time * 8.0) * 0.02 * u_beam_width;
  worldPos += localUp * wave;

  v_world_pos = worldPos;

  v_edge_dist =
      clamp(length(sectionOffset) / max(u_beam_width, 0.0001), 0.0, 1.0);

  float pulse = 0.7 + 0.3 * sin(u_time * 6.0 + t * 10.0);
  v_glow_intensity = (1.0 - v_edge_dist) * pulse;

  if (t > u_progress * 1.1) {
    v_glow_intensity *= 0.0;
  }

  v_normal = a_normal;
  v_tex_coord = vec2(t, a_position.x * 0.5 + 0.5);

  gl_Position = u_mvp * vec4(worldPos, 1.0);
}
