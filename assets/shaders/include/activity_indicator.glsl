#ifndef ACTIVITY_INDICATOR_GLSL
#define ACTIVITY_INDICATOR_GLSL

vec4 activity_indicator_shade(float layer,
                              float radial,
                              vec3 raw_normal,
                              vec3 tint,
                              float alpha,
                              float time,
                              float phase) {
  vec3 normal = normalize(raw_normal);
  vec3 light_dir = normalize(vec3(-0.42, 0.58, 0.70));
  vec3 view_dir = vec3(0.0, 0.0, 1.0);
  vec3 half_dir = normalize(light_dir + view_dir);

  float lambert = max(dot(normal, light_dir), 0.0);
  float specular = pow(max(dot(normal, half_dir), 0.0), 30.0);
  float fresnel = pow(1.0 - clamp(normal.z, 0.0, 1.0), 3.0);
  float breathe = 0.5 + 0.5 * sin(time * 2.0 + phase);

  if (layer > 6.5) {
    float t = clamp(radial, 0.0, 1.0);
    float falloff = pow(1.0 - t, 2.2);
    float strength = 0.42 + 0.22 * breathe;
    return vec4(tint * 1.45, alpha * falloff * strength);
  }

  if (layer < 0.5) {
    float falloff = 1.0 - smoothstep(0.62, 1.20, radial);
    return vec4(vec3(0.0), alpha * 0.62 * falloff * falloff);
  }

  vec3 base;
  float gloss;
  if (layer < 1.5) {

    base = mix(vec3(0.042, 0.045, 0.055), tint * 0.22, 0.30);
    gloss = 0.25;
  } else if (layer < 2.5) {
    base = tint * (0.95 + 0.18 * breathe);
    gloss = 1.55;
  } else if (layer < 3.5) {
    base = vec3(0.016, 0.018, 0.024);
    gloss = 0.0;
  } else if (layer < 4.5) {
    base = mix(tint, vec3(1.0), 0.58);
    gloss = 0.85;
  } else if (layer < 5.5) {
    base = mix(tint, vec3(1.0), 0.80);
    gloss = 1.20;
  } else {
    base = tint * 0.62;
    gloss = 1.00;
  }

  float ambient = 0.30 + 0.16 * (normal.z * 0.5 + 0.5);
  vec3 color = base * (ambient + 0.90 * lambert);
  color += vec3(1.0) * specular * gloss * 0.55;
  color += tint * fresnel * 0.22;

  return vec4(color, clamp(alpha, 0.0, 1.0));
}

#endif
