#version 330 core

out vec4 frag_color;
in vec2 tex_coord;
in vec3 world_pos;

uniform float time;
uniform sampler2D u_visibility_tex;
uniform vec2 u_visibility_size;
uniform float u_visibility_tile_size;
uniform float u_explored_alpha;
uniform int u_has_visibility;
uniform float u_segment_visibility;
uniform int u_water_surface_kind;
uniform vec3 u_camera_pos;
uniform vec3 u_light_dir;
uniform vec3 u_fog_color;
uniform float u_fog_start;
uniform float u_fog_end;

const float PI = 3.14159265359;

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

float hash21(vec2 point) {
  point = fract(point * vec2(123.34, 456.21));
  point += dot(point, point + 45.32);
  return fract(point.x * point.y);
}

float value_noise(vec2 point) {
  vec2 cell = floor(point);
  vec2 local = fract(point);
  local = local * local * local * (local * (local * 6.0 - 15.0) + 10.0);
  float a = hash21(cell);
  float b = hash21(cell + vec2(1.0, 0.0));
  float c = hash21(cell + vec2(0.0, 1.0));
  float d = hash21(cell + vec2(1.0, 1.0));
  return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);
}

float fbm(vec2 point) {
  float result = 0.0;
  float amplitude = 0.52;
  mat2 octave_rotation = mat2(0.80, -0.60, 0.60, 0.80);
  for (int octave = 0; octave < 4; ++octave) {
    result += value_noise(point) * amplitude;
    point = octave_rotation * point * 2.03 + vec2(7.1, -3.8);
    amplitude *= 0.48;
  }
  return result;
}

float water_height(vec2 point) {
  float river = float(u_water_surface_kind == 0);
  float speed = mix(0.055, 0.105, river);
  vec2 flow = vec2(0.78, 0.62) * time * speed;
  vec2 counter_flow = vec2(-0.42, 0.91) * time * speed * 0.62;
  vec2 warp = vec2(fbm(point * 0.55 - flow),
                   fbm(point * 0.55 + counter_flow));
  vec2 warped = point + (warp - 0.5) * 0.72;
  float height = (fbm(warped * 1.35 - flow * 1.4) - 0.5) * 0.58;
  height += (fbm(rotate2d(1.17) * warped * 2.65 + counter_flow * 1.7) - 0.5) *
            0.29;
  height += (fbm(rotate2d(2.35) * warped * 5.3 - flow * 2.2) - 0.5) * 0.13;
  return height;
}

void water_derivatives(vec2 point, out float height, out vec2 gradient, out float lap) {
  float step_size = max(0.003, length(fwidth(point)) * 0.42);
  vec2 dx = vec2(step_size, 0.0);
  vec2 dz = vec2(0.0, step_size);
  float center = water_height(point);
  float left = water_height(point - dx);
  float right = water_height(point + dx);
  float down = water_height(point - dz);
  float up = water_height(point + dz);
  gradient = vec2(right - left, up - down) / (2.0 * step_size);
  lap = (left + right + down + up - 4.0 * center) /
        max(step_size * step_size * 2.0, 1e-5);
  height = center;
}

float fresnel_schlick(float cosine, float f0) {
  return f0 + (1.0 - f0) * pow(1.0 - saturate(cosine), 5.0);
}

float ggx_specular(vec3 normal,
                   vec3 view_dir,
                   vec3 light_dir,
                   float roughness,
                   float f0) {
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
  return distribution * geometry_v * geometry_l * fresnel /
         max(4.0 * ndv * ndl, 0.001);
}

vec3 procedural_sky(vec3 direction, vec3 sun_dir) {
  float elevation = saturate(direction.y * 0.5 + 0.5);
  vec3 horizon = vec3(0.56, 0.70, 0.74);
  vec3 zenith = vec3(0.16, 0.34, 0.48);
  vec3 sky = mix(horizon, zenith, elevation);
  float halo = pow(max(dot(direction, sun_dir), 0.0), 12.0);
  return sky + vec3(0.95, 0.82, 0.62) * halo * 0.12;
}

