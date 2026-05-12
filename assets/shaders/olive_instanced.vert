#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec4 a_pos_scale;
layout(location = 4) in vec4 a_color_sway;
layout(location = 5) in vec4 a_rotation;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;
uniform float u_wind_strength;
uniform float u_wind_speed;

out vec3 v_world_pos;
out vec3 v_normal;
out vec3 v_color;
out vec2 v_tex_coord;
out float v_foliage_mask;
out float v_leaf_seed;
out float v_bark_seed;
out float v_branch_id;
out vec2 v_local_pos_xz;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = a_pos_scale.w;
  vec3 worldPos = a_pos_scale.xyz;
  float swayPhase = a_color_sway.a;
  float rotation = a_rotation.x;
  float silhouetteSeed = a_rotation.y;
  float leafSeed = a_rotation.z;
  float barkSeed = a_rotation.w;

  vec3 modelPos = a_pos;

  float trunkMask = 1.0 - smoothstep(0.12, 0.20, a_tex_coord.y);
  float foliageMask = smoothstep(0.45, 0.55, a_tex_coord.y);

  float angle = a_tex_coord.x * TWO_PI;
  float branchId = floor(angle / TWO_PI * 4.0 + silhouetteSeed * 4.0);

  if (trunkMask > 0.0) {
    float twist = sin(a_tex_coord.y * 20.0 + barkSeed * TWO_PI) * 0.02;
    modelPos.x += twist * trunkMask;
    modelPos.z += twist * 0.7 * trunkMask;
  }

  float heightNorm = clamp(a_pos.y / 0.55, 0.0, 1.0);
  float leanAngle = (silhouetteSeed - 0.5) * 0.22;
  float leanYaw = barkSeed * TWO_PI;
  modelPos.x += cos(leanYaw) * leanAngle * heightNorm * heightNorm;
  modelPos.z += sin(leanYaw) * leanAngle * heightNorm * heightNorm;

  if (foliageMask > 0.1) {
    float ang = atan(modelPos.z, modelPos.x);
    float lumpBase = sin(ang * 3.0 + silhouetteSeed * TWO_PI) * 0.22;
    float lumpFine = sin(ang * 5.0 + leafSeed * TWO_PI * 1.7) * 0.10;
    float lumpMag = (lumpBase + lumpFine) * foliageMask;
    modelPos.xz *= (1.0 + lumpMag);

    float stretch = mix(0.78, 1.28, leafSeed);
    modelPos.y *= mix(1.0, stretch, foliageMask);
  }

  vec3 localPos = modelPos * scale;

  float heightFactor = clamp(a_pos.y * 2.0, 0.0, 1.0);
  float windTime = u_time * u_wind_speed * 0.4;
  float sway = sin(windTime + swayPhase) * u_wind_strength * 0.3;
  float sway2 = sin(windTime * 1.7 + swayPhase * 2.3) * u_wind_strength * 0.15;

  float swayAmount = mix(0.02, 0.12, foliageMask) * heightFactor;
  localPos.x += sway * swayAmount;
  localPos.z += sway2 * swayAmount * 0.6;

  float cosR = cos(rotation);
  float sinR = sin(rotation);
  mat2 rot = mat2(cosR, -sinR, sinR, cosR);

  vec2 rotatedXZ = rot * localPos.xz;
  localPos = vec3(rotatedXZ.x, localPos.y, rotatedXZ.y);

  vec2 rotatedNormalXZ = rot * a_normal.xz;
  vec3 finalNormal =
      normalize(vec3(rotatedNormalXZ.x, a_normal.y, rotatedNormalXZ.y));

  v_world_pos = localPos + worldPos;
  v_normal = finalNormal;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord;
  v_foliage_mask = foliageMask;
  v_leaf_seed = leafSeed;
  v_bark_seed = barkSeed;
  v_branch_id = branchId;
  v_local_pos_xz = modelPos.xz;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
