#include "swordsman_renderer.h"

#include <array>

#include "../../../creature/pipeline/creature_asset.h"
#include "../../../humanoid/humanoid_proportion_profiles.h"
#include "swordsman_style.h"

namespace Render::GL::Carthage {
namespace {

void ensure_swordsman_styles_registered() {
  static const bool registered = []() {
    register_carthage_swordsman_style();
    return true;
  }();
  (void)registered;
}

const SwordsmanRendererProfile k_swordsman_profile{
    .proportion_profile = Humanoid::k_sword_infantry_proportion_profile.with_offset(
        {.y = 0.02F, .z = 0.02F, .torso_scale = 0.02F}),
    .variation = {.bulk_scale = 1.0F, .stance_width = 1.0F},
    .kneel_depth_multiplier = 0.825F,
    .loadout_slots = {SwordsmanLoadoutSlot::Helmet,
                      SwordsmanLoadoutSlot::Shield,
                      SwordsmanLoadoutSlot::Shoulder,
                      SwordsmanLoadoutSlot::Armor,
                      SwordsmanLoadoutSlot::Sword,
                      SwordsmanLoadoutSlot::Cloak,
                      SwordsmanLoadoutSlot::Cloak},
    .loadout_slot_count = 6U,
    .apply_skin_override = true,
    .ensure_styles_registered = ensure_swordsman_styles_registered};

const std::array<SwordsmanRendererRegistration, 2> k_swordsman_renderers{{
    {.renderer_key = "troops/carthage/swordsman",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/carthage/commanders/hannibal_barca",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
}};

const std::array<SwordsmanRendererRegistration, 1> k_skeleton_swordsman_renderers{{
    {.renderer_key = "troops/iron_sepulcher/skeleton_swordsman",
     .creature_asset_id = Render::Creature::Pipeline::k_skeleton_humanoid_asset},
}};

} // namespace

void register_knight_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_swordsman_renderer_profile(
      registry, k_swordsman_profile, k_swordsman_renderers);
}

void register_skeleton_swordsman_renderer(
    Render::GL::EntityRendererRegistry& registry) {
  register_swordsman_renderer_profile(
      registry, k_swordsman_profile, k_skeleton_swordsman_renderers);
}

} // namespace Render::GL::Carthage
