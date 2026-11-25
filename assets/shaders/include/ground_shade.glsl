#include "environment_lighting.glsl"

// Ground in shade keeps its own colour. The sky fill and the cast-shadow tint
// are both blue; multiplied into a green or straw albedo they turn unlit
// slopes and long shadows teal-black. This pulls a shaded ground pixel back
// toward a darker, slightly cooler copy of what the same ground looks like
// lying open to the sky, and keeps the deepest shade off black. Shadows that
// keep half their light or more are left at full contrast, so cast shadows
// under units and buildings read exactly as before.
const float k_ground_shade_hue_keep = 0.78;
const float k_ground_shade_floor = 0.30;
const vec3 k_ground_shade_tone = vec3(0.90, 1.02, 1.05);
const vec3 k_ground_shade_luma = vec3(0.2126, 0.7152, 0.0722);

vec3 ground_shade_grade(vec3 lit, vec3 open_lit, vec3 shade_tone) {
  float open_luma = max(dot(open_lit, k_ground_shade_luma), 1e-4);
  float relative = dot(lit, k_ground_shade_luma) / open_luma;
  float sunk = 1.0 - smoothstep(0.0, 2.0 * k_ground_shade_floor, relative);
  float lifted = relative + k_ground_shade_floor * sunk * sunk;
  vec3 raised = lit + open_lit * (lifted - relative);
  vec3 tone = mix(shade_tone, vec3(1.0), smoothstep(0.25, 1.0, lifted));
  vec3 own_shade = open_lit * lifted * tone;
  float in_shade = 1.0 - smoothstep(0.82, 1.0, relative);
  return mix(raised, own_shade, k_ground_shade_hue_keep * in_shade);
}

// A low sun rakes across level ground: walls and soldiers take it full on
// while the field under them gets a third of it, so mornings and evenings
// drew a bright army on a murky floor. The eye (and any camera) exposes for
// the field. This brings open ground most of the way back to its midday
// brightness while the sun is up; the colour of the light and every shadow
// ratio are untouched, and night keeps its darkness.
const float k_ground_daylight_target = 0.84;
const float k_ground_daylight_follow = 0.62;
const float k_ground_daylight_max_gain = 1.85;

float ground_daylight_gain(vec3 open_ground_light) {
  float open_luma = max(dot(open_ground_light, k_ground_shade_luma), 1e-3);
  float gain = clamp(k_ground_daylight_target / open_luma, 1.0, k_ground_daylight_max_gain);
  float day = 1.0 - environment_darkness_amount();
  return mix(1.0, gain, k_ground_daylight_follow * day);
}

vec3 ground_shade_grade(vec3 lit, vec3 open_lit) {
  return ground_shade_grade(lit, open_lit, k_ground_shade_tone);
}

// The light open, level ground receives: what the shade grade and the
// daylight gain both measure against.
vec3 ground_open_light() {
  return environment_ambient_light(vec3(0.0, 1.0, 0.0)) +
         environment_primary_color() * environment_primary_intensity() *
             (soi_wrapped_diffuse(vec3(0.0, 1.0, 0.0)) * 0.76);
}

// The gain is measured on level ground. A slope that faces a low sun already
// takes it full on, and the same gain on top bleached sunward cliffs to cream;
// such a face is only raised as far as a little above the level ground beside
// it, and never lowered.
const float k_ground_daylight_slope_headroom = 1.12;

float ground_daylight_gain(vec3 open_ground_light, vec3 lit, vec3 open_lit) {
  float gain = ground_daylight_gain(open_ground_light);
  float luma = max(dot(lit, k_ground_shade_luma), 1e-4);
  float ceiling = gain * dot(open_lit, k_ground_shade_luma) *
                  k_ground_daylight_slope_headroom;
  return clamp(ceiling / luma, 1.0, gain);
}
