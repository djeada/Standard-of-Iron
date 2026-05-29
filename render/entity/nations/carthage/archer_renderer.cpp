#include "archer_renderer.h"

#include <array>

#include "../../../creature/pipeline/creature_asset.h"
#include "../../../humanoid/humanoid_proportion_profiles.h"
#include "archer_style.h"

namespace Render::GL::Carthage {
namespace {

void ensure_archer_styles_registered() {
  static const bool registered = []() {
    register_carthage_archer_style();
    return true;
  }();
  (void)registered;
}

const ArcherRendererProfile k_archer_profile{
    .default_renderer_key = "troops/carthage/archer",
    .default_style_key = "carthage",
    .base_debug_name = "troops/carthage/archer/base",
    .proportion_profile = Humanoid::k_ranged_infantry_proportion_profile.with_offset(
        {.x = 0.05F, .z = 0.01F}),
    .variation = {.height_scale = 1.06F,
                  .bulk_scale = 0.90F,
                  .stance_width = 0.90F,
                  .arm_swing_amp = 0.92F},
    .kneel_depth_multiplier = 1.125F,
    .creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset,
    .loadout_slots = {ArcherLoadoutSlot::Helmet,
                      ArcherLoadoutSlot::Armor,
                      ArcherLoadoutSlot::Quiver,
                      ArcherLoadoutSlot::Bow,
                      ArcherLoadoutSlot::Cloak,
                      ArcherLoadoutSlot::Cloak},
    .loadout_slot_count = 5U,
    .apply_skin_override = true,
    .apply_carthage_variant_traits = true,
    .ensure_styles_registered = ensure_archer_styles_registered};

const std::array<ArcherRendererRegistration, 2> k_archer_renderers{{
    {.renderer_key = "troops/carthage/archer",
     .style_key = "carthage",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset},
    {.renderer_key = "troops/carthage/commanders/hasdrubal_barca",
     .style_key = "carthage",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_asset},
}};

const std::array<ArcherRendererRegistration, 1> k_skeleton_archer_renderers{{
    {.renderer_key = "troops/iron_sepulcher/skeleton_archer",
     .style_key = "iron_sepulcher",
     .creature_asset_id = Render::Creature::Pipeline::k_skeleton_humanoid_asset},
}};

} // namespace

void register_archer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_archer_renderer_profile(registry, k_archer_profile, k_archer_renderers);
}

void register_skeleton_archer_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_archer_renderer_profile(
      registry, k_archer_profile, k_skeleton_archer_renderers);
}

} // namespace Render::GL::Carthage
