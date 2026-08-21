#include <gtest/gtest.h>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/command/command_queue.h"
#include "game/command/command_validator.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/builder_product_types.h"
#include "game/systems/civilian_delivery_system.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"
#include "game/systems/structure_placement_service.h"
#include "game/systems/wall_plan_service.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Game::Command::Command;
using Game::Command::CommandQueue;
using Game::Command::Rejection;
using Game::Command::Source;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

struct Match {
  Match() {
    scope = std::make_unique<ScopedSession>(session);
    auto& owners = session.owners();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "player");
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "enemy");
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);
  }

  auto spawn(int owner_id, float x, float z) -> EntityID {
    auto* entity = session.world().create_entity();
    auto* transform = entity->add_component<Engine::Core::TransformComponent>();
    transform->position.x = x;
    transform->position.z = z;
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
    unit->owner_id = owner_id;
    return entity->get_id();
  }

  SessionContext session;
  std::unique_ptr<ScopedSession> scope;
};

TEST(CommandValidatorTest, RejectsOrdersAddressedToSomeoneElsesUnits) {
  Match match;
  const EntityID enemy = match.spawn(2, 5.0F, 5.0F);

  const Command command{
      .source = Source::LocalPlayer,
      .owner_id = 1,
      .payload = Game::Command::Move{.units = {enemy},
                                     .targets = {QVector3D(1.0F, 0.0F, 1.0F)}}};

  const auto validation = Game::Command::validate(match.session.world(), command);
  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::NoSubjects);
}

TEST(CommandValidatorTest, JudgesPlayerAndAiOrdersIdentically) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID theirs = match.spawn(2, 5.0F, 5.0F);

  const Game::Command::Payload payload = Game::Command::Move{
      .units = {mine, theirs},
      .targets = {QVector3D(1.0F, 0.0F, 1.0F), QVector3D(2.0F, 0.0F, 2.0F)}};

  const auto as_player = Game::Command::validate(
      match.session.world(),
      Command{.source = Source::LocalPlayer, .owner_id = 1, .payload = payload});
  const auto as_ai = Game::Command::validate(
      match.session.world(),
      Command{.source = Source::AI, .owner_id = 1, .payload = payload});

  ASSERT_TRUE(as_player.accepted());
  ASSERT_TRUE(as_ai.accepted());

  const auto& player_move = std::get<Game::Command::Move>(as_player.command.payload);
  const auto& ai_move = std::get<Game::Command::Move>(as_ai.command.payload);
  EXPECT_EQ(player_move.units, ai_move.units);
  EXPECT_EQ(player_move.units, std::vector<EntityID>{mine});
}

TEST(CommandValidatorTest, KeepsUnitsAndTargetsAlignedWhenFiltering) {
  Match match;
  const EntityID first = match.spawn(1, 0.0F, 0.0F);
  const EntityID stranger = match.spawn(2, 1.0F, 1.0F);
  const EntityID second = match.spawn(1, 2.0F, 2.0F);

  const Command command{
      .source = Source::LocalPlayer,
      .owner_id = 1,
      .payload = Game::Command::Move{.units = {first, stranger, second},
                                     .targets = {QVector3D(10.0F, 0.0F, 0.0F),
                                                 QVector3D(20.0F, 0.0F, 0.0F),
                                                 QVector3D(30.0F, 0.0F, 0.0F)}}};

  const auto validation = Game::Command::validate(match.session.world(), command);
  ASSERT_TRUE(validation.accepted());

  const auto& move = std::get<Game::Command::Move>(validation.command.payload);
  ASSERT_EQ(move.units.size(), 2U);
  ASSERT_EQ(move.targets.size(), 2U);
  EXPECT_EQ(move.units[0], first);
  EXPECT_FLOAT_EQ(move.targets[0].x(), 10.0F);
  EXPECT_EQ(move.units[1], second);

  EXPECT_FLOAT_EQ(move.targets[1].x(), 30.0F);
}

