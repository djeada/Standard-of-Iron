#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "game/core/component.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "render/entity/building_archetype_catalog.h"
#include "render/entity/building_render_common.h"
#include "render/entity/entity_presentation.h"
#include "render/entity/home_activity.h"
#include "render/entity/home_activity_submit.h"
#include "render/entity/registry.h"
#include "render/frame/draw_context.h"
#include "render/frame/submitter.h"

namespace {
using namespace Render::GL;

class NullSubmitter final : public ISubmitter {
public:
  void mesh(Mesh*, const QMatrix4x4&, const QVector3D&, Texture*, float, int) override {
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {}
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {}
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {}
};

auto built_in_registry() -> const EntityRendererRegistry& {
  static EntityRendererRegistry registry;
  static const bool registered = [] {
    register_built_in_entity_renderers(registry);
    return true;
  }();
  (void)registered;
  return registry;
}

auto system_names(const PresentationSystems& systems) -> std::vector<std::string> {
  std::vector<std::string> names;
  for (const auto& system : systems) {
    names.emplace_back(system->name());
  }
  return names;
}

auto is_building_kind(const EntityRendererRegistry& registry,
                      std::string_view kind) -> bool {
  const auto& systems = registry.presentation(kind);
  return !systems.empty() && systems.front()->name() == "building_state";
}

TEST(EntityRegistry, EachBuildingKindPresentsItsStateThenItsActivity) {
  const auto& registry = built_in_registry();
  using Names = std::vector<std::string>;
  for (const std::string nation : {"roman", "carthage"}) {
    const auto kind = [&](std::string_view type) {
      return building_renderer_key(nation, type);
    };
    EXPECT_EQ(system_names(registry.presentation(kind("home"))),
              (Names{"building_state", "home_activity"}));
    EXPECT_EQ(system_names(registry.presentation(kind("farm"))),
              (Names{"building_state", "farm_activity"}));
    EXPECT_EQ(system_names(registry.presentation(kind("barracks"))),
              (Names{"building_state", "barracks_activity"}));
    EXPECT_EQ(system_names(registry.presentation(kind("temple"))),
              (Names{"building_state"}));
    EXPECT_EQ(system_names(registry.presentation(kind("marketplace"))),
              (Names{"building_state"}));
    EXPECT_EQ(system_names(registry.presentation(kind("defense_tower"))),
              (Names{"building_state"}));
    EXPECT_EQ(system_names(registry.presentation(kind("wall_gate"))),
              (Names{"building_state"}));
  }
  EXPECT_TRUE(registry.presentation("wildlife/sheep").empty())
      << "creatures prepare through their preparer, not building presentation";
}

TEST(EntityRegistry, EveryBuildingKindDrawsCatalogDescriptionsAndEveryEntryHasAKind) {
  const auto& registry = built_in_registry();
  const auto kinds = registry.kinds();
  const std::set<std::string> registered(kinds.begin(), kinds.end());

  const std::set<std::string> primitive_only{
      building_renderer_key("iron_sepulcher", "barracks")};

  std::size_t building_kinds = 0;
  for (const auto& kind : kinds) {
    if (!is_building_kind(registry, kind) || primitive_only.contains(kind)) {
      continue;
    }
    ++building_kinds;
    EXPECT_FALSE(registry.descriptions(kind).empty())
        << kind << " draws nothing from the catalog";
  }
  EXPECT_GT(building_kinds, 20U);

  for (const auto& entry : building_archetype_catalog()) {
    EXPECT_FALSE(entry.kinds.empty()) << entry.name << " is drawn by no kind";
    for (const auto& kind : entry.kinds) {
      EXPECT_TRUE(registered.contains(kind))
          << entry.name << " names " << kind << ", which is not registered";
    }
  }

  const auto home = registry.descriptions(building_renderer_key("roman", "home"));
  EXPECT_NE(std::find(home.begin(), home.end(), "roman_home"), home.end());
  const auto barracks =
      registry.descriptions(building_renderer_key("carthage", "barracks"));
  EXPECT_NE(std::find(barracks.begin(), barracks.end(), "carthage_barracks_grounds"),
            barracks.end());
}

class RecordingSystem final : public EntityPresentationSystem {
public:
  explicit RecordingSystem(std::vector<std::string>* log)
      : m_log(log) {}
  [[nodiscard]] auto name() const noexcept -> std::string_view override {
    return "recording";
  }
  void present(const DrawContext&, EntityPresentation& out) const override {
    m_log->emplace_back("present");
    out.building_state = BuildingState::Damaged;
    out.building_state_resolved = true;
  }

private:
  std::vector<std::string>* m_log;
};

TEST(EntityRegistry, DispatchPresentsBeforeItSubmits) {
  std::vector<std::string> log;
  BuildingState seen = BuildingState::Normal;
  EntityRendererRegistry registry;
  registry.register_presented_renderer(
      "test/kind",
      PresentationSystems{std::make_shared<const RecordingSystem>(&log)},
      [&](const DrawContext&, const EntityPresentation& presentation, ISubmitter&) {
        log.emplace_back("submit");
        seen = presentation.building_state;
      });

  const auto draw = registry.get("test/kind");
  ASSERT_TRUE(static_cast<bool>(draw));
  NullSubmitter out;
  draw(DrawContext{}, out);
  EXPECT_EQ(log, (std::vector<std::string>{"present", "submit"}));
  EXPECT_EQ(seen, BuildingState::Damaged) << "the submission draws what was presented";
  EXPECT_EQ(registry.presentation("test/kind").size(), 1U);
}

class EntityPresentationTest : public ::testing::Test {
protected:
  void SetUp() override {
    (void)built_in_registry();
    entity = world.create_entity();
    unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->owner_id = 1;
    unit->health = 400;
    unit->max_health = 400;
    ctx.entity = entity;
    ctx.world = &world;
  }

