#include "spearman_renderer.h"

#include <array>

#include "../../spearman_renderer_common.h"
#include "spearman_style.h"

namespace Render::GL::Carthage {
namespace {

const SpearmanRendererProfile k_profile{
    .proportion_profile =
        Render::GL::Humanoid::k_polearm_infantry_proportion_profile.with_offset(
            {.x = -0.02F, .y = 0.01F, .torso_scale = 0.03F}),
    .variation = {.bulk_scale = 0.90F, .stance_width = 0.92F},
    .kneel_depth_multiplier = 0.875F,
    .lean_amount_multiplier = 0.67F,
    .loadout_slots = {SpearmanLoadoutSlot::Helmet,
                      SpearmanLoadoutSlot::Shoulder,
                      SpearmanLoadoutSlot::Spear,
                      SpearmanLoadoutSlot::Armor,
                      SpearmanLoadoutSlot::Cloak},
    .loadout_slot_count = 5U,
    .apply_carthage_beard_traits = true,
    .use_carthage_beard_archetypes = true,
    .ensure_styles_registered = &register_carthage_spearman_styles,
};

const std::array<SpearmanRendererRegistration, 2> k_renderers{{
    {"troops/carthage/spearman"},
    {"troops/carthage/commanders/hanno_the_great"},
}};

} // namespace

void register_spearman_renderer(EntityRendererRegistry& registry) {
  register_spearman_renderer_profile(registry, k_profile, k_renderers);
}

} // namespace Render::GL::Carthage
