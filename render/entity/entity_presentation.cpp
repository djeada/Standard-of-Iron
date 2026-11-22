#include "render/entity/entity_presentation.h"

#include <memory>
#include <string_view>

#include "render/entity/building_render_common.h"
#include "render/frame/draw_context.h"

namespace Render::GL {

namespace {

class BuildingStatePresentation final : public EntityPresentationSystem {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "building_state";
  }
  void present(const DrawContext& ctx, EntityPresentation& out) const override {
    out.building_state = resolve_building_state(ctx);
    out.building_state_resolved = true;
  }
};

class HomeActivityPresentation final : public EntityPresentationSystem {
public:
  explicit HomeActivityPresentation(bool carthage)
      : m_carthage(carthage) {}
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "home_activity";
  }
  void present(const DrawContext& ctx, EntityPresentation& out) const override {
    out.home = resolve_home_activity_plan(ctx, m_carthage);
  }

private:
  bool m_carthage;
};

class FarmActivityPresentation final : public EntityPresentationSystem {
public:
  explicit FarmActivityPresentation(bool carthage)
      : m_carthage(carthage) {}
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "farm_activity";
  }
  void present(const DrawContext& ctx, EntityPresentation& out) const override {
    out.farm = resolve_farm_activity_plan(ctx, m_carthage);
  }

private:
  bool m_carthage;
};

class BarracksActivityPresentation final : public EntityPresentationSystem {
public:
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "barracks_activity";
  }
  void present(const DrawContext& ctx, EntityPresentation& out) const override {
    out.barracks = resolve_barracks_activity_plan(ctx);
  }
};

} // namespace

auto building_state_presentation() -> std::shared_ptr<const EntityPresentationSystem> {
  static const auto k_system = std::make_shared<const BuildingStatePresentation>();
  return k_system;
}

auto home_activity_presentation(bool carthage)
    -> std::shared_ptr<const EntityPresentationSystem> {
  static const auto k_roman = std::make_shared<const HomeActivityPresentation>(false);
  static const auto k_carthage = std::make_shared<const HomeActivityPresentation>(true);
  return carthage ? k_carthage : k_roman;
}

auto farm_activity_presentation(bool carthage)
    -> std::shared_ptr<const EntityPresentationSystem> {
  static const auto k_roman = std::make_shared<const FarmActivityPresentation>(false);
  static const auto k_carthage = std::make_shared<const FarmActivityPresentation>(true);
  return carthage ? k_carthage : k_roman;
}

auto barracks_activity_presentation()
    -> std::shared_ptr<const EntityPresentationSystem> {
  static const auto k_system = std::make_shared<const BarracksActivityPresentation>();
  return k_system;
}

auto presented_building_state(const EntityPresentation& presentation,
                              const DrawContext& ctx) -> BuildingState {
  return presentation.building_state_resolved ? presentation.building_state
                                              : resolve_building_state(ctx);
}

} // namespace Render::GL
