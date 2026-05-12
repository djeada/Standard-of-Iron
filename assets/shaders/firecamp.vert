#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_tex_coord;

layout(location = 3) in vec4 i_pos_intensity;
layout(location = 4) in vec4 i_radius_phase;

layout(std140) uniform FrameData { mat4 u_view_proj; };
uniform float u_time;
uniform float u_flicker_speed;
uniform float u_flicker_amount;
uniform vec3 u_camera_right;
uniform vec3 u_camera_forward;

out vec2 tex_coord;
out float intensity_val;
out float flame_phase;
out float flame_height;

void main() {
  vec3 campPos = i_pos_intensity.xyz;
  float intensity = i_pos_intensity.w;
  float phase = i_radius_phase.y;
  float radius = i_radius_phase.x;

  vec3 rightVec = normalize(vec3(u_camera_right.x, 0.0, u_camera_right.z));
  if (length(rightVec) < 1e-4)
    rightVec = vec3(1.0, 0.0, 0.0);
  vec3 forwardVec = normalize(vec3(u_camera_forward.x, 0.0, u_camera_forward.z));
  if (length(forwardVec) < 1e-4)
    forwardVec = normalize(vec3(-rightVec.z, 0.0, rightVec.x));
  vec3 upVec = vec3(0.0, 1.0, 0.0);

  float planeId = floor(a_pos.z + 0.5);
  float angle = planeId * 2.0943951;
  float c = cos(angle);
  float s = sin(angle);
  vec3 horizontalAxis = normalize(rightVec * c + forwardVec * s);
  if (length(horizontalAxis) < 1e-4)
    horizontalAxis = rightVec;

  float intensityScale = clamp(intensity, 0.65, 1.4);
  float heightT = clamp(a_tex_coord.y, 0.0, 1.0);

  float widthBase = clamp(radius * 0.18 * intensityScale, 0.55, 0.95);
  float widthScale = mix(widthBase, widthBase * 0.35, heightT);
  float heightScale = clamp(radius * 0.24 * intensityScale, 0.55, 1.05);

  float flickerOffset =
      sin(u_time * u_flicker_speed + phase) * (u_flicker_amount * 0.55);
  float sway =
      sin(u_time * (u_flicker_speed * 1.05) + phase * 2.1 + heightT * 2.7);
  vec3 wobbleOffset = horizontalAxis * (sway * u_flicker_amount * radius *
                                        (0.18 + heightT * 0.35));

  vec3 localOffset = horizontalAxis * (a_pos.x * widthScale) +
                     upVec * (a_pos.y * heightScale * (0.85 + heightT * 0.25));

  float taper = mix(0.0, widthBase * 0.25, heightT * heightT);
  localOffset += horizontalAxis * (-a_pos.x * taper);

  float baseLift = radius * 0.02 + intensity * 0.04;
  vec3 pos =
      campPos + localOffset + wobbleOffset + upVec * (flickerOffset + baseLift);

  gl_Position = u_view_proj * vec4(pos, 1.0);
  tex_coord = a_tex_coord;
  intensity_val = intensity;
  flame_phase = phase;
  flame_height = heightT;
}
