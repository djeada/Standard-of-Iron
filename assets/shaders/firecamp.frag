#version 330 core
out vec4 frag_color;
in vec2 tex_coord;
in float intensity_val;
in float flame_phase;
in float flame_height;

uniform sampler2D fire_texture;
uniform float u_time;
uniform float u_glow_strength;

void main() {
  float flameHeight = clamp(flame_height, 0.0, 1.0);
  float intensityScale = clamp(intensity_val, 0.6, 1.6);

  vec2 animatedUV =
      vec2(tex_coord.x, tex_coord.y + fract(u_time * 0.45 + flame_phase * 0.05));
  vec4 texColor = texture(fire_texture, animatedUV);

  float noiseLow =
      0.5 + 0.5 * sin(u_time * 2.3 + flame_phase * 1.9 + flameHeight * 7.0);
  float noiseHigh =
      0.5 + 0.5 * sin(u_time * 4.8 + flame_phase * 3.6 + tex_coord.x * 10.0);
  float flicker = mix(noiseLow, noiseHigh, clamp(flameHeight * 1.2, 0.0, 1.0));

  vec3 baseColor =
      mix(vec3(1.18, 0.56, 0.15), vec3(0.95, 0.28, 0.08), flameHeight);
  vec3 coreGlow = vec3(1.45, 0.92, 0.44);
  vec3 flame = mix(baseColor, coreGlow, pow(1.0 - flameHeight, 2.4) * 0.6) *
               mix(0.85, 1.35, flicker) * intensityScale *
               mix(vec3(1.0), vec3(1.55), clamp(texColor.rgb, 0.0, 1.0));

  float edgeFade =
      smoothstep(0.0, 0.2, tex_coord.x) * smoothstep(0.0, 0.2, 1.0 - tex_coord.x);
  float heightFade = smoothstep(1.05, 0.42, tex_coord.y);
  float alpha = edgeFade * heightFade * mix(0.78, 1.05, flicker) *
                intensityScale * texColor.a;

  float glow = pow(1.0 - flameHeight, 2.8) * u_glow_strength;
  flame += vec3(1.26, 0.64, 0.22) * glow * intensityScale;

  flame = clamp(flame, 0.0, 3.2);
  frag_color = vec4(flame, clamp(alpha, 0.0, 1.0));
}
