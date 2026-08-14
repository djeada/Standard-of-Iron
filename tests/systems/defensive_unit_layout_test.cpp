#include <QJsonObject>
#include <QVector3D>

#include <gtest/gtest.h>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "formation/unit_layout.h"
#include "formation/unit_layout_state_system.h"
#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "save/serialization.h"
#include "systems/defensive_unit_layout_service.h"
#include "systems/nation_loader.h"
#include "systems/nation_registry.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

namespace {

class DefensiveUnitLayoutTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();

    auto const nations = NationLoader::load_default_nations();
    ASSERT_FALSE(nations.empty());
    auto& registry = NationRegistry::instance();
    registry.clear();
    registry.clear_player_assignments();
    for (const auto& nation : nations) {
      registry.register_nation(nation);
    }
  }

  auto spawn(NationID nation,
             Game::Units::SpawnType type = Game::Units::SpawnType::Knight,
             float x = 0.0F,
             float z = 0.0F) -> Entity* {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(400, 400, 2.0F, 12.0F);
    unit->owner_id = 1;
    unit->spawn_type = type;
    unit->nation_id = nation;
    entity->add_component<MovementComponent>();
    entity->add_component<AttackComponent>(8.0F, 20, 1.0F);
    return entity;
  }

  void initialize_layouts() { layouts.update(world.get(), 1.0F / 60.0F); }

  void issue_guard(std::initializer_list<Entity*> entities, bool active = true) {
    std::vector<EntityID> ids;
    ids.reserve(entities.size());
    for (auto* entity : entities) {
      ids.push_back(entity->get_id());
    }
    Game::Command::Command command{};
    command.source = Game::Command::Source::LocalPlayer;
    command.owner_id = 1;
    command.payload =
        Game::Command::SetGuard{.units = std::move(ids), .active = active};
    Game::Command::dispatch(*world, command);
  }

  void advance(float seconds, float step = 1.0F / 60.0F) {
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
      layouts.update(world.get(), step);
    }
  }

  static auto layout(const Entity& entity) -> const UnitLayoutStateComponent* {
    return entity.get_component<UnitLayoutStateComponent>();
  }

  std::unique_ptr<World> world;
  Game::Formation::UnitLayoutStateSystem layouts;
};

TEST_F(DefensiveUnitLayoutTest, NationDataDescribesOneUnitsInternalLayout) {
  auto const& registry = NationRegistry::instance();
  auto const* roman = registry.get_nation(NationID::RomanRepublic);
  auto const* carthage = registry.get_nation(NationID::Carthage);
  auto const* sepulcher = registry.get_nation(NationID::IronSepulcher);

  ASSERT_NE(roman, nullptr);
  ASSERT_NE(carthage, nullptr);
  ASSERT_NE(sepulcher, nullptr);
  ASSERT_TRUE(roman->defensive_unit_layout.has_value());
  ASSERT_TRUE(carthage->defensive_unit_layout.has_value());
  EXPECT_FALSE(sepulcher->defensive_unit_layout.has_value());

  EXPECT_EQ(roman->defensive_unit_layout->id, "testudo");
  EXPECT_EQ(carthage->defensive_unit_layout->id, "shield_wall");
  EXPECT_GT(roman->defensive_unit_layout->form_seconds,
            carthage->defensive_unit_layout->form_seconds);
  EXPECT_LT(roman->defensive_unit_layout->frontal_missile_multiplier,
            carthage->defensive_unit_layout->frontal_missile_multiplier);
  EXPECT_GT(carthage->defensive_unit_layout->move_speed_multiplier,
            roman->defensive_unit_layout->move_speed_multiplier);
}

TEST_F(DefensiveUnitLayoutTest, RomeUsesAShellAndCarthageUsesAnOpenShieldWall) {
  auto& library = Game::Formation::UnitLayoutLibrary::instance();
  auto const roman = library.resolve("rome", "shield_wall");
  auto const carthage = library.resolve("carthage", "shield_wall");

  ASSERT_NE(roman, Game::Formation::k_invalid_layout);
  ASSERT_NE(carthage, Game::Formation::k_invalid_layout);
  EXPECT_EQ(library.style(roman).shape, Game::Formation::UnitLayoutShape::Shell);
  EXPECT_EQ(library.style(carthage).shape, Game::Formation::UnitLayoutShape::Arc);
  EXPECT_LT(library.style(roman).lateral_spacing_scale,
            library.style(carthage).lateral_spacing_scale);
  EXPECT_LT(library.style(roman).depth_spacing_scale,
            library.style(carthage).depth_spacing_scale);
}

