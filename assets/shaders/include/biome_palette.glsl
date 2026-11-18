

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
    return BiomeGrassPalette(vec3(0.79, 1.17, 1.04), vec3(1.08, 1.04, 0.84));
  if (ground_type == k_grass_dry)
    return BiomeGrassPalette(vec3(0.94, 1.16, 0.84), vec3(1.27, 1.13, 0.74));
  if (ground_type == k_soil_rocky)
    return BiomeGrassPalette(vec3(0.88, 1.13, 1.06), vec3(1.12, 1.06, 0.89));
  if (ground_type == k_soil_fertile)
    return BiomeGrassPalette(vec3(0.81, 1.29, 0.92), vec3(1.22, 1.13, 0.78));

  return BiomeGrassPalette(vec3(1.0), vec3(1.0));
}

vec3 biome_grass_color(vec3 albedo, int ground_type, float dryness, float cover) {
  BiomeGrassPalette palette = biome_grass_palette(ground_type);
  vec3 tone = mix(palette.fresh, palette.cured, clamp(dryness, 0.0, 1.0));
  return albedo * mix(vec3(1.0), tone, clamp(cover, 0.0, 1.0));
}

vec3 biome_surface_saturation(vec3 albedo, float authored_saturation, float tactical) {
  float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
  float saturation = clamp(authored_saturation * 1.16, 0.0, 1.16);
  return mix(vec3(luma), albedo, saturation * mix(1.0, 0.94, tactical));
}
