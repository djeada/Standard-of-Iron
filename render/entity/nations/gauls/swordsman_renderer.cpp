#include "swordsman_renderer.h"

#include <array>

#include "palette.h"
#include "render/creature/pipeline/creature_asset.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"

namespace Render::GL::Gauls {
namespace {

void ensure_swordsman_styles_registered() {
  static const bool registered = []() {
    ::Render::GL::register_swordsman_style("gauls", troop_style());
    return true;
  }();
  (void)registered;
}

const SwordsmanRendererProfile k_swordsman_profile{
    .proportion_profile = Humanoid::k_sword_infantry_proportion_profile.with_offset(
        {.y = 0.03F, .z = 0.01F, .torso_scale = 0.03F}),
    .variation = {.bulk_scale = 1.04F, .stance_width = 1.04F},
    .kneel_depth_multiplier = 0.825F,
    .loadout_slots = {SwordsmanLoadoutSlot::Helmet,
                      SwordsmanLoadoutSlot::Shield,
                      SwordsmanLoadoutSlot::Greaves,
                      SwordsmanLoadoutSlot::Armor,
                      SwordsmanLoadoutSlot::Sword},
    .loadout_slot_count = 5U,
    .apply_skin_override = false,
    .ensure_styles_registered = ensure_swordsman_styles_registered};

const std::array<SwordsmanRendererRegistration, 1> k_swordsman_renderers{{
    {.renderer_key = "troops/gauls/swordsman",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
}};

} // namespace

void register_swordsman_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_swordsman_renderer_profile(
      registry, k_swordsman_profile, k_swordsman_renderers);
}

} // namespace Render::GL::Gauls
