#version 330 core
#include "environment_lighting.glsl"
#include "noise.glsl"
#include "visibility_mask.glsl"

out vec4 frag_color;
in vec2 tex_coord;
in vec3 world_pos;

uniform float time;
uniform float u_segment_visibility;
uniform int u_water_surface_kind;
uniform vec3 u_camera_pos;
uniform vec3 u_soil_color;
uniform float u_moisture_level;
uniform float u_snow_coverage;

uniform vec4 u_ford_a[2];
uniform vec4 u_ford_b[2];
uniform int u_ford_count;

uniform vec4 u_water_neighbor_segment[4];
uniform float u_water_neighbor_radius[4];
uniform int u_water_neighbor_count;

const float PI = 3.14159265359;
const float k_shallow_bed_reach = 0.20;
const float k_shallow_bed_amount = 0.58;
const float k_shallow_tint_reach = 0.16;
const float k_shallow_tint_amount = 0.42;
const vec3 k_shallow_tint = vec3(0.150, 0.320, 0.290);
const float k_exposed_edge_base = 0.006;
const float k_exposed_edge_noise = 0.075;
const float k_flow_streak_gain = 0.085;

float saturate(float value) {
  return clamp(value, 0.0, 1.0);
}

vec3 saturate(vec3 value) {
  return clamp(value, vec3(0.0), vec3(1.0));
}

mat2 rotate2d(float angle) {
  float c = cos(angle);
  float s = sin(angle);
  return mat2(c, -s, s, c);
}

#if SOI_QUALITY_TIER >= SOI_TIER_ULTRA
#define SOI_WATER_OCTAVES 5
#elif SOI_QUALITY_TIER >= SOI_TIER_HIGH
#define SOI_WATER_OCTAVES 4
#elif SOI_QUALITY_TIER >= SOI_TIER_MEDIUM
#define SOI_WATER_OCTAVES 3
#else
#define SOI_WATER_OCTAVES 2
#endif

float fbm(vec2 point) {
  float result = 0.0;
  float amplitude = 0.52;
  mat2 octave_rotation = mat2(0.80, -0.60, 0.60, 0.80);
  for (int octave = 0; octave < SOI_WATER_OCTAVES; ++octave) {
    result += soi_value_noise_e2c097(point) * amplitude;
    point = octave_rotation * point * 2.03 + vec2(7.1, -3.8);
    amplitude *= 0.48;
  }
  return result;
}

float water_height(vec2 point, vec2 flow_dir) {
  float river = float(u_water_surface_kind == 0);
  float speed = mix(0.055, 0.105, river);
  vec2 flow = flow_dir * time * speed;
  vec2 counter_flow = vec2(-flow_dir.y, flow_dir.x) * time * speed * 0.62;
  vec2 warp = vec2(fbm(point * 0.55 - flow), fbm(point * 0.55 + counter_flow));
  vec2 warped = point + (warp - 0.5) * 0.72;
  float height = (fbm(warped * 1.35 - flow * 1.4) - 0.5) * 0.58;
  height += (fbm(rotate2d(1.17) * warped * 2.65 + counter_flow * 1.7) - 0.5) * 0.29;
  height += (fbm(rotate2d(2.35) * warped * 5.3 - flow * 2.2) - 0.5) * 0.13;
  return height;
}

void water_derivatives(
    vec2 point, vec2 flow_dir, out float height, out vec2 gradient, out float lap) {
  float step_size = max(0.003, length(fwidth(point)) * 0.42);
  vec2 dx = vec2(step_size, 0.0);
  vec2 dz = vec2(0.0, step_size);
  float center = water_height(point, flow_dir);
  float left = water_height(point - dx, flow_dir);
  float right = water_height(point + dx, flow_dir);
  float down = water_height(point - dz, flow_dir);
  float up = water_height(point + dz, flow_dir);
  gradient = vec2(right - left, up - down) / (2.0 * step_size);
  lap = (left + right + down + up - 4.0 * center) /
        max(step_size * step_size * 2.0, 1e-5);
  height = center;
}

float fresnel_schlick(float cosine, float f0) {
  return f0 + (1.0 - f0) * pow(1.0 - saturate(cosine), 5.0);
}

