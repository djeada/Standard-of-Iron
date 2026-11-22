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

// What the presentation systems decided for one entity this frame, before its
// submission draws: plain values, no GL and no submitter. Each system fills its
// own part; the kind's submission reads them.
struct EntityPresentation {
  BuildingState building_state{BuildingState::Normal};
  bool building_state_resolved{false};
  HomeActivityPlan home;
  FarmActivityPlan farm;
  BarracksActivityPlan barracks;
};

// One step that turns the snapshot (through the DrawContext) into presentation
// values for an entity. Systems run in registration order, so a later one may
// read what an earlier one decided.
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

// Damage state (normal, damaged, destroyed) with its hysteresis; decay colours,
// torches, smoke and incense all follow it.
[[nodiscard]] auto
building_state_presentation() -> std::shared_ptr<const EntityPresentationSystem>;
// Hearth, doorstep and window life of a home, or its fallen residents.
[[nodiscard]] auto home_activity_presentation(bool carthage)
    -> std::shared_ptr<const EntityPresentationSystem>;
// Field workers and harvest props of a farm.
[[nodiscard]] auto farm_activity_presentation(bool carthage)
    -> std::shared_ptr<const EntityPresentationSystem>;
// Forge fire, drill and off-duty soldiers of a barracks yard.
[[nodiscard]] auto
barracks_activity_presentation() -> std::shared_ptr<const EntityPresentationSystem>;

// The building state a submission draws with: the presented one when a
// presentation system resolved it, otherwise resolved now.
[[nodiscard]] auto presented_building_state(const EntityPresentation& presentation,
                                            const DrawContext& ctx) -> BuildingState;

} // namespace Render::GL
