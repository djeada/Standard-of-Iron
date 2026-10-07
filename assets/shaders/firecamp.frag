#version 330 core
#include "environment_lighting.glsl"
#include "noise.glsl"
out vec4 frag_color;
in vec2 tex_coord;
in float intensity_val;
in float flame_phase;
in float flame_height;
flat in int v_part;

uniform sampler2D fire_texture;
uniform float u_time;
uniform float u_glow_strength;

float fbm(vec2 p) {
  float value = 0.0;
  float amplitude = 0.5;
  for (int octave = 0; octave < 3; ++octave) {
    value += amplitude * soi_noise_95f501(p);
    p = p * 2.03 + vec2(17.13, 9.37);
    amplitude *= 0.5;
  }
  return value / 0.875;
}

float band(float value, float edge) {
  return smoothstep(edge - 0.06, edge + 0.06, value);
}

vec4 shade_hearth() {
  vec2 uv = tex_coord;
  float r = length(uv);
  float edge_noise = fbm(uv * 2.6 + vec2(flame_phase * 1.7, 3.1));
  float ash_mask = 1.0 - smoothstep(0.70, 1.0, r + (edge_noise - 0.5) * 0.34);

  vec3 up = vec3(0.0, 1.0, 0.0);
  vec3 lit = environment_lighting(up, 0.2);
  vec3 ash_albedo =
      mix(vec3(0.11, 0.10, 0.092), vec3(0.04, 0.034, 0.03), smoothstep(0.95, 0.35, r));
  ash_albedo *= mix(0.8, 1.15, fbm(uv * 7.0 + flame_phase));
  vec3 colour = ash_albedo * lit;

  float bed_noise = fbm(uv * 3.4 + vec2(flame_phase * 2.3, 0.0));
  float bed = 1.0 - smoothstep(0.5, 0.72, r + (bed_noise - 0.5) * 0.28);
  float coal_cells = fbm(uv * 7.5 + vec2(flame_phase, u_time * 0.05));
  float cracks = smoothstep(0.50, 0.70, coal_cells);
  float pulse = 0.72 + 0.28 * sin(u_time * 2.3 + coal_cells * 9.0 + flame_phase * 3.0);
  float heat = bed * (0.35 + 0.65 * cracks) * pulse * u_glow_strength;
  vec3 charcoal = vec3(0.025, 0.017, 0.013) * lit;
  vec3 ember = mix(vec3(0.95, 0.22, 0.05), vec3(1.9, 0.86, 0.26), cracks * pulse);
  colour = mix(colour, charcoal, bed);
  colour += ember * heat * clamp(intensity_val, 0.7, 1.4);

  return vec4(colour, ash_mask * 0.94);
}

vec4 shade_smoke() {
  float h = clamp(flame_height, 0.0, 1.0);
  float x = tex_coord.x * 2.0 - 1.0;
  float drift = fbm(vec2(x * 0.9 + flame_phase, h * 1.6 - u_time * 0.18));
  float column = 1.0 - smoothstep(0.18, 0.9, abs(x + (drift - 0.5) * 0.9));
  float puffs = smoothstep(
      0.30, 0.78, fbm(vec2(x * 1.7 + flame_phase * 2.0, h * 2.6 - u_time * 0.42)));
  float fade = smoothstep(0.0, 0.25, h) * (1.0 - smoothstep(0.45, 1.0, h));
  float alpha = column * puffs * fade * 0.26;

  vec3 lit = environment_lighting(vec3(0.0, 0.6, 0.8), 0.6);
  vec3 smoke = vec3(0.50, 0.48, 0.46) * lit;
  smoke += vec3(0.55, 0.24, 0.07) * pow(1.0 - h, 3.0) * 0.55 * u_glow_strength;
  return vec4(smoke, alpha);
}

vec4 shade_flame() {
  float h = clamp(flame_height, 0.0, 1.0);
  float intensity_scale = clamp(intensity_val, 0.7, 1.4);
  float x = tex_coord.x * 2.0 - 1.0;

  float rise = u_time * 1.15;
  float lick = fbm(vec2(x * 1.4 + flame_phase * 0.7, h * 2.2 - rise));
  float tongues = fbm(vec2(x * 2.6 + flame_phase * 1.3, h * 1.4 - rise * 1.4));
  float bend =
      (lick - 0.5) * 0.55 * h + sin(h * 4.0 + u_time * 2.6 + flame_phase) * 0.06 * h;
  float px = x + bend;

  float profile = pow(1.0 - h, 0.85) * (0.78 + 0.32 * smoothstep(0.0, 0.25, h));
  float shape = profile - abs(px) * 1.05 + (tongues - 0.5) * 0.55 * h;
  float body = smoothstep(0.0, 0.09, shape);
  body *= smoothstep(0.0, 0.05, h + 0.02);
  body *= 1.0 - smoothstep(0.80, 0.98, h + (tongues - 0.5) * 0.3);

  float heat = shape / max(profile, 0.05) * (1.0 - h * 0.65);
  vec3 rim = vec3(0.95, 0.24, 0.05);
  vec3 body_col = vec3(1.35, 0.56, 0.11);
  vec3 core_col = vec3(1.7, 1.0, 0.3);
  vec3 base_col = vec3(1.9, 1.38, 0.62);
  vec3 colour = rim;
  colour = mix(colour, body_col, band(heat, 0.24));
  colour = mix(colour, core_col, band(heat, 0.6));
  colour = mix(colour, base_col, band(heat, 0.8) * (1.0 - smoothstep(0.08, 0.3, h)));

  float flicker = 0.9 + 0.1 * sin(u_time * 9.0 + flame_phase * 3.0 + h * 6.0);
  colour *= flicker * mix(0.85, 1.0, u_glow_strength / 1.16) * intensity_scale;

  float alpha = body * mix(0.96, 0.75, h);
  return vec4(colour, alpha);
}

void main() {
  vec4 result;
  if (v_part == 1) {
    result = shade_hearth();
  } else if (v_part == 2) {
    result = shade_smoke();
  } else {
    result = shade_flame();
  }
  result.rgb = clamp(result.rgb, 0.0, 3.0);
  if (result.a < 0.004) {
    discard;
  }
  frag_color = vec4(result.rgb, clamp(result.a, 0.0, 1.0));
}
