const int SOI_MAX_SHADOW_CASCADES = 4;

layout(std140) uniform DirectionalShadows {
  mat4 u_shadow_light_vp[SOI_MAX_SHADOW_CASCADES];
  vec4 u_shadow_split_distances;
  vec4 u_shadow_params;
  vec4 u_shadow_camera_position;
  vec4 u_shadow_bias;
};

uniform sampler2DArray u_directional_shadow_map;

vec3 environment_primary_direction();
vec3 environment_shadow_tint();
float environment_shadow_strength();

float sample_shadow_cascade(vec3 world_position, vec3 normal, int cascade) {
  vec4 light_clip = u_shadow_light_vp[cascade] * vec4(world_position, 1.0);
  vec3 projected = light_clip.xyz / max(light_clip.w, 0.00001);
  projected = projected * 0.5 + 0.5;
  if (projected.z <= 0.0 || projected.z >= 1.0 ||
      any(lessThan(projected.xy, vec2(0.0))) ||
      any(greaterThan(projected.xy, vec2(1.0)))) {
    return 0.0;
  }

  float slope = 1.0 - max(dot(normalize(normal), environment_primary_direction()), 0.0);
  float bias = u_shadow_bias.x + slope * u_shadow_bias.y;
  int radius = clamp(int(u_shadow_params.w + 0.5), 1, 3);
  float blocked = 0.0;
  float samples = 0.0;
  for (int y = -3; y <= 3; ++y) {
    for (int x = -3; x <= 3; ++x) {
      if (abs(x) > radius || abs(y) > radius)
        continue;
      vec2 offset = vec2(x, y) * u_shadow_params.z;
      float closest =
          texture(u_directional_shadow_map, vec3(projected.xy + offset, cascade)).r;
      blocked += projected.z - bias > closest ? 1.0 : 0.0;
      samples += 1.0;
    }
  }
  return blocked / max(samples, 1.0);
}

float directional_shadow_occlusion(vec3 world_position, vec3 normal) {
  if (u_shadow_params.x < 0.5) {
    return 0.0;
  }

  float camera_distance = length(world_position - u_shadow_camera_position.xyz);
  int cascade = 0;
  if (camera_distance > u_shadow_split_distances.x)
    cascade = 1;
  if (camera_distance > u_shadow_split_distances.y)
    cascade = 2;
  if (camera_distance > u_shadow_split_distances.z)
    cascade = 3;
  int cascade_count = int(u_shadow_params.y);
  cascade = min(cascade, cascade_count - 1);

  float shadow = sample_shadow_cascade(world_position, normal, cascade);
  if (cascade < cascade_count - 1) {
    float split = u_shadow_split_distances[cascade];
    float blend_width = max(split * u_shadow_bias.z, 0.001);
    float blend = smoothstep(split - blend_width, split, camera_distance);
    if (blend > 0.0) {
      float next_shadow = sample_shadow_cascade(world_position, normal, cascade + 1);
      shadow = mix(shadow, next_shadow, blend);
    }
  }
  return shadow;
}

vec3 apply_directional_shadow(vec3 lit_color, vec3 world_position, vec3 normal) {
  float amount = directional_shadow_occlusion(world_position, normal) *
                 environment_shadow_strength();
  return mix(lit_color, lit_color * environment_shadow_tint(), clamp(amount, 0.0, 1.0));
}
