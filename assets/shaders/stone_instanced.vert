#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_pos_scale;
layout(location = 3) in vec4 a_color_rot;
layout(location = 4) in vec4 a_ground_fit;

layout(std140) uniform FrameData {
  mat4 u_view_proj;
};

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec3 v_local_pos;
out float v_ground_height;
flat out float v_scale;
flat out float v_seed;

float fit_hash(float seed, float salt) {
  return fract(sin(seed * 127.1 + salt * 311.7) * 43758.5453);
}

mat3 align_up_to(vec3 n) {
  vec3 v = vec3(n.z, 0.0, -n.x);
  float c = n.y;
  float k = 1.0 / max(1.0 + c, 1.0e-4);
  vec3 col0 = vec3(
      1.0 - k * (v.y * v.y + v.z * v.z), v.z + k * v.x * v.y, -v.y + k * v.x * v.z);
  vec3 col1 = vec3(
      -v.z + k * v.x * v.y, 1.0 - k * (v.x * v.x + v.z * v.z), v.x + k * v.y * v.z);
  vec3 col2 = vec3(
      v.y + k * v.x * v.z, -v.x + k * v.y * v.z, 1.0 - k * (v.x * v.x + v.y * v.y));
  return mat3(col0, col1, col2);
}

void main() {
  float scale = a_pos_scale.w;
  vec3 world_origin = a_pos_scale.xyz;
  float rotation = a_color_rot.a;
  float seed = a_ground_fit.y;
  float sink = a_ground_fit.w;

  float sx = mix(0.80, 1.24, fit_hash(seed, 1.0));
  float sz = mix(0.80, 1.24, fit_hash(seed, 2.0));
  float sy = mix(0.70, 1.16, fit_hash(seed, 3.0));

  float shear_x = (fit_hash(seed, 4.0) - 0.5) * 0.34;
  float shear_z = (fit_hash(seed, 5.0) - 0.5) * 0.34;

  mat3 shape = mat3(
      vec3(sx, 0.0, 0.0), vec3(shear_x * sx, sy, shear_z * sz), vec3(0.0, 0.0, sz));

  float cos_r = cos(rotation);
  float sin_r = sin(rotation);
  mat3 yaw =
      mat3(vec3(cos_r, 0.0, sin_r), vec3(0.0, 1.0, 0.0), vec3(-sin_r, 0.0, cos_r));

  vec2 fit_xz = a_ground_fit.xz;
  float fit_y = sqrt(max(1.0 - dot(fit_xz, fit_xz), 0.0));
  vec3 ground_normal = vec3(fit_xz.x, fit_y, fit_xz.y);
  vec3 cant =
      vec3((fit_hash(seed, 6.0) - 0.5) * 0.18, 0.0, (fit_hash(seed, 7.0) - 0.5) * 0.18);
  vec3 up = normalize(ground_normal + cant);
  mat3 tilt = align_up_to(up);

  mat3 orient = tilt * yaw;
  mat3 model = orient * shape;
  mat3 normal_shape = transpose(inverse(shape));

  vec3 local_pos = model * a_pos * scale;
  vec3 world_pos = local_pos + world_origin - vec3(0.0, sink, 0.0);

  v_world_pos = world_pos;
  v_normal = normalize(orient * (normal_shape * a_normal));
  v_color = a_color_rot.rgb;
  v_local_pos = a_pos;
  v_ground_height = world_pos.y - world_origin.y;
  v_scale = scale;
  v_seed = seed;

  gl_Position = u_view_proj * vec4(world_pos, 1.0);
}