void main() {
  float lake = float(u_water_surface_kind == 1);
  vec2 water_uv = rotate2d(0.31) * world_pos.xz * 0.115;

  float height;
  float laplacian;
  vec2 gradient;
  water_derivatives(water_uv, height, gradient, laplacian);
  // Keep the optical response common across every connected water body.
  // Rivers move faster, but are not assigned a brighter or rougher material.
  float normal_strength = 0.58;
  vec3 normal = normalize(vec3(-gradient.x * normal_strength,
                               1.0,
                               -gradient.y * normal_strength));

  vec3 view_dir = normalize(u_camera_pos - world_pos);
  vec3 light_dir = normalize(u_light_dir);
  float ndv = max(dot(normal, view_dir), 0.0);
  float ndl = max(dot(normal, light_dir), 0.0);

  // Both rivers and lakes use this exact optical palette. Their only visual
  // difference is hydrodynamic energy; normalized shore depth is shared too.
  const vec3 shallow_water = vec3(0.085, 0.285, 0.280);
  const vec3 deep_water = vec3(0.070, 0.255, 0.270);
  const vec3 suspended_silt = vec3(0.155, 0.270, 0.200);

  float shore_distance = u_water_surface_kind == 1
                             ? tex_coord.y
                             : min(tex_coord.x, 1.0 - tex_coord.x);
  float normalized_depth = smoothstep(0.018, 0.38, shore_distance);
  float depth_variation = (fbm(world_pos.xz * 0.026 + vec2(17.0, -9.0)) - 0.5) *
                          0.055;
  float optical_depth = saturate(normalized_depth * 0.72 + depth_variation + 0.16);
  vec3 body_color = mix(shallow_water, deep_water, optical_depth);
  float silt = (1.0 - normalized_depth) *
               smoothstep(0.45, 0.78, fbm(world_pos.xz * 0.075 + 31.0));
  body_color = mix(body_color, suspended_silt, silt * 0.16);

  vec3 reflected_dir = reflect(-view_dir, normal);
  vec3 reflection = procedural_sky(reflected_dir, light_dir);
  float fresnel = fresnel_schlick(ndv, 0.020);
  float reflection_weight = 0.035 + fresnel * 0.22;
  vec3 color = mix(body_color * (0.91 + ndl * 0.12),
                   reflection,
                   reflection_weight);

  float roughness = mix(0.34, 0.46, saturate(length(gradient) * 1.5));
  float specular = ggx_specular(normal, view_dir, light_dir, roughness, 0.020);
  color += vec3(0.92, 0.88, 0.76) * min(specular, 0.42) * 0.15;

  float river_energy = 1.0 - lake;
  float shore_band = 1.0 - smoothstep(0.006, 0.060, shore_distance);
  float broken_edge = smoothstep(
      0.40,
      0.78,
      fbm(world_pos.xz * 0.72 + vec2(time * 0.12, -time * 0.08)));
  float shore_foam = shore_band * broken_edge * 0.055;
  float crest = smoothstep(0.64, 1.18, abs(laplacian) * 0.006 + length(gradient));
  crest *= smoothstep(0.55, 0.86,
                      fbm(world_pos.xz * 1.15 - vec2(time * 0.18, time * 0.08)));
  float foam = saturate(shore_foam + crest * mix(0.010, 0.022, river_energy));
  color = mix(color, vec3(0.76, 0.86, 0.84), foam);

  float visibility_factor = 1.0;
  if (u_has_visibility == 1 && u_visibility_size.x > 0.0 &&
      u_visibility_size.y > 0.0) {
    float tile_size = max(u_visibility_tile_size, 0.0001);
    vec2 grid = world_pos.xz / tile_size;
    grid += (u_visibility_size * 0.5) - vec2(0.5);
    vec2 visibility_uv = (grid + vec2(0.5)) / u_visibility_size;
    float visibility = texture(u_visibility_tex, visibility_uv).r;
    if (visibility < 0.25) {
      discard;
    }
    if (visibility < 0.75) {
      visibility_factor = u_explored_alpha;
    }
  }
  color *= visibility_factor * u_segment_visibility;

  float view_distance = length(u_camera_pos - world_pos);
  float fog_amount = smoothstep(u_fog_start, max(u_fog_start + 0.001, u_fog_end),
                                view_distance);
  color = mix(color, u_fog_color, fog_amount);

  // Opaque water keeps overlapping river segments and lake junctions free of
  // dark alpha seams while depth and reflection provide the material response.
  frag_color = vec4(saturate(color), 1.0);
}
