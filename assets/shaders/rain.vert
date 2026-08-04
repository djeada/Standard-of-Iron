#version 330 core

layout(location = 0) in vec2 a_corner;

layout(location = 1) in vec4 a_seed;

layout(location = 2) in vec4 a_props;

uniform mat4 u_view_proj;
uniform vec3 u_camera_pos;
uniform vec3 u_camera_right;
uniform vec3 u_camera_up;
uniform float u_time;
uniform float u_intensity;
uniform float u_density;
uniform float u_rank_step;
uniform int u_weather_type;
uniform vec3 u_wind;
uniform float u_wind_strength;
uniform float u_fall_speed;
uniform float u_streak_half_length;
uniform float u_particle_half_size;
uniform vec2 u_field;
uniform float u_pixel_scale;
uniform vec2 u_speed_range;
uniform vec2 u_size_range;
uniform vec2 u_alpha_range;

out vec2 v_uv;
out float v_alpha;
out float v_shade;
out float v_detail;

const float DENSITY_FADE_BAND = 0.12;

const float EDGE_FADE_START = 0.7;

const float NEAR_FADE_START = 0.5;
const float NEAR_FADE_END = 3.5;

const float TOP_FADE_START = 0.88;

const float FIELD_BASE_FRACTION = 0.82;

const float RAIN_WIND_SPEED = 14.0;
const float SNOW_WIND_SPEED = 5.0;

const float INTENSITY_ALPHA_FLOOR = 0.55;
const float INTENSITY_FADE_IN_END = 0.12;

const float SNOW_FLUTTER_BASE_RATE = 0.7;
const float SNOW_FLUTTER_SPEED_RATE = 0.5;
const float SNOW_FLUTTER_CROSS_RATE = 0.73;
const float SNOW_FLUTTER_BASE_AMP = 0.3;
const float SNOW_FLUTTER_SIZE_AMP = 0.5;
const float SNOW_BOB_RATE = 0.5;
const float SNOW_BOB_AMP = 0.12;

const float SNOW_MIN_HALF_PIXELS = 0.65;

const float SNOW_DETAIL_HALF_PIXELS = 9.0;
const float RAIN_MIN_HALF_PIXELS = 0.55;

const float RAIN_EDGE_ON_FLOOR = 0.55;

const float INV_TWO_PI = 0.15915494;
const float SHADE_FLOOR = 0.75;
const float SHADE_RANGE = 0.25;

const float CULLED_CLIP_POSITION = 2.0;
const float MIN_VISIBLE_ALPHA = 0.002;

