#include <QBuffer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <random>
#include <variant>

#include "game/command/command.h"
#include "game/command/command_codec.h"
#include "game/command/command_queue.h"
#include "game/command/command_system.h"
#include "game/command/commander_input.h"
#include "game/command/json_u64.h"
#include "game/command/log_format.h"
#include "game/command/replay.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/save/replay_checkpoint.h"
#include "game/session/deterministic_rng.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"

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
  all.emplace_back(AllyTribute{.ally_owner = 3,
                               .resource = Game::Systems::ResourceType::Iron,
                               .amount = 75,
                               .request = true});
  all.emplace_back(AllyCall{.target = 21, .kind = Game::Systems::AllyCallKind::Attack});
  all.emplace_back(AllyAppealAnswer{.appeal_id = 7, .accept = true});
  all.emplace_back(RollStones{.units = {21, 22}});
  all.emplace_back(CrossByRaft{.units = {23, 24}});
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
  all.emplace_back(DismantleStructure{.units = {27}, .structure = 28});
  all.emplace_back(DivideSquads{.units = {29, 30}});
  all.emplace_back(MergeSquads{.units = {31, 32}});
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
  all.emplace_back(Game::Systems::AllyTributeAnswer{
      .requester = 2, .giver = 3, .requested = 50, .granted = 25});
  all.emplace_back(AIAllyCall{.target = 42, .target_owner = 2, .target_z = -6.0F});
  all.emplace_back(
      Game::Systems::AllyCallAnswer{.call_id = 5, .requester = 2, .ally = 3});
  all.emplace_back(
      Game::Systems::AllyAppeal{.appeal_id = 72, .from_ally = 3, .to_owner = 2});
  all.emplace_back(
      AIAllyAppealReply{.reply = {.appeal_id = 72, .from_ally = 3, .to_owner = 2},
                        .goodwill_change = 0.2F,
                        .answers_player = true});
  all.emplace_back(
      SetGarrisonLeash{.x = 3.0F, .z = 5.0F, .half_x = 2.0F, .half_z = 6.0F});
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

TEST(CommandCodecTest, ADeployWithTheLastValueOfEveryOptionSurvivesTheRoundTrip) {
  Game::Command::DeployFormation deploy;
  deploy.units = {1};
  deploy.intent = Game::Formation::ArmyFormationIntent::ElephantScreen;
  deploy.options.flank_preference = Game::Formation::FlankPreference::Split;
  deploy.options.movement_policy = Game::Formation::MovementPolicy::DoctrineDefault;
  deploy.options.ranged_placement = Game::Formation::RangedPlacement::Automatic;
  deploy.options.mixed_policy = Game::Formation::MixedDoctrinePolicy::MajorityDoctrine;
  const Command original{.owner_id = 1, .payload = deploy};
  const auto decoded = Game::Command::from_json(Game::Command::to_json(original));
  ASSERT_TRUE(decoded.has_value());
  EXPECT_TRUE(same_wire(original, *decoded));
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

TEST(ReplayTest, RecordsPreValidationInputsAndAppliesThemOnTheSameTicks) {
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
    queue.drain(match.session.world(), 1);
    queue.log().complete_tick(1);
    queue.submit(
        Source::AI, 1, Game::Command::SetRunMode{.units = {mine}, .active = true});
    queue.drain(match.session.world(), 2);
    queue.log().complete_tick(2);
    EXPECT_EQ(recorder->recorded_count(), 3U);
    recorder->finish();
  }

  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  ASSERT_TRUE(file.has_value()) << error.toStdString();
  EXPECT_EQ(file->header.kind, "skirmish");
  EXPECT_EQ(file->header.launch["players"].toInt(), 2);
  ASSERT_EQ(file->command_count(), 3U);
  const auto first = file->entries_at(1);
  ASSERT_EQ(first.size(), 3U);
  EXPECT_LT(first[0]->seq, first[1]->seq);
  EXPECT_TRUE(std::holds_alternative<Game::Command::SetHold>(
      std::get<Payload>(first[1]->payload)));
  EXPECT_EQ(file->last_tick(), 2U);

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
  EXPECT_FALSE(again.holding(mine));
  for (std::uint64_t tick = 1; tick <= 2; ++tick) {
    player->feed(tick, queue);
    queue.drain(again.session.world(), tick);
  }
  EXPECT_TRUE(player->finished());
  EXPECT_TRUE(again.holding(mine));
  EXPECT_EQ(queue.accepted_count(), 2U);
  EXPECT_EQ(queue.rejected_count(), 1U);

  again.session.set_replay_player(nullptr);
  queue.submit(Source::LocalPlayer, 1, Game::Command::Stop{.units = {mine}});
  EXPECT_EQ(queue.pending(), 1U);
}

