#include "environment_lighting.glsl"
const int SOI_MAX_SHADOW_CASCADES = 4;

layout(std140) uniform DirectionalShadows {
  mat4 u_shadow_light_vp[SOI_MAX_SHADOW_CASCADES];
  vec4 u_shadow_split_distances;
  vec4 u_shadow_params;
  vec4 u_shadow_camera_position;
  vec4 u_shadow_bias;
  vec4 u_shadow_cascade_texel_world;
  vec4 u_shadow_cascade_depth_span;
};

uniform sampler2DArrayShadow u_directional_shadow_map;
uniform sampler2DArrayShadow u_directional_shadow_map_far;

uniform sampler2DArray u_directional_shadow_depth;

vec3 environment_primary_direction();
vec3 environment_shadow_tint();
float environment_shadow_strength();
float environment_shadow_softness();

const float k_shadow_penumbra_clear_m = 0.045;
const float k_shadow_penumbra_overcast_m = 0.34;
const float k_shadow_min_spread_texels = 0.55;
const float k_shadow_max_spread_texels = 3.0;
const float k_shadow_far_fade_start = 0.82;

#if SOI_QUALITY_TIER >= SOI_TIER_HIGH
#define SOI_SHADOW_PCF_RADIUS 2
#else
#define SOI_SHADOW_PCF_RADIUS 1
#endif
#if SOI_QUALITY_TIER >= SOI_TIER_ULTRA
#define SOI_SHADOW_PCSS 1
#else
#define SOI_SHADOW_PCSS 0
#endif
const float k_shadow_pcss_light_size_m = 0.55;
const float k_shadow_pcss_search_m = 1.6;
const float k_shadow_pcss_max_spread_texels = 9.0;

#if SOI_SHADOW_PCSS

float shadow_blocker_depth(vec3 projected, int cascade, float search_texels) {
  vec2 step_uv = vec2(search_texels * 0.5 * u_shadow_params.z);
  float blocker_sum = 0.0;
  float blocker_count = 0.0;
  for (int y = -2; y <= 2; ++y) {
    for (int x = -2; x <= 2; ++x) {
      vec2 uv = projected.xy + vec2(x, y) * step_uv;
      float depth = texture(u_directional_shadow_depth, vec3(uv, float(cascade))).r;
      if (depth < projected.z) {
        blocker_sum += depth;
        blocker_count += 1.0;
      }
    }
  }
  return blocker_count > 0.0 ? blocker_sum / blocker_count : -1.0;
}
#endif

float sample_shadow_cascade(vec3 world_position, vec3 normal, int cascade) {
  vec3 n = normalize(normal);
  vec3 l = environment_primary_direction();
  float ndl = clamp(dot(n, l), 0.0, 1.0);
  float slope = sqrt(max(1.0 - ndl * ndl, 0.0));

  float texel_world = max(u_shadow_cascade_texel_world[cascade], 1e-5);
  float depth_span = max(u_shadow_cascade_depth_span[cascade], 1e-3);

  vec3 offset_position = world_position + n * (texel_world * u_shadow_bias.y * slope);

  vec4 light_clip = u_shadow_light_vp[cascade] * vec4(offset_position, 1.0);
  vec3 projected = light_clip.xyz / max(light_clip.w, 0.00001);
  projected = projected * 0.5 + 0.5;
  if (projected.z <= 0.0 || projected.z >= 1.0 ||
      any(lessThan(projected.xy, vec2(0.0))) ||
      any(greaterThan(projected.xy, vec2(1.0)))) {
    return 0.0;
  }

  int near_cascade_count = int(u_shadow_camera_position.w + 0.5);
  bool in_far_atlas = cascade >= near_cascade_count;
  float layer = float(in_far_atlas ? cascade - near_cascade_count : cascade);
  float texel_uv = in_far_atlas ? u_shadow_bias.w : u_shadow_params.z;

  float bias_world = u_shadow_bias.x + texel_world * 0.5 * slope;
  float compare_depth = projected.z - bias_world / depth_span;

  float penumbra_world = mix(k_shadow_penumbra_clear_m,
                             k_shadow_penumbra_overcast_m,
                             environment_shadow_softness());
  float spread = clamp(penumbra_world / texel_world,
                       k_shadow_min_spread_texels,
                       k_shadow_max_spread_texels);

#if SOI_SHADOW_PCSS

  if (!in_far_atlas) {
    float search_texels = clamp(k_shadow_pcss_search_m / texel_world, 2.0, 24.0);
    float blocker = shadow_blocker_depth(projected, cascade, search_texels);
    if (blocker >= 0.0) {
      float gap_world = max(compare_depth - blocker, 0.0) * depth_span;
      float soft_world = gap_world * k_shadow_pcss_light_size_m /
                         max(gap_world + k_shadow_pcss_light_size_m * 0.5, 0.05);
      spread = clamp(max(spread, soft_world / texel_world),
                     k_shadow_min_spread_texels,
                     k_shadow_pcss_max_spread_texels);
    }
  }
#endif

  const int radius = SOI_SHADOW_PCF_RADIUS;
  float step_texels = spread / float(radius);
  vec2 step_uv = vec2(step_texels * texel_uv);

  float lit = 0.0;
  float samples = 0.0;
  for (int y = -radius; y <= radius; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      vec4 lookup = vec4(projected.xy + vec2(x, y) * step_uv, layer, compare_depth);
      lit += in_far_atlas ? texture(u_directional_shadow_map_far, lookup)
                          : texture(u_directional_shadow_map, lookup);
      samples += 1.0;
    }
  }
  return 1.0 - lit / max(samples, 1.0);
}

float directional_shadow_occlusion(vec3 world_position, vec3 normal) {
#if SOI_QUALITY_TIER <= SOI_TIER_LOW

  return 0.0;
#else
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

  float shadow_far = u_shadow_split_distances[cascade_count - 1];
  float far_fade =
      1.0 -
      smoothstep(shadow_far * k_shadow_far_fade_start, shadow_far, camera_distance);
  return shadow * far_fade;
#endif
}

vec3 apply_directional_shadow(vec3 lit_color, vec3 world_position, vec3 normal) {
  float amount = directional_shadow_occlusion(world_position, normal) *
                 environment_shadow_strength();
  return mix(lit_color, lit_color * environment_shadow_tint(), clamp(amount, 0.0, 1.0));
}
