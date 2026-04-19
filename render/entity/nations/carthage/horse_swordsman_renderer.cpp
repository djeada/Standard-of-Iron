#include "horse_swordsman_renderer.h"

#include "../../../equipment/horse/saddles/carthage_saddle_renderer.h"
#include "../../../equipment/horse/tack/reins_renderer.h"
#include "../../../humanoid/style_palette.h"
#include "../../../submitter.h"
#include "../../mounted_knight_renderer_base.h"
#include "swordsman_style.h"

#include <memory>
#include <optional>

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

  void get_variant(const DrawContext &ctx, uint32_t seed,
                   HumanoidVariant &v) const override {
    MountedKnightRendererBase::get_variant(ctx, seed, v);
    const KnightStyleConfig style = carthage_style();
    QVector3D const team_tint = resolve_team_tint(ctx);

    auto apply_color = [&](const std::optional<QVector3D> &override_color,
                           QVector3D &target) {
      target = Render::GL::Humanoid::mix_palette_color(
          target, override_color, team_tint, k_team_mix_weight,
          k_style_mix_weight);
    };

    apply_color(style.cloth_color, v.palette.cloth);
    apply_color(style.leather_color, v.palette.leather);
    apply_color(style.leather_dark_color, v.palette.leather_dark);
    apply_color(style.metal_color, v.palette.metal);
  }

};

auto make_mounted_knight_config() -> MountedKnightRendererConfig {
  MountedKnightRendererConfig config;
  config.sword_equipment_id = "sword_carthage";
  config.shield_equipment_id = "shield_carthage_cavalry";
  config.helmet_equipment_id = "carthage_heavy";
  config.armor_equipment_id = "armor_heavy_carthage";
  config.shoulder_equipment_id = "carthage_shoulder_cover_cavalry";
  config.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  config.has_shoulder = true;
  config.helmet_offset_moving = 0.03F;
  config.horse_attachments.emplace_back(
      std::make_shared<CarthageSaddleRenderer>());
  config.horse_attachments.emplace_back(std::make_shared<ReinsRenderer>());
  return config;
}

} // namespace

void register_mounted_knight_renderer(EntityRendererRegistry &registry) {
  registry.register_renderer(
      "troops/carthage/horse_swordsman",
	    [](const DrawContext &ctx, ISubmitter &out) {
	        static CarthageMountedKnightRenderer const static_renderer(
	            make_mounted_knight_config());
	        static_renderer.render(ctx, out);
	      });
}

} // namespace Render::GL::Carthage
