#version 330 core
#include "noise.glsl"

in vec2 v_uv;
in vec3 v_normal;
in vec3 v_world_pos;
in float v_height;

uniform sampler2D u_base_texture;
uniform sampler2D u_hillshade_texture;
uniform sampler2D u_parchment_texture;

uniform vec3 u_light_direction;
uniform float u_ambient_strength;
uniform float u_hillshade_strength;
uniform float u_ao_strength;

uniform bool u_use_hillshade;
uniform bool u_use_parchment;
uniform bool u_use_lighting;

uniform vec3 u_water_deep_color;
uniform vec3 u_water_shallow_color;
uniform vec3 u_lowland_tint;
uniform vec3 u_highland_tint;
uniform vec3 u_mountain_tint;

uniform float u_elevation_scale;

out vec4 frag_color;

float compute_ao(vec3 normal) {

  float ao = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);

  return mix(1.0, ao, u_ao_strength);
}

float compute_hillshade(vec3 normal, vec3 light_dir) {
  float ndotl = max(0.0, dot(normal, light_dir));
  return u_ambient_strength + (1.0 - u_ambient_strength) * ndotl;
}

// A hypsometric ramp in the tradition of a printed atlas: coastal plains stay
// the colour of the paper, foothills warm towards ochre, and only real mountain
// takes the tan.
//
// These are multipliers over the base texture, and the base texture is already
// a finished cream-and-slate map -- so every stop sits near white. A ramp that
// bottoms out at 0.6 would be a perfectly good standalone palette and is the
// wrong thing entirely on top of paper: it just drains it. The elevation pass
// is a wash, not a repaint.
vec3 get_elevation_tint(float height) {
  if (height < 0.0) {
    float depth = clamp(-height, 0.0, 1.0);
    return mix(u_water_shallow_color, u_water_deep_color, depth);
  }

  float h = clamp(height, 0.0, 1.0);
  if (h < 0.14) {
    // Coastal plain: leave the paper alone.
    return mix(vec3(1.0), u_lowland_tint, smoothstep(0.0, 0.14, h));
  }
  if (h < 0.38) {
    float t = smoothstep(0.14, 0.38, h);
    return mix(u_lowland_tint, u_highland_tint, t);
  }
  float t = smoothstep(0.38, 0.85, h);
  return mix(u_highland_tint, u_mountain_tint, t);
}

// The land mask, read at full resolution. Cream paper is warm (red above blue),
// open sea is cold, so the base texture tells land from water exactly where the
// coastline was drawn -- anti-aliasing included -- without a second lookup into
// the height field, which is a quarter of the resolution and disagrees with the
// drawn coast by a pixel or two everywhere.
float land_at(vec2 uv) {
  vec4 c = textureLod(u_base_texture, vec2(uv.x, 1.0 - uv.y), 0.0);
  return smoothstep(-0.01, 0.05, c.r - c.b);
}

float get_parchment_pattern(vec2 uv) {

  float n1 = soi_fbm_d6cc9d(uv * 8.0, 3);
  float n2 = soi_fbm_d6cc9d(uv * 20.0 + vec2(100.0), 2);

  float pattern = n1 * 0.6 + n2 * 0.4;

  // Paper tooth, not a stain: a few percent of variation is all a print grain
  // ever is, and the base texture already carries the map's own value range.
  return 0.955 + pattern * 0.045;
}

