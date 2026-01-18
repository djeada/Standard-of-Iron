#version 330 core

in vec2 v_uv;
in vec2 v_world_pos;

uniform vec4 u_color;
uniform vec4 u_hover_color;
uniform float u_hover_blend;
uniform float u_time;

uniform sampler2D u_parchment_texture;
uniform bool u_use_parchment;
uniform float u_parchment_scale;
uniform float u_parchment_strength;

uniform bool u_is_hovered;
uniform float u_pulse_speed;
uniform float u_pulse_amplitude;

out vec4 fragColor;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

float valueNoise(vec2 p) {
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
    value += amplitude * valueNoise(p * frequency);
    amplitude *= 0.5;
    frequency *= 2.0;
  }

  return value;
}

float getParchmentMask(vec2 uv) {

  float n1 = fbm(uv * u_parchment_scale, 4);
  float n2 = fbm(uv * u_parchment_scale * 2.5 + vec2(100.0), 3);
  float n3 = fbm(uv * u_parchment_scale * 5.0 + vec2(200.0), 2);

  float combined = n1 * 0.5 + n2 * 0.35 + n3 * 0.15;

  return 0.85 + combined * 0.15;
}

float getEdgeDarkening(vec2 uv) {

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

    vec4 hoverEffect = mix(color, u_hover_color, u_hover_blend);
    hoverEffect.rgb += vec3(brightness);

    color = hoverEffect;
  }

  if (u_use_parchment) {
    float parchment;

    if (textureSize(u_parchment_texture, 0).x > 1) {
      vec2 texUV = v_uv * u_parchment_scale;
      parchment = texture(u_parchment_texture, texUV).r;
    } else {
      parchment = getParchmentMask(v_uv);
    }

    float texStrength = u_parchment_strength;
    color.rgb *= mix(1.0, parchment, texStrength);

    float edge = getEdgeDarkening(v_uv);
    color.rgb *= mix(1.0, edge, texStrength * 0.5);
  }

  vec3 tintedColor = color.rgb;

  float distFromCenter = length(v_uv - vec2(0.5));
  float provinceVignette = 1.0 - distFromCenter * distFromCenter * 0.1;
  color.rgb *= provinceVignette;

  fragColor = vec4(color.rgb * color.a, color.a);
}
