#include "character_wear.glsl"
// Pulled in here rather than relying on the including stage: the stage's
// #include lines are kept alphabetically sorted, so this file can be expanded
// before the lighting helpers it calls.
#include "environment_lighting.glsl"
#include "local_lighting.glsl"

// Character shading shared by the single-draw and GPU-instanced programs. The
// material-specific passes are guarded by SOI_CHARACTER_VARIANT so a program
// built for one material family carries only that family's code; the shared
// lighting, rim, wetness and readability work is identical either way.

uniform vec3 u_camera_position;

const float k_readable_zoom_near = 18.0;
const float k_readable_zoom_far = 70.0;
const float k_readable_pivot = 0.42;
const float k_readable_contrast_gain = 0.44;
const float k_readable_saturation_gain = 0.40;
const float k_readable_fill_near = 0.18;
const float k_readable_fill_far = 0.22;
const float k_readable_rim_near = 0.040;
const float k_readable_rim_far = 0.055;
const float k_readable_wear_far = 0.30;
const float k_readable_grime_far = 0.20;
const float k_readable_blood_far = 0.75;
const float k_readable_shadow_floor = 0.62;
const float k_sun_rim_power = 2.4;
const float k_sun_rim_wrap = 0.35;
const float k_sun_rim_gain = 0.10;
const float k_sun_rim_backlight_gain = 0.32;
const float k_wet_sheen_gloss = 0.55;
const float k_wet_sheen_power = 14.0;
const vec3 k_wet_darken = vec3(0.78, 0.80, 0.82);
float readable_zoom(vec3 world_position) {
  float view_distance = length(u_camera_position - world_position);
  return smoothstep(k_readable_zoom_near, k_readable_zoom_far, view_distance);
}

vec3 apply_zoom_readability(vec3 color, float zoom) {
  if (zoom <= 0.0) {
    return color;
  }

  vec3 contrasted =
      (color - k_readable_pivot) * (1.0 + k_readable_contrast_gain * zoom) +
      k_readable_pivot;
  contrasted = clamp(contrasted, 0.0, 1.0);
  float luma = dot(contrasted, vec3(0.299, 0.587, 0.114));
  vec3 saturated = mix(vec3(luma), contrasted, 1.0 + k_readable_saturation_gain * zoom);
  return clamp(saturated, 0.0, 1.0);
}

const int k_humanoid_role_skin = 2;
const int k_humanoid_role_leather = 3;
const int k_humanoid_role_leather_dark = 4;
const int k_humanoid_role_metal = 6;

vec3 shade_readable_character(vec3 base,
                              vec3 surface_normal,
                              vec3 world_position,
                              int material_id,
                              int color_role,
                              float zoom) {
  vec3 light_dir = environment_primary_direction();
  vec3 view_dir = normalize(u_camera_position - world_position);
  float scene_ambient = clamp(environment_ambient_intensity(), 0.08, 0.40);
  float readable_ambient = max(scene_ambient, 0.29);
  vec3 sun_color = environment_primary_color();
  vec3 sky_color = environment_sky_color();

  float ndl = dot(surface_normal, light_dir);
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
  float diffuse_wrap =
      material_id == k_elephant_material ? k_elephant_diffuse_wrap : 0.25;
#else
  const float diffuse_wrap = 0.25;
#endif
  float wrapped_diffuse = clamp((ndl + diffuse_wrap) / (1.0 + diffuse_wrap), 0.0, 1.0);
  float hemisphere = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);
  float fill = readable_ambient * (0.78 + hemisphere * 0.22);

  float direct = wrapped_diffuse * environment_primary_intensity() *
                 mix(1.0, 0.50, environment_night_amount());
  vec3 ambient_light =
      mix(environment_ground_bounce_color(), sky_color, hemisphere) * readable_ambient;
  vec3 color = base * (ambient_light + sun_color * direct) * environment_exposure();

  float shadow_side = 1.0 - wrapped_diffuse;
  color += base * sky_color * fill * shadow_side *
           mix(k_readable_fill_near, k_readable_fill_far, zoom);

  float rim = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), 3.0);
  color += sky_color * rim * mix(k_readable_rim_near, k_readable_rim_far, zoom);

  float back_facing = clamp(dot(-view_dir, light_dir), 0.0, 1.0);
  float sun_rim = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), k_sun_rim_power) *
                  clamp(dot(surface_normal, light_dir) + k_sun_rim_wrap, 0.0, 1.0);
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
  float sun_rim_scale =
      material_id == k_elephant_material ? k_elephant_sun_rim_scale : 1.0;
#else
  const float sun_rim_scale = 1.0;