namespace {

auto write_replay(const QString& path,
                  const QJsonObject& header,
                  const QByteArray& body) -> bool {
  QFile file(path);
  using namespace Game::Command;
  if (!file.open(QIODevice::WriteOnly) || file.write(k_log_magic) < 0 ||
      !write_log_frame(file, header)) {
    return false;
  }
  if (!body.isEmpty()) {
    auto object = QJsonDocument::fromJson(body).object();
    const auto tick = object.take("tick");
    const QString kind =
        object.contains("commander_action")  ? QStringLiteral("commander_action")
        : object.contains("commander_input") ? QStringLiteral("commander_input")
        : object.contains("digest")          ? QStringLiteral("digest")
                                             : QStringLiteral("command");
    return write_log_frame(file,
                           {{"kind", kind},
                            {"tick", tick},
                            {"seq", "1"},
                            {"source", "ai"},
                            {"owner", 1},
                            {"body", object}});
  }
  return true;
}

} // namespace

TEST(ReplayTest, RefusesAFileWithALineItCannotApply) {
  QTemporaryDir dir;
  const QString path = dir.filePath("bad.soireplay");
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  ASSERT_TRUE(write_replay(path,
                           header.to_json(),
                           "{\"type\":\"teleport\",\"source\":\"ai\",\"owner\":1,"
                           "\"tick\":3,\"payload\":{}}\n"));
  QString error;
  EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
  EXPECT_TRUE(error.contains("invalid command log record")) << error.toStdString();
}

TEST(ReplayTest, RefusesAFileRecordedByAnotherSimulationBuild) {
  QTemporaryDir dir;
  const QString path = dir.filePath("other-build.soireplay");
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  header.build_id = QStringLiteral("0000deadbeef");
  ASSERT_TRUE(write_replay(path, header.to_json(), ""));
  QString error;
  EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
  EXPECT_TRUE(error.contains("simulation build")) << error.toStdString();
}

TEST(ReplayTest, RefusesAFileRecordedAgainstOtherContent) {
  QTemporaryDir dir;
  const QString path = dir.filePath("other-content.soireplay");
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  header.content_digest = QStringLiteral("00000000cafef00d");
  ASSERT_TRUE(write_replay(path, header.to_json(), ""));
  QString error;
  EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
  EXPECT_TRUE(error.contains("content")) << error.toStdString();
}

TEST(ReplayTest, RequiresTheBuildThatRecordedTheReplay) {
  QTemporaryDir dir;
  const QString path = dir.filePath("legacy.soireplay");
  QJsonObject header;
  header["replay_format"] = Game::Command::k_replay_format_version;
  header["kind"] = QStringLiteral("skirmish");
  ASSERT_TRUE(write_replay(path, header, ""));
  QString error;
  const auto loaded = Game::Command::ReplayFile::load(path, &error);
  EXPECT_FALSE(loaded.has_value());
  EXPECT_FALSE(error.isEmpty());
}

TEST(ReplayTest, RoundTripsItsOwnHeader) {
  QTemporaryDir dir;
  const QString path = dir.filePath("own.soireplay");
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  header.reference = QStringLiteral("map");
  ASSERT_TRUE(write_replay(path, header.to_json(), ""));
  QString error;
  const auto loaded = Game::Command::ReplayFile::load(path, &error);
  ASSERT_TRUE(loaded.has_value()) << error.toStdString();
  EXPECT_EQ(loaded->header.build_id, Game::Command::simulation_build_id());
  EXPECT_EQ(loaded->header.content_digest, Game::Command::simulation_content_digest());
}

