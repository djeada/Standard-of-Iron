#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <gtest/gtest.h>
#include <variant>

#include "game/command/command.h"
#include "game/command/command_codec.h"
#include "game/command/command_queue.h"
#include "game/command/replay.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/owner_registry.h"

namespace {

using Engine::Core::EntityID;
using Game::Command::Command;
using Game::Command::Payload;
using Game::Command::Source;

auto every_payload() -> std::vector<Payload> {
  using namespace Game::Command;
  std::vector<Payload> all;
  all.emplace_back(
      Move{.units = {1, 2},
           .targets = {QVector3D(1.5F, 0.0F, -2.0F), QVector3D(3.0F, 1.0F, 4.0F)},
           .facing_angles = {0.25F, 1.5F},
           .kind = Game::Systems::MoveOrderKind::ScriptedMove,
           .preserve_formation_mode = true});
  all.emplace_back(AttackTarget{.units = {3}, .target = 9, .should_chase = false});
  all.emplace_back(Stop{.units = {4, 5, 6}});
  all.emplace_back(SetHold{.units = {7}, .active = false});
  all.emplace_back(SetGuard{.units = {8},
                            .active = true,
                            .anchor = QVector3D(2.0F, 0.0F, 2.0F),
                            .has_anchor = true});
  all.emplace_back(SetRunMode{.units = {9}, .active = false});
  all.emplace_back(Patrol{.units = {10},
                          .first_waypoint = QVector3D(1.0F, 0.0F, 1.0F),
                          .second_waypoint = QVector3D(5.0F, 0.0F, 5.0F)});
  all.emplace_back(
      SetRallyPoint{.building = 11, .position = QVector3D(6.0F, 0.0F, 7.0F)});
  all.emplace_back(SetGateMode{
      .units = {12}, .mode = Engine::Core::GateComponent::ManualMode::ForcedOpen});
  all.emplace_back(SetAutoGather{
      .units = {13}, .active = false, .priority_product_type = "cut_tree"});
  all.emplace_back(
      Produce{.building = 14, .product = Game::Units::TroopType::Spearman});
  all.emplace_back(Trade{.resource = Game::Systems::ResourceType::Stone,
                         .direction = TradeDirection::Sell});
  all.emplace_back(UseCommanderAbility{.commander = 15,
                                       .ability = CommanderAbility::FlagRally,
                                       .target = QVector3D(8.0F, 0.0F, 9.0F)});
  all.emplace_back(SetFormationMode{.units = {16}, .active = false});
  DeployFormation deploy;
  deploy.units = {17, 18};
  deploy.anchor = QVector3D(10.0F, 0.0F, 11.0F);
  deploy.facing = 1.2F;
  deploy.frontage = 12.0F;
  deploy.spacing = 1.5F;
  deploy.intent = Game::Formation::ArmyFormationIntent::Encirclement;
  deploy.doctrine = "roman_manipular";
  deploy.options.flank_preference = Game::Formation::FlankPreference::StrongLeft;
  deploy.options.movement_policy = Game::Formation::MovementPolicy::MaintainFormation;
  deploy.options.ranged_placement = Game::Formation::RangedPlacement::Front;
  deploy.options.mixed_policy =
      Game::Formation::MixedDoctrinePolicy::SeparateContingents;
  deploy.options.frontage_scale = 1.25F;
  deploy.options.depth_scale = 0.75F;
  deploy.options.spacing_scale = 1.1F;
  deploy.options.reserve_rows = 2;
  deploy.options.preserve_member_order = true;
  deploy.options.doctrine_locked = true;
  all.emplace_back(deploy);
  all.emplace_back(ReleaseFormation{.units = {19}});
  all.emplace_back(StartConstruction{.units = {20},
                                     .construction_type = "temple",
                                     .site = QVector3D(12.0F, 0.0F, 13.0F),
                                     .rotation_y = 0.5F});
  all.emplace_back(StartHarvest{.units = {21},
                                .construction_type = "collect_stone",
                                .resource_target = 4242,
                                .site = QVector3D(14.0F, 0.0F, 15.0F)});
  all.emplace_back(DeliverCivilians{.units = {22, 23}, .barracks = 24});
  all.emplace_back(RepairStructure{.units = {25}, .structure = 26});
  all.emplace_back(PlaceWallPlan{.units = {27},
                                 .gate = true,
                                 .anchor_x = 4,
                                 .anchor_z = 6,
                                 .target_x = 12,
                                 .target_z = 6,
                                 .rotation_y = 90.0F});
  all.emplace_back(PlaceBuilding{.building_type = "defense_tower",
                                 .position = QVector3D(16.0F, 0.0F, 17.0F),
                                 .rotation_y = 45.0F});
  return all;
}

auto same_wire(const Command& a, const Command& b) -> bool {
  return QJsonDocument(Game::Command::to_json(a)).toJson(QJsonDocument::Compact) ==
         QJsonDocument(Game::Command::to_json(b)).toJson(QJsonDocument::Compact);
}

TEST(CommandCodecTest, CoversEveryAlternativeOfThePayloadVariant) {
  EXPECT_EQ(every_payload().size(), std::variant_size_v<Payload>)
      << "a payload was added to Game::Command::Payload without a sample here";
}

TEST(CommandCodecTest, EveryPayloadSurvivesTheRoundTrip) {
  for (const auto& payload : every_payload()) {
    const Command original{
        .source = Source::AI, .owner_id = 3, .submitted_tick = 777, .payload = payload};
    const auto object = Game::Command::to_json(original);
    const auto decoded = Game::Command::from_json(object);
    ASSERT_TRUE(decoded.has_value()) << Game::Command::payload_name(payload);
    EXPECT_EQ(decoded->payload.index(), payload.index());
    EXPECT_EQ(decoded->source, Source::AI);
    EXPECT_EQ(decoded->owner_id, 3);
    EXPECT_EQ(decoded->submitted_tick, 777U);
    EXPECT_TRUE(same_wire(original, *decoded)) << Game::Command::payload_name(payload);
  }
}

TEST(CommandCodecTest, RefusesAnUnknownTypeOrAMissingField) {
  auto object = Game::Command::to_json(
      Command{.owner_id = 1, .payload = Game::Command::Stop{.units = {1}}});
  object["type"] = "teleport";
  EXPECT_FALSE(Game::Command::from_json(object).has_value());

  object = Game::Command::to_json(
      Command{.owner_id = 1,
              .payload = Game::Command::AttackTarget{.units = {1}, .target = 2}});
  auto body = object["payload"].toObject();
  body.remove("target");
  object["payload"] = body;
  EXPECT_FALSE(Game::Command::from_json(object).has_value());
}

TEST(CommandCodecTest, RefusesAnEnumOutOfRange) {
  auto object = Game::Command::to_json(
      Command{.owner_id = 1,
              .payload = Game::Command::UseCommanderAbility{
                  .commander = 1, .ability = Game::Command::CommanderAbility::Aura}});
  auto body = object["payload"].toObject();
  body["ability"] = 99;
  object["payload"] = body;
  EXPECT_FALSE(Game::Command::from_json(object).has_value());
}

struct Match {
  Match() {
    scope = std::make_unique<Game::Session::ScopedSession>(session);
    auto& owners = session.owners();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "player");
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "enemy");
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);
  }
  auto spawn(int owner_id) -> EntityID {
    auto* entity = session.world().create_entity();
    entity->add_component<Engine::Core::TransformComponent>();
    auto* unit =
        entity->add_component<Engine::Core::UnitComponent>(100, 100, 1.0F, 5.0F);
    unit->owner_id = owner_id;
    return entity->get_id();
  }
  auto holding(EntityID id) -> bool {
    return session.world()
               .get_entity(id)
               ->get_component<Engine::Core::HoldModeComponent>() != nullptr;
  }
  Game::Session::SessionContext session;
  std::unique_ptr<Game::Session::ScopedSession> scope;
};

