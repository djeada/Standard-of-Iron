#include <algorithm>
#include <cmath>
#include <cstdio>
#include <gtest/gtest.h>
#include <numbers>

#include "core/component.h"
#include "core/world.h"
#include "systems/combat_system/formation_contact_processor.h"
#include "systems/default_content.h"
#include "systems/formation_combat_geometry.h"
#include "systems/nation_registry.h"
#include "systems/nav_grid.h"
#include "systems/troop_profile_service.h"

namespace {

constexpr float k_dt = 1.0F / 60.0F;
constexpr float k_speed = 2.5F;

constexpr float k_snap_step = 0.4F;

auto add_spearmen(Engine::Core::World& world) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit =
      entity->add_component<Engine::Core::UnitComponent>(120, 120, k_speed, 15.0F);
  auto* attack = entity->add_component<Engine::Core::AttackComponent>();
  entity->add_component<Engine::Core::MovementComponent>();
  transform->position = {0.0F, 0.0F, 0.0F};
  transform->rotation.y = 0.0F;
  transform->scale = {0.55F, 0.55F, 0.55F};
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Spearman;
  unit->render_individuals_per_unit_override = 12;
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->preferred_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.5F;
  return entity;
}

struct MarchStats {
  float max_lag{0.0F};
  float final_lag{0.0F};
  float max_jump{0.0F};
  int lagging_soldiers{0};
};

auto march(Engine::Core::World& world,
           Engine::Core::Entity* entity,
           float seconds,
           float yaw_rate_degrees,
           MarchStats& stats) -> void {
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto const* presentation =
      entity->get_component<Engine::Core::FormationPresentationComponent>();
  std::vector<std::pair<float, float>> previous;
  if (presentation != nullptr) {
    for (auto const& soldier : presentation->soldiers) {
      previous.emplace_back(soldier.world_x, soldier.world_z);
    }
  }
  int const ticks = static_cast<int>(std::round(seconds / k_dt));
  for (int tick = 0; tick < ticks; ++tick) {
    transform->rotation.y += yaw_rate_degrees * k_dt;
    float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
    float const vx = std::sin(yaw) * k_speed;
    float const vz = std::cos(yaw) * k_speed;
    movement->set_manual_velocity(vx, vz);
    transform->position.x += vx * k_dt;
    transform->position.z += vz * k_dt;
    Game::Systems::Combat::update_formation_contacts(&world, k_dt);

    presentation =
        entity->get_component<Engine::Core::FormationPresentationComponent>();
    ASSERT_NE(presentation, nullptr);
    auto const layout = Game::Systems::FormationCombat::resolve_layout(*entity);
    float const sin_yaw = std::sin(yaw);
    float const cos_yaw = std::cos(yaw);
    float tick_max_lag = 0.0F;
    for (auto const& soldier : presentation->soldiers) {
      if (!soldier.alive || !soldier.world_motion_valid) {
        continue;
      }
      auto const& slot = layout.all_slots[soldier.slot_index];
      float const slot_world_x =
          transform->position.x + cos_yaw * slot.local_x + sin_yaw * slot.local_z;
      float const slot_world_z =
          transform->position.z - sin_yaw * slot.local_x + cos_yaw * slot.local_z;
      float const lag =
          std::hypot(soldier.world_x - slot_world_x, soldier.world_z - slot_world_z);
      tick_max_lag = std::max(tick_max_lag, lag);
      if (soldier.slot_index < previous.size()) {
        float const jump =
            std::hypot(soldier.world_x - previous[soldier.slot_index].first,
                       soldier.world_z - previous[soldier.slot_index].second);
        stats.max_jump = std::max(stats.max_jump, jump);
      } else {
        previous.resize(soldier.slot_index + 1);
      }
      previous[soldier.slot_index] = {soldier.world_x, soldier.world_z};
    }
    stats.max_lag = std::max(stats.max_lag, tick_max_lag);
    stats.final_lag = tick_max_lag;
  }
  stats.lagging_soldiers = 0;
  auto const layout = Game::Systems::FormationCombat::resolve_layout(*entity);
  float const yaw = transform->rotation.y * std::numbers::pi_v<float> / 180.0F;
  for (auto const& soldier : presentation->soldiers) {
    auto const& slot = layout.all_slots[soldier.slot_index];
    float const slot_world_x = transform->position.x + std::cos(yaw) * slot.local_x +
                               std::sin(yaw) * slot.local_z;
    float const slot_world_z = transform->position.z - std::sin(yaw) * slot.local_x +
                               std::cos(yaw) * slot.local_z;
    float const lag =
        std::hypot(soldier.world_x - slot_world_x, soldier.world_z - slot_world_z);
    if (lag > 0.35F) {
      ++stats.lagging_soldiers;
    }
  }
}

class FormationMarchKeepUp : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::TroopProfileService::instance().prime();
    Game::Systems::NavGrid::initialize(96, 96);
  }
  void TearDown() override { Game::Systems::NationRegistry::instance().clear(); }
};

} // namespace

TEST_F(FormationMarchKeepUp, EveryoneKeepsStationOnAStraightMarch) {
  Engine::Core::World world;
  auto* entity = add_spearmen(world);
  MarchStats stats;
  Game::Systems::Combat::update_formation_contacts(&world, k_dt);
  march(world, entity, 8.0F, 0.0F, stats);
  std::printf("straight: max_lag=%.3f final=%.3f max_jump=%.3f lagging=%d\n",
              stats.max_lag,
              stats.final_lag,
              stats.max_jump,
              stats.lagging_soldiers);
  EXPECT_LT(stats.final_lag, 0.35F);
  EXPECT_EQ(stats.lagging_soldiers, 0);
  EXPECT_LT(stats.max_jump, k_snap_step);
}

TEST_F(FormationMarchKeepUp, EveryoneCatchesUpAfterAWheelAndNobodyTeleports) {
  Engine::Core::World world;
  auto* entity = add_spearmen(world);
  Game::Systems::Combat::update_formation_contacts(&world, k_dt);
  MarchStats settle;
  march(world, entity, 3.0F, 0.0F, settle);
  MarchStats wheel;
  march(world, entity, 3.0F, 30.0F, wheel);
  std::printf("wheel: max_lag=%.3f final=%.3f max_jump=%.3f\n",
              wheel.max_lag,
              wheel.final_lag,
              wheel.max_jump);
  MarchStats after;
  march(world, entity, 6.0F, 0.0F, after);
  std::printf("after: max_lag=%.3f final=%.3f max_jump=%.3f lagging=%d\n",
              after.max_lag,
              after.final_lag,
              after.max_jump,
              after.lagging_soldiers);
  EXPECT_LT(after.final_lag, 0.35F);
  EXPECT_EQ(after.lagging_soldiers, 0);
  EXPECT_LT(wheel.max_jump, k_snap_step);
  EXPECT_LT(after.max_jump, k_snap_step);
}
