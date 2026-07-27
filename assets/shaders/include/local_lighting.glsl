const int SOI_MAX_LOCAL_LIGHTS = 8;

layout(std140) uniform LocalLighting {
  vec4 u_local_position_radius[SOI_MAX_LOCAL_LIGHTS];
  vec4 u_local_color_intensity[SOI_MAX_LOCAL_LIGHTS];
  vec4 u_local_light_meta;
};

vec3 local_lighting(vec3 world_position, vec3 normal) {
  vec3 result = vec3(0.0);
  int count = clamp(int(u_local_light_meta.x + 0.5), 0, SOI_MAX_LOCAL_LIGHTS);
  for (int i = 0; i < SOI_MAX_LOCAL_LIGHTS; ++i) {
    if (i >= count) {
      break;
    }
    vec3 delta = u_local_position_radius[i].xyz - world_position;
    float radius = max(u_local_position_radius[i].w, 0.001);
    float distance_to_light = length(delta);
    vec3 direction = delta / max(distance_to_light, 0.001);
    float falloff = clamp(1.0 - distance_to_light / radius, 0.0, 1.0);
    falloff *= falloff;
    float diffuse = max(dot(normal, direction), 0.0);
    result += u_local_color_intensity[i].rgb *
              (u_local_color_intensity[i].w * falloff * diffuse);
  }
  return result;
}
