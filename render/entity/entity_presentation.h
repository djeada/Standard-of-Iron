#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "render/entity/barracks_activity_submit.h"
#include "render/entity/building_state.h"
#include "render/entity/farm_activity_submit.h"
#include "render/entity/home_activity_submit.h"

namespace Render::GL {

struct DrawContext;

struct EntityPresentation {
  BuildingState building_state{BuildingState::Normal};
  bool building_state_resolved{false};
  HomeActivityPlan home;
  FarmActivityPlan farm;
  BarracksActivityPlan barracks;
};

class EntityPresentationSystem {
public:
  EntityPresentationSystem() = default;
  virtual ~EntityPresentationSystem() = default;
  EntityPresentationSystem(const EntityPresentationSystem&) = delete;
  auto operator=(const EntityPresentationSystem&) -> EntityPresentationSystem& = delete;
  EntityPresentationSystem(EntityPresentationSystem&&) = delete;
  auto operator=(EntityPresentationSystem&&) -> EntityPresentationSystem& = delete;

  [[nodiscard]] virtual auto name() const noexcept -> std::string_view = 0;
  virtual void present(const DrawContext& ctx, EntityPresentation& out) const = 0;
};

using PresentationSystems =
    std::vector<std::shared_ptr<const EntityPresentationSystem>>;

[[nodiscard]] auto
building_state_presentation() -> std::shared_ptr<const EntityPresentationSystem>;

[[nodiscard]] auto home_activity_presentation(bool carthage)
    -> std::shared_ptr<const EntityPresentationSystem>;

[[nodiscard]] auto farm_activity_presentation(bool carthage)
    -> std::shared_ptr<const EntityPresentationSystem>;

[[nodiscard]] auto
barracks_activity_presentation() -> std::shared_ptr<const EntityPresentationSystem>;

[[nodiscard]] auto presented_building_state(const EntityPresentation& presentation,
                                            const DrawContext& ctx) -> BuildingState;

} // namespace Render::GL