TEST(ReplayTest, RecordsWhatTheQueueAcceptedAndPlaysItBackOnTheSameTicks) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath("match.soireplay");

  {
    Match match;
    const EntityID mine = match.spawn(1);
    const EntityID theirs = match.spawn(2);

    auto recorder = std::make_unique<Game::Command::ReplayRecorder>();
    Game::Command::ReplayHeader header;
    header.kind = "skirmish";
    header.reference = "maps/test.json";
    header.launch["players"] = 2;
    ASSERT_TRUE(recorder->begin(path, header, match.session.commands()));

    auto& queue = match.session.commands();
    queue.submit(Source::LocalPlayer, 1, Game::Command::SetHold{.units = {mine}});
    queue.submit(Source::LocalPlayer, 1, Game::Command::SetHold{.units = {theirs}});
    queue.drain(match.session.world(), 5);
    queue.submit(
        Source::AI, 1, Game::Command::SetRunMode{.units = {mine}, .active = true});
    queue.drain(match.session.world(), 9);
    EXPECT_EQ(recorder->recorded_count(), 2U);
    recorder->finish();
  }

  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  ASSERT_TRUE(file.has_value()) << error.toStdString();
  EXPECT_EQ(file->header.kind, "skirmish");
  EXPECT_EQ(file->header.launch["players"].toInt(), 2);
  ASSERT_EQ(file->commands.size(), 2U);
  EXPECT_EQ(file->commands[0].submitted_tick, 5U);
  EXPECT_EQ(file->commands[1].submitted_tick, 9U);
  EXPECT_EQ(file->last_tick(), 9U);

  Match again;
  const EntityID mine = again.spawn(1);
  (void)again.spawn(2);
  again.session.set_replay_player(std::make_unique<Game::Command::ReplayPlayer>(*file));
  auto& queue = again.session.commands();

  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  EXPECT_EQ(queue.pending(), 0U);
  EXPECT_EQ(queue.dropped_count(), 1U);

  auto* player = again.session.replay_player();
  ASSERT_NE(player, nullptr);
  for (std::uint64_t tick = 0; tick <= 9; ++tick) {
    player->feed(tick, queue);
    queue.drain(again.session.world(), tick);
    if (tick < 5) {
      EXPECT_FALSE(again.holding(mine)) << "hold applied before its tick " << tick;
    }
  }
  EXPECT_TRUE(player->finished());
  EXPECT_TRUE(again.holding(mine));
  EXPECT_EQ(queue.accepted_count(), 2U);

  again.session.set_replay_player(nullptr);
  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  EXPECT_EQ(queue.pending(), 1U);
}

TEST(ReplayTest, RefusesAFileWithALineItCannotApply) {
  QTemporaryDir dir;
  const QString path = dir.filePath("bad.soireplay");
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("{\"replay_format\":1,\"kind\":\"skirmish\"}\n");
    file.write("{\"type\":\"teleport\",\"source\":\"ai\",\"owner\":1,\"tick\":3,"
               "\"payload\":{}}\n");
  }
  QString error;
  EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
  EXPECT_TRUE(error.contains("bad.soireplay:2")) << error.toStdString();
}

} // namespace