TEST_F(DefensiveUnitLayoutTest, ASelectedSingleUnitCanFormTestudoByItself) {
  auto* roman = spawn(NationID::RomanRepublic);
  initialize_layouts();
  issue_guard({roman});

  advance(0.05F);
  ASSERT_NE(layout(*roman), nullptr);
  EXPECT_EQ(layout(*roman)->state,
            static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Defensive));
  EXPECT_FALSE(DefensiveUnitLayoutService::is_formed(*roman));

  advance(3.1F);
  EXPECT_TRUE(DefensiveUnitLayoutService::is_formed(*roman));
}

TEST_F(DefensiveUnitLayoutTest, SelectedUnitsDoNotReceiveInterUnitSlotsOrMoveOrders) {
  auto* left = spawn(NationID::RomanRepublic, Game::Units::SpawnType::Knight, -9.0F);
  auto* right = spawn(NationID::RomanRepublic, Game::Units::SpawnType::Knight, 11.0F);
  initialize_layouts();

  issue_guard({left, right});
  advance(3.2F);

  auto const* left_transform = left->get_component<TransformComponent>();
  auto const* right_transform = right->get_component<TransformComponent>();
  auto const* left_movement = left->get_component<MovementComponent>();
  auto const* right_movement = right->get_component<MovementComponent>();
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);
  EXPECT_FLOAT_EQ(left_transform->position.x, -9.0F);
  EXPECT_FLOAT_EQ(right_transform->position.x, 11.0F);
  EXPECT_FALSE(left_movement->get_has_target());
  EXPECT_FALSE(right_movement->get_has_target());
  EXPECT_TRUE(DefensiveUnitLayoutService::is_formed(*left));
  EXPECT_TRUE(DefensiveUnitLayoutService::is_formed(*right));
}

TEST_F(DefensiveUnitLayoutTest, EachFactionUsesItsOwnFormationTime) {
  auto* roman = spawn(NationID::RomanRepublic, Game::Units::SpawnType::Knight, -3.0F);
  auto* carthage = spawn(NationID::Carthage, Game::Units::SpawnType::Knight, 3.0F);
  initialize_layouts();
  issue_guard({roman, carthage});

  advance(2.1F);
  EXPECT_FALSE(DefensiveUnitLayoutService::is_formed(*roman));
  EXPECT_TRUE(DefensiveUnitLayoutService::is_formed(*carthage));

  advance(1.0F);
  EXPECT_TRUE(DefensiveUnitLayoutService::is_formed(*roman));
}

TEST_F(DefensiveUnitLayoutTest, IneligibleTroopsKeepTheirOrdinaryLayout) {
  auto* archer = spawn(NationID::RomanRepublic, Game::Units::SpawnType::Archer);
  initialize_layouts();
  auto const normal_layout = layout(*archer)->layout_id;

  issue_guard({archer});
  advance(4.0F);

  EXPECT_FALSE(DefensiveUnitLayoutService::is_active(*archer));
  EXPECT_EQ(layout(*archer)->layout_id, normal_layout);
  EXPECT_NE(layout(*archer)->state,
            static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Defensive));
}

TEST_F(DefensiveUnitLayoutTest, BonusesWaitUntilTheInternalRanksAreLocked) {
  auto* roman = spawn(NationID::RomanRepublic);
  initialize_layouts();
  issue_guard({roman});
  advance(1.0F);

  EXPECT_TRUE(DefensiveUnitLayoutService::is_active(*roman));
  EXPECT_FALSE(DefensiveUnitLayoutService::is_formed(*roman));
  EXPECT_FLOAT_EQ(DefensiveUnitLayoutService::move_speed_multiplier(*roman), 1.0F);
  EXPECT_FLOAT_EQ(DefensiveUnitLayoutService::attack_output_multiplier(*roman), 1.0F);

  advance(2.1F);
  EXPECT_LT(DefensiveUnitLayoutService::move_speed_multiplier(*roman), 0.5F);
  EXPECT_LT(DefensiveUnitLayoutService::attack_output_multiplier(*roman), 0.5F);
  EXPECT_TRUE(DefensiveUnitLayoutService::blocks_charge(*roman));
  EXPECT_TRUE(DefensiveUnitLayoutService::holds_position(*roman));
}

