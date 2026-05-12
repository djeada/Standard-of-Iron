#version 330 core

in vec2 v_uv;
in vec2 v_world_pos;

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform float u_hover_blend;
uniform float u_time;

uniform sampler2_d u_parchment_texture;
uniform bool u_use_parchment;
uniform float u_parchment_scale;
uniform float u_parchment_strength;

uniform bool u_is_hovered;
uniform float u_pulse_speed;
uniform float u_pulse_amplitude;

out vec4 frag_color;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float value_noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * (3.0 - 2.0 * f);

  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));

  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p, int octaves) {
  float value = 0.0;
  float amplitude = 0.5;
  float frequency = 1.0;

  for (int i = 0; i < octaves; i++) {
    value += amplitude * value_noise(p * frequency);
    amplitude *= 0.5;
    frequency *= 2.0;
  }

  return value;
}

float get_parchment_mask(vec2 uv) {

  float n1 = fbm(uv * u_parchment_scale, 4);
  float n2 = fbm(uv * u_parchment_scale * 2.5 + vec2(100.0), 3);
  float n3 = fbm(uv * u_parchment_scale * 5.0 + vec2(200.0), 2);

  float combined = n1 * 0.5 + n2 * 0.35 + n3 * 0.15;

  return 0.85 + combined * 0.15;
}

float get_edge_darkening(vec2 uv) {

  float center = fbm(uv * 20.0, 2);
  float right = fbm((uv + vec2(0.002, 0.0)) * 20.0, 2);
  float up = fbm((uv + vec2(0.0, 0.002)) * 20.0, 2);

  float edge = abs(center - right) + abs(center - up);
  return 1.0 - edge * 0.3;
}

void main() {
  vec4 color = u_color;

  if (u_is_hovered) {
    float pulse = 0.5 + 0.5 * sin(u_time * u_pulse_speed);
    float brightness = u_pulse_amplitude * pulse;

    vec4 hover_effect = mix(color, u_hover_color, u_hover_blend);
    hover_effect.rgb += vec3(brightness);

    color = hover_effect;
  }

  if (u_use_parchment) {
    float parchment;

    if (textureSize(u_parchment_texture, 0).x > 1) {
      vec2 tex_uv = v_uv * u_parchment_scale;
      parchment = texture(u_parchment_texture, tex_uv).r;
    } else {
      parchment = get_parchment_mask(v_uv);
    }

    float tex_strength = u_parchment_strength;
    color.rgb *= mix(1.0, parchment, tex_strength);

    float edge = get_edge_darkening(v_uv);
    color.rgb *= mix(1.0, edge, tex_strength * 0.5);
  }

  vec3 tinted_color = color.rgb;

  float dist_from_center = length(v_uv - vec2(0.5));
  float province_vignette = 1.0 - dist_from_center * dist_from_center * 0.1;
  color.rgb *= province_vignette;

  frag_color = vec4(color.rgb * color.a, color.a);
}
