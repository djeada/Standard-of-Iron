

#include <QCoreApplication>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include "game/command/command_queue.h"
#include "game/command/replay.h"
#include "game/core/world.h"
#include "game/game_config.h"
#include "game/map/map_context.h"
#include "game/map/map_transformer.h"
#include "game/map/match_loader.h"
#include "game/save/replay_checkpoint.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/session/world_digest.h"
#include "game/systems/ai_system.h"
#include "game/systems/battlefield_capture.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/systems/victory_service.h"

namespace {

struct Options {
  std::string scenario = "bot_skirmish";
  std::string map;
  int local_owner = 1;
  double seconds = 30.0;
  std::uint64_t seed = 1;
  std::string record;
  std::string replay;
  std::optional<std::uint64_t> seek_tick;
  bool verify = false;
  bool realtime = false;
  bool scripted_orders = false;
  bool until_victory = false;
  int digest_every = 60;
  int checkpoint_every = 3600;
};

void usage() {
  std::fputs(
      "usage: soi_headless [--scenario id | --map path] [--seconds s]\n"
      "                    [--seed n] [--local-owner id]\n"
      "                    [--record file] [--replay file [--seek tick] [--verify]]\n"
      "                    [--realtime] [--digest-every ticks] "
      "[--checkpoint-every ticks]\n"
      "                    [--scripted-orders] [--until-victory]\n",
      stderr);
}

auto parse(int argc, char** argv, Options& out) -> bool {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&](const char* name) -> std::string {
      if (++i >= argc) {
        std::fprintf(stderr, "missing value for %s\n", name);
        std::exit(2);
      }
      return argv[i];
    };
    if (arg == "--scenario") {
      out.scenario = value("--scenario");
    } else if (arg == "--map") {
      out.map = value("--map");
    } else if (arg == "--local-owner") {
      out.local_owner = std::stoi(value("--local-owner"));
    } else if (arg == "--seconds") {
      out.seconds = std::stod(value("--seconds"));
    } else if (arg == "--seed") {
      out.seed = std::stoull(value("--seed"));
    } else if (arg == "--record") {
      out.record = value("--record");
    } else if (arg == "--replay") {
      out.replay = value("--replay");
    } else if (arg == "--seek") {
      const auto input = value("--seek");
      const auto* end = input.data() + input.size();
      std::uint64_t tick = 0;
      const auto parsed = std::from_chars(input.data(), end, tick);
      if (parsed.ec != std::errc{} || parsed.ptr != end) {
        std::fputs("soi_headless: invalid seek tick\n", stderr);
        return false;
      }
      out.seek_tick = tick;
    } else if (arg == "--verify") {
      out.verify = true;
    } else if (arg == "--realtime") {
      out.realtime = true;
    } else if (arg == "--scripted-orders") {
      out.scripted_orders = true;
    } else if (arg == "--until-victory") {
      out.until_victory = true;
    } else if (arg == "--digest-every") {
      out.digest_every = std::stoi(value("--digest-every"));
    } else if (arg == "--checkpoint-every") {
      out.checkpoint_every = std::stoi(value("--checkpoint-every"));
    } else if (arg == "--help" || arg == "-h") {
      usage();
      std::exit(0);
    } else {
      std::fprintf(stderr, "unknown option: %s\n", arg.c_str());
      usage();
      return false;
    }
  }
  return true;
}

constexpr int k_scripted_owner = 3;

auto add_scripted_player(Game::Session::SessionContext& session,
                         const Game::Units::UnitFactoryRegistry& factories)
    -> Engine::Core::EntityID {
  session.owners().register_owner_with_id(
      k_scripted_owner, Game::Systems::OwnerType::Player, "Scripted player");
  session.owners().set_owner_team(k_scripted_owner, 1);
  constexpr auto type = Game::Units::SpawnType::Archer;
  const auto unit = factories.create(type,
                                     session.world(),
                                     {.position = QVector3D(-18.0F, 0.0F, 10.0F),
                                      .player_id = k_scripted_owner,
                                      .spawn_type = type});
  return unit != nullptr ? unit->id() : Engine::Core::NULL_ENTITY;
}

