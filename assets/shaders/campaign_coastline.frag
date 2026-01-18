#version 330 core

in vec2 v_uv;
in float v_distance;
in vec2 v_world_uv;

uniform vec4 u_outer_color;
uniform vec4 u_inner_color;
uniform float u_outer_width;
uniform float u_inner_width;
uniform float u_total_width;

uniform float u_smoothing;
uniform float u_ink_variation;

out vec4 fragColor;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float valueNoise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);

  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));

  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p, int octaves) {
  float value = 0.0;
  float amplitude = 0.5;

  for (int i = 0; i < octaves; i++) {
    value += amplitude * valueNoise(p);
    amplitude *= 0.5;
    p *= 2.0;
  }

  return value;
}

float getInkVariation(vec2 uv, float distance) {
  if (u_ink_variation <= 0.0)
    return 0.0;

  float variation = fbm(vec2(distance * 50.0, uv.y * 100.0), 3);
  return (variation - 0.5) * u_ink_variation * 0.1;
}

void main() {

  float crossPos = abs(v_uv.y);

  float inkOffset = getInkVariation(v_world_uv, v_distance);
  crossPos += inkOffset;

  float halfTotal = u_total_width * 0.5;
  float innerStart = (halfTotal - u_inner_width) / halfTotal;
  float outerStart = (halfTotal - u_outer_width) / halfTotal;

  float outerMask =
      smoothstep(outerStart - u_smoothing, outerStart + u_smoothing, crossPos);

  float innerEnd = outerStart;
  float innerMask =
      smoothstep(innerStart - u_smoothing, innerStart + u_smoothing, crossPos) *
      (1.0 -
       smoothstep(innerEnd - u_smoothing, innerEnd + u_smoothing, crossPos));

  float edgeFade = 1.0 - smoothstep(1.0 - u_smoothing * 2.0, 1.0, crossPos);

  vec4 color = vec4(0.0);

  color = mix(color, u_outer_color, outerMask * edgeFade);

  color = mix(color, u_inner_color, innerMask * edgeFade);

  float inkTexture = 0.95 + valueNoise(v_world_uv * 200.0) * 0.05;
  color.a *= inkTexture;

  fragColor = color;
}
