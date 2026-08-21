const int SOI_MAX_LOCAL_LIGHTS = 16;

layout(std140) uniform LocalLighting {
  vec4 u_local_position_radius[SOI_MAX_LOCAL_LIGHTS];
  vec4 u_local_color_intensity[SOI_MAX_LOCAL_LIGHTS];
  vec4 u_local_light_meta;
};

uniform int u_local_light_mask;
uniform int u_has_local_light_mask;

bool soi_local_light_active(int index) {
  if (u_has_local_light_mask == 0) {
    return true;
  }
  return (u_local_light_mask & (1 << index)) != 0;
}

const float k_soi_local_wrap = 0.18;
const float k_soi_local_specular_power = 22.0;
const float k_soi_local_specular_gain = 0.28;

float soi_local_falloff(float distance_to_light, float radius) {
  float normalized = clamp(distance_to_light / radius, 0.0, 1.0);
  float window = 1.0 - normalized * normalized;
  window *= window;
  return window / (1.0 + 4.0 * normalized * normalized);
}

vec3 local_lighting(vec3 world_position, vec3 normal) {
  vec3 result = vec3(0.0);
  int count = clamp(int(u_local_light_meta.x + 0.5), 0, SOI_MAX_LOCAL_LIGHTS);
  for (int i = 0; i < SOI_MAX_LOCAL_LIGHTS; ++i) {
    if (i >= count) {
      break;
    }
    if (!soi_local_light_active(i)) {
      continue;
    }
    vec3 delta = u_local_position_radius[i].xyz - world_position;
    float radius = max(u_local_position_radius[i].w, 0.001);
    float distance_to_light = length(delta);
    vec3 direction = delta / max(distance_to_light, 0.001);
    float falloff = soi_local_falloff(distance_to_light, radius);
    float diffuse =
        clamp((dot(normal, direction) + k_soi_local_wrap) / (1.0 + k_soi_local_wrap),
              0.0,
              1.0);
    result += u_local_color_intensity[i].rgb *
              (u_local_color_intensity[i].w * falloff * diffuse);
  }
  return result;
}

vec3 local_lighting_specular(vec3 world_position,
                             vec3 normal,
                             vec3 view_dir,
                             float gloss) {
  vec3 result = vec3(0.0);
  int count = clamp(int(u_local_light_meta.x + 0.5), 0, SOI_MAX_LOCAL_LIGHTS);
  for (int i = 0; i < SOI_MAX_LOCAL_LIGHTS; ++i) {
    if (i >= count) {
      break;
    }
    if (!soi_local_light_active(i)) {
      continue;
    }
    vec3 delta = u_local_position_radius[i].xyz - world_position;
    float radius = max(u_local_position_radius[i].w, 0.001);
    float distance_to_light = length(delta);
    vec3 direction = delta / max(distance_to_light, 0.001);
    float falloff = soi_local_falloff(distance_to_light, radius);
    vec3 half_vector = normalize(direction + view_dir);
    float facing = step(0.0, dot(normal, direction));
    float glint = pow(max(dot(normal, half_vector), 0.0), k_soi_local_specular_power);
    result += u_local_color_intensity[i].rgb *
              (u_local_color_intensity[i].w * falloff * glint * facing * gloss *
               k_soi_local_specular_gain);
  }
  return result;
}
