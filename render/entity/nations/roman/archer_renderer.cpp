#include "archer_renderer.h"

#include <array>

#include "../../../creature/pipeline/creature_asset.h"
#include "../../../humanoid/humanoid_proportion_profiles.h"
#include "archer_style.h"

namespace Render::GL::Roman {
namespace {

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_roman_archer_style();
    return true;
  }();
  (void)registered;
}

const ArcherRendererProfile k_archer_profile{
    .default_renderer_key = "troops/roman/archer",
    .default_style_key = "roman_republic",
    .base_debug_name = "troops/roman/archer/base",
    .proportion_profile = Humanoid::k_ranged_infantry_proportion_profile.with_offset(
        {.x = -0.04F, .z = -0.01F}),
    .variation = {.height_scale = 1.02F,
                  .bulk_scale = 0.95F,
                  .stance_width = 0.94F,
                  .arm_swing_amp = 0.96F},
    .kneel_depth_multiplier = 1.125F,
    .creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset,
    .loadout_slots = {ArcherLoadoutSlot::Helmet,
                      ArcherLoadoutSlot::Greaves,
                      ArcherLoadoutSlot::Quiver,
                      ArcherLoadoutSlot::Armor,
                      ArcherLoadoutSlot::Bow,
                      ArcherLoadoutSlot::Cloak},
    .loadout_slot_count = 6U,
    .apply_skin_override = false,
    .apply_carthage_variant_traits = false,
    .ensure_styles_registered = ensure_archer_styles_registered};

const std::array<ArcherRendererRegistration, 2> k_archer_renderers{{
    {.renderer_key = "troops/roman/archer",
     .style_key = "roman_republic",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset},
    {.renderer_key = "troops/roman/commanders/marcellus",
     .style_key = "roman_republic",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset},
}};

} // namespace

void register_archer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_archer_renderer_profile(registry, k_archer_profile, k_archer_renderers);
}

} // namespace Render::GL::Roman
