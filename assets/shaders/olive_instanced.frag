#version 330 core

in vec3 v_world_pos;
in vec3 v_normal;
in vec3 v_color;
in vec2 v_tex_coord;
in float v_foliage_mask;
in float v_leaf_seed;
in float v_bark_seed;
in float v_branch_id;
in vec2 v_local_pos_xz;

uniform vec3 u_light_direction;

out vec4 frag_color;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

float hash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float hash3(vec3 p) {
  return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise2D(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + vec2(1.0, 0.0));
  float c = hash(i + vec2(0.0, 1.0));
  float d = hash(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {

  vec3 n = normalize(v_normal);
  vec3 l = normalize(u_light_direction);
  float diffuse = max(dot(n, l), 0.0);
  float ambient = 0.25;
  float lighting = ambient + diffuse * 0.62;

  vec3 sun_color = vec3(1.08, 0.92, 0.74);
  vec3 sky_color = vec3(0.72, 0.80, 1.00);
  float lit_t = clamp(diffuse * 1.4, 0.0, 1.0);
  vec3 light_tint = mix(sky_color * 0.50, sun_color, lit_t);

  vec2 leafPos = v_local_pos_xz * 120.0 + vec2(v_leaf_seed * 17.3, v_bark_seed * 23.1);

  float leafLayer1 = hash(floor(leafPos));
  float leafLayer2 = hash(floor(leafPos * 1.7 + vec2(5.3, 8.7)));
  float leafLayer3 = hash(floor(leafPos * 0.6 + vec2(13.1, 3.9)));

  float leafDensity = (leafLayer1 + leafLayer2 + leafLayer3) / 3.0;

  float leafEdge = noise2D(leafPos * 2.5);
  float leafFine = hash(leafPos * 0.37 + vec2(v_branch_id * 7.0));

  vec3 leafDarkGreen = vec3(0.22, 0.32, 0.20);
  vec3 leafMidGreen = vec3(0.32, 0.42, 0.28);
  vec3 leafLightGreen = vec3(0.42, 0.50, 0.38);
  vec3 leafSilver = vec3(0.52, 0.56, 0.50);

  float colorChoice = leafFine;
  vec3 leafColor = mix(leafDarkGreen, leafMidGreen, colorChoice);

  float backfacing = 1.0 - max(dot(n, l), 0.0);
  float silverShow = smoothstep(0.4, 0.8, backfacing) * leafLayer2;
  leafColor = mix(leafColor, leafSilver, silverShow * 0.5);

  float highlight = smoothstep(0.7, 0.9, leafLayer1 * leafEdge);
  leafColor = mix(leafColor, leafLightGreen, highlight * 0.4);

  leafColor = mix(leafColor, v_color, 0.15);

  float canopyDepth = 1.0 - smoothstep(0.0, 0.35, length(v_local_pos_xz));
  leafColor *= mix(0.75, 1.0, canopyDepth);

  float barkU = v_tex_coord.x * TWO_PI;
  float barkV = v_tex_coord.y;

  float furrows = pow(abs(sin(barkU * 5.0 + v_bark_seed * TWO_PI)), 0.4);
  float verticalGrain =
      noise2D(vec2(barkU * 3.0, barkV * 25.0 + v_bark_seed * 7.0));
  float barkNoise = noise2D(vec2(barkU * 8.0, barkV * 15.0)) * 0.3;
  float barkTexture = furrows * 0.5 + verticalGrain * 0.35 + barkNoise;

  vec3 barkDark = vec3(0.18, 0.16, 0.13);
  vec3 barkMid = vec3(0.30, 0.27, 0.23);
  vec3 barkLight = vec3(0.42, 0.38, 0.33);

  vec3 barkColor = mix(barkDark, barkMid, barkTexture);
  float barkHighlight =
      smoothstep(0.75, 0.95, hash(vec2(barkV * 15.0, barkU * 3.0)));
  barkColor = mix(barkColor, barkLight, barkHighlight * 0.35);

  vec3 baseColor = mix(barkColor, leafColor, v_foliage_mask);
  vec3 color = baseColor * lighting * light_tint;

  float alpha = 1.0;

  if (v_foliage_mask > 0.1) {

    float leafMask = leafDensity;

    float holePattern = noise2D(leafPos * 0.8);
    float holes = smoothstep(0.20, 0.35, holePattern);

    float clusterGaps = noise2D(v_local_pos_xz * 15.0 + vec2(v_leaf_seed * 3.0));
    float gaps = smoothstep(0.15, 0.40, clusterGaps);

    float edgeDist = length(v_local_pos_xz);
    float edgeFade = 1.0 - smoothstep(0.25, 0.50, edgeDist) * 0.5;

    float topFade = 1.0 - smoothstep(0.85, 1.0, v_tex_coord.y) * 0.4;

    alpha = leafMask * holes * gaps * edgeFade * topFade;
    alpha = mix(1.0, alpha, v_foliage_mask);

    if (leafFine > 0.82) {
      alpha *= 0.2;
    }
  }

  alpha *= smoothstep(0.0, 0.06, v_tex_coord.y);

  if (alpha < 0.15)
    discard;

  alpha = clamp(alpha * 1.3, 0.0, 1.0);

  frag_color = vec4(color, alpha);
}
