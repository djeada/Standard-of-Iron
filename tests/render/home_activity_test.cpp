#include <gtest/gtest.h>

#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "render/creature/archetype_registry.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/home_activity.h"
#include "render/entity/registry.h"
#include "render/graphics_settings.h"

namespace {
using namespace Render::GL;

class HomeRecorder final : public ISubmitter {
public:
  std::vector<QVector3D> plumes;
  std::vector<float> plume_intensities;
  std::vector<QMatrix4x4> actors;
  std::vector<QMatrix4x4> ground;

  void hearth_smoke(const QVector3D& position,
                    const QVector3D&,
                    float,
                    float intensity,
                    float) override {
    plumes.push_back(position);
    plume_intensities.push_back(intensity);
  }
  void rigged(const RiggedCreatureCmd& cmd) override { actors.push_back(cmd.world); }
  void mesh(
      Mesh*, const QMatrix4x4& model, const QVector3D&, Texture*, float, int) override {
    ground.push_back(model);
  }
  void cylinder(
      const QVector3D&, const QVector3D&, float, const QVector3D&, float) override {}
  void ground_marker(const GroundMarkerCmd&) override {}
  void grid(const QMatrix4x4&, const QVector3D&, float, float, float) override {}
  void selection_smoke(const QMatrix4x4&, const QVector3D&, float) override {
    ADD_FAILURE();
  }
  void healing_beam(const QVector3D&,
                    const QVector3D&,
                    const QVector3D&,
                    float,
                    float,
                    float,
                    float) override {}
  void healer_aura(const QVector3D&, const QVector3D&, float, float, float) override {}
  void combat_dust(const QVector3D&, const QVector3D&, float, float, float) override {
    ADD_FAILURE() << "a hearth is not a dust cloud";
  }
  void stone_impact(const QVector3D&, const QVector3D&, float, float, float) override {}
  void mode_indicator(const QMatrix4x4&, int, const QVector3D&, float) override {
    ADD_FAILURE();
  }
  void clear() {
    plumes.clear();
    plume_intensities.clear();
    actors.clear();
    ground.clear();
  }
};

class HomeActivityTest : public ::testing::Test {
protected:
  void SetUp() override {
    static const bool registered = [] {
      static EntityRendererRegistry shared_registry;
      Render::GL::register_built_in_entity_renderers(shared_registry);
      return true;
    }();
    (void)registered;
    old_quality = Render::GraphicsSettings::instance().quality();
    Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Ultra);
    entity = world.create_entity();
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Home;
    unit->owner_id = 1;
    unit->health = 400;
    unit->max_health = 400;
    ctx.entity = entity;
    ctx.world = &world;
    ctx.home_activity = &activity;
    ctx.submission_visibility = &visibility;
    ctx.submission_fog_mode = SubmissionFogMode::Revealed;
    visibility.reset(nullptr, nullptr);
    ctx.model.scale(1.36F, 1.36F, 1.36F);
  }
  void TearDown() override {
    Render::GraphicsSettings::instance().set_quality(old_quality);
  }
  void draw(float time = 1.0F, bool carthage = false) {
    recorder.clear();
    ctx.animation_time = time;
    activity.begin_frame(&world, time);
    submit_home_activity(ctx, recorder, carthage);
  }
  // Finds a time at which this house's hearth is alight.
  auto lit_time(std::uint64_t id) -> float {
    for (int step = 0; step < 4000; ++step) {
      const float time = static_cast<float>(step) * 0.5F;
      if (activity.hearth_intensity(id, time) > 0.25F) {
        return time;
      }
    }
    return -1.0F;
  }
  Engine::Core::World world;
  Engine::Core::Entity* entity{};
  HomeActivity activity;
  SubmissionVisibilityPolicy visibility;
  DrawContext ctx;
  HomeRecorder recorder;
  Render::GraphicsQuality old_quality{};
};

TEST_F(HomeActivityTest, BothNationsVentFromTheirOwnAnchorAndCarryTheirOwnBowl) {
  const auto& roman = home_smoke_anchor(false);
  const auto& punic = home_smoke_anchor(true);
  ASSERT_TRUE(roman.valid());
  ASSERT_TRUE(punic.valid());
  // No shared chimney silhouette: each nation vents from its own opening.
  EXPECT_NE(roman.vent, punic.vent);
  EXPECT_GT(roman.vent.y(), 1.0F);
  EXPECT_GT(punic.vent.y(), 1.0F);
  // The Punic oven sits off to one side of the roof; the domus ridge does not.
  EXPECT_NEAR(roman.vent.x(), 0.0F, 0.05F);
  EXPECT_GT(std::abs(punic.vent.x()), 0.2F);

  const auto& registry = Render::Creature::ArchetypeRegistry::instance();
  for (bool carthage : {false, true}) {
    const auto& rig = nation_civilian_rig(carthage);
    ASSERT_TRUE(rig.valid());
    const auto bowl = home_bowl_archetype(carthage);
    ASSERT_NE(bowl, Render::Creature::k_invalid_archetype);
    EXPECT_NE(bowl, rig.idle);
    EXPECT_EQ(registry.get(bowl)->bake_attachment_count,
              registry.get(rig.idle)->bake_attachment_count + 1);
  }
}

