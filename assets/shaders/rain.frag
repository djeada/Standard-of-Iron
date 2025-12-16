#version 330 core

in float v_alpha;
in float v_rotation;

uniform vec3 u_rain_color;
uniform int u_weather_type;

out vec4 frag_color;

// Function to draw a snowflake pattern
float snowflake(vec2 uv, float rotation) {
  // Rotate UV coordinates
  float c = cos(rotation);
  float s = sin(rotation);
  vec2 rotated_uv = vec2(
    uv.x * c - uv.y * s,
    uv.x * s + uv.y * c
  );
  
  float dist = length(rotated_uv);
  
  // Six-pointed star pattern
  float angle = atan(rotated_uv.y, rotated_uv.x);
  float ray = abs(sin(angle * 3.0));
  
  // Center dot
  float center = 1.0 - smoothstep(0.0, 0.15, dist);
  
  // Arms of the snowflake
  float arms = ray * (1.0 - smoothstep(0.1, 0.5, dist));
  
  // Secondary details
  float details = abs(sin(angle * 6.0)) * 0.5 * (1.0 - smoothstep(0.2, 0.4, dist));
  
  return clamp(center + arms + details, 0.0, 1.0);
}

void main() {
  if (u_weather_type == 1) {
    // Snow rendering with snowflake shape
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float flake = snowflake(coord, v_rotation);
    
    // Soft edge falloff
    float edge = 1.0 - smoothstep(0.7, 1.0, length(coord));
    
    // Increased alpha for better visibility
    float alpha = flake * edge * v_alpha * 0.95;
    frag_color = vec4(u_rain_color, alpha);
  } else {
    // Rain rendering (lines)
    float alpha_multiplier = 0.6;
    frag_color = vec4(u_rain_color, v_alpha * alpha_multiplier);
  }
}
