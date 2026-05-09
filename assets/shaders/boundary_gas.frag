#version 330 core

in vec2 v_texCoord;
in vec3 v_worldPos;
in vec3 v_worldNormal;
in float v_sheetDrift;

uniform vec3 u_color;
uniform float u_alpha;
uniform float u_time;

out vec4 FragColor;

float hash(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
  vec2 flowA = vec2(u_time * 0.018, -u_time * 0.031);
  vec2 flowB = vec2(-u_time * 0.024, u_time * 0.013);

  float lowFreq = noise(v_worldPos.xz * 0.045 + flowA + v_texCoord.yy * 0.9);
  float midFreq = noise(v_worldPos.xz * 0.090 + flowB +
                        vec2(v_texCoord.x, v_texCoord.y * 1.4));
  float verticalRipples =
      noise(vec2(v_worldPos.y * 0.18 + u_time * 0.06, v_worldPos.x * 0.05));

  float densityField = lowFreq * 0.55 + midFreq * 0.35 + verticalRipples * 0.10;

  float wisps = smoothstep(0.36, 0.84,
                           densityField + v_texCoord.y * 0.18 -
                               abs(v_texCoord.x - 0.5) * 0.12);

  float edgeFade = smoothstep(0.0, 0.12, v_texCoord.x) *
                   smoothstep(0.0, 0.12, 1.0 - v_texCoord.x);
  float baseFade = smoothstep(0.0, 0.05, v_texCoord.y);
  float topFade = 1.0 - smoothstep(0.62, 1.0, v_texCoord.y);
  float heightFade = baseFade * (0.65 + 0.35 * topFade);

  float movingBands = 0.75 + 0.25 * sin(v_worldPos.y * 0.22 + u_time * 0.70 +
                                        densityField * 4.0);
  float frontLight = 0.80 + 0.20 * max(dot(normalize(v_worldNormal),
                                           normalize(vec3(0.4, 0.7, 0.5))),
                                       0.0);

  float alpha =
      u_alpha * edgeFade * heightFade * movingBands * (0.22 + 0.78 * wisps);
  alpha *= 0.82 + 0.18 * smoothstep(-0.25, 0.25, v_sheetDrift);

  vec3 darkColor = u_color * vec3(0.78, 0.80, 0.82);
  vec3 lightColor = u_color * vec3(1.18, 1.20, 1.24);
  vec3 color = mix(darkColor, lightColor, densityField);
  color *= frontLight;
  color += vec3(0.035, 0.040, 0.038) * wisps;

  FragColor = vec4(color, clamp(alpha, 0.0, 0.92));
}
