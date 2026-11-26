

const int k_forest_mud = 0;
const int k_grass_dry = 1;
const int k_soil_rocky = 2;
const int k_alpine_mix = 3;
const int k_soil_fertile = 4;

struct BiomeGrassPalette {
  vec3 fresh;
  vec3 cured;
};

BiomeGrassPalette biome_grass_palette(int ground_type) {
  if (ground_type == k_forest_mud)
    return BiomeGrassPalette(vec3(0.84, 0.96, 1.05), vec3(1.01, 0.94, 0.84));
  if (ground_type == k_grass_dry)
    return BiomeGrassPalette(vec3(1.12, 0.99, 0.79), vec3(1.23, 1.06, 0.77));
  if (ground_type == k_soil_rocky)
    return BiomeGrassPalette(vec3(1.01, 0.96, 1.03), vec3(1.10, 1.01, 0.97));
  if (ground_type == k_alpine_mix)
    return BiomeGrassPalette(vec3(0.91, 0.98, 1.10), vec3(0.99, 1.02, 1.11));
  if (ground_type == k_soil_fertile)
    return BiomeGrassPalette(vec3(1.00, 1.08, 0.85), vec3(1.14, 1.05, 0.84));

  return BiomeGrassPalette(vec3(1.0), vec3(1.0));
}

const float k_sward_red_share = 0.71;
const float k_sward_blue_share = 0.50;
const float k_sward_natural_amount = 0.88;
const float k_sward_value_gain = 1.08;
const vec3 k_sward_luma = vec3(0.2126, 0.7152, 0.0722);

vec3 biome_natural_sward(vec3 color) {
  float leafy = smoothstep(0.0, 0.05, color.g - max(color.r, color.b));
  vec3 natural = vec3(max(color.r, color.g * k_sward_red_share),
                      color.g,
                      min(color.b, color.g * k_sward_blue_share));
  natural *= dot(color, k_sward_luma) / max(dot(natural, k_sward_luma), 1e-4);
  return mix(color, natural * k_sward_value_gain, leafy * k_sward_natural_amount);
}

vec3 biome_grass_color(vec3 albedo, int ground_type, float dryness, float cover) {
  BiomeGrassPalette palette = biome_grass_palette(ground_type);
  vec3 tone = mix(palette.fresh, palette.cured, clamp(dryness, 0.0, 1.0));
  float weight = clamp(cover, 0.0, 1.0);
  return mix(albedo, biome_natural_sward(albedo) * tone, weight);
}

vec3 biome_surface_saturation(vec3 albedo, float authored_saturation, float tactical) {
  float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
  float saturation = clamp(authored_saturation * 1.07, 0.0, 1.10);
  return mix(vec3(luma), albedo, saturation * mix(1.0, 0.94, tactical));
}
