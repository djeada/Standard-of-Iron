#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_pos_scale;
layout(location = 4) in vec4 a_color_sway;
layout(location = 5) in vec4 a_type_params;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_tex_coord;
out float v_alpha;
out float v_height;
out float v_seed;
out float v_type;
out vec3 v_tangent;
out vec3 v_bitangent;

float h11(float n) { return fract(sin(n) * 43758.5453123); }

float h31(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

void main() {
  float scale = a_pos_scale.w;
  vec3 worldOrigin = a_pos_scale.xyz;
  float rotation = a_type_params.y;
  float swayStrength = a_type_params.z;
  float swaySpeed = a_type_params.w;
  float swayPhase = a_color_sway.a;

  float seed = h31(worldOrigin * 0.173 + vec3(0.27, 0.49, 0.19));

  const float SIZE_MULT = 1.85;
  float sizeJitter = mix(0.90, 1.45, h11(seed * 3.9));
  float finalScale = scale * SIZE_MULT * sizeJitter;

  vec3 localPos = a_pos * finalScale;
  float h = clamp(a_pos.y, 0.0, 1.0);

  float leanAngle = (h11(seed * 2.1) - 0.5) * 0.18;
  float leanYaw = h11(seed * 3.7) * 6.28318;
  vec2 leanDir = vec2(cos(leanYaw), sin(leanYaw));
  localPos.xz += leanDir * (h * h) * tan(leanAngle) * finalScale;

  float gust = sin(u_time * 0.35 + seed * 6.0) * 0.5 + 0.5;
  float sway = sin(u_time * swaySpeed * u_wind_speed + swayPhase + seed * 4.0);
  sway *= (0.22 + 0.55 * gust) * swayStrength * u_wind_strength * pow(h, 1.25);

  float windYaw = seed * 9.0;
  vec2 windDir = normalize(vec2(cos(windYaw), sin(windYaw)) + vec2(0.6, 0.8));
  localPos.xz += windDir * (0.10 * sway);

  float twist = (h11(seed * 5.5) - 0.5) * 0.30;
  float twistAngle = twist * h;
  mat2 tw =
      mat2(cos(twistAngle), -sin(twistAngle), sin(twistAngle), cos(twistAngle));
  localPos.xz = tw * localPos.xz;

  float cs = cos(rotation), sn = sin(rotation);
  mat2 rot = mat2(cs, -sn, sn, cs);
  localPos.xz = rot * localPos.xz;

  v_world_pos = localPos + worldOrigin;

  vec3 n = a_normal;
  n.xz = tw * n.xz;
  n.xz = rot * n.xz;
  v_normal = normalize(n);

  vec3 t = vec3(1.0, 0.0, 0.0);
  vec3 b = vec3(0.0, 0.0, 1.0);
  t.xz = tw * t.xz;
  b.xz = tw * b.xz;
  t.xz = rot * t.xz;
  b.xz = rot * b.xz;

  v_tangent = normalize(t);
  v_bitangent = normalize(b);

  v_height = h;
  v_seed = seed;
  v_type = a_type_params.x;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord;

  v_alpha = 1.0 - smoothstep(0.49, 0.56, abs(a_tex_coord.x - 0.5));

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
