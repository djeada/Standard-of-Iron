#version 330 core

in float v_alpha;

uniform vec3 u_rain_color;
uniform int u_weather_type; // 0 = Rain, 1 = Snow

out vec4 frag_color;

void main() { 
  float alpha_multiplier = (u_weather_type == 1) ? 0.8 : 0.6;
  frag_color = vec4(u_rain_color, v_alpha * alpha_multiplier); 
}
