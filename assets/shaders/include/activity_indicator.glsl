#ifndef ACTIVITY_INDICATOR_GLSL
#define ACTIVITY_INDICATOR_GLSL

vec4 activity_indicator_shade(float layer,
                              float radial,
                              vec3 raw_normal,
                              vec3 tint,
                              float alpha,
                              float time,
                              float phase,
                              float height) {
  vec3 normal = normalize(raw_normal);
  vec3 light_dir = normalize(vec3(-0.38, 0.62, 0.68));
  vec3 view_dir = vec3(0.0, 0.0, 1.0);
  vec3 half_dir = normalize(light_dir + view_dir);

  float lambert = max(dot(normal, light_dir), 0.0);
  float specular = pow(max(dot(normal, half_dir), 0.0), 42.0);
  float fresnel = pow(1.0 - clamp(normal.z, 0.0, 1.0), 3.0);
  float breathe = 0.5 + 0.5 * sin(time * 2.0 + phase);

  if (layer < 0.5) {
    float edge = smoothstep(0.0, 1.0, clamp(radial, 0.0, 1.0));
    float strength = mix(0.24, 0.0, edge);
    return vec4(vec3(0.0), alpha * strength);
  }

  if (layer < 1.5) {
    vec3 outline = mix(vec3(0.018), tint * 0.16, 0.32);
    return vec4(outline, clamp(alpha, 0.0, 1.0));
  }

  float sheen = clamp(height * 0.5 + 0.5, 0.0, 1.0);
  float smooth_sheen = sheen * sheen * (3.0 - 2.0 * sheen);

  vec3 base;
  float gloss;
  float rim = 0.0;
  if (layer < 2.5) {
    vec3 lit = mix(tint, vec3(1.0), 0.14);
    vec3 deep = mix(tint, vec3(0.0), 0.12);
    base = mix(deep, lit, smooth_sheen);
    gloss = 0.42;
    rim = pow(smooth_sheen, 7.0) * 0.12;
  } else if (layer < 3.5) {
    base = mix(tint, vec3(1.0), 0.42);
    gloss = 0.72;
  } else {
    base = mix(tint, vec3(0.0), mix(0.42, 0.28, smooth_sheen));
    gloss = 0.22;
  }

  float ambient = 0.58 + 0.12 * (normal.z * 0.5 + 0.5);
  vec3 color = base * (ambient + 0.38 * lambert);
  color += vec3(1.0) * specular * gloss * 0.24;
  color += tint * fresnel * 0.14;
  color += mix(tint, vec3(1.0), 0.7) * rim;
  color *= 0.985 + 0.03 * breathe;

  return vec4(color, clamp(alpha, 0.0, 1.0));
}

#endif