TEST_F(DefensiveUnitLayoutTest, TestudoProtectsFrontAndPunishesRearExposure) {
  auto* roman = spawn(NationID::RomanRepublic);
  initialize_layouts();
  issue_guard({roman});
  advance(3.2F);

  auto* transform = roman->get_component<TransformComponent>();
  ASSERT_NE(transform, nullptr);
  transform->rotation.y = 0.0F;
  QVector3D const front(0.0F, 0.0F, 10.0F);
  QVector3D const flank(10.0F, 0.0F, 0.0F);
  QVector3D const rear(0.0F, 0.0F, -10.0F);

  float const front_missile = DefensiveUnitLayoutService::damage_multiplier(
      *roman, {.is_missile = true, .attack_origin = front});
  float const front_melee = DefensiveUnitLayoutService::damage_multiplier(
      *roman, {.is_missile = false, .attack_origin = front});
  float const flank_melee = DefensiveUnitLayoutService::damage_multiplier(
      *roman, {.is_missile = false, .attack_origin = flank});
  float const rear_melee = DefensiveUnitLayoutService::damage_multiplier(
      *roman, {.is_missile = false, .attack_origin = rear});

  EXPECT_LT(front_missile, front_melee);
  EXPECT_LT(front_melee, 1.0F);
  EXPECT_GT(flank_melee, 1.0F);
  EXPECT_GT(rear_melee, flank_melee);
}

TEST_F(DefensiveUnitLayoutTest, CarthaginianWallIsFasterButLessMissileProof) {
  auto* roman = spawn(NationID::RomanRepublic, Game::Units::SpawnType::Knight, -2.0F);
  auto* carthage = spawn(NationID::Carthage, Game::Units::SpawnType::Knight, 2.0F);
  initialize_layouts();
  issue_guard({roman, carthage});
  advance(3.2F);

  QVector3D const roman_front(-2.0F, 0.0F, 10.0F);
  QVector3D const carthage_front(2.0F, 0.0F, 10.0F);
  float const roman_missile = DefensiveUnitLayoutService::damage_multiplier(
      *roman, {.is_missile = true, .attack_origin = roman_front});
  float const carthage_missile = DefensiveUnitLayoutService::damage_multiplier(
      *carthage, {.is_missile = true, .attack_origin = carthage_front});

  EXPECT_LT(roman_missile, carthage_missile);
  EXPECT_GT(DefensiveUnitLayoutService::move_speed_multiplier(*carthage),
            DefensiveUnitLayoutService::move_speed_multiplier(*roman));
}

TEST_F(DefensiveUnitLayoutTest, GuardOffOpensTheSameUnitWithoutMovingIt) {
  auto* roman = spawn(NationID::RomanRepublic, Game::Units::SpawnType::Knight, 7.0F);
  initialize_layouts();
  issue_guard({roman});
  advance(3.2F);
  ASSERT_TRUE(DefensiveUnitLayoutService::is_formed(*roman));

  issue_guard({roman}, false);
  advance(0.05F);
  EXPECT_FALSE(DefensiveUnitLayoutService::is_active(*roman));
  EXPECT_EQ(layout(*roman)->phase,
            static_cast<std::uint8_t>(Game::Formation::LayoutPhase::Breaking));

  advance(1.5F);
  EXPECT_TRUE(layout(*roman)->is_formed());
  EXPECT_EQ(layout(*roman)->state,
            static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Normal));
  EXPECT_FLOAT_EQ(roman->get_component<TransformComponent>()->position.x, 7.0F);
}

TEST_F(DefensiveUnitLayoutTest, RoutingImmediatelyDropsDefensiveBenefits) {
  auto* roman = spawn(NationID::RomanRepublic);
  initialize_layouts();
  issue_guard({roman});
  advance(3.2F);
  ASSERT_TRUE(DefensiveUnitLayoutService::is_formed(*roman));

  auto* morale = roman->add_component<MoraleComponent>();
  morale->routing = true;
  advance(0.05F);

  EXPECT_FALSE(DefensiveUnitLayoutService::is_active(*roman));
  EXPECT_EQ(layout(*roman)->state,
            static_cast<std::uint8_t>(Game::Formation::UnitLayoutState::Routing));
}

TEST_F(DefensiveUnitLayoutTest, UnitLayoutTransitionSurvivesSaveAndLoad) {
  auto* roman = spawn(NationID::RomanRepublic);
  initialize_layouts();
  issue_guard({roman});
  advance(1.0F);
  ASSERT_NE(layout(*roman), nullptr);

  QJsonObject const json = Serialization::serialize_entity(roman);
  EXPECT_FALSE(json.contains("defense_formation"));
  ASSERT_TRUE(json.contains("unit_layout_state"));

  World restored_world;
  auto* restored = restored_world.create_entity();
  Serialization::deserialize_entity(restored, json);
  auto const* restored_layout = layout(*restored);
  ASSERT_NE(restored_layout, nullptr);
  EXPECT_EQ(restored_layout->state, layout(*roman)->state);
  EXPECT_EQ(restored_layout->phase, layout(*roman)->phase);
  EXPECT_EQ(restored_layout->layout_id, layout(*roman)->layout_id);
  EXPECT_FLOAT_EQ(restored_layout->transition_progress,
                  layout(*roman)->transition_progress);
}

} // namespace
