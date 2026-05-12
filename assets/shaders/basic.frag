#version 330 core

in vec3 v_normal;
in vec2 v_tex_coord;
in vec3 v_world_pos;

uniform sampler2D u_texture;
uniform vec3 u_color;
uniform bool u_use_texture;
uniform float u_alpha;
uniform int u_material_id;
uniform vec3 u_light_dir;
uniform float u_ambient_strength;

out vec4 frag_color;

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

float wood_grain(vec2 p, float y) {
  float grain = sin(y * 22.0 + noise(p * 4.5) * 3.5) * 0.5 + 0.5;
  float fine = noise(p * 48.0) * 0.10;
  float knot = step(0.93, noise(p * 2.2)) * 0.16;
  return grain * 0.13 + fine - knot;
}

vec3 proceduralMaterialVariation(vec3 baseColor, vec3 worldPos, vec3 normal) {
  vec2 uv = worldPos.xz * 4.0;

  float avgColor = (baseColor.r + baseColor.g + baseColor.b) / 3.0;

  bool bIsWood = false, bIsMetal = false, bIsCloth = false;
  if (u_material_id == 2) {
    bIsWood = true;
  } else if (u_material_id == 1) {
    bIsMetal = true;
  } else if (u_material_id == 3) {
    bIsCloth = true;
  } else if (u_material_id != 4) {

    bIsWood =
        (baseColor.r < baseColor.g * 2.5 && baseColor.r > baseColor.b * 1.45 &&
         avgColor > 0.18 && avgColor < 0.50);
    bIsMetal = (!bIsWood && avgColor < 0.40);
    bIsCloth = (!bIsWood && !bIsMetal && avgColor > 0.65);
  }

  vec3 variation = baseColor;

  if (bIsWood) {
    float woodNoise = wood_grain(uv, worldPos.y);
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.3))));
    float sheen = pow(1.0 - viewAngle, 4.0) * 0.06;
    variation = baseColor * (1.0 + woodNoise) + vec3(sheen);
  } else if (bIsMetal) {
    float metalNoise = noise(uv * 9.0) * 0.018;
    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float fresnel = pow(1.0 - viewAngle, 2.0) * 0.10;
    variation = baseColor + vec3(metalNoise + fresnel);
  } else if (bIsCloth) {
    float weaveX = sin(worldPos.x * 55.0);
    float weaveZ = sin(worldPos.z * 55.0);
    float weavePattern = weaveX * weaveZ * 0.025;
    float clothNoise = noise(uv * 2.5) * 0.10 - 0.05;

    float viewAngle = abs(dot(normal, normalize(vec3(0.0, 1.0, 0.5))));
    float sheen = pow(1.0 - viewAngle, 3.0) * 0.15;

    variation = baseColor * (1.0 + clothNoise + weavePattern) + vec3(sheen);
  } else {
    float leatherNoise = noise(uv * 5.5);
    float blotches = noise(uv * 1.8) * 0.12 - 0.06;
    variation = baseColor * (1.0 + leatherNoise * 0.14 - 0.07 + blotches);
  }

  return clamp(variation, 0.0, 1.0);
}

void main() {
  vec3 color = u_color;
  if (u_use_texture) {
    color *= texture(u_texture, v_tex_coord).rgb;
  }

  vec3 normal = normalize(v_normal);
  color = proceduralMaterialVariation(color, v_world_pos, normal);

  if (u_material_id >= 10) {
    float sootAmt = float(u_material_id - 9) * 0.45;
    vec2 sootUv = v_world_pos.xz * 3.5;
    float sootPatch = noise(sootUv) * 0.6 + noise(sootUv * 4.1) * 0.4;
    float sootMask = smoothstep(0.42, 0.65, sootPatch) * sootAmt;
    vec3 charColor = mix(color * 0.25, vec3(0.08, 0.07, 0.06), 0.5);
    color = mix(color, charColor, clamp(sootMask, 0.0, 0.85));
  }

  vec3 lightDir = length(u_light_dir) > 0.001
                      ? u_light_dir
                      : normalize(vec3(0.65, 0.50, 0.40));
  float avgColor = (u_color.r + u_color.g + u_color.b) / 3.0;
  float wrapAmount = avgColor > 0.65 ? 0.52 : (avgColor > 0.40 ? 0.20 : 0.05);

  float nDotL = dot(normal, lightDir);
  float diff_raw = nDotL * (1.0 - wrapAmount) + wrapAmount;
  float ambient = u_ambient_strength > 0.001 ? u_ambient_strength : 0.18;
  float diff = max(diff_raw, ambient);

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp((diff_raw + 1.0) / 2.5, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * (ambient * 1.1), sun_color, lit_t);

  color *= diff * light_tint;
  frag_color = vec4(color, u_alpha);
}