TEST(ReplayTest, ACheckpointRestoresAWorldAndContinuesWithTheLogTail) {
  using namespace Game::Command;
  using Game::Session::SessionContext;
  const auto advance = [](Match& match) {
    return match.session.advance(match.session.clock().tick_seconds(),
                                 1,
                                 SessionContext::OverloadPolicy::KeepBacklog);
  };
  QTemporaryDir temp;
  const auto path = temp.filePath("checkpoint.soireplay");
  Match live;
  auto& session = live.session;
  const auto unit = live.spawn(1);
  session.world().add_system(std::make_unique<CommandSystem>());
  session.economy().set(1, Game::Systems::ResourceType::Gold, 123);
  (void)session.rng().next_u64();
  ReplayHeader header;
  header.checkpoint_interval = 2;
  header.digest_interval = 1;
  auto recorder = std::make_unique<ReplayRecorder>();
  ASSERT_TRUE(recorder->begin(path, header, session.commands(), [&session]() {
    return Game::Save::capture_replay_checkpoint(session);
  }));
  session.set_replay_recorder(std::move(recorder));
  session.commands().submit(Source::LocalPlayer, 1, SetHold{.units = {unit}});
  ASSERT_EQ(advance(live), 1);
  ASSERT_EQ(advance(live), 1);
  const auto before = Game::Session::subsystem_digests(session);
  session.commands().submit(
      Source::LocalPlayer, 1, SetHold{.units = {unit}, .active = false});
  ASSERT_EQ(advance(live), 1);
  ASSERT_EQ(advance(live), 1);
  const auto expected = Game::Session::subsystem_digests(session);
  session.replay_recorder()->finish();

  QString error;
  auto file = ReplayFile::load(path, &error);
  ASSERT_TRUE(file) << error.toStdString();
  EXPECT_EQ(file->checkpoint_at_or_before(1), nullptr);
  const auto* checkpoint = file->checkpoint_at_or_before(3);
  ASSERT_NE(checkpoint, nullptr);
  EXPECT_EQ(checkpoint->tick, 2U);
  Match restored;
  restored.session.world().add_system(std::make_unique<CommandSystem>());
  ASSERT_TRUE(Game::Save::restore_replay_checkpoint(
      restored.session, std::get<Checkpoint>(checkpoint->payload).snapshot));
  EXPECT_EQ(Game::Session::subsystem_digests(restored.session).root, before.root);
  restored.session.set_replay_player(std::make_unique<ReplayPlayer>(std::move(*file)));
  EXPECT_EQ(restored.session.advance_from_log(10), 2);
  EXPECT_FALSE(restored.session.replay_player()->divergence());
  EXPECT_EQ(Game::Session::subsystem_digests(restored.session).root, expected.root);
}

} // namespace

TEST(CommanderInputRecordTest, AFrameRoundTripsThroughItsJsonForm) {
  Game::Command::CommanderInputFrame frame;
  frame.commander = 4242;
  frame.view_yaw = -37.5F;
  frame.view_pitch = -11.25F;
  frame.sequence = 19;
  frame.dodge_direction = QVector3D(0.0F, 0.0F, -1.0F);
  frame.set(Game::Command::CommanderInputFrame::Forward, true);
  frame.set(Game::Command::CommanderInputFrame::Run, true);
  frame.set(Game::Command::CommanderInputFrame::HeavyPressed, true);
  frame.set(Game::Command::CommanderInputFrame::HasDodgeDirection, true);

  const auto decoded =
      Game::Command::commander_input_from_json(Game::Command::to_json(frame));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->commander, frame.commander);
  EXPECT_EQ(decoded->buttons, frame.buttons);
  EXPECT_EQ(decoded->sequence, frame.sequence);
  EXPECT_FLOAT_EQ(decoded->view_yaw, frame.view_yaw);
  EXPECT_FLOAT_EQ(decoded->view_pitch, frame.view_pitch);
  EXPECT_EQ(decoded->dodge_direction, frame.dodge_direction);
  EXPECT_TRUE(decoded->held(Game::Command::CommanderInputFrame::Forward));
  EXPECT_TRUE(decoded->held(Game::Command::CommanderInputFrame::HeavyPressed));
  EXPECT_FALSE(decoded->held(Game::Command::CommanderInputFrame::JumpPressed));
}