TEST(CommandValidatorTest, RefusesToAttackAnAlly) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID friendly = match.spawn(1, 1.0F, 1.0F);

  const auto validation =
      Game::Command::validate(match.session.world(),
                              Command{.source = Source::LocalPlayer,
                                      .owner_id = 1,
                                      .payload = Game::Command::AttackTarget{
                                          .units = {mine}, .target = friendly}});

  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::FriendlyTarget);
}

TEST(CommandValidatorTest, RefusesToAttackAHandleThatNoLongerResolves) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID enemy = match.spawn(2, 5.0F, 5.0F);
  match.session.world().destroy_entity(enemy);

  const auto validation =
      Game::Command::validate(match.session.world(),
                              Command{.source = Source::LocalPlayer,
                                      .owner_id = 1,
                                      .payload = Game::Command::AttackTarget{
                                          .units = {mine}, .target = enemy}});

  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::DeadTarget);
}

TEST(CommandValidatorTest, RejectsAnOrderWithNoIssuer) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  const auto validation =
      Game::Command::validate(match.session.world(),
                              Command{.source = Source::Script,
                                      .owner_id = 0,
                                      .payload = Game::Command::Stop{.units = {mine}}});

  EXPECT_FALSE(validation.accepted());
  EXPECT_EQ(validation.rejection, Rejection::NoOwner);
}

TEST(CommandQueueTest, AppliesNothingUntilDrained) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  auto& queue = match.session.commands();
  queue.submit(
      Source::LocalPlayer,
      1,
      Game::Command::Move{.units = {mine}, .targets = {QVector3D(9.0F, 0.0F, 9.0F)}});

  EXPECT_EQ(queue.pending(), 1U);
  auto* entity = match.session.world().get_entity(mine);
  ASSERT_NE(entity, nullptr);
  EXPECT_EQ(entity->get_component<Engine::Core::MovementComponent>(), nullptr);

  EXPECT_EQ(queue.drain(match.session.world(), 1), 1U);
  EXPECT_EQ(queue.pending(), 0U);
  EXPECT_NE(entity->get_component<Engine::Core::MovementComponent>(), nullptr);
}

TEST(CommandQueueTest, PreservesSubmissionOrderAcrossSources) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  std::vector<Source> executed;
  auto& queue = match.session.commands();
  queue.set_observer(
      [&executed](const Command& command) { executed.push_back(command.source); });

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  queue.submit(Source::AI, 1, Game::Command::Stop{.units = {mine}});
  queue.submit(Source::Replay, 1, Game::Command::Stop{.units = {mine}});
  queue.drain(match.session.world(), 7);

  ASSERT_EQ(executed.size(), 3U);
  EXPECT_EQ(executed[0], Source::LocalPlayer);
  EXPECT_EQ(executed[1], Source::AI);
  EXPECT_EQ(executed[2], Source::Replay);
}

TEST(CommandQueueTest, ObserverSeesEveryAcceptedCommandForReplay) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID theirs = match.spawn(2, 5.0F, 5.0F);

  std::vector<Command> recorded;
  auto& queue = match.session.commands();
  queue.set_observer(
      [&recorded](const Command& command) { recorded.push_back(command); });

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {theirs}});
  queue.drain(match.session.world(), 42);

  ASSERT_EQ(recorded.size(), 1U);
  EXPECT_EQ(recorded.front().submitted_tick, 42U);
  EXPECT_EQ(queue.accepted_count(), 1U);
  EXPECT_EQ(queue.rejected_count(), 1U);
}

TEST(CommandQueueTest, ReportsWhyACommandWasThrownOut) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID friendly = match.spawn(1, 1.0F, 1.0F);

  std::vector<Rejection> rejections;
  auto& queue = match.session.commands();
  queue.set_rejection_observer([&rejections](const Command&, Rejection reason) {
    rejections.push_back(reason);
  });

  queue.submit(
      Source::AI, 1, Game::Command::AttackTarget{.units = {mine}, .target = friendly});
  queue.drain(match.session.world(), 1);

  ASSERT_EQ(rejections.size(), 1U);
  EXPECT_EQ(rejections.front(), Rejection::FriendlyTarget);
}

