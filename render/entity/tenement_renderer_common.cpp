#include "render/entity/tenement_renderer_common.h"

#include <array>
#include <memory>
#include <string>

#include "game/core/entity.h"
#include "game/visuals/building_asset_key.h"
#include "render/entity/building_archetype_library.h"
#include "render/entity/building_render_common.h"
#include "render/entity/building_torches.h"
#include "render/entity/entity_presentation.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_activity_submit.h"
#include "render/entity/home_renderer_common.h"
#include "render/entity/registry.h"
#include "render/frame/entity_appearance.h"
#include "render/frame/submitter.h"

namespace Render::GL {
namespace {

// The baked variants and activity anchors of one nation's tenement, built once.
struct TenementKit {
  std::array<const BuildingArchetypeSet*, k_tenement_variant_count> sets{};
  std::array<HomeSmokeAnchor, k_tenement_variant_count> anchors{};
  std::array<TorchMount, k_tenement_variant_count> torches{};
};

auto door_x(TenementGround ground) -> float {
  return ground == TenementGround::Shop ? 0.72F : 0.0F;
}

auto build_kit(const TenementRendererConfig& config)
    -> std::shared_ptr<const TenementKit> {
  auto kit = std::make_shared<TenementKit>();
  for (int variant = 0; variant < k_tenement_variant_count; ++variant) {
    const auto index = static_cast<std::size_t>(variant);
    kit->sets[index] = &building_archetype_set(std::string(config.archetype_prefix) +
                                               std::to_string(variant));
    const float door = door_x(tenement_traits(variant).ground);
    const float line_y =
        TenementLot::k_pavement_top + TenementLot::k_ground_storey + 0.30F;
    kit->anchors[index] = HomeSmokeAnchor{
        .vent = config.vent(variant),
        .smoke_tint = QVector3D(0.56F, 0.54F, 0.51F),
        .plume_radius = 0.58F,
        .doorstep =
            QVector3D(door, TenementLot::k_pavement_top, TenementLot::k_front + 0.20F),
        .outward = QVector3D(0.0F, 0.0F, 1.0F),
        .cloth_span = 0.0F,
        .lamp = QVector3D(door - 0.34F, 0.66F, TenementLot::k_front + 0.07F),
        .shutter_width = 0.0F,
        // Washing dries over the court, strung from the first floor.
        .line_a = QVector3D(-0.92F, line_y, TenementLot::k_rear - 0.05F),
        .line_b = QVector3D(0.84F, line_y, TenementLot::k_rear - 0.05F),
        .line_pole = false,
        .max_laundry = 4,
        .hens = false,
    };
    kit->torches[index] =
        TorchMount{.at = QVector3D(door + 0.33F, 0.70F, TenementLot::k_front + 0.02F),
                   .outward = QVector3D(0.0F, 0.0F, 1.0F),
                   .ghost = config.carthage};
  }
  return kit;
}

auto variant_of(const DrawContext& ctx) -> int {
  return tenement_visual_variant(ctx.model(0, 3), ctx.model(2, 3));
}

class TenementActivity final : public EntityPresentationSystem {
public:
  explicit TenementActivity(std::shared_ptr<const TenementKit> kit)
      : m_kit(std::move(kit)) {}
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "home_activity";
  }
  void present(const DrawContext& ctx, EntityPresentation& out) const override {
    out.home = resolve_home_activity_plan(
        ctx, m_kit->anchors[static_cast<std::size_t>(variant_of(ctx))]);
  }

private:
  std::shared_ptr<const TenementKit> m_kit;
};

} // namespace

void register_tenement_renderer_variant(EntityRendererRegistry& registry,
                                        const TenementRendererConfig& config) {
  const std::shared_ptr<const TenementKit> kit = build_kit(config);
  register_building_renderer(
      registry,
      config.nation_slug,
      Game::Visuals::styled_building_type("home", "tenement"),
      [config, kit](const DrawContext& ctx,
                    const EntityPresentation& presentation,
                    ISubmitter& out) {
        if (ctx.entity == nullptr) {
          return;
        }
        const QVector3D team = Render::entity_color(*ctx.entity);
        const std::uint32_t seed =
            home_placement_seed(ctx.model(0, 3),
                                ctx.model(2, 3),
                                static_cast<std::uint32_t>(ctx.entity->get_id()));
        const auto variant = static_cast<std::size_t>(variant_of(ctx));
        const BuildingState state = presented_building_state(presentation, ctx);
        const auto palette = config.palette(team, seed, state);
        submit_building_instance(
            out, ctx, kit->sets[variant]->for_state(state), palette);
        submit_home_activity(
            ctx, out, config.carthage, presentation.home, kit->anchors[variant]);
        const std::array<TorchMount, 1> torches{kit->torches[variant]};
        submit_building_torches(ctx, out, torches);
        draw_building_selection_overlay(out, ctx, BuildingSelectionStyle{2.45F, 2.50F});
      },
      PresentationSystems{std::make_shared<const TenementActivity>(kit)});
}

} // namespace Render::GL