TEST(CommanderInputRecordTest, TheBowAimRayRoundTripsWithTheFrame) {
  Game::Command::CommanderInputFrame frame;
  frame.commander = 9;
  frame.aim_origin = QVector3D(1.25F, 3.5F, -7.0F);
  frame.aim_forward = QVector3D(0.0F, -0.1F, -0.995F);
  frame.set(Game::Command::CommanderInputFrame::HasAimRay, true);

  const auto decoded =
      Game::Command::commander_input_from_json(Game::Command::to_json(frame));

  ASSERT_TRUE(decoded.has_value());
  EXPECT_TRUE(decoded->held(Game::Command::CommanderInputFrame::HasAimRay))
      << "the bow fires along the camera, which is presentation state; a replay "
         "has to carry the ray the simulation used";
  EXPECT_EQ(decoded->aim_origin, frame.aim_origin);
  EXPECT_EQ(decoded->aim_forward, frame.aim_forward);
}

TEST(CommanderInputRecordTest, RecordedFramesComeBackAtTheTickTheyWereAppliedOn) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath("commander.soireplay");

  Game::Command::CommandQueue queue;
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  header.digest_interval = 0;

  Game::Command::CommanderInputFrame first;
  first.commander = 7;
  first.view_yaw = 90.0F;
  first.set(Game::Command::CommanderInputFrame::Forward, true);

  Game::Command::CommanderInputFrame second;
  second.commander = 7;
  second.view_yaw = 91.5F;
  second.set(Game::Command::CommanderInputFrame::JumpPressed, true);

  {
    Game::Command::ReplayRecorder recorder;
    ASSERT_TRUE(recorder.begin(path, header, queue));
    queue.log().append(1, Source::LocalPlayer, 1, first);
    queue.log().complete_tick(1);
    queue.log().append(2, Source::LocalPlayer, 1, second);
    queue.log().complete_tick(2);
    recorder.finish();
  }

  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  ASSERT_TRUE(file.has_value()) << error.toStdString();
  const auto first_tick = file->entries_at(1);
  ASSERT_EQ(first_tick.size(), 2U);
  const auto& first_frame =
      std::get<Game::Command::CommanderInputFrame>(first_tick.front()->payload);
  EXPECT_TRUE(first_frame.held(Game::Command::CommanderInputFrame::Forward));
  EXPECT_FLOAT_EQ(first_frame.view_yaw, 90.0F);
  const auto second_tick = file->entries_at(2);
  ASSERT_EQ(second_tick.size(), 3U);
  const auto& second_frame =
      std::get<Game::Command::CommanderInputFrame>(second_tick.front()->payload);
  EXPECT_TRUE(second_frame.held(Game::Command::CommanderInputFrame::JumpPressed));
  EXPECT_TRUE(file->entries_at(3).empty());
}

TEST(CommanderInputRecordTest, AFileWithoutCommanderInputStillLoads) {
  QTemporaryDir dir;
  const QString path = dir.filePath("plain.soireplay");
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  ASSERT_TRUE(write_replay(path, header.to_json(), ""));

  QString error;
  auto file = Game::Command::ReplayFile::load(path, &error);
  ASSERT_TRUE(file.has_value()) << error.toStdString();
  EXPECT_TRUE(file->records.empty());
}

TEST(CommanderInputRecordTest, ActionsAndFramesShareTheLogOrder) {
  using namespace Game::Command;
  QTemporaryDir dir;
  const QString path = dir.filePath("commander_actions.soireplay");
  CommandQueue queue;
  ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  ReplayRecorder recorder;
  ASSERT_TRUE(recorder.begin(path, header, queue));
  queue.log().append(1,
                     Source::LocalPlayer,
                     1,
                     CommanderActionRecord{.action = CommanderAction::EnterControl});
  queue.log().append(1, Source::LocalPlayer, 1, CommanderInputFrame{.commander = 2});
  queue.log().append(1,
                     Source::LocalPlayer,
                     1,
                     CommanderActionRecord{.action = CommanderAction::CycleLockOn,
                                           .view_yaw = 171.5F,
                                           .view_pitch = -6.0F});
  queue.log().complete_tick(1);
  recorder.finish();

  QString error;
  auto file = ReplayFile::load(path, &error);
  ASSERT_TRUE(file) << error.toStdString();
  CommandQueue follower;
  follower.set_replay_only(true);
  ReplayPlayer player(std::move(*file));
  player.feed(1, follower);
  player.feed(1, follower);
  const auto entries = follower.log().entries_at(1);
  ASSERT_EQ(entries.size(), 5U);
  EXPECT_EQ(std::get<CommanderActionRecord>(entries[0]->payload).action,
            CommanderAction::EnterControl);
  EXPECT_EQ(std::get<CommanderInputFrame>(entries[1]->payload).commander, 2U);
  EXPECT_FLOAT_EQ(std::get<CommanderActionRecord>(entries[2]->payload).view_yaw,
                  171.5F);
}

