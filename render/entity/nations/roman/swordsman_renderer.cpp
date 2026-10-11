#include "swordsman_renderer.h"

#include <array>

#include "render/creature/pipeline/creature_asset.h"
#include "render/entity/nations/carthage/builder_renderer.h"
#include "render/entity/nations/roman/builder_renderer.h"
#include "render/humanoid/schema/humanoid_proportion_profiles.h"
#include "swordsman_style.h"

namespace Render::GL::Roman {
namespace {

void ensure_swordsman_styles_registered() {
  static const bool registered = []() {
    register_roman_swordsman_style();
    return true;
  }();
  (void)registered;
}

const SwordsmanRendererProfile k_swordsman_profile{
    .proportion_profile = Humanoid::k_sword_infantry_proportion_profile.with_offset(
        {.y = -0.02F, .z = -0.02F, .torso_scale = -0.04F}),
    .variation = {.bulk_scale = 0.95F, .stance_width = 0.95F},
    .kneel_depth_multiplier = 0.825F,
    .loadout_slots = {SwordsmanLoadoutSlot::Helmet,
                      SwordsmanLoadoutSlot::Greaves,
                      SwordsmanLoadoutSlot::Shoulder,
                      SwordsmanLoadoutSlot::Shield,
                      SwordsmanLoadoutSlot::Armor,
                      SwordsmanLoadoutSlot::Sword,
                      SwordsmanLoadoutSlot::Cloak},
    .loadout_slot_count = 7U,
    .apply_skin_override = false,
    .ensure_styles_registered = ensure_swordsman_styles_registered};

const std::array<SwordsmanRendererRegistration, 10> k_swordsman_renderers{{
    {.renderer_key = "troops/roman/swordsman",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/commanders/scipio_africanus",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/commanders/sempronius_longus",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/commanders/terentius_varro",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/commanders/scipio_consul_218",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/showcase_athlete",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/showcase_blademaster",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/camp_rest",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/camp_rest_helmed",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/camp_rest_armed",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
}};

void ensure_cast_wardrobe_registered() {
  ensure_swordsman_styles_registered();
  Render::GL::Roman::ensure_civilian_equipment_registered();
  Render::GL::Carthage::ensure_civilian_equipment_registered();
}

const SwordsmanRendererProfile k_cast_profile{
    .proportion_profile = k_swordsman_profile.proportion_profile,
    .variation = k_swordsman_profile.variation,
    .kneel_depth_multiplier = 0.825F,
    .loadout_slots = k_swordsman_profile.loadout_slots,
    .loadout_slot_count = 7U,
    .apply_skin_override = false,
    .ensure_styles_registered = ensure_cast_wardrobe_registered};

const SwordsmanRendererProfile k_cast_slender_profile{
    .proportion_profile = Humanoid::k_sword_infantry_proportion_profile.with_offset(
        {.x = -0.14F, .y = -0.07F, .z = -0.12F, .torso_scale = -0.14F}),
    .variation = {.bulk_scale = 0.78F, .stance_width = 0.72F},
    .kneel_depth_multiplier = 0.825F,
    .loadout_slots = k_swordsman_profile.loadout_slots,
    .loadout_slot_count = 7U,
    .apply_skin_override = false,
    .ensure_styles_registered = ensure_cast_wardrobe_registered};

const std::array<SwordsmanRendererRegistration, 6> k_cast_renderers{{
    {.renderer_key = "troops/roman/dead_company/felix_scroll",
     .creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset},
    {.renderer_key = "troops/roman/dead_company/felix_tablet",
     .creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset},
    {.renderer_key = "troops/roman/dead_company/felix",
     .creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset},
    {.renderer_key = "troops/roman/dead_company/tiro",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/dead_company/severus",
     .creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset},
    {.renderer_key = "troops/roman/dead_company/clerk",
     .creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset},
}};

const std::array<SwordsmanRendererRegistration, 2> k_cast_slender_renderers{{
    {.renderer_key = "troops/roman/dead_company/livia_scroll",
     .creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset},
    {.renderer_key = "troops/roman/dead_company/livia",
     .creature_asset_id = Render::Creature::Pipeline::k_caster_humanoid_asset},
}};

} // namespace

void register_swordsman_renderer(Render::GL::EntityRendererRegistry& registry) {
  register_swordsman_renderer_profile(
      registry, k_swordsman_profile, k_swordsman_renderers);
  register_swordsman_renderer_profile(registry, k_cast_profile, k_cast_renderers);
  register_swordsman_renderer_profile(
      registry, k_cast_slender_profile, k_cast_slender_renderers);
}

} // namespace Render::GL::Roman
