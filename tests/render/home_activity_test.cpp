#include <gtest/gtest.h>
#include <set>

#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "render/creature/archetype_registry.h"
#include "render/entity/civilian_actor.h"
#include "render/entity/home_activity.h"
#include "render/entity/registry.h"
#include "render/graphics_settings.h"
#include "render/local_lighting.h"

namespace {
using namespace Render::GL;

class HomeRecorder final : public ISubmitter {
public:
  struct Plume {
    QVector3D position;
    QVector3D color;
    float radius;
    float intensity;
    float time;
  };
  std::vector<QVector3D> plumes;
  std::vector<float> plume_intensities;
  std::vector<Plume> plume_details;
  std::vector<QMatrix4x4> actors;
  std::vector<QMatrix4x4> props;
  std::vector<Render::LocalLight> lights;

  void hearth_smoke(const QVector3D& position,
                    const QVector3D& color,
                    float radius,
                    float intensity,
                    float time) override {
    plumes.push_back(position);
    plume_intensities.push_back(intensity);
    plume_details.push_back({position, color, radius, intensity, time});
  }
  void rigged(const RiggedCreatureCmd& cmd) override { actors.push_back(cmd.world); }
  void mesh(
      Mesh*, const QMatrix4x4& model, const QVector3D&, Texture*, float, int) override {
    props.push_back(model);
  }
  void local_light(const Render::LocalLight& light) override {
    lights.push_back(light);
  }
  // Props lying on the ground: the spilled broth, never a hung cloth.
  [[nodiscard]] auto ground() const -> std::vector<QMatrix4x4> {
    std::vector<QMatrix4x4> low;
    for (const auto& model : props) {
      if (model.column(3).y() < 0.30F) {
        low.push_back(model);
      }
    }
    return low;
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
    plume_details.clear();
    actors.clear();
    props.clear();
    lights.clear();
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
  activity.set_night(1.0F);
  // A moment when this household's lamp is lit, if it ever is.
  float lamp_time = -1.0F;
  for (float probe = 0.0F; probe < 900.0F && lamp_time < 0.0F; probe += 1.0F) {
    if (activity.lamp_state(entity->get_id(), probe).strength > 0.0F) {
      lamp_time = probe;
    }
  }
  for (std::uint8_t state : {0, 1, 2}) {
    std::fill(fog.cells.begin(), fog.cells.end(), state);
    draw(time);
    // Explored-but-unseen must not betray an occupied house: no smoke, and no
    // lamplight, cloth or shutters either.
    EXPECT_EQ(recorder.plumes.empty(), state != 2);
    if (state != 2) {
      EXPECT_TRUE(recorder.lights.empty());
      EXPECT_TRUE(recorder.props.empty());
    }
    if (lamp_time >= 0.0F) {
      draw(lamp_time);
      EXPECT_EQ(recorder.lights.empty(), state != 2);
    }
  }
  activity.set_night(0.0F);

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
  EXPECT_TRUE(recorder.ground().empty());

  // Sprawled: the broth is on the ground.
  draw(3.0F);
  EXPECT_GT(recorder.actors.size(), 0U);
  EXPECT_FALSE(recorder.ground().empty());

  // Back on their feet, mess still there, house untouched.
  draw(5.5F);
  EXPECT_GT(recorder.actors.size(), 0U);
  EXPECT_FALSE(recorder.ground().empty());
  EXPECT_EQ(world.entity_count(), 1U);
  EXPECT_EQ(entity->get_component<Engine::Core::UnitComponent>()->health, 400);

  // After the sequence there is no actor and no mess.
  draw(Spill::k_sequence_end + 1.0F);
  EXPECT_TRUE(recorder.actors.empty());
  EXPECT_TRUE(recorder.ground().empty());
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

TEST_F(HomeActivityTest, PlumesDifferInCharacterAcrossAStreet) {
  constexpr int k_houses = 64;
  std::set<int> sizes;
  std::set<int> warmths;
  std::set<int> clocks;
  int tended = 0;
  for (int house = 1; house <= k_houses; ++house) {
    const auto character = hearth_character(static_cast<std::uint64_t>(house));
    EXPECT_GT(character.size, 0.5F);
    EXPECT_LT(character.size, 1.5F);
    EXPECT_GT(character.density, 0.4F);
    EXPECT_LE(character.density, 1.0F);
    sizes.insert(static_cast<int>(character.size * 20.0F));
    warmths.insert(static_cast<int>(character.warmth * 10.0F));
    clocks.insert(static_cast<int>(character.time_offset / 30.0F));
    if (character.puff_depth > 0.0F) {
      ++tended;
    }
  }
  // A street of different fires, not one plume stamped N times.
  EXPECT_GT(sizes.size(), 5U);
  EXPECT_GT(warmths.size(), 5U);
  EXPECT_GT(clocks.size(), 8U);
  EXPECT_GT(tended, k_houses / 6);
  EXPECT_LT(tended, k_houses * 5 / 6);

  // Two houses lit at the same moment submit visibly different plumes.
  const std::uint64_t id = entity->get_id();
  const float time = lit_time(id);
  ASSERT_GE(time, 0.0F);
  draw(time);
  ASSERT_EQ(recorder.plume_details.size(), 1U);
  const auto mine = recorder.plume_details.front();
  auto* other = world.create_entity();
  auto* unit = other->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Home;
  unit->owner_id = 1;
  unit->health = 400;
  unit->max_health = 400;
  const float other_time = lit_time(other->get_id());
  ASSERT_GE(other_time, 0.0F);
  ctx.entity = other;
  draw(other_time);
  ASSERT_EQ(recorder.plume_details.size(), 1U);
  const auto theirs = recorder.plume_details.front();
  EXPECT_NE(mine.radius, theirs.radius);
  EXPECT_NE(mine.color, theirs.color);
  // The noise clock is the house's own, never raw animation time.
  EXPECT_NE(mine.time, time);
  EXPECT_NE(theirs.time, other_time);
  EXPECT_NE(mine.time - time, theirs.time - other_time);
}

TEST_F(HomeActivityTest, MealTimesLightMoreHearthsWithoutSwitchingTheStreet) {
  constexpr int k_houses = 64;
  auto lit_count = [&](float time) {
    int lit = 0;
    for (int house = 1; house <= k_houses; ++house) {
      if (activity.hearth_intensity(static_cast<std::uint64_t>(house), time) > 0.0F) {
        ++lit;
      }
    }
    return lit;
  };
  auto average_lit = [&]() {
    int total = 0;
    for (int sample = 0; sample < 40; ++sample) {
      total += lit_count(7.0F + static_cast<float>(sample) * 11.0F);
    }
    return static_cast<float>(total) / 40.0F;
  };
  activity.set_night(0.0F);
  const float midday = average_lit();
  activity.set_night(0.5F);
  const float dusk = average_lit();
  activity.set_night(1.0F);
  const float deep_night = average_lit();
  EXPECT_GT(dusk, midday * 1.2F);
  EXPECT_LT(deep_night, midday);
  EXPECT_LT(dusk, static_cast<float>(k_houses) * 0.8F);

  // Dusk arriving must light houses one at a time, never as a group.
  const float probe = 131.0F;
  int worst_step = 0;
  activity.set_night(0.0F);
  int previous = lit_count(probe);
  for (int step = 1; step <= 100; ++step) {
    activity.set_night(static_cast<float>(step) / 100.0F);
    const int now = lit_count(probe);
    worst_step = std::max(worst_step, std::abs(now - previous));
    previous = now;
  }
  EXPECT_LE(worst_step, 3);
  activity.set_night(0.0F);
}

TEST_F(HomeActivityTest, LampsComeOnHouseByHouseOnTheirOwnClocks) {
  constexpr int k_houses = 64;
  auto lit = [&](float time) {
    int count = 0;
    for (int house = 1; house <= k_houses; ++house) {
      const auto lamp = activity.lamp_state(static_cast<std::uint64_t>(house), time);
      EXPECT_GE(lamp.strength, 0.0F);
      EXPECT_LE(lamp.strength, 1.0F);
      if (lamp.strength > 0.0F) {
        ++count;
      }
    }
    return count;
  };
  activity.set_night(0.0F);
  EXPECT_EQ(lit(50.0F), 0);
  activity.set_night(1.0F);
  const int at_night = lit(50.0F);
  // Some houses are dark, most are lit.
  EXPECT_GT(at_night, k_houses / 2);
  EXPECT_LT(at_night, k_houses);
  activity.set_night(0.3F);
  EXPECT_LT(lit(50.0F), at_night);

  // Dusk lights windows one by one.
  int worst_step = 0;
  activity.set_night(0.0F);
  int previous = 0;
  for (int step = 1; step <= 100; ++step) {
    activity.set_night(static_cast<float>(step) / 100.0F);
    const int now = lit(50.0F);
    worst_step = std::max(worst_step, now - previous);
    previous = now;
  }
  EXPECT_LE(worst_step, 4);

  // Each lamp keeps its own schedule and its own flicker: two neighbours are
  // never in lockstep, and a lit house sometimes goes dark for a while.
  activity.set_night(1.0F);
  std::vector<std::uint64_t> lit_now;
  for (int house = 1; house <= k_houses && lit_now.size() < 2; ++house) {
    const auto id = static_cast<std::uint64_t>(house);
    if (activity.lamp_state(id, 0.0F).strength > 0.0F &&
        activity.lamp_state(id, 20.0F).strength > 0.0F) {
      lit_now.push_back(id);
    }
  }
  ASSERT_EQ(lit_now.size(), 2U);
  int agree = 0;
  int samples = 0;
  bool went_dark = false;
  for (int sample = 0; sample < 400; ++sample) {
    const float time = static_cast<float>(sample) * 0.05F;
    const auto a = activity.lamp_state(lit_now[0], time);
    const auto b = activity.lamp_state(lit_now[1], time);
    const auto a_next = activity.lamp_state(lit_now[0], time + 0.05F);
    const auto b_next = activity.lamp_state(lit_now[1], time + 0.05F);
    if (a.strength > 0.0F && b.strength > 0.0F) {
      ++samples;
      if ((a_next.strength > a.strength) == (b_next.strength > b.strength)) {
        ++agree;
      }
    }
  }
  for (int sample = 0; sample < 200 && !went_dark; ++sample) {
    went_dark =
        activity.lamp_state(lit_now[0], static_cast<float>(sample) * 5.0F).strength ==
        0.0F;
  }
  EXPECT_GT(samples, 100);
  EXPECT_LT(agree, samples * 3 / 4);
  EXPECT_TRUE(went_dark);
  std::set<int> colours;
  for (int house = 1; house <= k_houses; ++house) {
    const auto lamp = activity.lamp_state(static_cast<std::uint64_t>(house), 50.0F);
    if (lamp.strength > 0.0F) {
      colours.insert(static_cast<int>(lamp.color.y() * 100.0F));
      // Candle-warm, never neon.
      EXPECT_GT(lamp.color.x(), lamp.color.y());
      EXPECT_GT(lamp.color.y(), lamp.color.z());
    }
  }
  EXPECT_GT(colours.size(), 3U);
  activity.set_night(0.0F);
}

TEST_F(HomeActivityTest, DoorstepVisitsVaryInLengthActivityAndCompany) {
  constexpr int k_houses = 64;
  std::set<int> activities;
  std::set<int> durations;
  int pairs = 0;
  int most_out = 0;
  for (int sample = 0; sample < 120; ++sample) {
    const float time = 3.0F + static_cast<float>(sample) * 7.3F;
    int out = 0;
    for (int house = 1; house <= k_houses; ++house) {
      const auto visit =
          activity.doorstep_plan(static_cast<std::uint64_t>(house), time);
      if (!visit.active()) {
        continue;
      }
      ++out;
      EXPECT_LT(visit.t, visit.duration);
      EXPECT_GE(visit.duration, Doorstep::k_visit_min);
      activities.insert(static_cast<int>(visit.activity));
      durations.insert(static_cast<int>(visit.duration / 4.0F));
      if (visit.residents == 2) {
        EXPECT_EQ(visit.activity, DoorstepActivity::Talk);
        ++pairs;
      }
    }
    most_out = std::max(most_out, out);
  }
  EXPECT_GE(activities.size(), 4U);
  EXPECT_GE(durations.size(), 3U);
  EXPECT_GT(pairs, 0);
  // Never a street stepping out together.
  EXPECT_LT(most_out, k_houses / 2);

  // Deterministic: the same house at the same moment is the same visit.
  const auto once = activity.doorstep_plan(9U, 400.0F);
  const auto again = activity.doorstep_plan(9U, 400.0F);
  EXPECT_EQ(once.t, again.t);
  EXPECT_EQ(once.activity, again.activity);
}

TEST_F(HomeActivityTest, DoorCurtainPartsOnlyAsSomeonePassesThrough) {
  // Find a visit and check the curtain moves at its ends, not its middle.
  bool found = false;
  for (int house = 1; house <= 64 && !found; ++house) {
    for (float time = 0.0F; time < 600.0F && !found; time += 0.5F) {
      const auto id = static_cast<std::uint64_t>(house);
      const auto visit = activity.doorstep_plan(id, time);
      if (!visit.active() || visit.t > 0.1F || visit.residents != 1) {
        continue;
      }
      found = true;
      const float start = time - visit.t;
      EXPECT_GT(activity.curtain_parting(id, start + 0.15F), 0.5F);
      EXPECT_EQ(activity.curtain_parting(id, start + (visit.duration * 0.5F)), 0.0F);
      EXPECT_GT(activity.curtain_parting(id, start + visit.duration - 0.15F), 0.5F);
      EXPECT_EQ(activity.curtain_parting(id, start - 5.0F), 0.0F);
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(HomeActivityTest, ShuttersStandAtTheirOwnAnglesAndCloseForTheNight) {
  constexpr int k_houses = 64;
  int closed = 0;
  int open = 0;
  std::set<int> angles;
  activity.set_night(0.0F);
  for (int house = 1; house <= k_houses; ++house) {
    for (int window = 0; window < 4; ++window) {
      const float angle =
          activity.shutter_angle(static_cast<std::uint64_t>(house), window);
      EXPECT_GE(angle, 0.0F);
      EXPECT_LE(angle, 180.0F);
      angles.insert(static_cast<int>(angle / 10.0F));
      closed += angle < 10.0F ? 1 : 0;
      open += angle > 130.0F ? 1 : 0;
    }
  }
  EXPECT_GT(angles.size(), 6U);
  EXPECT_GT(closed, 0);
  EXPECT_GT(open, k_houses);
  // The same house never has four identical windows.
  int identical_houses = 0;
  for (int house = 1; house <= k_houses; ++house) {
    const auto id = static_cast<std::uint64_t>(house);
    if (activity.shutter_angle(id, 0) == activity.shutter_angle(id, 1) &&
        activity.shutter_angle(id, 1) == activity.shutter_angle(id, 2)) {
      ++identical_houses;
    }
  }
  EXPECT_LT(identical_houses, k_houses / 8);

  // Dusk closes them one window at a time; full night closes them all.
  auto open_count = [&]() {
    int count = 0;
    for (int house = 1; house <= k_houses; ++house) {
      for (int window = 0; window < 4; ++window) {
        count +=
            activity.shutter_angle(static_cast<std::uint64_t>(house), window) > 10.0F
                ? 1
                : 0;
      }
    }
    return count;
  };
  int previous = open_count();
  int worst_step = 0;
  for (int step = 1; step <= 100; ++step) {
    activity.set_night(static_cast<float>(step) / 100.0F);
    const int now = open_count();
    worst_step = std::max(worst_step, previous - now);
    previous = now;
  }
  EXPECT_LE(worst_step, 12);
  EXPECT_EQ(open_count(), 0);
  activity.set_night(0.0F);
}

TEST_F(HomeActivityTest, ResidentsStandOnTheStreetSideAndPairsFaceEachOther) {
  const std::uint64_t id = entity->get_id();
  const auto& anchor = home_smoke_anchor(false);
  bool saw_pair = false;
  bool saw_single = false;
  for (float time = 0.0F; time < 3000.0F && !(saw_pair && saw_single); time += 1.0F) {
    const auto visit = activity.doorstep_plan(id, time);
    if (!visit.active() || visit.t < visit.step_seconds() + 3.0F ||
        visit.t > visit.duration - visit.step_seconds() - 3.0F) {
      continue;
    }
    draw(time);
    // One actor may submit more than one rigged command; count bodies.
    std::vector<QMatrix4x4> bodies;
    for (const auto& actor : recorder.actors) {
      if (bodies.empty() || bodies.back().column(3) != actor.column(3)) {
        bodies.push_back(actor);
      }
    }
    ASSERT_EQ(bodies.size(), static_cast<std::size_t>(visit.residents));
    for (const auto& actor : bodies) {
      const auto at = actor.column(3).toVector3D();
      // Outside the door, never inside the house.
      EXPECT_GT(at.z(), anchor.doorstep.z() * 1.36F + 0.3F);
      EXPECT_LT(std::abs(at.x()), 5.0F);
    }
    if (visit.residents == 2) {
      saw_pair = true;
      const auto a = bodies[0].column(3).toVector3D();
      const auto b = bodies[1].column(3).toVector3D();
      const float apart = (a - b).length();
      EXPECT_GT(apart, 0.8F);
      EXPECT_LT(apart, 1.6F);
      // Each faces the other: their forward axes point at one another.
      const auto forward_a = bodies[0].mapVector(QVector3D(0, 0, 1));
      const auto forward_b = bodies[1].mapVector(QVector3D(0, 0, 1));
      EXPECT_GT(QVector3D::dotProduct(forward_a, (b - a).normalized()), 0.9F);
      EXPECT_GT(QVector3D::dotProduct(forward_b, (a - b).normalized()), 0.9F);
    } else {
      saw_single = true;
    }
  }
  EXPECT_TRUE(saw_pair);
  EXPECT_TRUE(saw_single);
}

TEST_F(HomeActivityTest, DetailPropsSitOnTheHouseAndVaryBetweenHouses) {
  // Everything hung on the house stays within its footprint and above its
  // plinth: nothing floats off into the street or sinks into the ground.
  draw(12.0F);
  EXPECT_FALSE(recorder.props.empty());
  for (const auto& prop : recorder.props) {
    const auto at = prop.column(3).toVector3D();
    EXPECT_LT(std::abs(at.x()), 1.36F * 1.25F);
    EXPECT_LT(std::abs(at.z()), 1.36F * 1.25F);
    EXPECT_GT(at.y(), 0.15F);
    EXPECT_LT(at.y(), 1.36F * 2.1F);
  }
  // The cloth is never still and never the same on two houses.
  const auto first = recorder.props;
  draw(12.5F);
  ASSERT_EQ(recorder.props.size(), first.size());
  int moved = 0;
  for (std::size_t i = 0; i < first.size(); ++i) {
    moved += first[i] != recorder.props[i] ? 1 : 0;
  }
  EXPECT_GT(moved, 0);

  auto* other = world.create_entity();
  auto* unit = other->add_component<Engine::Core::UnitComponent>();
  unit->spawn_type = Game::Units::SpawnType::Home;
  unit->owner_id = 1;
  unit->health = 400;
  unit->max_health = 400;
  ctx.entity = other;
  draw(12.0F);
  int differ = 0;
  for (std::size_t i = 0; i < std::min(first.size(), recorder.props.size()); ++i) {
    differ += first[i] != recorder.props[i] ? 1 : 0;
  }
  EXPECT_GT(differ, 0);
}
} // namespace