TEST_F(HomeActivityTest, SmokeRisesFromTheAnchorWithoutTouchingTheWorld) {
  const auto entity_count = world.entity_count();
  const float time = lit_time(entity->get_id());
  ASSERT_GE(time, 0.0F);
  draw(time);
  ASSERT_FALSE(recorder.plumes.empty());
  const auto& anchor = home_smoke_anchor(false);
  EXPECT_NEAR(recorder.plumes.front().y(), anchor.vent.y() * 1.36F, 0.01F);
  EXPECT_GT(recorder.plume_intensities.front(), 0.0F);
  EXPECT_LE(recorder.plume_intensities.front(), 1.0F);
  EXPECT_EQ(world.entity_count(), entity_count);
  EXPECT_EQ(entity->get_component<Engine::Core::UnitComponent>()->health, 400);
}

TEST_F(HomeActivityTest, DestroyedRuinedAndNeutralHousesStopSmoking) {
  const float time = lit_time(entity->get_id());
  ASSERT_GE(time, 0.0F);
  draw(time);
  ASSERT_FALSE(recorder.plumes.empty());

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  unit->health = 0;
  draw(time);
  EXPECT_TRUE(recorder.plumes.empty());

  unit->health = 40; // Ruined: the building renderer already shows the wreck.
  draw(time);
  EXPECT_TRUE(recorder.plumes.empty());

  unit->health = 400;
  unit->owner_id = -1;
  draw(time);
  EXPECT_TRUE(recorder.plumes.empty());

  unit->owner_id = 1;
  entity->add_component<Engine::Core::DismantleSiteComponent>();
  draw(time);
  EXPECT_TRUE(recorder.plumes.empty());
}

TEST_F(HomeActivityTest, NeighbouringHousesDoNotSmokeInLockstep) {
  // A street should always show a subset alight, never all and never none.
  constexpr int k_houses = 64;
  int ever_lit = 0;
  int simultaneous_max = 0;
  int simultaneous_min = k_houses;
  for (int sample = 0; sample < 40; ++sample) {
    const float time = 7.0F + static_cast<float>(sample) * 11.0F;
    int lit = 0;
    for (int house = 1; house <= k_houses; ++house) {
      if (activity.hearth_intensity(static_cast<std::uint64_t>(house), time) > 0.0F) {
        ++lit;
      }
    }
    simultaneous_max = std::max(simultaneous_max, lit);
    simultaneous_min = std::min(simultaneous_min, lit);
  }
  for (int house = 1; house <= k_houses; ++house) {
    if (lit_time(static_cast<std::uint64_t>(house)) >= 0.0F) {
      ++ever_lit;
    }
  }
  EXPECT_GT(ever_lit, k_houses / 2);
  EXPECT_GT(simultaneous_max, 0);
  EXPECT_LT(simultaneous_max, k_houses);
  EXPECT_LT(simultaneous_min, k_houses / 2);

  // Adjacent ids must not share a phase.
  const float probe = 53.0F;
  int identical = 0;
  for (int house = 1; house < k_houses; ++house) {
    const float a = activity.hearth_intensity(static_cast<std::uint64_t>(house), probe);
    const float b =
        activity.hearth_intensity(static_cast<std::uint64_t>(house + 1), probe);
    if (a == b && a > 0.0F) {
      ++identical;
    }
  }
  EXPECT_EQ(identical, 0);
}

TEST_F(HomeActivityTest, HearthRampsInAndOutRatherThanSnapping) {
  const std::uint64_t id = entity->get_id();
  const float lit = lit_time(id);
  ASSERT_GE(lit, 0.0F);
  // Walk back to the edge of the window and check the intensity climbs.
  float previous = activity.hearth_intensity(id, lit);
  bool saw_partial = false;
  for (float back = 0.5F; back < 9.0F; back += 0.5F) {
    const float value = activity.hearth_intensity(id, lit - back);
    if (value > 0.0F && value < previous) {
      saw_partial = true;
    }
    previous = std::min(previous, value <= 0.0F ? previous : value);
  }
  EXPECT_TRUE(saw_partial) << "a hearth should not switch on at full strength";
}