TEST(CommanderInputRecordTest, AnUnknownCommanderActionIsAMalformedReplay) {
  QTemporaryDir dir;
  const QString path = dir.filePath("bad_action.soireplay");
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  ASSERT_TRUE(write_replay(
      path, header.to_json(), R"({"commander_action":"teleport","tick":3})"));
  QString error;
  EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
  EXPECT_TRUE(error.contains("invalid command log record")) << error.toStdString();
}

constexpr std::array<std::uint64_t, 8> k_u64_boundary_values = {
    0ULL,
    1ULL,
    (1ULL << 53U) - 1ULL,
    (1ULL << 53U) + 1ULL,
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()),
    static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ULL,
    std::numeric_limits<std::uint64_t>::max(),
    0x5EED5EED5EED5EEDULL,
};

TEST(JsonU64Test, RoundTripsEveryBoundary) {
  for (const std::uint64_t value : k_u64_boundary_values) {
    const QJsonValue encoded = Game::Command::u64_to_json(value);
    EXPECT_TRUE(encoded.isString()) << value;
    const auto decoded = Game::Command::u64_from_json(encoded);
    ASSERT_TRUE(decoded.has_value()) << value;
    EXPECT_EQ(*decoded, value);
  }
}

TEST(JsonU64Test, RefusesMalformedValues) {
  const std::array<QJsonValue, 9> bad_values = {
      QJsonValue(QStringLiteral("-1")),
      QJsonValue(QStringLiteral("1.5")),
      QJsonValue(QStringLiteral(" 1")),
      QJsonValue(QStringLiteral("0x10")),
      QJsonValue(QStringLiteral("18446744073709551616")),
      QJsonValue(QStringLiteral("")),
      QJsonValue(true),
      QJsonValue(-1.0),
      QJsonValue(1.5),
  };
  for (const QJsonValue& value : bad_values) {
    EXPECT_FALSE(Game::Command::u64_from_json(value).has_value());
    EXPECT_FALSE(Game::Command::u64_from_json(value, true).has_value());
  }
}

TEST(JsonU64Test, AcceptsLegacyNonNegativeIntegersWhenAllowed) {
  EXPECT_EQ(Game::Command::u64_from_json(QJsonValue(0), true), 0ULL);
  EXPECT_EQ(Game::Command::u64_from_json(QJsonValue(42), true), 42ULL);
  EXPECT_FALSE(Game::Command::u64_from_json(QJsonValue(42), false).has_value());
}

TEST(ReplayTest, HeaderSeedRoundTripsThroughTheFile) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  for (std::size_t i = 0; i < k_u64_boundary_values.size(); ++i) {
    const std::uint64_t seed = k_u64_boundary_values[i];
    const QString path = dir.filePath(QStringLiteral("seed_%1.soireplay").arg(i));
    Game::Command::ReplayHeader header;
    header.kind = QStringLiteral("skirmish");
    header.reference = QStringLiteral("map");
    header.rng_seed = seed;
    ASSERT_TRUE(write_replay(path, header.to_json(), ""));

    QString error;
    const auto loaded = Game::Command::ReplayFile::load(path, &error);
    ASSERT_TRUE(loaded.has_value()) << error.toStdString();
    EXPECT_EQ(loaded->header.rng_seed, seed);
  }
}

TEST(ReplayTest, LogPositionsRoundTripWithoutLosingPrecision) {
  using namespace Game::Command;
  for (const std::uint64_t tick : k_u64_boundary_values) {
    const LogEntry entry{tick, 1, 1, Source::AI, Payload{Stop{.units = {tick}}}};
    const auto decoded = log_entry_from_json(to_json(entry));
    ASSERT_TRUE(decoded);
    EXPECT_EQ(decoded->tick, tick);
    ASSERT_EQ(std::get<Stop>(std::get<Payload>(decoded->payload)).units,
              (std::vector<Engine::Core::EntityID>{tick}));
  }
}

