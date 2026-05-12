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
out float v_needle_seed;
out float v_bark_seed;

void main() {
  const float TWO_PI = 6.2831853;

  float scale = a_pos_scale.w;
  vec3 worldPos = a_pos_scale.xyz;
  float swayPhase = a_color_sway.a;
  float rotation = a_rotation.x;
  float silhouetteSeed = a_rotation.y;
  float needleSeed = a_rotation.z;
  float barkSeed = a_rotation.w;

  vec3 modelPos = a_pos;

  float foliageMask = smoothstep(0.34, 0.42, a_tex_coord.y);
  float tipMask = smoothstep(0.88, 1.02, a_tex_coord.y);
  float angle = a_tex_coord.x * TWO_PI;

  float aspectW = mix(0.72, 1.38, needleSeed);
  float aspectH = mix(1.12, 0.84, needleSeed);
  modelPos.xz *= mix(1.0, aspectW, foliageMask);
  modelPos.y *= mix(1.0, aspectH, foliageMask);

  float irregularBase = sin(angle * 3.0 + silhouetteSeed * TWO_PI);
  float irregularMid = sin(angle * 5.0 + silhouetteSeed * TWO_PI * 2.3 + 1.1);
  float irregularFine = sin(angle * 7.0 + silhouetteSeed * TWO_PI * 3.7 + 2.3);
  float irregular =
      (irregularBase * 0.22 + irregularMid * 0.10 + irregularFine * 0.05) *
      foliageMask * (1.0 - tipMask * 0.5);

  modelPos.xz *= (1.0 + irregular);

  float droop = foliageMask * (1.0 - tipMask) * 0.12;
  modelPos.y -= droop;

  float leanNorm = clamp(a_pos.y / 1.13, 0.0, 1.0);
  float leanAngle = (silhouetteSeed - 0.5) * 0.18;
  float leanYaw = barkSeed * TWO_PI;
  modelPos.x += cos(leanYaw) * leanAngle * leanNorm;
  modelPos.z += sin(leanYaw) * leanAngle * leanNorm;

  float heightFactor = clamp(modelPos.y, 0.0, 1.1);
  vec3 localPos = modelPos * scale;

  float sway = sin(u_time * u_wind_speed * 0.5 + swayPhase) * u_wind_strength * 0.8 *
               heightFactor * heightFactor;

  float swayInfluence = mix(0.04, 0.12, foliageMask);
  localPos.x += sway * swayInfluence;

  localPos.y -= sway * 0.02 * foliageMask;

  vec3 localNormal = a_normal;
  if (foliageMask > 0.0) {
    float normalScale = 1.0 + irregular;
    localNormal = normalize(vec3(localNormal.x * normalScale,
                                 localNormal.y - foliageMask * 0.2,
                                 localNormal.z * normalScale));
  }

  float cosR = cos(rotation);
  float sinR = sin(rotation);
  mat2 rot = mat2(cosR, -sinR, sinR, cosR);

  vec2 rotatedXZ = rot * localPos.xz;
  localPos = vec3(rotatedXZ.x, localPos.y, rotatedXZ.y);

  vec2 rotatedNormalXZ = rot * localNormal.xz;
  vec3 finalNormal =
      normalize(vec3(rotatedNormalXZ.x, localNormal.y, rotatedNormalXZ.y));

  v_world_pos = localPos + worldPos;
  v_normal = finalNormal;
  v_color = a_color_sway.rgb;
  v_tex_coord = a_tex_coord;
  v_foliage_mask = foliageMask;
  v_needle_seed = needleSeed;
  v_bark_seed = barkSeed;

  gl_Position = u_view_proj * vec4(v_world_pos, 1.0);
}