TEST(CommandQueueTest, SubmittingDuringADrainDefersToTheNextTick) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  auto& queue = match.session.commands();
  bool resubmitted = false;
  queue.set_observer([&](const Command&) {
    if (!resubmitted) {
      resubmitted = true;
      queue.submit(Source::Script, 1, Game::Command::Stop{.units = {mine}});
    }
  });

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  EXPECT_EQ(queue.drain(match.session.world(), 1), 1U);

  EXPECT_EQ(queue.pending(), 1U);
  EXPECT_EQ(queue.drain(match.session.world(), 2), 1U);
}

TEST(CommandSubmitTest, RoutesThroughTheSessionQueueWhenThereIsOne) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);

  Game::Command::submit(match.session.world(),
                        Source::LocalPlayer,
                        1,
                        Game::Command::Stop{.units = {mine}});

  EXPECT_EQ(match.session.commands().pending(), 1U);
}

TEST(CommandSubmitTest, AppliesImmediatelyForAWorldWithNoSession) {

  Engine::Core::World world;
  auto* entity = world.create_entity();
  entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
  unit->owner_id = 1;

  Game::Command::submit(world,
                        Source::Script,
                        1,
                        Game::Command::Move{.units = {entity->get_id()},
                                            .targets = {QVector3D(4.0F, 0.0F, 4.0F)}});

  EXPECT_NE(entity->get_component<Engine::Core::MovementComponent>(), nullptr);
}

auto drain(Match& match) -> std::size_t {
  return match.session.commands().drain(match.session.world(), 1);
}

TEST(CommandPipelineTest, TradeBuysThroughTheOwnersMarketplace) {
  Match match;
  auto* market = match.session.world().create_entity();
  market->add_component<Engine::Core::TransformComponent>();
  market->add_component<Engine::Core::BuildingComponent>();
  auto* unit = market->add_component<Engine::Core::UnitComponent>(100, 100, 0.0F, 0.0F);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::Marketplace;

  auto& economy = match.session.economy();
  economy.add(1, Game::Systems::ResourceType::Gold, 100);
  const int wood_before = economy.get(1, Game::Systems::ResourceType::Wood);

  Game::Command::submit(
      match.session.world(),
      Source::LocalPlayer,
      1,
      Game::Command::Trade{.resource = Game::Systems::ResourceType::Wood,
                           .direction = Game::Command::TradeDirection::Buy});
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Wood), wood_before);

  EXPECT_EQ(drain(match), 1U);
  EXPECT_GT(economy.get(1, Game::Systems::ResourceType::Wood), wood_before);
  EXPECT_LT(economy.get(1, Game::Systems::ResourceType::Gold), 100);
}

TEST(CommandPipelineTest, TradeWithoutAMarketplaceChangesNothing) {
  Match match;
  auto& economy = match.session.economy();
  economy.add(1, Game::Systems::ResourceType::Gold, 100);

  Game::Command::submit(
      match.session.world(),
      Source::AI,
      1,
      Game::Command::Trade{.resource = Game::Systems::ResourceType::Wood,
                           .direction = Game::Command::TradeDirection::Buy});
  drain(match);

  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Gold), 100);
}

TEST(CommandPipelineTest, CommanderAbilityIsRejectedForAUnitWithoutACommand) {
  Match match;
  const EntityID soldier = match.spawn(1, 0.0F, 0.0F);

  const Command command{
      .source = Source::LocalPlayer,
      .owner_id = 1,
      .payload = Game::Command::UseCommanderAbility{
          .commander = soldier, .ability = Game::Command::CommanderAbility::Rally}};

  EXPECT_FALSE(Game::Command::validate(match.session.world(), command).accepted());
}