  Engine::Core::World world;
  Engine::Core::Entity* entity{};
  Engine::Core::UnitComponent* unit{};
  DrawContext ctx;
};

TEST_F(EntityPresentationTest, BuildingStateFollowsHealth) {
  EntityPresentation presented;
  building_state_presentation()->present(ctx, presented);
  EXPECT_TRUE(presented.building_state_resolved);
  EXPECT_EQ(presented.building_state, BuildingState::Normal);
  EXPECT_EQ(presented_building_state(presented, ctx), resolve_building_state(ctx));

  ctx.template_prewarm = true;
  unit->health = 0;
  EntityPresentation destroyed;
  building_state_presentation()->present(ctx, destroyed);
  EXPECT_EQ(destroyed.building_state, BuildingState::Destroyed);
}

TEST_F(EntityPresentationTest, BarracksYardIsAliveOnlyWhenIntactAndNear) {
  EntityPresentation presented;
  barracks_activity_presentation()->present(ctx, presented);
  EXPECT_TRUE(presented.barracks.active);
  EXPECT_FALSE(presented.barracks.training);

  entity->add_component<Engine::Core::ProductionComponent>()->in_progress = true;
  EntityPresentation training;
  barracks_activity_presentation()->present(ctx, training);
  EXPECT_TRUE(training.barracks.training);

  ctx.distance_sq = 1.0e9F;
  EntityPresentation far;
  barracks_activity_presentation()->present(ctx, far);
  EXPECT_FALSE(far.barracks.active);

  ctx.distance_sq = 0.0F;
  ctx.template_prewarm = true;
  EntityPresentation prewarm;
  barracks_activity_presentation()->present(ctx, prewarm);
  EXPECT_FALSE(prewarm.barracks.active) << "template prewarm draws no yard life";
}

TEST_F(EntityPresentationTest, HomeActivityPlansNothingWithoutActivityOrWhenFar) {
  unit->spawn_type = Game::Units::SpawnType::Home;
  EntityPresentation without;
  home_activity_presentation(false)->present(ctx, without);
  EXPECT_EQ(without.home.mode, HomeActivityPlan::Mode::None);

  HomeActivity activity;
  ctx.home_activity = &activity;
  activity.begin_frame(&world, 1.0F);
  ctx.animation_time = 1.0F;
  EntityPresentation near;
  home_activity_presentation(false)->present(ctx, near);
  if (activity.inhabited(entity->get_id()) && activity.max_plumes > 0) {
    EXPECT_EQ(near.home.mode, HomeActivityPlan::Mode::Live);
  }

  ctx.distance_sq = (k_home_activity_far + 1.0F) * (k_home_activity_far + 1.0F);
  EntityPresentation far;
  home_activity_presentation(false)->present(ctx, far);
  EXPECT_EQ(far.home.mode, HomeActivityPlan::Mode::None);
}

TEST_F(EntityPresentationTest, FarmActivityPlansNoFieldWithoutActivity) {
  EntityPresentation presented;
  farm_activity_presentation(false)->present(ctx, presented);
  EXPECT_EQ(presented.farm.field, nullptr);
}

} // namespace