void submit_scripted_orders(Game::Command::CommandQueue& queue,
                            std::uint64_t tick,
                            Engine::Core::EntityID archer) {
  using namespace Game::Command;
  // Include a rejected order so the verifier exercises validation on playback.
  if (tick == 0) {
    queue.submit(Source::LocalPlayer, k_scripted_owner, Stop{.units = {0}});
    queue.submit(Source::LocalPlayer,
                 k_scripted_owner,
                 Move{.units = {archer}, .targets = {QVector3D(-14.0F, 0.0F, 10.0F)}});
  } else if (tick == 90) {
    queue.submit(Source::LocalPlayer, k_scripted_owner, SetHold{.units = {archer}});
  } else if (tick == 180) {
    queue.submit(Source::LocalPlayer,
                 k_scripted_owner,
                 Move{.units = {archer}, .targets = {QVector3D(-9.0F, 0.0F, 10.0F)}});
  }
}

auto run_real_match_setup(Game::Session::SessionContext& session,
                          const Options& options) -> bool {
  Game::Systems::initialize_default_content(session.nations());
  session.nations().clear_player_assignments();
  session.owners().set_local_player_id(options.local_owner);

  const auto loaded = Game::Map::load_match(QString::fromStdString(options.map),
                                            session.world(),
                                            true,
                                            {.human_owners = {options.local_owner}});
  if (!loaded.ok) {
    std::fprintf(
        stderr, "soi_headless: %s\n", loaded.error_message.toStdString().c_str());
    return false;
  }

  Game::Systems::NavGrid::initialize(loaded.grid_width, loaded.grid_height);
  Game::Systems::register_runtime_systems(session.world());

  if (const auto context = Game::Map::MapContextStore::acquire(
          QString::fromStdString(options.map), nullptr);
      context.valid()) {
    const auto& definition = *context.definition();
    Game::Session::configure_map_systems(session.world(), definition, nullptr);
    Game::Systems::ResourceAmounts stock = definition.starting_resources;
    if (Game::GameConfig::instance().has_chosen_starting_gold()) {
      stock.set(Game::Systems::ResourceType::Gold,
                Game::GameConfig::instance().get_starting_gold());
    }
    for (const auto& owner : session.owners().get_all_owners()) {
      for (const auto type : Game::Systems::k_all_resource_types) {
        session.economy().set(owner.owner_id, type, stock.get(type));
      }
    }
  }

  std::printf("soi_headless: loaded %s (%dx%d, local owner %d)\n",
              loaded.map_name.toStdString().c_str(),
              loaded.grid_width,
              loaded.grid_height,
              options.local_owner);
  return true;
}

} // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Options options;
  if (!parse(argc, argv, options)) {
    return 2;
  }
  if (options.seek_tick && options.replay.empty()) {
    std::fputs("soi_headless: --seek requires --replay\n", stderr);
    return 2;
  }

  std::optional<Game::Command::ReplayFile> replay_file;
  if (!options.replay.empty()) {
    QString error;
    replay_file =
        Game::Command::ReplayFile::load(QString::fromStdString(options.replay), &error);
    if (!replay_file.has_value()) {
      std::fprintf(stderr, "soi_headless: %s\n", error.toStdString().c_str());
      return 2;
    }
    if (replay_file->header.kind != QLatin1String("headless-scenario") &&
        replay_file->header.kind != QLatin1String("skirmish-map")) {
      std::fprintf(stderr,
                   "soi_headless: replay kind '%s' is neither a headless scenario "
                   "nor a skirmish map\n",
                   replay_file->header.kind.toStdString().c_str());
      return 2;
    }
    if (replay_file->header.kind == QLatin1String("skirmish-map")) {
      options.map = replay_file->header.reference.toStdString();
    } else {
      options.scenario = replay_file->header.reference.toStdString();
    }
    options.seed = replay_file->header.rng_seed;
    options.local_owner =
        replay_file->header.launch.value(QLatin1String("local_owner")).toInt(1);
    options.scripted_orders =
        replay_file->header.launch.value(QLatin1String("scripted_orders")).toBool();
  }

  Game::BattlefieldCapture::ScenarioId scenario_id{};
  if (options.map.empty() &&
      !Game::BattlefieldCapture::parse_scenario(options.scenario, scenario_id)) {
    std::fprintf(
        stderr, "soi_headless: unknown scenario '%s'\n", options.scenario.c_str());
    return 2;
  }
  if (options.scripted_orders &&
      (!options.map.empty() ||
       scenario_id != Game::BattlefieldCapture::ScenarioId::BotSkirmish)) {
    std::fputs("soi_headless: --scripted-orders requires bot_skirmish\n", stderr);
    return 2;
  }
  if (options.until_victory && (!options.scripted_orders || replay_file)) {
    std::fputs("soi_headless: --until-victory requires a live scripted bot skirmish\n",
               stderr);
    return 2;
  }

  Game::Session::SessionContext session(Game::Session::SessionContext::Config{
      .rng_seed = options.seed,
      .tick_seconds = replay_file
                          ? replay_file->header.tick_seconds
                          : Game::Session::SimulationClock::k_default_tick_seconds});
  Game::Session::ScopedSession const active(session);
  if (replay_file) {
    if (replay_file->recovered_tail) {
      std::fprintf(stderr,
                   "soi_headless: recovered log through tick %llu\n",
                   static_cast<unsigned long long>(replay_file->last_tick()));
    }
    session.set_replay_player(
        std::make_unique<Game::Command::ReplayPlayer>(std::move(*replay_file)));
  }

  const QString reference = options.map.empty()
                                ? QString::fromStdString(options.scenario)
                                : QString::fromStdString(options.map);
  Engine::Core::EntityID scripted_archer = Engine::Core::NULL_ENTITY;
  if (!options.map.empty()) {
    if (!run_real_match_setup(session, options)) {
      return 2;
    }
  } else {
    auto match = Game::BattlefieldCapture::build_scenario(
        scenario_id, session.world(), options.scripted_orders);
    if (options.scripted_orders) {
      scripted_archer = add_scripted_player(session, match->factories);
      if (scripted_archer == Engine::Core::NULL_ENTITY) {
        std::fputs("soi_headless: cannot spawn scripted player\n", stderr);
        return 2;
      }
    }
  }

  if (!options.record.empty()) {
    Game::Command::ReplayHeader header;
    header.kind = options.map.empty() ? QStringLiteral("headless-scenario")
                                      : QStringLiteral("skirmish-map");
    header.reference = reference;
    header.launch["local_owner"] = options.local_owner;
    header.launch["scripted_orders"] = options.scripted_orders;
    header.tick_seconds = session.clock().tick_seconds();
    header.rng_seed = options.seed;
    header.digest_interval =
        static_cast<std::uint32_t>(std::max(options.digest_every, 0));
    header.checkpoint_interval =
        static_cast<std::uint32_t>(std::max(options.checkpoint_every, 0));
    auto recorder = std::make_unique<Game::Command::ReplayRecorder>();
    if (!recorder->begin(
            QString::fromStdString(options.record),
            header,
            session.commands(),
            [&session]() { return Game::Save::capture_replay_checkpoint(session); })) {
      std::fprintf(stderr, "soi_headless: cannot write %s\n", options.record.c_str());
      return 2;
    }
    session.set_replay_recorder(std::move(recorder));
  }
  const double tick_seconds = session.clock().tick_seconds();
  const double requested_ticks = options.seconds / tick_seconds;
  if (!std::isfinite(requested_ticks) || requested_ticks < 0.0 ||
      requested_ticks >=
          static_cast<double>(std::numeric_limits<std::uint64_t>::max())) {
    std::fputs("soi_headless: invalid match duration\n", stderr);
    return 2;
  }
  const auto* replay = session.replay_player();
  const auto total_ticks = replay != nullptr
                               ? replay->file().last_tick()
                               : static_cast<std::uint64_t>(requested_ticks);
  const auto started = std::chrono::steady_clock::now();
  std::uint64_t checkpoint_tick = 0;
  if (options.seek_tick) {
    const auto checkpoint = Game::Save::seek_replay(session, *options.seek_tick);
    if (!checkpoint) {
      const auto& divergence = replay->divergence();
      if (divergence) {
        std::fprintf(stderr,
                     "soi_headless: seek diverged at tick %llu in %s\n",
                     static_cast<unsigned long long>(divergence->tick),
                     divergence->subsystem);
      } else {
        std::fputs("soi_headless: could not seek to the requested tick\n", stderr);
      }
      return 12;
    }
    checkpoint_tick = *checkpoint;
    std::printf("soi_headless: sought to tick %llu from checkpoint %llu\n",
                static_cast<unsigned long long>(*options.seek_tick),
                static_cast<unsigned long long>(checkpoint_tick));
  }
  std::uint64_t ticks = session.clock().tick();
  bool victory = false;
  while (ticks < total_ticks) {

    if (replay != nullptr) {
      const auto stepped = session.advance_from_log(options.realtime ? 1 : 120);
      if (stepped == 0) {
        std::fputs("soi_headless: log clock stopped before the last complete tick\n",
                   stderr);
        return 12;
      }
      ticks += static_cast<std::uint64_t>(stepped);
    } else {
      if (options.scripted_orders) {
        submit_scripted_orders(session.commands(), ticks, scripted_archer);
      }
      ticks += static_cast<std::uint64_t>(session.advance(
          tick_seconds,
          1,
          Game::Session::SessionContext::OverloadPolicy::DiscardBacklog));
      if (options.until_victory &&
          Game::Systems::living_competing_teams(session.world(), session.owners())
                  .size() < 2U) {
        victory = true;
        break;
      }
    }
    if (options.realtime) {
      std::this_thread::sleep_for(std::chrono::duration<double>(tick_seconds));
    }
  }
  const double wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

  std::printf("soi_headless: %s ran %llu ticks (%.1f s simulated) in %.2f s wall; "
              "digest %llu; %llu commands accepted, %llu rejected, %llu dropped\n",
              reference.toStdString().c_str(),
              static_cast<unsigned long long>(ticks),
              static_cast<double>(ticks) * tick_seconds,
              wall,
              static_cast<unsigned long long>(Game::Session::session_digest(session)),
              static_cast<unsigned long long>(session.commands().accepted_count()),
              static_cast<unsigned long long>(session.commands().rejected_count()),
              static_cast<unsigned long long>(session.commands().dropped_count()));

  if (auto* recorder = session.replay_recorder()) {
    recorder->finish();
    if (recorder->failed()) {
      std::fprintf(
          stderr, "soi_headless: failed to write %s\n", options.record.c_str());
      return 2;
    }
    std::printf("soi_headless: recorded %llu commands to %s\n",
                static_cast<unsigned long long>(recorder->recorded_count()),
                options.record.c_str());
  }
  if (auto* player = session.replay_player()) {
    if (const auto& divergence = player->divergence(); divergence.has_value()) {
      std::printf("SOI_HEADLESS_REPLAY: FAIL - diverged at tick %llu (recorded %llu, "
                  "observed %llu)\n",
                  static_cast<unsigned long long>(divergence->tick),
                  static_cast<unsigned long long>(divergence->recorded),
                  static_cast<unsigned long long>(divergence->observed));
      return options.verify ? 12 : 0;
    }
    const auto* ai = session.world().get_system<Game::Systems::AISystem>();
    const auto& entries = player->file().records;
    const auto expected_commands = std::count_if(
        entries.begin(), entries.end(), [checkpoint_tick](const auto& entry) {
          return entry->tick > checkpoint_tick &&
                 std::holds_alternative<Game::Command::Payload>(entry->payload);
        });
    const auto expected_digests = std::count_if(
        entries.begin(), entries.end(), [checkpoint_tick](const auto& entry) {
          return entry->tick >= checkpoint_tick &&
                 std::holds_alternative<Game::Session::SubsystemDigests>(
                     entry->payload);
        });
    if (player->fed_count() != static_cast<std::size_t>(expected_commands) ||
        player->checked_count() != static_cast<std::size_t>(expected_digests) ||
        (ai != nullptr && ai->completed_decision_count() != 0)) {
      std::fputs(
          "SOI_HEADLESS_REPLAY: FAIL - playback did not consume the recorded stream\n",
          stderr);
      return options.verify ? 12 : 0;
    }
    std::printf("SOI_HEADLESS_REPLAY: PASS - %zu commands fed, %zu digests matched\n",
                player->fed_count(),
                player->checked_count());
  }
  if (options.until_victory && victory) {
    for (const auto& [team, outcome] :
         Game::Systems::team_elimination_outcomes(session.world(), session.owners())) {
      const char* label =
          outcome == Game::Systems::EliminationOutcome::Victory ? "victory" : "defeat";
      if (team.team_id != 0) {
        std::printf("soi_headless: team %d: %s\n", team.team_id, label);
      } else {
        std::printf("soi_headless: owner %d: %s\n", team.unteamed_owner, label);
      }
    }
  }
  if (options.until_victory && !victory) {
    std::fputs("soi_headless: neither bot was eliminated before the time limit\n",
               stderr);
    return 2;
  }
  return 0;
}
