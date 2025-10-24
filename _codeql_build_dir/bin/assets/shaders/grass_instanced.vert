#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_posHeight;
layout(location = 3) in vec4 a_colorWidth;
layout(location = 4) in vec4 a_swayParams;

uniform mat4 u_viewProj;
uniform float u_time;
uniform float u_windStrength;
uniform float u_windSpeed;
uniform vec3 u_soilColor;
uniform vec3 u_lightDir;

out vec3 v_color;
out float v_alpha;

void main() {
  vec3 basePos = a_posHeight.xyz;
  float bladeHeight = a_posHeight.w;

  vec3 bladeColor = a_colorWidth.xyz;
  float bladeWidth = a_colorWidth.w;

  float swayStrength = a_swayParams.x * u_windStrength;
  float swaySpeed = a_swayParams.y * u_windSpeed;
  float swayPhase = a_swayParams.z;
  float orientation = a_swayParams.w;

  float tip = clamp(a_uv.y, 0.0, 1.0);
  float sway = sin(u_time * swaySpeed + swayPhase) * swayStrength;
  float bend = smoothstep(0.0, 1.0, tip);
  float swayOffset = sway * bend;

  vec3 localPos = vec3(a_position.x * bladeWidth + swayOffset,
                       a_position.y * bladeHeight, 0.0);

  float sinO = sin(orientation);
  float cosO = cos(orientation);
  vec3 rotated = vec3(localPos.x * cosO - localPos.z * sinO, localPos.y,
                      localPos.x * sinO + localPos.z * cosO);

  vec3 worldPos = basePos + rotated;

  vec3 lightDir = normalize(u_lightDir);
  vec3 normal = normalize(vec3(sinO, 1.6, cosO));
  float lightTerm = clamp(dot(normal, lightDir), 0.0, 1.0);
  float tipHighlight = mix(0.7, 1.0, tip);
  vec3 soilBlend = mix(u_soilColor, bladeColor, tip);
  v_color = soilBlend * (0.7 + 0.3 * lightTerm) * tipHighlight;

  float edgeFade = 1.0 - smoothstep(0.35, 0.5, abs(a_uv.x - 0.5));
  v_alpha = clamp(0.35 + 0.45 * tip, 0.25, 0.85) * edgeFade;

  gl_Position = u_viewProj * vec4(worldPos, 1.0);
}