TEST(CommandPipelineTest, RallyOrderRaisesTheCommandersRallyRequest) {
  Match match;
  const EntityID commander = match.spawn(1, 0.0F, 0.0F);
  auto* commander_data = match.session.world()
                             .get_entity(commander)
                             ->add_component<Engine::Core::CommanderComponent>();

  Game::Command::submit(
      match.session.world(),
      Source::LocalPlayer,
      1,
      Game::Command::UseCommanderAbility{
          .commander = commander, .ability = Game::Command::CommanderAbility::Rally});
  EXPECT_FALSE(commander_data->rally_requested);

  drain(match);
  EXPECT_TRUE(commander_data->rally_requested);
}

TEST(CommandPipelineTest, FormationModeTogglesOnlyTheIssuersUnits) {
  Match match;
  const EntityID mine = match.spawn(1, 0.0F, 0.0F);
  const EntityID theirs = match.spawn(2, 3.0F, 3.0F);
  auto& world = match.session.world();
  world.get_entity(mine)->add_component<Engine::Core::FormationModeComponent>();
  world.get_entity(theirs)->add_component<Engine::Core::FormationModeComponent>();

  Game::Command::submit(
      world,
      Source::LocalPlayer,
      1,
      Game::Command::SetFormationMode{.units = {mine, theirs}, .active = true});
  drain(match);

  EXPECT_TRUE(world.get_entity(mine)
                  ->get_component<Engine::Core::FormationModeComponent>()
                  ->active);
  EXPECT_FALSE(world.get_entity(theirs)
                   ->get_component<Engine::Core::FormationModeComponent>()
                   ->active);

  Game::Command::submit(
      world,
      Source::LocalPlayer,
      1,
      Game::Command::SetFormationMode{.units = {mine}, .active = false});
  drain(match);
  EXPECT_FALSE(world.get_entity(mine)
                   ->get_component<Engine::Core::FormationModeComponent>()
                   ->active);
}

auto spawn_builder(Match& match, int owner_id)
    -> std::pair<EntityID, Engine::Core::BuilderProductionComponent*> {
  const EntityID id = match.spawn(owner_id, 0.0F, 0.0F);
  auto* entity = match.session.world().get_entity(id);
  entity->get_component<Engine::Core::UnitComponent>()->spawn_type =
      Game::Units::SpawnType::Builder;
  entity->add_component<Engine::Core::MovementComponent>();
  return {id, entity->add_component<Engine::Core::BuilderProductionComponent>()};
}

TEST(CommandPipelineTest, StartConstructionAssignsTheCrewAndChargesOnce) {
  Match match;
  auto [first, first_builder] = spawn_builder(match, 1);
  auto [second, second_builder] = spawn_builder(match, 1);
  auto& economy = match.session.economy();
  economy.add(1, Game::Systems::ResourceType::Wood, 100);
  economy.add(1, Game::Systems::ResourceType::Stone, 100);

  Game::Command::submit(
      match.session.world(),
      Source::LocalPlayer,
      1,
      Game::Command::StartConstruction{.units = {first, second},
                                       .construction_type = "defense_tower",
                                       .site = QVector3D(8.0F, 0.0F, 8.0F),
                                       .rotation_y = 0.5F});
  drain(match);

  EXPECT_TRUE(first_builder->has_construction_site);
  EXPECT_TRUE(second_builder->has_construction_site);
  EXPECT_EQ(first_builder->product_type, "defense_tower");
  EXPECT_FLOAT_EQ(first_builder->construction_site_x, 8.0F);
  EXPECT_FLOAT_EQ(first_builder->construction_site_rotation_y, 0.5F);
  EXPECT_GT(first_builder->build_time, 0.0F);
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Wood), 40);
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Stone), 20);
}