#endif
  color += sun_color * environment_primary_intensity() * sun_rim * sun_rim_scale *
           (k_sun_rim_gain + k_sun_rim_backlight_gain * back_facing);

  float wetness = environment_wetness();
  vec3 half_vector = normalize(light_dir + view_dir);
  float n_dot_h = max(dot(surface_normal, half_vector), 0.0);
  float fresnel = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), 4.0);
  if (material_id == 2 || (material_id == 0 && color_role == k_humanoid_role_metal)) {

    float metal_glint = pow(n_dot_h, 42.0);
    float metal_sheen = pow(n_dot_h, 9.0);
    color += sun_color * environment_primary_intensity() *
             (metal_glint * 0.42 + metal_sheen * 0.12) * base;
    color += sky_color * (0.08 + fresnel * 0.22) * base;
    color += local_lighting_specular(world_position, surface_normal, view_dir, 1.0);
  } else if (material_id == 0 && (color_role == k_humanoid_role_leather ||
                                  color_role == k_humanoid_role_leather_dark)) {

    color += sun_color * environment_primary_intensity() * pow(n_dot_h, 12.0) * 0.10;
  } else if (material_id == 0 && color_role == k_humanoid_role_skin) {

    color += base * vec3(0.16, 0.05, 0.02) * shadow_side * readable_ambient;
    color += sun_color * environment_primary_intensity() * pow(n_dot_h, 18.0) * 0.05;
  } else if (wetness > 0.0) {

    bool coat = false;
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_HORSE)
    coat = coat || material_id == 6;
#endif
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_WILDLIFE)
    coat = coat || material_id == k_wildlife_material;
#endif
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
    coat = coat || material_id == k_elephant_material;
#endif
    float sheen_gain = coat ? 0.22 : 1.0;
    float sheen = pow(max(dot(surface_normal, half_vector), 0.0), k_wet_sheen_power);
    color += sun_color * sheen * 0.10 * sheen_gain * wetness *
             environment_primary_intensity();
    color += local_lighting_specular(world_position,
                                     surface_normal,
                                     view_dir,
                                     wetness * k_wet_sheen_gloss * sheen_gain);
    color = mix(color, color * k_wet_darken, wetness * 0.5);
  }
  return clamp(color, 0.0, 1.0);
}

vec3 soi_finish_character(vec3 color,
                          vec3 base,
                          vec3 surface_normal,
                          vec3 world_position,
                          vec3 camera_position,
                          int material_id,
                          int color_role,
                          float zoom) {
  vec3 light_dir = environment_primary_direction();
  vec3 sun_color = environment_primary_color();
  vec3 sky_color = environment_sky_color();
  float shadow_floor = material_id == k_elephant_material   ? k_elephant_shadow_floor
                       : material_id == k_wildlife_material ? k_wildlife_shadow_floor
                                                            : k_readable_shadow_floor;

  shadow_floor *= mix(1.0, 0.30, environment_night_amount());
  color = max(color, base * sky_color * shadow_floor);
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_WILDLIFE)
  if (material_id == k_wildlife_material) {
    vec3 view_dir = normalize(camera_position - world_position);
    float skylight = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);
    float edge = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), 3.0);
    float sheen =
        pow(max(dot(normalize(surface_normal + vec3(0.0, 0.30, 0.12)), light_dir), 0.0),
            9.0);
    color += sun_color * sheen * 0.09;
    color -= sky_color * edge * mix(k_readable_rim_near, k_readable_rim_far, zoom);
    color += sun_color * edge * k_wildlife_rim;
    color = mix(color * vec3(1.07, 1.00, 0.85), color, skylight);
    color = max(color, vec3(0.0));
  }
#endif
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_HORSE)
  if (material_id == 6) {
    bool horse_hair = color_role == 5 || color_role == 6;
    bool dark_detail = color_role == 4 || color_role == 8;
    float sheen =
        pow(max(dot(normalize(surface_normal + vec3(0.0, 0.28, 0.14)), light_dir), 0.0),
            horse_hair ? 12.0 : 7.0);
    float skylight = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);
    color += sun_color * sheen * (horse_hair ? 0.14 : 0.06);
    color += sky_color * (1.0 - skylight) * (horse_hair ? 0.04 : 0.02);
    if (dark_detail) {
      color = min(color, base * 1.10 + vec3(0.015));
    }
  }
#endif
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
  if (material_id == k_elephant_material) {
    vec3 view_dir = normalize(camera_position - world_position);
    float edge = pow(1.0 - max(dot(surface_normal, view_dir), 0.0), 3.0);
    float skylight = clamp(surface_normal.y * 0.5 + 0.5, 0.0, 1.0);

    color -= sky_color * edge * mix(k_readable_rim_near, k_readable_rim_far, zoom) *
             k_elephant_rim_cancel;

    color += environment_ground_bounce_color() * (1.0 - skylight) * 0.06;
    color = max(color, vec3(0.0));

    vec3 shoulder = vec3(1.0 - k_elephant_highlight_knee);
    vec3 over = max(color - vec3(k_elephant_highlight_knee), vec3(0.0));
    color = min(color,
                vec3(k_elephant_highlight_knee) + over * shoulder / (shoulder + over));
  }
#endif
  color = apply_zoom_readability(color, zoom);
#if SOI_CHARACTER_WANTS(SOI_CHARACTER_ELEPHANT)
  if (material_id == k_elephant_material) {

    float hide_luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(hide_luma), color, k_elephant_hide_saturation);
  }
#endif
  return color;
}
