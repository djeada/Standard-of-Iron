#include "horse_swordsman_renderer.h"

#include <optional>

#include "../../../creature/pipeline/creature_asset.h"
#include "../../../humanoid/style_palette.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"
#include "../equipment_loadout_catalog.h"
#include "swordsman_style.h"

namespace Render::GL::Carthage {
namespace {

constexpr float k_team_mix_weight = 0.6F;
constexpr float k_style_mix_weight = 0.4F;

auto carthage_style() -> KnightStyleConfig {
  KnightStyleConfig style;
  style.cloth_color = QVector3D(0.15F, 0.36F, 0.55F);
  style.leather_color = QVector3D(0.32F, 0.22F, 0.12F);
  style.leather_dark_color = QVector3D(0.20F, 0.14F, 0.09F);
  style.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  return style;
}

class CarthageMountedKnightRenderer : public MountedKnightRendererBase {
public:
  using MountedKnightRendererBase::MountedKnightRendererBase;

  void get_variant(const DrawContext& ctx,
                   uint32_t seed,
                   HumanoidVariant& v) const override {
    MountedKnightRendererBase::get_variant(ctx, seed, v);
    const KnightStyleConfig style = carthage_style();
    QVector3D const team_tint = resolve_team_tint(ctx);

    auto apply_color = [&](const std::optional<QVector3D>& override_color,
                           QVector3D& target) {
      target = Render::GL::Humanoid::mix_palette_color(
          target, override_color, team_tint, k_team_mix_weight, k_style_mix_weight);
    };

    apply_color(style.cloth_color, v.palette.cloth);
    apply_color(style.leather_color, v.palette.leather);
    apply_color(style.leather_dark_color, v.palette.leather_dark);
    apply_color(style.metal_color, v.palette.metal);
  }
};

auto make_mounted_knight_config() -> MountedKnightRendererConfig {
  MountedKnightRendererConfig config;
  const auto loadout =
      Render::GL::Nation::resolve_equipment_loadout("troops/carthage/horse_swordsman");
  config.sword_equipment_id = loadout.ids.sword;
  config.shield_equipment_id = loadout.ids.shield;
  config.helmet_equipment_id = loadout.ids.helmet;
  config.armor_equipment_id = loadout.ids.armor;
  config.shoulder_equipment_id = loadout.ids.shoulder;
  config.horse_saddle_equipment_id = loadout.ids.horse_saddle;
  config.horse_bridle_equipment_id = loadout.ids.horse_bridle;
  config.horse_reins_equipment_id = loadout.ids.horse_reins;
  config.horse_blanket_equipment_id = loadout.ids.horse_blanket;
  config.horse_barding_equipment_id = loadout.ids.horse_barding;
  config.horse_crupper_equipment_id = loadout.ids.horse_crupper;
  config.horse_decoration_equipment_id = loadout.ids.horse_decoration;
  config.sword_handle = loadout.sword_handle;
  config.shield_handle = loadout.shield_handle;
  config.helmet_handle = loadout.helmet_handle;
  config.armor_handle = loadout.armor_handle;
  config.shoulder_handle = loadout.shoulder_handle;
  config.horse_saddle_handle = loadout.horse_saddle_handle;
  config.horse_bridle_handle = loadout.horse_bridle_handle;
  config.horse_reins_handle = loadout.horse_reins_handle;
  config.horse_blanket_handle = loadout.horse_blanket_handle;
  config.horse_barding_handle = loadout.horse_barding_handle;
  config.horse_crupper_handle = loadout.horse_crupper_handle;
  config.horse_decoration_handle = loadout.horse_decoration_handle;
  config.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  config.has_shoulder = loadout.shoulder_handle != k_invalid_equipment_handle;
  config.rider_debug_name = "troops/carthage/horse_swordsman/rider";
  config.mount_debug_name = "troops/carthage/horse_swordsman/mount";
  config.rider_creature_asset_id = Render::Creature::Pipeline::k_humanoid_sword_asset;
  return config;
}

} // namespace

void register_mounted_knight_renderer(EntityRendererRegistry& registry) {
  registry.register_renderer(
      "troops/carthage/horse_swordsman", [](const DrawContext& ctx, ISubmitter& out) {
        static CarthageMountedKnightRenderer const static_renderer(
            make_mounted_knight_config());
        static_renderer.render(ctx, out);
      });
}

} // namespace Render::GL::Carthage