TEST(CommandPipelineTest, StartConstructionRefusesWhatTheIssuerCannotAfford) {
  Match match;
  auto [builder_id, builder] = spawn_builder(match, 1);
  match.session.economy().add(1, Game::Systems::ResourceType::Wood, 5);

  Game::Command::submit(
      match.session.world(),
      Source::AI,
      1,
      Game::Command::StartConstruction{.units = {builder_id},
                                       .construction_type = "defense_tower",
                                       .site = QVector3D(8.0F, 0.0F, 8.0F)});
  drain(match);

  EXPECT_FALSE(builder->has_construction_site);
  EXPECT_EQ(match.session.economy().get(1, Game::Systems::ResourceType::Wood), 5);
}

TEST(CommandPipelineTest, StartHarvestNeedsAResourceTheTerrainKnows) {
  Match match;
  auto [builder_id, builder] = spawn_builder(match, 1);

  Game::Command::submit(
      match.session.world(),
      Source::LocalPlayer,
      1,
      Game::Command::StartHarvest{.units = {builder_id},
                                  .construction_type = "cut_tree",
                                  .resource_target = 4242,
                                  .site = QVector3D(2.0F, 0.0F, 2.0F)});
  drain(match);

  EXPECT_FALSE(builder->has_construction_site);
  EXPECT_FALSE(builder->has_task_target);
  EXPECT_FALSE(match.session.terrain().is_world_prop_reserved(4242));
}

auto spawn_structure(Match& match,
                     int owner_id,
                     Game::Units::SpawnType type,
                     int health) -> EntityID {
  const EntityID id = match.spawn(owner_id, 20.0F, 20.0F);
  auto* entity = match.session.world().get_entity(id);
  entity->add_component<Engine::Core::BuildingComponent>();
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  unit->spawn_type = type;
  unit->health = health;
  return id;
}

TEST(CommandPipelineTest, RepairPutsTheCrewOnTheStructure) {
  Match match;
  auto [builder_id, builder] = spawn_builder(match, 1);
  const EntityID tower =
      spawn_structure(match, 1, Game::Units::SpawnType::DefenseTower, 40);

  Game::Command::submit(
      match.session.world(),
      Source::LocalPlayer,
      1,
      Game::Command::RepairStructure{.units = {builder_id}, .structure = tower});
  drain(match);

  EXPECT_TRUE(builder->has_construction_site);
  EXPECT_EQ(builder->structure_task_entity_id, tower);
  EXPECT_EQ(builder->product_type,
            std::string(Game::Systems::k_builder_product_repair));
}

TEST(CommandPipelineTest, RepairIsRefusedOnSomeoneElsesStructure) {
  Match match;
  auto [builder_id, builder] = spawn_builder(match, 1);
  const EntityID tower =
      spawn_structure(match, 2, Game::Units::SpawnType::DefenseTower, 40);

  const Command command{.source = Source::LocalPlayer,
                        .owner_id = 1,
                        .payload = Game::Command::RepairStructure{.units = {builder_id},
                                                                  .structure = tower}};
  EXPECT_EQ(Game::Command::validate(match.session.world(), command).rejection,
            Rejection::NotOwnedBuilding);
}

TEST(CommandPipelineTest, DeliverySendsOnlyAsManyCiviliansAsTheBarracksCanTake) {
  Match match;
  auto& world = match.session.world();
  const EntityID barracks =
      spawn_structure(match, 1, Game::Units::SpawnType::Barracks, 100);
  auto* production =
      world.get_entity(barracks)->add_component<Engine::Core::ProductionComponent>();
  production->max_units = 100;
  production->manpower_available =
      100 - Game::Systems::k_civilian_delivery_population_grant;

  std::vector<EntityID> civilians;
  for (int i = 0; i < 3; ++i) {
    const EntityID id = match.spawn(1, float(i), 0.0F);
    world.get_entity(id)->get_component<Engine::Core::UnitComponent>()->spawn_type =
        Game::Units::SpawnType::Civilian;
    civilians.push_back(id);
  }

  Game::Command::submit(
      world,
      Source::LocalPlayer,
      1,
      Game::Command::DeliverCivilians{.units = civilians, .barracks = barracks});
  drain(match);

  int carrying = 0;
  for (const EntityID id : civilians) {
    if (const auto* delivery =
            world.get_entity(id)
                ->get_component<Engine::Core::CivilianDeliveryComponent>();
        delivery != nullptr && delivery->target_barracks_id == barracks) {
      ++carrying;
    }
  }
  EXPECT_EQ(carrying, 1);
}

