#version 330 core

in vec2 v_uv;
in float v_alpha;
in float v_shade;
in float v_detail;

uniform vec3 u_rain_color;
uniform int u_weather_type;

out vec4 frag_color;

const vec2 ARM_NORMAL_A = vec2(0.0, 1.0);
const vec2 ARM_NORMAL_B = vec2(0.8660254, 0.5);
const vec2 ARM_NORMAL_C = vec2(0.8660254, -0.5);

const float SNOW_CORE_INNER = 0.08;
const float SNOW_CORE_OUTER = 0.55;
const float SNOW_HALO_INNER = 0.1;
const float SNOW_HALO_OUTER = 0.85;
const float SNOW_HALO_WEIGHT = 0.22;
const float SNOW_CRYSTAL_WEIGHT = 0.55;
const float SNOW_ARM_INNER = 0.03;
const float SNOW_ARM_OUTER = 0.2;
const float SNOW_ARM_REACH_START = 0.2;
const float SNOW_ARM_REACH_END = 0.78;

const float RAIN_HEAD_START = 0.05;
const float RAIN_HEAD_END = 0.9;
const float RAIN_TAIL_FLOOR = 0.15;
const float RAIN_CAP_START = 0.86;
const float RAIN_HIGHLIGHT_BASE = 0.85;
const float RAIN_HIGHLIGHT_GAIN = 0.4;

const float ALPHA_CUTOFF = 0.004;

float snow_crystal(vec2 point, float radius) {

  float arm_distance =
      min(min(abs(dot(point, ARM_NORMAL_A)), abs(dot(point, ARM_NORMAL_B))),
          abs(dot(point, ARM_NORMAL_C)));
  float spine = 1.0 - smoothstep(SNOW_ARM_INNER, SNOW_ARM_OUTER, arm_distance);
  float reach = 1.0 - smoothstep(SNOW_ARM_REACH_START, SNOW_ARM_REACH_END, radius);
  return spine * reach;
}

void main() {
  float radius = length(v_uv);

  if (u_weather_type == 1) {

    float core = 1.0 - smoothstep(SNOW_CORE_INNER, SNOW_CORE_OUTER, radius);
    float halo =
        (1.0 - smoothstep(SNOW_HALO_INNER, SNOW_HALO_OUTER, radius)) * SNOW_HALO_WEIGHT;
    float shape = max(core, halo);

    if (v_detail > 0.5) {
      shape = max(shape, snow_crystal(v_uv, radius) * SNOW_CRYSTAL_WEIGHT);
    }

    float alpha = shape * v_alpha;
    if (alpha < ALPHA_CUTOFF) {
      discard;
    }
    frag_color = vec4(u_rain_color * v_shade, alpha);
  } else {

    float across = 1.0 - (v_uv.x * v_uv.x);

    float head = 0.5 * (v_uv.y + 1.0);

    float taper =
        mix(RAIN_TAIL_FLOOR, 1.0, smoothstep(RAIN_HEAD_START, RAIN_HEAD_END, head));

    float cap = 1.0 - smoothstep(RAIN_CAP_START, 1.0, abs(v_uv.y));

    float alpha = across * taper * cap * v_alpha;
    if (alpha < ALPHA_CUTOFF) {
      discard;
    }

    vec3 color =
        u_rain_color * (RAIN_HIGHLIGHT_BASE + (RAIN_HIGHLIGHT_GAIN * head * v_shade));
    frag_color = vec4(color, alpha);
  }
}
