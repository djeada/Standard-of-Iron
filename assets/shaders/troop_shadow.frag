#version 330 core

in vec2 v_texCoord;
in vec3 v_worldPos;

uniform float u_alpha;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform sampler2D u_texture;
uniform vec2 u_lightDir;

out vec4 FragColor;

void main() {

  vec2 uv = v_texCoord * 2.0 - 1.0;

  vec2 dir = u_lightDir;
  if (length(dir) < 1e-4)
    dir = vec2(0.0, 1.0);
  dir = normalize(dir);
  vec2 tangent = vec2(-dir.y, dir.x);

  float along = dot(uv, dir);
  float across = dot(uv, tangent);

  float alongScale = 1.15;
  float acrossScale = 0.95;

  float ax = along / alongScale;
  float ay = across / acrossScale;

  float r = length(vec2(ax, ay));

  float wobble = 0.04 * sin(uv.x * 5.3) * sin(uv.y * 4.7);
  r = max(0.0, r + wobble);

  float gaussian = exp(-r * r * 2.2);
  float feather = clamp(1.0 - r, 0.0, 1.0);
  float shadowIntensity = mix(feather, gaussian, 0.7);
  shadowIntensity = pow(shadowIntensity, 1.35);

  float heightFade = clamp(1.0 - max(v_worldPos.y, 0.0) * 0.08, 0.6, 1.0);
  shadowIntensity *= heightFade;

  vec3 texColor = vec3(1.0);
  float texAlpha = 1.0;
  if (u_useTexture) {
    vec4 tex = texture(u_texture, v_texCoord);
    texColor = tex.rgb;
    texAlpha = tex.a;
  }

  shadowIntensity *= texAlpha;

  vec3 shadowColor = vec3(0.013) * u_color * texColor;

  float finalAlpha = shadowIntensity * u_alpha * 0.95;

  vec3 finalColor = shadowColor * shadowIntensity;

  FragColor = vec4(finalColor, finalAlpha);
}
