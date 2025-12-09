#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

layout(location = 3) in vec4 i_posIntensity;
layout(location = 4) in vec4 i_radiusPhase;

uniform mat4 u_viewProj;
uniform float u_time;
uniform float u_flickerSpeed;
uniform float u_flickerAmount;
uniform vec3 u_cameraRight;
uniform vec3 u_cameraForward;

out vec2 TexCoord;
out float Intensity;
out float FlamePhase;
out float FlameHeight;

void main() {
  vec3 campPos = i_posIntensity.xyz;
  float intensity = i_posIntensity.w;
  float phase = i_radiusPhase.y;
  float radius = i_radiusPhase.x;

  vec3 rightVec = normalize(vec3(u_cameraRight.x, 0.0, u_cameraRight.z));
  if (length(rightVec) < 1e-4)
    rightVec = vec3(1.0, 0.0, 0.0);
  vec3 forwardVec = normalize(vec3(u_cameraForward.x, 0.0, u_cameraForward.z));
  if (length(forwardVec) < 1e-4)
    forwardVec = normalize(vec3(-rightVec.z, 0.0, rightVec.x));
  vec3 upVec = vec3(0.0, 1.0, 0.0);

  float planeId = floor(aPos.z + 0.5);
  float angle = planeId * 2.0943951;
  float c = cos(angle);
  float s = sin(angle);
  vec3 horizontalAxis = normalize(rightVec * c + forwardVec * s);
  if (length(horizontalAxis) < 1e-4)
    horizontalAxis = rightVec;

  float intensityScale = clamp(intensity, 0.65, 1.4);
  float heightT = clamp(aTexCoord.y, 0.0, 1.0);

  float widthBase = clamp(radius * 0.18 * intensityScale, 0.55, 0.95);
  float widthScale = mix(widthBase, widthBase * 0.35, heightT);
  float heightScale = clamp(radius * 0.24 * intensityScale, 0.55, 1.05);

  float flickerOffset =
      sin(u_time * u_flickerSpeed + phase) * (u_flickerAmount * 0.55);
  float sway =
      sin(u_time * (u_flickerSpeed * 1.05) + phase * 2.1 + heightT * 2.7);
  vec3 wobbleOffset = horizontalAxis * (sway * u_flickerAmount * radius *
                                        (0.18 + heightT * 0.35));

  vec3 localOffset = horizontalAxis * (aPos.x * widthScale) +
                     upVec * (aPos.y * heightScale * (0.85 + heightT * 0.25));

  float taper = mix(0.0, widthBase * 0.25, heightT * heightT);
  localOffset += horizontalAxis * (-aPos.x * taper);

  float baseLift = radius * 0.02 + intensity * 0.04;
  vec3 pos =
      campPos + localOffset + wobbleOffset + upVec * (flickerOffset + baseLift);

  gl_Position = u_viewProj * vec4(pos, 1.0);
  TexCoord = aTexCoord;
  Intensity = intensity;
  FlamePhase = phase;
  FlameHeight = heightT;
}
