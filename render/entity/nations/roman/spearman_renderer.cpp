#include "spearman_renderer.h"

#include <array>

#include "../../spearman_renderer_common.h"
#include "spearman_style.h"

namespace Render::GL::Roman {
namespace {

const SpearmanRendererProfile k_profile{
    .proportion_profile =
        Render::GL::Humanoid::k_polearm_infantry_proportion_profile.with_offset(
            {.x = 0.02F, .y = -0.01F, .torso_scale = -0.03F}),
    .variation = {.bulk_scale = 0.94F, .stance_width = 0.95F},
    .kneel_depth_multiplier = 0.875F,
    .lean_amount_multiplier = 0.67F,
    .loadout_slots = {SpearmanLoadoutSlot::Helmet,
                      SpearmanLoadoutSlot::Shoulder,
                      SpearmanLoadoutSlot::Armor,
                      SpearmanLoadoutSlot::Spear,
                      SpearmanLoadoutSlot::Cloak,
                      SpearmanLoadoutSlot::Greaves},
    .loadout_slot_count = 6U,
    .ensure_styles_registered = &register_roman_spearman_styles,
};

const std::array<SpearmanRendererRegistration, 2> k_renderers{{
    {"troops/roman/spearman"},
    {"troops/roman/commanders/fabius_maximus"},
}};

} // namespace

void register_spearman_renderer(EntityRendererRegistry& registry) {
  register_spearman_renderer_profile(registry, k_profile, k_renderers);
}

} // namespace Render::GL::Roman