void main() {
  float radius = u_field.x;
  float height = u_field.y;
  bool is_snow = (u_weather_type == 1);

  float speed_scale = u_speed_range.x + a_seed.w * u_speed_range.y;
  float size_scale = u_size_range.x + a_props.x * u_size_range.y;
  float base_alpha = u_alpha_range.x + a_props.y * u_alpha_range.y;
  float fall = u_fall_speed * speed_scale;

  float carry = is_snow ? SNOW_WIND_SPEED : RAIN_WIND_SPEED;
  vec2 wind_velocity = u_wind.xz * u_wind_strength * carry;

  vec2 lattice = (a_seed.xz * radius) + (wind_velocity * u_time);
  vec2 rel = mod(lattice - u_camera_pos.xz + radius, 2.0 * radius) - radius;

  float base_y = u_camera_pos.y - (height * FIELD_BASE_FRACTION);
  float drift_y = (a_seed.y * height) - (fall * u_time);
  float y = base_y + mod(drift_y - base_y, height);

  vec3 center = vec3(u_camera_pos.x + rel.x, y, u_camera_pos.z + rel.y);

  if (is_snow) {
    vec2 wind_dir = normalize(u_wind.xz + vec2(1e-4, 0.0));
    vec2 wind_tangent = vec2(-wind_dir.y, wind_dir.x);
    float phase =
        (u_time * (SNOW_FLUTTER_BASE_RATE + (SNOW_FLUTTER_SPEED_RATE * speed_scale))) +
        a_props.z;
    float amplitude = SNOW_FLUTTER_BASE_AMP + (SNOW_FLUTTER_SIZE_AMP * size_scale);
    center.xz += ((wind_dir * sin(phase)) +
                  (wind_tangent * cos(phase * SNOW_FLUTTER_CROSS_RATE))) *
                 amplitude;
    center.y += sin(phase * SNOW_BOB_RATE) * SNOW_BOB_AMP;
  }

  vec3 to_camera = u_camera_pos - center;
  float distance_to_camera = max(length(to_camera), 1e-3);

  float pixel_world = distance_to_camera * u_pixel_scale;

  float rank = float(gl_InstanceID) * u_rank_step;
  float density_fade = clamp((u_density - rank) / DENSITY_FADE_BAND, 0.0, 1.0);

  float radial = length(rel) / radius;
  float edge_fade = 1.0 - smoothstep(EDGE_FADE_START, 1.0, radial);

  float near_fade = smoothstep(NEAR_FADE_START, NEAR_FADE_END, distance_to_camera);

  float height_fraction = (center.y - base_y) / height;
  float top_fade = 1.0 - smoothstep(TOP_FADE_START, 1.0, height_fraction);

  float visibility = density_fade * edge_fade * near_fade * top_fade;

  vec3 world_position;
  float legibility_fade = 1.0;

  if (is_snow) {

    float half_size = u_particle_half_size * size_scale;
    float legible_half = max(half_size, pixel_world * SNOW_MIN_HALF_PIXELS);

    legibility_fade = (half_size * half_size) / (legible_half * legible_half);

    float spin = a_props.z + (u_time * a_props.w);
    float spin_cos = cos(spin);
    float spin_sin = sin(spin);
    vec2 spun = vec2((a_corner.x * spin_cos) - (a_corner.y * spin_sin),
                     (a_corner.x * spin_sin) + (a_corner.y * spin_cos));

    world_position =
        center + (((u_camera_right * spun.x) + (u_camera_up * spun.y)) * legible_half);

    v_detail = legible_half > (pixel_world * SNOW_DETAIL_HALF_PIXELS) ? 1.0 : 0.0;
  } else {

    vec3 fall_direction = normalize(vec3(wind_velocity.x, -fall, wind_velocity.y) +
                                    vec3(0.0, -1e-4, 0.0));
    vec3 view_direction = -to_camera / distance_to_camera;

    vec3 across = cross(fall_direction, view_direction);
    float across_length = length(across);
    across = across_length > 1e-4 ? across / across_length : u_camera_right;

    float half_length = u_streak_half_length * speed_scale;
    float half_width = u_particle_half_size * size_scale;
    float legible_half_width = max(half_width, pixel_world * RAIN_MIN_HALF_PIXELS);

    legibility_fade = half_width / legible_half_width;

    legibility_fade *= mix(RAIN_EDGE_ON_FLOOR, 1.0, across_length);

    world_position = center + (fall_direction * (a_corner.y * half_length)) +
                     (across * (a_corner.x * legible_half_width));

    v_detail = 0.0;
  }

  float intensity_alpha = mix(INTENSITY_ALPHA_FLOOR, 1.0, u_intensity) *
                          smoothstep(0.0, INTENSITY_FADE_IN_END, u_intensity);
  v_alpha = base_alpha * intensity_alpha * visibility * legibility_fade;
  v_uv = a_corner;
  v_shade = SHADE_FLOOR + (SHADE_RANGE * a_props.z * INV_TWO_PI);

  if (v_alpha < MIN_VISIBLE_ALPHA) {

    gl_Position =
        vec4(CULLED_CLIP_POSITION, CULLED_CLIP_POSITION, CULLED_CLIP_POSITION, 1.0);
    return;
  }

  gl_Position = u_view_proj * vec4(world_position, 1.0);
}