float ggx_specular(
    vec3 normal, vec3 view_dir, vec3 light_dir, float roughness, float f0) {
  vec3 half_dir = normalize(view_dir + light_dir);
  float ndv = max(dot(normal, view_dir), 0.001);
  float ndl = max(dot(normal, light_dir), 0.001);
  float ndh = max(dot(normal, half_dir), 0.0);
  float vdh = max(dot(view_dir, half_dir), 0.0);
  float alpha = max(roughness * roughness, 0.002);
  float alpha2 = alpha * alpha;
  float denominator = ndh * ndh * (alpha2 - 1.0) + 1.0;
  float distribution = alpha2 / max(PI * denominator * denominator, 0.001);
  float k = (alpha + 1.0) * (alpha + 1.0) / 8.0;
  float geometry_v = ndv / (ndv * (1.0 - k) + k);
  float geometry_l = ndl / (ndl * (1.0 - k) + k);
  float fresnel = fresnel_schlick(vdh, f0);
  return distribution * geometry_v * geometry_l * fresnel / max(4.0 * ndv * ndl, 0.001);
}

vec3 procedural_sky(vec3 direction, vec3 sun_dir) {
  float elevation = saturate(direction.y * 0.5 + 0.5);
  vec3 horizon = mix(environment_fog_color(), environment_sky_color(), 0.45);
  vec3 zenith = environment_sky_color() * 0.72;
  vec3 sky = mix(horizon, zenith, elevation);
  float halo = pow(max(dot(direction, sun_dir), 0.0), 12.0);
  return sky +
         environment_primary_color() * environment_primary_intensity() * halo * 0.12;
}

vec3 ford_amount(vec2 point) {
  vec3 result = vec3(0.0, 1.0, 0.0);
  for (int index = 0; index < 2; ++index) {
    if (index >= u_ford_count) {
      break;
    }
    vec2 along = u_ford_a[index].zw;
    vec2 across = vec2(-along.y, along.x);
    vec2 local = point - u_ford_a[index].xy;
    float along_dist = abs(dot(local, along));
    float across_dist = abs(dot(local, across));
    float amount =
        (1.0 -
         smoothstep(u_ford_b[index].x - 2.5, u_ford_b[index].x + 0.5, along_dist)) *
        (1.0 -
         smoothstep(u_ford_b[index].y - 0.5, u_ford_b[index].y + 0.5, across_dist));
    if (amount > result.x) {
      result = vec3(amount, along);
    }
  }
  return result;
}

float inside_neighbor_water(vec2 point) {
  float inside = 0.0;
  for (int index = 0; index < 4; ++index) {
    if (index >= u_water_neighbor_count) {
      break;
    }
    vec2 a = u_water_neighbor_segment[index].xy;
    vec2 b = u_water_neighbor_segment[index].zw;
    vec2 ab = b - a;
    float t = clamp(dot(point - a, ab) / max(dot(ab, ab), 1e-4), 0.0, 1.0);
    float distance_to_core = length(point - (a + ab * t));
    float radius = u_water_neighbor_radius[index];
    inside =
        max(inside, 1.0 - smoothstep(radius - 1.0, radius + 0.5, distance_to_core));
  }
  return inside;
}

vec2 river_flow_direction() {
  vec2 dpdx = dFdx(world_pos.xz);
  vec2 dpdy = dFdy(world_pos.xz);
  float dtdx = dFdx(tex_coord.y);
  float dtdy = dFdy(tex_coord.y);
  float determinant = dpdx.x * dpdy.y - dpdx.y * dpdy.x;
  vec2 fallback = vec2(0.78, 0.62);
  if (abs(determinant) < 1e-10) {
    return fallback;
  }
  vec2 along =
      vec2(dpdy.y * dtdx - dpdx.y * dtdy, dpdx.x * dtdy - dpdy.x * dtdx) / determinant;
  float along_length = length(along);
  if (!(along_length > 1e-6) || along_length > 1e6) {
    return fallback;
  }
  return along / along_length;
}