TEST(ReplayTest, RecoversAtTheLastCompleteTickAfterATruncatedFrame) {
  using namespace Game::Command;
  QTemporaryDir dir;
  const QString path = dir.filePath("truncated.soireplay");
  CommandQueue queue;
  ReplayRecorder recorder;
  ASSERT_TRUE(recorder.begin(path, ReplayHeader{}, queue));
  queue.submit(Source::LocalPlayer, 1, Stop{.units = {1}});
  queue.log().complete_tick(1);
  queue.submit(Source::LocalPlayer, 1, Stop{.units = {2}});
  recorder.finish();
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_TRUE(file.resize(file.size() - 6));
  }
  QString error;
  auto file = ReplayFile::load(path, &error);
  ASSERT_TRUE(file) << error.toStdString();
  EXPECT_TRUE(file->recovered_tail);
  EXPECT_EQ(file->last_tick(), 1U);
  EXPECT_EQ(file->command_count(), 1U);
}

TEST(ReplayTest, RejectsCorruptedFrames) {
  using namespace Game::Command;
  QTemporaryDir dir;
  const QString path = dir.filePath("corrupt.soireplay");
  CommandQueue queue;
  ReplayRecorder recorder;
  ASSERT_TRUE(recorder.begin(path, ReplayHeader{}, queue));
  queue.log().complete_tick(1);
  recorder.finish();
  {
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadWrite));
    ASSERT_TRUE(file.seek(file.size() - 1));
    ASSERT_EQ(file.write("!"), 1);
  }
  QString error;
  EXPECT_FALSE(ReplayFile::load(path, &error));
  EXPECT_TRUE(error.contains("invalid command log record"));
}

TEST(ReplayFrameTest, AcceptsOnlyCompleteAndIntactFrames) {
  using namespace Game::Command;
  QBuffer output;
  ASSERT_TRUE(output.open(QIODevice::WriteOnly));
  const QJsonObject record{{"kind", "tick_complete"}, {"tick", "1"}, {"seq", "1"}};
  ASSERT_TRUE(write_log_frame(output, record));
  const QByteArray bytes = output.data();
  for (qsizetype length = 0; length < bytes.size(); ++length) {
    QBuffer input;
    input.setData(bytes.left(length));
    ASSERT_TRUE(input.open(QIODevice::ReadOnly));
    QJsonObject decoded;
    EXPECT_EQ(read_log_frame(input, decoded),
              length == 0 ? FrameStatus::End : FrameStatus::Truncated)
        << length;
  }

  std::mt19937 rng(1692);
  for (int trial = 0; trial < 512; ++trial) {
    auto mutated = bytes;
    const auto index = static_cast<qsizetype>(rng() % mutated.size());
    mutated[index] ^= static_cast<char>(1U << (rng() % 8));
    QBuffer input(&mutated);
    ASSERT_TRUE(input.open(QIODevice::ReadOnly));
    QJsonObject decoded;
    EXPECT_NE(read_log_frame(input, decoded), FrameStatus::Valid) << trial;
  }
}

TEST(ReplayTest, RefusesANegativeOrFractionalTick) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");

  const std::array<const char*, 3> bad_lines = {
      R"({"digest":"123","tick":"-1"})",
      R"({"commander_input":true,"commander":1,"tick":"1.5"})",
      R"({"commander_action":"enter_control","tick":1.5})",
  };
  for (std::size_t i = 0; i < bad_lines.size(); ++i) {
    const QString path = dir.filePath(QStringLiteral("bad_tick_%1.soireplay").arg(i));
    ASSERT_TRUE(write_replay(path, header.to_json(), bad_lines[i]));
    QString error;
    EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
    EXPECT_FALSE(error.isEmpty());
  }
}

TEST(ReplayTest, RefusesAnOlderReplayFormat) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString path = dir.filePath("v6.soireplay");

  Game::Command::ReplayHeader header;
  header.kind = QStringLiteral("skirmish");
  QJsonObject json = header.to_json();
  json["replay_format"] = 6;
  ASSERT_TRUE(write_replay(path, json, ""));

  QString error;
  EXPECT_FALSE(Game::Command::ReplayFile::load(path, &error).has_value());
  EXPECT_FALSE(error.isEmpty());
}
