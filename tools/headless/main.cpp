

#include <QCoreApplication>
#include <QJsonObject>
#include <QString>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#include "game/command/command_queue.h"
#include "game/command/replay.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/session/world_digest.h"
#include "game/systems/battlefield_capture.h"

namespace {

struct Options {
  std::string scenario = "bot_skirmish";
  double seconds = 30.0;
  std::uint64_t seed = 1;
  std::string record;
  std::string replay;
  bool verify = false;
  bool realtime = false;
  int digest_every = 60;
};

void usage() {
  std::fputs("usage: soi_headless [--scenario id] [--seconds s] [--seed n]\n"
             "                    [--record file] [--replay file [--verify]]\n"
             "                    [--realtime] [--digest-every ticks]\n",
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
    } else if (arg == "--seconds") {
      out.seconds = std::stod(value("--seconds"));
    } else if (arg == "--seed") {
      out.seed = std::stoull(value("--seed"));
    } else if (arg == "--record") {
      out.record = value("--record");
    } else if (arg == "--replay") {
      out.replay = value("--replay");
    } else if (arg == "--verify") {
      out.verify = true;
    } else if (arg == "--realtime") {
      out.realtime = true;
    } else if (arg == "--digest-every") {
      out.digest_every = std::stoi(value("--digest-every"));
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

} // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Options options;
  if (!parse(argc, argv, options)) {
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
    if (replay_file->header.kind != QLatin1String("headless-scenario")) {
      std::fprintf(stderr,
                   "soi_headless: replay kind '%s' is not a headless scenario\n",
                   replay_file->header.kind.toStdString().c_str());
      return 2;
    }
    options.scenario = replay_file->header.reference.toStdString();
    options.seed = replay_file->header.rng_seed;
    if (const auto seconds = replay_file->header.launch.value(QLatin1String("seconds"));
        seconds.isDouble()) {
      options.seconds = seconds.toDouble();
    }
  }

  Game::BattlefieldCapture::ScenarioId scenario_id{};
  if (!Game::BattlefieldCapture::parse_scenario(options.scenario, scenario_id)) {
    std::fprintf(
        stderr, "soi_headless: unknown scenario '%s'\n", options.scenario.c_str());
    return 2;
  }

  Game::Session::SessionContext session(
      Game::Session::SessionContext::Config{.rng_seed = options.seed});
  Game::Session::ScopedSession const active(session);

  auto match = Game::BattlefieldCapture::build_scenario(scenario_id, session.world());

  if (!options.record.empty()) {
    Game::Command::ReplayHeader header;
    header.kind = QStringLiteral("headless-scenario");
    header.reference = QString::fromStdString(options.scenario);
    header.launch["seconds"] = options.seconds;
    header.tick_seconds = session.clock().tick_seconds();
    header.rng_seed = options.seed;
    header.digest_interval =
        static_cast<std::uint32_t>(std::max(options.digest_every, 0));
    auto recorder = std::make_unique<Game::Command::ReplayRecorder>();
    if (!recorder->begin(
            QString::fromStdString(options.record), header, session.commands())) {
      std::fprintf(stderr, "soi_headless: cannot write %s\n", options.record.c_str());
      return 2;
    }
    session.set_replay_recorder(std::move(recorder));
  }
  if (replay_file.has_value()) {
    session.set_replay_player(
        std::make_unique<Game::Command::ReplayPlayer>(std::move(*replay_file)));
  }

  const double tick_seconds = session.clock().tick_seconds();
  const auto total_ticks = static_cast<std::uint64_t>(options.seconds / tick_seconds);
  const auto started = std::chrono::steady_clock::now();
  std::uint64_t ticks = 0;
  while (ticks < total_ticks) {

    ticks += static_cast<std::uint64_t>(session.advance(tick_seconds, 1));
    if (options.realtime) {
      std::this_thread::sleep_for(std::chrono::duration<double>(tick_seconds));
    }
  }
  const double wall =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

  std::printf("soi_headless: %s ran %llu ticks (%.1f s simulated) in %.2f s wall; "
              "digest %llu; %llu commands accepted, %llu rejected, %llu dropped\n",
              options.scenario.c_str(),
              static_cast<unsigned long long>(ticks),
              static_cast<double>(ticks) * tick_seconds,
              wall,
              static_cast<unsigned long long>(Game::Session::session_digest(session)),
              static_cast<unsigned long long>(session.commands().accepted_count()),
              static_cast<unsigned long long>(session.commands().rejected_count()),
              static_cast<unsigned long long>(session.commands().dropped_count()));

  if (auto* recorder = session.replay_recorder()) {
    recorder->finish();
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
    std::printf("SOI_HEADLESS_REPLAY: PASS - %zu commands fed, %zu digests matched\n",
                player->fed_count(),
                player->checked_count());
  }
  return 0;
}