void main() {

  vec2 uv = vec2(v_uv.x, 1.0 - v_uv.y);
  vec4 base_color = texture(u_base_texture, uv);

  vec3 color = base_color.rgb;

  // Land is told from sea by the base texture's own palette -- cream paper is
  // warm (red above blue), open water is cold. Taking the mask from the texture
  // instead of from the height field means it lands exactly on the coastline
  // that was drawn, anti-aliasing included.
  //
  // The terrain layer then paints land only and lets the ocean pass show
  // through everywhere else. Before this the map drew its own sea inside the
  // surveyed rectangle and the ocean drew another one outside it, and the two
  // met in a hard edge straight down the Adriatic.
  float land = land_at(v_uv);
  color *= mix(vec3(1.0), get_elevation_tint(v_height * u_elevation_scale), land);

  // Relief is applied once. Hillshade and lighting used to be two separate
  // multiplies over the same normal, so every slope was darkened twice and the
  // whole map sat far below the paper's own brightness.
  if (u_use_hillshade || u_use_lighting) {
    vec3 light_dir = normalize(u_light_direction);
    float ndotl = max(0.0, dot(v_normal, light_dir));
    float shade = u_ambient_strength + (1.0 - u_ambient_strength) * ndotl;
    shade *= compute_ao(v_normal);

    // Centred on 1.0 rather than applied as a straight multiply: slopes facing
    // the light lift above the paper and slopes facing away drop below it, so
    // relief reads as relief instead of as dirt.
    float relief = 1.0 + (shade - 1.0) * u_hillshade_strength;
    color *= mix(1.0, relief, land);
  }

  if (u_use_parchment) {
    float parchment = get_parchment_pattern(v_uv);
    color *= mix(1.0, parchment, land);
    color = mix(color, color * vec3(1.02, 1.0, 0.96), 0.3);
  }

  // An inked coastline, drawn from the land side. Every shore is stepped out by
  // a fixed number of *pixels* rather than texels -- fwidth gives the uv a
  // fragment covers -- so the line holds its weight from the whole-basin view
  // down to a single province. Drawing it as geometry instead would mean
  // glLineWidth, which core profile drivers are free to clamp to one pixel and
  // mostly do.
  vec2 pixel = fwidth(v_uv);

  vec2 ink_step = pixel * 1.7;
  float offshore = min(
      min(land_at(v_uv + vec2(ink_step.x, 0.0)), land_at(v_uv - vec2(ink_step.x, 0.0))),
      min(land_at(v_uv + vec2(0.0, ink_step.y)),
          land_at(v_uv - vec2(0.0, ink_step.y))));
  float coast = land * (1.0 - offshore);
  color = mix(color, color * vec3(0.34, 0.31, 0.28), coast);

  // The shallows every engraved chart draws: a pale band of water hugging the
  // shore, widest where the coast is straight and pinched inside bays. Sampled
  // on the diagonals as well, or the band squares off around headlands.
  vec2 halo_step = pixel * 5.0;
  float ashore =
      land_at(v_uv + vec2(halo_step.x, 0.0)) + land_at(v_uv - vec2(halo_step.x, 0.0)) +
      land_at(v_uv + vec2(0.0, halo_step.y)) + land_at(v_uv - vec2(0.0, halo_step.y)) +
      land_at(v_uv + halo_step) + land_at(v_uv - halo_step) +
      land_at(v_uv + vec2(halo_step.x, -halo_step.y)) +
      land_at(v_uv - vec2(halo_step.x, -halo_step.y));
  // Averaged, not maxed. Taking the strongest neighbour draws a halo around
  // every sub-pixel islet in the Tyrrhenian, and since the islet itself is too
  // small to see, the reader gets a field of pale dashes floating in open sea.
  // A mean asks for a real shore in the neighbourhood before it draws one.
  float shallows = (1.0 - land) * smoothstep(0.18, 0.62, ashore * 0.125);
  color = mix(color, mix(color, vec3(0.55, 0.68, 0.74), 0.5), shallows);

  // A soft edge fall-off, not a bowl. The old vignette pulled the corners down
  // hard enough to look like a stain.
  vec2 vignette_coord = v_uv * 2.0 - 1.0;
  float vignette = 1.0 - smoothstep(0.75, 1.5, length(vignette_coord)) * 0.06;
  color *= vignette;

  // Land is opaque and open water is left to the ocean pass underneath, but the
  // shallows have to survive to be seen, so they carry their own coverage.
  frag_color = vec4(color, base_color.a * max(land, shallows));
}
