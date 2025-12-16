#version 330 core

in float v_alpha;
in float v_rotation;

uniform vec3 u_rain_color;
uniform int u_weather_type;

out vec4 frag_color;

float snowflake(vec2 uv, float rotation) {

  float c = cos(rotation);
  float s = sin(rotation);
  vec2 rotated_uv = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);

  float dist = length(rotated_uv);

  float angle = atan(rotated_uv.y, rotated_uv.x);
  float ray = abs(sin(angle * 3.0));

  float center = 1.0 - smoothstep(0.0, 0.15, dist);

  float arms = ray * (1.0 - smoothstep(0.1, 0.5, dist));

  float details =
      abs(sin(angle * 6.0)) * 0.5 * (1.0 - smoothstep(0.2, 0.4, dist));

  return clamp(center + arms + details, 0.0, 1.0);
}

void main() {
  if (u_weather_type == 1) {

    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float flake = snowflake(coord, v_rotation);

    float edge = 1.0 - smoothstep(0.7, 1.0, length(coord));

    float alpha = min(flake * edge * v_alpha * 1.3, 1.0);
    frag_color = vec4(u_rain_color, alpha);
  } else {

    float alpha_multiplier = 0.6;
    frag_color = vec4(u_rain_color, v_alpha * alpha_multiplier);
  }
}