TEST(CommandPipelineTest, WallPlanRaisesSitesChargesWoodAndSeatsTheCrew) {
  Match match;
  auto [builder_id, builder] = spawn_builder(match, 1);
  auto& economy = match.session.economy();
  economy.add(1, Game::Systems::ResourceType::Wood, 500);

  const auto plan = Game::Systems::WallPlanService::plan(
      match.session.world(),
      Game::Systems::WallPlanRequest{
          .owner_id = 1, .anchor = {4, 4}, .target = {12, 4}});
  ASSERT_GT(plan.valid_count, 0);

  Game::Command::submit(match.session.world(),
                        Source::LocalPlayer,
                        1,
                        Game::Command::PlaceWallPlan{.units = {builder_id},
                                                     .anchor_x = 4,
                                                     .anchor_z = 4,
                                                     .target_x = 12,
                                                     .target_z = 4});
  EXPECT_TRUE(match.session.world()
                  .collect_entities_with<Engine::Core::WallConstructionSiteComponent>()
                  .empty());
  drain(match);

  EXPECT_EQ(
      static_cast<int>(
          match.session.world()
              .collect_entities_with<Engine::Core::WallConstructionSiteComponent>()
              .size()),
      plan.valid_count);
  EXPECT_TRUE(builder->has_construction_site);
  EXPECT_EQ(builder->product_type, "wall_segment");
  EXPECT_EQ(economy.get(1, Game::Systems::ResourceType::Wood), 500 - plan.wood_cost());
}

TEST(CommandPipelineTest, WallPlanFromSomeoneElsesBuilderIsIgnored) {
  Match match;
  auto [builder_id, builder] = spawn_builder(match, 2);
  match.session.economy().add(1, Game::Systems::ResourceType::Wood, 500);

  Game::Command::submit(match.session.world(),
                        Source::LocalPlayer,
                        1,
                        Game::Command::PlaceWallPlan{.units = {builder_id},
                                                     .anchor_x = 4,
                                                     .anchor_z = 4,
                                                     .target_x = 12,
                                                     .target_z = 4});
  drain(match);

  EXPECT_TRUE(match.session.world()
                  .collect_entities_with<Engine::Core::WallConstructionSiteComponent>()
                  .empty());
  EXPECT_FALSE(builder->has_construction_site);
  EXPECT_EQ(match.session.economy().get(1, Game::Systems::ResourceType::Wood), 500);
}

TEST(CommandPipelineTest, PlaceBuildingRefusesAUnitTypeThatIsNotAStructure) {
  Match match;
  match.session.economy().add(1, Game::Systems::ResourceType::Wood, 500);
  const auto before =
      match.session.world().collect_entities_with<Engine::Core::UnitComponent>().size();

  Game::Command::submit(
      match.session.world(),
      Source::LocalPlayer,
      1,
      Game::Command::PlaceBuilding{.building_type = "archer",
                                   .position = QVector3D(3.0F, 0.0F, 3.0F)});
  drain(match);

  EXPECT_EQ(
      match.session.world().collect_entities_with<Engine::Core::UnitComponent>().size(),
      before);
  EXPECT_EQ(Game::Systems::StructurePlacementService::ruling(
                match.session.world(), 1, "archer", QVector3D(3.0F, 0.0F, 3.0F)),
            Game::Systems::PlacementRuling::UnknownStructure);
}

} // namespace