TEST_F(HomeActivityTest, FogAndQualityGateEverything) {
  const float time = lit_time(entity->get_id());
  ASSERT_GE(time, 0.0F);

  Game::Map::VisibilityService::Snapshot fog;
  fog.initialized = true;
  fog.width = fog.height = 32;
  fog.half_width = fog.half_height = 16;
  fog.cells.resize(32 * 32);
  visibility.reset(nullptr, &fog);
  for (std::uint8_t state : {0, 1, 2}) {
    std::fill(fog.cells.begin(), fog.cells.end(), state);
    draw(time);
    // Explored-but-unseen must not betray an occupied house.
    EXPECT_EQ(recorder.plumes.empty(), state != 2);
  }

  visibility.reset(nullptr, nullptr);
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Low);
  draw(time);
  EXPECT_TRUE(recorder.plumes.empty());
  EXPECT_EQ(activity.max_plumes, 0);

  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::Medium);
  draw(time);
  EXPECT_EQ(activity.max_plumes, 8);
  // Reduced effects also means no rigged gag actor.
  EXPECT_FALSE(activity.actors_allowed);
}

TEST_F(HomeActivityTest, DenseStreetsStayInsideThePlumeBudget) {
  Render::GraphicsSettings::instance().set_quality(Render::GraphicsQuality::High);
  activity.begin_frame(&world, 1.0F);
  const int budget = activity.max_plumes;
  recorder.clear();
  for (int i = 0; i < 200; ++i) {
    submit_home_activity(ctx, recorder, false);
  }
  EXPECT_LE(static_cast<int>(recorder.plumes.size()), budget);
}

TEST_F(HomeActivityTest, SpillIsRareSharesOneSlotAndCancelsCleanly) {
  activity.begin_frame(&world, 1.0F);
  int starts = 0;
  float last_start = -10000.0F;
  for (int tick = 0; tick < 24000; ++tick) {
    const float time = static_cast<float>(tick) * 0.25F;
    activity.begin_frame(&world, time);
    const float phase = activity.gag(entity->get_id(), time);
    if (phase == 0.0F) {
      EXPECT_GE(time - last_start, 300.0F);
      last_start = time;
      ++starts;
      // One slot for the whole renderer: a neighbour cannot also start.
      EXPECT_LT(activity.gag(entity->get_id() + 1, time), 0.0F);
    }
  }
  EXPECT_GT(starts, 0);
  EXPECT_LT(starts, 12);

  // Losing visibility for a frame releases the slot rather than stranding it.
  activity.gag_home = entity->get_id();
  activity.gag_started = 5000.0F;
  activity.gag_seen = true;
  activity.begin_frame(&world, 5001.0F);
  activity.begin_frame(&world, 5002.0F);
  EXPECT_EQ(activity.gag_home, 0U);
}

TEST_F(HomeActivityTest, SpillCarriesABowlOutThenLeavesTheMessBehind) {
  const std::uint64_t id = entity->get_id();
  activity.begin_frame(&world, 1.0F);
  activity.gag_home = id;
  activity.gag_started = 0.0F;
  activity.gag_seen = true;
  activity.world = &world;

  // Carrying it out: an actor, no spill on the ground yet.
  draw(1.0F);
  const auto carried = recorder.actors.size();
  EXPECT_GT(carried, 0U);
  EXPECT_TRUE(recorder.ground.empty());

  // Sprawled: the broth is on the ground.
  draw(3.0F);
  EXPECT_GT(recorder.actors.size(), 0U);
  EXPECT_FALSE(recorder.ground.empty());

  // Back on their feet, mess still there, house untouched.
  draw(5.5F);
  EXPECT_GT(recorder.actors.size(), 0U);
  EXPECT_FALSE(recorder.ground.empty());
  EXPECT_EQ(world.entity_count(), 1U);
  EXPECT_EQ(entity->get_component<Engine::Core::UnitComponent>()->health, 400);

  // After the sequence there is no actor and no mess.
  draw(Spill::k_sequence_end + 1.0F);
  EXPECT_TRUE(recorder.actors.empty());
  EXPECT_TRUE(recorder.ground.empty());
}

TEST_F(HomeActivityTest, HousesArePublishedOnTheUnitRenderPath) {
  entity->add_component<Engine::Core::TransformComponent>();
  entity->add_component<Engine::Core::BuildingComponent>();
  entity->add_component<Engine::Core::RenderableComponent>()->renderer_id =
      "troops/roman/home";
  world.ensure_render_snapshot();
  const auto snapshot = world.acquire_render_snapshot();
  ASSERT_NE(snapshot, nullptr);
  const auto units = snapshot->render_unit_ids();
  EXPECT_NE(std::find(units.begin(), units.end(), entity->get_id()), units.end());
  const auto buildings = snapshot->render_building_ids();
  EXPECT_EQ(std::find(buildings.begin(), buildings.end(), entity->get_id()),
            buildings.end());
}
} // namespace