void main() {
  float lake = float(u_water_surface_kind == 1);
  vec2 water_uv = rotate2d(0.31) * world_pos.xz * 0.115;
  vec2 ribbon_flow = river_flow_direction();
  vec2 world_flow = u_water_surface_kind == 0 ? ribbon_flow : vec2(0.78, 0.62);
  vec2 uv_flow = rotate2d(0.31) * world_flow;

  float height;
  float laplacian;
  vec2 gradient;
  water_derivatives(water_uv, uv_flow, height, gradient, laplacian);

  float normal_strength = 0.58;
  vec3 normal = normalize(
      vec3(-gradient.x * normal_strength, 1.0, -gradient.y * normal_strength));

  vec3 view_dir = normalize(u_camera_pos - world_pos);
  vec3 light_dir = environment_primary_direction();
  float ndv = max(dot(normal, view_dir), 0.0);
  float ndl = max(dot(normal, light_dir), 0.0);

  float snow = saturate(u_snow_coverage);
  float sediment =
      saturate(0.18 + u_moisture_level * 0.32 + environment_wetness() * 0.25) *
      (1.0 - snow * 0.75);
  vec3 suspended_silt =
      mix(vec3(0.165, 0.250, 0.185), max(u_soil_color, vec3(0.025)) * 0.58, 0.65);
  vec3 shallow_water = mix(vec3(0.095, 0.255, 0.240), vec3(0.115, 0.285, 0.310), snow);
  shallow_water = mix(shallow_water, suspended_silt, sediment * 0.45);
  vec3 deep_water = mix(vec3(0.046, 0.158, 0.165), vec3(0.050, 0.160, 0.210), snow);

  float ribbon_shore_distance =
      u_water_surface_kind == 1 ? tex_coord.y : min(tex_coord.x, 1.0 - tex_coord.x);
  float shore_aa = max(fwidth(ribbon_shore_distance), 1e-4);
  float shore_distance =
      mix(ribbon_shore_distance, 0.5, inside_neighbor_water(world_pos.xz));
  float normalized_depth = smoothstep(0.018, 0.38, shore_distance);
  float depth_variation = (fbm(world_pos.xz * 0.026 + vec2(17.0, -9.0)) - 0.5) * 0.055;
  float optical_depth = saturate(normalized_depth * 0.72 + depth_variation + 0.16);
  vec3 body_color = mix(shallow_water, deep_water, optical_depth);
  float silt = (1.0 - normalized_depth) *
               smoothstep(0.45, 0.78, fbm(world_pos.xz * 0.075 + 31.0));
  body_color = mix(body_color, suspended_silt, silt * 0.28);
  float shore_noise = fbm(world_pos.xz * 0.21 + vec2(-13.0, 5.0));
  float shallow_reach = k_shallow_tint_reach * (0.65 + 0.70 * shore_noise);
  float shallow_tint = 1.0 - smoothstep(0.0, shallow_reach, shore_distance);
  body_color = mix(body_color,
                   mix(k_shallow_tint, suspended_silt, sediment * 0.35),
                   shallow_tint * k_shallow_tint_amount * (1.0 - snow * 0.5));
  vec3 shallow_bed = mix(
      max(u_soil_color, vec3(0.025)) * vec3(0.66, 0.70, 0.62), suspended_silt, 0.30);
  float bed_reach = k_shallow_bed_reach * (0.55 + 0.80 * shore_noise);
  float bed_visibility = 1.0 - smoothstep(0.0, bed_reach, shore_distance);
  bed_visibility *= bed_visibility;
  body_color = mix(body_color, shallow_bed, bed_visibility * k_shallow_bed_amount);

  vec3 ford = ford_amount(world_pos.xz);
  float ford_mix = ford.x;
  vec2 flow_axis = ford.yz;
  vec2 flow_cross = vec2(-flow_axis.y, flow_axis.x);
  if (ford_mix > 0.0) {
    float pebbles = fbm(world_pos.xz * 1.9 + vec2(3.1, -7.4));
    float bars = fbm(world_pos.xz * 0.21 + vec2(-11.0, 4.0));
    vec3 gravel =
        mix(shallow_bed, max(u_soil_color, vec3(0.03)) * vec3(0.92, 0.90, 0.80), 0.35) *
        (0.82 + 0.32 * pebbles);
    vec3 ford_water = mix(gravel, shallow_water, 0.40 + 0.18 * bars);
    body_color = mix(body_color, ford_water, ford_mix * 0.62);
  }

  vec3 sun_light = environment_primary_color() * environment_primary_intensity();
  vec3 water_lighting =
      (environment_ambient_light(normal) + sun_light * (ndl * 0.62 + 0.20)) *
      environment_exposure();

  vec3 reflected_dir = reflect(-view_dir, normal);
  vec3 reflection = procedural_sky(reflected_dir, light_dir) * environment_exposure();
  float fresnel = fresnel_schlick(ndv, 0.020);
  float reflection_weight = 0.050 + fresnel * 0.34;
  vec3 color = mix(body_color * water_lighting, reflection, reflection_weight);

  float roughness = mix(0.34, 0.46, saturate(length(gradient) * 1.5));
  float specular = ggx_specular(normal, view_dir, light_dir, roughness, 0.020);
  color += sun_light * environment_exposure() * min(specular, 0.42) * 0.19;

  float river_energy = 1.0 - lake;
  float shore_band = 1.0 - smoothstep(0.006, 0.060, shore_distance);
  float broken_edge = smoothstep(
      0.40, 0.78, fbm(world_pos.xz * 0.72 + vec2(time * 0.12, -time * 0.08)));
  float shore_foam = shore_band * broken_edge * mix(0.065, 0.11, river_energy);
  float crest = smoothstep(0.64, 1.18, abs(laplacian) * 0.006 + length(gradient));
  crest *=
      smoothstep(0.55, 0.86, fbm(world_pos.xz * 1.15 - vec2(time * 0.18, time * 0.08)));
  float riffle = 0.0;
  if (ford_mix > 0.0) {

    vec2 flow_uv = vec2(dot(world_pos.xz, flow_axis), dot(world_pos.xz, flow_cross));
    float bands = fbm(vec2(flow_uv.x * 1.25 - time * 0.85, flow_uv.y * 0.32));
    float sparkle = fbm(world_pos.xz * 2.4 + vec2(time * 0.35, -time * 0.21));
    riffle = ford_mix * smoothstep(0.56, 0.84, bands) * (0.55 + 0.45 * sparkle);
  }
  vec2 world_cross = vec2(-world_flow.y, world_flow.x);
  vec2 stream_uv = vec2(dot(world_pos.xz, world_flow), dot(world_pos.xz, world_cross));
  float streak_field =
      fbm(vec2(stream_uv.x * 0.16 - time * 0.55, stream_uv.y * 1.35)) * 0.65 +
      fbm(vec2(stream_uv.x * 0.42 - time * 0.95, stream_uv.y * 3.10 + 7.0)) * 0.35;
  float channel = smoothstep(0.08, 0.30, shore_distance);
  float streak_footprint = length(fwidth(stream_uv)) * 3.1;
  float streak = smoothstep(0.58, 0.80, streak_field) * channel * river_energy *
                 (1.0 - smoothstep(0.35, 0.9, streak_footprint));
  float foam = saturate(shore_foam + crest * mix(0.010, 0.022, river_energy) +
                        riffle * 0.16 + streak * k_flow_streak_gain);
  color = mix(color, vec3(0.76, 0.86, 0.84) * water_lighting, foam);
  color *=
      1.0 - smoothstep(0.30, 0.55, 1.0 - streak_field) * channel * river_energy * 0.06;

  float edge_field = fbm(world_pos.xz * 0.34 + vec2(41.0, -17.0)) * 0.70 +
                     fbm(world_pos.xz * 1.45 + vec2(-6.0, 29.0)) * 0.30;
  float exposed_reach =
      k_exposed_edge_base + k_exposed_edge_noise * smoothstep(0.36, 0.80, edge_field);
  exposed_reach *= mix(1.0, 0.50, lake) * (1.0 - ford_mix);
  float edge_aa = shore_aa;
  float exposed =
      1.0 -
      smoothstep(exposed_reach - edge_aa, exposed_reach + edge_aa, shore_distance);
  float lap_line =
      (1.0 -
       smoothstep(0.0, edge_aa * 2.5 + 0.006, abs(shore_distance - exposed_reach))) *
      (1.0 - exposed);
  vec3 sediment_soil = max(u_soil_color, vec3(0.025));
  sediment_soil =
      mix(sediment_soil, vec3(dot(sediment_soil, vec3(0.299, 0.587, 0.114))), 0.35);
  vec3 wet_sediment = sediment_soil * vec3(0.46, 0.47, 0.45);
  wet_sediment = mix(wet_sediment, shallow_bed * 0.55, 0.25);
  wet_sediment *= 0.88 + 0.24 * fbm(world_pos.xz * 0.90 + vec2(3.0, 11.0));
  wet_sediment = mix(wet_sediment, vec3(0.42, 0.44, 0.46), snow * 0.6);
  vec3 sediment_lit = wet_sediment * soi_surface_lighting(vec3(0.0, 1.0, 0.0));
  float sediment_gloss =
      ggx_specular(vec3(0.0, 1.0, 0.0), view_dir, light_dir, 0.30, 0.03);
  sediment_lit += sun_light * environment_exposure() * min(sediment_gloss, 0.5) * 0.10;
  color = mix(color, vec3(0.70, 0.80, 0.78) * water_lighting, lap_line * 0.14);
  color = mix(color, sediment_lit, exposed);

  color = apply_visibility_world_shading(color, world_pos.xz);
  color *= u_segment_visibility;

  frag_color = vec4(color, 1.0);
}
