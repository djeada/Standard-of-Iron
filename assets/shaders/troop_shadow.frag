#version 330 core

in vec2 v_texCoord;
in vec3 v_worldPos;

uniform float u_alpha;
uniform vec3 u_color;
uniform bool u_useTexture;
uniform sampler2D u_texture;
uniform vec2 u_lightDir; // normalized XZ direction of light

out vec4 FragColor;

void main() {
  // v_texCoord is [0,1]. Map to [-1,1] so the quad border is around |uv| = 1.
  vec2 uv = v_texCoord * 2.0 - 1.0;

  // Slight stretch in light direction so it’s not a perfect circle.
  vec2 dir = u_lightDir;
  if (length(dir) < 1e-4)
    dir = vec2(0.0, 1.0);
  dir = normalize(dir);
  vec2 tangent = vec2(-dir.y, dir.x);

  float along = dot(uv, dir);
  float across = dot(uv, tangent);

  // Ellipse shape: close to circular with a tiny light-lean.
  float alongScale = 1.15;  // bigger = longer soft shadow
  float acrossScale = 0.95; // smaller = thinner shadow

  float ax = along / alongScale;
  float ay = across / acrossScale;

  // 0 at center, grows toward ellipse boundary (>1 outside).
  float r = length(vec2(ax, ay));

  // Tiny wobble so the outline isn’t too clean.
  float wobble = 0.04 * sin(uv.x * 5.3) * sin(uv.y * 4.7);
  r = max(0.0, r + wobble);

  // Blend a wide gaussian with a gentle linear falloff to get a fuzzy blob.
  float gaussian = exp(-r * r * 2.2);       // wide, very soft bell curve
  float feather = clamp(1.0 - r, 0.0, 1.0); // keeps a hint of shape
  float shadowIntensity = mix(feather, gaussian, 0.7);
  shadowIntensity = pow(shadowIntensity, 1.35); // keep soft but less vanishing

  // Fade slightly with height so the model uniform stays in the program.
  float heightFade = clamp(1.0 - max(v_worldPos.y, 0.0) * 0.08, 0.6, 1.0);
  shadowIntensity *= heightFade;

  // Texture tint / mask if provided.
  vec3 texColor = vec3(1.0);
  float texAlpha = 1.0;
  if (u_useTexture) {
    vec4 tex = texture(u_texture, v_texCoord);
    texColor = tex.rgb;
    texAlpha = tex.a;
  }

  shadowIntensity *= texAlpha;

  // Base shadow color (dark but not black), intentionally very faint.
  vec3 shadowColor = vec3(0.013) * u_color * texColor;

  // Keep the shadow extremely transparent.
  float finalAlpha = shadowIntensity * u_alpha * 0.95;

  // Also modulate color by intensity so the blob looks soft even if blending is
  // odd.
  vec3 finalColor = shadowColor * shadowIntensity;

  FragColor = vec4(finalColor, finalAlpha);
}
