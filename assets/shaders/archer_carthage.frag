#version 330 core

in vec3 v_normal;
in vec3 v_worldNormal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord;
in vec3 v_worldPos;
in float v_armorLayer;
in float v_leatherTension;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform float u_alpha;
uniform float u_time;
uniform float u_rainIntensity;

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

float triplanarNoise(vec3 pos, vec3 normal, float scale) {
  vec3 blending = abs(normal);
  blending = max(blending, vec3(0.0001));
  blending /= (blending.x + blending.y + blending.z);

  float xy = noise(pos.xy * scale);
  float yz = noise(pos.yz * scale);
  float zx = noise(pos.zx * scale);
  return xy * blending.z + yz * blending.x + zx * blending.y;
}

float fbm(vec3 pos, vec3 normal, float scale) {
  float total = 0.0;
  float amplitude = 0.5;
  float frequency = 1.0;
  for (int i = 0; i < 5; ++i) {
    total += amplitude * triplanarNoise(pos * frequency, normal, scale * frequency);
    frequency *= 2.02;
    amplitude *= 0.45;
  }
  return total;
}

float fresnelSchlick(float cosTheta, float F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float computeCurvature(vec3 normal) {
  vec3 dx = dFdx(normal);
  vec3 dy = dFdy(normal);
  return length(dx) + length(dy);
}

float anisotropicHighlight(vec3 normal, vec3 tangent, vec3 bitangent,
                           vec3 lightDir, vec3 viewDir, float roughness) {
  vec3 h = normalize(lightDir + viewDir);
  float nDotL = max(dot(normal, lightDir), 0.0);
  float nDotV = max(dot(normal, viewDir), 0.0);
  if (nDotL < 1e-4 || nDotV < 1e-4) {
    return 0.0;
  }

  float tDotH = dot(normalize(tangent), h);
  float bDotH = dot(normalize(bitangent), h);
  float anis = tDotH * tDotH * 0.6 + bDotH * bDotH * 0.4;
  float exponent = mix(20.0, 50.0, clamp(1.0 - roughness, 0.0, 1.0));
  return pow(max(anis, 0.0), exponent) * nDotL;
}

float wrapDiffuse(vec3 normal, vec3 lightDir, float wrap) {
  float nl = dot(normal, lightDir);
  return max(nl * (1.0 - wrap) + wrap, 0.0);
}

void main() {
  vec3 baseColor = u_color;
  if (u_useTexture) {
    baseColor *= texture(u_texture, v_texCoord).rgb;
  }

  vec3 normal = normalize(v_worldNormal);
  float avgColor = (baseColor.r + baseColor.g + baseColor.b) / 3.0;

  bool isLeather = (avgColor > 0.25 && avgColor < 0.55 && baseColor.r > baseColor.g * 1.05);
  bool isLinen = (avgColor > 0.68 && avgColor < 0.90 && baseColor.r > baseColor.b * 1.04);
  bool isBronze = (baseColor.r > baseColor.g * 1.03 && baseColor.r > baseColor.b * 1.12 && avgColor > 0.45);

  vec3 lightDir = normalize(vec3(0.55, 1.15, 0.35));
  vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0));

  float rain = clamp(u_rainIntensity, 0.0, 1.0);
  float curvature = computeCurvature(normal);

  vec3 color = baseColor;
  vec3 specularAccum = vec3(0.0);

  if (isLeather) {
    float grain = fbm(v_worldPos * 5.2, normal, 6.5);
    float micro = fbm(v_worldPos * 11.0 + vec3(7.1), normal, 12.0);
    float stretch = mix(-0.06, 0.12, v_leatherTension);

    float seamMask = smoothstep(0.0, 0.12, abs(sin((v_texCoord.y + v_armorLayer * 0.2) * 3.14159 * 6.0)));
    float seamWeave = smoothstep(0.75, 1.0, sin(v_worldPos.x * 14.0) * sin(v_worldPos.y * 18.0));
    float rainMask = rain * (1.0 - clamp(normal.y, 0.0, 1.0)) * (0.35 + 0.65 * fbm(v_worldPos * 1.5, normal, 2.5));
    float edgeWear = smoothstep(0.35, 1.1, curvature);

    float roughness = clamp(0.55 - v_leatherTension * 0.25, 0.15, 0.6);
    float anis = anisotropicHighlight(normal, v_tangent, v_bitangent, lightDir, viewDir, roughness);
    float fres = fresnelSchlick(max(dot(normal, viewDir), 0.0), 0.04);

    color *= 1.0 + grain * 0.18 + micro * 0.08 + stretch;
    color -= vec3(rainMask * 0.45);
    color += vec3(seamMask * 0.12 + seamWeave * 0.05);
    color = mix(color, color + vec3(0.12, 0.10, 0.08), edgeWear * 0.25);

    specularAccum += vec3(anis) * 0.35 + vec3(fres) * 0.2;
  } else if (isLinen) {
    float warp = sin(v_worldPos.x * 62.0);
    float weft = sin(v_worldPos.z * 66.0);
    float weave = warp * weft * 0.08;
    float cross = sin(v_worldPos.y * 24.0) * 0.05;
    float glue = fbm(v_worldPos * 3.2, normal, 4.5) * 0.12;
    float fray = fbm(v_worldPos * 9.0, normal, 10.0) * clamp(1.4 - normal.y, 0.0, 1.0) * 0.12;
    float dust = clamp(1.0 - normal.y, 0.0, 1.0) * fbm(v_worldPos * 1.1, normal, 2.0) * 0.2;
    float damp = rain * (0.18 + 0.32 * fbm(v_worldPos * 2.0, normal, 3.5));

    color += vec3(weave + cross);
    color -= vec3(glue * 0.4 + damp * 0.35);
    color -= vec3(fray * 0.2);
    color = mix(color, color * (1.0 - dust * 0.4), 0.6);

    float clothSpec = fresnelSchlick(max(dot(normal, viewDir), 0.0), 0.03) * 0.08;
    specularAccum += vec3(clothSpec);
  } else if (isBronze) {
    float hammer = fbm(v_worldPos * 14.0, normal, 20.0) * 0.18;
    float patinaNoise = fbm(v_worldPos * 6.0 + vec3(5.0), normal, 8.0) * 0.12;
    vec3 patinaColor = vec3(0.18, 0.45, 0.36);

    color += vec3(hammer);
    color = mix(color, patinaColor, patinaNoise * 0.3);

    float fres = fresnelSchlick(max(dot(normal, viewDir), 0.0), 0.08);
    specularAccum += vec3(fres * 0.6);
  } else {
    float subtleNoise = fbm(v_worldPos * 4.0, normal, 5.5) * 0.1;
    color *= 1.0 + subtleNoise;
  }

  float wrap = isBronze ? 0.22 : (isLeather ? 0.35 : 0.42);
  float diff = wrapDiffuse(normal, lightDir, wrap);

  vec3 finalColor = clamp(color * diff + specularAccum, 0.0, 1.0);
  FragColor = vec4(finalColor, u_alpha);
}
